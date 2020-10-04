#pragma once

#include <infiniband/verbs.h>
#include "comm_network/ibverbs.pb.h"
#include "comm_network/utils.h"

namespace comm_network {
class IBVerbsMemDesc final {
 public:
  DISALLOW_COPY_AND_MOVE(IBVerbsMemDesc);
  IBVerbsMemDesc() = delete;
  IBVerbsMemDesc(ibv_pd* pd, void* mem_ptr, size_t byte_size);
  ~IBVerbsMemDesc();

  const std::vector<ibv_sge>& sge_vec() const { return sge_vec_; }

  IBVerbsMemDescProto ToProto();

 private:
  std::vector<ibv_sge> sge_vec_;
  std::vector<ibv_mr*> mr_vec_;
};

}  // namespace comm_network