#include "comm_network/comm_network.h"
#include "comm_network/env_desc.h"
#include "comm_network/control/ctrl_server.h"

namespace comm_network {
CommNet::CommNet() {
  int64_t total_machine_num = Global<EnvDesc>::Get()->TotalMachineNum();
  this_machine_id_ =
      Global<EnvDesc>::Get()->GetMachineId(Global<CtrlServer>::Get()->this_machine_addr());
  for (int64_t i = 0; i < total_machine_num; ++i) {
    if (i == this_machine_id_) { continue; }
    peer_machine_id_.insert(i);
  }
	cb_ = nullptr;
}

void CommNet::BindCbHandler(std::function<void(const Msg&)> callback) { 
	cb_ = std::move(callback);
}

void CommNet::AddUnfinishedDataReadyItem(const Msg& msg, std::function<void()> cb) {
  uint32_t read_id = msg.data_is_ready.read_id;
  callback_queue_.emplace(read_id, std::move(cb)); 
}

uint32_t CommNet::AllocateReadId() {
  // Generate a random number
  std::mt19937 gen(NewRandomSeed());
  std::uniform_int_distribution<uint32_t> read_id_distrib(1, pow(2, 24) - 1);
  uint32_t read_id;
  bool flag = false;
  do {
    read_id = read_id_distrib(gen);
    std::unique_lock<std::mutex> lock(busy_read_id_mtx_);
    flag = (busy_read_ids_.find(read_id) == busy_read_ids_.end());
    if (flag) { busy_read_ids_.insert(read_id); }
  } while (!flag);
  CHECK_GE(read_id, 1);
  CHECK_LE(read_id, pow(2, 24) - 1);
  return read_id;
}

void CommNet::FreeReadId(uint32_t read_id) {
  std::unique_lock<std::mutex> lock(busy_read_id_mtx_);
  CHECK_EQ(busy_read_ids_.erase(read_id), 1);
}

}  // namespace comm_network

