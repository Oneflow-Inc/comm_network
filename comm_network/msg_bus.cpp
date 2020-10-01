#include "comm_network/msg_bus.h"

namespace comm_network {
MsgBus::MsgBus() {
	exit_flag_ = false;
	msg_handler_ = std::thread(&MsgBus::HandleMsg, this);
}

MsgBus::~MsgBus() {
	CHECK(msg_buffer_.empty());
	exit_flag_ = true;
	msg_handler_.join();
}

void MsgBus::AddNewMsg(const Msg& msg) {
	msg_buffer_.push(msg);		
}

void MsgBus::HandleMsg() {
	while (!exit_flag_) {
		if (!msg_buffer_.empty()) {
			const Msg msg = msg_buffer_.front();
			msg_buffer_.pop();
			LOG(INFO) << msg.src_id() << " " << msg.dst_id();	
		}
	}
}
}
