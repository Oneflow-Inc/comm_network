#include "comm_network/control/ctrl_client.h"

namespace comm_network {
void CtrlClient::PushKV(const std::string& k, const PbMessage& v) {
  std::string o = "";
  v.SerializeToString(&o);
  PushKVRequest request;
  request.set_key(k);
  request.set_val(o);
  PushKVResponse response;
  grpc::ClientContext context;
	grpc::CompletionQueue cq;
  grpc::Status status;
	std::unique_ptr<grpc::ClientAsyncResponseReader<PushKVResponse> > rpc(	
		stub_->PrepareAsyncPushKV(&context, request, &cq));
	rpc->StartCall();
	rpc->Finish(&response, &status, (void*)1);
	void* got_tag;
	bool ok = false;
	CHECK(cq.Next(&got_tag, &ok));
	CHECK(got_tag == (void*)1);
	CHECK(ok);
  if (status.ok()) {
    LOG(INFO) << "rpc reply message is okay";
  } else {
    LOG(INFO) << status.error_code() << ": " << status.error_message();
  }
}
}  // namespace comm_network
