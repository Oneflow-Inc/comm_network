
#include "comm_network/ibverbs/ibverbs_memory_desc.h"
#include "comm_network/comm_network_config_desc.h"

namespace comm_network {

IBVerbsMemDesc::IBVerbsMemDesc(ibv_pd* pd, void* mem_ptr, size_t byte_size) {
  CHECK_GE(byte_size, 1);
  size_t sge_bytes = Global<CommNetConfigDesc>::Get()->SgeBytes();
  size_t block_num = (byte_size - 1) / sge_bytes + 1;
  sge_vec_.reserve(block_num);
  mr_vec_.reserve(block_num);
  char* ch_mem_ptr = reinterpret_cast<char*>(mem_ptr);
  while (byte_size > 0) {
    size_t cur_size = std::min<size_t>(byte_size, sge_bytes);
    ibv_mr* cur_mr =
        ibv_reg_mr(pd, ch_mem_ptr, cur_size,
                   IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_WRITE | IBV_ACCESS_REMOTE_READ);
    CHECK(cur_mr);
    mr_vec_.push_back(cur_mr);
    ibv_sge cur_sge;
    cur_sge.addr = reinterpret_cast<uint64_t>(ch_mem_ptr);
    cur_sge.length = cur_size;
    cur_sge.lkey = cur_mr->lkey;
    sge_vec_.push_back(cur_sge);
    ch_mem_ptr += cur_size;
    byte_size -= cur_size;
  }
  CHECK_EQ(byte_size, 0);
  CHECK_EQ(block_num, sge_vec_.size());
  CHECK_EQ(block_num, mr_vec_.size());
}

IBVerbsMemDesc::~IBVerbsMemDesc() {
  for (ibv_mr* mr : mr_vec_) { CHECK_EQ(ibv_dereg_mr(mr), 0); }
}

IBVerbsMemDescProto IBVerbsMemDesc::ToProto() {
  IBVerbsMemDescProto proto;
  for (const ibv_sge& sge : sge_vec_) { proto.add_mem_ptr(sge.addr); }
  for (ibv_mr* mr : mr_vec_) { proto.add_mr_rkey(mr->rkey); }
  return proto;
}

void IBVerbsMemDesc::ToProto(IBVerbsMemDescProto* proto) {
  for (const ibv_sge& sge : sge_vec_) { proto->add_mem_ptr(sge.addr); }
  for (ibv_mr* mr : mr_vec_) { proto->add_mr_rkey(mr->rkey); }
}

}  // namespace comm_network
