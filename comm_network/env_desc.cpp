#include <fstream>
#include <string>
#include "comm_network/env_desc.h"

namespace comm_network {
EnvDesc::EnvDesc(std::string config_file, int64_t machine_id) {
  this->machine_id_ = machine_id;
  std::ifstream settings(config_file);
  int id;
  std::string ip;
  while (settings >> id >> ip) { machine_cfgs_.emplace(id, ip); }
  // initialize control server
  ctrl_server_ = new CtrlServer();
	// initialize control client
//	std::string target_str = "0.0.0.0:50051";
//	ctrl_client_ = new CtrlClient(grpc::CreateChannel(
//      target_str, grpc::InsecureChannelCredentials()));
}
}  // namespace comm_network
