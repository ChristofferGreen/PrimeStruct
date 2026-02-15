#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace primec {

enum class IrOpcode : uint8_t {
  PushI32 = 1,
  PushI64,
  LoadLocal,
  StoreLocal,
  AddressOfLocal,
  LoadIndirect,
  StoreIndirect,
  Dup,
  Pop,
  AddI32,
  SubI32,
  MulI32,
  DivI32,
  NegI32,
  AddI64,
  SubI64,
  MulI64,
  DivI64,
  DivU64,
  NegI64,
  CmpEqI32,
  CmpNeI32,
  CmpLtI32,
  CmpLeI32,
  CmpGtI32,
  CmpGeI32,
  CmpEqI64,
  CmpNeI64,
  CmpLtI64,
  CmpLeI64,
  CmpGtI64,
  CmpGeI64,
  CmpLtU64,
  CmpLeU64,
  CmpGtU64,
  CmpGeU64,
  JumpIfZero,
  Jump,
  ReturnVoid,
  ReturnI32,
  ReturnI64,
  PrintI32,
  PrintI64,
  PrintU64,
  PrintString,
  PushArgc,
  PrintArgv,
  PrintArgvUnsafe,
  LoadStringByte,
  PushF32,
  PushF64,
  AddF32,
  SubF32,
  MulF32,
  DivF32,
  NegF32,
  AddF64,
  SubF64,
  MulF64,
  DivF64,
  NegF64,
  CmpEqF32,
  CmpNeF32,
  CmpLtF32,
  CmpLeF32,
  CmpGtF32,
  CmpGeF32,
  CmpEqF64,
  CmpNeF64,
  CmpLtF64,
  CmpLeF64,
  CmpGtF64,
  CmpGeF64,
  ConvertI32ToF32,
  ConvertI32ToF64,
  ConvertI64ToF32,
  ConvertI64ToF64,
  ConvertU64ToF32,
  ConvertU64ToF64,
  ConvertF32ToI32,
  ConvertF32ToI64,
  ConvertF32ToU64,
  ConvertF64ToI32,
  ConvertF64ToI64,
  ConvertF64ToU64,
  ConvertF32ToF64,
  ConvertF64ToF32,
  ReturnF32,
  ReturnF64,
};

enum class IrStructFieldCategory : uint8_t {
  Default = 0,
  Pod,
  Handle,
  GpuLane,
};

enum class IrStructPaddingKind : uint8_t {
  None = 0,
  Align,
};

enum class IrStructVisibility : uint8_t {
  Private = 0,
  Package,
  Public,
};

struct IrStructField {
  std::string name;
  std::string envelope;
  uint32_t offsetBytes = 0;
  uint32_t sizeBytes = 0;
  uint32_t alignmentBytes = 1;
  IrStructPaddingKind paddingKind = IrStructPaddingKind::None;
  IrStructFieldCategory category = IrStructFieldCategory::Default;
  IrStructVisibility visibility = IrStructVisibility::Private;
  bool isStatic = false;
};

struct IrStructLayout {
  std::string name;
  uint32_t totalSizeBytes = 0;
  uint32_t alignmentBytes = 1;
  std::vector<IrStructField> fields;
};

constexpr uint64_t PrintFlagNewline = 1ull << 0;
constexpr uint64_t PrintFlagStderr = 1ull << 1;
constexpr uint64_t PrintFlagMask = PrintFlagNewline | PrintFlagStderr;
constexpr uint64_t PrintStringIndexShift = 2;

inline uint64_t encodePrintFlags(bool newline, bool stderrOut) {
  return (newline ? PrintFlagNewline : 0) | (stderrOut ? PrintFlagStderr : 0);
}

inline uint64_t encodePrintStringImm(uint64_t stringIndex, uint64_t flags) {
  return (stringIndex << PrintStringIndexShift) | (flags & PrintFlagMask);
}

inline uint64_t decodePrintFlags(uint64_t imm) {
  return imm & PrintFlagMask;
}

inline uint64_t decodePrintStringIndex(uint64_t imm) {
  return imm >> PrintStringIndexShift;
}

struct IrInstruction {
  IrOpcode op = IrOpcode::PushI32;
  uint64_t imm = 0;
};

struct IrFunction {
  std::string name;
  std::vector<IrInstruction> instructions;
};

struct IrModule {
  std::vector<IrFunction> functions;
  int32_t entryIndex = -1;
  std::vector<std::string> stringTable;
  std::vector<IrStructLayout> structLayouts;
};

} // namespace primec
