#pragma once
#include "comm_network/env.pb.h"
#include "comm_network/env_desc.h"
#include "comm_network/control/ctrl_server.h"
#include "comm_network/control/ctrl_client.h"
#include "comm_network/global.h"
#include "comm_network/ibverbs_comm_network.h"

namespace comm_network {
IBVerbsCommNet* InitCommNet(const EnvProto& env_proto, Channel<Msg>* action_channel) {
  Global<EnvDesc>::New(env_proto);
  Global<CtrlServer>::New();
	Global<CtrlClient>::New();
  Global<IBVerbsCommNet>::New(action_channel);
  Global<IBVerbsCommNet>::Get()->RegisterFixNumMemory();
  return Global<IBVerbsCommNet>::Get();
}

void DestroyCommNet() {
  Global<EnvDesc>::Delete();
  Global<CtrlServer>::Delete();
	Global<CtrlClient>::Delete();
  Global<IBVerbsCommNet>::Get()->UnRegisterFixNumMemory();
  Global<IBVerbsCommNet>::Delete();
}

int64_t ThisMachineId() {
  return Global<EnvDesc>::Get()->GetMachineId(Global<CtrlServer>::Get()->this_machine_addr());
}

}