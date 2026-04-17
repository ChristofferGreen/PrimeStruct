#pragma once

#include <string>
#include <string_view>

#include "IrLowererSharedTypes.h"
#include "primec/Ast.h"

namespace primec::ir_lowerer::count_access_detail {

bool isExplicitArrayCountName(const Expr &expr);
bool isVectorCountTarget(const Expr &target, const LocalMap &localsIn);
bool isDereferencedCollectionCountTarget(const Expr &countExpr, const Expr &target, const LocalMap &localsIn);
bool resolveVectorHelperAliasName(const Expr &expr, std::string &helperNameOut);
bool resolveMapHelperAliasName(const Expr &expr, std::string &helperNameOut);
bool isVectorBuiltinName(const Expr &expr, const char *name);
bool isMapBuiltinName(const Expr &expr, const char *name);

} // namespace primec::ir_lowerer::count_access_detail
