#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>

#include "primec/ast/Ast.h"

namespace primec {

// Canonical std::function callback aliases shared by the emitter, lowerer and
// their testing mirrors. One name per signature; a call site's meaning comes
// from the parameter or field it initializes, not from a bespoke alias. The
// signatures that mention lowerer-local types (LocalMap, LocalInfo) live next
// to those types in src/ir_lowerer/IrLowererSharedTypes.h.
using ExprPredicateFn = std::function<bool(const Expr &)>;
using ExprStringFn = std::function<std::string(const Expr &)>;
using Int32ProviderFn = std::function<int32_t()>;
using SizeProviderFn = std::function<size_t()>;
using ActionFn = std::function<void()>;

} // namespace primec
