#pragma once

#include <functional>
#include <string>

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

} // namespace primec::ir_lowerer
