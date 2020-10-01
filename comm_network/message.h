#pragma once

namespace comm_network {
class Msg {
 public:
  Msg() = default;
	Msg(void* src_addr, size_t src_data_size) : src_addr_(src_addr), src_data_size_(src_data_size) {}
  ~Msg() = default;
  int64_t src_id() const { return src_id_; }
  int64_t dst_id() const { return dst_id_; }

 private:
	void* src_addr_;
	size_t src_data_size_;
  int64_t src_id_;
  int64_t dst_id_;
};
}  // namespace comm_network
