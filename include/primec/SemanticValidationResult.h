#pragma once

#include "primec/Diagnostics.h"

#include <string>
#include <vector>

namespace primec {

class SemanticValidationResultSink {
public:
  SemanticValidationResultSink(std::string &legacyError,
                               DiagnosticSinkReport *diagnosticInfo,
                               bool collectDiagnostics);

  void reset();
  bool shouldCollectStructuredDiagnostics() const;
  bool hasFailure() const;
  const std::string &message() const;

  void clearContext();
  void capturePrimarySpanIfUnset(int line, int column);
  void addRelatedSpan(int line, int column, const std::string &label);

  bool fail(std::string message);
  bool publishCurrentDiagnosticNow();
  void moveCurrentDiagnosticTo(std::vector<DiagnosticSinkRecord> &out);
  void rememberFirstCollectedMessage(const std::string &message);
  bool finalizeCollectedDiagnostics(std::vector<DiagnosticSinkRecord> &records);

  DiagnosticSinkRecord makeRecord(std::string message) const;
  void setRecords(std::vector<DiagnosticSinkRecord> records);

private:
  std::string &legacyError_;
  DiagnosticSinkReport *diagnosticInfo_ = nullptr;
  DiagnosticSink diagnosticSink_;
  bool collectDiagnostics_ = false;
};

} // namespace primec
