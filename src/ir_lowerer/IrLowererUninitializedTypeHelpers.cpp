#include "IrLowererUninitializedTypeHelpers.h"

#include <vector>

#include "IrLowererBindingTransformHelpers.h"
#include "IrLowererBindingTypeHelpers.h"
#include "IrLowererCallHelpers.h"
#include "IrLowererHelpers.h"
#include "IrLowererSetupTypeHelpers.h"
#include "IrLowererStructTypeHelpers.h"
#include "IrLowererTemplateTypeParseHelpers.h"

namespace primec::ir_lowerer {

namespace {

std::string resolveScopedCallPath(const Expr &expr) {
  if (expr.name.find('/') != std::string::npos || expr.namespacePrefix.empty()) {
    return expr.name;
  }
  if (expr.namespacePrefix == "/") {
    return "/" + expr.name;
  }
  return expr.namespacePrefix + "/" + expr.name;
}

bool isBareOrInternalSoaHelperCall(const Expr &expr, const char *helperName) {
  if (isSimpleCallName(expr, helperName)) {
    return true;
  }
  const std::string scopedCallPath = resolveScopedCallPath(expr);
  const std::string helperPath = helperName == nullptr ? std::string() : std::string(helperName);
  return scopedCallPath == "/" + helperPath ||
         scopedCallPath == "/std/collections/internal_soa_storage/" + helperPath;
}

} // namespace

bool resolveUninitializedTypeInfo(const std::string &typeText,
                                  const std::string &namespacePrefix,
                                  const ResolveStructTypeNameFn &resolveStructTypePath,
                                  UninitializedTypeInfo &out,
                                  std::string &error) {
  out = UninitializedTypeInfo{};
  if (typeText.empty()) {
    return false;
  }
  auto isSupportedNumeric = [](LocalInfo::ValueKind kind) {
    return kind == LocalInfo::ValueKind::Int32 || kind == LocalInfo::ValueKind::Int64 ||
           kind == LocalInfo::ValueKind::UInt64 || kind == LocalInfo::ValueKind::Float32 ||
           kind == LocalInfo::ValueKind::Float64 || kind == LocalInfo::ValueKind::Bool;
  };

  std::string base;
  std::string argText;
  if (splitTemplateTypeName(typeText, base, argText)) {
    base = normalizeCollectionBindingTypeName(base);
    if (base == "array" || base == "vector") {
      std::vector<std::string> args;
      if (!splitTemplateArgs(argText, args) || args.size() != 1) {
        error = "native backend requires " + base + " to have exactly one template argument";
        return false;
      }
      LocalInfo::ValueKind elemKind = valueKindFromTypeName(trimTemplateTypeText(args.front()));
      if (!isSupportedNumeric(elemKind)) {
        error = "native backend only supports numeric/bool uninitialized " + base + " storage";
        return false;
      }
      out.kind = (base == "array") ? LocalInfo::Kind::Array : LocalInfo::Kind::Vector;
      out.valueKind = elemKind;
      return true;
    }
    if (base == "map") {
      std::vector<std::string> args;
      if (!splitTemplateArgs(argText, args) || args.size() != 2) {
        error = "native backend requires map to have exactly two template arguments";
        return false;
      }
      LocalInfo::ValueKind keyKind = valueKindFromTypeName(trimTemplateTypeText(args.front()));
      LocalInfo::ValueKind valueKind = valueKindFromTypeName(trimTemplateTypeText(args.back()));
      if (keyKind == LocalInfo::ValueKind::Unknown || valueKind == LocalInfo::ValueKind::Unknown ||
          valueKind == LocalInfo::ValueKind::String) {
        error = "native backend only supports numeric/bool map values for uninitialized storage";
        return false;
      }
      out.kind = LocalInfo::Kind::Map;
      out.valueKind = valueKind;
      out.mapKeyKind = keyKind;
      out.mapValueKind = valueKind;
      return true;
    }
    if (base == "Pointer" || base == "Reference" || base == "Buffer") {
      out.kind = LocalInfo::Kind::Value;
      out.valueKind = LocalInfo::ValueKind::Int64;
      return true;
    }
    error = "native backend does not support uninitialized storage for type: " + typeText;
    return false;
  }

  LocalInfo::ValueKind kind = valueKindFromTypeName(typeText);
  if (isSupportedNumeric(kind)) {
    out.kind = LocalInfo::Kind::Value;
    out.valueKind = kind;
    return true;
  }
  std::string resolved;
  if (resolveStructTypePath(typeText, namespacePrefix, resolved)) {
    out.kind = LocalInfo::Kind::Value;
    out.structPath = resolved;
    return true;
  }
  if (kind == LocalInfo::ValueKind::String) {
    out.kind = LocalInfo::Kind::Value;
    out.valueKind = kind;
    return true;
  }
  return false;
}

bool resolveUninitializedTypeInfoFromLocalStorage(const LocalInfo &local, UninitializedTypeInfo &out) {
  out = UninitializedTypeInfo{};
  if (!local.isUninitializedStorage) {
    return false;
  }
  out.kind = local.kind;
  out.valueKind = local.valueKind;
  out.mapKeyKind = local.mapKeyKind;
  out.mapValueKind = local.mapValueKind;
  out.structPath = local.structTypeName;
  return true;
}

bool resolveUninitializedTypeInfoFromPointerTargetLocal(const LocalInfo &local, UninitializedTypeInfo &out) {
  out = UninitializedTypeInfo{};
  if (!local.targetsUninitializedStorage ||
      (local.kind != LocalInfo::Kind::Pointer && local.kind != LocalInfo::Kind::Reference)) {
    return false;
  }

  out.kind = LocalInfo::Kind::Value;
  if (local.referenceToArray || local.pointerToArray) {
    out.kind = LocalInfo::Kind::Array;
  } else if (local.referenceToVector || local.pointerToVector) {
    out.kind = LocalInfo::Kind::Vector;
  } else if (local.referenceToMap || local.pointerToMap) {
    out.kind = LocalInfo::Kind::Map;
  } else if (local.referenceToBuffer || local.pointerToBuffer) {
    out.kind = LocalInfo::Kind::Buffer;
  }
  out.valueKind = local.valueKind;
  out.mapKeyKind = local.mapKeyKind;
  out.mapValueKind = local.mapValueKind;
  out.structPath = local.structTypeName;
  return true;
}

bool resolveUninitializedTypeInfoFromArgsPackPointerTargetLocal(const LocalInfo &local, UninitializedTypeInfo &out) {
  out = UninitializedTypeInfo{};
  if (!local.isArgsPack || !local.targetsUninitializedStorage ||
      (local.argsPackElementKind != LocalInfo::Kind::Pointer &&
       local.argsPackElementKind != LocalInfo::Kind::Reference)) {
    return false;
  }

  out.kind = LocalInfo::Kind::Value;
  if (local.referenceToArray || local.pointerToArray) {
    out.kind = LocalInfo::Kind::Array;
  } else if (local.referenceToVector || local.pointerToVector) {
    out.kind = LocalInfo::Kind::Vector;
  } else if (local.referenceToMap || local.pointerToMap) {
    out.kind = LocalInfo::Kind::Map;
  } else if (local.referenceToBuffer || local.pointerToBuffer) {
    out.kind = LocalInfo::Kind::Buffer;
  }
  out.valueKind = local.valueKind;
  out.mapKeyKind = local.mapKeyKind;
  out.mapValueKind = local.mapValueKind;
  out.structPath = local.structTypeName;
  return true;
}

bool resolveUninitializedLocalStorageCandidate(const Expr &storage,
                                               const LocalMap &localsIn,
                                               const LocalInfo *&localOut,
                                               UninitializedTypeInfo &typeInfoOut,
                                               bool &resolvedOut) {
  localOut = nullptr;
  typeInfoOut = UninitializedTypeInfo{};
  resolvedOut = false;
  if (storage.kind != Expr::Kind::Name) {
    return false;
  }
  auto localIt = localsIn.find(storage.name);
  if (localIt != localsIn.end() &&
      resolveUninitializedTypeInfoFromLocalStorage(localIt->second, typeInfoOut)) {
    localOut = &localIt->second;
    resolvedOut = true;
  }
  return true;
}

bool resolveUninitializedLocalStorageAccess(const Expr &storage,
                                            const LocalMap &localsIn,
                                            UninitializedLocalStorageAccessInfo &out,
                                            bool &resolvedOut) {
  out = UninitializedLocalStorageAccessInfo{};
  resolvedOut = false;

  const LocalInfo *local = nullptr;
  UninitializedTypeInfo typeInfo;
  if (!resolveUninitializedLocalStorageCandidate(storage, localsIn, local, typeInfo, resolvedOut)) {
    return false;
  }
  if (!resolvedOut) {
    return true;
  }
  out.local = local;
  out.typeInfo = typeInfo;
  resolvedOut = true;
  return true;
}

StructAndUninitializedFieldIndexes buildStructAndUninitializedFieldIndexes(
    std::size_t structReserveHint,
    const EnumerateStructLayoutFieldsFn &enumerateStructLayoutFields) {
  StructAndUninitializedFieldIndexes indexes;
  indexes.structLayoutFieldIndex =
      buildStructLayoutFieldIndex(structReserveHint, enumerateStructLayoutFields);
  indexes.uninitializedFieldBindingIndex =
      buildUninitializedFieldBindingIndexFromStructLayoutFieldIndex(indexes.structLayoutFieldIndex);
  return indexes;
}

UninitializedFieldBindingIndex buildUninitializedFieldBindingIndex(
    std::size_t structReserveHint,
    const EnumerateUninitializedFieldBindingsFn &enumerateFieldBindings) {
  UninitializedFieldBindingIndex fieldIndex;
  fieldIndex.reserve(structReserveHint);
  if (!enumerateFieldBindings) {
    return fieldIndex;
  }
  enumerateFieldBindings(
      [&](const std::string &structPath, const UninitializedFieldBindingInfo &fieldBinding) {
        fieldIndex[structPath].push_back(fieldBinding);
      });
  return fieldIndex;
}

UninitializedFieldBindingIndex buildUninitializedFieldBindingIndexFromStructLayoutFieldIndex(
    const StructLayoutFieldIndex &fieldIndex) {
  return buildUninitializedFieldBindingIndex(
      fieldIndex.size(),
      [&](const AppendUninitializedFieldBindingFn &appendFieldBinding) {
        for (const auto &entry : fieldIndex) {
          for (const auto &field : entry.second) {
            UninitializedFieldBindingInfo info;
            info.name = field.name;
            info.typeName = field.typeName;
            info.typeTemplateArg = field.typeTemplateArg;
            info.isStatic = field.isStatic;
            appendFieldBinding(entry.first, info);
          }
        }
      });
}

bool hasUninitializedFieldBindingsForStructPath(const UninitializedFieldBindingIndex &fieldIndex,
                                                const std::string &structPath) {
  return fieldIndex.count(structPath) > 0;
}

bool collectUninitializedFieldBindingsFromIndex(const UninitializedFieldBindingIndex &fieldIndex,
                                                const std::string &structPath,
                                                std::vector<UninitializedFieldBindingInfo> &fieldsOut) {
  auto fieldIt = fieldIndex.find(structPath);
  if (fieldIt == fieldIndex.end()) {
    return false;
  }
  fieldsOut = fieldIt->second;
  return true;
}

bool resolveUninitializedFieldTemplateArg(const std::string &structPath,
                                          const std::string &fieldName,
                                          const CollectUninitializedFieldBindingsFn &collectFieldBindings,
                                          std::string &typeTemplateArgOut) {
  std::vector<UninitializedFieldBindingInfo> fields;
  if (!collectFieldBindings(structPath, fields)) {
    return false;
  }
  return findUninitializedFieldTemplateArg(fields, fieldName, typeTemplateArgOut);
}

bool findUninitializedFieldTemplateArg(const std::vector<UninitializedFieldBindingInfo> &fields,
                                       const std::string &fieldName,
                                       std::string &typeTemplateArgOut) {
  for (const auto &field : fields) {
    if (field.isStatic) {
      continue;
    }
    if (field.name != fieldName) {
      continue;
    }
    if (field.typeName != "uninitialized" || field.typeTemplateArg.empty()) {
      return false;
    }
    typeTemplateArgOut = field.typeTemplateArg;
    return true;
  }
  return false;
}

const Definition *resolveUninitializedReceiverHelperDefinition(
    const Expr &expr,
    const std::unordered_map<std::string, const Definition *> &defMap) {
  if (expr.kind != Expr::Kind::Call || expr.isMethodCall || expr.name.empty()) {
    return nullptr;
  }

  std::vector<std::string> candidates;
  if (expr.name.front() == '/') {
    candidates.push_back(expr.name);
  } else {
    if (!expr.namespacePrefix.empty()) {
      candidates.push_back(expr.namespacePrefix + "/" + expr.name);
    }
    candidates.push_back("/" + expr.name);
    if (expr.name.find('/') != std::string::npos) {
      candidates.push_back(expr.name);
    }
  }

  for (const auto &candidate : candidates) {
    auto it = defMap.find(candidate);
    if (it != defMap.end() && it->second != nullptr) {
      return it->second;
    }
  }
  return nullptr;
}

bool resolveUninitializedFieldReceiverInfo(const Expr &receiverExpr,
                                           const LocalMap &localsIn,
                                           const std::unordered_map<std::string, const Definition *> &defMap,
                                           const LocalInfo *&receiverOut,
                                           std::string &structPathOut) {
  receiverOut = nullptr;
  structPathOut.clear();

  if (receiverExpr.kind == Expr::Kind::Name) {
    auto recvIt = localsIn.find(receiverExpr.name);
    if (recvIt == localsIn.end() || recvIt->second.structTypeName.empty()) {
      return false;
    }
    receiverOut = &recvIt->second;
    structPathOut = recvIt->second.structTypeName;
    return true;
  }

  if (receiverExpr.kind != Expr::Kind::Call || receiverExpr.isMethodCall) {
    return false;
  }

  if ((isBareOrInternalSoaHelperCall(receiverExpr, "location") ||
       isBareOrInternalSoaHelperCall(receiverExpr, "dereference")) &&
      receiverExpr.args.size() == 1) {
    return resolveUninitializedFieldReceiverInfo(receiverExpr.args.front(),
                                                 localsIn,
                                                 defMap,
                                                 receiverOut,
                                                 structPathOut);
  }

  const Definition *helperDef = resolveUninitializedReceiverHelperDefinition(receiverExpr, defMap);
  if (helperDef == nullptr) {
    return false;
  }

  const Expr *returnedValueExpr = nullptr;
  for (const auto &stmt : helperDef->statements) {
    if (isReturnCall(stmt) && stmt.args.size() == 1) {
      returnedValueExpr = &stmt.args.front();
    }
  }
  if (helperDef->returnExpr.has_value()) {
    returnedValueExpr = &*helperDef->returnExpr;
  }
  if (returnedValueExpr == nullptr || returnedValueExpr->kind != Expr::Kind::Name) {
    return false;
  }

  std::vector<const Expr *> orderedArgs;
  std::string argError;
  if (!buildOrderedCallArguments(receiverExpr, helperDef->parameters, orderedArgs, argError)) {
    return false;
  }

  for (size_t index = 0; index < helperDef->parameters.size() && index < orderedArgs.size(); ++index) {
    const auto &param = helperDef->parameters[index];
    const Expr *argExpr = orderedArgs[index];
    if (argExpr == nullptr || param.name != returnedValueExpr->name) {
      continue;
    }
    std::string paramTypeName;
    std::vector<std::string> paramTemplateArgs;
    if (!extractFirstBindingTypeTransform(param, paramTypeName, paramTemplateArgs)) {
      return false;
    }
    const std::string normalizedParamType = normalizeCollectionBindingTypeName(paramTypeName);
    if (normalizedParamType != "Reference" && normalizedParamType != "Pointer") {
      return false;
    }
    return resolveUninitializedFieldReceiverInfo(*argExpr, localsIn, defMap, receiverOut, structPathOut);
  }

  return false;
}

namespace {

bool resolveUninitializedFieldStorageCandidateWithDefinitions(
    const Expr &storage,
    const LocalMap &localsIn,
    const FindUninitializedFieldTemplateArgFn &findFieldTemplateArg,
    const std::unordered_map<std::string, const Definition *> &defMap,
    const LocalInfo *&receiverOut,
    std::string &structPathOut,
    std::string &typeTemplateArgOut) {
  receiverOut = nullptr;
  structPathOut.clear();
  typeTemplateArgOut.clear();
  if (!storage.isFieldAccess || storage.args.size() != 1) {
    return false;
  }
  if (!resolveUninitializedFieldReceiverInfo(storage.args.front(), localsIn, defMap, receiverOut, structPathOut)) {
    return false;
  }
  std::string typeTemplateArg;
  if (!findFieldTemplateArg(structPathOut, storage.name, typeTemplateArg)) {
    return false;
  }
  typeTemplateArgOut = std::move(typeTemplateArg);
  return true;
}

bool resolveUninitializedFieldStorageTypeInfoWithDefinitions(
    const Expr &storage,
    const LocalMap &localsIn,
    const FindUninitializedFieldTemplateArgFn &findFieldTemplateArg,
    const std::unordered_map<std::string, const Definition *> &defMap,
    const ResolveDefinitionNamespacePrefixFn &resolveDefinitionNamespacePrefix,
    const ResolveUninitializedFieldTypeInfoFn &resolveUninitializedTypeInfo,
    UninitializedFieldStorageTypeInfo &out,
    bool &resolvedOut,
    std::string &error) {
  out = UninitializedFieldStorageTypeInfo{};
  resolvedOut = false;

  const LocalInfo *receiver = nullptr;
  std::string structPath;
  std::string typeTemplateArg;
  if (!resolveUninitializedFieldStorageCandidateWithDefinitions(
          storage, localsIn, findFieldTemplateArg, defMap, receiver, structPath, typeTemplateArg)) {
    return true;
  }

  UninitializedTypeInfo typeInfo;
  const std::string namespacePrefix = resolveDefinitionNamespacePrefix(structPath);
  if (!resolveUninitializedTypeInfo(typeTemplateArg, namespacePrefix, typeInfo)) {
    if (error.empty()) {
      error = "native backend does not support uninitialized storage for type: " + typeTemplateArg;
    }
    return false;
  }

  out.receiver = receiver;
  out.structPath = std::move(structPath);
  out.typeInfo = typeInfo;
  resolvedOut = true;
  return true;
}

bool resolveUninitializedFieldStorageAccessWithDefinitions(
    const Expr &storage,
    const LocalMap &localsIn,
    const FindUninitializedFieldTemplateArgFn &findFieldTemplateArg,
    const std::unordered_map<std::string, const Definition *> &defMap,
    const ResolveDefinitionNamespacePrefixFn &resolveDefinitionNamespacePrefix,
    const ResolveUninitializedFieldTypeInfoFn &resolveUninitializedTypeInfo,
    const ResolveStructFieldSlotFn &resolveStructFieldSlot,
    UninitializedFieldStorageAccessInfo &out,
    bool &resolvedOut,
    std::string &error) {
  out = UninitializedFieldStorageAccessInfo{};
  resolvedOut = false;

  UninitializedFieldStorageTypeInfo typeInfo;
  if (!resolveUninitializedFieldStorageTypeInfoWithDefinitions(storage,
                                                               localsIn,
                                                               findFieldTemplateArg,
                                                               defMap,
                                                               resolveDefinitionNamespacePrefix,
                                                               resolveUninitializedTypeInfo,
                                                               typeInfo,
                                                               resolvedOut,
                                                               error)) {
    return false;
  }
  if (!resolvedOut) {
    return true;
  }

  StructSlotFieldInfo slot;
  if (!resolveStructFieldSlot(typeInfo.structPath, storage.name, slot)) {
    return false;
  }
  out.receiver = typeInfo.receiver;
  out.fieldSlot = slot;
  out.typeInfo = typeInfo.typeInfo;
  resolvedOut = true;
  return true;
}

bool resolveUninitializedStorageAccessWithDefinitions(
    const Expr &storage,
    const LocalMap &localsIn,
    const FindUninitializedFieldTemplateArgFn &findFieldTemplateArg,
    const std::unordered_map<std::string, const Definition *> &defMap,
    const ResolveDefinitionNamespacePrefixFn &resolveDefinitionNamespacePrefix,
    const ResolveUninitializedFieldTypeInfoFn &resolveUninitializedTypeInfo,
    const ResolveStructFieldSlotFn &resolveStructFieldSlot,
    UninitializedStorageAccessInfo &out,
    bool &resolvedOut,
    std::string &error) {
  out = UninitializedStorageAccessInfo{};
  resolvedOut = false;

  if (storage.kind == Expr::Kind::Call && isBareOrInternalSoaHelperCall(storage, "dereference") &&
      storage.args.size() == 1) {
    const Expr &pointerExpr = storage.args.front();
    if (pointerExpr.kind == Expr::Kind::Call && isBareOrInternalSoaHelperCall(pointerExpr, "location") &&
        pointerExpr.args.size() == 1) {
      return resolveUninitializedStorageAccessWithDefinitions(pointerExpr.args.front(),
                                                              localsIn,
                                                              findFieldTemplateArg,
                                                              defMap,
                                                              resolveDefinitionNamespacePrefix,
                                                              resolveUninitializedTypeInfo,
                                                              resolveStructFieldSlot,
                                                              out,
                                                              resolvedOut,
                                                              error);
    }
    if (pointerExpr.kind == Expr::Kind::Name) {
      auto pointerIt = localsIn.find(pointerExpr.name);
      if (pointerIt != localsIn.end()) {
        UninitializedTypeInfo pointerTypeInfo;
        if (resolveUninitializedTypeInfoFromPointerTargetLocal(pointerIt->second, pointerTypeInfo)) {
          out.location = UninitializedStorageAccessInfo::Location::Indirect;
          out.pointer = &pointerIt->second;
          out.pointerExpr = &pointerExpr;
          out.typeInfo = pointerTypeInfo;
          resolvedOut = true;
          return true;
        }
      }
    }
    std::string accessName;
    if (pointerExpr.kind == Expr::Kind::Call && getBuiltinArrayAccessName(pointerExpr, accessName) &&
        pointerExpr.args.size() == 2 && pointerExpr.args.front().kind == Expr::Kind::Name) {
      auto pointerIt = localsIn.find(pointerExpr.args.front().name);
      if (pointerIt != localsIn.end()) {
        UninitializedTypeInfo pointerTypeInfo;
        if (resolveUninitializedTypeInfoFromArgsPackPointerTargetLocal(pointerIt->second, pointerTypeInfo)) {
          out.location = UninitializedStorageAccessInfo::Location::Indirect;
          out.pointer = &pointerIt->second;
          out.pointerExpr = &pointerExpr;
          out.typeInfo = pointerTypeInfo;
          resolvedOut = true;
          return true;
        }
      }
    }
  }

  UninitializedLocalStorageAccessInfo localStorage;
  if (resolveUninitializedLocalStorageAccess(storage, localsIn, localStorage, resolvedOut)) {
    if (resolvedOut) {
      out.location = UninitializedStorageAccessInfo::Location::Local;
      out.local = localStorage.local;
      out.typeInfo = localStorage.typeInfo;
    }
    return true;
  }

  UninitializedFieldStorageAccessInfo fieldStorage;
  if (!resolveUninitializedFieldStorageAccessWithDefinitions(storage,
                                                             localsIn,
                                                             findFieldTemplateArg,
                                                             defMap,
                                                             resolveDefinitionNamespacePrefix,
                                                             resolveUninitializedTypeInfo,
                                                             resolveStructFieldSlot,
                                                             fieldStorage,
                                                             resolvedOut,
                                                             error)) {
    return false;
  }
  if (!resolvedOut) {
    return true;
  }

  out.location = UninitializedStorageAccessInfo::Location::Field;
  out.receiver = fieldStorage.receiver;
  out.fieldSlot = fieldStorage.fieldSlot;
  out.typeInfo = fieldStorage.typeInfo;
  resolvedOut = true;
  return true;
}

bool resolveUninitializedStorageAccessWithFieldBindingsAndDefinitions(
    const Expr &storage,
    const LocalMap &localsIn,
    const CollectUninitializedFieldBindingsFn &collectFieldBindings,
    const std::unordered_map<std::string, const Definition *> &defMap,
    const ResolveDefinitionNamespacePrefixFn &resolveDefinitionNamespacePrefix,
    const ResolveUninitializedFieldTypeInfoFn &resolveUninitializedTypeInfo,
    const ResolveStructFieldSlotFn &resolveStructFieldSlot,
    UninitializedStorageAccessInfo &out,
    bool &resolvedOut,
    std::string &error) {
  return resolveUninitializedStorageAccessWithDefinitions(
      storage,
      localsIn,
      [&](const std::string &structPath, const std::string &fieldName, std::string &typeTemplateArgOut) {
        return resolveUninitializedFieldTemplateArg(structPath, fieldName, collectFieldBindings, typeTemplateArgOut);
      },
      defMap,
      resolveDefinitionNamespacePrefix,
      resolveUninitializedTypeInfo,
      resolveStructFieldSlot,
      out,
      resolvedOut,
      error);
}

} // namespace

bool resolveUninitializedFieldStorageCandidate(const Expr &storage,
                                               const LocalMap &localsIn,
                                               const FindUninitializedFieldTemplateArgFn &findFieldTemplateArg,
                                               const LocalInfo *&receiverOut,
                                               std::string &structPathOut,
                                               std::string &typeTemplateArgOut) {
  static const std::unordered_map<std::string, const Definition *> kEmptyDefMap;
  return resolveUninitializedFieldStorageCandidateWithDefinitions(
      storage, localsIn, findFieldTemplateArg, kEmptyDefMap, receiverOut, structPathOut, typeTemplateArgOut);
}

bool resolveUninitializedFieldStorageTypeInfo(
    const Expr &storage,
    const LocalMap &localsIn,
    const FindUninitializedFieldTemplateArgFn &findFieldTemplateArg,
    const ResolveDefinitionNamespacePrefixFn &resolveDefinitionNamespacePrefix,
    const ResolveUninitializedFieldTypeInfoFn &resolveUninitializedTypeInfo,
    UninitializedFieldStorageTypeInfo &out,
    bool &resolvedOut,
    std::string &error) {
  static const std::unordered_map<std::string, const Definition *> kEmptyDefMap;
  return resolveUninitializedFieldStorageTypeInfoWithDefinitions(storage,
                                                                 localsIn,
                                                                 findFieldTemplateArg,
                                                                 kEmptyDefMap,
                                                                 resolveDefinitionNamespacePrefix,
                                                                 resolveUninitializedTypeInfo,
                                                                 out,
                                                                 resolvedOut,
                                                                 error);
}

bool resolveUninitializedFieldStorageAccess(
    const Expr &storage,
    const LocalMap &localsIn,
    const FindUninitializedFieldTemplateArgFn &findFieldTemplateArg,
    const ResolveDefinitionNamespacePrefixFn &resolveDefinitionNamespacePrefix,
    const ResolveUninitializedFieldTypeInfoFn &resolveUninitializedTypeInfo,
    const ResolveStructFieldSlotFn &resolveStructFieldSlot,
    UninitializedFieldStorageAccessInfo &out,
    bool &resolvedOut,
    std::string &error) {
  static const std::unordered_map<std::string, const Definition *> kEmptyDefMap;
  return resolveUninitializedFieldStorageAccessWithDefinitions(storage,
                                                               localsIn,
                                                               findFieldTemplateArg,
                                                               kEmptyDefMap,
                                                               resolveDefinitionNamespacePrefix,
                                                               resolveUninitializedTypeInfo,
                                                               resolveStructFieldSlot,
                                                               out,
                                                               resolvedOut,
                                                               error);
}

bool resolveUninitializedStorageAccess(const Expr &storage,
                                       const LocalMap &localsIn,
                                       const FindUninitializedFieldTemplateArgFn &findFieldTemplateArg,
                                       const ResolveDefinitionNamespacePrefixFn &resolveDefinitionNamespacePrefix,
                                       const ResolveUninitializedFieldTypeInfoFn &resolveUninitializedTypeInfo,
                                       const ResolveStructFieldSlotFn &resolveStructFieldSlot,
                                       UninitializedStorageAccessInfo &out,
                                       bool &resolvedOut,
                                       std::string &error) {
  static const std::unordered_map<std::string, const Definition *> kEmptyDefMap;
  return resolveUninitializedStorageAccessWithDefinitions(storage,
                                                          localsIn,
                                                          findFieldTemplateArg,
                                                          kEmptyDefMap,
                                                          resolveDefinitionNamespacePrefix,
                                                          resolveUninitializedTypeInfo,
                                                          resolveStructFieldSlot,
                                                          out,
                                                          resolvedOut,
                                                          error);
}

bool resolveUninitializedStorageAccessWithFieldBindings(
    const Expr &storage,
    const LocalMap &localsIn,
    const CollectUninitializedFieldBindingsFn &collectFieldBindings,
    const ResolveDefinitionNamespacePrefixFn &resolveDefinitionNamespacePrefix,
    const ResolveUninitializedFieldTypeInfoFn &resolveUninitializedTypeInfo,
    const ResolveStructFieldSlotFn &resolveStructFieldSlot,
    UninitializedStorageAccessInfo &out,
    bool &resolvedOut,
    std::string &error) {
  static const std::unordered_map<std::string, const Definition *> kEmptyDefMap;
  return resolveUninitializedStorageAccessWithFieldBindingsAndDefinitions(storage,
                                                                          localsIn,
                                                                          collectFieldBindings,
                                                                          kEmptyDefMap,
                                                                          resolveDefinitionNamespacePrefix,
                                                                          resolveUninitializedTypeInfo,
                                                                          resolveStructFieldSlot,
                                                                          out,
                                                                          resolvedOut,
                                                                          error);
}

bool resolveUninitializedStorageAccessFromDefinitions(
    const Expr &storage,
    const LocalMap &localsIn,
    const CollectUninitializedFieldBindingsFn &collectFieldBindings,
    const std::unordered_map<std::string, const Definition *> &defMap,
    const ResolveUninitializedFieldTypeInfoFn &resolveUninitializedTypeInfo,
    const ResolveStructFieldSlotFn &resolveStructFieldSlot,
    UninitializedStorageAccessInfo &out,
    bool &resolvedOut,
    std::string &error) {
  return resolveUninitializedStorageAccessWithFieldBindingsAndDefinitions(
      storage,
      localsIn,
      collectFieldBindings,
      defMap,
      [&](const std::string &structPath) { return resolveDefinitionNamespacePrefix(defMap, structPath); },
      resolveUninitializedTypeInfo,
      resolveStructFieldSlot,
      out,
      resolvedOut,
      error);
}

bool resolveUninitializedStorageAccessFromDefinitionFieldIndex(
    const Expr &storage,
    const LocalMap &localsIn,
    const UninitializedFieldBindingIndex &fieldIndex,
    const std::unordered_map<std::string, const Definition *> &defMap,
    const ResolveUninitializedFieldTypeInfoFn &resolveUninitializedTypeInfo,
    const ResolveStructFieldSlotFn &resolveStructFieldSlot,
    UninitializedStorageAccessInfo &out,
    bool &resolvedOut,
    std::string &error) {
  return resolveUninitializedStorageAccessFromDefinitions(
      storage,
      localsIn,
      [&](const std::string &structPath, std::vector<UninitializedFieldBindingInfo> &fieldsOut) {
        return collectUninitializedFieldBindingsFromIndex(fieldIndex, structPath, fieldsOut);
      },
      defMap,
      resolveUninitializedTypeInfo,
      resolveStructFieldSlot,
      out,
      resolvedOut,
      error);
}

ResolveUninitializedStorageAccessFromFieldIndexFn
makeResolveUninitializedStorageAccessFromDefinitionFieldIndex(
    const UninitializedFieldBindingIndex &fieldIndex,
    const std::unordered_map<std::string, const Definition *> &defMap,
    const ResolveUninitializedFieldTypeInfoFn &resolveUninitializedTypeInfo,
    const ResolveStructFieldSlotFn &resolveStructFieldSlot,
    std::string &error) {
  return [fieldIndex, defMap, resolveUninitializedTypeInfo, resolveStructFieldSlot, &error](
             const Expr &storage, const LocalMap &localsIn, UninitializedStorageAccessInfo &out, bool &resolvedOut) {
    return resolveUninitializedStorageAccessFromDefinitionFieldIndex(
        storage,
        localsIn,
        fieldIndex,
        defMap,
        resolveUninitializedTypeInfo,
        resolveStructFieldSlot,
        out,
        resolvedOut,
        error);
  };
}

} // namespace primec::ir_lowerer
