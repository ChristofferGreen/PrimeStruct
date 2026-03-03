#pragma once

#include <string>

#include "IrLowererSharedTypes.h"
#include "primec/Ast.h"

namespace primec::ir_lowerer {

LocalInfo::Kind bindingKindFromTransforms(const Expr &expr);
bool isStringTypeName(const std::string &name);
bool isStringBindingType(const Expr &expr);
bool isFileErrorBindingType(const Expr &expr);
LocalInfo::ValueKind bindingValueKindFromTransforms(const Expr &expr, LocalInfo::Kind kind);
void setReferenceArrayInfoFromTransforms(const Expr &expr, LocalInfo &info);

} // namespace primec::ir_lowerer
