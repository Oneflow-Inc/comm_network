#include "comm_network/ibverbs_write_helper.h"

namespace comm_network {
IBVerbsWriteHelper::IBVerbsWriteHelper() {
  // thread_ = std::thread(&IBVerbsWriteHelper::PollQueue, this);
}

IBVerbsWriteHelper::~IBVerbsWriteHelper() {
  // thread_.join();
  // msg_queue_.Close();
}

// void IBVerbsWriteHelper::AddWork(IBVerbsQP* qp, char* src_addr, char* dst_addr, size_t data_size)
// {
//   WritePartial write_partial;
//   write_partial.qp = qp;
//   write_partial.src_addr = src_addr;
//   write_partial.dst_addr = dst_addr;
//   write_partial.data_size = data_size;
//   msg_queue_.Send(write_partial);
// }

// void IBVerbsWriteHelper::PollQueue() {
//   WritePartial write_partial;
//   while (msg_queue_.Receive(&write_partial) == kChannelStatusSuccess) {
//     int64_t src_machine_id = write_partial.src_machine_id;
//     int64_t dst_machine_id = write_partial.dst_machine_id;
//     bool has_free = false;
//     std::string send_key, recv_key;
//     do {
//       has_free = Global<IBVerbsCommNet>::Get()->FindAvailSendMemory(src_machine_id,
//       dst_machine_id, send_key, recv_key);
//     } while (!has_free);
//     IBVerbsMemDesc* send_mem_desc = Global<IBVerbsCommNet>::Get()->mem_desc()[send_key].first;
//     IBVerbsMemDescProto recv_mem_desc_proto =
//     Global<IBVerbsCommNet>::Get()->mem_desc_list()[dst_machine_id][recv_key];

//   }
// }

}  // namespace comm_network