#pragma once

#include <string>
#include <vector>

namespace primec {
struct Options {
  std::string emitKind;
  std::string inputPath;
  std::string outputPath;
  std::string outDir = "build";
  std::string entryPath = "/main";
  std::string dumpStage;
  std::vector<std::string> textFilters = {"operators", "collections", "implicit-utf8"};
  std::vector<std::string> defaultEffects;
  std::vector<std::string> includePaths;
};
} // namespace primec
