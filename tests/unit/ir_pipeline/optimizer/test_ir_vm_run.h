#pragma once

#include "primec/ir/Ir.h"
#include "primec/runtime/Vm.h"

#include <cstdint>
#include <cstdio>
#include <string>
#include <string_view>
#include <vector>

// Runs a module in the VM and captures what it prints, for differential tests.
namespace optimizer_test {

struct Outcome {
  bool ok = false;
  uint64_t result = 0;
  std::string error;
  std::string output;

  bool operator==(const Outcome &other) const {
    return ok == other.ok && result == other.result && error == other.error &&
           output == other.output;
  }
};

// What the VM printed, received through the VM's output sink (in process, no fd redirection).
struct CapturedOutput {
  std::string out;
  std::string err;
};

inline void collectOutput(int fd, std::string_view chunk, void *userData) {
  auto *captured = static_cast<CapturedOutput *>(userData);
  (fd == 2 ? captured->err : captured->out).append(chunk);
}

inline Outcome run(const primec::IrModule &module, const std::vector<std::string_view> &args = {}) {
  Outcome outcome;
  CapturedOutput captured;
  primec::Vm vm;
  vm.setOutputSink({&collectOutput, &captured});
  outcome.ok = vm.execute(module, outcome.result, outcome.error, args);
  outcome.output = std::move(captured.out);
  return outcome;
}

} // namespace optimizer_test
