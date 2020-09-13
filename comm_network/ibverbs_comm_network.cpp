#include "comm_network/ibverbs_comm_network.h"
#include "comm_network/env.pb.h"

namespace comm_network {
IBVerbsCommNet::IBVerbsCommNet(EnvDesc& env_desc) {
  // get all other machines id
}

void IBVerbsCommNet::set_env_cfg(int64_t id, std::string addr) {
  Machine machine;
  machine.set_id(id);
  machine.set_addr("192.168.1.14");
}

}  // namespace comm_network