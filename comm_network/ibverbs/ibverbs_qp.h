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
    // Previous check configuration
    size_t sge_bytes = Global<CommNetConfigDesc>::Get()->SgeBytes();
    CHECK_LE(sizeof(msg_), sge_bytes);
    mem_desc_.reset(new IBVerbsMemDesc(pd, &msg_, sizeof(msg_)));
    CHECK_EQ(mem_desc_->sge_vec().size(), 1);
  }
  ~MsgMR() { mem_desc_.reset(); }

  const Msg& msg() const { return msg_; }
  void set_msg(const Msg& val) { msg_ = val; }
  const IBVerbsMemDesc& mem_desc() const { return *mem_desc_; }

 private:
  Msg msg_;
  std::unique_ptr<IBVerbsMemDesc> mem_desc_;
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
  void RecvDone(WorkRequestId*);
  void RDMAWriteDone(WorkRequestId*);
  void PushRegisterMemoryKey();
  void PullRegisterMemoryKey();
  void ClearKeyAndCreateHelper();

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
};
}  // namespace comm_network