#pragma once

#include <string>
#include <vector>

#include "primec/Ast.h"

namespace primec::ir_lowerer {

bool isBindingMutable(const Expr &expr);
bool isBindingQualifierName(const std::string &name);
bool hasExplicitBindingTypeTransform(const Expr &expr);
bool extractFirstBindingTypeTransform(const Expr &expr,
                                      std::string &typeNameOut,
                                      std::vector<std::string> &templateArgsOut);
bool extractUninitializedTemplateArg(const Expr &expr, std::string &typeTextOut);
bool isArgsPackBinding(const Expr &expr);
bool isEntryArgsParam(const Expr &param);

} // namespace primec::ir_lowerer
