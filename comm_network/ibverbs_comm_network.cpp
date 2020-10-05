#include "comm_network/ibverbs_comm_network.h"
#include "comm_network/ibverbs.pb.h"
#include "comm_network/control/control.pb.h"

namespace comm_network {

std::string GenTokensMsgKey(int64_t src_machine_id, int dst_machine_id) {
  return "IBVerbsTokensMsg/" + std::to_string(src_machine_id) + "/" + std::to_string(dst_machine_id);
}

std::string GenConnInfoKey(int64_t src_machine_id, int64_t dst_machine_id) {
  return "IBVerbsConnInfo/" + std::to_string(src_machine_id) + "/" + std::to_string(dst_machine_id);
}

std::string GenRegisterSendMemoryKey(int64_t src_machine_id, int64_t dst_machine_id, int64_t idx) {
  return "IBVerbsSendMemory/" + std::to_string(src_machine_id) + "/" + std::to_string(dst_machine_id) 
    + "/" + std::to_string(idx);
}

std::string GenRegisterRecvMemoryKey(int64_t src_machine_id, int64_t dst_machine_id, int64_t idx) {
  return "IBVerbsRecvMemory/" + std::to_string(src_machine_id) + "/" + std::to_string(dst_machine_id) 
    + "/" + std::to_string(idx);
}

IBVerbsCommNet::IBVerbsCommNet(Channel<Msg> *action_channel)
    : poll_exit_flag_(ATOMIC_FLAG_INIT), action_channel_(action_channel) {
  int64_t total_machine_num = Global<CtrlClient>::Get()->env_desc()->TotalMachineNum();
  this_machine_id_ = Global<CtrlClient>::Get()->env_desc()->GetMachineId(Global<CtrlServer>::Get()->this_machine_addr());
  mem_desc_list_.resize(total_machine_num);
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
  poll_thread_ = std::thread(&IBVerbsCommNet::PollCQ, this);
  BARRIER();
  read_helper_ = new IBVerbsReadHelper();
  write_helper_ = new IBVerbsWriteHelper();
  poll_channel_thread_ = std::thread(&IBVerbsCommNet::PollQueue, this);
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
        case IBV_WC_RDMA_WRITE: {
          qp->WriteDone(wr_id); 
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

void IBVerbsCommNet::PollQueue() {
  WritePartial write_partial;
  while (write_channel_.Receive(&write_partial) == kChannelStatusSuccess) {
    int64_t src_machine_id = write_partial.src_machine_id;
    int64_t dst_machine_id = write_partial.dst_machine_id;
    bool has_free = false;
    std::string send_key, recv_key;
    int buffer_id;
    do {
      has_free = FindAvailSendMemory(src_machine_id, dst_machine_id, send_key, recv_key, buffer_id); 
    } while (!has_free);
    IBVerbsMemDesc* send_mem_desc = mem_desc_[send_key].first;
    IBVerbsMemDescProto recv_mem_desc_proto = mem_desc_list_[dst_machine_id][recv_key];
    LOG(INFO) << send_key << " " << recv_key;
    LOG(INFO) << "Write from " << src_machine_id << " to " << dst_machine_id << " bytes: " << write_partial.data_size;
    // copy normal memory to register memory
    ibv_sge cur_sge = send_mem_desc->sge_vec().at(0);
    memcpy(reinterpret_cast<void*>(cur_sge.addr), write_partial.src_addr, write_partial.data_size);
    // write immediately
    write_partial.buffer_id = buffer_id;
    qp_vec_.at(dst_machine_id)->PostWriteRequest(recv_mem_desc_proto, *send_mem_desc, &write_partial); 
  }
}

IBVerbsCommNet::~IBVerbsCommNet() {
  while (poll_exit_flag_.test_and_set() == true) {}
  poll_thread_.join();
  delete read_helper_;
  delete write_helper_;
  write_channel_.Close();
  poll_channel_thread_.join();
  for (IBVerbsQP* qp : qp_vec_) {
    if (qp) { delete qp; }
  }
  CHECK_EQ(ibv_destroy_cq(cq_), 0);
  CHECK_EQ(ibv_dealloc_pd(pd_), 0);
  CHECK_EQ(ibv_close_device(context_), 0);
}

void IBVerbsCommNet::SendMsg(int64_t dst_machine_id, const Msg& msg) {
  qp_vec_.at(dst_machine_id)->PostSendRequest(msg);
}

void IBVerbsCommNet::AsyncWrite(int64_t src_machine_id, int64_t dst_machine_id, void* src_addr, void* dst_addr, size_t data_size) {
  size_t size = 0;
  char* src_ptr = reinterpret_cast<char*>(src_addr);
  char* dst_ptr = reinterpret_cast<char*>(dst_addr); 
  int piece_id = 0;
  int total_piece_num = (data_size - 1) / buffer_size + 1;
  LOG(INFO) << "total piece num " << total_piece_num;
  while (size < data_size) {
    size_t transfer_size = std::min(data_size - size, buffer_size);
    WritePartial write_partial;
    write_partial.src_machine_id = src_machine_id;
    write_partial.dst_machine_id = dst_machine_id;
    write_partial.src_addr = src_ptr;
    write_partial.dst_addr = dst_ptr;
    write_partial.data_size = transfer_size;
    write_partial.piece_id = piece_id;
    write_partial.total_piece_num = total_piece_num;
    write_channel_.Send(write_partial);
    size += transfer_size;
    src_ptr += transfer_size;
    dst_ptr += transfer_size;
    piece_id++;
  }
}

void IBVerbsCommNet::Register2NormalMemory(const Msg& msg) {
  int64_t src_machine_id = msg.partial_write_done.src_machine_id;
  int64_t dst_machine_id = msg.partial_write_done.dst_machine_id;
  int buffer_id = msg.partial_write_done.buffer_id;
  size_t data_size = msg.partial_write_done.data_size;
  void* dst_addr = msg.partial_write_done.dst_addr;
  std::string recv_key = GenRegisterRecvMemoryKey(src_machine_id, dst_machine_id, buffer_id);
  std::string send_key = GenRegisterSendMemoryKey(src_machine_id, dst_machine_id, buffer_id);
  IBVerbsMemDesc* recv_mem_desc = mem_desc_[recv_key].first; 
  ibv_sge cur_sge = recv_mem_desc->sge_vec().at(0);
  memcpy(dst_addr, reinterpret_cast<void*>(cur_sge.addr), data_size); 
  Msg new_msg;
  new_msg.msg_type = MsgType::FreeBufferPair;
  FreeBufferPair free_buffer_pair;
  free_buffer_pair.src_machine_id = src_machine_id;
  free_buffer_pair.dst_machine_id = dst_machine_id;
  free_buffer_pair.buffer_id = buffer_id;
  new_msg.free_buffer_pair = free_buffer_pair;
  SendMsg(src_machine_id, new_msg);
  if (msg.partial_write_done.piece_id == msg.partial_write_done.total_piece_num - 1) {
    LOG(INFO) << "All data has been received";
    Msg finish_msg;
    finish_msg.msg_type = MsgType::ReadDone;
    action_channel_->Send(finish_msg);
  }
}

void IBVerbsCommNet::ReleaseBuffer(int64_t src_machine_id, int64_t dst_machine_id, int buffer_id) {
  std::string send_key = GenRegisterSendMemoryKey(src_machine_id, dst_machine_id, buffer_id);
  mem_desc_[send_key].second = true;
}

void* IBVerbsCommNet::RegisterMemory(std::string key, void* ptr, size_t byte_size) {
  IBVerbsMemDesc* mem_desc = new IBVerbsMemDesc(pd_, ptr, byte_size);
  mem_desc_.emplace(key, std::make_pair(mem_desc, true));
  return mem_desc;
}

void IBVerbsCommNet::UnRegisterMemory(std::string key) {
  IBVerbsMemDesc* mem_desc = mem_desc_[key].first;
  delete mem_desc;
  CHECK_EQ(mem_desc_.erase(key), 1);
}

void IBVerbsCommNet::RegisterFixNumMemory() {
  for (int64_t peer_id : peer_machine_id()) { 
    IBVerbsTokensMsg this_tokens_msg;
    for (int i = 0;i < num_of_register_buffer / 2;i++) {
      std::string send_key = GenRegisterSendMemoryKey(this_machine_id_, peer_id, i);
      std::string recv_key = GenRegisterRecvMemoryKey(peer_id, this_machine_id_, i);
      void* send_buffer = malloc(buffer_size);
      void* recv_buffer = malloc(buffer_size);
      RegisterMemory(send_key, send_buffer, buffer_size);
      RegisterMemory(recv_key, recv_buffer, buffer_size);
      this_tokens_msg.mutable_mem_desc()->insert(
        {send_key, mem_desc_[send_key].first->ToProto()});
      this_tokens_msg.mutable_mem_desc()->insert(
        {recv_key, mem_desc_[recv_key].first->ToProto()});
    }
    Global<CtrlClient>::Get()->PushKV(GenTokensMsgKey(this_machine_id_, peer_id), this_tokens_msg);
  }
  for (int64_t peer_id : peer_machine_id()) {
    IBVerbsTokensMsg peer_tokens_msg;
    Global<CtrlClient>::Get()->PullKV(GenTokensMsgKey(peer_id, this_machine_id_), &peer_tokens_msg);
    for (const auto& pair : peer_tokens_msg.mem_desc()) {
      mem_desc_list_.at(peer_id).emplace(pair.first, pair.second);
    }
  }
  BARRIER();
  for (int64_t peer_id : peer_machine_id()) {
    Global<CtrlClient>::Get()->ClearKV(GenTokensMsgKey(this_machine_id_, peer_id));
  }
}

void IBVerbsCommNet::UnRegisterFixNumMemory() {
  while (!mem_desc().empty()) {
		auto iter = Global<IBVerbsCommNet>::Get()->mem_desc().begin();
		Global<IBVerbsCommNet>::Get()->UnRegisterMemory(iter->first);
	}
}

bool IBVerbsCommNet::FindAvailSendMemory(int64_t src_machine_id, int64_t dst_machine_id, std::string& send_key, std::string& recv_key, int& buffer_id) {
  for (int i = 0;i < num_of_register_buffer / 2;i++) {
    std::string key = GenRegisterSendMemoryKey(src_machine_id, dst_machine_id, i);
    if (mem_desc_[key].second) {
      mem_desc_[key].second = false;
      send_key = key;
      recv_key = GenRegisterRecvMemoryKey(src_machine_id, dst_machine_id, i); 
      buffer_id = i;
      return true;
    }
  }
  return false;
}

const int32_t IBVerbsCommNet::max_poll_wc_num_ = 32;
}  // namespace comm_network
