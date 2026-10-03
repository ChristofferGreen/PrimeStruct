#pragma once

#include "primec/support/ProcessRunner.h"

#include <filesystem>
#include <string>
#include <vector>

namespace primec {

bool commandSucceeds(const ProcessRunner &runner, const std::vector<std::string> &args);

bool findSpirvCompiler(const ProcessRunner &runner, std::string &toolName);

bool compileSpirv(const ProcessRunner &runner,
                  const std::string &toolName,
                  const std::filesystem::path &inputPath,
                  const std::filesystem::path &outputPath);

bool compileCppExecutable(const ProcessRunner &runner,
                          const std::filesystem::path &cppPath,
                          const std::filesystem::path &outputPath);

// Compiles generated C++ at host optimization level `optimizationLevel` (0..3)
// with floating-point contraction disabled, so results match the VM's separate
// IEEE operations. Used for the optexe emit kind, whose output is meant to be
// fast; compileCppExecutable keeps -O0 for the stack-machine emitter.
bool compileCppExecutableOptimized(const ProcessRunner &runner,
                                   const std::filesystem::path &cppPath,
                                   const std::filesystem::path &outputPath,
                                   int optimizationLevel);

} // namespace primec
