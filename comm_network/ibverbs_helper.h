#pragma once
#include "comm_network/ibverbs_read_helper.h"
#include "comm_network/ibverbs_write_helper.h"
#include "comm_network/message.h"

namespace comm_network {
class IBVerbsHelper final {
 public:
  CN_DISALLOW_COPY_AND_MOVE(IBVerbsHelper);
  IBVerbsHelper();
  ~IBVerbsHelper();

  void AsyncWrite(const Msg& msg);
  void AsyncRead(uint32_t read_id, uint8_t buffer_id);
  void FreeBuffer(uint8_t buffer_id);

 private:
  IBVerbsReadHelper* read_helper_;
  IBVerbsWriteHelper* write_helper_;
};
}