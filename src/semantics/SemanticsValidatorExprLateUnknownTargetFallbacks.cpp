#include "SemanticsValidator.h"

#include <string>
#include <string_view>

namespace primec::semantics {

namespace {

bool isCanonicalMapMethodHelper(std::string_view helperName) {
  return helperName == "count" || helperName == "count_ref" ||
         helperName == "contains" || helperName == "contains_ref" ||
         helperName == "tryAt" || helperName == "tryAt_ref" ||
         helperName == "at" || helperName == "at_ref" ||
         helperName == "at_unsafe" || helperName == "at_unsafe_ref" ||
         helperName == "insert" || helperName == "insert_ref";
}

} // namespace

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
  std::string normalizedMethodName = expr.name;
  if (!normalizedMethodName.empty() && normalizedMethodName.front() == '/') {
    normalizedMethodName.erase(normalizedMethodName.begin());
  }
  if (context.resolveMapTarget != nullptr && expr.isMethodCall &&
      isCanonicalMapMethodHelper(normalizedMethodName) && !expr.args.empty() &&
      context.resolveMapTarget(expr.args.front())) {
    Expr rewrittenMapMethodCall = expr;
    rewrittenMapMethodCall.isMethodCall = false;
    rewrittenMapMethodCall.namespacePrefix.clear();
    rewrittenMapMethodCall.name = preferredMapMethodTargetForCall(
        params, locals, expr.args.front(), normalizedMethodName);
    if (rewrittenMapMethodCall.name.empty()) {
      rewrittenMapMethodCall.name =
          "/std/collections/map/" + normalizedMethodName;
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
