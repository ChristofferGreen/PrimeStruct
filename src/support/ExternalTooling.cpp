#include "primec/support/ExternalTooling.h"

#include "primec/backend/IrToOptCppEmitter.h"
#include "primec/support/CppPrecompiledHeader.h"

#include <cstdlib>

#include <string>
#include <system_error>
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

bool compileCppExecutableWithPrecompiledHeader(const ProcessRunner &runner,
                                               const std::filesystem::path &cppPath,
                                               const std::filesystem::path &outputPath,
                                               const std::filesystem::path &precompiledHeader,
                                               const std::vector<std::string> &linkArgs) {
  const auto compile = [&](bool usePrecompiledHeader) {
    std::vector<std::string> args = {"clang++", "-std=c++23", "-O0"};
    if (usePrecompiledHeader) {
      args.push_back("-include-pch");
      args.push_back(precompiledHeader.string());
    }
    args.push_back(cppPath.string());
    args.push_back("-o");
    args.push_back(outputPath.string());
    args.insert(args.end(), linkArgs.begin(), linkArgs.end());
    return commandSucceeds(runner, args);
  };
  if (precompiledHeader.empty()) {
    return compile(false);
  }
  // A header made by another compiler is refused ("built from a different branch"); the compile is repeated
  // without it, so a stale header costs time and never a failed build.
  return compile(true) || compile(false);
}

bool compileCppExecutable(const ProcessRunner &runner,
                          const std::filesystem::path &cppPath,
                          const std::filesystem::path &outputPath,
                          const std::vector<std::string> &linkArgs) {
  std::filesystem::path precompiledHeader;
#if defined(PRIMEC_GENERATED_CPP_PCH_PATH)
  // Precompiled header for the fixed stdlib #includes the generated code always starts with (see
  // src/backend/GeneratedCppRuntimePreamble.h). Left out when it was not built or is missing on disk.
  if (std::filesystem::exists(PRIMEC_GENERATED_CPP_PCH_PATH)) {
    precompiledHeader = PRIMEC_GENERATED_CPP_PCH_PATH;
  }
#endif
  return compileCppExecutableWithPrecompiledHeader(
      runner, cppPath, outputPath, precompiledHeader, linkArgs);
}

bool compileCppExecutableOptimizedWithHeaderCache(const ProcessRunner &runner,
                                                  const std::filesystem::path &cppPath,
                                                  const std::filesystem::path &outputPath,
                                                  int optimizationLevel,
                                                  const std::filesystem::path &headerCacheDir,
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
  // The runtime's standard headers are most of the compile of a small program (src/support/CppPrecompiledHeader.h).
  // The header is cached under a key of the compiler, the flags and the header text, so a stale one is never
  // used; a compile that fails with it is repeated without it, so it can only ever save time.
  const char *disabled = std::getenv("PRIMEC_OPTEXE_PCH");
  if (!headerCacheDir.empty() && (disabled == nullptr || std::string(disabled) != "0") &&
      linkArgsAllowPrecompiledHeader(linkArgs)) {
    const std::filesystem::path precompiledHeader =
        ensureCppPrecompiledHeader(runner, headerCacheDir, flags, optCppRuntimeHeaders());
    if (!precompiledHeader.empty()) {
      if (compile(precompiledHeader)) {
        return true;
      }
      if (compile({})) {
        // The program compiles, so the header was refused: it goes, and the next compile builds it again
        // instead of paying for a refused compile every time.
        std::error_code ec;
        std::filesystem::remove(precompiledHeader, ec);
        return true;
      }
      return false;
    }
  }
  return compile({});
}

bool compileCppExecutableOptimized(const ProcessRunner &runner,
                                   const std::filesystem::path &cppPath,
                                   const std::filesystem::path &outputPath,
                                   int optimizationLevel,
                                   const std::vector<std::string> &linkArgs) {
#if defined(PRIMEC_OPTEXE_PCH_DIR)
  // Inside the build directory, never outside the checkout.
  const std::filesystem::path headerCacheDir = PRIMEC_OPTEXE_PCH_DIR;
#else
  const std::filesystem::path headerCacheDir;
#endif
  return compileCppExecutableOptimizedWithHeaderCache(
      runner, cppPath, outputPath, optimizationLevel, headerCacheDir, linkArgs);
}

} // namespace primec
