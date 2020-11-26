#pragma once
#include "comm_network/ibverbs/ibverbs_memory_desc.h"
#include "comm_network/message.h"
#include "comm_network/ibverbs/ibverbs_helper.h"

namespace comm_network {

class MsgMR final {
 public:
  CN_DISALLOW_COPY_AND_MOVE(MsgMR);
  MsgMR() = delete;
  MsgMR(ibv_pd* pd) {
    // Register memory, it's user's responsibility to configure a suitable maximum message bytes
    size_t max_msg_bytes = Global<CommNetConfigDesc>::Get()->MaxMsgBytes();
    ptr_ = (char*)malloc(max_msg_bytes);
    memset(ptr_, 0, max_msg_bytes);
    mem_desc_.reset(new IBVerbsMemDesc(pd, ptr_, max_msg_bytes));
    CHECK_EQ(mem_desc_->sge_vec().size(), 1);
  }
  ~MsgMR() {
    // No need to free(ptr_), due to the memory of ptr_ points to
    // now change to a register memory and managed by mem_desc
    mem_desc_.reset();
  }
  const IBVerbsMemDesc& mem_desc() const { return *mem_desc_; }
  void set_msg(int32_t msg_type_key, const char* ptr, size_t bytes, std::function<void()> cb) {
    // Check for overflow bytes for message
    size_t max_msg_bytes = Global<CommNetConfigDesc>::Get()->MaxMsgBytes();
    CHECK_LE(bytes, max_msg_bytes - sizeof(size_t));
    memcpy(ptr_, &bytes, sizeof(size_t));
    memcpy(ptr_ + sizeof(size_t), ptr, bytes);
    msg_type_key_ = msg_type_key;
    if (cb) {
      cb_ = std::move(cb);
    } else {
      cb_ = NULL;
    }
  }
  char* msg_ptr() { return ptr_ + sizeof(size_t); }
  size_t msg_bytes() {
    size_t bytes = 0;
    memcpy(&bytes, ptr_, sizeof(size_t));
    return bytes;
  }
  bool user_msg() const { return (msg_type_key_ >= static_cast<int32_t>(MsgType::kNumType)); }
  std::function<void()> user_cb() const { return cb_; }

 private:
  std::unique_ptr<IBVerbsMemDesc> mem_desc_;
  char* ptr_;
  int32_t msg_type_key_;
  std::function<void()> cb_;
};

class IBVerbsQP;

struct WorkRequestId {
  IBVerbsQP* qp;
  int32_t outstanding_sge_cnt;
  MsgMR* msg_mr;
};

class IBVerbsQP final {
 public:
  CN_DISALLOW_COPY_AND_MOVE(IBVerbsQP);
  IBVerbsQP() = delete;
  IBVerbsQP(ibv_context*, ibv_pd*, ibv_cq* send_cq, ibv_cq* recv_cq, int64_t this_machine_id,
            int64_t peer_machine_id);
  ~IBVerbsQP();

  uint32_t qp_num() const { return qp_->qp_num; }
  void Connect(const IBVerbsConnectionInfo& peer_info);
  void PostAllRecvRequest();

  void PostWriteRequest(const IBVerbsMemDescProto& remote_mem, const IBVerbsMemDesc& local_mem,
                        int32_t buffer_id, int32_t sge_num);
  void PostSendRequest(const Msg& msg);

  void RDMARecvDone(WorkRequestId*, int32_t buffer_id);
  void SendDone(WorkRequestId*);
  void RecvDone(WorkRequestId*, int32_t msg_type_key);
  void RDMAWriteDone(WorkRequestId*);
  void PushRegisterMemoryKey();
  void PullRegisterMemoryKey();
  void ClearKeyAndCreateHelper();
  void DealWorkRecord(WorkRecord& record, void* src_addr);
  WorkRecord GetWorkRecord();
  void SetWorkRecordOffset(size_t offset);

 private:
  WorkRequestId* NewWorkRequestId();
  void DeleteWorkRequestId(WorkRequestId* wr_id);
  MsgMR* GetOneSendMsgMRFromBuf();
  void PostRecvRequest(MsgMR*);

  ibv_context* ctx_;
  ibv_pd* pd_;
  ibv_qp* qp_;
  std::vector<MsgMR*> recv_msg_buf_;

  std::mutex send_msg_buf_mtx_;
  std::queue<MsgMR*> send_msg_buf_;
  IBVerbsHelper* helper_;
  int64_t this_machine_id_;
  int64_t peer_machine_id_;
  std::vector<std::pair<IBVerbsMemDesc*, IBVerbsMemDesc*>> mem_desc_;
  std::vector<std::pair<IBVerbsMemDesc*, IBVerbsMemDescProto>> send_recv_mem_desc_;
  std::mutex read_queue_mtx_;
  std::queue<WorkRecord> read_queue_;
};
}  // namespace comm_network