#include "comm_network/comm_network_config_desc.h"

namespace comm_network {

int64_t CommNetConfigDesc::GetMachineId(const std::string& addr) const {
  int64_t machine_id = -1;
  int64_t machine_num = comm_net_config_.machine_size();
  FOR_RANGE(int64_t, i, 0, machine_num) {
    if (addr == comm_net_config_.machine(i).addr()) {
      machine_id = i;
      break;
    }
  }
  CHECK_GE(machine_id, 0);
  CHECK_LT(machine_id, machine_num);
  return machine_id;
}
}  // namespace comm_network
