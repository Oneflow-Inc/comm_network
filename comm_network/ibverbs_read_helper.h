#pragma once 
#include "comm_network/common/utils.h"

namespace comm_network {
class IBVerbsReadHelper {
 public:
  CN_DISALLOW_COPY_AND_MOVE(IBVerbsReadHelper);
  IBVerbsReadHelper() = default;
  ~IBVerbsReadHelper() = default;
  void AsyncRead(uint32_t read_id, uint8_t buffer_id);
};
}