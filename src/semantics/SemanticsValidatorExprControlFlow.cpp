#include "SemanticsValidator.h"

#include <optional>

namespace primec::semantics {

bool SemanticsValidator::isIfBlockEnvelope(const Expr &expr) const {
  return isEnvelopeValueExpr(expr, true);
}

const Expr *SemanticsValidator::getIfBlockEnvelopeValueExpr(const Expr &expr) const {
  return getEnvelopeValueExpr(expr, true);
}

bool SemanticsValidator::isStructConstructorValueExpr(const Expr &expr) {
  if (expr.kind == Expr::Kind::Call && !expr.isBinding) {
    const std::string resolved = resolveCalleePath(expr);
    if (structNames_.count(resolved) > 0) {
      return true;
    }
  }
  if (isMatchCall(expr)) {
    Expr expanded;
    std::string error;
    if (!lowerMatchToIf(expr, expanded, error)) {
      return false;
    }
    return isStructConstructorValueExpr(expanded);
  }
  if (isIfCall(expr) && expr.args.size() == 3) {
    const Expr &thenArg = expr.args[1];
    const Expr &elseArg = expr.args[2];
    const Expr *thenValue = getIfBlockEnvelopeValueExpr(thenArg);
    const Expr *elseValue = getIfBlockEnvelopeValueExpr(elseArg);
    const Expr &thenExpr = thenValue ? *thenValue : thenArg;
    const Expr &elseExpr = elseValue ? *elseValue : elseArg;
    return isStructConstructorValueExpr(thenExpr) && isStructConstructorValueExpr(elseExpr);
  }
  if (const Expr *valueExpr = getIfBlockEnvelopeValueExpr(expr)) {
    return isStructConstructorValueExpr(*valueExpr);
  }
  return false;
}

bool SemanticsValidator::validateIfExpr(const std::vector<ParameterInfo> &params,
                                        const std::unordered_map<std::string, BindingInfo> &locals,
                                        const Expr &expr) {
  auto failIfDiagnostic = [&](const Expr &diagnosticExpr,
                              std::string message) -> bool {
    return failExprDiagnostic(diagnosticExpr, std::move(message));
  };
  if (hasNamedArguments(expr.argNames)) {
    return failIfDiagnostic(expr, "named arguments not supported for builtin calls");
  }
  if (expr.hasBodyArguments || !expr.bodyArguments.empty()) {
    return failIfDiagnostic(expr, "if does not accept trailing block arguments");
  }
  if (expr.args.size() != 3) {
    return failIfDiagnostic(expr, "if requires condition, then, else");
  }

  const Expr &cond = expr.args[0];
  const Expr &thenArg = expr.args[1];
  const Expr &elseArg = expr.args[2];
  if (!validateExpr(params, locals, cond)) {
    return false;
  }
  ReturnKind condKind = inferExprReturnKind(cond, params, locals);
  if (condKind != ReturnKind::Bool) {
    return failIfDiagnostic(cond, "if condition requires bool");
  }

  std::unordered_map<std::string, BindingInfo> branchLocals = locals;
  auto validateBranchValueKind = [&](const Expr &branch, const char *label, ReturnKind &kindOut, bool &stringOut) -> bool {
    kindOut = ReturnKind::Unknown;
    stringOut = false;
    if (!isIfBlockEnvelope(branch)) {
      return failIfDiagnostic(branch, "if branches require block envelopes");
    }
    if (branch.bodyArguments.empty()) {
      return failIfDiagnostic(branch,
                              std::string(label) + " block must produce a value");
    }

    LocalBindingScope branchScope(*this, branchLocals);
    const Expr *valueExpr = nullptr;
    bool sawReturn = false;
    for (const auto &bodyExpr : branch.bodyArguments) {
      if (isSyntheticBlockValueBinding(bodyExpr)) {
        if (!validateExpr(params, branchLocals, bodyExpr.args.front())) {
          return false;
        }
        if (!sawReturn) {
          valueExpr = &bodyExpr.args.front();
        }
        continue;
      }
      if (bodyExpr.isBinding) {
        if (isParam(params, bodyExpr.name) || branchLocals.count(bodyExpr.name) > 0) {
          return failIfDiagnostic(bodyExpr,
                                  "duplicate binding name: " + bodyExpr.name);
        }
        BindingInfo info;
        std::optional<std::string> restrictType;
        if (!parseBindingInfo(bodyExpr, bodyExpr.namespacePrefix, structNames_, importAliases_, info, restrictType,
                              error_)) {
          return false;
        }
        if (bodyExpr.args.empty()) {
          if (!validateOmittedBindingInitializer(bodyExpr, info, bodyExpr.namespacePrefix)) {
            return false;
          }
        } else {
          if (bodyExpr.args.size() != 1) {
            return failIfDiagnostic(bodyExpr,
                                    "binding requires exactly one argument");
          }
          if (!validateExpr(params, branchLocals, bodyExpr.args.front())) {
            return false;
          }
          ReturnKind initKind = inferExprReturnKind(bodyExpr.args.front(), params, branchLocals);
          if (initKind == ReturnKind::Void && !isStructConstructorValueExpr(bodyExpr.args.front())) {
            return failIfDiagnostic(bodyExpr,
                                    "binding initializer requires a value");
          }
          if (!hasExplicitBindingTypeTransform(bodyExpr)) {
            (void)inferBindingTypeFromInitializer(bodyExpr.args.front(), params, branchLocals, info, &bodyExpr);
          }
        }
        if (restrictType.has_value()) {
          const bool hasTemplate = !info.typeTemplateArg.empty();
          if (!restrictMatchesBinding(*restrictType,
                                      info.typeName,
                                      info.typeTemplateArg,
                                      hasTemplate,
                                      bodyExpr.namespacePrefix)) {
            return failIfDiagnostic(bodyExpr,
                                    "restrict type does not match binding type");
          }
        }
        if (info.typeName == "Reference" && !bodyExpr.args.empty()) {
          const Expr &init = bodyExpr.args.front();
          std::string pointerName;
          auto isReferenceInitializer = [&](const Expr &candidate) {
            if (candidate.kind != Expr::Kind::Call) {
              return false;
            }
            if (getBuiltinPointerName(candidate, pointerName) && pointerName == "location" && candidate.args.size() == 1) {
              return true;
            }
            if (!candidate.isMethodCall && isSimpleCallName(candidate, "borrow") && candidate.args.size() == 1) {
              const Expr &storage = candidate.args.front();
              std::string builtinName;
              if (storage.kind == Expr::Kind::Name ||
                  (storage.kind == Expr::Kind::Call && storage.isFieldAccess && storage.args.size() == 1) ||
                  (storage.kind == Expr::Kind::Call && getBuiltinPointerName(storage, builtinName) &&
                   builtinName == "dereference" && storage.args.size() == 1)) {
                BindingInfo binding;
                bool resolved = false;
                if (resolveUninitializedStorageBinding(params, branchLocals, storage, binding, resolved) &&
                    resolved && binding.typeName == "uninitialized" && !binding.typeTemplateArg.empty()) {
                  return true;
                }
              }
            }
            auto defIt = defMap_.find(resolveCalleePath(candidate));
            if (defIt == defMap_.end() || defIt->second == nullptr) {
              return false;
            }
            for (const auto &transform : defIt->second->transforms) {
              if (transform.name != "return" || transform.templateArgs.size() != 1) {
                continue;
              }
              std::string base;
              std::string arg;
              if (!splitTemplateTypeName(transform.templateArgs.front(), base, arg)) {
                continue;
              }
              if ((base == "Reference" || base == "Pointer") && !arg.empty()) {
                return true;
              }
            }
            return false;
          };
          if (!isReferenceInitializer(init)) {
            return failIfDiagnostic(bodyExpr,
                                    "Reference bindings require location(...)");
          }
        }
        insertLocalBinding(branchLocals, bodyExpr.name, std::move(info));
        continue;
      }

      if (isReturnCall(bodyExpr)) {
        if (bodyExpr.args.size() != 1) {
          return failIfDiagnostic(
              bodyExpr,
              std::string("return requires a value in ") + label + " block");
        }
        if (!validateExpr(params, branchLocals, bodyExpr.args.front())) {
          return false;
        }
        valueExpr = &bodyExpr.args.front();
        sawReturn = true;
        continue;
      }

      if (!validateExpr(params, branchLocals, bodyExpr)) {
        return false;
      }
      if (!sawReturn) {
        valueExpr = &bodyExpr;
      }
    }

    if (!valueExpr) {
      return failIfDiagnostic(branch,
                              std::string(label) + " block must end with an expression");
    }
    kindOut = inferExprReturnKind(*valueExpr, params, branchLocals);
    stringOut = (kindOut == ReturnKind::String);
    if (kindOut == ReturnKind::Void) {
      if (isStructConstructorValueExpr(*valueExpr)) {
        kindOut = ReturnKind::Unknown;
      } else {
        return failIfDiagnostic(*valueExpr, "if branches must produce a value");
      }
    }
    return true;
  };

  ReturnKind thenKind = ReturnKind::Unknown;
  ReturnKind elseKind = ReturnKind::Unknown;
  bool thenIsString = false;
  bool elseIsString = false;
  if (!validateBranchValueKind(thenArg, "then", thenKind, thenIsString)) {
    return false;
  }
  if (!validateBranchValueKind(elseArg, "else", elseKind, elseIsString)) {
    return false;
  }
  if (thenIsString != elseIsString) {
    return failIfDiagnostic(expr, "if branches must return compatible types");
  }

  ReturnKind combined = inferExprReturnKind(expr, params, locals);
  if (thenKind != ReturnKind::Unknown && elseKind != ReturnKind::Unknown && combined == ReturnKind::Unknown) {
    return failIfDiagnostic(expr, "if branches must return compatible types");
  }
  return true;
}

} // namespace primec::semantics
