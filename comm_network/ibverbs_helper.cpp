#include "comm_network/ibverbs_helper.h"

namespace comm_network {
IBVerbsHelper::IBVerbsHelper(const std::vector<std::pair<IBVerbsMemDesc*, IBVerbsMemDescProto>>& send_recv_pair) {
  read_helper_ = new IBVerbsReadHelper;
  write_helper_ = new IBVerbsWriteHelper(send_recv_pair);
}

IBVerbsHelper::~IBVerbsHelper() {
  delete read_helper_;
  delete write_helper_;
}

void IBVerbsHelper::AsyncWrite(const WorkRecord& record) { write_helper_->AsyncWrite(record); }
void IBVerbsHelper::SyncRead(uint32_t read_id, uint8_t buffer_id, IBVerbsMemDesc* recv_mem_desc) {
  read_helper_->SyncRead(read_id, buffer_id, recv_mem_desc);
}
void IBVerbsHelper::FreeBuffer(uint8_t buffer_id) { write_helper_->FreeBuffer(buffer_id); }
}  // namespace comm_network