#pragma once
#include <mutex>
#include <thread>
#include <string>
#include <unordered_set>
#include "comm_network/message.h"
#include "comm_network/env_desc.h"

namespace comm_network {
class IBVerbsCommNet final {
 public:
  IBVerbsCommNet(const IBVerbsCommNet&) = delete;
  IBVerbsCommNet& operator=(const IBVerbsCommNet&) = delete;
  IBVerbsCommNet(IBVerbsCommNet&&) = delete;
  IBVerbsCommNet& operator=(IBVerbsCommNet&&) = delete;
  IBVerbsCommNet(EnvDesc& env_desc);
  ~IBVerbsCommNet();

  void* RegisterMemory(void* ptr, size_t byte_size);
  void UnRegisterMemory(void* token);
  void RegisterMemoryDone();

  void Read(void* read_id, int64_t src_machine_id, void* src_token, void* dst_token);
  void AddReadCallBack(void* read_id, std::function<void()> callback);
  void ReadDone(void* read_id);
  void SendMsg(int64_t dst_machine_id, const Msg& msg);

  static void set_env_cfg(int64_t id, std::string addr);

 private:
  std::unordered_set<int64_t> peer_machine_id_;
};
}  // namespace comm_network