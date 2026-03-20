#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace primec {

inline constexpr uint8_t WasmFunctionKind = 0x00;
inline constexpr uint8_t WasmFunctionTypeTag = 0x60;
inline constexpr uint8_t WasmSectionType = 1;
inline constexpr uint8_t WasmSectionImport = 2;
inline constexpr uint8_t WasmSectionFunction = 3;
inline constexpr uint8_t WasmSectionMemory = 5;
inline constexpr uint8_t WasmSectionExport = 7;
inline constexpr uint8_t WasmSectionCode = 10;
inline constexpr uint8_t WasmSectionData = 11;

struct WasmFunctionType {
  std::vector<uint8_t> params;
  std::vector<uint8_t> results;
};

struct WasmImport {
  std::string module;
  std::string name;
  uint8_t kind = WasmFunctionKind;
  uint32_t typeIndex = 0;
};

struct WasmExport {
  std::string name;
  uint8_t kind = WasmFunctionKind;
  uint32_t index = 0;
};

struct WasmCodeBody {
  std::vector<uint8_t> localDecls;
  std::vector<uint8_t> instructions;
};

struct WasmDataSegment {
  uint32_t memoryIndex = 0;
  std::vector<uint8_t> offsetExpression;
  std::vector<uint8_t> bytes;
};

struct WasmMemoryLimit {
  uint32_t minPages = 0;
  bool hasMax = false;
  uint32_t maxPages = 0;
};

struct WasmLocalLayout {
  uint32_t irLocalCount = 0;
  bool needsDupTempLocal = false;
  uint32_t dupTempIndex = 0;
  bool hasArgvHelpers = false;
  uint32_t argvIndexLocal = 0;
  uint32_t argvCountLocal = 0;
  uint32_t argvPtrLocal = 0;
  uint32_t argvLenLocal = 0;
  uint32_t argvNextPtrLocal = 0;
  bool hasFileHelpers = false;
  uint32_t fileHandleLocal = 0;
  uint32_t fileValueLocal = 0;
  uint32_t fileErrLocal = 0;
  bool hasFileNumericHelpers = false;
  uint32_t fileDigitsPtrLocal = 0;
  uint32_t fileDigitsLenLocal = 0;
  uint32_t fileDigitsNegLocal = 0;
  uint32_t fileDigitsValueLocal = 0;
  uint32_t fileDigitsRemLocal = 0;
  uint32_t i32LocalCount = 0;
  uint32_t i64LocalCount = 0;
  uint32_t totalLocals = 0;
};

struct WasmRuntimeContext {
  bool enabled = false;
  bool hasArgvOps = false;
  bool hasOutputOps = false;
  bool hasFileOps = false;
  bool hasFileReadOps = false;
  bool hasFileWriteOps = false;
  uint32_t functionIndexOffset = 0;
  uint32_t fdWriteImportIndex = 0;
  uint32_t fdReadImportIndex = 0;
  uint32_t argsSizesGetImportIndex = 0;
  uint32_t argsGetImportIndex = 0;
  uint32_t fdCloseImportIndex = 0;
  uint32_t fdSyncImportIndex = 0;
  uint32_t pathOpenImportIndex = 0;
  uint32_t argcAddr = 0;
  uint32_t argvBufSizeAddr = 0;
  uint32_t nwrittenAddr = 0;
  uint32_t openedFdAddr = 0;
  uint32_t iovAddr = 0;
  uint32_t argvPtrsBase = 0;
  uint32_t argvBufferBase = 0;
  uint32_t argvPtrsCapacityBytes = 0;
  uint32_t argvBufferCapacityBytes = 0;
  uint32_t newlineAddr = 0;
  uint32_t byteScratchAddr = 0;
  uint32_t numericScratchAddr = 0;
  uint32_t numericScratchBytes = 0;
  std::vector<uint32_t> stringPtrs;
  std::vector<uint32_t> stringLens;
  std::vector<WasmMemoryLimit> memories;
  std::vector<WasmDataSegment> dataSegments;
};

inline void appendU32Leb(uint32_t value, std::vector<uint8_t> &out) {
  do {
    uint8_t byte = static_cast<uint8_t>(value & 0x7f);
    value >>= 7;
    if (value != 0) {
      byte |= 0x80;
    }
    out.push_back(byte);
  } while (value != 0);
}

inline void appendS32Leb(int32_t value, std::vector<uint8_t> &out) {
  bool more = true;
  while (more) {
    uint8_t byte = static_cast<uint8_t>(value & 0x7f);
    value >>= 7;
    const bool signBit = (byte & 0x40) != 0;
    if ((value == 0 && !signBit) || (value == -1 && signBit)) {
      more = false;
    } else {
      byte |= 0x80;
    }
    out.push_back(byte);
  }
}

inline void appendS64Leb(int64_t value, std::vector<uint8_t> &out) {
  bool more = true;
  while (more) {
    uint8_t byte = static_cast<uint8_t>(value & 0x7f);
    value >>= 7;
    const bool signBit = (byte & 0x40) != 0;
    if ((value == 0 && !signBit) || (value == -1 && signBit)) {
      more = false;
    } else {
      byte |= 0x80;
    }
    out.push_back(byte);
  }
}

inline void appendFixedU32Le(uint32_t value, std::vector<uint8_t> &out) {
  out.push_back(static_cast<uint8_t>(value & 0xff));
  out.push_back(static_cast<uint8_t>((value >> 8) & 0xff));
  out.push_back(static_cast<uint8_t>((value >> 16) & 0xff));
  out.push_back(static_cast<uint8_t>((value >> 24) & 0xff));
}

inline void appendFixedU64Le(uint64_t value, std::vector<uint8_t> &out) {
  for (size_t byte = 0; byte < 8; ++byte) {
    out.push_back(static_cast<uint8_t>((value >> (byte * 8)) & 0xff));
  }
}

bool appendSection(uint8_t sectionId,
                   const std::vector<uint8_t> &payload,
                   std::vector<uint8_t> &out,
                   std::string &error);

bool encodeTypeSection(const std::vector<WasmFunctionType> &types,
                       std::vector<uint8_t> &payload,
                       std::string &error);

bool encodeImportSection(const std::vector<WasmImport> &imports,
                         std::vector<uint8_t> &payload,
                         std::string &error);

bool encodeFunctionSection(const std::vector<uint32_t> &functionTypeIndexes,
                           std::vector<uint8_t> &payload,
                           std::string &error);

bool encodeMemorySection(const std::vector<WasmMemoryLimit> &memories,
                         std::vector<uint8_t> &payload,
                         std::string &error);

bool encodeExportSection(const std::vector<WasmExport> &exports,
                         std::vector<uint8_t> &payload,
                         std::string &error);

bool encodeCodeSection(const std::vector<WasmCodeBody> &bodies,
                       std::vector<uint8_t> &payload,
                       std::string &error);

bool encodeDataSection(const std::vector<WasmDataSegment> &segments,
                       std::vector<uint8_t> &payload,
                       std::string &error);

} // namespace primec
