#include "NativeEmitterInternals.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>
#include <string>
#include <vector>

#if defined(_WIN32)
#include <process.h>
#else
#include <unistd.h>
#endif

#if defined(__APPLE__)
#include <mach/machine.h>
#include <mach-o/loader.h>
#endif

namespace primec::native_emitter {
bool computeMaxStackDepth(const IrFunction &fn, int64_t &maxDepth, std::string &error) {
  if (fn.instructions.empty()) {
    error = "native backend requires at least one instruction";
    return false;
  }
  auto opcodeName = [](IrOpcode op) -> const char * {
    switch (op) {
      case IrOpcode::PushI32:
        return "PushI32";
      case IrOpcode::PushI64:
        return "PushI64";
      case IrOpcode::PushArgc:
        return "PushArgc";
      case IrOpcode::LoadLocal:
        return "LoadLocal";
      case IrOpcode::StoreLocal:
        return "StoreLocal";
      case IrOpcode::AddressOfLocal:
        return "AddressOfLocal";
      case IrOpcode::LoadIndirect:
        return "LoadIndirect";
      case IrOpcode::StoreIndirect:
        return "StoreIndirect";
      case IrOpcode::Dup:
        return "Dup";
      case IrOpcode::Pop:
        return "Pop";
      case IrOpcode::AddI32:
        return "AddI32";
      case IrOpcode::SubI32:
        return "SubI32";
      case IrOpcode::MulI32:
        return "MulI32";
      case IrOpcode::DivI32:
        return "DivI32";
      case IrOpcode::NegI32:
        return "NegI32";
      case IrOpcode::AddI64:
        return "AddI64";
      case IrOpcode::SubI64:
        return "SubI64";
      case IrOpcode::MulI64:
        return "MulI64";
      case IrOpcode::DivI64:
        return "DivI64";
      case IrOpcode::DivU64:
        return "DivU64";
      case IrOpcode::NegI64:
        return "NegI64";
      case IrOpcode::CmpEqI32:
        return "CmpEqI32";
      case IrOpcode::CmpNeI32:
        return "CmpNeI32";
      case IrOpcode::CmpLtI32:
        return "CmpLtI32";
      case IrOpcode::CmpLeI32:
        return "CmpLeI32";
      case IrOpcode::CmpGtI32:
        return "CmpGtI32";
      case IrOpcode::CmpGeI32:
        return "CmpGeI32";
      case IrOpcode::CmpEqI64:
        return "CmpEqI64";
      case IrOpcode::CmpNeI64:
        return "CmpNeI64";
      case IrOpcode::CmpLtI64:
        return "CmpLtI64";
      case IrOpcode::CmpLeI64:
        return "CmpLeI64";
      case IrOpcode::CmpGtI64:
        return "CmpGtI64";
      case IrOpcode::CmpGeI64:
        return "CmpGeI64";
      case IrOpcode::CmpLtU64:
        return "CmpLtU64";
      case IrOpcode::CmpLeU64:
        return "CmpLeU64";
      case IrOpcode::CmpGtU64:
        return "CmpGtU64";
      case IrOpcode::CmpGeU64:
        return "CmpGeU64";
      case IrOpcode::JumpIfZero:
        return "JumpIfZero";
      case IrOpcode::Jump:
        return "Jump";
      case IrOpcode::ReturnVoid:
        return "ReturnVoid";
      case IrOpcode::ReturnI32:
        return "ReturnI32";
      case IrOpcode::ReturnI64:
        return "ReturnI64";
      case IrOpcode::ReturnF32:
        return "ReturnF32";
      case IrOpcode::ReturnF64:
        return "ReturnF64";
      case IrOpcode::PrintI32:
        return "PrintI32";
      case IrOpcode::PrintI64:
        return "PrintI64";
      case IrOpcode::PrintU64:
        return "PrintU64";
      case IrOpcode::PrintString:
        return "PrintString";
      case IrOpcode::PrintArgv:
        return "PrintArgv";
      case IrOpcode::PrintArgvUnsafe:
        return "PrintArgvUnsafe";
      case IrOpcode::LoadStringByte:
        return "LoadStringByte";
      case IrOpcode::FileOpenRead:
        return "FileOpenRead";
      case IrOpcode::FileOpenWrite:
        return "FileOpenWrite";
      case IrOpcode::FileOpenAppend:
        return "FileOpenAppend";
      case IrOpcode::FileClose:
        return "FileClose";
      case IrOpcode::FileFlush:
        return "FileFlush";
      case IrOpcode::FileWriteI32:
        return "FileWriteI32";
      case IrOpcode::FileWriteI64:
        return "FileWriteI64";
      case IrOpcode::FileWriteU64:
        return "FileWriteU64";
      case IrOpcode::FileWriteString:
        return "FileWriteString";
      case IrOpcode::FileWriteByte:
        return "FileWriteByte";
      case IrOpcode::FileWriteNewline:
        return "FileWriteNewline";
      case IrOpcode::PushF32:
        return "PushF32";
      case IrOpcode::PushF64:
        return "PushF64";
      case IrOpcode::AddF32:
        return "AddF32";
      case IrOpcode::SubF32:
        return "SubF32";
      case IrOpcode::MulF32:
        return "MulF32";
      case IrOpcode::DivF32:
        return "DivF32";
      case IrOpcode::NegF32:
        return "NegF32";
      case IrOpcode::AddF64:
        return "AddF64";
      case IrOpcode::SubF64:
        return "SubF64";
      case IrOpcode::MulF64:
        return "MulF64";
      case IrOpcode::DivF64:
        return "DivF64";
      case IrOpcode::NegF64:
        return "NegF64";
      case IrOpcode::CmpEqF32:
        return "CmpEqF32";
      case IrOpcode::CmpNeF32:
        return "CmpNeF32";
      case IrOpcode::CmpLtF32:
        return "CmpLtF32";
      case IrOpcode::CmpLeF32:
        return "CmpLeF32";
      case IrOpcode::CmpGtF32:
        return "CmpGtF32";
      case IrOpcode::CmpGeF32:
        return "CmpGeF32";
      case IrOpcode::CmpEqF64:
        return "CmpEqF64";
      case IrOpcode::CmpNeF64:
        return "CmpNeF64";
      case IrOpcode::CmpLtF64:
        return "CmpLtF64";
      case IrOpcode::CmpLeF64:
        return "CmpLeF64";
      case IrOpcode::CmpGtF64:
        return "CmpGtF64";
      case IrOpcode::CmpGeF64:
        return "CmpGeF64";
      case IrOpcode::ConvertI32ToF32:
        return "ConvertI32ToF32";
      case IrOpcode::ConvertI32ToF64:
        return "ConvertI32ToF64";
      case IrOpcode::ConvertI64ToF32:
        return "ConvertI64ToF32";
      case IrOpcode::ConvertI64ToF64:
        return "ConvertI64ToF64";
      case IrOpcode::ConvertU64ToF32:
        return "ConvertU64ToF32";
      case IrOpcode::ConvertU64ToF64:
        return "ConvertU64ToF64";
      case IrOpcode::ConvertF32ToI32:
        return "ConvertF32ToI32";
      case IrOpcode::ConvertF32ToI64:
        return "ConvertF32ToI64";
      case IrOpcode::ConvertF32ToU64:
        return "ConvertF32ToU64";
      case IrOpcode::ConvertF64ToI32:
        return "ConvertF64ToI32";
      case IrOpcode::ConvertF64ToI64:
        return "ConvertF64ToI64";
      case IrOpcode::ConvertF64ToU64:
        return "ConvertF64ToU64";
      case IrOpcode::ConvertF32ToF64:
        return "ConvertF32ToF64";
      case IrOpcode::ConvertF64ToF32:
        return "ConvertF64ToF32";
      default:
        return "Unknown";
    }
  };
  auto stackDelta = [](IrOpcode op) -> int32_t {
    switch (op) {
      case IrOpcode::PushI32:
      case IrOpcode::PushI64:
      case IrOpcode::PushF32:
      case IrOpcode::PushF64:
      case IrOpcode::PushArgc:
      case IrOpcode::LoadLocal:
      case IrOpcode::AddressOfLocal:
        return 1;
      case IrOpcode::StoreLocal:
      case IrOpcode::Pop:
        return -1;
      case IrOpcode::LoadIndirect:
        return 0;
      case IrOpcode::StoreIndirect:
        return -1;
      case IrOpcode::Dup:
        return 1;
      case IrOpcode::AddI32:
      case IrOpcode::SubI32:
      case IrOpcode::MulI32:
      case IrOpcode::DivI32:
      case IrOpcode::AddI64:
      case IrOpcode::SubI64:
      case IrOpcode::MulI64:
      case IrOpcode::DivI64:
      case IrOpcode::DivU64:
      case IrOpcode::AddF32:
      case IrOpcode::SubF32:
      case IrOpcode::MulF32:
      case IrOpcode::DivF32:
      case IrOpcode::AddF64:
      case IrOpcode::SubF64:
      case IrOpcode::MulF64:
      case IrOpcode::DivF64:
      case IrOpcode::CmpEqI32:
      case IrOpcode::CmpNeI32:
      case IrOpcode::CmpLtI32:
      case IrOpcode::CmpLeI32:
      case IrOpcode::CmpGtI32:
      case IrOpcode::CmpGeI32:
      case IrOpcode::CmpEqI64:
      case IrOpcode::CmpNeI64:
      case IrOpcode::CmpLtI64:
      case IrOpcode::CmpLeI64:
      case IrOpcode::CmpGtI64:
      case IrOpcode::CmpGeI64:
      case IrOpcode::CmpLtU64:
      case IrOpcode::CmpLeU64:
      case IrOpcode::CmpGtU64:
      case IrOpcode::CmpGeU64:
      case IrOpcode::CmpEqF32:
      case IrOpcode::CmpNeF32:
      case IrOpcode::CmpLtF32:
      case IrOpcode::CmpLeF32:
      case IrOpcode::CmpGtF32:
      case IrOpcode::CmpGeF32:
      case IrOpcode::CmpEqF64:
      case IrOpcode::CmpNeF64:
      case IrOpcode::CmpLtF64:
      case IrOpcode::CmpLeF64:
      case IrOpcode::CmpGtF64:
      case IrOpcode::CmpGeF64:
        return -1;
      case IrOpcode::NegI32:
      case IrOpcode::NegI64:
      case IrOpcode::NegF32:
      case IrOpcode::NegF64:
      case IrOpcode::ConvertI32ToF32:
      case IrOpcode::ConvertI32ToF64:
      case IrOpcode::ConvertI64ToF32:
      case IrOpcode::ConvertI64ToF64:
      case IrOpcode::ConvertU64ToF32:
      case IrOpcode::ConvertU64ToF64:
      case IrOpcode::ConvertF32ToI32:
      case IrOpcode::ConvertF32ToI64:
      case IrOpcode::ConvertF32ToU64:
      case IrOpcode::ConvertF64ToI32:
      case IrOpcode::ConvertF64ToI64:
      case IrOpcode::ConvertF64ToU64:
      case IrOpcode::ConvertF32ToF64:
      case IrOpcode::ConvertF64ToF32:
        return 0;
      case IrOpcode::JumpIfZero:
        return -1;
      case IrOpcode::Jump:
        return 0;
      case IrOpcode::ReturnVoid:
        return 0;
      case IrOpcode::ReturnI32:
      case IrOpcode::ReturnI64:
      case IrOpcode::ReturnF32:
      case IrOpcode::ReturnF64:
        return -1;
      case IrOpcode::PrintI32:
      case IrOpcode::PrintI64:
      case IrOpcode::PrintU64:
        return -1;
      case IrOpcode::PrintString:
        return 0;
      case IrOpcode::PrintArgv:
      case IrOpcode::PrintArgvUnsafe:
        return -1;
      case IrOpcode::LoadStringByte:
        return 0;
      case IrOpcode::FileOpenRead:
      case IrOpcode::FileOpenWrite:
      case IrOpcode::FileOpenAppend:
        return 1;
      case IrOpcode::FileClose:
      case IrOpcode::FileFlush:
        return 0;
      case IrOpcode::FileWriteString:
      case IrOpcode::FileWriteNewline:
        return 0;
      case IrOpcode::FileWriteI32:
      case IrOpcode::FileWriteI64:
      case IrOpcode::FileWriteU64:
      case IrOpcode::FileWriteByte:
        return -1;
      default:
        return 0;
    }
  };

  const int64_t kUnset = std::numeric_limits<int64_t>::min();
  std::vector<int64_t> depth(fn.instructions.size(), kUnset);
  std::vector<size_t> worklist;
  depth[0] = 0;
  worklist.push_back(0);
  maxDepth = 0;

  while (!worklist.empty()) {
    size_t index = worklist.back();
    worklist.pop_back();
    int64_t currentDepth = depth[index];
    maxDepth = std::max(maxDepth, currentDepth);
    const auto &inst = fn.instructions[index];
    int64_t nextDepth = currentDepth + stackDelta(inst.op);
    if (nextDepth < 0) {
      error = "native backend detected invalid stack usage at instruction " + std::to_string(index) + " (" +
              opcodeName(inst.op) + ")";
      return false;
    }
    maxDepth = std::max(maxDepth, nextDepth);

    auto pushSuccessor = [&](size_t nextIndex) -> bool {
      if (nextIndex >= fn.instructions.size()) {
        return true;
      }
      if (depth[nextIndex] == kUnset) {
        depth[nextIndex] = nextDepth;
        worklist.push_back(nextIndex);
        return true;
      }
      if (depth[nextIndex] != nextDepth) {
        error = "native backend detected inconsistent stack depth at instruction " + std::to_string(nextIndex) + " (" +
                opcodeName(fn.instructions[nextIndex].op) + ")";
        return false;
      }
      return true;
    };

    if (inst.op == IrOpcode::ReturnVoid || inst.op == IrOpcode::ReturnI32 || inst.op == IrOpcode::ReturnI64 ||
        inst.op == IrOpcode::ReturnF32 || inst.op == IrOpcode::ReturnF64) {
      continue;
    }
    if (inst.op == IrOpcode::Jump || inst.op == IrOpcode::JumpIfZero) {
      if (static_cast<size_t>(inst.imm) > fn.instructions.size()) {
        error = "native backend detected invalid jump target";
        return false;
      }
      if (!pushSuccessor(static_cast<size_t>(inst.imm))) {
        return false;
      }
      if (inst.op == IrOpcode::JumpIfZero) {
        if (!pushSuccessor(index + 1)) {
          return false;
        }
      }
      continue;
    }
    if (!pushSuccessor(index + 1)) {
      return false;
    }
  }
  return true;
}

bool writeBinaryFile(const std::string &path, const std::vector<uint8_t> &data, std::string &error) {
  std::filesystem::path outputPath(path);
  std::filesystem::path parent = outputPath.parent_path();
  if (parent.empty()) {
    parent = ".";
  }

  // Write to a temporary file and rename into place. This avoids keeping a
  // potentially problematic inode alive across runs (e.g. when tests leave
  // behind executing processes).
  const long long pid =
#if defined(_WIN32)
      static_cast<long long>(::_getpid());
#else
      static_cast<long long>(::getpid());
#endif
  std::filesystem::path tempPath =
      parent / (outputPath.filename().string() + ".tmp." + std::to_string(pid));

  {
    std::error_code cleanupEc;
    std::filesystem::remove(tempPath, cleanupEc);
  }

  std::ofstream file(tempPath, std::ios::binary | std::ios::trunc);
  if (!file) {
    error = "failed to open output file";
    return false;
  }
  file.write(reinterpret_cast<const char *>(data.data()), static_cast<std::streamsize>(data.size()));
  if (!file.good()) {
    std::error_code cleanupEc;
    std::filesystem::remove(tempPath, cleanupEc);
    error = "failed to write output file";
    return false;
  }
  file.close();

  std::error_code ec;
  std::filesystem::permissions(tempPath,
                               std::filesystem::perms::owner_exec | std::filesystem::perms::group_exec |
                                   std::filesystem::perms::others_exec,
                               std::filesystem::perm_options::add,
                               ec);
  if (ec) {
    std::filesystem::remove(tempPath, ec);
    error = "failed to set executable permissions";
    return false;
  }

  std::filesystem::remove(outputPath, ec);
  ec.clear();
  std::filesystem::rename(tempPath, outputPath, ec);
  if (ec) {
    std::filesystem::remove(tempPath, ec);
    error = "failed to move output file into place";
    return false;
  }
  return true;
}

#if defined(__APPLE__)
class Sha256 {
 public:
  Sha256() {
    reset();
  }

  void reset() {
    state_[0] = 0x6a09e667u;
    state_[1] = 0xbb67ae85u;
    state_[2] = 0x3c6ef372u;
    state_[3] = 0xa54ff53au;
    state_[4] = 0x510e527fu;
    state_[5] = 0x9b05688cu;
    state_[6] = 0x1f83d9abu;
    state_[7] = 0x5be0cd19u;
    totalLen_ = 0;
    bufferLen_ = 0;
  }

  void update(const uint8_t *data, size_t len) {
    if (len == 0) {
      return;
    }
    totalLen_ += len;
    size_t offset = 0;
    if (bufferLen_ > 0) {
      size_t toCopy = std::min(len, sizeof(buffer_) - bufferLen_);
      std::memcpy(buffer_ + bufferLen_, data, toCopy);
      bufferLen_ += toCopy;
      offset += toCopy;
      if (bufferLen_ == sizeof(buffer_)) {
        processBlock(buffer_);
        bufferLen_ = 0;
      }
    }
    while (offset + sizeof(buffer_) <= len) {
      processBlock(data + offset);
      offset += sizeof(buffer_);
    }
    if (offset < len) {
      bufferLen_ = len - offset;
      std::memcpy(buffer_, data + offset, bufferLen_);
    }
  }

  std::array<uint8_t, 32> finalize() {
    uint64_t totalBits = totalLen_ * 8;
    buffer_[bufferLen_++] = 0x80;
    if (bufferLen_ > 56) {
      while (bufferLen_ < 64) {
        buffer_[bufferLen_++] = 0;
      }
      processBlock(buffer_);
      bufferLen_ = 0;
    }
    while (bufferLen_ < 56) {
      buffer_[bufferLen_++] = 0;
    }
    for (int i = 7; i >= 0; --i) {
      buffer_[bufferLen_++] = static_cast<uint8_t>((totalBits >> (i * 8)) & 0xFF);
    }
    processBlock(buffer_);

    std::array<uint8_t, 32> digest{};
    for (size_t i = 0; i < 8; ++i) {
      digest[i * 4 + 0] = static_cast<uint8_t>((state_[i] >> 24) & 0xFF);
      digest[i * 4 + 1] = static_cast<uint8_t>((state_[i] >> 16) & 0xFF);
      digest[i * 4 + 2] = static_cast<uint8_t>((state_[i] >> 8) & 0xFF);
      digest[i * 4 + 3] = static_cast<uint8_t>(state_[i] & 0xFF);
    }
    return digest;
  }

 private:
  static uint32_t rotr(uint32_t value, uint32_t bits) {
    return (value >> bits) | (value << (32 - bits));
  }

  void processBlock(const uint8_t *block) {
    static constexpr uint32_t k[64] = {
        0x428a2f98u, 0x71374491u, 0xb5c0fbcfu, 0xe9b5dba5u, 0x3956c25bu, 0x59f111f1u,
        0x923f82a4u, 0xab1c5ed5u, 0xd807aa98u, 0x12835b01u, 0x243185beu, 0x550c7dc3u,
        0x72be5d74u, 0x80deb1feu, 0x9bdc06a7u, 0xc19bf174u, 0xe49b69c1u, 0xefbe4786u,
        0x0fc19dc6u, 0x240ca1ccu, 0x2de92c6fu, 0x4a7484aau, 0x5cb0a9dcu, 0x76f988dau,
        0x983e5152u, 0xa831c66du, 0xb00327c8u, 0xbf597fc7u, 0xc6e00bf3u, 0xd5a79147u,
        0x06ca6351u, 0x14292967u, 0x27b70a85u, 0x2e1b2138u, 0x4d2c6dfcu, 0x53380d13u,
        0x650a7354u, 0x766a0abbu, 0x81c2c92eu, 0x92722c85u, 0xa2bfe8a1u, 0xa81a664bu,
        0xc24b8b70u, 0xc76c51a3u, 0xd192e819u, 0xd6990624u, 0xf40e3585u, 0x106aa070u,
        0x19a4c116u, 0x1e376c08u, 0x2748774cu, 0x34b0bcb5u, 0x391c0cb3u, 0x4ed8aa4au,
        0x5b9cca4fu, 0x682e6ff3u, 0x748f82eeu, 0x78a5636fu, 0x84c87814u, 0x8cc70208u,
        0x90befffau, 0xa4506cebu, 0xbef9a3f7u, 0xc67178f2u};

    uint32_t w[64];
    for (size_t i = 0; i < 16; ++i) {
      size_t j = i * 4;
      w[i] = (static_cast<uint32_t>(block[j]) << 24) | (static_cast<uint32_t>(block[j + 1]) << 16) |
             (static_cast<uint32_t>(block[j + 2]) << 8) | static_cast<uint32_t>(block[j + 3]);
    }
    for (size_t i = 16; i < 64; ++i) {
      uint32_t s0 = rotr(w[i - 15], 7) ^ rotr(w[i - 15], 18) ^ (w[i - 15] >> 3);
      uint32_t s1 = rotr(w[i - 2], 17) ^ rotr(w[i - 2], 19) ^ (w[i - 2] >> 10);
      w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }

    uint32_t a = state_[0];
    uint32_t b = state_[1];
    uint32_t c = state_[2];
    uint32_t d = state_[3];
    uint32_t e = state_[4];
    uint32_t f = state_[5];
    uint32_t g = state_[6];
    uint32_t h = state_[7];

    for (size_t i = 0; i < 64; ++i) {
      uint32_t s1 = rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25);
      uint32_t ch = (e & f) ^ (~e & g);
      uint32_t temp1 = h + s1 + ch + k[i] + w[i];
      uint32_t s0 = rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22);
      uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
      uint32_t temp2 = s0 + maj;

      h = g;
      g = f;
      f = e;
      e = d + temp1;
      d = c;
      c = b;
      b = a;
      a = temp1 + temp2;
    }

    state_[0] += a;
    state_[1] += b;
    state_[2] += c;
    state_[3] += d;
    state_[4] += e;
    state_[5] += f;
    state_[6] += g;
    state_[7] += h;
  }

  uint32_t state_[8]{};
  uint64_t totalLen_ = 0;
  uint8_t buffer_[64]{};
  size_t bufferLen_ = 0;
};

void appendU32BE(std::vector<uint8_t> &out, uint32_t value) {
  out.push_back(static_cast<uint8_t>((value >> 24) & 0xFF));
  out.push_back(static_cast<uint8_t>((value >> 16) & 0xFF));
  out.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
  out.push_back(static_cast<uint8_t>(value & 0xFF));
}

void appendU64BE(std::vector<uint8_t> &out, uint64_t value) {
  appendU32BE(out, static_cast<uint32_t>((value >> 32) & 0xFFFFFFFFu));
  appendU32BE(out, static_cast<uint32_t>(value & 0xFFFFFFFFu));
}

bool buildCodeSignature(const std::vector<uint8_t> &image,
                        uint32_t codeLimit,
                        std::vector<uint8_t> &out,
                        std::string &error) {
  constexpr uint32_t superBlobMagic = 0xFADE0CC0u;
  constexpr uint32_t codeDirMagic = 0xFADE0C02u;
  constexpr uint32_t codeDirVersion = 0x20400u;
  constexpr uint32_t codeDirFlags = 0x00000002u; // adhoc
  constexpr uint8_t hashSize = 32;
  constexpr uint8_t hashType = 2; // SHA-256
  constexpr uint8_t platform = 2;
  constexpr uint8_t pageSizeLog2 = 14; // 16KB pages on arm64 macOS

  if (codeLimit == 0 || codeLimit > image.size()) {
    error = "invalid code signature limit";
    return false;
  }

  const std::string identifier = "primec.native";
  const uint32_t headerSize = 88;
  const uint32_t identOffset = headerSize;
  const uint32_t identSize = static_cast<uint32_t>(identifier.size() + 1);
  const uint32_t hashOffset = identOffset + identSize;
  const uint32_t nCodeSlots = static_cast<uint32_t>((codeLimit + (1u << pageSizeLog2) - 1) >> pageSizeLog2);
  const uint32_t length = hashOffset + nCodeSlots * hashSize;

  std::vector<uint8_t> codeDir;
  codeDir.reserve(length);
  appendU32BE(codeDir, codeDirMagic);
  appendU32BE(codeDir, length);
  appendU32BE(codeDir, codeDirVersion);
  appendU32BE(codeDir, codeDirFlags);
  appendU32BE(codeDir, hashOffset);
  appendU32BE(codeDir, identOffset);
  appendU32BE(codeDir, 0); // nSpecialSlots
  appendU32BE(codeDir, nCodeSlots);
  appendU32BE(codeDir, codeLimit);
  uint32_t hashInfo = (static_cast<uint32_t>(hashSize) << 24) |
                      (static_cast<uint32_t>(hashType) << 16) |
                      (static_cast<uint32_t>(platform) << 8) |
                      static_cast<uint32_t>(pageSizeLog2);
  appendU32BE(codeDir, hashInfo);
  appendU32BE(codeDir, 0); // spare2
  appendU32BE(codeDir, 0); // scatterOffset
  appendU32BE(codeDir, 0); // teamOffset
  appendU32BE(codeDir, 0); // spare3
  appendU64BE(codeDir, 0); // codeLimit64
  appendU64BE(codeDir, 0); // execSegBase
  appendU64BE(codeDir, 0); // execSegLimit
  appendU64BE(codeDir, 0x400000000000ull); // execSegFlags

  codeDir.insert(codeDir.end(), identifier.begin(), identifier.end());
  codeDir.push_back(0);

  const size_t pageSize = static_cast<size_t>(1u << pageSizeLog2);
  for (uint32_t slot = 0; slot < nCodeSlots; ++slot) {
    size_t start = static_cast<size_t>(slot) * pageSize;
    size_t end = std::min(static_cast<size_t>(codeLimit), start + pageSize);
    Sha256 sha;
    if (start < end) {
      sha.update(image.data() + start, end - start);
    }
    if (end < start + pageSize) {
      std::vector<uint8_t> pad(start + pageSize - end, 0);
      sha.update(pad.data(), pad.size());
    }
    auto digest = sha.finalize();
    codeDir.insert(codeDir.end(), digest.begin(), digest.end());
  }

  const uint32_t count = 1;
  const uint32_t superSize = 12 + count * 8 + static_cast<uint32_t>(codeDir.size());
  out.clear();
  out.reserve(superSize);
  appendU32BE(out, superBlobMagic);
  appendU32BE(out, superSize);
  appendU32BE(out, count);
  appendU32BE(out, 0); // type: CodeDirectory
  appendU32BE(out, 12 + count * 8);
  out.insert(out.end(), codeDir.begin(), codeDir.end());
  return true;
}

uint32_t computeMachOCodeOffset() {
  const std::string dyldPath = "/usr/lib/dyld";
  const std::string libSystemPath = "/usr/lib/libSystem.B.dylib";

  const uint32_t headerSize = sizeof(mach_header_64);
  const uint32_t pageZeroCmdSize = sizeof(segment_command_64);
  const uint32_t textCmdSize = sizeof(segment_command_64) + sizeof(section_64);
  const uint32_t linkeditCmdSize = sizeof(segment_command_64);
  const uint32_t dylinkerCmdSize =
      static_cast<uint32_t>(alignTo(sizeof(dylinker_command) + dyldPath.size() + 1, 8));
  const uint32_t dylibCmdSize =
      static_cast<uint32_t>(alignTo(sizeof(dylib_command) + libSystemPath.size() + 1, 8));
  const uint32_t dyldInfoCmdSize = sizeof(dyld_info_command);
  const uint32_t symtabCmdSize = sizeof(symtab_command);
  const uint32_t dysymtabCmdSize = sizeof(dysymtab_command);
  const uint32_t funcStartsCmdSize = sizeof(linkedit_data_command);
  const uint32_t dataInCodeCmdSize = sizeof(linkedit_data_command);
  const uint32_t entryCmdSize = sizeof(entry_point_command);
  const uint32_t codeSigCmdSize = sizeof(linkedit_data_command);
  const uint32_t sizeofcmds =
      pageZeroCmdSize + textCmdSize + linkeditCmdSize + dylinkerCmdSize + dylibCmdSize + dyldInfoCmdSize +
      symtabCmdSize + dysymtabCmdSize + funcStartsCmdSize + dataInCodeCmdSize + entryCmdSize + codeSigCmdSize;

  return static_cast<uint32_t>(alignTo(headerSize + sizeofcmds, 16));
}

bool buildMachO(const std::vector<uint8_t> &code, std::vector<uint8_t> &image, std::string &error) {
  if (code.empty()) {
    error = "native backend requires non-empty code";
    return false;
  }

  constexpr uint8_t pageSizeLog2 = 14;
  constexpr uint32_t hashSize = 32;
  const std::string identifier = "primec.native";
  const std::string dyldPath = "/usr/lib/dyld";
  const std::string libSystemPath = "/usr/lib/libSystem.B.dylib";

  const uint32_t pageZeroCmdSize = sizeof(segment_command_64);
  const uint32_t textCmdSize = sizeof(segment_command_64) + sizeof(section_64);
  const uint32_t linkeditCmdSize = sizeof(segment_command_64);
  const uint32_t dylinkerCmdSize =
      static_cast<uint32_t>(alignTo(sizeof(dylinker_command) + dyldPath.size() + 1, 8));
  const uint32_t dylibCmdSize =
      static_cast<uint32_t>(alignTo(sizeof(dylib_command) + libSystemPath.size() + 1, 8));
  const uint32_t dyldInfoCmdSize = sizeof(dyld_info_command);
  const uint32_t symtabCmdSize = sizeof(symtab_command);
  const uint32_t dysymtabCmdSize = sizeof(dysymtab_command);
  const uint32_t funcStartsCmdSize = sizeof(linkedit_data_command);
  const uint32_t dataInCodeCmdSize = sizeof(linkedit_data_command);
  const uint32_t entryCmdSize = sizeof(entry_point_command);
  const uint32_t codeSigCmdSize = sizeof(linkedit_data_command);
  const uint32_t sizeofcmds =
      pageZeroCmdSize + textCmdSize + linkeditCmdSize + dylinkerCmdSize + dylibCmdSize + dyldInfoCmdSize +
      symtabCmdSize + dysymtabCmdSize + funcStartsCmdSize + dataInCodeCmdSize + entryCmdSize + codeSigCmdSize;

  const uint32_t codeOffset = computeMachOCodeOffset();
  const uint64_t codeSize = code.size();
  const uint64_t textFileSize = codeOffset + codeSize;
  const uint64_t textVmSize = alignTo(textFileSize, PageSize);

  const uint64_t linkeditFileOff = alignTo(textFileSize, PageSize);
  const uint64_t sigOffset = alignTo(linkeditFileOff, 16);
  const uint32_t codeLimit = static_cast<uint32_t>(sigOffset);

  const uint32_t codeDirHeaderSize = 88;
  const uint32_t identSize = static_cast<uint32_t>(identifier.size() + 1);
  const uint32_t hashOffset = codeDirHeaderSize + identSize;
  const uint32_t nCodeSlots = static_cast<uint32_t>((codeLimit + (1u << pageSizeLog2) - 1) >> pageSizeLog2);
  const uint32_t codeDirLength = hashOffset + nCodeSlots * hashSize;
  const uint32_t sigSize = 12 + 8 + codeDirLength;

  const uint64_t linkeditVmAddr = alignTo(TextVmAddr + textVmSize, PageSize);
  const uint64_t linkeditFileSize = (sigOffset - linkeditFileOff) + sigSize;
  const uint64_t linkeditVmSize = alignTo(linkeditFileSize, PageSize);
  const uint64_t fileSize = sigOffset + sigSize;

  image.assign(static_cast<size_t>(fileSize), 0);

  mach_header_64 header{};
  header.magic = MH_MAGIC_64;
  header.cputype = CPU_TYPE_ARM64;
  header.cpusubtype = CPU_SUBTYPE_ARM64_ALL;
  header.filetype = MH_EXECUTE;
  header.ncmds = 12;
  header.sizeofcmds = sizeofcmds;
  header.flags = MH_NOUNDEFS | MH_DYLDLINK | MH_TWOLEVEL | MH_PIE;

  segment_command_64 pageZero{};
  pageZero.cmd = LC_SEGMENT_64;
  pageZero.cmdsize = pageZeroCmdSize;
  std::memcpy(pageZero.segname, "__PAGEZERO", sizeof("__PAGEZERO") - 1);
  pageZero.vmaddr = 0;
  pageZero.vmsize = PageZeroSize;
  pageZero.fileoff = 0;
  pageZero.filesize = 0;
  pageZero.maxprot = 0;
  pageZero.initprot = 0;
  pageZero.nsects = 0;
  pageZero.flags = 0;

  segment_command_64 text{};
  text.cmd = LC_SEGMENT_64;
  text.cmdsize = textCmdSize;
  std::memcpy(text.segname, "__TEXT", sizeof("__TEXT") - 1);
  text.vmaddr = TextVmAddr;
  text.vmsize = textVmSize;
  text.fileoff = 0;
  text.filesize = textFileSize;
  text.maxprot = VM_PROT_READ | VM_PROT_EXECUTE;
  text.initprot = VM_PROT_READ | VM_PROT_EXECUTE;
  text.nsects = 1;
  text.flags = 0;

  section_64 textSection{};
  std::memcpy(textSection.sectname, "__text", sizeof("__text") - 1);
  std::memcpy(textSection.segname, "__TEXT", sizeof("__TEXT") - 1);
  textSection.addr = TextVmAddr + codeOffset;
  textSection.size = codeSize;
  textSection.offset = codeOffset;
  textSection.align = 2;
  textSection.reloff = 0;
  textSection.nreloc = 0;
  textSection.flags = S_ATTR_PURE_INSTRUCTIONS | S_ATTR_SOME_INSTRUCTIONS;
  textSection.reserved1 = 0;
  textSection.reserved2 = 0;
  textSection.reserved3 = 0;

  segment_command_64 linkedit{};
  linkedit.cmd = LC_SEGMENT_64;
  linkedit.cmdsize = linkeditCmdSize;
  std::memcpy(linkedit.segname, "__LINKEDIT", sizeof("__LINKEDIT") - 1);
  linkedit.vmaddr = linkeditVmAddr;
  linkedit.vmsize = linkeditVmSize;
  linkedit.fileoff = linkeditFileOff;
  linkedit.filesize = linkeditFileSize;
  linkedit.maxprot = VM_PROT_READ;
  linkedit.initprot = VM_PROT_READ;
  linkedit.nsects = 0;
  linkedit.flags = 0;

  linkedit_data_command codeSig{};
  codeSig.cmd = LC_CODE_SIGNATURE;
  codeSig.cmdsize = codeSigCmdSize;
  codeSig.dataoff = static_cast<uint32_t>(sigOffset);
  codeSig.datasize = sigSize;

  dyld_info_command dyldInfo{};
  dyldInfo.cmd = LC_DYLD_INFO_ONLY;
  dyldInfo.cmdsize = dyldInfoCmdSize;

  symtab_command symtab{};
  symtab.cmd = LC_SYMTAB;
  symtab.cmdsize = symtabCmdSize;

  dysymtab_command dysymtab{};
  dysymtab.cmd = LC_DYSYMTAB;
  dysymtab.cmdsize = dysymtabCmdSize;

  linkedit_data_command funcStarts{};
  funcStarts.cmd = LC_FUNCTION_STARTS;
  funcStarts.cmdsize = funcStartsCmdSize;

  linkedit_data_command dataInCode{};
  dataInCode.cmd = LC_DATA_IN_CODE;
  dataInCode.cmdsize = dataInCodeCmdSize;

  size_t offset = 0;
  std::memcpy(image.data() + offset, &header, sizeof(header));
  offset += sizeof(header);
  std::memcpy(image.data() + offset, &pageZero, sizeof(pageZero));
  offset += sizeof(pageZero);
  std::memcpy(image.data() + offset, &text, sizeof(text));
  offset += sizeof(text);
  std::memcpy(image.data() + offset, &textSection, sizeof(textSection));
  offset += sizeof(textSection);
  std::memcpy(image.data() + offset, &linkedit, sizeof(linkedit));
  offset += sizeof(linkedit);
  dylinker_command dyldCmd{};
  dyldCmd.cmd = LC_LOAD_DYLINKER;
  dyldCmd.cmdsize = dylinkerCmdSize;
  dyldCmd.name.offset = sizeof(dylinker_command);
  std::vector<uint8_t> dyldBlob(dylinkerCmdSize, 0);
  std::memcpy(dyldBlob.data(), &dyldCmd, sizeof(dyldCmd));
  std::memcpy(dyldBlob.data() + dyldCmd.name.offset, dyldPath.c_str(), dyldPath.size() + 1);
  std::memcpy(image.data() + offset, dyldBlob.data(), dyldBlob.size());
  offset += dyldBlob.size();
  dylib_command libSystemCmd{};
  libSystemCmd.cmd = LC_LOAD_DYLIB;
  libSystemCmd.cmdsize = dylibCmdSize;
  libSystemCmd.dylib.name.offset = sizeof(dylib_command);
  libSystemCmd.dylib.timestamp = 2;
  libSystemCmd.dylib.current_version = 0x00010000;
  libSystemCmd.dylib.compatibility_version = 0x00010000;
  std::vector<uint8_t> dylibBlob(dylibCmdSize, 0);
  std::memcpy(dylibBlob.data(), &libSystemCmd, sizeof(libSystemCmd));
  std::memcpy(dylibBlob.data() + libSystemCmd.dylib.name.offset,
              libSystemPath.c_str(),
              libSystemPath.size() + 1);
  std::memcpy(image.data() + offset, dylibBlob.data(), dylibBlob.size());
  offset += dylibBlob.size();
  std::memcpy(image.data() + offset, &dyldInfo, sizeof(dyldInfo));
  offset += sizeof(dyldInfo);
  std::memcpy(image.data() + offset, &symtab, sizeof(symtab));
  offset += sizeof(symtab);
  std::memcpy(image.data() + offset, &dysymtab, sizeof(dysymtab));
  offset += sizeof(dysymtab);
  std::memcpy(image.data() + offset, &funcStarts, sizeof(funcStarts));
  offset += sizeof(funcStarts);
  std::memcpy(image.data() + offset, &dataInCode, sizeof(dataInCode));
  offset += sizeof(dataInCode);
  entry_point_command entryCmd{};
  entryCmd.cmd = LC_MAIN;
  entryCmd.cmdsize = entryCmdSize;
  entryCmd.entryoff = codeOffset;
  entryCmd.stacksize = 0;
  std::memcpy(image.data() + offset, &entryCmd, sizeof(entryCmd));
  offset += sizeof(entryCmd);
  std::memcpy(image.data() + offset, &codeSig, sizeof(codeSig));

  std::memcpy(image.data() + codeOffset, code.data(), code.size());
  std::vector<uint8_t> signature;
  if (!buildCodeSignature(image, codeLimit, signature, error)) {
    return false;
  }
  if (signature.size() != sigSize) {
    error = "code signature size mismatch";
    return false;
  }
  std::memcpy(image.data() + sigOffset, signature.data(), signature.size());
  return true;
}
#endif

} // namespace primec::native_emitter
