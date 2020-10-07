#include "comm_network/control/ctrl_service.h"

namespace comm_network {

namespace {

template<size_t method_index>
const grpc::internal::RpcMethod BuildOneRpcMethod(std::shared_ptr<grpc::ChannelInterface> channel) {
  return grpc::internal::RpcMethod(GetMethodName(static_cast<CtrlMethod>(method_index)),
                                   grpc::internal::RpcMethod::NORMAL_RPC, channel);
}

template<size_t... method_indices>
std::array<const grpc::internal::RpcMethod, kCtrlMethodNum> BuildRpcMethods(
    std::index_sequence<method_indices...>, std::shared_ptr<grpc::ChannelInterface> channel) {
  return {BuildOneRpcMethod<method_indices>(channel)...};
}

}  // namespace

CtrlService::Stub::Stub(std::shared_ptr<grpc::ChannelInterface> channel)
    : rpcmethods_(BuildRpcMethods(std::make_index_sequence<kCtrlMethodNum>{}, channel)),
      channel_(channel) {}

std::unique_ptr<CtrlService::Stub> CtrlService::NewStub(const std::string& addr) {
  grpc::ChannelArguments ch_args;
  ch_args.SetInt(GRPC_ARG_MAX_MESSAGE_LENGTH, 64 * 1024 * 1024);
  return std::make_unique<Stub>(
      grpc::CreateCustomChannel(addr, grpc::InsecureChannelCredentials(), ch_args));
}

CtrlService::AsyncService::AsyncService() {
  for (int32_t i = 0; i < kCtrlMethodNum; ++i) {
    AddMethod(new grpc::internal::RpcServiceMethod(GetMethodName(static_cast<CtrlMethod>(i)),
                                                   grpc::internal::RpcMethod::NORMAL_RPC, nullptr));
    grpc::Service::MarkMethodAsync(i);
  }
}

}  // namespace comm_network
