#include "primec/Emitter.h"

#include <algorithm>
#include <functional>
#include <sstream>
#include <unordered_map>

namespace primec {
namespace {
enum class ReturnKind { Int, Float32, Float64, Bool, Void };

struct BindingInfo {
  std::string typeName;
  bool isMutable = false;
};

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
  return ReturnKind::Int;
}

BindingInfo getBindingInfo(const Expr &expr) {
  BindingInfo info;
  for (const auto &transform : expr.transforms) {
    if (transform.name == "mut" && !transform.templateArg && transform.arguments.empty()) {
      info.isMutable = true;
      continue;
    }
    if (transform.arguments.empty()) {
      if (transform.name == "public" || transform.name == "private" || transform.name == "package" ||
          transform.name == "static") {
        continue;
      }
      if (!transform.templateArg) {
        info.typeName = transform.name;
      } else if (info.typeName.empty()) {
        info.typeName = transform.name;
      }
    }
  }
  if (info.typeName.empty()) {
    info.typeName = "int";
  }
  return info;
}

std::string bindingTypeToCpp(const std::string &typeName) {
  if (typeName == "i32" || typeName == "int") {
    return "int";
  }
  if (typeName == "bool") {
    return "bool";
  }
  if (typeName == "float" || typeName == "f32") {
    return "float";
  }
  if (typeName == "f64") {
    return "double";
  }
  return "int";
}

bool getBuiltinOperator(const Expr &expr, char &out) {
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
  if (name == "plus") {
    out = '+';
    return true;
  }
  if (name == "minus") {
    out = '-';
    return true;
  }
  if (name == "multiply") {
    out = '*';
    return true;
  }
  if (name == "divide") {
    out = '/';
    return true;
  }
  return false;
}

bool getBuiltinComparison(const Expr &expr, const char *&out) {
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
  if (name == "greater_than") {
    out = ">";
    return true;
  }
  if (name == "less_than") {
    out = "<";
    return true;
  }
  if (name == "equal") {
    out = "==";
    return true;
  }
  if (name == "not_equal") {
    out = "!=";
    return true;
  }
  if (name == "greater_equal") {
    out = ">=";
    return true;
  }
  if (name == "less_equal") {
    out = "<=";
    return true;
  }
  if (name == "and") {
    out = "&&";
    return true;
  }
  if (name == "or") {
    out = "||";
    return true;
  }
  if (name == "not") {
    out = "!";
    return true;
  }
  return false;
}

bool isBuiltinNegate(const Expr &expr) {
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
  return name == "negate";
}

bool isBuiltinClamp(const Expr &expr) {
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
  return name == "clamp";
}

bool getBuiltinPointerOperator(const Expr &expr, char &out) {
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
  if (name == "deref") {
    out = '*';
    return true;
  }
  if (name == "address_of") {
    out = '&';
    return true;
  }
  return false;
}

bool getBuiltinPointerBinaryOperator(const Expr &expr, char &out) {
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
  if (name == "pointer_add") {
    out = '+';
    return true;
  }
  return false;
}

bool getBuiltinConvertName(const Expr &expr, std::string &out) {
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
  if (name == "convert") {
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

std::string resolveExprPath(const Expr &expr) {
  if (!expr.name.empty() && expr.name[0] == '/') {
    return expr.name;
  }
  if (!expr.namespacePrefix.empty()) {
    return expr.namespacePrefix + "/" + expr.name;
  }
  return "/" + expr.name;
}

bool isBuiltinAssign(const Expr &expr, const std::unordered_map<std::string, std::string> &nameMap) {
  if (expr.kind != Expr::Kind::Call || expr.name.empty()) {
    return false;
  }
  std::string full = resolveExprPath(expr);
  if (nameMap.count(full) > 0) {
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

std::vector<const Expr *> orderCallArguments(const Expr &expr, const std::vector<std::string> &params) {
  std::vector<const Expr *> ordered;
  ordered.reserve(expr.args.size());
  auto fallback = [&]() {
    ordered.clear();
    ordered.reserve(expr.args.size());
    for (const auto &arg : expr.args) {
      ordered.push_back(&arg);
    }
  };
  if (expr.argNames.empty() || expr.argNames.size() != expr.args.size() || params.size() != expr.args.size()) {
    fallback();
    return ordered;
  }
  ordered.assign(params.size(), nullptr);
  size_t positionalIndex = 0;
  for (size_t i = 0; i < expr.args.size(); ++i) {
    if (i < expr.argNames.size() && expr.argNames[i].has_value()) {
      const std::string &name = *expr.argNames[i];
      auto it = std::find(params.begin(), params.end(), name);
      if (it == params.end()) {
        fallback();
        return ordered;
      }
      size_t index = static_cast<size_t>(std::distance(params.begin(), it));
      ordered[index] = &expr.args[i];
      continue;
    }
    while (positionalIndex < ordered.size() && ordered[positionalIndex] != nullptr) {
      ++positionalIndex;
    }
    if (positionalIndex >= ordered.size()) {
      fallback();
      return ordered;
    }
    ordered[positionalIndex] = &expr.args[i];
    ++positionalIndex;
  }
  for (const auto *arg : ordered) {
    if (arg == nullptr) {
      fallback();
      return ordered;
    }
  }
  return ordered;
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

bool isBuiltinIf(const Expr &expr, const std::unordered_map<std::string, std::string> &nameMap) {
  std::string full = resolveExprPath(expr);
  if (nameMap.count(full) > 0) {
    return false;
  }
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
} // namespace

std::string Emitter::toCppName(const std::string &fullPath) const {
  std::string name = "ps";
  for (char c : fullPath) {
    if (c == '/') {
      name += "_";
    } else {
      name += c;
    }
  }
  return name;
}

std::string Emitter::emitExpr(const Expr &expr,
                              const std::unordered_map<std::string, std::string> &nameMap,
                              const std::unordered_map<std::string, std::vector<std::string>> &paramMap) const {
  if (expr.kind == Expr::Kind::Literal) {
    return std::to_string(expr.literalValue);
  }
  if (expr.kind == Expr::Kind::FloatLiteral) {
    if (expr.floatWidth == 64) {
      return expr.floatValue;
    }
    return expr.floatValue + "f";
  }
  if (expr.kind == Expr::Kind::StringLiteral) {
    return expr.stringValue;
  }
  if (expr.kind == Expr::Kind::Name) {
    return expr.name;
  }
  std::string full = resolveExprPath(expr);
  auto it = nameMap.find(full);
  if (it == nameMap.end()) {
    char op = '\0';
    if (getBuiltinOperator(expr, op) && expr.args.size() == 2) {
      std::ostringstream out;
      out << "(" << emitExpr(expr.args[0], nameMap, paramMap) << " " << op << " "
          << emitExpr(expr.args[1], nameMap, paramMap) << ")";
      return out.str();
    }
    if (isBuiltinNegate(expr) && expr.args.size() == 1) {
      std::ostringstream out;
      out << "(-" << emitExpr(expr.args[0], nameMap, paramMap) << ")";
      return out.str();
    }
    const char *cmp = nullptr;
    if (getBuiltinComparison(expr, cmp)) {
      std::ostringstream out;
      if (expr.args.size() == 1) {
        out << "(" << cmp << emitExpr(expr.args[0], nameMap, paramMap) << ")";
        return out.str();
      }
      if (expr.args.size() == 2) {
        out << "(" << emitExpr(expr.args[0], nameMap, paramMap) << " " << cmp << " "
            << emitExpr(expr.args[1], nameMap, paramMap) << ")";
        return out.str();
      }
    }
    if (isBuiltinClamp(expr) && expr.args.size() == 3) {
      std::ostringstream out;
      out << "ps_builtin_clamp(" << emitExpr(expr.args[0], nameMap, paramMap) << ", "
          << emitExpr(expr.args[1], nameMap, paramMap) << ", " << emitExpr(expr.args[2], nameMap, paramMap) << ")";
      return out.str();
    }
    std::string convertName;
    if (getBuiltinConvertName(expr, convertName) && expr.templateArgs.size() == 1 && expr.args.size() == 1) {
      std::string targetType = bindingTypeToCpp(expr.templateArgs[0]);
      std::ostringstream out;
      out << "static_cast<" << targetType << ">(" << emitExpr(expr.args[0], nameMap, paramMap) << ")";
      return out.str();
    }
    char pointerOp = '\0';
    if (getBuiltinPointerOperator(expr, pointerOp) && expr.args.size() == 1) {
      std::ostringstream out;
      out << "(" << pointerOp << emitExpr(expr.args[0], nameMap, paramMap) << ")";
      return out.str();
    }
    char pointerBinaryOp = '\0';
    if (getBuiltinPointerBinaryOperator(expr, pointerBinaryOp) && expr.args.size() == 2) {
      std::ostringstream out;
      out << "(" << emitExpr(expr.args[0], nameMap, paramMap) << " " << pointerBinaryOp << " "
          << emitExpr(expr.args[1], nameMap, paramMap) << ")";
      return out.str();
    }
    std::string collection;
    if (getBuiltinCollectionName(expr, collection)) {
      if (collection == "array" && expr.templateArgs.size() == 1) {
        std::ostringstream out;
        const std::string elemType = bindingTypeToCpp(expr.templateArgs[0]);
        out << "std::vector<" << elemType << ">{";
        for (size_t i = 0; i < expr.args.size(); ++i) {
          if (i > 0) {
            out << ", ";
          }
          out << emitExpr(expr.args[i], nameMap, paramMap);
        }
        out << "}";
        return out.str();
      }
      if (collection == "map" && expr.templateArgs.size() == 2) {
        std::ostringstream out;
        const std::string keyType = bindingTypeToCpp(expr.templateArgs[0]);
        const std::string valueType = bindingTypeToCpp(expr.templateArgs[1]);
        out << "std::unordered_map<" << keyType << ", " << valueType << ">{";
        for (size_t i = 0; i + 1 < expr.args.size(); i += 2) {
          if (i > 0) {
            out << ", ";
          }
          out << "{" << emitExpr(expr.args[i], nameMap, paramMap) << ", "
              << emitExpr(expr.args[i + 1], nameMap, paramMap) << "}";
        }
        out << "}";
        return out.str();
      }
    }
    if (isBuiltinAssign(expr, nameMap) && expr.args.size() == 2) {
      std::ostringstream out;
      out << "(" << emitExpr(expr.args[0], nameMap, paramMap) << " = "
          << emitExpr(expr.args[1], nameMap, paramMap) << ")";
      return out.str();
    }
    return "0";
  }
  std::ostringstream out;
  out << it->second << "(";
  std::vector<const Expr *> orderedArgs;
  auto paramIt = paramMap.find(full);
  if (paramIt != paramMap.end()) {
    orderedArgs = orderCallArguments(expr, paramIt->second);
  } else {
    for (const auto &arg : expr.args) {
      orderedArgs.push_back(&arg);
    }
  }
  for (size_t i = 0; i < orderedArgs.size(); ++i) {
    if (i > 0) {
      out << ", ";
    }
    out << emitExpr(*orderedArgs[i], nameMap, paramMap);
  }
  out << ")";
  return out.str();
}

std::string Emitter::emitCpp(const Program &program, const std::string &entryPath) const {
  std::unordered_map<std::string, std::string> nameMap;
  std::unordered_map<std::string, std::vector<std::string>> paramMap;
  for (const auto &def : program.definitions) {
    nameMap[def.fullPath] = toCppName(def.fullPath);
    paramMap[def.fullPath] = def.parameters;
  }

  std::ostringstream out;
  out << "// Generated by primec (minimal)\n";
  out << "#include <unordered_map>\n";
  out << "#include <vector>\n";
  out << "static int ps_builtin_clamp(int value, int minValue, int maxValue) {\n";
  out << "  if (value < minValue) {\n";
  out << "    return minValue;\n";
  out << "  }\n";
  out << "  if (value > maxValue) {\n";
  out << "    return maxValue;\n";
  out << "  }\n";
  out << "  return value;\n";
  out << "}\n";
  for (const auto &def : program.definitions) {
    ReturnKind returnKind = getReturnKind(def);
    std::string returnType = "int";
    if (returnKind == ReturnKind::Void) {
      returnType = "void";
    } else if (returnKind == ReturnKind::Float32) {
      returnType = "float";
    } else if (returnKind == ReturnKind::Float64) {
      returnType = "double";
    } else if (returnKind == ReturnKind::Bool) {
      returnType = "bool";
    }
    out << "static " << returnType << " " << nameMap[def.fullPath] << "(";
    for (size_t i = 0; i < def.parameters.size(); ++i) {
      if (i > 0) {
        out << ", ";
      }
      out << "int " << def.parameters[i];
    }
    out << ") {\n";
    std::function<void(const Expr &, int)> emitStatement;
    emitStatement = [&](const Expr &stmt, int indent) {
      std::string pad(static_cast<size_t>(indent) * 2, ' ');
      if (isReturnCall(stmt)) {
        out << pad << "return";
        if (!stmt.args.empty()) {
          out << " " << emitExpr(stmt.args.front(), nameMap, paramMap);
        }
        out << ";\n";
        return;
      }
      if (stmt.isBinding) {
        BindingInfo binding = getBindingInfo(stmt);
        std::string type = bindingTypeToCpp(binding.typeName);
        out << pad << (binding.isMutable ? "" : "const ") << type << " " << stmt.name;
        if (!stmt.args.empty()) {
          out << " = " << emitExpr(stmt.args.front(), nameMap, paramMap);
        }
        out << ";\n";
        return;
      }
      if (isBuiltinIf(stmt, nameMap) && stmt.args.size() == 3) {
        const Expr &cond = stmt.args[0];
        const Expr &thenBlock = stmt.args[1];
        const Expr &elseBlock = stmt.args[2];
        out << pad << "if (" << emitExpr(cond, nameMap, paramMap) << ") {\n";
        if (isThenCall(thenBlock)) {
          for (const auto &bodyStmt : thenBlock.bodyArguments) {
            emitStatement(bodyStmt, indent + 1);
          }
        }
        out << pad << "} else {\n";
        if (isElseCall(elseBlock)) {
          for (const auto &bodyStmt : elseBlock.bodyArguments) {
            emitStatement(bodyStmt, indent + 1);
          }
        }
        out << pad << "}\n";
        return;
      }
      if (stmt.kind == Expr::Kind::Call && !stmt.bodyArguments.empty()) {
        std::string full = resolveExprPath(stmt);
        auto it = nameMap.find(full);
        if (it == nameMap.end()) {
          out << pad << emitExpr(stmt, nameMap, paramMap) << ";\n";
          return;
        }
        out << pad << it->second << "(";
        std::vector<const Expr *> orderedArgs;
        auto paramIt = paramMap.find(full);
        if (paramIt != paramMap.end()) {
          orderedArgs = orderCallArguments(stmt, paramIt->second);
        } else {
          for (const auto &arg : stmt.args) {
            orderedArgs.push_back(&arg);
          }
        }
        for (size_t i = 0; i < orderedArgs.size(); ++i) {
          if (i > 0) {
            out << ", ";
          }
          out << emitExpr(*orderedArgs[i], nameMap, paramMap);
        }
        if (!orderedArgs.empty()) {
          out << ", ";
        }
        out << "[&]() {\n";
        for (const auto &bodyStmt : stmt.bodyArguments) {
          emitStatement(bodyStmt, indent + 1);
        }
        out << pad << "}";
        out << ");\n";
        return;
      }
      if (stmt.kind == Expr::Kind::Call && isBuiltinAssign(stmt, nameMap) && stmt.args.size() == 2 &&
          stmt.args.front().kind == Expr::Kind::Name) {
        out << pad << stmt.args.front().name << " = " << emitExpr(stmt.args[1], nameMap, paramMap) << ";\n";
        return;
      }
      out << pad << emitExpr(stmt, nameMap, paramMap) << ";\n";
    };
    for (const auto &stmt : def.statements) {
      emitStatement(stmt, 1);
    }
    if (returnKind == ReturnKind::Void) {
      out << "  return;\n";
    }
    out << "}\n";
  }
  ReturnKind entryReturn = ReturnKind::Int;
  for (const auto &def : program.definitions) {
    if (def.fullPath == entryPath) {
      entryReturn = getReturnKind(def);
      break;
    }
  }
  if (entryReturn == ReturnKind::Void) {
    out << "int main() { " << nameMap.at(entryPath) << "(); return 0; }\n";
  } else if (entryReturn == ReturnKind::Int) {
    out << "int main() { return " << nameMap.at(entryPath) << "(); }\n";
  } else {
    out << "int main() { return static_cast<int>(" << nameMap.at(entryPath) << "()); }\n";
  }
  return out.str();
}

} // namespace primec
