#pragma once
#include "comm_network/ibverbs_memory_desc.h"
#include "comm_network/message.h"
#include "comm_network/ibverbs_helper.h"

namespace comm_network {
class MsgMR final {
 public:
  CN_DISALLOW_COPY_AND_MOVE(MsgMR);
  MsgMR() = delete;
  MsgMR(ibv_pd* pd) {
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
  IBVerbsQP(ibv_context*, ibv_pd*, ibv_cq* send_cq, ibv_cq* recv_cq, IBVerbsHelper* helper);
  ~IBVerbsQP();

  uint32_t qp_num() const { return qp_->qp_num; }
  void Connect(const IBVerbsConnectionInfo& peer_info);
  void PostAllRecvRequest();

  void PostWriteRequest(const IBVerbsMemDescProto& remote_mem, const IBVerbsMemDesc& local_mem,
                        uint32_t imm_data);
  void PostSendRequest(const Msg& msg);

  void WriteDone(WorkRequestId*, uint32_t imm_data);
  void SendDone(WorkRequestId*);
  void RecvDone(WorkRequestId*);

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
};
}  // namespace comm_network
