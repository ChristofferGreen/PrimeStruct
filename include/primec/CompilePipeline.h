#pragma once

#include "primec/Ast.h"
#include "primec/Diagnostics.h"
#include "primec/Options.h"
#include "primec/SemanticProduct.h"

#include <string>

namespace primec {

enum class CompilePipelineErrorStage {
  None,
  Import,
  Transform,
  Parse,
  UnsupportedDumpStage,
  Semantic,
};

using CompilePipelineDiagnosticInfo = DiagnosticSinkReport;

struct CompilePipelineFailure {
  CompilePipelineErrorStage stage = CompilePipelineErrorStage::None;
  std::string message;
  CompilePipelineDiagnosticInfo diagnosticInfo;
};

struct CompilePipelineOutput {
  Program program;
  SemanticProgram semanticProgram;
  bool hasSemanticProgram = false;
  bool semanticProductRequested = false;
  bool semanticProductBuilt = false;
  std::string filteredSource;
  std::string dumpOutput;
  bool hasDumpOutput = false;
  CompilePipelineFailure failure;
  bool hasFailure = false;
};

void addDefaultStdlibInclude(const std::string &inputPath, std::vector<std::string> &importPaths);

bool runCompilePipeline(const Options &options,
                        CompilePipelineOutput &output,
                        CompilePipelineErrorStage &errorStage,
                        std::string &error,
                        CompilePipelineDiagnosticInfo *diagnosticInfo = nullptr);

} // namespace primec
