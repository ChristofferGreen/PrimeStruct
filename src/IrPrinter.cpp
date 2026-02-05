#include "primec/IrPrinter.h"

#include <cstdint>
#include <functional>
#include <sstream>
#include <unordered_map>
#include <unordered_set>

namespace primec {

namespace {
enum class ReturnKind { Unknown, Int, Int64, UInt64, Float32, Float64, Bool, Void };

std::string bindingTypeName(const Expr &expr) {
  std::string typeName;
  for (const auto &transform : expr.transforms) {
    if (transform.name == "mut" || transform.name == "copy" || transform.name == "restrict" ||
        transform.name == "public" || transform.name == "private" || transform.name == "package" ||
        transform.name == "static" || transform.name == "align_bytes" || transform.name == "align_kbytes") {
      continue;
    }
    if (!transform.arguments.empty()) {
      continue;
    }
    if (transform.templateArg) {
      typeName = transform.name + "<" + *transform.templateArg + ">";
    } else {
      typeName = transform.name;
    }
  }
  if (typeName == "int" || typeName == "i32") {
    return "i32";
  }
  if (typeName == "i64") {
    return "i64";
  }
  if (typeName == "u64") {
    return "u64";
  }
  if (typeName == "bool") {
    return "bool";
  }
  if (typeName == "float" || typeName == "f32") {
    return "f32";
  }
  if (typeName == "f64") {
    return "f64";
  }
  if (typeName == "string") {
    return "string";
  }
  return "";
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

bool isReturnCall(const Expr &expr) {
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
  return name == "return";
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

bool getBuiltinComparisonName(const Expr &expr, std::string &out) {
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
  if (name == "greater_than" || name == "less_than" || name == "equal" || name == "not_equal" ||
      name == "greater_equal" || name == "less_equal" || name == "and" || name == "or" || name == "not") {
    out = name;
    return true;
  }
  return false;
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

bool getBuiltinClampName(const Expr &expr, std::string &out) {
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
  if (name == "clamp") {
    out = name;
    return true;
  }
  return false;
}

bool getBuiltinConvertName(const Expr &expr, std::string &out) {
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
  if (name == "convert") {
    out = name;
    return true;
  }
  return false;
}

ReturnKind getReturnKind(const Definition &def) {
  for (const auto &transform : def.transforms) {
    if (transform.name == "struct" || transform.name == "pod" || transform.name == "stack" ||
        transform.name == "heap" || transform.name == "buffer") {
      return ReturnKind::Void;
    }
    if (transform.name != "return" || !transform.templateArg) {
      continue;
    }
    if (*transform.templateArg == "void") {
      return ReturnKind::Void;
    }
    if (*transform.templateArg == "int") {
      return ReturnKind::Int;
    }
    if (*transform.templateArg == "i64") {
      return ReturnKind::Int64;
    }
    if (*transform.templateArg == "u64") {
      return ReturnKind::UInt64;
    }
    if (*transform.templateArg == "bool") {
      return ReturnKind::Bool;
    }
    if (*transform.templateArg == "i32") {
      return ReturnKind::Int;
    }
    if (*transform.templateArg == "float" || *transform.templateArg == "f32") {
      return ReturnKind::Float32;
    }
    if (*transform.templateArg == "f64") {
      return ReturnKind::Float64;
    }
  }
  return ReturnKind::Unknown;
}

const char *returnTypeName(ReturnKind kind) {
  if (kind == ReturnKind::Void) {
    return "void";
  }
  if (kind == ReturnKind::Int64) {
    return "i64";
  }
  if (kind == ReturnKind::UInt64) {
    return "u64";
  }
  if (kind == ReturnKind::Float32) {
    return "f32";
  }
  if (kind == ReturnKind::Float64) {
    return "f64";
  }
  if (kind == ReturnKind::Bool) {
    return "bool";
  }
  return "i32";
}

ReturnKind returnKindForTypeName(const std::string &name) {
  if (name == "int" || name == "i32") {
    return ReturnKind::Int;
  }
  if (name == "i64") {
    return ReturnKind::Int64;
  }
  if (name == "u64") {
    return ReturnKind::UInt64;
  }
  if (name == "bool") {
    return ReturnKind::Bool;
  }
  if (name == "float" || name == "f32") {
    return ReturnKind::Float32;
  }
  if (name == "f64") {
    return ReturnKind::Float64;
  }
  if (name == "void") {
    return ReturnKind::Void;
  }
  return ReturnKind::Unknown;
}

void indent(std::ostringstream &out, int depth) {
  for (int i = 0; i < depth; ++i) {
    out << "  ";
  }
}

void printExpr(std::ostringstream &out, const Expr &expr) {
  switch (expr.kind) {
  case Expr::Kind::Literal:
    if (!expr.isUnsigned && expr.intWidth == 32) {
      out << static_cast<int64_t>(expr.literalValue);
    } else {
      if (expr.isUnsigned) {
        out << expr.literalValue;
        out << (expr.intWidth == 64 ? "u64" : "u32");
      } else {
        out << static_cast<int64_t>(expr.literalValue);
        out << (expr.intWidth == 64 ? "i64" : "i32");
      }
    }
    break;
  case Expr::Kind::BoolLiteral:
    out << (expr.boolValue ? "true" : "false");
    break;
  case Expr::Kind::FloatLiteral:
    out << expr.floatValue << (expr.floatWidth == 64 ? "f64" : "f32");
    break;
  case Expr::Kind::StringLiteral:
    out << expr.stringValue;
    break;
  case Expr::Kind::Name:
    out << expr.name;
    break;
  case Expr::Kind::Call:
    if (expr.isMethodCall && !expr.args.empty()) {
      printExpr(out, expr.args.front());
      out << "." << expr.name << "(";
      for (size_t i = 1; i < expr.args.size(); ++i) {
        if (i > 1) {
          out << ", ";
        }
        if (i < expr.argNames.size() && expr.argNames[i].has_value()) {
          out << *expr.argNames[i] << " = ";
        }
        printExpr(out, expr.args[i]);
      }
      out << ")";
    } else {
      out << expr.name << "(";
      for (size_t i = 0; i < expr.args.size(); ++i) {
        if (i > 0) {
          out << ", ";
        }
        if (i < expr.argNames.size() && expr.argNames[i].has_value()) {
          out << *expr.argNames[i] << " = ";
        }
        printExpr(out, expr.args[i]);
      }
      out << ")";
    }
    if (!expr.bodyArguments.empty()) {
      out << " { ";
      for (size_t i = 0; i < expr.bodyArguments.size(); ++i) {
        if (i > 0) {
          out << ", ";
        }
        printExpr(out, expr.bodyArguments[i]);
      }
      out << " }";
    }
    break;
  }
}

void printParameter(std::ostringstream &out, const Expr &param) {
  if (!param.transforms.empty()) {
    out << "[";
    for (size_t i = 0; i < param.transforms.size(); ++i) {
      if (i > 0) {
        out << ", ";
      }
      out << param.transforms[i].name;
      if (param.transforms[i].templateArg) {
        out << "<" << *param.transforms[i].templateArg << ">";
      }
    }
    out << "] ";
  }
  out << param.name;
  if (!param.args.empty()) {
    out << "(";
    for (size_t i = 0; i < param.args.size(); ++i) {
      if (i > 0) {
        out << ", ";
      }
      printExpr(out, param.args[i]);
    }
    out << ")";
  }
}

void printDefinition(std::ostringstream &out,
                     const Definition &def,
                     int depth,
                     const std::unordered_map<std::string, ReturnKind> &returnKinds) {
  ReturnKind kind = ReturnKind::Int;
  auto kindIt = returnKinds.find(def.fullPath);
  if (kindIt != returnKinds.end()) {
    kind = kindIt->second;
  } else {
    kind = getReturnKind(def);
  }
  indent(out, depth);
  out << "def " << def.fullPath << "(";
  for (size_t i = 0; i < def.parameters.size(); ++i) {
    if (i > 0) {
      out << ", ";
    }
    printParameter(out, def.parameters[i]);
  }
  out << "): " << returnTypeName(kind) << " {\n";
  bool sawReturn = false;
  for (const auto &stmt : def.statements) {
    indent(out, depth + 1);
    if (isReturnCall(stmt)) {
      out << "return";
      if (!stmt.args.empty()) {
        out << " ";
        printExpr(out, stmt.args.front());
      }
      out << "\n";
      sawReturn = true;
    } else if (stmt.isBinding) {
      out << "let " << stmt.name;
      std::string typeName = bindingTypeName(stmt);
      if (!typeName.empty()) {
        out << ": " << typeName;
      }
      if (!stmt.args.empty()) {
        out << " = ";
        printExpr(out, stmt.args.front());
      }
      out << "\n";
    } else if (isAssignCall(stmt) && stmt.args.size() == 2) {
      out << "assign ";
      printExpr(out, stmt.args[0]);
      out << ", ";
      printExpr(out, stmt.args[1]);
      out << "\n";
    } else {
      out << "call ";
      printExpr(out, stmt);
      out << "\n";
    }
  }
  if (kind == ReturnKind::Void && !sawReturn) {
    indent(out, depth + 1);
    out << "return\n";
  }
  indent(out, depth);
  out << "}\n";
}

void printExecution(std::ostringstream &out, const Execution &exec, int depth) {
  indent(out, depth);
  out << "exec ";
  if (!exec.transforms.empty()) {
    out << "[";
    for (size_t i = 0; i < exec.transforms.size(); ++i) {
      if (i > 0) {
        out << ", ";
      }
      out << exec.transforms[i].name;
      if (exec.transforms[i].templateArg) {
        out << "<" << *exec.transforms[i].templateArg << ">";
      }
      if (!exec.transforms[i].arguments.empty()) {
        out << "(";
        for (size_t argIndex = 0; argIndex < exec.transforms[i].arguments.size(); ++argIndex) {
          if (argIndex > 0) {
            out << ", ";
          }
          out << exec.transforms[i].arguments[argIndex];
        }
        out << ")";
      }
    }
    out << "] ";
  }
  out << exec.fullPath << "(";
  for (size_t i = 0; i < exec.arguments.size(); ++i) {
    if (i > 0) {
      out << ", ";
    }
    if (i < exec.argumentNames.size() && exec.argumentNames[i].has_value()) {
      out << *exec.argumentNames[i] << " = ";
    }
    printExpr(out, exec.arguments[i]);
  }
  out << ")";
  if (!exec.bodyArguments.empty()) {
    out << " { ";
    for (size_t i = 0; i < exec.bodyArguments.size(); ++i) {
      if (i > 0) {
        out << ", ";
      }
      printExpr(out, exec.bodyArguments[i]);
    }
    out << " }";
  }
  out << "\n";
}
} // namespace

std::string IrPrinter::print(const Program &program) const {
  std::unordered_map<std::string, const Definition *> defMap;
  std::unordered_map<std::string, ReturnKind> returnKinds;
  for (const auto &def : program.definitions) {
    defMap[def.fullPath] = &def;
    returnKinds[def.fullPath] = getReturnKind(def);
  }

  auto isParam = [](const std::vector<Expr> &params, const std::string &name) -> bool {
    for (const auto &param : params) {
      if (param.name == name) {
        return true;
      }
    }
    return false;
  };

  std::unordered_set<std::string> inferenceStack;
  std::function<ReturnKind(const Definition &)> inferDefinitionReturnKind;
  std::function<ReturnKind(const Expr &,
                           const std::vector<Expr> &,
                           const std::unordered_map<std::string, ReturnKind> &)>
      inferExprReturnKind;

  inferExprReturnKind = [&](const Expr &expr,
                            const std::vector<Expr> &params,
                            const std::unordered_map<std::string, ReturnKind> &locals) -> ReturnKind {
    auto combineNumeric = [&](ReturnKind left, ReturnKind right) -> ReturnKind {
      if (left == ReturnKind::Unknown || right == ReturnKind::Unknown) {
        return ReturnKind::Unknown;
      }
      if (left == ReturnKind::Bool || right == ReturnKind::Bool || left == ReturnKind::Void || right == ReturnKind::Void) {
        return ReturnKind::Unknown;
      }
      if (left == ReturnKind::Float64 || right == ReturnKind::Float64) {
        return ReturnKind::Float64;
      }
      if (left == ReturnKind::Float32 || right == ReturnKind::Float32) {
        return ReturnKind::Float32;
      }
      if (left == ReturnKind::UInt64 || right == ReturnKind::UInt64) {
        return (left == ReturnKind::UInt64 && right == ReturnKind::UInt64) ? ReturnKind::UInt64 : ReturnKind::Unknown;
      }
      if (left == ReturnKind::Int64 || right == ReturnKind::Int64) {
        if ((left == ReturnKind::Int64 || left == ReturnKind::Int) &&
            (right == ReturnKind::Int64 || right == ReturnKind::Int)) {
          return ReturnKind::Int64;
        }
        return ReturnKind::Unknown;
      }
      if (left == ReturnKind::Int && right == ReturnKind::Int) {
        return ReturnKind::Int;
      }
      return ReturnKind::Unknown;
    };

    if (expr.kind == Expr::Kind::Literal) {
      if (expr.isUnsigned) {
        return ReturnKind::UInt64;
      }
      if (expr.intWidth == 64) {
        return ReturnKind::Int64;
      }
      return ReturnKind::Int;
    }
    if (expr.kind == Expr::Kind::BoolLiteral) {
      return ReturnKind::Bool;
    }
    if (expr.kind == Expr::Kind::FloatLiteral) {
      return expr.floatWidth == 64 ? ReturnKind::Float64 : ReturnKind::Float32;
    }
    if (expr.kind == Expr::Kind::StringLiteral) {
      return ReturnKind::Unknown;
    }
    if (expr.kind == Expr::Kind::Name) {
      if (isParam(params, expr.name)) {
        return ReturnKind::Unknown;
      }
      auto it = locals.find(expr.name);
      if (it == locals.end()) {
        return ReturnKind::Unknown;
      }
      return it->second;
    }
    if (expr.kind == Expr::Kind::Call) {
      std::string resolved = expr.name;
      if (!resolved.empty() && resolved[0] != '/') {
        if (!expr.namespacePrefix.empty()) {
          resolved = expr.namespacePrefix + "/" + resolved;
        } else {
          resolved = "/" + resolved;
        }
      }
      auto defIt = defMap.find(resolved);
      if (defIt != defMap.end()) {
        ReturnKind calleeKind = inferDefinitionReturnKind(*defIt->second);
        if (calleeKind == ReturnKind::Void || calleeKind == ReturnKind::Unknown) {
          return ReturnKind::Unknown;
        }
        return calleeKind;
      }
      std::string builtinName;
      if (getBuiltinComparisonName(expr, builtinName)) {
        return ReturnKind::Bool;
      }
      if (getBuiltinOperatorName(expr, builtinName)) {
        if (builtinName == "negate") {
          if (expr.args.size() != 1) {
            return ReturnKind::Unknown;
          }
          ReturnKind argKind = inferExprReturnKind(expr.args.front(), params, locals);
          if (argKind == ReturnKind::Bool || argKind == ReturnKind::Void) {
            return ReturnKind::Unknown;
          }
          return argKind;
        }
        if (expr.args.size() != 2) {
          return ReturnKind::Unknown;
        }
        ReturnKind left = inferExprReturnKind(expr.args[0], params, locals);
        ReturnKind right = inferExprReturnKind(expr.args[1], params, locals);
        return combineNumeric(left, right);
      }
      if (getBuiltinClampName(expr, builtinName)) {
        if (expr.args.size() != 3) {
          return ReturnKind::Unknown;
        }
        ReturnKind result = inferExprReturnKind(expr.args[0], params, locals);
        result = combineNumeric(result, inferExprReturnKind(expr.args[1], params, locals));
        result = combineNumeric(result, inferExprReturnKind(expr.args[2], params, locals));
        return result;
      }
      if (getBuiltinConvertName(expr, builtinName) && expr.templateArgs.size() == 1) {
        return returnKindForTypeName(expr.templateArgs.front());
      }
      return ReturnKind::Unknown;
    }
    return ReturnKind::Unknown;
  };

  inferDefinitionReturnKind = [&](const Definition &def) -> ReturnKind {
    ReturnKind &kind = returnKinds[def.fullPath];
    if (kind != ReturnKind::Unknown) {
      return kind;
    }
    if (!inferenceStack.insert(def.fullPath).second) {
      return ReturnKind::Unknown;
    }
    ReturnKind inferred = ReturnKind::Unknown;
    bool sawReturn = false;
    std::unordered_map<std::string, ReturnKind> locals;
    std::function<void(const Expr &, std::unordered_map<std::string, ReturnKind> &)> visitStmt;
    visitStmt = [&](const Expr &stmt, std::unordered_map<std::string, ReturnKind> &activeLocals) {
      if (stmt.isBinding) {
        ReturnKind bindingKind = returnKindForTypeName(bindingTypeName(stmt));
        activeLocals.emplace(stmt.name, bindingKind);
        return;
      }
      if (isReturnCall(stmt)) {
        sawReturn = true;
        ReturnKind exprKind = ReturnKind::Void;
        if (!stmt.args.empty()) {
          exprKind = inferExprReturnKind(stmt.args.front(), def.parameters, activeLocals);
        }
        if (exprKind == ReturnKind::Unknown) {
          inferred = ReturnKind::Unknown;
          return;
        }
        if (inferred == ReturnKind::Unknown) {
          inferred = exprKind;
          return;
        }
        if (inferred != exprKind) {
          inferred = ReturnKind::Unknown;
          return;
        }
        return;
      }
      if (isIfCall(stmt) && stmt.args.size() == 3) {
        const Expr &thenBlock = stmt.args[1];
        const Expr &elseBlock = stmt.args[2];
        auto walkBlock = [&](const Expr &block) {
          std::unordered_map<std::string, ReturnKind> blockLocals = activeLocals;
          for (const auto &bodyStmt : block.bodyArguments) {
            visitStmt(bodyStmt, blockLocals);
          }
        };
        walkBlock(thenBlock);
        walkBlock(elseBlock);
      }
    };
    for (const auto &stmt : def.statements) {
      visitStmt(stmt, locals);
    }
    if (!sawReturn) {
      kind = ReturnKind::Void;
    } else if (inferred == ReturnKind::Unknown) {
      kind = ReturnKind::Int;
    } else {
      kind = inferred;
    }
    inferenceStack.erase(def.fullPath);
    return kind;
  };

  for (const auto &def : program.definitions) {
    if (returnKinds[def.fullPath] == ReturnKind::Unknown) {
      inferDefinitionReturnKind(def);
    }
  }

  std::ostringstream out;
  out << "module {\n";
  for (const auto &def : program.definitions) {
    printDefinition(out, def, 1, returnKinds);
  }
  for (const auto &exec : program.executions) {
    printExecution(out, exec, 1);
  }
  out << "}\n";
  return out.str();
}

} // namespace primec
