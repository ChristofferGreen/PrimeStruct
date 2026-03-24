#include "primec/ExternalTooling.h"

#include <string>
#include <vector>

namespace primec {

bool commandSucceeds(const ProcessRunner &runner, const std::vector<std::string> &args) {
  return runner.run(args) == 0;
}

bool findSpirvCompiler(const ProcessRunner &runner, std::string &toolName) {
  if (commandSucceeds(runner, {"glslangValidator", "-v"})) {
    toolName = "glslangValidator";
    return true;
  }
  if (commandSucceeds(runner, {"glslc", "--version"})) {
    toolName = "glslc";
    return true;
  }
  return false;
}

bool compileSpirv(const ProcessRunner &runner,
                  const std::string &toolName,
                  const std::filesystem::path &inputPath,
                  const std::filesystem::path &outputPath) {
  std::vector<std::string> args;
  if (toolName == "glslangValidator") {
    args = {"glslangValidator", "-V", "-S", "comp", inputPath.string(), "-o", outputPath.string()};
  } else if (toolName == "glslc") {
    args = {"glslc", "-fshader-stage=compute", inputPath.string(), "-o", outputPath.string()};
  } else {
    return false;
  }
  return commandSucceeds(runner, args);
}

bool compileCppExecutable(const ProcessRunner &runner,
                          const std::filesystem::path &cppPath,
                          const std::filesystem::path &outputPath) {
  return commandSucceeds(runner,
                         {"clang++", "-std=c++23", "-O0", cppPath.string(), "-o", outputPath.string()});
}

} // namespace primec
