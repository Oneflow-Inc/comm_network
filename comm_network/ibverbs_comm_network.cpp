#include "comm_network/ibverbs_comm_network.h"
#include "comm_network/ibverbs.pb.h"

namespace comm_network {

IBVerbsCommNet::IBVerbsCommNet(EnvDesc& env_desc) {
  // machine configurations
  auto machine_cfg = env_desc.machine_cfgs();
  int64_t this_machine_id = env_desc.my_machine_id();
  for (auto it = machine_cfg.begin(); it != machine_cfg.end(); it++) {
    if (it->first != this_machine_id) {
      peer_machine_id_.insert(it->first);
    }
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
  qp_vec_.assign(peer_machine_id_.size(), nullptr);
  for (int64_t peer_id : peer_machine_id()) {
    IBVerbsQP* cur_qp = new IBVerbsQP(context_, pd_, cq_, cq_);
    qp_vec_.at(peer_id) = cur_qp;
    IBVerbsConnectionInfo conn_info;
    conn_info.set_lid(port_attr.lid);
    conn_info.set_qp_num(cur_qp->qp_num());
    conn_info.set_subnet_prefix(gid.global.subnet_prefix);
    conn_info.set_interface_id(gid.global.interface_id);
    // Global<CtrlClient>::Get()->PushKV(GenConnInfoKey(this_machine_id, peer_id), conn_info);
  }
  // for (int64_t peer_id : peer_machine_id()) {
  //   IBVerbsConnectionInfo conn_info;
  //   // Global<CtrlClient>::Get()->PullKV(GenConnInfoKey(peer_id, this_machine_id), &conn_info);
  //   qp_vec_.at(peer_id)->Connect(conn_info);
  // }
  // // OF_BARRIER();
  // for (int64_t peer_id : peer_machine_id()) {
  //   qp_vec_.at(peer_id)->PostAllRecvRequest();
  //   // Global<CtrlClient>::Get()->ClearKV(GenConnInfoKey(this_machine_id, peer_id));
  // }
  // // OF_BARRIER();
  // poll_thread_ = std::thread(&IBVerbsCommNet::PollCQ, this);
  // // OF_BARRIER(); 
}

}  // namespace comm_network