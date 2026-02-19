#include "SemanticsHelpers.h"

#include <cctype>
#include <functional>
#include <limits>

namespace primec::semantics {

bool isAsciiAlpha(char c) {
  return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
}

bool isAsciiDigit(char c) {
  return c >= '0' && c <= '9';
}

bool isEffectName(const std::string &text) {
  if (text.empty()) {
    return false;
  }
  if (!isAsciiAlpha(text[0]) && text[0] != '_') {
    return false;
  }
  for (size_t i = 1; i < text.size(); ++i) {
    if (!isAsciiAlpha(text[i]) && !isAsciiDigit(text[i]) && text[i] != '_') {
      return false;
    }
  }
  return true;
}

bool validateEffectsTransform(const Transform &transform, const std::string &context, std::string &error) {
  if (!transform.templateArgs.empty()) {
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
  if (!transform.templateArgs.empty()) {
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
  if (text.empty()) {
    return false;
  }
  std::string cleaned;
  cleaned.reserve(text.size());
  for (char c : text) {
    if (c != ',') {
      cleaned.push_back(c);
    }
  }
  std::string digits = cleaned;
  if (digits.size() > 3) {
    const std::string suffix = digits.substr(digits.size() - 3);
    if (suffix == "i32" || suffix == "i64" || suffix == "u64") {
      digits.resize(digits.size() - 3);
    }
  }
  if (digits.empty() || digits[0] == '-') {
    return false;
  }
  int base = 10;
  if (digits.size() > 2 && digits[0] == '0' && (digits[1] == 'x' || digits[1] == 'X')) {
    base = 16;
    digits = digits.substr(2);
  }
  if (digits.empty()) {
    return false;
  }
  for (char c : digits) {
    if (base == 16) {
      if (!std::isxdigit(static_cast<unsigned char>(c))) {
        return false;
      }
    } else if (!std::isdigit(static_cast<unsigned char>(c))) {
      return false;
    }
  }
  unsigned long long parsed = 0;
  try {
    parsed = std::stoull(digits, nullptr, base);
  } catch (const std::exception &) {
    return false;
  }
  if (parsed == 0 || parsed > static_cast<unsigned long long>(std::numeric_limits<int>::max())) {
    return false;
  }
  value = static_cast<int>(parsed);
  return true;
}

bool validateAlignTransform(const Transform &transform, const std::string &context, std::string &error) {
  if (!transform.templateArgs.empty()) {
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
  return name == "struct" || name == "pod" || name == "handle" || name == "gpu_lane" || name == "no_padding" ||
         name == "platform_independent_padding";
}

bool validateNamedArguments(const std::vector<Expr> &args,
                            const std::vector<std::optional<std::string>> &argNames,
                            const std::string &context,
                            std::string &error) {
  if (!argNames.empty() && argNames.size() != args.size()) {
    error = "argument name count mismatch for " + context;
    return false;
  }
  std::unordered_set<std::string> used;
  for (size_t i = 0; i < args.size(); ++i) {
    if (i < argNames.size() && argNames[i].has_value()) {
      const std::string &name = *argNames[i];
      if (!used.insert(name).second) {
        error = "duplicate named argument: " + name;
        return false;
      }
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

bool isDefaultExprAllowed(const Expr &expr) {
  if (expr.isBinding) {
    return false;
  }
  if (expr.kind == Expr::Kind::Name) {
    return false;
  }
  if (expr.kind == Expr::Kind::Call) {
    if (hasNamedArguments(expr.argNames)) {
      return false;
    }
    if (expr.hasBodyArguments || !expr.bodyArguments.empty()) {
      return false;
    }
    std::string builtinName;
    if (!(getBuiltinOperatorName(expr, builtinName) || getBuiltinComparisonName(expr, builtinName) ||
          getBuiltinClampName(expr, builtinName, true) || getBuiltinMinMaxName(expr, builtinName, true) ||
          getBuiltinAbsSignName(expr, builtinName, true) || getBuiltinSaturateName(expr, builtinName, true) ||
          getBuiltinMathName(expr, builtinName, true) || getBuiltinConvertName(expr, builtinName) ||
          getBuiltinCollectionName(expr, builtinName) || getBuiltinArrayAccessName(expr, builtinName) ||
          isIfCall(expr))) {
      return false;
    }
    for (const auto &arg : expr.args) {
      if (!isDefaultExprAllowed(arg)) {
        return false;
      }
    }
    return true;
  }
  return true;
}

std::string inferPrimitiveBindingTypeFromInitializer(const Expr &expr) {
  if (expr.kind == Expr::Kind::Literal) {
    if (expr.isUnsigned) {
      return "u64";
    }
    return expr.intWidth == 64 ? "i64" : "i32";
  }
  if (expr.kind == Expr::Kind::BoolLiteral) {
    return "bool";
  }
  if (expr.kind == Expr::Kind::FloatLiteral) {
    return expr.floatWidth == 64 ? "f64" : "f32";
  }
  if (expr.kind == Expr::Kind::StringLiteral) {
    return "string";
  }
  std::string builtinName;
  if (expr.kind == Expr::Kind::Call && getBuiltinConvertName(expr, builtinName) && expr.templateArgs.size() == 1) {
    const std::string &typeName = expr.templateArgs.front();
    if (isPrimitiveBindingTypeName(typeName)) {
      return typeName;
    }
  }
  return "";
}

bool tryInferBindingTypeFromInitializer(const Expr &initializer,
                                        const std::vector<ParameterInfo> &params,
                                        const std::unordered_map<std::string, BindingInfo> &locals,
                                        BindingInfo &bindingOut,
                                        bool allowMathBare) {
  const auto findParamBinding = [&](const std::string &name) -> const BindingInfo * {
    for (const auto &param : params) {
      if (param.name == name) {
        return &param.binding;
      }
    }
    return nullptr;
  };

  std::string inferredPrimitive = inferPrimitiveBindingTypeFromInitializer(initializer);
  if (!inferredPrimitive.empty()) {
    bindingOut.typeName = inferredPrimitive;
    bindingOut.typeTemplateArg.clear();
    return true;
  }
  if (initializer.kind == Expr::Kind::Name) {
    const std::string &name = initializer.name;
    if (const BindingInfo *paramBinding = findParamBinding(name)) {
      bindingOut.typeName = paramBinding->typeName;
      bindingOut.typeTemplateArg = paramBinding->typeTemplateArg;
      return true;
    }
    auto it = locals.find(name);
    if (it != locals.end()) {
      bindingOut.typeName = it->second.typeName;
      bindingOut.typeTemplateArg = it->second.typeTemplateArg;
      return true;
    }
    return false;
  }
  if (initializer.kind == Expr::Kind::Call) {
    std::string collection;
    if (getBuiltinCollectionName(initializer, collection)) {
      if ((collection == "array" || collection == "vector") && initializer.templateArgs.size() == 1) {
        bindingOut.typeName = collection;
        bindingOut.typeTemplateArg = initializer.templateArgs.front();
        return true;
      }
      if (collection == "map" && initializer.templateArgs.size() == 2) {
        bindingOut.typeName = "map";
        bindingOut.typeTemplateArg = joinTemplateArgs(initializer.templateArgs);
        return true;
      }
    }
  }
  const auto typeNameForKind = [](ReturnKind kind) -> std::string {
    switch (kind) {
      case ReturnKind::Int:
        return "i32";
      case ReturnKind::Int64:
        return "i64";
      case ReturnKind::UInt64:
        return "u64";
      case ReturnKind::Bool:
        return "bool";
      case ReturnKind::Float32:
        return "f32";
      case ReturnKind::Float64:
        return "f64";
      case ReturnKind::Array:
        return "array";
      default:
        return "";
    }
  };
  const auto combineNumeric = [](ReturnKind left, ReturnKind right) -> ReturnKind {
    if (left == ReturnKind::Unknown || right == ReturnKind::Unknown) {
      return ReturnKind::Unknown;
    }
    if (left == ReturnKind::Bool || right == ReturnKind::Bool || left == ReturnKind::Void || right == ReturnKind::Void ||
        left == ReturnKind::Array || right == ReturnKind::Array) {
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
      if ((left == ReturnKind::Int64 || left == ReturnKind::Int) && (right == ReturnKind::Int64 || right == ReturnKind::Int)) {
        return ReturnKind::Int64;
      }
      return ReturnKind::Unknown;
    }
    if (left == ReturnKind::Int && right == ReturnKind::Int) {
      return ReturnKind::Int;
    }
    return ReturnKind::Unknown;
  };

  std::function<ReturnKind(const Expr &)> inferPrimitiveReturnKind;
  inferPrimitiveReturnKind = [&](const Expr &expr) -> ReturnKind {
    std::string inferred = inferPrimitiveBindingTypeFromInitializer(expr);
    if (!inferred.empty()) {
      return returnKindForTypeName(normalizeBindingTypeName(inferred));
    }
    if (expr.kind == Expr::Kind::Name) {
      if (isBuiltinMathConstant(expr.name, allowMathBare)) {
        return ReturnKind::Float64;
      }
      const BindingInfo *binding = findParamBinding(expr.name);
      if (!binding) {
        auto it = locals.find(expr.name);
        if (it != locals.end()) {
          binding = &it->second;
        }
      }
      if (!binding) {
        return ReturnKind::Unknown;
      }
      if (binding->typeName == "Reference" && !binding->typeTemplateArg.empty()) {
        ReturnKind kind = returnKindForTypeName(normalizeBindingTypeName(binding->typeTemplateArg));
        return kind == ReturnKind::Unknown ? ReturnKind::Unknown : kind;
      }
      ReturnKind kind = returnKindForTypeName(normalizeBindingTypeName(binding->typeName));
      return kind == ReturnKind::Unknown ? ReturnKind::Unknown : kind;
    }
    if (expr.kind == Expr::Kind::Call) {
      if (expr.isMethodCall) {
        return ReturnKind::Unknown;
      }
      if (expr.hasBodyArguments && expr.args.empty()) {
        ReturnKind last = ReturnKind::Unknown;
        bool sawValue = false;
        for (const auto &bodyExpr : expr.bodyArguments) {
          if (bodyExpr.isBinding) {
            continue;
          }
          sawValue = true;
          last = inferPrimitiveReturnKind(bodyExpr);
        }
        return sawValue ? last : ReturnKind::Unknown;
      }
      std::string builtinName;
      if (getBuiltinComparisonName(expr, builtinName)) {
        return ReturnKind::Bool;
      }
      if (getBuiltinMathName(expr, builtinName, allowMathBare)) {
        if (builtinName == "is_nan" || builtinName == "is_inf" || builtinName == "is_finite") {
          return ReturnKind::Bool;
        }
        if (builtinName == "lerp" && expr.args.size() == 3) {
          ReturnKind result = inferPrimitiveReturnKind(expr.args[0]);
          result = combineNumeric(result, inferPrimitiveReturnKind(expr.args[1]));
          result = combineNumeric(result, inferPrimitiveReturnKind(expr.args[2]));
          return result;
        }
        if (expr.args.empty()) {
          return ReturnKind::Unknown;
        }
        ReturnKind argKind = inferPrimitiveReturnKind(expr.args.front());
        if (argKind == ReturnKind::Float64) {
          return ReturnKind::Float64;
        }
        if (argKind == ReturnKind::Float32) {
          return ReturnKind::Float32;
        }
        return ReturnKind::Unknown;
      }
      if (getBuiltinOperatorName(expr, builtinName)) {
        if (builtinName == "negate") {
          if (expr.args.size() != 1) {
            return ReturnKind::Unknown;
          }
          ReturnKind argKind = inferPrimitiveReturnKind(expr.args.front());
          if (argKind == ReturnKind::Bool || argKind == ReturnKind::Void) {
            return ReturnKind::Unknown;
          }
          return argKind;
        }
        if (expr.args.size() != 2) {
          return ReturnKind::Unknown;
        }
        ReturnKind left = inferPrimitiveReturnKind(expr.args[0]);
        ReturnKind right = inferPrimitiveReturnKind(expr.args[1]);
        return combineNumeric(left, right);
      }
      if (getBuiltinClampName(expr, builtinName, allowMathBare)) {
        if (expr.args.size() != 3) {
          return ReturnKind::Unknown;
        }
        ReturnKind result = inferPrimitiveReturnKind(expr.args[0]);
        result = combineNumeric(result, inferPrimitiveReturnKind(expr.args[1]));
        result = combineNumeric(result, inferPrimitiveReturnKind(expr.args[2]));
        return result;
      }
      if (getBuiltinMinMaxName(expr, builtinName, allowMathBare)) {
        if (expr.args.size() != 2) {
          return ReturnKind::Unknown;
        }
        ReturnKind result = inferPrimitiveReturnKind(expr.args[0]);
        result = combineNumeric(result, inferPrimitiveReturnKind(expr.args[1]));
        return result;
      }
      if (getBuiltinAbsSignName(expr, builtinName, allowMathBare)) {
        if (expr.args.size() != 1) {
          return ReturnKind::Unknown;
        }
        ReturnKind argKind = inferPrimitiveReturnKind(expr.args.front());
        if (argKind == ReturnKind::Bool || argKind == ReturnKind::Void) {
          return ReturnKind::Unknown;
        }
        return argKind;
      }
      if (getBuiltinSaturateName(expr, builtinName, allowMathBare)) {
        if (expr.args.size() != 1) {
          return ReturnKind::Unknown;
        }
        ReturnKind argKind = inferPrimitiveReturnKind(expr.args.front());
        if (argKind == ReturnKind::Bool || argKind == ReturnKind::Void) {
          return ReturnKind::Unknown;
        }
        return argKind;
      }
      if (getBuiltinConvertName(expr, builtinName)) {
        if (expr.templateArgs.size() != 1) {
          return ReturnKind::Unknown;
        }
        ReturnKind kind = returnKindForTypeName(normalizeBindingTypeName(expr.templateArgs.front()));
        return kind == ReturnKind::Unknown ? ReturnKind::Unknown : kind;
      }
      if (getBuiltinMutationName(expr, builtinName)) {
        if (expr.args.size() != 1) {
          return ReturnKind::Unknown;
        }
        return inferPrimitiveReturnKind(expr.args.front());
      }
      if (isAssignCall(expr)) {
        if (expr.args.size() != 2) {
          return ReturnKind::Unknown;
        }
        return inferPrimitiveReturnKind(expr.args[1]);
      }
      if (isIfCall(expr)) {
        if (expr.args.size() != 3) {
          return ReturnKind::Unknown;
        }
        ReturnKind thenKind = inferPrimitiveReturnKind(expr.args[1]);
        ReturnKind elseKind = inferPrimitiveReturnKind(expr.args[2]);
        if (thenKind == elseKind) {
          return thenKind;
        }
        return combineNumeric(thenKind, elseKind);
      }
      return ReturnKind::Unknown;
    }
    return ReturnKind::Unknown;
  };

  ReturnKind inferredKind = inferPrimitiveReturnKind(initializer);
  std::string inferredType = typeNameForKind(inferredKind);
  if (!inferredType.empty()) {
    bindingOut.typeName = inferredType;
    bindingOut.typeTemplateArg.clear();
    return true;
  }
  return false;
}

bool buildOrderedArguments(const std::vector<ParameterInfo> &params,
                           const std::vector<Expr> &args,
                           const std::vector<std::optional<std::string>> &argNames,
                           std::vector<const Expr *> &ordered,
                           std::string &error) {
  ordered.assign(params.size(), nullptr);
  size_t positionalIndex = 0;
  for (size_t i = 0; i < args.size(); ++i) {
    if (i < argNames.size() && argNames[i].has_value()) {
      const std::string &name = *argNames[i];
      size_t index = params.size();
      for (size_t p = 0; p < params.size(); ++p) {
        if (params[p].name == name) {
          index = p;
          break;
        }
      }
      if (index >= params.size()) {
        error = "unknown named argument: " + name;
        return false;
      }
      if (ordered[index] != nullptr) {
        error = "named argument duplicates parameter: " + name;
        return false;
      }
      ordered[index] = &args[i];
      continue;
    }
    while (positionalIndex < params.size() && ordered[positionalIndex] != nullptr) {
      ++positionalIndex;
    }
    if (positionalIndex >= params.size()) {
      error = "argument count mismatch";
      return false;
    }
    ordered[positionalIndex] = &args[i];
    ++positionalIndex;
  }
  for (size_t i = 0; i < params.size(); ++i) {
    if (ordered[i] != nullptr) {
      continue;
    }
    if (params[i].defaultExpr) {
      ordered[i] = params[i].defaultExpr;
      continue;
    }
    error = "argument count mismatch";
    return false;
  }
  return true;
}

bool validateNamedArgumentsAgainstParams(const std::vector<ParameterInfo> &params,
                                         const std::vector<std::optional<std::string>> &argNames,
                                         std::string &error) {
  if (argNames.empty()) {
    return true;
  }
  std::vector<bool> bound(params.size(), false);
  size_t positionalIndex = 0;
  for (const auto &argName : argNames) {
    if (argName.has_value()) {
      const std::string &name = *argName;
      size_t index = params.size();
      for (size_t p = 0; p < params.size(); ++p) {
        if (params[p].name == name) {
          index = p;
          break;
        }
      }
      if (index >= params.size()) {
        error = "unknown named argument: " + name;
        return false;
      }
      if (bound[index]) {
        error = "named argument duplicates parameter: " + name;
        return false;
      }
      bound[index] = true;
      continue;
    }
    while (positionalIndex < params.size() && bound[positionalIndex]) {
      ++positionalIndex;
    }
    if (positionalIndex >= params.size()) {
      error = "argument count mismatch";
      return false;
    }
    bound[positionalIndex] = true;
    ++positionalIndex;
  }
  return true;
}

} // namespace primec::semantics
