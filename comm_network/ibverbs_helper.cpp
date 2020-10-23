#include "comm_network/ibverbs_helper.h"

namespace comm_network {
IBVerbsHelper::IBVerbsHelper() {
  read_helper_ = new IBVerbsReadHelper;
  write_helper_ = new IBVerbsWriteHelper;
}

IBVerbsHelper::~IBVerbsHelper() {
  delete read_helper_;
  delete write_helper_;
}

void IBVerbsHelper::AsyncWrite(const WorkRecord& record) { write_helper_->AsyncWrite(record); }
void IBVerbsHelper::AsyncRead(uint32_t read_id, uint8_t buffer_id) {
  read_helper_->AsyncRead(read_id, buffer_id);
}
void IBVerbsHelper::FreeBuffer(uint8_t buffer_id) { write_helper_->FreeBuffer(buffer_id); }
}  // namespace comm_network