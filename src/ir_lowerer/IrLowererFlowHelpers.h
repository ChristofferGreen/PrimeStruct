#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "primec/Ast.h"
#include "primec/Ir.h"

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

void emitFileCloseIfValid(std::vector<IrInstruction> &instructions, int32_t localIndex);
void emitFileScopeCleanup(std::vector<IrInstruction> &instructions, const std::vector<int32_t> &scope);
void emitAllFileScopeCleanup(std::vector<IrInstruction> &instructions,
                             const std::vector<std::vector<int32_t>> &fileScopeStack);

} // namespace primec::ir_lowerer
