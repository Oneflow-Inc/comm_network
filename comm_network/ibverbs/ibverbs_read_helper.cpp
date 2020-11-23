#include "comm_network/ibverbs/ibverbs_read_helper.h"
#include "comm_network/message.h"
#include "comm_network/ibverbs/ibverbs_comm_network.h"
#include "comm_network/comm_network_config_desc.h"

namespace comm_network {

void IBVerbsReadHelper::SyncRead(int32_t sge_num, int32_t buffer_id,
                                 IBVerbsMemDesc* recv_mem_desc) {
  // get work record using read_id
  WorkRecord cur_msg = Global<IBVerbsCommNet>::Get()->GetWorkRecord();
  int64_t src_machine_id = cur_msg.machine_id;
  // register memory to normal memory
  size_t offset = cur_msg.offset;
  void* dst_addr = reinterpret_cast<char*>(cur_msg.begin_addr) + offset;
  size_t bytes = cur_msg.bytes;
  size_t transfer_bytes =
      std::min(bytes - offset, Global<CommNetConfigDesc>::Get()->PerRegisterBufferMBytes());
  size_t transfer_record = 0;
  size_t sge_bytes = Global<CommNetConfigDesc>::Get()->SgeBytes();
  for (int i = 0; i < sge_num; i++) {
    ibv_sge cur_sge = recv_mem_desc->sge_vec().at(i);
    size_t temp_bytes = std::min(sge_bytes, transfer_bytes - transfer_record);
    memcpy(dst_addr, reinterpret_cast<void*>(cur_sge.addr), temp_bytes);
    transfer_record += temp_bytes;
  }
  Global<IBVerbsCommNet>::Get()->SetWorkRecordOffset(offset + transfer_bytes);
  bool is_last = (offset + transfer_bytes < bytes) ? false : true;
  Global<IBVerbsCommNet>::Get()->Register2NormalDone(src_machine_id, buffer_id, is_last);
}

}  // namespace comm_network
