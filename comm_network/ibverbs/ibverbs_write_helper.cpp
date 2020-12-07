#include "comm_network/ibverbs/ibverbs_write_helper.h"
#include "comm_network/ibverbs/ibverbs_comm_network.h"
#include "comm_network/comm_network_config_desc.h"

namespace comm_network {
IBVerbsWriteHelper::IBVerbsWriteHelper(
    const std::vector<std::pair<IBVerbsMemDesc*, IBVerbsMemDescProto>>& send_recv_pair)
    : send_recv_pair_(send_recv_pair) {
  cur_record_queue_ = new std::queue<WorkRecord>;
  pending_record_queue_ = new std::queue<WorkRecord>;
  for (int i = 0; i < Global<CommNetConfigDesc>::Get()->RegisterBufferNum(); i++) {
    idle_buffer_queue_.push(i);
  }
  cur_write_handle_ = &IBVerbsWriteHelper::InitWriteHandle;
}

IBVerbsWriteHelper::~IBVerbsWriteHelper() {
  CHECK(cur_record_queue_->empty());
  delete cur_record_queue_;
  cur_record_queue_ = nullptr;
  {
    std::unique_lock<std::mutex> lck(pending_record_queue_mtx_);
    delete pending_record_queue_;
    pending_record_queue_ = nullptr;
  }
}

bool IBVerbsWriteHelper::InitWriteHandle() {
  if (cur_record_queue_->empty()) {
    {
      std::unique_lock<std::mutex> lck(pending_record_queue_mtx_);
      std::swap(cur_record_queue_, pending_record_queue_);
    }
    if (cur_record_queue_->empty()) { return false; }
  }
  cur_record_ = cur_record_queue_->front();
  cur_record_queue_->pop();
  cur_write_handle_ = &IBVerbsWriteHelper::RequestBuffer;
  return true;
}

bool IBVerbsWriteHelper::RequestBuffer() {
  std::unique_lock<std::mutex> lock(idle_buffer_queue_mtx_);
  if (idle_buffer_queue_.empty()) { return false; }
  buffer_id_ = idle_buffer_queue_.front();
  idle_buffer_queue_.pop();
  cur_write_handle_ = &IBVerbsWriteHelper::DoCurWrite;
  return true;
}

void IBVerbsWriteHelper::AsyncWrite(const WorkRecord& record) {
  pending_record_queue_mtx_.lock();
  bool need_send_reminder = pending_record_queue_->empty();
  pending_record_queue_->push(record);
  pending_record_queue_mtx_.unlock();
  if (need_send_reminder) { NotifyMeToWrite(); }
}

void IBVerbsWriteHelper::FreeBuffer(int32_t buffer_id) {
  {
    std::unique_lock<std::mutex> lock(idle_buffer_queue_mtx_);
    idle_buffer_queue_.push(buffer_id);
  }
  NotifyMeToWrite();
}

bool IBVerbsWriteHelper::DoCurWrite() {
  // normal memory to register memory
  auto mem_desc_pair = send_recv_pair_[buffer_id_];
  LOG(INFO) << buffer_id_;
  IBVerbsMemDesc* send_mem_desc = mem_desc_pair.first;
  IBVerbsMemDescProto recv_mem_desc_proto = mem_desc_pair.second;
  void* begin_addr = cur_record_.begin_addr;
  size_t bytes = cur_record_.bytes;
  size_t offset = cur_record_.offset;
  size_t transfer_bytes = std::min(
      bytes - offset, Global<CommNetConfigDesc>::Get()->PerRegisterBufferMBytes() * 1024 * 1024);
  char* src_addr = reinterpret_cast<char*>(begin_addr) + offset;
  size_t sge_bytes = Global<CommNetConfigDesc>::Get()->SgeBytes();
  int32_t sge_num = (transfer_bytes - 1) / sge_bytes + 1;
  sge_num = std::min(Global<CommNetConfigDesc>::Get()->SgeNum(), sge_num);
  size_t transfer_record = 0;
  for (int i = 0; i < sge_num; i++) {
    ibv_sge cur_sge = send_mem_desc->sge_vec().at(i);
    size_t temp_bytes = std::min(sge_bytes, transfer_bytes - transfer_record);
    memcpy(reinterpret_cast<char*>(cur_sge.addr), src_addr, temp_bytes);
    transfer_record += temp_bytes;
    src_addr += temp_bytes;
  }
  cur_record_.offset += transfer_bytes;
  Global<IBVerbsCommNet>::Get()->Normal2RegisterDone(cur_record_.machine_id, send_mem_desc,
                                                     recv_mem_desc_proto, buffer_id_, sge_num);
  if (cur_record_.offset < bytes) {
    cur_write_handle_ = &IBVerbsWriteHelper::RequestBuffer;
  } else {
    cur_write_handle_ = &IBVerbsWriteHelper::InitWriteHandle;
  }
  return true;
}

void IBVerbsWriteHelper::NotifyMeToWrite() { WriteUntilQueueEmptyOrNoBuffer(); }

void IBVerbsWriteHelper::WriteUntilQueueEmptyOrNoBuffer() {
  while ((this->*cur_write_handle_)()) {}
}

}  // namespace comm_network