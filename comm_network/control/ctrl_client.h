#pragma once
#include <grpcpp/grpcpp.h>
#include "comm_network/env_desc.h"
#include "comm_network/utils.h"
#include "comm_network/control/control.grpc.pb.h"

namespace comm_network {
using PbMessage = google::protobuf::Message;

#define OF_PP_INTERNAL_STRINGIZE(text) OF_PP_INTERNAL_STRINGIZE_I(text)
#define OF_PP_INTERNAL_STRINGIZE_I(text) #text
#define OF_PP_STRINGIZE(x) OF_PP_INTERNAL_STRINGIZE(x)
#define FILE_LINE_STR	__FILE__ ":" OF_PP_STRINGIZE(__LINE) 

#define BARRIER_ALL(ctrl_client) ctrl_client->Barrier(FILE_LINE_STR)
#define BARRIER(ctrl_client)  \
  ctrl_client->Barrier(FILE_LINE_STR, ctrl_client->env_desc()->TotalMachineNum())
                                     

class CtrlClient {
 public:
  DISALLOW_COPY_AND_MOVE(CtrlClient);
	CtrlClient(const EnvDesc* env_desc);
  //CtrlClient(std::shared_ptr<grpc::Channel> channel) : stub_(CtrlService::NewStub(channel)) {}
  ~CtrlClient() = default;
  void PushKV(const std::string& k, const PbMessage& msg);
	void PullKV(const std::string& k, PbMessage* msg);
	void ClearKV(const std::string& k);
	void Barrier(const std::string& barrier_name, int32_t barrier_num);
	const EnvDesc* env_desc() const { return env_desc_; }

 private:
	void LoadServer(const std::string& server_addr, CtrlService::Stub* stub);
	CtrlService::Stub* GetResponsibleStub(const std::string& key);
	CtrlService::Stub* GetMasterStub() { return stubs_[0].get(); }
  //std::unique_ptr<CtrlService::Stub> stub_;
	std::vector<std::unique_ptr<CtrlService::Stub>> stubs_;
	const EnvDesc* env_desc_;
};
}  // namespace comm_network
