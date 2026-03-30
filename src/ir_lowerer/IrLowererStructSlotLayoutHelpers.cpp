#include "IrLowererStructTypeHelpers.h"

#include <memory>
#include <sstream>

#include "IrLowererBindingTransformHelpers.h"
#include "IrLowererBindingTypeHelpers.h"
#include "IrLowererCallHelpers.h"
#include "IrLowererHelpers.h"
#include "IrLowererSetupTypeHelpers.h"
#include "IrLowererTemplateTypeParseHelpers.h"

namespace primec::ir_lowerer {

namespace {

bool isBuiltinVectorTypeName(const std::string &typeName) {
  return typeName == "vector" || typeName == "/vector" || typeName == "std/collections/vector" ||
         typeName == "/std/collections/vector";
}

bool isBuiltinSoaVectorTypeName(const std::string &typeName) {
  return typeName == "soa_vector" || typeName == "/soa_vector" ||
         typeName == "std/collections/soa_vector" || typeName == "/std/collections/soa_vector";
}

bool isExperimentalVectorTypeName(const std::string &typeName) {
  return typeName == "Vector" || typeName == "std/collections/experimental_vector/Vector" ||
         typeName == "/std/collections/experimental_vector/Vector" ||
         typeName.rfind("std/collections/experimental_vector/Vector__", 0) == 0 ||
         typeName.rfind("/std/collections/experimental_vector/Vector__", 0) == 0;
}

bool isVectorTypeName(const std::string &typeName) {
  return isBuiltinVectorTypeName(typeName) || isExperimentalVectorTypeName(typeName);
}

bool isBuiltinMapTypeName(const std::string &typeName) {
  return typeName == "map" || typeName == "/map" || typeName == "std/collections/map" ||
         typeName == "/std/collections/map";
}

bool isExperimentalMapTypeName(const std::string &typeName) {
  return typeName == "Map" || typeName == "std/collections/experimental_map/Map" ||
         typeName == "/std/collections/experimental_map/Map" ||
         typeName.rfind("std/collections/experimental_map/Map__", 0) == 0 ||
         typeName.rfind("/std/collections/experimental_map/Map__", 0) == 0;
}

bool isMapTypeName(const std::string &typeName) {
  return isBuiltinMapTypeName(typeName) || isExperimentalMapTypeName(typeName);
}

std::string normalizeVectorStructPath(const std::string &typeName) {
  if (isBuiltinVectorTypeName(typeName)) {
    return "/vector";
  }
  if (isBuiltinSoaVectorTypeName(typeName)) {
    return "/soa_vector";
  }
  if (typeName == "Vector") {
    return "/std/collections/experimental_vector/Vector";
  }
  if (typeName == "std/collections/experimental_vector/Vector" ||
      typeName.rfind("std/collections/experimental_vector/Vector__", 0) == 0) {
    return "/" + typeName;
  }
  return typeName;
}

std::string buildTemplatedTypeName(const std::string &typeName, const std::string &typeTemplateArg) {
  if (typeTemplateArg.empty()) {
    return trimTemplateTypeText(typeName);
  }
  return trimTemplateTypeText(typeName) + "<" + typeTemplateArg + ">";
}

bool resolveSpecializedExperimentalMapStructPath(const std::string &typeName,
                                                 const std::string &typeTemplateArg,
                                                 std::string &structPathOut) {
  structPathOut.clear();
  std::string normalizedType = trimTemplateTypeText(typeName);
  if (!normalizedType.empty() && normalizedType.front() != '/') {
    normalizedType.insert(normalizedType.begin(), '/');
  }
  if (normalizedType.rfind("/std/collections/experimental_map/Map__", 0) == 0) {
    structPathOut = std::move(normalizedType);
    return true;
  }
  if (!isMapTypeName(typeName) || typeTemplateArg.empty()) {
    return false;
  }
  std::vector<std::string> templateArgs;
  if (!splitTemplateArgs(typeTemplateArg, templateArgs) || templateArgs.size() != 2) {
    return false;
  }
  std::string canonicalArgs = joinTemplateArgsText(templateArgs);
  canonicalArgs.erase(
      std::remove_if(canonicalArgs.begin(), canonicalArgs.end(), [](unsigned char ch) {
        return std::isspace(ch) != 0;
      }),
      canonicalArgs.end());

  uint64_t hash = 1469598103934665603ULL;
  for (unsigned char ch : canonicalArgs) {
    hash ^= static_cast<uint64_t>(ch);
    hash *= 1099511628211ULL;
  }

  std::ostringstream specializedPath;
  specializedPath << "/std/collections/experimental_map/Map__t" << std::hex << hash;
  structPathOut = specializedPath.str();
  return true;
}

} // namespace

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
  if (isBuiltinVectorTypeName(structPath)) {
    StructSlotLayoutInfo layout;
    layout.structPath = normalizeVectorStructPath(structPath);
    layout.totalSlots = 3;
    layout.fields.push_back({"count", 0, 1, LocalInfo::ValueKind::Int32, ""});
    layout.fields.push_back({"capacity", 1, 1, LocalInfo::ValueKind::Int32, ""});
    layout.fields.push_back({"data", 2, 1, LocalInfo::ValueKind::Int64, ""});
    out = layout;
    return true;
  }
  if (isBuiltinSoaVectorTypeName(structPath)) {
    StructSlotLayoutInfo layout;
    layout.structPath = normalizeVectorStructPath(structPath);
    layout.totalSlots = 3;
    layout.fields.push_back({"count", 0, 1, LocalInfo::ValueKind::Int32, ""});
    layout.fields.push_back({"capacity", 1, 1, LocalInfo::ValueKind::Int32, ""});
    layout.fields.push_back({"data", 2, 1, LocalInfo::ValueKind::Int64, ""});
    out = layout;
    return true;
  }
  if (isExperimentalVectorTypeName(structPath)) {
    StructSlotLayoutInfo layout;
    layout.structPath = normalizeVectorStructPath(structPath);
    layout.totalSlots = 4;
    layout.fields.push_back({"count", 0, 1, LocalInfo::ValueKind::Int32, ""});
    layout.fields.push_back({"capacity", 1, 1, LocalInfo::ValueKind::Int32, ""});
    layout.fields.push_back({"data", 2, 1, LocalInfo::ValueKind::Int64, ""});
    layout.fields.push_back({"ownsData", 3, 1, LocalInfo::ValueKind::Bool, ""});
    out = layout;
    return true;
  }
  if (isExperimentalMapTypeName(structPath)) {
    StructSlotLayoutInfo layout;
    layout.structPath = structPath;
    layout.totalSlots = 8;
    layout.fields.push_back({"keys", 0, 4, LocalInfo::ValueKind::Unknown, "/std/collections/experimental_vector/Vector"});
    layout.fields.push_back({"payloads",
                             4,
                             4,
                             LocalInfo::ValueKind::Unknown,
                             "/std/collections/experimental_vector/Vector"});
    out = layout;
    return true;
  }

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
    StructSlotFieldInfo info;
    info.name = binding.name;
    info.slotOffset = offset;
    if (!binding.typeTemplateArg.empty()) {
      if (isVectorTypeName(binding.typeName)) {
        const LocalInfo::ValueKind elementKind = valueKindFromTypeName(binding.typeTemplateArg);
        if (elementKind == LocalInfo::ValueKind::Unknown) {
          error = "native backend only supports numeric/bool/string vector fields on " + structPath;
          layoutStack.erase(structPath);
          return false;
        }
        info.valueKind = elementKind;
        info.structPath = normalizeVectorStructPath(binding.typeName);
        info.slotCount = isExperimentalVectorTypeName(binding.typeName) ? 4 : 3;
      } else if (normalizeCollectionBindingTypeName(binding.typeName) == "soa_vector") {
        info.structPath = normalizeVectorStructPath(binding.typeName);
        info.slotCount = 3;
      } else if (normalizeCollectionBindingTypeName(binding.typeName) == "Result") {
        std::vector<std::string> args;
        if (!splitTemplateArgs(binding.typeTemplateArg, args) || (args.size() != 1 && args.size() != 2)) {
          error = "Result requires one or two template arguments on " + structPath;
          layoutStack.erase(structPath);
          return false;
        }
        info.valueKind = (args.size() == 1) ? LocalInfo::ValueKind::Int32 : LocalInfo::ValueKind::Int64;
        info.slotCount = 1;
      } else if (isMapTypeName(binding.typeName)) {
        std::string nestedStruct;
        if (!resolveSpecializedExperimentalMapStructPath(binding.typeName, binding.typeTemplateArg, nestedStruct) &&
            !resolveStructTypeName(buildTemplatedTypeName(binding.typeName, binding.typeTemplateArg),
                                   namespacePrefix,
                                   nestedStruct)) {
          error = "native backend does not support struct field type: " + binding.typeName + " on " + structPath;
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
      } else if (binding.typeName == "Pointer" || binding.typeName == "Reference") {
        info.valueKind = LocalInfo::ValueKind::Int64;
        info.slotCount = 1;
      } else {
        error = "native backend does not support templated struct fields on " + structPath;
        layoutStack.erase(structPath);
        return false;
      }
    } else {
      std::string inlineTemplateBase;
      std::string inlineTemplateArg;
      if (splitTemplateTypeName(binding.typeName, inlineTemplateBase, inlineTemplateArg) &&
          isVectorTypeName(inlineTemplateBase)) {
        info.valueKind = valueKindFromTypeName(inlineTemplateArg);
        info.structPath = normalizeVectorStructPath(inlineTemplateBase);
        info.slotCount = isExperimentalVectorTypeName(inlineTemplateBase) ? 4 : 3;
        layout.fields.push_back(info);
        offset += info.slotCount;
        continue;
      }
      if (splitTemplateTypeName(binding.typeName, inlineTemplateBase, inlineTemplateArg) &&
          normalizeCollectionBindingTypeName(inlineTemplateBase) == "soa_vector") {
        info.structPath = normalizeVectorStructPath(inlineTemplateBase);
        info.slotCount = 3;
        layout.fields.push_back(info);
        offset += info.slotCount;
        continue;
      }
      if (splitTemplateTypeName(binding.typeName, inlineTemplateBase, inlineTemplateArg) &&
          isMapTypeName(inlineTemplateBase)) {
        std::string nestedStruct;
        if (!resolveSpecializedExperimentalMapStructPath(inlineTemplateBase, inlineTemplateArg, nestedStruct) &&
            !resolveStructTypeName(binding.typeName, namespacePrefix, nestedStruct)) {
          error = "native backend does not support struct field type: " + binding.typeName + " on " + structPath;
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
        layout.fields.push_back(info);
        offset += info.slotCount;
        continue;
      }
      if (isVectorTypeName(binding.typeName)) {
        info.structPath = normalizeVectorStructPath(binding.typeName);
        info.slotCount = isExperimentalVectorTypeName(binding.typeName) ? 4 : 3;
        layout.fields.push_back(info);
        offset += info.slotCount;
        continue;
      }
      if (normalizeCollectionBindingTypeName(binding.typeName) == "soa_vector") {
        info.structPath = normalizeVectorStructPath(binding.typeName);
        info.slotCount = 3;
        layout.fields.push_back(info);
        offset += info.slotCount;
        continue;
      }
      if (isMapTypeName(binding.typeName)) {
        std::string nestedStruct;
        if (!resolveSpecializedExperimentalMapStructPath(binding.typeName, "", nestedStruct) &&
            !resolveStructTypeName(binding.typeName, namespacePrefix, nestedStruct)) {
          error = "native backend does not support struct field type: " + binding.typeName + " on " + structPath;
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
        layout.fields.push_back(info);
        offset += info.slotCount;
        continue;
      }
      LocalInfo::ValueKind kind = valueKindFromTypeName(binding.typeName);
      if (kind != LocalInfo::ValueKind::Unknown) {
        info.valueKind = kind;
        info.slotCount = 1;
      } else {
        std::string nestedStruct;
        if (!resolveStructTypeName(binding.typeName, namespacePrefix, nestedStruct)) {
          error = "native backend does not support struct field type: " + binding.typeName + " on " + structPath;
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
  const ResolveStructTypeNameFn resolveStructTypeNameCopy = resolveStructTypeName;
  const ValueKindFromTypeNameFn valueKindFromTypeNameCopy = valueKindFromTypeName;
  auto *layoutCachePtr = &layoutCache;
  auto *layoutStackPtr = &layoutStack;
  auto *errorPtr = &error;
  return [fieldIndexPtr,
          defMapPtr,
          resolveStructTypeNameCopy,
          valueKindFromTypeNameCopy,
          layoutCachePtr,
          layoutStackPtr,
          errorPtr](const std::string &structPath, StructSlotLayoutInfo &out) {
    return resolveStructSlotLayoutFromDefinitionFieldIndex(structPath,
                                                           *fieldIndexPtr,
                                                           *defMapPtr,
                                                           resolveStructTypeNameCopy,
                                                           valueKindFromTypeNameCopy,
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
  const ResolveStructTypeNameFn resolveStructTypeNameCopy = resolveStructTypeName;
  const ValueKindFromTypeNameFn valueKindFromTypeNameCopy = valueKindFromTypeName;
  auto *layoutCachePtr = &layoutCache;
  auto *layoutStackPtr = &layoutStack;
  auto *errorPtr = &error;
  return [fieldIndexPtr,
          defMapPtr,
          resolveStructTypeNameCopy,
          valueKindFromTypeNameCopy,
          layoutCachePtr,
          layoutStackPtr,
          errorPtr](const std::string &structPath, const std::string &fieldName, StructSlotFieldInfo &out) {
    return resolveStructFieldSlotFromDefinitionFieldIndex(structPath,
                                                          fieldName,
                                                          *fieldIndexPtr,
                                                          *defMapPtr,
                                                          resolveStructTypeNameCopy,
                                                          valueKindFromTypeNameCopy,
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
      if (resolveStructTypeName(unwrapTopLevelUninitializedTypeText(typeTemplateArg),
                                expr.namespacePrefix,
                                resolved)) {
        info.structTypeName = resolved;
      }
    }
    return;
  }

  std::string resolved;
  const std::string fullTypeName = buildTemplatedTypeName(typeName, typeTemplateArg);
  if (!typeTemplateArg.empty() &&
      resolveStructTypeName(fullTypeName, expr.namespacePrefix, resolved)) {
    info.structTypeName = resolved;
    return;
  }
  if (resolveStructTypeName(typeName, expr.namespacePrefix, resolved)) {
    info.structTypeName = resolved;
  }
}

std::string inferStructReturnPathFromDefinition(
    const Definition &def,
    const ResolveStructTypeNameFn &resolveStructTypeName,
    const InferStructExprPathFn &inferStructExprPath) {
  std::function<std::string(const Expr &, const std::unordered_map<std::string, std::string> &)> inferValueStructPath;
  auto inferBindingStructPath = [&](const Expr &bindingExpr,
                                    const std::unordered_map<std::string, std::string> &knownBindings) {
    for (const auto &transform : bindingExpr.transforms) {
      if (transform.name == "effects" || transform.name == "capabilities") {
        continue;
      }
      if (isBindingQualifierName(transform.name)) {
        continue;
      }
      if (!transform.arguments.empty()) {
        continue;
      }
      std::string resolved;
      if (resolveStructTypeName(transform.name, bindingExpr.namespacePrefix, resolved)) {
        return resolved;
      }
      if ((transform.name == "Reference" || transform.name == "Pointer") && transform.templateArgs.size() == 1 &&
          resolveStructTypeName(transform.templateArgs.front(), bindingExpr.namespacePrefix, resolved)) {
        return resolved;
      }
      break;
    }
    if (bindingExpr.args.size() != 1) {
      return std::string{};
    }
    const Expr &initializer = bindingExpr.args.front();
    if (initializer.kind == Expr::Kind::Name) {
      auto bindingIt = knownBindings.find(initializer.name);
      if (bindingIt != knownBindings.end()) {
        return bindingIt->second;
      }
    }
    return inferStructExprPath(initializer);
  };
  inferValueStructPath = [&](const Expr &valueExpr,
                             const std::unordered_map<std::string, std::string> &knownBindings) -> std::string {
    if (valueExpr.kind == Expr::Kind::Name) {
      auto bindingIt = knownBindings.find(valueExpr.name);
      if (bindingIt != knownBindings.end()) {
        return bindingIt->second;
      }
    }
    if (isMatchCall(valueExpr)) {
      Expr lowered;
      std::string loweredError;
      if (lowerMatchToIf(valueExpr, lowered, loweredError)) {
        return inferValueStructPath(lowered, knownBindings);
      }
    }
    if (isIfCall(valueExpr) && valueExpr.args.size() == 3) {
      const Expr *thenValue = getEnvelopeValueExpr(valueExpr.args[1], true);
      const Expr *elseValue = getEnvelopeValueExpr(valueExpr.args[2], true);
      const std::string thenStruct =
          inferValueStructPath(thenValue ? *thenValue : valueExpr.args[1], knownBindings);
      if (thenStruct.empty()) {
        return {};
      }
      const std::string elseStruct =
          inferValueStructPath(elseValue ? *elseValue : valueExpr.args[2], knownBindings);
      return thenStruct == elseStruct ? thenStruct : std::string{};
    }
    if (const Expr *innerValue = getEnvelopeValueExpr(valueExpr, false)) {
      if (isReturnCall(*innerValue) && !innerValue->args.empty()) {
        return inferValueStructPath(innerValue->args.front(), knownBindings);
      }
      return inferValueStructPath(*innerValue, knownBindings);
    }
    return inferStructExprPath(valueExpr);
  };

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
    std::string inferred = inferValueStructPath(*def.returnExpr, {});
    if (!inferred.empty()) {
      return inferred;
    }
  }

  std::unordered_map<std::string, std::string> knownBindings;
  const Expr *lastValueStmt = nullptr;
  for (const auto &stmt : def.statements) {
    if (stmt.isBinding) {
      std::string inferredBinding = inferBindingStructPath(stmt, knownBindings);
      if (!inferredBinding.empty()) {
        knownBindings.emplace(stmt.name, std::move(inferredBinding));
      }
      continue;
    }
    lastValueStmt = &stmt;
    if (!isReturnCall(stmt) || stmt.args.size() != 1) {
      continue;
    }
    std::string inferred = inferValueStructPath(stmt.args.front(), knownBindings);
    if (!inferred.empty()) {
      return inferred;
    }
  }

  if (lastValueStmt != nullptr) {
    return inferValueStructPath(*lastValueStmt, knownBindings);
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

  std::string collectionName;
  if (getBuiltinCollectionName(expr, collectionName) && expr.templateArgs.size() == 1) {
    if (collectionName == "vector") {
      return "/vector";
    }
    if (collectionName == "soa_vector") {
      return "/soa_vector";
    }
  }

  std::string normalizedName = expr.name;
  if (!normalizedName.empty() && normalizedName.front() == '/') {
    normalizedName.erase(normalizedName.begin());
  }
  auto resolveExperimentalMapConstructorStructPath = [&](const std::string &path) -> std::string {
    constexpr std::string_view prefix = "std/collections/experimental_map/";
    if (path.rfind(prefix, 0) != 0) {
      return "";
    }
    const std::string suffix = path.substr(prefix.size());
    auto remap = [&](std::string_view helperStem) -> std::string {
      const std::string helperPrefix = std::string(helperStem) + "__";
      if (suffix.rfind(helperPrefix, 0) != 0) {
        return "";
      }
      return "/std/collections/experimental_map/Map__" + suffix.substr(helperPrefix.size());
    };
    if (std::string structPath = remap("mapNew"); !structPath.empty()) {
      return structPath;
    }
    if (std::string structPath = remap("mapSingle"); !structPath.empty()) {
      return structPath;
    }
    if (std::string structPath = remap("mapDouble"); !structPath.empty()) {
      return structPath;
    }
    if (std::string structPath = remap("mapPair"); !structPath.empty()) {
      return structPath;
    }
    if (std::string structPath = remap("mapTriple"); !structPath.empty()) {
      return structPath;
    }
    if (std::string structPath = remap("mapQuad"); !structPath.empty()) {
      return structPath;
    }
    if (std::string structPath = remap("mapQuint"); !structPath.empty()) {
      return structPath;
    }
    if (std::string structPath = remap("mapSext"); !structPath.empty()) {
      return structPath;
    }
    if (std::string structPath = remap("mapSept"); !structPath.empty()) {
      return structPath;
    }
    if (std::string structPath = remap("mapOct"); !structPath.empty()) {
      return structPath;
    }
    return "";
  };
  if (const std::string experimentalStructPath = resolveExperimentalMapConstructorStructPath(normalizedName);
      !experimentalStructPath.empty() && isKnownStructPath(experimentalStructPath)) {
    return experimentalStructPath;
  }

  const std::string resolved = resolveExprPath(expr);
  if (isKnownStructPath(resolved)) {
    return resolved;
  }

  return inferDefinitionStructReturnPath(resolved);
}

} // namespace primec::ir_lowerer
