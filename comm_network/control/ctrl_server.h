#pragma once
#include <grpcpp/grpcpp.h>
#include "comm_network/control/control.grpc.pb.h"
#include "comm_network/utils.h"

namespace comm_network {

class CtrlServer {
 public:
  CtrlServer();
  ~CtrlServer();
  std::unordered_map<std::string, std::string> get_kv() const { return kv_; }

 private:
  class CallData {
   public:
    CallData(CtrlService::AsyncService* service, grpc::ServerCompletionQueue* cq)
        : service_(service), cq_(cq), responder_(&ctx_), status_(CREATE) {
      Proceed();
    }
    const PushKVRequest& request() const { return request_; }
    void set_request_handler(std::function<void(CallData*)> val) { request_handler_ = val; }

    void Proceed() {
      if (status_ == CREATE) {
        status_ = PROCESS;
        service_->RequestPushKV(&ctx_, &request_, &responder_, cq_, cq_, this);
      } else if (status_ == PROCESS) {
        request_handler_(this);
        auto call = new CallData(service_, cq_);
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
    std::function<void(CallData*)> request_handler_;
  };
  void HandleRpcs();
  CtrlService::AsyncService service_;
  std::unique_ptr<grpc::ServerCompletionQueue> cq_;
  std::thread loop_thread_;
  std::unique_ptr<grpc::Server> grpc_server_;
  std::unordered_map<std::string, std::string> kv_;
};
}  // namespace comm_network
