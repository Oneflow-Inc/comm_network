#pragma once
#include "comm_network/env.pb.h"
#include "comm_network/common/utils.h"

namespace comm_network {
const int num_of_register_buffer = 4;
const size_t buffer_size = 4 * 1024 * 1024;

class EnvDesc {
 public:
  CN_DISALLOW_COPY_AND_MOVE(EnvDesc);
  explicit EnvDesc(const EnvProto& env_proto) : env_proto_(env_proto) {}
  ~EnvDesc() = default;

  size_t TotalMachineNum() const { return env_proto_.machine().size(); }
  const Machine& machine(int32_t idx) const { return env_proto_.machine(idx); }
  int32_t ctrl_port() const { return env_proto_.ctrl_port(); }
  int64_t GetMachineId(const std::string& addr) const;

 private:
  EnvProto env_proto_;
};
}  // namespace comm_network
