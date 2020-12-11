#pragma once
#include "comm_network/comm_network.h"

namespace comm_network {
class EpollCommNet final : public CommNet {
 public:
  EpollCommNet() {}
  ~EpollCommNet() {}
  void SendMsg(int64_t dst_machine_id, int32_t msg_type, const char* ptr, size_t bytes,
               std::function<void()> handler = NULL) {}
  void DoRead(int64_t src_machine_id, void* src_addr, size_t bytes, void* dst_addr,
              std::function<void()> handler = NULL) {}
  void RegisterReadDoneCb(int64_t dst_machine_id, std::function<void()> handler) {}
  void SetUp() {}
  void TearDown() {}

 private:
};
}  // namespace comm_network
