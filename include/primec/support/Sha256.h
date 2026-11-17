#pragma once

#include <string>
#include <string_view>

namespace primec {

// SHA-256 of `bytes` as 64 lowercase hex digits.
std::string sha256Hex(std::string_view bytes);

// SHA-256 of the file at `path`. Returns false (and leaves `hex` empty) when the file cannot be read.
bool sha256FileHex(const std::string &path, std::string &hex);

// Incremental SHA-256, for inputs that should not be held in memory at once.
class Sha256 {
public:
  Sha256();
  void update(const char *data, size_t size);
  // Finishes the hash; the object is not usable afterwards.
  std::string finishHex();

private:
  void block(const unsigned char *chunk);
  unsigned state_[8];
  unsigned char buffer_[64];
  size_t buffered_ = 0;
  unsigned long long total_ = 0;
};

} // namespace primec
