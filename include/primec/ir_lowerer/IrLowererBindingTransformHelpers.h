#pragma once

#include <string>
#include <vector>

#include "primec/ast/Ast.h"

namespace primec::ir_lowerer {

bool isBindingMutable(const Expr &expr);
bool isBindingQualifierName(const std::string &name);
bool hasExplicitBindingTypeTransform(const Expr &expr);
bool extractFirstBindingTypeTransform(const Expr &expr,
                                      std::string &typeNameOut,
                                      std::vector<std::string> &templateArgsOut);
bool extractUninitializedTemplateArg(const Expr &expr, std::string &typeTextOut);
bool extractTopLevelUninitializedTypeText(const std::string &typeText, std::string &typeTextOut);
std::string unwrapTopLevelUninitializedTypeText(const std::string &typeText);
// The lowering error for an `alloc` of a type that has no slot; names the way out for a map or a soa value.
std::string unsupportedAllocTargetMessage(const std::string &targetTypeName);
bool isArgsPackBinding(const Expr &expr);
bool isEntryArgsParam(const Expr &param);

} // namespace primec::ir_lowerer
