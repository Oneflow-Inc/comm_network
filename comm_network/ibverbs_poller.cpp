#include "comm_network/ibverbs_poller.h"

namespace comm_network {

IBVerbsPoller::IBVerbsPoller() {
  read_helper_ = new IBVerbsReadHelper();
  write_helper_ = new IBVerbsWriteHelper();
}

void IBVerbsPoller::Start() {
  
}

void IBVerbsPoller::Stop() {
  
}

void IBVerbsPoller::AsyncWrite(int64_t src_machine_id, int64_t dst_machine_id, void* src_addr, void* dst_addr, size_t data_size) {
  size_t size = 0;
  char* src_ptr = reinterpret_cast<char*>(src_addr);
  char* dst_ptr = reinterpret_cast<char*>(dst_addr); 
  while (size < data_size) {
    size_t transfer_size = std::min(data_size - size, buffer_size);
    write_helper_->AddWork(src_machine_id, dst_machine_id, src_ptr, dst_ptr, transfer_size);
    size += transfer_size;
    src_ptr += transfer_size;
    dst_ptr += transfer_size;
  }
}

IBVerbsPoller::~IBVerbsPoller() {
  delete read_helper_;
  delete write_helper_;
}

}