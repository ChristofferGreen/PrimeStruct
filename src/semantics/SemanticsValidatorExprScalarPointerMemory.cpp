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
  auto failScalarPointerMemoryBuiltin = [&](std::string message) -> bool {
    return failExprDiagnostic(expr, std::move(message));
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
  auto isDeclaredPointerLikeCall = [&](const Expr &candidate) -> bool {
    if (candidate.kind != Expr::Kind::Call) {
      return false;
    }
    auto isPointerLikeBinding = [](const BindingInfo &binding) {
      const std::string normalizedType = normalizeBindingTypeName(binding.typeName);
      return normalizedType == "Pointer" || normalizedType == "Reference";
    };
    auto isImplicitSoaRefMethod = [&]() {
      const std::string normalizedName =
          !candidate.name.empty() && candidate.name.front() == '/'
              ? candidate.name.substr(1)
              : candidate.name;
      if (!candidate.isMethodCall || normalizedName != "ref" ||
          candidate.args.empty()) {
        return false;
      }
      std::string elemType;
      const Expr &receiver = candidate.args.front();
      if (receiver.kind == Expr::Kind::Name) {
        if (const BindingInfo *paramBinding = findParamBinding(params, receiver.name)) {
          return extractExperimentalSoaVectorElementType(*paramBinding, elemType);
        }
        auto localIt = locals.find(receiver.name);
        return localIt != locals.end() &&
               extractExperimentalSoaVectorElementType(localIt->second, elemType);
      }
      BindingInfo receiverBinding;
      std::string receiverTypeText;
      if (!inferQueryExprTypeText(receiver, params, locals, receiverTypeText)) {
        return false;
      }
      std::string base;
      std::string argText;
      const std::string normalizedType = normalizeBindingTypeName(receiverTypeText);
      if (splitTemplateTypeName(normalizedType, base, argText)) {
        receiverBinding.typeName = normalizeBindingTypeName(base);
        receiverBinding.typeTemplateArg = argText;
      } else {
        receiverBinding.typeName = normalizedType;
        receiverBinding.typeTemplateArg.clear();
      }
      return extractExperimentalSoaVectorElementType(receiverBinding, elemType);
    };
    if (isImplicitSoaRefMethod()) {
      return true;
    }
    std::string resolvedPath = resolveCalleePath(candidate);
    if (candidate.isMethodCall) {
      if (candidate.args.empty()) {
        return false;
      }
      bool isBuiltin = false;
      if (!resolveMethodTarget(params,
                               locals,
                               candidate.namespacePrefix,
                               candidate.args.front(),
                               candidate.name,
                               resolvedPath,
                               isBuiltin)) {
        return false;
      }
    }
    resolvedPath = resolveExprConcreteCallPath(params, locals, candidate, resolvedPath);
    BindingInfo inferredReturn;
    if (inferResolvedDirectCallBindingType(resolvedPath, inferredReturn) &&
        isPointerLikeBinding(inferredReturn)) {
      return true;
    }
    if (inferBindingTypeFromInitializer(candidate, params, locals, inferredReturn) &&
        isPointerLikeBinding(inferredReturn)) {
      return true;
    }
    auto defIt = defMap_.find(resolvedPath);
    return defIt != defMap_.end() && defIt->second != nullptr &&
           inferDefinitionReturnBinding(*defIt->second, inferredReturn) &&
           isPointerLikeBinding(inferredReturn);
  };

  auto validateMemoryTargetType = [&](const std::string &targetType) -> bool {
    std::string targetBase;
    std::string targetArg;
    if (splitTemplateTypeName(targetType, targetBase, targetArg) &&
        normalizeBindingTypeName(targetBase) == "array") {
      return failScalarPointerMemoryBuiltin("unsupported pointer target type: " + targetType);
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
      return failScalarPointerMemoryBuiltin(std::move(parseError));
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
  auto isMapLikeCollectionExpr = [&](const Expr &candidate) -> bool {
    std::string keyType;
    std::string valueType;
    if (candidate.kind == Expr::Kind::Name) {
      if (const BindingInfo *paramBinding = findParamBinding(params, candidate.name)) {
        return extractMapKeyValueTypes(*paramBinding, keyType, valueType);
      }
      auto it = locals.find(candidate.name);
      return it != locals.end() &&
             extractMapKeyValueTypes(it->second, keyType, valueType);
    }
    if (candidate.kind != Expr::Kind::Call) {
      return false;
    }
    std::string collectionName;
    if (getBuiltinCollectionName(candidate, collectionName) &&
        collectionName == "map" && candidate.templateArgs.size() == 2) {
      return true;
    }
    auto defIt = defMap_.find(resolveCalleePath(candidate));
    if (defIt != defMap_.end() && defIt->second != nullptr) {
      BindingInfo inferredReturn;
      if (inferDefinitionReturnBinding(*defIt->second, inferredReturn) &&
          extractMapKeyValueTypes(inferredReturn, keyType, valueType)) {
        return true;
      }
    }
    std::string inferredTypeText;
    return inferQueryExprTypeText(candidate, params, locals, inferredTypeText) &&
           returnsMapCollectionType(inferredTypeText);
  };
  if (getBuiltinConvertName(expr, builtinName)) {
    handledOut = true;
    if (expr.templateArgs.size() != 1) {
      return failScalarPointerMemoryBuiltin("convert requires exactly one template argument");
    }
    const std::string &typeName = expr.templateArgs[0];
    if (typeName != "int" && typeName != "i32" && typeName != "i64" &&
        typeName != "u64" && typeName != "bool" && typeName != "float" &&
        typeName != "f32" && typeName != "f64") {
      return failScalarPointerMemoryBuiltin("unsupported convert target type: " + typeName);
    }
    if (expr.args.size() != 1) {
      return failScalarPointerMemoryBuiltin("argument count mismatch for builtin " + builtinName);
    }
    if (!validateExpr(params, locals, expr.args.front())) {
      return false;
    }
    if (!isConvertibleExpr(expr.args.front())) {
      return failScalarPointerMemoryBuiltin("convert requires numeric or bool operand");
    }
    return true;
  }

  PrintBuiltin printBuiltin;
  if (getPrintBuiltin(expr, printBuiltin)) {
    handledOut = true;
    return failScalarPointerMemoryBuiltin(printBuiltin.name + " is only supported as a statement");
  }

  if (getBuiltinPointerName(expr, builtinName)) {
    handledOut = true;
    if (hasNamedArguments(expr.argNames)) {
      return failScalarPointerMemoryBuiltin("named arguments not supported for builtin calls");
    }
    if (!expr.templateArgs.empty()) {
      return failScalarPointerMemoryBuiltin("pointer helpers do not accept template arguments");
    }
    if (expr.hasBodyArguments || !expr.bodyArguments.empty()) {
      return failScalarPointerMemoryBuiltin("pointer helpers do not accept block arguments");
    }
    if (expr.args.size() != 1) {
      return failScalarPointerMemoryBuiltin("argument count mismatch for builtin " + builtinName);
    }
    if (builtinName == "location") {
      const Expr &target = expr.args.front();
      if (target.kind != Expr::Kind::Name || isEntryArgsName(target.name) ||
          (locals.count(target.name) == 0 && !isParam(params, target.name))) {
        return failScalarPointerMemoryBuiltin("location requires a local binding");
      }
    }
    if (builtinName == "dereference" &&
        !isPointerLikeExpr(expr.args.front(), params, locals) &&
        !isDeclaredPointerLikeCall(expr.args.front())) {
      return failScalarPointerMemoryBuiltin("dereference requires a pointer or reference");
    }
    if (!validateExpr(params, locals, expr.args.front())) {
      return false;
    }
    return true;
  }

  if (getBuiltinMemoryName(expr, builtinName)) {
    if (isExplicitMapAccessHelperPath(expr) ||
        ((builtinName == "at" || builtinName == "at_unsafe") &&
         expr.args.size() == 2 &&
         (expr.templateArgs.size() == 2 ||
          isMapLikeCollectionExpr(expr.args.front()) ||
          isMapLikeCollectionExpr(expr.args[1])))) {
      return true;
    }
    handledOut = true;
    if (expr.hasBodyArguments || !expr.bodyArguments.empty()) {
      return failScalarPointerMemoryBuiltin(builtinName + " does not accept block arguments");
    }
    if (hasNamedArguments(expr.argNames)) {
      return failScalarPointerMemoryBuiltin(builtinName + " does not accept named arguments");
    }
    if (builtinName == "alloc") {
      if (expr.templateArgs.size() != 1) {
        return failScalarPointerMemoryBuiltin("alloc requires exactly one template argument");
      }
      if (expr.args.size() != 1) {
        return failScalarPointerMemoryBuiltin("argument count mismatch for builtin alloc");
      }
      if (!validateMemoryTargetType(expr.templateArgs.front())) {
        return false;
      }
      if (currentValidationState_.context.activeEffects.count("heap_alloc") == 0) {
        return failScalarPointerMemoryBuiltin("alloc requires heap_alloc effect");
      }
      if (!validateExpr(params, locals, expr.args.front())) {
        return false;
      }
      if (!isIntegerExpr(expr.args.front())) {
        return failScalarPointerMemoryBuiltin("alloc requires integer element count");
      }
      return true;
    }
    if (builtinName == "at") {
      if (!expr.templateArgs.empty()) {
        return failScalarPointerMemoryBuiltin("at does not accept template arguments");
      }
      if (expr.args.size() != 3) {
        return failScalarPointerMemoryBuiltin("argument count mismatch for builtin at");
      }
      if (!validateExpr(params, locals, expr.args.front())) {
        return false;
      }
      if (!isPointerExpr(expr.args.front(), params, locals)) {
        return failScalarPointerMemoryBuiltin("at requires pointer target");
      }
      if (!validateExpr(params, locals, expr.args[1])) {
        return false;
      }
      ReturnKind indexKind = normalizeMemoryIndexKind(
          inferExprReturnKind(expr.args[1], params, locals));
      if (!isSupportedMemoryIndexKind(indexKind)) {
        return failScalarPointerMemoryBuiltin("at requires integer index");
      }
      if (!validateExpr(params, locals, expr.args[2])) {
        return false;
      }
      ReturnKind countKind = normalizeMemoryIndexKind(
          inferExprReturnKind(expr.args[2], params, locals));
      if (!isSupportedMemoryIndexKind(countKind)) {
        return failScalarPointerMemoryBuiltin("at requires integer element count");
      }
      if (countKind != indexKind) {
        return failScalarPointerMemoryBuiltin(
            "at requires matching integer index and element count kinds");
      }
      return true;
    }
    if (builtinName == "at_unsafe") {
      if (!expr.templateArgs.empty()) {
        return failScalarPointerMemoryBuiltin("at_unsafe does not accept template arguments");
      }
      if (expr.args.size() != 2) {
        return failScalarPointerMemoryBuiltin("argument count mismatch for builtin at_unsafe");
      }
      if (!validateExpr(params, locals, expr.args.front())) {
        return false;
      }
      if (!isPointerExpr(expr.args.front(), params, locals)) {
        return failScalarPointerMemoryBuiltin("at_unsafe requires pointer target");
      }
      if (!validateExpr(params, locals, expr.args[1])) {
        return false;
      }
      ReturnKind indexKind = normalizeMemoryIndexKind(
          inferExprReturnKind(expr.args[1], params, locals));
      if (!isSupportedMemoryIndexKind(indexKind)) {
        return failScalarPointerMemoryBuiltin("at_unsafe requires integer index");
      }
      return true;
    }
    if (builtinName == "reinterpret") {
      if (expr.templateArgs.size() != 1) {
        return failScalarPointerMemoryBuiltin("reinterpret requires exactly one template argument");
      }
      if (expr.args.size() != 1) {
        return failScalarPointerMemoryBuiltin("argument count mismatch for builtin reinterpret");
      }
      if (!validateMemoryTargetType(expr.templateArgs.front())) {
        return false;
      }
      if (!validateExpr(params, locals, expr.args.front())) {
        return false;
      }
      if (!isPointerExpr(expr.args.front(), params, locals)) {
        return failScalarPointerMemoryBuiltin("reinterpret requires pointer target");
      }
      return true;
    }
    if (!expr.templateArgs.empty()) {
      return failScalarPointerMemoryBuiltin(builtinName + " does not accept template arguments");
    }
    const size_t expectedArgs = builtinName == "free" ? 1 : 2;
    if (expr.args.size() != expectedArgs) {
      return failScalarPointerMemoryBuiltin("argument count mismatch for builtin " + builtinName);
    }
    if (!validateExpr(params, locals, expr.args.front())) {
      return false;
    }
    if (!isPointerExpr(expr.args.front(), params, locals)) {
      return failScalarPointerMemoryBuiltin(builtinName + " requires pointer target");
    }
    if (currentValidationState_.context.activeEffects.count("heap_alloc") == 0) {
      return failScalarPointerMemoryBuiltin(builtinName + " requires heap_alloc effect");
    }
    if (builtinName == "realloc") {
      if (!validateExpr(params, locals, expr.args[1])) {
        return false;
      }
      if (!isIntegerExpr(expr.args[1])) {
        return failScalarPointerMemoryBuiltin("realloc requires integer element count");
      }
    }
    return true;
  }

  return true;
}

} // namespace primec::semantics
