#include "comm_network/msg_bus.h"

namespace comm_network {
MsgBus::MsgBus() {
	msg_handler_ = std::thread(&MsgBus::HandleMsg, this);
}

MsgBus::~MsgBus() {
	CHECK(msg_buffer_.empty());
}

void MsgBus::AddNewMsg(const Msg& msg) {
	msg_buffer_.push(msg);		
}

void MsgBus::HandleMsg() {
	while (true) {
		if (!msg_buffer_.empty()) {
			const Msg msg = msg_buffer_.front();
			msg_buffer_.pop();
			LOG(INFO) << msg.src_id() << " " << msg.dst_id();	
		}
	}
}
}
