#pragma once

#include <cstring>
#include <stdexcept>
#include <type_traits>
#include <utility>

#include "nf7.hh"


namespace pp {

template <typename T>
T Get(const nf7_value_t* v) {
  if constexpr (std::is_integral_v<T>) {
    int64_t i;
    if (nf7->value.get_integer(v, &i)) {
      return static_cast<T>(i);
    }
  } else if constexpr (std::is_same_v<T, std::string> ||
                       std::is_same_v<T, const char*>) {
    if (auto str = nf7->value.get_string(v, nullptr)) {
      return str;
    }
  } else {
    []<bool F = false>() { static_assert(F, "unknown type"); }();
  }
  throw std::runtime_error {"incompatible type"};
}

inline void Set(nf7_value_t* v, int64_t i) noexcept {
  nf7->value.set_integer(v, i);
}
inline void Set(nf7_value_t* v, std::string_view str) noexcept {
  const size_t n = str.size();
  char* ptr = nf7->value.set_string(v, n);
  std::memcpy(ptr, str.data(), n);
}

struct UniqValue final {
 public:
  UniqValue() = delete;
  UniqValue(const nf7_value_t* v) noexcept : ptr_(nf7->value.create(v)) {
  }
  ~UniqValue() noexcept {
    nf7->value.destroy(ptr_);
  }
  UniqValue(const UniqValue&) = delete;
  UniqValue(UniqValue&& src) noexcept {
    std::swap(src.ptr_, ptr_);
  }
  UniqValue& operator=(const UniqValue&) = delete;
  UniqValue& operator=(UniqValue&& src) noexcept {
    std::swap(src.ptr_, ptr_);
    return *this;
  }

  nf7_value_t* get() const noexcept { return ptr_; }

 private:
  nf7_value_t* ptr_ = nullptr;
};

}  // namespace pp
