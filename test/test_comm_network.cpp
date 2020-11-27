#include <iostream>
#include "comm_network/comm_network.h"
#include "comm_network/common/blocking_counter.h"
using namespace comm_network;

// User defined message type, supplied by themselves,
// user should guarantee the key of message type is unique.
// For it is the key to identify different message.
enum class UserDefineMsgType { kDataIsReady = 10 };

// User defined specific message body
struct DataIsReady {
  DataIsReady(int64_t src_machine_id, size_t bytes, void* src_addr)
      : src_machine_id(src_machine_id), bytes(bytes), src_addr(src_addr) {}
  int64_t src_machine_id;
  size_t bytes;
  void* src_addr;
};

int main() {
  // User-defined ip configuration
  std::string ip_confs_raw_array[2] = {"192.168.1.12", "192.168.1.13"};
  int32_t ctrl_port = 10534;
  int64_t machine_num = sizeof(ip_confs_raw_array) / sizeof(std::string);
  // Configuration Preference using proto syntax
  CommNetConfig comm_net_config;
  for (int i = 0; i < machine_num; i++) {
    auto* new_machine = comm_net_config.add_machine();
    new_machine->set_id(i);
    new_machine->set_addr(ip_confs_raw_array[i]);
  }
  comm_net_config.set_ctrl_port(ctrl_port);
  comm_net_config.set_use_rdma(true);
  // There are also some other options can be user-defined..., comm_network_config.proto for detail
  comm_net_config.set_poller_num(4);
  // Intialize comm_network environment.
  // Get the handler from comm_network library
  // Generalize base pointer points to subclass
  CommNet* comm_net = SetUpCommNet(comm_net_config);
  comm_net->Init();
  int64_t this_machine_id = comm_net->ThisMachineId();
  // Allocate test-case memory
  // Register message handler with implicit synchronize ensuring all the machines
  // have register the specific callback handler.
  // Machine 0 (sender) responsible for sending "DataIsReady" message, notifying receiver
  // the required data is ready.
  // After the sending process finishes, sender will invoke the user-defined callback which
  // is the last parameter of "SendMsg" operation.
  // As soon as Machine 1 (receiver) receiving the "DataIsReady" message, it should invoke "DoRead"
  // operation to read the data from sender. After the reading process finishes, receiver will
  // invoke the user-defined callback which is the last parameter of "DoRead" operation.
  BlockingCounter bc(1);
  comm_net->RegisterMsgHandler(
      static_cast<int32_t>(UserDefineMsgType::kDataIsReady),
      [comm_net, &bc](const char* ptr, size_t bytes) {
        const DataIsReady* data_is_ready = reinterpret_cast<const DataIsReady*>(ptr);
        CHECK_EQ(bytes, sizeof(*data_is_ready));
        void* dst_ptr = malloc(data_is_ready->bytes);
        std::cout << "In data is ready callback" << std::endl;
        comm_net->DoRead(data_is_ready->src_machine_id, data_is_ready->src_addr,
                         data_is_ready->bytes, dst_ptr, [&bc, dst_ptr]() {
                           // Do nothing in this case...
                           std::cout << "In Do Read callback" << std::endl;
                           int* data = reinterpret_cast<int*>(dst_ptr);
                           std::cout << data[0] << " " << data[2432] << " " << data[65312] << " " << data[1024*1024+54236]
                                     << std::endl;
                           bc.Decrease();
                         });
      });
  if (this_machine_id == 0) {
    size_t bytes = 4 * 1024 * 1024 * 7;
    void* addr = malloc(bytes);
    int* data = reinterpret_cast<int*>(addr);
    data[0] = 10567;
    data[2432] = 43222;
    data[65312] = 52788;
    data[1024*1024+54236] = 52498;
    DataIsReady* data_is_ready = new DataIsReady(0, bytes, addr);
    comm_net->SendMsg(1, static_cast<int32_t>(UserDefineMsgType::kDataIsReady),
                      reinterpret_cast<const char*>(data_is_ready), sizeof(*data_is_ready),
                      []() {
                        // Do nothing in this case...
                        std::cout << "In send message callback" << std::endl;
                      });
    comm_net->RegisterReadDoneCb(1, [&bc, addr](){
      bc.Decrease();
      free(addr);
    });
  }
  bc.WaitUntilCntEqualZero();
  comm_net->Finalize();
  return 0;
}