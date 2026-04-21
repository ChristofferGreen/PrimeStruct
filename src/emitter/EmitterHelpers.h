#pragma once

#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "primec/Emitter.h"

namespace primec::emitter {

using BindingInfo = Emitter::BindingInfo;
using ReturnKind = Emitter::ReturnKind;

enum class PrintTarget { Out, Err };

struct PrintBuiltin {
  PrintTarget target = PrintTarget::Out;
  bool newline = false;
  std::string name;
};

inline bool isCanonicalMapHelperName(std::string_view helperName) {
  return helperName == "count" || helperName == "count_ref" ||
         helperName == "contains" || helperName == "contains_ref" ||
         helperName == "tryAt" || helperName == "tryAt_ref" ||
         helperName == "at" || helperName == "at_ref" ||
         helperName == "at_unsafe" || helperName == "at_unsafe_ref" ||
         helperName == "insert" || helperName == "insert_ref";
}

inline bool isCanonicalMapCountHelperName(std::string_view helperName) {
  return helperName == "count" || helperName == "count_ref";
}

inline bool isCanonicalMapAccessHelperName(std::string_view helperName) {
  return helperName == "at" || helperName == "at_ref" ||
         helperName == "at_unsafe" || helperName == "at_unsafe_ref";
}

inline bool isRemovedMapSlashMethodMetadataHelperName(std::string_view helperName) {
  return helperName == "contains" || helperName == "contains_ref" ||
         helperName == "tryAt" || helperName == "tryAt_ref";
}

inline bool isRemovedMapDirectCallResultCompatibilityHelperName(std::string_view helperName) {
  return isRemovedMapSlashMethodMetadataHelperName(helperName) ||
         isCanonicalMapAccessHelperName(helperName);
}

inline std::string_view mapHelperNameFromPath(std::string_view path) {
  if (!path.empty() && path.front() == '/') {
    path.remove_prefix(1);
  }
  constexpr std::string_view kMapPrefix = "map/";
  constexpr std::string_view kCanonicalMapPrefix = "std/collections/map/";
  if (path.rfind(kMapPrefix, 0) == 0) {
    return path.substr(kMapPrefix.size());
  }
  if (path.rfind(kCanonicalMapPrefix, 0) == 0) {
    return path.substr(kCanonicalMapPrefix.size());
  }
  return {};
}

inline bool isCanonicalMapHelperPath(std::string_view path) {
  const std::string_view helperName = mapHelperNameFromPath(path);
  return !helperName.empty() && isCanonicalMapHelperName(helperName);
}

inline bool isCanonicalMapAccessHelperPath(std::string_view path) {
  const std::string_view helperName = mapHelperNameFromPath(path);
  return !helperName.empty() && isCanonicalMapAccessHelperName(helperName);
}

std::string joinTemplateArgs(const std::vector<std::string> &args);
ReturnKind getReturnKind(const Definition &def);
bool isPrimitiveBindingTypeName(const std::string &name);
std::string normalizeBindingTypeName(const std::string &name);
bool splitTemplateTypeName(const std::string &text, std::string &base, std::string &arg);
bool splitTopLevelTemplateArgs(const std::string &text, std::vector<std::string> &out);
bool isBindingQualifierName(const std::string &name);
bool isBindingAuxTransformName(const std::string &name);
bool hasExplicitBindingTypeTransform(const Expr &expr);
BindingInfo getBindingInfo(const Expr &expr);
bool isReferenceCandidate(const BindingInfo &info);
bool isAliasingBinding(const BindingInfo &info);
std::string bindingTypeToCpp(const BindingInfo &info);
std::string bindingTypeToCpp(const std::string &typeName);
std::string bindingTypeToCpp(const BindingInfo &info,
                             const std::string &namespacePrefix,
                             const std::unordered_map<std::string, std::string> &importAliases,
                             const std::unordered_map<std::string, std::string> &structTypeMap);
std::string bindingTypeToCpp(const std::string &typeName,
                             const std::string &namespacePrefix,
                             const std::unordered_map<std::string, std::string> &importAliases,
                             const std::unordered_map<std::string, std::string> &structTypeMap);
std::string resolveTypePath(const std::string &name, const std::string &namespacePrefix);
std::string typeNameForReturnKind(ReturnKind kind);
ReturnKind returnKindForTypeName(const std::string &name);
ReturnKind combineNumericKinds(ReturnKind left, ReturnKind right);
ReturnKind inferPrimitiveReturnKind(const Expr &expr,
                                   const std::unordered_map<std::string, BindingInfo> &localTypes,
                                   const std::unordered_map<std::string, ReturnKind> &returnKinds,
                                   bool allowMathBare);

bool getBuiltinOperator(const Expr &expr, char &out);
bool getBuiltinComparison(const Expr &expr, const char *&out);
bool getBuiltinMutationName(const Expr &expr, std::string &out);
bool isSimpleCallName(const Expr &expr, const char *nameToMatch);
bool getPrintBuiltin(const Expr &expr, PrintBuiltin &out);
bool isPathSpaceBuiltinName(const Expr &expr);
std::string stripStringLiteralSuffix(const std::string &token);
bool isBuiltinNegate(const Expr &expr);
bool isBuiltinClamp(const Expr &expr, bool allowBare);
bool getBuiltinMinMaxName(const Expr &expr, std::string &out, bool allowBare);
bool getBuiltinAbsSignName(const Expr &expr, std::string &out, bool allowBare);
bool getBuiltinSaturateName(const Expr &expr, std::string &out, bool allowBare);
bool getBuiltinMathName(const Expr &expr, std::string &out, bool allowBare);
bool isBuiltinMathConstantName(const std::string &name, bool allowBare);
bool getBuiltinPointerOperator(const Expr &expr, char &out);
bool getBuiltinMemoryName(const Expr &expr, std::string &out);
bool getBuiltinConvertName(const Expr &expr, std::string &out);
bool getBuiltinCollectionName(const Expr &expr, std::string &out);
bool inferCollectionElementTypeNameFromExpr(
    const Expr &expr,
    const std::unordered_map<std::string, BindingInfo> &localTypes,
    const std::function<bool(const Expr &, std::string &)> &resolveCallElementTypeName,
    std::string &typeOut);
std::string resolveExprPath(const Expr &expr);
std::string preferVectorStdlibHelperPath(const std::string &path,
                                         const std::unordered_map<std::string, std::string> &nameMap);
bool isArrayValue(const Expr &target, const std::unordered_map<std::string, BindingInfo> &localTypes);
bool isVectorValue(const Expr &target, const std::unordered_map<std::string, BindingInfo> &localTypes);
bool isMapValue(const Expr &target, const std::unordered_map<std::string, BindingInfo> &localTypes);
bool isStringValue(const Expr &target, const std::unordered_map<std::string, BindingInfo> &localTypes);
bool isArrayCountCall(const Expr &call, const std::unordered_map<std::string, BindingInfo> &localTypes);
bool isMapCountCall(const Expr &call, const std::unordered_map<std::string, BindingInfo> &localTypes);
bool isStringCountCall(const Expr &call, const std::unordered_map<std::string, BindingInfo> &localTypes);
bool isVectorCapacityCall(const Expr &call, const std::unordered_map<std::string, BindingInfo> &localTypes);
bool resolveMethodCallPath(const Expr &call,
                           const std::unordered_map<std::string, const Definition *> &defMap,
                           const std::unordered_map<std::string, BindingInfo> &localTypes,
                           const std::unordered_map<std::string, std::string> &importAliases,
                           const std::unordered_map<std::string, std::string> &structTypeMap,
                           const std::unordered_map<std::string, ReturnKind> &returnKinds,
                           const std::unordered_map<std::string, std::string> &returnStructs,
                           std::string &resolvedOut);
bool isBuiltinAssign(const Expr &expr, const std::unordered_map<std::string, std::string> &nameMap);
bool getVectorMutatorName(const Expr &expr,
                          const std::unordered_map<std::string, std::string> &nameMap,
                          std::string &out);
std::vector<const Expr *> orderCallArguments(const Expr &expr,
                                             const std::string &resolvedPath,
                                             const std::vector<Expr> &params,
                                             const std::unordered_map<std::string, BindingInfo> &localTypes);
bool isBuiltinIf(const Expr &expr, const std::unordered_map<std::string, std::string> &nameMap);
bool isBuiltinBlock(const Expr &expr, const std::unordered_map<std::string, std::string> &nameMap);
bool isLoopCall(const Expr &expr);
bool isWhileCall(const Expr &expr);
bool isForCall(const Expr &expr);
bool isRepeatCall(const Expr &expr);
bool hasNamedArguments(const std::vector<std::optional<std::string>> &argNames);
bool isReturnCall(const Expr &expr);

} // namespace primec::emitter
