#pragma once

#include <string>
#include <vector>

namespace primec {
struct Options {
  std::string emitKind;
  std::string inputPath;
  std::string outputPath;
  std::string entryPath = "/main";
  std::string dumpStage;
  bool implicitI32Suffix = false;
  std::vector<std::string> includePaths;
};
} // namespace primec
