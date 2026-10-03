#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>

#include "primec/ir_lowerer/IrLowererSharedTypes.h"
#include "primec/ast/Ast.h"
#include "primec/ir/Ir.h"
#include "primec/support/CallbackTypes.h"

namespace primec::ir_lowerer {

using ResolveStringTableTargetForWriteFn = std::function<bool(const Expr &, int32_t &, size_t &)>;
using InferExprKindForWriteFn = std::function<LocalInfo::ValueKind(const Expr &)>;
using PatchInstructionImmForWriteFn = std::function<void(size_t, int32_t)>;
using EmitFileWriteStepFn = std::function<bool(const Expr &, int32_t)>;
using ResolveStringTableTargetWithLocalsForWriteFn =
    std::function<bool(const Expr &, const LocalMap &, int32_t &, size_t &)>;
using InferExprKindWithLocalsForWriteFn =
    ExprLocalsValueKindFn;

enum class FileHandleMethodCallEmitResult {
  NotMatched,
  Emitted,
  Error,
};
enum class FileConstructorCallEmitResult {
  NotMatched,
  Emitted,
  Error,
};

bool resolveFileOpenModeOpcode(const std::string &mode, IrOpcode &opcodeOut);
bool resolveDynamicFileOpenModeOpcode(const std::string &mode, IrOpcode &opcodeOut);
bool emitFileOpenCall(const std::string &mode,
                      int32_t stringIndex,
                      const EmitInstructionFn &emitInstruction,
                      std::string &error);
FileConstructorCallEmitResult tryEmitFileConstructorCall(
    const Expr &expr,
    const LocalMap &localsIn,
    const ResolveStringTableTargetWithLocalsForWriteFn &resolveStringTableTarget,
    const InferExprKindWithLocalsForWriteFn &inferExprKind,
    const ExprLocalsPredicateFn &emitExpr,
    const ExprLocalsPredicateFn &isEntryArgsName,
    const EmitInstructionFn &emitInstruction,
    std::string &error);
FileConstructorCallEmitResult tryEmitFileConstructorCall(
    const Expr &expr,
    const LocalMap &localsIn,
    const ResolveStringTableTargetWithLocalsForWriteFn &resolveStringTableTarget,
    const EmitInstructionFn &emitInstruction,
    std::string &error);
bool resolveFileWriteValueOpcode(LocalInfo::ValueKind kind, IrOpcode &opcodeOut);
bool emitFileWriteStep(const Expr &arg,
                       int32_t handleIndex,
                       int32_t errorLocal,
                       const ResolveStringTableTargetForWriteFn &resolveStringTableTarget,
                       const InferExprKindForWriteFn &inferExprKind,
                       const ExprPredicateFn &emitExpr,
                       const EmitInstructionFn &emitInstruction,
                       std::string &error);
bool emitFileWriteCall(const Expr &expr,
                       int32_t handleIndex,
                       const EmitFileWriteStepFn &emitWriteStep,
                       const Int32ProviderFn &allocTempLocal,
                       const EmitInstructionFn &emitInstruction,
                       const SizeProviderFn &getInstructionCount,
                       const PatchInstructionImmForWriteFn &patchInstructionImm);
bool emitFileWriteByteCall(const Expr &expr,
                           int32_t handleIndex,
                           const ExprPredicateFn &emitExpr,
                           const EmitInstructionFn &emitInstruction,
                           std::string &error);
bool emitFileReadByteCall(const Expr &expr,
                          const LocalMap &localsIn,
                          int32_t handleIndex,
                          const Int32ProviderFn &allocTempLocal,
                          const EmitInstructionFn &emitInstruction,
                          const SizeProviderFn &getInstructionCount,
                          const PatchInstructionImmForWriteFn &patchInstructionImm,
                          std::string &error);
bool emitFileReadByteCall(const Expr &expr,
                          const LocalMap &localsIn,
                          int32_t handleIndex,
                          const EmitInstructionFn &emitInstruction,
                          std::string &error);
bool emitFileWriteBytesCall(const Expr &expr,
                            int32_t handleIndex,
                            const ExprPredicateFn &emitExpr,
                            const Int32ProviderFn &allocTempLocal,
                            const EmitInstructionFn &emitInstruction,
                            const SizeProviderFn &getInstructionCount,
                            const PatchInstructionImmForWriteFn &patchInstructionImm,
                            std::string &error);
bool emitFileWriteBytesLoop(const Expr &bytesExpr,
                            int32_t handleIndex,
                            const ExprPredicateFn &emitExpr,
                            const Int32ProviderFn &allocTempLocal,
                            const EmitInstructionFn &emitInstruction,
                            const SizeProviderFn &getInstructionCount,
                            const PatchInstructionImmForWriteFn &patchInstructionImm);
FileHandleMethodCallEmitResult tryEmitFileHandleMethodCall(
    const Expr &expr,
    const LocalMap &localsIn,
    const ExprLocalsPredicateFn &shouldBypassBuiltin,
    const ResolveStringTableTargetWithLocalsForWriteFn &resolveStringTableTarget,
    const InferExprKindWithLocalsForWriteFn &inferExprKind,
    const ExprLocalsPredicateFn &emitExpr,
    const Int32ProviderFn &allocTempLocal,
    const EmitInstructionFn &emitInstruction,
    const SizeProviderFn &getInstructionCount,
    const PatchInstructionImmForWriteFn &patchInstructionImm,
    std::string &error);
void emitFileFlushCall(int32_t handleIndex, const EmitInstructionFn &emitInstruction);
void emitFileCloseCall(int32_t handleIndex,
                       const Int32ProviderFn &allocTempLocal,
                       const EmitInstructionFn &emitInstruction);

} // namespace primec::ir_lowerer
