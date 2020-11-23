#include "comm_network/ibverbs/ibverbs_comm_network.h"
#include "comm_network/message.h"
#include "comm_network/ibverbs/ibverbs.pb.h"
#include "comm_network/control/ctrl_client.h"
#include "comm_network/control/ctrl_server.h"

namespace comm_network {
std::string GenConnInfoKey(int64_t src_machine_id, int64_t dst_machine_id) {
  return "IBVerbsConnInfo/" + std::to_string(src_machine_id) + "/" + std::to_string(dst_machine_id);
}

void IBVerbsCommNet::SetUp() {
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
  // create multiple completion queues
  int64_t poller_num = Global<CommNetConfigDesc>::Get()->PollerNum();
  for (int i = 0; i < poller_num; i++) {
    ibv_cq* cq = ibv_create_cq(context_, device_attr.max_cqe, nullptr, nullptr, 0);
    CHECK(cq);
    cq_vec_.emplace_back(cq);
    IBVerbsPoller* poller = new IBVerbsPoller(cq);
    AddPoller(poller);
  }
  CHECK_EQ(poller_vec_.size(), poller_num);
  ibv_port_attr port_attr;
  CHECK_EQ(ibv_query_port(context_, 1, &port_attr), 0);
  ibv_gid gid;
  CHECK_EQ(ibv_query_gid(context_, 1, 0, &gid), 0);
  qp_vec_.assign(peer_machine_id_.size() + 1, nullptr);
  size_t cq_idx = 0;
  for (int64_t peer_id : peer_machine_id_) {
    // Balanced select cq for qp
    ibv_cq* cur_cq = cq_vec_[cq_idx];
    cq_idx = (cq_idx + 1) % poller_num;
    IBVerbsQP* cur_qp = new IBVerbsQP(context_, pd_, cur_cq, cur_cq, ThisMachineId(), peer_id);
    qp_vec_.at(peer_id) = cur_qp;
    IBVerbsConnectionInfo conn_info;
    conn_info.set_lid(port_attr.lid);
    conn_info.set_qp_num(cur_qp->qp_num());
    conn_info.set_subnet_prefix(gid.global.subnet_prefix);
    conn_info.set_interface_id(gid.global.interface_id);
    Global<CtrlClient>::Get()->PushKV(GenConnInfoKey(ThisMachineId(), peer_id), conn_info);
    cur_qp->PushRegisterMemoryKey();
  }
  for (int64_t peer_id : peer_machine_id_) {
    IBVerbsConnectionInfo conn_info;
    Global<CtrlClient>::Get()->PullKV(GenConnInfoKey(peer_id, ThisMachineId()), &conn_info);
    qp_vec_.at(peer_id)->PullRegisterMemoryKey();
    qp_vec_.at(peer_id)->Connect(conn_info);
  }
  BARRIER();
  for (int64_t peer_id : peer_machine_id_) {
    qp_vec_.at(peer_id)->PostAllRecvRequest();
    Global<CtrlClient>::Get()->ClearKV(GenConnInfoKey(ThisMachineId(), peer_id));
    qp_vec_.at(peer_id)->ClearKeyAndCreateHelper();
  }
  BARRIER();
}

void IBVerbsCommNet::TearDown() {
  for (IBVerbsQP* qp : qp_vec_) {
    if (qp) { delete qp; }
  }
  for (ibv_cq* cq : cq_vec_) { CHECK_EQ(ibv_destroy_cq(cq), 0); }
  CHECK_EQ(ibv_dealloc_pd(pd_), 0);
  CHECK_EQ(ibv_close_device(context_), 0);
}

void IBVerbsCommNet::SendMsg(int64_t dst_machine_id, int64_t msg_type, const char* ptr,
                             size_t bytes, std::function<void()> cb) {
  // Construct user message
  {
    std::unique_lock<std::mutex> lock(cb_queue_mtx_);
    cb_queue_.push(std::move(cb));
  }
  UserMsg user_msg = {msg_type, ptr, bytes};
  Msg msg;
  msg.msg_type = MsgType::kUserMsg;
  msg.user_msg = user_msg;
  qp_vec_.at(dst_machine_id)->PostSendRequest(msg);
}

void IBVerbsCommNet::DoRead(int64_t src_machine_id, void* src_addr, size_t bytes, void* dst_addr,
                            std::function<void()> cb) {
  PleaseWrite please_write = {src_machine_id, src_addr, bytes};
  Msg msg;
  msg.msg_type = MsgType::kPleaseWrite;
  msg.please_write = please_write;
  // enqueue this new work record into read_queue
  WorkRecord record(src_machine_id, dst_addr, bytes, 0, cb);
  {
    std::unique_lock<std::mutex> lock(read_queue_mtx_);
    read_queue_.push(record);
  }
  // Send please_write message to sender, request for data
  qp_vec_.at(src_machine_id)->PostSendRequest(msg);
}

void IBVerbsCommNet::Normal2RegisterDone(int64_t dst_machine_id, IBVerbsMemDesc* send_mem_desc,
                                         IBVerbsMemDescProto recv_mem_desc_proto, int32_t buffer_id,
                                         int32_t sge_num) {
  qp_vec_.at(dst_machine_id)
      ->PostWriteRequest(recv_mem_desc_proto, *send_mem_desc, buffer_id, sge_num);
}

void IBVerbsCommNet::Register2NormalDone(int64_t machine_id, int32_t buffer_id, bool last_piece) {
  FreeBufferPair free_buffer_pair = {machine_id, buffer_id};
  Msg msg;
  msg.msg_type = MsgType::kPleaseWrite;
  msg.free_buffer_pair = free_buffer_pair;
  qp_vec_.at(machine_id)->PostSendRequest(msg);
  if (last_piece) {
    read_queue_.front().cb();
    {
      std::unique_lock<std::mutex> lock(read_queue_mtx_);
      read_queue_.pop();
    }
  }
}

std::function<void()> IBVerbsCommNet::UserSendMsgCb() {
  std::unique_lock<std::mutex> lock(cb_queue_mtx_);
  auto cb = cb_queue_.front();
  cb_queue_.pop();
  return cb;
}

}  // namespace comm_network