#pragma once

namespace comm_network {
enum class MsgType { DataIsReady, PleaseWrite }; 

class Msg {
 public:
  Msg() = default;
	Msg(int64_t src_id, int64_t dst_id, void* src_addr, size_t src_data_size, MsgType msg_type) 
		: src_id_(src_id), dst_id_(dst_id), src_addr_(src_addr), 
			src_data_size_(src_data_size), msg_type_(msg_type) {}
  ~Msg() = default;
  int64_t src_id() const { return src_id_; }
  int64_t dst_id() const { return dst_id_; }
	size_t data_size() const { return src_data_size_; }
	void* src_addr() const { return src_addr_; }
	MsgType msg_type() const { return msg_type_; }

 private:
	MsgType msg_type_;
	void* src_addr_;
	size_t src_data_size_;
  int64_t src_id_;
  int64_t dst_id_;
};
}  // namespace comm_network
