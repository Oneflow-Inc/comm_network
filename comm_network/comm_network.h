#pragma once
#include "comm_network/common/utils.h"
#include "comm_network/message.h"

namespace comm_network {

class CommNet {
 public:
  CN_DISALLOW_COPY_AND_MOVE(CommNet);
  virtual ~CommNet() = default;
  virtual void SendMsg(int64_t dst_machine_id, Msg& msg, std::function<void()> callback = nullptr) = 0;
  virtual void DoRead(uint32_t read_id, int64_t src_machine_id, void* src_addr, void* dst_addr, size_t data_size,
                 std::function<void()> callback = nullptr) = 0;
  const std::unordered_set<int64_t>& peer_machine_id() { return peer_machine_id_; }
  void BindCbHandler(std::function<void(const Msg&)>);
  void ExecuteCallBack(const Msg& msg) { 
    CHECK(cb_);
    cb_(msg); 
  }

 protected:
  CommNet();
  void AddUnfinishedDataReadyItem(const Msg& msg, std::function<void()> cb);
  uint32_t AllocateReadId();
  void FreeReadId(uint32_t read_id);
  std::unordered_set<int64_t> peer_machine_id_;
  int64_t this_machine_id_;
  std::mutex callback_queue_mtx_;
  std::unordered_map<uint32_t, std::function<void()>> callback_queue_;
  std::unordered_set<uint32_t> busy_read_ids_;
  std::mutex busy_read_id_mtx_;

 private:
  std::function<void(const Msg&)> cb_;
};
}  // namespace comm_network

