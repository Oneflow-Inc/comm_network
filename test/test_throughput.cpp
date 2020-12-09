#include <iostream>
#include <iomanip>
#include "comm_network/comm_network.h"
#include "comm_network/common/blocking_counter.h"
using namespace comm_network;

enum class UserDefineMsgType { kDataIsReady = 10 };

struct DataIsReady {
  DataIsReady(int64_t src_machine_id, size_t bytes, void* src_addr)
      : src_machine_id(src_machine_id), bytes(bytes), src_addr(src_addr) {}
  int64_t src_machine_id;
  size_t bytes;
  void* src_addr;
};

void TestThroughputWithBytes(uint64_t bytes, int64_t this_machine_id, CommNet* comm_net) {
  int32_t total_iteration = 5000;
  BlockingCounter bc(total_iteration);
  std::vector<std::chrono::steady_clock::time_point> time_points(total_iteration + 10,
                                                                 std::chrono::steady_clock::now());
  int idx = 0;
  comm_net->RegisterMsgHandler(
      static_cast<int32_t>(UserDefineMsgType::kDataIsReady),
      [comm_net, &bc, &idx, &time_points](const char* ptr, size_t bytes) {
        const DataIsReady* data_is_ready = reinterpret_cast<const DataIsReady*>(ptr);
        CHECK_EQ(bytes, sizeof(*data_is_ready));
        void* dst_ptr = malloc(data_is_ready->bytes);
        comm_net->DoRead(data_is_ready->src_machine_id, data_is_ready->src_addr,
                         data_is_ready->bytes, dst_ptr, [&bc, dst_ptr, &idx, &time_points]() {
                           time_points[idx] = std::chrono::steady_clock::now();
                           idx++;
                           bc.Decrease();
                         });
      });
  if (this_machine_id == 0) {
    for (int i = 0; i < total_iteration; i++) {
      void* ptr = malloc(bytes);
      DataIsReady* data_is_ready = new DataIsReady(0, bytes, ptr);
      comm_net->SendMsg(1, static_cast<int32_t>(UserDefineMsgType::kDataIsReady),
                        reinterpret_cast<const char*>(data_is_ready), sizeof(*data_is_ready), []() {
                          // Do nothing...
                        });
      comm_net->RegisterReadDoneCb(1, [&time_points, i, &bc, ptr]() {
        time_points.at(i) = std::chrono::steady_clock::now();
        bc.Decrease();
        free(ptr);
      });
    }
  }
  bc.WaitUntilCntEqualZero();
  double total_throughput = 0;
  double throughput_peak = 0;
  for (int i = 1; i < total_iteration; ++i) {
    double duration_micro_sec = std::chrono::duration_cast<std::chrono::microseconds>(
                                    time_points.at(i) - time_points.at(i - 1))
                                    .count();
    total_throughput += bytes * 1.0 / duration_micro_sec;
    throughput_peak = std::max(throughput_peak, bytes * 1.0 / duration_micro_sec);  // MiB/s
  }
  double throughput_average = total_throughput / (total_iteration - 1);
  std::cout << std::setw(25) << std::left << bytes << std::setw(25) << std::left << total_iteration
            << std::setw(25) << std::left << throughput_peak << std::setw(25) << std::left
            << throughput_average << std::endl;
}

void TestThroughput(int64_t this_machine_id, CommNet* comm_net) {
  std::cout << "Test for throughput. Start.\n";
  std::cout << "-------------------------------------------------------------------------------\n";
  std::cout << std::setw(25) << std::left << "#bytes" << std::setw(25) << std::left << "#iterations"
            << std::setw(25) << std::left << "#throughput peek[MiB/s]" << std::setw(25) << std::left
            << "#throughput average[MiB/s]" << std::endl;
  uint64_t bytes = 2;
  for (int i = 0; i < 23; ++i) { TestThroughputWithBytes(bytes << i, this_machine_id, comm_net); }
  std::cout << "-------------------------------------------------------------------------------\n";
  std::cout << "Test for throughput. Done.\n\n";
}

int main() {
  std::string ip_confs_raw_array[2] = {"192.168.1.12", "192.168.1.13"};
  int32_t ctrl_port = 10534;
  int64_t machine_num = sizeof(ip_confs_raw_array) / sizeof(std::string);
  CommNetConfig comm_net_config;
  for (int i = 0; i < machine_num; i++) {
    auto* new_machine = comm_net_config.add_machine();
    new_machine->set_id(i);
    new_machine->set_addr(ip_confs_raw_array[i]);
  }
  comm_net_config.set_ctrl_port(ctrl_port);
  comm_net_config.set_use_rdma(true);
  comm_net_config.set_poller_num(4);
  comm_net_config.set_sge_num(1);
  CommNet* comm_net = SetUpCommNet(comm_net_config);
  comm_net->Init();
  int64_t this_machine_id = comm_net->ThisMachineId();
  TestThroughput(this_machine_id, comm_net);
  comm_net->Finalize();
  return 0;
}