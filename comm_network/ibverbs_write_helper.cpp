#include "comm_network/ibverbs_write_helper.h"
#include "comm_network/ibverbs_comm_network.h"

namespace comm_network {
IBVerbsWriteHelper::IBVerbsWriteHelper() {
  cur_msg_queue_ = new std::queue<Msg>;
  pending_msg_queue_ = new std::queue<Msg>;
  thread_is_busy_ = false;
  for (int i = 0; i < num_of_register_buffer / 2; i++) { idle_buffer_queue_.push(i); }
}

IBVerbsWriteHelper::~IBVerbsWriteHelper() {
  thread_.join();
  CHECK(cur_msg_queue_->empty());
  delete cur_msg_queue_;
  cur_msg_queue_ = nullptr;
  {
    std::unique_lock<std::mutex> lck(pending_msg_queue_mtx_);
    delete pending_msg_queue_;
    pending_msg_queue_ = nullptr;
  }
}

void IBVerbsWriteHelper::NotifyMeToWrite() {
  if (!thread_is_busy_) {
    thread_ = std::thread(&IBVerbsWriteHelper::LoopProcess, this);
    thread_is_busy_ = true;
  }
}

void IBVerbsWriteHelper::LoopProcess() {
  if (cur_msg_queue_->empty()) {
    std::unique_lock<std::mutex> lck(pending_msg_queue_mtx_);
    std::swap(cur_msg_queue_, pending_msg_queue_);
  }
  while (!cur_msg_queue_->empty()) {
    Msg cur_msg = cur_msg_queue_->front();
    cur_msg_queue_->pop();
    // get an available buffer
    uint8_t buffer_id;
    {
      std::unique_lock<std::mutex> lock(idle_buffer_queue_mtx_);
      while (idle_buffer_queue_.empty()) { idle_buffer_queue_cv_.wait(lock); }
      buffer_id = idle_buffer_queue_.front();
      idle_buffer_queue_.pop();
    }
    // normal memory to register memory
    auto mem_desc_pair = Global<IBVerbsCommNet>::Get()->GetSendRecvMemPairForSender(
        cur_msg.work_record.machine_id, buffer_id);
    IBVerbsMemDesc* send_mem_desc = mem_desc_pair.first;
    IBVerbsMemDescProto recv_mem_desc_proto = mem_desc_pair.second;
    ibv_sge cur_sge = send_mem_desc->sge_vec().at(0);
    void* begin_addr = cur_msg.work_record.begin_addr;
    size_t data_size = cur_msg.work_record.data_size;
    size_t offset = cur_msg.work_record.offset;
    size_t transfer_size = std::min(data_size - offset, buffer_size - sizeof(size_t));
    char* src_addr = reinterpret_cast<char*>(begin_addr) + offset;
    // header include start offset
    memcpy(reinterpret_cast<void*>(cur_sge.addr), &offset, sizeof(size_t));
    cur_sge.addr += sizeof(size_t);
    memcpy(reinterpret_cast<void*>(cur_sge.addr), src_addr, transfer_size);
    cur_msg.work_record.offset += transfer_size;
    {
      std::unique_lock<std::mutex> lck(pending_msg_queue_mtx_);
      if (cur_msg.work_record.offset < data_size) { pending_msg_queue_->push(cur_msg); }
    }
    uint32_t imm_data = cur_msg.work_record.id << 8;
    imm_data += buffer_id;
    Global<IBVerbsCommNet>::Get()->Normal2RegisterDone(
        cur_msg.work_record.machine_id, send_mem_desc, recv_mem_desc_proto, imm_data);
    if (cur_msg_queue_->empty()) {
      std::unique_lock<std::mutex> lck(pending_msg_queue_mtx_);
      std::swap(cur_msg_queue_, pending_msg_queue_);
    }
  }
  thread_is_busy_ = false;
}

void IBVerbsWriteHelper::AsyncWrite(const Msg& msg) {
  pending_msg_queue_mtx_.lock();
  bool need_send_reminder = pending_msg_queue_->empty();
  pending_msg_queue_->push(msg);
  pending_msg_queue_mtx_.unlock();
  if (need_send_reminder) { NotifyMeToWrite(); }
}

void IBVerbsWriteHelper::FreeBuffer(uint8_t buffer_id) {
  std::unique_lock<std::mutex> lock(idle_buffer_queue_mtx_);
  idle_buffer_queue_.push(buffer_id);
  idle_buffer_queue_cv_.notify_one();
}
}  // namespace comm_network
