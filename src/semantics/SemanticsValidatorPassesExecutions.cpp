#include "SemanticsValidator.h"

#include <utility>
#include <vector>

namespace primec::semantics {

bool SemanticsValidator::validateExecutions() {
  std::vector<SemanticDiagnosticRecord> collectedRecords;
  const bool collectDiagnostics = shouldCollectStructuredDiagnostics();
  auto validateExecution = [&](const Execution &exec) -> bool {
    auto failPassesExecutionsDiagnostic = [&](std::string message) -> bool {
      return failExecutionDiagnostic(exec, std::move(message));
    };
    ExecutionContextScope executionScope(*this, exec);
    ValidationStateScope validationContextScope(*this, buildExecutionValidationState(exec));
    bool sawEffects = false;
    bool sawCapabilities = false;
    for (const auto &transform : exec.transforms) {
      if (transform.name == "return") {
        return failPassesExecutionsDiagnostic("return transform not allowed on executions: " + exec.fullPath);
      }
      if (transform.name == "mut") {
        return failPassesExecutionsDiagnostic("mut transform is not allowed on executions: " + exec.fullPath);
      }
      if (transform.name == "unsafe") {
        return failPassesExecutionsDiagnostic("unsafe transform is not allowed on executions: " + exec.fullPath);
      }
      if (transform.name == "no_padding" || transform.name == "platform_independent_padding") {
        return failPassesExecutionsDiagnostic("layout transforms are not supported on executions: " + exec.fullPath);
      }
      if (isBindingQualifierName(transform.name)) {
        return failPassesExecutionsDiagnostic("binding visibility/static transforms are only valid on bindings: " +
                                              exec.fullPath);
      }
      if (transform.name == "copy") {
        return failPassesExecutionsDiagnostic("copy transform is not allowed on executions: " + exec.fullPath);
      }
      if (transform.name == "restrict") {
        return failPassesExecutionsDiagnostic("restrict transform is not allowed on executions: " + exec.fullPath);
      }
      if (transform.name == "stack" || transform.name == "heap" || transform.name == "buffer") {
        return failPassesExecutionsDiagnostic("placement transforms are not supported: " + exec.fullPath);
      }
      if (transform.name == "effects") {
        if (sawEffects) {
          return failPassesExecutionsDiagnostic("duplicate effects transform on " + exec.fullPath);
        }
        sawEffects = true;
        if (!validateEffectsTransform(transform, exec.fullPath, error_)) {
          return failExecutionDiagnostic(exec, error_);
        }
      } else if (transform.name == "capabilities") {
        if (sawCapabilities) {
          return failPassesExecutionsDiagnostic("duplicate capabilities transform on " + exec.fullPath);
        }
        sawCapabilities = true;
        if (!validateCapabilitiesTransform(transform, exec.fullPath, error_)) {
          return failExecutionDiagnostic(exec, error_);
        }
      } else if (transform.name == "align_bytes" || transform.name == "align_kbytes") {
        return failPassesExecutionsDiagnostic("alignment transforms are not supported on executions: " + exec.fullPath);
      } else if (isReflectionTransformName(transform.name)) {
        return failPassesExecutionsDiagnostic("reflection transforms are only valid on struct definitions: " +
                                              exec.fullPath);
      } else if (isStructTransformName(transform.name)) {
        return failPassesExecutionsDiagnostic("struct transforms are not allowed on executions: " + exec.fullPath);
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
      return failPassesExecutionsDiagnostic("unknown execution target: " + resolvedPath);
    }
    const std::unordered_set<std::string> targetEffects =
        resolveEffects(it->second->transforms, it->second->fullPath == entryPath_);
    for (const auto &effect : currentValidationState_.context.activeEffects) {
      if (targetEffects.count(effect) == 0) {
        return failPassesExecutionsDiagnostic("execution effects must be a subset of enclosing effects on " +
                                              resolvedPath + ": " + effect);
      }
    }
    if (!validateNamedArguments(exec.arguments, exec.argumentNames, resolvedPath, error_)) {
      return failExecutionDiagnostic(exec, error_);
    }
    const auto &execParams = paramsByDef_[resolvedPath];
    if (!validateNamedArgumentsAgainstParams(execParams, exec.argumentNames, error_)) {
      return failExecutionDiagnostic(exec, error_);
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
    observeLocalMapSize(execLocals.size());
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
      observeLocalMapSize(execLocals.size());
      expireReferenceBorrowsForRemainder({}, execLocals, exec.bodyArguments, bodyIndex + 1);
    }
    return true;
  };

  for (const auto &exec : program_.executions) {
    clearStructuredDiagnosticContext();
    if (collectDiagnostics) {
      ValidationStateScope validationContextScope(*this, buildExecutionValidationState(exec));
      std::vector<SemanticDiagnosticRecord> intraExecutionRecords;
      collectExecutionIntraBodyCallDiagnostics(exec, intraExecutionRecords);
      if (!intraExecutionRecords.empty()) {
        rememberFirstCollectedDiagnosticMessage(intraExecutionRecords.front().message);
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
      moveCurrentStructuredDiagnosticTo(collectedRecords);
    }
  }

  if (!finalizeCollectedStructuredDiagnostics(collectedRecords)) {
    return false;
  }

  return true;
}

}  // namespace primec::semantics
