#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace primec {

enum class DiagnosticCode {
  ArgumentError,
  ImportError,
  TransformError,
  ParseError,
  UnsupportedDumpStage,
  SemanticError,
  LoweringError,
  IrSerializeError,
  EmitError,
  OutputError,
  RuntimeError,
};

struct DiagnosticSpan {
  std::string file;
  int line = 0;
  int column = 0;
  int endLine = 0;
  int endColumn = 0;
};

struct DiagnosticRelatedSpan {
  DiagnosticSpan span;
  std::string label;
};

struct DiagnosticSinkRecord {
  std::string message;
  DiagnosticSpan primarySpan;
  std::vector<DiagnosticRelatedSpan> relatedSpans;
  bool hasPrimarySpan = false;
};

struct DiagnosticSinkReport {
  std::string message;
  DiagnosticSpan primarySpan;
  std::vector<DiagnosticRelatedSpan> relatedSpans;
  bool hasPrimarySpan = false;
  std::vector<DiagnosticSinkRecord> records;
};

class DiagnosticSink {
public:
  explicit DiagnosticSink(DiagnosticSinkReport *report = nullptr) : report_(report) {}

  void reset();
  bool enabled() const { return report_ != nullptr; }

  void clearContext();
  void capturePrimarySpanIfUnset(const DiagnosticSpan &span);
  void capturePrimarySpanIfUnset(int line, int column, std::string_view file = {});
  void addRelatedSpan(const DiagnosticRelatedSpan &span);
  void addRelatedSpan(int line, int column, std::string label, std::string_view file = {});

  void setSummary(std::string message);
  DiagnosticSinkRecord makeRecord(std::string message) const;
  void setRecords(std::vector<DiagnosticSinkRecord> records);

private:
  DiagnosticSinkReport *report_ = nullptr;
};

struct DiagnosticRecord {
  std::string code;
  std::string message;
  std::string severity = "error";
  DiagnosticSpan primarySpan;
  std::vector<DiagnosticRelatedSpan> relatedSpans;
  std::vector<std::string> notes;
};

std::string diagnosticCodeString(DiagnosticCode code);

DiagnosticRecord makeDiagnosticRecord(DiagnosticCode code,
                                      const std::string &message,
                                      const std::string &inputPath,
                                      const std::vector<std::string> &notes = {},
                                      const DiagnosticSpan *primarySpan = nullptr,
                                      const std::vector<DiagnosticRelatedSpan> &relatedSpans = {});

std::string encodeDiagnosticsJson(const std::vector<DiagnosticRecord> &diagnostics);

std::string formatDiagnosticText(const DiagnosticRecord &diagnostic,
                                 const std::string &messagePrefix = {},
                                 const std::string *sourceText = nullptr);

std::string formatDiagnosticsText(const std::vector<DiagnosticRecord> &diagnostics,
                                  const std::string &messagePrefix = {},
                                  const std::string *sourceText = nullptr);

} // namespace primec
