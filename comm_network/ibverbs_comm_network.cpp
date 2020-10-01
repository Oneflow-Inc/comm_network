#include "comm_network/ibverbs_comm_network.h"
#include "comm_network/ibverbs.pb.h"
#include "comm_network/control/control.pb.h"

namespace comm_network {

std::string GenConnInfoKey(int64_t src_machine_id, int64_t dst_machine_id) {
  return "IBVerbsConnInfo/" + std::to_string(src_machine_id) + "/" + std::to_string(dst_machine_id);
}

IBVerbsCommNet::IBVerbsCommNet(CtrlClient* ctrl_client, MsgBus* msg_bus, int64_t this_machine_id)
    : poll_exit_flag_(ATOMIC_FLAG_INIT),
      ctrl_client_(ctrl_client),
      this_machine_id_(this_machine_id),
			msg_bus_(msg_bus) {
  int64_t total_machine_num = ctrl_client->env_desc()->TotalMachineNum();
  for (int64_t i = 0; i < total_machine_num; ++i) {
    if (i == this_machine_id) { continue; }
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
    ctrl_client->PushKV(GenConnInfoKey(this_machine_id, peer_id), conn_info);
  }
  for (int64_t peer_id : peer_machine_id()) {
    IBVerbsConnectionInfo conn_info;
    ctrl_client->PullKV(GenConnInfoKey(peer_id, this_machine_id), &conn_info);
    qp_vec_.at(peer_id)->Connect(conn_info);
  }
  BARRIER(ctrl_client);
  for (int64_t peer_id : peer_machine_id()) {
    qp_vec_.at(peer_id)->PostAllRecvRequest();
    ctrl_client->ClearKV(GenConnInfoKey(this_machine_id, peer_id));
  }
  BARRIER(ctrl_client);
  poll_thread_ = std::thread(&IBVerbsCommNet::PollCQ, this);
  BARRIER(ctrl_client);
}

IBVerbsCommNet::~IBVerbsCommNet() {
  while (poll_exit_flag_.test_and_set() == true) {}
  poll_thread_.join();
  for (IBVerbsQP* qp : qp_vec_) {
    if (qp) { delete qp; }
  }
  CHECK_EQ(ibv_destroy_cq(cq_), 0);
  CHECK_EQ(ibv_dealloc_pd(pd_), 0);
  CHECK_EQ(ibv_close_device(context_), 0);
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
          qp->RecvDone(wr_id, msg_bus_);
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

void IBVerbsCommNet::SendMsg(int64_t dst_machine_id, const Msg& msg) {
  qp_vec_.at(dst_machine_id)->PostSendRequest(msg);
}

const int32_t IBVerbsCommNet::max_poll_wc_num_ = 32;
}  // namespace comm_network
