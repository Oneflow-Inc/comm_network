#pragma once
#include <grpcpp/grpcpp.h>
#include "comm_network/env_desc.h"
#include "comm_network/utils.h"
#include "comm_network/control/control.grpc.pb.h"

namespace comm_network {
using PbMessage = google::protobuf::Message;

#define FILE_LINE_STR __FILE__ ":" __LINE__

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
	const EnvDesc* env_desc() const { return env_desc_; }

 private:
	void LoadServer(const std::string& server_addr, CtrlService::Stub* stub);
	CtrlService::Stub* GetResponsibleStub(const std::string& key);
  //std::unique_ptr<CtrlService::Stub> stub_;
	std::vector<std::unique_ptr<CtrlService::Stub>> stubs_;
	const EnvDesc* env_desc_;
};
}  // namespace comm_network
