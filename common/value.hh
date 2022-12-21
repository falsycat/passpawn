#pragma once

#include <type_traits>
#include <stdexcept>

#include "nf7.hh"


namespace pp {

template <typename T>
T Get(const nf7_value_t* v) {
  if constexpr (std::is_integral_v<T>) {
    int64_t i;
    if (nf7->value.get_integer(v, &i)) {
      return static_cast<T>(i);
    }
  } else if constexpr (std::is_same_v<T, std::string>) {
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

}  // namespace pp
