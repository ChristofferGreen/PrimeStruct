#include "IrLowererStructTypeHelpers.h"

#include "IrLowererBindingTransformHelpers.h"
#include "IrLowererHelpers.h"

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

} // namespace primec::ir_lowerer
