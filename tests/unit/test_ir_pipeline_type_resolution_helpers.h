#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "test_ir_pipeline_helpers.h"

namespace {

struct TypeResolverPipelineSnapshot {
  bool ok = false;
  primec::CompilePipelineErrorStage errorStage = primec::CompilePipelineErrorStage::None;
  std::string error;
  primec::CompilePipelineDiagnosticInfo diagnosticInfo;
};

inline TypeResolverPipelineSnapshot runTypeResolverPipelineSnapshot(const std::string &source,
                                                                   const std::string &entry = "/main") {
  TypeResolverPipelineSnapshot snapshot;
  PreparedCompilePipelineIrForTesting prepared;
  primec::CompilePipelineDiagnosticInfo diagnosticInfo;
  snapshot.ok = prepareIrThroughCompilePipeline(
      source, entry, "vm", prepared, snapshot.error, &diagnosticInfo);
  snapshot.errorStage = prepared.errorStage;
  snapshot.diagnosticInfo = std::move(diagnosticInfo);
  return snapshot;
}

} // namespace
