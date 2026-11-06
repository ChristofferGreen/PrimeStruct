// The host side of bytes_host.prime: byte buffers arrive as `uint8_t *` checked to cover their length.
#include <cstdint>

extern "C" {

void host_fill(uint8_t *buffer, uint64_t length) {
  for (uint64_t i = 0; i < length; ++i) {
    buffer[i] = static_cast<uint8_t>(i + 1);
  }
}

int32_t host_sum(uint8_t *buffer, uint64_t length) {
  int32_t total = 0;
  for (uint64_t i = 0; i < length; ++i) {
    total += buffer[i];
  }
  return total;
}
}
