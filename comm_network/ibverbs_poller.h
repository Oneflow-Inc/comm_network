#pragma once
#include <infiniband/verbs.h>
#include "comm_network/common/utils.h"
#include "comm_network/message.h"

namespace comm_network {
class IBVerbsPoller {
 public:
  CN_DISALLOW_COPY_AND_MOVE(IBVerbsPoller);
  IBVerbsPoller(ibv_cq* cq);
  ~IBVerbsPoller() = default;
  void Start();
  void Stop();

 private:
  void PollCQ();
  static const int32_t max_poll_wc_num_;
  std::thread poll_thread_;
  std::atomic_flag poll_exit_flag_;
  ibv_cq* cq_;
};
}