#pragma once

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <unordered_map>
#include <string_view>
#include <vector>

#include "primec/ir_lowerer/IrLowererSharedTypes.h"
#include "primec/ast/Ast.h"
#include "primec/ir/Ir.h"

namespace primec::ir_lowerer {

struct SemanticProductTargetAdapter;

struct OnErrorHandler {
  std::string errorType;
  std::string handlerPath;
  std::vector<Expr> boundArgs = {};
};

struct ResultReturnInfo {
  bool isResult = false;
  bool hasValue = false;
};

class OnErrorScope {
 public:
  OnErrorScope(std::optional<OnErrorHandler> &targetIn, std::optional<OnErrorHandler> next);
  ~OnErrorScope();

 private:
  std::optional<OnErrorHandler> &target;
  std::optional<OnErrorHandler> previous;
};

class ResultReturnScope {
 public:
  ResultReturnScope(std::optional<ResultReturnInfo> &targetIn, std::optional<ResultReturnInfo> next);
  ~ResultReturnScope();

 private:
  std::optional<ResultReturnInfo> &target;
  std::optional<ResultReturnInfo> previous;
};

void emitFileCloseIfValid(std::vector<IrInstruction> &instructions, int32_t localIndex);
void emitFileScopeCleanup(std::vector<IrInstruction> &instructions, const std::vector<int32_t> &scope);
void emitAllFileScopeCleanup(std::vector<IrInstruction> &instructions,
                             const std::vector<std::vector<int32_t>> &fileScopeStack);
bool emitDestroyHelperFromPtr(
    int32_t valuePtrLocal,
    const std::string &structPath,
    const Definition *destroyHelper,
    const LocalMap &localsIn,
    const std::function<bool(const Expr &, const Definition &, const LocalMap &, bool)> &emitInlineDefinitionCall,
    std::string &error);
bool emitMoveHelperFromPtrs(
    int32_t destPtrLocal,
    int32_t srcPtrLocal,
    const std::string &structPath,
    const Definition *moveHelper,
    const LocalMap &localsIn,
    const std::function<bool(const Expr &, const Definition &, const LocalMap &, bool)> &emitInlineDefinitionCall,
    std::string &error);
struct StructSlotLayoutInfo;
// The helper that destroys a `structPath` value on the stack (`DestroyStack`, else `Destroy`), or
// null when there is none or its body is empty (nothing to run).
const Definition *
findStackDestroyHelper(const std::unordered_map<std::string, const Definition *> &defMap,
                       const std::string &structPath);
// Whether a parameter declaration carries the transform `name` (for example `copy` or `move`).
bool parameterHasTransform(const Expr &param, std::string_view name);
// Clears the drop flag of local `name` when it has one: the value now belongs elsewhere (moved,
// returned), so the local's scope must not destroy it.
void emitReleaseDropFlag(const LocalMap &locals,
                         const std::string &name,
                         const std::function<void(IrOpcode, uint64_t)> &emitInstruction);
// Whether destroying a `structPath` value runs any code: the type has a destroy helper, or one
// of its fields (recursively) does.
bool structNeedsDestroyHelpers(
    const std::string &structPath,
    const std::function<const Definition *(const std::string &)> &findDestroyHelper,
    const std::function<bool(const std::string &, StructSlotLayoutInfo &)>
        &resolveStructSlotLayout);
// Destroys the struct value at `valuePtrLocal`: runs its destroy helper when it has one,
// otherwise destroys its fields that need it, last field first.
bool emitStructDestroyHelpersFromPtr(
    int32_t valuePtrLocal,
    const std::string &structPath,
    const std::function<const Definition *(const std::string &)> &findDestroyHelper,
    const std::function<bool(const std::string &, StructSlotLayoutInfo &)> &resolveStructSlotLayout,
    const Int32ProviderFn &allocTempLocal,
    const std::function<void(IrOpcode, uint64_t)> &emitInstruction,
    const LocalMap &localsIn,
    const std::function<bool(const Expr &, const Definition &, const LocalMap &, bool)>
        &emitInlineDefinitionCall,
    std::string &error);
// Finishes copying a struct value whose slots were already copied from `srcPtrLocal` to
// `destPtrLocal`: runs the type's `Copy` helper when it has one, otherwise the copies of its
// fields that need one (docs/spec/value-lifecycle.md, Copies). Sets `ranHelper` when any ran.
bool emitStructCopyHelpersFromPtrs(
    int32_t destPtrLocal,
    int32_t srcPtrLocal,
    const std::string &structPath,
    const std::function<const Definition *(const std::string &)> &findCopyHelper,
    const std::function<bool(const std::string &, StructSlotLayoutInfo &)> &resolveStructSlotLayout,
    const Int32ProviderFn &allocTempLocal,
    const std::function<void(IrOpcode, uint64_t)> &emitInstruction,
    const LocalMap &localsIn,
    const std::function<bool(const Expr &, const Definition &, const LocalMap &, bool)>
        &emitInlineDefinitionCall,
    bool &ranHelper,
    std::string &error);
bool emitStructCopyFromPtrs(std::vector<IrInstruction> &instructions,
                            int32_t destPtrLocal,
                            int32_t srcPtrLocal,
                            int32_t slotCount);
bool emitStructCopySlots(std::vector<IrInstruction> &instructions,
                         int32_t destBaseLocal,
                         int32_t srcPtrLocal,
                         int32_t slotCount,
                         const Int32ProviderFn &allocTempLocal);
bool emitVectorDestroySlot(
    std::vector<IrInstruction> &instructions,
    int32_t dataPtrLocal,
    int32_t indexLocal,
    LocalInfo::ValueKind indexKind,
    const std::string &structPath,
    const Definition *destroyHelper,
    const LocalMap &localsIn,
    const Int32ProviderFn &allocTempLocal,
    const std::function<bool(const Expr &, const Definition &, const LocalMap &, bool)> &emitInlineDefinitionCall,
    std::string &error);
bool emitVectorMoveSlot(
    std::vector<IrInstruction> &instructions,
    int32_t dataPtrLocal,
    int32_t destIndexLocal,
    int32_t srcIndexLocal,
    LocalInfo::ValueKind indexKind,
    const std::string &structPath,
    const Definition *moveHelper,
    const LocalMap &localsIn,
    const Int32ProviderFn &allocTempLocal,
    const std::function<bool(const Expr &, const Definition &, const LocalMap &, bool)> &emitInlineDefinitionCall,
    std::string &error);
// Clears the ownership flags of a collection value whose slots were copied elsewhere; returns
// false (emitting nothing) for types without known ownership flags.
bool emitDisarmTemporaryStructAfterCopy(const EmitInstructionFn &emitInstruction,
                                        int32_t srcPtrLocal,
                                        const std::string &structPath);
bool shouldDisarmStructCopySourceExpr(const Expr &expr);
bool emitCompareToZero(std::vector<IrInstruction> &instructions,
                       LocalInfo::ValueKind kind,
                       bool equals,
                       std::string &error);
bool emitFloatLiteral(std::vector<IrInstruction> &instructions, const Expr &expr, std::string &error);
bool emitReturnForDefinition(std::vector<IrInstruction> &instructions,
                             const std::string &defPath,
                             const ReturnInfo &returnInfo,
                             std::string &error);
const char *resolveGpuBuiltinLocalName(const std::string &gpuBuiltin);
bool emitGpuBuiltinLoad(
    const std::string &gpuBuiltin,
    const std::function<std::optional<int32_t>(const char *)> &resolveLocalIndex,
    const EmitInstructionFn &emitInstruction,
    std::string &error);
enum class UnaryPassthroughCallResult {
  NotMatched,
  Emitted,
  Error,
};
enum class BufferBuiltinCallEmitResult {
  NotMatched,
  Emitted,
  Error,
};
struct CountedLoopControl {
  int32_t counterLocal = -1;
  LocalInfo::ValueKind countKind = LocalInfo::ValueKind::Unknown;
  size_t checkIndex = 0;
  size_t jumpEndIndex = 0;
};
UnaryPassthroughCallResult tryEmitUnaryPassthroughCall(const Expr &expr,
                                                       const char *callName,
                                                       const ExprPredicateFn &emitExpr,
                                                       std::string &error);
bool resolveCountedLoopKind(LocalInfo::ValueKind inferredKind,
                            bool allowBool,
                            const char *errorMessage,
                            LocalInfo::ValueKind &countKindOut,
                            std::string &error);
bool emitCountedLoopPrologue(
    LocalInfo::ValueKind countKind,
    const Int32ProviderFn &allocTempLocal,
    const SizeProviderFn &instructionCount,
    const EmitInstructionFn &emitInstruction,
    const std::function<void(size_t, int32_t)> &patchInstructionImm,
    const ActionFn &emitLoopCountNegative,
    CountedLoopControl &out,
    std::string &error);
void emitCountedLoopIterationStep(
    const CountedLoopControl &control,
    const EmitInstructionFn &emitInstruction);
void patchCountedLoopEnd(
    const CountedLoopControl &control,
    const SizeProviderFn &instructionCount,
    const std::function<void(size_t, int32_t)> &patchInstructionImm);
bool emitBodyStatements(
    const std::vector<Expr> &bodyStatements,
    const LocalMap &localsIn,
    const std::function<bool(const Expr &, LocalMap &)> &emitStatement);
bool emitBodyStatementsWithFileScope(
    const std::vector<Expr> &bodyStatements,
    const LocalMap &localsIn,
    const std::function<bool(const Expr &, LocalMap &)> &emitStatement,
    const std::function<bool()> &emitAfterBody,
    const ActionFn &pushFileScope,
    const ActionFn &emitCurrentFileScopeCleanup,
    const ActionFn &popFileScope);
bool declareForConditionBinding(
    const Expr &binding,
    LocalMap &locals,
    int32_t &nextLocal,
    const ExprPredicateFn &isBindingMutable,
    const std::function<LocalInfo::Kind(const Expr &)> &bindingKind,
    const ExprPredicateFn &hasExplicitBindingTypeTransform,
    const std::function<LocalInfo::ValueKind(const Expr &, LocalInfo::Kind)> &bindingValueKind,
    const ExprLocalsValueKindFn &inferExprKind,
    const std::function<std::string(const Expr &, const LocalMap &)> &inferStructExprPath,
    const ExprLocalInfoVisitorFn &applyStructArrayInfo,
    const ExprLocalInfoVisitorFn &applyStructValueInfo,
    std::string &error);
bool emitForConditionBindingInit(
    const Expr &binding,
    const LocalMap &localsIn,
    const ExprLocalsPredicateFn &emitExpr,
    const EmitInstructionFn &emitInstruction,
    std::string &error);
struct BufferInitInfo {
  int32_t count = 0;
  LocalInfo::ValueKind elemKind = LocalInfo::ValueKind::Unknown;
  IrOpcode zeroOpcode = IrOpcode::PushI32;
};
bool resolveBufferInitInfo(const Expr &expr,
                           const std::function<LocalInfo::ValueKind(const std::string &)> &resolveValueKind,
                           BufferInitInfo &out,
                           std::string &error);
struct BufferLoadInfo {
  LocalInfo::ValueKind elemKind = LocalInfo::ValueKind::Unknown;
  LocalInfo::ValueKind indexKind = LocalInfo::ValueKind::Unknown;
};
bool resolveBufferLoadInfo(
    const Expr &expr,
    const std::function<std::optional<LocalInfo::ValueKind>(const Expr &)> &resolveBufferElemKind,
    const std::function<LocalInfo::ValueKind(const std::string &)> &resolveValueKind,
    const std::function<LocalInfo::ValueKind(const Expr &)> &inferExprKind,
    BufferLoadInfo &out,
    std::string &error);
bool emitBufferLoadCall(const Expr &expr,
                        LocalInfo::ValueKind indexKind,
                        const ExprPredicateFn &emitExpr,
                        const Int32ProviderFn &allocTempLocal,
                        const EmitInstructionFn &emitInstruction);
BufferBuiltinCallEmitResult tryEmitBufferBuiltinCall(
    const Expr &expr,
    const LocalMap &localsIn,
    const std::function<LocalInfo::ValueKind(const std::string &)> &resolveValueKind,
    const ExprLocalsValueKindFn &inferExprKind,
    const std::function<int32_t(int32_t)> &allocLocalRange,
    const Int32ProviderFn &allocTempLocal,
    const ExprLocalsPredicateFn &emitExpr,
    const EmitInstructionFn &emitInstruction,
    std::string &error,
    const SemanticProductTargetAdapter *semanticProductTargets = nullptr);

} // namespace primec::ir_lowerer
