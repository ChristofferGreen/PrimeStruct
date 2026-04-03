#include "SemanticsValidator.h"

#include <functional>
#include <unordered_set>
#include <utility>
#include <vector>

namespace primec::semantics {

bool SemanticsValidator::publishPassesOmittedInitializersDiagnostic(const Expr *expr) {
  if (expr != nullptr) {
    captureExprContext(*expr);
  } else if (currentDefinitionContext_ != nullptr) {
    captureDefinitionContext(*currentDefinitionContext_);
  }
  return publishCurrentStructuredDiagnosticNow();
}

bool SemanticsValidator::validateOmittedBindingInitializer(const Expr &binding,
                                                           const BindingInfo &info,
                                                           const std::string &namespacePrefix) {
  if (!hasExplicitBindingTypeTransform(binding)) {
    error_ = "omitted initializer requires explicit struct type: " + binding.name;
    return publishPassesOmittedInitializersDiagnostic(&binding);
  }
  const std::string normalizedType = normalizeBindingTypeName(info.typeName);
  if (normalizedType == "vector" || normalizedType == "soa_vector") {
    std::vector<std::string> args;
    if (!splitTopLevelTemplateArgs(info.typeTemplateArg, args) || args.size() != 1) {
      error_ = normalizedType + " requires exactly one template argument";
      return publishPassesOmittedInitializersDiagnostic(&binding);
    }
    return true;
  }
  if (!info.typeTemplateArg.empty()) {
    error_ = "omitted initializer requires struct type: " + info.typeName;
    return publishPassesOmittedInitializersDiagnostic(&binding);
  }
  std::string structPath = resolveStructTypePath(info.typeName, namespacePrefix, structNames_);
  if (structPath.empty()) {
    auto importIt = importAliases_.find(info.typeName);
    if (importIt != importAliases_.end() && structNames_.count(importIt->second) > 0) {
      structPath = importIt->second;
    }
  }
  if (structPath.empty()) {
    error_ = "omitted initializer requires struct type: " + info.typeName;
    return publishPassesOmittedInitializersDiagnostic(&binding);
  }
  if (!hasStructZeroArgConstructor(structPath)) {
    error_ = "omitted initializer requires zero-arg constructor: " + structPath;
    return publishPassesOmittedInitializersDiagnostic(&binding);
  }
  if (!isOutsideEffectFreeStructConstructor(structPath)) {
    error_ = "omitted initializer requires effect-free zero-arg constructor: " + structPath;
    return publishPassesOmittedInitializersDiagnostic(&binding);
  }
  return true;
}

bool SemanticsValidator::hasStructZeroArgConstructor(const std::string &structPath) const {
  auto defIt = defMap_.find(structPath);
  if (defIt == defMap_.end() || !defIt->second) {
    return false;
  }
  auto isStaticField = [](const Expr &stmt) -> bool {
    for (const auto &transform : stmt.transforms) {
      if (transform.name == "static") {
        return true;
      }
    }
    return false;
  };
  const Definition &def = *defIt->second;
  std::unordered_set<std::string> missingFields;
  for (const auto &stmt : def.statements) {
    if (!stmt.isBinding || isStaticField(stmt)) {
      continue;
    }
    if (stmt.args.empty()) {
      missingFields.insert(stmt.name);
    }
  }
  if (missingFields.empty()) {
    return true;
  }
  const std::string createPath = structPath + "/Create";
  auto createIt = defMap_.find(createPath);
  if (createIt == defMap_.end() || !createIt->second) {
    return false;
  }
  const Definition &createDef = *createIt->second;
  std::string thisName = "this";
  auto paramsIt = paramsByDef_.find(createDef.fullPath);
  if (paramsIt != paramsByDef_.end() && !paramsIt->second.empty() && paramsIt->second.front().name == "this") {
    thisName = paramsIt->second.front().name;
  }

  struct FieldInitFlow {
    bool ok = true;
    bool fallsThrough = true;
    std::unordered_set<std::string> assigned;
  };

  auto allAssigned = [&](const std::unordered_set<std::string> &assigned) -> bool {
    for (const auto &field : missingFields) {
      if (assigned.count(field) == 0) {
        return false;
      }
    }
    return true;
  };
  auto intersectAssigned = [&](const std::unordered_set<std::string> &left,
                               const std::unordered_set<std::string> &right) -> std::unordered_set<std::string> {
    std::unordered_set<std::string> result;
    if (left.size() < right.size()) {
      for (const auto &field : left) {
        if (right.count(field) > 0) {
          result.insert(field);
        }
      }
    } else {
      for (const auto &field : right) {
        if (left.count(field) > 0) {
          result.insert(field);
        }
      }
    }
    return result;
  };
  auto isThisFieldTarget = [&](const Expr &target) -> bool {
    if (thisName.empty()) {
      return false;
    }
    if (!target.isFieldAccess || target.args.size() != 1) {
      return false;
    }
    const Expr &receiver = target.args.front();
    return receiver.kind == Expr::Kind::Name && receiver.name == thisName;
  };

  std::function<FieldInitFlow(const std::vector<Expr> &, const std::unordered_set<std::string> &)>
      analyzeStatements;
  std::function<FieldInitFlow(const Expr &, const std::unordered_set<std::string> &)> analyzeStatement;
  std::function<FieldInitFlow(const Expr &, const std::unordered_set<std::string> &)> analyzeBlockExpr;

  analyzeStatements = [&](const std::vector<Expr> &statements,
                          const std::unordered_set<std::string> &assignedIn) -> FieldInitFlow {
    std::unordered_set<std::string> current = assignedIn;
    bool fallsThrough = true;
    for (const auto &stmt : statements) {
      if (!fallsThrough) {
        break;
      }
      FieldInitFlow next = analyzeStatement(stmt, current);
      if (!next.ok) {
        return next;
      }
      fallsThrough = next.fallsThrough;
      current = std::move(next.assigned);
    }
    FieldInitFlow result;
    result.ok = true;
    result.fallsThrough = fallsThrough;
    result.assigned = std::move(current);
    return result;
  };

  analyzeBlockExpr = [&](const Expr &expr,
                         const std::unordered_set<std::string> &assignedIn) -> FieldInitFlow {
    if (expr.kind == Expr::Kind::Call && expr.hasBodyArguments) {
      return analyzeStatements(expr.bodyArguments, assignedIn);
    }
    return analyzeStatement(expr, assignedIn);
  };

  analyzeStatement = [&](const Expr &stmt,
                         const std::unordered_set<std::string> &assignedIn) -> FieldInitFlow {
    if (isReturnCall(stmt)) {
      if (!allAssigned(assignedIn)) {
        return FieldInitFlow{false, false, assignedIn};
      }
      return FieldInitFlow{true, false, assignedIn};
    }
    if (isMatchCall(stmt)) {
      Expr expanded;
      std::string error;
      if (!lowerMatchToIf(stmt, expanded, error)) {
        return FieldInitFlow{false, false, assignedIn};
      }
      return analyzeStatement(expanded, assignedIn);
    }
    if (isIfCall(stmt) && stmt.args.size() == 3) {
      FieldInitFlow thenFlow = analyzeBlockExpr(stmt.args[1], assignedIn);
      if (!thenFlow.ok) {
        return thenFlow;
      }
      FieldInitFlow elseFlow = analyzeBlockExpr(stmt.args[2], assignedIn);
      if (!elseFlow.ok) {
        return elseFlow;
      }
      FieldInitFlow result;
      result.ok = true;
      result.fallsThrough = thenFlow.fallsThrough || elseFlow.fallsThrough;
      if (thenFlow.fallsThrough && elseFlow.fallsThrough) {
        result.assigned = intersectAssigned(thenFlow.assigned, elseFlow.assigned);
      } else if (thenFlow.fallsThrough) {
        result.assigned = std::move(thenFlow.assigned);
      } else if (elseFlow.fallsThrough) {
        result.assigned = std::move(elseFlow.assigned);
      } else {
        result.assigned = assignedIn;
      }
      return result;
    }
    if (isLoopCall(stmt) || isWhileCall(stmt) || isForCall(stmt) || isRepeatCall(stmt)) {
      if (!stmt.args.empty()) {
        const Expr &body = stmt.args.back();
        if (body.kind == Expr::Kind::Call && body.hasBodyArguments) {
          FieldInitFlow bodyFlow = analyzeStatements(body.bodyArguments, assignedIn);
          if (!bodyFlow.ok) {
            return bodyFlow;
          }
        }
      }
      FieldInitFlow result;
      result.ok = true;
      result.fallsThrough = true;
      result.assigned = assignedIn;
      return result;
    }
    if (isBuiltinBlockCall(stmt) && stmt.hasBodyArguments) {
      return analyzeStatements(stmt.bodyArguments, assignedIn);
    }
    std::unordered_set<std::string> assignedOut = assignedIn;
    if (isAssignCall(stmt) && stmt.args.size() == 2) {
      const Expr &target = stmt.args.front();
      if (isThisFieldTarget(target) && missingFields.count(target.name) > 0) {
        assignedOut.insert(target.name);
      }
    }
    FieldInitFlow result;
    result.ok = true;
    result.fallsThrough = true;
    result.assigned = std::move(assignedOut);
    return result;
  };

  FieldInitFlow flow = analyzeStatements(createDef.statements, {});
  if (!flow.ok) {
    return false;
  }
  if (flow.fallsThrough && !allAssigned(flow.assigned)) {
    return false;
  }
  return true;
}

}  // namespace primec::semantics
