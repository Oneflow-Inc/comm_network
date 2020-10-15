#pragma once

namespace comm_network {
enum class MsgType {
  kDataIsReady = 0,
  kAllocateMemory,
  kPleaseWrite,
  kWorkRecord,
  kFreeBufferPair,
  kReadDone
};

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

struct WorkRecord {
  int64_t machine_id;
  uint32_t id;
  void* begin_addr;
  size_t data_size;
  size_t offset;
};

struct FreeBufferPair {
  int64_t src_machine_id;
  uint8_t buffer_id;
};

struct ReadDone {};

struct Msg {
  MsgType msg_type;
  union {
    DataIsReady data_is_ready;
    AllocateMemory allocate_memory;
    PleaseWrite please_write;
    WorkRecord work_record;
    FreeBufferPair free_buffer_pair;
    ReadDone read_done;
  };
};

}  // namespace comm_network
