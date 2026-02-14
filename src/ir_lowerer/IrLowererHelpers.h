#pragma once

#include <optional>
#include <string>
#include <vector>

#include "primec/Ast.h"

namespace primec::ir_lowerer {

bool isSimpleCallName(const Expr &expr, const char *nameToMatch);
bool isReturnCall(const Expr &expr);
bool isIfCall(const Expr &expr);
bool isLoopCall(const Expr &expr);
bool isWhileCall(const Expr &expr);
bool isForCall(const Expr &expr);
bool isRepeatCall(const Expr &expr);
bool isBlockCall(const Expr &expr);

bool hasNamedArguments(const std::vector<std::optional<std::string>> &argNames);

enum class PrintTarget { Out, Err };

struct PrintBuiltin {
  PrintTarget target = PrintTarget::Out;
  bool newline = false;
  std::string name;
};

struct PathSpaceBuiltin {
  std::string name;
  size_t argumentCount = 0;
};

bool getPrintBuiltin(const Expr &expr, PrintBuiltin &out);
bool getPathSpaceBuiltin(const Expr &expr, PathSpaceBuiltin &out);

bool getBuiltinOperatorName(const Expr &expr, std::string &out);
bool getBuiltinComparisonName(const Expr &expr, std::string &out);
bool getBuiltinClampName(const Expr &expr, bool allowBare);
bool getBuiltinMinMaxName(const Expr &expr, std::string &out, bool allowBare);
bool getBuiltinLerpName(const Expr &expr, std::string &out, bool allowBare);
bool getBuiltinAbsSignName(const Expr &expr, std::string &out, bool allowBare);
bool getBuiltinSaturateName(const Expr &expr, std::string &out, bool allowBare);
bool getBuiltinPowName(const Expr &expr, std::string &out, bool allowBare);
bool getBuiltinMathPredicateName(const Expr &expr, std::string &out, bool allowBare);
bool getBuiltinRoundingName(const Expr &expr, std::string &out, bool allowBare);
bool getBuiltinConvertName(const Expr &expr);
bool getBuiltinArrayAccessName(const Expr &expr, std::string &out);
bool getBuiltinPointerName(const Expr &expr, std::string &out);
bool getBuiltinCollectionName(const Expr &expr, std::string &out);

} // namespace primec::ir_lowerer
