#pragma once

#include "primec/ir/Ir.h"
#include "primec/ir/IrSerializer.h"

#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

// Bytecode fixtures must not depend on where the repository is checked out. The source map of a module records the
// file of every instruction, and stdlib files are recorded by absolute path (`/home/me/PrimeStruct/stdlib/std/...`),
// so a fixture written in one checkout would not match a compile in another (a git worktree, CI). These helpers
// rewrite those paths to start at `stdlib/` before bytes are compared or written, in single modules and in export
// bundles (`PSBN`: version, optional main module, then named export modules).
namespace embed_fixture {

inline std::string checkoutIndependentPath(const std::string &path) {
  const size_t stdlib = path.rfind("/stdlib/");
  return stdlib == std::string::npos ? path : path.substr(stdlib + 1);
}

inline bool normalizeModuleBytes(const std::vector<uint8_t> &in, std::vector<uint8_t> &out) {
  primec::IrModule module;
  std::string error;
  if (!primec::deserializeIr(in, module, error)) {
    return false;
  }
  for (primec::IrInstructionSourceMapEntry &entry : module.instructionSourceMap) {
    entry.sourceUnit = checkoutIndependentPath(entry.sourceUnit);
  }
  out.clear();
  return primec::serializeIr(module, out, error);
}

class BundleCursor {
public:
  explicit BundleCursor(const std::vector<uint8_t> &bytes) : bytes_(bytes) {}

  bool u32(uint32_t &value) {
    if (pos_ + 4 > bytes_.size()) {
      return false;
    }
    value = 0;
    for (int shift = 0; shift < 32; shift += 8) {
      value |= static_cast<uint32_t>(bytes_[pos_++]) << shift;
    }
    return true;
  }

  bool take(size_t count, std::vector<uint8_t> &out) {
    if (pos_ + count > bytes_.size()) {
      return false;
    }
    out.assign(bytes_.begin() + static_cast<std::ptrdiff_t>(pos_),
               bytes_.begin() + static_cast<std::ptrdiff_t>(pos_ + count));
    pos_ += count;
    return true;
  }

  bool blob(std::vector<uint8_t> &out) {
    uint32_t size = 0;
    return u32(size) && take(size, out);
  }

  bool atEnd() const { return pos_ == bytes_.size(); }

private:
  const std::vector<uint8_t> &bytes_;
  size_t pos_ = 0;
};

inline void putU32(std::vector<uint8_t> &out, uint32_t value) {
  for (int shift = 0; shift < 32; shift += 8) {
    out.push_back(static_cast<uint8_t>((value >> shift) & 0xFFu));
  }
}

inline void putBlob(std::vector<uint8_t> &out, const std::vector<uint8_t> &blob) {
  putU32(out, static_cast<uint32_t>(blob.size()));
  out.insert(out.end(), blob.begin(), blob.end());
}

// The bytes with every stdlib source path made checkout-independent; the input unchanged when it does not parse.
inline std::vector<uint8_t> normalizedBytecode(const std::vector<uint8_t> &bytes) {
  static const char Magic[4] = {'P', 'S', 'B', 'N'};
  const bool isBundle = bytes.size() >= 4 && std::equal(Magic, Magic + 4, bytes.begin());
  std::vector<uint8_t> out;
  if (!isBundle) {
    return normalizeModuleBytes(bytes, out) ? out : bytes;
  }
  BundleCursor cursor(bytes);
  std::vector<uint8_t> magic;
  std::vector<uint8_t> blob;
  std::vector<uint8_t> normalized;
  uint32_t version = 0;
  uint32_t hasMain = 0;
  uint32_t exportCount = 0;
  if (!cursor.take(4, magic) || !cursor.u32(version) || !cursor.u32(hasMain)) {
    return bytes;
  }
  out.assign(Magic, Magic + 4);
  putU32(out, version);
  putU32(out, hasMain);
  if (hasMain != 0) {
    if (!cursor.blob(blob) || !normalizeModuleBytes(blob, normalized)) {
      return bytes;
    }
    putBlob(out, normalized);
  }
  if (!cursor.u32(exportCount)) {
    return bytes;
  }
  putU32(out, exportCount);
  for (uint32_t i = 0; i < exportCount; ++i) {
    std::vector<uint8_t> name;
    std::vector<uint8_t> parameterTypes;
    std::vector<uint8_t> returnType;
    uint32_t parameterCount = 0;
    if (!cursor.blob(name) || !cursor.u32(parameterCount) || !cursor.take(parameterCount, parameterTypes) ||
        !cursor.take(1, returnType) || !cursor.blob(blob) || !normalizeModuleBytes(blob, normalized)) {
      return bytes;
    }
    putBlob(out, name);
    putU32(out, parameterCount);
    out.insert(out.end(), parameterTypes.begin(), parameterTypes.end());
    out.insert(out.end(), returnType.begin(), returnType.end());
    putBlob(out, normalized);
  }
  return cursor.atEnd() ? out : bytes;
}

} // namespace embed_fixture
