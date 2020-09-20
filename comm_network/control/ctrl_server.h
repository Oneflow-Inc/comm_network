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
  class CallData {
   public:
    CallData(CtrlService::AsyncService* service, grpc::ServerCompletionQueue* cq)
        : service_(service), cq_(cq), responder_(&ctx_), status_(CREATE) {
				Proceed();
    }
		void Proceed() {
					if (status_ == CREATE) {
						status_ = PROCESS;
						service_->RequestPushKV(&ctx_, &request_, &responder_, cq_, cq_, this);
					} else if(status_ == PROCESS) {
						new CallData(service_, cq_);
						status_ = FINISH;
						responder_.Finish(response_, grpc::Status::OK, this);
					} else {
						CHECK(status_ == FINISH);
						delete this;
					}
		}
		private:
			CtrlService::AsyncService* service_;
			grpc::ServerCompletionQueue* cq_;
			grpc::ServerContext ctx_;
			PushKVRequest request_;
			PushKVResponse response_;
			grpc::ServerAsyncResponseWriter<PushKVResponse> responder_;
			enum CallStatus { CREATE, PROCESS, FINISH };
			CallStatus status_;
  };
  void HandleRpcs();
  CtrlService::AsyncService service_;
  std::unique_ptr<grpc::ServerCompletionQueue> cq_;
	std::thread loop_thread_;
  std::unique_ptr<grpc::Server> grpc_server_;
};
}  // namespace comm_network
