#include <grpc++/alarm.h>
#include "comm_network/control/ctrl_server.h"
#include "comm_network/env_desc.h"

namespace comm_network {

CtrlServer::CtrlServer() : is_first_connect_(true), this_machine_addr_("") {
  Init();
  int port = Global<EnvDesc>::Get()->ctrl_port();
  std::string server_address("0.0.0.0:" + std::to_string(port));
  grpc::ServerBuilder server_builder;
  server_builder.SetMaxMessageSize(INT_MAX);
  int bound_port = 0;
  server_builder.AddListeningPort(server_address, grpc::InsecureServerCredentials(), &bound_port);
  grpc_service_.reset(new CtrlService::AsyncService);
  server_builder.RegisterService(grpc_service_.get());
  cq_ = server_builder.AddCompletionQueue();
  grpc_server_ = server_builder.BuildAndStart();
  CHECK_EQ(port, bound_port) << "Port " << port << " is unavailable";
  LOG(INFO) << "CtrlServer listening on " << server_address;
  loop_thread_ = std::thread(&CtrlServer::HandleRpcs, this);
}

void CtrlServer::HandleRpcs() {
  EnqueueRequests();
  void* tag = nullptr;
  bool ok = false;
  bool is_shutdown = false;
  while (cq_->Next(&tag, &ok)) {
    auto call = static_cast<CtrlCallIf*>(tag);
    if (!ok) {
      CHECK(is_shutdown);
      CHECK(call);
      delete call;
      continue;
    }
    if (call) {
      call->Process();
    } else {
      CHECK(!is_shutdown);
      is_shutdown = true;
      grpc_server_->Shutdown();
      cq_->Shutdown();
    }
  }
}

CtrlServer::~CtrlServer() {
  grpc::Alarm alarm(cq_.get(), gpr_now(GPR_CLOCK_MONOTONIC), nullptr);
  loop_thread_.join();
}

void CtrlServer::Init() {
  Add([this](CtrlCall<CtrlMethod::kLoadServer>* call) {
    if (this->is_first_connect_) {
      this->this_machine_addr_ = call->request().addr();
      this->is_first_connect_ = false;
    } else {
      CHECK_EQ(call->request().addr(), this->this_machine_addr_);
    }
    call->SendResponse();
    EnqueueRequest<CtrlMethod::kLoadServer>();
  });

  Add([this](CtrlCall<CtrlMethod::kBarrier>* call) {
    const std::string& barrier_name = call->request().name();
    int32_t barrier_num = call->request().num();
    auto barrier_call_it = barrier_calls_.find(barrier_name);
    if (barrier_call_it == barrier_calls_.end()) {
      barrier_call_it =
          barrier_calls_
              .emplace(barrier_name, std::make_pair(std::list<CtrlCallIf*>{}, barrier_num))
              .first;
    }
    CHECK_EQ(barrier_num, barrier_call_it->second.second);
    barrier_call_it->second.first.push_back(call);
    if (barrier_call_it->second.first.size() == barrier_call_it->second.second) {
      for (CtrlCallIf* pending_call : barrier_call_it->second.first) {
        pending_call->SendResponse();
      }
      barrier_calls_.erase(barrier_call_it);
    }

    EnqueueRequest<CtrlMethod::kBarrier>();
  });

  Add([this](CtrlCall<CtrlMethod::kPushKV>* call) {
    const std::string& k = call->request().key();
    const std::string& v = call->request().val();
    CHECK(kv_.emplace(k, v).second);

    auto pending_kv_calls_it = pending_kv_calls_.find(k);
    if (pending_kv_calls_it != pending_kv_calls_.end()) {
      for (auto pending_call : pending_kv_calls_it->second) {
        pending_call->mut_response()->set_val(v);
        pending_call->SendResponse();
      }
      pending_kv_calls_.erase(pending_kv_calls_it);
    }
    call->SendResponse();
    EnqueueRequest<CtrlMethod::kPushKV>();
  });

  Add([this](CtrlCall<CtrlMethod::kClearKV>* call) {
    const std::string& k = call->request().key();
    CHECK_EQ(kv_.erase(k), 1);
    CHECK(pending_kv_calls_.find(k) == pending_kv_calls_.end());
    call->SendResponse();
    EnqueueRequest<CtrlMethod::kClearKV>();
  });

  Add([this](CtrlCall<CtrlMethod::kPullKV>* call) {
    const std::string& k = call->request().key();
    auto kv_it = kv_.find(k);
    if (kv_it != kv_.end()) {
      call->mut_response()->set_val(kv_it->second);
      call->SendResponse();
    } else {
      pending_kv_calls_[k].push_back(call);
    }
    EnqueueRequest<CtrlMethod::kPullKV>();
  });
}

}  // namespace comm_network
