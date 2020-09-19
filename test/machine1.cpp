#include <iostream>
#include <vector>
#include <string>
#include "comm_network/env_desc.h"
#include "comm_network/ibverbs_comm_network.h"

int main() {
	std::string env_config_file = "/home/liyurui/oneflow-develop/comm_network/test/env_config.in";
	comm_network::EnvDesc env_desc(env_config_file, 1);
	auto machine_cfg = env_desc.machine_cfgs();
	comm_network::IBVerbsCommNet ibverbs_comm_net(env_desc);

}
