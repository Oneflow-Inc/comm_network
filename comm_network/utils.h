#pragma once

#include <glog/logging.h>
#include <mutex>
#include <thread>
#include <string>
#include <unordered_set>
#include <vector>
#include <queue>
#include <memory>

namespace comm_network {
const size_t rdma_recv_msg_buf_byte = 6 * 1024 * 1024;
const size_t rdma_mem_block_byte = 8 * 1024 * 1024;
const int rdma_mem_block_num = 2;

#define DISALLOW_COPY(ClassName)        \
  ClassName(const ClassName&) = delete; \
  ClassName& operator=(const ClassName&) = delete;

#define DISALLOW_MOVE(ClassName)   \
  ClassName(ClassName&&) = delete; \
  ClassName& operator=(ClassName&&) = delete;

#define DISALLOW_COPY_AND_MOVE(ClassName) \
  DISALLOW_COPY(ClassName)                \
  DISALLOW_MOVE(ClassName)

#define FOR_RANGE(type, i, begin, end) for (type i = (begin), __end = (end); i < __end; ++i)
}  // namespace comm_network
