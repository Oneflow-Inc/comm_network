#pragma once

namespace comm_network {
enum class MsgType { DataIsReady, PleaseWrite, AllocateMemory, DoWrite, FreeBufferPair};

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

struct FreeBufferPair {
	void* src_token;
	void* dst_token;
	int64_t machine_id;
};

struct Msg {
	MsgType msg_type;
	union {
		DataIsReady data_is_ready;
		PleaseWrite please_write;
		FreeBufferPair free_buffer_pair;
		AllocateMemory allocate_memory;
		DoWrite do_write;
	};
};

}  // namespace comm_network
