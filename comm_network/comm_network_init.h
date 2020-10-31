#pragma once

#include "comm_network/ibverbs/ibverbs_comm_network.h"
#include "comm_network/env_desc.h"
#include "comm_network/control/ctrl_server.h"
#include "comm_network/control/ctrl_client.h"

namespace comm_network {
template <typename CommNetType>
CommNet* InitCommNet(const EnvProto& env_proto) {
  Global<EnvDesc>::New(env_proto);
  Global<CtrlServer>::New();
  Global<CtrlClient>::New();
  Global<CommNetType>::New();
	Global<CommNet>::SetAllocated(Global<CommNetType>::Get());
  return Global<CommNet>::Get();
}

void DestroyCommNet() {
  Global<EnvDesc>::Delete();
  Global<CtrlServer>::Delete();
  Global<CtrlClient>::Delete();
  Global<CommNet>::Delete();
}

int64_t ThisMachineId() {
  return Global<EnvDesc>::Get()->GetMachineId(Global<CtrlServer>::Get()->this_machine_addr());
}
}
