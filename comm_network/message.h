#pragma once

namespace comm_network {
class Msg {
 public:
	Msg() = default;
	~Msg() = default;
 private:
	int64_t src_id_;
	int64_t dst_id_;

};
}  // namespace comm_network
