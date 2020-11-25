#include "comm_network/ibverbs/ibverbs_poller.h"
#include "comm_network/ibverbs/ibverbs_qp.h"

namespace comm_network {
IBVerbsPoller::IBVerbsPoller(ibv_cq* cq) : poll_exit_flag_(ATOMIC_FLAG_INIT) { cq_ = cq; }

void IBVerbsPoller::Start() { poll_thread_ = std::thread(&IBVerbsPoller::PollCQ, this); }

void IBVerbsPoller::Stop() {
  while (poll_exit_flag_.test_and_set() == true) {}
  poll_thread_.join();
}

void IBVerbsPoller::PollCQ() {
  std::vector<ibv_wc> wc_vec(max_poll_wc_num_);
  while (poll_exit_flag_.test_and_set() == false) {
    poll_exit_flag_.clear();
    int32_t found_wc_num = ibv_poll_cq(cq_, max_poll_wc_num_, wc_vec.data());
    CHECK_GE(found_wc_num, 0);
    FOR_RANGE(int32_t, i, 0, found_wc_num) {
      const ibv_wc& wc = wc_vec.at(i);
      CHECK_EQ(wc.status, IBV_WC_SUCCESS) << wc.opcode;
      WorkRequestId* wr_id = reinterpret_cast<WorkRequestId*>(wc.wr_id);
      IBVerbsQP* qp = wr_id->qp;
      switch (wc.opcode) {
        case IBV_WC_SEND: {
          qp->SendDone(wr_id);
          break;
        }
        case IBV_WC_RDMA_WRITE: {
          qp->RDMAWriteDone(wr_id);
          break;
        }
        case IBV_WC_RECV_RDMA_WITH_IMM: {
          qp->RDMARecvDone(wr_id, wc.imm_data);
          break;
        }
        case IBV_WC_RECV: {
          qp->RecvDone(wr_id, wc.imm_data);
          break;
        }
        default: {
          UNIMPLEMENTED();
          break;
        }
      }
    }
  }
}

const int32_t IBVerbsPoller::max_poll_wc_num_ = 32;
}  // namespace comm_network