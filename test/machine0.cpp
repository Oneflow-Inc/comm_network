#include <iostream>
#include <vector>
#include <string>
#include "comm_network/env_desc.h"
#include "comm_network/ibverbs_comm_network.h"
#include "comm_network/message.h"

int main() {
  std::string env_config_file = "/home/liyurui/oneflow-develop/comm_network/test/env_config.in";
  comm_network::EnvDesc env_desc(env_config_file, 0);
  auto machine_cfg = env_desc.machine_cfgs();
	//comm_network::CtrlServer ctrl_server;
  comm_network::IBVerbsCommNet ibverbs_comm_net(env_desc);
	comm_network::Msg msg;
	ibverbs_comm_net.SendMsg(1, msg);
	while (true) {}
  // // create test array
  // std::vector<int> test_values(10000, 10);
  // // init global ibverbs comm_net
  // IBVerbsCommNet::Init();
  // // send register ready message
  // Global<CommNet>::Get()->SendMsg(dst_machine_id, msg);
}
