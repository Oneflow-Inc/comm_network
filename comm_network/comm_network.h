#pragma once
#include "comm_network/common/utils.h"
#include "comm_network/comm_network_config_desc.h"

namespace comm_network {

// Abstract poller design
class GenericPoller {
 public:
  CN_DISALLOW_COPY_AND_MOVE(GenericPoller);
  GenericPoller() = default;
  virtual ~GenericPoller() = default;
  virtual void Start() = 0;
  virtual void Stop() = 0;
};

class CommNet {
 public:
  CN_DISALLOW_COPY_AND_MOVE(CommNet);
  CommNet() = default;
  virtual ~CommNet() = default;
  // Interface for outside users to initialize environment
  void Init();
  // Interface for outside users to finalize environment
  void Finalize();
  int64_t ThisMachineId() { return this_machine_id_; }
  // Interface for outside users to register self-defined message handler
  // With implicit barrier to ensure all machines have successful register
  // this message handler
  void RegisterMsgHandler(int32_t msg_type,
                          std::function<void(const char* ptr, size_t bytes)> handler);
  std::function<void(const char* ptr, size_t bytes)> GetMsgHandler(int32_t msg_type);
  virtual void SendMsg(int64_t dst_machine_id, int32_t msg_type, const char* ptr, size_t bytes,
                       std::function<void()> cb = NULL) = 0;
  virtual void DoRead(int64_t src_machine_id, void* src_addr, size_t bytes, void* dst_addr,
                      std::function<void()> cb = NULL) = 0;
  virtual void RegisterReadDoneCb(int64_t dst_machine_id, std::function<void()> cb) = 0;

 protected:
  std::vector<GenericPoller*> poller_vec_;
  std::unordered_set<int64_t> peer_machine_id_;
  std::unordered_map<int32_t, std::function<void(const char* ptr, size_t bytes)>> msg_handlers_;
  virtual void SetUp() = 0;
  virtual void TearDown() = 0;
  void AddPoller(GenericPoller* poller) { poller_vec_.emplace_back(poller); }

 private:
  int64_t this_machine_id_;
};

// Set up CommNet environment, return the CommNet handler
// to invoke variety supported methods.
CommNet* SetUpCommNet(const CommNetConfig& comm_net_config);
}  // namespace comm_network
