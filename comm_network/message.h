#pragma once

namespace comm_network {
enum class MsgType {
  kDataIsReady = 0,
  kPleaseWrite,
  kAllocateMemory,
  kDoWrite,
  kPartialWriteDone,
  kFreeBufferPair,
  kReadDone
};

struct MsgBody {
  void* src_addr;
  void* dst_addr;
  size_t data_size;
  int64_t src_machine_id;
  int64_t dst_machine_id;
  int8_t buffer_id;
  int64_t piece_id;
  int64_t num_of_pieces;
};

struct Msg {
  MsgType msg_type;
  MsgBody msg_body;
};

struct WritePartial {
  char* src_addr;
  char* dst_addr;
  size_t data_size;
  int64_t src_machine_id;
  int64_t dst_machine_id;
  int buffer_id;
  int piece_id;
  int total_piece_num;
};

}  // namespace comm_network
