#include "SemanticsValidator.h"

#include <algorithm>
#include <limits>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace primec::semantics {

void SemanticsValidator::capturePrimarySpanIfUnset(int line, int column) {
  diagnosticSink_.capturePrimarySpanIfUnset(line, column);
}

void SemanticsValidator::captureRelatedSpan(int line, int column, const std::string &label) {
  diagnosticSink_.addRelatedSpan(line, column, label);
}

void SemanticsValidator::captureExprContext(const Expr &expr) {
  capturePrimarySpanIfUnset(expr.sourceLine, expr.sourceColumn);
  if (currentDefinitionContext_ != nullptr) {
    captureRelatedSpan(currentDefinitionContext_->sourceLine,
                       currentDefinitionContext_->sourceColumn,
                       "definition: " + currentDefinitionContext_->fullPath);
  }
  if (currentExecutionContext_ != nullptr) {
    captureRelatedSpan(currentExecutionContext_->sourceLine,
                       currentExecutionContext_->sourceColumn,
                       "execution: " + currentExecutionContext_->fullPath);
  }
}

void SemanticsValidator::captureDefinitionContext(const Definition &def) {
  capturePrimarySpanIfUnset(def.sourceLine, def.sourceColumn);
  captureRelatedSpan(def.sourceLine, def.sourceColumn, "definition: " + def.fullPath);
}

void SemanticsValidator::captureExecutionContext(const Execution &exec) {
  capturePrimarySpanIfUnset(exec.sourceLine, exec.sourceColumn);
  captureRelatedSpan(exec.sourceLine, exec.sourceColumn, "execution: " + exec.fullPath);
}

bool SemanticsValidator::collectDuplicateDefinitionDiagnostics() {
  std::unordered_map<std::string, std::vector<const Definition *>> definitionsByPath;
  definitionsByPath.reserve(program_.definitions.size());
  for (const auto &def : program_.definitions) {
    definitionsByPath[def.fullPath].push_back(&def);
  }

  struct DuplicateGroup {
    std::string path;
    std::vector<const Definition *> definitions;
  };
  std::vector<DuplicateGroup> duplicateGroups;
  duplicateGroups.reserve(definitionsByPath.size());
  for (auto &entry : definitionsByPath) {
    if (entry.second.size() <= 1) {
      continue;
    }
    auto &defs = entry.second;
    std::stable_sort(defs.begin(), defs.end(), [](const Definition *left, const Definition *right) {
      const int leftLine = left->sourceLine > 0 ? left->sourceLine : std::numeric_limits<int>::max();
      const int rightLine = right->sourceLine > 0 ? right->sourceLine : std::numeric_limits<int>::max();
      if (leftLine != rightLine) {
        return leftLine < rightLine;
      }
      const int leftColumn = left->sourceColumn > 0 ? left->sourceColumn : std::numeric_limits<int>::max();
      const int rightColumn = right->sourceColumn > 0 ? right->sourceColumn : std::numeric_limits<int>::max();
      if (leftColumn != rightColumn) {
        return leftColumn < rightColumn;
      }
      return left->fullPath < right->fullPath;
    });
    duplicateGroups.push_back(DuplicateGroup{entry.first, defs});
  }
  if (duplicateGroups.empty()) {
    return true;
  }

  std::stable_sort(duplicateGroups.begin(),
                   duplicateGroups.end(),
                   [](const DuplicateGroup &left, const DuplicateGroup &right) {
                     const Definition *leftDef = left.definitions.front();
                     const Definition *rightDef = right.definitions.front();
                     const int leftLine = leftDef->sourceLine > 0 ? leftDef->sourceLine
                                                                  : std::numeric_limits<int>::max();
                     const int rightLine = rightDef->sourceLine > 0 ? rightDef->sourceLine
                                                                    : std::numeric_limits<int>::max();
                     if (leftLine != rightLine) {
                       return leftLine < rightLine;
                     }
                     const int leftColumn = leftDef->sourceColumn > 0 ? leftDef->sourceColumn
                                                                      : std::numeric_limits<int>::max();
                     const int rightColumn = rightDef->sourceColumn > 0 ? rightDef->sourceColumn
                                                                        : std::numeric_limits<int>::max();
                     if (leftColumn != rightColumn) {
                       return leftColumn < rightColumn;
                     }
                     return left.path < right.path;
                   });

  if (diagnosticInfo_ != nullptr) {
    std::vector<DiagnosticSinkRecord> records;
    records.reserve(duplicateGroups.size());
    for (const auto &group : duplicateGroups) {
      DiagnosticSinkRecord record;
      record.message = "duplicate definition: " + group.path;
      const Definition *primary = group.definitions.front();
      if (primary->sourceLine > 0 && primary->sourceColumn > 0) {
        record.primarySpan.line = primary->sourceLine;
        record.primarySpan.column = primary->sourceColumn;
        record.primarySpan.endLine = primary->sourceLine;
        record.primarySpan.endColumn = primary->sourceColumn;
        record.hasPrimarySpan = true;
      }
      for (const Definition *def : group.definitions) {
        if (def->sourceLine <= 0 || def->sourceColumn <= 0) {
          continue;
        }
        const std::string label = "definition: " + def->fullPath;
        bool duplicateSpan = false;
        for (const auto &existing : record.relatedSpans) {
          if (existing.span.line == def->sourceLine && existing.span.column == def->sourceColumn &&
              existing.label == label) {
            duplicateSpan = true;
            break;
          }
        }
        if (duplicateSpan) {
          continue;
        }
        DiagnosticRelatedSpan span;
        span.span.line = def->sourceLine;
        span.span.column = def->sourceColumn;
        span.span.endLine = def->sourceLine;
        span.span.endColumn = def->sourceColumn;
        span.label = label;
        record.relatedSpans.push_back(std::move(span));
      }
      records.push_back(std::move(record));
    }
    diagnosticSink_.setRecords(std::move(records));
  }

  error_ = "duplicate definition: " + duplicateGroups.front().path;
  return false;
}

bool SemanticsValidator::shouldCollectStructuredDiagnostics() const {
  return collectDiagnostics_ && diagnosticInfo_ != nullptr;
}

void SemanticsValidator::clearStructuredDiagnosticContext() {
  if (!shouldCollectStructuredDiagnostics()) {
    return;
  }
  diagnosticSink_.clearContext();
}

void SemanticsValidator::moveCurrentStructuredDiagnosticTo(std::vector<SemanticDiagnosticRecord> &out) {
  if (!shouldCollectStructuredDiagnostics() || error_.empty()) {
    return;
  }
  out.push_back(diagnosticSink_.makeRecord(error_));
  error_.clear();
  clearStructuredDiagnosticContext();
}

bool SemanticsValidator::finalizeCollectedStructuredDiagnostics(
    std::vector<SemanticDiagnosticRecord> &records) {
  if (!shouldCollectStructuredDiagnostics() || records.empty()) {
    return true;
  }
  diagnosticSink_.setRecords(std::move(records));
  error_ = diagnosticInfo_->records.front().message;
  return false;
}

} // namespace primec::semantics
