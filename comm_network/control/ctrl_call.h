#pragma once
#include <grpcpp/grpcpp.h>
#include "comm_network/control/control.grpc.pb.h"
#include "comm_network/utils.h"

namespace comm_network {

class CtrlCallIf {
 public:
	DISALLOW_COPY_AND_MOVE(CtrlCallIf);
	virtual ~CtrlCallIf() = default;

	virtual void Process() = 0;
  virtual void SendResponse() = 0;
 protected:
  CtrlCallIf() = default;
};

enum class CtrlMethod { kLoadServer, kPushKV, kPullKV, kBarrier};
using CtrlRequestTuple = std::tuple<LoadServerRequest, PushKVRequest, PullKVRequest, BarrierRequest>;
using CtrlResponseTuple = std::tuple<LoadServerResponse, PushKVResponse, PullKVResponse, BarrierResponse>;

template<CtrlMethod ctrl_method>
using CtrlRequest =
    typename std::tuple_element<static_cast<size_t>(ctrl_method), CtrlRequestTuple>::type;

template<CtrlMethod ctrl_method>
using CtrlResponse =
    typename std::tuple_element<static_cast<size_t>(ctrl_method), CtrlResponseTuple>::type;

template <CtrlMethod ctrl_method>
class CtrlCall final : public CtrlCallIf {
 public:
	DISALLOW_COPY_AND_MOVE(CtrlCall);
	CtrlCall() : status_(Status::kBeforeHandleRequest), responder_(&server_ctx_) {}
  ~CtrlCall() = default;

	const CtrlRequest<ctrl_method>& request() const { return request_; }
  CtrlRequest<ctrl_method>* mut_request() { return &request_; }
  CtrlResponse<ctrl_method>* mut_response() { return &response_; }
  grpc::ServerContext* mut_server_ctx() { return &server_ctx_; }
  grpc::ServerAsyncResponseWriter<CtrlResponse<ctrl_method>>* mut_responder() {
    return &responder_;
  }
  void set_request_handler(std::function<void()> val) { request_handler_ = val; }
	void Process() override {
    switch (status_) {
      case Status::kBeforeHandleRequest: {
        request_handler_();
        return;
      }
      case Status::kBeforeDelete: {
        delete this;
        return;
      }
    }
  }

  void SendResponse() override {
    responder_.Finish(response_, grpc::Status::OK, this);
    status_ = Status::kBeforeDelete;
  }

 private:
	enum class Status { kBeforeHandleRequest, kBeforeDelete };
	Status status_;
	CtrlRequest<ctrl_method> request_;
	CtrlResponse<ctrl_method> response_;
	grpc::ServerContext server_ctx_;
  grpc::ServerAsyncResponseWriter<CtrlResponse<ctrl_method>> responder_;
  std::function<void()> request_handler_;	
};
}

