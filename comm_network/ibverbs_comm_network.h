#pragma once
#include "comm_network/ibverbs_memory_desc.h"
#include "comm_network/env_desc.h"
#include "comm_network/ibverbs_qp.h"
#include "comm_network/control/ctrl_client.h"
#include "comm_network/control/ctrl_server.h"
#include "comm_network/ibverbs_helper.h"
#include "comm_network/ibverbs_poller.h"
#include "comm_network/common/channel.h"

namespace comm_network {
class IBVerbsCommNet final {
 public:
  CN_DISALLOW_COPY_AND_MOVE(IBVerbsCommNet);
  IBVerbsCommNet(Channel<Msg>* action_channel);
  ~IBVerbsCommNet();

  const std::unordered_set<int64_t>& peer_machine_id() { return peer_machine_id_; }
  void RegisterFixNumMemory();
  void UnRegisterFixNumMemory();

  std::pair<IBVerbsMemDesc*, IBVerbsMemDescProto> GetSendRecvMemPairForSender(int64_t machine_id,
                                                                              uint8_t buffer_id);
  WorkRecord GetWorkRecord(uint32_t read_id) {
    CHECK(read_queue_.find(read_id) != read_queue_.end());
    return read_queue_.at(read_id);
  }
  void SetWorkRecordOffset(uint32_t read_id, size_t offset) {
    CHECK(read_queue_.find(read_id) != read_queue_.end());
    read_queue_.at(read_id).offset = offset;
  }
  IBVerbsMemDesc* GetRecvMemDescForReceiver(int64_t machine_id, uint8_t buffer_id);

  void DoRead(int64_t src_machine_id, void* src_addr, void* dst_addr, size_t data_size,
              std::function<void()> callback);
  void SendMsg(int64_t dst_machine_id, const Msg& msg);
  void SendToChannel(const Msg& msg) { action_channel_->Send(msg); }
  void Normal2RegisterDone(int64_t dst_machine_id, IBVerbsMemDesc* send_mem_desc,
                           IBVerbsMemDescProto recv_mem_desc_proto, uint32_t imm_data);
  void Register2NormalDone(int64_t machine_id, uint8_t buffer_id, uint32_t read_id,
                           bool last_piece);

 private:
  void* RegisterMemory(std::string key, void* ptr, size_t byte_size);
  void UnRegisterMemory(std::string key);
  uint32_t AllocateReadId();
  void FreeReadId(uint32_t read_id);
  std::unordered_set<int64_t> peer_machine_id_;
  std::vector<IBVerbsQP*> qp_vec_;
  std::vector<IBVerbsHelper*> helper_vec_;
  ibv_context* context_;
  ibv_pd* pd_;
  ibv_cq* cq_;
  Channel<Msg>* action_channel_;
  int64_t this_machine_id_;
  std::unordered_map<std::string, IBVerbsMemDesc*> mem_desc_;
  std::vector<std::unordered_map<std::string, IBVerbsMemDescProto>> mem_desc_list_;
  std::unordered_set<uint32_t> busy_read_ids_;
  std::mutex busy_read_id_mtx_;
  std::unordered_map<uint32_t, WorkRecord> read_queue_;
  IBVerbsPoller* poller_;
};
}  // namespace comm_network
