#pragma once

#include "primec/Ast.h"
#include "primec/Options.h"

#include <string>

namespace primec {

enum class CompilePipelineErrorStage {
  None,
  Include,
  Transform,
  Parse,
  UnsupportedDumpStage,
  Semantic,
};

struct CompilePipelineOutput {
  Program program;
  std::string filteredSource;
  std::string dumpOutput;
  bool hasDumpOutput = false;
};

void addDefaultStdlibInclude(const std::string &inputPath, std::vector<std::string> &includePaths);

bool runCompilePipeline(const Options &options,
                        CompilePipelineOutput &output,
                        CompilePipelineErrorStage &errorStage,
                        std::string &error);

} // namespace primec
