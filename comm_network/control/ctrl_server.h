#pragma once
#include <grpc++/alarm.h>
#include <grpc++/server_builder.h>
#include "comm_network/control/ctrl_call.h"
#include "comm_network/common/function_traits.h"

namespace comm_network {

namespace {
template<size_t... Idx>
static std::tuple<std::function<void(CtrlCall<(CtrlMethod)Idx>*)>...> GetHandlerTuple(
    std::index_sequence<Idx...>) {
  return {};
}

template<typename... Args, typename Func, std::size_t... Idx>
void for_each_i(const std::tuple<Args...>& t, Func&& f, std::index_sequence<Idx...>) {
  (void)std::initializer_list<int>{
      (f(std::get<Idx>(t), std::integral_constant<size_t, Idx>{}), void(), 0)...};
}

}  // namespace

class CtrlServer final {
 public:
  CN_DISALLOW_COPY_AND_MOVE(CtrlServer);
  ~CtrlServer();

  CtrlServer();
  const std::string& this_machine_addr() { return this_machine_addr_; }

 private:
  void HandleRpcs();
  void Init();

  void EnqueueRequests() {
    for_each_i(handlers_, helper{this}, std::make_index_sequence<kCtrlMethodNum>{});
  }

  template<CtrlMethod kMethod>
  void EnqueueRequest() {
    constexpr const size_t I = (size_t)kMethod;
    auto handler = std::get<I>(handlers_);
    auto call = new CtrlCall<(CtrlMethod)I>();
    call->set_request_handler(std::bind(handler, call));
    grpc_service_->RequestAsyncUnary(I, call->mut_server_ctx(), call->mut_request(),
                                     call->mut_responder(), cq_.get(), cq_.get(), call);
  }

  template<typename F>
  void Add(F f) {
    using args_type = typename function_traits<F>::args_type;
    using arg_type =
        typename std::remove_pointer<typename std::tuple_element<0, args_type>::type>::type;

    std::get<arg_type::value>(handlers_) = std::move(f);
  }

  struct helper {
    helper(CtrlServer* s) : s_(s) {}
    template<typename T, typename V>
    void operator()(const T& t, V) {
      s_->EnqueueRequest<(CtrlMethod)V::value>();
    }

    CtrlServer* s_;
  };

  using HandlerTuple = decltype(GetHandlerTuple(std::make_index_sequence<kCtrlMethodNum>{}));

  HandlerTuple handlers_;
  std::unique_ptr<CtrlService::AsyncService> grpc_service_;
  std::unique_ptr<grpc::ServerCompletionQueue> cq_;
  std::unique_ptr<grpc::Server> grpc_server_;
  std::thread loop_thread_;
  // Barrier
  std::unordered_map<std::string, std::pair<std::list<CtrlCallIf*>, int32_t>> barrier_calls_;
  // PushKV, ClearKV, PullKV
  std::unordered_map<std::string, std::string> kv_;
  std::unordered_map<std::string, std::list<CtrlCall<CtrlMethod::kPullKV>*>> pending_kv_calls_;

  bool is_first_connect_;
  std::string this_machine_addr_;
};

}  // namespace comm_network
