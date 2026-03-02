#pragma once

#include <optional>
#include <string>
#include <vector>

#include "primec/Ast.h"

namespace primec::ir_lowerer {

struct OnErrorHandler {
  std::string handlerPath;
  std::vector<Expr> boundArgs;
};

struct ResultReturnInfo {
  bool isResult = false;
  bool hasValue = false;
};

class OnErrorScope {
 public:
  OnErrorScope(std::optional<OnErrorHandler> &targetIn, std::optional<OnErrorHandler> next);
  ~OnErrorScope();

 private:
  std::optional<OnErrorHandler> &target;
  std::optional<OnErrorHandler> previous;
};

class ResultReturnScope {
 public:
  ResultReturnScope(std::optional<ResultReturnInfo> &targetIn, std::optional<ResultReturnInfo> next);
  ~ResultReturnScope();

 private:
  std::optional<ResultReturnInfo> &target;
  std::optional<ResultReturnInfo> previous;
};

} // namespace primec::ir_lowerer
