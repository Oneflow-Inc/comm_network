#pragma once
#include <grpcpp/grpcpp.h>
#include "comm_network/control/ctrl_call.h"
#include "comm_network/control/control.grpc.pb.h"
#include "comm_network/utils.h"

namespace comm_network {

class CtrlServer {
 public:
  CtrlServer(int32_t ctrl_port);
  ~CtrlServer();
  const std::string& this_machine_addr() { return this_machine_addr_; }

 private:
  void HandleRpcs();
  void LoadServerEnqueueRequest();
  void PushKVEnqueueRequest();
  void PullKVEnqueueRequest();
  void BarrierEnqueueRequest();
	void ClearKVEnqueueRequest();
  void Init();

  //  	template <CtrlMethod kMethod>
  //  	void EnqueueRequest() {
  //  		constexpr const size_t I = (size_t)kMethod;
  //  	  auto handler = std::get<I>(handlers_);
  //  	  auto call = new CtrlCall<(CtrlMethod)I>();
  //  	  call->set_request_handler(std::bind(handler, call));
  //  		grpc_service_->RequestAsyncUnary(I, call->mut_server_ctx(), call->mut_request(),
  //  	                 call->mut_responder(), cq_.get(), cq_.get(), call);
  //  	}

  std::unordered_map<std::string, std::string> kv_;
  std::unordered_map<std::string, std::list<CtrlCall<CtrlMethod::kPullKV>*>> pending_kv_calls_;
  std::unordered_map<std::string, std::pair<std::list<CtrlCallIf*>, int32_t>> barrier_calls_;

  std::tuple<std::function<void(CtrlCall<CtrlMethod::kLoadServer>*)>,
             std::function<void(CtrlCall<CtrlMethod::kPushKV>*)>,
             std::function<void(CtrlCall<CtrlMethod::kPullKV>*)>,
             std::function<void(CtrlCall<CtrlMethod::kBarrier>*)>,
						 std::function<void(CtrlCall<CtrlMethod::kClearKV>*)>>
      handlers_;
  std::unique_ptr<CtrlService::AsyncService> grpc_service_;
  std::unique_ptr<grpc::ServerCompletionQueue> cq_;
  std::unique_ptr<grpc::Server> grpc_server_;
  std::thread loop_thread_;
  bool is_first_connect_;
  std::string this_machine_addr_;
};
}  // namespace comm_network
