#pragma once

#include <string>
#include <vector>

#include "primec/ast/Ast.h"
#include "primec/support/TypeText.h"

namespace primec::ir_printer {

enum class ReturnKind { Unknown, Int, Int64, UInt64, UInt8, Float32, Float64, Bool, String, Void, Array };

std::string joinTemplateArgs(const std::vector<std::string> &args);
using primec::splitTemplateTypeName;
using primec::splitTopLevelTemplateArgs;
std::string bindingTypeName(const Expr &expr);
bool isAssignCall(const Expr &expr);
bool isReturnCall(const Expr &expr);
bool isSimpleCallName(const Expr &expr, const char *nameToMatch);
bool isIfCall(const Expr &expr);
bool getBuiltinComparisonName(const Expr &expr, std::string &out);
bool getBuiltinOperatorName(const Expr &expr, std::string &out);
bool getBuiltinClampName(const Expr &expr, std::string &out);
bool getBuiltinMinMaxName(const Expr &expr, std::string &out);
bool getBuiltinAbsSignName(const Expr &expr, std::string &out);
bool getBuiltinSaturateName(const Expr &expr, std::string &out);
bool getBuiltinConvertName(const Expr &expr, std::string &out);
ReturnKind getReturnKind(const Definition &def);
const char *returnTypeName(ReturnKind kind);
ReturnKind returnKindForTypeName(const std::string &name);

} // namespace primec::ir_printer
