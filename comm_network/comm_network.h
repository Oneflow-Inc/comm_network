#pragma once
#include <mutex>
#include <thread>
#include <unordered_set>

namespace comm_network {
class CommNet {
 public:
  CommNet(const CommNet&) = delete;
  CommNet& operator=(const CommNet&) = delete;
  CommNet(CommNet&&) = delete;
  CommNet& operator=(CommNet&&) = delete;
  CommNet();
  virtual ~CommNet();

  virtual void* RegisterMemory(void* ptr, size_t byte_size) = 0;
  virtual void UnRegisterMemory(void* token) = 0;
  virtual void RegisterMemoryDone() = 0;

  void Read(void* read_id, int64_t src_machine_id, void* src_token,
            void* dst_token);
  void AddReadCallBack(void* read_id, std::function<void()> callback);
  void ReadDone(void* read_id);
  virtual void SendMsg(int64_t dst_machine_id, const Msg& msg) = 0;

 protected:
  const HashSet<int64_t>& peer_machine_id() { return peer_machine_id_; }
  virtual void DoRead(void* read_id, int64_t src_machine_id, void* src_token,
                      void* dst_token) = 0;

 private:
  std::unordered_set<int64_t> peer_machine_id_;
};

template <typename MemDescType>
class CommNetIf : public CommNet {
 public:
  CommNetIf(const CommNetIf&) = delete;
  CommNetIf& operator=(const CommNetIf&) = delete;
  CommNetIf(CommNetIf&&) = delete;
  CommNetIf& operator=(CommNetIf&&) = delete;
  CommNetIf() = default;
  virtual ~CommNetIf() {}

  void* RegisterMemory(void* ptr, size_t byte_size) override {
    MemDescType* mem_desc = NewMemDesc(ptr, byte_size);
    std::unique_lock<std::mutex> lck(mem_descs_mtx_);
    CHECK(mem_descs_.insert(mem_desc).second);
    return mem_desc;
  }

  void UnRegisterMemory(void* token) override {
    MemDescType* mem_desc = static_cast<MemDescType*>(token);
    delete mem_desc;
    std::unique_lock<std::mutex> lck(mem_descs_mtx_);
    CHECK_EQ(mem_descs_.erase(mem_desc), 1);
  }

 protected:
  virtual MemDescType* NewMemDesc(void* ptr, size_t byte_size) = 0;
  const HashSet<MemDescType*>& mem_descs() { return mem_descs_; }

 private:
  std::mutex mem_descs_mtx_;
  std::unordered_set<MemDescType*> mem_descs_;
};
}  // namespace comm_network