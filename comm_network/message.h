#pragma once

namespace comm_network {

enum class MsgType { kUserMsg = 0, kPleaseWrite = 1, kFreeBufferPair = 2 };

struct UserMsg {
  int64_t msg_type;
  const char* ptr;
  size_t bytes;
};

struct PleaseWrite {
  int64_t src_machine_id;
  void* src_addr;
  size_t bytes;
};

struct FreeBufferPair {
  int64_t src_machine_id;
  int32_t buffer_id;
};

struct Msg {
  MsgType msg_type;
  union {
    UserMsg user_msg;
    PleaseWrite please_write;
    FreeBufferPair free_buffer_pair;
  };
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