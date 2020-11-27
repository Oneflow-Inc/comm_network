#pragma once
#include <functional>

namespace comm_network {

enum class MsgType { kUserMsg = 0, kPleaseWrite, kFreeBufferPair, kNumType };

struct UserMsg {
  int32_t msg_type;
  const char* ptr;
  size_t bytes;
};

struct PleaseWrite {
  int64_t dst_machine_id;
  void* src_addr;
  size_t bytes;
};

struct FreeBufferPair {
  int64_t src_machine_id;
  int32_t buffer_id;
  bool last_piece;
};

class Msg {
 public:
  Msg() = default;
  Msg(const UserMsg& user_msg, std::function<void()> cb) {
    msg_type_ = MsgType::kUserMsg;
    bytes_ = sizeof(user_msg);
    ptr_ = (char*)malloc(bytes_);
    memcpy(ptr_, reinterpret_cast<const char*>(&user_msg), bytes_);
    cb_ = std::move(cb);
  }
  Msg(const PleaseWrite& please_write) {
    msg_type_ = MsgType::kPleaseWrite;
    bytes_ = sizeof(please_write);
    ptr_ = (char*)malloc(bytes_);
    memcpy(ptr_, reinterpret_cast<const char*>(&please_write), bytes_);
    cb_ = NULL;
  }
  Msg(const FreeBufferPair& free_buffer_pair) {
    msg_type_ = MsgType::kFreeBufferPair;
    bytes_ = sizeof(free_buffer_pair);
    ptr_ = (char*)malloc(bytes_);
    memcpy(ptr_, reinterpret_cast<const char*>(&free_buffer_pair), bytes_);
    cb_ = NULL;
  }
  ~Msg() { free(ptr_); }
  MsgType msg_type() const { return msg_type_; }
  const char* ptr() const { return ptr_; }
  size_t bytes() const { return bytes_; }
  std::function<void()> cb() const { return cb_; }

 private:
  MsgType msg_type_;
  char* ptr_;
  size_t bytes_;
  std::function<void()> cb_;
};

struct WorkRecord {
  int64_t machine_id;
  void* begin_addr;
  size_t bytes;
  size_t offset;
  std::function<void()> cb;
  WorkRecord() {}
  WorkRecord(int64_t machine_id, void* begin_addr, size_t bytes, size_t offset,
             std::function<void()> cb = NULL)
      : machine_id(machine_id),
        begin_addr(begin_addr),
        bytes(bytes),
        offset(offset),
        cb(std::move(cb)) {}
};

}  // namespace comm_network