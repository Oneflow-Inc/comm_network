#include <iostream>
#include "comm_network/env.pb.h"
#include "comm_network/env_desc.h"
#include "comm_network/control/ctrl_server.h"
#include "comm_network/control/ctrl_client.h"
#include "comm_network/ibverbs_comm_network.h"
#include "comm_network/message.h"
#include "comm_network/global.h"
#include "comm_network/channel.h"

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

int main(int argc, char* argv[]) {
	std::string ip_confs_raw_array[2] = {"192.168.1.12", "192.168.1.13"};
	int32_t ctrl_port = 10534;
	std::vector<std::string> ip_confs(ip_confs_raw_array, ip_confs_raw_array + 2);
	EnvProto env_proto = InitEnvProto(ip_confs, ctrl_port);
	EnvDesc* env_desc = new EnvDesc(env_proto);
	Channel<Msg>* action_channel = new Channel<Msg>();
	Global<CtrlServer>::New(ctrl_port);
	Global<CtrlClient>::New(env_desc);
	Global<IBVerbsCommNet>::New(action_channel);
	
	int64_t this_machine_id = env_desc->GetMachineId(Global<CtrlServer>::Get()->this_machine_addr());
	std::cout << "This machine id is: " << this_machine_id << std::endl;
	if (this_machine_id == 0) {
		int test_data_arr[100];
		for (int i = 0;i < 100;i++) {
			test_data_arr[i] = i;
		}
		Msg msg;
		msg.msg_type = MsgType::DataIsReady;
		DataIsReady data_is_ready;
		data_is_ready.src_addr = test_data_arr;
		data_is_ready.data_size = 100 * sizeof(int);
		data_is_ready.src_machine_id = this_machine_id;
		data_is_ready.dst_machine_id = 1;
		msg.data_is_ready = data_is_ready; 
		action_channel->Send(msg);
	}
	Global<IBVerbsCommNet>::Get()->RegisterFixNumMemory();
	Global<IBVerbsCommNet>::Get()->RegisterMemoryDone();

	std::thread action_poller = std::thread([&]() {
		Msg msg;
		while (action_channel->Receive(&msg) == kChannelStatusSuccess) {
			switch (msg.msg_type) {
				case(MsgType::DataIsReady): {
					int64_t dst_machine_id = msg.data_is_ready.dst_machine_id;
					Global<IBVerbsCommNet>::Get()->SendMsg(dst_machine_id, msg);	
					break;
				}
				case(MsgType::AllocateMemory): {
					size_t data_size = msg.allocate_memory.data_size;
					int64_t src_machine_id = msg.allocate_memory.src_machine_id;
					void* src_addr = msg.allocate_memory.src_addr;
					void* data = malloc(data_size);	
					Msg new_msg;
					new_msg.msg_type = MsgType::PleaseWrite;
					PleaseWrite please_write;
					please_write.src_addr = src_addr;
					please_write.dst_addr = data;
					please_write.data_size = data_size;
					please_write.src_machine_id = src_machine_id;
					please_write.dst_machine_id = this_machine_id;
					new_msg.please_write = please_write; 
					action_channel->Send(new_msg);	
					break;
				}
				case(MsgType::PleaseWrite): {
					int64_t src_machine_id = msg.please_write.src_machine_id;
					std::cout << src_machine_id << std::endl;
					Global<IBVerbsCommNet>::Get()->SendMsg(src_machine_id, msg);
					break;
				}
				case(MsgType::DoWrite): {
					void* src_addr = msg.do_write.src_addr;
					void* dst_addr = msg.do_write.dst_addr;
					size_t data_size = msg.do_write.data_size;
					int64_t src_machine_id = msg.do_write.src_machine_id;
					int64_t dst_machine_id = msg.do_write.dst_machine_id;
					std::cout << "write from " << src_machine_id << " to " << dst_machine_id << std::endl;
					Global<IBVerbsCommNet>::Get()->AsyncWrite(src_machine_id, dst_machine_id, src_addr, dst_addr, data_size);
					break;
				}
				case(MsgType::FreeBufferPair): {
					break;
				}
				default: {
					std::cout << "Unsupport message" << std::endl;
				}
			}
		}
	});
	sleep(10);
	
	delete env_desc;
	Global<CtrlServer>::Delete();
	Global<CtrlClient>::Delete();
	action_channel->Close();
	action_poller.join();
	delete action_channel;
	Global<IBVerbsCommNet>::Get()->UnRegisterFixNumMemory();	
	Global<IBVerbsCommNet>::Delete();
	return 0;
}

