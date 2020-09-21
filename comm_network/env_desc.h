#pragma once
#include <unordered_map>
#include <string>
#include "comm_network/control/ctrl_server.h"

namespace comm_network {
static const size_t kMB = 1024 * 1024;

class EnvDesc {
 public:
  DISALLOW_COPY_AND_MOVE(EnvDesc);
  EnvDesc() = delete;
  ~EnvDesc() = default;
  EnvDesc(std::string config_file, int64_t machine_id);
  std::unordered_map<int64_t, std::string> machine_cfgs() const { return machine_cfgs_; }
  int64_t my_machine_id() const { return machine_id_; }
  const CtrlServer* ctrl_server() const { return ctrl_server_; }

 private:
  std::unordered_map<int64_t, std::string> machine_cfgs_;
  int64_t machine_id_;
  CtrlServer* ctrl_server_;
};
}  // namespace comm_network
