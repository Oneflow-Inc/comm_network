#pragma once
#include "comm_network/ibverbs/ibverbs_read_helper.h"
#include "comm_network/ibverbs/ibverbs_write_helper.h"
#include "comm_network/message.h"

namespace comm_network {
class IBVerbsHelper final {
 public:
  CN_DISALLOW_COPY_AND_MOVE(IBVerbsHelper);
  IBVerbsHelper(const std::vector<std::pair<IBVerbsMemDesc*, IBVerbsMemDescProto>>& send_recv_pair);
  ~IBVerbsHelper();

  void AsyncWrite(const WorkRecord& record);
  void SyncRead(int32_t sge_num, int32_t buffer_id, IBVerbsMemDesc* recv_mem_desc);
  void FreeBuffer(int32_t buffer_id);

 private:
  IBVerbsReadHelper* read_helper_;
  IBVerbsWriteHelper* write_helper_;
};
}  // namespace comm_network