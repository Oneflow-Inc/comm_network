#include "comm_network/ibverbs/ibverbs_write_helper.h"
#include "comm_network/ibverbs/ibverbs_comm_network.h"

namespace comm_network {
IBVerbsWriteHelper::IBVerbsWriteHelper() {
  cur_record_queue_ = new std::queue<WorkRecord>;
  pending_record_queue_ = new std::queue<WorkRecord>;
  for (int i = 0; i < num_of_register_buffer / 2; i++) { idle_buffer_queue_.push(i); }
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

void IBVerbsWriteHelper::FreeBuffer(uint8_t buffer_id) {
  {
    std::unique_lock<std::mutex> lock(idle_buffer_queue_mtx_);
    idle_buffer_queue_.push(buffer_id);
  }
  NotifyMeToWrite();
}

bool IBVerbsWriteHelper::DoCurWrite() {
  // normal memory to register memory
  auto mem_desc_pair = Global<IBVerbsCommNet>::Get()->GetSendRecvMemPairForSender(
      cur_record_.machine_id, buffer_id_);
  IBVerbsMemDesc* send_mem_desc = mem_desc_pair.first;
  IBVerbsMemDescProto recv_mem_desc_proto = mem_desc_pair.second;
  ibv_sge cur_sge = send_mem_desc->sge_vec().at(0);
  void* begin_addr = cur_record_.begin_addr;
  size_t data_size = cur_record_.data_size;
  size_t offset = cur_record_.offset;
  size_t transfer_size = std::min(data_size - offset, buffer_size);
  char* src_addr = reinterpret_cast<char*>(begin_addr) + offset;
  memcpy(reinterpret_cast<void*>(cur_sge.addr), src_addr, transfer_size);
  cur_record_.offset += transfer_size;
  uint32_t imm_data = cur_record_.id << 8;
  imm_data += buffer_id_;
  Global<IBVerbsCommNet>::Get()->Normal2RegisterDone(cur_record_.machine_id, send_mem_desc,
                                                     recv_mem_desc_proto, imm_data);
  if (cur_record_.offset < data_size) {
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
