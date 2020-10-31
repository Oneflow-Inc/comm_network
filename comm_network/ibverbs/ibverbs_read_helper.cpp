#include "comm_network/ibverbs/ibverbs_read_helper.h"
#include "comm_network/message.h"
#include "comm_network/ibverbs/ibverbs_comm_network.h"

namespace comm_network {

void IBVerbsReadHelper::AsyncRead(uint32_t read_id, uint8_t buffer_id) {
  // get work record using read_id
  WorkRecord cur_msg = Global<IBVerbsCommNet>::Get()->GetWorkRecord(read_id);
  int64_t src_machine_id = cur_msg.machine_id;
  IBVerbsMemDesc* recv_mem_desc =
      Global<IBVerbsCommNet>::Get()->GetRecvMemDescForReceiver(src_machine_id, buffer_id);
  // register memory to normal memory
  ibv_sge cur_sge = recv_mem_desc->sge_vec().at(0);
  size_t offset = cur_msg.offset;
  void* dst_addr = reinterpret_cast<char*>(cur_msg.begin_addr) + offset;
  size_t data_size = cur_msg.data_size;
  size_t transfer_size = std::min(data_size - offset, buffer_size);
  memcpy(dst_addr, reinterpret_cast<void*>(cur_sge.addr), transfer_size);
  Global<IBVerbsCommNet>::Get()->SetWorkRecordOffset(read_id, offset + transfer_size);
  bool is_last = (offset + transfer_size < data_size) ? false : true;
  Global<IBVerbsCommNet>::Get()->Register2NormalDone(src_machine_id, buffer_id, read_id, is_last);
}

}  // namespace comm_network
