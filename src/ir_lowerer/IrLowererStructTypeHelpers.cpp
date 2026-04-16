#include "IrLowererStructTypeHelpers.h"

#include "IrLowererBindingTransformHelpers.h"
#include "IrLowererCallHelpers.h"
#include "IrLowererHelpers.h"
#include "IrLowererSemanticProductTargetAdapters.h"
#include "IrLowererSetupTypeHelpers.h"
#include "IrLowererStructFieldBindingHelpers.h"

#include <sstream>

namespace primec::ir_lowerer {

namespace {

std::string normalizeMapImportAliasPath(const std::string &path) {
  if (path.empty() || path.front() == '/') {
    return path;
  }
  if (path.rfind("map/", 0) == 0 || path.rfind("std/collections/map/", 0) == 0) {
    return "/" + path;
  }
  return path;
}

bool isStructLikeSemanticProductCategory(const std::string &category) {
  return category == "struct" || category == "pod" || category == "handle" || category == "gpu_lane";
}

std::string mapKindTypeName(LocalInfo::ValueKind kind) {
  switch (kind) {
  case LocalInfo::ValueKind::Int32:
    return "i32";
  case LocalInfo::ValueKind::Int64:
    return "i64";
  case LocalInfo::ValueKind::UInt64:
    return "u64";
  case LocalInfo::ValueKind::Bool:
    return "bool";
  case LocalInfo::ValueKind::Float32:
    return "f32";
  case LocalInfo::ValueKind::Float64:
    return "f64";
  case LocalInfo::ValueKind::String:
    return "string";
  default:
    return "";
  }
}

std::string inferExperimentalMapStructPathFromKinds(LocalInfo::ValueKind keyKind,
                                                    LocalInfo::ValueKind valueKind) {
  const std::string keyType = mapKindTypeName(keyKind);
  const std::string valueType = mapKindTypeName(valueKind);
  if (keyType.empty() || valueType.empty()) {
    return "";
  }

  const std::string canonicalArgs = keyType + "," + valueType;
  uint64_t hash = 1469598103934665603ULL;
  for (unsigned char ch : canonicalArgs) {
    hash ^= static_cast<uint64_t>(ch);
    hash *= 1099511628211ULL;
  }

  std::ostringstream specializedPath;
  specializedPath << "/std/collections/experimental_map/Map__t" << std::hex << hash;
  return specializedPath.str();
}

} // namespace

std::string joinTemplateArgsText(const std::vector<std::string> &args) {
  std::string out;
  for (size_t i = 0; i < args.size(); ++i) {
    if (i > 0) {
      out += ", ";
    }
    out += args[i];
  }
  return out;
}

StructTypeResolutionAdapters makeStructTypeResolutionAdapters(
    const std::unordered_set<std::string> &structNames,
    const std::unordered_map<std::string, std::string> &importAliases) {
  StructTypeResolutionAdapters adapters;
  adapters.resolveStructTypeName = makeResolveStructTypePathFromScope(structNames, importAliases);
  adapters.applyStructValueInfo = makeApplyStructValueInfoFromBinding(adapters.resolveStructTypeName);
  return adapters;
}

SetupTypeAndStructTypeAdapters makeSetupTypeAndStructTypeAdapters(
    const std::unordered_set<std::string> &structNames,
    const std::unordered_map<std::string, std::string> &importAliases) {
  SetupTypeAndStructTypeAdapters adapters;
  const SetupTypeAdapters setupTypeAdapters = makeSetupTypeAdapters();
  adapters.valueKindFromTypeName = setupTypeAdapters.valueKindFromTypeName;
  adapters.combineNumericKinds = setupTypeAdapters.combineNumericKinds;
  adapters.structTypeResolutionAdapters = makeStructTypeResolutionAdapters(structNames, importAliases);
  return adapters;
}

StructArrayInfoAdapters makeStructArrayInfoAdapters(
    const StructLayoutFieldIndex &fieldIndex,
    const ResolveStructTypeNameFn &resolveStructTypeName,
    const ValueKindFromTypeNameFn &valueKindFromTypeName) {
  StructArrayInfoAdapters adapters;
  adapters.resolveStructArrayTypeInfoFromPath =
      makeResolveStructArrayTypeInfoFromLayoutFieldIndex(fieldIndex, valueKindFromTypeName);
  adapters.applyStructArrayInfo = makeApplyStructArrayInfoFromBindingWithLayoutFieldIndex(
      resolveStructTypeName, fieldIndex, valueKindFromTypeName);
  return adapters;
}

ResolveStructTypeNameFn makeResolveStructTypePathFromScope(
    const std::unordered_set<std::string> &structNames,
    const std::unordered_map<std::string, std::string> &importAliases) {
  return [structNames, importAliases](
             const std::string &typeName, const std::string &namespacePrefix, std::string &resolvedOut) {
    return resolveStructTypePathFromScope(typeName, namespacePrefix, structNames, importAliases, resolvedOut);
  };
}

bool isWildcardImportPath(const std::string &path, std::string &prefixOut) {
  if (path.size() >= 2 && path.compare(path.size() - 2, 2, "/*") == 0) {
    prefixOut = path.substr(0, path.size() - 2);
    return true;
  }
  if (path.find('/', 1) == std::string::npos) {
    prefixOut = path;
    return true;
  }
  return false;
}

void buildDefinitionMapAndStructNames(
    const std::vector<Definition> &definitions,
    std::unordered_map<std::string, const Definition *> &defMapOut,
    std::unordered_set<std::string> &structNamesOut,
    const SemanticProductTargetAdapter *semanticProductTargets) {
  defMapOut.clear();
  structNamesOut.clear();
  defMapOut.reserve(definitions.size());
  for (const auto &def : definitions) {
    defMapOut.emplace(def.fullPath, &def);
  }

  if (semanticProductTargets != nullptr && !semanticProductTargets->typeMetadataByPath.empty()) {
    structNamesOut.reserve(semanticProductTargets->typeMetadataByPath.size());
    for (const auto &[fullPath, typeMetadata] : semanticProductTargets->typeMetadataByPath) {
      if (typeMetadata != nullptr && isStructLikeSemanticProductCategory(typeMetadata->category)) {
        structNamesOut.insert(std::string(fullPath));
      }
    }
    return;
  }

  structNamesOut.reserve(definitions.size());
  for (const auto &def : definitions) {
    if (isStructDefinition(def, semanticProductTargets)) {
      structNamesOut.insert(def.fullPath);
    }
  }
}

void appendStructLayoutFieldsFromFieldBindings(
    const std::unordered_map<std::string, std::vector<LayoutFieldBinding>> &structFieldInfoByName,
    const std::unordered_map<std::string, const Definition *> &defMap,
    const AppendStructLayoutFieldFn &appendStructLayoutField) {
  if (!appendStructLayoutField) {
    return;
  }

  auto isStaticFieldBinding = [](const Expr &fieldExpr) {
    for (const auto &transform : fieldExpr.transforms) {
      if (transform.name == "static") {
        return true;
      }
    }
    return false;
  };

  for (const auto &entry : structFieldInfoByName) {
    auto defIt = defMap.find(entry.first);
    if (defIt == defMap.end() || defIt->second == nullptr) {
      continue;
    }

    const Definition &structDef = *defIt->second;
    std::size_t fieldIndex = 0;
    for (const auto &fieldStmt : structDef.statements) {
      if (!fieldStmt.isBinding || fieldIndex >= entry.second.size()) {
        continue;
      }
      const LayoutFieldBinding *field = nullptr;
      for (const auto &candidate : entry.second) {
        if (!candidate.name.empty() && candidate.name == fieldStmt.name) {
          field = &candidate;
          break;
        }
      }
      if (field == nullptr) {
        field = &entry.second[fieldIndex];
      }
      ++fieldIndex;

      StructLayoutFieldInfo info;
      info.name = fieldStmt.name;
      info.typeName = field->typeName;
      info.typeTemplateArg = field->typeTemplateArg;
      info.isStatic = isStaticFieldBinding(fieldStmt);
      appendStructLayoutField(entry.first, info);
    }
  }
}

std::unordered_map<std::string, std::string> buildImportAliasesFromProgram(
    const std::vector<std::string> &imports,
    const std::vector<Definition> &definitions,
    const std::unordered_map<std::string, const Definition *> &defMap) {
  std::unordered_map<std::string, std::string> importAliases;
  for (const auto &importPath : imports) {
    if (importPath.empty() || importPath[0] != '/') {
      continue;
    }
    if (importPath.size() >= 2 && importPath.compare(importPath.size() - 2, 2, "/*") == 0) {
      std::string prefix = importPath.substr(0, importPath.size() - 2);
      const std::string scopedPrefix = prefix + "/";
      for (const auto &def : definitions) {
        if (def.fullPath.rfind(scopedPrefix, 0) != 0) {
          continue;
        }
        const std::string remainder = def.fullPath.substr(scopedPrefix.size());
        if (remainder.empty() || remainder.find('/') != std::string::npos) {
          continue;
        }
        importAliases.emplace(remainder, def.fullPath);
      }
      continue;
    }
    auto defIt = defMap.find(importPath);
    if (defIt == defMap.end()) {
      continue;
    }
    const std::string remainder = importPath.substr(importPath.find_last_of('/') + 1);
    if (remainder.empty()) {
      continue;
    }
    importAliases.emplace(remainder, importPath);
  }
  return importAliases;
}

bool resolveStructTypePathFromScope(
    const std::string &typeName,
    const std::string &namespacePrefix,
    const std::unordered_set<std::string> &structNames,
    const std::unordered_map<std::string, std::string> &importAliases,
    std::string &resolvedOut) {
  if (typeName.empty()) {
    return false;
  }
  if (typeName[0] == '/') {
    if (structNames.count(typeName) > 0) {
      resolvedOut = typeName;
      return true;
    }
    return false;
  }
  if (!namespacePrefix.empty()) {
    if (namespacePrefix.size() > typeName.size() &&
        namespacePrefix.compare(namespacePrefix.size() - typeName.size() - 1,
                                typeName.size() + 1,
                                "/" + typeName) == 0) {
      if (structNames.count(namespacePrefix) > 0) {
        resolvedOut = namespacePrefix;
        return true;
      }
    }
    std::string candidate = namespacePrefix + "/" + typeName;
    if (structNames.count(candidate) > 0) {
      resolvedOut = candidate;
      return true;
    }
  }
  auto importIt = importAliases.find(typeName);
  if (importIt != importAliases.end()) {
    const std::string normalizedAlias = normalizeMapImportAliasPath(importIt->second);
    if (structNames.count(normalizedAlias) > 0) {
      resolvedOut = normalizedAlias;
      return true;
    }
  }
  std::string root = "/" + typeName;
  if (structNames.count(root) > 0) {
    resolvedOut = root;
    return true;
  }
  return false;
}

std::string resolveStructTypePathCandidateFromScope(
    const std::string &typeName,
    const std::string &namespacePrefix,
    const std::unordered_set<std::string> &structNames,
    const std::unordered_map<std::string, std::string> &importAliases) {
  if (!typeName.empty() && typeName[0] == '/') {
    return typeName;
  }
  std::string resolved = namespacePrefix.empty() ? ("/" + typeName) : (namespacePrefix + "/" + typeName);
  if (structNames.count(resolved) > 0) {
    return resolved;
  }
  auto importIt = importAliases.find(typeName);
  if (importIt != importAliases.end()) {
    return normalizeMapImportAliasPath(importIt->second);
  }
  return resolved;
}

std::string resolveStructLayoutExprPathFromScope(
    const Expr &expr,
    const std::unordered_map<std::string, const Definition *> &defMap,
    const std::unordered_map<std::string, std::string> &importAliases) {
  if (expr.name.empty()) {
    return "";
  }
  if (expr.name[0] == '/') {
    return expr.name;
  }
  if (expr.name.find('/') != std::string::npos) {
    return "/" + expr.name;
  }
  if (!expr.namespacePrefix.empty()) {
    const size_t lastSlash = expr.namespacePrefix.find_last_of('/');
    const std::string suffix = lastSlash == std::string::npos ? expr.namespacePrefix
                                                               : expr.namespacePrefix.substr(lastSlash + 1);
    if (suffix == expr.name && defMap.count(expr.namespacePrefix) > 0) {
      return expr.namespacePrefix;
    }
    std::string prefix = expr.namespacePrefix;
    while (!prefix.empty()) {
      std::string candidate = prefix + "/" + expr.name;
      if (defMap.count(candidate) > 0) {
        return candidate;
      }
      const size_t slash = prefix.find_last_of('/');
      if (slash == std::string::npos) {
        break;
      }
      prefix = prefix.substr(0, slash);
    }
    auto importIt = importAliases.find(expr.name);
    if (importIt != importAliases.end()) {
      return normalizeMapImportAliasPath(importIt->second);
    }
    return expr.namespacePrefix + "/" + expr.name;
  }
  std::string root = "/" + expr.name;
  if (defMap.count(root) > 0) {
    return root;
  }
  auto importIt = importAliases.find(expr.name);
  if (importIt != importAliases.end()) {
    return normalizeMapImportAliasPath(importIt->second);
  }
  return root;
}

bool resolveDefinitionNamespacePrefixFromMap(
    const std::unordered_map<std::string, const Definition *> &defMap,
    const std::string &defPath,
    std::string &namespacePrefixOut) {
  auto defIt = defMap.find(defPath);
  if (defIt == defMap.end() || defIt->second == nullptr) {
    return false;
  }
  namespacePrefixOut = defIt->second->namespacePrefix;
  return true;
}

StructLayoutFieldIndex buildStructLayoutFieldIndex(
    std::size_t structReserveHint,
    const EnumerateStructLayoutFieldsFn &enumerateStructLayoutFields) {
  StructLayoutFieldIndex fieldIndex;
  fieldIndex.reserve(structReserveHint);
  if (!enumerateStructLayoutFields) {
    return fieldIndex;
  }
  enumerateStructLayoutFields([&](const std::string &structPath, const StructLayoutFieldInfo &field) {
    fieldIndex[structPath].push_back(field);
  });
  return fieldIndex;
}

bool collectStructLayoutFieldsFromIndex(const StructLayoutFieldIndex &fieldIndex,
                                        const std::string &structPath,
                                        std::vector<StructLayoutFieldInfo> &out) {
  auto fieldIt = fieldIndex.find(structPath);
  if (fieldIt == fieldIndex.end()) {
    return false;
  }
  out = fieldIt->second;
  return true;
}

bool collectStructArrayFieldsFromLayoutIndex(const StructLayoutFieldIndex &fieldIndex,
                                             const std::string &structPath,
                                             std::vector<StructArrayFieldInfo> &out) {
  std::vector<StructLayoutFieldInfo> layoutFields;
  if (!collectStructLayoutFieldsFromIndex(fieldIndex, structPath, layoutFields)) {
    return false;
  }
  out.clear();
  out.reserve(layoutFields.size());
  for (const auto &field : layoutFields) {
    StructArrayFieldInfo info;
    info.typeName = field.typeName;
    info.typeTemplateArg = field.typeTemplateArg;
    info.isStatic = field.isStatic;
    out.push_back(std::move(info));
  }
  return true;
}

bool resolveStructArrayTypeInfoFromLayoutFieldIndex(const std::string &structPath,
                                                    const StructLayoutFieldIndex &fieldIndex,
                                                    const ValueKindFromTypeNameFn &valueKindFromTypeName,
                                                    StructArrayTypeInfo &out) {
  return resolveStructArrayTypeInfoFromPath(
      structPath,
      [&](const std::string &structPathIn, std::vector<StructArrayFieldInfo> &fieldsOut) {
        return collectStructArrayFieldsFromLayoutIndex(fieldIndex, structPathIn, fieldsOut);
      },
      valueKindFromTypeName,
      out);
}

bool resolveStructArrayTypeInfoFromBindingWithLayoutFieldIndex(
    const Expr &expr,
    const ResolveStructTypeNameFn &resolveStructTypeName,
    const StructLayoutFieldIndex &fieldIndex,
    const ValueKindFromTypeNameFn &valueKindFromTypeName,
    StructArrayTypeInfo &out) {
  return resolveStructArrayTypeInfoFromBinding(
      expr,
      resolveStructTypeName,
      [&](const std::string &structPathIn, std::vector<StructArrayFieldInfo> &fieldsOut) {
        return collectStructArrayFieldsFromLayoutIndex(fieldIndex, structPathIn, fieldsOut);
      },
      valueKindFromTypeName,
      out);
}

ResolveStructArrayTypeInfoFn makeResolveStructArrayTypeInfoFromLayoutFieldIndex(
    const StructLayoutFieldIndex &fieldIndex,
    const ValueKindFromTypeNameFn &valueKindFromTypeName) {
  return [fieldIndex, valueKindFromTypeName](const std::string &structPath, StructArrayTypeInfo &out) {
    return resolveStructArrayTypeInfoFromLayoutFieldIndex(
        structPath, fieldIndex, valueKindFromTypeName, out);
  };
}

bool resolveStructArrayTypeInfoFromPath(const std::string &structPath,
                                        const CollectStructArrayFieldsFn &collectStructArrayFields,
                                        const ValueKindFromTypeNameFn &valueKindFromTypeName,
                                        StructArrayTypeInfo &out) {
  out = StructArrayTypeInfo{};
  std::vector<StructArrayFieldInfo> fields;
  if (!collectStructArrayFields(structPath, fields)) {
    return false;
  }
  LocalInfo::ValueKind elementKind = LocalInfo::ValueKind::Unknown;
  int32_t fieldCount = 0;
  for (const auto &field : fields) {
    if (field.isStatic) {
      continue;
    }
    if (!field.typeTemplateArg.empty()) {
      return false;
    }
    LocalInfo::ValueKind kind = valueKindFromTypeName(field.typeName);
    if (kind == LocalInfo::ValueKind::Unknown || kind == LocalInfo::ValueKind::String) {
      return false;
    }
    if (elementKind == LocalInfo::ValueKind::Unknown) {
      elementKind = kind;
    } else if (elementKind != kind) {
      return false;
    }
    ++fieldCount;
  }
  if (fieldCount == 0 || elementKind == LocalInfo::ValueKind::Unknown) {
    return false;
  }
  out.structPath = structPath;
  out.elementKind = elementKind;
  out.fieldCount = fieldCount;
  return true;
}

bool resolveStructArrayTypeInfoFromBinding(const Expr &expr,
                                           const ResolveStructTypeNameFn &resolveStructTypeName,
                                           const CollectStructArrayFieldsFn &collectStructArrayFields,
                                           const ValueKindFromTypeNameFn &valueKindFromTypeName,
                                           StructArrayTypeInfo &out) {
  out = StructArrayTypeInfo{};
  std::string typeName;
  for (const auto &transform : expr.transforms) {
    if (transform.name == "effects" || transform.name == "capabilities") {
      continue;
    }
    if (isBindingQualifierName(transform.name)) {
      continue;
    }
    if (!transform.arguments.empty()) {
      continue;
    }
    typeName = transform.name;
    break;
  }
  if (typeName.empty() || typeName == "Reference" || typeName == "Pointer") {
    return false;
  }
  std::string resolved;
  if (!resolveStructTypeName(typeName, expr.namespacePrefix, resolved)) {
    return false;
  }
  return resolveStructArrayTypeInfoFromPath(
      resolved, collectStructArrayFields, valueKindFromTypeName, out);
}

void applyStructArrayInfoFromBinding(const Expr &expr,
                                     const ResolveStructTypeNameFn &resolveStructTypeName,
                                     const CollectStructArrayFieldsFn &collectStructArrayFields,
                                     const ValueKindFromTypeNameFn &valueKindFromTypeName,
                                     LocalInfo &info) {
  StructArrayTypeInfo structInfo;
  if (!resolveStructArrayTypeInfoFromBinding(
          expr, resolveStructTypeName, collectStructArrayFields, valueKindFromTypeName, structInfo)) {
    return;
  }
  info.kind = LocalInfo::Kind::Array;
  info.valueKind = structInfo.elementKind;
  info.structTypeName = structInfo.structPath;
  info.structFieldCount = structInfo.fieldCount;
}

void applyStructArrayInfoFromBindingWithLayoutFieldIndex(
    const Expr &expr,
    const ResolveStructTypeNameFn &resolveStructTypeName,
    const StructLayoutFieldIndex &fieldIndex,
    const ValueKindFromTypeNameFn &valueKindFromTypeName,
    LocalInfo &info) {
  applyStructArrayInfoFromBinding(
      expr,
      resolveStructTypeName,
      [&](const std::string &structPathIn, std::vector<StructArrayFieldInfo> &fieldsOut) {
        return collectStructArrayFieldsFromLayoutIndex(fieldIndex, structPathIn, fieldsOut);
      },
      valueKindFromTypeName,
      info);
}

ApplyStructArrayInfoFn makeApplyStructArrayInfoFromBindingWithLayoutFieldIndex(
    const ResolveStructTypeNameFn &resolveStructTypeName,
    const StructLayoutFieldIndex &fieldIndex,
    const ValueKindFromTypeNameFn &valueKindFromTypeName) {
  return [resolveStructTypeName, fieldIndex, valueKindFromTypeName](const Expr &expr, LocalInfo &info) {
    applyStructArrayInfoFromBindingWithLayoutFieldIndex(
        expr, resolveStructTypeName, fieldIndex, valueKindFromTypeName, info);
  };
}

std::string inferStructPathFromNameExpr(const Expr &expr, const LocalMap &localsIn) {
  if (expr.kind != Expr::Kind::Name) {
    return "";
  }
  auto localIt = localsIn.find(expr.name);
  if (localIt == localsIn.end()) {
    return "";
  }
  if (!localIt->second.structTypeName.empty()) {
    return localIt->second.structTypeName;
  }
  const bool isMapLikeLocal =
      localIt->second.kind == LocalInfo::Kind::Map ||
      ((localIt->second.kind == LocalInfo::Kind::Reference || localIt->second.kind == LocalInfo::Kind::Pointer) &&
       (localIt->second.referenceToMap || localIt->second.pointerToMap));
  if (isMapLikeLocal) {
    const std::string inferredMapStruct =
        inferExperimentalMapStructPathFromKinds(localIt->second.mapKeyKind, localIt->second.mapValueKind);
    if (!inferredMapStruct.empty()) {
      return inferredMapStruct;
    }
  }
  if (localIt->second.kind == LocalInfo::Kind::Vector) {
    return localIt->second.isSoaVector ? "/soa_vector" : "/vector";
  }
  if (localIt->second.kind == LocalInfo::Kind::Value && localIt->second.isSoaVector) {
    return "/soa_vector";
  }
  return localIt->second.structTypeName;
}

std::string inferStructPathFromFieldAccessCall(
    const Expr &expr,
    const LocalMap &localsIn,
    const InferStructExprWithLocalsFn &inferStructExprPath,
    const ResolveStructFieldSlotFn &resolveStructFieldSlot) {
  if (expr.kind != Expr::Kind::Call || !expr.isFieldAccess || expr.args.size() != 1) {
    return "";
  }

  const std::string receiverStruct = inferStructExprPath(expr.args.front(), localsIn);
  if (receiverStruct.empty()) {
    return "";
  }

  StructSlotFieldInfo fieldInfo;
  if (!resolveStructFieldSlot(receiverStruct, expr.name, fieldInfo)) {
    return "";
  }

  return fieldInfo.structPath;
}

} // namespace primec::ir_lowerer
