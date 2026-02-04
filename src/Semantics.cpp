#include "primec/Semantics.h"

#include <functional>
#include <unordered_map>
#include <unordered_set>

namespace primec {
namespace {
enum class ReturnKind { Unknown, Int, Float32, Float64, Bool, Void };

struct BindingInfo {
  std::string typeName;
  bool isMutable = false;
};

ReturnKind getReturnKind(const Definition &def, std::string &error) {
  ReturnKind kind = ReturnKind::Unknown;
  for (const auto &transform : def.transforms) {
    if (transform.name != "return") {
      continue;
    }
    if (!transform.templateArg) {
      error = "return transform requires a type on " + def.fullPath;
      return ReturnKind::Unknown;
    }
    const std::string &arg = *transform.templateArg;
    if (arg == "int") {
      if (kind != ReturnKind::Unknown && kind != ReturnKind::Int) {
        error = "conflicting return types on " + def.fullPath;
        return ReturnKind::Unknown;
      }
      kind = ReturnKind::Int;
    } else if (arg == "bool") {
      if (kind != ReturnKind::Unknown && kind != ReturnKind::Bool) {
        error = "conflicting return types on " + def.fullPath;
        return ReturnKind::Unknown;
      }
      kind = ReturnKind::Bool;
    } else if (arg == "i32") {
      if (kind != ReturnKind::Unknown && kind != ReturnKind::Int) {
        error = "conflicting return types on " + def.fullPath;
        return ReturnKind::Unknown;
      }
      kind = ReturnKind::Int;
    } else if (arg == "void") {
      if (kind != ReturnKind::Unknown && kind != ReturnKind::Void) {
        error = "conflicting return types on " + def.fullPath;
        return ReturnKind::Unknown;
      }
      kind = ReturnKind::Void;
    } else if (arg == "float" || arg == "f32") {
      if (kind != ReturnKind::Unknown && kind != ReturnKind::Float32) {
        error = "conflicting return types on " + def.fullPath;
        return ReturnKind::Unknown;
      }
      kind = ReturnKind::Float32;
    } else if (arg == "f64") {
      if (kind != ReturnKind::Unknown && kind != ReturnKind::Float64) {
        error = "conflicting return types on " + def.fullPath;
        return ReturnKind::Unknown;
      }
      kind = ReturnKind::Float64;
    } else {
      error = "unsupported return type on " + def.fullPath;
      return ReturnKind::Unknown;
    }
  }
  return kind;
}

bool getBuiltinOperatorName(const Expr &expr, std::string &out) {
  if (expr.name.empty()) {
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

bool getBuiltinComparisonName(const Expr &expr, std::string &out) {
  if (expr.name.empty()) {
    return false;
  }
  std::string name = expr.name;
  if (!name.empty() && name[0] == '/') {
    name.erase(0, 1);
  }
  if (name.find('/') != std::string::npos) {
    return false;
  }
  if (name == "greater_than" || name == "less_than" || name == "equal" || name == "not_equal" ||
      name == "greater_equal" || name == "less_equal" || name == "and" || name == "or" || name == "not") {
    out = name;
    return true;
  }
  return false;
}

bool getBuiltinClampName(const Expr &expr, std::string &out) {
  if (expr.name.empty()) {
    return false;
  }
  std::string name = expr.name;
  if (!name.empty() && name[0] == '/') {
    name.erase(0, 1);
  }
  if (name.find('/') != std::string::npos) {
    return false;
  }
  if (name == "clamp") {
    out = name;
    return true;
  }
  return false;
}

bool getBuiltinCollectionName(const Expr &expr, std::string &out) {
  if (expr.name.empty()) {
    return false;
  }
  std::string name = expr.name;
  if (!name.empty() && name[0] == '/') {
    name.erase(0, 1);
  }
  if (name.find('/') != std::string::npos) {
    return false;
  }
  if (name == "array" || name == "map") {
    out = name;
    return true;
  }
  return false;
}

bool isAssignCall(const Expr &expr) {
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
  return name == "assign";
}

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

bool isIfCall(const Expr &expr) {
  return isSimpleCallName(expr, "if");
}

bool isThenCall(const Expr &expr) {
  return isSimpleCallName(expr, "then");
}

bool isElseCall(const Expr &expr) {
  return isSimpleCallName(expr, "else");
}

bool isReturnCall(const Expr &expr) {
  return isSimpleCallName(expr, "return");
}

bool parseBindingInfo(const Expr &expr, BindingInfo &info, std::string &error) {
  std::string typeName;
  for (const auto &transform : expr.transforms) {
    if (transform.name == "mut" && !transform.templateArg) {
      info.isMutable = true;
      continue;
    }
    if (transform.templateArg) {
      error = "binding transforms do not take template arguments";
      return false;
    }
    if (!typeName.empty()) {
      error = "binding requires exactly one type";
      return false;
    }
    typeName = transform.name;
  }
  if (typeName.empty()) {
    error = "binding requires a type";
    return false;
  }
  if (typeName != "int" && typeName != "i32" && typeName != "float" && typeName != "f32" &&
      typeName != "f64" && typeName != "bool") {
    error = "unsupported binding type: " + typeName;
    return false;
  }
  info.typeName = typeName;
  return true;
}

bool validateNamedArguments(const std::vector<Expr> &args,
                            const std::vector<std::optional<std::string>> &argNames,
                            const std::string &context,
                            std::string &error) {
  if (!argNames.empty() && argNames.size() != args.size()) {
    error = "argument name count mismatch for " + context;
    return false;
  }
  bool sawNamed = false;
  std::unordered_set<std::string> used;
  for (size_t i = 0; i < args.size(); ++i) {
    if (i < argNames.size() && argNames[i].has_value()) {
      sawNamed = true;
      const std::string &name = *argNames[i];
      if (!used.insert(name).second) {
        error = "duplicate named argument: " + name;
        return false;
      }
      continue;
    }
    if (sawNamed) {
      error = "positional argument cannot follow named arguments";
      return false;
    }
  }
  return true;
}

bool hasNamedArguments(const std::vector<std::optional<std::string>> &argNames) {
  for (const auto &name : argNames) {
    if (name.has_value()) {
      return true;
    }
  }
  return false;
}

bool validateNamedArgumentsAgainstParams(const std::vector<std::string> &params,
                                         const std::vector<std::optional<std::string>> &argNames,
                                         std::string &error) {
  if (argNames.empty()) {
    return true;
  }
  size_t positionalCount = 0;
  while (positionalCount < argNames.size() && !argNames[positionalCount].has_value()) {
    ++positionalCount;
  }
  std::unordered_set<std::string> bound;
  for (size_t i = 0; i < positionalCount && i < params.size(); ++i) {
    bound.insert(params[i]);
  }
  for (size_t i = positionalCount; i < argNames.size(); ++i) {
    if (!argNames[i].has_value()) {
      continue;
    }
    const std::string &name = *argNames[i];
    bool found = false;
    for (const auto &param : params) {
      if (param == name) {
        found = true;
        break;
      }
    }
    if (!found) {
      error = "unknown named argument: " + name;
      return false;
    }
    if (bound.count(name) > 0) {
      error = "named argument duplicates parameter: " + name;
      return false;
    }
    bound.insert(name);
  }
  return true;
}
} // namespace

bool Semantics::validate(const Program &program, const std::string &entryPath, std::string &error) const {
  std::unordered_map<std::string, const Definition *> defMap;
  std::unordered_map<std::string, ReturnKind> returnKinds;
  for (const auto &def : program.definitions) {
    if (defMap.count(def.fullPath) > 0) {
      error = "duplicate definition: " + def.fullPath;
      return false;
    }
    ReturnKind kind = getReturnKind(def, error);
    if (!error.empty()) {
      return false;
    }
    returnKinds[def.fullPath] = kind;
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

  auto isMutableLocal = [](const std::unordered_map<std::string, BindingInfo> &locals,
                           const std::string &name) -> bool {
    auto it = locals.find(name);
    return it != locals.end() && it->second.isMutable;
  };

  std::function<bool(const std::vector<std::string> &, const std::unordered_map<std::string, BindingInfo> &,
                     const Expr &)>
      validateExpr = [&](const std::vector<std::string> &params,
                         const std::unordered_map<std::string, BindingInfo> &locals,
                         const Expr &expr) -> bool {
    if (expr.kind == Expr::Kind::Literal) {
      return true;
    }
    if (expr.kind == Expr::Kind::FloatLiteral) {
      return true;
    }
    if (expr.kind == Expr::Kind::StringLiteral) {
      return true;
    }
    if (expr.kind == Expr::Kind::Name) {
      if (isParam(params, expr.name) || locals.count(expr.name) > 0) {
        return true;
      }
      error = "unknown identifier: " + expr.name;
      return false;
    }
    if (expr.kind == Expr::Kind::Call) {
      if (expr.isBinding) {
        error = "binding not allowed in expression context";
        return false;
      }
      if (!expr.bodyArguments.empty()) {
        error = "block arguments are only supported on if blocks";
        return false;
      }
      if (isReturnCall(expr)) {
        error = "return not allowed in expression context";
        return false;
      }
      if (isIfCall(expr) || isThenCall(expr) || isElseCall(expr)) {
        error = "control-flow blocks cannot appear in expressions";
        return false;
      }
      std::string resolved = resolveCalleePath(expr);
      if (!validateNamedArguments(expr.args, expr.argNames, resolved, error)) {
        return false;
      }
      auto it = defMap.find(resolved);
      if (it == defMap.end()) {
        if (hasNamedArguments(expr.argNames)) {
          error = "named arguments not supported for builtin calls";
          return false;
        }
        std::string builtinName;
        if (getBuiltinOperatorName(expr, builtinName)) {
          size_t expectedArgs = builtinName == "negate" ? 1 : 2;
          if (expr.args.size() != expectedArgs) {
            error = "argument count mismatch for builtin " + builtinName;
            return false;
          }
          for (const auto &arg : expr.args) {
            if (!validateExpr(params, locals, arg)) {
              return false;
            }
          }
          return true;
        }
        if (getBuiltinComparisonName(expr, builtinName)) {
          size_t expectedArgs = builtinName == "not" ? 1 : 2;
          if (expr.args.size() != expectedArgs) {
            error = "argument count mismatch for builtin " + builtinName;
            return false;
          }
          for (const auto &arg : expr.args) {
            if (!validateExpr(params, locals, arg)) {
              return false;
            }
          }
          return true;
        }
      if (getBuiltinClampName(expr, builtinName)) {
        if (expr.args.size() != 3) {
          error = "argument count mismatch for builtin " + builtinName;
          return false;
        }
        for (const auto &arg : expr.args) {
          if (!validateExpr(params, locals, arg)) {
            return false;
          }
        }
        return true;
      }
      if (getBuiltinCollectionName(expr, builtinName)) {
        if (builtinName == "array") {
          if (expr.templateArgs.size() != 1) {
            error = "array literal requires exactly one template argument";
            return false;
          }
        } else {
          if (expr.templateArgs.size() != 2) {
            error = "map literal requires exactly two template arguments";
            return false;
          }
          if (expr.args.size() % 2 != 0) {
            error = "map literal requires an even number of arguments";
            return false;
          }
        }
        for (const auto &arg : expr.args) {
          if (!validateExpr(params, locals, arg)) {
            return false;
          }
        }
        return true;
      }
      if (isAssignCall(expr)) {
        if (expr.args.size() != 2) {
          error = "assign requires exactly two arguments";
          return false;
        }
          if (expr.args.front().kind != Expr::Kind::Name) {
            error = "assign target must be a mutable binding";
            return false;
          }
          const std::string &targetName = expr.args.front().name;
          if (!isMutableLocal(locals, targetName)) {
            error = "assign target must be a mutable binding: " + targetName;
            return false;
          }
          if (!validateExpr(params, locals, expr.args[1])) {
            return false;
          }
          return true;
        }
        error = "unknown call target: " + expr.name;
        return false;
      }
      size_t expectedArgs = paramCounts[resolved];
      if (expr.args.size() != expectedArgs) {
        error = "argument count mismatch for " + resolved;
        return false;
      }
      if (!validateNamedArgumentsAgainstParams(it->second->parameters, expr.argNames, error)) {
        return false;
      }
      for (const auto &arg : expr.args) {
        if (!validateExpr(params, locals, arg)) {
          return false;
        }
      }
      return true;
    }
    return false;
  };

  std::function<bool(const std::vector<std::string> &,
                     std::unordered_map<std::string, BindingInfo> &,
                     const Expr &,
                     ReturnKind,
                     bool,
                     bool *)>
      validateStatement = [&](const std::vector<std::string> &params,
                              std::unordered_map<std::string, BindingInfo> &locals,
                              const Expr &stmt,
                              ReturnKind returnKind,
                              bool allowReturn,
                              bool *sawReturn) -> bool {
    if (stmt.isBinding) {
      if (isParam(params, stmt.name) || locals.count(stmt.name) > 0) {
        error = "duplicate binding name: " + stmt.name;
        return false;
      }
      BindingInfo info;
      if (!parseBindingInfo(stmt, info, error)) {
        return false;
      }
      if (stmt.args.size() != 1) {
        error = "binding requires exactly one argument";
        return false;
      }
      if (!validateExpr(params, locals, stmt.args.front())) {
        return false;
      }
      locals.emplace(stmt.name, info);
      return true;
    }
    if (stmt.kind != Expr::Kind::Call) {
      error = "statements must be calls";
      return false;
    }
    if (isReturnCall(stmt)) {
      if (!allowReturn) {
        error = "return not allowed in execution body";
        return false;
      }
      if (!stmt.bodyArguments.empty()) {
        error = "return does not accept block arguments";
        return false;
      }
      if (returnKind == ReturnKind::Void) {
        if (!stmt.args.empty()) {
          error = "return value not allowed for void definition";
          return false;
        }
      } else {
        if (stmt.args.size() != 1) {
          error = "return requires exactly one argument";
          return false;
        }
        if (!validateExpr(params, locals, stmt.args.front())) {
          return false;
        }
      }
      if (sawReturn) {
        *sawReturn = true;
      }
      return true;
    }
    if (isIfCall(stmt)) {
      if (!stmt.bodyArguments.empty()) {
        error = "if does not accept trailing block arguments";
        return false;
      }
      if (stmt.args.size() != 3) {
        error = "if requires condition, then, else";
        return false;
      }
      const Expr &cond = stmt.args[0];
      const Expr &thenBlock = stmt.args[1];
      const Expr &elseBlock = stmt.args[2];
      if (!validateExpr(params, locals, cond)) {
        return false;
      }
      auto validateBlock = [&](const Expr &block, const char *label) -> bool {
        if (block.kind != Expr::Kind::Call) {
          error = std::string(label) + " must be a call";
          return false;
        }
        if ((std::string(label) == "then" && !isThenCall(block)) ||
            (std::string(label) == "else" && !isElseCall(block))) {
          error = std::string(label) + " must use the " + label + " wrapper";
          return false;
        }
        if (!block.args.empty()) {
          error = std::string(label) + " does not accept arguments";
          return false;
        }
        std::unordered_map<std::string, BindingInfo> blockLocals = locals;
        for (const auto &bodyExpr : block.bodyArguments) {
          if (!validateStatement(params, blockLocals, bodyExpr, returnKind, allowReturn, sawReturn)) {
            return false;
          }
        }
        return true;
      };
      if (!validateBlock(thenBlock, "then")) {
        return false;
      }
      if (!validateBlock(elseBlock, "else")) {
        return false;
      }
      return true;
    }
    if (!stmt.bodyArguments.empty()) {
      error = "block arguments are only supported on if";
      return false;
    }
    return validateExpr(params, locals, stmt);
  };

  for (const auto &def : program.definitions) {
    std::unordered_map<std::string, BindingInfo> locals;
    ReturnKind kind = ReturnKind::Unknown;
    auto kindIt = returnKinds.find(def.fullPath);
    if (kindIt != returnKinds.end()) {
      kind = kindIt->second;
    }
    bool sawReturn = false;
    for (const auto &stmt : def.statements) {
      if (!validateStatement(def.parameters, locals, stmt, kind, true, &sawReturn)) {
        return false;
      }
    }
    if (kind != ReturnKind::Void && !sawReturn) {
      error = "missing return statement in " + def.fullPath;
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
    if (!validateNamedArguments(exec.arguments, exec.argumentNames, exec.fullPath, error)) {
      return false;
    }
    if (!validateNamedArgumentsAgainstParams(it->second->parameters, exec.argumentNames, error)) {
      return false;
    }
    for (const auto &arg : exec.arguments) {
      if (!validateExpr({}, std::unordered_map<std::string, BindingInfo>{}, arg)) {
        return false;
      }
    }
    std::unordered_map<std::string, BindingInfo> execLocals;
    for (const auto &arg : exec.bodyArguments) {
      if (arg.kind != Expr::Kind::Call) {
        error = "execution body arguments must be calls";
        return false;
      }
      if (!validateStatement({}, execLocals, arg, ReturnKind::Unknown, false, nullptr)) {
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
