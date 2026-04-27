#include "primec/SemanticValidationResult.h"

#include <utility>

namespace primec {

SemanticValidationResultSink::SemanticValidationResultSink(
    std::string &legacyError,
    DiagnosticSinkReport *diagnosticInfo,
    bool collectDiagnostics)
    : legacyError_(legacyError),
      diagnosticInfo_(diagnosticInfo),
      diagnosticSink_(diagnosticInfo),
      collectDiagnostics_(collectDiagnostics) {}

void SemanticValidationResultSink::reset() {
  diagnosticSink_.reset();
}

bool SemanticValidationResultSink::shouldCollectStructuredDiagnostics() const {
  return collectDiagnostics_ && diagnosticInfo_ != nullptr;
}

bool SemanticValidationResultSink::hasFailure() const {
  return !legacyError_.empty();
}

const std::string &SemanticValidationResultSink::message() const {
  return legacyError_;
}

void SemanticValidationResultSink::clearContext() {
  if (!shouldCollectStructuredDiagnostics()) {
    return;
  }
  diagnosticSink_.clearContext();
}

void SemanticValidationResultSink::capturePrimarySpanIfUnset(int line, int column) {
  diagnosticSink_.capturePrimarySpanIfUnset(line, column);
}

void SemanticValidationResultSink::addRelatedSpan(int line,
                                                  int column,
                                                  const std::string &label) {
  diagnosticSink_.addRelatedSpan(line, column, label);
}

bool SemanticValidationResultSink::fail(std::string message) {
  legacyError_ = std::move(message);
  return publishCurrentDiagnosticNow();
}

bool SemanticValidationResultSink::publishCurrentDiagnosticNow() {
  if (!shouldCollectStructuredDiagnostics() || legacyError_.empty()) {
    return false;
  }
  diagnosticSink_.setRecords({diagnosticSink_.makeRecord(legacyError_)});
  return false;
}

void SemanticValidationResultSink::moveCurrentDiagnosticTo(
    std::vector<DiagnosticSinkRecord> &out) {
  if (!shouldCollectStructuredDiagnostics() || legacyError_.empty()) {
    return;
  }
  out.push_back(diagnosticSink_.makeRecord(legacyError_));
  legacyError_.clear();
  clearContext();
}

void SemanticValidationResultSink::rememberFirstCollectedMessage(
    const std::string &message) {
  if (legacyError_.empty()) {
    legacyError_ = message;
  }
}

bool SemanticValidationResultSink::finalizeCollectedDiagnostics(
    std::vector<DiagnosticSinkRecord> &records) {
  if (!shouldCollectStructuredDiagnostics() || records.empty()) {
    return true;
  }
  const std::string firstMessage = records.front().message;
  diagnosticSink_.setRecords(std::move(records));
  rememberFirstCollectedMessage(firstMessage);
  return false;
}

DiagnosticSinkRecord SemanticValidationResultSink::makeRecord(
    std::string message) const {
  return diagnosticSink_.makeRecord(std::move(message));
}

void SemanticValidationResultSink::setRecords(
    std::vector<DiagnosticSinkRecord> records) {
  diagnosticSink_.setRecords(std::move(records));
}

} // namespace primec
