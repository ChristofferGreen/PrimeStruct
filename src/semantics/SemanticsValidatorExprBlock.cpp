#include "SemanticsValidator.h"

#include <optional>

namespace primec::semantics {

bool SemanticsValidator::validateBlockExpr(const std::vector<ParameterInfo> &params,
                                           const std::unordered_map<std::string, BindingInfo> &locals,
                                           const Expr &expr) {
  auto failBlockDiagnostic = [&](const Expr &diagnosticExpr,
                                 std::string message) -> bool {
    return failExprDiagnostic(diagnosticExpr, std::move(message));
  };
  if (!expr.args.empty() || !expr.templateArgs.empty() || hasNamedArguments(expr.argNames)) {
    return failBlockDiagnostic(expr, "block expression does not accept arguments");
  }
  if (expr.bodyArguments.empty()) {
    return failBlockDiagnostic(expr, "block expression requires a value");
  }

  std::unordered_map<std::string, BindingInfo> blockLocals = locals;
  bool sawReturn = false;
  for (size_t i = 0; i < expr.bodyArguments.size(); ++i) {
    const Expr &bodyExpr = expr.bodyArguments[i];
    const bool isLast = (i + 1 == expr.bodyArguments.size());
    if (isSyntheticBlockValueBinding(bodyExpr)) {
      if (!validateExpr(params, blockLocals, bodyExpr.args.front())) {
        return false;
      }
      if (isLast && !sawReturn) {
        ReturnKind kind = inferExprReturnKind(bodyExpr.args.front(), params, blockLocals);
        if (kind == ReturnKind::Void && !isStructConstructorValueExpr(bodyExpr.args.front())) {
          return failBlockDiagnostic(bodyExpr,
                                     "block expression requires a value");
        }
      }
      continue;
    }
    if (bodyExpr.isBinding) {
      if (isLast && !sawReturn) {
        return failBlockDiagnostic(bodyExpr,
                                   "block expression must end with an expression");
      }
      if (isParam(params, bodyExpr.name) || blockLocals.count(bodyExpr.name) > 0) {
        return failBlockDiagnostic(bodyExpr,
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
          return failBlockDiagnostic(bodyExpr,
                                     "binding requires exactly one argument");
        }
        if (!validateExpr(params, blockLocals, bodyExpr.args.front())) {
          return false;
        }
        ReturnKind initKind = inferExprReturnKind(bodyExpr.args.front(), params, blockLocals);
        if (initKind == ReturnKind::Void && !isStructConstructorValueExpr(bodyExpr.args.front())) {
          return failBlockDiagnostic(bodyExpr,
                                     "binding initializer requires a value");
        }
        if (!hasExplicitBindingTypeTransform(bodyExpr)) {
          (void)inferBindingTypeFromInitializer(bodyExpr.args.front(), params, blockLocals, info, &bodyExpr);
        }
      }
      if (restrictType.has_value()) {
        const bool hasTemplate = !info.typeTemplateArg.empty();
        if (!restrictMatchesBinding(*restrictType,
                                    info.typeName,
                                    info.typeTemplateArg,
                                    hasTemplate,
                                    bodyExpr.namespacePrefix)) {
          return failBlockDiagnostic(bodyExpr,
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
              if (resolveUninitializedStorageBinding(params, blockLocals, storage, binding, resolved) &&
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
          return failBlockDiagnostic(bodyExpr,
                                     "Reference bindings require location(...)");
        }
      }
      insertLocalBinding(blockLocals, bodyExpr.name, std::move(info));
      continue;
    }

    if (isReturnCall(bodyExpr)) {
      if (bodyExpr.args.size() != 1) {
        return failBlockDiagnostic(bodyExpr,
                                   "return requires a value in block expression");
      }
      if (!validateExpr(params, blockLocals, bodyExpr.args.front())) {
        return false;
      }
      ReturnKind kind = inferExprReturnKind(bodyExpr.args.front(), params, blockLocals);
      if (kind == ReturnKind::Void && !isStructConstructorValueExpr(bodyExpr.args.front())) {
        return failBlockDiagnostic(bodyExpr,
                                   "block expression requires a value");
      }
      sawReturn = true;
      continue;
    }

    if (!validateExpr(params, blockLocals, bodyExpr)) {
      return false;
    }
    if (isLast && !sawReturn) {
      ReturnKind kind = inferExprReturnKind(bodyExpr, params, blockLocals);
      if (kind == ReturnKind::Void && !isStructConstructorValueExpr(bodyExpr)) {
        return failBlockDiagnostic(bodyExpr,
                                   "block expression requires a value");
      }
    }
  }

  return true;
}

} // namespace primec::semantics
