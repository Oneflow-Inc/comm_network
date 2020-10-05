#pragma once
#include "comm_network/utils.h"
#include "comm_network/channel.h"

namespace comm_network {
class IBVerbsWriteHelper {
 public:
  IBVerbsWriteHelper();
  ~IBVerbsWriteHelper();
  // void AddWork(IBVerbsQP* qp, char* src_addr, char* dst_addr, size_t data_size);
 private:
  // struct WritePartial {
  //   char* src_addr;
  //   char* dst_addr;
  //   size_t data_size;
  //   IBVerbsQP* qp; 
  // };
  // void PollQueue();
  // Channel<WritePartial> msg_queue_;
  // std::thread thread_;
};
}