#include "SemanticsValidator.h"
#include "SemanticsValidatorExprCaptureSplitStep.h"

#include <cctype>
#include <functional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace primec::semantics {

namespace {

std::string trimText(const std::string &text) {
  size_t start = 0;
  while (start < text.size() && std::isspace(static_cast<unsigned char>(text[start]))) {
    ++start;
  }
  size_t end = text.size();
  while (end > start && std::isspace(static_cast<unsigned char>(text[end - 1]))) {
    --end;
  }
  return text.substr(start, end - start);
}

std::string_view normalizeFileResultMethodName(std::string_view methodName) {
  if (methodName == "readByte") {
    return "read_byte";
  }
  if (methodName == "writeLine") {
    return "write_line";
  }
  if (methodName == "writeByte") {
    return "write_byte";
  }
  if (methodName == "writeBytes") {
    return "write_bytes";
  }
  return methodName;
}

std::string_view normalizeFileErrorResultMethodName(std::string_view methodName) {
  if (methodName == "isEof") {
    return "is_eof";
  }
  return methodName;
}

} // namespace

bool SemanticsValidator::resolveResultTypeFromTemplateArg(const std::string &templateArg, ResultTypeInfo &out) const {
  out = ResultTypeInfo{};
  std::vector<std::string> parts;
  if (!splitTopLevelTemplateArgs(templateArg, parts)) {
    return false;
  }
  if (parts.size() == 1) {
    out.isResult = true;
    out.hasValue = false;
    out.errorType = trimText(parts.front());
    return true;
  }
  if (parts.size() == 2) {
    out.isResult = true;
    out.hasValue = true;
    out.valueType = trimText(parts[0]);
    out.errorType = trimText(parts[1]);
    return true;
  }
  return false;
}

bool SemanticsValidator::resolveResultTypeFromTypeName(const std::string &typeName, ResultTypeInfo &out) const {
  out = ResultTypeInfo{};
  std::string base;
  std::string arg;
  if (!splitTemplateTypeName(typeName, base, arg) || base != "Result") {
    return false;
  }
  return resolveResultTypeFromTemplateArg(arg, out);
}

bool SemanticsValidator::resolveResultTypeForExpr(const Expr &expr,
                                                  const std::vector<ParameterInfo> &params,
                                                  const std::unordered_map<std::string, BindingInfo> &locals,
                                                  ResultTypeInfo &out) {
  out = ResultTypeInfo{};
  auto bindingTypeText = [](const BindingInfo &binding) {
    if (binding.typeTemplateArg.empty()) {
      return binding.typeName;
    }
    return binding.typeName + "<" + binding.typeTemplateArg + ">";
  };
  auto resolveDefinitionResultType = [&](const Definition &definition, ResultTypeInfo &resultOut) -> bool {
    for (const auto &transform : definition.transforms) {
      if (transform.name != "return" || transform.templateArgs.size() != 1) {
        continue;
      }
      if (resolveResultTypeFromTypeName(transform.templateArgs.front(), resultOut)) {
        return true;
      }
    }
    BindingInfo inferredReturn;
    return inferDefinitionReturnBinding(definition, inferredReturn) &&
           resolveResultTypeFromTypeName(bindingTypeText(inferredReturn), resultOut);
  };
  auto resolveDirectResultOkType = [&](const Expr &candidate,
                                       const std::vector<ParameterInfo> &currentParams,
                                       const std::unordered_map<std::string, BindingInfo> &currentLocals,
                                       const std::string &preferredErrorType,
                                       ResultTypeInfo &resultOut) -> bool {
    resultOut = ResultTypeInfo{};
    if (candidate.kind != Expr::Kind::Call || !candidate.isMethodCall || candidate.name != "ok" ||
        !candidate.templateArgs.empty() || candidate.hasBodyArguments || !candidate.bodyArguments.empty() ||
        candidate.args.empty()) {
      return false;
    }
    const Expr &receiver = candidate.args.front();
    if (receiver.kind != Expr::Kind::Name || normalizeBindingTypeName(receiver.name) != "Result") {
      return false;
    }
    std::string errorType = preferredErrorType;
    if (errorType.empty() && currentValidationState_.context.resultType.has_value() &&
        currentValidationState_.context.resultType->isResult &&
        !currentValidationState_.context.resultType->errorType.empty()) {
      errorType = currentValidationState_.context.resultType->errorType;
    }
    if (errorType.empty() && currentValidationState_.context.onError.has_value() &&
        !currentValidationState_.context.onError->errorType.empty()) {
      errorType = currentValidationState_.context.onError->errorType;
    }
    if (errorType.empty()) {
      errorType = "_";
    }
    resultOut.isResult = true;
    resultOut.errorType = std::move(errorType);
    if (candidate.args.size() == 1) {
      resultOut.hasValue = false;
      return true;
    }
    if (candidate.args.size() != 2) {
      return false;
    }
    BindingInfo payloadBinding;
    if (!inferBindingTypeFromInitializer(candidate.args.back(), currentParams, currentLocals, payloadBinding)) {
      return false;
    }
    resultOut.hasValue = true;
    resultOut.valueType = bindingTypeText(payloadBinding);
    return !resultOut.valueType.empty();
  };
  auto resolveBindingTypeText = [&](const std::string &name, std::string &typeTextOut) -> bool {
    typeTextOut.clear();
    auto describeBindingType = [](const BindingInfo &binding) {
      if (binding.typeTemplateArg.empty()) {
        return binding.typeName;
      }
      return binding.typeName + "<" + binding.typeTemplateArg + ">";
    };
    if (const BindingInfo *paramBinding = findParamBinding(params, name)) {
      typeTextOut = describeBindingType(*paramBinding);
      return true;
    }
    auto it = locals.find(name);
    if (it != locals.end()) {
      typeTextOut = describeBindingType(it->second);
      return true;
    }
    return false;
  };
  auto resolveBuiltinMapResultType = [&](const std::string &typeText) -> bool {
    const std::string normalizedTypeText = normalizeBindingTypeName(unwrapReferencePointerTypeText(typeText));
    std::string base;
    std::string argText;
    if (splitTemplateTypeName(normalizedTypeText, base, argText) &&
        normalizeBindingTypeName(base) == "map") {
      std::vector<std::string> args;
      if (!splitTopLevelTemplateArgs(argText, args) || args.size() != 2) {
        return false;
      }
      out.isResult = true;
      out.hasValue = true;
      out.valueType = args[1];
      out.errorType = "ContainerError";
      return true;
    }
    std::string resolvedPath = normalizedTypeText;
    if (!resolvedPath.empty() && resolvedPath.front() != '/') {
      resolvedPath.insert(resolvedPath.begin(), '/');
    }
    if (resolvedPath.rfind("/std/collections/experimental_map/Map__", 0) != 0) {
      return false;
    }
    auto defIt = defMap_.find(resolvedPath);
    if (defIt == defMap_.end() || defIt->second == nullptr) {
      return false;
    }
    std::string valueType;
    for (const auto &fieldExpr : defIt->second->statements) {
      if (!fieldExpr.isBinding || fieldExpr.name != "payloads") {
        continue;
      }
      BindingInfo fieldBinding;
      std::optional<std::string> restrictType;
      std::string parseError;
      if (!parseBindingInfo(fieldExpr,
                            defIt->second->namespacePrefix,
                            structNames_,
                            importAliases_,
                            fieldBinding,
                            restrictType,
                            parseError)) {
        continue;
      }
      if (normalizeBindingTypeName(fieldBinding.typeName) != "vector" ||
          fieldBinding.typeTemplateArg.empty()) {
        continue;
      }
      std::vector<std::string> args;
      if (!splitTopLevelTemplateArgs(fieldBinding.typeTemplateArg, args) || args.size() != 1) {
        continue;
      }
      valueType = args.front();
      break;
    }
    if (valueType.empty()) {
      return false;
    }
    out.isResult = true;
    out.hasValue = true;
    out.valueType = valueType;
    out.errorType = "ContainerError";
    return true;
  };
  std::function<bool(const Expr &, std::string &)> resolveMapReceiverTypeText;
  resolveMapReceiverTypeText = [&](const Expr &receiverExpr, std::string &typeTextOut) -> bool {
    typeTextOut.clear();
    std::string accessName;
    if (receiverExpr.kind == Expr::Kind::Call && getBuiltinArrayAccessName(receiverExpr, accessName) &&
        receiverExpr.args.size() == 2 && !receiverExpr.args.empty()) {
      const Expr &accessReceiver = receiverExpr.args.front();
      if (accessReceiver.kind != Expr::Kind::Name) {
        return false;
      }
      const BindingInfo *binding = findParamBinding(params, accessReceiver.name);
      if (!binding) {
        auto it = locals.find(accessReceiver.name);
        if (it != locals.end()) {
          binding = &it->second;
        }
      }
      if (binding == nullptr) {
        return false;
      }
      if (getArgsPackElementType(*binding, typeTextOut)) {
        return true;
      }
    }
    return inferQueryExprTypeText(receiverExpr, params, locals, typeTextOut);
  };
  auto seedLambdaLocals = [&](const Expr &lambdaExpr,
                              std::unordered_map<std::string, BindingInfo> &lambdaLocals) -> bool {
    lambdaLocals.clear();
    bool captureAll = false;
    std::vector<std::string> captureNames;
    captureNames.reserve(lambdaExpr.lambdaCaptures.size());
    for (const auto &capture : lambdaExpr.lambdaCaptures) {
      const std::vector<std::string> tokens = runSemanticsValidatorExprCaptureSplitStep(capture);
      if (tokens.empty()) {
        return false;
      }
      if (tokens.size() == 1) {
        if (tokens[0] == "=" || tokens[0] == "&") {
          captureAll = true;
          continue;
        }
        captureNames.push_back(tokens[0]);
        continue;
      }
      captureNames.push_back(tokens.back());
    }
    auto addCapturedBinding = [&](const std::string &name) {
      if (const BindingInfo *paramBinding = findParamBinding(params, name)) {
        lambdaLocals.emplace(name, *paramBinding);
        return true;
      }
      auto localIt = locals.find(name);
      if (localIt != locals.end()) {
        lambdaLocals.emplace(name, localIt->second);
        return true;
      }
      return false;
    };
    if (captureAll) {
      for (const auto &param : params) {
        lambdaLocals.emplace(param.name, param.binding);
      }
      for (const auto &entry : locals) {
        lambdaLocals.emplace(entry.first, entry.second);
      }
    } else {
      for (const auto &name : captureNames) {
        (void)addCapturedBinding(name);
      }
    }
    for (const auto &param : lambdaExpr.args) {
      if (!param.isBinding) {
        return false;
      }
      BindingInfo binding;
      std::optional<std::string> restrictType;
      if (!parseBindingInfo(
              param, lambdaExpr.namespacePrefix, structNames_, importAliases_, binding, restrictType, error_)) {
        return false;
      }
      if (!hasExplicitBindingTypeTransform(param) && param.args.size() == 1) {
        (void)tryInferBindingTypeFromInitializer(param.args.front(), {}, {}, binding, hasAnyMathImport());
      }
      lambdaLocals[param.name] = std::move(binding);
    }
    return true;
  };
  auto inferLambdaBodyTypeText = [&](const Expr &lambdaExpr, std::string &typeTextOut) -> bool {
    typeTextOut.clear();
    if (!lambdaExpr.isLambda || (!lambdaExpr.hasBodyArguments && lambdaExpr.bodyArguments.empty())) {
      return false;
    }
    std::unordered_map<std::string, BindingInfo> lambdaLocals;
    if (!seedLambdaLocals(lambdaExpr, lambdaLocals)) {
      return false;
    }
    const Expr *valueExpr = nullptr;
    for (const auto &bodyExpr : lambdaExpr.bodyArguments) {
      if (bodyExpr.isBinding) {
        BindingInfo binding;
        std::optional<std::string> restrictType;
        if (!parseBindingInfo(
                bodyExpr, bodyExpr.namespacePrefix, structNames_, importAliases_, binding, restrictType, error_)) {
          return false;
        }
        if (!hasExplicitBindingTypeTransform(bodyExpr) && bodyExpr.args.size() == 1) {
          (void)inferBindingTypeFromInitializer(bodyExpr.args.front(), {}, lambdaLocals, binding, &bodyExpr);
        }
        if (restrictType.has_value()) {
          const bool hasTemplate = !binding.typeTemplateArg.empty();
          if (!restrictMatchesBinding(
                  *restrictType, binding.typeName, binding.typeTemplateArg, hasTemplate, bodyExpr.namespacePrefix)) {
            return false;
          }
        }
        lambdaLocals[bodyExpr.name] = std::move(binding);
        continue;
      }
      if (isReturnCall(bodyExpr)) {
        if (bodyExpr.args.size() != 1) {
          return false;
        }
        valueExpr = &bodyExpr.args.front();
        break;
      }
      valueExpr = &bodyExpr;
    }
    if (valueExpr == nullptr) {
      return false;
    }
    BindingInfo inferredBinding;
    if (inferBindingTypeFromInitializer(*valueExpr, {}, lambdaLocals, inferredBinding)) {
      typeTextOut = bindingTypeText(inferredBinding);
      if (!typeTextOut.empty()) {
        return true;
      }
    }
    return inferQueryExprTypeText(*valueExpr, {}, lambdaLocals, typeTextOut) && !typeTextOut.empty();
  };
  auto inferLambdaBodyResultType = [&](const Expr &lambdaExpr,
                                       const std::string &preferredErrorType,
                                       ResultTypeInfo &resultOut) -> bool {
    resultOut = ResultTypeInfo{};
    if (!lambdaExpr.isLambda || (!lambdaExpr.hasBodyArguments && lambdaExpr.bodyArguments.empty())) {
      return false;
    }
    std::unordered_map<std::string, BindingInfo> lambdaLocals;
    if (!seedLambdaLocals(lambdaExpr, lambdaLocals)) {
      return false;
    }
    const Expr *valueExpr = nullptr;
    for (const auto &bodyExpr : lambdaExpr.bodyArguments) {
      if (bodyExpr.isBinding) {
        BindingInfo binding;
        std::optional<std::string> restrictType;
        if (!parseBindingInfo(
                bodyExpr, bodyExpr.namespacePrefix, structNames_, importAliases_, binding, restrictType, error_)) {
          return false;
        }
        if (!hasExplicitBindingTypeTransform(bodyExpr) && bodyExpr.args.size() == 1) {
          (void)inferBindingTypeFromInitializer(bodyExpr.args.front(), {}, lambdaLocals, binding, &bodyExpr);
        }
        if (restrictType.has_value()) {
          const bool hasTemplate = !binding.typeTemplateArg.empty();
          if (!restrictMatchesBinding(
                  *restrictType, binding.typeName, binding.typeTemplateArg, hasTemplate, bodyExpr.namespacePrefix)) {
            return false;
          }
        }
        lambdaLocals[bodyExpr.name] = std::move(binding);
        continue;
      }
      if (isReturnCall(bodyExpr)) {
        if (bodyExpr.args.size() != 1) {
          return false;
        }
        valueExpr = &bodyExpr.args.front();
        break;
      }
      valueExpr = &bodyExpr;
    }
    if (valueExpr == nullptr) {
      return false;
    }
    if (resolveDirectResultOkType(*valueExpr, {}, lambdaLocals, preferredErrorType, resultOut)) {
      return true;
    }
    if (!resolveResultTypeForExpr(*valueExpr, {}, lambdaLocals, resultOut) || !resultOut.isResult) {
      return false;
    }
    if (!preferredErrorType.empty() && resultOut.errorType == "_") {
      resultOut.errorType = preferredErrorType;
    }
    return true;
  };
  auto preferredImageErrorHelperTarget = [&](std::string_view helperName) -> std::string {
    if (helperName == "why") {
      if (defMap_.count("/std/image/ImageError/why") > 0) {
        return "/std/image/ImageError/why";
      }
      if (defMap_.count("/ImageError/why") > 0) {
        return "/ImageError/why";
      }
      return "";
    }
    if (helperName == "status") {
      if (defMap_.count("/std/image/ImageError/status") > 0) {
        return "/std/image/ImageError/status";
      }
      if (defMap_.count("/ImageError/status") > 0) {
        return "/ImageError/status";
      }
      if (defMap_.count("/std/image/imageErrorStatus") > 0) {
        return "/std/image/imageErrorStatus";
      }
      return "";
    }
    if (helperName == "result") {
      if (defMap_.count("/std/image/ImageError/result") > 0) {
        return "/std/image/ImageError/result";
      }
      if (defMap_.count("/ImageError/result") > 0) {
        return "/ImageError/result";
      }
      if (defMap_.count("/std/image/imageErrorResult") > 0) {
        return "/std/image/imageErrorResult";
      }
      return "";
    }
    return "";
  };
  auto resolveMethodResultPath = [&]() -> std::string {
    if (!expr.isMethodCall || expr.args.empty()) {
      return "";
    }
    const BuiltinCollectionDispatchResolvers dispatchResolvers =
        makeBuiltinCollectionDispatchResolvers(params, locals);
    std::string resolvedPath;
    if (resolveLeadingNonCollectionAccessReceiverPath(
            params, locals, expr.args.front(), expr.name, dispatchResolvers, resolvedPath)) {
      return resolvedPath;
    }
    const Expr &receiver = expr.args.front();
    const std::string receiverTypeName = [&]() -> std::string {
      if (receiver.kind == Expr::Kind::Name) {
        if (const BindingInfo *paramBinding = findParamBinding(params, receiver.name)) {
          return paramBinding->typeName;
        }
        auto localIt = locals.find(receiver.name);
        if (localIt != locals.end()) {
          return localIt->second.typeName;
        }
        if (isPrimitiveBindingTypeName(receiver.name)) {
          return receiver.name;
        }
        const std::string rootReceiverPath = "/" + receiver.name;
        if (defMap_.find(rootReceiverPath) != defMap_.end() || structNames_.count(rootReceiverPath) > 0) {
          return rootReceiverPath;
        }
        auto importIt = importAliases_.find(receiver.name);
        if (importIt != importAliases_.end()) {
          return importIt->second;
        }
        const std::string resolvedType = resolveStructTypePath(receiver.name, receiver.namespacePrefix, structNames_);
        if (!resolvedType.empty()) {
          return resolvedType;
        }
      }
      if (receiver.kind == Expr::Kind::Call) {
        std::string inferredReceiverType;
        if (inferQueryExprTypeText(receiver, params, locals, inferredReceiverType) &&
            !inferredReceiverType.empty()) {
          return inferredReceiverType;
        }
        std::string collectionTypePath;
        if (resolveCallCollectionTypePath(receiver, params, locals, collectionTypePath) &&
            !collectionTypePath.empty()) {
          return collectionTypePath;
        }
        if (!receiver.isMethodCall) {
          const std::string resolvedReceiverPath = resolveCalleePath(receiver);
          if (!resolvedReceiverPath.empty()) {
            return resolvedReceiverPath;
          }
        }
      }
      return std::string();
    }();
    auto normalizedTypeLeafName = [](std::string value) {
      value = normalizeBindingTypeName(value);
      std::string base;
      std::string argText;
      if (splitTemplateTypeName(value, base, argText) && !base.empty()) {
        value = base;
      }
      if (!value.empty() && value.front() == '/') {
        value.erase(value.begin());
      }
      const size_t slash = value.find_last_of('/');
      return slash == std::string::npos ? value : value.substr(slash + 1);
    };
    auto typeMatches = [&](std::string_view candidate, std::string_view expected) {
      return candidate == expected || normalizedTypeLeafName(std::string(candidate)) == expected;
    };
    auto isCanonicalSoaWrapperMethodName = [](std::string_view methodName) {
      return methodName == "count" || methodName == "count_ref" ||
             methodName == "get" || methodName == "get_ref" ||
             methodName == "ref" || methodName == "ref_ref" ||
             methodName == "to_aos" || methodName == "to_aos_ref" ||
             methodName == "push" || methodName == "reserve";
    };
    const std::string normalizedMethodName =
        std::string(normalizeFileResultMethodName(expr.name));
    const std::string normalizedFileErrorMethodName =
        std::string(normalizeFileErrorResultMethodName(expr.name));
    if (receiver.kind == Expr::Kind::Name && receiver.name == "FileError" &&
        (normalizedFileErrorMethodName == "why" || normalizedFileErrorMethodName == "is_eof" ||
         normalizedFileErrorMethodName == "eof" || normalizedFileErrorMethodName == "status" ||
         normalizedFileErrorMethodName == "result")) {
      return this->preferredFileErrorHelperTarget(normalizedFileErrorMethodName);
    }
    if (typeMatches(receiverTypeName, "FileError") &&
        (normalizedFileErrorMethodName == "why" || normalizedFileErrorMethodName == "is_eof" ||
         normalizedFileErrorMethodName == "status" || normalizedFileErrorMethodName == "result")) {
      return this->preferredFileErrorHelperTarget(normalizedFileErrorMethodName);
    }
    if (typeMatches(receiverTypeName, "ImageError") &&
        (expr.name == "why" || expr.name == "status" || expr.name == "result")) {
      return preferredImageErrorHelperTarget(expr.name);
    }
    if (typeMatches(receiverTypeName, "ContainerError") &&
        (expr.name == "why" || expr.name == "status" || expr.name == "result")) {
      return this->preferredContainerErrorHelperTarget(expr.name);
    }
    if (typeMatches(receiverTypeName, "GfxError") &&
        (expr.name == "why" || expr.name == "status" || expr.name == "result")) {
      return this->preferredGfxErrorHelperTarget(
          expr.name,
          resolveStructTypePath(receiverTypeName, receiver.namespacePrefix, structNames_));
    }
    if (receiverTypeName.empty() || typeMatches(receiverTypeName, "File")) {
      return "";
    }
    if (isPrimitiveBindingTypeName(receiverTypeName)) {
      return "/" + normalizeBindingTypeName(receiverTypeName) + "/" + expr.name;
    }
    std::string resolvedType;
    if (!receiverTypeName.empty() && receiverTypeName.front() == '/') {
      resolvedType = receiverTypeName;
    } else {
      resolvedType = resolveStructTypePath(receiverTypeName, receiver.namespacePrefix, structNames_);
      if (resolvedType.empty()) {
        auto importIt = importAliases_.find(receiverTypeName);
        if (importIt != importAliases_.end()) {
          resolvedType = importIt->second;
        }
      }
      if (resolvedType.empty()) {
        resolvedType = resolveTypePath(receiverTypeName, receiver.namespacePrefix);
      }
    }
    if (resolvedType.empty()) {
      return "";
    }
    if (resolvedType.rfind("/std/collections/experimental_soa_vector/SoaVector__", 0) == 0 &&
        isCanonicalSoaWrapperMethodName(normalizedMethodName)) {
      return preferredSoaHelperTargetForCollectionType(normalizedMethodName, "/soa_vector");
    }
    return resolvedType + "/" + expr.name;
  };
  if (expr.kind == Expr::Kind::Name) {
    if (const BindingInfo *paramBinding = findParamBinding(params, expr.name)) {
      if (paramBinding->typeName == "Result") {
        return resolveResultTypeFromTemplateArg(paramBinding->typeTemplateArg, out);
      }
    }
    auto it = locals.find(expr.name);
    if (it != locals.end() && it->second.typeName == "Result") {
      return resolveResultTypeFromTemplateArg(it->second.typeTemplateArg, out);
    }
    return false;
  }
  if (expr.kind != Expr::Kind::Call) {
    return false;
  }
  if (expr.isFieldAccess && expr.args.size() == 1) {
    BindingInfo fieldBinding;
    if (resolveStructFieldBinding(params, locals, expr.args.front(), expr.name, fieldBinding)) {
      return resolveResultTypeFromTypeName(bindingTypeText(fieldBinding), out);
    }
  }
  if (resolveDirectResultOkType(expr, params, locals, "", out)) {
    return true;
  }
  if (!expr.isMethodCall && isSimpleCallName(expr, "File") && expr.templateArgs.size() == 1) {
    out.isResult = true;
    out.hasValue = true;
    out.valueType = "File<" + expr.templateArgs.front() + ">";
    out.errorType = "FileError";
    return true;
  }
  if (isSimpleCallName(expr, "dereference") && expr.args.size() == 1) {
    std::string pointeeType;
    const Expr &target = expr.args.front();
    if (target.kind == Expr::Kind::Name) {
      if (resolveBindingTypeText(target.name, pointeeType)) {
        return resolveResultTypeFromTypeName(unwrapReferencePointerTypeText(pointeeType), out);
      }
    }
    std::string accessName;
    if (getBuiltinArrayAccessName(target, accessName) && target.args.size() == 2 && !target.args.empty()) {
      const Expr &receiver = target.args.front();
      if (receiver.kind == Expr::Kind::Name && resolveBindingTypeText(receiver.name, pointeeType)) {
        std::string elemType;
        if (resolveResultTypeFromTypeName(unwrapReferencePointerTypeText(pointeeType), out)) {
          return true;
        }
        if (const BindingInfo *binding = findParamBinding(params, receiver.name)) {
          if (getArgsPackElementType(*binding, elemType)) {
            return resolveResultTypeFromTypeName(unwrapReferencePointerTypeText(elemType), out);
          }
        } else if (auto it = locals.find(receiver.name); it != locals.end()) {
          if (getArgsPackElementType(it->second, elemType)) {
            return resolveResultTypeFromTypeName(unwrapReferencePointerTypeText(elemType), out);
          }
        }
      }
    }
  }
  std::string accessName;
  if (getBuiltinArrayAccessName(expr, accessName) && expr.args.size() == 2 && !expr.args.empty()) {
    const Expr &receiver = expr.args.front();
    if (receiver.kind == Expr::Kind::Name) {
      const BindingInfo *binding = findParamBinding(params, receiver.name);
      if (!binding) {
        auto it = locals.find(receiver.name);
        if (it != locals.end()) {
          binding = &it->second;
        }
      }
      std::string elemType;
      if (binding != nullptr && getArgsPackElementType(*binding, elemType)) {
        return resolveResultTypeFromTypeName(unwrapReferencePointerTypeText(elemType), out);
      }
    }
  }
  if (expr.isMethodCall) {
    if (!expr.args.empty() &&
        expr.args.front().kind == Expr::Kind::Name &&
        normalizeBindingTypeName(expr.args.front().name) == "Result") {
      if ((expr.name == "map" || expr.name == "and_then") && expr.args.size() == 3) {
        ResultTypeInfo argResult;
        if (!resolveResultTypeForExpr(expr.args[1], params, locals, argResult) || !argResult.isResult ||
            !argResult.hasValue) {
          return false;
        }
        if (expr.name == "map") {
          std::string mappedType;
          if (!inferLambdaBodyTypeText(expr.args[2], mappedType) || mappedType.empty()) {
            return false;
          }
          out.isResult = true;
          out.hasValue = true;
          out.valueType = std::move(mappedType);
          out.errorType = argResult.errorType;
          return true;
        }
        ResultTypeInfo chainedResult;
        if (!inferLambdaBodyResultType(expr.args[2], argResult.errorType, chainedResult) || !chainedResult.isResult) {
          return false;
        }
        if (!chainedResult.errorType.empty() && !argResult.errorType.empty() &&
            !errorTypesMatch(chainedResult.errorType, argResult.errorType, expr.namespacePrefix)) {
          return false;
        }
        out = std::move(chainedResult);
        return true;
      }
      if (expr.name == "map2" && expr.args.size() == 4) {
        ResultTypeInfo leftResult;
        ResultTypeInfo rightResult;
        if (!resolveResultTypeForExpr(expr.args[1], params, locals, leftResult) || !leftResult.isResult ||
            !leftResult.hasValue ||
            !resolveResultTypeForExpr(expr.args[2], params, locals, rightResult) || !rightResult.isResult ||
            !rightResult.hasValue) {
          return false;
        }
        if (!errorTypesMatch(leftResult.errorType, rightResult.errorType, expr.namespacePrefix)) {
          return false;
        }
        std::string mappedType;
        if (!inferLambdaBodyTypeText(expr.args[3], mappedType) || mappedType.empty()) {
          return false;
        }
        out.isResult = true;
        out.hasValue = true;
        out.valueType = std::move(mappedType);
        out.errorType = leftResult.errorType;
        return true;
      }
    }
    const std::string normalizedMethodName =
        std::string(normalizeFileResultMethodName(expr.name));
    if (normalizedMethodName == "write" || normalizedMethodName == "write_line" ||
        normalizedMethodName == "write_byte" || normalizedMethodName == "read_byte" ||
        normalizedMethodName == "write_bytes" || normalizedMethodName == "flush" ||
        normalizedMethodName == "close") {
      out.isResult = true;
      out.hasValue = false;
      out.errorType = "FileError";
      return true;
    }
    const std::string resolvedMethodPath =
        resolveExprConcreteCallPath(params, locals, expr, resolveMethodResultPath());
    if (!resolvedMethodPath.empty()) {
      auto it = defMap_.find(resolvedMethodPath);
      if (it != defMap_.end() && it->second != nullptr) {
        return resolveDefinitionResultType(*it->second, out);
      }
    }
    if (expr.name == "tryAt" && !expr.args.empty()) {
      std::string receiverTypeText;
      if (resolveMapReceiverTypeText(expr.args.front(), receiverTypeText) &&
          resolveBuiltinMapResultType(receiverTypeText)) {
        return true;
      }
    }
  }
  if (!expr.isMethodCall) {
    const std::string resolved = resolveCalleePath(expr);
    auto it = defMap_.find(resolved);
    if (it != defMap_.end() && it->second != nullptr) {
      return resolveDefinitionResultType(*it->second, out);
    }
    if ((resolved == "/std/collections/map/tryAt" ||
         resolved == "/std/collections/map/tryAt_ref" ||
         resolved == "/map/tryAt" ||
         isSimpleCallName(expr, "tryAt") ||
         isSimpleCallName(expr, "tryAt_ref")) &&
        !expr.args.empty()) {
      const Expr *receiverExpr = &expr.args.front();
      if (hasNamedArguments(expr.argNames)) {
        for (size_t i = 0; i < expr.args.size(); ++i) {
          if (i < expr.argNames.size() && expr.argNames[i].has_value() &&
              *expr.argNames[i] == "values") {
            receiverExpr = &expr.args[i];
            break;
          }
        }
      }
      std::string receiverTypeText;
      if (resolveMapReceiverTypeText(*receiverExpr, receiverTypeText) &&
          resolveBuiltinMapResultType(receiverTypeText)) {
        return true;
      }
    }
  }
  return false;
}

bool SemanticsValidator::errorTypesMatch(const std::string &left,
                                         const std::string &right,
                                         const std::string &namespacePrefix) const {
  const auto stripInnerWhitespace = [](const std::string &text) {
    std::string out;
    out.reserve(text.size());
    for (unsigned char ch : text) {
      if (!std::isspace(ch)) {
        out.push_back(static_cast<char>(ch));
      }
    }
    return out;
  };
  const auto normalizeType = [&](const std::string &name) -> std::string {
    const std::string trimmed = trimText(name);
    const std::string normalized = normalizeBindingTypeName(trimmed);
    std::string base;
    std::string arg;
    if (splitTemplateTypeName(normalized, base, arg) &&
        (isBuiltinTemplateTypeName(base) || base == "array" || base == "vector" || base == "soa_vector" ||
         base == "map" || base == "Result" || base == "File")) {
      return stripInnerWhitespace(normalized);
    }
    if (normalized != trimmed || isPrimitiveBindingTypeName(trimmed)) {
      return stripInnerWhitespace(normalized);
    }
    if (!trimmed.empty() && trimmed[0] == '/') {
      return stripInnerWhitespace(trimmed);
    }
    if (auto aliasIt = importAliases_.find(trimmed); aliasIt != importAliases_.end()) {
      return stripInnerWhitespace(aliasIt->second);
    }
    return stripInnerWhitespace(resolveTypePath(trimmed, namespacePrefix));
  };
  return normalizeType(left) == normalizeType(right);
}

bool SemanticsValidator::parseOnErrorTransform(const std::vector<Transform> &transforms,
                                               const std::string &namespacePrefix,
                                               const std::string &context,
                                               std::optional<OnErrorHandler> &out) {
  out.reset();
  auto failOnErrorTransformDiagnostic = [&](std::string message) -> bool {
    if (currentDefinitionContext_ != nullptr) {
      return failDefinitionDiagnostic(*currentDefinitionContext_, std::move(message));
    }
    return failUncontextualizedDiagnostic(std::move(message));
  };
  for (const auto &transform : transforms) {
    if (transform.name != "on_error") {
      continue;
    }
    if (out.has_value()) {
      return failOnErrorTransformDiagnostic("duplicate on_error transform on " +
                                            context);
    }
    if (transform.templateArgs.size() != 2) {
      return failOnErrorTransformDiagnostic(
          "on_error requires exactly two template arguments on " + context);
    }
    OnErrorHandler handler;
    handler.errorType = transform.templateArgs.front();
    Expr handlerExpr;
    handlerExpr.kind = Expr::Kind::Name;
    handlerExpr.name = transform.templateArgs[1];
    handlerExpr.namespacePrefix = namespacePrefix;
    handler.handlerPath = resolveCalleePath(handlerExpr);
    if (defMap_.count(handler.handlerPath) == 0) {
      return failOnErrorTransformDiagnostic("unknown on_error handler: " +
                                            handler.handlerPath);
    }
    auto paramIt = paramsByDef_.find(handler.handlerPath);
    if (paramIt == paramsByDef_.end() || paramIt->second.empty()) {
      return failOnErrorTransformDiagnostic(
          "on_error handler requires an error parameter: " +
          handler.handlerPath);
    }
    const BindingInfo &paramBinding = paramIt->second.front().binding;
    if (!errorTypesMatch(handler.errorType, paramBinding.typeName, namespacePrefix)) {
      return failOnErrorTransformDiagnostic(
          "on_error handler error type mismatch: " + handler.handlerPath);
    }
    handler.boundArgs.reserve(transform.arguments.size());
    for (const auto &argText : transform.arguments) {
      Expr argExpr;
      if (!parseTransformArgumentExpr(argText, namespacePrefix, argExpr)) {
        if (error_.empty()) {
          return failOnErrorTransformDiagnostic("invalid on_error argument on " +
                                                context);
        }
        return false;
      }
      handler.boundArgs.push_back(std::move(argExpr));
    }
    out = std::move(handler);
  }
  return true;
}

} // namespace primec::semantics
