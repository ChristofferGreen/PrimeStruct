#include "SemanticsValidator.h"

#include <string>

namespace primec::semantics {

bool SemanticsValidator::validateExprLateUnknownTargetFallbacks(
    const std::vector<ParameterInfo> &params,
    const std::unordered_map<std::string, BindingInfo> &locals,
    const Expr &expr,
    const ExprLateUnknownTargetFallbackContext &context,
    bool &handledOut) {
  auto failLateUnknownTargetDiagnostic = [&](std::string message) -> bool {
    return failExprDiagnostic(expr, std::move(message));
  };
  handledOut = false;
  if (context.resolveMapTarget != nullptr && expr.isMethodCall &&
      (expr.name == "count" || expr.name == "contains" ||
       expr.name == "tryAt" || expr.name == "at" ||
       expr.name == "at_unsafe" || expr.name == "insert") &&
      !expr.args.empty() && context.resolveMapTarget(expr.args.front())) {
    const std::string canonicalMapMethodTarget =
        "/std/collections/map/" + expr.name;
    const std::string aliasMapMethodTarget = "/map/" + expr.name;
    Expr rewrittenMapMethodCall = expr;
    rewrittenMapMethodCall.isMethodCall = false;
    rewrittenMapMethodCall.namespacePrefix.clear();
    if (hasDeclaredDefinitionPath(canonicalMapMethodTarget) ||
        hasImportedDefinitionPath(canonicalMapMethodTarget)) {
      rewrittenMapMethodCall.name = canonicalMapMethodTarget;
    } else if (hasDeclaredDefinitionPath(aliasMapMethodTarget) ||
               hasImportedDefinitionPath(aliasMapMethodTarget)) {
      rewrittenMapMethodCall.name = aliasMapMethodTarget;
    } else {
      rewrittenMapMethodCall.name = canonicalMapMethodTarget;
    }
    handledOut = true;
    return validateExpr(params, locals, rewrittenMapMethodCall);
  }

  auto isFileBinding = [&](const BindingInfo &binding) {
    const std::string normalizedType = normalizeBindingTypeName(binding.typeName);
    if (normalizedType == "File") {
      return true;
    }
    if ((normalizedType == "Reference" || normalizedType == "Pointer") &&
        !binding.typeTemplateArg.empty()) {
      std::string base;
      std::string argText;
      if (!splitTemplateTypeName(binding.typeTemplateArg, base, argText)) {
        return false;
      }
      return normalizeBindingTypeName(base) == "File";
    }
    return false;
  };
  auto isFileReceiverExpr = [&](const Expr &target) {
    if (target.kind != Expr::Kind::Name) {
      return false;
    }
    if (const BindingInfo *paramBinding = findParamBinding(params, target.name)) {
      return isFileBinding(*paramBinding);
    }
    auto it = locals.find(target.name);
    return it != locals.end() && isFileBinding(it->second);
  };
  auto isStdlibFileWriteFacadeName = [&](const std::string &name) {
    return name == "/File/write" || name == "/File/write_line" ||
           name == "/std/file/File/write" || name == "/std/file/File/write_line";
  };
  auto isBroaderStdlibFileWriteFacadeCall = [&]() {
    if (expr.args.size() <= 10) {
      return false;
    }
    if (expr.isMethodCall &&
        (expr.name == "write" || expr.name == "write_line") &&
        !expr.args.empty() && isFileReceiverExpr(expr.args.front())) {
      return true;
    }
    if (!expr.isMethodCall && isStdlibFileWriteFacadeName(expr.name)) {
      return true;
    }
    const std::string resolvedTarget = resolveCalleePath(expr);
    return !expr.isMethodCall && isStdlibFileWriteFacadeName(resolvedTarget);
  };
  if (isBroaderStdlibFileWriteFacadeCall()) {
    handledOut = true;
    return failLateUnknownTargetDiagnostic(
        "stdlib File write/write_line currently support up to nine values; broader arities await [args<T>] runtime support");
  }

  const std::string resolvedTarget = resolveCalleePath(expr);
  std::string soaFieldName;
  if (splitSoaFieldViewHelperPath(resolvedTarget, &soaFieldName)) {
    handledOut = true;
    return failLateUnknownTargetDiagnostic(
        "soa_vector field views are not implemented yet: " + soaFieldName);
  }

  handledOut = true;
  return failLateUnknownTargetDiagnostic("unknown call target: " +
                                         formatUnknownCallTarget(expr));
}

} // namespace primec::semantics
