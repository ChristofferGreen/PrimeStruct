#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace primec::embed {

// Outcome of compiling or running a script. Failures are reported as data:
// the embed API never exits the process or writes to stdout/stderr itself.
struct ScriptResult {
  bool ok = false;
  // Return value of `main` truncated to 32 bits, like the primevm exit code.
  int exitCode = 0;
  // Diagnostic text in the same plain format the CLI prints on failure.
  std::string diagnostics;
};

// A compiled script. Cheap to copy (shares the compiled module); `run` may be
// called repeatedly without recompiling.
class Script {
public:
  Script() = default;

  // False when compilation failed; `diagnostics()` then says why.
  bool valid() const { return module_ != nullptr; }
  const std::string &diagnostics() const { return diagnostics_; }

  // Runs the entry definition. `args` become the script's argv after the
  // leading program name.
  ScriptResult run(const std::vector<std::string> &args = {}) const;

  // Serializes the compiled module to portable bytecode. Returns false with
  // `error` set when the script is not valid. Ship the bytes to a host that
  // links only the runtime-only library (for example iOS, where in-process
  // code generation is unavailable) and load them with `loadBytecode`.
  bool saveBytecode(std::vector<uint8_t> &out, std::string &error) const;

  // Loads bytecode produced by `saveBytecode`. The module is validated for
  // the VM; corrupt, truncated, or incompatible bytes yield an invalid Script
  // whose `diagnostics()` explains why.
  static Script loadBytecode(const std::vector<uint8_t> &bytes, const std::string &name = "script");

private:
  friend class ScriptEngine;
  struct Module;
  std::shared_ptr<const Module> module_;
  std::string name_;
  std::string diagnostics_;
};

} // namespace primec::embed
