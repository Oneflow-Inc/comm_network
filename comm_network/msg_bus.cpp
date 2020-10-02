#include "comm_network/msg_bus.h"

namespace comm_network {

MsgBus::~MsgBus() { CHECK(msg_buffer_.empty()); }

void MsgBus::AddNewMsg(const Msg& msg) { msg_buffer_.push(msg); }

Msg MsgBus::GetAndRemoveTopRecvMsg() {
  CHECK(!msg_buffer_.empty());
  Msg msg = msg_buffer_.front();
  msg_buffer_.pop();
  return msg;
}
}  // namespace comm_network
