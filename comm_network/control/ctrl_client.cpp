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
		grpc::Status status = stub_->PushKV(&context, request, &response);
		if (status.ok()) {
			LOG(INFO) << "rpc reply message is okay";
		}
		else {
			LOG(INFO) << status.error_code() << ": " << status.error_message();
		}
	}	
}
