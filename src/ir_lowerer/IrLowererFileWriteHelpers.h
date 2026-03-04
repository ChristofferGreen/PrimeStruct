#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>

#include "IrLowererSharedTypes.h"
#include "primec/Ast.h"
#include "primec/Ir.h"

namespace primec::ir_lowerer {

using ResolveStringTableTargetForWriteFn = std::function<bool(const Expr &, int32_t &, size_t &)>;
using InferExprKindForWriteFn = std::function<LocalInfo::ValueKind(const Expr &)>;
using EmitExprForWriteFn = std::function<bool(const Expr &)>;
using EmitInstructionForWriteFn = std::function<void(IrOpcode, uint64_t)>;
using AllocTempLocalForWriteFn = std::function<int32_t()>;
using GetInstructionCountForWriteFn = std::function<size_t()>;
using PatchInstructionImmForWriteFn = std::function<void(size_t, int32_t)>;

bool resolveFileOpenModeOpcode(const std::string &mode, IrOpcode &opcodeOut);
bool emitFileOpenCall(const std::string &mode,
                      int32_t stringIndex,
                      const EmitInstructionForWriteFn &emitInstruction,
                      std::string &error);
bool resolveFileWriteValueOpcode(LocalInfo::ValueKind kind, IrOpcode &opcodeOut);
bool emitFileWriteStep(const Expr &arg,
                       int32_t handleIndex,
                       int32_t errorLocal,
                       const ResolveStringTableTargetForWriteFn &resolveStringTableTarget,
                       const InferExprKindForWriteFn &inferExprKind,
                       const EmitExprForWriteFn &emitExpr,
                       const EmitInstructionForWriteFn &emitInstruction,
                       std::string &error);
bool emitFileWriteBytesLoop(const Expr &bytesExpr,
                            int32_t handleIndex,
                            const EmitExprForWriteFn &emitExpr,
                            const AllocTempLocalForWriteFn &allocTempLocal,
                            const EmitInstructionForWriteFn &emitInstruction,
                            const GetInstructionCountForWriteFn &getInstructionCount,
                            const PatchInstructionImmForWriteFn &patchInstructionImm);
void emitFileFlushCall(int32_t handleIndex, const EmitInstructionForWriteFn &emitInstruction);
void emitFileCloseCall(int32_t handleIndex,
                       const AllocTempLocalForWriteFn &allocTempLocal,
                       const EmitInstructionForWriteFn &emitInstruction);

} // namespace primec::ir_lowerer
