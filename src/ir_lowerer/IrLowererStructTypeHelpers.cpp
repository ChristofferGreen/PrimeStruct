#include "IrLowererStructTypeHelpers.h"

#include <memory>

#include "IrLowererBindingTransformHelpers.h"
#include "IrLowererCallHelpers.h"
#include "IrLowererHelpers.h"
#include "IrLowererSetupTypeHelpers.h"

namespace primec::ir_lowerer {

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
    std::unordered_set<std::string> &structNamesOut) {
  defMapOut.clear();
  structNamesOut.clear();
  defMapOut.reserve(definitions.size());
  structNamesOut.reserve(definitions.size());
  for (const auto &def : definitions) {
    defMapOut.emplace(def.fullPath, &def);
    if (isStructDefinition(def)) {
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
      const auto &field = entry.second[fieldIndex++];

      StructLayoutFieldInfo info;
      info.name = fieldStmt.name;
      info.typeName = field.typeName;
      info.typeTemplateArg = field.typeTemplateArg;
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
  if (importIt != importAliases.end() && structNames.count(importIt->second) > 0) {
    resolvedOut = importIt->second;
    return true;
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
    return importIt->second;
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
      return importIt->second;
    }
    return expr.namespacePrefix + "/" + expr.name;
  }
  std::string root = "/" + expr.name;
  if (defMap.count(root) > 0) {
    return root;
  }
  auto importIt = importAliases.find(expr.name);
  if (importIt != importAliases.end()) {
    return importIt->second;
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

bool resolveStructSlotFieldByName(const std::vector<StructSlotFieldInfo> &fields,
                                  const std::string &fieldName,
                                  StructSlotFieldInfo &out) {
  for (const auto &field : fields) {
    if (field.name == fieldName) {
      out = field;
      return true;
    }
  }
  return false;
}

bool resolveStructFieldSlotFromLayout(const std::string &structPath,
                                      const std::string &fieldName,
                                      const ResolveStructSlotFieldsFn &resolveStructSlotFields,
                                      StructSlotFieldInfo &out) {
  std::vector<StructSlotFieldInfo> fields;
  if (!resolveStructSlotFields(structPath, fields)) {
    return false;
  }
  return resolveStructSlotFieldByName(fields, fieldName, out);
}

bool resolveStructSlotLayoutFromDefinitionFields(
    const std::string &structPath,
    const CollectStructLayoutFieldsFn &collectStructLayoutFields,
    const ResolveDefinitionNamespacePrefixByPathFn &resolveDefinitionNamespacePrefix,
    const ResolveStructTypeNameFn &resolveStructTypeName,
    const ValueKindFromTypeNameFn &valueKindFromTypeName,
    StructSlotLayoutCache &layoutCache,
    std::unordered_set<std::string> &layoutStack,
    StructSlotLayoutInfo &out,
    std::string &error) {
  auto cached = layoutCache.find(structPath);
  if (cached != layoutCache.end()) {
    out = cached->second;
    return true;
  }
  if (!layoutStack.insert(structPath).second) {
    error = "recursive struct layout not supported: " + structPath;
    return false;
  }

  std::string namespacePrefix;
  if (!resolveDefinitionNamespacePrefix(structPath, namespacePrefix)) {
    error = "native backend cannot resolve struct layout: " + structPath;
    layoutStack.erase(structPath);
    return false;
  }

  std::vector<StructLayoutFieldInfo> fields;
  if (!collectStructLayoutFields(structPath, fields)) {
    // Zero-field structs may not produce any enumerated field entries.
    // At this point the definition path already resolved, so treat this as
    // an empty struct layout instead of an error.
    fields.clear();
  }

  StructSlotLayoutInfo layout;
  layout.structPath = structPath;
  int32_t offset = 1;
  layout.fields.reserve(fields.size());
  for (const auto &field : fields) {
    if (field.isStatic) {
      continue;
    }
    StructLayoutFieldInfo binding = field;
    if (binding.typeName == "uninitialized") {
      if (binding.typeTemplateArg.empty()) {
        error = "uninitialized requires a template argument on " + structPath;
        layoutStack.erase(structPath);
        return false;
      }
      std::string base;
      std::string arg;
      if (splitTemplateTypeName(binding.typeTemplateArg, base, arg)) {
        binding.typeName = base;
        binding.typeTemplateArg = arg;
      } else {
        binding.typeName = binding.typeTemplateArg;
        binding.typeTemplateArg.clear();
      }
    }
    if (!binding.typeTemplateArg.empty()) {
      error = "native backend does not support templated struct fields on " + structPath;
      layoutStack.erase(structPath);
      return false;
    }

    StructSlotFieldInfo info;
    info.name = binding.name;
    info.slotOffset = offset;
    LocalInfo::ValueKind kind = valueKindFromTypeName(binding.typeName);
    if (kind != LocalInfo::ValueKind::Unknown) {
      info.valueKind = kind;
      info.slotCount = 1;
    } else {
      std::string nestedStruct;
      if (!resolveStructTypeName(binding.typeName, namespacePrefix, nestedStruct)) {
        error = "native backend does not support struct field type: " + binding.typeName + " on " +
                structPath;
        layoutStack.erase(structPath);
        return false;
      }
      StructSlotLayoutInfo nestedLayout;
      if (!resolveStructSlotLayoutFromDefinitionFields(nestedStruct,
                                                       collectStructLayoutFields,
                                                       resolveDefinitionNamespacePrefix,
                                                       resolveStructTypeName,
                                                       valueKindFromTypeName,
                                                       layoutCache,
                                                       layoutStack,
                                                       nestedLayout,
                                                       error)) {
        layoutStack.erase(structPath);
        return false;
      }
      info.structPath = nestedStruct;
      info.slotCount = nestedLayout.totalSlots;
    }
    layout.fields.push_back(info);
    offset += info.slotCount;
  }
  layout.totalSlots = offset;
  layoutStack.erase(structPath);
  layoutCache.emplace(structPath, layout);
  out = std::move(layout);
  return true;
}

bool resolveStructSlotLayoutFromDefinitionFieldIndex(
    const std::string &structPath,
    const StructLayoutFieldIndex &fieldIndex,
    const std::unordered_map<std::string, const Definition *> &defMap,
    const ResolveStructTypeNameFn &resolveStructTypeName,
    const ValueKindFromTypeNameFn &valueKindFromTypeName,
    StructSlotLayoutCache &layoutCache,
    std::unordered_set<std::string> &layoutStack,
    StructSlotLayoutInfo &out,
    std::string &error) {
  return resolveStructSlotLayoutFromDefinitionFields(
      structPath,
      [&](const std::string &structPathIn, std::vector<StructLayoutFieldInfo> &fieldsOut) {
        return collectStructLayoutFieldsFromIndex(fieldIndex, structPathIn, fieldsOut);
      },
      [&](const std::string &defPathIn, std::string &namespacePrefixOut) {
        return resolveDefinitionNamespacePrefixFromMap(defMap, defPathIn, namespacePrefixOut);
      },
      resolveStructTypeName,
      valueKindFromTypeName,
      layoutCache,
      layoutStack,
      out,
      error);
}

bool resolveStructFieldSlotFromDefinitionFields(
    const std::string &structPath,
    const std::string &fieldName,
    const CollectStructLayoutFieldsFn &collectStructLayoutFields,
    const ResolveDefinitionNamespacePrefixByPathFn &resolveDefinitionNamespacePrefix,
    const ResolveStructTypeNameFn &resolveStructTypeName,
    const ValueKindFromTypeNameFn &valueKindFromTypeName,
    StructSlotLayoutCache &layoutCache,
    std::unordered_set<std::string> &layoutStack,
    StructSlotFieldInfo &out,
    std::string &error) {
  StructSlotLayoutInfo layout;
  if (!resolveStructSlotLayoutFromDefinitionFields(structPath,
                                                   collectStructLayoutFields,
                                                   resolveDefinitionNamespacePrefix,
                                                   resolveStructTypeName,
                                                   valueKindFromTypeName,
                                                   layoutCache,
                                                   layoutStack,
                                                   layout,
                                                   error)) {
    return false;
  }
  return resolveStructSlotFieldByName(layout.fields, fieldName, out);
}

bool resolveStructFieldSlotFromDefinitionFieldIndex(
    const std::string &structPath,
    const std::string &fieldName,
    const StructLayoutFieldIndex &fieldIndex,
    const std::unordered_map<std::string, const Definition *> &defMap,
    const ResolveStructTypeNameFn &resolveStructTypeName,
    const ValueKindFromTypeNameFn &valueKindFromTypeName,
    StructSlotLayoutCache &layoutCache,
    std::unordered_set<std::string> &layoutStack,
    StructSlotFieldInfo &out,
    std::string &error) {
  return resolveStructFieldSlotFromDefinitionFields(
      structPath,
      fieldName,
      [&](const std::string &structPathIn, std::vector<StructLayoutFieldInfo> &fieldsOut) {
        return collectStructLayoutFieldsFromIndex(fieldIndex, structPathIn, fieldsOut);
      },
      [&](const std::string &defPathIn, std::string &namespacePrefixOut) {
        return resolveDefinitionNamespacePrefixFromMap(defMap, defPathIn, namespacePrefixOut);
      },
      resolveStructTypeName,
      valueKindFromTypeName,
      layoutCache,
      layoutStack,
      out,
      error);
}

ResolveStructSlotLayoutFn makeResolveStructSlotLayoutFromDefinitionFieldIndex(
    const StructLayoutFieldIndex &fieldIndex,
    const std::unordered_map<std::string, const Definition *> &defMap,
    const ResolveStructTypeNameFn &resolveStructTypeName,
    const ValueKindFromTypeNameFn &valueKindFromTypeName,
    StructSlotLayoutCache &layoutCache,
    std::unordered_set<std::string> &layoutStack,
    std::string &error) {
  const auto *fieldIndexPtr = &fieldIndex;
  const auto *defMapPtr = &defMap;
  const auto *resolveStructTypeNamePtr = &resolveStructTypeName;
  const auto *valueKindFromTypeNamePtr = &valueKindFromTypeName;
  auto *layoutCachePtr = &layoutCache;
  auto *layoutStackPtr = &layoutStack;
  auto *errorPtr = &error;
  return [fieldIndexPtr,
          defMapPtr,
          resolveStructTypeNamePtr,
          valueKindFromTypeNamePtr,
          layoutCachePtr,
          layoutStackPtr,
          errorPtr](const std::string &structPath, StructSlotLayoutInfo &out) {
    return resolveStructSlotLayoutFromDefinitionFieldIndex(structPath,
                                                           *fieldIndexPtr,
                                                           *defMapPtr,
                                                           *resolveStructTypeNamePtr,
                                                           *valueKindFromTypeNamePtr,
                                                           *layoutCachePtr,
                                                           *layoutStackPtr,
                                                           out,
                                                           *errorPtr);
  };
}

ResolveStructFieldSlotFn makeResolveStructFieldSlotFromDefinitionFieldIndex(
    const StructLayoutFieldIndex &fieldIndex,
    const std::unordered_map<std::string, const Definition *> &defMap,
    const ResolveStructTypeNameFn &resolveStructTypeName,
    const ValueKindFromTypeNameFn &valueKindFromTypeName,
    StructSlotLayoutCache &layoutCache,
    std::unordered_set<std::string> &layoutStack,
    std::string &error) {
  const auto *fieldIndexPtr = &fieldIndex;
  const auto *defMapPtr = &defMap;
  const auto *resolveStructTypeNamePtr = &resolveStructTypeName;
  const auto *valueKindFromTypeNamePtr = &valueKindFromTypeName;
  auto *layoutCachePtr = &layoutCache;
  auto *layoutStackPtr = &layoutStack;
  auto *errorPtr = &error;
  return [fieldIndexPtr,
          defMapPtr,
          resolveStructTypeNamePtr,
          valueKindFromTypeNamePtr,
          layoutCachePtr,
          layoutStackPtr,
          errorPtr](const std::string &structPath, const std::string &fieldName, StructSlotFieldInfo &out) {
    return resolveStructFieldSlotFromDefinitionFieldIndex(structPath,
                                                          fieldName,
                                                          *fieldIndexPtr,
                                                          *defMapPtr,
                                                          *resolveStructTypeNamePtr,
                                                          *valueKindFromTypeNamePtr,
                                                          *layoutCachePtr,
                                                          *layoutStackPtr,
                                                          out,
                                                          *errorPtr);
  };
}

StructSlotResolutionAdapters makeStructSlotResolutionAdapters(
    const StructLayoutFieldIndex &fieldIndex,
    const std::unordered_map<std::string, const Definition *> &defMap,
    const ResolveStructTypeNameFn &resolveStructTypeName,
    const ValueKindFromTypeNameFn &valueKindFromTypeName,
    StructSlotLayoutCache &layoutCache,
    std::unordered_set<std::string> &layoutStack,
    std::string &error) {
  StructSlotResolutionAdapters adapters;
  adapters.resolveStructSlotLayout = makeResolveStructSlotLayoutFromDefinitionFieldIndex(
      fieldIndex, defMap, resolveStructTypeName, valueKindFromTypeName, layoutCache, layoutStack, error);
  adapters.resolveStructFieldSlot = makeResolveStructFieldSlotFromDefinitionFieldIndex(
      fieldIndex, defMap, resolveStructTypeName, valueKindFromTypeName, layoutCache, layoutStack, error);
  return adapters;
}

StructSlotResolutionAdapters makeStructSlotResolutionAdaptersWithOwnedState(
    const StructLayoutFieldIndex &fieldIndex,
    const std::unordered_map<std::string, const Definition *> &defMap,
    const ResolveStructTypeNameFn &resolveStructTypeName,
    const ValueKindFromTypeNameFn &valueKindFromTypeName,
    std::string &error) {
  auto fieldIndexOwned = std::make_shared<StructLayoutFieldIndex>(fieldIndex);
  auto defMapOwned = std::make_shared<std::unordered_map<std::string, const Definition *>>(defMap);
  auto resolveStructTypeNameFn = resolveStructTypeName;
  auto valueKindFromTypeNameFn = valueKindFromTypeName;
  auto *errorPtr = &error;
  auto layoutCache = std::make_shared<StructSlotLayoutCache>();
  auto layoutStack = std::make_shared<std::unordered_set<std::string>>();

  StructSlotResolutionAdapters adapters;
  adapters.resolveStructSlotLayout = [fieldIndexOwned,
                                      defMapOwned,
                                      resolveStructTypeNameFn,
                                      valueKindFromTypeNameFn,
                                      errorPtr,
                                      layoutCache,
                                      layoutStack](
                                         const std::string &structPath, StructSlotLayoutInfo &out) {
    return resolveStructSlotLayoutFromDefinitionFieldIndex(structPath,
                                                           *fieldIndexOwned,
                                                           *defMapOwned,
                                                           resolveStructTypeNameFn,
                                                           valueKindFromTypeNameFn,
                                                           *layoutCache,
                                                           *layoutStack,
                                                           out,
                                                           *errorPtr);
  };
  adapters.resolveStructFieldSlot = [fieldIndexOwned,
                                     defMapOwned,
                                     resolveStructTypeNameFn,
                                     valueKindFromTypeNameFn,
                                     errorPtr,
                                     layoutCache,
                                     layoutStack](
                                        const std::string &structPath,
                                        const std::string &fieldName,
                                        StructSlotFieldInfo &out) {
    return resolveStructFieldSlotFromDefinitionFieldIndex(structPath,
                                                          fieldName,
                                                          *fieldIndexOwned,
                                                          *defMapOwned,
                                                          resolveStructTypeNameFn,
                                                          valueKindFromTypeNameFn,
                                                          *layoutCache,
                                                          *layoutStack,
                                                          out,
                                                          *errorPtr);
  };
  return adapters;
}

StructLayoutResolutionAdapters makeStructLayoutResolutionAdaptersWithOwnedSlotState(
    const StructLayoutFieldIndex &fieldIndex,
    const std::unordered_map<std::string, const Definition *> &defMap,
    const ResolveStructTypeNameFn &resolveStructTypeName,
    const ValueKindFromTypeNameFn &valueKindFromTypeName,
    std::string &error) {
  StructLayoutResolutionAdapters adapters;
  adapters.structArrayInfo =
      makeStructArrayInfoAdapters(fieldIndex, resolveStructTypeName, valueKindFromTypeName);
  adapters.structSlotResolution = makeStructSlotResolutionAdaptersWithOwnedState(
      fieldIndex, defMap, resolveStructTypeName, valueKindFromTypeName, error);
  return adapters;
}

ApplyStructValueInfoFn makeApplyStructValueInfoFromBinding(
    const ResolveStructTypeNameFn &resolveStructTypeName) {
  return [resolveStructTypeName](const Expr &expr, LocalInfo &info) {
    applyStructValueInfoFromBinding(expr, resolveStructTypeName, info);
  };
}

void applyStructValueInfoFromBinding(const Expr &expr,
                                     const ResolveStructTypeNameFn &resolveStructTypeName,
                                     LocalInfo &info) {
  if (!info.structTypeName.empty()) {
    return;
  }

  std::string typeName;
  std::string typeTemplateArg;
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
    if (!transform.templateArgs.empty()) {
      typeTemplateArg = joinTemplateArgsText(transform.templateArgs);
    }
    break;
  }

  if (typeName.empty()) {
    return;
  }

  if (typeName == "Reference" || typeName == "Pointer") {
    if (!typeTemplateArg.empty()) {
      std::string resolved;
      if (resolveStructTypeName(typeTemplateArg, expr.namespacePrefix, resolved)) {
        info.structTypeName = resolved;
      }
    }
    return;
  }

  if (info.kind != LocalInfo::Kind::Value) {
    return;
  }

  std::string resolved;
  if (resolveStructTypeName(typeName, expr.namespacePrefix, resolved)) {
    info.structTypeName = resolved;
  }
}

std::string inferStructReturnPathFromDefinition(
    const Definition &def,
    const ResolveStructTypeNameFn &resolveStructTypeName,
    const InferStructExprPathFn &inferStructExprPath) {
  for (const auto &transform : def.transforms) {
    if (transform.name != "return" || transform.templateArgs.size() != 1) {
      continue;
    }
    std::string resolved;
    if (resolveStructTypeName(transform.templateArgs.front(), def.namespacePrefix, resolved)) {
      return resolved;
    }
    break;
  }

  for (const auto &transform : def.transforms) {
    if (transform.name == "effects" || transform.name == "capabilities") {
      continue;
    }
    if (transform.name == "return") {
      continue;
    }
    if (isBindingQualifierName(transform.name)) {
      continue;
    }
    if (!transform.arguments.empty()) {
      continue;
    }
    std::string resolved;
    if (resolveStructTypeName(transform.name, def.namespacePrefix, resolved)) {
      return resolved;
    }
  }

  if (def.returnExpr.has_value()) {
    std::string inferred = inferStructExprPath(*def.returnExpr);
    if (!inferred.empty()) {
      return inferred;
    }
  }

  for (const auto &stmt : def.statements) {
    if (!isReturnCall(stmt) || stmt.args.size() != 1) {
      continue;
    }
    std::string inferred = inferStructExprPath(stmt.args.front());
    if (!inferred.empty()) {
      return inferred;
    }
  }

  return "";
}

std::string inferStructPathFromCallTarget(
    const Expr &expr,
    const InferStructExprPathFn &resolveExprPath,
    const IsKnownStructPathFn &isKnownStructPath,
    const InferDefinitionStructReturnPathFn &inferDefinitionStructReturnPath) {
  if (expr.kind != Expr::Kind::Call || expr.isMethodCall || expr.isFieldAccess) {
    return "";
  }

  const std::string resolved = resolveExprPath(expr);
  if (isKnownStructPath(resolved)) {
    return resolved;
  }

  return inferDefinitionStructReturnPath(resolved);
}

std::string inferStructPathFromNameExpr(const Expr &expr, const LocalMap &localsIn) {
  if (expr.kind != Expr::Kind::Name) {
    return "";
  }
  auto localIt = localsIn.find(expr.name);
  if (localIt == localsIn.end()) {
    return "";
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
