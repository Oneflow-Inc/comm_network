#pragma once

#include <grpc++/grpc++.h>
#include <grpc++/impl/codegen/async_stream.h>
#include <grpc++/impl/codegen/async_unary_call.h>
#include <grpc++/impl/codegen/proto_utils.h>
#include <grpc++/impl/codegen/rpc_method.h>
#include <grpc++/impl/codegen/service_type.h>
#include <grpc++/impl/codegen/status.h>
#include <grpc++/impl/codegen/stub_options.h>
#include <grpc++/impl/codegen/sync_stream.h>
#include <grpc++/impl/codegen/client_unary_call.h>
#include "comm_network/common/preprocessor.h"
#include "comm_network/common/utils.h"
#include "comm_network/control/control.pb.h"

namespace comm_network {

#define CTRL_METHOD_SEQ            \
  CN_PP_MAKE_TUPLE_SEQ(LoadServer) \
  CN_PP_MAKE_TUPLE_SEQ(Barrier)    \
  CN_PP_MAKE_TUPLE_SEQ(PushKV)     \
  CN_PP_MAKE_TUPLE_SEQ(ClearKV)    \
  CN_PP_MAKE_TUPLE_SEQ(PullKV)

#define CatRequest(method) method##Request,
#define CatReqponse(method) method##Response,
#define CatEnum(method) k##method,
#define CatName(method) "/comm_network.CtrlService/" CN_PP_STRINGIZE(method),

#define MAKE_META_DATA()                                                                       \
  enum class CtrlMethod { CN_PP_FOR_EACH_TUPLE(CatEnum, CTRL_METHOD_SEQ) };                    \
  static const char* g_method_name[] = {CN_PP_FOR_EACH_TUPLE(CatName, CTRL_METHOD_SEQ)};       \
  using CtrlRequestTuple = std::tuple<CN_PP_FOR_EACH_TUPLE(CatRequest, CTRL_METHOD_SEQ) void>; \
  using CtrlResponseTuple = std::tuple<CN_PP_FOR_EACH_TUPLE(CatReqponse, CTRL_METHOD_SEQ) void>;

MAKE_META_DATA()

constexpr const size_t kCtrlMethodNum = CN_PP_SEQ_SIZE(CTRL_METHOD_SEQ);

template<CtrlMethod ctrl_method>
using CtrlRequest =
    typename std::tuple_element<static_cast<size_t>(ctrl_method), CtrlRequestTuple>::type;

template<CtrlMethod ctrl_method>
using CtrlResponse =
    typename std::tuple_element<static_cast<size_t>(ctrl_method), CtrlResponseTuple>::type;

inline const char* GetMethodName(CtrlMethod method) {
  return g_method_name[static_cast<int32_t>(method)];
}

class CtrlService final {
 public:
  class Stub final {
   public:
    Stub(std::shared_ptr<grpc::ChannelInterface> channel);

    template<CtrlMethod ctrl_method>
    grpc::Status CallMethod(grpc::ClientContext* context, const CtrlRequest<ctrl_method>& request,
                            CtrlResponse<ctrl_method>* response) {
      return grpc::internal::BlockingUnaryCall(channel_.get(),
                                               rpcmethods_.at(static_cast<size_t>(ctrl_method)),
                                               context, request, response);
    }

   private:
    std::array<const grpc::internal::RpcMethod, kCtrlMethodNum> rpcmethods_;

    std::shared_ptr<grpc::ChannelInterface> channel_;
  };

  static std::unique_ptr<Stub> NewStub(const std::string& addr);

  class AsyncService final : public grpc::Service {
   public:
    AsyncService();
    ~AsyncService() = default;
    using grpc::Service::RequestAsyncUnary;
  };
};
}