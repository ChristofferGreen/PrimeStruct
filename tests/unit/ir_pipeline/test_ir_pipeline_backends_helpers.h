#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include "primec/Ir.h"

static inline primec::IrModule makeReturnI32Module(int32_t value) {
  primec::IrModule module;
  module.entryIndex = 0;
  primec::IrFunction function;
  function.name = "/main";
  function.instructions.push_back({primec::IrOpcode::PushI32, static_cast<uint64_t>(static_cast<uint32_t>(value))});
  function.instructions.push_back({primec::IrOpcode::ReturnI32, 0});
  module.functions.push_back(function);
  return module;
}

static inline uint64_t f32ToBits(float value) {
  uint32_t bits = 0;
  std::memcpy(&bits, &value, sizeof(bits));
  return static_cast<uint64_t>(bits);
}

static inline uint64_t f64ToBits(double value) {
  uint64_t bits = 0;
  std::memcpy(&bits, &value, sizeof(bits));
  return bits;
}

static inline std::vector<uint8_t> readBinaryFile(const std::filesystem::path &path) {
  std::ifstream input(path, std::ios::binary);
  return std::vector<uint8_t>(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
}

static inline std::string readTextFile(const std::filesystem::path &path) {
  std::ifstream input(path);
  return std::string(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
}

static inline std::string readTextFiles(const std::vector<std::filesystem::path> &paths) {
  std::string combined;
  for (const auto &path : paths) {
    combined += readTextFile(path);
    combined.push_back('\n');
  }
  return combined;
}
