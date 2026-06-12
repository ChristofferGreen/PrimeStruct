#pragma once

#include <cstdint>
#include <cstring>

namespace primec {

inline float bitsToF32(uint64_t raw) {
  uint32_t bits = static_cast<uint32_t>(raw);
  float value = 0.0f;
  std::memcpy(&value, &bits, sizeof(value));
  return value;
}

inline uint64_t f32ToBits(float value) {
  uint32_t bits = 0;
  std::memcpy(&bits, &value, sizeof(bits));
  return static_cast<uint64_t>(bits);
}

inline double bitsToF64(uint64_t raw) {
  double value = 0.0;
  std::memcpy(&value, &raw, sizeof(value));
  return value;
}

inline uint64_t f64ToBits(double value) {
  uint64_t bits = 0;
  std::memcpy(&bits, &value, sizeof(bits));
  return bits;
}

} // namespace primec
