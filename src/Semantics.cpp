#include "primec/Semantics.h"

#include <cctype>
#include <functional>
#include <limits>
#include <unordered_map>
#include <unordered_set>

namespace primec {
namespace {
enum class ReturnKind { Unknown, Int, Float32, Float64, Bool, Void };

struct BindingInfo {
  std::string typeName;
  bool isMutable = false;
};

bool validateAlignTransform(const Transform &transform, const std::string &context, std::string &error);

bool isBindingQualifierName(const std::string &name) {
  return name == "public" || name == "private" || name == "package" || name == "static";
}

bool isPrimitiveBindingTypeName(const std::string &name) {
  return name == "int" || name == "i32" || name == "float" || name == "f32" || name == "f64" || name == "bool" ||
         name == "string";
}

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

bool getBuiltinPointerName(const Expr &expr, std::string &out) {
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
  if (name == "deref" || name == "address_of") {
    out = name;
    return true;
  }
  return false;
}

bool getBuiltinPointerBinaryName(const Expr &expr, std::string &out) {
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
    out = name;
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

std::string resolveTypePath(const std::string &name, const std::string &namespacePrefix) {
  if (!name.empty() && name[0] == '/') {
    return name;
  }
  if (!namespacePrefix.empty()) {
    return namespacePrefix + "/" + name;
  }
  return "/" + name;
}

bool parseBindingInfo(const Expr &expr,
                      const std::string &namespacePrefix,
                      const std::unordered_set<std::string> &structTypes,
                      BindingInfo &info,
                      std::string &error) {
  std::string typeName;
  bool typeHasTemplate = false;
  for (const auto &transform : expr.transforms) {
    if (transform.name == "mut") {
      if (transform.templateArg) {
        error = "binding transforms do not take template arguments";
        return false;
      }
      if (!transform.arguments.empty()) {
        error = "binding transforms do not take arguments";
        return false;
      }
      info.isMutable = true;
      continue;
    }
    if (transform.name == "copy") {
      if (transform.templateArg) {
        error = "binding transforms do not take template arguments";
        return false;
      }
      if (!transform.arguments.empty()) {
        error = "binding transforms do not take arguments";
        return false;
      }
      continue;
    }
    if (transform.name == "restrict") {
      if (!transform.templateArg) {
        error = "restrict requires a template argument";
        return false;
      }
      if (!transform.arguments.empty()) {
        error = "binding transforms do not take arguments";
        return false;
      }
      continue;
    }
    if (isBindingQualifierName(transform.name)) {
      if (transform.templateArg) {
        error = "binding transforms do not take template arguments";
        return false;
      }
      if (!transform.arguments.empty()) {
        error = "binding transforms do not take arguments";
        return false;
      }
      continue;
    }
    if (transform.name == "align_bytes" || transform.name == "align_kbytes") {
      if (!validateAlignTransform(transform, "binding " + expr.name, error)) {
        return false;
      }
      continue;
    }
    if (isPrimitiveBindingTypeName(transform.name)) {
      if (transform.templateArg) {
        error = "binding transforms do not take template arguments";
        return false;
      }
      if (!transform.arguments.empty()) {
        error = "binding transforms do not take arguments";
        return false;
      }
    }
    if (transform.templateArg) {
      if (!transform.arguments.empty()) {
        error = "binding transforms do not take arguments";
        return false;
      }
      if (!typeName.empty()) {
        error = "binding requires exactly one type";
        return false;
      }
      typeName = transform.name;
      typeHasTemplate = true;
      continue;
    }
    if (!transform.arguments.empty()) {
      continue;
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
  if (!isPrimitiveBindingTypeName(typeName) && !typeHasTemplate) {
    std::string resolved = resolveTypePath(typeName, namespacePrefix);
    if (structTypes.count(resolved) == 0) {
      error = "unsupported binding type: " + typeName;
      return false;
    }
  }
  if (isPrimitiveBindingTypeName(typeName)) {
    info.typeName = typeName;
    return true;
  }
  if (typeHasTemplate) {
    info.typeName = typeName;
    return true;
  }
  info.typeName = typeName;
  return true;
}

bool isEffectName(const std::string &text) {
  if (text.empty()) {
    return false;
  }
  unsigned char first = static_cast<unsigned char>(text[0]);
  if (!std::isalpha(first) && text[0] != '_') {
    return false;
  }
  for (size_t i = 1; i < text.size(); ++i) {
    unsigned char c = static_cast<unsigned char>(text[i]);
    if (!std::isalnum(c) && text[i] != '_') {
      return false;
    }
  }
  return true;
}

bool validateEffectsTransform(const Transform &transform, const std::string &context, std::string &error) {
  if (transform.templateArg) {
    error = "effects transform does not accept template arguments on " + context;
    return false;
  }
  std::unordered_set<std::string> seen;
  for (const auto &arg : transform.arguments) {
    if (!isEffectName(arg)) {
      error = "invalid effects capability: " + arg;
      return false;
    }
    if (!seen.insert(arg).second) {
      error = "duplicate effects capability: " + arg;
      return false;
    }
  }
  return true;
}

bool validateCapabilitiesTransform(const Transform &transform, const std::string &context, std::string &error) {
  if (transform.templateArg) {
    error = "capabilities transform does not accept template arguments on " + context;
    return false;
  }
  std::unordered_set<std::string> seen;
  for (const auto &arg : transform.arguments) {
    if (!isEffectName(arg)) {
      error = "invalid capability: " + arg;
      return false;
    }
    if (!seen.insert(arg).second) {
      error = "duplicate capability: " + arg;
      return false;
    }
  }
  return true;
}

bool parsePositiveIntArg(const std::string &text, int &value) {
  std::string digits = text;
  if (digits.size() > 3 && digits.compare(digits.size() - 3, 3, "i32") == 0) {
    digits.resize(digits.size() - 3);
  }
  if (digits.empty()) {
    return false;
  }
  int parsed = 0;
  for (char c : digits) {
    if (!std::isdigit(static_cast<unsigned char>(c))) {
      return false;
    }
    int digit = c - '0';
    if (parsed > (std::numeric_limits<int>::max() - digit) / 10) {
      return false;
    }
    parsed = parsed * 10 + digit;
  }
  if (parsed <= 0) {
    return false;
  }
  value = parsed;
  return true;
}

bool validateAlignTransform(const Transform &transform, const std::string &context, std::string &error) {
  if (transform.templateArg) {
    error = transform.name + " does not accept template arguments on " + context;
    return false;
  }
  if (transform.arguments.size() != 1) {
    error = transform.name + " requires exactly one integer argument";
    return false;
  }
  int value = 0;
  if (!parsePositiveIntArg(transform.arguments[0], value)) {
    error = transform.name + " requires a positive integer argument";
    return false;
  }
  return true;
}

bool isStructTransformName(const std::string &name) {
  return name == "struct" || name == "pod" || name == "stack" || name == "heap" || name == "buffer";
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
  std::unordered_set<std::string> structNames;
  for (const auto &def : program.definitions) {
    if (defMap.count(def.fullPath) > 0) {
      error = "duplicate definition: " + def.fullPath;
      return false;
    }
    for (const auto &transform : def.transforms) {
      if (transform.name == "effects") {
        if (!validateEffectsTransform(transform, def.fullPath, error)) {
          return false;
        }
      } else if (transform.name == "capabilities") {
        if (!validateCapabilitiesTransform(transform, def.fullPath, error)) {
          return false;
        }
      } else if (transform.name == "align_bytes" || transform.name == "align_kbytes") {
        if (!validateAlignTransform(transform, def.fullPath, error)) {
          return false;
        }
      } else if (isStructTransformName(transform.name)) {
        if (transform.templateArg) {
          error = "struct transform does not accept template arguments on " + def.fullPath;
          return false;
        }
        if (!transform.arguments.empty()) {
          error = "struct transform does not accept arguments on " + def.fullPath;
          return false;
        }
      }
    }
    if (def.name == "Create" || def.name == "Destroy" || def.name == "CreateStack" ||
        def.name == "DestroyStack" || def.name == "CreateHeap" || def.name == "DestroyHeap" ||
        def.name == "CreateBuffer" || def.name == "DestroyBuffer") {
      error = "lifecycle helper must be nested inside a struct: " + def.fullPath;
      return false;
    }
    bool isStruct = false;
    bool hasReturnTransform = false;
    for (const auto &transform : def.transforms) {
      if (transform.name == "return") {
        hasReturnTransform = true;
      }
      if (isStructTransformName(transform.name)) {
        isStruct = true;
      }
    }
    if (isStruct) {
      structNames.insert(def.fullPath);
      if (hasReturnTransform) {
        error = "struct definitions cannot declare return types: " + def.fullPath;
        return false;
      }
      if (def.hasReturnStatement) {
        error = "struct definitions cannot contain return statements: " + def.fullPath;
        return false;
      }
      if (def.returnExpr.has_value()) {
        error = "struct definitions cannot return a value: " + def.fullPath;
        return false;
      }
      if (!def.parameters.empty()) {
        error = "struct definitions cannot declare parameters: " + def.fullPath;
        return false;
      }
      for (const auto &stmt : def.statements) {
        if (!stmt.isBinding) {
          error = "struct definitions may only contain field bindings: " + def.fullPath;
          return false;
        }
      }
    }
    ReturnKind kind = ReturnKind::Void;
    if (!isStruct) {
      kind = getReturnKind(def, error);
      if (!error.empty()) {
        return false;
      }
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
        error = "block arguments are only supported on statement calls";
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
        if (getBuiltinConvertName(expr, builtinName)) {
          if (expr.templateArgs.size() != 1) {
            error = "convert requires exactly one template argument";
            return false;
          }
          const std::string &typeName = expr.templateArgs[0];
          if (typeName != "int" && typeName != "i32" && typeName != "bool" && typeName != "float" &&
              typeName != "f32" && typeName != "f64") {
            error = "unsupported convert target type: " + typeName;
            return false;
          }
          if (expr.args.size() != 1) {
            error = "argument count mismatch for builtin " + builtinName;
            return false;
          }
          if (!validateExpr(params, locals, expr.args.front())) {
            return false;
          }
          return true;
        }
        if (getBuiltinPointerName(expr, builtinName)) {
          if (!expr.templateArgs.empty()) {
            error = "pointer helpers do not accept template arguments";
            return false;
          }
          if (expr.args.size() != 1) {
            error = "argument count mismatch for builtin " + builtinName;
            return false;
          }
          if (!validateExpr(params, locals, expr.args.front())) {
            return false;
          }
          return true;
        }
        if (getBuiltinPointerBinaryName(expr, builtinName)) {
          if (!expr.templateArgs.empty()) {
            error = "pointer helpers do not accept template arguments";
            return false;
          }
          if (expr.args.size() != 2) {
            error = "argument count mismatch for builtin " + builtinName;
            return false;
          }
          if (!validateExpr(params, locals, expr.args[0])) {
            return false;
          }
          if (!validateExpr(params, locals, expr.args[1])) {
            return false;
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
                     bool *,
                     const std::string &)>
      validateStatement = [&](const std::vector<std::string> &params,
                              std::unordered_map<std::string, BindingInfo> &locals,
                              const Expr &stmt,
                              ReturnKind returnKind,
                              bool allowReturn,
                              bool *sawReturn,
                              const std::string &namespacePrefix) -> bool {
    if (stmt.isBinding) {
      if (!stmt.bodyArguments.empty()) {
        error = "binding does not accept block arguments";
        return false;
      }
      if (isParam(params, stmt.name) || locals.count(stmt.name) > 0) {
        error = "duplicate binding name: " + stmt.name;
        return false;
      }
      BindingInfo info;
      if (!parseBindingInfo(stmt, namespacePrefix, structNames, info, error)) {
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
          if (!validateStatement(params,
                                 blockLocals,
                                 bodyExpr,
                                 returnKind,
                                 allowReturn,
                                 sawReturn,
                                 namespacePrefix)) {
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
      if (isThenCall(stmt) || isElseCall(stmt)) {
        error = "then/else blocks must be nested inside if";
        return false;
      }
      std::string resolved = resolveCalleePath(stmt);
      if (defMap.count(resolved) == 0) {
        error = "block arguments require a definition target: " + resolved;
        return false;
      }
      Expr call = stmt;
      call.bodyArguments.clear();
      if (!validateExpr(params, locals, call)) {
        return false;
      }
      std::unordered_map<std::string, BindingInfo> blockLocals = locals;
      for (const auto &bodyExpr : stmt.bodyArguments) {
        if (!validateStatement(params, blockLocals, bodyExpr, returnKind, false, nullptr, namespacePrefix)) {
          return false;
        }
      }
      return true;
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
      if (!validateStatement(def.parameters, locals, stmt, kind, true, &sawReturn, def.namespacePrefix)) {
        return false;
      }
    }
    if (kind != ReturnKind::Void && !sawReturn) {
      error = "missing return statement in " + def.fullPath;
      return false;
    }
  }

  for (const auto &exec : program.executions) {
    for (const auto &transform : exec.transforms) {
      if (transform.name == "effects") {
        if (!validateEffectsTransform(transform, exec.fullPath, error)) {
          return false;
        }
      } else if (transform.name == "capabilities") {
        if (!validateCapabilitiesTransform(transform, exec.fullPath, error)) {
          return false;
        }
      } else if (transform.name == "align_bytes" || transform.name == "align_kbytes") {
        if (!validateAlignTransform(transform, exec.fullPath, error)) {
          return false;
        }
      } else if (isStructTransformName(transform.name)) {
        error = "struct transforms are not allowed on executions: " + exec.fullPath;
        return false;
      }
    }
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
      if (!validateStatement({}, execLocals, arg, ReturnKind::Unknown, false, nullptr, exec.namespacePrefix)) {
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
