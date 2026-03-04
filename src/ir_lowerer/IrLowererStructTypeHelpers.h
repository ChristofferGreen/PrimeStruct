#pragma once

#include <functional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "IrLowererSharedTypes.h"
#include "primec/Ast.h"

namespace primec::ir_lowerer {

using ResolveStructTypeNameFn = std::function<bool(const std::string &, const std::string &, std::string &)>;
using ValueKindFromTypeNameFn = std::function<LocalInfo::ValueKind(const std::string &)>;
using InferStructExprPathFn = std::function<std::string(const Expr &)>;
using InferStructExprWithLocalsFn = std::function<std::string(const Expr &, const LocalMap &)>;
using IsKnownStructPathFn = std::function<bool(const std::string &)>;
using InferDefinitionStructReturnPathFn = std::function<std::string(const std::string &)>;

struct StructArrayFieldInfo {
  std::string typeName;
  std::string typeTemplateArg;
  bool isStatic = false;
};

struct StructArrayTypeInfo {
  std::string structPath;
  LocalInfo::ValueKind elementKind = LocalInfo::ValueKind::Unknown;
  int32_t fieldCount = 0;
};

using CollectStructArrayFieldsFn = std::function<bool(const std::string &, std::vector<StructArrayFieldInfo> &)>;

struct StructSlotFieldInfo {
  std::string name;
  int32_t slotOffset = -1;
  int32_t slotCount = 0;
  LocalInfo::ValueKind valueKind = LocalInfo::ValueKind::Unknown;
  std::string structPath;
};

struct StructLayoutFieldInfo {
  std::string name;
  std::string typeName;
  std::string typeTemplateArg;
  bool isStatic = false;
};

struct StructSlotLayoutInfo {
  std::string structPath;
  int32_t totalSlots = 0;
  std::vector<StructSlotFieldInfo> fields;
};

using ResolveStructFieldSlotFn =
    std::function<bool(const std::string &, const std::string &, StructSlotFieldInfo &)>;
using ResolveStructSlotFieldsFn =
    std::function<bool(const std::string &, std::vector<StructSlotFieldInfo> &)>;
using CollectStructLayoutFieldsFn =
    std::function<bool(const std::string &, std::vector<StructLayoutFieldInfo> &)>;
using ResolveDefinitionNamespacePrefixByPathFn =
    std::function<bool(const std::string &, std::string &)>;
using StructSlotLayoutCache = std::unordered_map<std::string, StructSlotLayoutInfo>;

std::string joinTemplateArgsText(const std::vector<std::string> &args);

bool resolveStructTypePathFromScope(
    const std::string &typeName,
    const std::string &namespacePrefix,
    const std::unordered_set<std::string> &structNames,
    const std::unordered_map<std::string, std::string> &importAliases,
    std::string &resolvedOut);
bool resolveStructArrayTypeInfoFromPath(const std::string &structPath,
                                        const CollectStructArrayFieldsFn &collectStructArrayFields,
                                        const ValueKindFromTypeNameFn &valueKindFromTypeName,
                                        StructArrayTypeInfo &out);
bool resolveStructArrayTypeInfoFromBinding(const Expr &expr,
                                           const ResolveStructTypeNameFn &resolveStructTypeName,
                                           const CollectStructArrayFieldsFn &collectStructArrayFields,
                                           const ValueKindFromTypeNameFn &valueKindFromTypeName,
                                           StructArrayTypeInfo &out);
void applyStructArrayInfoFromBinding(const Expr &expr,
                                     const ResolveStructTypeNameFn &resolveStructTypeName,
                                     const CollectStructArrayFieldsFn &collectStructArrayFields,
                                     const ValueKindFromTypeNameFn &valueKindFromTypeName,
                                     LocalInfo &info);
bool resolveStructSlotFieldByName(const std::vector<StructSlotFieldInfo> &fields,
                                  const std::string &fieldName,
                                  StructSlotFieldInfo &out);
bool resolveStructFieldSlotFromLayout(const std::string &structPath,
                                      const std::string &fieldName,
                                      const ResolveStructSlotFieldsFn &resolveStructSlotFields,
                                      StructSlotFieldInfo &out);
bool resolveStructSlotLayoutFromDefinitionFields(
    const std::string &structPath,
    const CollectStructLayoutFieldsFn &collectStructLayoutFields,
    const ResolveDefinitionNamespacePrefixByPathFn &resolveDefinitionNamespacePrefix,
    const ResolveStructTypeNameFn &resolveStructTypeName,
    const ValueKindFromTypeNameFn &valueKindFromTypeName,
    StructSlotLayoutCache &layoutCache,
    std::unordered_set<std::string> &layoutStack,
    StructSlotLayoutInfo &out,
    std::string &error);
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
    std::string &error);
void applyStructValueInfoFromBinding(const Expr &expr,
                                     const ResolveStructTypeNameFn &resolveStructTypeName,
                                     LocalInfo &info);
std::string inferStructReturnPathFromDefinition(
    const Definition &def,
    const ResolveStructTypeNameFn &resolveStructTypeName,
    const InferStructExprPathFn &inferStructExprPath);
std::string inferStructPathFromCallTarget(
    const Expr &expr,
    const InferStructExprPathFn &resolveExprPath,
    const IsKnownStructPathFn &isKnownStructPath,
    const InferDefinitionStructReturnPathFn &inferDefinitionStructReturnPath);
std::string inferStructPathFromNameExpr(const Expr &expr, const LocalMap &localsIn);
std::string inferStructPathFromFieldAccessCall(
    const Expr &expr,
    const LocalMap &localsIn,
    const InferStructExprWithLocalsFn &inferStructExprPath,
    const ResolveStructFieldSlotFn &resolveStructFieldSlot);

} // namespace primec::ir_lowerer
