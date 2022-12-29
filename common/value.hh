#pragma once

#include <cstring>
#include <span>
#include <stdexcept>
#include <string_view>
#include <type_traits>
#include <utility>

#include "nf7.hh"


namespace pp {

struct ConstValue {
 public:
  struct Pulse {};

  ConstValue() = delete;
  ConstValue(const nf7_value_t* ptr) noexcept : ptr_(ptr) {
  }
  ConstValue(const ConstValue&) = default;
  ConstValue(ConstValue&&) = default;
  ConstValue& operator=(const ConstValue&) = default;
  ConstValue& operator=(ConstValue&& src) noexcept {
    ptr_ = std::exchange(src.ptr_, nullptr);
    return *this;
  }

  int64_t integer() const {
    int64_t i;
    if (nf7->value.get_integer(ptr_, &i)) {
      return i;
    }
    throw std::runtime_error {"expected integer"};
  }
  template <typename T>
  T integer() const {
    return SafeCast<T>(integer());
  }
  template <typename T>
  T integerOrScalar() const {
    int64_t i;
    if (nf7->value.get_integer(ptr_, &i)) {
      return static_cast<T>(i);
    }
    double f;
    if (nf7->value.get_scalar(ptr_, &f)) {
      return static_cast<T>(f);
    }
    throw std::runtime_error {"expected integer or scalar"};
  }

  double scalar() const {
    double f;
    if (nf7->value.get_scalar(ptr_, &f)) {
      return f;
    }
    throw std::runtime_error {"expected scalar"};
  }
  template <typename T>
  T scalar() const {
    return SafeCast<T>(scalar());
  }
  template <typename T>
  T scalarOrInteger() const {
    double f;
    if (nf7->value.get_scalar(ptr_, &f)) {
      return static_cast<T>(f);
    }
    int64_t i;
    if (nf7->value.get_integer(ptr_, &i)) {
      return static_cast<T>(i);
    }
    throw std::runtime_error {"expected scalar or integer"};
  }

  std::string_view string() const {
    size_t n;
    if (const char* ret = nf7->value.get_string(ptr_, &n)) {
      return std::string_view(ret, n);
    }
    throw std::runtime_error {"expected string"};
  }
  std::string_view stringOrVector() const {
    size_t n;
    if (const char* ret = nf7->value.get_string(ptr_, &n)) {
      return std::string_view(ret, n);
    }
    if (const uint8_t* ret = nf7->value.get_vector(ptr_, &n)) {
      return std::string_view(reinterpret_cast<const char*>(ret), n);
    }
    throw std::runtime_error {"expected string or vector"};
  }

  std::span<const uint8_t> vector() const {
    size_t n;
    if (const uint8_t* ret = nf7->value.get_vector(ptr_, &n)) {
      return std::span<const uint8_t> {ret, n};
    }
    throw std::runtime_error {"expected vector"};
  }
  std::span<const uint8_t> vectorOrString() const {
    size_t n;
    if (const uint8_t* ret = nf7->value.get_vector(ptr_, &n)) {
      return std::span<const uint8_t>(ret, n);
    }
    if (const char* ret = nf7->value.get_string(ptr_, &n)) {
      return std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(ret), n);
    }
    throw std::runtime_error {"expected vector or string"};
  }

  ConstValue operator[](const char* name) const {
    if (auto ret = nf7->value.get_tuple(ptr_, name)) {
      return ret;
    }
    throw std::runtime_error {"missing tuple field"};
  }

  uint8_t type() const noexcept { return nf7->value.get_type(ptr_); }
  const nf7_value_t* ptr() const noexcept { return ptr_; }

 private:
  const nf7_value_t* ptr_ = nullptr;


  template <typename R, typename N>
  static R SafeCast(N in) {
    const auto ret  = static_cast<R>(in);
    const auto retn = static_cast<N>(ret);
    if constexpr (std::is_unsigned<R>::value) {
      if (in < 0) {
        throw std::runtime_error {"integer underflow"};
      }
    }
    if constexpr (std::is_integral<R>::value && std::is_integral<N>::value) {
      if (in != retn) {
        throw std::runtime_error {"integer out of range"};
      }
    }
    if constexpr (std::is_integral<R>::value && std::is_floating_point<N>::value) {
      if (std::max(retn, in) - std::min(retn, in) > 1) {
        throw std::runtime_error {"bad precision while conversion of floating point"};
      }
    }
    return ret;
  }
};


struct MutValue : public ConstValue {
 public:
  MutValue() = delete;
  MutValue(const nf7_value_t*) = delete;
  MutValue(nf7_value_t* ptr) noexcept : ConstValue(ptr), ptr_(ptr) {
  }
  MutValue(const MutValue& src) noexcept : ConstValue(src.ptr_), ptr_(src.ptr_) {
  }
  MutValue(MutValue&& src) noexcept : ConstValue(src.ptr_), ptr_(src.ptr_) {
    src.ptr_ = nullptr;
  }
  MutValue& operator=(const MutValue& src) noexcept {
    ptr_ = src.ptr_;
    ConstValue::operator=(src);
    return *this;
  }
  MutValue& operator=(MutValue&& src) noexcept {
    ptr_ = std::exchange(src.ptr_, nullptr);
    ConstValue::operator=(std::move(src));
    return *this;
  }

  MutValue& operator=(Pulse) noexcept {
    nf7->value.set_pulse(ptr_);
    return *this;
  }
  MutValue& operator=(bool b) noexcept {
    nf7->value.set_boolean(ptr_, b);
    return *this;
  }
  MutValue& operator=(int64_t i) noexcept {
    nf7->value.set_integer(ptr_, i);
    return *this;
  }
  MutValue& operator=(double f) noexcept {
    nf7->value.set_scalar(ptr_, f);
    return *this;
  }
  MutValue& operator=(const char* v) noexcept {
    return operator=(std::string_view {v});
  }
  MutValue& operator=(std::string_view v) noexcept {
    auto ptr = nf7->value.set_string(ptr_, v.size());
    std::memcpy(ptr, v.data(), v.size());
    return *this;
  }

  char* AllocateString(size_t n) noexcept {
    return nf7->value.set_string(ptr_, n);
  }
  uint8_t* AllocateVector(size_t n) noexcept {
    return nf7->value.set_vector(ptr_, n);
  }
  void AllocateTuple(const char** names, nf7_value_t** ret) noexcept {
    nf7->value.set_tuple(ptr_, names, ret);
  }

  nf7_value_t* ptr() noexcept { return ptr_; }

 private:
  nf7_value_t* ptr_;
};


struct UniqValue : public MutValue {
 public:
  UniqValue() = delete;
  UniqValue(const nf7_value_t* v) noexcept :
      MutValue(nf7->value.create(v)) {
  }
  UniqValue(const ConstValue& v) noexcept :
      UniqValue(v.ptr()) {
  }
  ~UniqValue() noexcept {
    Destroy();
  }
  UniqValue(const UniqValue&) = delete;
  UniqValue(UniqValue&& src) noexcept : MutValue(std::move(src)) {
  }
  UniqValue& operator=(const UniqValue&) = delete;
  UniqValue& operator=(UniqValue&& src) noexcept {
    Destroy();
    MutValue::operator=(std::move(src));
    return *this;
  }

 private:
  void Destroy() noexcept {
    if (ptr()) nf7->value.destroy(ptr());
  }
};

}  // namespace pp
