#include "SemanticsValidator.h"

#include <string>

namespace primec::semantics {
namespace {

std::string_view normalizeFileBuiltinHelperName(std::string_view helperName) {
  if (helperName == "readByte") {
    return "read_byte";
  }
  if (helperName == "writeLine") {
    return "write_line";
  }
  if (helperName == "writeByte") {
    return "write_byte";
  }
  if (helperName == "writeBytes") {
    return "write_bytes";
  }
  return helperName;
}

} // namespace

bool SemanticsValidator::validateExprResultFileBuiltins(
    const std::vector<ParameterInfo> &params,
    const std::unordered_map<std::string, BindingInfo> &locals,
    const Expr &expr,
    const std::string &resolved,
    bool resolvedMethod,
    const ExprResultFileBuiltinContext &context,
    bool &handledOut) {
  auto failResultFileDiagnostic = [&](std::string message) -> bool {
    return failExprDiagnostic(expr, std::move(message));
  };
  handledOut = false;
  auto isMutableBinding = [&](const std::string &name) -> bool {
    if (const BindingInfo *paramBinding = findParamBinding(params, name)) {
      return paramBinding->isMutable;
    }
    auto it = locals.find(name);
    return it != locals.end() && it->second.isMutable;
  };
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
  auto defaultIsStringExpr = [&](const Expr &arg) -> bool {
    if (arg.kind == Expr::Kind::StringLiteral) {
      return true;
    }
    if (inferExprReturnKind(arg, params, locals) == ReturnKind::String) {
      return true;
    }
    std::string collectionTypePath;
    return arg.kind == Expr::Kind::Call &&
           resolveCallCollectionTypePath(arg, params, locals, collectionTypePath) &&
           collectionTypePath == "/string";
  };
  auto isStringExpr = [&](const Expr &arg) -> bool {
    return context.isStringExpr != nullptr ? context.isStringExpr(arg)
                                           : defaultIsStringExpr(arg);
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

  if (!expr.isMethodCall && isSimpleCallName(expr, "File")) {
    handledOut = true;
    if (hasNamedArguments(expr.argNames)) {
      return failResultFileDiagnostic("named arguments not supported for builtin calls");
    }
    if (expr.templateArgs.size() != 1) {
      return failResultFileDiagnostic("File requires exactly one template argument");
    }
    const std::string &mode = expr.templateArgs.front();
    if (mode != "Read" && mode != "Write" && mode != "Append") {
      return failResultFileDiagnostic("File requires Read, Write, or Append mode");
    }
    if (expr.args.size() != 1) {
      return failResultFileDiagnostic("File requires exactly one path argument");
    }
    if (expr.hasBodyArguments || !expr.bodyArguments.empty()) {
      return failResultFileDiagnostic("File does not accept block arguments");
    }
    const bool requiresWrite = mode == "Write" || mode == "Append";
    const char *requiredEffect = requiresWrite ? "file_write" : "file_read";
    if (currentValidationState_.context.activeEffects.count(requiredEffect) == 0) {
      return failResultFileDiagnostic(std::string("File requires ") + requiredEffect + " effect");
    }
    if (!validateExpr(params, locals, expr.args.front())) {
      return false;
    }
    if (!isStringExpr(expr.args.front())) {
      return failResultFileDiagnostic("File requires string path argument");
    }
    return true;
  }

  if (resolvedMethod && resolved == "/result/ok") {
    handledOut = true;
    if (hasNamedArguments(expr.argNames)) {
      return failResultFileDiagnostic("named arguments not supported for builtin calls");
    }
    if (!expr.templateArgs.empty()) {
      return failResultFileDiagnostic("Result.ok does not accept template arguments");
    }
    if (expr.hasBodyArguments || !expr.bodyArguments.empty()) {
      return failResultFileDiagnostic("Result.ok does not accept block arguments");
    }
    if (expr.args.size() > 2) {
      return failResultFileDiagnostic("Result.ok accepts at most one value argument");
    }
    for (size_t i = 1; i < expr.args.size(); ++i) {
      if (!validateExpr(params, locals, expr.args[i])) {
        return false;
      }
    }
    return true;
  }

  if (resolvedMethod && resolved == "/result/error") {
    handledOut = true;
    if (hasNamedArguments(expr.argNames)) {
      return failResultFileDiagnostic("named arguments not supported for builtin calls");
    }
    if (!expr.templateArgs.empty()) {
      return failResultFileDiagnostic("Result.error does not accept template arguments");
    }
    if (expr.hasBodyArguments || !expr.bodyArguments.empty()) {
      return failResultFileDiagnostic("Result.error does not accept block arguments");
    }
    if (expr.args.size() != 2) {
      return failResultFileDiagnostic("Result.error requires exactly one argument");
    }
    if (!validateExpr(params, locals, expr.args[1])) {
      return false;
    }
    ResultTypeInfo argResult;
    if (!resolveResultTypeForExpr(expr.args[1], params, locals, argResult) || !argResult.isResult) {
      return failResultFileDiagnostic("Result.error requires Result argument");
    }
    return true;
  }

  if (resolvedMethod && resolved == "/result/why") {
    handledOut = true;
    if (hasNamedArguments(expr.argNames)) {
      return failResultFileDiagnostic("named arguments not supported for builtin calls");
    }
    if (!expr.templateArgs.empty()) {
      return failResultFileDiagnostic("Result.why does not accept template arguments");
    }
    if (expr.hasBodyArguments || !expr.bodyArguments.empty()) {
      return failResultFileDiagnostic("Result.why does not accept block arguments");
    }
    if (expr.args.size() != 2) {
      return failResultFileDiagnostic("Result.why requires exactly one argument");
    }
    if (!validateExpr(params, locals, expr.args[1])) {
      return false;
    }
    ResultTypeInfo argResult;
    if (!resolveResultTypeForExpr(expr.args[1], params, locals, argResult) || !argResult.isResult) {
      return failResultFileDiagnostic("Result.why requires Result argument");
    }
    return true;
  }

  if (resolvedMethod && (resolved == "/result/map" || resolved == "/result/and_then")) {
    handledOut = true;
    if (hasNamedArguments(expr.argNames)) {
      return failResultFileDiagnostic("named arguments not supported for builtin calls");
    }
    if (!expr.templateArgs.empty()) {
      return failResultFileDiagnostic("Result." + expr.name + " does not accept template arguments");
    }
    if (expr.hasBodyArguments || !expr.bodyArguments.empty()) {
      return failResultFileDiagnostic("Result." + expr.name + " does not accept block arguments");
    }
    if (expr.args.size() != 3) {
      return failResultFileDiagnostic("Result." + expr.name + " requires exactly two arguments");
    }
    if (!validateExpr(params, locals, expr.args[1])) {
      return false;
    }
    ResultTypeInfo argResult;
    if (!resolveResultTypeForExpr(expr.args[1], params, locals, argResult) || !argResult.isResult) {
      return failResultFileDiagnostic("Result." + expr.name + " requires Result argument");
    }
    if (!argResult.hasValue) {
      return failResultFileDiagnostic("Result." + expr.name + " requires value Result");
    }
    if (!expr.args[2].isLambda) {
      return failResultFileDiagnostic("Result." + expr.name + " requires a lambda argument");
    }
    if (expr.args[2].args.size() != 1) {
      return failResultFileDiagnostic("Result." + expr.name + " requires a single-parameter lambda");
    }
    if (!validateExpr(params, locals, expr.args[2])) {
      return false;
    }
    return true;
  }

  if (resolvedMethod && resolved == "/result/map2") {
    handledOut = true;
    if (hasNamedArguments(expr.argNames)) {
      return failResultFileDiagnostic("named arguments not supported for builtin calls");
    }
    if (!expr.templateArgs.empty()) {
      return failResultFileDiagnostic("Result.map2 does not accept template arguments");
    }
    if (expr.hasBodyArguments || !expr.bodyArguments.empty()) {
      return failResultFileDiagnostic("Result.map2 does not accept block arguments");
    }
    if (expr.args.size() != 4) {
      return failResultFileDiagnostic("Result.map2 requires exactly three arguments");
    }
    if (!validateExpr(params, locals, expr.args[1]) || !validateExpr(params, locals, expr.args[2])) {
      return false;
    }
    ResultTypeInfo leftResult;
    ResultTypeInfo rightResult;
    if (!resolveResultTypeForExpr(expr.args[1], params, locals, leftResult) || !leftResult.isResult ||
        !resolveResultTypeForExpr(expr.args[2], params, locals, rightResult) || !rightResult.isResult) {
      return failResultFileDiagnostic("Result.map2 requires Result arguments");
    }
    if (!leftResult.hasValue || !rightResult.hasValue) {
      return failResultFileDiagnostic("Result.map2 requires value Results");
    }
    if (!errorTypesMatch(leftResult.errorType, rightResult.errorType, expr.namespacePrefix)) {
      return failResultFileDiagnostic("Result.map2 requires matching error types");
    }
    if (!expr.args[3].isLambda) {
      return failResultFileDiagnostic("Result.map2 requires a lambda argument");
    }
    if (expr.args[3].args.size() != 2) {
      return failResultFileDiagnostic("Result.map2 requires a two-parameter lambda");
    }
    if (!validateExpr(params, locals, expr.args[3])) {
      return false;
    }
    return true;
  }

  if (resolved == "/file_error/why") {
    handledOut = true;
    if (hasNamedArguments(expr.argNames)) {
      return failResultFileDiagnostic("named arguments not supported for builtin calls");
    }
    if (!expr.templateArgs.empty()) {
      return failResultFileDiagnostic("why does not accept template arguments");
    }
    if (expr.hasBodyArguments || !expr.bodyArguments.empty()) {
      return failResultFileDiagnostic("why does not accept block arguments");
    }
    if (expr.args.size() != 1) {
      return failResultFileDiagnostic("why does not accept arguments");
    }
    if (!validateExpr(params, locals, expr.args.front())) {
      return false;
    }
    return true;
  }

  if (resolved.rfind("/file/", 0) == 0) {
    const std::string fileHelperName = std::string(
        normalizeFileBuiltinHelperName(resolved.substr(std::string("/file/").size())));
    if (hasNamedArguments(expr.argNames)) {
      return failResultFileDiagnostic("named arguments not supported for builtin calls");
    }
    if (!expr.templateArgs.empty()) {
      return failResultFileDiagnostic("file methods do not accept template arguments");
    }
    if (expr.hasBodyArguments || !expr.bodyArguments.empty()) {
      return failResultFileDiagnostic("file methods do not accept block arguments");
    }
    if (expr.args.empty()) {
      return failResultFileDiagnostic("file method missing receiver");
    }
    const bool requiresRead = fileHelperName == "read_byte" || fileHelperName == "close";
    const char *requiredEffect = requiresRead ? "file_read" : "file_write";
    if (currentValidationState_.context.activeEffects.count(requiredEffect) == 0) {
      return failResultFileDiagnostic(std::string("file operations require ") + requiredEffect +
                                      " effect");
    }

    const Expr &receiverExpr = expr.args.front();
    if (context.isNamedArgsPackWrappedFileBuiltinAccessCall != nullptr &&
        context.isNamedArgsPackWrappedFileBuiltinAccessCall(receiverExpr)) {
      if (!receiverExpr.templateArgs.empty()) {
        return failResultFileDiagnostic("at does not accept template arguments");
      }
      if (receiverExpr.hasBodyArguments || !receiverExpr.bodyArguments.empty()) {
        return failResultFileDiagnostic("at does not accept block arguments");
      }
      if (!isIntegerExpr(receiverExpr.args[1])) {
        return failResultFileDiagnostic("at requires integer index");
      }
      if (!validateExpr(params, locals, receiverExpr.args[0]) ||
          !validateExpr(params, locals, receiverExpr.args[1])) {
        return false;
      }
    } else if (!validateExpr(params, locals, receiverExpr)) {
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
        if (defMap_.find(resolveCalleePath(arg)) == defMap_.end() &&
            getBuiltinCollectionName(arg, builtinName)) {
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

    if (fileHelperName == "write" || fileHelperName == "write_line") {
      handledOut = true;
      for (size_t i = 1; i < expr.args.size(); ++i) {
        if (!validateExpr(params, locals, expr.args[i])) {
          return false;
        }
        if (!isFilePrintableExpr(expr.args[i])) {
          return failResultFileDiagnostic(
              "file write requires integer/bool or string arguments");
        }
      }
      return true;
    }
    if (fileHelperName == "write_byte") {
      handledOut = true;
      if (expr.args.size() != 2) {
        return failResultFileDiagnostic("write_byte requires exactly one argument");
      }
      if (!validateExpr(params, locals, expr.args[1])) {
        return false;
      }
      if (!isIntegerOrBoolExpr(expr.args[1])) {
        return failResultFileDiagnostic("write_byte requires integer argument");
      }
      return true;
    }
    if (fileHelperName == "read_byte") {
      handledOut = true;
      if (expr.args.size() != 2) {
        return failResultFileDiagnostic("read_byte requires exactly one argument");
      }
      if (!validateExpr(params, locals, expr.args[1])) {
        return false;
      }
      if (expr.args[1].kind != Expr::Kind::Name || !isMutableBinding(expr.args[1].name)) {
        return failResultFileDiagnostic("read_byte requires mutable integer binding");
      }
      ReturnKind kind = ReturnKind::Unknown;
      if (const BindingInfo *paramBinding = findParamBinding(params, expr.args[1].name)) {
        kind = returnKindForBinding(*paramBinding);
      } else {
        auto itLocal = locals.find(expr.args[1].name);
        if (itLocal != locals.end()) {
          kind = returnKindForBinding(itLocal->second);
        }
      }
      if (kind != ReturnKind::Int && kind != ReturnKind::Int64 && kind != ReturnKind::UInt64) {
        return failResultFileDiagnostic("read_byte requires mutable integer binding");
      }
      return true;
    }
    if (fileHelperName == "write_bytes") {
      handledOut = true;
      if (expr.args.size() != 2) {
        return failResultFileDiagnostic("write_bytes requires exactly one argument");
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
        return failResultFileDiagnostic("write_bytes requires array argument");
      }
      return true;
    }
    if (fileHelperName == "flush" || fileHelperName == "close") {
      handledOut = true;
      if (expr.args.size() != 1) {
        return failResultFileDiagnostic(fileHelperName + " does not accept arguments");
      }
      return true;
    }
  }

  return true;
}

} // namespace primec::semantics
