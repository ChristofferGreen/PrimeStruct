#include "SemanticsValidator.h"

#include <optional>

namespace primec::semantics {

bool SemanticsValidator::validateBlockExpr(const std::vector<ParameterInfo> &params,
                                           const std::unordered_map<std::string, BindingInfo> &locals,
                                           const Expr &expr) {
  if (!expr.args.empty() || !expr.templateArgs.empty() || hasNamedArguments(expr.argNames)) {
    error_ = "block expression does not accept arguments";
    return false;
  }
  if (expr.bodyArguments.empty()) {
    error_ = "block expression requires a value";
    return false;
  }

  std::unordered_map<std::string, BindingInfo> blockLocals = locals;
  bool sawReturn = false;
  for (size_t i = 0; i < expr.bodyArguments.size(); ++i) {
    const Expr &bodyExpr = expr.bodyArguments[i];
    const bool isLast = (i + 1 == expr.bodyArguments.size());
    if (bodyExpr.isBinding) {
      if (isLast && !sawReturn) {
        error_ = "block expression must end with an expression";
        return false;
      }
      if (isParam(params, bodyExpr.name) || blockLocals.count(bodyExpr.name) > 0) {
        error_ = "duplicate binding name: " + bodyExpr.name;
        return false;
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
          error_ = "binding requires exactly one argument";
          return false;
        }
        if (!validateExpr(params, blockLocals, bodyExpr.args.front())) {
          return false;
        }
        ReturnKind initKind = inferExprReturnKind(bodyExpr.args.front(), params, blockLocals);
        if (initKind == ReturnKind::Void && !isStructConstructorValueExpr(bodyExpr.args.front())) {
          error_ = "binding initializer requires a value";
          return false;
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
          error_ = "restrict type does not match binding type";
          return false;
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
          error_ = "Reference bindings require location(...)";
          return false;
        }
      }
      blockLocals.emplace(bodyExpr.name, info);
      continue;
    }

    if (isReturnCall(bodyExpr)) {
      if (bodyExpr.args.size() != 1) {
        error_ = "return requires a value in block expression";
        return false;
      }
      if (!validateExpr(params, blockLocals, bodyExpr.args.front())) {
        return false;
      }
      ReturnKind kind = inferExprReturnKind(bodyExpr.args.front(), params, blockLocals);
      if (kind == ReturnKind::Void && !isStructConstructorValueExpr(bodyExpr.args.front())) {
        error_ = "block expression requires a value";
        return false;
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
        error_ = "block expression requires a value";
        return false;
      }
    }
  }

  return true;
}

} // namespace primec::semantics
