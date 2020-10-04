#include "comm_network/ibverbs_write_helper.h"

namespace comm_network {
IBVerbsWriteHelper::IBVerbsWriteHelper() : poll_exit_flag_(ATOMIC_FLAG_INIT) {
  thread_ = std::thread(&IBVerbsWriteHelper::PollQueue, this);
}

IBVerbsWriteHelper::~IBVerbsWriteHelper() {
  while (poll_exit_flag_.test_and_set() == true) {}
  CHECK(msg_queue_.empty());
  thread_.join();
}

void IBVerbsWriteHelper::AddWork(int64_t src_machine_id, int64_t dst_machine_id, char* src_addr, char* dst_addr, size_t data_size) {
  WritePartial write_partial;
  write_partial.src_machine_id = src_machine_id;
  write_partial.dst_machine_id = dst_machine_id;
  write_partial.src_addr = src_addr;
  write_partial.dst_addr = dst_addr;
  write_partial.data_size = data_size;
  std::unique_lock<std::mutex> lck(msg_queue_mtx_);
  msg_queue_.push(write_partial);
}

void IBVerbsWriteHelper::PollQueue() {
  while (poll_exit_flag_.test_and_set() == false) {
    poll_exit_flag_.clear();
    // check for free register buffer

    // std::unique_lock<std::mutex> lck(msg_queue_mtx_);
    // if (!msg_queue_.empty()) {
    //   WritePartial write_partial = msg_queue_.front(); 
      
    // }
  }
}

}