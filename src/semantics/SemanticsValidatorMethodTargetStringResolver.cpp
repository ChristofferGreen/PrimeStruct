// collection-surface-audit: exempt
#include "SemanticsValidator.h"
#include "StdlibCollectionSurfaceHelpers.h"
#include "SemanticsValidatorInferCollectionCompatibilityInternal.h"
#include "primec/support/CollectionSpellingClassifier.h"
#include "primec/support/StdlibSurfaceRegistry.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <sstream>
#include <string>
#include <string_view>

namespace primec::semantics {
bool SemanticsValidator::resolveStringTarget(
    const Expr &target, const std::vector<ParameterInfo> &params,
    const std::unordered_map<std::string, BindingInfo> &locals,
    const std::function<bool(const Expr &, std::string &)> &resolveArgsPackAccessTarget) {
  if (target.kind == Expr::Kind::StringLiteral) {
    return true;
  }
  if (target.kind == Expr::Kind::Name) {
    if (const BindingInfo *paramBinding = findParamBinding(params, target.name)) {
      return paramBinding->typeName == "string";
    }
    auto it = locals.find(target.name);
    return it != locals.end() && it->second.typeName == "string";
  }
  BindingInfo fieldBinding;
  if (resolveFieldBindingTarget(params, locals, target, fieldBinding)) {
    return fieldBinding.typeName == "string";
  }
  if (target.kind == Expr::Kind::Call) {
    std::string collectionTypePath;
    if (resolveCallCollectionTypePath(target, params, locals, collectionTypePath) &&
        collectionTypePath == "/string") {
      return true;
    }
    if (target.isMethodCall && target.name == "why" && !target.args.empty()) {
      const Expr &receiverExpr = target.args.front();
      if (receiverExpr.kind == Expr::Kind::Name) {
        if (const BindingInfo *paramBinding = findParamBinding(params, receiverExpr.name)) {
          if (normalizeBindingTypeName(paramBinding->typeName) == "FileError") {
            return true;
          }
        } else if (auto it = locals.find(receiverExpr.name); it != locals.end()) {
          if (normalizeBindingTypeName(it->second.typeName) == "FileError") {
            return true;
          }
        }
        if (receiverExpr.name == "Result") {
          return true;
        }
      }
      std::string elemType;
      if ((resolveIndexedArgsPackElementType(receiverExpr, elemType, resolveArgsPackAccessTarget) ||
           resolveDereferencedIndexedArgsPackElementType(receiverExpr, elemType,
                                                          resolveArgsPackAccessTarget)) &&
          normalizeBindingTypeName(unwrapReferencePointerTypeText(elemType)) == "FileError") {
        return true;
      }
    }
    std::string builtinName;
    if (getBuiltinArrayAccessName(target, builtinName) && target.args.size() == 2) {
      if (const Expr *accessReceiver = resolveBuiltinAccessReceiverExpr(target)) {
        std::string elemType;
        std::string keyValueValueType;
        if (resolveArgsPackAccessTarget(*accessReceiver, elemType) ||
            resolveArrayTarget(*accessReceiver, elemType, params, locals,
                               resolveArgsPackAccessTarget) ||
            resolveVectorTarget(*accessReceiver, elemType, params, locals,
                                resolveArgsPackAccessTarget)) {
          return normalizeBindingTypeName(elemType) == "string";
        }
        if (resolveMethodTargetKeyValueValueType(*accessReceiver, keyValueValueType, params, locals,
                                                  resolveArgsPackAccessTarget)) {
          return normalizeBindingTypeName(keyValueValueType) == "string";
        }
        if (resolveStringTarget(*accessReceiver, params, locals, resolveArgsPackAccessTarget)) {
          return false;
        }
      }
    }
  }
  return inferExprReturnKind(target, params, locals) == ReturnKind::String;
}

} // namespace primec::semantics
