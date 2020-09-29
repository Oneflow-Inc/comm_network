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
		stubs_.push_back(CtrlService::NewStub(grpc::CreateCustomChannel(addr, grpc::InsecureChannelCredentials(), ch_args)));	
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

  // grpc::ClientContext context;
	// grpc::CompletionQueue cq;
  // grpc::Status status;
	// std::unique_ptr<grpc::ClientAsyncResponseReader<PushKVResponse> > rpc(	
	// 	stub_->PrepareAsyncPushKV(&context, request, &cq));
	// rpc->StartCall();
	// rpc->Finish(&response, &status, (void*)1);
	// void* got_tag;
	// bool ok = false;
	// CHECK(cq.Next(&got_tag, &ok));
	// CHECK(got_tag == (void*)1);
	// CHECK(ok);
  // if (status.ok()) {
  //   LOG(INFO) << "rpc reply message is okay";
  // } else {
  //   LOG(INFO) << status.error_code() << ": " << status.error_message();
  // }
}
}  // namespace comm_network
