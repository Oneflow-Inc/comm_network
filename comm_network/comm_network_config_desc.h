#pragma once
#include "comm_network/comm_network_config.pb.h"
#include "comm_network/common/utils.h"

namespace comm_network {
class CommNetConfigDesc {
 public:
  CN_DISALLOW_COPY_AND_MOVE(CommNetConfigDesc);
  explicit CommNetConfigDesc(const CommNetConfig& comm_net_config)
      : comm_net_config_(comm_net_config) {}
  ~CommNetConfigDesc() = default;

  size_t TotalMachineNum() const { return comm_net_config_.machine().size(); }
  const Machine& machine(int32_t idx) const { return comm_net_config_.machine(idx); }
  int32_t ctrl_port() const { return comm_net_config_.ctrl_port(); }
  int64_t GetMachineId(const std::string& addr) const;
  int64_t PollerNum() const { return comm_net_config_.poller_num(); }
  bool UseRDMA() const { return comm_net_config_.use_rdma(); }
  int64_t RegisterBufferNum() const { return comm_net_config_.register_buffer_num(); }
  uint64_t PerRegisterBufferMBytes() const { return comm_net_config_.per_register_buffer_mbytes(); }
  int32_t SgeNum() const { return comm_net_config_.sge_num(); }
  size_t SgeBytes() const {
    return comm_net_config_.per_register_buffer_mbytes() * 1024 * 1024 / comm_net_config_.sge_num();
  }
  size_t MaxMsgBytes() const { return comm_net_config_.max_msg_bytes(); }

 private:
  CommNetConfig comm_net_config_;
};
}  // namespace comm_network
