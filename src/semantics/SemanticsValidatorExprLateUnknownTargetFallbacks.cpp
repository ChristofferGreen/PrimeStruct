#include "SemanticsValidator.h"

#include <functional>
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

bool isCanonicalVectorAccessMethodHelper(std::string_view helperName) {
  return helperName == "at" || helperName == "at_unsafe";
}

bool isExplicitVectorCompatibilityMethodNamespace(std::string_view namespacePrefix) {
  return namespacePrefix == "vector" ||
         namespacePrefix == "std/collections/vector";
}

bool isVectorFamilyHelperPath(const std::string &path) {
  return path.rfind("/soa_vector/", 0) == 0 ||
         path.rfind("/std/collections/soa_vector/", 0) == 0 ||
         path.rfind("/std/collections/vector/", 0) == 0 ||
         path.rfind("/std/collections/experimental_vector/", 0) == 0;
}

std::string_view normalizeFileLateFallbackMethodName(std::string_view methodName) {
  if (methodName == "readByte") {
    return "read_byte";
  }
  if (methodName == "writeLine") {
    return "write_line";
  }
  if (methodName == "writeByte") {
    return "write_byte";
  }
  if (methodName == "writeBytes") {
    return "write_bytes";
  }
  return methodName;
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
  const std::string resolvedTarget = resolveCalleePath(expr);
  if (!expr.isMethodCall && resolvedTarget == "/file_error/why") {
    handledOut = true;
    if (hasNamedArguments(expr.argNames)) {
      return failLateUnknownTargetDiagnostic("named arguments not supported for builtin calls");
    }
    if (!expr.templateArgs.empty()) {
      return failLateUnknownTargetDiagnostic("why does not accept template arguments");
    }
    if (expr.hasBodyArguments || !expr.bodyArguments.empty()) {
      return failLateUnknownTargetDiagnostic("why does not accept block arguments");
    }
    if (expr.args.size() != 1) {
      return failLateUnknownTargetDiagnostic("why does not accept arguments");
    }
    return validateExpr(params, locals, expr.args.front());
  }
  std::string normalizedMethodName = expr.name;
  if (!normalizedMethodName.empty() && normalizedMethodName.front() == '/') {
    normalizedMethodName.erase(normalizedMethodName.begin());
  }
  const size_t methodSlash = normalizedMethodName.find_last_of('/');
  if (methodSlash != std::string::npos) {
    normalizedMethodName = normalizedMethodName.substr(methodSlash + 1);
  }
  std::string normalizedMethodNamespace = expr.namespacePrefix;
  if (!normalizedMethodNamespace.empty() &&
      normalizedMethodNamespace.front() == '/') {
    normalizedMethodNamespace.erase(normalizedMethodNamespace.begin());
  }
  const bool requestsExplicitVectorCompatibilityMethod =
      isExplicitVectorCompatibilityMethodNamespace(
          normalizedMethodNamespace) ||
      expr.name.rfind("/vector/", 0) == 0 ||
      expr.name.rfind("/std/collections/vector/", 0) == 0;
  if (context.resolveMapTarget != nullptr && expr.isMethodCall &&
      !requestsExplicitVectorCompatibilityMethod &&
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
  if (expr.isMethodCall &&
      !requestsExplicitVectorCompatibilityMethod &&
      isCanonicalVectorAccessMethodHelper(normalizedMethodName) &&
      !expr.args.empty()) {
    const Expr &receiverExpr = expr.args.front();
    const bool isMapReceiver =
        context.resolveMapTarget != nullptr &&
        context.resolveMapTarget(receiverExpr);
    if (!isMapReceiver) {
      std::string vectorMethodTarget;
      if (resolveVectorHelperMethodTarget(params, locals, receiverExpr,
                                          normalizedMethodName,
                                          vectorMethodTarget) &&
          isVectorFamilyHelperPath(vectorMethodTarget)) {
        Expr rewrittenVectorMethodCall = expr;
        rewrittenVectorMethodCall.isMethodCall = false;
        rewrittenVectorMethodCall.namespacePrefix.clear();
        rewrittenVectorMethodCall.name = vectorMethodTarget;
        handledOut = true;
        return validateExpr(params, locals, rewrittenVectorMethodCall);
      }
      bool hasCollectionReceiver = false;
      std::string receiverCollectionTypePath;
      if (resolveCallCollectionTypePath(receiverExpr, params, locals,
                                        receiverCollectionTypePath)) {
        hasCollectionReceiver = receiverCollectionTypePath == "/vector" ||
                                receiverCollectionTypePath == "/array" ||
                                receiverCollectionTypePath == "/string" ||
                                receiverCollectionTypePath == "/soa_vector";
      }
      if (!hasCollectionReceiver) {
        std::string receiverTypeText;
        if (inferQueryExprTypeText(receiverExpr, params, locals,
                                   receiverTypeText)) {
          const std::string normalizedCollectionType =
              normalizeCollectionTypePath(receiverTypeText);
          hasCollectionReceiver = normalizedCollectionType == "/vector" ||
                                  normalizedCollectionType == "/array" ||
                                  normalizedCollectionType == "/string" ||
                                  normalizedCollectionType == "/soa_vector";
        }
      }
      if (!hasCollectionReceiver) {
        const ReturnKind receiverKind =
            inferExprReturnKind(receiverExpr, params, locals);
        hasCollectionReceiver =
            receiverKind == ReturnKind::Array || receiverKind == ReturnKind::String;
      }
      if (!hasCollectionReceiver && receiverExpr.kind == Expr::Kind::Name) {
        if (const BindingInfo *paramBinding =
                findParamBinding(params, receiverExpr.name)) {
          hasCollectionReceiver = primec::semantics::isArgsPackBinding(*paramBinding);
        } else {
          auto localIt = locals.find(receiverExpr.name);
          if (localIt != locals.end()) {
            hasCollectionReceiver = primec::semantics::isArgsPackBinding(localIt->second);
          }
        }
      }
      if (hasCollectionReceiver) {
        Expr rewrittenVectorMethodCall = expr;
        rewrittenVectorMethodCall.isMethodCall = false;
        rewrittenVectorMethodCall.namespacePrefix.clear();
        rewrittenVectorMethodCall.name =
            preferredBareVectorHelperTarget(normalizedMethodName);
        handledOut = true;
        return validateExpr(params, locals, rewrittenVectorMethodCall);
      }
    }
  }

  if (!expr.isMethodCall && expr.args.size() == 2 &&
      expr.name.find('/') == std::string::npos &&
      (normalizedMethodName == "get" || normalizedMethodName == "get_ref" ||
       normalizedMethodName == "ref" || normalizedMethodName == "ref_ref")) {
    const std::string samePathHelper = "/soa_vector/" + normalizedMethodName;
    if (hasVisibleDefinitionPathForCurrentImports(samePathHelper)) {
      std::function<bool(const Expr &)> isVectorOrSoaLikeReceiver =
          [&](const Expr &receiverExpr) {
        if (receiverExpr.kind == Expr::Kind::Call && !receiverExpr.isBinding &&
            isSimpleCallName(receiverExpr, "location") &&
            receiverExpr.args.size() == 1) {
          return isVectorOrSoaLikeReceiver(receiverExpr.args.front());
        }
        if (receiverExpr.kind == Expr::Kind::Call && !receiverExpr.isBinding &&
            isSimpleCallName(receiverExpr, "dereference") &&
            receiverExpr.args.size() == 1) {
          return isVectorOrSoaLikeReceiver(receiverExpr.args.front());
        }
        std::string collectionTypePath;
        if (resolveCallCollectionTypePath(receiverExpr, params, locals,
                                          collectionTypePath)) {
          if (collectionTypePath == "/vector" ||
              collectionTypePath == "/soa_vector") {
            return true;
          }
        }
        std::string receiverTypeText;
        if (!inferQueryExprTypeText(receiverExpr, params, locals,
                                    receiverTypeText)) {
          return false;
        }
        std::string normalizedReceiverType =
            normalizeBindingTypeName(receiverTypeText);
        const std::string directCollectionType =
            normalizeCollectionTypePath(normalizedReceiverType);
        if (directCollectionType == "/vector" ||
            directCollectionType == "/soa_vector") {
          return true;
        }
        std::string base;
        std::string argText;
        if (splitTemplateTypeName(normalizedReceiverType, base, argText) &&
            (normalizeBindingTypeName(base) == "Reference" ||
             normalizeBindingTypeName(base) == "Pointer")) {
          const std::string pointeeCollectionType =
              normalizeCollectionTypePath(argText);
          return pointeeCollectionType == "/vector" ||
                 pointeeCollectionType == "/soa_vector";
        }
        return false;
      };
      if (isVectorOrSoaLikeReceiver(expr.args.front())) {
        Expr rewrittenSoaHelperCall = expr;
        rewrittenSoaHelperCall.isMethodCall = false;
        rewrittenSoaHelperCall.namespacePrefix.clear();
        rewrittenSoaHelperCall.name = samePathHelper;
        handledOut = true;
        return validateExpr(params, locals, rewrittenSoaHelperCall);
      }
    }
  }

  if (splitSoaFieldViewHelperPath(resolvedTarget)) {
    handledOut = true;
    return failLateUnknownTargetDiagnostic(
        soaUnavailableMethodDiagnostic(resolvedTarget));
  }

  const std::string normalizedFileMethodName =
      std::string(normalizeFileLateFallbackMethodName(expr.name));
  if (expr.isMethodCall &&
      (normalizedFileMethodName == "write" || normalizedFileMethodName == "write_line" ||
       normalizedFileMethodName == "write_byte" || normalizedFileMethodName == "read_byte" ||
       normalizedFileMethodName == "write_bytes" || normalizedFileMethodName == "flush" ||
       normalizedFileMethodName == "close") &&
      !expr.args.empty()) {
    auto normalizedTypeLeafName = [](std::string value) {
      value = normalizeBindingTypeName(value);
      std::string base;
      std::string argText;
      if (splitTemplateTypeName(value, base, argText) && !base.empty()) {
        value = base;
      }
      if (!value.empty() && value.front() == '/') {
        value.erase(value.begin());
      }
      const size_t slash = value.find_last_of('/');
      return slash == std::string::npos ? value : value.substr(slash + 1);
    };
    std::string receiverTypeText;
    if (inferQueryExprTypeText(expr.args.front(), params, locals, receiverTypeText) &&
        normalizedTypeLeafName(receiverTypeText) == "File") {
      Expr rewrittenFileBuiltin = expr;
      rewrittenFileBuiltin.isMethodCall = false;
      rewrittenFileBuiltin.namespacePrefix.clear();
      rewrittenFileBuiltin.name = "/file/" + normalizedFileMethodName;
      handledOut = true;
      return validateExpr(params, locals, rewrittenFileBuiltin);
    }
  }

  handledOut = true;
  return failLateUnknownTargetDiagnostic("unknown call target: " +
                                         formatUnknownCallTarget(expr));
}

} // namespace primec::semantics
