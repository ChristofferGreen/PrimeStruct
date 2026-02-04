#include "primec/Emitter.h"

#include <algorithm>
#include <cstdint>
#include <functional>
#include <sstream>
#include <unordered_map>
#include <unordered_set>

namespace primec {
namespace {
enum class ReturnKind { Unknown, Int, Int64, UInt64, Float32, Float64, Bool, Void };

struct BindingInfo {
  std::string typeName;
  std::string typeTemplateArg;
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
  if (*transform.templateArg == "i64") {
    return ReturnKind::Int64;
  }
  if (*transform.templateArg == "u64") {
    return ReturnKind::UInt64;
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

BindingInfo getBindingInfo(const Expr &expr) {
  BindingInfo info;
  for (const auto &transform : expr.transforms) {
    if (transform.name == "mut" && !transform.templateArg && transform.arguments.empty()) {
      info.isMutable = true;
      continue;
    }
    if (transform.name == "copy" || transform.name == "restrict" || transform.name == "align_bytes" ||
        transform.name == "align_kbytes") {
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
        info.typeTemplateArg = *transform.templateArg;
      }
    }
  }
  if (info.typeName.empty()) {
    info.typeName = "int";
  }
  return info;
}

std::string bindingTypeToCpp(const BindingInfo &info);

std::string bindingTypeToCpp(const std::string &typeName) {
  if (typeName == "i32" || typeName == "int") {
    return "int";
  }
  if (typeName == "i64") {
    return "int64_t";
  }
  if (typeName == "u64") {
    return "uint64_t";
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
  if (typeName == "string") {
    return "const char *";
  }
  return "int";
}

std::string bindingTypeToCpp(const BindingInfo &info) {
  if (info.typeName == "Pointer") {
    std::string base = bindingTypeToCpp(info.typeTemplateArg);
    if (base.empty()) {
      base = "void";
    }
    return base + " *";
  }
  if (info.typeName == "Reference") {
    std::string base = bindingTypeToCpp(info.typeTemplateArg);
    if (base.empty()) {
      base = "void";
    }
    return base + " &";
  }
  return bindingTypeToCpp(info.typeName);
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
  if (name == "dereference") {
    out = '*';
    return true;
  }
  if (name == "location") {
    out = '&';
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
                              const std::unordered_map<std::string, std::vector<std::string>> &paramMap,
                              const std::unordered_map<std::string, std::string> &localTypes) const {
  std::function<bool(const Expr &)> isPointerExpr;
  isPointerExpr = [&](const Expr &candidate) -> bool {
    if (candidate.kind == Expr::Kind::Name) {
      auto it = localTypes.find(candidate.name);
      return it != localTypes.end() && it->second == "Pointer";
    }
    if (candidate.kind != Expr::Kind::Call) {
      return false;
    }
    if (isSimpleCallName(candidate, "location")) {
      return true;
    }
    char op = '\0';
    if (getBuiltinOperator(candidate, op) && candidate.args.size() == 2 && (op == '+' || op == '-')) {
      return isPointerExpr(candidate.args[0]) && !isPointerExpr(candidate.args[1]);
    }
    return false;
  };

  if (expr.kind == Expr::Kind::Literal) {
    if (!expr.isUnsigned && expr.intWidth == 32) {
      return std::to_string(static_cast<int64_t>(expr.literalValue));
    }
    if (expr.isUnsigned) {
      std::ostringstream out;
      out << "static_cast<uint64_t>(" << static_cast<uint64_t>(expr.literalValue) << ")";
      return out.str();
    }
    std::ostringstream out;
    out << "static_cast<int64_t>(" << static_cast<int64_t>(expr.literalValue) << ")";
    return out.str();
  }
  if (expr.kind == Expr::Kind::BoolLiteral) {
    return expr.boolValue ? "true" : "false";
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
      if ((op == '+' || op == '-') && isPointerExpr(expr.args[0])) {
        out << "ps_pointer_add(" << emitExpr(expr.args[0], nameMap, paramMap, localTypes) << ", ";
        if (op == '-') {
          out << "-(" << emitExpr(expr.args[1], nameMap, paramMap, localTypes) << ")";
        } else {
          out << emitExpr(expr.args[1], nameMap, paramMap, localTypes);
        }
        out << ")";
        return out.str();
      }
      out << "(" << emitExpr(expr.args[0], nameMap, paramMap, localTypes) << " " << op << " "
          << emitExpr(expr.args[1], nameMap, paramMap, localTypes) << ")";
      return out.str();
    }
    if (isBuiltinNegate(expr) && expr.args.size() == 1) {
      std::ostringstream out;
      out << "(-" << emitExpr(expr.args[0], nameMap, paramMap, localTypes) << ")";
      return out.str();
    }
    const char *cmp = nullptr;
    if (getBuiltinComparison(expr, cmp)) {
      std::ostringstream out;
      if (expr.args.size() == 1) {
        out << "(" << cmp << emitExpr(expr.args[0], nameMap, paramMap, localTypes) << ")";
        return out.str();
      }
      if (expr.args.size() == 2) {
        out << "(" << emitExpr(expr.args[0], nameMap, paramMap, localTypes) << " " << cmp << " "
            << emitExpr(expr.args[1], nameMap, paramMap, localTypes) << ")";
        return out.str();
      }
    }
    if (isBuiltinClamp(expr) && expr.args.size() == 3) {
      std::ostringstream out;
      out << "ps_builtin_clamp(" << emitExpr(expr.args[0], nameMap, paramMap, localTypes) << ", "
          << emitExpr(expr.args[1], nameMap, paramMap, localTypes) << ", "
          << emitExpr(expr.args[2], nameMap, paramMap, localTypes) << ")";
      return out.str();
    }
    std::string convertName;
    if (getBuiltinConvertName(expr, convertName) && expr.templateArgs.size() == 1 && expr.args.size() == 1) {
      std::string targetType = bindingTypeToCpp(expr.templateArgs[0]);
      std::ostringstream out;
      out << "static_cast<" << targetType << ">(" << emitExpr(expr.args[0], nameMap, paramMap, localTypes) << ")";
      return out.str();
    }
    char pointerOp = '\0';
    if (getBuiltinPointerOperator(expr, pointerOp) && expr.args.size() == 1) {
      std::ostringstream out;
      out << "(" << pointerOp << emitExpr(expr.args[0], nameMap, paramMap, localTypes) << ")";
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
          out << emitExpr(expr.args[i], nameMap, paramMap, localTypes);
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
          out << "{" << emitExpr(expr.args[i], nameMap, paramMap, localTypes) << ", "
              << emitExpr(expr.args[i + 1], nameMap, paramMap, localTypes) << "}";
        }
        out << "}";
        return out.str();
      }
    }
    if (isBuiltinAssign(expr, nameMap) && expr.args.size() == 2) {
      std::ostringstream out;
      out << "(" << emitExpr(expr.args[0], nameMap, paramMap, localTypes) << " = "
          << emitExpr(expr.args[1], nameMap, paramMap, localTypes) << ")";
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
    out << emitExpr(*orderedArgs[i], nameMap, paramMap, localTypes);
  }
  out << ")";
  return out.str();
}

std::string Emitter::emitCpp(const Program &program, const std::string &entryPath) const {
  std::unordered_map<std::string, std::string> nameMap;
  std::unordered_map<std::string, std::vector<std::string>> paramMap;
  std::unordered_map<std::string, const Definition *> defMap;
  std::unordered_map<std::string, ReturnKind> returnKinds;
  for (const auto &def : program.definitions) {
    nameMap[def.fullPath] = toCppName(def.fullPath);
    paramMap[def.fullPath] = def.parameters;
    defMap[def.fullPath] = &def;
    returnKinds[def.fullPath] = getReturnKind(def);
  }

  auto isParam = [](const std::vector<std::string> &params, const std::string &name) -> bool {
    for (const auto &param : params) {
      if (param == name) {
        return true;
      }
    }
    return false;
  };

  std::unordered_set<std::string> inferenceStack;
  std::function<ReturnKind(const Definition &)> inferDefinitionReturnKind;
  std::function<ReturnKind(const Expr &,
                           const std::vector<std::string> &,
                           const std::unordered_map<std::string, ReturnKind> &)>
      inferExprReturnKind;

  inferExprReturnKind = [&](const Expr &expr,
                            const std::vector<std::string> &params,
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
        if (left == ReturnKind::Int64 || left == ReturnKind::Int) {
          if (right == ReturnKind::Int64 || right == ReturnKind::Int) {
            return ReturnKind::Int64;
          }
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
      std::string full = resolveExprPath(expr);
      auto defIt = defMap.find(full);
      if (defIt != defMap.end()) {
        ReturnKind calleeKind = inferDefinitionReturnKind(*defIt->second);
        if (calleeKind == ReturnKind::Void || calleeKind == ReturnKind::Unknown) {
          return ReturnKind::Unknown;
        }
        return calleeKind;
      }
      const char *cmp = nullptr;
      if (getBuiltinComparison(expr, cmp)) {
        return ReturnKind::Bool;
      }
      char op = '\0';
      if (getBuiltinOperator(expr, op)) {
        if (expr.args.size() != 2) {
          return ReturnKind::Unknown;
        }
        ReturnKind left = inferExprReturnKind(expr.args[0], params, locals);
        ReturnKind right = inferExprReturnKind(expr.args[1], params, locals);
        return combineNumeric(left, right);
      }
      if (isBuiltinNegate(expr)) {
        if (expr.args.size() != 1) {
          return ReturnKind::Unknown;
        }
        ReturnKind argKind = inferExprReturnKind(expr.args.front(), params, locals);
        if (argKind == ReturnKind::Bool || argKind == ReturnKind::Void) {
          return ReturnKind::Unknown;
        }
        return argKind;
      }
      if (isBuiltinClamp(expr)) {
        if (expr.args.size() != 3) {
          return ReturnKind::Unknown;
        }
        ReturnKind result = inferExprReturnKind(expr.args[0], params, locals);
        result = combineNumeric(result, inferExprReturnKind(expr.args[1], params, locals));
        result = combineNumeric(result, inferExprReturnKind(expr.args[2], params, locals));
        return result;
      }
      std::string convertName;
      if (getBuiltinConvertName(expr, convertName) && expr.templateArgs.size() == 1) {
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
        BindingInfo info = getBindingInfo(stmt);
        ReturnKind bindingKind = returnKindForTypeName(info.typeName);
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
      if (isBuiltinIf(stmt, nameMap) && stmt.args.size() == 3) {
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
        return;
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
  out << "// Generated by primec (minimal)\n";
  out << "#include <cstdint>\n";
  out << "#include <type_traits>\n";
  out << "#include <unordered_map>\n";
  out << "#include <vector>\n";
  out << "template <typename T, typename Offset>\n";
  out << "static inline T *ps_pointer_add(T *ptr, Offset offset) {\n";
  out << "  if constexpr (std::is_signed_v<Offset>) {\n";
  out << "    return reinterpret_cast<T *>(reinterpret_cast<std::intptr_t>(ptr) + "
         "static_cast<std::intptr_t>(offset));\n";
  out << "  }\n";
  out << "  return reinterpret_cast<T *>(reinterpret_cast<std::uintptr_t>(ptr) + "
         "static_cast<std::uintptr_t>(offset));\n";
  out << "}\n";
  out << "template <typename T, typename Offset>\n";
  out << "static inline T *ps_pointer_add(T &ref, Offset offset) {\n";
  out << "  return ps_pointer_add(&ref, offset);\n";
  out << "}\n";
  out << "template <typename T, typename U, typename V>\n";
  out << "static inline std::common_type_t<T, U, V> ps_builtin_clamp(T value, U minValue, V maxValue) {\n";
  out << "  using R = std::common_type_t<T, U, V>;\n";
  out << "  R v = static_cast<R>(value);\n";
  out << "  R minV = static_cast<R>(minValue);\n";
  out << "  R maxV = static_cast<R>(maxValue);\n";
  out << "  if (v < minV) {\n";
  out << "    return minV;\n";
  out << "  }\n";
  out << "  if (v > maxV) {\n";
  out << "    return maxV;\n";
  out << "  }\n";
  out << "  return v;\n";
  out << "}\n";
  for (const auto &def : program.definitions) {
    ReturnKind returnKind = returnKinds[def.fullPath];
    std::string returnType = "int";
    if (returnKind == ReturnKind::Void) {
      returnType = "void";
    } else if (returnKind == ReturnKind::Int64) {
      returnType = "int64_t";
    } else if (returnKind == ReturnKind::UInt64) {
      returnType = "uint64_t";
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
    std::function<void(const Expr &, int, std::unordered_map<std::string, std::string> &)> emitStatement;
    emitStatement = [&](const Expr &stmt, int indent, std::unordered_map<std::string, std::string> &localTypes) {
      std::string pad(static_cast<size_t>(indent) * 2, ' ');
      if (isReturnCall(stmt)) {
        out << pad << "return";
        if (!stmt.args.empty()) {
          out << " " << emitExpr(stmt.args.front(), nameMap, paramMap, localTypes);
        }
        out << ";\n";
        return;
      }
      if (stmt.isBinding) {
        BindingInfo binding = getBindingInfo(stmt);
        std::string type = bindingTypeToCpp(binding);
        bool isReference = binding.typeName == "Reference";
        localTypes[stmt.name] = binding.typeName;
        out << pad << (binding.isMutable ? "" : "const ") << type << " " << stmt.name;
        if (!stmt.args.empty()) {
          if (isReference) {
            out << " = *(" << emitExpr(stmt.args.front(), nameMap, paramMap, localTypes) << ")";
          } else {
            out << " = " << emitExpr(stmt.args.front(), nameMap, paramMap, localTypes);
          }
        }
        out << ";\n";
        return;
      }
      if (isBuiltinIf(stmt, nameMap) && stmt.args.size() == 3) {
        const Expr &cond = stmt.args[0];
        const Expr &thenBlock = stmt.args[1];
        const Expr &elseBlock = stmt.args[2];
        out << pad << "if (" << emitExpr(cond, nameMap, paramMap, localTypes) << ") {\n";
        if (isThenCall(thenBlock)) {
          auto blockTypes = localTypes;
          for (const auto &bodyStmt : thenBlock.bodyArguments) {
            emitStatement(bodyStmt, indent + 1, blockTypes);
          }
        }
        out << pad << "} else {\n";
        if (isElseCall(elseBlock)) {
          auto blockTypes = localTypes;
          for (const auto &bodyStmt : elseBlock.bodyArguments) {
            emitStatement(bodyStmt, indent + 1, blockTypes);
          }
        }
        out << pad << "}\n";
        return;
      }
      if (stmt.kind == Expr::Kind::Call && !stmt.bodyArguments.empty()) {
        std::string full = resolveExprPath(stmt);
        auto it = nameMap.find(full);
        if (it == nameMap.end()) {
          out << pad << emitExpr(stmt, nameMap, paramMap, localTypes) << ";\n";
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
          out << emitExpr(*orderedArgs[i], nameMap, paramMap, localTypes);
        }
        if (!orderedArgs.empty()) {
          out << ", ";
        }
        out << "[&]() {\n";
        for (const auto &bodyStmt : stmt.bodyArguments) {
          emitStatement(bodyStmt, indent + 1, localTypes);
        }
        out << pad << "}";
        out << ");\n";
        return;
      }
      if (stmt.kind == Expr::Kind::Call && isBuiltinAssign(stmt, nameMap) && stmt.args.size() == 2 &&
          stmt.args.front().kind == Expr::Kind::Name) {
        out << pad << stmt.args.front().name << " = " << emitExpr(stmt.args[1], nameMap, paramMap, localTypes) << ";\n";
        return;
      }
      out << pad << emitExpr(stmt, nameMap, paramMap, localTypes) << ";\n";
    };
    std::unordered_map<std::string, std::string> localTypes;
    for (const auto &stmt : def.statements) {
      emitStatement(stmt, 1, localTypes);
    }
    if (returnKind == ReturnKind::Void) {
      out << "  return;\n";
    }
    out << "}\n";
  }
  ReturnKind entryReturn = ReturnKind::Int;
  for (const auto &def : program.definitions) {
    if (def.fullPath == entryPath) {
      entryReturn = returnKinds[def.fullPath];
      break;
    }
  }
  if (entryReturn == ReturnKind::Void) {
    out << "int main() { " << nameMap.at(entryPath) << "(); return 0; }\n";
  } else if (entryReturn == ReturnKind::Int) {
    out << "int main() { return " << nameMap.at(entryPath) << "(); }\n";
  } else if (entryReturn == ReturnKind::Int64 || entryReturn == ReturnKind::UInt64) {
    out << "int main() { return static_cast<int>(" << nameMap.at(entryPath) << "()); }\n";
  } else {
    out << "int main() { return static_cast<int>(" << nameMap.at(entryPath) << "()); }\n";
  }
  return out.str();
}

} // namespace primec
