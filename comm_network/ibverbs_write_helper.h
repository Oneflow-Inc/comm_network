#pragma once
#include "comm_network/common/utils.h"
#include "comm_network/ibverbs_poller.h"
#include "comm_network/message.h"

namespace comm_network {
class IBVerbsWriteHelper final {
 public:
  CN_DISALLOW_COPY_AND_MOVE(IBVerbsWriteHelper);
  ~IBVerbsWriteHelper();
  IBVerbsWriteHelper();

  void AsyncWrite(const Msg& msg);
  void FreeBuffer(uint8_t buffer_id);
  void LoopProcess();
  void NotifyMeToWrite();
  
 private:
  std::thread thread_;
  bool thread_is_busy_;
  std::queue<Msg>* pending_msg_queue_;
  std::mutex pending_msg_queue_mtx_;
  std::queue<Msg>* cur_msg_queue_;
  std::queue<uint8_t> idle_buffer_queue_;
  std::mutex idle_buffer_queue_mtx_;
  std::condition_variable idle_buffer_queue_cv_;
};
}