#pragma once
#include <grpcpp/grpcpp.h>
#include "comm_network/control/control.grpc.pb.h"
#include "comm_network/utils.h"

namespace comm_network {

class CtrlServiceImpl final : public CtrlService::Service {
 public:
  grpc::Status PullKV(grpc::ServerContext* context, const PullKVRequest* request,
                      PullKVResponse* response) override {
    return grpc::Status::OK;
  }
  grpc::Status PushKV(grpc::ServerContext* context, const PushKVRequest* request,
                      PushKVResponse* response) override {
		LOG(INFO) << "In service PushKV";
    kv_[request->key()] = request->val();
    return grpc::Status::OK;
  }
  std::unordered_map<std::string, std::string> get_kv() const { return kv_; }

 private:
  std::unordered_map<std::string, std::string> kv_;
};

class CtrlServer {
 public:
  CtrlServer();
  ~CtrlServer();

 private:
  std::unique_ptr<grpc::Server> grpc_server_;
  std::thread loop_thread_;
};
}  // namespace comm_network
