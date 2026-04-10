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

bool isVectorFamilyHelperPath(const std::string &path) {
  return path.rfind("/vector/", 0) == 0 ||
         path.rfind("/soa_vector/", 0) == 0 ||
         path.rfind("/std/collections/soa_vector/", 0) == 0 ||
         path.rfind("/std/collections/vector/", 0) == 0 ||
         path.rfind("/std/collections/experimental_vector/", 0) == 0;
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
  const size_t methodSlash = normalizedMethodName.find_last_of('/');
  if (methodSlash != std::string::npos) {
    normalizedMethodName = normalizedMethodName.substr(methodSlash + 1);
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
  if (expr.isMethodCall &&
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
        rewrittenVectorMethodCall.name = normalizedMethodName;
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
        rewrittenVectorMethodCall.name = normalizedMethodName;
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
  if (splitSoaFieldViewHelperPath(resolvedTarget)) {
    handledOut = true;
    return failLateUnknownTargetDiagnostic(
        soaDirectPendingUnavailableMethodDiagnostic(resolvedTarget));
  }

  handledOut = true;
  return failLateUnknownTargetDiagnostic("unknown call target: " +
                                         formatUnknownCallTarget(expr));
}

} // namespace primec::semantics
