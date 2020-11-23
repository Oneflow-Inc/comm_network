#pragma once
#include "comm_network/common/utils.h"
#include "comm_network/ibverbs/ibverbs_poller.h"
#include "comm_network/message.h"
#include "comm_network/ibverbs/ibverbs_memory_desc.h"

namespace comm_network {
class IBVerbsWriteHelper final {
 public:
  CN_DISALLOW_COPY_AND_MOVE(IBVerbsWriteHelper);
  ~IBVerbsWriteHelper();
  IBVerbsWriteHelper(
      const std::vector<std::pair<IBVerbsMemDesc*, IBVerbsMemDescProto>>& send_recv_pair);

  void AsyncWrite(const WorkRecord& record);
  void FreeBuffer(int32_t buffer_id);

 private:
  bool InitWriteHandle();
  bool RequestBuffer();
  bool DoCurWrite();
  void NotifyMeToWrite();
  void WriteUntilQueueEmptyOrNoBuffer();
  int32_t buffer_id_;
  std::queue<WorkRecord>* pending_record_queue_;
  std::mutex pending_record_queue_mtx_;
  std::queue<WorkRecord>* cur_record_queue_;
  std::queue<int32_t> idle_buffer_queue_;
  std::mutex idle_buffer_queue_mtx_;
  WorkRecord cur_record_;
  bool (IBVerbsWriteHelper::*cur_write_handle_)();
  std::vector<std::pair<IBVerbsMemDesc*, IBVerbsMemDescProto>> send_recv_pair_;
};

}  // namespace comm_network