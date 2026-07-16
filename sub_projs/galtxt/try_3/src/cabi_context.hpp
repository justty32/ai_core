#pragma once
// cabi_context.hpp — C++ 薄鏡像的非同步控制（對應 C ABI 的 cabi_context.h）。
// trivial、內嵌物件內、免 new/free。用法：另開 thread 跑阻塞的 ask，本 thread 持同一 &ctx 戳。

#include "cabi_context.h" // llm_context_t / llm_phase_t / llm_cancel / llm_phase

namespace llm::abi {

enum class Phase { Idle, Connect, Upload, Wait, Stream, Done, Error, Cancelled };

class Client; // 前置宣告：Context 讓 Client friend 存取底層 c_（Client 定義在 cabi.hpp）

class Context {
public:
  Context() : c_{0, 0} {}
  Context(const Context &) = delete; // 控制塊不可複製/搬移（別的 thread 持著 &它）
  Context &operator=(const Context &) = delete;
  void cancel() { llm_cancel(&c_); } // 任一 thread：請求取消
  Phase phase() const {              // 任一 thread：原子讀當前階段
    return static_cast<Phase>(llm_phase(&c_));
  }

private:
  friend class Client;
  llm_context_t c_; // 就在物件裡，隨物件生滅、無 heap
};

} // namespace llm::abi
