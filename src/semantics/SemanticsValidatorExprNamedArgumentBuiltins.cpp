#include "SemanticsValidator.h"

#include <string>
#include <vector>

namespace primec::semantics {

bool SemanticsValidator::validateExprNamedArguments(
    const std::vector<ParameterInfo> &params,
    const std::unordered_map<std::string, BindingInfo> &locals,
    const Expr &expr,
    const std::string &resolved,
    bool resolvedMethod,
    const ExprNamedArgumentBuiltinContext &context) {
  auto failNamedArgumentDiagnostic = [&](std::string message) -> bool {
    return failExprDiagnostic(expr, std::move(message));
  };
  if (hasNamedArguments(expr.argNames)) {
    auto resolveVectorMutatorName = [&](const std::string &name,
                                        std::string &helperOut) -> bool {
      std::string normalized = name;
      if (!normalized.empty() && normalized.front() == '/') {
        normalized.erase(normalized.begin());
      }
      if (normalized.rfind("vector/", 0) == 0) {
        normalized = normalized.substr(std::string("vector/").size());
      } else if (normalized.rfind("array/", 0) == 0) {
        const std::string helperName =
            normalized.substr(std::string("array/").size());
        const std::string canonicalPath =
            "/std/collections/vector/" + helperName;
        if (hasDefinitionPath(canonicalPath) ||
            hasImportedDefinitionPath(canonicalPath)) {
          return false;
        }
        normalized = helperName;
      } else if (normalized.rfind("std/collections/vector/", 0) == 0) {
        normalized =
            normalized.substr(std::string("std/collections/vector/").size());
      }
      if (normalized == "push" || normalized == "pop" ||
          normalized == "reserve" || normalized == "clear" ||
          normalized == "remove_at" || normalized == "remove_swap") {
        helperOut = normalized;
        return true;
      }
      return false;
    };

    std::string vectorHelperName;
    if (resolveVectorMutatorName(expr.name, vectorHelperName) &&
        (resolvedMethod || !context.hasVectorHelperCallResolution) &&
        !hasDeclaredDefinitionPath(resolved) &&
        !hasImportedDefinitionPath(resolved)) {
      return failNamedArgumentDiagnostic(vectorHelperName + " is only supported as a statement");
    }
  }

  if (!validateNamedArguments(expr.args, expr.argNames, resolved, error_)) {
    return false;
  }
  if (defMap_.find(resolved) == defMap_.end() || resolvedMethod) {
    return validateExprNamedArgumentBuiltins(
        params, locals, expr, resolved, resolvedMethod, context);
  }
  return true;
}

bool SemanticsValidator::validateExprNamedArgumentBuiltins(
    const std::vector<ParameterInfo> &params,
    const std::unordered_map<std::string, BindingInfo> &locals,
    const Expr &expr,
    const std::string &resolved,
    bool resolvedMethod,
    const ExprNamedArgumentBuiltinContext &context) {
  auto failNamedArgumentDiagnostic = [&](std::string message) -> bool {
    return failExprDiagnostic(expr, std::move(message));
  };
  const bool allowsNamedArgsPackBuiltinLabels =
      (context.isNamedArgsPackMethodAccessCall != nullptr &&
       context.isNamedArgsPackMethodAccessCall(expr)) ||
      (context.isNamedArgsPackWrappedFileBuiltinAccessCall != nullptr &&
       context.isNamedArgsPackWrappedFileBuiltinAccessCall(expr));
  if (hasNamedArguments(expr.argNames) && resolvedMethod &&
      !allowsNamedArgsPackBuiltinLabels) {
    std::string vectorHelperName;
    if (getVectorStatementHelperName(expr, vectorHelperName)) {
      return failNamedArgumentDiagnostic(vectorHelperName + " is only supported as a statement");
    }
    return failNamedArgumentDiagnostic("named arguments not supported for builtin calls");
  }
  if (!hasNamedArguments(expr.argNames) || allowsNamedArgsPackBuiltinLabels) {
    return true;
  }

  std::string builtinName;
  auto isLegacyCollectionBuiltinCall = [&]() {
    std::string collectionName;
    if (!getBuiltinCollectionName(expr, collectionName)) {
      return false;
    }
    return defMap_.find(resolved) == defMap_.end();
  };
  auto isLegacyArrayAccessBuiltinCall = [&]() {
    std::string arrayAccessName;
    if (!getBuiltinArrayAccessName(expr, arrayAccessName)) {
      return false;
    }
    const std::string resolvedPath = resolveCalleePath(expr);
    if (resolvedPath.rfind("/std/collections/vector/at", 0) == 0 ||
        resolvedPath.rfind("/std/collections/map/at", 0) == 0) {
      return false;
    }
    if (defMap_.find(resolved) == defMap_.end() && !expr.args.empty()) {
      for (const auto &receiverCandidate : expr.args) {
        bool isBuiltinMethod = false;
        std::string methodResolved;
        if (resolveMethodTarget(params, locals, expr.namespacePrefix, receiverCandidate,
                                expr.name, methodResolved, isBuiltinMethod) &&
            !isBuiltinMethod && defMap_.find(methodResolved) != defMap_.end()) {
          return false;
        }
      }
    }
    return defMap_.find(resolved) == defMap_.end();
  };
  auto isLegacyCountLikeBuiltinCall = [&](const char *helperName) {
    if (!isVectorBuiltinName(expr, helperName)) {
      return false;
    }
    const std::string resolvedPath = resolveCalleePath(expr);
    if (resolvedPath == "/std/collections/vector/count") {
      return false;
    }
    if (resolvedPath == "/std/collections/vector/capacity") {
      return false;
    }
    if (std::string(helperName) == "count" &&
        context.isArrayNamespacedVectorCountCompatibilityCall != nullptr &&
        context.isArrayNamespacedVectorCountCompatibilityCall(expr)) {
      return false;
    }
    if (defMap_.find(resolved) == defMap_.end() && !expr.args.empty()) {
      for (const auto &receiverCandidate : expr.args) {
        bool isBuiltinMethod = false;
        std::string methodResolved;
        if (resolveMethodTarget(params, locals, expr.namespacePrefix, receiverCandidate,
                                helperName, methodResolved, isBuiltinMethod) &&
            !isBuiltinMethod && defMap_.find(methodResolved) != defMap_.end()) {
          return false;
        }
      }
    }
    return defMap_.find(resolved) == defMap_.end();
  };
  auto isLegacyCountBuiltinCall = [&]() {
    return isLegacyCountLikeBuiltinCall("count");
  };
  auto isLegacyCapacityBuiltinCall = [&]() {
    return isLegacyCountLikeBuiltinCall("capacity");
  };
  auto isLegacySoaAccessBuiltinCall = [&]() {
    if (!(expr.name == "get" || expr.name == "get_ref" ||
          expr.name == "ref" || expr.name == "ref_ref")) {
      return false;
    }
    if (defMap_.find(resolved) == defMap_.end() && !expr.args.empty()) {
      auto tryResolveReceiverIndex = [&](size_t index) {
        if (index >= expr.args.size()) {
          return false;
        }
        const Expr &receiverCandidate = expr.args[index];
        bool isBuiltinMethod = false;
        std::string methodResolved;
        return resolveMethodTarget(params, locals, expr.namespacePrefix, receiverCandidate,
                                   expr.name, methodResolved, isBuiltinMethod) &&
               !isBuiltinMethod && defMap_.find(methodResolved) != defMap_.end();
      };
      const bool hasNamedArgs = hasNamedArguments(expr.argNames);
      if (hasNamedArgs) {
        bool hasValuesNamedReceiver = false;
        for (size_t i = 0; i < expr.args.size(); ++i) {
          if (i < expr.argNames.size() && expr.argNames[i].has_value() &&
              *expr.argNames[i] == "values") {
            hasValuesNamedReceiver = true;
            if (tryResolveReceiverIndex(i)) {
              return false;
            }
          }
        }
        if (!hasValuesNamedReceiver) {
          for (size_t i = 0; i < expr.args.size(); ++i) {
            if (tryResolveReceiverIndex(i)) {
              return false;
            }
          }
        }
      } else {
        for (size_t i = 0; i < expr.args.size(); ++i) {
          if (tryResolveReceiverIndex(i)) {
            return false;
          }
        }
      }
    }
    return defMap_.find(resolved) == defMap_.end();
  };
  auto resolveLegacyVectorHelperName = [&](std::string &nameOut) -> bool {
    if (isSimpleCallName(expr, "push")) {
      nameOut = "push";
      return true;
    }
    if (isSimpleCallName(expr, "pop")) {
      nameOut = "pop";
      return true;
    }
    if (isSimpleCallName(expr, "reserve")) {
      nameOut = "reserve";
      return true;
    }
    if (isSimpleCallName(expr, "clear")) {
      nameOut = "clear";
      return true;
    }
    if (isSimpleCallName(expr, "remove_at")) {
      nameOut = "remove_at";
      return true;
    }
    if (isSimpleCallName(expr, "remove_swap")) {
      nameOut = "remove_swap";
      return true;
    }
    if (expr.name.empty()) {
      return false;
    }
    std::string normalized = expr.name;
    if (!normalized.empty() && normalized.front() == '/') {
      normalized.erase(normalized.begin());
    }
    if (normalized.rfind("vector/", 0) != 0) {
      if (normalized.rfind("array/", 0) == 0) {
        normalized = normalized.substr(std::string("array/").size());
      } else if (normalized.rfind("std/collections/vector/", 0) == 0) {
        normalized =
            normalized.substr(std::string("std/collections/vector/").size());
      } else {
        return false;
      }
    } else {
      normalized = normalized.substr(std::string("vector/").size());
    }
    if (normalized == "push" || normalized == "pop" || normalized == "reserve" ||
        normalized == "clear" || normalized == "remove_at" ||
        normalized == "remove_swap") {
      nameOut = normalized;
      return true;
    }
    return false;
  };
  auto isLegacyVectorHelperBuiltinCall = [&]() {
    std::string helperName;
    if (!resolveLegacyVectorHelperName(helperName)) {
      return false;
    }
    const bool isStdNamespacedVectorCanonicalHelperCall =
        !expr.isMethodCall &&
        resolveCalleePath(expr).rfind("/std/collections/vector/", 0) == 0;
    if (isStdNamespacedVectorCanonicalHelperCall &&
        defMap_.find("/vector/" + helperName) == defMap_.end()) {
      return false;
    }
    if (defMap_.find(resolved) == defMap_.end() && !expr.args.empty()) {
      for (const auto &receiverCandidate : expr.args) {
        bool isBuiltinMethod = false;
        std::string methodResolved;
        if (resolveMethodTarget(params, locals, expr.namespacePrefix, receiverCandidate,
                                expr.name, methodResolved, isBuiltinMethod) &&
            !isBuiltinMethod && defMap_.find(methodResolved) != defMap_.end()) {
          return false;
        }
      }
    }
    return defMap_.find(resolved) == defMap_.end();
  };
  const bool isLegacyVectorHelperBuiltin = isLegacyVectorHelperBuiltinCall();
  std::string vectorHelperName;
  if (isLegacyVectorHelperBuiltin &&
      resolveLegacyVectorHelperName(vectorHelperName)) {
    return failNamedArgumentDiagnostic(vectorHelperName + " is only supported as a statement");
  }

  bool isBuiltin = false;
  if (getBuiltinOperatorName(expr, builtinName) ||
      getBuiltinComparisonName(expr, builtinName) ||
      getBuiltinMutationName(expr, builtinName) ||
      getBuiltinClampName(expr, builtinName, allowMathBareName(expr.name)) ||
      getBuiltinMinMaxName(expr, builtinName, allowMathBareName(expr.name)) ||
      getBuiltinAbsSignName(expr, builtinName, allowMathBareName(expr.name)) ||
      getBuiltinSaturateName(expr, builtinName, allowMathBareName(expr.name)) ||
      getBuiltinMathName(expr, builtinName, allowMathBareName(expr.name)) ||
      getBuiltinGpuName(expr, builtinName) ||
      getBuiltinPointerName(expr, builtinName) ||
      getBuiltinMemoryName(expr, builtinName) ||
      getBuiltinConvertName(expr, builtinName) ||
      isLegacyCollectionBuiltinCall() || isLegacyArrayAccessBuiltinCall() ||
      isAssignCall(expr) || isIfCall(expr) || isMatchCall(expr) ||
      isLoopCall(expr) || isWhileCall(expr) || isForCall(expr) ||
      isRepeatCall(expr) || isLegacyCountBuiltinCall() || expr.name == "File" ||
      expr.name == "try" || isLegacyCapacityBuiltinCall() ||
      isLegacySoaAccessBuiltinCall() || isLegacyVectorHelperBuiltin ||
      isSimpleCallName(expr, "to_soa") || isSimpleCallName(expr, "to_aos") ||
      isSimpleCallName(expr, "to_aos_ref") ||
      isSimpleCallName(expr, "dispatch") || isSimpleCallName(expr, "buffer") ||
      isSimpleCallName(expr, "upload") || isSimpleCallName(expr, "readback") ||
      isSimpleCallName(expr, "buffer_load") ||
      isSimpleCallName(expr, "buffer_store")) {
    isBuiltin = true;
  }

  if (isBuiltin) {
    return failNamedArgumentDiagnostic("named arguments not supported for builtin calls");
  }
  return true;
}

} // namespace primec::semantics
