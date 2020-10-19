#include "comm_network/ibverbs_write_helper.h"
#include "comm_network/ibverbs_comm_network.h"

namespace comm_network {
IBVerbsWriteHelper::IBVerbsWriteHelper() {
  cur_msg_queue_ = new std::queue<Msg>;
  pending_msg_queue_ = new std::queue<Msg>;
  for (int i = 0; i < num_of_register_buffer / 2; i++) { idle_buffer_queue_.push(i); }
  cur_write_handle_ = &IBVerbsWriteHelper::InitWriteHandle;
}

IBVerbsWriteHelper::~IBVerbsWriteHelper() {
  CHECK(cur_msg_queue_->empty());
  delete cur_msg_queue_;
  cur_msg_queue_ = nullptr;
  {
    std::unique_lock<std::mutex> lck(pending_msg_queue_mtx_);
    delete pending_msg_queue_;
    pending_msg_queue_ = nullptr;
  }
}

bool IBVerbsWriteHelper::InitWriteHandle() {
  if (cur_msg_queue_->empty()) {
    {
      std::unique_lock<std::mutex> lck(pending_msg_queue_mtx_);
      std::swap(cur_msg_queue_, pending_msg_queue_);
    }
    if (cur_msg_queue_->empty()) { return false; }
  }
  cur_msg_ = cur_msg_queue_->front();
  cur_msg_queue_->pop();
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

void IBVerbsWriteHelper::AsyncWrite(const Msg& msg) {
  pending_msg_queue_mtx_.lock();
  bool need_send_reminder = pending_msg_queue_->empty();
  pending_msg_queue_->push(msg);
  pending_msg_queue_mtx_.unlock();
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
      cur_msg_.work_record.machine_id, buffer_id_);
  IBVerbsMemDesc* send_mem_desc = mem_desc_pair.first;
  IBVerbsMemDescProto recv_mem_desc_proto = mem_desc_pair.second;
  ibv_sge cur_sge = send_mem_desc->sge_vec().at(0);
  void* begin_addr = cur_msg_.work_record.begin_addr;
  size_t data_size = cur_msg_.work_record.data_size;
  size_t offset = cur_msg_.work_record.offset;
  size_t transfer_size = std::min(data_size - offset, buffer_size);
  char* src_addr = reinterpret_cast<char*>(begin_addr) + offset;
  memcpy(reinterpret_cast<void*>(cur_sge.addr), src_addr, transfer_size);
  cur_msg_.work_record.offset += transfer_size;
  {
    std::unique_lock<std::mutex> lck(pending_msg_queue_mtx_);
    if (cur_msg_.work_record.offset < data_size) { pending_msg_queue_->push(cur_msg_); }
  }
  uint32_t imm_data = cur_msg_.work_record.id << 8;
  imm_data += buffer_id_;
  Global<IBVerbsCommNet>::Get()->Normal2RegisterDone(
      cur_msg_.work_record.machine_id, send_mem_desc, recv_mem_desc_proto, imm_data);
  cur_write_handle_ = &IBVerbsWriteHelper::InitWriteHandle; 
  return true;
}

void IBVerbsWriteHelper::NotifyMeToWrite() {
  WriteUntilQueueEmptyOrNoBuffer();
}

void IBVerbsWriteHelper::WriteUntilQueueEmptyOrNoBuffer() {
  while ((this->*cur_write_handle_)()) {}
}

}  // namespace comm_network
