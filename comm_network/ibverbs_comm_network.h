#pragma once
#include "comm_network/ibverbs_memory_desc.h"
#include "comm_network/env_desc.h"
#include "comm_network/ibverbs_qp.h"
#include "comm_network/control/ctrl_client.h"
#include "comm_network/msg_bus.h"

namespace comm_network {
class IBVerbsCommNet final {
 public:
  DISALLOW_COPY_AND_MOVE(IBVerbsCommNet);
  IBVerbsCommNet(CtrlClient* ctrl_client, MsgBus* msg_bus, int64_t this_machine_id);
  ~IBVerbsCommNet();

  const std::unordered_set<int64_t>& peer_machine_id() { return peer_machine_id_; }
  void* RegisterMemory(void* ptr, size_t byte_size);
  void UnRegisterMemory(void* token);
  void RegisterMemoryDone();

  void Read(void* read_id, int64_t src_machine_id, void* src_token, void* dst_token);
  void AddReadCallBack(void* read_id, std::function<void()> callback);
  void ReadDone(void* read_id);
  void SendMsg(int64_t dst_machine_id, const Msg& msg);

 private:
  void PollCQ();

  static const int32_t max_poll_wc_num_;
  std::unordered_set<int64_t> peer_machine_id_;
  std::vector<IBVerbsQP*> qp_vec_;
  ibv_context* context_;
  ibv_pd* pd_;
  ibv_cq* cq_;
  std::thread poll_thread_;
  std::atomic_flag poll_exit_flag_;
  int64_t this_machine_id_;
  CtrlClient* ctrl_client_;
	MsgBus* msg_bus_;
};
}  // namespace comm_network
