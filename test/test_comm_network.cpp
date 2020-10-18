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

namespace {
	std::unordered_map<void*, std::vector<std::chrono::steady_clock::time_point>> time_points;
	size_t total_bytes = 0;
}

void HandleActions(Channel<Msg>* action_channel, IBVerbsCommNet* ibverbs_comm_net, int64_t this_machine_id) {
	Msg msg;
	while (action_channel->Receive(&msg) == kChannelStatusSuccess) {
		switch (msg.msg_type) {
			// sender machine reminds receiver of specific data is ready
			case(MsgType::kDataIsReady): {
				int64_t dst_machine_id = msg.data_is_ready.dst_machine_id;
				ibverbs_comm_net->SendMsg(dst_machine_id, msg);	
				break;
			}
			// receiver machine allocates memory and starts to read data
			case(MsgType::kAllocateMemory): {
				size_t data_size = msg.allocate_memory.data_size;
				int64_t src_machine_id = msg.allocate_memory.src_machine_id;
				void* src_addr = msg.allocate_memory.src_addr;
				void* data = malloc(data_size);	
				time_points[data].push_back(std::chrono::steady_clock::now());
				ibverbs_comm_net->DoRead(src_machine_id, src_addr, data, data_size);
				break;
			}
			// receiver machine finish reading data procedure
			case(MsgType::kReadDone): {
				void* data = msg.read_done.begin_addr;
				size_t data_size = msg.read_done.data_size;
				time_points[data].push_back(std::chrono::steady_clock::now());
				int32_t* result = reinterpret_cast<int32_t*>(data);
				int num_of_elements = data_size / sizeof(result[0]);
				double err = 0;
				for (int i = 0; i < num_of_elements; i++) {
					err += abs(result[i] - i - num_of_elements);
				}
				std::cout << "Reading " <<  (data_size/1024/1024) << " MB data done. With error " << err << std::endl; 
				if (abs(err) >= 0.001) {
					std::cerr << "Error occurs when reading " << (total_bytes/1024/1024) << " MB data. " << std::endl; 
				}
				total_bytes += data_size;
				break;
			}
			default: {
				std::cout << "Unsupport message type" << std::endl;
			}
		}
	}
}

void TestCorrectness(int64_t this_machine_id, Channel<Msg>* action_channel, IBVerbsCommNet* ibverbs_comm_net) {
  std::cout << "Test for correctness. Start. \nEach machine will read 50 blocks of data alternately. "
              "Each read procedure has its correctness verification.\n";
  int test_num = 50;
	time_points.clear();
	total_bytes = 0;
	std::vector<size_t> origin_size_list(test_num);
	std::vector<void*> origin_ptr_list(test_num);
	srand(time(NULL));
	for (int i = 0; i < test_num; i++) {
		origin_size_list.at(i) = (rand() % 100 + 1) * 1024 * 1024; 
		origin_ptr_list.at(i) = malloc(origin_size_list.at(i));
		int32_t* test_data = reinterpret_cast<int32_t*>(origin_ptr_list.at(i));
		int num_of_elements = origin_size_list.at(i) / sizeof(int32_t); 
		for (int j = 0; j < num_of_elements; j++) {
			test_data[j] = j + num_of_elements;
		}
	}
	int64_t peer_machine_id;
	if (this_machine_id == 0) {
		peer_machine_id = 1;
	} else if (this_machine_id == 1) {
		peer_machine_id = 0;
	}
	for (int i = 0; i < test_num; i++) {
		Msg msg;
		msg.msg_type = MsgType::kDataIsReady;
		msg.data_is_ready.src_addr = origin_ptr_list.at(i);
		msg.data_is_ready.data_size = origin_size_list.at(i);
		msg.data_is_ready.src_machine_id = this_machine_id;
		msg.data_is_ready.dst_machine_id = peer_machine_id;
		action_channel->Send(msg);	
	}
	std::thread action_poller(HandleActions, action_channel, ibverbs_comm_net, this_machine_id);
	sleep(30);
	action_channel->Close();
	action_poller.join();
	auto iter = time_points.begin();
	std::chrono::steady_clock::time_point start = iter->second[0];
	std::chrono::steady_clock::time_point end = iter->second[1];
	iter++;
	while (iter != time_points.end()) {
		double diff = std::chrono::duration_cast<std::chrono::microseconds>(iter->second[0] - start).count();
		if (diff < 0) {
			start = iter->second[0];
		} 
		diff = std::chrono::duration_cast<std::chrono::microseconds>(iter->second[1] - end).count(); 
		if (diff > 0) {
			end = iter->second[1];
		}
		iter++;
	} 
	double duration_sec = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count() / 1000000.0;  
	std::cout << "the latency is : " << duration_sec << " s, the throughput is : " 
		<< (1.0 * total_bytes / 1024 / 1024 ) / duration_sec << "MB/s. \n";
}

int main(int argc, char* argv[]) {
	std::string ip_confs_raw_array[2] = {"192.168.1.12", "192.168.1.15"};
	int32_t ctrl_port = 10534;
	std::vector<std::string> ip_confs(ip_confs_raw_array, ip_confs_raw_array + 2);
	EnvProto env_proto = InitEnvProto(ip_confs, ctrl_port);
	Channel<Msg> action_channel;
	IBVerbsCommNet* ibverbs_comm_net = InitCommNet(env_proto, &action_channel);
	
	int64_t this_machine_id = ThisMachineId();
	std::cout << "This machine id is: " << this_machine_id << std::endl;
  TestCorrectness(this_machine_id, &action_channel, ibverbs_comm_net);
	DestroyCommNet();
	return 0;
}

