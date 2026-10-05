#pragma once

#include "primec/ir/Ir.h"

#include "test_ir_optimizer_helpers.h"

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

// Hand-written IR programs shared by the differential tests of the VM kernels
// and the optexe emitter: calls and recursion, heap and indirect addressing,
// strings/argv/files, and one module per runtime fault.
namespace optimizer_test {

inline primec::IrFunction functionOf(const std::string &name,
                                     std::vector<primec::IrInstruction> instructions,
                                     uint32_t parameterCount = 0) {
  primec::IrFunction function;
  function.name = name;
  function.parameterCount = parameterCount;
  function.instructions = std::move(instructions);
  function.metadata.effectMask = primec::EffectIoOut;
  function.metadata.capabilityMask = primec::EffectIoOut;
  return function;
}

// A module whose entry (function 0) calls functions 1..N in order, then returns 0.
inline primec::IrModule driverModule(std::vector<primec::IrFunction> bodies) {
  primec::IrModule module;
  module.entryIndex = 0;
  std::vector<primec::IrInstruction> driver;
  for (size_t i = 0; i < bodies.size(); ++i) {
    driver.push_back({primec::IrOpcode::CallVoid, i + 1});
  }
  driver.push_back({primec::IrOpcode::PushI32, 0});
  driver.push_back({primec::IrOpcode::ReturnI32, 0});
  module.functions.push_back(functionOf("/main", std::move(driver)));
  for (primec::IrFunction &body : bodies) {
    module.functions.push_back(std::move(body));
  }
  return module;
}

inline primec::IrModule callsProgram() {
  primec::IrModule module;
  module.entryIndex = 0;
  // main: print(sub(10, 3) via /sub), print(fib(15)), then exit with fib(10) - 50.
  module.functions.push_back(functionOf("/main",
                                        assemble({"PushI64 10",
                                                  "PushI64 3",
                                                  "Call 1",
                                                  "PrintI64 1",
                                                  "PushI32 15",
                                                  "Call 2",
                                                  "PrintI32 1",
                                                  "PushI32 10",
                                                  "Call 2",
                                                  "PushI32 50",
                                                  "SubI32",
                                                  "ReturnI32"})));
  module.functions.push_back(functionOf("/sub", assemble({"SubI64", "ReturnI64"}), 2));
  // fib(n): n < 2 ? n : fib(n-1) + fib(n-2)
  module.functions.push_back(functionOf("/fib",
                                        assemble({"Dup",
                                                  "PushI32 2",
                                                  "CmpLtI32",
                                                  "JumpIfZero 5",
                                                  "ReturnI32",
                                                  "Dup",
                                                  "PushI32 1",
                                                  "SubI32",
                                                  "Call 2",
                                                  "StoreLocal 0",
                                                  "PushI32 2",
                                                  "SubI32",
                                                  "Call 2",
                                                  "LoadLocal 0",
                                                  "AddI32",
                                                  "ReturnI32"}),
                                        1));
  return module;
}

inline primec::IrModule heapProgram() {
  primec::IrModule module = moduleOf(assemble({"PushI64 3",
                                               "HeapAlloc",
                                               "StoreLocal 0",
                                               "LoadLocal 0",
                                               "PushI64 11",
                                               "StoreIndirect",
                                               "PrintI64 1",
                                               "LoadLocal 0",
                                               "PushI64 16",
                                               "AddI64",
                                               "PushI64 22",
                                               "StoreIndirect",
                                               "Pop",
                                               "LoadLocal 0",
                                               "PushI64 6",
                                               "HeapRealloc",
                                               "StoreLocal 0",
                                               "LoadLocal 0",
                                               "LoadIndirect",
                                               "PrintI64 1",
                                               "LoadLocal 0",
                                               "PushI64 16",
                                               "AddI64",
                                               "LoadIndirect",
                                               "PrintI64 1",
                                               "LoadLocal 0",
                                               "PushI64 80",
                                               "AddI64",
                                               "LoadIndirect",
                                               "PrintI64 1",
                                               "LoadLocal 0",
                                               "HeapFree",
                                               "PushI64 0",
                                               "HeapFree",
                                               "PushI64 99",
                                               "StoreLocal 1",
                                               "AddressOfLocal 1",
                                               "PushI64 123",
                                               "StoreIndirect",
                                               "Pop",
                                               "LoadLocal 1",
                                               "PrintI64 1",
                                               "AddressOfLocal 1",
                                               "LoadIndirect",
                                               "PrintI64 1",
                                               "PushI64 0",
                                               "HeapAlloc",
                                               "PrintU64 1",
                                               "PushI32 0",
                                               "ReturnI32"}));
  module.functions[0].metadata.effectMask = primec::EffectIoOut;
  return module;
}

// Freed heap slots are reused: a new allocation of them carries the next generation in its
// address (bits 48-62), starts zeroed, and a reallocation that does not fit moves to new slots.
inline primec::IrModule heapReuseProgram() {
  primec::IrModule module = moduleOf(
      assemble({"PushI64 2",     "HeapAlloc",   "StoreLocal 0", "LoadLocal 0",  "PushI64 5",
                "StoreIndirect", "Pop",         "LoadLocal 0",  "PrintU64 1",   "LoadLocal 0",
                "HeapFree",      "PushI64 2",   "HeapAlloc",    "StoreLocal 1", "LoadLocal 1",
                "PrintU64 1",    "LoadLocal 1", "LoadIndirect", "PrintI64 1",   "PushI64 1",
                "HeapAlloc",     "PrintU64 1",  "LoadLocal 1",  "PushI64 7",    "StoreIndirect",
                "Pop",           "LoadLocal 1", "PushI64 4",    "HeapRealloc",  "StoreLocal 1",
                "LoadLocal 1",   "PrintU64 1",  "LoadLocal 1",  "LoadIndirect", "PrintI64 1",
                "PushI64 2",     "HeapAlloc",   "PrintU64 1",   "PushI32 0",    "ReturnI32"}));
  module.functions[0].metadata.effectMask = primec::EffectIoOut;
  return module;
}

inline primec::IrModule ioProgram(const std::string &file) {
  primec::IrModule module;
  module.entryIndex = 0;
  module.stringTable = {
      "hello", std::string("a\0b\"q?\n", 7), file, "tail", file + ".does_not_exist"};
  module.functions.push_back(
      functionOf("/main",
                 assemble({// The read target (local 1) must be a real local for the VM too.
                           "PushI64 0",
                           "StoreLocal 1",
                           // Static and dynamic string access.
                           "PushI64 1",
                           "LoadStringByte 0",
                           "PrintI32 1",
                           "PushI64 3",
                           "LoadStringByte 1",
                           "PrintI32 1",
                           "PushI64 0",
                           "LoadStringLength",
                           "PrintU64 1",
                           "PushI64 1",
                           "LoadStringLength",
                           "PrintU64 1",
                           "PushI64 3",
                           "PrintStringDynamic 1",
                           "PushI64 0",
                           "PushI64 4",
                           "LoadStringByteDynamic",
                           "PrintI32 1",
                           // Arguments.
                           "PushArgc",
                           "PrintI32 1",
                           "PushI32 1",
                           "PrintArgv 1",
                           "PushI32 2",
                           "PrintArgv 1",
                           "PushI32 9",
                           "PrintArgvUnsafe 1",
                           // Write a file, close, read it back byte by byte.
                           "FileOpenWrite 2",
                           "StoreLocal 0",
                           "LoadLocal 0",
                           "PushI64 12345",
                           "FileWriteI64",
                           "Pop",
                           "LoadLocal 0",
                           "PushI32 65",
                           "FileWriteByte",
                           "Pop",
                           "LoadLocal 0",
                           "FileWriteNewline",
                           "Pop",
                           "LoadLocal 0",
                           "FileWriteString 0",
                           "Pop",
                           "LoadLocal 0",
                           "PushI64 3",
                           "FileWriteStringDynamic",
                           "Pop",
                           "LoadLocal 0",
                           "PushI64 18446744073709551615",
                           "FileWriteU64",
                           "Pop",
                           "LoadLocal 0",
                           "PushI64 7",
                           "FileWriteI32",
                           "Pop",
                           "LoadLocal 0",
                           "FileFlush",
                           "PrintU64 1",
                           "LoadLocal 0",
                           "FileClose",
                           "PrintU64 1",
                           "FileOpenRead 2",
                           "StoreLocal 0",
                           "LoadLocal 0",
                           "FileReadByte 1",
                           "PrintU64 1",
                           "LoadLocal 1",
                           "PrintU64 1",
                           "LoadLocal 0",
                           "FileClose",
                           "Pop",
                           "FileOpenRead 4",
                           "PrintU64 1",
                           "PushI64 2",
                           "FileOpenAppendDynamic",
                           "StoreLocal 0",
                           "LoadLocal 0",
                           "FileWriteNewline",
                           "PrintU64 1",
                           "LoadLocal 0",
                           "FileClose",
                           "Pop",
                           "PushI32 0",
                           "ReturnI32"})));
  return module;
}

struct FaultProgram {
  std::string name;
  primec::IrModule module;
};

inline std::vector<FaultProgram> faultPrograms() {
  struct Case {
    const char *name;
    std::vector<const char *> code;
    uint32_t extraFunctions;
  };
  const std::vector<Case> cases = {
      {"div_zero", {"PushI64 1", "PushI64 0", "DivI64", "ReturnI64"}, 0},
      {"divu_zero", {"PushI64 1", "PushI64 0", "DivU64", "ReturnI64"}, 0},
      {"free_invalid", {"PushI64 9223372036854775824", "HeapFree", "ReturnVoid"}, 0},
      {"realloc_invalid",
       {"PushI64 9223372036854775824", "PushI64 2", "HeapRealloc", "Pop", "ReturnVoid"},
       0},
      {"unaligned", {"PushI64 8", "LoadIndirect", "ReturnI64"}, 0},
      {"stack_address", {"PushI64 4096", "LoadIndirect", "ReturnI64"}, 0},
      {"use_after_free",
       {"PushI64 2",
        "HeapAlloc",
        "StoreLocal 0",
        "LoadLocal 0",
        "HeapFree",
        "LoadLocal 0",
        "LoadIndirect",
        "ReturnI64"},
       0},
      {"stale_after_reuse",
       {"PushI64 2",
        "HeapAlloc",
        "StoreLocal 0",
        "LoadLocal 0",
        "HeapFree",
        "PushI64 2",
        "HeapAlloc",
        "Pop",
        "LoadLocal 0",
        "LoadIndirect",
        "ReturnI64"},
       0},
      {"free_stale_after_reuse",
       {"PushI64 1",
        "HeapAlloc",
        "StoreLocal 0",
        "LoadLocal 0",
        "HeapFree",
        "PushI64 1",
        "HeapAlloc",
        "Pop",
        "LoadLocal 0",
        "HeapFree",
        "ReturnVoid"},
       0},
      {"string_bounds", {"PushI64 99", "LoadStringByte 0", "ReturnI32"}, 0},
      {"string_index", {"PushI64 7", "LoadStringLength", "ReturnI64"}, 0},
      {"argv_index", {"PushI32 5", "PrintArgv 1", "ReturnVoid"}, 0},
      {"recursion", {"CallVoid 1", "ReturnVoid"}, 1},
  };
  std::vector<FaultProgram> programs;
  for (const Case &testCase : cases) {
    primec::IrModule module = moduleOf({});
    module.stringTable = {"abc"};
    std::vector<primec::IrInstruction> code;
    for (const char *line : testCase.code) {
      code.push_back(assembleOne(line));
    }
    module.functions[0] = functionOf("/main", std::move(code));
    if (testCase.extraFunctions > 0) {
      module.functions.push_back(functionOf("/recurse", assemble({"CallVoid 1", "ReturnVoid"})));
    }
    programs.push_back({testCase.name, std::move(module)});
  }
  return programs;
}

// The loop fuses sequences such as `LoadLocal a; Push c; Cmp; JumpIfZero` into
// single instructions. This module exercises every fused form over operands
// with the signs and widths that tell the comparisons and constant widths apart,
// printing 0/1 for each branch so any wrong fusion shows up as different output.
inline primec::IrModule fusedFormsProgram() {
  using primec::IrInstruction;
  using primec::IrOpcode;
  const std::vector<uint64_t> values = {0,
                                        1,
                                        static_cast<uint64_t>(-1),
                                        5,
                                        static_cast<uint64_t>(-5),
                                        0x7FFFFFFFull,
                                        0x80000000ull,
                                        0x8000000000000000ull,
                                        0x7FFFFFFFFFFFFFFFull};
  const std::vector<IrOpcode> comparisons = {IrOpcode::CmpEqI32,
                                             IrOpcode::CmpNeI32,
                                             IrOpcode::CmpLtI32,
                                             IrOpcode::CmpLeI32,
                                             IrOpcode::CmpGtI32,
                                             IrOpcode::CmpGeI64,
                                             IrOpcode::CmpLtI64};
  std::vector<IrInstruction> code;
  const auto emit = [&](IrOpcode op, uint64_t imm = 0) {
    code.push_back({op, imm});
    return code.size() - 1;
  };
  // Branches print 1 when the comparison holds and 0 otherwise.
  const auto printBranch = [&](size_t jumpIfZeroIndex) {
    emit(IrOpcode::PushI32, 1);
    emit(IrOpcode::PrintI32, primec::PrintFlagNewline);
    const size_t skip = emit(IrOpcode::Jump);
    code[jumpIfZeroIndex].imm = code.size();
    emit(IrOpcode::PushI32, 0);
    emit(IrOpcode::PrintI32, primec::PrintFlagNewline);
    code[skip].imm = code.size();
  };
  for (const uint64_t lhs : values) {
    for (const uint64_t rhs : values) {
      for (const IrOpcode cmp : comparisons) {
        emit(IrOpcode::PushI64, lhs);
        emit(IrOpcode::StoreLocal, 0);
        emit(IrOpcode::PushI64, rhs);
        emit(IrOpcode::StoreLocal, 1);
        // local, constant (PushI32 sign-extends, PushI64 does not)
        emit(IrOpcode::LoadLocal, 0);
        emit(IrOpcode::PushI32, rhs);
        emit(cmp);
        printBranch(emit(IrOpcode::JumpIfZero));
        emit(IrOpcode::LoadLocal, 0);
        emit(IrOpcode::PushI64, rhs);
        emit(cmp);
        printBranch(emit(IrOpcode::JumpIfZero));
        // local, local
        emit(IrOpcode::LoadLocal, 0);
        emit(IrOpcode::LoadLocal, 1);
        emit(cmp);
        printBranch(emit(IrOpcode::JumpIfZero));
        // two stack values from computed operands
        emit(IrOpcode::LoadLocal, 0);
        emit(IrOpcode::PushI64, 0);
        emit(IrOpcode::AddI64);
        emit(IrOpcode::LoadLocal, 1);
        emit(cmp);
        printBranch(emit(IrOpcode::JumpIfZero));
      }
      // Arithmetic with a local and a constant or a second local, stored back.
      emit(IrOpcode::PushI64, lhs);
      emit(IrOpcode::StoreLocal, 0);
      emit(IrOpcode::PushI64, rhs);
      emit(IrOpcode::StoreLocal, 1);
      for (const IrOpcode arithmetic : {IrOpcode::AddI64, IrOpcode::SubI32, IrOpcode::MulI64}) {
        emit(IrOpcode::LoadLocal, 0);
        emit(IrOpcode::PushI32, rhs);
        emit(arithmetic);
        emit(IrOpcode::PrintI64, primec::PrintFlagNewline);
        emit(IrOpcode::LoadLocal, 0);
        emit(IrOpcode::LoadLocal, 1);
        emit(arithmetic);
        emit(IrOpcode::PrintI64, primec::PrintFlagNewline);
      }
      emit(IrOpcode::LoadLocal, 0);
      emit(IrOpcode::PushI32, rhs);
      emit(IrOpcode::AddI32);
      emit(IrOpcode::StoreLocal, 2);
      emit(IrOpcode::LoadLocal, 1);
      emit(IrOpcode::PushI64, lhs);
      emit(IrOpcode::SubI64);
      emit(IrOpcode::StoreLocal, 3);
      emit(IrOpcode::LoadLocal, 2);
      emit(IrOpcode::PrintI64, primec::PrintFlagNewline);
      emit(IrOpcode::LoadLocal, 3);
      emit(IrOpcode::PrintI64, primec::PrintFlagNewline);
      // Constant stores, copies, the assignment idiom and branch on a local.
      emit(IrOpcode::PushI32, rhs);
      emit(IrOpcode::StoreLocal, 4);
      emit(IrOpcode::LoadLocal, 4);
      emit(IrOpcode::StoreLocal, 5);
      emit(IrOpcode::LoadLocal, 5);
      emit(IrOpcode::PushI64, 3);
      emit(IrOpcode::AddI64);
      emit(IrOpcode::Dup);
      emit(IrOpcode::StoreLocal, 6);
      emit(IrOpcode::Pop);
      emit(IrOpcode::LoadLocal, 6);
      emit(IrOpcode::PrintI64, primec::PrintFlagNewline);
      emit(IrOpcode::LoadLocal, 5);
      printBranch(emit(IrOpcode::JumpIfZero));
    }
  }
  emit(IrOpcode::PushI32, 0);
  emit(IrOpcode::ReturnI32);
  primec::IrModule module = optimizer_test::moduleOf(std::move(code));
  module.functions[0].metadata.effectMask = primec::EffectIoOut;
  return module;
}

} // namespace optimizer_test
