#pragma once

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <unordered_map>

#include "IrLowererCallHelpers.h"
#include "IrLowererInlineParamHelpers.h"
#include "primec/ast/Ast.h"
#include "primec/frontend/SemanticProduct.h"
#include "primec/ir/Ir.h"

namespace primec::ir_lowerer {

std::optional<bool> tryLowerEmitExprCollectionHelpers(
    std::unordered_map<std::string, const Definition *> &defMap,
    IrFunction &function,
    int32_t &nextLocal,
    const std::function<bool(const Expr &, const LocalMap &)> &emitExpr,
    const std::function<int32_t()> &allocTempLocal,
    const std::function<bool(int32_t, int32_t, int32_t)> &emitStructCopySlots,
    const ResolveDefinitionCallFn &resolveDefinitionCall,
    const ResolveExprPathFn &resolveExprPath,
    const ResolveInlineParameterStructSlotLayoutFn &resolveStructSlotLayout,
    const std::function<LocalInfo::ValueKind(const Expr &, const LocalMap &)> &inferExprKind,
    const std::function<std::string(const Expr &, const LocalMap &)> &inferStructExprPath,
    const std::function<void()> &emitArrayIndexOutOfBounds,
    const CallResolutionAdapters &callResolutionAdapters,
    const SemanticProgram *semanticProgram,
    const Expr &expr,
    const LocalMap &localsIn,
    std::string &error);

} // namespace primec::ir_lowerer
