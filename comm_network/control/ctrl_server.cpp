#include "comm_network/control/ctrl_server.h"

namespace comm_network {

CtrlServer::CtrlServer() {
	grpc::EnableDefaultHealthCheckService(true);
  std::string server_address("0.0.0.0:50051");
  CtrlServiceImpl ctrl_service;
  auto option = grpc::MakeChannelArgumentOption(GRPC_ARG_ALLOW_REUSEPORT, 0);
  grpc::ServerBuilder serverBuilder;
  serverBuilder.SetOption(std::move(option));
  serverBuilder.SetMaxMessageSize(INT_MAX);
  serverBuilder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
  serverBuilder.RegisterService(&ctrl_service);
	grpc_server_ = serverBuilder.BuildAndStart();
  CHECK(grpc_server_) << "create grpc server failed";
	LOG(INFO) << "grpc listening on " << server_address;
  this->grpc_server_->Wait();
  auto grpc_server_run = [this, server_address]() {
    LOG(INFO) << "grpc listening on " << server_address;
    this->grpc_server_->Wait();
  };
  loop_thread_ = std::thread(grpc_server_run);
}

CtrlServer::~CtrlServer() {
  loop_thread_.join();
  grpc_server_->Shutdown();
}

}  // namespace comm_network
