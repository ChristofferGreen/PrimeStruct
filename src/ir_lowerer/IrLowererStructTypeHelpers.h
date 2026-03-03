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
void applyStructValueInfoFromBinding(const Expr &expr,
                                     const ResolveStructTypeNameFn &resolveStructTypeName,
                                     LocalInfo &info);
std::string inferStructReturnPathFromDefinition(
    const Definition &def,
    const ResolveStructTypeNameFn &resolveStructTypeName,
    const InferStructExprPathFn &inferStructExprPath);

} // namespace primec::ir_lowerer
