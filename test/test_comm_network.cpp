#include <iostream>
#include "comm_network/comm_network.h"
#include "comm_network/message.h"
#include "comm_network/common/channel.h"

using namespace comm_network;

EnvProto InitEnvProto(const std::vector<std::string>& ip_confs, int32_t ctrl_port) {
	EnvProto env_proto;
	int num_of_machines = ip_confs.size();
	for (int i = 0; i < num_of_machines; i++) {
		auto* new_machine = env_proto.add_machine();
		new_machine->set_id(i);
		new_machine->set_addr(ip_confs[i]);
	}
	env_proto.set_ctrl_port(ctrl_port);
	return env_proto;
}

void HandleActions(Channel<Msg>* action_channel, IBVerbsCommNet* ibverbs_comm_net, int64_t this_machine_id) {
	void* data;
	Msg msg;
	while (action_channel->Receive(&msg) == kChannelStatusSuccess) {
		switch (msg.msg_type) {
			case(MsgType::kDataIsReady): {
				int64_t dst_machine_id = msg.msg_body.dst_machine_id;
				ibverbs_comm_net->SendMsg(dst_machine_id, msg);	
				break;
			}
			case(MsgType::kAllocateMemory): {
				size_t data_size = msg.msg_body.data_size;
				int64_t src_machine_id = msg.msg_body.src_machine_id;
				void* src_addr = msg.msg_body.src_addr;
				data = malloc(data_size);	
				Msg new_msg;
				new_msg.msg_type = MsgType::kPleaseWrite;
				MsgBody msg_body;
				msg_body.src_addr = src_addr;
				msg_body.dst_addr = data;
				msg_body.data_size = data_size;
				msg_body.src_machine_id = src_machine_id;
				msg_body.dst_machine_id = this_machine_id;
				new_msg.msg_body = msg_body; 
				action_channel->Send(new_msg);	
				break;
			}
			case(MsgType::kPleaseWrite): {
				int64_t src_machine_id = msg.msg_body.src_machine_id;
				ibverbs_comm_net->SendMsg(src_machine_id, msg);
				break;
			}
			case(MsgType::kDoWrite): {
				void* src_addr = msg.msg_body.src_addr;
				void* dst_addr = msg.msg_body.dst_addr;
				size_t data_size = msg.msg_body.data_size;
				int64_t src_machine_id = msg.msg_body.src_machine_id;
				int64_t dst_machine_id = msg.msg_body.dst_machine_id;
				std::cout << "write from " << src_machine_id << " to " << dst_machine_id << std::endl;
				ibverbs_comm_net->AsyncWrite(src_machine_id, dst_machine_id, src_addr, dst_addr, data_size);
				break;
			}
			case(MsgType::kReadDone): {
				int* result = reinterpret_cast<int*>(data);
				for (int i = 8 * 1024 * 1024; i < 8 * 1024 * 1024 + 100;i++) {
					std::cout << result[i] << " ";
				}
				std::cout << std::endl;
				break;
			}
			default: {
				std::cout << "Unsupport message" << std::endl;
			}
		}
	}
}

int main(int argc, char* argv[]) {
	std::string ip_confs_raw_array[2] = {"192.168.1.12", "192.168.1.13"};
	int32_t ctrl_port = 10534;
	std::vector<std::string> ip_confs(ip_confs_raw_array, ip_confs_raw_array + 2);
	EnvProto env_proto = InitEnvProto(ip_confs, ctrl_port);
	Channel<Msg> action_channel;
	IBVerbsCommNet* ibverbs_comm_net = InitCommNet(env_proto, &action_channel);
	
	int64_t this_machine_id = ThisMachineId();
	std::cout << "This machine id is: " << this_machine_id << std::endl;
	if (this_machine_id == 0) {
		int *test_data_arr = new int[1024 * 1024 * 16];
		for (int i = 0;i < 1024 * 1024 * 16;i++) {
			test_data_arr[i] = i;
		}
		Msg msg;
		msg.msg_type = MsgType::kDataIsReady;
		MsgBody msg_body;
		msg_body.src_addr = reinterpret_cast<void*>(test_data_arr);
		msg_body.data_size = 1024 * 1024 * 16 * sizeof(int);
		msg_body.src_machine_id = this_machine_id;
		msg_body.dst_machine_id = 1;
		msg.msg_body = msg_body; 
		action_channel.Send(msg);
	}
	std::thread action_poller(HandleActions, &action_channel, ibverbs_comm_net, this_machine_id);
	sleep(120);
	DestroyCommNet();
	action_channel.Close();
	action_poller.join();
	return 0;
}

