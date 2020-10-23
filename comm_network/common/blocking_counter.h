#pragma once

#include "comm_network/common/utils.h"

namespace comm_network {

class BlockingCounter final {
 public:
  CN_DISALLOW_COPY_AND_MOVE(BlockingCounter);
  BlockingCounter() = delete;
  ~BlockingCounter() = default;

  BlockingCounter(int64_t cnt_val) { cnt_val_ = cnt_val; }

  int64_t Decrease() {
    std::unique_lock<std::mutex> lck(mtx_);
    cnt_val_ -= 1;
    if (cnt_val_ == 0) { cond_.notify_all(); }
    return cnt_val_;
  }
  void WaitUntilCntEqualZero() {
    std::unique_lock<std::mutex> lck(mtx_);
    cond_.wait(lck, [this]() { return cnt_val_ == 0; });
  }

 private:
  std::mutex mtx_;
  std::condition_variable cond_;
  int64_t cnt_val_;
};

}  // namespace comm_network
