#include "primec/support/ExternalTooling.h"

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
  std::vector<std::string> args = {"clang++", "-std=c++23", "-O0"};
#if defined(PRIMEC_GENERATED_CPP_PCH_PATH)
  // Precompiled header for the fixed stdlib #includes IrToCppEmitter.cpp
  // always emits ahead of the program-specific generated code - see
  // src/GeneratedCppRuntimePreamble.h. Falls back to a normal (slower)
  // compile if the PCH wasn't built (e.g. clang++ missing at configure
  // time) or has gone stale/missing on disk.
  if (std::filesystem::exists(PRIMEC_GENERATED_CPP_PCH_PATH)) {
    args.push_back("-include-pch");
    args.push_back(PRIMEC_GENERATED_CPP_PCH_PATH);
  }
#endif
  args.push_back(cppPath.string());
  args.push_back("-o");
  args.push_back(outputPath.string());
  return commandSucceeds(runner, args);
}

} // namespace primec
