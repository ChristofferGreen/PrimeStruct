#include "SemanticsValidator.h"

#include <algorithm>
#include <string>
#include <vector>

namespace primec::semantics {
namespace {

void appendUniqueReceiverIndex(std::vector<size_t> &receiverIndices, size_t index, size_t limit) {
  if (index >= limit) {
    return;
  }
  for (size_t existing : receiverIndices) {
    if (existing == index) {
      return;
    }
  }
  receiverIndices.push_back(index);
}

} // namespace

bool SemanticsValidator::resolveExprCollectionAccessTarget(
    const std::vector<ParameterInfo> &params,
    const std::unordered_map<std::string, BindingInfo> &locals,
    const Expr &expr,
    const ExprCollectionAccessDispatchContext &context,
    bool &handledOut,
    std::string &resolved,
    bool &resolvedMethod,
    bool &usedMethodTarget,
    bool &hasMethodReceiverIndex,
    size_t &methodReceiverIndex) {
  handledOut = false;

  std::string accessHelperName;
  const bool hasBuiltinAccessSpelling =
      !expr.isMethodCall && getBuiltinArrayAccessName(expr, accessHelperName);
  const bool isStdNamespacedVectorAccessCall =
      hasBuiltinAccessSpelling && !expr.isMethodCall &&
      resolveCalleePath(expr).rfind("/std/collections/vector/at", 0) == 0;
  const bool hasStdNamespacedVectorAccessDefinition =
      isStdNamespacedVectorAccessCall && hasImportedDefinitionPath(resolveCalleePath(expr));
  const bool isStdNamespacedMapAccessCall =
      hasBuiltinAccessSpelling && !expr.isMethodCall &&
      (resolveCalleePath(expr) == "/std/collections/map/at" ||
       resolveCalleePath(expr) == "/std/collections/map/at_unsafe");
  const bool prefersExplicitDirectMapAccessAliasDefinition =
      !expr.isMethodCall &&
      (((context.isNamespacedMapHelperCall &&
         (context.namespacedHelper == "at" || context.namespacedHelper == "at_unsafe")) ||
        (((expr.namespacePrefix == "map") || (expr.namespacePrefix == "/map")) &&
         (expr.name == "at" || expr.name == "at_unsafe")))) &&
      defMap_.find("/map/" +
                   ((expr.name == "at" || expr.name == "at_unsafe") ? expr.name : context.namespacedHelper)) !=
          defMap_.end();
  if (prefersExplicitDirectMapAccessAliasDefinition) {
    resolved = "/map/" + ((expr.name == "at" || expr.name == "at_unsafe") ? expr.name : context.namespacedHelper);
  }
  const bool isResolvedMapAccessCall =
      !expr.isMethodCall && (resolved == "/map/at" || resolved == "/map/at_unsafe") &&
      !isMapNamespacedAccessCompatibilityCall(expr);
  const bool isBuiltinAccessName =
      hasBuiltinAccessSpelling &&
      (!isStdNamespacedVectorAccessCall || hasStdNamespacedVectorAccessDefinition) &&
      !isStdNamespacedMapAccessCall && !isResolvedMapAccessCall;
  const bool isNamespacedVectorAccessCall =
      !isStdNamespacedVectorAccessCall && isBuiltinAccessName &&
      context.isNamespacedVectorHelperCall &&
      (context.namespacedHelper == "at" || context.namespacedHelper == "at_unsafe") &&
      defMap_.find(resolved) == defMap_.end();
  const bool isNamespacedMapAccessCall =
      isBuiltinAccessName && context.isNamespacedMapHelperCall &&
      (context.namespacedHelper == "at" || context.namespacedHelper == "at_unsafe") &&
      !prefersExplicitDirectMapAccessAliasDefinition &&
      defMap_.find(resolved) == defMap_.end();

  if (isBuiltinAccessName &&
      !(isStdNamespacedVectorAccessCall && hasNamedArguments(expr.argNames)) &&
      (defMap_.find(resolved) == defMap_.end() || isNamespacedVectorAccessCall ||
       isNamespacedMapAccessCall)) {
    handledOut = true;
    std::vector<size_t> receiverIndices;
    const bool hasNamedArgs = hasNamedArguments(expr.argNames);
    if (hasNamedArgs) {
      bool hasValuesNamedReceiver = false;
      for (size_t i = 0; i < expr.args.size(); ++i) {
        if (i < expr.argNames.size() && expr.argNames[i].has_value() && *expr.argNames[i] == "values") {
          appendUniqueReceiverIndex(receiverIndices, i, expr.args.size());
          hasValuesNamedReceiver = true;
        }
      }
      if (!hasValuesNamedReceiver) {
        appendUniqueReceiverIndex(receiverIndices, 0, expr.args.size());
        for (size_t i = 1; i < expr.args.size(); ++i) {
          appendUniqueReceiverIndex(receiverIndices, i, expr.args.size());
        }
      }
    } else {
      appendUniqueReceiverIndex(receiverIndices, 0, expr.args.size());
    }

    auto isCollectionAccessReceiverExpr = [&](const Expr &candidate) -> bool {
      std::string elemType;
      return context.resolveVectorTarget(candidate, elemType) ||
             context.resolveArrayTarget(candidate, elemType) ||
             context.resolveStringTarget(candidate) ||
             context.resolveMapTarget(candidate);
    };
    const bool probePositionalReorderedReceiver =
        !hasNamedArgs && expr.args.size() > 1 &&
        (expr.args.front().kind == Expr::Kind::Literal ||
         expr.args.front().kind == Expr::Kind::BoolLiteral ||
         expr.args.front().kind == Expr::Kind::FloatLiteral ||
         expr.args.front().kind == Expr::Kind::StringLiteral ||
         (expr.args.front().kind == Expr::Kind::Name &&
          !isCollectionAccessReceiverExpr(expr.args.front())));
    if (probePositionalReorderedReceiver) {
      for (size_t i = 1; i < expr.args.size(); ++i) {
        appendUniqueReceiverIndex(receiverIndices, i, expr.args.size());
      }
    }
    const bool hasAlternativeCollectionReceiver =
        probePositionalReorderedReceiver &&
        std::any_of(receiverIndices.begin(), receiverIndices.end(), [&](size_t index) {
          if (index == 0 || index >= expr.args.size()) {
            return false;
          }
          const Expr &candidate = expr.args[index];
          std::string elemType;
          return context.resolveVectorTarget(candidate, elemType) ||
                 context.resolveArrayTarget(candidate, elemType) ||
                 context.resolveStringTarget(candidate) ||
                 context.resolveMapTarget(candidate);
        });
    for (size_t receiverIndex : receiverIndices) {
      const Expr &receiverCandidate = expr.args[receiverIndex];
      std::string elemType;
      if (!(context.resolveVectorTarget(receiverCandidate, elemType) ||
            context.resolveArrayTarget(receiverCandidate, elemType) ||
            context.resolveStringTarget(receiverCandidate) ||
            context.resolveMapTarget(receiverCandidate))) {
        continue;
      }
      usedMethodTarget = true;
      bool isBuiltinMethod = false;
      std::string methodResolved;
      if (!resolveMethodTarget(params, locals, expr.namespacePrefix, receiverCandidate, accessHelperName,
                               methodResolved, isBuiltinMethod)) {
        (void)validateExpr(params, locals, receiverCandidate);
        return false;
      }
      if (!isBuiltinMethod && defMap_.find(methodResolved) == defMap_.end()) {
        error_ = "unknown method: " + methodResolved;
        return false;
      }
      if (hasAlternativeCollectionReceiver && receiverIndex == 0) {
        continue;
      }
      resolved = methodResolved;
      resolvedMethod = isBuiltinMethod;
      hasMethodReceiverIndex = true;
      methodReceiverIndex = receiverIndex;
      break;
    }
    if (!hasMethodReceiverIndex && !expr.args.empty() &&
        (expr.args.front().kind == Expr::Kind::Name || expr.args.front().kind == Expr::Kind::Call)) {
      bool isBuiltinMethod = false;
      std::string methodResolved;
      if (resolveMethodTarget(params, locals, expr.namespacePrefix, expr.args.front(), accessHelperName,
                              methodResolved, isBuiltinMethod)) {
        if (isBuiltinMethod) {
          usedMethodTarget = true;
          hasMethodReceiverIndex = true;
          methodReceiverIndex = 0;
          resolved = methodResolved;
          resolvedMethod = true;
        } else {
          const size_t methodSlash = methodResolved.find_last_of('/');
          const bool hasStructReceiver =
              methodSlash != std::string::npos && methodSlash > 0 &&
              structNames_.count(methodResolved.substr(0, methodSlash)) > 0;
          if (hasStructReceiver) {
            usedMethodTarget = true;
            hasMethodReceiverIndex = true;
            methodReceiverIndex = 0;
            if (defMap_.find(methodResolved) == defMap_.end()) {
              error_ = "unknown method: " + methodResolved;
              return false;
            }
            resolved = methodResolved;
            resolvedMethod = false;
          }
        }
      }
    }
    return true;
  }

  if ((isSimpleCallName(expr, "get") || isSimpleCallName(expr, "ref")) &&
      expr.args.size() == 2 && defMap_.find(resolved) == defMap_.end()) {
    handledOut = true;
    std::vector<size_t> receiverIndices;
    const bool hasNamedArgs = hasNamedArguments(expr.argNames);
    if (hasNamedArgs) {
      bool hasValuesNamedReceiver = false;
      for (size_t i = 0; i < expr.args.size(); ++i) {
        if (i < expr.argNames.size() && expr.argNames[i].has_value() && *expr.argNames[i] == "values") {
          appendUniqueReceiverIndex(receiverIndices, i, expr.args.size());
          hasValuesNamedReceiver = true;
        }
      }
      if (!hasValuesNamedReceiver) {
        appendUniqueReceiverIndex(receiverIndices, 0, expr.args.size());
        for (size_t i = 1; i < expr.args.size(); ++i) {
          appendUniqueReceiverIndex(receiverIndices, i, expr.args.size());
        }
      }
    } else {
      appendUniqueReceiverIndex(receiverIndices, 0, expr.args.size());
    }

    for (size_t receiverIndex : receiverIndices) {
      const Expr &receiverCandidate = expr.args[receiverIndex];
      std::string methodTarget;
      if (resolveVectorHelperMethodTarget(params, locals, receiverCandidate, expr.name, methodTarget)) {
        methodTarget = preferVectorStdlibHelperPath(methodTarget);
        if (defMap_.count(methodTarget) > 0) {
          usedMethodTarget = true;
          resolved = methodTarget;
          resolvedMethod = false;
          hasMethodReceiverIndex = true;
          methodReceiverIndex = receiverIndex;
          break;
        }
      }
      std::string elemType;
      if (!context.resolveSoaVectorTarget(receiverCandidate, elemType)) {
        continue;
      }
      usedMethodTarget = true;
      bool isBuiltinMethod = false;
      std::string methodResolved;
      if (!resolveMethodTarget(params, locals, expr.namespacePrefix, receiverCandidate, expr.name,
                               methodResolved, isBuiltinMethod)) {
        (void)validateExpr(params, locals, receiverCandidate);
        return false;
      }
      if (!isBuiltinMethod && defMap_.find(methodResolved) == defMap_.end()) {
        error_ = "unknown method: " + methodResolved;
        return false;
      }
      resolved = methodResolved;
      resolvedMethod = isBuiltinMethod;
      hasMethodReceiverIndex = true;
      methodReceiverIndex = receiverIndex;
      break;
    }
    return true;
  }

  if (expr.args.size() == 1 && defMap_.find(resolved) == defMap_.end() &&
      !isSimpleCallName(expr, "to_soa") && !isSimpleCallName(expr, "to_aos")) {
    handledOut = true;
    const Expr &receiverCandidate = expr.args.front();
    std::string elemType;
    if (context.resolveSoaVectorTarget(receiverCandidate, elemType)) {
      usedMethodTarget = true;
      bool isBuiltinMethod = false;
      std::string methodResolved;
      if (!resolveMethodTarget(params, locals, expr.namespacePrefix, receiverCandidate, expr.name,
                               methodResolved, isBuiltinMethod)) {
        (void)validateExpr(params, locals, receiverCandidate);
        return false;
      }
      if (!isBuiltinMethod && defMap_.find(methodResolved) == defMap_.end()) {
        error_ = "unknown method: " + methodResolved;
        return false;
      }
      resolved = methodResolved;
      resolvedMethod = isBuiltinMethod;
      hasMethodReceiverIndex = true;
      methodReceiverIndex = 0;
    }
    return true;
  }

  if (!expr.isMethodCall && expr.args.size() == 2 &&
      defMap_.find(resolved) == defMap_.end() &&
      (isSimpleCallName(expr, "contains") || getBuiltinArrayAccessName(expr, accessHelperName))) {
    handledOut = true;
    const Expr &receiverCandidate = expr.args.front();
    if (context.resolveMapTarget(receiverCandidate)) {
      usedMethodTarget = true;
      bool isBuiltinMethod = false;
      std::string methodResolved;
      if (!resolveMethodTarget(params, locals, expr.namespacePrefix, receiverCandidate, expr.name,
                               methodResolved, isBuiltinMethod)) {
        (void)validateExpr(params, locals, receiverCandidate);
        return false;
      }
      if (isBuiltinMethod) {
        if (((methodResolved == "/std/collections/map/contains" &&
              (hasDeclaredDefinitionPath("/map/contains") ||
               hasDeclaredDefinitionPath("/std/collections/map/contains"))) ||
             (methodResolved == "/std/collections/map/at" &&
              (hasDeclaredDefinitionPath("/map/at") ||
               hasDeclaredDefinitionPath("/std/collections/map/at"))) ||
             (methodResolved == "/std/collections/map/at_unsafe" &&
              (hasDeclaredDefinitionPath("/map/at_unsafe") ||
               hasDeclaredDefinitionPath("/std/collections/map/at_unsafe")))) &&
            !(methodResolved == "/std/collections/map/contains" &&
              context.shouldBuiltinValidateBareMapContainsCall) &&
            !((methodResolved == "/std/collections/map/at" ||
               methodResolved == "/std/collections/map/at_unsafe") &&
              context.shouldBuiltinValidateBareMapAccessCall) &&
            !context.isIndexedArgsPackMapReceiverTarget(receiverCandidate)) {
          isBuiltinMethod = false;
        }
      }
      if (isBuiltinMethod) {
        if (methodResolved == "/std/collections/map/contains" &&
            !context.shouldBuiltinValidateBareMapContainsCall &&
            !context.isIndexedArgsPackMapReceiverTarget(receiverCandidate) &&
            !hasDeclaredDefinitionPath("/map/contains") &&
            !hasImportedDefinitionPath("/std/collections/map/contains") &&
            !hasDeclaredDefinitionPath("/std/collections/map/contains")) {
          error_ = "unknown call target: /std/collections/map/contains";
          return false;
        }
        if ((methodResolved == "/map/at" || methodResolved == "/map/at_unsafe" ||
             methodResolved == "/std/collections/map/at" ||
             methodResolved == "/std/collections/map/at_unsafe") &&
            !context.shouldBuiltinValidateBareMapAccessCall &&
            !context.isIndexedArgsPackMapReceiverTarget(receiverCandidate) &&
            !hasDeclaredDefinitionPath("/map/" +
                                       std::string(methodResolved.find("unsafe") != std::string::npos ? "at_unsafe"
                                                                                                       : "at")) &&
            !hasImportedDefinitionPath("/std/collections/map/" +
                                       std::string(methodResolved.find("unsafe") != std::string::npos ? "at_unsafe"
                                                                                                       : "at")) &&
            !hasDeclaredDefinitionPath("/std/collections/map/" +
                                       std::string(methodResolved.find("unsafe") != std::string::npos ? "at_unsafe"
                                                                                                       : "at"))) {
          error_ = "unknown call target: /std/collections/map/" +
                   std::string(methodResolved.find("unsafe") != std::string::npos ? "at_unsafe" : "at");
          return false;
        }
      } else if (defMap_.find(methodResolved) == defMap_.end() &&
                 !context.hasResolvableMapHelperPath(methodResolved)) {
        error_ = "unknown method: " + methodResolved;
        return false;
      }
      resolved = methodResolved;
      resolvedMethod = isBuiltinMethod;
      hasMethodReceiverIndex = true;
      methodReceiverIndex = 0;
    }
    return true;
  }

  return true;
}

} // namespace primec::semantics
