#pragma once
#include "comm_network/common/utils.h"
#include "comm_network/ibverbs/ibverbs_poller.h"
#include "comm_network/message.h"

namespace comm_network {
class IBVerbsWriteHelper final {
 public:
  CN_DISALLOW_COPY_AND_MOVE(IBVerbsWriteHelper);
  ~IBVerbsWriteHelper();
  IBVerbsWriteHelper();

  void AsyncWrite(const WorkRecord& record);
  void FreeBuffer(uint8_t buffer_id);

 private:
  bool InitWriteHandle();
  bool RequestBuffer();
  bool DoCurWrite();
  void NotifyMeToWrite();
  void WriteUntilQueueEmptyOrNoBuffer();
  uint8_t buffer_id_;
  std::queue<WorkRecord>* pending_record_queue_;
  std::mutex pending_record_queue_mtx_;
  std::queue<WorkRecord>* cur_record_queue_;
  std::queue<uint8_t> idle_buffer_queue_;
  std::mutex idle_buffer_queue_mtx_;
  WorkRecord cur_record_;
  bool (IBVerbsWriteHelper::*cur_write_handle_)();
};

}  // namespace comm_network
