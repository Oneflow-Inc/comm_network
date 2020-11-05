#include "comm_network/ibverbs_comm_network.h"
#include "comm_network/ibverbs.pb.h"
#include "comm_network/control/control.pb.h"

namespace comm_network {

std::string GenConnInfoKey(int64_t src_machine_id, int64_t dst_machine_id) {
  return "IBVerbsConnInfo/" + std::to_string(src_machine_id) + "/" + std::to_string(dst_machine_id);
}

IBVerbsCommNet::IBVerbsCommNet(Channel<Msg>* action_channel) : action_channel_(action_channel) {
  int64_t total_machine_num = Global<EnvDesc>::Get()->TotalMachineNum();
  this_machine_id_ =
      Global<EnvDesc>::Get()->GetMachineId(Global<CtrlServer>::Get()->this_machine_addr());
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
  // poller : cq = 1 : 1, cq : qp : helper = 1 : peer_machines : peer_machines
  poller_ = new IBVerbsPoller(cq_);
  qp_vec_.assign(peer_machine_id_.size() + 1, nullptr);
  for (int64_t peer_id : peer_machine_id()) {
    IBVerbsQP* cur_qp = new IBVerbsQP(context_, pd_, cq_, cq_, this_machine_id_, peer_id);
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
  poller_->Start();
}

IBVerbsCommNet::~IBVerbsCommNet() {
  CHECK_EQ(read_queue_.size(), 0);
  for (IBVerbsQP* qp : qp_vec_) {
    if (qp) { delete qp; }
  }
  poller_->Stop();
  delete poller_;
  CHECK_EQ(ibv_destroy_cq(cq_), 0);
  CHECK_EQ(ibv_dealloc_pd(pd_), 0);
  CHECK_EQ(ibv_close_device(context_), 0);
}

void IBVerbsCommNet::SendMsg(int64_t dst_machine_id, const Msg& msg) {
  qp_vec_.at(dst_machine_id)->PostSendRequest(msg);
}

void IBVerbsCommNet::DoRead(int64_t src_machine_id, void* src_addr, void* dst_addr,
                            size_t data_size, std::function<void()> callback) {
  // send "PleaseWrite" message to sender machine
  Msg msg;
  msg.msg_type = MsgType::kPleaseWrite;
  msg.please_write.dst_machine_id = this_machine_id_;
  msg.please_write.src_addr = src_addr;
  msg.please_write.data_size = data_size;
  uint32_t read_id = AllocateReadId();
  msg.please_write.read_id = read_id;
  // enqueue this new work record into read_queue
  WorkRecord record(read_id, src_machine_id, dst_addr, data_size, 0, callback);
  {
    std::unique_lock<std::mutex> lock(read_queue_mtx_);
    read_queue_.emplace(read_id, record);
  }
  qp_vec_.at(src_machine_id)->PostSendRequest(msg);
}

uint32_t IBVerbsCommNet::AllocateReadId() {
  // Generate a random number
  std::mt19937 gen(NewRandomSeed());
  std::uniform_int_distribution<uint32_t> read_id_distrib(0, pow(2, 24) - 1);
  uint32_t read_id;
  bool flag = false;
  do {
    read_id = read_id_distrib(gen);
    std::unique_lock<std::mutex> lock(busy_read_id_mtx_);
    flag = (busy_read_ids_.find(read_id) == busy_read_ids_.end());
    if (flag) { busy_read_ids_.insert(read_id); }
  } while (!flag);
  CHECK_GE(read_id, 0);
  CHECK_LE(read_id, pow(2, 24) - 1);
  return read_id;
}

void IBVerbsCommNet::FreeReadId(uint32_t read_id) {
  std::unique_lock<std::mutex> lock(busy_read_id_mtx_);
  CHECK_EQ(busy_read_ids_.erase(read_id), 1);
}

void IBVerbsCommNet::Normal2RegisterDone(int64_t dst_machine_id, IBVerbsMemDesc* send_mem_desc,
                                         IBVerbsMemDescProto recv_mem_desc_proto,
                                         uint32_t imm_data) {
  qp_vec_.at(dst_machine_id)->PostWriteRequest(recv_mem_desc_proto, *send_mem_desc, imm_data);
}

void IBVerbsCommNet::Register2NormalDone(int64_t machine_id, uint8_t buffer_id, uint32_t read_id,
                                         bool last_piece) {
  Msg cur_msg;
  cur_msg.msg_type = MsgType::kFreeBufferPair;
  cur_msg.free_buffer_pair.src_machine_id = machine_id;
  cur_msg.free_buffer_pair.buffer_id = buffer_id;
  SendMsg(machine_id, cur_msg);
  if (last_piece) {
    std::function<void()> cb;
    {
      std::unique_lock<std::mutex> lock(read_queue_mtx_);
      auto read_iter = read_queue_.find(read_id);
      CHECK(read_iter != read_queue_.end());
      cb = read_iter->second.callback;
      read_queue_.erase(read_iter);
    }
    cb();
    FreeReadId(read_id);
  }
}

}  // namespace comm_network
