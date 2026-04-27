#include "SemanticsValidator.h"

#include "TypeResolutionGraph.h"

#include <algorithm>
#include <limits>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace primec::semantics {

namespace {

int diagnosticOrderCoordinate(int value) {
  return value > 0 ? value : std::numeric_limits<int>::max();
}

std::string diagnosticOwningPath(const std::string &path) {
  const size_t lastSlash = path.find_last_of('/');
  if (lastSlash == std::string::npos || lastSlash == 0) {
    return {};
  }
  return path.substr(0, lastSlash);
}

bool isExplicitStdNamespacedVectorHelperCall(const Expr &expr,
                                             const char *helperName) {
  const std::string absolutePath =
      "/std/collections/vector/" + std::string(helperName);
  const std::string relativePath =
      "std/collections/vector/" + std::string(helperName);
  if (expr.name == absolutePath || expr.name == relativePath) {
    return true;
  }
  std::string normalizedNamespace = expr.namespacePrefix;
  if (!normalizedNamespace.empty() && normalizedNamespace.front() == '/') {
    normalizedNamespace.erase(normalizedNamespace.begin());
  }
  return normalizedNamespace == "std/collections/vector" &&
         expr.name == helperName;
}

} // namespace

void SemanticsValidator::capturePrimarySpanIfUnset(int line, int column) {
  resultSink_.capturePrimarySpanIfUnset(line, column);
}

void SemanticsValidator::captureRelatedSpan(int line, int column, const std::string &label) {
  resultSink_.addRelatedSpan(line, column, label);
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

bool SemanticsValidator::failExprDiagnostic(const Expr &expr,
                                            std::string message) {
  if (message.rfind("unknown call target: /std/collections/vector/count", 0) == 0 &&
      expr.args.size() != 1 &&
      !isExplicitStdNamespacedVectorHelperCall(expr, "count")) {
    message = hasNamedArguments(expr.argNames)
                  ? "named arguments not supported for builtin calls"
                  : "argument count mismatch for builtin count";
  } else if (message.rfind("unknown call target: /std/collections/vector/capacity", 0) == 0 &&
             expr.args.size() != 1 &&
             !isExplicitStdNamespacedVectorHelperCall(expr, "capacity")) {
    message = hasNamedArguments(expr.argNames)
                  ? "named arguments not supported for builtin calls"
                  : "argument count mismatch for builtin capacity";
  }
  captureExprContext(expr);
  return resultSink_.fail(std::move(message));
}

bool SemanticsValidator::failDefinitionDiagnostic(
    const Definition &def,
    std::string message) {
  captureDefinitionContext(def);
  return resultSink_.fail(std::move(message));
}

bool SemanticsValidator::failExecutionDiagnostic(const Execution &exec,
                                                 std::string message) {
  captureExecutionContext(exec);
  return resultSink_.fail(std::move(message));
}

bool SemanticsValidator::definitionDiagnosticOrderLess(
    const Definition *left,
    const Definition *right) {
  const std::string leftOwningPath = diagnosticOwningPath(left->fullPath);
  const std::string rightOwningPath = diagnosticOwningPath(right->fullPath);
  if (leftOwningPath != rightOwningPath) {
    return leftOwningPath < rightOwningPath;
  }
  const int leftLine = diagnosticOrderCoordinate(left->sourceLine);
  const int rightLine = diagnosticOrderCoordinate(right->sourceLine);
  if (leftLine != rightLine) {
    return leftLine < rightLine;
  }
  const int leftColumn = diagnosticOrderCoordinate(left->sourceColumn);
  const int rightColumn = diagnosticOrderCoordinate(right->sourceColumn);
  if (leftColumn != rightColumn) {
    return leftColumn < rightColumn;
  }
  return left->fullPath < right->fullPath;
}

void SemanticsValidator::sortDefinitionsForDiagnosticOrder(
    std::vector<const Definition *> &definitions) {
  std::stable_sort(definitions.begin(), definitions.end(), [](const Definition *left,
                                                              const Definition *right) {
    return definitionDiagnosticOrderLess(left, right);
  });
}

bool SemanticsValidator::typeResolutionNodeDiagnosticOrderLess(
    const TypeResolutionGraphNode &left,
    const TypeResolutionGraphNode &right) {
  if (left.scopePath != right.scopePath) {
    return left.scopePath < right.scopePath;
  }
  const int leftLine = diagnosticOrderCoordinate(left.sourceLine);
  const int rightLine = diagnosticOrderCoordinate(right.sourceLine);
  if (leftLine != rightLine) {
    return leftLine < rightLine;
  }
  const int leftColumn = diagnosticOrderCoordinate(left.sourceColumn);
  const int rightColumn = diagnosticOrderCoordinate(right.sourceColumn);
  if (leftColumn != rightColumn) {
    return leftColumn < rightColumn;
  }
  if (left.resolvedPath != right.resolvedPath) {
    return left.resolvedPath < right.resolvedPath;
  }
  if (left.kind != right.kind) {
    return static_cast<int>(left.kind) < static_cast<int>(right.kind);
  }
  if (left.id != right.id) {
    return left.id < right.id;
  }
  return left.label < right.label;
}

void SemanticsValidator::sortTypeResolutionNodesForDiagnosticOrder(
    std::vector<const TypeResolutionGraphNode *> &nodes) {
  std::stable_sort(nodes.begin(),
                   nodes.end(),
                   [](const TypeResolutionGraphNode *left,
                      const TypeResolutionGraphNode *right) {
                     return typeResolutionNodeDiagnosticOrderLess(*left, *right);
                   });
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
    sortDefinitionsForDiagnosticOrder(defs);
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
                     if (definitionDiagnosticOrderLess(leftDef, rightDef)) {
                       return true;
                     }
                     if (definitionDiagnosticOrderLess(rightDef, leftDef)) {
                       return false;
                     }
                     return left.path < right.path;
                   });

  if (shouldCollectStructuredDiagnostics()) {
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
    resultSink_.setRecords(std::move(records));
  }

  return failUncontextualizedDiagnostic("duplicate definition: " +
                                        duplicateGroups.front().path);
}

bool SemanticsValidator::shouldCollectStructuredDiagnostics() const {
  return resultSink_.shouldCollectStructuredDiagnostics();
}

void SemanticsValidator::clearStructuredDiagnosticContext() {
  resultSink_.clearContext();
}

void SemanticsValidator::moveCurrentStructuredDiagnosticTo(std::vector<SemanticDiagnosticRecord> &out) {
  resultSink_.moveCurrentDiagnosticTo(out);
}

void SemanticsValidator::rememberFirstCollectedDiagnosticMessage(
    const std::string &message) {
  resultSink_.rememberFirstCollectedMessage(message);
}

bool SemanticsValidator::failUncontextualizedDiagnostic(std::string message) {
  return resultSink_.fail(std::move(message));
}

bool SemanticsValidator::publishCurrentStructuredDiagnosticNow() {
  return resultSink_.publishCurrentDiagnosticNow();
}

bool SemanticsValidator::finalizeCollectedStructuredDiagnostics(
    std::vector<SemanticDiagnosticRecord> &records) {
  return resultSink_.finalizeCollectedDiagnostics(records);
}

} // namespace primec::semantics
