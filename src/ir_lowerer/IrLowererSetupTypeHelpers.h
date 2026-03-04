#pragma once

#include <functional>
#include <string>
#include <unordered_map>
#include <unordered_set>

#include "primec/Ast.h"

#include "IrLowererSharedTypes.h"

namespace primec::ir_lowerer {

using ValueKindFromTypeNameFn = std::function<LocalInfo::ValueKind(const std::string &)>;
using CombineNumericKindsFn =
    std::function<LocalInfo::ValueKind(LocalInfo::ValueKind, LocalInfo::ValueKind)>;

struct SetupTypeAdapters {
  ValueKindFromTypeNameFn valueKindFromTypeName;
  CombineNumericKindsFn combineNumericKinds;
};

SetupTypeAdapters makeSetupTypeAdapters();
ValueKindFromTypeNameFn makeValueKindFromTypeName();
CombineNumericKindsFn makeCombineNumericKinds();

LocalInfo::ValueKind valueKindFromTypeName(const std::string &name);
LocalInfo::ValueKind combineNumericKinds(LocalInfo::ValueKind left, LocalInfo::ValueKind right);
LocalInfo::ValueKind comparisonKind(LocalInfo::ValueKind left, LocalInfo::ValueKind right);
std::string typeNameForValueKind(LocalInfo::ValueKind kind);
bool resolveMethodReceiverTypeFromLocalInfo(const LocalInfo &localInfo,
                                            std::string &typeNameOut,
                                            std::string &resolvedTypePathOut);
std::string resolveMethodReceiverTypeNameFromCallExpr(const Expr &receiverCallExpr,
                                                      LocalInfo::ValueKind inferredKind);
std::string resolveMethodReceiverStructTypePathFromCallExpr(
    const Expr &receiverCallExpr,
    const std::string &resolvedReceiverPath,
    const std::unordered_map<std::string, std::string> &importAliases,
    const std::unordered_set<std::string> &structNames);

} // namespace primec::ir_lowerer
