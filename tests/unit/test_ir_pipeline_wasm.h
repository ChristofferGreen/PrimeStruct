#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "primec/WasmEmitter.h"

TEST_SUITE_BEGIN("primestruct.ir.pipeline.validation");

namespace {

bool hasMinimalWasmHeader(const std::vector<uint8_t> &bytes) {
  static constexpr uint8_t Expected[] = {0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00};
  if (bytes.size() < sizeof(Expected)) {
    return false;
  }
  for (size_t i = 0; i < sizeof(Expected); ++i) {
    if (bytes[i] != Expected[i]) {
      return false;
    }
  }
  return true;
}

bool readU32Leb(const std::vector<uint8_t> &bytes, size_t &offset, uint32_t &value) {
  value = 0;
  uint32_t shift = 0;
  for (int i = 0; i < 5; ++i) {
    if (offset >= bytes.size()) {
      return false;
    }
    const uint8_t byte = bytes[offset++];
    value |= static_cast<uint32_t>(byte & 0x7f) << shift;
    if ((byte & 0x80) == 0) {
      return true;
    }
    shift += 7;
  }
  return false;
}

bool parseSections(const std::vector<uint8_t> &bytes,
                   std::vector<uint8_t> &sectionIds,
                   std::vector<uint32_t> &sectionLengths,
                   std::string &error) {
  error.clear();
  sectionIds.clear();
  sectionLengths.clear();
  if (!hasMinimalWasmHeader(bytes)) {
    error = "missing wasm header";
    return false;
  }

  size_t offset = 8;
  while (offset < bytes.size()) {
    const uint8_t sectionId = bytes[offset++];
    uint32_t sectionLength = 0;
    if (!readU32Leb(bytes, offset, sectionLength)) {
      error = "failed to decode section length";
      return false;
    }
    if (offset + sectionLength > bytes.size()) {
      error = "section payload exceeds module bounds";
      return false;
    }
    sectionIds.push_back(sectionId);
    sectionLengths.push_back(sectionLength);
    offset += sectionLength;
  }
  return true;
}

bool sectionPayloadIsEmptyVector(const std::vector<uint8_t> &bytes, uint8_t sectionId) {
  size_t offset = 8;
  while (offset < bytes.size()) {
    const uint8_t currentSectionId = bytes[offset++];
    uint32_t sectionLength = 0;
    if (!readU32Leb(bytes, offset, sectionLength)) {
      return false;
    }
    if (offset + sectionLength > bytes.size()) {
      return false;
    }
    if (currentSectionId == sectionId) {
      return sectionLength == 1 && bytes[offset] == 0;
    }
    offset += sectionLength;
  }
  return false;
}

} // namespace

TEST_CASE("wasm emitter writes deterministic section snapshots") {
  primec::WasmEmitter emitter;
  primec::IrModule module;
  module.entryIndex = -1;

  std::vector<uint8_t> first;
  std::vector<uint8_t> second;
  std::string error;
  REQUIRE(emitter.emitModule(module, first, error));
  CHECK(error.empty());
  REQUIRE(emitter.emitModule(module, second, error));
  CHECK(error.empty());
  CHECK(first == second);
  CHECK(hasMinimalWasmHeader(first));

  const std::vector<uint8_t> expected = {
      0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00,
      0x01, 0x01, 0x00,
      0x02, 0x01, 0x00,
      0x03, 0x01, 0x00,
      0x07, 0x01, 0x00,
      0x0a, 0x01, 0x00,
      0x0b, 0x01, 0x00,
  };
  CHECK(first == expected);
}

TEST_CASE("wasm emitter passes structural section validation") {
  primec::WasmEmitter emitter;
  primec::IrModule module;
  module.entryIndex = -1;

  std::vector<uint8_t> bytes;
  std::string error;
  REQUIRE(emitter.emitModule(module, bytes, error));
  CHECK(error.empty());

  std::vector<uint8_t> sectionIds;
  std::vector<uint32_t> sectionLengths;
  REQUIRE(parseSections(bytes, sectionIds, sectionLengths, error));
  CHECK(error.empty());

  const std::vector<uint8_t> expectedSectionIds = {1, 2, 3, 7, 10, 11};
  CHECK(sectionIds == expectedSectionIds);
  CHECK(sectionLengths.size() == expectedSectionIds.size());
  for (uint32_t sectionLength : sectionLengths) {
    CHECK(sectionLength == 1u);
  }

  CHECK(sectionPayloadIsEmptyVector(bytes, 1));
  CHECK(sectionPayloadIsEmptyVector(bytes, 2));
  CHECK(sectionPayloadIsEmptyVector(bytes, 3));
  CHECK(sectionPayloadIsEmptyVector(bytes, 7));
  CHECK(sectionPayloadIsEmptyVector(bytes, 10));
  CHECK(sectionPayloadIsEmptyVector(bytes, 11));
}

TEST_CASE("wasm emitter rejects invalid empty-module entry index") {
  primec::WasmEmitter emitter;
  primec::IrModule module;
  module.entryIndex = 0;

  std::vector<uint8_t> bytes;
  std::string error;
  CHECK_FALSE(emitter.emitModule(module, bytes, error));
  CHECK(error == "invalid IR entry index");
}

TEST_CASE("wasm emitter rejects function lowering before codegen support") {
  primec::WasmEmitter emitter;
  primec::IrModule module;
  module.entryIndex = 0;

  primec::IrFunction entry;
  entry.name = "/main";
  entry.instructions.push_back({primec::IrOpcode::ReturnVoid, 0});
  module.functions.push_back(entry);

  std::vector<uint8_t> bytes;
  std::string error;
  CHECK_FALSE(emitter.emitModule(module, bytes, error));
  CHECK(error.find("function lowering is not implemented yet") != std::string::npos);
}

TEST_SUITE_END();
