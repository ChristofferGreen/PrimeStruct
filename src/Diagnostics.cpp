#include "primec/Diagnostics.h"

#include <cctype>
#include <sstream>

namespace primec {
namespace {

std::string jsonEscape(const std::string &text) {
  std::string out;
  out.reserve(text.size() + 8);
  for (unsigned char c : text) {
    switch (c) {
      case '"':
        out += "\\\"";
        break;
      case '\\':
        out += "\\\\";
        break;
      case '\b':
        out += "\\b";
        break;
      case '\f':
        out += "\\f";
        break;
      case '\n':
        out += "\\n";
        break;
      case '\r':
        out += "\\r";
        break;
      case '\t':
        out += "\\t";
        break;
      default:
        if (c < 0x20) {
          std::ostringstream hex;
          hex << "\\u00";
          const char *digits = "0123456789abcdef";
          hex << digits[(c >> 4) & 0x0F] << digits[c & 0x0F];
          out += hex.str();
        } else {
          out.push_back(static_cast<char>(c));
        }
        break;
    }
  }
  return out;
}

bool parseTrailingLocation(const std::string &text, int &lineOut, int &columnOut, std::string &messageOut) {
  const size_t markerPos = text.rfind(" at ");
  if (markerPos == std::string::npos) {
    messageOut = text;
    return false;
  }
  const size_t valuePos = markerPos + 4;
  if (valuePos >= text.size()) {
    messageOut = text;
    return false;
  }
  size_t colonPos = valuePos;
  while (colonPos < text.size() && std::isdigit(static_cast<unsigned char>(text[colonPos])) != 0) {
    ++colonPos;
  }
  if (colonPos == valuePos || colonPos >= text.size() || text[colonPos] != ':') {
    messageOut = text;
    return false;
  }
  const size_t columnPos = colonPos + 1;
  if (columnPos >= text.size()) {
    messageOut = text;
    return false;
  }
  size_t endPos = columnPos;
  while (endPos < text.size() && std::isdigit(static_cast<unsigned char>(text[endPos])) != 0) {
    ++endPos;
  }
  if (endPos != text.size() || endPos == columnPos) {
    messageOut = text;
    return false;
  }

  try {
    lineOut = std::stoi(text.substr(valuePos, colonPos - valuePos));
    columnOut = std::stoi(text.substr(columnPos, endPos - columnPos));
  } catch (...) {
    messageOut = text;
    return false;
  }

  messageOut = text.substr(0, markerPos);
  return true;
}

void appendSpanJson(std::ostringstream &out, const DiagnosticSpan &span) {
  out << "{\"file\":\"" << jsonEscape(span.file) << "\",\"line\":" << span.line << ",\"column\":" << span.column
      << ",\"end_line\":" << span.endLine << ",\"end_column\":" << span.endColumn << "}";
}

bool spanHasLocation(const DiagnosticSpan &span) {
  return span.line > 0 && span.column > 0;
}

std::string spanFileText(const DiagnosticSpan &span) {
  if (!span.file.empty()) {
    return span.file;
  }
  return "<input>";
}

bool lineTextAt(const std::string &sourceText, int lineNumber, std::string &lineOut) {
  if (lineNumber <= 0) {
    return false;
  }

  int currentLine = 1;
  size_t lineStart = 0;
  for (size_t i = 0; i <= sourceText.size(); ++i) {
    if (i < sourceText.size() && sourceText[i] != '\n') {
      continue;
    }
    if (currentLine == lineNumber) {
      size_t lineEnd = i;
      if (lineEnd > lineStart && sourceText[lineEnd - 1] == '\r') {
        --lineEnd;
      }
      lineOut = sourceText.substr(lineStart, lineEnd - lineStart);
      return true;
    }
    ++currentLine;
    lineStart = i + 1;
  }

  return false;
}

std::string caretMarkerForSpan(const DiagnosticSpan &span) {
  const int column = span.column > 0 ? span.column : 1;
  int endColumn = span.endColumn > 0 ? span.endColumn : column;
  if (endColumn < column) {
    endColumn = column;
  }
  const int squiggles = endColumn - column;

  std::string marker(static_cast<size_t>(column - 1), ' ');
  marker.push_back('^');
  marker.append(static_cast<size_t>(squiggles), '~');
  return marker;
}

void appendSpanSnippet(std::ostringstream &out, const DiagnosticSpan &span, const std::string *sourceText) {
  if (!spanHasLocation(span) || sourceText == nullptr) {
    return;
  }

  std::string lineText;
  if (!lineTextAt(*sourceText, span.line, lineText)) {
    return;
  }

  const std::string lineNumber = std::to_string(span.line);
  out << " " << lineNumber << " | " << lineText << "\n";
  out << " " << std::string(lineNumber.size(), ' ') << " | " << caretMarkerForSpan(span) << "\n";
}

} // namespace

std::string diagnosticCodeString(DiagnosticCode code) {
  switch (code) {
    case DiagnosticCode::ArgumentError:
      return "PSC0001";
    case DiagnosticCode::ImportError:
      return "PSC1001";
    case DiagnosticCode::TransformError:
      return "PSC1002";
    case DiagnosticCode::ParseError:
      return "PSC1003";
    case DiagnosticCode::UnsupportedDumpStage:
      return "PSC1004";
    case DiagnosticCode::SemanticError:
      return "PSC1005";
    case DiagnosticCode::LoweringError:
      return "PSC2001";
    case DiagnosticCode::IrSerializeError:
      return "PSC2002";
    case DiagnosticCode::EmitError:
      return "PSC2003";
    case DiagnosticCode::OutputError:
      return "PSC2004";
    case DiagnosticCode::RuntimeError:
      return "PSC3001";
    default:
      return "PSC9999";
  }
}

DiagnosticRecord makeDiagnosticRecord(DiagnosticCode code,
                                      const std::string &message,
                                      const std::string &inputPath,
                                      const std::vector<std::string> &notes,
                                      const DiagnosticSpan *primarySpan,
                                      const std::vector<DiagnosticRelatedSpan> &relatedSpans) {
  DiagnosticRecord record;
  record.code = diagnosticCodeString(code);
  record.notes = notes;
  record.relatedSpans = relatedSpans;

  if (primarySpan != nullptr) {
    record.primarySpan = *primarySpan;
    if (record.primarySpan.file.empty()) {
      record.primarySpan.file = inputPath;
    }
    record.message = message;
    if (record.message.empty()) {
      record.message = "unknown error";
    }
    return record;
  }

  record.primarySpan.file = inputPath;

  int line = 0;
  int column = 0;
  std::string normalizedMessage;
  if (parseTrailingLocation(message, line, column, normalizedMessage)) {
    record.primarySpan.line = line;
    record.primarySpan.column = column;
    record.primarySpan.endLine = line;
    record.primarySpan.endColumn = column;
  } else {
    record.primarySpan.line = 0;
    record.primarySpan.column = 0;
    record.primarySpan.endLine = 0;
    record.primarySpan.endColumn = 0;
  }
  record.message = normalizedMessage;
  if (record.message.empty()) {
    record.message = message;
  }
  if (record.message.empty()) {
    record.message = "unknown error";
  }
  return record;
}

std::string encodeDiagnosticsJson(const std::vector<DiagnosticRecord> &diagnostics) {
  std::ostringstream out;
  out << "{\"version\":1,\"diagnostics\":[";
  for (size_t i = 0; i < diagnostics.size(); ++i) {
    const DiagnosticRecord &diag = diagnostics[i];
    if (i != 0) {
      out << ",";
    }
    out << "{\"code\":\"" << jsonEscape(diag.code) << "\",\"severity\":\"" << jsonEscape(diag.severity)
        << "\",\"message\":\"" << jsonEscape(diag.message) << "\",\"primary_span\":";
    appendSpanJson(out, diag.primarySpan);
    out << ",\"related_spans\":[";
    for (size_t relIndex = 0; relIndex < diag.relatedSpans.size(); ++relIndex) {
      if (relIndex != 0) {
        out << ",";
      }
      out << "{\"span\":";
      appendSpanJson(out, diag.relatedSpans[relIndex].span);
      out << ",\"label\":\"" << jsonEscape(diag.relatedSpans[relIndex].label) << "\"}";
    }
    out << "],\"notes\":[";
    for (size_t noteIndex = 0; noteIndex < diag.notes.size(); ++noteIndex) {
      if (noteIndex != 0) {
        out << ",";
      }
      out << "\"" << jsonEscape(diag.notes[noteIndex]) << "\"";
    }
    out << "]}";
  }
  out << "]}";
  return out.str();
}

std::string formatDiagnosticText(const DiagnosticRecord &diagnostic,
                                 const std::string &messagePrefix,
                                 const std::string *sourceText) {
  std::ostringstream out;
  const std::string severity = diagnostic.severity.empty() ? "error" : diagnostic.severity;
  const std::string message = messagePrefix + diagnostic.message;

  if (spanHasLocation(diagnostic.primarySpan)) {
    out << spanFileText(diagnostic.primarySpan) << ":" << diagnostic.primarySpan.line << ":"
        << diagnostic.primarySpan.column << ": " << severity << ": " << message;
    if (!diagnostic.code.empty()) {
      out << " [" << diagnostic.code << "]";
    }
    out << "\n";
    appendSpanSnippet(out, diagnostic.primarySpan, sourceText);
  } else {
    out << message << "\n";
  }

  for (const auto &related : diagnostic.relatedSpans) {
    if (spanHasLocation(related.span)) {
      out << spanFileText(related.span) << ":" << related.span.line << ":" << related.span.column << ": note";
      if (!related.label.empty()) {
        out << ": " << related.label;
      }
      out << "\n";
      appendSpanSnippet(out, related.span, sourceText);
      continue;
    }
    if (!related.label.empty()) {
      out << "note: " << related.label << "\n";
    }
  }

  for (const auto &note : diagnostic.notes) {
    if (!note.empty()) {
      out << "note: " << note << "\n";
    }
  }

  return out.str();
}

std::string formatDiagnosticsText(const std::vector<DiagnosticRecord> &diagnostics,
                                  const std::string &messagePrefix,
                                  const std::string *sourceText) {
  std::ostringstream out;
  for (size_t i = 0; i < diagnostics.size(); ++i) {
    if (i != 0) {
      out << "\n";
    }
    out << formatDiagnosticText(diagnostics[i], messagePrefix, sourceText);
  }
  return out.str();
}

} // namespace primec
