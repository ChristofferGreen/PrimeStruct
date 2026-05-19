#include "IrLowererStructTypeHelpers.h"

#include <algorithm>
#include <memory>
#include <sstream>

#include "IrLowererBindingTransformHelpers.h"
#include "IrLowererBindingTypeHelpers.h"
#include "IrLowererCallHelpers.h"
#include "IrLowererHelpers.h"
#include "IrLowererSetupTypeCollectionHelpers.h"
#include "IrLowererSetupTypeHelpers.h"
#include "IrLowererTemplateTypeParseHelpers.h"
#include "IrLowererUninitializedTypeHelpers.h"

namespace primec::ir_lowerer {

namespace {

bool isBuiltinVectorTypeName(const std::string &typeName) {
  return isBuiltinCollectionTypeName(typeName, "vector");
}

bool isBuiltinSoaVectorTypeName(const std::string &typeName) {
  return typeName == "soa" "_vector" || typeName == "/soa" "_vector" ||
         typeName == "std/collections/" "soa" "_vector" || typeName == "/std/collections/" "soa" "_vector";
}

bool isExperimentalSoaVectorTypeName(const std::string &typeName) {
  return typeName == "Soa" "Vector" ||
         typeName == "std/collections/experimental" "_soa" "_vector/Soa" "Vector" ||
         typeName == "/std/collections/experimental" "_soa" "_vector/Soa" "Vector" ||
         typeName.rfind("std/collections/experimental" "_soa" "_vector/Soa" "Vector" "__", 0) == 0 ||
         typeName.rfind("/std/collections/experimental" "_soa" "_vector/Soa" "Vector" "__", 0) == 0;
}

bool isInternalSoaColumnTypeName(const std::string &typeName) {
  return typeName == "SoaColumn" ||
         typeName == "std/collections/internal_soa_storage/SoaColumn" ||
         typeName == "/std/collections/internal_soa_storage/SoaColumn" ||
         typeName.rfind("std/collections/internal_soa_storage/SoaColumn__", 0) == 0 ||
         typeName.rfind("/std/collections/internal_soa_storage/SoaColumn__", 0) == 0;
}

bool isExperimentalVectorTypeName(const std::string &typeName) {
  return isExperimentalCollectionTypeName(typeName, "vector", "Vector");
}

bool isVectorTypeName(const std::string &typeName) {
  return isBuiltinVectorTypeName(typeName) || isExperimentalVectorTypeName(typeName);
}

bool isBuiltinMapTypeName(const std::string &typeName) {
  return isBuiltinCollectionTypeName(typeName, "map");
}

bool isExperimentalMapTypeName(const std::string &typeName) {
  const std::string experimentalKeyValueType = experimentalCollectionTypePath("map", "Map", false);
  const std::string rootedExperimentalKeyValueType = experimentalCollectionTypePath("map", "Map");
  const std::string keyValueRoot = collectionTypePath("map") + "/MapValue";
  const std::string keyValueRootNoSlash =
      !keyValueRoot.empty() && keyValueRoot.front() == '/' ? keyValueRoot.substr(1) : keyValueRoot;
  return typeName == experimentalKeyValueType ||
         typeName == rootedExperimentalKeyValueType ||
         typeName.rfind(experimentalKeyValueType + "__", 0) == 0 ||
         typeName.rfind(rootedExperimentalKeyValueType + "__", 0) == 0 ||
         typeName == keyValueRootNoSlash ||
         typeName == keyValueRoot ||
         typeName.rfind(keyValueRootNoSlash + "__", 0) == 0 ||
         typeName.rfind(keyValueRoot + "__", 0) == 0;
}

bool isMapTypeName(const std::string &typeName) {
  return isBuiltinMapTypeName(typeName) || isExperimentalMapTypeName(typeName);
}

bool isKnownStdUiStructAlias(const std::string &typeName) {
  return typeName.find('/') == std::string::npos &&
         (typeName == "CommandList" || typeName == "HtmlCommandList" ||
          typeName == "LayoutTree" || typeName == "LoginFormNodes" ||
          typeName == "Rgba8" || typeName == "UiEventStream");
}

std::string resolveStructSlotLookupPath(
    const std::string &structPath,
    const StructLayoutFieldIndex &fieldIndex) {
  if (fieldIndex.count(structPath) > 0 || !isKnownStdUiStructAlias(structPath)) {
    return structPath;
  }
  const std::string stdUiPath = "/std/ui/" + structPath;
  return fieldIndex.count(stdUiPath) > 0 ? stdUiPath : structPath;
}

std::string normalizeVectorStructPath(const std::string &typeName) {
  if (isBuiltinVectorTypeName(typeName)) {
    return normalizeBuiltinCollectionStructPath("vector");
  }
  if (isBuiltinSoaVectorTypeName(typeName)) {
    return "/soa" "_vector";
  }
  if (typeName == "Vector") {
    return experimentalCollectionTypePath("vector", "Vector");
  }
  if (isExperimentalCollectionTypeName(typeName, "vector", "Vector")) {
    return normalizeExperimentalCollectionTypePath(typeName, "vector", "Vector");
  }
  return typeName;
}

std::string internalSoaColumnStructPathForSoaVectorPath(
    const std::string &columnarVectorStructPath) {
  const std::string slashPrefix = "/std/collections/experimental" "_soa" "_vector/Soa" "Vector";
  const std::string noSlashPrefix = "std/collections/experimental" "_soa" "_vector/Soa" "Vector";
  if (columnarVectorStructPath.rfind(slashPrefix, 0) == 0) {
    return "/std/collections/internal_soa_storage/SoaColumn" +
           columnarVectorStructPath.substr(slashPrefix.size());
  }
  if (columnarVectorStructPath.rfind(noSlashPrefix, 0) == 0) {
    return "/std/collections/internal_soa_storage/SoaColumn" +
           columnarVectorStructPath.substr(noSlashPrefix.size());
  }
  return "/std/collections/internal_soa_storage/SoaColumn";
}

std::string normalizeInternalSoaColumnStructPath(const std::string &structPath) {
  if (structPath == "SoaColumn") {
    return "/std/collections/internal_soa_storage/SoaColumn";
  }
  return structPath.front() == '/' ? structPath : "/" + structPath;
}

std::string normalizeExperimentalSoaVectorStructPath(const std::string &structPath) {
  if (structPath == "Soa" "Vector") {
    return "/std/collections/experimental" "_soa" "_vector/Soa" "Vector";
  }
  return structPath.front() == '/' ? structPath : "/" + structPath;
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
  if (isExperimentalMapStructTypePath(normalizedType)) {
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

  std::ostringstream specializedPath;
  specializedPath << collectionTypePath("map") << "/MapValue"
                  << mangleTemplateTypeArgsSuffix(templateArgs);
  structPathOut = specializedPath.str();
  return true;
}

bool resolveSpecializedExperimentalSoaVectorStructPathFromTypeText(
    std::string typeText,
    std::string &structPathOut) {
  structPathOut.clear();

  typeText = trimTemplateTypeText(typeText);
  while (!typeText.empty()) {
    std::string base;
    std::string argText;
    if (!splitTemplateTypeName(typeText, base, argText)) {
      break;
    }
    base = normalizeCollectionBindingTypeName(trimTemplateTypeText(base));
    std::vector<std::string> args;
    if ((base != "Reference" && base != "Pointer") ||
        !splitTemplateArgs(argText, args) || args.size() != 1) {
      break;
    }
    typeText = trimTemplateTypeText(args.front());
  }

  std::string normalizedType = trimTemplateTypeText(typeText);
  if (normalizedType.rfind("/std/collections/experimental" "_soa" "_vector/Soa" "Vector" "__", 0) == 0) {
    structPathOut = std::move(normalizedType);
    return true;
  }
  if (normalizedType.rfind("std/collections/experimental" "_soa" "_vector/Soa" "Vector" "__", 0) == 0) {
    structPathOut = "/" + normalizedType;
    return true;
  }

  std::string base;
  std::string argText;
  if (!splitTemplateTypeName(normalizedType, base, argText)) {
    return false;
  }
  if (normalizeCollectionBindingTypeName(trimTemplateTypeText(base)) != "soa" "_vector") {
    return false;
  }

  std::vector<std::string> args;
  if (!splitTemplateArgs(argText, args) || args.size() != 1) {
    return false;
  }

  std::string normalizedArg = trimTemplateTypeText(args.front());
  if (!normalizedArg.empty() && normalizedArg.front() == '/') {
    normalizedArg.erase(normalizedArg.begin());
  }
  structPathOut =
      specializedExperimentalSoaVectorStructPathForElementType(normalizedArg);
  return true;
}

std::string resolveSoaVectorFieldStructPath(const std::string &typeName,
                                            const std::string &typeTemplateArg) {
  if (!isBuiltinSoaVectorTypeName(typeName)) {
    std::string specializedStructPath;
    if (resolveSpecializedExperimentalSoaVectorStructPathFromTypeText(
            buildTemplatedTypeName(typeName, typeTemplateArg),
            specializedStructPath)) {
      return specializedStructPath;
    }
  }
  return normalizeVectorStructPath(typeName);
}

bool resolveExperimentalVectorElementKindFromLayoutFields(
    const std::string &vectorStructPath,
    const CollectStructLayoutFieldsFn &collectStructLayoutFields,
    const ValueKindFromTypeNameFn &valueKindFromTypeName,
    LocalInfo::ValueKind &kindOut) {
  kindOut = LocalInfo::ValueKind::Unknown;
  std::vector<StructLayoutFieldInfo> vectorFields;
  if (!collectStructLayoutFields(vectorStructPath, vectorFields)) {
    return false;
  }
  for (const auto &field : vectorFields) {
    if (field.name != "data") {
      continue;
    }
    std::string pointerBase = field.typeName;
    std::string pointerArg = field.typeTemplateArg;
    if (pointerArg.empty()) {
      splitTemplateTypeName(field.typeName, pointerBase, pointerArg);
    }
    if (normalizeCollectionBindingTypeName(trimTemplateTypeText(pointerBase)) !=
        "Pointer") {
      return false;
    }
    std::vector<std::string> pointerArgs;
    if (!splitTemplateArgs(pointerArg, pointerArgs) ||
        pointerArgs.size() != 1) {
      return false;
    }
    std::string elementType = trimTemplateTypeText(pointerArgs.front());
    if (!extractTopLevelUninitializedTypeText(elementType, elementType)) {
      return false;
    }
    kindOut = valueKindFromTypeName(elementType);
    return kindOut != LocalInfo::ValueKind::Unknown;
  }
  return false;
}

std::string resolveExperimentalVectorFieldStructPath(
    const StructLayoutFieldInfo &field) {
  if (!isExperimentalVectorTypeName(field.typeName)) {
    return normalizeVectorStructPath(field.typeName);
  }
  if (!field.typeTemplateArg.empty()) {
    return specializedExperimentalVectorStructPathForElementType(
        trimTemplateTypeText(field.typeTemplateArg));
  }
  return normalizeVectorStructPath(field.typeName);
}

void applySpecializedExperimentalMapFieldLayout(
    const std::string &structPath,
    const std::string &fieldName,
    const CollectStructLayoutFieldsFn &collectStructLayoutFields,
    const ValueKindFromTypeNameFn &valueKindFromTypeName,
    StructSlotFieldInfo &fieldInfo) {
  if (!isExperimentalMapStructTypePath(structPath)) {
    return;
  }
  std::vector<StructLayoutFieldInfo> fields;
  if (!collectStructLayoutFields(structPath, fields)) {
    return;
  }
  for (const auto &field : fields) {
    if (field.name != fieldName || !isExperimentalVectorTypeName(field.typeName)) {
      continue;
    }
    fieldInfo.structPath = resolveExperimentalVectorFieldStructPath(field);
    if (!field.typeTemplateArg.empty()) {
      fieldInfo.valueKind = valueKindFromTypeName(field.typeTemplateArg);
    }
    if (fieldInfo.valueKind == LocalInfo::ValueKind::Unknown &&
        !fieldInfo.structPath.empty()) {
      LocalInfo::ValueKind elementKind = LocalInfo::ValueKind::Unknown;
      if (resolveExperimentalVectorElementKindFromLayoutFields(
              fieldInfo.structPath,
              collectStructLayoutFields,
              valueKindFromTypeName,
              elementKind)) {
        fieldInfo.valueKind = elementKind;
      }
    }
    return;
  }
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
    layout.fields.push_back({"fieldCount", 0, 1, LocalInfo::ValueKind::Int32, ""});
    layout.fields.push_back({"fieldCapacity", 1, 1, LocalInfo::ValueKind::Int32, ""});
    layout.fields.push_back({"data", 2, 1, LocalInfo::ValueKind::Int64, ""});
    layout.fields.push_back({"ownsData", 3, 1, LocalInfo::ValueKind::Bool, ""});
    out = layout;
    return true;
  }
  if (isInternalSoaColumnTypeName(structPath)) {
    StructSlotLayoutInfo layout;
    layout.structPath = normalizeInternalSoaColumnStructPath(structPath);
    layout.totalSlots = 5;
    layout.fields.push_back({"count", 1, 1, LocalInfo::ValueKind::Int32, ""});
    layout.fields.push_back({"capacity", 2, 1, LocalInfo::ValueKind::Int32, ""});
    layout.fields.push_back({"data", 3, 1, LocalInfo::ValueKind::Int64, ""});
    layout.fields.push_back({"ownsData", 4, 1, LocalInfo::ValueKind::Bool, ""});
    out = layout;
    return true;
  }
  if (isExperimentalSoaVectorTypeName(structPath)) {
    StructSlotLayoutInfo layout;
    layout.structPath = normalizeExperimentalSoaVectorStructPath(structPath);
    const std::string storageStructPath =
        internalSoaColumnStructPathForSoaVectorPath(layout.structPath);
    layout.totalSlots = 6;
    layout.fields.push_back({"storage", 1, 5, LocalInfo::ValueKind::Unknown, storageStructPath});
    out = layout;
    return true;
  }
  if (isExperimentalMapTypeName(structPath)) {
    StructSlotLayoutInfo layout;
    layout.structPath = structPath;
    layout.totalSlots = 8;
    StructSlotFieldInfo keysField{
        "keys", 0, 4, LocalInfo::ValueKind::Unknown,
        experimentalCollectionTypePath("vector", "Vector")};
    StructSlotFieldInfo payloadsField{
        "payloads", 4, 4, LocalInfo::ValueKind::Unknown,
        experimentalCollectionTypePath("vector", "Vector")};
    applySpecializedExperimentalMapFieldLayout(
        structPath, "keys", collectStructLayoutFields, valueKindFromTypeName, keysField);
    applySpecializedExperimentalMapFieldLayout(
        structPath, "payloads", collectStructLayoutFields, valueKindFromTypeName, payloadsField);
    layout.fields.push_back(std::move(keysField));
    layout.fields.push_back(std::move(payloadsField));
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
      } else if (normalizeCollectionBindingTypeName(binding.typeName) == "soa" "_vector") {
        info.structPath = resolveSoaVectorFieldStructPath(binding.typeName, binding.typeTemplateArg);
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
          normalizeCollectionBindingTypeName(inlineTemplateBase) == "soa" "_vector") {
        info.structPath = resolveSoaVectorFieldStructPath(inlineTemplateBase, inlineTemplateArg);
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
      if (normalizeCollectionBindingTypeName(binding.typeName) == "soa" "_vector") {
        info.structPath = resolveSoaVectorFieldStructPath(binding.typeName, "");
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
  const std::string resolvedStructPath =
      resolveStructSlotLookupPath(structPath, fieldIndex);
  if (auto defIt = defMap.find(resolvedStructPath);
      defIt != defMap.end() && defIt->second != nullptr) {
    const Definition &def = *defIt->second;
    const bool isSumDefinition =
        std::any_of(def.transforms.begin(), def.transforms.end(),
                    [](const Transform &transform) { return transform.name == "sum"; });
    if (isSumDefinition) {
      int32_t maxPayloadSlots = 0;
      for (const SumVariant &variant : def.sumVariants) {
        if (!variant.hasPayload) {
          continue;
        }
        std::string payloadType = !variant.payloadTypeText.empty()
                                      ? variant.payloadTypeText
                                      : variant.payloadType;
        if (payloadType.empty()) {
          maxPayloadSlots = std::max(maxPayloadSlots, 1);
          continue;
        }
        if (valueKindFromTypeName(payloadType) != LocalInfo::ValueKind::Unknown) {
          maxPayloadSlots = std::max(maxPayloadSlots, 1);
          continue;
        }
        std::string payloadStructPath;
        if (!resolveStructTypeName(payloadType, def.namespacePrefix, payloadStructPath)) {
          maxPayloadSlots = std::max(maxPayloadSlots, 1);
          continue;
        }
        StructSlotLayoutInfo payloadLayout;
        if (!resolveStructSlotLayoutFromDefinitionFieldIndex(payloadStructPath,
                                                             fieldIndex,
                                                             defMap,
                                                             resolveStructTypeName,
                                                             valueKindFromTypeName,
                                                             layoutCache,
                                                             layoutStack,
                                                             payloadLayout,
                                                             error)) {
          return false;
        }
        maxPayloadSlots = std::max(maxPayloadSlots, payloadLayout.totalSlots);
      }
      StructSlotLayoutInfo layout;
      layout.structPath = resolvedStructPath;
      layout.totalSlots = 2 + maxPayloadSlots;
      out = layout;
      layoutCache[resolvedStructPath] = layout;
      return true;
    }
  }
  return resolveStructSlotLayoutFromDefinitionFields(
      resolvedStructPath,
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
  const std::string resolvedStructPath =
      resolveStructSlotLookupPath(structPath, fieldIndex);
  return resolveStructFieldSlotFromDefinitionFields(
      resolvedStructPath,
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
  if (info.kind == LocalInfo::Kind::Value &&
      info.valueKind != LocalInfo::ValueKind::Unknown) {
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
      const std::string pointeeType =
          unwrapTopLevelUninitializedTypeText(typeTemplateArg);
      std::string pointeeBase;
      std::string pointeeArgText;
      if (splitTemplateTypeName(pointeeType, pointeeBase, pointeeArgText)) {
        const std::string normalizedPointeeBase =
            normalizeCollectionBindingTypeName(trimTemplateTypeText(pointeeBase));
        if (normalizedPointeeBase == "Result" || normalizedPointeeBase == "File" ||
            normalizedPointeeBase == "array" || normalizedPointeeBase == "vector" ||
            normalizedPointeeBase == "soa" "_vector" || normalizedPointeeBase == "map" ||
            normalizedPointeeBase == "Buffer") {
          return;
        }
      }
      std::string resolved;
      if (resolveStructTypeName(pointeeType, expr.namespacePrefix, resolved)) {
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
      if (resolveSpecializedExperimentalSoaVectorStructPathFromTypeText(
              buildTemplatedTypeName(transform.name, joinTemplateArgsText(transform.templateArgs)),
              resolved)) {
        return resolved;
      }
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
    if (resolveSpecializedExperimentalSoaVectorStructPathFromTypeText(transform.templateArgs.front(), resolved)) {
      return resolved;
    }
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
    if (resolveSpecializedExperimentalSoaVectorStructPathFromTypeText(
            buildTemplatedTypeName(transform.name, joinTemplateArgsText(transform.templateArgs)),
            resolved)) {
      return resolved;
    }
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
    if (collectionName == "soa" "_vector") {
      return "/soa" "_vector";
    }
  }

  std::string normalizedName = expr.name;
  if (!normalizedName.empty() && normalizedName.front() == '/') {
    normalizedName.erase(normalizedName.begin());
  }
  auto resolveExperimentalKeyValueConstructorStructPath = [&](const std::string &path) -> std::string {
    return inferPublishedExperimentalMapStructPathFromConstructorPath(path);
  };
  if (const std::string experimentalStructPath = resolveExperimentalKeyValueConstructorStructPath(normalizedName);
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
