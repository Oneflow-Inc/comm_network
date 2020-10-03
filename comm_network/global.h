#pragma once

namespace comm_network {
template<typename T, typename Kind = void>
class Global final {
 public:
  static T* Get() { return *GetPPtr(); }
  static void SetAllocated(T* val) { *GetPPtr() = val; }
  template<typename... Args>
  static void New(Args&&... args) {
    CHECK(Get() == nullptr);
    LOG(INFO) << "NewGlobal " << typeid(T).name();
    *GetPPtr() = new T(std::forward<Args>(args)...);
  }
  static void Delete() {
    if (Get() != nullptr) {
      LOG(INFO) << "DeleteGlobal " << typeid(T).name();
      delete Get();
      *GetPPtr() = nullptr;
    }
  }

 private:
  static T** GetPPtr() {
    CheckKind();
    static T* ptr = nullptr;
    return &ptr;
  }
  static void CheckKind() {
    if (!std::is_same<Kind, void>::value) {
      CHECK(Global<T>::Get() == nullptr)
          << typeid(Global<T>).name() << " are disable for avoiding misuse";
    }
  }
};
}