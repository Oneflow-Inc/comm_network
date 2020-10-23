#pragma once

namespace comm_network {
enum class MsgType { kDataIsReady = 0, kAllocateMemory, kPleaseWrite, kFreeBufferPair };

struct DataIsReady {
  int64_t src_machine_id;
  int64_t dst_machine_id;
  void* src_addr;
  size_t data_size;
};

struct AllocateMemory {
  int64_t src_machine_id;
  void* src_addr;
  size_t data_size;
};

struct PleaseWrite {
  int64_t dst_machine_id;
  void* src_addr;
  size_t data_size;
  uint32_t read_id;
};

struct FreeBufferPair {
  int64_t src_machine_id;
  uint8_t buffer_id;
};

struct Msg {
  MsgType msg_type;
  union {
    DataIsReady data_is_ready;
    AllocateMemory allocate_memory;
    PleaseWrite please_write;
    FreeBufferPair free_buffer_pair;
  };
};

struct WorkRecord {
  uint32_t id;
  int64_t machine_id;
  void* begin_addr;
  size_t data_size;
  size_t offset;
  std::function<void()> callback;
  WorkRecord() {}
  WorkRecord(uint32_t id, int64_t machine_id, void* begin_addr, size_t data_size, size_t offset,
             std::function<void()> callback = NULL)
      : id(id),
        machine_id(machine_id),
        begin_addr(begin_addr),
        data_size(data_size),
        offset(offset),
        callback(std::move(callback)) {}
};

}  // namespace comm_network
