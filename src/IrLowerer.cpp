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
  struct LocalInfo {
    int32_t index = 0;
    bool isMutable = false;
  };
  std::unordered_map<std::string, LocalInfo> locals;
  int32_t nextLocal = 0;

  auto isBindingMutable = [](const Expr &expr) -> bool {
    for (const auto &transform : expr.transforms) {
      if (transform.name == "mut") {
        return true;
      }
    }
    return false;
  };

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
      case Expr::Kind::Name: {
        auto it = locals.find(expr.name);
        if (it == locals.end()) {
          error = "native backend does not know identifier: " + expr.name;
          return false;
        }
        function.instructions.push_back({IrOpcode::LoadLocal, it->second.index});
        return true;
      }
      case Expr::Kind::Call: {
        std::string builtin;
        if (getBuiltinOperatorName(expr, builtin)) {
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
        if (isSimpleCallName(expr, "assign")) {
          if (expr.args.size() != 2) {
            error = "assign requires exactly two arguments";
            return false;
          }
          const Expr &target = expr.args.front();
          if (target.kind != Expr::Kind::Name) {
            error = "native backend only supports assign to local names";
            return false;
          }
          auto it = locals.find(target.name);
          if (it == locals.end()) {
            error = "assign target must be a known binding: " + target.name;
            return false;
          }
          if (!it->second.isMutable) {
            error = "assign target must be mutable: " + target.name;
            return false;
          }
          if (!emitExpr(expr.args[1])) {
            return false;
          }
          function.instructions.push_back({IrOpcode::Dup, 0});
          function.instructions.push_back({IrOpcode::StoreLocal, it->second.index});
          return true;
        }
        error = "native backend only supports arithmetic/assign calls in expressions";
        return false;
      }
      default:
        error = "native backend only supports literals, names, and calls";
        return false;
    }
  };

  for (const auto &stmt : entryDef->statements) {
    if (stmt.isBinding) {
      if (stmt.args.size() != 1) {
        error = "binding requires exactly one argument";
        return false;
      }
      if (!emitExpr(stmt.args.front())) {
        return false;
      }
      LocalInfo info;
      info.index = nextLocal++;
      info.isMutable = isBindingMutable(stmt);
      locals.emplace(stmt.name, info);
      function.instructions.push_back({IrOpcode::StoreLocal, info.index});
      continue;
    }
    if (!isReturnCall(stmt)) {
      if (stmt.kind == Expr::Kind::Call && isSimpleCallName(stmt, "assign")) {
        if (!emitExpr(stmt)) {
          return false;
        }
        function.instructions.push_back({IrOpcode::Pop, 0});
        continue;
      }
      error = "native backend only supports return or assign statements";
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
