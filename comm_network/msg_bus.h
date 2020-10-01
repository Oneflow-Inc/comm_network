#pragma once
#include "comm_network/utils.h"
#include "comm_network/message.h"

namespace comm_network {
class MsgBus {
 public:
	MsgBus();
	~MsgBus();
	void AddNewMsg(const Msg& msg);
	void HandleMsg();
 
 private:
	std::queue<Msg> msg_buffer_;
	std::thread msg_handler_;
};
}
