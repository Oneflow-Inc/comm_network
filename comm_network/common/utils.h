#pragma once

#include <glog/logging.h>
#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <forward_list>
#include <fstream>
#include <functional>
#include <list>
#include <memory>
#include <mutex>
#include <queue>
#include <random>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <utility>

#include "comm_network/common/global.h"
#include "comm_network/common/cplusplus_14.h"

namespace comm_network {

#define CN_DISALLOW_COPY(ClassName)     \
  ClassName(const ClassName&) = delete; \
  ClassName& operator=(const ClassName&) = delete;

#define CN_DISALLOW_MOVE(ClassName) \
  ClassName(ClassName&&) = delete;  \
  ClassName& operator=(ClassName&&) = delete;

#define CN_DISALLOW_COPY_AND_MOVE(ClassName) \
  CN_DISALLOW_COPY(ClassName)                \
  CN_DISALLOW_MOVE(ClassName)

#define FOR_RANGE(type, i, begin, end) for (type i = (begin), __end = (end); i < __end; ++i)

#define UNIMPLEMENTED() LOG(FATAL) << "UNIMPLEMENTED"

inline uint32_t NewRandomSeed() {
  static std::mt19937 gen{std::random_device{}()};
  return gen();
}

}  // namespace comm_network
