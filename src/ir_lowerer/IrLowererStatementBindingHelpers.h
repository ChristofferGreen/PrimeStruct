#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "IrLowererSharedTypes.h"
#include "primec/Ast.h"
#include "primec/Ir.h"

namespace primec::ir_lowerer {

struct StructSlotLayoutInfo;
struct UninitializedStorageAccessInfo;
struct PrintBuiltin;

using HasExplicitBindingTypeTransformFn = std::function<bool(const Expr &)>;
using BindingKindFn = std::function<LocalInfo::Kind(const Expr &)>;
using BindingValueKindFn = std::function<LocalInfo::ValueKind(const Expr &, LocalInfo::Kind)>;
using InferBindingExprKindFn = std::function<LocalInfo::ValueKind(const Expr &, const LocalMap &)>;
using IsBindingMutableFn = std::function<bool(const Expr &)>;
using IsFileErrorBindingFn = std::function<bool(const Expr &)>;
using SetReferenceArrayInfoForBindingFn = std::function<void(const Expr &, LocalInfo &)>;
using ApplyStructBindingInfoFn = std::function<void(const Expr &, LocalInfo &)>;
using IsStringBindingFn = std::function<bool(const Expr &)>;
using EmitExprForBindingFn = std::function<bool(const Expr &, const LocalMap &)>;
using IsEntryArgsNameFn = std::function<bool(const Expr &, const LocalMap &)>;
using ResolveUninitializedStorageForStatementFn =
    std::function<bool(const Expr &, const LocalMap &, UninitializedStorageAccessInfo &, bool &)>;
using ResolveStructSlotLayoutForStatementFn = std::function<bool(const std::string &, StructSlotLayoutInfo &)>;
using EmitStructCopyFromPtrsForStatementFn = std::function<bool(int32_t, int32_t, int32_t)>;
using EmitPrintArgForStatementFn = std::function<bool(const Expr &, const LocalMap &, const PrintBuiltin &)>;
using ResolveDefinitionCallForStatementFn = std::function<const Definition *(const Expr &)>;

enum class UninitializedStorageInitDropEmitResult {
  NotMatched,
  Emitted,
  Error,
};
enum class UninitializedStorageTakeEmitResult {
  NotMatched,
  Emitted,
  Error,
};
enum class StatementPrintPathSpaceEmitResult {
  NotMatched,
  Emitted,
  Error,
};

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
bool emitStringStatementBindingInitializer(const Expr &stmt,
                                           const Expr &init,
                                           LocalMap &localsIn,
                                           int32_t &nextLocal,
                                           std::vector<IrInstruction> &instructions,
                                           const IsBindingMutableFn &isBindingMutable,
                                           const std::function<int32_t(const std::string &)> &internString,
                                           const EmitExprForBindingFn &emitExpr,
                                           const InferBindingExprKindFn &inferExprKind,
                                           const std::function<int32_t()> &allocTempLocal,
                                           const IsEntryArgsNameFn &isEntryArgsName,
                                           const std::function<void()> &emitArrayIndexOutOfBounds,
                                           std::string &error);
UninitializedStorageInitDropEmitResult tryEmitUninitializedStorageInitDropStatement(
    const Expr &stmt,
    LocalMap &localsIn,
    std::vector<IrInstruction> &instructions,
    const ResolveUninitializedStorageForStatementFn &resolveUninitializedStorage,
    const EmitExprForBindingFn &emitExpr,
    const ResolveStructSlotLayoutForStatementFn &resolveStructSlotLayout,
    const std::function<int32_t()> &allocTempLocal,
    const EmitStructCopyFromPtrsForStatementFn &emitStructCopyFromPtrs,
    std::string &error);
UninitializedStorageTakeEmitResult tryEmitUninitializedStorageTakeStatement(
    const Expr &stmt,
    const LocalMap &localsIn,
    std::vector<IrInstruction> &instructions,
    const ResolveUninitializedStorageForStatementFn &resolveUninitializedStorage,
    const EmitExprForBindingFn &emitExpr,
    std::string &error);
StatementPrintPathSpaceEmitResult tryEmitPrintPathSpaceStatementBuiltin(
    const Expr &stmt,
    const LocalMap &localsIn,
    const EmitPrintArgForStatementFn &emitPrintArg,
    const ResolveDefinitionCallForStatementFn &resolveDefinitionCall,
    const EmitExprForBindingFn &emitExpr,
    std::vector<IrInstruction> &instructions,
    std::string &error);

} // namespace primec::ir_lowerer
