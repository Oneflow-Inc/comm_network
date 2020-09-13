#include <iostream>
#include <vector>
#include "comm_network/ibverbs_comm_network.h"

int main() {
  comm_network::IBVerbsCommNet::set_env_cfg(1, "192.168.1.14");
  // // create test array
  // std::vector<int> test_values(10000, 10);
  // // init global ibverbs comm_net
  // IBVerbsCommNet::Init();
  // // send register ready message
  // Global<CommNet>::Get()->SendMsg(dst_machine_id, msg);
}