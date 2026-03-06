#include "IrLowererSetupTypeHelpers.h"

#include "IrLowererHelpers.h"

namespace primec::ir_lowerer {

SetupTypeAdapters makeSetupTypeAdapters() {
  SetupTypeAdapters adapters;
  adapters.valueKindFromTypeName = makeValueKindFromTypeName();
  adapters.combineNumericKinds = makeCombineNumericKinds();
  return adapters;
}

ValueKindFromTypeNameFn makeValueKindFromTypeName() {
  return [](const std::string &name) {
    return valueKindFromTypeName(name);
  };
}

CombineNumericKindsFn makeCombineNumericKinds() {
  return [](LocalInfo::ValueKind left, LocalInfo::ValueKind right) {
    return combineNumericKinds(left, right);
  };
}

LocalInfo::ValueKind valueKindFromTypeName(const std::string &name) {
  if (name == "int" || name == "i32") {
    return LocalInfo::ValueKind::Int32;
  }
  if (name == "i64") {
    return LocalInfo::ValueKind::Int64;
  }
  if (name == "u64") {
    return LocalInfo::ValueKind::UInt64;
  }
  if (name == "float" || name == "f32") {
    return LocalInfo::ValueKind::Float32;
  }
  if (name == "f64") {
    return LocalInfo::ValueKind::Float64;
  }
  if (name == "bool") {
    return LocalInfo::ValueKind::Bool;
  }
  if (name == "string") {
    return LocalInfo::ValueKind::String;
  }
  if (name == "FileError") {
    return LocalInfo::ValueKind::Int32;
  }

  std::string base;
  std::string arg;
  if (splitTemplateTypeName(name, base, arg) && base == "File") {
    return LocalInfo::ValueKind::Int64;
  }
  return LocalInfo::ValueKind::Unknown;
}

LocalInfo::ValueKind combineNumericKinds(LocalInfo::ValueKind left, LocalInfo::ValueKind right) {
  if (left == LocalInfo::ValueKind::Unknown || right == LocalInfo::ValueKind::Unknown) {
    return LocalInfo::ValueKind::Unknown;
  }
  if (left == LocalInfo::ValueKind::String || right == LocalInfo::ValueKind::String) {
    return LocalInfo::ValueKind::Unknown;
  }
  if (left == LocalInfo::ValueKind::Bool || right == LocalInfo::ValueKind::Bool) {
    return LocalInfo::ValueKind::Unknown;
  }
  if (left == LocalInfo::ValueKind::Float32 || right == LocalInfo::ValueKind::Float32 ||
      left == LocalInfo::ValueKind::Float64 || right == LocalInfo::ValueKind::Float64) {
    if (left == LocalInfo::ValueKind::Float32 && right == LocalInfo::ValueKind::Float32) {
      return LocalInfo::ValueKind::Float32;
    }
    if (left == LocalInfo::ValueKind::Float64 && right == LocalInfo::ValueKind::Float64) {
      return LocalInfo::ValueKind::Float64;
    }
    return LocalInfo::ValueKind::Unknown;
  }
  if (left == LocalInfo::ValueKind::UInt64 || right == LocalInfo::ValueKind::UInt64) {
    return (left == LocalInfo::ValueKind::UInt64 && right == LocalInfo::ValueKind::UInt64)
               ? LocalInfo::ValueKind::UInt64
               : LocalInfo::ValueKind::Unknown;
  }
  if (left == LocalInfo::ValueKind::Int64 || right == LocalInfo::ValueKind::Int64) {
    if ((left == LocalInfo::ValueKind::Int64 || left == LocalInfo::ValueKind::Int32) &&
        (right == LocalInfo::ValueKind::Int64 || right == LocalInfo::ValueKind::Int32)) {
      return LocalInfo::ValueKind::Int64;
    }
    return LocalInfo::ValueKind::Unknown;
  }
  if (left == LocalInfo::ValueKind::Int32 && right == LocalInfo::ValueKind::Int32) {
    return LocalInfo::ValueKind::Int32;
  }
  return LocalInfo::ValueKind::Unknown;
}

LocalInfo::ValueKind comparisonKind(LocalInfo::ValueKind left, LocalInfo::ValueKind right) {
  if (left == LocalInfo::ValueKind::Bool) {
    left = LocalInfo::ValueKind::Int32;
  }
  if (right == LocalInfo::ValueKind::Bool) {
    right = LocalInfo::ValueKind::Int32;
  }
  return combineNumericKinds(left, right);
}

std::string typeNameForValueKind(LocalInfo::ValueKind kind) {
  switch (kind) {
    case LocalInfo::ValueKind::Int32:
      return "i32";
    case LocalInfo::ValueKind::Int64:
      return "i64";
    case LocalInfo::ValueKind::UInt64:
      return "u64";
    case LocalInfo::ValueKind::Float32:
      return "f32";
    case LocalInfo::ValueKind::Float64:
      return "f64";
    case LocalInfo::ValueKind::Bool:
      return "bool";
    case LocalInfo::ValueKind::String:
      return "string";
    default:
      return "";
  }
}

bool resolveMethodCallReceiverExpr(const Expr &callExpr,
                                   const LocalMap &localsIn,
                                   const IsMethodCallClassifierFn &isArrayCountCall,
                                   const IsMethodCallClassifierFn &isVectorCapacityCall,
                                   const IsMethodCallClassifierFn &isEntryArgsName,
                                   const Expr *&receiverOut,
                                   std::string &errorOut) {
  receiverOut = nullptr;

  if (callExpr.kind != Expr::Kind::Call || callExpr.isBinding || !callExpr.isMethodCall) {
    return false;
  }
  if (callExpr.args.empty()) {
    errorOut = "method call missing receiver";
    return false;
  }
  const bool allowBuiltinFallback =
      (isArrayCountCall && isArrayCountCall(callExpr, localsIn)) ||
      (isVectorCapacityCall && isVectorCapacityCall(callExpr, localsIn));
  const Expr &receiver = callExpr.args.front();
  if (isEntryArgsName && isEntryArgsName(receiver, localsIn)) {
    if (allowBuiltinFallback) {
      return false;
    }
    errorOut = "unknown method target for " + callExpr.name;
    return false;
  }

  receiverOut = &receiver;
  return true;
}

bool resolveMethodReceiverTypeFromLocalInfo(const LocalInfo &localInfo,
                                            std::string &typeNameOut,
                                            std::string &resolvedTypePathOut) {
  typeNameOut.clear();
  resolvedTypePathOut.clear();

  if (localInfo.kind == LocalInfo::Kind::Array) {
    if (!localInfo.structTypeName.empty()) {
      resolvedTypePathOut = localInfo.structTypeName;
    } else {
      typeNameOut = "array";
    }
    return true;
  }
  if (localInfo.kind == LocalInfo::Kind::Vector) {
    typeNameOut = "vector";
    return true;
  }
  if (localInfo.kind == LocalInfo::Kind::Map) {
    typeNameOut = "map";
    return true;
  }
  if (localInfo.kind == LocalInfo::Kind::Reference && localInfo.referenceToArray) {
    if (!localInfo.structTypeName.empty()) {
      resolvedTypePathOut = localInfo.structTypeName;
    } else {
      typeNameOut = "array";
    }
    return true;
  }
  if (localInfo.kind == LocalInfo::Kind::Pointer || localInfo.kind == LocalInfo::Kind::Reference) {
    return false;
  }

  typeNameOut = typeNameForValueKind(localInfo.valueKind);
  return true;
}

std::string resolveMethodReceiverTypeNameFromCallExpr(const Expr &receiverCallExpr,
                                                      LocalInfo::ValueKind inferredKind) {
  std::string collection;
  if (getBuiltinCollectionName(receiverCallExpr, collection)) {
    if (collection == "array" && receiverCallExpr.templateArgs.size() == 1) {
      return "array";
    }
    if (collection == "vector" && receiverCallExpr.templateArgs.size() == 1) {
      return "vector";
    }
    if (collection == "map" && receiverCallExpr.templateArgs.size() == 2) {
      return "map";
    }
  }
  return typeNameForValueKind(inferredKind);
}

std::string resolveMethodReceiverStructTypePathFromCallExpr(
    const Expr &receiverCallExpr,
    const std::string &resolvedReceiverPath,
    const std::unordered_map<std::string, std::string> &importAliases,
    const std::unordered_set<std::string> &structNames) {
  if (receiverCallExpr.isBinding || receiverCallExpr.isMethodCall) {
    return "";
  }

  std::string resolved = resolvedReceiverPath;
  auto importIt = importAliases.find(receiverCallExpr.name);
  if (structNames.count(resolved) == 0 && importIt != importAliases.end()) {
    resolved = importIt->second;
  }
  if (structNames.count(resolved) > 0) {
    return resolved;
  }
  return "";
}

const Definition *resolveMethodDefinitionFromReceiverTarget(
    const std::string &methodName,
    const std::string &typeName,
    const std::string &resolvedTypePath,
    const std::unordered_map<std::string, const Definition *> &defMap,
    std::string &errorOut) {
  if (!resolvedTypePath.empty()) {
    const std::string resolved = resolvedTypePath + "/" + methodName;
    auto defIt = defMap.find(resolved);
    if (defIt == defMap.end()) {
      errorOut = "unknown method: " + resolved;
      return nullptr;
    }
    return defIt->second;
  }

  if (typeName.empty()) {
    errorOut = "unknown method target for " + methodName;
    return nullptr;
  }

  const std::string resolved = "/" + typeName + "/" + methodName;
  auto defIt = defMap.find(resolved);
  if (defIt == defMap.end()) {
    errorOut = "unknown method: " + resolved;
    return nullptr;
  }
  return defIt->second;
}

bool resolveReturnInfoKindForPath(const std::string &path,
                                  const GetReturnInfoForPathFn &getReturnInfo,
                                  bool requireArrayReturn,
                                  LocalInfo::ValueKind &kindOut) {
  kindOut = LocalInfo::ValueKind::Unknown;
  if (!getReturnInfo) {
    return false;
  }

  ReturnInfo info;
  if (!getReturnInfo(path, info) || info.returnsVoid) {
    return false;
  }

  if (requireArrayReturn) {
    if (!info.returnsArray) {
      return false;
    }
  } else if (info.returnsArray) {
    return false;
  }

  kindOut = info.kind;
  return true;
}

bool resolveMethodCallReturnKind(const Expr &methodCallExpr,
                                 const LocalMap &localsIn,
                                 const ResolveMethodCallDefinitionFn &resolveMethodCallDefinition,
                                 const GetReturnInfoForPathFn &getReturnInfo,
                                 bool requireArrayReturn,
                                 LocalInfo::ValueKind &kindOut,
                                 bool *methodResolvedOut) {
  kindOut = LocalInfo::ValueKind::Unknown;
  if (methodResolvedOut != nullptr) {
    *methodResolvedOut = false;
  }
  if (!resolveMethodCallDefinition) {
    return false;
  }

  const Definition *callee = resolveMethodCallDefinition(methodCallExpr, localsIn);
  if (callee == nullptr) {
    return false;
  }

  if (methodResolvedOut != nullptr) {
    *methodResolvedOut = true;
  }
  return resolveReturnInfoKindForPath(callee->fullPath, getReturnInfo, requireArrayReturn, kindOut);
}

bool resolveDefinitionCallReturnKind(const Expr &callExpr,
                                     const std::unordered_map<std::string, const Definition *> &defMap,
                                     const ResolveReceiverExprPathFn &resolveExprPath,
                                     const GetReturnInfoForPathFn &getReturnInfo,
                                     bool requireArrayReturn,
                                     LocalInfo::ValueKind &kindOut,
                                     bool *definitionMatchedOut) {
  kindOut = LocalInfo::ValueKind::Unknown;
  if (definitionMatchedOut != nullptr) {
    *definitionMatchedOut = false;
  }

  if (callExpr.kind != Expr::Kind::Call || callExpr.isMethodCall) {
    return false;
  }

  const std::string resolved = resolveExprPath(callExpr);
  auto defIt = defMap.find(resolved);
  if (defIt == defMap.end()) {
    return false;
  }

  if (definitionMatchedOut != nullptr) {
    *definitionMatchedOut = true;
  }
  return resolveReturnInfoKindForPath(resolved, getReturnInfo, requireArrayReturn, kindOut);
}

bool resolveCountMethodCallReturnKind(const Expr &callExpr,
                                      const LocalMap &localsIn,
                                      const IsMethodCallClassifierFn &isArrayCountCall,
                                      const IsMethodCallClassifierFn &isStringCountCall,
                                      const ResolveMethodCallDefinitionFn &resolveMethodCallDefinition,
                                      const GetReturnInfoForPathFn &getReturnInfo,
                                      bool requireArrayReturn,
                                      LocalInfo::ValueKind &kindOut,
                                      bool *methodResolvedOut) {
  kindOut = LocalInfo::ValueKind::Unknown;
  if (methodResolvedOut != nullptr) {
    *methodResolvedOut = false;
  }

  if (callExpr.kind != Expr::Kind::Call || callExpr.isMethodCall) {
    return false;
  }
  const bool isCountCall = isSimpleCallName(callExpr, "count") && callExpr.args.size() == 1;
  const bool isAccessCall =
      (isSimpleCallName(callExpr, "at") || isSimpleCallName(callExpr, "at_unsafe")) && callExpr.args.size() == 2;
  if (!isCountCall && !isAccessCall) {
    return false;
  }
  (void)isArrayCountCall;
  (void)isStringCountCall;

  Expr methodExpr = callExpr;
  methodExpr.isMethodCall = true;
  return resolveMethodCallReturnKind(methodExpr,
                                     localsIn,
                                     resolveMethodCallDefinition,
                                     getReturnInfo,
                                     requireArrayReturn,
                                     kindOut,
                                     methodResolvedOut);
}

bool resolveCapacityMethodCallReturnKind(const Expr &callExpr,
                                         const LocalMap &localsIn,
                                         const ResolveMethodCallDefinitionFn &resolveMethodCallDefinition,
                                         const GetReturnInfoForPathFn &getReturnInfo,
                                         bool requireArrayReturn,
                                         LocalInfo::ValueKind &kindOut,
                                         bool *methodResolvedOut) {
  kindOut = LocalInfo::ValueKind::Unknown;
  if (methodResolvedOut != nullptr) {
    *methodResolvedOut = false;
  }

  if (callExpr.kind != Expr::Kind::Call || callExpr.isMethodCall) {
    return false;
  }
  if (!isSimpleCallName(callExpr, "capacity") || callExpr.args.size() != 1) {
    return false;
  }

  Expr methodExpr = callExpr;
  methodExpr.isMethodCall = true;
  return resolveMethodCallReturnKind(methodExpr,
                                     localsIn,
                                     resolveMethodCallDefinition,
                                     getReturnInfo,
                                     requireArrayReturn,
                                     kindOut,
                                     methodResolvedOut);
}

const Definition *resolveMethodCallDefinitionFromExpr(
    const Expr &callExpr,
    const LocalMap &localsIn,
    const IsMethodCallClassifierFn &isArrayCountCall,
    const IsMethodCallClassifierFn &isVectorCapacityCall,
    const IsMethodCallClassifierFn &isEntryArgsName,
    const std::unordered_map<std::string, std::string> &importAliases,
    const std::unordered_set<std::string> &structNames,
    const InferReceiverExprKindFn &inferExprKind,
    const ResolveReceiverExprPathFn &resolveExprPath,
    const std::unordered_map<std::string, const Definition *> &defMap,
    std::string &errorOut) {
  const bool allowBuiltinFallback =
      (isArrayCountCall && isArrayCountCall(callExpr, localsIn)) ||
      (isVectorCapacityCall && isVectorCapacityCall(callExpr, localsIn));

  const std::string priorError = errorOut;
  const Expr *receiver = nullptr;
  if (!resolveMethodCallReceiverExpr(callExpr,
                                     localsIn,
                                     isArrayCountCall,
                                     isVectorCapacityCall,
                                     isEntryArgsName,
                                     receiver,
                                     errorOut)) {
    if (allowBuiltinFallback) {
      errorOut = priorError;
    }
    return nullptr;
  }
  if (receiver == nullptr) {
    return nullptr;
  }

  std::string typeName;
  std::string resolvedTypePath;
  if (!resolveMethodReceiverTarget(*receiver,
                                   localsIn,
                                   callExpr.name,
                                   importAliases,
                                   structNames,
                                   inferExprKind,
                                   resolveExprPath,
                                   typeName,
                                   resolvedTypePath,
                                   errorOut)) {
    if (allowBuiltinFallback) {
      errorOut = priorError;
    }
    return nullptr;
  }
  std::string lookupError;
  const Definition *resolvedDef = resolveMethodDefinitionFromReceiverTarget(
      callExpr.name, typeName, resolvedTypePath, defMap, lookupError);
  if (resolvedDef == nullptr) {
    if (allowBuiltinFallback) {
      errorOut = priorError;
      return nullptr;
    }
    errorOut = std::move(lookupError);
    return nullptr;
  }
  return resolvedDef;
}

bool resolveMethodReceiverTypeFromNameExpr(const Expr &receiverNameExpr,
                                           const LocalMap &localsIn,
                                           const std::string &methodName,
                                           std::string &typeNameOut,
                                           std::string &resolvedTypePathOut,
                                           std::string &errorOut) {
  if (receiverNameExpr.kind != Expr::Kind::Name) {
    errorOut = "internal method receiver type resolution requires name expression";
    return false;
  }

  auto it = localsIn.find(receiverNameExpr.name);
  if (it == localsIn.end()) {
    errorOut = "native backend does not know identifier: " + receiverNameExpr.name;
    return false;
  }
  if (!resolveMethodReceiverTypeFromLocalInfo(it->second, typeNameOut, resolvedTypePathOut)) {
    errorOut = "unknown method target for " + methodName;
    return false;
  }
  return true;
}

bool resolveMethodReceiverTarget(const Expr &receiverExpr,
                                 const LocalMap &localsIn,
                                 const std::string &methodName,
                                 const std::unordered_map<std::string, std::string> &importAliases,
                                 const std::unordered_set<std::string> &structNames,
                                 const InferReceiverExprKindFn &inferExprKind,
                                 const ResolveReceiverExprPathFn &resolveExprPath,
                                 std::string &typeNameOut,
                                 std::string &resolvedTypePathOut,
                                 std::string &errorOut) {
  typeNameOut.clear();
  resolvedTypePathOut.clear();

  if (receiverExpr.kind == Expr::Kind::Name) {
    return resolveMethodReceiverTypeFromNameExpr(
        receiverExpr, localsIn, methodName, typeNameOut, resolvedTypePathOut, errorOut);
  }
  if (receiverExpr.kind == Expr::Kind::Call) {
    typeNameOut = resolveMethodReceiverTypeNameFromCallExpr(receiverExpr, inferExprKind(receiverExpr, localsIn));
    if (typeNameOut.empty()) {
      resolvedTypePathOut = resolveMethodReceiverStructTypePathFromCallExpr(
          receiverExpr, resolveExprPath(receiverExpr), importAliases, structNames);
    }
    return true;
  }

  typeNameOut = typeNameForValueKind(inferExprKind(receiverExpr, localsIn));
  return true;
}

} // namespace primec::ir_lowerer
