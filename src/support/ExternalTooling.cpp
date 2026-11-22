#include "primec/support/ExternalTooling.h"

#include "primec/backend/IrToOptCppEmitter.h"
#include "primec/support/CppPrecompiledHeader.h"

#include <cstdlib>

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
                          const std::filesystem::path &outputPath,
                          const std::vector<std::string> &linkArgs) {
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
  args.insert(args.end(), linkArgs.begin(), linkArgs.end());
  return commandSucceeds(runner, args);
}

bool compileCppExecutableOptimized(const ProcessRunner &runner,
                                   const std::filesystem::path &cppPath,
                                   const std::filesystem::path &outputPath,
                                   int optimizationLevel,
                                   const std::vector<std::string> &linkArgs) {
  const int level = optimizationLevel < 0 ? 0 : (optimizationLevel > 3 ? 3 : optimizationLevel);
  const std::vector<std::string> flags = {
      "-std=c++23", "-O" + std::to_string(level), "-ffp-contract=off", "-pthread"};
  const auto compile = [&](const std::filesystem::path &precompiledHeader) {
    std::vector<std::string> args = {"clang++"};
    args.insert(args.end(), flags.begin(), flags.end());
    if (!precompiledHeader.empty()) {
      args.push_back("-include-pch");
      args.push_back(precompiledHeader.string());
    }
    args.push_back(cppPath.string());
    args.push_back("-o");
    args.push_back(outputPath.string());
    args.insert(args.end(), linkArgs.begin(), linkArgs.end());
    return commandSucceeds(runner, args);
  };
#if defined(PRIMEC_OPTEXE_PCH_DIR)
  // The runtime's standard headers are most of the compile of a small program (src/support/CppPrecompiledHeader.h).
  // The header is cached inside the build directory under a key of the compiler, the flags and the header text, so
  // a stale one is never used; a compile that fails with it is repeated without it, so it can only ever save time.
  const char *disabled = std::getenv("PRIMEC_OPTEXE_PCH");
  if ((disabled == nullptr || std::string(disabled) != "0") &&
      linkArgsAllowPrecompiledHeader(linkArgs)) {
    const std::filesystem::path precompiledHeader =
        ensureCppPrecompiledHeader(runner, PRIMEC_OPTEXE_PCH_DIR, flags, optCppRuntimeHeaders());
    if (!precompiledHeader.empty()) {
      if (compile(precompiledHeader)) {
        return true;
      }
    }
  }
#endif
  return compile({});
}

} // namespace primec
