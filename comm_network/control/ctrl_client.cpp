#include "comm_network/control/ctrl_client.h"

namespace comm_network {
const int32_t max_retry_num = 60;
const int64_t sleep_seconds = 10;

CtrlClient::CtrlClient(const EnvDesc* env_desc) : env_desc_(env_desc) {
  stubs_.reserve(env_desc->TotalMachineNum());
  int32_t port = -1;
  std::string addr = "";
  for (int64_t i = 0; i < env_desc->TotalMachineNum(); i++) {
    const Machine& mchn = env_desc->machine(i);
    port = (mchn.ctrl_port_agent() != -1) ? (mchn.ctrl_port_agent()) : env_desc->ctrl_port();
    addr = mchn.addr() + ":" + std::to_string(port);
    grpc::ChannelArguments ch_args;
    ch_args.SetInt(GRPC_ARG_MAX_MESSAGE_LENGTH, 64 * 1024 * 1024);
    stubs_.push_back(CtrlService::NewStub(
        grpc::CreateCustomChannel(addr, grpc::InsecureChannelCredentials(), ch_args)));
    LoadServer(mchn.addr(), stubs_[i].get());
  }
}

void CtrlClient::LoadServer(const std::string& server_addr, CtrlService::Stub* stub) {
  int32_t retry_idx = 0;
  for (; retry_idx < max_retry_num; retry_idx++) {
    grpc::ClientContext client_ctx;
    LoadServerRequest request;
    request.set_addr(server_addr);
    LoadServerResponse response;
    grpc::Status st = stub->LoadServer(&client_ctx, request, &response);
    if (st.error_code() == grpc::StatusCode::OK) {
      LOG(INFO) << "LoadServer " << server_addr << " Successful at " << retry_idx << " times";
      break;
    } else if (st.error_code() == grpc::StatusCode::UNAVAILABLE) {
      LOG(INFO) << "LoadServer " << server_addr << " Failed at " << retry_idx << " times";
      std::this_thread::sleep_for(std::chrono::seconds(sleep_seconds));
      continue;
    } else {
      LOG(FATAL) << st.error_message();
    }
  }
  CHECK_LT(retry_idx, max_retry_num);
}

CtrlService::Stub* CtrlClient::GetResponsibleStub(const std::string& key) {
  int64_t machine_id = (std::hash<std::string>{}(key)) % env_desc_->TotalMachineNum();
  return stubs_[machine_id].get();
}

void CtrlClient::PushKV(const std::string& k, const PbMessage& msg) {
  std::string v = "";
  msg.SerializeToString(&v);
  PushKVRequest request;
  request.set_key(k);
  request.set_val(v);
  PushKVResponse response;
  grpc::ClientContext client_ctx;
  CtrlService::Stub* stub = GetResponsibleStub(k);
  grpc::Status st = stub->PushKV(&client_ctx, request, &response);
  if (st.error_code() == grpc::StatusCode::OK) {
    LOG(INFO) << "PushKV " << k << " Successful.";
  } else {
    LOG(INFO) << "PushKV " << k << " Fail because " << st.error_message();
  }
}

void CtrlClient::PullKV(const std::string& k, PbMessage* msg) {
  PullKVRequest request;
  request.set_key(k);
  PullKVResponse response;
  grpc::ClientContext client_ctx;
  CtrlService::Stub* stub = GetResponsibleStub(k);
  grpc::Status st = stub->PullKV(&client_ctx, request, &response);
  if (st.error_code() == grpc::StatusCode::OK) {
    LOG(INFO) << "PullKV " << k << " Successful.";
  } else {
    LOG(INFO) << "PullKV " << k << " Fail because " << st.error_message();
  }
  msg->ParseFromString(response.val());
}

void CtrlClient::ClearKV(const std::string& k) {
  ClearKVRequest request;
  request.set_key(k);
  ClearKVResponse response;
  grpc::ClientContext client_ctx;
  CtrlService::Stub* stub = GetResponsibleStub(k);
  grpc::Status st = stub->ClearKV(&client_ctx, request, &response);
  if (st.error_code() == grpc::StatusCode::OK) {
    LOG(INFO) << "ClearKV " << k << " Successful.";
  } else {
    LOG(INFO) << "ClearKV " << k << " Fail because " << st.error_message();
  }
}

void CtrlClient::Barrier(const std::string& barrier_name, int32_t barrier_num) {
  BarrierRequest request;
  request.set_name(barrier_name);
  request.set_num(barrier_num);
  BarrierResponse response;
  grpc::ClientContext client_ctx;
  CtrlService::Stub* stub = GetMasterStub();
  grpc::Status st = stub->Barrier(&client_ctx, request, &response);
  if (st.error_code() == grpc::StatusCode::OK) {
    LOG(INFO) << "Barrier  Successful.";
  } else {
    LOG(INFO) << "Barrier Fail because " << st.error_message();
  }
}
}  // namespace comm_network
