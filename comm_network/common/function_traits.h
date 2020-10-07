#pragma onc

namespace comm_network {

template<typename... Args>
using void_t = void;

template<typename T, typename = void>
struct function_traits;

template<typename Ret, typename... Args>
struct function_traits<Ret (*)(Args...)> {
  using return_type = Ret;
  using args_type = std::tuple<Args...>;
};

template<typename Ret, typename C, typename... Args>
struct function_traits<Ret (C::*)(Args...)> {
  using return_type = Ret;
  using args_type = std::tuple<Args...>;
};

template<typename Ret, typename C, typename... Args>
struct function_traits<Ret (C::*)(Args...) const> {
  using return_type = Ret;
  using args_type = std::tuple<Args...>;
};

template<typename F>
struct function_traits<F, void_t<decltype(&F::operator())>>
    : public function_traits<decltype(&F::operator())> {};

}  // namespace comm_network
