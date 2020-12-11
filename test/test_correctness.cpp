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

void TestCorrectnessWithBytes(uint64_t bytes, int64_t this_machine_id, CommNet* comm_net) {
  int32_t total_iteration = 1;
  BlockingCounter bc(total_iteration * 2);
  comm_net->RegisterMsgHandler(
      static_cast<int32_t>(UserDefineMsgType::kDataIsReady),
      [comm_net, &bc](const char* ptr, size_t bytes) {
        const DataIsReady* data_is_ready = reinterpret_cast<const DataIsReady*>(ptr);
        CHECK_EQ(bytes, sizeof(*data_is_ready));
        void* dst_ptr = malloc(data_is_ready->bytes);
        size_t data_bytes = data_is_ready->bytes;
        comm_net->DoRead(data_is_ready->src_machine_id, data_is_ready->src_addr,
                         data_is_ready->bytes, dst_ptr, [&bc, dst_ptr, data_bytes]() {
                           int* data = reinterpret_cast<int*>(dst_ptr);
                           int num_of_elements = data_bytes / sizeof(int32_t);
                           CHECK_EQ(data[0], 100);
                           CHECK_EQ(data[num_of_elements - 1], 50);
                           bc.Decrease();
                         });
      });
  int64_t peer_machine_id;
  if (this_machine_id == 0) {
    peer_machine_id = 1;
  } else {
    peer_machine_id = 0;
  }
  for (int i = 0; i < total_iteration; i++) {
    void* ptr = malloc(bytes);
    int* data = reinterpret_cast<int32_t*>(ptr);
    int num_of_elements = bytes / sizeof(int32_t);
    data[0] = 100;
    data[num_of_elements - 1] = 50;
    std::cout << num_of_elements << std::endl;
    DataIsReady* data_is_ready = new DataIsReady(this_machine_id, bytes, ptr);
    comm_net->SendMsg(peer_machine_id, static_cast<int32_t>(UserDefineMsgType::kDataIsReady),
                      reinterpret_cast<const char*>(data_is_ready), sizeof(*data_is_ready));
    comm_net->RegisterReadDoneCb(peer_machine_id, [&bc, ptr]() {
      free(ptr);
      bc.Decrease();
    });
  }
  bc.WaitUntilCntEqualZero();
}

void TestCorrectness(int64_t this_machine_id, CommNet* comm_net) {
  std::cout << "Test for Correctness. Start.\n";
  uint64_t bytes = 32;
  for (int i = 0; i < 10; ++i) { TestCorrectnessWithBytes(bytes << i, this_machine_id, comm_net); }
  std::cout << "-------------------------------------------------------------------------------\n";
  std::cout << "Test for Correctness. Done.\n\n";
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
  TestCorrectness(this_machine_id, comm_net);
  comm_net->Finalize();
  return 0;
}
