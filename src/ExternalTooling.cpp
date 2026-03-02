#include "primec/ExternalTooling.h"

#include <string>

namespace primec {

std::string quoteShellPath(const std::filesystem::path &path) {
  std::string text = path.string();
  std::string quoted = "\"";
  for (char c : text) {
    if (c == '\"') {
      quoted += "\\\"";
    } else {
      quoted += c;
    }
  }
  quoted += "\"";
  return quoted;
}

bool commandSucceeds(const ProcessRunner &runner, const std::string &command) {
  return runner.run(command) == 0;
}

bool findSpirvCompiler(const ProcessRunner &runner, std::string &toolName) {
  if (commandSucceeds(runner, "glslangValidator -v > /dev/null 2>&1")) {
    toolName = "glslangValidator";
    return true;
  }
  if (commandSucceeds(runner, "glslc --version > /dev/null 2>&1")) {
    toolName = "glslc";
    return true;
  }
  return false;
}

bool compileSpirv(const ProcessRunner &runner,
                  const std::string &toolName,
                  const std::filesystem::path &inputPath,
                  const std::filesystem::path &outputPath) {
  std::string command;
  if (toolName == "glslangValidator") {
    command = "glslangValidator -V -S comp " + quoteShellPath(inputPath) + " -o " + quoteShellPath(outputPath);
  } else if (toolName == "glslc") {
    command = "glslc -fshader-stage=compute " + quoteShellPath(inputPath) + " -o " + quoteShellPath(outputPath);
  } else {
    return false;
  }
  return commandSucceeds(runner, command);
}

bool compileCppExecutable(const ProcessRunner &runner,
                          const std::filesystem::path &cppPath,
                          const std::filesystem::path &outputPath) {
  std::string command = "clang++ -std=c++23 -O2 " + quoteShellPath(cppPath) + " -o " + quoteShellPath(outputPath);
  return commandSucceeds(runner, command);
}

} // namespace primec
