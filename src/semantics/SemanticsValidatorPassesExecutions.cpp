#include "SemanticsValidator.h"

#include <utility>
#include <vector>

namespace primec::semantics {

bool SemanticsValidator::validateExecutions() {
  std::vector<SemanticDiagnosticRecord> collectedRecords;
  const bool collectDiagnostics = collectDiagnostics_ && diagnosticInfo_ != nullptr;
  auto resetCollectedState = [&]() {
    if (!collectDiagnostics) {
      return;
    }
    diagnosticSink_.clearContext();
  };
  auto pushCollectedRecord = [&]() {
    if (!collectDiagnostics || error_.empty()) {
      return;
    }
    collectedRecords.push_back(diagnosticSink_.makeRecord(error_));
    error_.clear();
    resetCollectedState();
  };
  auto validateExecution = [&](const Execution &exec) -> bool {
    ExecutionContextScope executionScope(*this, exec);
    ValidationContextScope validationContextScope(*this, buildExecutionValidationContext(exec));
    bool sawEffects = false;
    bool sawCapabilities = false;
    for (const auto &transform : exec.transforms) {
      if (transform.name == "return") {
        error_ = "return transform not allowed on executions: " + exec.fullPath;
        return false;
      }
      if (transform.name == "mut") {
        error_ = "mut transform is not allowed on executions: " + exec.fullPath;
        return false;
      }
      if (transform.name == "unsafe") {
        error_ = "unsafe transform is not allowed on executions: " + exec.fullPath;
        return false;
      }
      if (transform.name == "no_padding" || transform.name == "platform_independent_padding") {
        error_ = "layout transforms are not supported on executions: " + exec.fullPath;
        return false;
      }
      if (isBindingQualifierName(transform.name)) {
        error_ = "binding visibility/static transforms are only valid on bindings: " + exec.fullPath;
        return false;
      }
      if (transform.name == "copy") {
        error_ = "copy transform is not allowed on executions: " + exec.fullPath;
        return false;
      }
      if (transform.name == "restrict") {
        error_ = "restrict transform is not allowed on executions: " + exec.fullPath;
        return false;
      }
      if (transform.name == "stack" || transform.name == "heap" || transform.name == "buffer") {
        error_ = "placement transforms are not supported: " + exec.fullPath;
        return false;
      }
      if (transform.name == "effects") {
        if (sawEffects) {
          error_ = "duplicate effects transform on " + exec.fullPath;
          return false;
        }
        sawEffects = true;
        if (!validateEffectsTransform(transform, exec.fullPath, error_)) {
          return false;
        }
      } else if (transform.name == "capabilities") {
        if (sawCapabilities) {
          error_ = "duplicate capabilities transform on " + exec.fullPath;
          return false;
        }
        sawCapabilities = true;
        if (!validateCapabilitiesTransform(transform, exec.fullPath, error_)) {
          return false;
        }
      } else if (transform.name == "align_bytes" || transform.name == "align_kbytes") {
        error_ = "alignment transforms are not supported on executions: " + exec.fullPath;
        return false;
      } else if (isReflectionTransformName(transform.name)) {
        error_ = "reflection transforms are only valid on struct definitions: " + exec.fullPath;
        return false;
      } else if (isStructTransformName(transform.name)) {
        error_ = "struct transforms are not allowed on executions: " + exec.fullPath;
        return false;
      }
    }
    if (!validateCapabilitiesSubset(exec.transforms, exec.fullPath)) {
      return false;
    }
    Expr execTarget;
    if (!exec.name.empty()) {
      execTarget.name = exec.name;
      execTarget.namespacePrefix = exec.namespacePrefix;
    } else {
      execTarget.name = exec.fullPath;
    }
    const std::string resolvedPath = resolveCalleePath(execTarget);
    auto it = defMap_.find(resolvedPath);
    if (it == defMap_.end()) {
      error_ = "unknown execution target: " + resolvedPath;
      return false;
    }
    const std::unordered_set<std::string> targetEffects =
        resolveEffects(it->second->transforms, it->second->fullPath == entryPath_);
    for (const auto &effect : currentValidationContext_.activeEffects) {
      if (targetEffects.count(effect) == 0) {
        error_ = "execution effects must be a subset of enclosing effects on " + resolvedPath + ": " + effect;
        return false;
      }
    }
    if (!validateNamedArguments(exec.arguments, exec.argumentNames, resolvedPath, error_)) {
      return false;
    }
    const auto &execParams = paramsByDef_[resolvedPath];
    if (!validateNamedArgumentsAgainstParams(execParams, exec.argumentNames, error_)) {
      return false;
    }
    Expr execCall;
    execCall.kind = Expr::Kind::Call;
    if (!exec.name.empty()) {
      execCall.name = exec.name;
      execCall.namespacePrefix = exec.namespacePrefix;
    } else {
      execCall.name = resolvedPath;
      execCall.namespacePrefix.clear();
    }
    execCall.templateArgs = exec.templateArgs;
    execCall.args = exec.arguments;
    execCall.argNames = exec.argumentNames;
    execCall.sourceLine = exec.sourceLine;
    execCall.sourceColumn = exec.sourceColumn;
    if (!validateExpr({}, std::unordered_map<std::string, BindingInfo>{}, execCall)) {
      return false;
    }
    std::unordered_map<std::string, BindingInfo> execLocals;
    for (size_t bodyIndex = 0; bodyIndex < exec.bodyArguments.size(); ++bodyIndex) {
      const Expr &arg = exec.bodyArguments[bodyIndex];
      if (!validateStatement({},
                             execLocals,
                             arg,
                             ReturnKind::Unknown,
                             false,
                             true,
                             nullptr,
                             exec.namespacePrefix,
                             &exec.bodyArguments,
                             bodyIndex)) {
        return false;
      }
      expireReferenceBorrowsForRemainder({}, execLocals, exec.bodyArguments, bodyIndex + 1);
    }
    return true;
  };

  for (const auto &exec : program_.executions) {
    resetCollectedState();
    if (collectDiagnostics) {
      ValidationContextScope validationContextScope(*this, buildExecutionValidationContext(exec));
      std::vector<SemanticDiagnosticRecord> intraExecutionRecords;
      collectExecutionIntraBodyCallDiagnostics(exec, intraExecutionRecords);
      if (!intraExecutionRecords.empty()) {
        if (error_.empty()) {
          error_ = intraExecutionRecords.front().message;
        }
        collectedRecords.insert(collectedRecords.end(),
                                std::make_move_iterator(intraExecutionRecords.begin()),
                                std::make_move_iterator(intraExecutionRecords.end()));
        continue;
      }
    }
    if (!validateExecution(exec)) {
      if (!collectDiagnostics) {
        return false;
      }
      pushCollectedRecord();
    }
  }

  if (collectDiagnostics && !collectedRecords.empty()) {
    diagnosticSink_.setRecords(std::move(collectedRecords));
    error_ = diagnosticInfo_->records.front().message;
    return false;
  }

  return true;
}

}  // namespace primec::semantics
