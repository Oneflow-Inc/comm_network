#pragma once
#include <grpcpp/grpcpp.h>
#include "comm_network/utils.h"
#include "comm_network/control/control.grpc.pb.h"

namespace comm_network {
using PbMessage = google::protobuf::Message;
class CtrlClient {
 public:
  DISALLOW_COPY_AND_MOVE(CtrlClient);
  CtrlClient(std::shared_ptr<grpc::Channel> channel) : stub_(CtrlService::NewStub(channel)) {}
  ~CtrlClient() = default;
  void PushKV(const std::string& k, const PbMessage& msg);

 private:
  std::unique_ptr<CtrlService::Stub> stub_;
};
}  // namespace comm_network
