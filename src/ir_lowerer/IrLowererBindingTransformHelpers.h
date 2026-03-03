#pragma once

#include <string>

#include "primec/Ast.h"

namespace primec::ir_lowerer {

bool isBindingMutable(const Expr &expr);
bool isBindingQualifierName(const std::string &name);
bool hasExplicitBindingTypeTransform(const Expr &expr);
bool extractUninitializedTemplateArg(const Expr &expr, std::string &typeTextOut);
bool isEntryArgsParam(const Expr &param);

} // namespace primec::ir_lowerer
