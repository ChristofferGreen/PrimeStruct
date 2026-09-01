// collection-surface-audit: exempt
#include "SemanticsValidator.h"
#include "StdlibCollectionSurfaceHelpers.h"
#include "SemanticsValidatorInferCollectionCompatibilityInternal.h"
#include "SemanticsValidatorMethodTargetResolutionDetail.h"
#include "primec/support/CollectionSpellingClassifier.h"
#include "primec/support/StdlibSurfaceRegistry.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <sstream>
#include <string>
#include <string_view>

namespace primec::semantics {
using namespace method_target_detail;

bool SemanticsValidator::resolveIndexedArgsPackElementType(
    const Expr &target, std::string &elemTypeOut,
    const std::function<bool(const Expr &, std::string &)> &resolveArgsPackAccessTarget) const {
  elemTypeOut.clear();
  std::string accessName;
  if (target.kind != Expr::Kind::Call || !getBuiltinArrayAccessName(target, accessName) ||
      target.args.size() != 2) {
    return false;
  }
  const Expr *accessReceiver = resolveBuiltinAccessReceiverExpr(target);
  return accessReceiver != nullptr && resolveArgsPackAccessTarget(*accessReceiver, elemTypeOut);
}

bool SemanticsValidator::resolveDereferencedIndexedArgsPackElementType(
    const Expr &target, std::string &elemTypeOut,
    const std::function<bool(const Expr &, std::string &)> &resolveArgsPackAccessTarget) const {
  elemTypeOut.clear();
  if (!isSimpleCallName(target, "dereference") || target.args.size() != 1) {
    return false;
  }
  std::string wrappedType;
  return resolveIndexedArgsPackElementType(target.args.front(), wrappedType,
                                           resolveArgsPackAccessTarget) &&
         extractWrappedPointeeType(wrappedType, elemTypeOut);
}

bool SemanticsValidator::resolveWrappedIndexedArgsPackElementType(
    const Expr &target, std::string &elemTypeOut,
    const std::function<bool(const Expr &, std::string &)> &resolveArgsPackAccessTarget) const {
  elemTypeOut.clear();
  std::string wrappedType;
  return resolveIndexedArgsPackElementType(target, wrappedType, resolveArgsPackAccessTarget) &&
         extractWrappedPointeeType(wrappedType, elemTypeOut);
}

bool SemanticsValidator::extractCollectionElementType(const std::string &typeText,
                                                       const std::string &expectedBase,
                                                       std::string &elemTypeOut) const {
  elemTypeOut.clear();
  std::string base;
  std::string argText;
  if (!splitTemplateTypeName(normalizeBindingTypeName(typeText), base, argText)) {
    return false;
  }
  base = normalizeBindingTypeName(base);
  if (base != expectedBase) {
    return false;
  }
  std::vector<std::string> args;
  if (!splitTopLevelTemplateArgs(argText, args) || args.size() != 1) {
    return false;
  }
  elemTypeOut = args.front();
  return true;
}

bool SemanticsValidator::resolveArgsPackElementMethodTarget(
    const std::string &elementTypeText, const Expr &receiverExpr,
    const std::string &normalizedMethodName,
    const std::function<bool(const std::string &)> &setCollectionMethodTarget,
    const std::function<bool(const Expr &, const std::string &)>
        &setPreferredKeyValueMethodTarget,
    std::string &resolvedOut, bool &isBuiltinOut) {
  const std::string normalizedElemType = normalizeBindingTypeName(elementTypeText);
  std::string normalizedElemBaseType = normalizedElemType;
  if (!normalizedElemBaseType.empty() && normalizedElemBaseType.front() == '/') {
    normalizedElemBaseType.erase(normalizedElemBaseType.begin());
  }
  std::string collectionElemType = normalizedElemType;
  std::string wrappedPointeeType;
  if (extractWrappedPointeeType(normalizedElemType, wrappedPointeeType)) {
    collectionElemType = normalizeBindingTypeName(wrappedPointeeType);
  }
  if (collectionElemType == "string" || normalizedElemBaseType == "string") {
    return setCollectionMethodTarget("/string/" + normalizedMethodName);
  }
  if (collectionElemType == "FileError" &&
      (normalizedMethodName == "why" || normalizedMethodName == "is_eof" ||
       normalizedMethodName == "status" || normalizedMethodName == "result")) {
    resolvedOut = preferredFileErrorHelperTarget(normalizedMethodName);
    isBuiltinOut = resolvedOut == "/file_error/why";
    return !resolvedOut.empty();
  }
  std::string elemBase;
  std::string elemArgText;
  if (splitTemplateTypeName(collectionElemType, elemBase, elemArgText)) {
    elemBase = normalizeBindingTypeName(elemBase);
    if (elemBase == "vector" || elemBase == "array" ||
        isInternalSoaCollectionTypeName(elemBase)) {
      return setCollectionMethodTarget("/" + elemBase + "/" + normalizedMethodName);
    }
    if (elemBase == "Buffer" &&
        (normalizedMethodName == "count" || normalizedMethodName == "empty" ||
         normalizedMethodName == "is_valid" || normalizedMethodName == "readback" ||
         normalizedMethodName == "load" || normalizedMethodName == "store")) {
      return setCollectionMethodTarget(preferredBufferMethodTarget(normalizedMethodName));
    }
    if (isKeyValueSurfaceTypeName(elemBase)) {
      return setPreferredKeyValueMethodTarget(receiverExpr, normalizedMethodName);
    }
    if (elemBase == "File" && isFileMethodName(normalizedMethodName)) {
      resolvedOut = preferredFileHelperTarget(normalizedMethodName,
                                             currentValidationState_.context.definitionPath);
      isBuiltinOut = (resolvedOut.rfind("/file/", 0) == 0);
      return true;
    }
  }
  if (isPrimitiveBindingTypeName(normalizedElemBaseType)) {
    resolvedOut = "/" + normalizedElemBaseType + "/" + normalizedMethodName;
    return true;
  }
  std::string resolvedElemType =
      resolveMethodTargetStructTypePath(collectionElemType, receiverExpr.namespacePrefix);
  if (resolvedElemType.empty()) {
    resolvedElemType = resolveTypePath(collectionElemType, receiverExpr.namespacePrefix);
  }
  if (!resolvedElemType.empty()) {
    resolvedOut = resolvedElemType + "/" + normalizedMethodName;
    return true;
  }
  return false;
}

} // namespace primec::semantics
