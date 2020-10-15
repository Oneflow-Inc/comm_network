#include "comm_network/ibverbs_read_helper.h"
#include "comm_network/message.h"
#include "comm_network/ibverbs_comm_network.h"

namespace comm_network {

void IBVerbsReadHelper::AsyncRead(uint32_t read_id, uint8_t buffer_id) {
  // get work record using read_id
  Msg cur_msg = Global<IBVerbsCommNet>::Get()->GetWorkRecord(read_id);
  int64_t src_machine_id = cur_msg.work_record.machine_id;
  IBVerbsMemDesc* recv_mem_desc = Global<IBVerbsCommNet>::Get()->GetRecvMemDescForReceiver(src_machine_id, buffer_id);
  // register memory to normal memory
  ibv_sge cur_sge = recv_mem_desc->sge_vec().at(0);
  size_t offset = *reinterpret_cast<size_t*>(cur_sge.addr);
  void* dst_addr = reinterpret_cast<char*>(cur_msg.work_record.begin_addr) + offset;
  size_t data_size = cur_msg.work_record.data_size;
  size_t transfer_size = std::min(data_size - offset, buffer_size - sizeof(size_t));
  memcpy(dst_addr, reinterpret_cast<void*>(cur_sge.addr + sizeof(size_t)), transfer_size);
  bool is_last = (offset + transfer_size < data_size) ? false : true;
  Global<IBVerbsCommNet>::Get()->Register2NormalDone(src_machine_id, buffer_id, read_id, is_last);
}

}
