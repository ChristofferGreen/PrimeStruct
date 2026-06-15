// soa-surface-audit: exempt
#include "EmitterBuiltinCallPathHelpersInternal.h"
#include "EmitterBuiltinCollectionInferenceInternal.h"
#include "EmitterCollectionSurfaceMetadata.h"
#include "EmitterHelpers.h"
#include "../semantics/SemanticsHelpers.h"
#include "primec/AstCallPathHelpers.h"

namespace primec::emitter {

namespace {

bool matchesScopedBuiltinSimpleCall(const Expr &expr, const char *nameToMatch) {
  if (expr.kind != Expr::Kind::Call || expr.name.empty()) {
    return false;
  }
  const std::string resolved = resolveExprPath(expr);
  if (normalizeInternalSoaStorageBuiltinAlias(resolved) == nameToMatch) {
    return true;
  }
  return isSimpleCallName(expr, nameToMatch);
}

bool isSoaVectorTypeNameLocal(const std::string &typeName) {
  std::string normalized = typeName;
  if (!normalized.empty() && normalized.front() == '/') {
    normalized.erase(normalized.begin());
  }
  if (normalized == "soa_vector" || normalized.rfind("soa_vector<", 0) == 0 ||
      semantics::isExperimentalSoaVectorTypePath(normalized)) {
    return true;
  }
  std::string base;
  std::string arg;
  return splitTemplateTypeName(normalized, base, arg) && isSoaVectorTypeNameLocal(base);
}

bool isSurfaceAliasTypeName(const std::string &typeName,
                            EmitterCollectionSurface surface) {
  const std::string normalized = normalizeBindingTypeName(typeName);
  const StdlibSurfaceMetadata *metadata =
      emitterCollectionSurfaceMetadata(surface);
  if (metadata != nullptr) {
    if (!metadata->backingTypeName.empty() &&
        normalizeBindingTypeName(std::string(metadata->backingTypeName)) ==
            normalized) {
      return true;
    }
    for (std::string_view alias : metadata->importAliasSpellings) {
      if (normalizeBindingTypeName(std::string(alias)) == normalized) {
        return true;
      }
    }
  }
  return false;
}

bool isSurfaceCollectionName(const std::string &collectionName,
                             EmitterCollectionSurface surface) {
  return isSurfaceAliasTypeName(collectionName, surface);
}

bool bindingBaseAndTemplateArg(const BindingInfo &binding,
                               std::string &baseOut,
                               std::string &argOut) {
  baseOut = normalizeBindingTypeName(binding.typeName);
  argOut = binding.typeTemplateArg;
  if (baseOut != "Reference") {
    return !baseOut.empty();
  }
  if (argOut.empty()) {
    return false;
  }
  std::string referencedBase;
  std::string referencedArg;
  if (splitTemplateTypeName(argOut, referencedBase, referencedArg)) {
    baseOut = normalizeBindingTypeName(referencedBase);
    argOut = referencedArg;
    return !baseOut.empty();
  }
  baseOut = normalizeBindingTypeName(argOut);
  argOut.clear();
  return !baseOut.empty();
}

bool isVectorCollectionBindingLocal(const BindingInfo &binding) {
  std::string base;
  std::string arg;
  return bindingBaseAndTemplateArg(binding, base, arg) &&
         isSurfaceAliasTypeName(base,
                                EmitterCollectionSurface::VectorConstructors);
}

bool isSoaVectorBindingLocal(const BindingInfo &binding) {
  if (isSoaVectorTypeNameLocal(binding.typeName)) {
    return true;
  }
  if (binding.typeName == "Reference") {
    std::string base;
    std::string arg;
    if (splitTemplateTypeName(binding.typeTemplateArg, base, arg)) {
      return isSoaVectorTypeNameLocal(base);
    }
    return isSoaVectorTypeNameLocal(binding.typeTemplateArg);
  }
  return false;
}

bool isSoaVectorValueLocal(const Expr &target,
                           const std::unordered_map<std::string, BindingInfo> &localTypes);
bool isCollectionVectorValueLocal(const Expr &target,
                        const std::unordered_map<std::string, BindingInfo> &localTypes);

bool isPublicOrCompatibilitySoaToAosCall(const Expr &target) {
  return isCanonicalCollectionHelperCall(target, "std/collections/soa", "to_aos", 1) ||
         isCanonicalCollectionHelperCall(target, "std/collections/soa_vector", "to_aos", 1);
}

bool isSoaVectorValueLocal(const Expr &target,
                           const std::unordered_map<std::string, BindingInfo> &localTypes) {
  if (target.kind == Expr::Kind::Name) {
    auto it = localTypes.find(target.name);
    return it != localTypes.end() && isSoaVectorBindingLocal(it->second);
  }
  if (target.kind == Expr::Kind::Call) {
    std::string collection;
    if (getBuiltinCollectionName(target, collection) && collection == "soa_vector") {
      return target.templateArgs.size() == 1;
    }
    if (!target.isMethodCall && isSimpleCallName(target, "to_soa") && target.args.size() == 1) {
      return isCollectionVectorValueLocal(target.args.front(), localTypes);
    }
  }
  return false;
}

bool isCollectionVectorValueLocal(const Expr &target,
                        const std::unordered_map<std::string, BindingInfo> &localTypes) {
  if (target.kind == Expr::Kind::Name) {
    auto it = localTypes.find(target.name);
    if (it == localTypes.end()) {
      return false;
    }
    return isVectorCollectionBindingLocal(it->second);
  }
  if (target.kind == Expr::Kind::Call) {
    std::string collection;
    if (getBuiltinCollectionName(target, collection) &&
        isSurfaceCollectionName(
            collection, EmitterCollectionSurface::VectorConstructors)) {
      return target.templateArgs.size() == 1;
    }
    if (isPublicOrCompatibilitySoaToAosCall(target)) {
      return isSoaVectorValueLocal(target.args.front(), localTypes);
    }
  }
  return false;
}

} // namespace

bool isArrayValue(const Expr &target, const std::unordered_map<std::string, BindingInfo> &localTypes) {
  if (target.kind == Expr::Kind::Name) {
    auto it = localTypes.find(target.name);
    if (it == localTypes.end()) {
      return false;
    }
    if (it->second.typeName == "array" || it->second.typeName == "args" ||
        isVectorCollectionBindingLocal(it->second)) {
      return true;
    }
    if (it->second.typeName == "Reference") {
      std::string base;
      std::string arg;
      if (splitTemplateTypeName(it->second.typeTemplateArg, base, arg)) {
        return base == "array" || base == "args";
      }
    }
    return false;
  }
  if (target.kind == Expr::Kind::Call) {
    std::string collection;
    return getBuiltinCollectionName(target, collection) &&
           (collection == "array" ||
            isSurfaceCollectionName(
                collection, EmitterCollectionSurface::VectorConstructors)) &&
           target.templateArgs.size() == 1;
  }
  return false;
}

bool isExplicitArrayCountName(const Expr &expr) {
  if (expr.kind != Expr::Kind::Call || expr.name.empty()) {
    return false;
  }
  std::string normalized = resolveExprPath(expr);
  if (!normalized.empty() && normalized.front() == '/') {
    normalized.erase(normalized.begin());
  }
  return normalized == "array/count";
}

bool isExplicitMapCountNameLocal(const Expr &expr) {
  if (expr.kind != Expr::Kind::Call || expr.name.empty()) {
    return false;
  }
  std::string resolved = resolveExprPath(expr);
  if (!resolved.empty() && resolved.front() != '/') {
    resolved.insert(resolved.begin(), '/');
  }
  const StdlibSurfaceMetadata *metadata =
      emitterCollectionSurfaceMetadata(EmitterCollectionSurface::KeyValueHelpers);
  return metadata != nullptr &&
         resolveStdlibSurfaceMemberName(*metadata, resolved) == "count";
}

bool isCollectionVectorValue(const Expr &target, const std::unordered_map<std::string, BindingInfo> &localTypes) {
  return isCollectionVectorValueLocal(target, localTypes);
}

bool isKeyValueStorageValue(const Expr &target, const std::unordered_map<std::string, BindingInfo> &localTypes) {
  if (target.kind == Expr::Kind::Name) {
    auto it = localTypes.find(target.name);
    if (it == localTypes.end()) {
      return false;
    }
    std::string keyType;
    std::string valueType;
    return extractKeyValueCollectionTypesLocal(it->second, keyType, valueType);
  }
  if (target.kind == Expr::Kind::Call) {
    std::string collection;
    return getBuiltinCollectionName(target, collection) &&
           isSurfaceCollectionName(
               collection, EmitterCollectionSurface::KeyValueConstructors) &&
           target.templateArgs.size() == 2;
  }
  return false;
}

bool isStringValue(const Expr &target, const std::unordered_map<std::string, BindingInfo> &localTypes) {
  if (target.kind == Expr::Kind::StringLiteral) {
    return true;
  }
  if (target.kind == Expr::Kind::Name) {
    auto it = localTypes.find(target.name);
    return it != localTypes.end() && it->second.typeName == "string";
  }
  if (target.kind == Expr::Kind::Call) {
    if ((matchesScopedBuiltinSimpleCall(target, "at") ||
         matchesScopedBuiltinSimpleCall(target, "at_unsafe")) &&
        target.args.size() == 2) {
      std::string elementType;
      if (inferCollectionElementTypeNameFromExpr(target.args.front(), localTypes, {}, elementType) &&
          normalizeBindingTypeName(elementType) == "string") {
        return true;
      }
    }
  }
  return false;
}

bool isArrayCountCall(const Expr &call, const std::unordered_map<std::string, BindingInfo> &localTypes) {
  if (!matchesScopedBuiltinSimpleCall(call, "count") || call.args.size() != 1) {
    return false;
  }
  if (isExplicitArrayCountName(call) && isCollectionVectorValue(call.args.front(), localTypes)) {
    return false;
  }
  return isArrayValue(call.args.front(), localTypes);
}

bool isStringCountCall(const Expr &call, const std::unordered_map<std::string, BindingInfo> &localTypes) {
  if (!matchesScopedBuiltinSimpleCall(call, "count") || call.args.size() != 1) {
    return false;
  }
  return isStringValue(call.args.front(), localTypes);
}

bool isVectorCapacityCall(const Expr &call, const std::unordered_map<std::string, BindingInfo> &localTypes) {
  if (!matchesScopedBuiltinSimpleCall(call, "capacity") || call.args.size() != 1) {
    return false;
  }
  return isCollectionVectorValue(call.args.front(), localTypes);
}

size_t getAccessCallReceiverIndex(const Expr &call,
                                  const std::unordered_map<std::string, BindingInfo> &localTypes) {
  if (call.args.size() != 2) {
    return 0;
  }
  if (hasNamedArguments(call.argNames)) {
    for (size_t i = 0; i < call.argNames.size() && i < call.args.size(); ++i) {
      if (call.argNames[i].has_value() && *call.argNames[i] == "values") {
        return i;
      }
    }
  }
  const bool leadingIsCollectionLike = isArrayValue(call.args.front(), localTypes) ||
                                       isCollectionVectorValue(call.args.front(), localTypes) ||
                                       isKeyValueStorageValue(call.args.front(), localTypes) ||
                                       isStringValue(call.args.front(), localTypes);
  const bool trailingIsCollectionLike = isArrayValue(call.args[1], localTypes) ||
                                        isCollectionVectorValue(call.args[1], localTypes) ||
                                        isKeyValueStorageValue(call.args[1], localTypes) ||
                                        isStringValue(call.args[1], localTypes);
  if (!leadingIsCollectionLike && trailingIsCollectionLike) {
    return 1;
  }
  return 0;
}

bool inferCollectionElementTypeNameFromBinding(const BindingInfo &binding, std::string &typeOut) {
  std::string typeName = normalizeBindingTypeName(binding.typeName);
  std::string templateArg = binding.typeTemplateArg;
  if (typeName == "Reference" && !templateArg.empty()) {
    std::string base;
    std::string arg;
    if (splitTemplateTypeName(templateArg, base, arg)) {
      typeName = normalizeBindingTypeName(base);
      templateArg = arg;
    } else {
      typeName = normalizeBindingTypeName(templateArg);
      templateArg.clear();
    }
  }
  if ((typeName == "array" || typeName == "args" ||
       isSurfaceAliasTypeName(typeName,
                              EmitterCollectionSurface::VectorConstructors)) &&
      !templateArg.empty()) {
    typeOut = normalizeBindingTypeName(templateArg);
    return true;
  }
  if (isSurfaceAliasTypeName(typeName,
                             EmitterCollectionSurface::KeyValueConstructors) &&
      !templateArg.empty()) {
    std::string keyType;
    if (extractKeyValueCollectionTypesLocal(binding, keyType, typeOut)) {
      typeOut = normalizeBindingTypeName(typeOut);
      return true;
    }
  }
  return false;
}

bool inferCollectionElementTypeNameFromExpr(const Expr &expr,
                                            const std::unordered_map<std::string, BindingInfo> &localTypes,
                                            const std::function<bool(const Expr &, std::string &)> &resolveCallElementTypeName,
                                            std::string &typeOut) {
  if (expr.kind == Expr::Kind::Name) {
    auto it = localTypes.find(expr.name);
    if (it == localTypes.end()) {
      return false;
    }
    return inferCollectionElementTypeNameFromBinding(it->second, typeOut);
  }
  if (expr.kind == Expr::Kind::Call) {
    if (resolveCallElementTypeName && resolveCallElementTypeName(expr, typeOut)) {
      return true;
    }
    std::string collectionName;
    if (!getBuiltinCollectionName(expr, collectionName)) {
      return false;
    }
    if ((collectionName == "array" ||
         isSurfaceCollectionName(collectionName,
                                 EmitterCollectionSurface::VectorConstructors)) &&
        expr.templateArgs.size() == 1) {
      typeOut = normalizeBindingTypeName(expr.templateArgs.front());
      return true;
    }
    if (isSurfaceCollectionName(collectionName,
                                EmitterCollectionSurface::KeyValueConstructors) &&
        expr.templateArgs.size() == 2) {
      typeOut = normalizeBindingTypeName(expr.templateArgs[1]);
      return true;
    }
  }
  return false;
}

std::string inferAccessCallTypeName(const Expr &call,
                                    const std::unordered_map<std::string, BindingInfo> &localTypes,
                                    const std::function<std::string(const Expr &)> &inferPrimitiveTypeName,
                                    const std::function<bool(const Expr &, std::string &)> &resolveCallElementTypeName) {
  std::string accessName;
  if (!getBuiltinArrayAccessNameLocal(call, accessName) || call.args.size() != 2) {
    return "";
  }
  const size_t receiverIndex = getAccessCallReceiverIndex(call, localTypes);
  if (receiverIndex >= call.args.size()) {
    return "";
  }
  const Expr &receiver = call.args[receiverIndex];
  if (isStringValue(receiver, localTypes)) {
    return "i32";
  }
  if (inferPrimitiveTypeName) {
    const std::string inferredReceiverType = normalizeBindingTypeName(inferPrimitiveTypeName(receiver));
    if (inferredReceiverType == "string") {
      return "i32";
    }
  }
  std::string elementType;
  if (inferCollectionElementTypeNameFromExpr(receiver, localTypes, resolveCallElementTypeName, elementType)) {
    return elementType;
  }
  return "";
}

} // namespace primec::emitter
