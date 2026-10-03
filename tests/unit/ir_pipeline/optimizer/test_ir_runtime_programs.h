#pragma once

#include "primec/ir/Ir.h"

#include "test_ir_optimizer_helpers.h"

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

} // namespace optimizer_test
