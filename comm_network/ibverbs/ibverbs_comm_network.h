#pragma once
#include "comm_network/comm_network.h"
#include "comm_network/ibverbs/ibverbs_qp.h"

namespace comm_network {
class IBVerbsCommNet final : public CommNet {
 public:
  CN_DISALLOW_COPY_AND_MOVE(IBVerbsCommNet);
  IBVerbsCommNet() = default;
  ~IBVerbsCommNet() = default;
  void SetUp();
  void TearDown();
  void SendMsg(int64_t dst_machine_id, int32_t msg_type, const char* ptr, size_t bytes,
               std::function<void()> cb = NULL);
  void DoRead(int64_t src_machine_id, void* src_addr, size_t bytes, void* dst_addr,
              std::function<void()> cb = NULL);
  void RegisterReadDoneCb(int64_t dst_machine_id, std::function<void()> cb);
  void Normal2RegisterDone(int64_t dst_machine_id, IBVerbsMemDesc* send_mem_desc,
                           IBVerbsMemDescProto recv_mem_desc_proto, int32_t buffer_id,
                           int32_t sge_num);
  void Register2NormalDone(int64_t machine_id, int32_t buffer_id, bool last_piece);
  WorkRecord GetWorkRecord(int64_t machine_id);
  void SetWorkRecordOffset(int64_t machine_id, size_t offset);

 private:
  std::vector<IBVerbsQP*> qp_vec_;
  ibv_context* context_;
  ibv_pd* pd_;
  std::vector<ibv_cq*> cq_vec_;
};
}  // namespace comm_network
