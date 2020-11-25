#include "comm_network/comm_network.h"
#include "comm_network/comm_network_config_desc.h"
#include "comm_network/control/ctrl_client.h"
#include "comm_network/control/ctrl_server.h"
#include "comm_network/epoll/epoll_comm_network.h"
#include "comm_network/ibverbs/ibverbs_comm_network.h"

namespace comm_network {
CommNet* SetUpCommNet(const CommNetConfig& comm_net_config) {
  Global<CommNetConfigDesc>::New(comm_net_config);
  Global<CtrlServer>::New();
  Global<CtrlClient>::New();
  if (comm_net_config.use_rdma()) {
    Global<IBVerbsCommNet>::New();
    Global<CommNet>::SetAllocated(Global<IBVerbsCommNet>::Get());
  } else {
    Global<EpollCommNet>::New();
    Global<CommNet>::SetAllocated(Global<EpollCommNet>::Get());
  }
  return Global<CommNet>::Get();
}

void CommNet::Init() {
  int64_t total_machine_num = Global<CommNetConfigDesc>::Get()->TotalMachineNum();
  this_machine_id_ = Global<CommNetConfigDesc>::Get()->GetMachineId(
      Global<CtrlServer>::Get()->this_machine_addr());
  for (int64_t i = 0; i < total_machine_num; ++i) {
    if (i == this_machine_id_) { continue; }
    peer_machine_id_.insert(i);
  }
  SetUp();
  for (auto* poller : poller_vec_) { poller->Start(); }
}

void CommNet::Finalize() {
  for (auto* poller : poller_vec_) {
    if (poller) {
      poller->Stop();
      delete poller;
    }
  }
  TearDown();
}

void CommNet::RegisterMsgHandler(int32_t msg_type,
                                 std::function<void(const char* ptr, size_t bytes)> handler) {
  msg_handlers_.emplace(msg_type, std::move(handler));
  BARRIER();
}

std::function<void(const char* ptr, size_t bytes)> CommNet::GetMsgHandler(int32_t msg_type) {
  auto msg_handler_iter = msg_handlers_.find(msg_type);
  CHECK(msg_handler_iter != msg_handlers_.end());
  return msg_handler_iter->second;
}

}  // namespace comm_network
