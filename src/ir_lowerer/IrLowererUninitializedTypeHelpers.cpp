#include "IrLowererUninitializedTypeHelpers.h"

#include <vector>

#include "IrLowererHelpers.h"
#include "IrLowererSetupTypeHelpers.h"
#include "IrLowererTemplateTypeParseHelpers.h"

namespace primec::ir_lowerer {

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

} // namespace primec::ir_lowerer
