#pragma once

#include <cstdint>
#include <functional>
#include <string>

#include "IrLowererSharedTypes.h"
#include "primec/Ast.h"
#include "primec/Ir.h"

namespace primec::ir_lowerer {

using HasExplicitBindingTypeTransformFn = std::function<bool(const Expr &)>;
using BindingKindFn = std::function<LocalInfo::Kind(const Expr &)>;
using BindingValueKindFn = std::function<LocalInfo::ValueKind(const Expr &, LocalInfo::Kind)>;
using InferBindingExprKindFn = std::function<LocalInfo::ValueKind(const Expr &, const LocalMap &)>;
using IsBindingMutableFn = std::function<bool(const Expr &)>;
using IsFileErrorBindingFn = std::function<bool(const Expr &)>;
using SetReferenceArrayInfoForBindingFn = std::function<void(const Expr &, LocalInfo &)>;
using ApplyStructBindingInfoFn = std::function<void(const Expr &, LocalInfo &)>;
using IsStringBindingFn = std::function<bool(const Expr &)>;

struct StatementBindingTypeInfo {
  LocalInfo::Kind kind = LocalInfo::Kind::Value;
  LocalInfo::ValueKind valueKind = LocalInfo::ValueKind::Unknown;
  LocalInfo::ValueKind mapKeyKind = LocalInfo::ValueKind::Unknown;
  LocalInfo::ValueKind mapValueKind = LocalInfo::ValueKind::Unknown;
};

StatementBindingTypeInfo inferStatementBindingTypeInfo(const Expr &stmt,
                                                       const Expr &init,
                                                       const LocalMap &localsIn,
                                                       const HasExplicitBindingTypeTransformFn &hasExplicitBindingTypeTransform,
                                                       const BindingKindFn &bindingKind,
                                                       const BindingValueKindFn &bindingValueKind,
                                                       const InferBindingExprKindFn &inferExprKind);

bool selectUninitializedStorageZeroInstruction(LocalInfo::Kind kind,
                                               LocalInfo::ValueKind valueKind,
                                               const std::string &bindingName,
                                               IrOpcode &zeroOp,
                                               uint64_t &zeroImm,
                                               std::string &error);
bool inferCallParameterLocalInfo(const Expr &param,
                                 const LocalMap &localsForKindInference,
                                 const IsBindingMutableFn &isBindingMutable,
                                 const HasExplicitBindingTypeTransformFn &hasExplicitBindingTypeTransform,
                                 const BindingKindFn &bindingKind,
                                 const BindingValueKindFn &bindingValueKind,
                                 const InferBindingExprKindFn &inferExprKind,
                                 const IsFileErrorBindingFn &isFileErrorBinding,
                                 const SetReferenceArrayInfoForBindingFn &setReferenceArrayInfo,
                                 const ApplyStructBindingInfoFn &applyStructArrayInfo,
                                 const ApplyStructBindingInfoFn &applyStructValueInfo,
                                 const IsStringBindingFn &isStringBinding,
                                 LocalInfo &infoOut,
                                 std::string &error);

} // namespace primec::ir_lowerer
