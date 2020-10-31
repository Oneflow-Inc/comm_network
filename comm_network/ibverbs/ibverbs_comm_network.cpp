#include "comm_network/ibverbs/ibverbs_comm_network.h"
#include "comm_network/ibverbs/ibverbs.pb.h"
#include "comm_network/control/control.pb.h"

namespace comm_network {

std::string GenTokensMsgKey(int64_t src_machine_id, int dst_machine_id) {
  return "IBVerbsTokensMsg/" + std::to_string(src_machine_id) + "/"
         + std::to_string(dst_machine_id);
}

std::string GenConnInfoKey(int64_t src_machine_id, int64_t dst_machine_id) {
  return "IBVerbsConnInfo/" + std::to_string(src_machine_id) + "/" + std::to_string(dst_machine_id);
}

std::string GenRegisterSendMemoryKey(int64_t src_machine_id, int64_t dst_machine_id, uint8_t idx) {
  return "IBVerbsSendMemory/" + std::to_string(src_machine_id) + "/"
         + std::to_string(dst_machine_id) + "/" + std::to_string(idx);
}

std::string GenRegisterRecvMemoryKey(int64_t src_machine_id, int64_t dst_machine_id, uint8_t idx) {
  return "IBVerbsRecvMemory/" + std::to_string(src_machine_id) + "/"
         + std::to_string(dst_machine_id) + "/" + std::to_string(idx);
}

IBVerbsCommNet::IBVerbsCommNet() : CommNet() {
	int64_t total_machine_num = Global<EnvDesc>::Get()->TotalMachineNum();
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
  // poller : cq = 1 : 1, cq : qp : helper = 1 : peer_machines : peer_machines
  poller_ = new IBVerbsPoller(cq_);
  qp_vec_.assign(peer_machine_id_.size() + 1, nullptr);
  helper_vec_.assign(peer_machine_id_.size() + 1, nullptr);
  for (int64_t peer_id : peer_machine_id()) {
    IBVerbsHelper* cur_helper = new IBVerbsHelper;
    IBVerbsQP* cur_qp = new IBVerbsQP(context_, pd_, cq_, cq_, cur_helper);
    qp_vec_.at(peer_id) = cur_qp;
    helper_vec_.at(peer_id) = cur_helper;
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
  RegisterFixNumMemory();
}

IBVerbsCommNet::~IBVerbsCommNet() {
  UnRegisterFixNumMemory();
  CHECK_EQ(read_queue_.size(), 0);
  for (IBVerbsQP* qp : qp_vec_) {
    if (qp) { delete qp; }
  }
  for (IBVerbsHelper* helper : helper_vec_) {
    if (helper) { delete helper; }
  }
  poller_->Stop();
  delete poller_;
  CHECK_EQ(ibv_destroy_cq(cq_), 0);
  CHECK_EQ(ibv_dealloc_pd(pd_), 0);
  CHECK_EQ(ibv_close_device(context_), 0);
}

void IBVerbsCommNet::SendMsg(int64_t dst_machine_id, Msg& msg,
                             std::function<void()> callback) {
  if (msg.msg_type == MsgType::kDataIsReady) {
    uint32_t read_id = AllocateReadId();
    msg.data_is_ready.read_id = read_id; 
    AddUnfinishedDataReadyItem(msg, callback);
  }
  qp_vec_.at(dst_machine_id)->PostSendRequest(msg);
}

void IBVerbsCommNet::DoRead(uint32_t read_id, int64_t src_machine_id, void* src_addr, void* dst_addr,
                            size_t data_size, std::function<void()> callback) {
  // send "PleaseWrite" message to sender machine
  Msg msg;
  msg.msg_type = MsgType::kPleaseWrite;
  msg.please_write.dst_machine_id = this_machine_id_;
  msg.please_write.src_addr = src_addr;
  msg.please_write.data_size = data_size;
  msg.please_write.read_id = read_id;
  // enqueue this new work record into read_queue
  WorkRecord record(read_id, src_machine_id, dst_addr, data_size, 0, callback);
  {
    std::unique_lock<std::mutex> lock(read_queue_mtx_);
    read_queue_.emplace(read_id, record);
  }
  qp_vec_.at(src_machine_id)->PostSendRequest(msg);
}

std::pair<IBVerbsMemDesc*, IBVerbsMemDescProto> IBVerbsCommNet::GetSendRecvMemPairForSender(
    int64_t machine_id, uint8_t buffer_id) {
  std::string send_key = GenRegisterSendMemoryKey(this_machine_id_, machine_id, buffer_id);
  std::string recv_key = GenRegisterRecvMemoryKey(this_machine_id_, machine_id, buffer_id);
  IBVerbsMemDesc* send_mem_desc = mem_desc_[send_key];
  IBVerbsMemDescProto recv_mem_desc_proto = mem_desc_list_[machine_id][recv_key];
  return std::make_pair(send_mem_desc, recv_mem_desc_proto);
}

IBVerbsMemDesc* IBVerbsCommNet::GetRecvMemDescForReceiver(int64_t machine_id, uint8_t buffer_id) {
  std::string recv_key = GenRegisterRecvMemoryKey(machine_id, this_machine_id_, buffer_id);
  IBVerbsMemDesc* recv_mem_desc = mem_desc_[recv_key];
  return recv_mem_desc;
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
  cur_msg.free_buffer_pair.read_id = 0;
  if (last_piece) {
    cur_msg.free_buffer_pair.read_id = read_id;
  }
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
  }
}

void IBVerbsCommNet::SendLargeDataDone(uint32_t read_id) {
  std::function<void()> cb;
  {
    std::unique_lock<std::mutex> lock(callback_queue_mtx_);
    auto cb_iter = callback_queue_.find(read_id);
    CHECK(cb_iter != callback_queue_.end());
    cb = cb_iter->second;
    callback_queue_.erase(cb_iter);
  }
  cb();
  FreeReadId(read_id);
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
  for (int64_t peer_id : peer_machine_id()) {
    IBVerbsTokensMsg this_tokens_msg;
    for (int i = 0; i < num_of_register_buffer / 2; i++) {
      std::string send_key = GenRegisterSendMemoryKey(this_machine_id_, peer_id, i);
      std::string recv_key = GenRegisterRecvMemoryKey(peer_id, this_machine_id_, i);
      void* send_buffer = malloc(buffer_size);
      void* recv_buffer = malloc(buffer_size);
      RegisterMemory(send_key, send_buffer, buffer_size);
      RegisterMemory(recv_key, recv_buffer, buffer_size);
      this_tokens_msg.mutable_mem_desc()->insert({send_key, mem_desc_[send_key]->ToProto()});
      this_tokens_msg.mutable_mem_desc()->insert({recv_key, mem_desc_[recv_key]->ToProto()});
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
  while (!mem_desc_.empty()) {
    auto iter = mem_desc_.begin();
    UnRegisterMemory(iter->first);
  }
}

}  // namespace comm_network
