#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "primec/IrSerializer.h"
#include "primec/testing/CompilePipelineDumpHelpers.h"
#include "test_ir_pipeline_helpers.h"

namespace {

inline std::string snapshotDiagnosticReport(const primec::DiagnosticSinkReport &report) {
  std::string snapshot = "message=" + report.message + "\n";
  snapshot += "primary=" + report.primarySpan.file + ":" + std::to_string(report.primarySpan.line) + ":" +
              std::to_string(report.primarySpan.column) + ":" + std::to_string(report.primarySpan.endLine) + ":" +
              std::to_string(report.primarySpan.endColumn) + ":" + (report.hasPrimarySpan ? "1" : "0") + "\n";
  for (size_t relatedIndex = 0; relatedIndex < report.relatedSpans.size(); ++relatedIndex) {
    const auto &related = report.relatedSpans[relatedIndex];
    snapshot += "related[" + std::to_string(relatedIndex) + "]=" + related.label + "@" + related.span.file + ":" +
                std::to_string(related.span.line) + ":" + std::to_string(related.span.column) + ":" +
                std::to_string(related.span.endLine) + ":" + std::to_string(related.span.endColumn) + "\n";
  }
  for (size_t recordIndex = 0; recordIndex < report.records.size(); ++recordIndex) {
    const auto &record = report.records[recordIndex];
    snapshot += "record[" + std::to_string(recordIndex) + "]=" + record.message + "@" + record.primarySpan.file +
                ":" + std::to_string(record.primarySpan.line) + ":" + std::to_string(record.primarySpan.column) + ":" +
                std::to_string(record.primarySpan.endLine) + ":" + std::to_string(record.primarySpan.endColumn) + ":" +
                (record.hasPrimarySpan ? "1" : "0") + "\n";
    for (size_t relatedIndex = 0; relatedIndex < record.relatedSpans.size(); ++relatedIndex) {
      const auto &related = record.relatedSpans[relatedIndex];
      snapshot += "record_related[" + std::to_string(recordIndex) + "][" + std::to_string(relatedIndex) + "]=" +
                  related.label + "@" + related.span.file + ":" + std::to_string(related.span.line) + ":" +
                  std::to_string(related.span.column) + ":" + std::to_string(related.span.endLine) + ":" +
                  std::to_string(related.span.endColumn) + "\n";
    }
  }
  return snapshot;
}

struct TypeResolverPipelineSnapshot {
  bool ok = false;
  primec::CompilePipelineErrorStage errorStage = primec::CompilePipelineErrorStage::None;
  std::string error;
  std::string diagnosticSnapshot;
  std::vector<uint8_t> serializedIr;
};

inline TypeResolverPipelineSnapshot runTypeResolverPipelineSnapshot(const std::string &source,
                                                                   const std::string &entry = "/main") {
  TypeResolverPipelineSnapshot snapshot;
  primec::testing::CompilePipelinePreparedIr prepared;
  primec::CompilePipelineDiagnosticInfo diagnosticInfo;
  snapshot.ok = primec::testing::prepareCompilePipelineIrForTesting(
      source, entry, "vm", prepared, snapshot.error, &diagnosticInfo);
  snapshot.errorStage = prepared.errorStage;
  snapshot.diagnosticSnapshot = snapshotDiagnosticReport(diagnosticInfo);
  if (snapshot.ok) {
    std::string serializeError;
    const bool serialized = primec::serializeIr(prepared.ir, snapshot.serializedIr, serializeError);
    if (!serialized) {
      snapshot.ok = false;
      snapshot.error = serializeError;
    }
  }
  return snapshot;
}

} // namespace
