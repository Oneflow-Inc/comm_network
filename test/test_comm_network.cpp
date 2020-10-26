#include <iostream>
#include <iomanip>
#include "comm_network/comm_network.h"
#include "comm_network/message.h"
#include "comm_network/common/channel.h"
#include "comm_network/common/blocking_counter.h"

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

void TestThroughputWithBytes(uint64_t bytes, int64_t this_machine_id, Channel<Msg>* action_channel,
                             IBVerbsCommNet* ibverbs_comm_net) {
  int32_t total_iteration = 5000;
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
        ibverbs_comm_net->DoRead(src_machine_id, src_addr, data, data_size,
                                 [&time_points, &bc, data, i]() {
                                   time_points.at(i + 1) = std::chrono::steady_clock::now();
                                   bc.Decrease();
                                   free(data);
                                 });
      }
    }
  }
  bc.WaitUntilCntEqualZero();
  if (this_machine_id == 1) {
    double total_bytes = bytes * total_iteration;
    double throughput_peak = 0;
    for (int i = 1; i <= total_iteration; ++i) {
      double duration_micro_sec = std::chrono::duration_cast<std::chrono::microseconds>(
                                      time_points.at(i) - time_points.at(i - 1))
                                      .count();
      throughput_peak = std::max(throughput_peak, bytes * 1.0 / duration_micro_sec);  // MiB/s
    }
    double throughput_average = total_bytes
                                / (std::chrono::duration_cast<std::chrono::microseconds>(
                                       time_points.at(total_iteration) - time_points.at(0))
                                       .count());
    std::cout << std::setw(25) << std::left << bytes << std::setw(25) << std::left
              << total_iteration << std::setw(25) << std::left << throughput_peak << std::setw(25)
              << std::left << throughput_average << std::endl;
  }
  BARRIER();
  free(ptr);
}

void TestThroughput(int64_t this_machine_id, Channel<Msg>* action_channel,
                    IBVerbsCommNet* ibverbs_comm_net) {
  std::cout << "Test for throughput. Start.\n";
  std::cout << "-------------------------------------------------------------------------------\n";
  std::cout << std::setw(25) << std::left << "#bytes" << std::setw(25) << std::left << "#iterations"
            << std::setw(25) << std::left << "#throughput peek[MiB/s]" << std::setw(25) << std::left
            << "#throughput average[MiB/s]" << std::endl;
  uint64_t bytes = 2;
  for (int i = 0; i < 23; ++i) {
    TestThroughputWithBytes(bytes << i, this_machine_id, action_channel, ibverbs_comm_net);
  }
  std::cout << "-------------------------------------------------------------------------------\n";
  std::cout << "Test for throughput. Done.\n\n";
}

void TestCorrectness(int64_t this_machine_id, Channel<Msg>* action_channel,
                     IBVerbsCommNet* ibverbs_comm_net) {
  std::cout
      << "Test for correctness. Start. \nEach machine will read 50 blocks of data alternately. "
         "Each read procedure has its correctness verification.\n";
  int test_num = 50;
  size_t total_bytes = 0;
  std::vector<size_t> origin_size_list(test_num);
  std::vector<void*> origin_ptr_list(test_num);
  srand(time(NULL));
  for (int i = 0; i < test_num; i++) {
    origin_size_list.at(i) = (rand() % 100 + 1) * 1024 * 1024;
    origin_ptr_list.at(i) = malloc(origin_size_list.at(i));
    int32_t* test_data = reinterpret_cast<int32_t*>(origin_ptr_list.at(i));
    int num_of_elements = origin_size_list.at(i) / sizeof(int32_t);
    total_bytes += origin_size_list.at(i);
    for (int j = 0; j < num_of_elements; j++) { test_data[j] = j + num_of_elements; }
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
    ibverbs_comm_net->SendMsg(peer_machine_id, msg);
  }
  BlockingCounter bc(test_num);
  std::vector<std::chrono::steady_clock::time_point> time_points(test_num + 10,
                                                                 std::chrono::steady_clock::now());
  for (int i = 0; i < test_num; i++) {
    Msg msg;
    // receive allocate memory message
    if (action_channel->Receive(&msg) == kChannelStatusSuccess) {
      size_t data_size = msg.allocate_memory.data_size;
      int64_t src_machine_id = msg.allocate_memory.src_machine_id;
      void* src_addr = msg.allocate_memory.src_addr;
      void* data = malloc(data_size);
      ibverbs_comm_net->DoRead(
          src_machine_id, src_addr, data, data_size, [&time_points, &bc, data, i, data_size]() {
            int32_t* result = reinterpret_cast<int32_t*>(data);
            int num_of_elements = data_size / sizeof(result[0]);
            time_points.at(i + 1) = std::chrono::steady_clock::now();
            bc.Decrease();
            double err = 0;
            for (int j = 0; j < num_of_elements; j++) {
              err += abs(result[j] - j - num_of_elements);
            }
            if (abs(err) >= 0.001) {
              std::cerr << "Error occurs when reading test case index " << i << std::endl;
            }
            free(data);
          });
    }
  }
  bc.WaitUntilCntEqualZero();
  double duration_sec =
      std::chrono::duration_cast<std::chrono::microseconds>(time_points[test_num] - time_points[0])
          .count();
  std::cout << "the latency is : " << duration_sec / 1000000.0
            << " s, the throughput is : " << (1.0 * total_bytes) / duration_sec << "MB/s.\n"
            << "Test for correctness. Done.\n\n";
  for (int i = 0; i < test_num; i++) {
    free(origin_ptr_list.at(i));
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
  TestCorrectness(this_machine_id, &action_channel, ibverbs_comm_net);
  TestThroughput(this_machine_id, &action_channel, ibverbs_comm_net);
  action_channel.Close();
  DestroyCommNet();
  return 0;
}
