#include <iostream>
#include <iomanip>
#include "comm_network/comm_network.h"
#include "comm_network/message.h"
#include "comm_network/common/channel.h"
#include "comm_network/common/blocking_counter.h"

using namespace comm_network;

namespace {
	std::unordered_map<void*, std::vector<std::chrono::steady_clock::time_point>> time_points;
	BlockingCounter* bc;
}

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

void TestThroughputWithBytes(uint64_t bytes, int64_t this_machine_id, Channel<Msg>* action_channel, IBVerbsCommNet* ibverbs_comm_net) {
	int32_t total_iteration = 3000;
	BlockingCounter bc(total_iteration);
	void* ptr = malloc(bytes);
	std::vector<std::chrono::steady_clock::time_point> time_points(total_iteration + 10,
                                                                std::chrono::steady_clock::now());
	if (this_machine_id == 0) {
		for (int i = 0; i < total_iteration; i++) {
			Msg msg;
			msg.msg_type = MsgType::kDataIsReady;
			msg.data_is_ready.src_addr = ptr;
			msg.data_is_ready.data_size = bytes;
			msg.data_is_ready.src_machine_id = this_machine_id;
			msg.data_is_ready.dst_machine_id = 1;
			ibverbs_comm_net->SendMsg(1, msg);	
			bc.Decrease();	
		}
	} else if (this_machine_id == 1) {
		for (int i = 0; i < total_iteration; i++) {
			Msg msg;
			// receive allocate memory message
			if (action_channel->Receive(&msg) == kChannelStatusSuccess) {
				size_t data_size = msg.allocate_memory.data_size;
				int64_t src_machine_id = msg.allocate_memory.src_machine_id;
				void* src_addr = msg.allocate_memory.src_addr;
				void* data = malloc(data_size);	
				ibverbs_comm_net->DoRead(src_machine_id, src_addr, data, data_size, [&time_points, &bc, data, i](){
					time_points.at(i + 1) = std::chrono::steady_clock::now();
					bc.Decrease();
					free(data);
				});
			}
		}
	}
	bc.WaitUntilCntEqualZero();
  double total_bytes = bytes * total_iteration;
  double throughput_average = total_bytes
                              / (std::chrono::duration_cast<std::chrono::microseconds>(
                                     time_points.at(total_iteration) - time_points.at(0))
                                     .count());
  std::cout << std::setw(25) << std::left << bytes << std::setw(25) << std::left << total_iteration
            << std::setw(25) << std::left
            << throughput_average << std::endl;
	BARRIER();
	free(ptr);
}

void TestThroughput(int64_t this_machine_id, Channel<Msg>* action_channel, IBVerbsCommNet* ibverbs_comm_net) {
  std::cout << "Test for throughput. Start.\n";
  std::cout << "-------------------------------------------------------------------------------\n";
  std::cout << std::setw(25) << std::left << "#bytes" << std::setw(25) << std::left << "#iterations"
            << std::setw(25) << std::left
            << "#throughput average[MiB/s]" << std::endl;
  uint64_t bytes = 2;
  for (int i = 0; i < 23; ++i) { 
		TestThroughputWithBytes(bytes << i, this_machine_id, action_channel, ibverbs_comm_net); 
	}
  std::cout << "-------------------------------------------------------------------------------\n";
  std::cout << "Test for throughput. Done.\n\n";
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
	TestThroughput(this_machine_id, &action_channel, ibverbs_comm_net);
	action_channel.Close();
	DestroyCommNet();
	return 0;
}

