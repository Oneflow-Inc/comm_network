#pragma once
#include "comm_network/env_desc.h"
#include "comm_network/common/utils.h"
#include "comm_network/common/preprocessor.h"
#include "comm_network/control/ctrl_service.h"

namespace comm_network {
using PbMessage = google::protobuf::Message;

#define FILE_LINE_STR __FILE__ ":" CN_PP_STRINGIZE(__LINE__)

#define BARRIER_ALL() Global<CtrlClient>::Get()->Barrier(FILE_LINE_STR)
#define BARRIER() \
  Global<CtrlClient>::Get()->Barrier(FILE_LINE_STR, Global<EnvDesc>::Get()->TotalMachineNum())

class CtrlClient {
 public:
  CN_DISALLOW_COPY_AND_MOVE(CtrlClient);
  ~CtrlClient();

  void Barrier(const std::string& barrier_name);
  void Barrier(const std::string& barrier_name, int32_t barrier_num);
  void PushKV(const std::string& k, std::function<void(std::string*)> VSetter);
  void PushKV(const std::string& k, const std::string& v);
  void PushKV(const std::string& k, const PbMessage& msg);
  void ClearKV(const std::string& k);
  void PullKV(const std::string& k, std::function<void(const std::string&)> VGetter);
  void PullKV(const std::string& k, std::string* v);
  void PullKV(const std::string& k, PbMessage* msg);

 private:
  friend class Global<CtrlClient>;
  CtrlClient();
  void LoadServer(const std::string& server_addr, CtrlService::Stub* stub);
  CtrlService::Stub* GetResponsibleStub(const std::string& key);
  CtrlService::Stub* GetMasterStub() { return stubs_[0].get(); }
  std::vector<std::unique_ptr<CtrlService::Stub>> stubs_;

  bool need_heartbeat_thread_stop_;
  std::mutex need_heartbeat_thread_stop_mtx_;
  std::thread heartbeat_thread_;
};
}  // namespace comm_network
