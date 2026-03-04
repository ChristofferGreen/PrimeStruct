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

bool resolveFileWriteValueOpcode(LocalInfo::ValueKind kind, IrOpcode &opcodeOut);
bool emitFileWriteStep(const Expr &arg,
                       int32_t handleIndex,
                       int32_t errorLocal,
                       const ResolveStringTableTargetForWriteFn &resolveStringTableTarget,
                       const InferExprKindForWriteFn &inferExprKind,
                       const EmitExprForWriteFn &emitExpr,
                       const EmitInstructionForWriteFn &emitInstruction,
                       std::string &error);

} // namespace primec::ir_lowerer
