#pragma once
#include "comm_network/ibverbs_memory_desc.h"
#include "comm_network/env_desc.h"
#include "comm_network/ibverbs_qp.h"
#include "comm_network/control/ctrl_client.h"
#include "comm_network/control/ctrl_server.h"
#include "comm_network/ibverbs_read_helper.h"
#include "comm_network/ibverbs_write_helper.h"

namespace comm_network {
class IBVerbsCommNet final {
 public:
  DISALLOW_COPY_AND_MOVE(IBVerbsCommNet);
  IBVerbsCommNet(Channel<Msg> *action_channel);
  ~IBVerbsCommNet();

  const std::unordered_set<int64_t>& peer_machine_id() { return peer_machine_id_; }
  void RegisterFixNumMemory();
  void UnRegisterFixNumMemory();

  bool FindAvailSendMemory(int64_t src_machine_id, int64_t dst_machine_id, std::string& send_key, std::string& recv_key, int& buffer_id);
  const std::unordered_map<std::string, std::pair<IBVerbsMemDesc*, bool>>& mem_desc() { return mem_desc_; }
  const std::vector<std::unordered_map<std::string, IBVerbsMemDescProto>> mem_desc_list() { return mem_desc_list_; }

	void AsyncWrite(int64_t src_machine_id, int64_t dst_machine_id, void* src_addr, void* dst_addr, size_t data_size);
  void AddReadCallBack(void* read_id, std::function<void()> callback);
  void SendMsg(int64_t dst_machine_id, const Msg& msg);
  void Register2NormalMemory(const Msg& recv_msg);
  void ReleaseBuffer(int64_t src_machine_id, int64_t dst_machine_id, int buffer_id);

 private:
  void* RegisterMemory(std::string key, void* ptr, size_t byte_size);
  void UnRegisterMemory(std::string key);
  void PollCQ();
  void PollQueue();
  static const int32_t max_poll_wc_num_;
  std::unordered_set<int64_t> peer_machine_id_;
  std::vector<IBVerbsQP*> qp_vec_;
  ibv_context* context_;
  ibv_pd* pd_;
  ibv_cq* cq_;
  std::thread poll_thread_;
  std::thread poll_channel_thread_;
  Channel<WritePartial> write_channel_;
  std::atomic_flag poll_exit_flag_;
  Channel<Msg> *action_channel_;
  int64_t this_machine_id_;
  std::unordered_map<std::string, std::pair<IBVerbsMemDesc*, bool>> mem_desc_; 
  std::vector<std::unordered_map<std::string, IBVerbsMemDescProto>> mem_desc_list_;
  IBVerbsReadHelper* read_helper_;
  IBVerbsWriteHelper* write_helper_; 
};
}  // namespace comm_network
