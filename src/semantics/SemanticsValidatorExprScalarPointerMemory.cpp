#include "SemanticsValidator.h"

#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace primec::semantics {
namespace {

ReturnKind normalizeMemoryIndexKind(ReturnKind kind) {
  if (kind == ReturnKind::Bool) {
    return ReturnKind::Int;
  }
  return kind;
}

bool isSupportedMemoryIndexKind(ReturnKind kind) {
  return kind == ReturnKind::Int || kind == ReturnKind::Int64 ||
         kind == ReturnKind::UInt64;
}

} // namespace

bool SemanticsValidator::validateExprScalarPointerMemoryBuiltins(
    const std::vector<ParameterInfo> &params,
    const std::unordered_map<std::string, BindingInfo> &locals,
    const Expr &expr,
    bool &handledOut) {
  handledOut = false;
  auto returnKindForBinding = [](const BindingInfo &binding) -> ReturnKind {
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
  auto isIntegerExpr = [&](const Expr &arg) -> bool {
    ReturnKind kind = inferExprReturnKind(arg, params, locals);
    if (kind == ReturnKind::Int || kind == ReturnKind::Int64 || kind == ReturnKind::UInt64) {
      return true;
    }
    if (kind == ReturnKind::Bool || kind == ReturnKind::Float32 || kind == ReturnKind::Float64 ||
        kind == ReturnKind::String || kind == ReturnKind::Void || kind == ReturnKind::Array) {
      return false;
    }
    if (kind == ReturnKind::Unknown) {
      if (arg.kind == Expr::Kind::FloatLiteral || arg.kind == Expr::Kind::StringLiteral ||
          arg.kind == Expr::Kind::BoolLiteral) {
        return false;
      }
      if (isPointerExpr(arg, params, locals)) {
        return false;
      }
      if (arg.kind == Expr::Kind::Name) {
        if (const BindingInfo *paramBinding = findParamBinding(params, arg.name)) {
          ReturnKind paramKind = returnKindForBinding(*paramBinding);
          return paramKind == ReturnKind::Int || paramKind == ReturnKind::Int64 ||
                 paramKind == ReturnKind::UInt64;
        }
        auto it = locals.find(arg.name);
        if (it != locals.end()) {
          ReturnKind localKind = returnKindForBinding(it->second);
          return localKind == ReturnKind::Int || localKind == ReturnKind::Int64 ||
                 localKind == ReturnKind::UInt64;
        }
      }
      return true;
    }
    return false;
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
        return paramKind == ReturnKind::Int || paramKind == ReturnKind::Int64 ||
               paramKind == ReturnKind::UInt64 || paramKind == ReturnKind::Float32 ||
               paramKind == ReturnKind::Float64 || paramKind == ReturnKind::Bool ||
               paramKind == ReturnKind::Integer || paramKind == ReturnKind::Decimal ||
               paramKind == ReturnKind::Complex;
      }
      auto it = locals.find(arg.name);
      if (it != locals.end()) {
        ReturnKind localKind = returnKindForBinding(it->second);
        return localKind == ReturnKind::Int || localKind == ReturnKind::Int64 ||
               localKind == ReturnKind::UInt64 || localKind == ReturnKind::Float32 ||
               localKind == ReturnKind::Float64 || localKind == ReturnKind::Bool ||
               localKind == ReturnKind::Integer || localKind == ReturnKind::Decimal ||
               localKind == ReturnKind::Complex;
      }
    }
    return true;
  };

  auto validateMemoryTargetType = [&](const std::string &targetType) -> bool {
    std::string targetBase;
    std::string targetArg;
    if (splitTemplateTypeName(targetType, targetBase, targetArg) &&
        normalizeBindingTypeName(targetBase) == "array") {
      error_ = "unsupported pointer target type: " + targetType;
      return false;
    }
    Expr bindingExpr;
    bindingExpr.kind = Expr::Kind::Call;
    bindingExpr.name = "__memory_target";
    bindingExpr.namespacePrefix = expr.namespacePrefix;
    Transform pointerTransform;
    pointerTransform.name = "Pointer";
    pointerTransform.templateArgs.push_back(targetType);
    bindingExpr.transforms.push_back(std::move(pointerTransform));
    BindingInfo bindingInfo;
    std::optional<std::string> restrictType;
    std::string parseError;
    if (!parseBindingInfo(bindingExpr, expr.namespacePrefix, structNames_,
                          importAliases_, bindingInfo, restrictType,
                          parseError)) {
      error_ = parseError;
      return false;
    }
    return true;
  };

  std::string builtinName;
  auto isExplicitMapAccessHelperPath = [&](const Expr &candidate) {
    if (candidate.kind != Expr::Kind::Call || candidate.isMethodCall) {
      return false;
    }
    const std::string resolved = resolveCalleePath(candidate);
    return resolved == "/map/at" || resolved == "/map/at_unsafe" ||
           resolved == "/std/collections/map/at" ||
           resolved == "/std/collections/map/at_unsafe";
  };
  if (getBuiltinConvertName(expr, builtinName)) {
    handledOut = true;
    if (expr.templateArgs.size() != 1) {
      error_ = "convert requires exactly one template argument";
      return false;
    }
    const std::string &typeName = expr.templateArgs[0];
    if (typeName != "int" && typeName != "i32" && typeName != "i64" &&
        typeName != "u64" && typeName != "bool" && typeName != "float" &&
        typeName != "f32" && typeName != "f64") {
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
    handledOut = true;
    error_ = printBuiltin.name + " is only supported as a statement";
    return false;
  }

  if (getBuiltinPointerName(expr, builtinName)) {
    handledOut = true;
    if (hasNamedArguments(expr.argNames)) {
      error_ = "named arguments not supported for builtin calls";
      return false;
    }
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
      if (target.kind != Expr::Kind::Name || isEntryArgsName(target.name) ||
          (locals.count(target.name) == 0 && !isParam(params, target.name))) {
        error_ = "location requires a local binding";
        return false;
      }
    }
    if (builtinName == "dereference" &&
        !isPointerLikeExpr(expr.args.front(), params, locals)) {
      error_ = "dereference requires a pointer or reference";
      return false;
    }
    if (!validateExpr(params, locals, expr.args.front())) {
      return false;
    }
    return true;
  }

  if (getBuiltinMemoryName(expr, builtinName)) {
    if (isExplicitMapAccessHelperPath(expr) ||
        ((builtinName == "at" || builtinName == "at_unsafe") &&
         expr.args.size() == 2 && expr.templateArgs.size() == 2)) {
      return true;
    }
    handledOut = true;
    if (expr.hasBodyArguments || !expr.bodyArguments.empty()) {
      error_ = builtinName + " does not accept block arguments";
      return false;
    }
    if (hasNamedArguments(expr.argNames)) {
      error_ = builtinName + " does not accept named arguments";
      return false;
    }
    if (builtinName == "alloc") {
      if (expr.templateArgs.size() != 1) {
        error_ = "alloc requires exactly one template argument";
        return false;
      }
      if (expr.args.size() != 1) {
        error_ = "argument count mismatch for builtin alloc";
        return false;
      }
      if (!validateMemoryTargetType(expr.templateArgs.front())) {
        return false;
      }
      if (currentValidationContext_.activeEffects.count("heap_alloc") == 0) {
        error_ = "alloc requires heap_alloc effect";
        return false;
      }
      if (!validateExpr(params, locals, expr.args.front())) {
        return false;
      }
      if (!isIntegerExpr(expr.args.front())) {
        error_ = "alloc requires integer element count";
        return false;
      }
      return true;
    }
    if (builtinName == "at") {
      if (!expr.templateArgs.empty()) {
        error_ = "at does not accept template arguments";
        return false;
      }
      if (expr.args.size() != 3) {
        error_ = "argument count mismatch for builtin at";
        return false;
      }
      if (!validateExpr(params, locals, expr.args.front())) {
        return false;
      }
      if (!isPointerExpr(expr.args.front(), params, locals)) {
        error_ = "at requires pointer target";
        return false;
      }
      if (!validateExpr(params, locals, expr.args[1])) {
        return false;
      }
      ReturnKind indexKind = normalizeMemoryIndexKind(
          inferExprReturnKind(expr.args[1], params, locals));
      if (!isSupportedMemoryIndexKind(indexKind)) {
        error_ = "at requires integer index";
        return false;
      }
      if (!validateExpr(params, locals, expr.args[2])) {
        return false;
      }
      ReturnKind countKind = normalizeMemoryIndexKind(
          inferExprReturnKind(expr.args[2], params, locals));
      if (!isSupportedMemoryIndexKind(countKind)) {
        error_ = "at requires integer element count";
        return false;
      }
      if (countKind != indexKind) {
        error_ = "at requires matching integer index and element count kinds";
        return false;
      }
      return true;
    }
    if (builtinName == "at_unsafe") {
      if (!expr.templateArgs.empty()) {
        error_ = "at_unsafe does not accept template arguments";
        return false;
      }
      if (expr.args.size() != 2) {
        error_ = "argument count mismatch for builtin at_unsafe";
        return false;
      }
      if (!validateExpr(params, locals, expr.args.front())) {
        return false;
      }
      if (!isPointerExpr(expr.args.front(), params, locals)) {
        error_ = "at_unsafe requires pointer target";
        return false;
      }
      if (!validateExpr(params, locals, expr.args[1])) {
        return false;
      }
      ReturnKind indexKind = normalizeMemoryIndexKind(
          inferExprReturnKind(expr.args[1], params, locals));
      if (!isSupportedMemoryIndexKind(indexKind)) {
        error_ = "at_unsafe requires integer index";
        return false;
      }
      return true;
    }
    if (!expr.templateArgs.empty()) {
      error_ = builtinName + " does not accept template arguments";
      return false;
    }
    const size_t expectedArgs = builtinName == "free" ? 1 : 2;
    if (expr.args.size() != expectedArgs) {
      error_ = "argument count mismatch for builtin " + builtinName;
      return false;
    }
    if (!validateExpr(params, locals, expr.args.front())) {
      return false;
    }
    if (!isPointerExpr(expr.args.front(), params, locals)) {
      error_ = builtinName + " requires pointer target";
      return false;
    }
    if (currentValidationContext_.activeEffects.count("heap_alloc") == 0) {
      error_ = builtinName + " requires heap_alloc effect";
      return false;
    }
    if (builtinName == "realloc") {
      if (!validateExpr(params, locals, expr.args[1])) {
        return false;
      }
      if (!isIntegerExpr(expr.args[1])) {
        error_ = "realloc requires integer element count";
        return false;
      }
    }
    return true;
  }

  return true;
}

} // namespace primec::semantics
