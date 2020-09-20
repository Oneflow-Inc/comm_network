#include "comm_network/control/ctrl_server.h"

namespace comm_network {

CtrlServer::CtrlServer() {
  std::string server_address("0.0.0.0:50051");
  auto option = grpc::MakeChannelArgumentOption(GRPC_ARG_ALLOW_REUSEPORT, 0);
  grpc::ServerBuilder serverBuilder;
  serverBuilder.SetOption(std::move(option));
  serverBuilder.SetMaxMessageSize(INT_MAX);
  serverBuilder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
  serverBuilder.RegisterService(&service_);
  cq_ = serverBuilder.AddCompletionQueue();
  grpc_server_ = serverBuilder.BuildAndStart();
  CHECK(grpc_server_) << "create grpc server failed";
  LOG(INFO) << "grpc listening on " << server_address;
	loop_thread_ = std::thread(&CtrlServer::HandleRpcs, this);
}

void CtrlServer::HandleRpcs() {
  new CallData(&service_, cq_.get());
  void* tag;
  bool ok;
  while (true) {
                CHECK(cq_->Next(&tag, &ok));
		CHECK(ok);
		static_cast<CallData*>(tag)->Proceed();
  }
}

CtrlServer::~CtrlServer() {
  grpc_server_->Shutdown();
  cq_->Shutdown();
}

}  // namespace comm_network
