#include "SemanticsValidator.h"

#include <iterator>
#include <optional>
#include <utility>
#include <vector>

namespace primec::semantics {

bool SemanticsValidator::validateDefinitions() {
  std::vector<SemanticDiagnosticRecord> collectedRecords;
  const bool collectDiagnostics = shouldCollectStructuredDiagnostics();
  auto validateDefinition = [&](const Definition &def) -> bool {
    DefinitionContextScope definitionScope(*this, def);
    ValidationContextScope validationContextScope(*this, buildDefinitionValidationContext(def));
    auto isStructDefinition = [&](const Definition &candidate) {
      for (const auto &transform : candidate.transforms) {
        if (isStructTransformName(transform.name)) {
          return true;
        }
      }
      return false;
    };
    if (!validateCapabilitiesSubset(def.transforms, def.fullPath)) {
      if (error_.empty()) {
        error_ = "validateCapabilitiesSubset failed on " + def.fullPath;
      }
      return false;
    }
    std::unordered_map<std::string, BindingInfo> locals;
    const auto &defParams = paramsByDef_[def.fullPath];
    for (const auto &param : defParams) {
      locals.emplace(param.name, param.binding);
    }
    for (const auto &param : defParams) {
      if (param.defaultExpr == nullptr) {
        continue;
      }
      if (!validateExpr(defParams, locals, *param.defaultExpr)) {
        if (error_.empty()) {
          error_ = "default expression validation failed on " + def.fullPath;
        }
        return false;
      }
    }
    ReturnKind kind = ReturnKind::Unknown;
    auto kindIt = returnKinds_.find(def.fullPath);
    if (kindIt != returnKinds_.end()) {
      kind = kindIt->second;
    }
    if (isLifecycleHelperName(def.fullPath) && kind != ReturnKind::Void) {
      error_ = "lifecycle helpers must return void: " + def.fullPath;
      return false;
    }
    const std::optional<OnErrorHandler> &onErrorHandler = currentValidationContext_.onError;
    if (onErrorHandler.has_value() &&
        (!currentValidationContext_.resultType.has_value() ||
         !currentValidationContext_.resultType->isResult) &&
        kind != ReturnKind::Int) {
      error_ = "on_error requires Result or int return type on " + def.fullPath;
      return false;
    }
    if (onErrorHandler.has_value() &&
        currentValidationContext_.resultType.has_value() &&
        currentValidationContext_.resultType->isResult &&
        !errorTypesMatch(onErrorHandler->errorType,
                         currentValidationContext_.resultType->errorType,
                         def.namespacePrefix)) {
      error_ = "on_error error type mismatch on " + def.fullPath;
      return false;
    }
    if (onErrorHandler.has_value()) {
      OnErrorScope onErrorScope(*this, std::nullopt);
      for (const auto &arg : currentValidationContext_.onError->boundArgs) {
        if (!validateExpr(defParams, locals, arg)) {
          if (error_.empty()) {
            error_ = "on_error bound-arg validation failed on " + def.fullPath;
          }
          return false;
        }
      }
    }
    OnErrorScope onErrorScope(*this, onErrorHandler);
    bool sawReturn = false;
    for (size_t stmtIndex = 0; stmtIndex < def.statements.size(); ++stmtIndex) {
      const Expr &stmt = def.statements[stmtIndex];
      if (!validateStatement(defParams,
                             locals,
                             stmt,
                             kind,
                             true,
                             true,
                             &sawReturn,
                             def.namespacePrefix,
                             &def.statements,
                             stmtIndex)) {
        if (error_.empty()) {
          error_ = "statement validation failed on " + def.fullPath;
        }
        return false;
      }
      expireReferenceBorrowsForRemainder(defParams, locals, def.statements, stmtIndex + 1);
    }
    if (def.returnExpr.has_value()) {
      if (!validateExpr(defParams, locals, *def.returnExpr)) {
        if (error_.empty()) {
          error_ = "return expression validation failed on " + def.fullPath;
        }
        return false;
      }
      sawReturn = true;
    }
    if (kind != ReturnKind::Void && !isStructDefinition(def)) {
      bool allPathsReturn = def.returnExpr.has_value() || blockAlwaysReturns(def.statements);
      if (!allPathsReturn) {
        if (sawReturn) {
          error_ = "not all control paths return in " + def.fullPath + " (missing return statement)";
        } else {
          error_ = "missing return statement in " + def.fullPath;
        }
        return false;
      }
    }
    bool shouldCheckUninitialized = (kind == ReturnKind::Void);
    if (kind != ReturnKind::Void && !isStructDefinition(def)) {
      shouldCheckUninitialized = def.returnExpr.has_value() || blockAlwaysReturns(def.statements);
    }
    if (shouldCheckUninitialized) {
      std::optional<std::string> uninitError =
          validateUninitializedDefiniteState(defParams, def.statements);
      if (uninitError.has_value()) {
        error_ = *uninitError;
        return false;
      }
    }
    return true;
  };

  for (const auto &def : program_.definitions) {
    clearStructuredDiagnosticContext();
    if (collectDiagnostics) {
      ValidationContextScope validationContextScope(*this, buildDefinitionValidationContext(def));
      std::vector<SemanticDiagnosticRecord> intraDefinitionRecords;
      collectDefinitionIntraBodyCallDiagnostics(def, intraDefinitionRecords);
      if (!intraDefinitionRecords.empty()) {
        if (error_.empty()) {
          error_ = intraDefinitionRecords.front().message;
        }
        collectedRecords.insert(collectedRecords.end(),
                                std::make_move_iterator(intraDefinitionRecords.begin()),
                                std::make_move_iterator(intraDefinitionRecords.end()));
        continue;
      }
    }
    if (!validateDefinition(def)) {
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
