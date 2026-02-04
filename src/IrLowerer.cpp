#include "primec/IrLowerer.h"

#include <functional>

namespace primec {
namespace {

bool isSimpleCallName(const Expr &expr, const char *nameToMatch) {
  if (expr.kind != Expr::Kind::Call || expr.name.empty()) {
    return false;
  }
  std::string name = expr.name;
  if (!name.empty() && name[0] == '/') {
    name.erase(0, 1);
  }
  if (name.find('/') != std::string::npos) {
    return false;
  }
  return name == nameToMatch;
}

bool isReturnCall(const Expr &expr) {
  return isSimpleCallName(expr, "return");
}

bool getBuiltinOperatorName(const Expr &expr, std::string &out) {
  if (expr.kind != Expr::Kind::Call || expr.name.empty()) {
    return false;
  }
  std::string name = expr.name;
  if (!name.empty() && name[0] == '/') {
    name.erase(0, 1);
  }
  if (name.find('/') != std::string::npos) {
    return false;
  }
  if (name == "plus" || name == "minus" || name == "multiply" || name == "divide" || name == "negate") {
    out = name;
    return true;
  }
  return false;
}

} // namespace

bool IrLowerer::lower(const Program &program,
                      const std::string &entryPath,
                      IrModule &out,
                      std::string &error) const {
  out = IrModule{};

  const Definition *entryDef = nullptr;
  for (const auto &def : program.definitions) {
    if (def.fullPath == entryPath) {
      entryDef = &def;
      break;
    }
  }
  if (!entryDef) {
    error = "native backend requires entry definition " + entryPath;
    return false;
  }

  IrFunction function;
  function.name = entryPath;
  bool sawReturn = false;

  std::function<bool(const Expr &)> emitExpr;
  emitExpr = [&](const Expr &expr) -> bool {
    switch (expr.kind) {
      case Expr::Kind::Literal: {
        IrInstruction inst;
        inst.op = IrOpcode::PushI32;
        inst.imm = static_cast<int32_t>(expr.literalValue);
        function.instructions.push_back(inst);
        return true;
      }
      case Expr::Kind::Call: {
        std::string builtin;
        if (!getBuiltinOperatorName(expr, builtin)) {
          error = "native backend only supports arithmetic calls in expressions";
          return false;
        }
        if (builtin == "negate") {
          if (expr.args.size() != 1) {
            error = "negate requires exactly one argument";
            return false;
          }
          if (!emitExpr(expr.args.front())) {
            return false;
          }
          function.instructions.push_back({IrOpcode::NegI32, 0});
          return true;
        }
        if (expr.args.size() != 2) {
          error = builtin + " requires exactly two arguments";
          return false;
        }
        if (!emitExpr(expr.args[0])) {
          return false;
        }
        if (!emitExpr(expr.args[1])) {
          return false;
        }
        IrOpcode op = IrOpcode::AddI32;
        if (builtin == "plus") {
          op = IrOpcode::AddI32;
        } else if (builtin == "minus") {
          op = IrOpcode::SubI32;
        } else if (builtin == "multiply") {
          op = IrOpcode::MulI32;
        } else if (builtin == "divide") {
          op = IrOpcode::DivI32;
        }
        function.instructions.push_back({op, 0});
        return true;
      }
      default:
        error = "native backend only supports integer literal expressions";
        return false;
    }
  };

  for (const auto &stmt : entryDef->statements) {
    if (!isReturnCall(stmt)) {
      error = "native backend only supports return statements";
      return false;
    }
    if (sawReturn) {
      error = "native backend only supports a single return statement";
      return false;
    }
    if (stmt.args.size() != 1) {
      error = "return requires exactly one argument";
      return false;
    }
    if (!emitExpr(stmt.args.front())) {
      return false;
    }
    function.instructions.push_back({IrOpcode::ReturnI32, 0});
    sawReturn = true;
  }

  if (!sawReturn) {
    error = "native backend requires an explicit return statement";
    return false;
  }

  out.functions.push_back(std::move(function));
  out.entryIndex = 0;
  return true;
}

} // namespace primec
