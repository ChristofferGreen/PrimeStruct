#include "IrLowererUninitializedTypeHelpers.h"

#include <vector>

#include "IrLowererBindingTypeHelpers.h"
#include "IrLowererCallHelpers.h"
#include "IrLowererHelpers.h"
#include "IrLowererSetupTypeHelpers.h"
#include "IrLowererStructTypeHelpers.h"
#include "IrLowererTemplateTypeParseHelpers.h"

namespace primec::ir_lowerer {

bool buildProgramEntryReturnRuntimeEntrySetupMathTypeStructAndUninitializedResolutionSetup(
    std::vector<std::string> &stringTable,
    IrFunction &function,
    const Program &program,
    const Definition &entryDef,
    const std::string &entryPath,
    const std::unordered_map<std::string, const Definition *> &defMap,
    const std::unordered_map<std::string, std::string> &importAliases,
    const std::unordered_set<std::string> &structNames,
    std::size_t structReserveHint,
    const EnumerateStructLayoutFieldsFn &enumerateStructLayoutFields,
    EntryReturnRuntimeEntrySetupMathTypeStructAndUninitializedResolutionSetup &out,
    std::string &error) {
  const bool hasMathImport = hasProgramMathImport(program.imports);
  return buildEntryReturnRuntimeEntrySetupMathTypeStructAndUninitializedResolutionSetup(
      stringTable,
      function,
      program,
      entryDef,
      entryPath,
      defMap,
      importAliases,
      hasMathImport,
      structNames,
      structReserveHint,
      enumerateStructLayoutFields,
      out,
      error);
}

bool buildEntryReturnRuntimeEntrySetupMathTypeStructAndUninitializedResolutionSetup(
    std::vector<std::string> &stringTable,
    IrFunction &function,
    const Program &program,
    const Definition &entryDef,
    const std::string &entryPath,
    const std::unordered_map<std::string, const Definition *> &defMap,
    const std::unordered_map<std::string, std::string> &importAliases,
    bool hasMathImport,
    const std::unordered_set<std::string> &structNames,
    std::size_t structReserveHint,
    const EnumerateStructLayoutFieldsFn &enumerateStructLayoutFields,
    EntryReturnRuntimeEntrySetupMathTypeStructAndUninitializedResolutionSetup &out,
    std::string &error) {
  out = EntryReturnRuntimeEntrySetupMathTypeStructAndUninitializedResolutionSetup{};
  if (!analyzeEntryReturnTransforms(entryDef, entryPath, out.entryReturnConfig, error)) {
    return false;
  }
  if (!buildRuntimeEntrySetupMathTypeStructAndUninitializedResolutionSetup(
          stringTable,
          function,
          program,
          entryDef,
          out.entryReturnConfig.returnsVoid,
          defMap,
          importAliases,
          hasMathImport,
          structNames,
          structReserveHint,
          enumerateStructLayoutFields,
          out.runtimeEntrySetupMathTypeStructAndUninitializedResolutionSetup,
          error)) {
    return false;
  }
  return true;
}

bool buildRuntimeEntrySetupMathTypeStructAndUninitializedResolutionSetup(
    std::vector<std::string> &stringTable,
    IrFunction &function,
    const Program &program,
    const Definition &entryDef,
    bool definitionReturnsVoid,
    const std::unordered_map<std::string, const Definition *> &defMap,
    const std::unordered_map<std::string, std::string> &importAliases,
    bool hasMathImport,
    const std::unordered_set<std::string> &structNames,
    std::size_t structReserveHint,
    const EnumerateStructLayoutFieldsFn &enumerateStructLayoutFields,
    RuntimeEntrySetupMathTypeStructAndUninitializedResolutionSetup &out,
    std::string &error) {
  out = RuntimeEntrySetupMathTypeStructAndUninitializedResolutionSetup{};
  out.runtimeErrorAndStringLiteralSetup =
      makeRuntimeErrorAndStringLiteralSetup(stringTable, function, error);
  if (!buildEntrySetupMathTypeStructAndUninitializedResolutionSetup(
          program,
          entryDef,
          definitionReturnsVoid,
          defMap,
          importAliases,
          hasMathImport,
          structNames,
          structReserveHint,
          enumerateStructLayoutFields,
          out.entrySetupMathTypeStructAndUninitializedResolutionSetup,
          error)) {
    return false;
  }
  return true;
}

bool buildEntrySetupMathTypeStructAndUninitializedResolutionSetup(
    const Program &program,
    const Definition &entryDef,
    bool definitionReturnsVoid,
    const std::unordered_map<std::string, const Definition *> &defMap,
    const std::unordered_map<std::string, std::string> &importAliases,
    bool hasMathImport,
    const std::unordered_set<std::string> &structNames,
    std::size_t structReserveHint,
    const EnumerateStructLayoutFieldsFn &enumerateStructLayoutFields,
    EntrySetupMathTypeStructAndUninitializedResolutionSetup &out,
    std::string &error) {
  out = EntrySetupMathTypeStructAndUninitializedResolutionSetup{};
  if (!buildEntryCountCallOnErrorSetup(program,
                                        entryDef,
                                        definitionReturnsVoid,
                                        defMap,
                                        importAliases,
                                        out.entryCountCallOnErrorSetup,
                                        error)) {
    return false;
  }
  const auto &entryCallOnErrorSetup = out.entryCountCallOnErrorSetup.callOnErrorSetup;
  if (!buildSetupMathTypeStructAndUninitializedResolutionSetup(
          hasMathImport,
          structNames,
          importAliases,
          structReserveHint,
          enumerateStructLayoutFields,
          defMap,
          entryCallOnErrorSetup.callResolutionAdapters.resolveExprPath,
          out.setupMathTypeStructAndUninitializedResolutionSetup,
          error)) {
    return false;
  }
  return true;
}

bool buildSetupMathTypeStructAndUninitializedResolutionSetup(
    bool hasMathImport,
    const std::unordered_set<std::string> &structNames,
    const std::unordered_map<std::string, std::string> &importAliases,
    std::size_t structReserveHint,
    const EnumerateStructLayoutFieldsFn &enumerateStructLayoutFields,
    const std::unordered_map<std::string, const Definition *> &defMap,
    const InferStructExprPathFn &resolveExprPath,
    SetupMathTypeStructAndUninitializedResolutionSetup &out,
    std::string &error) {
  out = SetupMathTypeStructAndUninitializedResolutionSetup{};
  out.setupMathAndBindingAdapters = makeSetupMathAndBindingAdapters(hasMathImport);
  if (!buildSetupTypeStructAndUninitializedResolutionSetup(structNames,
                                                           importAliases,
                                                           structReserveHint,
                                                           enumerateStructLayoutFields,
                                                           defMap,
                                                           resolveExprPath,
                                                           out.setupTypeStructAndUninitializedResolutionSetup,
                                                           error)) {
    return false;
  }
  return true;
}

bool buildSetupTypeStructAndUninitializedResolutionSetup(
    const std::unordered_set<std::string> &structNames,
    const std::unordered_map<std::string, std::string> &importAliases,
    std::size_t structReserveHint,
    const EnumerateStructLayoutFieldsFn &enumerateStructLayoutFields,
    const std::unordered_map<std::string, const Definition *> &defMap,
    const InferStructExprPathFn &resolveExprPath,
    SetupTypeStructAndUninitializedResolutionSetup &out,
    std::string &error) {
  out = SetupTypeStructAndUninitializedResolutionSetup{};
  out.setupTypeAndStructTypeAdapters = makeSetupTypeAndStructTypeAdapters(structNames, importAliases);
  const auto &structTypeResolutionAdapters = out.setupTypeAndStructTypeAdapters.structTypeResolutionAdapters;
  if (!buildStructAndUninitializedResolutionSetup(structReserveHint,
                                                  enumerateStructLayoutFields,
                                                  defMap,
                                                  structTypeResolutionAdapters.resolveStructTypeName,
                                                  out.setupTypeAndStructTypeAdapters.valueKindFromTypeName,
                                                  resolveExprPath,
                                                  out.structAndUninitializedResolutionSetup,
                                                  error)) {
    return false;
  }
  return true;
}

bool buildStructAndUninitializedResolutionSetup(
    std::size_t structReserveHint,
    const EnumerateStructLayoutFieldsFn &enumerateStructLayoutFields,
    const std::unordered_map<std::string, const Definition *> &defMap,
    const ResolveStructTypeNameFn &resolveStructTypeName,
    const ValueKindFromTypeNameFn &valueKindFromTypeName,
    const InferStructExprPathFn &resolveExprPath,
    StructAndUninitializedResolutionSetup &out,
    std::string &error) {
  out = StructAndUninitializedResolutionSetup{};
  out.fieldIndexes =
      buildStructAndUninitializedFieldIndexes(structReserveHint, enumerateStructLayoutFields);
  out.structLayoutResolutionAdapters = makeStructLayoutResolutionAdaptersWithOwnedSlotState(
      out.fieldIndexes.structLayoutFieldIndex,
      defMap,
      resolveStructTypeName,
      valueKindFromTypeName,
      error);
  out.uninitializedResolutionAdapters = makeUninitializedResolutionAdapters(
      resolveStructTypeName,
      resolveExprPath,
      out.fieldIndexes.uninitializedFieldBindingIndex,
      defMap,
      out.structLayoutResolutionAdapters.structSlotResolution.resolveStructFieldSlot,
      error);
  return true;
}

UninitializedResolutionAdapters makeUninitializedResolutionAdapters(
    const ResolveStructTypeNameFn &resolveStructTypePath,
    const InferStructExprPathFn &resolveExprPath,
    const UninitializedFieldBindingIndex &fieldIndex,
    const std::unordered_map<std::string, const Definition *> &defMap,
    const ResolveStructFieldSlotFn &resolveStructFieldSlot,
    std::string &error) {
  UninitializedResolutionAdapters adapters;
  adapters.resolveUninitializedTypeInfo = makeResolveUninitializedTypeInfo(resolveStructTypePath, error);
  adapters.resolveUninitializedStorage = makeResolveUninitializedStorageAccessFromDefinitionFieldIndex(
      fieldIndex, defMap, adapters.resolveUninitializedTypeInfo, resolveStructFieldSlot, error);
  adapters.inferStructExprPath = makeInferStructExprPathFromDefinitionMapByCallTargetWithFieldIndex(
      defMap, resolveStructTypePath, resolveExprPath, fieldIndex, resolveStructFieldSlot);
  return adapters;
}

ResolveUninitializedFieldTypeInfoFn makeResolveUninitializedTypeInfo(
    const ResolveStructTypeNameFn &resolveStructTypePath,
    std::string &error) {
  return [resolveStructTypePath, &error](
             const std::string &typeText, const std::string &namespacePrefix, UninitializedTypeInfo &out) {
    return resolveUninitializedTypeInfo(typeText, namespacePrefix, resolveStructTypePath, out, error);
  };
}

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
  std::unordered_set<std::string> visitedDefs;
  inferStructExprPath = [&](const Expr &exprIn, const LocalMap &localsInExpr) -> std::string {
    const std::string nameStructPath = inferStructPathFromNameExpr(exprIn, localsInExpr);
    if (!nameStructPath.empty()) {
      return nameStructPath;
    }
    if (exprIn.kind == Expr::Kind::Name) {
      std::string resolvedTypePath;
      if (resolveStructTypeName(exprIn.name, exprIn.namespacePrefix, resolvedTypePath)) {
        return resolvedTypePath;
      }
      return "";
    }
    if (exprIn.kind == Expr::Kind::Call) {
      if (!exprIn.isMethodCall && isSimpleCallName(exprIn, "dereference") && exprIn.args.size() == 1) {
        return inferStructExprPath(exprIn.args.front(), localsInExpr);
      }
      if (!exprIn.isMethodCall && isSimpleCallName(exprIn, "try") && exprIn.args.size() == 1) {
        const Expr &resultExpr = exprIn.args.front();
        if (resultExpr.kind == Expr::Kind::Name) {
          auto localIt = localsInExpr.find(resultExpr.name);
          if (localIt != localsInExpr.end() && !localIt->second.resultValueStructType.empty()) {
            return localIt->second.resultValueStructType;
          }
        }
        if (resultExpr.kind == Expr::Kind::Call && resultExpr.isMethodCall && resultExpr.args.size() == 2 &&
            resultExpr.args.front().kind == Expr::Kind::Name && resultExpr.args.front().name == "Result" &&
            resultExpr.name == "ok") {
          return inferStructExprPath(resultExpr.args[1], localsInExpr);
        }
      }
      if (exprIn.isFieldAccess && exprIn.args.size() == 1) {
        const std::string receiverStruct = inferStructExprPath(exprIn.args.front(), localsInExpr);
        if (!receiverStruct.empty()) {
          auto fieldIt = fieldIndex.find(receiverStruct);
          if (fieldIt != fieldIndex.end()) {
            for (const auto &field : fieldIt->second) {
              if (!field.isStatic || field.name != exprIn.name) {
                continue;
              }
              std::string staticFieldStruct;
              if (resolveStructTypeName(field.typeName, exprIn.namespacePrefix, staticFieldStruct)) {
                return staticFieldStruct;
              }
              return "";
            }
          }
        }
      }
      std::string accessName;
      if (getBuiltinArrayAccessName(exprIn, accessName) && exprIn.args.size() == 2) {
        const Expr &accessReceiver = exprIn.args.front();
        if (accessReceiver.kind == Expr::Kind::Name) {
          auto receiverIt = localsInExpr.find(accessReceiver.name);
          if (receiverIt != localsInExpr.end() && receiverIt->second.isArgsPack &&
              !receiverIt->second.structTypeName.empty()) {
            return receiverIt->second.structTypeName;
          }
        }
      }
      const std::string fieldAccessStruct = inferStructPathFromFieldAccessCall(
          exprIn, localsInExpr, inferStructExprPath, resolveStructFieldSlot);
      if (!fieldAccessStruct.empty() || exprIn.isFieldAccess) {
        return fieldAccessStruct;
      }
      if (!exprIn.isMethodCall) {
        const std::string resolvedPath = resolveExprPath(exprIn);
        auto defIt = defMap.find(resolvedPath);
        if (defIt != defMap.end() && defIt->second != nullptr) {
          if (isStructDefinition(*defIt->second)) {
            return resolvedPath;
          }
          const std::string indexedStruct = inferStructPathFromCallTargetWithFieldBindingIndex(
              exprIn,
              resolveExprPath,
              fieldIndex,
              [&](const std::string &defPath) {
                return inferStructReturnPathFromDefinitionMapByCallTargetWithFieldIndex(
                    defPath, defMap, resolveStructTypeName, resolveExprPath, fieldIndex);
              });
          if (!indexedStruct.empty()) {
            return indexedStruct;
          }
          if (visitedDefs.insert(resolvedPath).second) {
            const Definition &nestedDef = *defIt->second;
            std::string returnedStruct = inferStructReturnPathFromDefinition(
                nestedDef,
                resolveStructTypeName,
                [&](const Expr &nestedExpr) { return inferStructExprPath(nestedExpr, localsInExpr); });
            visitedDefs.erase(resolvedPath);
            if (!returnedStruct.empty()) {
              return returnedStruct;
            }
          }
        }
      }
      if (exprIn.isMethodCall && !exprIn.args.empty()) {
        const std::string receiverStruct = inferStructExprPath(exprIn.args.front(), localsInExpr);
        if (!receiverStruct.empty()) {
          const std::string methodPath = receiverStruct + "/" + exprIn.name;
          if (defMap.count(methodPath) > 0) {
            const std::string methodStruct = inferStructReturnPathFromDefinitionMapByCallTargetWithFieldIndex(
                methodPath, defMap, resolveStructTypeName, resolveExprPath, fieldIndex);
            if (!methodStruct.empty()) {
              return methodStruct;
            }
          }
        }
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
  return [defMap, resolveStructTypeName, resolveExprPath, fieldIndex, resolveStructFieldSlot](
             const Expr &expr, const LocalMap &localsIn) {
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

  if (storage.kind == Expr::Kind::Call && isSimpleCallName(storage, "dereference") && storage.args.size() == 1) {
    const Expr &pointerExpr = storage.args.front();
    if (pointerExpr.kind == Expr::Kind::Call && isSimpleCallName(pointerExpr, "location") &&
        pointerExpr.args.size() == 1) {
      return resolveUninitializedStorageAccess(pointerExpr.args.front(),
                                               localsIn,
                                               findFieldTemplateArg,
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
