#pragma once
#include <string>

namespace comm_network {
enum class MsgType { DataIsReady, PleaseWrite, AllocateMemory, DoWrite, PartialWriteDone, FreeBufferPair, ReadDone};

struct DataIsReady {
	void* src_addr;
	size_t data_size;
	int64_t src_machine_id;
	int64_t dst_machine_id;
};

struct AllocateMemory {
	size_t data_size;
	void* src_addr;
	int64_t src_machine_id;
};

struct PleaseWrite {
	void* src_addr;
	void* dst_addr;
	size_t data_size;
	int64_t src_machine_id;
	int64_t dst_machine_id;
};

struct DoWrite {
	void* src_addr;
	void* dst_addr;
	size_t data_size;
	int64_t src_machine_id;
	int64_t dst_machine_id;	
};

struct PartialWriteDone {
	char* dst_addr;
	size_t data_size;
	int64_t src_machine_id;
	int64_t dst_machine_id; 
	int buffer_id;	
	int piece_id;
	int total_piece_num;
};

struct FreeBufferPair {
	int64_t src_machine_id;
	int64_t dst_machine_id; 
	int buffer_id;	
};

struct ReadDone {

};

struct Msg {
	MsgType msg_type;
	union {
		DataIsReady data_is_ready;
		PleaseWrite please_write;
		FreeBufferPair free_buffer_pair;
		AllocateMemory allocate_memory;
		PartialWriteDone partial_write_done;
		DoWrite do_write;
		ReadDone read_done;
	};
};

struct WritePartial {
	char* src_addr;
	char* dst_addr;
	size_t data_size;
	int64_t src_machine_id;
	int64_t dst_machine_id; 
	int buffer_id;
	int piece_id;
	int total_piece_num;
};

}  // namespace comm_network
