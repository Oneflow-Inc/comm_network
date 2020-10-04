#pragma once
#include "comm_network/utils.h"
#include "comm_network/message.h"
#include "comm_network/env_desc.h"
#include "comm_network/ibverbs_read_helper.h"
#include "comm_network/ibverbs_write_helper.h"

namespace comm_network {
class IBVerbsCommNet;
class IBVerbsPoller {
 public:
  DISALLOW_COPY_AND_MOVE(IBVerbsPoller);
  IBVerbsPoller();
  ~IBVerbsPoller();
  void Start();
  void Stop();
  void AsyncWrite(int64_t src_machine_id, int64_t dst_machine_id, void* src_addr, void* dst_addr, size_t data_size);
 private:
  IBVerbsReadHelper* read_helper_;
  IBVerbsWriteHelper* write_helper_; 
};
}
