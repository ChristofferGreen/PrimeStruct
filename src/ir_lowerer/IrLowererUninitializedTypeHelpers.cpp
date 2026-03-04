#include "IrLowererUninitializedTypeHelpers.h"

#include <vector>

#include "IrLowererHelpers.h"
#include "IrLowererCallHelpers.h"
#include "IrLowererSetupTypeHelpers.h"
#include "IrLowererTemplateTypeParseHelpers.h"

namespace primec::ir_lowerer {

ResolveUninitializedFieldTypeInfoFn makeResolveUninitializedTypeInfo(
    const ResolveStructTypePathFn &resolveStructTypePath,
    std::string &error) {
  return [&](const std::string &typeText, const std::string &namespacePrefix, UninitializedTypeInfo &out) {
    return resolveUninitializedTypeInfo(typeText, namespacePrefix, resolveStructTypePath, out, error);
  };
}

bool resolveUninitializedTypeInfo(const std::string &typeText,
                                  const std::string &namespacePrefix,
                                  const ResolveStructTypePathFn &resolveStructTypePath,
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

std::string inferStructPathFromCallTargetWithFieldBindingIndex(
    const Expr &expr,
    const InferStructExprPathFn &resolveExprPath,
    const UninitializedFieldBindingIndex &fieldIndex,
    const InferDefinitionStructReturnPathFn &inferDefinitionStructReturnPath) {
  return inferStructPathFromCallTarget(
      expr,
      resolveExprPath,
      [&](const std::string &resolvedPath) {
        return hasUninitializedFieldBindingsForStructPath(fieldIndex, resolvedPath);
      },
      inferDefinitionStructReturnPath);
}

std::string inferStructPathFromCallTargetWithFieldBindingIndexAndVisited(
    const Expr &expr,
    const InferStructExprPathFn &resolveExprPath,
    const UninitializedFieldBindingIndex &fieldIndex,
    const InferDefinitionStructReturnPathWithVisitedFn &inferDefinitionStructReturnPath,
    std::unordered_set<std::string> &visitedDefs) {
  return inferStructPathFromCallTargetWithFieldBindingIndex(
      expr,
      resolveExprPath,
      fieldIndex,
      [&](const std::string &resolvedPath) {
        return inferDefinitionStructReturnPath(resolvedPath, visitedDefs);
      });
}

std::string inferStructReturnPathFromDefinitionMapWithVisited(
    const std::string &defPath,
    const std::unordered_map<std::string, const Definition *> &defMap,
    const ResolveStructTypeNameFn &resolveStructTypeName,
    const InferStructReturnExprWithVisitedFn &inferStructReturnExprPath,
    std::unordered_set<std::string> &visitedDefs) {
  if (defPath.empty()) {
    return "";
  }
  if (!visitedDefs.insert(defPath).second) {
    return "";
  }
  const Definition *resolvedDef = resolveDefinitionByPath(defMap, defPath);
  if (resolvedDef == nullptr) {
    return "";
  }
  return inferStructReturnPathFromDefinition(
      *resolvedDef,
      resolveStructTypeName,
      [&](const Expr &expr) { return inferStructReturnExprPath(expr, visitedDefs); });
}

std::string inferStructReturnPathFromDefinitionMap(
    const std::string &defPath,
    const std::unordered_map<std::string, const Definition *> &defMap,
    const ResolveStructTypeNameFn &resolveStructTypeName,
    const InferStructReturnExprWithVisitedFn &inferStructReturnExprPath) {
  std::unordered_set<std::string> visitedDefs;
  return inferStructReturnPathFromDefinitionMapWithVisited(
      defPath, defMap, resolveStructTypeName, inferStructReturnExprPath, visitedDefs);
}

std::string inferStructReturnPathFromDefinitionMapByCallTargetWithFieldIndexWithVisited(
    const std::string &defPath,
    const std::unordered_map<std::string, const Definition *> &defMap,
    const ResolveStructTypeNameFn &resolveStructTypeName,
    const InferStructExprPathFn &resolveExprPath,
    const UninitializedFieldBindingIndex &fieldIndex,
    std::unordered_set<std::string> &visitedDefs) {
  return inferStructReturnPathFromDefinitionMapWithVisited(
      defPath,
      defMap,
      resolveStructTypeName,
      [&](const Expr &expr, std::unordered_set<std::string> &visitedIn) {
        return inferStructPathFromCallTargetWithFieldBindingIndexAndVisited(
            expr,
            resolveExprPath,
            fieldIndex,
            [&](const std::string &nestedDefPath, std::unordered_set<std::string> &nestedVisited) {
              return inferStructReturnPathFromDefinitionMapByCallTargetWithFieldIndexWithVisited(
                  nestedDefPath, defMap, resolveStructTypeName, resolveExprPath, fieldIndex, nestedVisited);
            },
            visitedIn);
      },
      visitedDefs);
}

std::string inferStructReturnPathFromDefinitionMapByCallTargetWithFieldIndex(
    const std::string &defPath,
    const std::unordered_map<std::string, const Definition *> &defMap,
    const ResolveStructTypeNameFn &resolveStructTypeName,
    const InferStructExprPathFn &resolveExprPath,
    const UninitializedFieldBindingIndex &fieldIndex) {
  std::unordered_set<std::string> visitedDefs;
  return inferStructReturnPathFromDefinitionMapByCallTargetWithFieldIndexWithVisited(
      defPath, defMap, resolveStructTypeName, resolveExprPath, fieldIndex, visitedDefs);
}

std::string inferStructExprPathFromDefinitionMapByCallTargetWithFieldIndex(
    const Expr &expr,
    const LocalMap &localsIn,
    const std::unordered_map<std::string, const Definition *> &defMap,
    const ResolveStructTypeNameFn &resolveStructTypeName,
    const InferStructExprPathFn &resolveExprPath,
    const UninitializedFieldBindingIndex &fieldIndex,
    const ResolveStructFieldSlotFn &resolveStructFieldSlot) {
  std::function<std::string(const Expr &, const LocalMap &)> inferStructExprPath;
  inferStructExprPath = [&](const Expr &exprIn, const LocalMap &localsInExpr) -> std::string {
    const std::string nameStructPath = inferStructPathFromNameExpr(exprIn, localsInExpr);
    if (!nameStructPath.empty() || exprIn.kind == Expr::Kind::Name) {
      return nameStructPath;
    }
    if (exprIn.kind == Expr::Kind::Call) {
      const std::string fieldAccessStruct = inferStructPathFromFieldAccessCall(
          exprIn, localsInExpr, inferStructExprPath, resolveStructFieldSlot);
      if (!fieldAccessStruct.empty() || exprIn.isFieldAccess) {
        return fieldAccessStruct;
      }
      return inferStructPathFromCallTargetWithFieldBindingIndex(
          exprIn,
          resolveExprPath,
          fieldIndex,
          [&](const std::string &defPath) {
            return inferStructReturnPathFromDefinitionMapByCallTargetWithFieldIndex(
                defPath, defMap, resolveStructTypeName, resolveExprPath, fieldIndex);
          });
    }
    return "";
  };
  return inferStructExprPath(expr, localsIn);
}

InferStructExprWithLocalsFn makeInferStructExprPathFromDefinitionMapByCallTargetWithFieldIndex(
    const std::unordered_map<std::string, const Definition *> &defMap,
    const ResolveStructTypeNameFn &resolveStructTypeName,
    const InferStructExprPathFn &resolveExprPath,
    const UninitializedFieldBindingIndex &fieldIndex,
    const ResolveStructFieldSlotFn &resolveStructFieldSlot) {
  return [&](const Expr &expr, const LocalMap &localsIn) {
    return inferStructExprPathFromDefinitionMapByCallTargetWithFieldIndex(
        expr, localsIn, defMap, resolveStructTypeName, resolveExprPath, fieldIndex, resolveStructFieldSlot);
  };
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

bool resolveUninitializedFieldStorageCandidate(const Expr &storage,
                                               const LocalMap &localsIn,
                                               const FindUninitializedFieldTemplateArgFn &findFieldTemplateArg,
                                               const LocalInfo *&receiverOut,
                                               std::string &structPathOut,
                                               std::string &typeTemplateArgOut) {
  receiverOut = nullptr;
  structPathOut.clear();
  typeTemplateArgOut.clear();
  if (!storage.isFieldAccess || storage.args.size() != 1) {
    return false;
  }
  const Expr &receiverExpr = storage.args.front();
  if (receiverExpr.kind != Expr::Kind::Name) {
    return false;
  }
  auto recvIt = localsIn.find(receiverExpr.name);
  if (recvIt == localsIn.end() || recvIt->second.structTypeName.empty()) {
    return false;
  }
  std::string typeTemplateArg;
  if (!findFieldTemplateArg(recvIt->second.structTypeName, storage.name, typeTemplateArg)) {
    return false;
  }
  receiverOut = &recvIt->second;
  structPathOut = recvIt->second.structTypeName;
  typeTemplateArgOut = std::move(typeTemplateArg);
  return true;
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
  out = UninitializedFieldStorageTypeInfo{};
  resolvedOut = false;

  const LocalInfo *receiver = nullptr;
  std::string structPath;
  std::string typeTemplateArg;
  if (!resolveUninitializedFieldStorageCandidate(
          storage, localsIn, findFieldTemplateArg, receiver, structPath, typeTemplateArg)) {
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
  out = UninitializedFieldStorageAccessInfo{};
  resolvedOut = false;

  UninitializedFieldStorageTypeInfo typeInfo;
  if (!resolveUninitializedFieldStorageTypeInfo(storage,
                                                localsIn,
                                                findFieldTemplateArg,
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

bool resolveUninitializedStorageAccess(const Expr &storage,
                                       const LocalMap &localsIn,
                                       const FindUninitializedFieldTemplateArgFn &findFieldTemplateArg,
                                       const ResolveDefinitionNamespacePrefixFn &resolveDefinitionNamespacePrefix,
                                       const ResolveUninitializedFieldTypeInfoFn &resolveUninitializedTypeInfo,
                                       const ResolveStructFieldSlotFn &resolveStructFieldSlot,
                                       UninitializedStorageAccessInfo &out,
                                       bool &resolvedOut,
                                       std::string &error) {
  out = UninitializedStorageAccessInfo{};
  resolvedOut = false;

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
  if (!resolveUninitializedFieldStorageAccess(storage,
                                              localsIn,
                                              findFieldTemplateArg,
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
  return resolveUninitializedStorageAccess(
      storage,
      localsIn,
      [&](const std::string &structPath, const std::string &fieldName, std::string &typeTemplateArgOut) {
        return resolveUninitializedFieldTemplateArg(structPath, fieldName, collectFieldBindings, typeTemplateArgOut);
      },
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
  return resolveUninitializedStorageAccessWithFieldBindings(
      storage,
      localsIn,
      collectFieldBindings,
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
  return [&](const Expr &storage, const LocalMap &localsIn, UninitializedStorageAccessInfo &out, bool &resolvedOut) {
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
