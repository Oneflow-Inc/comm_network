#pragma once

#include <infiniband/verbs.h>
#include "comm_network/ibverbs/ibverbs.pb.h"
#include "comm_network/common/utils.h"

namespace comm_network {
class IBVerbsMemDesc final {
 public:
  CN_DISALLOW_COPY_AND_MOVE(IBVerbsMemDesc);
  IBVerbsMemDesc() = delete;
  IBVerbsMemDesc(ibv_pd* pd, void* mem_ptr, size_t byte_size);
  ~IBVerbsMemDesc();

  const std::vector<ibv_sge>& sge_vec() const { return sge_vec_; }

  IBVerbsMemDescProto ToProto();
  void ToProto(IBVerbsMemDescProto* proto);

 private:
  std::vector<ibv_sge> sge_vec_;
  std::vector<ibv_mr*> mr_vec_;
};

}  // namespace comm_network
