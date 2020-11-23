#pragma once
#include "comm_network/comm_network.h"
#include "comm_network/ibverbs/ibverbs_qp.h"
#include "comm_network/ibverbs/ibverbs_poller.h"

namespace comm_network {
class IBVerbsCommNet final : public CommNet {
 public:
  CN_DISALLOW_COPY_AND_MOVE(IBVerbsCommNet);
  IBVerbsCommNet() = default;
  ~IBVerbsCommNet() = default;
  void SetUp();
  void TearDown();
  void SendMsg(int64_t dst_machine_id, int64_t msg_type, const char* ptr, size_t bytes,
               std::function<void()> cb = NULL);
  void DoRead(int64_t src_machine_id, void* src_addr, size_t bytes, void* dst_addr,
              std::function<void()> cb = NULL);
  void Normal2RegisterDone(int64_t dst_machine_id, IBVerbsMemDesc* send_mem_desc,
                           IBVerbsMemDescProto recv_mem_desc_proto, int32_t buffer_id,
                           int32_t sge_num);
  void Register2NormalDone(int64_t machine_id, int32_t buffer_id, bool last_piece);

  WorkRecord GetWorkRecord() {
    std::unique_lock<std::mutex> lock(read_queue_mtx_);
    return read_queue_.front();
  }

  void SetWorkRecordOffset(size_t offset) {
    std::unique_lock<std::mutex> lock(read_queue_mtx_);
    read_queue_.front().offset = offset;
  }

  std::function<void()> UserSendMsgCb();

 private:
  std::vector<IBVerbsQP*> qp_vec_;
  ibv_context* context_;
  ibv_pd* pd_;
  std::vector<ibv_cq*> cq_vec_;
  std::mutex read_queue_mtx_;
  std::queue<WorkRecord> read_queue_;
  std::mutex cb_queue_mtx_;
  std::queue<std::function<void()>> cb_queue_;
};
}  // namespace comm_network
