#pragma once

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "IrLowererSharedTypes.h"
#include "IrLowererStructTypeHelpers.h"
#include "primec/Ast.h"
#include "primec/Ir.h"

namespace primec::ir_lowerer {

using ResolveInlineStructSlotLayoutFn = std::function<bool(const std::string &, StructSlotLayoutInfo &)>;
using InferInlineStructExprKindFn = std::function<LocalInfo::ValueKind(const Expr &, const LocalMap &)>;
using InferInlineStructExprPathFn = std::function<std::string(const Expr &, const LocalMap &)>;
using EmitInlineStructExprFn = std::function<bool(const Expr &, const LocalMap &)>;
using InferInlineStructFieldLocalInfoFn = std::function<bool(const Expr &, const LocalMap &, LocalInfo &, std::string &)>;
using EmitInlineStructCopySlotsFn = std::function<bool(int32_t, int32_t, int32_t)>;
using AllocInlineStructTempLocalFn = std::function<int32_t()>;
using EmitInlineStructInstructionFn = std::function<void(IrOpcode, uint64_t)>;

bool emitInlineStructDefinitionArguments(const std::string &calleePath,
                                         const std::vector<Expr> &params,
                                         const std::vector<const Expr *> &orderedArgs,
                                         const LocalMap &callerLocals,
                                         bool requireValue,
                                         int32_t &nextLocal,
                                         const ResolveInlineStructSlotLayoutFn &resolveStructSlotLayout,
                                         const InferInlineStructExprKindFn &inferExprKind,
                                         const InferInlineStructExprPathFn &inferStructExprPath,
                                         const EmitInlineStructExprFn &emitExpr,
                                         const InferInlineStructFieldLocalInfoFn &inferFieldLocalInfo,
                                         const EmitInlineStructCopySlotsFn &emitStructCopySlots,
                                         const AllocInlineStructTempLocalFn &allocTempLocal,
                                         const EmitInlineStructInstructionFn &emitInstruction,
                                         std::string &error,
                                         std::optional<int32_t> destBaseLocal = std::nullopt);

} // namespace primec::ir_lowerer
