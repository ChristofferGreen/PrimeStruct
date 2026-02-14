#pragma once

#include <string>
#include <vector>

#include "primec/TextFilterPipeline.h"

namespace primec {
struct Options {
  std::string emitKind;
  std::string inputPath;
  std::string outputPath;
  std::string outDir = ".";
  std::string entryPath = "/main";
  std::string dumpStage;
  std::vector<std::string> textFilters = {"collections", "operators", "implicit-utf8"};
  std::vector<TextTransformRule> textTransformRules;
  std::vector<std::string> semanticTransforms;
  bool requireCanonicalSyntax = false;
  std::vector<std::string> defaultEffects = {"io_out", "io_err"};
  std::vector<std::string> programArgs;
  std::vector<std::string> includePaths;
};
} // namespace primec
