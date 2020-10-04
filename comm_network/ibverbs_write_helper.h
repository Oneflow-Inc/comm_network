#pragma once
#include "comm_network/utils.h"

namespace comm_network {
class IBVerbsWriteHelper {
 public:
  IBVerbsWriteHelper();
  ~IBVerbsWriteHelper();
  void AddWork(int64_t src_machine_id, int64_t dst_machine_id, char* src_addr, char* dst_addr, size_t data_size);
 private:
  struct WritePartial {
    char* src_addr;
    char* dst_addr;
    size_t data_size;
    int64_t src_machine_id;
    int64_t dst_machine_id;
  };
  void PollQueue();
  std::queue<WritePartial> msg_queue_;
  std::mutex msg_queue_mtx_;
  std::thread thread_;
  std::atomic_flag poll_exit_flag_;
};
}