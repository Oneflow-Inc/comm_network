#include <iostream>
#include "comm_network/env.pb.h"
#include "comm_network/env_desc.h"
#include "comm_network/control/ctrl_server.h"
#include "comm_network/control/ctrl_client.h"
#include "comm_network/ibverbs_comm_network.h"
#include "comm_network/message.h"
#include "comm_network/msg_bus.h"

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

void HandleMachineProcess(int64_t this_machine_id, IBVerbsCommNet* ibverbs_comm_net) {
	switch(this_machine_id) {
		case 0: {
			std::cout << "Machine 0: " << std::endl;
			int test_data_arr[100];
			for (int i = 0;i < 100;i++) {
				test_data_arr[i] = i;
			}
			Msg msg(0, 1, test_data_arr, 100 * sizeof(int));
			ibverbs_comm_net->SendMsg(1, msg);	
			break;
		}
		case 1: {
			std::cout << "Machine 1: " << std::endl;
			break;
		}
		default:
			std::cout << "Unsupport number of machines." << std::endl;
	}		
}

int main(int argc, char* argv[]) {
	std::string ip_confs_raw_array[2] = {"192.168.1.12", "192.168.1.13"};
	//std::string ip_confs_raw_array[1] = {"192.168.1.12"};
	int32_t ctrl_port = 10534;
	std::vector<std::string> ip_confs(ip_confs_raw_array, ip_confs_raw_array + 2);
	EnvProto env_proto = InitEnvProto(ip_confs, ctrl_port);
	EnvDesc* env_desc = new EnvDesc(env_proto);
	CtrlServer* ctrl_server = new CtrlServer(ctrl_port);
	CtrlClient* ctrl_client = new CtrlClient(env_desc);	
	MsgBus* msg_bus = new MsgBus();

	int64_t this_machine_id = env_desc->GetMachineId(ctrl_server->this_machine_addr());
	std::cout << "This machine id is: " << this_machine_id << std::endl;
	IBVerbsCommNet ibverbs_comm_net(ctrl_client, msg_bus, this_machine_id);
	HandleMachineProcess(this_machine_id, &ibverbs_comm_net);
	
	delete env_desc;
	delete ctrl_server;
	delete ctrl_client;
	delete msg_bus;

	return 0;
}

