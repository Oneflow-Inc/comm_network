#pragma once

namespace comm_network {
class Msg {
 public:
  Msg() = default;
  ~Msg() = default;
  int64_t src_id() const { return src_id_; }
  int64_t dst_id() const { return dst_id_; }

 private:
  int64_t src_id_;
  int64_t dst_id_;
};
}  // namespace comm_network
