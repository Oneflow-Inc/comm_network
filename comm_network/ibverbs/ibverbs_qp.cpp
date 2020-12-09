#include "comm_network/ibverbs/ibverbs_qp.h"
#include "comm_network/ibverbs/ibverbs_comm_network.h"
#include "comm_network/control/ctrl_client.h"
#include "comm_network/control/ctrl_server.h"

namespace comm_network {
std::string GenTokensMsgKey(int64_t src_machine_id, int dst_machine_id) {
  return "IBVerbsTokensMsg/" + std::to_string(src_machine_id) + "/"
         + std::to_string(dst_machine_id);
}

IBVerbsQP::IBVerbsQP(ibv_context* ctx, ibv_pd* pd, ibv_cq* send_cq, ibv_cq* recv_cq,
                     int64_t this_machine_id, int64_t peer_machine_id)
    : ctx_(ctx), pd_(pd) {
  // Create qp
  ibv_device_attr device_attr;
  CHECK_EQ(ibv_query_device(ctx, &device_attr), 0);
  uint32_t max_recv_wr = Global<CommNetConfigDesc>::Get()->SgeBytes() / sizeof(Msg);
  max_recv_wr = std::min<uint32_t>(max_recv_wr, device_attr.max_qp_wr);
  ibv_qp_init_attr qp_init_attr;
  qp_init_attr.qp_context = nullptr;
  qp_init_attr.send_cq = send_cq;
  qp_init_attr.recv_cq = recv_cq;
  qp_init_attr.srq = nullptr;
  qp_init_attr.cap.max_send_wr = device_attr.max_qp_wr;
  qp_init_attr.cap.max_recv_wr = max_recv_wr;
  qp_init_attr.cap.max_send_sge = 1;
  qp_init_attr.cap.max_recv_sge = 1;
  qp_init_attr.cap.max_inline_data = 0;
  qp_init_attr.qp_type = IBV_QPT_RC;
  qp_init_attr.sq_sig_all = 1;
  qp_ = ibv_create_qp(pd, &qp_init_attr);
  CHECK(qp_);
  // Prepare recv_msg_buf_
  recv_msg_buf_.assign(max_recv_wr, nullptr);
  FOR_RANGE(size_t, i, 0, recv_msg_buf_.size()) { recv_msg_buf_.at(i) = new MsgMR(pd_); }
  // Prepare send_msg_buf_
  CHECK(send_msg_buf_.empty());
  CHECK(read_queue_.empty());
  this_machine_id_ = this_machine_id;
  peer_machine_id_ = peer_machine_id;
  // Allocate send/recv register memory
  int64_t register_buffer_num = Global<CommNetConfigDesc>::Get()->RegisterBufferNum();
  size_t buffer_size = Global<CommNetConfigDesc>::Get()->PerRegisterBufferMBytes() * 1024 * 1024;
  for (int i = 0; i < register_buffer_num; i++) {
    void* send_buffer = malloc(buffer_size);
    IBVerbsMemDesc* send_mem_desc = new IBVerbsMemDesc(pd_, send_buffer, buffer_size);
    void* recv_buffer = malloc(buffer_size);
    IBVerbsMemDesc* recv_mem_desc = new IBVerbsMemDesc(pd_, recv_buffer, buffer_size);
    mem_desc_.emplace_back(send_mem_desc, recv_mem_desc);
  }
}

void IBVerbsQP::PushRegisterMemoryKey() {
  // Notify other machines the address of register memory
  // Only receive memory need to be send
  IBVerbsTokensMsg this_tokens_msg;
  for (int i = 0; i < Global<CommNetConfigDesc>::Get()->RegisterBufferNum(); i++) {
    IBVerbsMemDescProto* new_proto = this_tokens_msg.add_peer_recv_mem_desc();
    mem_desc_[i].second->ToProto(new_proto);
  }
  Global<CtrlClient>::Get()->PushKV(GenTokensMsgKey(this_machine_id_, peer_machine_id_),
                                    this_tokens_msg);
}

void IBVerbsQP::PullRegisterMemoryKey() {
  // Receive other machines' receive register memory
  IBVerbsTokensMsg peer_tokens_msg;
  Global<CtrlClient>::Get()->PullKV(GenTokensMsgKey(peer_machine_id_, this_machine_id_),
                                    &peer_tokens_msg);
  for (int i = 0; i < Global<CommNetConfigDesc>::Get()->RegisterBufferNum(); i++) {
    send_recv_mem_desc_.emplace_back(mem_desc_[i].first, peer_tokens_msg.peer_recv_mem_desc(i));
  }
}

void IBVerbsQP::ClearKeyAndCreateHelper() {
  Global<CtrlClient>::Get()->ClearKV(GenTokensMsgKey(this_machine_id_, peer_machine_id_));
  helper_ = new IBVerbsHelper(send_recv_mem_desc_);
}

IBVerbsQP::~IBVerbsQP() {
  CHECK_EQ(ibv_destroy_qp(qp_), 0);
  delete helper_;
  while (send_msg_buf_.empty() == false) {
    delete send_msg_buf_.front();
    send_msg_buf_.pop();
  }
  for (MsgMR* msg_mr : recv_msg_buf_) { delete msg_mr; }
  // Free send/recv register memory
  for (auto mem_desc_item : mem_desc_) {
    delete mem_desc_item.first;
    delete mem_desc_item.second;
  }
}

void IBVerbsQP::Connect(const IBVerbsConnectionInfo& peer_info) {
  ibv_port_attr port_attr;
  CHECK_EQ(ibv_query_port(ctx_, 1, &port_attr), 0);
  ibv_qp_attr qp_attr;
  // IBV_QPS_INIT
  memset(&qp_attr, 0, sizeof(ibv_qp_attr));
  qp_attr.qp_state = IBV_QPS_INIT;
  qp_attr.pkey_index = 0;
  qp_attr.port_num = 1;
  qp_attr.qp_access_flags =
      IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_WRITE | IBV_ACCESS_REMOTE_READ;
  CHECK_EQ(ibv_modify_qp(qp_, &qp_attr,
                         IBV_QP_STATE | IBV_QP_PKEY_INDEX | IBV_QP_PORT | IBV_QP_ACCESS_FLAGS),
           0);
  // IBV_QPS_RTR
  memset(&qp_attr, 0, sizeof(ibv_qp_attr));
  qp_attr.qp_state = IBV_QPS_RTR;
  qp_attr.ah_attr.grh.dgid.global.subnet_prefix = peer_info.subnet_prefix();
  qp_attr.ah_attr.grh.dgid.global.interface_id = peer_info.interface_id();
  qp_attr.ah_attr.grh.flow_label = 0;
  qp_attr.ah_attr.grh.sgid_index = 0;
  qp_attr.ah_attr.grh.hop_limit = std::numeric_limits<uint8_t>::max();
  qp_attr.ah_attr.dlid = peer_info.lid();
  qp_attr.ah_attr.sl = 0;
  qp_attr.ah_attr.src_path_bits = 0;
  qp_attr.ah_attr.static_rate = 0;
  qp_attr.ah_attr.is_global = 1;
  qp_attr.ah_attr.port_num = 1;
  qp_attr.path_mtu = port_attr.active_mtu;
  qp_attr.dest_qp_num = peer_info.qp_num();
  qp_attr.rq_psn = 0;
  qp_attr.max_dest_rd_atomic = 1;
  qp_attr.min_rnr_timer = 12;
  CHECK_EQ(ibv_modify_qp(qp_, &qp_attr,
                         IBV_QP_STATE | IBV_QP_AV | IBV_QP_PATH_MTU | IBV_QP_DEST_QPN
                             | IBV_QP_RQ_PSN | IBV_QP_MAX_DEST_RD_ATOMIC | IBV_QP_MIN_RNR_TIMER),
           0);
  // IBV_QPS_RTS
  memset(&qp_attr, 0, sizeof(ibv_qp_attr));
  qp_attr.qp_state = IBV_QPS_RTS;
  qp_attr.sq_psn = 0;
  qp_attr.max_rd_atomic = 1;
  qp_attr.retry_cnt = 7;
  qp_attr.rnr_retry = 7;
  qp_attr.timeout = 14;
  CHECK_EQ(ibv_modify_qp(qp_, &qp_attr,
                         IBV_QP_STATE | IBV_QP_SQ_PSN | IBV_QP_MAX_QP_RD_ATOMIC | IBV_QP_RETRY_CNT
                             | IBV_QP_RNR_RETRY | IBV_QP_TIMEOUT),

           0);
}

void IBVerbsQP::PostAllRecvRequest() {
  for (MsgMR* msg_mr : recv_msg_buf_) { PostRecvRequest(msg_mr); }
}

void IBVerbsQP::PostWriteRequest(const IBVerbsMemDescProto& remote_mem,
                                 const IBVerbsMemDesc& local_mem, int32_t buffer_id,
                                 int32_t sge_num) {
  WorkRequestId* wr_id = NewWorkRequestId();
  wr_id->outstanding_sge_cnt = sge_num;
  FOR_RANGE(size_t, i, 0, sge_num) {
    ibv_send_wr wr;
    wr.wr_id = reinterpret_cast<uint64_t>(wr_id);
    wr.next = nullptr;
    wr.sg_list = const_cast<ibv_sge*>(&(local_mem.sge_vec().at(i)));
    wr.num_sge = 1;
    wr.opcode = IBV_WR_RDMA_WRITE_WITH_IMM;
    wr.send_flags = 0;
    if (i == sge_num - 1) {
      wr.imm_data = buffer_id;
    } else {
      wr.imm_data = INT_MAX;
    }
    wr.wr.rdma.remote_addr = remote_mem.mem_ptr(i);
    wr.wr.rdma.rkey = remote_mem.mr_rkey(i);
    ibv_send_wr* bad_wr = nullptr;
    CHECK_EQ(ibv_post_send(qp_, &wr, &bad_wr), 0);
  }
}

void IBVerbsQP::DealWorkRecord(WorkRecord& record, void* src_addr) {
  PleaseWrite please_write = {this_machine_id_, src_addr, record.bytes};
  Msg msg(please_write);
  {
    std::unique_lock<std::mutex> lock(read_queue_mtx_);
    read_queue_.push(record);
  }
  PostSendRequest(msg);
}

void IBVerbsQP::PostSendRequest(const Msg& msg) {
  int32_t msg_type_key = -1;
  const char* ptr = msg.ptr();
  size_t bytes = msg.bytes();
  std::function<void()> cb = msg.cb();
  switch (msg.msg_type()) {
    case (MsgType::kUserMsg): {
      const UserMsg* user_msg = reinterpret_cast<const UserMsg*>(ptr);
      CHECK_EQ(bytes, sizeof(*user_msg));
      msg_type_key =
          static_cast<int32_t>(MsgType::kNumType) + static_cast<int32_t>(user_msg->msg_type);
      ptr = user_msg->ptr;
      bytes = user_msg->bytes;
      break;
    }
    case (MsgType::kPleaseWrite): {
      const PleaseWrite* please_write = reinterpret_cast<const PleaseWrite*>(ptr);
      CHECK_EQ(bytes, sizeof(*please_write));
      msg_type_key = static_cast<int32_t>(MsgType::kPleaseWrite);
      break;
    }
    case (MsgType::kFreeBufferPair): {
      const FreeBufferPair* free_buffer_pair = reinterpret_cast<const FreeBufferPair*>(ptr);
      CHECK_EQ(bytes, sizeof(*free_buffer_pair));
      msg_type_key = static_cast<int32_t>(MsgType::kFreeBufferPair);
      bool last_piece = free_buffer_pair->last_piece;
      if (last_piece) {
        WorkRecord record;
        {
          std::unique_lock<std::mutex> lock(read_queue_mtx_);
          record = read_queue_.front();
          read_queue_.pop();
        }
        if (record.cb) { record.cb(); }
      }
      break;
    }
    default: {
      LOG(INFO) << "Unsupport send message type";
      break;
    }
  }
  CHECK(ptr);
  CHECK_GT(bytes, 0);
  MsgMR* msg_mr = GetOneSendMsgMRFromBuf();
  msg_mr->set_msg(msg_type_key, ptr, bytes, cb);
  WorkRequestId* wr_id = NewWorkRequestId();
  wr_id->msg_mr = msg_mr;
  ibv_send_wr wr;
  wr.wr_id = reinterpret_cast<uint64_t>(wr_id);
  wr.next = nullptr;
  wr.sg_list = const_cast<ibv_sge*>(&(msg_mr->mem_desc().sge_vec().at(0)));
  wr.num_sge = 1;
  wr.opcode = IBV_WR_SEND_WITH_IMM;
  wr.send_flags = 0;
  wr.imm_data = msg_type_key;
  memset(&(wr.wr), 0, sizeof(wr.wr));
  ibv_send_wr* bad_wr = nullptr;
  CHECK_EQ(ibv_post_send(qp_, &wr, &bad_wr), 0);
}

void IBVerbsQP::RDMARecvDone(WorkRequestId* wr_id, int32_t imm_data) {
  int32_t buffer_id = imm_data;
  if (buffer_id != INT_MAX) {
    IBVerbsMemDesc* recv_mem_desc = mem_desc_[buffer_id].second;
    helper_->SyncRead(peer_machine_id_, buffer_id, recv_mem_desc);
  }
  PostRecvRequest(wr_id->msg_mr);
  DeleteWorkRequestId(wr_id);
}

void IBVerbsQP::RDMAWriteDone(WorkRequestId* wr_id) {
  wr_id->outstanding_sge_cnt -= 1;
  if (wr_id->outstanding_sge_cnt == 0) { DeleteWorkRequestId(wr_id); }
}

void IBVerbsQP::SendDone(WorkRequestId* wr_id) {
  // Invoke callback function when the message is user-defined
  if (wr_id->msg_mr->user_msg()) {
    auto cb = wr_id->msg_mr->user_cb();
    if (cb) { cb(); }
  }
  {
    std::unique_lock<std::mutex> lck(send_msg_buf_mtx_);
    send_msg_buf_.push(wr_id->msg_mr);
  }
  DeleteWorkRequestId(wr_id);
}

void IBVerbsQP::RecvDone(WorkRequestId* wr_id, int32_t msg_type_key) {
  char* ptr = wr_id->msg_mr->msg_ptr();
  size_t msg_bytes = wr_id->msg_mr->msg_bytes();
  if (msg_type_key < static_cast<int32_t>(MsgType::kNumType)) {
    MsgType msg_type = static_cast<MsgType>(msg_type_key);
    switch (msg_type) {
      case (MsgType::kPleaseWrite): {
        PleaseWrite* please_write = reinterpret_cast<PleaseWrite*>(ptr);
        CHECK_EQ(msg_bytes, sizeof(*please_write));
        void* src_addr = please_write->src_addr;
        size_t bytes = please_write->bytes;
        size_t dst_machine_id = please_write->dst_machine_id;
        // use write helper to write message
        WorkRecord record(dst_machine_id, src_addr, bytes, 0);
        helper_->AsyncWrite(record);
        break;
      }
      case (MsgType::kFreeBufferPair): {
        FreeBufferPair* free_buffer_pair = reinterpret_cast<FreeBufferPair*>(ptr);
        CHECK_EQ(msg_bytes, sizeof(*free_buffer_pair));
        int32_t buffer_id = free_buffer_pair->buffer_id;
        helper_->FreeBuffer(buffer_id);
        if (free_buffer_pair->last_piece) {
          std::function<void()> cb;
          {
            std::unique_lock<std::mutex> lock(read_done_cbs_mtx_);
            cb = std::move(read_done_cbs_.front());
            read_done_cbs_.pop();
          }
          CHECK(cb);
          cb();
        }
        break;
      }
      default: {
        LOG(INFO) << "Unsupport receive message type";
        break;
      }
    }
  } else {
    // user-defined message
    int32_t user_msg_key = msg_type_key - static_cast<int32_t>(MsgType::kNumType);
    auto handler = Global<IBVerbsCommNet>::Get()->GetMsgHandler(user_msg_key);
    CHECK(handler);
    handler(ptr, msg_bytes);
  }
  PostRecvRequest(wr_id->msg_mr);
  DeleteWorkRequestId(wr_id);
}

void IBVerbsQP::PostRecvRequest(MsgMR* msg_mr) {
  WorkRequestId* wr_id = NewWorkRequestId();
  wr_id->msg_mr = msg_mr;
  ibv_recv_wr wr;
  wr.wr_id = reinterpret_cast<uint64_t>(wr_id);
  wr.next = nullptr;
  wr.sg_list = const_cast<ibv_sge*>(&(msg_mr->mem_desc().sge_vec().at(0)));
  wr.num_sge = 1;
  ibv_recv_wr* bad_wr = nullptr;
  CHECK_EQ(ibv_post_recv(qp_, &wr, &bad_wr), 0);
}

MsgMR* IBVerbsQP::GetOneSendMsgMRFromBuf() {
  std::unique_lock<std::mutex> lck(send_msg_buf_mtx_);
  if (send_msg_buf_.empty()) { send_msg_buf_.push(new MsgMR(pd_)); }
  MsgMR* msg_mr = send_msg_buf_.front();
  send_msg_buf_.pop();
  return msg_mr;
}

WorkRequestId* IBVerbsQP::NewWorkRequestId() {
  WorkRequestId* wr_id = new WorkRequestId;
  wr_id->qp = this;
  wr_id->outstanding_sge_cnt = 0;
  wr_id->msg_mr = nullptr;
  return wr_id;
}

void IBVerbsQP::DeleteWorkRequestId(WorkRequestId* wr_id) {
  CHECK_EQ(wr_id->qp, this);
  delete wr_id;
}

WorkRecord IBVerbsQP::GetWorkRecord() {
  std::unique_lock<std::mutex> lock(read_queue_mtx_);
  return read_queue_.front();
}

void IBVerbsQP::SetWorkRecordOffset(size_t offset) {
  std::unique_lock<std::mutex> lock(read_queue_mtx_);
  read_queue_.front().offset = offset;
}

void IBVerbsQP::RegisterReadDoneCb(std::function<void()> cb) {
  std::unique_lock<std::mutex> lock(read_done_cbs_mtx_);
  read_done_cbs_.push(std::move(cb));
}

}  // namespace comm_network
