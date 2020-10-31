#pragma once
#include "comm_network/comm_network.h"
#include "comm_network/ibverbs/ibverbs_memory_desc.h"
#include "comm_network/env_desc.h"
#include "comm_network/ibverbs/ibverbs_qp.h"
#include "comm_network/control/ctrl_client.h"
#include "comm_network/control/ctrl_server.h"
#include "comm_network/ibverbs/ibverbs_helper.h"
#include "comm_network/ibverbs/ibverbs_poller.h"
#include "comm_network/common/channel.h"

namespace comm_network {
class IBVerbsCommNet final : public CommNet {
 public:
  CN_DISALLOW_COPY_AND_MOVE(IBVerbsCommNet);
  IBVerbsCommNet();
  ~IBVerbsCommNet();

  void RegisterFixNumMemory();
  void UnRegisterFixNumMemory();

  std::pair<IBVerbsMemDesc*, IBVerbsMemDescProto> GetSendRecvMemPairForSender(int64_t machine_id,
                                                                              uint8_t buffer_id);
  WorkRecord GetWorkRecord(uint32_t read_id) {
    std::unique_lock<std::mutex> lock(read_queue_mtx_);
    CHECK(read_queue_.find(read_id) != read_queue_.end());
    return read_queue_.at(read_id);
  }
  void SetWorkRecordOffset(uint32_t read_id, size_t offset) {
    std::unique_lock<std::mutex> lock(read_queue_mtx_);
    CHECK(read_queue_.find(read_id) != read_queue_.end());
    read_queue_.at(read_id).offset = offset;
  }
  IBVerbsMemDesc* GetRecvMemDescForReceiver(int64_t machine_id, uint8_t buffer_id);

  void DoRead(uint32_t read_id, int64_t src_machine_id, void* src_addr, void* dst_addr, size_t data_size,
              std::function<void()> callback = nullptr);
  void SendMsg(int64_t dst_machine_id, Msg& msg, std::function<void()> callback = nullptr);
  void Normal2RegisterDone(int64_t dst_machine_id, IBVerbsMemDesc* send_mem_desc,
                           IBVerbsMemDescProto recv_mem_desc_proto, uint32_t imm_data);
  void Register2NormalDone(int64_t machine_id, uint8_t buffer_id, uint32_t read_id,
                           bool last_piece);
  void SendLargeDataDone(uint32_t read_id);

 private:
  void* RegisterMemory(std::string key, void* ptr, size_t byte_size);
  void UnRegisterMemory(std::string key);
  std::vector<IBVerbsQP*> qp_vec_;
  std::vector<IBVerbsHelper*> helper_vec_;
  ibv_context* context_;
  ibv_pd* pd_;
  ibv_cq* cq_;
  std::unordered_map<std::string, IBVerbsMemDesc*> mem_desc_;
  std::vector<std::unordered_map<std::string, IBVerbsMemDescProto>> mem_desc_list_;
  std::mutex read_queue_mtx_;
  std::unordered_map<uint32_t, WorkRecord> read_queue_;
  IBVerbsPoller* poller_;
};
}  // namespace comm_network
