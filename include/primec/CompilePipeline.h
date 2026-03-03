#pragma once

#include "primec/Ast.h"
#include "primec/Diagnostics.h"
#include "primec/Options.h"

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

struct CompilePipelineOutput {
  Program program;
  std::string filteredSource;
  std::string dumpOutput;
  bool hasDumpOutput = false;
};

struct CompilePipelineDiagnosticInfo {
  struct RecordInfo {
    std::string normalizedMessage;
    DiagnosticSpan primarySpan;
    std::vector<DiagnosticRelatedSpan> relatedSpans;
    bool hasPrimarySpan = false;
  };

  std::string normalizedMessage;
  DiagnosticSpan primarySpan;
  std::vector<DiagnosticRelatedSpan> relatedSpans;
  bool hasPrimarySpan = false;
  std::vector<RecordInfo> records;
};

void addDefaultStdlibInclude(const std::string &inputPath, std::vector<std::string> &importPaths);

bool runCompilePipeline(const Options &options,
                        CompilePipelineOutput &output,
                        CompilePipelineErrorStage &errorStage,
                        std::string &error,
                        CompilePipelineDiagnosticInfo *diagnosticInfo = nullptr);

} // namespace primec
