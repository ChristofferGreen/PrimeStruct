#include "SemanticsValidator.h"
#include "SemanticsValidatorExprCaptureSplitStep.h"

#include "primec/StringLiteral.h"

#include <cctype>
#include <functional>
#include <optional>
#include <unordered_set>

namespace primec::semantics {

bool SemanticsValidator::validateExpr(const std::vector<ParameterInfo> &params,
                                      const std::unordered_map<std::string, BindingInfo> &locals,
                                      const Expr &expr,
                                      const std::vector<Expr> *enclosingStatements,
                                      size_t statementIndex) {
  ExprContextScope exprScope(*this, expr);
  if (expr.isLambda) {
    auto addCapturedBinding = [&](std::unordered_map<std::string, BindingInfo> &lambdaLocals,
                                  const std::string &name) -> bool {
      if (lambdaLocals.count(name) > 0) {
        error_ = "duplicate lambda capture: " + name;
        return false;
      }
      if (movedBindings_.count(name) > 0) {
        error_ = "use-after-move: " + name;
        return false;
      }
      if (const BindingInfo *paramBinding = findParamBinding(params, name)) {
        lambdaLocals.emplace(name, *paramBinding);
        return true;
      }
      auto it = locals.find(name);
      if (it != locals.end()) {
        lambdaLocals.emplace(name, it->second);
        return true;
      }
      error_ = "unknown capture: " + name;
      return false;
    };

    if (!expr.hasBodyArguments && expr.bodyArguments.empty()) {
      error_ = "lambda requires a body";
      return false;
    }

    std::unordered_map<std::string, BindingInfo> lambdaLocals;
    if (!expr.lambdaCaptures.empty()) {
      bool captureAll = false;
      std::string captureAllToken;
      std::vector<std::string> captureNames;
      std::unordered_set<std::string> explicitNames;
      captureNames.reserve(expr.lambdaCaptures.size());
      explicitNames.reserve(expr.lambdaCaptures.size());
      for (const auto &capture : expr.lambdaCaptures) {
        std::vector<std::string> tokens = runSemanticsValidatorExprCaptureSplitStep(capture);
        if (tokens.empty()) {
          error_ = "invalid lambda capture";
          return false;
        }
        if (tokens.size() == 1) {
          const std::string &token = tokens[0];
          if (token == "=" || token == "&") {
            if (!captureAllToken.empty()) {
              error_ = "invalid lambda capture";
              return false;
            }
            captureAllToken = token;
            captureAll = true;
            continue;
          }
          if (!explicitNames.insert(token).second) {
            error_ = "duplicate lambda capture: " + token;
            return false;
          }
          captureNames.push_back(token);
          continue;
        }
        if (tokens.size() == 2) {
          const std::string &qualifier = tokens[0];
          const std::string &name = tokens[1];
          if (qualifier != "value" && qualifier != "ref") {
            error_ = "invalid lambda capture";
            return false;
          }
          if (name == "=" || name == "&") {
            error_ = "invalid lambda capture";
            return false;
          }
          if (!explicitNames.insert(name).second) {
            error_ = "duplicate lambda capture: " + name;
            return false;
          }
          captureNames.push_back(name);
          continue;
        }
        error_ = "invalid lambda capture";
        return false;
      }
      if (captureAll) {
        for (const auto &param : params) {
          if (!addCapturedBinding(lambdaLocals, param.name)) {
            return false;
          }
        }
        for (const auto &entry : locals) {
          if (!addCapturedBinding(lambdaLocals, entry.first)) {
            return false;
          }
        }
        for (const auto &name : captureNames) {
          if (findParamBinding(params, name) || locals.count(name) > 0) {
            continue;
          }
          error_ = "unknown capture: " + name;
          return false;
        }
      } else {
        for (const auto &name : captureNames) {
          if (!addCapturedBinding(lambdaLocals, name)) {
            return false;
          }
        }
      }
    }

    MovedScope movedScope(*this, {});
    BorrowEndScope borrowScope(*this, {});
    std::unordered_set<std::string> seen;
    std::vector<ParameterInfo> lambdaParams;
    auto defaultResolvesToDefinition = [&](const Expr &candidate) -> bool {
      return candidate.kind == Expr::Kind::Call && defMap_.find(resolveCalleePath(candidate)) != defMap_.end();
    };
    lambdaParams.reserve(expr.args.size());
    for (const auto &param : expr.args) {
      if (!param.isBinding) {
        error_ = "lambda parameters must use binding syntax";
        return false;
      }
      if (param.hasBodyArguments || !param.bodyArguments.empty()) {
        error_ = "lambda parameter does not accept block arguments: " + param.name;
        return false;
      }
      if (!seen.insert(param.name).second) {
        error_ = "duplicate parameter: " + param.name;
        return false;
      }
      if (lambdaLocals.count(param.name) > 0) {
        error_ = "duplicate binding name: " + param.name;
        return false;
      }
      BindingInfo binding;
      std::optional<std::string> restrictType;
      if (!parseBindingInfo(param, expr.namespacePrefix, structNames_, importAliases_, binding, restrictType, error_)) {
        return false;
      }
      if (param.args.size() > 1) {
        error_ = "lambda parameter defaults accept at most one argument: " + param.name;
        return false;
      }
      if (param.args.size() == 1 && !isDefaultExprAllowed(param.args.front(), defaultResolvesToDefinition)) {
        if (param.args.front().kind == Expr::Kind::Call && hasNamedArguments(param.args.front().argNames)) {
          error_ = "lambda parameter default does not accept named arguments: " + param.name;
        } else {
          error_ = "lambda parameter default must be a literal or pure expression: " + param.name;
        }
        return false;
      }
      if (!hasExplicitBindingTypeTransform(param) && param.args.size() == 1) {
        (void)tryInferBindingTypeFromInitializer(param.args.front(), {}, {}, binding, hasAnyMathImport());
      }
      ParameterInfo info;
      info.name = param.name;
      info.binding = std::move(binding);
      if (param.args.size() == 1) {
        info.defaultExpr = &param.args.front();
      }
      if (restrictType.has_value()) {
        const bool hasTemplate = !info.binding.typeTemplateArg.empty();
        if (!restrictMatchesBinding(*restrictType,
                                    info.binding.typeName,
                                    info.binding.typeTemplateArg,
                                    hasTemplate,
                                    expr.namespacePrefix)) {
          error_ = "restrict type does not match binding type";
          return false;
        }
      }
      lambdaLocals.emplace(info.name, info.binding);
      lambdaParams.push_back(std::move(info));
    }

    std::vector<Expr> lambdaLivenessStatements = expr.bodyArguments;
    if (enclosingStatements != nullptr && statementIndex < enclosingStatements->size()) {
      for (size_t idx = statementIndex + 1; idx < enclosingStatements->size(); ++idx) {
        lambdaLivenessStatements.push_back((*enclosingStatements)[idx]);
      }
    }
    bool sawReturn = false;
    for (size_t stmtIndex = 0; stmtIndex < expr.bodyArguments.size(); ++stmtIndex) {
      const Expr &stmt = expr.bodyArguments[stmtIndex];
      if (!validateStatement(lambdaParams,
                             lambdaLocals,
                             stmt,
                             ReturnKind::Unknown,
                             true,
                             true,
                             &sawReturn,
                             expr.namespacePrefix,
                             &expr.bodyArguments,
                             stmtIndex)) {
        return false;
      }
      expireReferenceBorrowsForRemainder(lambdaParams, lambdaLocals, lambdaLivenessStatements, stmtIndex + 1);
    }
    return true;
  }
  if (!allowEntryArgStringUse_) {
    if (isEntryArgsAccess(expr) || isEntryArgStringBinding(locals, expr)) {
      error_ = "entry argument strings are only supported in print calls or string bindings";
      return false;
    }
  }
  std::optional<EffectScope> effectScope;
  if (expr.kind == Expr::Kind::Call && !expr.isBinding && !expr.transforms.empty()) {
    std::unordered_set<std::string> executionEffects;
    if (!resolveExecutionEffects(expr, executionEffects)) {
      return false;
    }
    effectScope.emplace(*this, std::move(executionEffects));
  }
  auto isMutableBinding = [&](const std::vector<ParameterInfo> &paramsIn,
                              const std::unordered_map<std::string, BindingInfo> &localsIn,
                              const std::string &name) -> bool {
    if (const BindingInfo *paramBinding = findParamBinding(paramsIn, name)) {
      return paramBinding->isMutable;
    }
    auto it = localsIn.find(name);
    return it != localsIn.end() && it->second.isMutable;
  };
  auto returnKindForBinding = [&](const BindingInfo &binding) -> ReturnKind {
    if (binding.typeName == "Reference") {
      std::string base;
      std::string arg;
      if (splitTemplateTypeName(binding.typeTemplateArg, base, arg) && base == "array") {
        std::vector<std::string> args;
        if (splitTopLevelTemplateArgs(arg, args) && args.size() == 1) {
          return ReturnKind::Array;
        }
      }
      return returnKindForTypeName(binding.typeTemplateArg);
    }
    return returnKindForTypeName(binding.typeName);
  };
  auto isIntegerOrBoolExpr = [&](const Expr &arg,
                                 const std::vector<ParameterInfo> &paramsIn,
                                 const std::unordered_map<std::string, BindingInfo> &localsIn) -> bool {
    ReturnKind kind = inferExprReturnKind(arg, paramsIn, localsIn);
    if (kind == ReturnKind::Int || kind == ReturnKind::Int64 || kind == ReturnKind::UInt64 || kind == ReturnKind::Bool) {
      return true;
    }
    if (kind == ReturnKind::Float32 || kind == ReturnKind::Float64 || kind == ReturnKind::Void ||
        kind == ReturnKind::Array) {
      return false;
    }
    if (kind == ReturnKind::Unknown) {
      if (arg.kind == Expr::Kind::FloatLiteral || arg.kind == Expr::Kind::StringLiteral) {
        return false;
      }
      if (isPointerExpr(arg, paramsIn, localsIn)) {
        return false;
      }
      if (arg.kind == Expr::Kind::Name) {
        if (const BindingInfo *paramBinding = findParamBinding(paramsIn, arg.name)) {
          ReturnKind paramKind = returnKindForBinding(*paramBinding);
          return paramKind == ReturnKind::Int || paramKind == ReturnKind::Int64 || paramKind == ReturnKind::UInt64 ||
                 paramKind == ReturnKind::Bool;
        }
        auto it = localsIn.find(arg.name);
        if (it != localsIn.end()) {
          ReturnKind localKind = returnKindForBinding(it->second);
          return localKind == ReturnKind::Int || localKind == ReturnKind::Int64 || localKind == ReturnKind::UInt64 ||
                 localKind == ReturnKind::Bool;
        }
      }
      return true;
    }
    return false;
  };
  auto isBoolExpr = [&](const Expr &arg,
                        const std::vector<ParameterInfo> &paramsIn,
                        const std::unordered_map<std::string, BindingInfo> &localsIn) -> bool {
    ReturnKind kind = inferExprReturnKind(arg, paramsIn, localsIn);
    if (kind == ReturnKind::Bool) {
      return true;
    }
    if (kind == ReturnKind::Void || kind == ReturnKind::Array || kind == ReturnKind::Int || kind == ReturnKind::Int64 ||
        kind == ReturnKind::UInt64 || kind == ReturnKind::Float32 || kind == ReturnKind::Float64) {
      return false;
    }
    if (kind == ReturnKind::Unknown) {
      if (arg.kind == Expr::Kind::FloatLiteral || arg.kind == Expr::Kind::StringLiteral) {
        return false;
      }
      if (isPointerExpr(arg, paramsIn, localsIn)) {
        return false;
      }
      if (arg.kind == Expr::Kind::Name) {
        if (const BindingInfo *paramBinding = findParamBinding(paramsIn, arg.name)) {
          ReturnKind paramKind = returnKindForBinding(*paramBinding);
          return paramKind == ReturnKind::Bool;
        }
        auto it = localsIn.find(arg.name);
        if (it != localsIn.end()) {
          ReturnKind localKind = returnKindForBinding(it->second);
          return localKind == ReturnKind::Bool;
        }
      }
      return true;
    }
    return false;
  };
  auto isNumericExpr = [&](const Expr &arg) -> bool {
    ReturnKind kind = inferExprReturnKind(arg, params, locals);
    const bool isSoftware =
        kind == ReturnKind::Integer || kind == ReturnKind::Decimal || kind == ReturnKind::Complex;
    if (kind == ReturnKind::Int || kind == ReturnKind::Int64 || kind == ReturnKind::UInt64 ||
        kind == ReturnKind::Float32 || kind == ReturnKind::Float64 || isSoftware) {
      return true;
    }
    if (kind == ReturnKind::Bool || kind == ReturnKind::Void || kind == ReturnKind::Array) {
      return false;
    }
    if (kind == ReturnKind::Unknown) {
      if (arg.kind == Expr::Kind::StringLiteral) {
        return false;
      }
      if (isPointerExpr(arg, params, locals)) {
        return false;
      }
      if (arg.kind == Expr::Kind::Name) {
        if (const BindingInfo *paramBinding = findParamBinding(params, arg.name)) {
          ReturnKind paramKind = returnKindForBinding(*paramBinding);
          return paramKind == ReturnKind::Int || paramKind == ReturnKind::Int64 || paramKind == ReturnKind::UInt64 ||
                 paramKind == ReturnKind::Float32 || paramKind == ReturnKind::Float64 ||
                 paramKind == ReturnKind::Integer || paramKind == ReturnKind::Decimal || paramKind == ReturnKind::Complex;
        }
        auto it = locals.find(arg.name);
        if (it != locals.end()) {
          ReturnKind localKind = returnKindForBinding(it->second);
          return localKind == ReturnKind::Int || localKind == ReturnKind::Int64 || localKind == ReturnKind::UInt64 ||
                 localKind == ReturnKind::Float32 || localKind == ReturnKind::Float64 ||
                 localKind == ReturnKind::Integer || localKind == ReturnKind::Decimal || localKind == ReturnKind::Complex;
        }
      }
      return true;
    }
    return false;
  };
  auto isFloatExpr = [&](const Expr &arg) -> bool {
    ReturnKind kind = inferExprReturnKind(arg, params, locals);
    if (kind == ReturnKind::Float32 || kind == ReturnKind::Float64 || kind == ReturnKind::Decimal) {
      return true;
    }
    if (kind == ReturnKind::Bool || kind == ReturnKind::Void || kind == ReturnKind::Array || kind == ReturnKind::Int ||
        kind == ReturnKind::Int64 || kind == ReturnKind::UInt64 || kind == ReturnKind::Integer ||
        kind == ReturnKind::Complex) {
      return false;
    }
    if (kind == ReturnKind::Unknown) {
      if (arg.kind == Expr::Kind::FloatLiteral) {
        return true;
      }
      if (arg.kind == Expr::Kind::StringLiteral) {
        return false;
      }
      if (isPointerExpr(arg, params, locals)) {
        return false;
      }
      if (arg.kind == Expr::Kind::Name) {
        if (const BindingInfo *paramBinding = findParamBinding(params, arg.name)) {
          ReturnKind paramKind = returnKindForBinding(*paramBinding);
          return paramKind == ReturnKind::Float32 || paramKind == ReturnKind::Float64 || paramKind == ReturnKind::Decimal;
        }
        auto it = locals.find(arg.name);
        if (it != locals.end()) {
          ReturnKind localKind = returnKindForBinding(it->second);
          return localKind == ReturnKind::Float32 || localKind == ReturnKind::Float64 || localKind == ReturnKind::Decimal;
        }
      }
      return true;
    }
    return false;
  };
  enum class NumericDomain { Unknown, Fixed, Software };
  enum class NumericCategory { Unknown, Integer, Float, Complex };
  struct NumericInfo {
    NumericDomain domain = NumericDomain::Unknown;
    NumericCategory category = NumericCategory::Unknown;
  };
  auto numericInfoForKind = [&](ReturnKind kind) -> NumericInfo {
    switch (kind) {
      case ReturnKind::Int:
      case ReturnKind::Int64:
      case ReturnKind::UInt64:
        return {NumericDomain::Fixed, NumericCategory::Integer};
      case ReturnKind::Float32:
      case ReturnKind::Float64:
        return {NumericDomain::Fixed, NumericCategory::Float};
      case ReturnKind::Integer:
        return {NumericDomain::Software, NumericCategory::Integer};
      case ReturnKind::Decimal:
        return {NumericDomain::Software, NumericCategory::Float};
      case ReturnKind::Complex:
        return {NumericDomain::Software, NumericCategory::Complex};
      case ReturnKind::Bool:
        return {NumericDomain::Fixed, NumericCategory::Integer};
      default:
        return {NumericDomain::Unknown, NumericCategory::Unknown};
    }
  };
  auto numericInfoForTypeName = [&](const std::string &typeName) -> NumericInfo {
    if (typeName == "int" || typeName == "i32" || typeName == "i64" || typeName == "u64" || typeName == "bool") {
      return {NumericDomain::Fixed, NumericCategory::Integer};
    }
    if (typeName == "float" || typeName == "f32" || typeName == "f64") {
      return {NumericDomain::Fixed, NumericCategory::Float};
    }
    if (typeName == "integer") {
      return {NumericDomain::Software, NumericCategory::Integer};
    }
    if (typeName == "decimal") {
      return {NumericDomain::Software, NumericCategory::Float};
    }
    if (typeName == "complex") {
      return {NumericDomain::Software, NumericCategory::Complex};
    }
    return {NumericDomain::Unknown, NumericCategory::Unknown};
  };
  auto numericCategoryForExpr = [&](const Expr &arg) -> NumericCategory {
    ReturnKind kind = inferExprReturnKind(arg, params, locals);
    NumericInfo info = numericInfoForKind(kind);
    if (info.category != NumericCategory::Unknown) {
      return info.category;
    }
    if (kind == ReturnKind::Unknown) {
      if (arg.kind == Expr::Kind::FloatLiteral) {
        return NumericCategory::Float;
      }
      if (arg.kind == Expr::Kind::StringLiteral) {
        return NumericCategory::Unknown;
      }
      if (isPointerExpr(arg, params, locals)) {
        return NumericCategory::Unknown;
      }
      if (arg.kind == Expr::Kind::Name) {
        if (const BindingInfo *paramBinding = findParamBinding(params, arg.name)) {
          const std::string &typeName = paramBinding->typeName;
          NumericInfo typeInfo = numericInfoForTypeName(typeName);
          if (typeInfo.category != NumericCategory::Unknown) {
            return typeInfo.category;
          }
          if (typeName == "Reference") {
            const std::string &refType = paramBinding->typeTemplateArg;
            NumericInfo refInfo = numericInfoForTypeName(refType);
            if (refInfo.category != NumericCategory::Unknown) {
              return refInfo.category;
            }
          }
        }
        auto it = locals.find(arg.name);
        if (it != locals.end()) {
          const std::string &typeName = it->second.typeName;
          NumericInfo typeInfo = numericInfoForTypeName(typeName);
          if (typeInfo.category != NumericCategory::Unknown) {
            return typeInfo.category;
          }
          if (typeName == "Reference") {
            const std::string &refType = it->second.typeTemplateArg;
            NumericInfo refInfo = numericInfoForTypeName(refType);
            if (refInfo.category != NumericCategory::Unknown) {
              return refInfo.category;
            }
          }
        }
      }
      return NumericCategory::Unknown;
    }
    return NumericCategory::Unknown;
  };
  auto numericDomainForExpr = [&](const Expr &arg) -> NumericDomain {
    ReturnKind kind = inferExprReturnKind(arg, params, locals);
    NumericInfo info = numericInfoForKind(kind);
    if (info.domain != NumericDomain::Unknown) {
      return info.domain;
    }
    if (kind == ReturnKind::Unknown) {
      if (arg.kind == Expr::Kind::StringLiteral) {
        return NumericDomain::Unknown;
      }
      if (isPointerExpr(arg, params, locals)) {
        return NumericDomain::Unknown;
      }
      if (arg.kind == Expr::Kind::Name) {
        if (const BindingInfo *paramBinding = findParamBinding(params, arg.name)) {
          NumericInfo typeInfo = numericInfoForTypeName(paramBinding->typeName);
          if (typeInfo.domain != NumericDomain::Unknown) {
            return typeInfo.domain;
          }
          if (paramBinding->typeName == "Reference") {
            NumericInfo refInfo = numericInfoForTypeName(paramBinding->typeTemplateArg);
            if (refInfo.domain != NumericDomain::Unknown) {
              return refInfo.domain;
            }
          }
        }
        auto it = locals.find(arg.name);
        if (it != locals.end()) {
          NumericInfo typeInfo = numericInfoForTypeName(it->second.typeName);
          if (typeInfo.domain != NumericDomain::Unknown) {
            return typeInfo.domain;
          }
          if (it->second.typeName == "Reference") {
            NumericInfo refInfo = numericInfoForTypeName(it->second.typeTemplateArg);
            if (refInfo.domain != NumericDomain::Unknown) {
              return refInfo.domain;
            }
          }
        }
      }
    }
    return NumericDomain::Unknown;
  };
  auto hasMixedNumericDomain = [&](const std::vector<Expr> &args) -> bool {
    bool sawFixed = false;
    bool sawSoftware = false;
    for (const auto &arg : args) {
      NumericDomain domain = numericDomainForExpr(arg);
      if (domain == NumericDomain::Fixed) {
        sawFixed = true;
      } else if (domain == NumericDomain::Software) {
        sawSoftware = true;
      }
    }
    return sawFixed && sawSoftware;
  };
  auto hasMixedNumericCategory = [&](const std::vector<Expr> &args) -> bool {
    bool sawFloat = false;
    bool sawInteger = false;
    bool sawComplex = false;
    for (const auto &arg : args) {
      NumericCategory category = numericCategoryForExpr(arg);
      if (category == NumericCategory::Float) {
        sawFloat = true;
      } else if (category == NumericCategory::Integer) {
        sawInteger = true;
      } else if (category == NumericCategory::Complex) {
        sawComplex = true;
      }
    }
    if (sawComplex && (sawFloat || sawInteger)) {
      return true;
    }
    return sawFloat && sawInteger;
  };
  auto hasComplexNumeric = [&](const std::vector<Expr> &args) -> bool {
    for (const auto &arg : args) {
      if (numericCategoryForExpr(arg) == NumericCategory::Complex) {
        return true;
      }
    }
    return false;
  };
  auto hasMixedComplexNumeric = [&](const std::vector<Expr> &args) -> bool {
    bool sawComplex = false;
    bool sawOther = false;
    for (const auto &arg : args) {
      NumericCategory category = numericCategoryForExpr(arg);
      if (category == NumericCategory::Complex) {
        sawComplex = true;
      } else if (category == NumericCategory::Integer || category == NumericCategory::Float) {
        sawOther = true;
      }
    }
    return sawComplex && sawOther;
  };
  auto isComparisonOperand = [&](const Expr &arg) -> bool {
    ReturnKind kind = inferExprReturnKind(arg, params, locals);
    if (kind == ReturnKind::Bool || kind == ReturnKind::Int || kind == ReturnKind::Int64 || kind == ReturnKind::UInt64 ||
        kind == ReturnKind::Float32 || kind == ReturnKind::Float64 || kind == ReturnKind::Integer ||
        kind == ReturnKind::Decimal || kind == ReturnKind::Complex) {
      return true;
    }
    if (kind == ReturnKind::Void || kind == ReturnKind::Array) {
      return false;
    }
    if (kind == ReturnKind::Unknown) {
      if (arg.kind == Expr::Kind::StringLiteral) {
        return false;
      }
      if (isPointerExpr(arg, params, locals)) {
        return false;
      }
      if (arg.kind == Expr::Kind::Name) {
        if (const BindingInfo *paramBinding = findParamBinding(params, arg.name)) {
          ReturnKind paramKind = returnKindForBinding(*paramBinding);
          return paramKind == ReturnKind::Bool || paramKind == ReturnKind::Int || paramKind == ReturnKind::Int64 ||
                 paramKind == ReturnKind::UInt64 || paramKind == ReturnKind::Float32 || paramKind == ReturnKind::Float64 ||
                 paramKind == ReturnKind::Integer || paramKind == ReturnKind::Decimal || paramKind == ReturnKind::Complex;
        }
        auto it = locals.find(arg.name);
        if (it != locals.end()) {
          ReturnKind localKind = returnKindForBinding(it->second);
          return localKind == ReturnKind::Bool || localKind == ReturnKind::Int || localKind == ReturnKind::Int64 ||
                 localKind == ReturnKind::UInt64 || localKind == ReturnKind::Float32 || localKind == ReturnKind::Float64 ||
                 localKind == ReturnKind::Integer || localKind == ReturnKind::Decimal || localKind == ReturnKind::Complex;
        }
      }
      return true;
    }
    return false;
  };
  auto isUnsignedExpr = [&](const Expr &arg) -> bool {
    ReturnKind kind = inferExprReturnKind(arg, params, locals);
    return kind == ReturnKind::UInt64;
  };
  auto hasMixedSignedness = [&](const std::vector<Expr> &args, bool boolCountsAsSigned) -> bool {
    bool sawUnsigned = false;
    bool sawSigned = false;
    for (const auto &arg : args) {
      ReturnKind kind = inferExprReturnKind(arg, params, locals);
      if (kind == ReturnKind::Integer || kind == ReturnKind::Decimal || kind == ReturnKind::Complex) {
        continue;
      }
      if (kind == ReturnKind::UInt64) {
        sawUnsigned = true;
        continue;
      }
      if (kind == ReturnKind::Int || kind == ReturnKind::Int64 || (boolCountsAsSigned && kind == ReturnKind::Bool)) {
        sawSigned = true;
      }
    }
    return sawUnsigned && sawSigned;
  };
  auto isStructConstructor = [&](const Expr &candidate) -> bool {
    if (candidate.kind != Expr::Kind::Call || candidate.isBinding) {
      return false;
    }
    const std::string resolved = resolveCalleePath(candidate);
    return structNames_.count(resolved) > 0;
  };
  auto isIfBlockEnvelope = [&](const Expr &candidate) -> bool {
    if (candidate.kind != Expr::Kind::Call || candidate.isBinding || candidate.isMethodCall) {
      return false;
    }
    if (!candidate.args.empty() || !candidate.templateArgs.empty() || hasNamedArguments(candidate.argNames)) {
      return false;
    }
    if (!candidate.hasBodyArguments && candidate.bodyArguments.empty()) {
      return false;
    }
    return true;
  };
  auto getEnvelopeValueExpr = [&](const Expr &candidate) -> const Expr * {
    if (!isIfBlockEnvelope(candidate)) {
      return nullptr;
    }
    const Expr *valueExpr = nullptr;
    for (size_t i = 0; i < candidate.bodyArguments.size(); ++i) {
      const Expr &bodyExpr = candidate.bodyArguments[i];
      if (bodyExpr.isBinding) {
        continue;
      }
      valueExpr = &bodyExpr;
    }
    return valueExpr;
  };
  std::function<bool(const Expr &)> isStructConstructorValueExpr;
  isStructConstructorValueExpr = [&](const Expr &candidate) -> bool {
    if (isStructConstructor(candidate)) {
      return true;
    }
    if (isMatchCall(candidate)) {
      Expr expanded;
      std::string error;
      if (!lowerMatchToIf(candidate, expanded, error)) {
        return false;
      }
      return isStructConstructorValueExpr(expanded);
    }
    if (isIfCall(candidate) && candidate.args.size() == 3) {
      const Expr &thenArg = candidate.args[1];
      const Expr &elseArg = candidate.args[2];
      const Expr *thenValue = getEnvelopeValueExpr(thenArg);
      const Expr *elseValue = getEnvelopeValueExpr(elseArg);
      const Expr &thenExpr = thenValue ? *thenValue : thenArg;
      const Expr &elseExpr = elseValue ? *elseValue : elseArg;
      return isStructConstructorValueExpr(thenExpr) && isStructConstructorValueExpr(elseExpr);
    }
    if (const Expr *valueExpr = getEnvelopeValueExpr(candidate)) {
      return isStructConstructorValueExpr(*valueExpr);
    }
    return false;
  };

  if (expr.kind == Expr::Kind::Literal) {
    return true;
  }
  if (expr.kind == Expr::Kind::BoolLiteral) {
    return true;
  }
  if (expr.kind == Expr::Kind::FloatLiteral) {
    return true;
  }
  if (expr.kind == Expr::Kind::StringLiteral) {
    ParsedStringLiteral parsed;
    if (!parseStringLiteralToken(expr.stringValue, parsed, error_)) {
      return false;
    }
    if (parsed.encoding == StringEncoding::Ascii && !isAsciiText(parsed.decoded)) {
      error_ = "ascii string literal contains non-ASCII characters";
      return false;
    }
    return true;
  }
  if (expr.kind == Expr::Kind::Name) {
    if (isParam(params, expr.name) || locals.count(expr.name) > 0) {
      if (movedBindings_.count(expr.name) > 0) {
        error_ = "use-after-move: " + expr.name;
        return false;
      }
      return true;
    }
    if (!allowMathBareName(expr.name) && expr.name.find('/') == std::string::npos &&
        isBuiltinMathConstant(expr.name, true)) {
      error_ = "math constant requires import /std/math/* or /std/math/<name>: " + expr.name;
      return false;
    }
    if (isBuiltinMathConstant(expr.name, allowMathBareName(expr.name))) {
      return true;
    }
    error_ = "unknown identifier: " + expr.name;
    return false;
  }
  if (expr.kind == Expr::Kind::Call) {
    if (expr.isBinding) {
      error_ = "binding not allowed in expression context";
      return false;
    }
    std::optional<EffectScope> effectScope;
    if (!expr.transforms.empty()) {
      std::unordered_set<std::string> executionEffects;
      if (!resolveExecutionEffects(expr, executionEffects)) {
        return false;
      }
      effectScope.emplace(*this, std::move(executionEffects));
    }
    if (isMatchCall(expr)) {
      Expr expanded;
      if (!lowerMatchToIf(expr, expanded, error_)) {
        return false;
      }
      return validateExpr(params, locals, expanded);
    }
    if (isIfCall(expr)) {
      if (hasNamedArguments(expr.argNames)) {
        error_ = "named arguments not supported for builtin calls";
        return false;
      }
      if (expr.hasBodyArguments || !expr.bodyArguments.empty()) {
        error_ = "if does not accept trailing block arguments";
        return false;
      }
      if (expr.args.size() != 3) {
        error_ = "if requires condition, then, else";
        return false;
      }
      const Expr &cond = expr.args[0];
      const Expr &thenArg = expr.args[1];
      const Expr &elseArg = expr.args[2];
      if (!validateExpr(params, locals, cond)) {
        return false;
      }
      ReturnKind condKind = inferExprReturnKind(cond, params, locals);
      if (condKind != ReturnKind::Bool) {
        error_ = "if condition requires bool";
        return false;
      }
      auto isStringValue = [&](const Expr &valueExpr,
                               const std::unordered_map<std::string, BindingInfo> &localsIn) -> bool {
        if (valueExpr.kind == Expr::Kind::StringLiteral) {
          return true;
        }
        if (valueExpr.kind == Expr::Kind::Name) {
          if (const BindingInfo *paramBinding = findParamBinding(params, valueExpr.name)) {
            return paramBinding->typeName == "string";
          }
          auto it = localsIn.find(valueExpr.name);
          return it != localsIn.end() && it->second.typeName == "string";
        }
        if (valueExpr.kind != Expr::Kind::Call) {
          return false;
        }
        std::string accessName;
        if (defMap_.find(resolveCalleePath(valueExpr)) != defMap_.end() ||
            !getBuiltinArrayAccessName(valueExpr, accessName) || valueExpr.args.size() != 2) {
          return false;
        }
        const Expr &target = valueExpr.args.front();
        auto isStringCollectionTarget = [&](const Expr &collectionExpr) -> bool {
          if (collectionExpr.kind == Expr::Kind::StringLiteral) {
            return true;
          }
          if (collectionExpr.kind == Expr::Kind::Name) {
            const BindingInfo *binding = findParamBinding(params, collectionExpr.name);
            if (!binding) {
              auto it = localsIn.find(collectionExpr.name);
              if (it != localsIn.end()) {
                binding = &it->second;
              }
            }
            if (!binding) {
              return false;
            }
            if (binding->typeName == "string") {
              return true;
            }
            if (binding->typeName == "array" || binding->typeName == "vector") {
              return normalizeBindingTypeName(binding->typeTemplateArg) == "string";
            }
            if (binding->typeName == "map") {
              std::vector<std::string> parts;
              if (!splitTopLevelTemplateArgs(binding->typeTemplateArg, parts) || parts.size() != 2) {
                return false;
              }
              return normalizeBindingTypeName(parts[1]) == "string";
            }
            return false;
          }
          if (collectionExpr.kind == Expr::Kind::Call) {
            std::string collection;
            if (defMap_.find(resolveCalleePath(collectionExpr)) != defMap_.end()) {
              return false;
            }
            if (!getBuiltinCollectionName(collectionExpr, collection)) {
              return false;
            }
            if ((collection == "array" || collection == "vector") && collectionExpr.templateArgs.size() == 1) {
              return normalizeBindingTypeName(collectionExpr.templateArgs.front()) == "string";
            }
            if (collection == "map" && collectionExpr.templateArgs.size() == 2) {
              return normalizeBindingTypeName(collectionExpr.templateArgs[1]) == "string";
            }
          }
          return false;
        };
        return isStringCollectionTarget(target);
      };
      auto validateBranchValueKind =
          [&](const Expr &branch, const char *label, ReturnKind &kindOut, bool &stringOut) -> bool {
        kindOut = ReturnKind::Unknown;
        stringOut = false;
        if (isIfBlockEnvelope(branch)) {
          if (branch.bodyArguments.empty()) {
            error_ = std::string(label) + " block must produce a value";
            return false;
          }
          std::unordered_map<std::string, BindingInfo> branchLocals = locals;
          const Expr *valueExpr = nullptr;
          bool sawReturn = false;
          for (const auto &bodyExpr : branch.bodyArguments) {
            if (bodyExpr.isBinding) {
              if (isParam(params, bodyExpr.name) || branchLocals.count(bodyExpr.name) > 0) {
                error_ = "duplicate binding name: " + bodyExpr.name;
                return false;
              }
              BindingInfo info;
              std::optional<std::string> restrictType;
              if (!parseBindingInfo(bodyExpr, bodyExpr.namespacePrefix, structNames_, importAliases_, info, restrictType, error_)) {
                return false;
              }
              if (bodyExpr.args.empty()) {
                if (!validateOmittedBindingInitializer(bodyExpr, info, bodyExpr.namespacePrefix)) {
                  return false;
                }
              } else {
                if (bodyExpr.args.size() != 1) {
                  error_ = "binding requires exactly one argument";
                  return false;
                }
                if (!validateExpr(params, branchLocals, bodyExpr.args.front())) {
                  return false;
                }
                ReturnKind initKind = inferExprReturnKind(bodyExpr.args.front(), params, branchLocals);
                if (initKind == ReturnKind::Void &&
                    !isStructConstructorValueExpr(bodyExpr.args.front())) {
                  error_ = "binding initializer requires a value";
                  return false;
                }
                if (!hasExplicitBindingTypeTransform(bodyExpr)) {
                  (void)inferBindingTypeFromInitializer(bodyExpr.args.front(), params, branchLocals, info);
                }
              }
              if (restrictType.has_value()) {
                const bool hasTemplate = !info.typeTemplateArg.empty();
                if (!restrictMatchesBinding(*restrictType,
                                            info.typeName,
                                            info.typeTemplateArg,
                                            hasTemplate,
                                            bodyExpr.namespacePrefix)) {
                  error_ = "restrict type does not match binding type";
                  return false;
                }
              }
              if (info.typeName == "Reference") {
                if (!bodyExpr.args.empty()) {
                  const Expr &init = bodyExpr.args.front();
                  std::string pointerName;
                  if (init.kind != Expr::Kind::Call || !getBuiltinPointerName(init, pointerName) ||
                      pointerName != "location" || init.args.size() != 1) {
                    error_ = "Reference bindings require location(...)";
                    return false;
                  }
                }
              }
              branchLocals.emplace(bodyExpr.name, info);
              continue;
            }
            if (isReturnCall(bodyExpr)) {
              if (bodyExpr.args.size() != 1) {
                error_ = std::string("return requires a value in ") + label + " block";
                return false;
              }
              if (!validateExpr(params, branchLocals, bodyExpr.args.front())) {
                return false;
              }
              valueExpr = &bodyExpr.args.front();
              sawReturn = true;
              continue;
            }
            if (!validateExpr(params, branchLocals, bodyExpr)) {
              return false;
            }
            if (!sawReturn) {
              valueExpr = &bodyExpr;
            }
          }
          if (!valueExpr) {
            error_ = std::string(label) + " block must end with an expression";
            return false;
          }
          kindOut = inferExprReturnKind(*valueExpr, params, branchLocals);
          stringOut = isStringValue(*valueExpr, branchLocals);
          if (kindOut == ReturnKind::Void) {
            if (isStructConstructorValueExpr(*valueExpr)) {
              kindOut = ReturnKind::Unknown;
            } else {
              error_ = "if branches must produce a value";
              return false;
            }
          }
          return true;
        }
        error_ = "if branches require block envelopes";
        return false;
      };

      ReturnKind thenKind = ReturnKind::Unknown;
      ReturnKind elseKind = ReturnKind::Unknown;
      bool thenIsString = false;
      bool elseIsString = false;
      if (!validateBranchValueKind(thenArg, "then", thenKind, thenIsString)) {
        return false;
      }
      if (!validateBranchValueKind(elseArg, "else", elseKind, elseIsString)) {
        return false;
      }
      if (thenIsString != elseIsString) {
        error_ = "if branches must return compatible types";
        return false;
      }

      ReturnKind combined = inferExprReturnKind(expr, params, locals);
      if (thenKind != ReturnKind::Unknown && elseKind != ReturnKind::Unknown && combined == ReturnKind::Unknown) {
        error_ = "if branches must return compatible types";
        return false;
      }
      return true;
    }
    if (!expr.isMethodCall && isSimpleCallName(expr, "uninitialized")) {
      if (hasNamedArguments(expr.argNames)) {
        error_ = "named arguments not supported for builtin calls";
        return false;
      }
      if (expr.hasBodyArguments || !expr.bodyArguments.empty()) {
        error_ = "uninitialized does not accept block arguments";
        return false;
      }
      if (!expr.args.empty()) {
        error_ = "uninitialized does not accept arguments";
        return false;
      }
      if (expr.templateArgs.size() != 1) {
        error_ = "uninitialized requires exactly one template argument";
        return false;
      }
      return true;
    }
    if (!expr.isMethodCall && (isSimpleCallName(expr, "init") ||
                               isSimpleCallName(expr, "drop") ||
                               isSimpleCallName(expr, "take") ||
                               isSimpleCallName(expr, "borrow"))) {
      const std::string name = expr.name;
      auto isUninitializedStorage = [&](const Expr &arg) -> bool {
        BindingInfo binding;
        bool resolved = false;
        if (!resolveUninitializedStorageBinding(params, locals, arg, binding, resolved)) {
          return false;
        }
        if (!resolved || binding.typeName != "uninitialized" || binding.typeTemplateArg.empty()) {
          return false;
        }
        return true;
      };
      const bool treatAsUninitializedHelper =
          (name != "take") || (!expr.args.empty() && isUninitializedStorage(expr.args.front()));
      if (treatAsUninitializedHelper) {
        if (hasNamedArguments(expr.argNames)) {
          error_ = "named arguments not supported for builtin calls";
          return false;
        }
        if (!expr.templateArgs.empty()) {
          error_ = name + " does not accept template arguments";
          return false;
        }
        if (expr.hasBodyArguments || !expr.bodyArguments.empty()) {
          error_ = name + " does not accept block arguments";
          return false;
        }
        const size_t expectedArgs = (name == "init") ? 2 : 1;
        if (expr.args.size() != expectedArgs) {
          error_ = name + " requires exactly " + std::to_string(expectedArgs) + " argument" +
                   (expectedArgs == 1 ? "" : "s");
          return false;
        }
        if (name == "init" || name == "drop") {
          error_ = name + " is only supported as a statement";
          return false;
        }
        for (const auto &arg : expr.args) {
          if (!validateExpr(params, locals, arg)) {
            return false;
          }
        }
        if (!isUninitializedStorage(expr.args.front())) {
          error_ = name + " requires uninitialized<T> storage";
          return false;
        }
        return true;
      }
    }
    if (isBuiltinBlockCall(expr) && expr.hasBodyArguments) {
      if (!expr.args.empty() || !expr.templateArgs.empty() || hasNamedArguments(expr.argNames)) {
        error_ = "block expression does not accept arguments";
        return false;
      }
      if (expr.bodyArguments.empty()) {
        error_ = "block expression requires a value";
        return false;
      }
      std::unordered_map<std::string, BindingInfo> blockLocals = locals;
      bool sawReturn = false;
      for (size_t i = 0; i < expr.bodyArguments.size(); ++i) {
        const Expr &bodyExpr = expr.bodyArguments[i];
        const bool isLast = (i + 1 == expr.bodyArguments.size());
        if (bodyExpr.isBinding) {
          if (isLast && !sawReturn) {
            error_ = "block expression must end with an expression";
            return false;
          }
          if (isParam(params, bodyExpr.name) || blockLocals.count(bodyExpr.name) > 0) {
            error_ = "duplicate binding name: " + bodyExpr.name;
            return false;
          }
          BindingInfo info;
          std::optional<std::string> restrictType;
          if (!parseBindingInfo(bodyExpr, bodyExpr.namespacePrefix, structNames_, importAliases_, info, restrictType, error_)) {
            return false;
          }
          if (bodyExpr.args.empty()) {
            if (!validateOmittedBindingInitializer(bodyExpr, info, bodyExpr.namespacePrefix)) {
              return false;
            }
          } else {
            if (bodyExpr.args.size() != 1) {
              error_ = "binding requires exactly one argument";
              return false;
            }
            if (!validateExpr(params, blockLocals, bodyExpr.args.front())) {
              return false;
            }
            ReturnKind initKind = inferExprReturnKind(bodyExpr.args.front(), params, blockLocals);
            if (initKind == ReturnKind::Void &&
                !isStructConstructorValueExpr(bodyExpr.args.front())) {
              error_ = "binding initializer requires a value";
              return false;
            }
            if (!hasExplicitBindingTypeTransform(bodyExpr)) {
              (void)inferBindingTypeFromInitializer(bodyExpr.args.front(), params, blockLocals, info);
            }
          }
          if (restrictType.has_value()) {
            const bool hasTemplate = !info.typeTemplateArg.empty();
            if (!restrictMatchesBinding(*restrictType,
                                        info.typeName,
                                        info.typeTemplateArg,
                                        hasTemplate,
                                        bodyExpr.namespacePrefix)) {
              error_ = "restrict type does not match binding type";
              return false;
            }
          }
          if (info.typeName == "Reference") {
            if (!bodyExpr.args.empty()) {
              const Expr &init = bodyExpr.args.front();
              std::string pointerName;
              if (init.kind != Expr::Kind::Call || !getBuiltinPointerName(init, pointerName) ||
                  pointerName != "location" || init.args.size() != 1) {
                error_ = "Reference bindings require location(...)";
                return false;
              }
            }
          }
          blockLocals.emplace(bodyExpr.name, info);
          continue;
        }
        if (isReturnCall(bodyExpr)) {
          if (bodyExpr.args.size() != 1) {
            error_ = "return requires a value in block expression";
            return false;
          }
          if (!validateExpr(params, blockLocals, bodyExpr.args.front())) {
            return false;
          }
          ReturnKind kind = inferExprReturnKind(bodyExpr.args.front(), params, blockLocals);
          if (kind == ReturnKind::Void) {
            if (!isStructConstructorValueExpr(bodyExpr.args.front())) {
              error_ = "block expression requires a value";
              return false;
            }
          }
          sawReturn = true;
          continue;
        }
        if (!validateExpr(params, blockLocals, bodyExpr)) {
          return false;
        }
        if (isLast && !sawReturn) {
          ReturnKind kind = inferExprReturnKind(bodyExpr, params, blockLocals);
          if (kind == ReturnKind::Void) {
            if (!isStructConstructorValueExpr(bodyExpr)) {
              error_ = "block expression requires a value";
              return false;
            }
          }
        }
      }
      return true;
    }
    if (isBuiltinBlockCall(expr)) {
      error_ = "block requires block arguments";
      return false;
    }
    if (isLoopCall(expr)) {
      error_ = "loop is only supported as a statement";
      return false;
    }
    if (isWhileCall(expr)) {
      error_ = "while is only supported as a statement";
      return false;
    }
    if (isForCall(expr)) {
      error_ = "for is only supported as a statement";
      return false;
    }
    if (isRepeatCall(expr)) {
      error_ = "repeat is only supported as a statement";
      return false;
    }
    if (isSimpleCallName(expr, "dispatch")) {
      error_ = "dispatch is only supported as a statement";
      return false;
    }
    if (isSimpleCallName(expr, "buffer_store")) {
      error_ = "buffer_store is only supported as a statement";
      return false;
    }
    auto getVectorStatementHelperName = [&](const Expr &candidate, std::string &nameOut) -> bool {
      if (candidate.kind != Expr::Kind::Call) {
        return false;
      }
      auto matchesHelper = [&](const char *helper) -> bool {
        if (isSimpleCallName(candidate, helper)) {
          return true;
        }
        if (candidate.name.empty()) {
          return false;
        }
        std::string normalized = candidate.name;
        if (!normalized.empty() && normalized[0] == '/') {
          normalized.erase(0, 1);
        }
        return normalized == std::string("vector/") + helper;
      };
      if (matchesHelper("push")) {
        nameOut = "push";
        return true;
      }
      if (matchesHelper("pop")) {
        nameOut = "pop";
        return true;
      }
      if (matchesHelper("reserve")) {
        nameOut = "reserve";
        return true;
      }
      if (matchesHelper("clear")) {
        nameOut = "clear";
        return true;
      }
      if (matchesHelper("remove_at")) {
        nameOut = "remove_at";
        return true;
      }
      if (matchesHelper("remove_swap")) {
        nameOut = "remove_swap";
        return true;
      }
      return false;
    };
    std::string vectorHelper;
    if (getVectorStatementHelperName(expr, vectorHelper)) {
      std::string resolved = resolveCalleePath(expr);
      if (defMap_.find(resolved) == defMap_.end() && expr.isMethodCall && !expr.args.empty()) {
        const Expr &receiver = expr.args.front();
        if (receiver.kind == Expr::Kind::Name) {
          std::string typeName;
          if (const BindingInfo *paramBinding = findParamBinding(params, receiver.name)) {
            typeName = paramBinding->typeName;
          } else {
            auto it = locals.find(receiver.name);
            if (it != locals.end()) {
              typeName = it->second.typeName;
            }
          }
          if (!typeName.empty() && typeName != "Pointer" && typeName != "Reference") {
            std::string resolvedType;
            if (isPrimitiveBindingTypeName(typeName)) {
              resolvedType = "/" + normalizeBindingTypeName(typeName);
            } else {
              resolvedType = resolveTypePath(typeName, receiver.namespacePrefix);
              if (structNames_.count(resolvedType) == 0 && defMap_.count(resolvedType) == 0) {
                auto importIt = importAliases_.find(typeName);
                if (importIt != importAliases_.end()) {
                  resolvedType = importIt->second;
                }
              }
            }
            const std::string methodTarget = resolvedType + "/" + expr.name;
            if (!resolvedType.empty() && defMap_.count(methodTarget) > 0) {
              resolved = methodTarget;
            }
          }
        }
      }
      if (defMap_.find(resolved) == defMap_.end()) {
        error_ = vectorHelper + " is only supported as a statement";
        return false;
      }
    }
    if (isReturnCall(expr)) {
      error_ = "return not allowed in expression context";
      return false;
    }
    auto normalizeCollectionTypePath = [](const std::string &typePath) -> std::string {
      if (typePath == "/array" || typePath == "array") {
        return "/array";
      }
      if (typePath == "/vector" || typePath == "vector") {
        return "/vector";
      }
      if (typePath == "/map" || typePath == "map") {
        return "/map";
      }
      if (typePath == "/string" || typePath == "string") {
        return "/string";
      }
      return "";
    };
    auto resolveCallCollectionTypePath = [&](const Expr &target, std::string &typePathOut) -> bool {
      typePathOut.clear();
      if (target.kind != Expr::Kind::Call) {
        return false;
      }
      const std::string resolvedTarget = resolveCalleePath(target);
      std::string collection;
      if (defMap_.find(resolvedTarget) == defMap_.end() && getBuiltinCollectionName(target, collection)) {
        if ((collection == "array" || collection == "vector") && target.templateArgs.size() == 1) {
          typePathOut = "/" + collection;
          return true;
        }
        if (collection == "map" && target.templateArgs.size() == 2) {
          typePathOut = "/map";
          return true;
        }
      }
      auto structIt = returnStructs_.find(resolvedTarget);
      if (structIt != returnStructs_.end()) {
        std::string normalized = normalizeCollectionTypePath(structIt->second);
        if (!normalized.empty()) {
          typePathOut = normalized;
          return true;
        }
      }
      auto kindIt = returnKinds_.find(resolvedTarget);
      if (kindIt != returnKinds_.end()) {
        if (kindIt->second == ReturnKind::Array) {
          typePathOut = "/array";
          return true;
        }
        if (kindIt->second == ReturnKind::String) {
          typePathOut = "/string";
          return true;
        }
      }
      return false;
    };
    auto resolveCallCollectionTemplateArgs =
        [&](const Expr &target, const std::string &expectedBase, std::vector<std::string> &argsOut) -> bool {
      argsOut.clear();
      if (target.kind != Expr::Kind::Call) {
        return false;
      }
      const std::string resolvedTarget = resolveCalleePath(target);
      std::string collection;
      if (defMap_.find(resolvedTarget) == defMap_.end() && getBuiltinCollectionName(target, collection) &&
          collection == expectedBase) {
        argsOut = target.templateArgs;
        return true;
      }
      auto defIt = defMap_.find(resolvedTarget);
      if (defIt == defMap_.end() || !defIt->second) {
        return false;
      }
      for (const auto &transform : defIt->second->transforms) {
        if (transform.name != "return" || transform.templateArgs.size() != 1) {
          continue;
        }
        std::string base;
        std::string arg;
        const std::string normalizedReturn = normalizeBindingTypeName(transform.templateArgs.front());
        if (!splitTemplateTypeName(normalizedReturn, base, arg) || base != expectedBase) {
          return false;
        }
        return splitTopLevelTemplateArgs(arg, argsOut);
      }
      return false;
    };
    auto resolveArrayTarget = [&](const Expr &target, std::string &elemType) -> bool {
      if (target.kind == Expr::Kind::Name) {
        auto resolveReference = [&](const BindingInfo &binding) -> bool {
          if (binding.typeName != "Reference" || binding.typeTemplateArg.empty()) {
            return false;
          }
          std::string base;
          std::string arg;
          if (!splitTemplateTypeName(binding.typeTemplateArg, base, arg) || base != "array") {
            return false;
          }
          std::vector<std::string> args;
          if (!splitTopLevelTemplateArgs(arg, args) || args.size() != 1) {
            return false;
          }
          elemType = args.front();
          return true;
        };
        if (const BindingInfo *paramBinding = findParamBinding(params, target.name)) {
          if (resolveReference(*paramBinding)) {
            return true;
          }
          if ((paramBinding->typeName != "array" && paramBinding->typeName != "vector") ||
              paramBinding->typeTemplateArg.empty()) {
            return false;
          }
          elemType = paramBinding->typeTemplateArg;
          return true;
        }
        auto it = locals.find(target.name);
        if (it != locals.end()) {
          if (resolveReference(it->second)) {
            return true;
          }
          if ((it->second.typeName != "array" && it->second.typeName != "vector") ||
              it->second.typeTemplateArg.empty()) {
            return false;
          }
          elemType = it->second.typeTemplateArg;
          return true;
        }
        return false;
      }
      if (target.kind == Expr::Kind::Call) {
        std::string collectionTypePath;
        if (resolveCallCollectionTypePath(target, collectionTypePath) &&
            (collectionTypePath == "/array" || collectionTypePath == "/vector")) {
          std::vector<std::string> args;
          const std::string expectedBase = collectionTypePath == "/vector" ? "vector" : "array";
          if (resolveCallCollectionTemplateArgs(target, expectedBase, args) && args.size() == 1) {
            elemType = args.front();
          }
          return true;
        }
      }
      return false;
    };
    auto resolveVectorTarget = [&](const Expr &target, std::string &elemType) -> bool {
      if (target.kind == Expr::Kind::Name) {
        if (const BindingInfo *paramBinding = findParamBinding(params, target.name)) {
          if (paramBinding->typeName != "vector" || paramBinding->typeTemplateArg.empty()) {
            return false;
          }
          elemType = paramBinding->typeTemplateArg;
          return true;
        }
        auto it = locals.find(target.name);
        if (it != locals.end()) {
          if (it->second.typeName != "vector" || it->second.typeTemplateArg.empty()) {
            return false;
          }
          elemType = it->second.typeTemplateArg;
          return true;
        }
        return false;
      }
      if (target.kind == Expr::Kind::Call) {
        std::string collectionTypePath;
        if (resolveCallCollectionTypePath(target, collectionTypePath) && collectionTypePath == "/vector") {
          std::vector<std::string> args;
          if (resolveCallCollectionTemplateArgs(target, "vector", args) && args.size() == 1) {
            elemType = args.front();
          }
          return true;
        }
      }
      return false;
    };
    auto resolveStringTarget = [&](const Expr &target) -> bool {
      if (target.kind == Expr::Kind::StringLiteral) {
        return true;
      }

      if (target.kind == Expr::Kind::StringLiteral) {
        return true;
      }
      if (target.kind == Expr::Kind::Name) {
        if (const BindingInfo *paramBinding = findParamBinding(params, target.name)) {
          return paramBinding->typeName == "string";
        }
        auto it = locals.find(target.name);
        return it != locals.end() && it->second.typeName == "string";
      }
      if (target.kind == Expr::Kind::Call) {
        std::string collectionTypePath;
        if (resolveCallCollectionTypePath(target, collectionTypePath) && collectionTypePath == "/string") {
          return true;
        }
        std::string builtinName;
        if (defMap_.find(resolveCalleePath(target)) == defMap_.end() && getBuiltinArrayAccessName(target, builtinName) &&
            target.args.size() == 2) {
          std::string elemType;
          if (resolveArrayTarget(target.args.front(), elemType) && elemType == "string") {
            return true;
          }
        }
      }
      return false;
    };
    auto resolveMapTarget = [&](const Expr &target) -> bool {
      if (target.kind == Expr::Kind::Name) {
        if (const BindingInfo *paramBinding = findParamBinding(params, target.name)) {
          return paramBinding->typeName == "map";
        }
        auto it = locals.find(target.name);
        return it != locals.end() && it->second.typeName == "map";
      }
      if (target.kind == Expr::Kind::Call) {
        std::string collectionTypePath;
        if (!resolveCallCollectionTypePath(target, collectionTypePath) || collectionTypePath != "/map") {
          return false;
        }
        return true;
      }
      return false;
    };
    auto resolveMapKeyType = [&](const Expr &target, std::string &keyTypeOut) -> bool {
      keyTypeOut.clear();
      if (target.kind == Expr::Kind::Name) {
        if (const BindingInfo *paramBinding = findParamBinding(params, target.name)) {
          if (paramBinding->typeName != "map") {
            return false;
          }
          std::vector<std::string> parts;
          if (!splitTopLevelTemplateArgs(paramBinding->typeTemplateArg, parts) || parts.size() != 2) {
            return false;
          }
          keyTypeOut = parts[0];
          return true;
        }
        auto it = locals.find(target.name);
        if (it == locals.end() || it->second.typeName != "map") {
          return false;
        }
        std::vector<std::string> parts;
        if (!splitTopLevelTemplateArgs(it->second.typeTemplateArg, parts) || parts.size() != 2) {
          return false;
        }
        keyTypeOut = parts[0];
        return true;
      }
      if (target.kind == Expr::Kind::Call) {
        std::string collectionTypePath;
        if (!resolveCallCollectionTypePath(target, collectionTypePath) || collectionTypePath != "/map") {
          return false;
        }
        std::vector<std::string> args;
        if (resolveCallCollectionTemplateArgs(target, "map", args) && args.size() == 2) {
          keyTypeOut = args.front();
        }
        return true;
      }
      return false;
    };
    auto resolveMapValueType = [&](const Expr &target, std::string &valueTypeOut) -> bool {
      valueTypeOut.clear();
      if (target.kind == Expr::Kind::Name) {
        if (const BindingInfo *paramBinding = findParamBinding(params, target.name)) {
          if (paramBinding->typeName != "map") {
            return false;
          }
          std::vector<std::string> parts;
          if (!splitTopLevelTemplateArgs(paramBinding->typeTemplateArg, parts) || parts.size() != 2) {
            return false;
          }
          valueTypeOut = parts[1];
          return true;
        }
        auto it = locals.find(target.name);
        if (it == locals.end() || it->second.typeName != "map") {
          return false;
        }
        std::vector<std::string> parts;
        if (!splitTopLevelTemplateArgs(it->second.typeTemplateArg, parts) || parts.size() != 2) {
          return false;
        }
        valueTypeOut = parts[1];
        return true;
      }
      if (target.kind == Expr::Kind::Call) {
        std::string collectionTypePath;
        if (!resolveCallCollectionTypePath(target, collectionTypePath) || collectionTypePath != "/map") {
          return false;
        }
        std::vector<std::string> args;
        if (resolveCallCollectionTemplateArgs(target, "map", args) && args.size() == 2) {
          valueTypeOut = args[1];
        }
        return true;
      }
      return false;
    };
    auto isStringExpr = [&](const Expr &arg) -> bool {
      if (resolveStringTarget(arg)) {
        return true;
      }
      if (arg.kind == Expr::Kind::Call) {
        std::string accessName;
        if (defMap_.find(resolveCalleePath(arg)) == defMap_.end() && getBuiltinArrayAccessName(arg, accessName) &&
            arg.args.size() == 2) {
          std::string mapValueType;
          if (resolveMapValueType(arg.args.front(), mapValueType) && normalizeBindingTypeName(mapValueType) == "string") {
            return true;
          }
        }
      }
      return false;
    };
    auto validateCollectionElementType = [&](const Expr &arg, const std::string &typeName,
                                              const std::string &errorPrefix) -> bool {
      const std::string normalizedType = normalizeBindingTypeName(typeName);
      if (normalizedType == "string") {
        if (!isStringExpr(arg)) {
          error_ = errorPrefix + typeName;
          return false;
        }
        return true;
      }
      ReturnKind expectedKind = returnKindForTypeName(normalizedType);
      if (expectedKind == ReturnKind::Unknown) {
        return true;
      }
      if (isStringExpr(arg)) {
        error_ = errorPrefix + typeName;
        return false;
      }
      ReturnKind argKind = inferExprReturnKind(arg, params, locals);
      if (argKind != ReturnKind::Unknown && argKind != expectedKind) {
        error_ = errorPrefix + typeName;
        return false;
      }
      return true;
    };
    auto isStaticHelperDefinition = [&](const Definition &def) -> bool {
      for (const auto &transform : def.transforms) {
        if (transform.name == "static") {
          return true;
        }
      }
      return false;
    };
    auto resolveMethodTarget =
        [&](const Expr &receiver, const std::string &methodName, std::string &resolvedOut, bool &isBuiltinOut) -> bool {
      isBuiltinOut = false;
      auto resolveStructTypePath = [&](const std::string &typeName,
                                       const std::string &namespacePrefix) -> std::string {
        if (typeName.empty()) {
          return "";
        }
        if (!typeName.empty() && typeName[0] == '/') {
          return typeName;
        }
        std::string current = namespacePrefix;
        while (true) {
          if (!current.empty()) {
            std::string scoped = current + "/" + typeName;
            if (structNames_.count(scoped) > 0) {
              return scoped;
            }
            if (current.size() > typeName.size()) {
              const size_t start = current.size() - typeName.size();
              if (start > 0 && current[start - 1] == '/' &&
                  current.compare(start, typeName.size(), typeName) == 0 &&
                  structNames_.count(current) > 0) {
                return current;
              }
            }
          } else {
            std::string root = "/" + typeName;
            if (structNames_.count(root) > 0) {
              return root;
            }
          }
          if (current.empty()) {
            break;
          }
          const size_t slash = current.find_last_of('/');
          if (slash == std::string::npos || slash == 0) {
            current.clear();
          } else {
            current.erase(slash);
          }
        }
        auto importIt = importAliases_.find(typeName);
        if (importIt != importAliases_.end()) {
          return importIt->second;
        }
        return "";
      };
      if (methodName == "ok" && receiver.kind == Expr::Kind::Name && receiver.name == "Result") {
        resolvedOut = "/result/ok";
        isBuiltinOut = true;
        return true;
      }
      if (methodName == "error" && receiver.kind == Expr::Kind::Name && receiver.name == "Result") {
        resolvedOut = "/result/error";
        isBuiltinOut = true;
        return true;
      }
      if (methodName == "why" && receiver.kind == Expr::Kind::Name && receiver.name == "Result") {
        resolvedOut = "/result/why";
        isBuiltinOut = true;
        return true;
      }
      if ((methodName == "map" || methodName == "and_then" || methodName == "map2") &&
          receiver.kind == Expr::Kind::Name && receiver.name == "Result") {
        resolvedOut = "/result/" + methodName;
        isBuiltinOut = true;
        return true;
      }
      std::string elemType;
      auto setCollectionMethodTarget = [&](const std::string &path) {
        resolvedOut = path;
        isBuiltinOut = defMap_.count(resolvedOut) == 0;
        return true;
      };
      if (methodName == "count") {
        if (resolveVectorTarget(receiver, elemType)) {
          return setCollectionMethodTarget("/vector/count");
        }
        if (resolveArrayTarget(receiver, elemType)) {
          return setCollectionMethodTarget("/array/count");
        }
        if (resolveStringTarget(receiver)) {
          return setCollectionMethodTarget("/string/count");
        }
        if (resolveMapTarget(receiver)) {
          return setCollectionMethodTarget("/map/count");
        }
      }
      if (methodName == "capacity") {
        if (resolveVectorTarget(receiver, elemType)) {
          return setCollectionMethodTarget("/vector/capacity");
        }
      }
      if (methodName == "at" || methodName == "at_unsafe") {
        if (resolveVectorTarget(receiver, elemType)) {
          return setCollectionMethodTarget("/vector/" + methodName);
        }
        if (resolveArrayTarget(receiver, elemType)) {
          return setCollectionMethodTarget("/array/" + methodName);
        }
        if (resolveStringTarget(receiver)) {
          return setCollectionMethodTarget("/string/" + methodName);
        }
        if (resolveMapTarget(receiver)) {
          return setCollectionMethodTarget("/map/" + methodName);
        }
      }
      if (receiver.kind == Expr::Kind::Call && !receiver.isBinding && !receiver.isMethodCall) {
        const std::string resolvedType = resolveCalleePath(receiver);
        if (!resolvedType.empty() && structNames_.count(resolvedType) > 0) {
          resolvedOut = resolvedType + "/" + methodName;
          return true;
        }
      }
      std::string typeName;
      std::string typeTemplateArg;
      if (receiver.kind == Expr::Kind::Name) {
        if (const BindingInfo *paramBinding = findParamBinding(params, receiver.name)) {
          typeName = paramBinding->typeName;
          typeTemplateArg = paramBinding->typeTemplateArg;
        } else {
          auto it = locals.find(receiver.name);
          if (it != locals.end()) {
            typeName = it->second.typeName;
            typeTemplateArg = it->second.typeTemplateArg;
          }
        }
      }
      if (typeName.empty()) {
        ReturnKind inferredKind = inferExprReturnKind(receiver, params, locals);
        std::string inferred;
        if (inferredKind == ReturnKind::Array) {
          inferred = inferStructReturnPath(receiver, params, locals);
          if (inferred.empty()) {
            inferred = typeNameForReturnKind(inferredKind);
          }
        } else {
          inferred = typeNameForReturnKind(inferredKind);
        }
        if (!inferred.empty()) {
          typeName = inferred;
        }
      }
      if (typeName == "File") {
        if (methodName == "write" || methodName == "write_line" || methodName == "write_byte" ||
            methodName == "write_bytes" || methodName == "flush" || methodName == "close") {
          resolvedOut = "/file/" + methodName;
          isBuiltinOut = true;
          return true;
        }
      }
      if (typeName == "FileError" && methodName == "why") {
        resolvedOut = "/file_error/why";
        isBuiltinOut = true;
        return true;
      }
      if (typeName.empty()) {
        error_ = "unknown method target for " + methodName;
        return false;
      }
      if (typeName == "Pointer" || typeName == "Reference") {
        error_ = "unknown method target for " + methodName;
        return false;
      }
      if (isPrimitiveBindingTypeName(typeName)) {
        resolvedOut = "/" + normalizeBindingTypeName(typeName) + "/" + methodName;
        return true;
      }
      std::string resolvedType = resolveStructTypePath(typeName, receiver.namespacePrefix);
      if (resolvedType.empty()) {
        resolvedType = resolveTypePath(typeName, receiver.namespacePrefix);
      }
      resolvedOut = resolvedType + "/" + methodName;
      return true;
    };
    auto returnKindForBinding = [&](const BindingInfo &binding) -> ReturnKind {
      if (binding.typeName == "Reference") {
        std::string base;
        std::string arg;
        if (splitTemplateTypeName(binding.typeTemplateArg, base, arg) && base == "array") {
          std::vector<std::string> args;
          if (splitTopLevelTemplateArgs(arg, args) && args.size() == 1) {
            return ReturnKind::Array;
          }
        }
        return returnKindForTypeName(binding.typeTemplateArg);
      }
      return returnKindForTypeName(binding.typeName);
    };
    auto isConvertibleExpr = [&](const Expr &arg) -> bool {
      ReturnKind kind = inferExprReturnKind(arg, params, locals);
      if (kind == ReturnKind::Int || kind == ReturnKind::Int64 || kind == ReturnKind::UInt64 ||
          kind == ReturnKind::Float32 || kind == ReturnKind::Float64 || kind == ReturnKind::Bool ||
          kind == ReturnKind::Integer || kind == ReturnKind::Decimal || kind == ReturnKind::Complex) {
        return true;
      }
      if (kind == ReturnKind::Void || kind == ReturnKind::Array) {
        return false;
      }
      if (arg.kind == Expr::Kind::StringLiteral) {
        return false;
      }
      if (isPointerExpr(arg, params, locals)) {
        return false;
      }
      if (arg.kind == Expr::Kind::Call) {
        std::string collection;
        if (defMap_.find(resolveCalleePath(arg)) == defMap_.end() &&
            getBuiltinCollectionName(arg, collection)) {
          return false;
        }
      }
      if (arg.kind == Expr::Kind::Name) {
        if (const BindingInfo *paramBinding = findParamBinding(params, arg.name)) {
          ReturnKind paramKind = returnKindForBinding(*paramBinding);
          return paramKind == ReturnKind::Int || paramKind == ReturnKind::Int64 || paramKind == ReturnKind::UInt64 ||
                 paramKind == ReturnKind::Float32 || paramKind == ReturnKind::Float64 || paramKind == ReturnKind::Bool ||
                 paramKind == ReturnKind::Integer || paramKind == ReturnKind::Decimal || paramKind == ReturnKind::Complex;
        }
        auto it = locals.find(arg.name);
        if (it != locals.end()) {
          ReturnKind localKind = returnKindForBinding(it->second);
          return localKind == ReturnKind::Int || localKind == ReturnKind::Int64 || localKind == ReturnKind::UInt64 ||
                 localKind == ReturnKind::Float32 || localKind == ReturnKind::Float64 || localKind == ReturnKind::Bool ||
                 localKind == ReturnKind::Integer || localKind == ReturnKind::Decimal || localKind == ReturnKind::Complex;
        }
      }
      return true;
    };

    auto inferStructFieldBinding = [&](const Definition &def,
                                       const std::string &fieldName,
                                       BindingInfo &bindingOut,
                                       bool allowStatic,
                                       bool allowPrivate) -> bool {
      auto isStaticField = [](const Expr &stmt) -> bool {
        for (const auto &transform : stmt.transforms) {
          if (transform.name == "static") {
            return true;
          }
        }
        return false;
      };
      auto isPrivateField = [](const Expr &stmt) -> bool {
        for (const auto &transform : stmt.transforms) {
          if (transform.name == "private") {
            return true;
          }
        }
        return false;
      };
      for (const auto &stmt : def.statements) {
        if (!stmt.isBinding) {
          error_ = "struct definitions may only contain field bindings: " + def.fullPath;
          return false;
        }
        if (stmt.name != fieldName) {
          continue;
        }
        if (isStaticField(stmt) && !allowStatic) {
          return false;
        }
        if (isPrivateField(stmt) && !allowPrivate) {
          error_ = "private field is not accessible: " + def.fullPath + "/" + fieldName;
          return false;
        }
        BindingInfo fieldBinding;
        if (!resolveStructFieldBinding(def, stmt, fieldBinding)) {
          return false;
        }
        bindingOut = std::move(fieldBinding);
        return true;
      }
      return false;
    };

    auto resolveStructFieldBinding =
        [&](const Expr &receiver, const std::string &fieldName, BindingInfo &bindingOut) -> bool {
      auto resolveStructPathFromType = [&](const std::string &typeName,
                                           const std::string &namespacePrefix,
                                           std::string &structPathOut) -> bool {
        if (typeName.empty() || isPrimitiveBindingTypeName(typeName)) {
          return false;
        }
        if (!typeName.empty() && typeName[0] == '/') {
          if (structNames_.count(typeName) > 0) {
            structPathOut = typeName;
            return true;
          }
          return false;
        }
        std::string current = namespacePrefix;
        while (true) {
          if (!current.empty()) {
            std::string scoped = current + "/" + typeName;
            if (structNames_.count(scoped) > 0) {
              structPathOut = scoped;
              return true;
            }
            if (current.size() > typeName.size()) {
              const size_t start = current.size() - typeName.size();
              if (start > 0 && current[start - 1] == '/' &&
                  current.compare(start, typeName.size(), typeName) == 0 &&
                  structNames_.count(current) > 0) {
                structPathOut = current;
                return true;
              }
            }
          } else {
            std::string root = "/" + typeName;
            if (structNames_.count(root) > 0) {
              structPathOut = root;
              return true;
            }
          }
          if (current.empty()) {
            break;
          }
          const size_t slash = current.find_last_of('/');
          if (slash == std::string::npos || slash == 0) {
            current.clear();
          } else {
            current.erase(slash);
          }
        }
        auto importIt = importAliases_.find(typeName);
        if (importIt != importAliases_.end() && structNames_.count(importIt->second) > 0) {
          structPathOut = importIt->second;
          return true;
        }
        return false;
      };
      std::string structPath;
      if (receiver.kind == Expr::Kind::Name) {
        const BindingInfo *binding = findParamBinding(params, receiver.name);
        if (!binding) {
          auto it = locals.find(receiver.name);
          if (it != locals.end()) {
            binding = &it->second;
          }
        }
        if (binding) {
          std::string typeName = binding->typeName;
          if ((typeName == "Reference" || typeName == "Pointer") && !binding->typeTemplateArg.empty()) {
            typeName = binding->typeTemplateArg;
          }
          (void)resolveStructPathFromType(typeName, receiver.namespacePrefix, structPath);
        }
      } else if (receiver.kind == Expr::Kind::Call && !receiver.isBinding && !receiver.isMethodCall) {
        std::string resolvedType = resolveCalleePath(receiver);
        if (structNames_.count(resolvedType) > 0) {
          structPath = resolvedType;
        }
      }
      if (structPath.empty()) {
        return false;
      }
      auto defIt = defMap_.find(structPath);
      if (defIt == defMap_.end() || !defIt->second) {
        return false;
      }
      BindingInfo inferred;
      if (!inferStructFieldBinding(*defIt->second, fieldName, inferred, false, true)) {
        return false;
      }
      bindingOut = std::move(inferred);
      return true;
    };

    std::string resolved = resolveCalleePath(expr);
    bool resolvedMethod = false;
    bool usedMethodTarget = false;
    auto promoteCapacityToBuiltinValidation = [&](const Expr &targetExpr,
                                                  std::string &resolvedOut,
                                                  bool &isBuiltinMethodOut) {
      (void)targetExpr;
      // Route unresolved capacity() calls through builtin validation so
      // non-vector targets emit deterministic vector-target diagnostics.
      resolvedOut = "/vector/capacity";
      isBuiltinMethodOut = true;
    };
    if (expr.isFieldAccess) {
      if (expr.args.size() != 1) {
        error_ = "field access requires a receiver";
        return false;
      }
      if (!expr.templateArgs.empty()) {
        error_ = "field access does not accept template arguments";
        return false;
      }
      if (hasNamedArguments(expr.argNames)) {
        error_ = "field access does not accept named arguments";
        return false;
      }
      if (expr.hasBodyArguments || !expr.bodyArguments.empty()) {
        error_ = "field access does not accept block arguments";
        return false;
      }
      if (!validateExpr(params, locals, expr.args.front())) {
        return false;
      }
      BindingInfo fieldBinding;
      if (!resolveStructFieldBinding(expr.args.front(), expr.name, fieldBinding)) {
        if (error_.empty()) {
          error_ = "unknown field: " + expr.name;
        }
        return false;
      }
      return true;
    }
    if (expr.isMethodCall) {
      if (expr.args.empty()) {
        error_ = "method call missing receiver";
        return false;
      }
      usedMethodTarget = true;
      bool isBuiltinMethod = false;
      if (!resolveMethodTarget(expr.args.front(), expr.name, resolved, isBuiltinMethod)) {
        return false;
      }
      if (!isBuiltinMethod && defMap_.find(resolved) == defMap_.end() && expr.name == "capacity") {
        promoteCapacityToBuiltinValidation(expr.args.front(), resolved, isBuiltinMethod);
      }
      if (!isBuiltinMethod && defMap_.find(resolved) == defMap_.end()) {
        error_ = "unknown method: " + resolved;
        return false;
      }
      resolvedMethod = isBuiltinMethod;
    } else if (expr.name == "count" && expr.args.size() == 1 && defMap_.find(resolved) == defMap_.end()) {
      usedMethodTarget = true;
      bool isBuiltinMethod = false;
      std::string methodResolved;
      if (!resolveMethodTarget(expr.args.front(), "count", methodResolved, isBuiltinMethod)) {
        return false;
      }
      if (!isBuiltinMethod && defMap_.find(methodResolved) == defMap_.end()) {
        error_ = "unknown method: " + methodResolved;
        return false;
      }
      resolved = methodResolved;
      resolvedMethod = isBuiltinMethod;
    } else if (expr.name == "capacity" && expr.args.size() == 1 && defMap_.find(resolved) == defMap_.end()) {
      usedMethodTarget = true;
      bool isBuiltinMethod = false;
      std::string methodResolved;
      if (!resolveMethodTarget(expr.args.front(), "capacity", methodResolved, isBuiltinMethod)) {
        return false;
      }
      if (!isBuiltinMethod && defMap_.find(methodResolved) == defMap_.end()) {
        promoteCapacityToBuiltinValidation(expr.args.front(), methodResolved, isBuiltinMethod);
      }
      if (!isBuiltinMethod && defMap_.find(methodResolved) == defMap_.end()) {
        error_ = "unknown method: " + methodResolved;
        return false;
      }
      resolved = methodResolved;
      resolvedMethod = isBuiltinMethod;
    } else if ((expr.name == "at" || expr.name == "at_unsafe") && expr.args.size() == 2 &&
               defMap_.find(resolved) == defMap_.end()) {
      std::string elemType;
      if (resolveVectorTarget(expr.args.front(), elemType) || resolveArrayTarget(expr.args.front(), elemType) ||
          resolveStringTarget(expr.args.front()) || resolveMapTarget(expr.args.front())) {
        usedMethodTarget = true;
        bool isBuiltinMethod = false;
        std::string methodResolved;
        if (!resolveMethodTarget(expr.args.front(), expr.name, methodResolved, isBuiltinMethod)) {
          return false;
        }
        if (!isBuiltinMethod && defMap_.find(methodResolved) == defMap_.end()) {
          error_ = "unknown method: " + methodResolved;
          return false;
        }
        resolved = methodResolved;
        resolvedMethod = isBuiltinMethod;
      }
    }
    if (usedMethodTarget && !resolvedMethod) {
      auto defIt = defMap_.find(resolved);
      if (defIt != defMap_.end() && isStaticHelperDefinition(*defIt->second)) {
        error_ = "static helper does not accept method-call syntax: " + resolved;
        return false;
      }
    }
      if ((expr.hasBodyArguments || !expr.bodyArguments.empty()) && !isBuiltinBlockCall(expr)) {
        if (resolvedMethod || defMap_.find(resolved) == defMap_.end()) {
          error_ = "block arguments require a definition target: " + resolved;
          return false;
        }
        std::unordered_map<std::string, BindingInfo> blockLocals = locals;
        std::vector<Expr> livenessStatements = expr.bodyArguments;
        if (enclosingStatements != nullptr && statementIndex < enclosingStatements->size()) {
          for (size_t idx = statementIndex + 1; idx < enclosingStatements->size(); ++idx) {
            livenessStatements.push_back((*enclosingStatements)[idx]);
          }
        }
        OnErrorScope onErrorScope(*this, std::nullopt);
        BorrowEndScope borrowScope(*this, endedReferenceBorrows_);
        for (size_t bodyIndex = 0; bodyIndex < expr.bodyArguments.size(); ++bodyIndex) {
          const Expr &bodyExpr = expr.bodyArguments[bodyIndex];
          if (!validateStatement(params, blockLocals, bodyExpr, ReturnKind::Unknown, false, true, nullptr,
                                 expr.namespacePrefix, &expr.bodyArguments, bodyIndex)) {
            return false;
          }
          expireReferenceBorrowsForRemainder(params, blockLocals, livenessStatements, bodyIndex + 1);
        }
      }
    std::string gpuBuiltin;
    if (getBuiltinGpuName(expr, gpuBuiltin)) {
      if (hasNamedArguments(expr.argNames)) {
        error_ = "named arguments not supported for builtin calls";
        return false;
      }
      if (!expr.templateArgs.empty()) {
        error_ = "gpu builtins do not accept template arguments";
        return false;
      }
      if (expr.hasBodyArguments || !expr.bodyArguments.empty()) {
        error_ = "gpu builtins do not accept block arguments";
        return false;
      }
      if (!expr.args.empty()) {
        error_ = "gpu builtins do not accept arguments";
        return false;
      }
      if (!currentDefinitionIsCompute_) {
        error_ = "gpu builtins require a compute definition";
        return false;
      }
      return true;
    }
    PathSpaceBuiltin pathSpaceBuiltin;
    if (getPathSpaceBuiltin(expr, pathSpaceBuiltin) && defMap_.find(resolved) == defMap_.end()) {
      error_ = pathSpaceBuiltin.name + " is statement-only";
      return false;
    }

    if (!validateNamedArguments(expr.args, expr.argNames, resolved, error_)) {
      return false;
    }
    std::function<bool(const Expr &)> isUnsafeReferenceExpr;
    isUnsafeReferenceExpr = [&](const Expr &argExpr) -> bool {
        if (argExpr.kind == Expr::Kind::Name) {
          if (const BindingInfo *paramBinding = findParamBinding(params, argExpr.name)) {
            return paramBinding->typeName == "Reference" && paramBinding->isUnsafeReference;
        }
        auto itLocal = locals.find(argExpr.name);
        return itLocal != locals.end() && itLocal->second.typeName == "Reference" && itLocal->second.isUnsafeReference;
        }
        if (argExpr.kind != Expr::Kind::Call || argExpr.isBinding) {
          return false;
        }
        auto hasUnsafeChildExpr = [&](const Expr &callExpr) -> bool {
          for (const auto &nestedArg : callExpr.args) {
            if (isUnsafeReferenceExpr(nestedArg)) {
              return true;
            }
          }
          for (const auto &bodyExpr : callExpr.bodyArguments) {
            if (isUnsafeReferenceExpr(bodyExpr)) {
              return true;
            }
          }
          return false;
        };
        if (isIfCall(argExpr) || isMatchCall(argExpr) || isBlockCall(argExpr) || isReturnCall(argExpr) ||
            isSimpleCallName(argExpr, "then") || isSimpleCallName(argExpr, "else") ||
            isSimpleCallName(argExpr, "case")) {
          return hasUnsafeChildExpr(argExpr);
        }
        const std::string nestedResolved = resolveCalleePath(argExpr);
        if (nestedResolved.empty()) {
          return false;
        }
      auto nestedIt = defMap_.find(nestedResolved);
      if (nestedIt == defMap_.end() || nestedIt->second == nullptr) {
        return false;
      }
      bool returnsReference = false;
      for (const auto &transform : nestedIt->second->transforms) {
        if (transform.name != "return" || transform.templateArgs.size() != 1) {
          continue;
        }
        std::string base;
        std::string arg;
        if (splitTemplateTypeName(transform.templateArgs.front(), base, arg) && base == "Reference") {
          returnsReference = true;
          break;
        }
      }
      if (!returnsReference) {
        return false;
      }
      const auto &nestedParams = paramsByDef_[nestedResolved];
      if (nestedParams.empty()) {
        return false;
      }
      std::string nestedArgError;
      if (!validateNamedArgumentsAgainstParams(nestedParams, argExpr.argNames, nestedArgError)) {
        return false;
      }
      std::vector<const Expr *> nestedOrderedArgs;
      if (!buildOrderedArguments(nestedParams, argExpr.args, argExpr.argNames, nestedOrderedArgs, nestedArgError)) {
        return false;
      }
      for (size_t nestedIndex = 0; nestedIndex < nestedOrderedArgs.size() && nestedIndex < nestedParams.size();
           ++nestedIndex) {
        const Expr *nestedArg = nestedOrderedArgs[nestedIndex];
        if (nestedArg == nullptr || nestedParams[nestedIndex].binding.typeName != "Reference") {
          continue;
        }
        if (isUnsafeReferenceExpr(*nestedArg)) {
          return true;
        }
      }
      return false;
    };
    std::function<bool(const Expr &, std::string &)> resolveEscapingReferenceRoot;
    resolveEscapingReferenceRoot = [&](const Expr &argExpr, std::string &rootOut) -> bool {
      rootOut.clear();
      if (argExpr.kind == Expr::Kind::Name) {
        if (findParamBinding(params, argExpr.name) != nullptr) {
          return false;
        }
        auto itLocal = locals.find(argExpr.name);
        if (itLocal == locals.end() || itLocal->second.typeName != "Reference") {
          return false;
        }
        std::string sourceRoot = itLocal->second.referenceRoot.empty() ? argExpr.name : itLocal->second.referenceRoot;
        if (const BindingInfo *rootParam = findParamBinding(params, sourceRoot)) {
          if (rootParam->typeName == "Reference") {
            return false;
          }
        }
        rootOut = sourceRoot;
        return true;
      }
      if (argExpr.kind != Expr::Kind::Call || argExpr.isBinding) {
        return false;
      }
      auto resolveChildRoot = [&](const Expr &callExpr) -> bool {
        for (const auto &nestedArg : callExpr.args) {
          if (resolveEscapingReferenceRoot(nestedArg, rootOut)) {
            return true;
          }
        }
        for (const auto &bodyExpr : callExpr.bodyArguments) {
          if (resolveEscapingReferenceRoot(bodyExpr, rootOut)) {
            return true;
          }
        }
        return false;
      };
      if (isIfCall(argExpr) || isMatchCall(argExpr) || isBlockCall(argExpr) || isReturnCall(argExpr) ||
          isSimpleCallName(argExpr, "then") || isSimpleCallName(argExpr, "else") ||
          isSimpleCallName(argExpr, "case")) {
        return resolveChildRoot(argExpr);
      }
      const std::string nestedResolved = resolveCalleePath(argExpr);
      if (nestedResolved.empty()) {
        return false;
      }
      auto nestedIt = defMap_.find(nestedResolved);
      if (nestedIt == defMap_.end() || nestedIt->second == nullptr) {
        return false;
      }
      bool returnsReference = false;
      for (const auto &transform : nestedIt->second->transforms) {
        if (transform.name != "return" || transform.templateArgs.size() != 1) {
          continue;
        }
        std::string base;
        std::string arg;
        if (splitTemplateTypeName(transform.templateArgs.front(), base, arg) && base == "Reference") {
          returnsReference = true;
          break;
        }
      }
      if (!returnsReference) {
        return false;
      }
      const auto &nestedParams = paramsByDef_[nestedResolved];
      if (nestedParams.empty()) {
        return false;
      }
      std::string nestedArgError;
      if (!validateNamedArgumentsAgainstParams(nestedParams, argExpr.argNames, nestedArgError)) {
        return false;
      }
      std::vector<const Expr *> nestedOrderedArgs;
      if (!buildOrderedArguments(nestedParams, argExpr.args, argExpr.argNames, nestedOrderedArgs, nestedArgError)) {
        return false;
      }
      for (size_t nestedIndex = 0; nestedIndex < nestedOrderedArgs.size() && nestedIndex < nestedParams.size();
           ++nestedIndex) {
        const Expr *nestedArg = nestedOrderedArgs[nestedIndex];
        if (nestedArg == nullptr || nestedParams[nestedIndex].binding.typeName != "Reference") {
          continue;
        }
        if (resolveEscapingReferenceRoot(*nestedArg, rootOut)) {
          return true;
        }
      }
      return false;
    };
    auto reportReferenceAssignmentEscape = [&](const std::string &sinkName, const Expr &rhsExpr) -> bool {
      std::string sourceRoot;
      if (!resolveEscapingReferenceRoot(rhsExpr, sourceRoot)) {
        return false;
      }
      if (sourceRoot.empty()) {
        sourceRoot = "<unknown>";
      }
      const std::string sink = sinkName.empty() ? "<unknown>" : sinkName;
      if (currentDefinitionIsUnsafe_ && isUnsafeReferenceExpr(rhsExpr)) {
        error_ = "unsafe reference escapes via assignment to " + sink +
                 " (root: " + sourceRoot + ", sink: " + sink + ")";
      } else {
        error_ = "reference escapes via assignment to " + sink +
                 " (root: " + sourceRoot + ", sink: " + sink + ")";
      }
      return true;
    };
    auto it = defMap_.find(resolved);
    if (it == defMap_.end() || resolvedMethod) {
      if (!expr.isMethodCall && isSimpleCallName(expr, "try")) {
        if (hasNamedArguments(expr.argNames)) {
          error_ = "named arguments not supported for builtin calls";
          return false;
        }
        if (!expr.templateArgs.empty()) {
          error_ = "try does not accept template arguments";
          return false;
        }
        if (expr.hasBodyArguments || !expr.bodyArguments.empty()) {
          error_ = "try does not accept block arguments";
          return false;
        }
        if (expr.args.size() != 1) {
          error_ = "try requires exactly one argument";
          return false;
        }
        if (!currentOnError_.has_value()) {
          error_ = "missing on_error for ? usage";
          return false;
        }
        if (!currentResultType_.has_value() || !currentResultType_->isResult) {
          error_ = "try requires Result return type";
          return false;
        }
        if (!errorTypesMatch(currentResultType_->errorType, currentOnError_->errorType, expr.namespacePrefix)) {
          error_ = "on_error error type mismatch";
          return false;
        }
        if (!validateExpr(params, locals, expr.args.front())) {
          return false;
        }
        ResultTypeInfo argResult;
        if (!resolveResultTypeForExpr(expr.args.front(), params, locals, argResult) || !argResult.isResult) {
          error_ = "try requires Result argument";
          return false;
        }
        if (!errorTypesMatch(argResult.errorType, currentOnError_->errorType, expr.namespacePrefix)) {
          error_ = "try error type mismatch";
          return false;
        }
        return true;
      }
      if (!expr.isMethodCall && isSimpleCallName(expr, "File")) {
        if (hasNamedArguments(expr.argNames)) {
          error_ = "named arguments not supported for builtin calls";
          return false;
        }
        if (expr.templateArgs.size() != 1) {
          error_ = "File requires exactly one template argument";
          return false;
        }
        const std::string &mode = expr.templateArgs.front();
        if (mode != "Read" && mode != "Write" && mode != "Append") {
          error_ = "File requires Read, Write, or Append mode";
          return false;
        }
        if (expr.args.size() != 1) {
          error_ = "File requires exactly one path argument";
          return false;
        }
        if (expr.hasBodyArguments || !expr.bodyArguments.empty()) {
          error_ = "File does not accept block arguments";
          return false;
        }
        if (activeEffects_.count("file_write") == 0) {
          error_ = "File requires file_write effect";
          return false;
        }
        if (!validateExpr(params, locals, expr.args.front())) {
          return false;
        }
        if (!isStringExpr(expr.args.front())) {
          error_ = "File requires string path argument";
          return false;
        }
        return true;
      }
      if (resolvedMethod && resolved == "/result/ok") {
        if (hasNamedArguments(expr.argNames)) {
          error_ = "named arguments not supported for builtin calls";
          return false;
        }
        if (!expr.templateArgs.empty()) {
          error_ = "Result.ok does not accept template arguments";
          return false;
        }
        if (expr.hasBodyArguments || !expr.bodyArguments.empty()) {
          error_ = "Result.ok does not accept block arguments";
          return false;
        }
        if (expr.args.size() > 2) {
          error_ = "Result.ok accepts at most one value argument";
          return false;
        }
        for (size_t i = 1; i < expr.args.size(); ++i) {
          if (!validateExpr(params, locals, expr.args[i])) {
            return false;
          }
        }
        return true;
      }
      if (resolvedMethod && resolved == "/result/error") {
        if (hasNamedArguments(expr.argNames)) {
          error_ = "named arguments not supported for builtin calls";
          return false;
        }
        if (!expr.templateArgs.empty()) {
          error_ = "Result.error does not accept template arguments";
          return false;
        }
        if (expr.hasBodyArguments || !expr.bodyArguments.empty()) {
          error_ = "Result.error does not accept block arguments";
          return false;
        }
        if (expr.args.size() != 2) {
          error_ = "Result.error requires exactly one argument";
          return false;
        }
        if (!validateExpr(params, locals, expr.args[1])) {
          return false;
        }
        ResultTypeInfo argResult;
        if (!resolveResultTypeForExpr(expr.args[1], params, locals, argResult) || !argResult.isResult) {
          error_ = "Result.error requires Result argument";
          return false;
        }
        return true;
      }
      if (resolvedMethod && resolved == "/result/why") {
        if (hasNamedArguments(expr.argNames)) {
          error_ = "named arguments not supported for builtin calls";
          return false;
        }
        if (!expr.templateArgs.empty()) {
          error_ = "Result.why does not accept template arguments";
          return false;
        }
        if (expr.hasBodyArguments || !expr.bodyArguments.empty()) {
          error_ = "Result.why does not accept block arguments";
          return false;
        }
        if (expr.args.size() != 2) {
          error_ = "Result.why requires exactly one argument";
          return false;
        }
        if (!validateExpr(params, locals, expr.args[1])) {
          return false;
        }
        ResultTypeInfo argResult;
        if (!resolveResultTypeForExpr(expr.args[1], params, locals, argResult) || !argResult.isResult) {
          error_ = "Result.why requires Result argument";
          return false;
        }
        return true;
      }
      if (resolvedMethod && (resolved == "/result/map" || resolved == "/result/and_then")) {
        if (hasNamedArguments(expr.argNames)) {
          error_ = "named arguments not supported for builtin calls";
          return false;
        }
        if (!expr.templateArgs.empty()) {
          error_ = "Result." + expr.name + " does not accept template arguments";
          return false;
        }
        if (expr.hasBodyArguments || !expr.bodyArguments.empty()) {
          error_ = "Result." + expr.name + " does not accept block arguments";
          return false;
        }
        if (expr.args.size() != 3) {
          error_ = "Result." + expr.name + " requires exactly two arguments";
          return false;
        }
        if (!validateExpr(params, locals, expr.args[1])) {
          return false;
        }
        ResultTypeInfo argResult;
        if (!resolveResultTypeForExpr(expr.args[1], params, locals, argResult) || !argResult.isResult) {
          error_ = "Result." + expr.name + " requires Result argument";
          return false;
        }
        if (!argResult.hasValue) {
          error_ = "Result." + expr.name + " requires value Result";
          return false;
        }
        if (!expr.args[2].isLambda) {
          error_ = "Result." + expr.name + " requires a lambda argument";
          return false;
        }
        if (expr.args[2].args.size() != 1) {
          error_ = "Result." + expr.name + " requires a single-parameter lambda";
          return false;
        }
        if (!validateExpr(params, locals, expr.args[2])) {
          return false;
        }
        return true;
      }
      if (resolvedMethod && resolved == "/result/map2") {
        if (hasNamedArguments(expr.argNames)) {
          error_ = "named arguments not supported for builtin calls";
          return false;
        }
        if (!expr.templateArgs.empty()) {
          error_ = "Result.map2 does not accept template arguments";
          return false;
        }
        if (expr.hasBodyArguments || !expr.bodyArguments.empty()) {
          error_ = "Result.map2 does not accept block arguments";
          return false;
        }
        if (expr.args.size() != 4) {
          error_ = "Result.map2 requires exactly three arguments";
          return false;
        }
        if (!validateExpr(params, locals, expr.args[1]) || !validateExpr(params, locals, expr.args[2])) {
          return false;
        }
        ResultTypeInfo leftResult;
        ResultTypeInfo rightResult;
        if (!resolveResultTypeForExpr(expr.args[1], params, locals, leftResult) || !leftResult.isResult ||
            !resolveResultTypeForExpr(expr.args[2], params, locals, rightResult) || !rightResult.isResult) {
          error_ = "Result.map2 requires Result arguments";
          return false;
        }
        if (!leftResult.hasValue || !rightResult.hasValue) {
          error_ = "Result.map2 requires value Results";
          return false;
        }
        if (!errorTypesMatch(leftResult.errorType, rightResult.errorType, expr.namespacePrefix)) {
          error_ = "Result.map2 requires matching error types";
          return false;
        }
        if (!expr.args[3].isLambda) {
          error_ = "Result.map2 requires a lambda argument";
          return false;
        }
        if (expr.args[3].args.size() != 2) {
          error_ = "Result.map2 requires a two-parameter lambda";
          return false;
        }
        if (!validateExpr(params, locals, expr.args[3])) {
          return false;
        }
        return true;
      }
      if (resolvedMethod && resolved == "/file_error/why") {
        if (hasNamedArguments(expr.argNames)) {
          error_ = "named arguments not supported for builtin calls";
          return false;
        }
        if (!expr.templateArgs.empty()) {
          error_ = "why does not accept template arguments";
          return false;
        }
        if (expr.hasBodyArguments || !expr.bodyArguments.empty()) {
          error_ = "why does not accept block arguments";
          return false;
        }
        if (expr.args.size() != 1) {
          error_ = "why does not accept arguments";
          return false;
        }
        if (!validateExpr(params, locals, expr.args.front())) {
          return false;
        }
        return true;
      }
      if (resolvedMethod && resolved.rfind("/file/", 0) == 0) {
        if (hasNamedArguments(expr.argNames)) {
          error_ = "named arguments not supported for builtin calls";
          return false;
        }
        if (!expr.templateArgs.empty()) {
          error_ = "file methods do not accept template arguments";
          return false;
        }
        if (expr.hasBodyArguments || !expr.bodyArguments.empty()) {
          error_ = "file methods do not accept block arguments";
          return false;
        }
        if (expr.args.empty()) {
          error_ = "file method missing receiver";
          return false;
        }
        if (activeEffects_.count("file_write") == 0) {
          error_ = "file operations require file_write effect";
          return false;
        }
        if (!validateExpr(params, locals, expr.args.front())) {
          return false;
        }
        auto isFilePrintableExpr = [&](const Expr &arg) -> bool {
          if (isStringExpr(arg)) {
            return true;
          }
          ReturnKind kind = inferExprReturnKind(arg, params, locals);
          if (kind == ReturnKind::Int || kind == ReturnKind::Int64 || kind == ReturnKind::UInt64 ||
              kind == ReturnKind::Bool) {
            return true;
          }
          if (kind == ReturnKind::Void || kind == ReturnKind::Array) {
            return false;
          }
          if (arg.kind == Expr::Kind::Name) {
            if (const BindingInfo *paramBinding = findParamBinding(params, arg.name)) {
              ReturnKind paramKind = returnKindForBinding(*paramBinding);
              return paramKind == ReturnKind::Int || paramKind == ReturnKind::Int64 ||
                     paramKind == ReturnKind::UInt64 || paramKind == ReturnKind::Bool;
            }
            auto itLocal = locals.find(arg.name);
            if (itLocal != locals.end()) {
              ReturnKind localKind = returnKindForBinding(itLocal->second);
              return localKind == ReturnKind::Int || localKind == ReturnKind::Int64 ||
                     localKind == ReturnKind::UInt64 || localKind == ReturnKind::Bool;
            }
          }
          if (isPointerLikeExpr(arg, params, locals)) {
            return false;
          }
          if (arg.kind == Expr::Kind::Call) {
            std::string builtinName;
            if (defMap_.find(resolveCalleePath(arg)) == defMap_.end() && getBuiltinCollectionName(arg, builtinName)) {
              return false;
            }
          }
          return false;
        };
        auto isIntegerOrBoolExpr = [&](const Expr &arg) -> bool {
          ReturnKind kind = inferExprReturnKind(arg, params, locals);
          if (kind == ReturnKind::Int || kind == ReturnKind::Int64 || kind == ReturnKind::UInt64 ||
              kind == ReturnKind::Bool) {
            return true;
          }
          if (kind == ReturnKind::Float32 || kind == ReturnKind::Float64 || kind == ReturnKind::Void ||
              kind == ReturnKind::Array) {
            return false;
          }
          if (kind == ReturnKind::Unknown) {
            if (arg.kind == Expr::Kind::FloatLiteral || arg.kind == Expr::Kind::StringLiteral) {
              return false;
            }
            if (isPointerExpr(arg, params, locals)) {
              return false;
            }
            if (arg.kind == Expr::Kind::Name) {
              if (const BindingInfo *paramBinding = findParamBinding(params, arg.name)) {
                ReturnKind paramKind = returnKindForBinding(*paramBinding);
                return paramKind == ReturnKind::Int || paramKind == ReturnKind::Int64 ||
                       paramKind == ReturnKind::UInt64 || paramKind == ReturnKind::Bool;
              }
              auto itLocal = locals.find(arg.name);
              if (itLocal != locals.end()) {
                ReturnKind localKind = returnKindForBinding(itLocal->second);
                return localKind == ReturnKind::Int || localKind == ReturnKind::Int64 ||
                       localKind == ReturnKind::UInt64 || localKind == ReturnKind::Bool;
              }
            }
          }
          return false;
        };
        if (expr.name == "write" || expr.name == "write_line") {
          for (size_t i = 1; i < expr.args.size(); ++i) {
            if (!validateExpr(params, locals, expr.args[i])) {
              return false;
            }
            if (!isFilePrintableExpr(expr.args[i])) {
              error_ = "file write requires integer/bool or string arguments";
              return false;
            }
          }
          return true;
        }
        if (expr.name == "write_byte") {
          if (expr.args.size() != 2) {
            error_ = "write_byte requires exactly one argument";
            return false;
          }
          if (!validateExpr(params, locals, expr.args[1])) {
            return false;
          }
          if (!isIntegerOrBoolExpr(expr.args[1])) {
            error_ = "write_byte requires integer argument";
            return false;
          }
          return true;
        }
        if (expr.name == "write_bytes") {
          if (expr.args.size() != 2) {
            error_ = "write_bytes requires exactly one argument";
            return false;
          }
          if (!validateExpr(params, locals, expr.args[1])) {
            return false;
          }
          bool ok = false;
          if (expr.args[1].kind == Expr::Kind::Call) {
            std::string collection;
            if (defMap_.find(resolveCalleePath(expr.args[1])) == defMap_.end() &&
                getBuiltinCollectionName(expr.args[1], collection) && collection == "array") {
              ok = true;
            }
          }
          if (expr.args[1].kind == Expr::Kind::Name) {
            if (const BindingInfo *paramBinding = findParamBinding(params, expr.args[1].name)) {
              ok = (paramBinding->typeName == "array");
            } else {
              auto itLocal = locals.find(expr.args[1].name);
              ok = (itLocal != locals.end() && itLocal->second.typeName == "array");
            }
          }
          if (!ok) {
            error_ = "write_bytes requires array argument";
            return false;
          }
          return true;
        }
        if (expr.name == "flush" || expr.name == "close") {
          if (expr.args.size() != 1) {
            error_ = expr.name + " does not accept arguments";
            return false;
          }
          return true;
        }
      }
      if (hasNamedArguments(expr.argNames)) {
        std::string builtinName;
        auto isLegacyCollectionBuiltinCall = [&]() {
          std::string collectionName;
          if (!getBuiltinCollectionName(expr, collectionName)) {
            return false;
          }
          return defMap_.find(resolved) == defMap_.end();
        };
        auto isLegacyArrayAccessBuiltinCall = [&]() {
          std::string arrayAccessName;
          if (!getBuiltinArrayAccessName(expr, arrayAccessName)) {
            return false;
          }
          return defMap_.find(resolved) == defMap_.end();
        };
        auto isLegacyCountBuiltinCall = [&]() {
          if (expr.name != "count") {
            return false;
          }
          return defMap_.find(resolved) == defMap_.end();
        };
        auto isLegacyCapacityBuiltinCall = [&]() {
          if (expr.name != "capacity") {
            return false;
          }
          return defMap_.find(resolved) == defMap_.end();
        };
        auto isLegacyVectorHelperBuiltinCall = [&]() {
          if (!(isSimpleCallName(expr, "push") || isSimpleCallName(expr, "pop") ||
                isSimpleCallName(expr, "reserve") || isSimpleCallName(expr, "clear") ||
                isSimpleCallName(expr, "remove_at") || isSimpleCallName(expr, "remove_swap"))) {
            return false;
          }
          return defMap_.find(resolved) == defMap_.end();
        };
        bool isBuiltin = false;
        if (getBuiltinOperatorName(expr, builtinName) || getBuiltinComparisonName(expr, builtinName) ||
            getBuiltinMutationName(expr, builtinName) ||
            getBuiltinClampName(expr, builtinName, allowMathBareName(expr.name)) ||
            getBuiltinMinMaxName(expr, builtinName, allowMathBareName(expr.name)) ||
            getBuiltinAbsSignName(expr, builtinName, allowMathBareName(expr.name)) ||
            getBuiltinSaturateName(expr, builtinName, allowMathBareName(expr.name)) ||
            getBuiltinMathName(expr, builtinName, allowMathBareName(expr.name)) ||
            getBuiltinGpuName(expr, builtinName) ||
            getBuiltinPointerName(expr, builtinName) || getBuiltinConvertName(expr, builtinName) ||
            isLegacyCollectionBuiltinCall() || isLegacyArrayAccessBuiltinCall() ||
            isAssignCall(expr) || isIfCall(expr) || isMatchCall(expr) || isLoopCall(expr) || isWhileCall(expr) ||
            isForCall(expr) ||
            isRepeatCall(expr) || isLegacyCountBuiltinCall() || expr.name == "File" || expr.name == "try" ||
            isLegacyCapacityBuiltinCall() || isLegacyVectorHelperBuiltinCall() ||
            isSimpleCallName(expr, "dispatch") || isSimpleCallName(expr, "buffer") ||
            isSimpleCallName(expr, "upload") || isSimpleCallName(expr, "readback") ||
            isSimpleCallName(expr, "buffer_load") || isSimpleCallName(expr, "buffer_store")) {
          isBuiltin = true;
        }
      if (isBuiltin) {
        error_ = "named arguments not supported for builtin calls";
        return false;
      }
    }
    auto isIntegerKind = [&](ReturnKind kind) -> bool {
      return kind == ReturnKind::Int || kind == ReturnKind::Int64 || kind == ReturnKind::UInt64;
    };
    auto isNumericOrBoolKind = [&](ReturnKind kind) -> bool {
      return kind == ReturnKind::Int || kind == ReturnKind::Int64 || kind == ReturnKind::UInt64 ||
             kind == ReturnKind::Float32 || kind == ReturnKind::Float64 || kind == ReturnKind::Bool;
    };
    auto resolveArrayElemType = [&](const Expr &arg, std::string &elemType) -> bool {
      if (arg.kind == Expr::Kind::Name) {
        if (const BindingInfo *paramBinding = findParamBinding(params, arg.name)) {
          if (paramBinding->typeName == "array" && !paramBinding->typeTemplateArg.empty()) {
            elemType = paramBinding->typeTemplateArg;
            return true;
          }
        }
        auto itLocal = locals.find(arg.name);
        if (itLocal != locals.end()) {
          if (itLocal->second.typeName == "array" && !itLocal->second.typeTemplateArg.empty()) {
            elemType = itLocal->second.typeTemplateArg;
            return true;
          }
        }
      }
      if (arg.kind == Expr::Kind::Call) {
        std::string collection;
        if (defMap_.find(resolveCalleePath(arg)) != defMap_.end()) {
          return false;
        }
        if (getBuiltinCollectionName(arg, collection) && collection == "array" && arg.templateArgs.size() == 1) {
          elemType = arg.templateArgs.front();
          return true;
        }
      }
      return false;
    };
    auto resolveBufferElemType = [&](const Expr &arg, std::string &elemType) -> bool {
      if (arg.kind == Expr::Kind::Name) {
        if (const BindingInfo *paramBinding = findParamBinding(params, arg.name)) {
          if (paramBinding->typeName == "Buffer" && !paramBinding->typeTemplateArg.empty()) {
            elemType = paramBinding->typeTemplateArg;
            return true;
          }
        }
        auto itLocal = locals.find(arg.name);
        if (itLocal != locals.end()) {
          if (itLocal->second.typeName == "Buffer" && !itLocal->second.typeTemplateArg.empty()) {
            elemType = itLocal->second.typeTemplateArg;
            return true;
          }
        }
      }
      if (arg.kind == Expr::Kind::Call) {
        if (isSimpleCallName(arg, "buffer") && arg.templateArgs.size() == 1) {
          elemType = arg.templateArgs.front();
          return true;
        }
        if (isSimpleCallName(arg, "upload") && arg.args.size() == 1) {
          return resolveArrayElemType(arg.args.front(), elemType);
        }
      }
      return false;
    };
    if (isSimpleCallName(expr, "dispatch")) {
      if (currentDefinitionIsCompute_) {
        error_ = "dispatch is not allowed in compute definitions";
        return false;
      }
      if (activeEffects_.count("gpu_dispatch") == 0) {
        error_ = "dispatch requires gpu_dispatch effect";
        return false;
      }
      if (!expr.templateArgs.empty()) {
        error_ = "dispatch does not accept template arguments";
        return false;
      }
      if (expr.hasBodyArguments || !expr.bodyArguments.empty()) {
        error_ = "dispatch does not accept block arguments";
        return false;
      }
      if (expr.args.size() < 4) {
        error_ = "dispatch requires kernel and three dimension arguments";
        return false;
      }
      if (expr.args.front().kind != Expr::Kind::Name) {
        error_ = "dispatch requires kernel name as first argument";
        return false;
      }
      const Expr &kernelExpr = expr.args.front();
      const std::string kernelPath = resolveCalleePath(kernelExpr);
      auto defIt = defMap_.find(kernelPath);
      if (defIt == defMap_.end()) {
        error_ = "unknown kernel: " + kernelPath;
        return false;
      }
      bool isCompute = false;
      for (const auto &transform : defIt->second->transforms) {
        if (transform.name == "compute") {
          isCompute = true;
          break;
        }
      }
      if (!isCompute) {
        error_ = "dispatch requires compute definition: " + kernelPath;
        return false;
      }
      for (size_t i = 1; i <= 3; ++i) {
        if (!validateExpr(params, locals, expr.args[i])) {
          return false;
        }
        ReturnKind dimKind = inferExprReturnKind(expr.args[i], params, locals);
        if (!isIntegerKind(dimKind)) {
          error_ = "dispatch dimensions require integer expressions";
          return false;
        }
      }
      const auto &kernelParams = paramsByDef_[kernelPath];
      if (expr.args.size() != kernelParams.size() + 4) {
        error_ = "dispatch argument count mismatch for " + kernelPath;
        return false;
      }
      for (size_t i = 4; i < expr.args.size(); ++i) {
        if (!validateExpr(params, locals, expr.args[i])) {
          return false;
        }
      }
      return true;
    }
    if (isSimpleCallName(expr, "buffer")) {
      if (currentDefinitionIsCompute_) {
        error_ = "buffer is not allowed in compute definitions";
        return false;
      }
      if (activeEffects_.count("gpu_dispatch") == 0) {
        error_ = "buffer requires gpu_dispatch effect";
        return false;
      }
      if (expr.templateArgs.size() != 1) {
        error_ = "buffer requires exactly one template argument";
        return false;
      }
      if (expr.args.size() != 1) {
        error_ = "buffer requires exactly one argument";
        return false;
      }
      if (expr.hasBodyArguments || !expr.bodyArguments.empty()) {
        error_ = "buffer does not accept block arguments";
        return false;
      }
      if (!validateExpr(params, locals, expr.args.front())) {
        return false;
      }
      ReturnKind countKind = inferExprReturnKind(expr.args.front(), params, locals);
      if (!isIntegerKind(countKind)) {
        error_ = "buffer size requires integer expression";
        return false;
      }
      ReturnKind elemKind = returnKindForTypeName(expr.templateArgs.front());
      if (!isNumericOrBoolKind(elemKind)) {
        error_ = "buffer requires numeric/bool element type";
        return false;
      }
      return true;
    }
    if (isSimpleCallName(expr, "upload")) {
      if (currentDefinitionIsCompute_) {
        error_ = "upload is not allowed in compute definitions";
        return false;
      }
      if (activeEffects_.count("gpu_dispatch") == 0) {
        error_ = "upload requires gpu_dispatch effect";
        return false;
      }
      if (!expr.templateArgs.empty()) {
        error_ = "upload does not accept template arguments";
        return false;
      }
      if (expr.args.size() != 1) {
        error_ = "upload requires exactly one argument";
        return false;
      }
      if (expr.hasBodyArguments || !expr.bodyArguments.empty()) {
        error_ = "upload does not accept block arguments";
        return false;
      }
      if (!validateExpr(params, locals, expr.args.front())) {
        return false;
      }
      std::string elemType;
      if (!resolveArrayElemType(expr.args.front(), elemType)) {
        error_ = "upload requires array input";
        return false;
      }
      ReturnKind elemKind = returnKindForTypeName(elemType);
      if (!isNumericOrBoolKind(elemKind)) {
        error_ = "upload requires numeric/bool element type";
        return false;
      }
      return true;
    }
    if (isSimpleCallName(expr, "readback")) {
      if (currentDefinitionIsCompute_) {
        error_ = "readback is not allowed in compute definitions";
        return false;
      }
      if (activeEffects_.count("gpu_dispatch") == 0) {
        error_ = "readback requires gpu_dispatch effect";
        return false;
      }
      if (!expr.templateArgs.empty()) {
        error_ = "readback does not accept template arguments";
        return false;
      }
      if (expr.args.size() != 1) {
        error_ = "readback requires exactly one argument";
        return false;
      }
      if (expr.hasBodyArguments || !expr.bodyArguments.empty()) {
        error_ = "readback does not accept block arguments";
        return false;
      }
      if (!validateExpr(params, locals, expr.args.front())) {
        return false;
      }
      std::string elemType;
      if (!resolveBufferElemType(expr.args.front(), elemType)) {
        error_ = "readback requires Buffer input";
        return false;
      }
      ReturnKind elemKind = returnKindForTypeName(elemType);
      if (!isNumericOrBoolKind(elemKind)) {
        error_ = "readback requires numeric/bool element type";
        return false;
      }
      return true;
    }
    if (isSimpleCallName(expr, "buffer_load")) {
      if (!currentDefinitionIsCompute_) {
        error_ = "buffer_load requires a compute definition";
        return false;
      }
      if (!expr.templateArgs.empty()) {
        error_ = "buffer_load does not accept template arguments";
        return false;
      }
      if (expr.args.size() != 2) {
        error_ = "buffer_load requires buffer and index arguments";
        return false;
      }
      if (expr.hasBodyArguments || !expr.bodyArguments.empty()) {
        error_ = "buffer_load does not accept block arguments";
        return false;
      }
      if (!validateExpr(params, locals, expr.args[0]) || !validateExpr(params, locals, expr.args[1])) {
        return false;
      }
      std::string elemType;
      if (!resolveBufferElemType(expr.args[0], elemType)) {
        error_ = "buffer_load requires Buffer input";
        return false;
      }
      ReturnKind elemKind = returnKindForTypeName(elemType);
      if (!isNumericOrBoolKind(elemKind)) {
        error_ = "buffer_load requires numeric/bool element type";
        return false;
      }
      ReturnKind indexKind = inferExprReturnKind(expr.args[1], params, locals);
      if (!isIntegerKind(indexKind)) {
        error_ = "buffer_load requires integer index";
        return false;
      }
      return true;
    }
    if (isSimpleCallName(expr, "buffer_store")) {
      if (!currentDefinitionIsCompute_) {
        error_ = "buffer_store requires a compute definition";
        return false;
      }
      if (!expr.templateArgs.empty()) {
        error_ = "buffer_store does not accept template arguments";
        return false;
      }
      if (expr.args.size() != 3) {
        error_ = "buffer_store requires buffer, index, and value arguments";
        return false;
      }
      if (expr.hasBodyArguments || !expr.bodyArguments.empty()) {
        error_ = "buffer_store does not accept block arguments";
        return false;
      }
      if (!validateExpr(params, locals, expr.args[0]) || !validateExpr(params, locals, expr.args[1]) ||
          !validateExpr(params, locals, expr.args[2])) {
        return false;
      }
      std::string elemType;
      if (!resolveBufferElemType(expr.args[0], elemType)) {
        error_ = "buffer_store requires Buffer input";
        return false;
      }
      ReturnKind elemKind = returnKindForTypeName(elemType);
      if (!isNumericOrBoolKind(elemKind)) {
        error_ = "buffer_store requires numeric/bool element type";
        return false;
      }
      ReturnKind indexKind = inferExprReturnKind(expr.args[1], params, locals);
      if (!isIntegerKind(indexKind)) {
        error_ = "buffer_store requires integer index";
        return false;
      }
      ReturnKind valueKind = inferExprReturnKind(expr.args[2], params, locals);
      if (valueKind != ReturnKind::Unknown && valueKind != elemKind) {
        error_ = "buffer_store value type mismatch";
        return false;
      }
      return true;
    }
      if (resolvedMethod && (resolved == "/array/count" || resolved == "/vector/count" || resolved == "/string/count" ||
                             resolved == "/map/count")) {
        if (!expr.templateArgs.empty()) {
          error_ = "count does not accept template arguments";
          return false;
        }
        if (expr.hasBodyArguments || !expr.bodyArguments.empty()) {
          error_ = "count does not accept block arguments";
          return false;
        }
        if (expr.args.size() != 1) {
          error_ = "argument count mismatch for builtin count";
          return false;
        }
        if (!validateExpr(params, locals, expr.args.front())) {
          return false;
        }
        return true;
      }
      if (!resolvedMethod && expr.name == "count" && it == defMap_.end()) {
        if (!expr.templateArgs.empty()) {
          error_ = "count does not accept template arguments";
          return false;
        }
        if (expr.hasBodyArguments || !expr.bodyArguments.empty()) {
          error_ = "count does not accept block arguments";
          return false;
        }
        if (expr.args.size() != 1) {
          error_ = "argument count mismatch for builtin count";
          return false;
        }
        if (!validateExpr(params, locals, expr.args.front())) {
          return false;
        }
        return true;
      }
      if (resolvedMethod && resolved == "/vector/capacity") {
        if (!expr.templateArgs.empty()) {
          error_ = "capacity does not accept template arguments";
          return false;
        }
        if (expr.hasBodyArguments || !expr.bodyArguments.empty()) {
          error_ = "capacity does not accept block arguments";
          return false;
        }
        if (expr.args.size() != 1) {
          error_ = "argument count mismatch for builtin capacity";
          return false;
        }
        std::string elemType;
        if (!resolveVectorTarget(expr.args.front(), elemType)) {
          error_ = "capacity requires vector target";
          return false;
        }
        if (!validateExpr(params, locals, expr.args.front())) {
          return false;
        }
        return true;
      }
      if (!resolvedMethod && expr.name == "capacity" && it == defMap_.end()) {
        if (!expr.templateArgs.empty()) {
          error_ = "capacity does not accept template arguments";
          return false;
        }
        if (expr.hasBodyArguments || !expr.bodyArguments.empty()) {
          error_ = "capacity does not accept block arguments";
          return false;
        }
        if (expr.args.size() != 1) {
          error_ = "argument count mismatch for builtin capacity";
          return false;
        }
        std::string elemType;
        if (!resolveVectorTarget(expr.args.front(), elemType)) {
          error_ = "capacity requires vector target";
          return false;
        }
        if (!validateExpr(params, locals, expr.args.front())) {
          return false;
        }
        return true;
      }
      std::string builtinName;
      if (getBuiltinOperatorName(expr, builtinName)) {
        size_t expectedArgs = builtinName == "negate" ? 1 : 2;
        if (expr.args.size() != expectedArgs) {
          error_ = "argument count mismatch for builtin " + builtinName;
          return false;
        }
        const bool isPlusMinus = builtinName == "plus" || builtinName == "minus";
        bool leftPointer = false;
        bool rightPointer = false;
        if (expr.args.size() == 2) {
          leftPointer = isPointerExpr(expr.args[0], params, locals);
          rightPointer = isPointerExpr(expr.args[1], params, locals);
        }
        if (isPlusMinus && expr.args.size() == 2 && (leftPointer || rightPointer)) {
          if (leftPointer && rightPointer) {
            error_ = "pointer arithmetic does not support pointer + pointer";
            return false;
          }
          if (rightPointer) {
            error_ = "pointer arithmetic requires pointer on the left";
            return false;
          }
          const Expr &offsetExpr = expr.args[1];
          ReturnKind offsetKind = inferExprReturnKind(offsetExpr, params, locals);
          if (offsetKind != ReturnKind::Unknown && offsetKind != ReturnKind::Int &&
              offsetKind != ReturnKind::Int64 && offsetKind != ReturnKind::UInt64) {
            error_ = "pointer arithmetic requires integer offset";
            return false;
          }
        } else if (leftPointer || rightPointer) {
          error_ = "arithmetic operators require numeric operands in " + currentDefinitionPath_;
          return false;
        }
        if (builtinName == "negate") {
          if (!isNumericExpr(expr.args.front())) {
            error_ = "arithmetic operators require numeric operands in " + currentDefinitionPath_;
            return false;
          }
          if (isUnsignedExpr(expr.args.front())) {
            error_ = "negate does not support unsigned operands";
            return false;
          }
        } else {
          if (!leftPointer && !rightPointer) {
            if (!isNumericExpr(expr.args[0]) || !isNumericExpr(expr.args[1])) {
              error_ = "arithmetic operators require numeric operands in " + currentDefinitionPath_;
              return false;
            }
            if (hasMixedNumericDomain(expr.args)) {
              error_ = "arithmetic operators do not support mixed software/fixed numeric operands";
              return false;
            }
            if (hasMixedComplexNumeric(expr.args)) {
              error_ = "arithmetic operators do not support mixed complex/real operands";
              return false;
            }
            if (hasMixedSignedness(expr.args, false)) {
              error_ = "arithmetic operators do not support mixed signed/unsigned operands";
              return false;
            }
            if (hasMixedNumericCategory(expr.args)) {
              error_ = "arithmetic operators do not support mixed int/float operands";
              return false;
            }
          } else if (leftPointer) {
            if (!isNumericExpr(expr.args[1])) {
              error_ = "arithmetic operators require numeric operands in " + currentDefinitionPath_;
              return false;
            }
          }
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
          error_ = "argument count mismatch for builtin " + builtinName;
          return false;
        }
        const bool isBooleanOp = builtinName == "and" || builtinName == "or" || builtinName == "not";
        if (isBooleanOp) {
          for (const auto &arg : expr.args) {
            if (!isBoolExpr(arg, params, locals)) {
              error_ = "boolean operators require bool operands";
              return false;
            }
          }
        } else {
          bool sawString = false;
          bool sawNonString = false;
          for (const auto &arg : expr.args) {
            if (isStringExpr(arg)) {
              sawString = true;
            } else {
              sawNonString = true;
            }
          }
          if (sawString) {
            if (sawNonString) {
              error_ = "comparisons do not support mixed string/numeric operands";
              return false;
            }
          } else {
            for (const auto &arg : expr.args) {
              if (!isComparisonOperand(arg)) {
                error_ = "comparisons require numeric, bool, or string operands";
                return false;
              }
            }
            if (hasMixedNumericDomain(expr.args)) {
              error_ = "comparisons do not support mixed software/fixed numeric operands";
              return false;
            }
            if (hasMixedComplexNumeric(expr.args)) {
              error_ = "comparisons do not support mixed complex/real operands";
              return false;
            }
            if (builtinName != "equal" && builtinName != "not_equal") {
              if (hasComplexNumeric(expr.args)) {
                error_ = "comparisons do not support ordered complex operands";
                return false;
              }
            }
            if (hasMixedSignedness(expr.args, true)) {
              error_ = "comparisons do not support mixed signed/unsigned operands";
              return false;
            }
            if (hasMixedNumericCategory(expr.args)) {
              error_ = "comparisons do not support mixed int/float operands";
              return false;
            }
          }
        }
        for (const auto &arg : expr.args) {
          if (!validateExpr(params, locals, arg)) {
            return false;
          }
        }
        return true;
      }
      if (getBuiltinClampName(expr, builtinName, allowMathBareName(expr.name))) {
        if (expr.args.size() != 3) {
          error_ = "argument count mismatch for builtin " + builtinName;
          return false;
        }
        for (const auto &arg : expr.args) {
          if (!isNumericExpr(arg)) {
            error_ = "clamp requires numeric operands";
            return false;
          }
        }
        if (hasMixedNumericDomain(expr.args)) {
          error_ = "clamp does not support mixed software/fixed numeric operands";
          return false;
        }
        if (hasComplexNumeric(expr.args)) {
          error_ = "clamp does not support complex operands";
          return false;
        }
        if (hasMixedSignedness(expr.args, false)) {
          error_ = "clamp does not support mixed signed/unsigned operands";
          return false;
        }
        if (hasMixedNumericCategory(expr.args)) {
          error_ = "clamp does not support mixed int/float operands";
          return false;
        }
        for (const auto &arg : expr.args) {
          if (!validateExpr(params, locals, arg)) {
            return false;
          }
        }
        return true;
      }
      if (getBuiltinMinMaxName(expr, builtinName, allowMathBareName(expr.name))) {
        if (expr.args.size() != 2) {
          error_ = "argument count mismatch for builtin " + builtinName;
          return false;
        }
        for (const auto &arg : expr.args) {
          if (!isNumericExpr(arg)) {
            error_ = builtinName + " requires numeric operands";
            return false;
          }
        }
        if (hasMixedNumericDomain(expr.args)) {
          error_ = builtinName + " does not support mixed software/fixed numeric operands";
          return false;
        }
        if (hasComplexNumeric(expr.args)) {
          error_ = builtinName + " does not support complex operands";
          return false;
        }
        if (hasMixedSignedness(expr.args, false)) {
          error_ = builtinName + " does not support mixed signed/unsigned operands";
          return false;
        }
        if (hasMixedNumericCategory(expr.args)) {
          error_ = builtinName + " does not support mixed int/float operands";
          return false;
        }
        for (const auto &arg : expr.args) {
          if (!validateExpr(params, locals, arg)) {
            return false;
          }
        }
        return true;
      }
      if (getBuiltinAbsSignName(expr, builtinName, allowMathBareName(expr.name))) {
        if (expr.args.size() != 1) {
          error_ = "argument count mismatch for builtin " + builtinName;
          return false;
        }
        if (!isNumericExpr(expr.args.front())) {
          error_ = builtinName + " requires numeric operand";
          return false;
        }
        if (builtinName == "sign" && hasComplexNumeric(expr.args)) {
          error_ = "sign does not support complex operands";
          return false;
        }
        if (!validateExpr(params, locals, expr.args.front())) {
          return false;
        }
        return true;
      }
      if (getBuiltinSaturateName(expr, builtinName, allowMathBareName(expr.name))) {
        if (expr.args.size() != 1) {
          error_ = "argument count mismatch for builtin " + builtinName;
          return false;
        }
        if (!isNumericExpr(expr.args.front())) {
          error_ = builtinName + " requires numeric operand";
          return false;
        }
        if (hasComplexNumeric(expr.args)) {
          error_ = builtinName + " does not support complex operands";
          return false;
        }
        if (!validateExpr(params, locals, expr.args.front())) {
          return false;
        }
        return true;
      }
      if (getBuiltinMathName(expr, builtinName, allowMathBareName(expr.name))) {
        if (!expr.templateArgs.empty()) {
          error_ = builtinName + " does not accept template arguments";
          return false;
        }
        if (expr.hasBodyArguments || !expr.bodyArguments.empty()) {
          error_ = builtinName + " does not accept block arguments";
          return false;
        }
        size_t expectedArgs = 1;
        if (builtinName == "lerp" || builtinName == "fma") {
          expectedArgs = 3;
        } else if (builtinName == "pow" || builtinName == "atan2" || builtinName == "hypot" ||
                   builtinName == "copysign") {
          expectedArgs = 2;
        }
        if (expr.args.size() != expectedArgs) {
          error_ = "argument count mismatch for builtin " + builtinName;
          return false;
        }
        auto validateFloatArgs = [&](size_t count) -> bool {
          for (size_t i = 0; i < count; ++i) {
            if (!isFloatExpr(expr.args[i])) {
              error_ = builtinName + " requires float operands";
              return false;
            }
          }
          return true;
        };
        if (builtinName == "lerp") {
          for (const auto &arg : expr.args) {
            if (!isNumericExpr(arg)) {
              error_ = builtinName + " requires numeric operands";
              return false;
            }
          }
          if (hasMixedNumericDomain(expr.args)) {
            error_ = builtinName + " does not support mixed software/fixed numeric operands";
            return false;
          }
          if (hasMixedComplexNumeric(expr.args)) {
            error_ = builtinName + " does not support mixed complex/real operands";
            return false;
          }
          if (hasMixedSignedness(expr.args, false)) {
            error_ = builtinName + " does not support mixed signed/unsigned operands";
            return false;
          }
          if (hasMixedNumericCategory(expr.args)) {
            error_ = builtinName + " does not support mixed int/float operands";
            return false;
          }
        } else if (builtinName == "pow") {
          for (const auto &arg : expr.args) {
            if (!isNumericExpr(arg)) {
              error_ = builtinName + " requires numeric operands";
              return false;
            }
          }
          if (hasMixedNumericDomain(expr.args)) {
            error_ = builtinName + " does not support mixed software/fixed numeric operands";
            return false;
          }
          if (hasMixedComplexNumeric(expr.args)) {
            error_ = builtinName + " does not support mixed complex/real operands";
            return false;
          }
          if (hasMixedSignedness(expr.args, false)) {
            error_ = builtinName + " does not support mixed signed/unsigned operands";
            return false;
          }
          if (hasMixedNumericCategory(expr.args)) {
            error_ = builtinName + " does not support mixed int/float operands";
            return false;
          }
        } else if (builtinName == "is_nan" || builtinName == "is_inf" || builtinName == "is_finite") {
          if (!validateFloatArgs(1)) {
            return false;
          }
        } else if (builtinName == "fma") {
          if (!validateFloatArgs(3)) {
            return false;
          }
        } else if (builtinName == "atan2" || builtinName == "hypot" || builtinName == "copysign") {
          if (!validateFloatArgs(2)) {
            return false;
          }
        } else {
          if (!validateFloatArgs(1)) {
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
      if (getBuiltinArrayAccessName(expr, builtinName)) {
        if (!expr.templateArgs.empty()) {
          error_ = builtinName + " does not accept template arguments";
          return false;
        }
        if (expr.hasBodyArguments || !expr.bodyArguments.empty()) {
          error_ = builtinName + " does not accept block arguments";
          return false;
        }
        if (expr.args.size() != 2) {
          error_ = "argument count mismatch for builtin " + builtinName;
          return false;
        }
        std::string elemType;
        bool isArrayOrString = resolveArrayTarget(expr.args.front(), elemType) || resolveStringTarget(expr.args.front());
        std::string mapKeyType;
        bool isMap = resolveMapKeyType(expr.args.front(), mapKeyType);
        if (!isArrayOrString && !isMap) {
          error_ = builtinName + " requires array, vector, map, or string target";
          return false;
        }
        if (!isMap) {
          if (!isIntegerOrBoolExpr(expr.args[1], params, locals)) {
            error_ = builtinName + " requires integer index";
            return false;
          }
        } else if (!mapKeyType.empty()) {
          if (normalizeBindingTypeName(mapKeyType) == "string") {
            if (!isStringExpr(expr.args[1])) {
              error_ = builtinName + " requires string map key";
              return false;
            }
          } else {
            ReturnKind keyKind = returnKindForTypeName(normalizeBindingTypeName(mapKeyType));
            if (keyKind != ReturnKind::Unknown) {
              if (resolveStringTarget(expr.args[1])) {
                error_ = builtinName + " requires map key type " + mapKeyType;
                return false;
              }
              ReturnKind indexKind = inferExprReturnKind(expr.args[1], params, locals);
              if (indexKind != ReturnKind::Unknown && indexKind != keyKind) {
                error_ = builtinName + " requires map key type " + mapKeyType;
                return false;
              }
            }
          }
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
          error_ = "convert requires exactly one template argument";
          return false;
        }
        const std::string &typeName = expr.templateArgs[0];
        if (typeName != "int" && typeName != "i32" && typeName != "i64" && typeName != "u64" &&
            typeName != "bool" && typeName != "float" && typeName != "f32" && typeName != "f64") {
          error_ = "unsupported convert target type: " + typeName;
          return false;
        }
        if (expr.args.size() != 1) {
          error_ = "argument count mismatch for builtin " + builtinName;
          return false;
        }
        if (!validateExpr(params, locals, expr.args.front())) {
          return false;
        }
        if (!isConvertibleExpr(expr.args.front())) {
          error_ = "convert requires numeric or bool operand";
          return false;
        }
        return true;
      }
      PrintBuiltin printBuiltin;
      if (getPrintBuiltin(expr, printBuiltin)) {
        error_ = printBuiltin.name + " is only supported as a statement";
        return false;
      }
      if (getBuiltinPointerName(expr, builtinName)) {
        if (!expr.templateArgs.empty()) {
          error_ = "pointer helpers do not accept template arguments";
          return false;
        }
        if (expr.hasBodyArguments || !expr.bodyArguments.empty()) {
          error_ = "pointer helpers do not accept block arguments";
          return false;
        }
        if (expr.args.size() != 1) {
          error_ = "argument count mismatch for builtin " + builtinName;
          return false;
        }
        if (builtinName == "location") {
          const Expr &target = expr.args.front();
          if (target.kind != Expr::Kind::Name) {
            error_ = "location requires a local binding";
            return false;
          }
          if (isEntryArgsName(target.name)) {
            error_ = "location requires a local binding";
            return false;
          }
          if (locals.count(target.name) == 0 && !isParam(params, target.name)) {
            error_ = "location requires a local binding";
            return false;
          }
        }
        if (builtinName == "dereference") {
          if (!isPointerLikeExpr(expr.args.front(), params, locals)) {
            error_ = "dereference requires a pointer or reference";
            return false;
          }
        }
        if (!validateExpr(params, locals, expr.args.front())) {
          return false;
        }
        return true;
      }
      if (getBuiltinCollectionName(expr, builtinName)) {
        if (expr.hasBodyArguments || !expr.bodyArguments.empty()) {
          error_ = builtinName + " literal does not accept block arguments";
          return false;
        }
        if (builtinName == "vector" && !expr.args.empty()) {
          if (activeEffects_.count("heap_alloc") == 0) {
            error_ = "vector literal requires heap_alloc effect";
            return false;
          }
        }
        if (builtinName == "array" || builtinName == "vector") {
          if (expr.templateArgs.size() != 1) {
            if (builtinName == "array" && expr.templateArgs.size() > 1) {
              error_ = "array<T, N> is unsupported; use array<T> (runtime-count array)";
              return false;
            }
            error_ = builtinName + " literal requires exactly one template argument";
            return false;
          }
        } else {
          if (expr.templateArgs.size() != 2) {
            error_ = "map literal requires exactly two template arguments";
            return false;
          }
          if (expr.args.size() % 2 != 0) {
            error_ = "map literal requires an even number of arguments";
            return false;
          }
        }
        for (const auto &arg : expr.args) {
          if (!validateExpr(params, locals, arg)) {
            return false;
          }
        }
        if ((builtinName == "array" || builtinName == "vector") && !expr.templateArgs.empty()) {
          const std::string &elemType = expr.templateArgs.front();
          for (const auto &arg : expr.args) {
            if (!validateCollectionElementType(arg, elemType, builtinName + " literal requires element type ")) {
              return false;
            }
          }
        }
        if (builtinName == "map" && expr.templateArgs.size() == 2) {
          const std::string &keyType = expr.templateArgs[0];
          const std::string &valueType = expr.templateArgs[1];
          for (size_t i = 0; i + 1 < expr.args.size(); i += 2) {
            if (!validateCollectionElementType(expr.args[i], keyType, "map literal requires key type ")) {
              return false;
            }
            if (!validateCollectionElementType(expr.args[i + 1], valueType, "map literal requires value type ")) {
              return false;
            }
          }
        }
        return true;
      }
      auto hasActiveBorrowForBinding = [&](const std::string &name,
                                           const std::string &ignoreBorrowName = std::string()) -> bool {
        if (currentDefinitionIsUnsafe_) {
          return false;
        }
        auto referenceRootForBinding = [](const std::string &bindingName, const BindingInfo &binding) -> std::string {
          if (binding.typeName != "Reference") {
            return "";
          }
          if (!binding.referenceRoot.empty()) {
            return binding.referenceRoot;
          }
          return bindingName;
        };
        auto hasBorrowFrom = [&](const std::string &borrowName, const BindingInfo &binding) -> bool {
          if (borrowName == name) {
            return false;
          }
          if (!ignoreBorrowName.empty() && borrowName == ignoreBorrowName) {
            return false;
          }
          if (endedReferenceBorrows_.count(borrowName) > 0) {
            return false;
          }
          const std::string root = referenceRootForBinding(borrowName, binding);
          return !root.empty() && root == name;
        };
        for (const auto &param : params) {
          if (hasBorrowFrom(param.name, param.binding)) {
            return true;
          }
        }
        for (const auto &entry : locals) {
          if (hasBorrowFrom(entry.first, entry.second)) {
            return true;
          }
        }
        return false;
      };
      auto formatBorrowedBindingError = [&](const std::string &borrowRoot, const std::string &sinkName) {
        const std::string sink = sinkName.empty() ? borrowRoot : sinkName;
        error_ = "borrowed binding: " + borrowRoot + " (root: " + borrowRoot + ", sink: " + sink + ")";
      };
      auto findNamedBinding = [&](const std::string &name) -> const BindingInfo * {
        if (const BindingInfo *paramBinding = findParamBinding(params, name)) {
          return paramBinding;
        }
        auto itBinding = locals.find(name);
        if (itBinding != locals.end()) {
          return &itBinding->second;
        }
        return nullptr;
      };
      auto resolveReferenceEscapeSink = [&](const std::string &targetName, std::string &sinkOut) -> bool {
        sinkOut.clear();
        if (const BindingInfo *targetParam = findParamBinding(params, targetName)) {
          if (targetParam->typeName == "Reference") {
            sinkOut = targetName;
            return true;
          }
          return false;
        }
        auto targetIt = locals.find(targetName);
        if (targetIt == locals.end() || targetIt->second.typeName != "Reference" ||
            targetIt->second.referenceRoot.empty()) {
          return false;
        }
        if (const BindingInfo *rootParam = findParamBinding(params, targetIt->second.referenceRoot)) {
          if (rootParam->typeName == "Reference") {
            sinkOut = targetIt->second.referenceRoot;
            return true;
          }
        }
        return false;
      };
      std::function<bool(const Expr &, std::string &)> resolveLocationRootBindingName;
      resolveLocationRootBindingName = [&](const Expr &pointerExpr, std::string &rootNameOut) -> bool {
        if (pointerExpr.kind != Expr::Kind::Call) {
          return false;
        }
        std::string pointerBuiltinName;
        if (getBuiltinPointerName(pointerExpr, pointerBuiltinName) && pointerBuiltinName == "location" &&
            pointerExpr.args.size() == 1) {
          if (pointerExpr.args.front().kind != Expr::Kind::Name) {
            return false;
          }
          rootNameOut = pointerExpr.args.front().name;
          return true;
        }
        std::string opName;
        if (!getBuiltinOperatorName(pointerExpr, opName) || (opName != "plus" && opName != "minus") ||
            pointerExpr.args.size() != 2) {
          return false;
        }
        if (isPointerLikeExpr(pointerExpr.args[1], params, locals)) {
          return false;
        }
        return resolveLocationRootBindingName(pointerExpr.args.front(), rootNameOut);
      };
      std::function<bool(const Expr &, std::string &, std::string &)> resolveMutablePointerWriteTarget;
      resolveMutablePointerWriteTarget =
          [&](const Expr &pointerExpr, std::string &borrowRootOut, std::string &ignoreBorrowNameOut) -> bool {
        if (pointerExpr.kind == Expr::Kind::Name) {
          if (!isMutableBinding(params, locals, pointerExpr.name)) {
            return false;
          }
          const BindingInfo *pointerBinding = findNamedBinding(pointerExpr.name);
          if (!pointerBinding || !isPointerLikeExpr(pointerExpr, params, locals)) {
            return false;
          }
          ignoreBorrowNameOut = pointerExpr.name;
          if (!pointerBinding->referenceRoot.empty()) {
            borrowRootOut = pointerBinding->referenceRoot;
          }
          return true;
        }
        if (pointerExpr.kind != Expr::Kind::Call) {
          return false;
        }
        std::string pointerBuiltinName;
        if (getBuiltinPointerName(pointerExpr, pointerBuiltinName) && pointerBuiltinName == "location" &&
            pointerExpr.args.size() == 1) {
          const Expr &locationTarget = pointerExpr.args.front();
          if (locationTarget.kind != Expr::Kind::Name || !isMutableBinding(params, locals, locationTarget.name)) {
            return false;
          }
          const BindingInfo *targetBinding = findNamedBinding(locationTarget.name);
          if (targetBinding == nullptr) {
            return false;
          }
          if (targetBinding->typeName == "Reference") {
            ignoreBorrowNameOut = locationTarget.name;
            if (!targetBinding->referenceRoot.empty()) {
              borrowRootOut = targetBinding->referenceRoot;
            } else {
              borrowRootOut = locationTarget.name;
            }
          } else {
            borrowRootOut = locationTarget.name;
          }
          return true;
        }
        std::string opName;
        if (!getBuiltinOperatorName(pointerExpr, opName) || (opName != "plus" && opName != "minus") ||
            pointerExpr.args.size() != 2) {
          return false;
        }
        if (isPointerLikeExpr(pointerExpr.args[1], params, locals)) {
          return false;
        }
        return resolveMutablePointerWriteTarget(pointerExpr.args.front(), borrowRootOut, ignoreBorrowNameOut);
      };
      if (isSimpleCallName(expr, "move")) {
        if (hasNamedArguments(expr.argNames)) {
          error_ = "named arguments not supported for builtin calls";
          return false;
        }
        if (expr.isMethodCall) {
          error_ = "move does not support method-call syntax";
          return false;
        }
        if (!expr.templateArgs.empty()) {
          error_ = "move does not accept template arguments";
          return false;
        }
        if (expr.hasBodyArguments || !expr.bodyArguments.empty()) {
          error_ = "move does not accept block arguments";
          return false;
        }
        if (expr.args.size() != 1) {
          error_ = "move requires exactly one argument";
          return false;
        }
        const Expr &target = expr.args.front();
        if (target.kind != Expr::Kind::Name) {
          error_ = "move requires a binding name";
          return false;
        }
        const BindingInfo *binding = findParamBinding(params, target.name);
        if (!binding) {
          auto it = locals.find(target.name);
          if (it != locals.end()) {
            binding = &it->second;
          }
        }
        if (!binding) {
          error_ = "move requires a local binding or parameter: " + target.name;
          return false;
        }
        if (binding->typeName == "Reference") {
          error_ = "move does not support Reference bindings: " + target.name;
          return false;
        }
        if (hasActiveBorrowForBinding(target.name)) {
          formatBorrowedBindingError(target.name, target.name);
          return false;
        }
        if (movedBindings_.count(target.name) > 0) {
          error_ = "use-after-move: " + target.name;
          return false;
        }
        movedBindings_.insert(target.name);
        return true;
      }
      if (isAssignCall(expr)) {
        if (expr.args.size() != 2) {
          error_ = "assign requires exactly two arguments";
          return false;
        }
        const Expr &target = expr.args.front();
        const bool targetIsName = target.kind == Expr::Kind::Name;
        if (target.kind == Expr::Kind::Name) {
          if (!isMutableBinding(params, locals, target.name)) {
            error_ = "assign target must be a mutable binding: " + target.name;
            return false;
          }
          if (hasActiveBorrowForBinding(target.name)) {
            formatBorrowedBindingError(target.name, target.name);
            return false;
          }
        } else if (target.kind == Expr::Kind::Call) {
          std::string pointerName;
          if (!getBuiltinPointerName(target, pointerName) || pointerName != "dereference" || target.args.size() != 1) {
            error_ = "assign target must be a mutable binding";
            return false;
          }
          const Expr &pointerExpr = target.args.front();
          std::string pointerBorrowRoot;
          std::string ignoreBorrowName;
          if (!resolveMutablePointerWriteTarget(pointerExpr, pointerBorrowRoot, ignoreBorrowName)) {
            if (pointerExpr.kind == Expr::Kind::Name && !isMutableBinding(params, locals, pointerExpr.name)) {
              error_ = "assign target must be a mutable binding";
              return false;
            }
            std::string locationRootName;
            if (resolveLocationRootBindingName(pointerExpr, locationRootName) &&
                !isMutableBinding(params, locals, locationRootName)) {
              error_ = "assign target must be a mutable binding: " + locationRootName;
              return false;
            }
            error_ = "assign target must be a mutable pointer binding";
            return false;
          }
          if (!pointerBorrowRoot.empty() && hasActiveBorrowForBinding(pointerBorrowRoot, ignoreBorrowName)) {
            const std::string borrowSink = !ignoreBorrowName.empty() ? ignoreBorrowName : pointerBorrowRoot;
            formatBorrowedBindingError(pointerBorrowRoot, borrowSink);
            return false;
          }
          if (currentDefinitionIsUnsafe_ && isUnsafeReferenceExpr(expr.args[1])) {
            std::string escapeSink;
            bool hasEscapeSink = false;
            if (pointerExpr.kind == Expr::Kind::Name) {
              hasEscapeSink = resolveReferenceEscapeSink(pointerExpr.name, escapeSink);
            }
            if (!hasEscapeSink) {
              std::string locationRootName;
              if (resolveLocationRootBindingName(pointerExpr, locationRootName)) {
                hasEscapeSink = resolveReferenceEscapeSink(locationRootName, escapeSink);
              }
            }
            if (!hasEscapeSink && !pointerBorrowRoot.empty()) {
              if (const BindingInfo *rootParam = findParamBinding(params, pointerBorrowRoot);
                  rootParam != nullptr && rootParam->typeName == "Reference") {
                hasEscapeSink = true;
                escapeSink = pointerBorrowRoot;
              }
            }
            if (hasEscapeSink) {
              if (reportReferenceAssignmentEscape(escapeSink, expr.args[1])) {
                return false;
              }
            }
          }
          if (!currentDefinitionIsUnsafe_) {
            std::string escapeSink;
            bool hasEscapeSink = false;
            if (pointerExpr.kind == Expr::Kind::Name) {
              hasEscapeSink = resolveReferenceEscapeSink(pointerExpr.name, escapeSink);
            }
            if (!hasEscapeSink) {
              std::string locationRootName;
              if (resolveLocationRootBindingName(pointerExpr, locationRootName)) {
                hasEscapeSink = resolveReferenceEscapeSink(locationRootName, escapeSink);
              }
            }
            if (!hasEscapeSink && !pointerBorrowRoot.empty()) {
              if (const BindingInfo *rootParam = findParamBinding(params, pointerBorrowRoot);
                  rootParam != nullptr && rootParam->typeName == "Reference") {
                hasEscapeSink = true;
                escapeSink = pointerBorrowRoot;
              }
            }
            if (hasEscapeSink && reportReferenceAssignmentEscape(escapeSink, expr.args[1])) {
              return false;
            }
          }
          if (!validateExpr(params, locals, pointerExpr)) {
            return false;
          }
        } else {
          error_ = "assign target must be a mutable binding";
          return false;
        }
        if (currentDefinitionIsUnsafe_ && targetIsName && isUnsafeReferenceExpr(expr.args[1])) {
          std::string escapeSink;
          if (resolveReferenceEscapeSink(target.name, escapeSink)) {
            if (reportReferenceAssignmentEscape(escapeSink, expr.args[1])) {
              return false;
            }
          }
        }
        if (!currentDefinitionIsUnsafe_ && targetIsName) {
          std::string escapeSink;
          if (resolveReferenceEscapeSink(target.name, escapeSink) &&
              reportReferenceAssignmentEscape(escapeSink, expr.args[1])) {
            return false;
          }
        }
        if (!validateExpr(params, locals, expr.args[1])) {
          return false;
        }
        if (targetIsName) {
          movedBindings_.erase(target.name);
        }
        return true;
      }
      std::string mutateName;
      if (getBuiltinMutationName(expr, mutateName)) {
        if (expr.args.size() != 1) {
          error_ = mutateName + " requires exactly one argument";
          return false;
        }
        const Expr &target = expr.args.front();
        if (target.kind == Expr::Kind::Name) {
          if (!isMutableBinding(params, locals, target.name)) {
            error_ = mutateName + " target must be a mutable binding: " + target.name;
            return false;
          }
          if (hasActiveBorrowForBinding(target.name)) {
            formatBorrowedBindingError(target.name, target.name);
            return false;
          }
        } else if (target.kind == Expr::Kind::Call) {
          std::string pointerName;
          if (!getBuiltinPointerName(target, pointerName) || pointerName != "dereference" || target.args.size() != 1) {
            error_ = mutateName + " target must be a mutable binding";
            return false;
          }
          const Expr &pointerExpr = target.args.front();
          std::string pointerBorrowRoot;
          std::string ignoreBorrowName;
          if (!resolveMutablePointerWriteTarget(pointerExpr, pointerBorrowRoot, ignoreBorrowName)) {
            if (pointerExpr.kind == Expr::Kind::Name && !isMutableBinding(params, locals, pointerExpr.name)) {
              error_ = mutateName + " target must be a mutable binding";
              return false;
            }
            std::string locationRootName;
            if (resolveLocationRootBindingName(pointerExpr, locationRootName) &&
                !isMutableBinding(params, locals, locationRootName)) {
              error_ = mutateName + " target must be a mutable binding: " + locationRootName;
              return false;
            }
            error_ = mutateName + " target must be a mutable pointer binding";
            return false;
          }
          if (!pointerBorrowRoot.empty() && hasActiveBorrowForBinding(pointerBorrowRoot, ignoreBorrowName)) {
            const std::string borrowSink = !ignoreBorrowName.empty() ? ignoreBorrowName : pointerBorrowRoot;
            formatBorrowedBindingError(pointerBorrowRoot, borrowSink);
            return false;
          }
          if (!validateExpr(params, locals, pointerExpr)) {
            return false;
          }
        } else {
          error_ = mutateName + " target must be a mutable binding";
          return false;
        }
        if (!isNumericExpr(target)) {
          error_ = mutateName + " requires numeric operand";
          return false;
        }
        return true;
      }
      if (!allowMathBareName(expr.name) && expr.name.find('/') == std::string::npos) {
        std::string builtinName;
        if (getBuiltinClampName(expr, builtinName, true) || getBuiltinMinMaxName(expr, builtinName, true) ||
            getBuiltinAbsSignName(expr, builtinName, true) || getBuiltinSaturateName(expr, builtinName, true) ||
            getBuiltinMathName(expr, builtinName, true)) {
          error_ = "math builtin requires import /std/math/* or /std/math/<name>: " + expr.name;
          return false;
        }
      }
      error_ = "unknown call target: " + expr.name;
      return false;
    }
    const auto &calleeParams = paramsByDef_[resolved];
    if (calleeParams.empty() && structNames_.count(resolved) > 0) {
      std::vector<ParameterInfo> fieldParams;
      fieldParams.reserve(it->second->statements.size());
      auto isStaticField = [](const Expr &stmt) -> bool {
        for (const auto &transform : stmt.transforms) {
          if (transform.name == "static") {
            return true;
          }
        }
        return false;
      };
      bool hasMissingDefaults = false;
      for (const auto &stmt : it->second->statements) {
        if (!stmt.isBinding) {
          error_ = "struct definitions may only contain field bindings: " + resolved;
          return false;
        }
        if (isStaticField(stmt)) {
          continue;
        }
        ParameterInfo field;
        field.name = stmt.name;
        if (stmt.args.size() == 1) {
          field.defaultExpr = &stmt.args.front();
        } else {
          hasMissingDefaults = true;
        }
        fieldParams.push_back(field);
      }
      if (hasMissingDefaults && expr.args.empty() && !hasNamedArguments(expr.argNames)) {
        if (hasStructZeroArgConstructor(resolved)) {
          return true;
        }
      }
      if (!validateNamedArgumentsAgainstParams(fieldParams, expr.argNames, error_)) {
        return false;
      }
      std::vector<const Expr *> orderedArgs;
      std::string orderError;
      if (!buildOrderedArguments(fieldParams, expr.args, expr.argNames, orderedArgs, orderError)) {
        if (orderError.find("argument count mismatch") != std::string::npos) {
          error_ = "argument count mismatch for " + resolved;
        } else {
          error_ = orderError;
        }
        return false;
      }
      std::unordered_set<const Expr *> explicitArgs;
      explicitArgs.reserve(expr.args.size());
      for (const auto &arg : expr.args) {
        explicitArgs.insert(&arg);
      }
      for (const auto *arg : orderedArgs) {
        if (!arg || explicitArgs.count(arg) == 0) {
          continue;
        }
        if (!validateExpr(params, locals, *arg)) {
          return false;
        }
      }
      return true;
    }
    if (!validateNamedArgumentsAgainstParams(calleeParams, expr.argNames, error_)) {
      return false;
    }
    std::vector<const Expr *> orderedArgs;
    std::string orderError;
    if (!buildOrderedArguments(calleeParams, expr.args, expr.argNames, orderedArgs, orderError)) {
      if (orderError.find("argument count mismatch") != std::string::npos) {
        error_ = "argument count mismatch for " + resolved;
      } else {
        error_ = orderError;
      }
      return false;
    }
    for (const auto *arg : orderedArgs) {
      if (!arg) {
        continue;
      }
      if (!validateExpr(params, locals, *arg)) {
        return false;
      }
    }
    bool calleeIsUnsafe = false;
    for (const auto &transform : it->second->transforms) {
      if (transform.name == "unsafe") {
        calleeIsUnsafe = true;
        break;
      }
    }
    if (currentDefinitionIsUnsafe_ && !calleeIsUnsafe) {
      for (size_t i = 0; i < orderedArgs.size() && i < calleeParams.size(); ++i) {
        const Expr *arg = orderedArgs[i];
        if (arg == nullptr) {
          continue;
        }
        if (calleeParams[i].binding.typeName != "Reference") {
          continue;
        }
        if (!isUnsafeReferenceExpr(*arg)) {
          continue;
        }
        error_ = "unsafe reference escapes across safe boundary to " + resolved;
        return false;
      }
    }
    return true;
  }
  return false;
}

} // namespace primec::semantics
