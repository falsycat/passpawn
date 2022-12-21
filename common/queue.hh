#pragma once

#include <mutex>
#include <optional>
#include <queue>
#include <utility>

#include "nf7.hh"

#include "common/value.hh"


namespace pp {

template <typename T>
struct Queue final {
 public:
  bool Push(T&& v) noexcept {
    std::unique_lock<std::mutex> k {mtx_};
    q_.push(std::move(v));
    return std::exchange(working_, true);
  }
  std::optional<T> Pop() noexcept {
    std::unique_lock<std::mutex> k {mtx_};
    if (q_.empty()) {
      working_ = false;
      return std::nullopt;
    }
    auto ret = std::move(q_.front());
    q_.pop();
    return ret;
  }

  template <typename U>
  void PushAndVisit(nf7_ctx_t* ctx, T&& v) {
    if (!Push(std::move(v))) {
      nf7->ctx.exec_async(ctx, this, [](auto ctx, auto ptr) {
        auto& q     = *reinterpret_cast<Queue*>(ptr);
        auto& udata = *reinterpret_cast<U*>(ctx->ptr);
        while (const auto v = q.Pop()) {
          udata(ctx, *v);
        }
      }, 0);
    }
  }

 private:
  std::mutex mtx_;
  std::queue<T> q_;
  bool working_ = false;
};

}  // namespace pp
