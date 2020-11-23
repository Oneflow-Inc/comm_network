#pragma once
#include "comm_network/common/utils.h"
#include "comm_network/ibverbs/ibverbs_memory_desc.h"

namespace comm_network {
class IBVerbsReadHelper {
 public:
  CN_DISALLOW_COPY_AND_MOVE(IBVerbsReadHelper);
  IBVerbsReadHelper() = default;
  ~IBVerbsReadHelper() = default;
  void SyncRead(int32_t sge_num, int32_t buffer_id, IBVerbsMemDesc* recv_mem_desc);
};
}  // namespace comm_network