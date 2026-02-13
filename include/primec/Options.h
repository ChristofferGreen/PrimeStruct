#pragma once

#include <string>
#include <vector>

namespace primec {
struct Options {
  std::string emitKind;
  std::string inputPath;
  std::string outputPath;
  std::string outDir = ".";
  std::string entryPath = "/main";
  std::string dumpStage;
  std::vector<std::string> textFilters = {"collections", "operators", "implicit-utf8"};
  bool singleTypeToReturn = false;
  std::vector<std::string> defaultEffects;
  std::vector<std::string> programArgs;
  std::vector<std::string> includePaths;
};
} // namespace primec
