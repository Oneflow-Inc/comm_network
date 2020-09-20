#include <fstream>
#include <string>
#include "comm_network/env_desc.h"

namespace comm_network {
EnvDesc::EnvDesc(std::string config_file, int64_t machine_id) {
  this->machine_id_ = machine_id;
  std::ifstream settings(config_file);
  int id;
  std::string ip;
  while (settings >> id >> ip) { machine_cfgs_.emplace(id, ip); }
  ctrl_server_ = new CtrlServer();
}
}  // namespace comm_network
