#pragma once
#include "comm_network/utils.h"
#include "comm_network/message.h"

namespace comm_network {
class MsgBus {
 public:
  MsgBus() = default;
  ~MsgBus();
  void AddNewMsg(const Msg& msg);
  Msg GetAndRemoveTopRecvMsg();
  bool is_empty() { return msg_buffer_.empty(); }

 private:
  std::queue<Msg> msg_buffer_;
};
}  // namespace comm_network
