#pragma once

#include "primec/ProcessRunner.h"

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

} // namespace primec
