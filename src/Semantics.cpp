#include "primec/Semantics.h"

#include <functional>
#include <unordered_map>

namespace primec {

bool Semantics::validate(const Program &program, const std::string &entryPath, std::string &error) const {
  std::unordered_map<std::string, const Definition *> defMap;
  for (const auto &def : program.definitions) {
    if (defMap.count(def.fullPath) > 0) {
      error = "duplicate definition: " + def.fullPath;
      return false;
    }
    for (const auto &transform : def.transforms) {
      if (transform.name == "return" && transform.templateArg && *transform.templateArg != "int") {
        error = "unsupported return type on " + def.fullPath;
        return false;
      }
    }
    defMap[def.fullPath] = &def;
  }

  auto resolveCalleePath = [&](const Expr &expr) -> std::string {
    if (expr.name.empty()) {
      return "";
    }
    if (!expr.name.empty() && expr.name[0] == '/') {
      return expr.name;
    }
    if (!expr.namespacePrefix.empty()) {
      return expr.namespacePrefix + "/" + expr.name;
    }
    return "/" + expr.name;
  };

  std::unordered_map<std::string, size_t> paramCounts;
  for (const auto &def : program.definitions) {
    paramCounts[def.fullPath] = def.parameters.size();
  }

  auto isParam = [](const std::vector<std::string> &params, const std::string &name) -> bool {
    for (const auto &param : params) {
      if (param == name) {
        return true;
      }
    }
    return false;
  };

  std::function<bool(const std::vector<std::string> &, const Expr &)> validateExpr =
      [&](const std::vector<std::string> &params, const Expr &expr) -> bool {
    if (expr.kind == Expr::Kind::Literal) {
      return true;
    }
    if (expr.kind == Expr::Kind::Name) {
      if (isParam(params, expr.name)) {
        return true;
      }
      error = "unknown identifier: " + expr.name;
      return false;
    }
    if (expr.kind == Expr::Kind::Call) {
      std::string resolved = resolveCalleePath(expr);
      auto it = defMap.find(resolved);
      if (it == defMap.end()) {
        error = "unknown call target: " + expr.name;
        return false;
      }
      size_t expectedArgs = paramCounts[resolved];
      if (expr.args.size() != expectedArgs) {
        error = "argument count mismatch for " + resolved;
        return false;
      }
      for (const auto &arg : expr.args) {
        if (!validateExpr(params, arg)) {
          return false;
        }
      }
      return true;
    }
    return false;
  };

  for (const auto &def : program.definitions) {
    for (const auto &stmt : def.statements) {
      if (!validateExpr(def.parameters, stmt)) {
        return false;
      }
    }
    if (!def.returnExpr) {
      error = "missing return statement in " + def.fullPath;
      return false;
    }
    if (!validateExpr(def.parameters, *def.returnExpr)) {
      return false;
    }
  }

  for (const auto &exec : program.executions) {
    auto it = defMap.find(exec.fullPath);
    if (it == defMap.end()) {
      error = "unknown execution target: " + exec.fullPath;
      return false;
    }
    size_t expectedArgs = paramCounts[exec.fullPath];
    if (exec.arguments.size() != expectedArgs) {
      error = "argument count mismatch for " + exec.fullPath;
      return false;
    }
    for (const auto &arg : exec.arguments) {
      if (!validateExpr({}, arg)) {
        return false;
      }
    }
    for (const auto &arg : exec.bodyArguments) {
      if (arg.kind != Expr::Kind::Call) {
        error = "execution body arguments must be calls";
        return false;
      }
      if (!validateExpr({}, arg)) {
        return false;
      }
    }
  }

  if (defMap.count(entryPath) == 0) {
    error = "missing entry definition " + entryPath;
    return false;
  }

  return true;
}

} // namespace primec
