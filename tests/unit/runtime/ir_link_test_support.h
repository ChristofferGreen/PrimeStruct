#pragma once

#include "primec/ir/Ir.h"
#include "primec/runtime/Vm.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

// Hand-built IR modules for the link tests (TODO-5922): a consumer that calls an import and a library that defines it.
namespace ir_link_test {

using primec::IrFunction;
using primec::IrInstruction;
using primec::IrModule;
using primec::IrOpcode;

inline IrFunction
functionOf(std::string name, uint32_t parameterCount, std::vector<IrInstruction> instructions) {
  IrFunction function;
  function.name = std::move(name);
  function.parameterCount = parameterCount;
  function.instructions = std::move(instructions);
  return function;
}

inline IrFunction importOf(std::string name, uint32_t parameterCount, std::string library) {
  IrFunction function;
  function.name = std::move(name);
  function.parameterCount = parameterCount;
  function.isImport = true;
  function.importLibrary = std::move(library);
  return function;
}

// entryIndex 0 is the program's entry; a library passes -1.
inline IrModule moduleOf(std::vector<IrFunction> functions, int32_t entryIndex = 0) {
  IrModule module;
  module.functions = std::move(functions);
  module.entryIndex = entryIndex;
  return module;
}

struct Run {
  bool ok = false;
  int32_t value = 0;
  std::string error;
  std::string out;
};

inline void appendOutput(int fd, std::string_view chunk, void *userData) {
  if (fd == 1) {
    static_cast<std::string *>(userData)->append(chunk);
  }
}

inline Run run(const IrModule &module) {
  Run result;
  primec::Vm vm;
  vm.setOutputSink({&appendOutput, &result.out});
  uint64_t raw = 0;
  result.ok = vm.execute(module, raw, result.error, std::vector<std::string_view>{"prog"});
  result.value = static_cast<int32_t>(raw);
  return result;
}

inline constexpr uint64_t Newline = primec::PrintFlagNewline;

// The program of most tests: main calls /lib/add_one on 20, which the library defines as `x + twice(0) + 1`,
// where /lib/twice is a second library function, so the call inside the library is remapped too.
inline IrModule consumerModule() {
  return moduleOf(
      {functionOf(
           "/main", 0, {{IrOpcode::PushI32, 20}, {IrOpcode::Call, 1}, {IrOpcode::ReturnI32, 0}}),
       importOf("/lib/add_one", 1, "lib")});
}

inline IrModule libraryModule() {
  return moduleOf({functionOf("/lib/add_one",
                              1,
                              {{IrOpcode::StoreLocal, 0},
                               {IrOpcode::LoadLocal, 0},
                               {IrOpcode::PushI32, 0},
                               {IrOpcode::Call, 1},
                               {IrOpcode::AddI32, 0},
                               {IrOpcode::PushI32, 1},
                               {IrOpcode::AddI32, 0},
                               {IrOpcode::ReturnI32, 0}}),
                   functionOf("/lib/twice",
                              1,
                              {{IrOpcode::StoreLocal, 0},
                               {IrOpcode::LoadLocal, 0},
                               {IrOpcode::LoadLocal, 0},
                               {IrOpcode::AddI32, 0},
                               {IrOpcode::ReturnI32, 0}})},
                  -1);
}

} // namespace ir_link_test
