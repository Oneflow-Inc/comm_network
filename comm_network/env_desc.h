#pragma once
#include <unordered_map>
#include <string>

namespace comm_network {
static const size_t kMB = 1024 * 1024;

class EnvDesc {
public:
  EnvDesc() = delete;
  ~EnvDesc() = default;
  EnvDesc(std::string config_file, int64_t machine_id);
  std::unordered_map<int64_t, std::string>& machine_cfgs() { return machine_cfgs_; }
  int64_t my_machine_id() { return machine_id_; }

 private:
  std::unordered_map<int64_t, std::string> machine_cfgs_;
  int64_t machine_id_;
};
}  // namespace comm_network