#include "comm_network/ibverbs_comm_network.h"
#include "comm_network/ibverbs.pb.h"
#include "comm_network/control/control.pb.h"

namespace comm_network {

std::string GenTokensMsgKey(int64_t machine_id) {
  return "IBVerbsTokensMsg/" + std::to_string(machine_id);
}

std::string GenRegisterMemoryKey(std::string prefix, int64_t idx) {
  return prefix + std::to_string(idx);
}

std::string GenConnInfoKey(int64_t src_machine_id, int64_t dst_machine_id) {
  return "IBVerbsConnInfo/" + std::to_string(src_machine_id) + "/" + std::to_string(dst_machine_id);
}

IBVerbsCommNet::IBVerbsCommNet(Channel<Msg> *action_channel)
    : poll_exit_flag_(ATOMIC_FLAG_INIT), action_channel_(action_channel) {
  int64_t total_machine_num = Global<CtrlClient>::Get()->env_desc()->TotalMachineNum();
  this_machine_id_ = Global<CtrlClient>::Get()->env_desc()->GetMachineId(Global<CtrlServer>::Get()->this_machine_addr());
  recv_mem_desc_.resize(total_machine_num);
  for (int64_t i = 0; i < total_machine_num; ++i) {
    if (i == this_machine_id_) { continue; }
    peer_machine_id_.insert(i);
  }
  // prepare qp connections
  ibv_device** device_list = ibv_get_device_list(nullptr);
  PCHECK(device_list);
  ibv_device* device = device_list[0];
  context_ = ibv_open_device(device);
  CHECK(context_);
  ibv_free_device_list(device_list);
  pd_ = ibv_alloc_pd(context_);
  CHECK(pd_);
  ibv_device_attr device_attr;
  CHECK_EQ(ibv_query_device(context_, &device_attr), 0);
  cq_ = ibv_create_cq(context_, device_attr.max_cqe, nullptr, nullptr, 0);
  CHECK(cq_);
  ibv_port_attr port_attr;
  CHECK_EQ(ibv_query_port(context_, 1, &port_attr), 0);
  ibv_gid gid;
  CHECK_EQ(ibv_query_gid(context_, 1, 0, &gid), 0);
  qp_vec_.assign(peer_machine_id_.size() + 1, nullptr);
  for (int64_t peer_id : peer_machine_id()) {
    IBVerbsQP* cur_qp = new IBVerbsQP(context_, pd_, cq_, cq_);
    qp_vec_.at(peer_id) = cur_qp;
    IBVerbsConnectionInfo conn_info;
    conn_info.set_lid(port_attr.lid);
    conn_info.set_qp_num(cur_qp->qp_num());
    conn_info.set_subnet_prefix(gid.global.subnet_prefix);
    conn_info.set_interface_id(gid.global.interface_id);
    Global<CtrlClient>::Get()->PushKV(GenConnInfoKey(this_machine_id_, peer_id), conn_info);
  }
  for (int64_t peer_id : peer_machine_id()) {
    IBVerbsConnectionInfo conn_info;
    Global<CtrlClient>::Get()->PullKV(GenConnInfoKey(peer_id, this_machine_id_), &conn_info);
    qp_vec_.at(peer_id)->Connect(conn_info);
  }
  BARRIER();
  for (int64_t peer_id : peer_machine_id()) {
    qp_vec_.at(peer_id)->PostAllRecvRequest();
    Global<CtrlClient>::Get()->ClearKV(GenConnInfoKey(this_machine_id_, peer_id));
  }
  BARRIER();
  poller_ = new IBVerbsPoller();
  poller_->Start();
  poll_thread_ = std::thread(&IBVerbsCommNet::PollCQ, this);
  BARRIER();
}

void IBVerbsCommNet::PollCQ() {
  std::vector<ibv_wc> wc_vec(max_poll_wc_num_);
  while (poll_exit_flag_.test_and_set() == false) {
    poll_exit_flag_.clear();
    int32_t found_wc_num = ibv_poll_cq(cq_, max_poll_wc_num_, wc_vec.data());
    CHECK_GE(found_wc_num, 0);
    FOR_RANGE(int32_t, i, 0, found_wc_num) {
      const ibv_wc& wc = wc_vec.at(i);
      CHECK_EQ(wc.status, IBV_WC_SUCCESS) << wc.opcode;
      WorkRequestId* wr_id = reinterpret_cast<WorkRequestId*>(wc.wr_id);
      IBVerbsQP* qp = wr_id->qp;
      switch (wc.opcode) {
        case IBV_WC_RDMA_READ: {
          qp->ReadDone(wr_id);
          break;
        }
        case IBV_WC_SEND: {
          qp->SendDone(wr_id);
          break;
        }
        case IBV_WC_RECV: {
          qp->RecvDone(wr_id, action_channel_);
          break;
        }
        default: {
          LOG(FATAL) << "UNIMPLEMENTED";
          break;
        }
      }
    }
  }
}

IBVerbsCommNet::~IBVerbsCommNet() {
  while (poll_exit_flag_.test_and_set() == true) {}
  poll_thread_.join();
  poller_->Stop();
  delete poller_;
  for (IBVerbsQP* qp : qp_vec_) {
    if (qp) { delete qp; }
  }
  CHECK_EQ(ibv_destroy_cq(cq_), 0);
  CHECK_EQ(ibv_dealloc_pd(pd_), 0);
  CHECK_EQ(ibv_close_device(context_), 0);
}

void IBVerbsCommNet::RegisterMemoryDone() {
  IBVerbsTokensMsg this_tokens_msg;
  for (int i = 0; i < num_of_register_buffer / 2; i++) {
    std::string key = GenRegisterMemoryKey("recv", i);
    IBVerbsMemDesc* mem_desc = mem_desc_[key];
    this_tokens_msg.mutable_recv_mem_desc()->insert(
        {i, mem_desc->ToProto()});
  }
  Global<CtrlClient>::Get()->PushKV(GenTokensMsgKey(this_machine_id_), this_tokens_msg);
  for (int64_t peer_id : peer_machine_id()) {
    IBVerbsTokensMsg peer_tokens_msg;
    Global<CtrlClient>::Get()->PullKV(GenTokensMsgKey(peer_id), &peer_tokens_msg);
    for (const auto& pair : peer_tokens_msg.recv_mem_desc()) {
      recv_mem_desc_.at(peer_id).emplace(pair.first, pair.second);
    }
  }
  BARRIER();
  Global<CtrlClient>::Get()->ClearKV(GenTokensMsgKey(this_machine_id_));
}

void IBVerbsCommNet::SendMsg(int64_t dst_machine_id, const Msg& msg) {
  qp_vec_.at(dst_machine_id)->PostSendRequest(msg);
}

void IBVerbsCommNet::Read(int64_t src_machine_id, void* src_addr, void* dst_addr,
                          size_t data_size) {
  // send message to source notify it to write data
	// Msg msg(src_machine_id, this_machine_id_, src_addr, dst_addr, data_size, MsgType::PleaseWrite);
	// ibverbs_comm_net->SendMsg(src_machine_id, msg);	
}

void IBVerbsCommNet::AsyncWrite(int64_t src_machine_id, int64_t dst_machine_id, void* src_addr, void* dst_addr, size_t data_size) {
  poller_->AsyncWrite(src_machine_id, dst_machine_id, src_addr, dst_addr, data_size);
}

void* IBVerbsCommNet::RegisterMemory(std::string key, void* ptr, size_t byte_size) {
  IBVerbsMemDesc* mem_desc = new IBVerbsMemDesc(pd_, ptr, byte_size);
  mem_desc_.emplace(key, mem_desc);
  return mem_desc;
}

void IBVerbsCommNet::UnRegisterMemory(std::string key) {
  IBVerbsMemDesc* mem_desc = mem_desc_[key];
  delete mem_desc;
  CHECK_EQ(mem_desc_.erase(key), 1);
}

void IBVerbsCommNet::RegisterFixNumMemory() {
  for (int j = 0;j < 2;j++) {
    std::string prefix = (j == 0 ? "send" : "recv");
    for (int i = 0;i < num_of_register_buffer / 2;i++) {
      std::string key = GenRegisterMemoryKey(prefix, i);
      void* buffer = malloc(buffer_size);
      RegisterMemory(key, buffer, buffer_size);
    }
  }
}

void IBVerbsCommNet::UnRegisterFixNumMemory() {
  while (mem_desc().empty()) {
		auto iter = Global<IBVerbsCommNet>::Get()->mem_desc().begin();
		Global<IBVerbsCommNet>::Get()->UnRegisterMemory(iter->first);
	}
}

const int32_t IBVerbsCommNet::max_poll_wc_num_ = 32;
}  // namespace comm_network
