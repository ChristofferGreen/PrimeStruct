#pragma once

#include <functional>
#include <optional>
#include <string>

#include "primec/Ast.h"

namespace primec::ir_lowerer {

struct ResultExprInfo {
  bool isResult = false;
  bool hasValue = false;
  std::string errorType;
};

struct LocalResultInfo {
  bool found = false;
  bool isResult = false;
  bool resultHasValue = false;
  std::string resultErrorType;
  bool isFileHandle = false;
};

using LookupLocalResultInfoFn = std::function<LocalResultInfo(const std::string &)>;
using ResolveCallDefinitionFn = std::function<const Definition *(const Expr &)>;
using LookupDefinitionResultInfoFn = std::function<bool(const std::string &, ResultExprInfo &)>;

bool resolveResultExprInfo(const Expr &expr,
                           const LookupLocalResultInfoFn &lookupLocal,
                           const ResolveCallDefinitionFn &resolveMethodCall,
                           const ResolveCallDefinitionFn &resolveDefinitionCall,
                           const LookupDefinitionResultInfoFn &lookupDefinitionResult,
                           ResultExprInfo &out);

} // namespace primec::ir_lowerer
