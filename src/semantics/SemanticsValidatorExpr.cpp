#include "SemanticsValidator.h"
#include "primec/StringLiteral.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <functional>
#include <optional>
#include <sstream>
#include <unordered_set>

namespace primec::semantics {

bool SemanticsValidator::validateExpr(const std::vector<ParameterInfo> &params,
                                      const std::unordered_map<std::string, BindingInfo> &locals,
                                      const Expr &expr,
                                      const std::vector<Expr> *enclosingStatements,
                                      size_t statementIndex) {
  ExprContextScope exprScope(*this, expr);
  if (expr.isLambda) {
    return validateLambdaExpr(params, locals, expr, enclosingStatements, statementIndex);
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
      if (currentValidationContext_.movedBindings.count(expr.name) > 0) {
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
      return validateIfExpr(params, locals, expr);
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
      return validateBlockExpr(params, locals, expr);
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
    auto hasImportedDefinitionPath = [&](const std::string &path) {
      std::string canonicalPath = path;
      const size_t suffix = canonicalPath.find("__t");
      if (suffix != std::string::npos) {
        canonicalPath.erase(suffix);
      }
      const auto &importPaths = program_.sourceImports.empty() ? program_.imports : program_.sourceImports;
      for (const auto &importPath : importPaths) {
        if (importPath == canonicalPath) {
          return true;
        }
        if (importPath.size() >= 2 && importPath.compare(importPath.size() - 2, 2, "/*") == 0) {
          const std::string prefix = importPath.substr(0, importPath.size() - 2);
          if (canonicalPath == prefix || canonicalPath.rfind(prefix + "/", 0) == 0) {
            return true;
          }
        }
      }
      return false;
    };
    auto hasDefinitionPath = [&](const std::string &path) {
      return this->hasDefinitionPath(path);
    };
    auto hasVisibleCanonicalVectorHelperPath = [&](const std::string &path) {
      const bool isStdlibVectorWrapperDefinition =
          currentValidationContext_.definitionPath.rfind("/std/collections/", 0) == 0 ||
          currentValidationContext_.definitionPath.rfind("/std/image/", 0) == 0;
      if (hasImportedDefinitionPath(path)) {
        return true;
      }
      if (defMap_.count(path) == 0) {
        return false;
      }
      auto paramsIt = paramsByDef_.find(path);
      if (paramsIt != paramsByDef_.end() && !paramsIt->second.empty()) {
        std::string experimentalElemType;
        if (extractExperimentalVectorElementType(paramsIt->second.front().binding, experimentalElemType)) {
          return isStdlibVectorWrapperDefinition;
        }
      }
      return true;
    };
    auto isExperimentalMapTypeText = [&](const std::string &typeText) {
      std::string normalizedType = normalizeBindingTypeName(typeText);
      while (true) {
        std::string base;
        std::string argText;
        if (!splitTemplateTypeName(normalizedType, base, argText)) {
          return false;
        }
        base = normalizeBindingTypeName(base);
        if (base == "Map" || base == "/Map" ||
            base == "std/collections/experimental_map/Map" ||
            base == "/std/collections/experimental_map/Map") {
          std::vector<std::string> parts;
          return splitTopLevelTemplateArgs(argText, parts) && parts.size() == 2;
        }
        if (base == "Reference" || base == "Pointer") {
          std::vector<std::string> parts;
          if (!splitTopLevelTemplateArgs(argText, parts) || parts.size() != 1) {
            return false;
          }
          normalizedType = normalizeBindingTypeName(parts.front());
          continue;
        }
        return false;
      }
    };
    auto isExperimentalMapReceiverExpr = [&](const Expr &candidate) {
      std::string receiverTypeText;
      if (inferQueryExprTypeText(candidate, params, locals, receiverTypeText) &&
          isExperimentalMapTypeText(receiverTypeText)) {
        return true;
      }
      if (candidate.kind != Expr::Kind::Call) {
        return false;
      }
      auto defIt = defMap_.find(resolveCalleePath(candidate));
      if (defIt == defMap_.end() || defIt->second == nullptr) {
        return false;
      }
      BindingInfo inferredReturn;
      if (!inferDefinitionReturnBinding(*defIt->second, inferredReturn)) {
        return false;
      }
      const std::string inferredTypeText =
          inferredReturn.typeTemplateArg.empty()
              ? inferredReturn.typeName
              : inferredReturn.typeName + "<" + inferredReturn.typeTemplateArg + ">";
      return isExperimentalMapTypeText(inferredTypeText);
    };
    bool hasVectorHelperCallResolution = false;
    std::string vectorHelperCallResolvedPath;
    size_t vectorHelperCallReceiverIndex = 0;
    if (!resolveExprVectorHelperCall(params,
                                     locals,
                                     expr,
                                     hasVectorHelperCallResolution,
                                     vectorHelperCallResolvedPath,
                                     vectorHelperCallReceiverIndex)) {
      return false;
    }
    if (isReturnCall(expr)) {
      error_ = "return not allowed in expression context";
      return false;
    }
    auto resolveFieldBindingTarget = [&](const Expr &target, BindingInfo &bindingOut) -> bool {
      if (!(target.kind == Expr::Kind::Call && target.isFieldAccess && target.args.size() == 1)) {
        return false;
      }
      std::string structPath;
      const Expr &receiver = target.args.front();
      if (receiver.kind == Expr::Kind::Name) {
        const BindingInfo *receiverBinding = findParamBinding(params, receiver.name);
        if (!receiverBinding) {
          auto it = locals.find(receiver.name);
          if (it != locals.end()) {
            receiverBinding = &it->second;
          }
        }
        if (receiverBinding != nullptr) {
          std::string receiverType = receiverBinding->typeName;
          if ((receiverType == "Reference" || receiverType == "Pointer") && !receiverBinding->typeTemplateArg.empty()) {
            receiverType = receiverBinding->typeTemplateArg;
          }
          structPath = resolveStructTypePath(receiverType, receiver.namespacePrefix, structNames_);
          if (structPath.empty()) {
            auto importIt = importAliases_.find(receiverType);
            if (importIt != importAliases_.end() && structNames_.count(importIt->second) > 0) {
              structPath = importIt->second;
            }
          }
        }
      } else if (receiver.kind == Expr::Kind::Call && !receiver.isBinding) {
        std::string inferredStruct = inferStructReturnPath(receiver, params, locals);
        if (!inferredStruct.empty() && structNames_.count(inferredStruct) > 0) {
          structPath = inferredStruct;
        } else {
          const std::string resolvedType = resolveCalleePath(receiver);
          if (structNames_.count(resolvedType) > 0) {
            structPath = resolvedType;
          }
        }
      }
      if (structPath.empty()) {
        return false;
      }
      auto defIt = defMap_.find(structPath);
      if (defIt == defMap_.end() || defIt->second == nullptr) {
        return false;
      }
      for (const auto &fieldStmt : defIt->second->statements) {
        bool isStaticField = false;
        for (const auto &transform : fieldStmt.transforms) {
          if (transform.name == "static") {
            isStaticField = true;
            break;
          }
        }
        if (!fieldStmt.isBinding || isStaticField || fieldStmt.name != target.name) {
          continue;
        }
        return resolveStructFieldBinding(*defIt->second, fieldStmt, bindingOut);
      }
      return false;
    };
    const BuiltinCollectionDispatchResolverAdapters builtinCollectionDispatchResolverAdapters{
        .resolveBindingTarget =
            [&](const Expr &target, BindingInfo &bindingOut) -> bool {
              return resolveFieldBindingTarget(target, bindingOut);
            },
        .inferCallBinding =
            [&](const Expr &target, BindingInfo &bindingOut) -> bool {
              if (target.kind != Expr::Kind::Call) {
                return false;
              }
              auto defIt = defMap_.find(resolveCalleePath(target));
              return defIt != defMap_.end() && defIt->second != nullptr &&
                     inferDefinitionReturnBinding(*defIt->second, bindingOut);
            }};
    const BuiltinCollectionDispatchResolvers builtinCollectionDispatchResolvers =
        makeBuiltinCollectionDispatchResolvers(params, locals, builtinCollectionDispatchResolverAdapters);
    const auto &resolveArgsPackAccessTarget = builtinCollectionDispatchResolvers.resolveArgsPackAccessTarget;
    const auto &resolveArrayTarget = builtinCollectionDispatchResolvers.resolveArrayTarget;
    const auto &resolveVectorTarget = builtinCollectionDispatchResolvers.resolveVectorTarget;
    const auto &resolveSoaVectorTarget = builtinCollectionDispatchResolvers.resolveSoaVectorTarget;
    const auto &resolveStringTarget = builtinCollectionDispatchResolvers.resolveStringTarget;
    const auto &resolveMapTargetWithTypes = builtinCollectionDispatchResolvers.resolveMapTarget;
    const auto &resolveExperimentalMapTarget =
        builtinCollectionDispatchResolvers.resolveExperimentalMapTarget;
    auto isDeclaredPointerLikeCall = [&](const Expr &candidate) -> bool {
      if (candidate.kind != Expr::Kind::Call) {
        return false;
      }
      auto defIt = defMap_.find(resolveCalleePath(candidate));
      if (defIt == defMap_.end() || defIt->second == nullptr) {
        return false;
      }
      BindingInfo inferredReturn;
      if (!inferDefinitionReturnBinding(*defIt->second, inferredReturn)) {
        return false;
      }
      return inferredReturn.typeName == "Pointer" || inferredReturn.typeName == "Reference";
    };
    std::function<bool(const Expr &)> resolveMapTarget;
    resolveMapTarget = [&](const Expr &target) -> bool {
      std::string keyType;
      std::string valueType;
      if (resolveMapTargetWithTypes(target, keyType, valueType) ||
          resolveExperimentalMapTarget(target, keyType, valueType)) {
        return true;
      }
      std::string inferredTypeText;
      return inferQueryExprTypeText(target, params, locals, inferredTypeText) &&
             returnsMapCollectionType(inferredTypeText);
    };
    const bool shouldBuiltinValidateBareMapCountCall = true;
    const bool shouldBuiltinValidateBareMapContainsCall = true;
    const bool shouldBuiltinValidateBareMapTryAtCall = true;
    const bool shouldBuiltinValidateBareMapAccessCall = true;
    std::string earlyPointerBuiltin;
    if (getBuiltinPointerName(expr, earlyPointerBuiltin)) {
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
        error_ = "argument count mismatch for builtin " + earlyPointerBuiltin;
        return false;
      }
      if (earlyPointerBuiltin == "location") {
        const Expr &target = expr.args.front();
        if (target.kind != Expr::Kind::Name || isEntryArgsName(target.name) ||
            (locals.count(target.name) == 0 && !isParam(params, target.name))) {
          error_ = "location requires a local binding";
          return false;
        }
      }
      if (earlyPointerBuiltin == "dereference" &&
          !isPointerLikeExpr(expr.args.front(), params, locals) &&
          !isDeclaredPointerLikeCall(expr.args.front())) {
        error_ = "dereference requires a pointer or reference";
        return false;
      }
      return validateExpr(params, locals, expr.args.front());
    }

    std::string resolved = resolveCalleePath(expr);
    std::string canonicalExperimentalMapHelperResolved;
    if (!expr.isMethodCall && defMap_.count(resolved) == 0 &&
        this->canonicalizeExperimentalMapHelperResolvedPath(resolved, canonicalExperimentalMapHelperResolved)) {
      resolved = canonicalExperimentalMapHelperResolved;
    }
    Expr rewrittenCanonicalExperimentalVectorHelperCall;
    if (this->tryRewriteCanonicalExperimentalVectorHelperCall(
            expr, builtinCollectionDispatchResolvers, rewrittenCanonicalExperimentalVectorHelperCall)) {
      return validateExpr(params, locals, rewrittenCanonicalExperimentalVectorHelperCall);
    }
    Expr rewrittenCanonicalExperimentalMapHelperCall;
    if (this->tryRewriteCanonicalExperimentalMapHelperCall(
            expr, builtinCollectionDispatchResolvers, rewrittenCanonicalExperimentalMapHelperCall)) {
      return validateExpr(params, locals, rewrittenCanonicalExperimentalMapHelperCall);
    }
    std::string borrowedCanonicalExperimentalMapHelperPath;
    if (!expr.isMethodCall &&
        this->explicitCanonicalExperimentalMapBorrowedHelperPath(
            expr, builtinCollectionDispatchResolvers, borrowedCanonicalExperimentalMapHelperPath)) {
      error_ = "unknown call target: " + borrowedCanonicalExperimentalMapHelperPath;
      return false;
    }
    if (expr.isMethodCall &&
        expr.args.size() == 1 &&
        (expr.name == "count" || expr.name == "capacity")) {
      std::string receiverCollectionTypePath;
      if (expr.args.front().kind == Expr::Kind::Call &&
          resolveCallCollectionTypePath(expr.args.front(), params, locals, receiverCollectionTypePath) &&
          receiverCollectionTypePath == "/vector") {
        if ((expr.name == "count" || expr.name == "capacity") &&
            expr.templateArgs.empty() &&
            !expr.hasBodyArguments &&
            expr.bodyArguments.empty()) {
          return validateExpr(params, locals, expr.args.front());
        }
        Expr rewrittenHelperCall = expr;
        rewrittenHelperCall.isMethodCall = false;
        rewrittenHelperCall.namespacePrefix.clear();
        rewrittenHelperCall.name = expr.name;
        return validateExpr(params, locals, rewrittenHelperCall);
      }
    }
    if (!expr.isMethodCall && expr.args.size() == 2) {
      std::string builtinAccessName;
      if (getBuiltinArrayAccessName(expr, builtinAccessName) &&
          hasImportedDefinitionPath("/std/collections/map/" + builtinAccessName) &&
          defMap_.find("/std/collections/map/" + builtinAccessName) == defMap_.end() &&
          !hasDeclaredDefinitionPath("/map/" + builtinAccessName)) {
        auto setCanonicalMapKeyMismatch = [&](const Expr &receiverExpr,
                                              const std::string &mapKeyType) {
          const std::string canonicalPath = "/std/collections/map/" + builtinAccessName;
          (void)receiverExpr;
          if (expr.name.rfind("/std/collections/map/", 0) == 0 ||
              expr.namespacePrefix == "/std/collections/map" ||
              expr.namespacePrefix == "std/collections/map") {
            error_ = "argument type mismatch for " + canonicalPath + " parameter key";
            return;
          }
          if (normalizeBindingTypeName(mapKeyType) == "string") {
            error_ = builtinAccessName + " requires string map key";
          } else {
            error_ = builtinAccessName + " requires map key type " + mapKeyType;
          }
        };
        size_t receiverIndex = 0;
        size_t keyIndex = 1;
        const bool hasBareMapOperands = this->bareMapHelperOperandIndices(
            expr, builtinCollectionDispatchResolvers, receiverIndex, keyIndex);
        const Expr &receiverExpr = hasBareMapOperands ? expr.args[receiverIndex] : expr.args.front();
        const Expr &keyExpr = hasBareMapOperands ? expr.args[keyIndex] : expr.args[1];
        std::string receiverTypeText;
        std::string mapKeyType;
        std::string mapValueType;
        if (inferQueryExprTypeText(receiverExpr, params, locals, receiverTypeText) &&
            extractMapKeyValueTypesFromTypeText(receiverTypeText, mapKeyType, mapValueType)) {
          if (!mapKeyType.empty()) {
            if (normalizeBindingTypeName(mapKeyType) == "string") {
              if (!this->isStringExprForArgumentValidation(keyExpr, builtinCollectionDispatchResolvers)) {
                setCanonicalMapKeyMismatch(receiverExpr, mapKeyType);
                return false;
              }
            } else {
              ReturnKind keyKind = returnKindForTypeName(normalizeBindingTypeName(mapKeyType));
              if (keyKind != ReturnKind::Unknown) {
                if (resolveStringTarget(keyExpr)) {
                  setCanonicalMapKeyMismatch(receiverExpr, mapKeyType);
                  return false;
                }
                ReturnKind indexKind = inferExprReturnKind(keyExpr, params, locals);
                if (indexKind != ReturnKind::Unknown && indexKind != keyKind) {
                  setCanonicalMapKeyMismatch(receiverExpr, mapKeyType);
                  return false;
                }
              }
            }
          }
          if (!validateExpr(params, locals, expr.args.front()) ||
              !validateExpr(params, locals, expr.args[1])) {
            return false;
          }
          return true;
        }
      }
    }
    if (!expr.isMethodCall) {
      std::string builtinAccessName;
      std::string receiverTypeText;
      if (getBuiltinArrayAccessName(expr, builtinAccessName) &&
          expr.args.size() == 2 &&
          ((builtinCollectionDispatchResolvers.resolveExperimentalMapValueTarget != nullptr &&
            (builtinCollectionDispatchResolvers.resolveExperimentalMapValueTarget(
                 expr.args.front(), receiverTypeText, borrowedCanonicalExperimentalMapHelperPath) ||
             builtinCollectionDispatchResolvers.resolveExperimentalMapValueTarget(
                 expr.args[1], receiverTypeText, borrowedCanonicalExperimentalMapHelperPath))) ||
           isExperimentalMapReceiverExpr(expr.args.front()) ||
           isExperimentalMapReceiverExpr(expr.args[1]))) {
        error_ = "unknown call target: /std/collections/map/" + builtinAccessName;
        return false;
      }
    }
    bool resolvedMethod = false;
    bool usedMethodTarget = false;
    bool hasMethodReceiverIndex = false;
    size_t methodReceiverIndex = 0;
    auto hasResolvableDefinitionTarget = [&](const std::string &path) {
      return hasDeclaredDefinitionPath(path) || hasImportedDefinitionPath(path);
    };
    if (hasVectorHelperCallResolution) {
      resolved = vectorHelperCallResolvedPath;
      usedMethodTarget = true;
      hasMethodReceiverIndex = true;
      methodReceiverIndex = vectorHelperCallReceiverIndex;
    }
    const std::string methodReflectionTarget =
        this->describeMethodReflectionTarget(params, locals, expr);
    if (!methodReflectionTarget.empty()) {
      if (methodReflectionTarget == "meta.object" || methodReflectionTarget == "meta.table") {
        error_ = "runtime reflection objects/tables are unsupported: " + methodReflectionTarget;
      } else if (isReflectionMetadataQueryName(expr.name)) {
        error_ = "reflection metadata queries are compile-time only and not yet implemented: " + methodReflectionTarget;
      } else {
        error_ = "unsupported reflection metadata query: " + methodReflectionTarget;
      }
      return false;
    }
    if (defMap_.count(resolved) == 0) {
      if (isReflectionMetadataQueryPath(resolved)) {
        error_ = "reflection metadata queries are compile-time only and not yet implemented: " + resolved;
        return false;
      }
      if (isRuntimeReflectionPath(resolved)) {
        error_ = "runtime reflection objects/tables are unsupported: " + resolved;
        return false;
      }
      if (resolved.rfind("/meta/", 0) == 0) {
        const std::string queryName = resolved.substr(6);
        if (!queryName.empty() && queryName.find('/') == std::string::npos) {
          error_ = "unsupported reflection metadata query: " + resolved;
          return false;
        }
      }
    }
    if (!expr.isMethodCall &&
        this->isArrayNamespacedVectorCountCompatibilityCall(expr, builtinCollectionDispatchResolvers)) {
      error_ = "unknown call target: /array/count";
      return false;
    }
    if (!expr.isMethodCall) {
      const std::string removedVectorCompatibilityPath =
          this->getDirectVectorHelperCompatibilityPath(expr);
      if (!removedVectorCompatibilityPath.empty()) {
        error_ = "unknown call target: " + removedVectorCompatibilityPath;
        return false;
      }
    }
    if (!expr.isMethodCall) {
      const std::string removedMapCompatibilityPath =
          this->directMapHelperCompatibilityPath(expr, params, locals, builtinCollectionDispatchResolverAdapters);
      if (!removedMapCompatibilityPath.empty()) {
        error_ = "unknown call target: " + removedMapCompatibilityPath;
        return false;
      }
    }
    auto isKnownCollectionTarget = [&](const Expr &targetExpr) -> bool {
      std::string elemType;
      return resolveVectorTarget(targetExpr, elemType) || resolveArrayTarget(targetExpr, elemType) ||
             resolveStringTarget(targetExpr) || resolveMapTarget(targetExpr);
    };
    auto promoteCapacityToBuiltinValidation = [&](const Expr &targetExpr,
                                                  std::string &resolvedOut,
                                                  bool &isBuiltinMethodOut,
                                                  bool requireKnownCollection) {
      if (requireKnownCollection && !isKnownCollectionTarget(targetExpr)) {
        return;
      }
      // Route unresolved capacity() calls through builtin validation so
      // non-vector targets emit deterministic vector-target diagnostics.
      resolvedOut = "/vector/capacity";
      isBuiltinMethodOut = true;
    };
    auto isNonCollectionStructCapacityTarget = [&](const std::string &resolvedPath) -> bool {
      constexpr std::string_view suffix = "/capacity";
      if (resolvedPath.size() <= suffix.size() ||
          resolvedPath.compare(resolvedPath.size() - suffix.size(), suffix.size(), suffix) != 0) {
        return false;
      }
      const std::string receiverPath = resolvedPath.substr(0, resolvedPath.size() - suffix.size());
      if (receiverPath == "/array" || receiverPath == "/vector" || receiverPath == "/map" || receiverPath == "/string") {
        return false;
      }
      return structNames_.count(receiverPath) > 0;
    };
    auto isNonCollectionStructAccessTarget = [&](const std::string &resolvedPath) -> bool {
      constexpr std::string_view atSuffix = "/at";
      constexpr std::string_view atUnsafeSuffix = "/at_unsafe";
      std::string receiverPath;
      if (resolvedPath.size() > atSuffix.size() &&
          resolvedPath.compare(resolvedPath.size() - atSuffix.size(), atSuffix.size(), atSuffix) == 0) {
        receiverPath = resolvedPath.substr(0, resolvedPath.size() - atSuffix.size());
      } else if (resolvedPath.size() > atUnsafeSuffix.size() &&
                 resolvedPath.compare(resolvedPath.size() - atUnsafeSuffix.size(), atUnsafeSuffix.size(),
                                      atUnsafeSuffix) == 0) {
        receiverPath = resolvedPath.substr(0, resolvedPath.size() - atUnsafeSuffix.size());
      } else {
        return false;
      }
      if (receiverPath == "/array" || receiverPath == "/vector" || receiverPath == "/map" ||
          receiverPath == "/string" ||
          receiverPath.rfind("/std/collections/experimental_vector/Vector__", 0) == 0) {
        return false;
      }
      return structNames_.count(receiverPath) > 0;
    };
    auto experimentalGfxUnavailableConstructorDiagnostic = [&](const Expr &callExpr,
                                                               const std::string &resolvedPath) -> std::string {
      if (resolvedPath != "/std/gfx/experimental/Device" || callExpr.isMethodCall ||
          callExpr.kind != Expr::Kind::Call) {
        return "";
      }
      if (!callExpr.templateArgs.empty() || hasNamedArguments(callExpr.argNames) || !callExpr.bodyArguments.empty() ||
          callExpr.hasBodyArguments || !callExpr.args.empty()) {
        return "";
      }
      return "experimental gfx entry point not implemented yet: Device()";
    };
    auto experimentalGfxUnavailableMethodDiagnostic = [&](const std::string &resolvedPath) -> std::string {
      if (resolvedPath == "/std/gfx/experimental/Device/create_pipeline") {
        return "experimental gfx entry point not implemented yet: Device.create_pipeline([vertex_type] type, ...)";
      }
      return "";
    };
    if (expr.isFieldAccess) {
      return validateExprFieldAccess(params, locals, expr);
    }
    std::string accessHelperName;
    std::string namespacedCollection;
    std::string namespacedHelper;
    const bool isNamespacedCollectionHelperCall =
        getNamespacedCollectionHelperName(expr, namespacedCollection, namespacedHelper);
    const bool isNamespacedVectorHelperCall =
        isNamespacedCollectionHelperCall && namespacedCollection == "vector";
    const bool isStdNamespacedVectorCountCall =
        !expr.isMethodCall && resolveCalleePath(expr).rfind("/std/collections/vector/count", 0) == 0;
    const bool hasStdNamespacedVectorCountDefinition =
        hasImportedDefinitionPath("/std/collections/vector/count");
    const bool shouldBuiltinValidateStdNamespacedVectorCountCall =
        isStdNamespacedVectorCountCall && hasStdNamespacedVectorCountDefinition;
    const bool isStdNamespacedMapCountCall =
        !expr.isMethodCall && resolveCalleePath(expr) == "/std/collections/map/count";
    const bool isNamespacedMapHelperCall =
        isNamespacedCollectionHelperCall && namespacedCollection == "map";
    const std::string directRemovedMapCompatibilityPath =
        !expr.isMethodCall
            ? this->directMapHelperCompatibilityPath(expr, params, locals, builtinCollectionDispatchResolverAdapters)
            : std::string();
    const bool isMapNamespacedCountCompatibilityCall =
        directRemovedMapCompatibilityPath == "/map/count";
    const bool isMapNamespacedAccessCompatibilityCall =
        directRemovedMapCompatibilityPath == "/map/at" ||
        directRemovedMapCompatibilityPath == "/map/at_unsafe";
    const bool isNamespacedVectorCountCall =
        !expr.isMethodCall && !isStdNamespacedVectorCountCall &&
        isNamespacedVectorHelperCall && namespacedHelper == "count" &&
        isVectorBuiltinName(expr, "count") && expr.args.size() == 1 &&
        !hasDefinitionPath(resolved) &&
        !this->isArrayNamespacedVectorCountCompatibilityCall(expr, builtinCollectionDispatchResolvers);
    const bool isNamespacedMapCountCall =
        !expr.isMethodCall && isNamespacedMapHelperCall && namespacedHelper == "count" &&
        !isStdNamespacedMapCountCall && !isMapNamespacedCountCompatibilityCall &&
        !hasDefinitionPath(resolved);
    const bool isUnnamespacedMapCountFallbackCall =
        !expr.isMethodCall &&
        this->isUnnamespacedMapCountBuiltinFallbackCall(expr, params, locals, builtinCollectionDispatchResolverAdapters);
    const bool isResolvedMapCountCall =
        !expr.isMethodCall && resolved == "/map/count" &&
        !isMapNamespacedCountCompatibilityCall &&
        !isUnnamespacedMapCountFallbackCall;
    const bool isStdNamespacedVectorCapacityCall =
        !expr.isMethodCall && resolveCalleePath(expr).rfind("/std/collections/vector/capacity", 0) == 0;
    const bool hasStdNamespacedVectorCapacityDefinition =
        hasImportedDefinitionPath("/std/collections/vector/capacity");
    const bool isNamespacedVectorCapacityCall =
        !expr.isMethodCall && !isStdNamespacedVectorCapacityCall &&
        isNamespacedVectorHelperCall && namespacedHelper == "capacity" &&
        isVectorBuiltinName(expr, "capacity") && expr.args.size() == 1 &&
        !hasDefinitionPath(resolved);
    const bool shouldBuiltinValidateStdNamespacedVectorCapacityCall =
        isStdNamespacedVectorCapacityCall && hasStdNamespacedVectorCapacityDefinition;
    const bool shouldSkipStdCapacityMethodFallback = false;
    const bool hasBuiltinAccessSpelling =
        !expr.isMethodCall && getBuiltinArrayAccessName(expr, accessHelperName);
    const bool isStdNamespacedVectorAccessCall =
        hasBuiltinAccessSpelling && !expr.isMethodCall &&
        resolveCalleePath(expr).rfind("/std/collections/vector/at", 0) == 0;
    const bool hasStdNamespacedVectorAccessDefinition =
        isStdNamespacedVectorAccessCall &&
        hasImportedDefinitionPath(resolveCalleePath(expr));
    const bool isStdNamespacedMapAccessCall =
        hasBuiltinAccessSpelling && !expr.isMethodCall &&
        (resolveCalleePath(expr) == "/std/collections/map/at" ||
         resolveCalleePath(expr) == "/std/collections/map/at_unsafe");
    const bool hasStdNamespacedMapAccessDefinition =
        isStdNamespacedMapAccessCall &&
        hasImportedDefinitionPath(resolveCalleePath(expr));
    const bool shouldAllowStdAccessCompatibilityFallback = false;
    const bool isDirectStdNamespacedVectorCountWrapperMapTarget =
        !expr.isMethodCall && isStdNamespacedVectorCountCall && expr.args.size() == 1 &&
        expr.args.front().kind == Expr::Kind::Call && resolveMapTarget(expr.args.front());
    const bool hasStdNamespacedVectorCountAliasDefinition =
        hasDefinitionPath("/std/collections/vector/count") ||
        hasImportedDefinitionPath("/std/collections/vector/count");
    const bool prefersCanonicalVectorCountAliasDefinition =
        !expr.isMethodCall && resolved == "/vector/count" && !hasDefinitionPath(resolved) &&
        hasDefinitionPath("/std/collections/vector/count");
    const bool allowStdNamespacedVectorUserReceiverProbe =
        !expr.isMethodCall && hasNamedArguments(expr.argNames) && expr.args.size() == 1 &&
        isNamespacedVectorHelperCall &&
        (namespacedHelper == "count" || namespacedHelper == "capacity");
    if (!expr.isMethodCall && isStdNamespacedVectorCountCall &&
        !hasVisibleCanonicalVectorHelperPath("/std/collections/vector/count") &&
        !allowStdNamespacedVectorUserReceiverProbe) {
      error_ = "unknown call target: /std/collections/vector/count";
      return false;
    }
    if (!expr.isMethodCall && isStdNamespacedVectorCapacityCall &&
        !hasVisibleCanonicalVectorHelperPath("/std/collections/vector/capacity") &&
        !allowStdNamespacedVectorUserReceiverProbe) {
      error_ = "unknown call target: /std/collections/vector/capacity";
      return false;
    }
    if (prefersCanonicalVectorCountAliasDefinition) {
      resolved = "/std/collections/vector/count";
    }
    if (!expr.isMethodCall && expr.args.size() > 1 && !hasNamedArguments(expr.argNames) &&
        (isSimpleCallName(expr, "at") || isSimpleCallName(expr, "at_unsafe")) &&
        defMap_.find("/" + expr.name) == defMap_.end()) {
      std::string shadowedReceiverPath;
      if (this->resolveDirectCallTemporaryAccessReceiverPath(expr.args.front(), expr.name, shadowedReceiverPath) ||
          this->resolveLeadingNonCollectionAccessReceiverPath(
              params,
              locals,
              expr.args.front(),
              expr.name,
              builtinCollectionDispatchResolvers,
              shadowedReceiverPath)) {
        error_ = "unknown method: " + shadowedReceiverPath;
        return false;
      }
    }
    if (expr.isMethodCall) {
      auto isExplicitVectorCompatibilityMethodWithTemplateArgs = [&]() {
        if (expr.templateArgs.empty() || expr.args.empty()) {
          return false;
        }
        if (expr.name != "/vector/count" &&
            expr.name != "/std/collections/vector/count") {
          return false;
        }
        std::string elemType;
        return resolveVectorTarget(expr.args.front(), elemType);
      };
      if (isExplicitVectorCompatibilityMethodWithTemplateArgs()) {
        error_ = "unknown method: /vector/count";
        return false;
      }
      if (resolved == "/std/collections/vector/count" &&
          !expr.templateArgs.empty() &&
          !expr.args.empty()) {
        std::string elemType;
        if (resolveVectorTarget(expr.args.front(), elemType)) {
          error_ = "unknown method: /vector/count";
          return false;
        }
      }
      if (resolved == "/std/collections/vector/count" &&
          !expr.hasBodyArguments &&
          expr.bodyArguments.empty() &&
          !expr.args.empty()) {
        std::string elemType;
        if (resolveVectorTarget(expr.args.front(), elemType) &&
            expr.args.size() != 1) {
          error_ = "argument count mismatch for builtin count";
          return false;
        }
      }
      if (expr.namespacePrefix.empty() &&
          !expr.args.empty() &&
          (expr.name == "count" || expr.name == "capacity" ||
           expr.name == "at" || expr.name == "at_unsafe")) {
        std::string elemType;
        const bool isVectorReceiver = resolveVectorTarget(expr.args.front(), elemType);
        const bool hasImplicitSamePathHelper =
            hasDeclaredDefinitionPath("/vector/" + expr.name) ||
            hasImportedDefinitionPath("/vector/" + expr.name);
        const bool hasCanonicalOnlyVectorHelper =
            !hasImplicitSamePathHelper &&
            hasDeclaredDefinitionPath("/std/collections/vector/" + expr.name) &&
            !hasImportedDefinitionPath("/std/collections/vector/" + expr.name);
        if (isVectorReceiver && hasCanonicalOnlyVectorHelper) {
          if (expr.name == "count" && expr.args.size() != 1) {
            error_ = "argument count mismatch for builtin count";
            return false;
          }
          if (expr.name == "capacity" && expr.args.size() != 1) {
            error_ = "argument count mismatch for builtin capacity";
            return false;
          }
          if ((expr.name == "at" || expr.name == "at_unsafe") &&
              expr.args.size() == 2) {
            const ReturnKind indexKind = inferExprReturnKind(expr.args[1], params, locals);
            if (indexKind != ReturnKind::Int &&
                indexKind != ReturnKind::Int64 &&
                indexKind != ReturnKind::UInt64) {
              error_ = expr.name + " requires integer index";
              return false;
            }
          }
        }
      }
      auto rejectBuiltinStringCountShadowOnMapAccessReceiver = [&](const std::string &resolvedPath) -> bool {
        if (resolvedPath != "/string/count" || expr.args.empty()) {
          return false;
        }
        const Expr &receiverExpr = expr.args.front();
        if (receiverExpr.kind != Expr::Kind::Call || !receiverExpr.isMethodCall) {
          return false;
        }
        std::string accessHelperName;
        if (!getBuiltinArrayAccessName(receiverExpr, accessHelperName) ||
            (accessHelperName != "at" && accessHelperName != "at_unsafe")) {
          return false;
        }
        const Expr *accessReceiver = this->resolveBuiltinAccessReceiverExpr(receiverExpr);
        if (accessReceiver == nullptr) {
          return false;
        }
        std::string mapValueType;
        if (!this->resolveMapValueType(*accessReceiver, builtinCollectionDispatchResolvers, mapValueType) ||
            normalizeBindingTypeName(mapValueType) == "string") {
          return false;
        }
        error_ = "argument type mismatch for /string/count parameter values: expected string";
        return true;
      };
      const std::string removedMapMethodPath =
          this->mapNamespacedMethodCompatibilityPath(expr, params, locals, builtinCollectionDispatchResolverAdapters);
      if (!removedMapMethodPath.empty()) {
        error_ = "unknown method: " + removedMapMethodPath;
        return false;
      }
      if (!hasVectorHelperCallResolution) {
        if (expr.args.empty()) {
          error_ = "method call missing receiver";
          return false;
        }
        usedMethodTarget = true;
        hasMethodReceiverIndex = true;
        methodReceiverIndex = 0;
        bool isBuiltinMethod = false;
        const bool hasBlockArgs = expr.hasBodyArguments || !expr.bodyArguments.empty();
        const std::string normalizedMethodNamespace = [&]() {
          std::string normalized = expr.namespacePrefix;
          if (!normalized.empty() && normalized.front() == '/') {
            normalized.erase(normalized.begin());
          }
          return normalized;
        }();
        const bool hasExplicitVectorCompatibilityNamespace =
            normalizedMethodNamespace == "vector" ||
            normalizedMethodNamespace == "std/collections/vector";
        std::string vectorMethodTarget;
        const bool isVectorCompatibilityMethodName =
            expr.name == "count" || expr.name == "capacity" || expr.name == "at" ||
            expr.name == "at_unsafe" || expr.name == "push" || expr.name == "pop" ||
            expr.name == "reserve" || expr.name == "clear" || expr.name == "remove_at" ||
            expr.name == "remove_swap";
        std::string receiverCollectionTypePath;
        std::string experimentalVectorElemType;
        const bool isExperimentalVectorCallReceiver =
            builtinCollectionDispatchResolvers.resolveExperimentalVectorValueTarget != nullptr &&
            builtinCollectionDispatchResolvers.resolveExperimentalVectorValueTarget(
                expr.args.front(), experimentalVectorElemType);
        const std::string preferredVisibleVectorMethodTarget =
            preferVectorStdlibHelperPath(preferredBareVectorHelperTarget(expr.name));
        const bool hasVisiblePreferredVectorMethodTarget =
            hasImportedDefinitionPath(preferredVisibleVectorMethodTarget) ||
            defMap_.count(preferredVisibleVectorMethodTarget) > 0;
        if (isVectorCompatibilityMethodName &&
            !hasExplicitVectorCompatibilityNamespace &&
            expr.args.front().kind == Expr::Kind::Call &&
            !expr.args.front().isBinding &&
            !expr.args.front().isMethodCall &&
            resolveCallCollectionTypePath(expr.args.front(), params, locals, receiverCollectionTypePath) &&
            receiverCollectionTypePath == "/vector" &&
            (isExperimentalVectorCallReceiver || hasVisiblePreferredVectorMethodTarget)) {
          resolved = preferredVisibleVectorMethodTarget;
          isBuiltinMethod = false;
        } else if (isVectorCompatibilityMethodName &&
            !hasExplicitVectorCompatibilityNamespace &&
            resolveVectorHelperMethodTarget(params, locals, expr.args.front(), expr.name,
                                            vectorMethodTarget)) {
          if (vectorMethodTarget.rfind("/std/collections/experimental_vector/", 0) == 0 &&
              !hasImportedDefinitionPath(vectorMethodTarget) &&
              defMap_.count(vectorMethodTarget) == 0) {
            const std::string canonicalVectorMethodTarget =
                "/std/collections/vector/" + expr.name;
            if (hasImportedDefinitionPath(canonicalVectorMethodTarget) ||
                defMap_.count(canonicalVectorMethodTarget) > 0) {
              vectorMethodTarget = canonicalVectorMethodTarget;
            } else {
              const std::string aliasVectorMethodTarget = "/vector/" + expr.name;
              if (hasImportedDefinitionPath(aliasVectorMethodTarget) ||
                  defMap_.count(aliasVectorMethodTarget) > 0) {
                vectorMethodTarget = aliasVectorMethodTarget;
              }
            }
          }
          if (hasImportedDefinitionPath(vectorMethodTarget) ||
              defMap_.count(vectorMethodTarget) > 0) {
            resolved = vectorMethodTarget;
            isBuiltinMethod = false;
          } else if (!resolveMethodTarget(params, locals, expr.namespacePrefix, expr.args.front(),
                                          expr.name, resolved, isBuiltinMethod)) {
            if (hasBlockArgs &&
                resolvePointerLikeMethodTarget(params, locals, expr.args.front(), expr.name, resolved)) {
              isBuiltinMethod = false;
            } else {
              return false;
            }
          }
        } else if (!resolveMethodTarget(params, locals, expr.namespacePrefix, expr.args.front(), expr.name, resolved,
                                        isBuiltinMethod)) {
          if (hasBlockArgs &&
              resolvePointerLikeMethodTarget(params, locals, expr.args.front(), expr.name, resolved)) {
            isBuiltinMethod = false;
          } else {
            return false;
          }
        } else if (rejectBuiltinStringCountShadowOnMapAccessReceiver(resolved)) {
          return false;
        } else if (hasBlockArgs) {
          const std::string pointerLikeType =
              inferPointerLikeCallReturnType(expr.args.front(), params, locals);
          if (!pointerLikeType.empty()) {
            resolved = "/" + pointerLikeType + "/" + normalizeCollectionMethodName(expr.name);
            isBuiltinMethod = false;
          }
        }
        if (!isBuiltinMethod && isVectorCompatibilityMethodName &&
            resolved.rfind("/std/collections/experimental_vector/Vector__", 0) == 0) {
          const std::string canonicalVectorMethodTarget =
              "/std/collections/vector/" + expr.name;
          if (hasImportedDefinitionPath(canonicalVectorMethodTarget) ||
              defMap_.count(canonicalVectorMethodTarget) > 0) {
            resolved = canonicalVectorMethodTarget;
          } else {
            const std::string aliasVectorMethodTarget = "/vector/" + expr.name;
            if (hasImportedDefinitionPath(aliasVectorMethodTarget) ||
                defMap_.count(aliasVectorMethodTarget) > 0) {
              resolved = aliasVectorMethodTarget;
            }
          }
          resolved = preferVectorStdlibHelperPath(resolved);
        }
        bool keepBuiltinIndexedArgsPackMapMethod = false;
        keepBuiltinIndexedArgsPackMapMethod = resolveMapTarget(expr.args.front());
        if (expr.args.front().kind == Expr::Kind::Call) {
          std::string accessName;
          if (getBuiltinArrayAccessName(expr.args.front(), accessName) && expr.args.front().args.size() == 2) {
            if (const Expr *accessReceiver = this->resolveBuiltinAccessReceiverExpr(expr.args.front())) {
              std::string elemType;
              std::string keyType;
              std::string valueType;
              keepBuiltinIndexedArgsPackMapMethod =
                  keepBuiltinIndexedArgsPackMapMethod ||
                  (resolveArgsPackAccessTarget(*accessReceiver, elemType) &&
                   extractMapKeyValueTypesFromTypeText(elemType, keyType, valueType));
            }
          }
        }
        if (((resolved == "/std/collections/map/count" &&
              !hasDeclaredDefinitionPath("/map/count") &&
              !hasImportedDefinitionPath("/std/collections/map/count")) ||
             (resolved == "/std/collections/map/contains" &&
              !hasDeclaredDefinitionPath("/map/contains") &&
              !hasImportedDefinitionPath("/std/collections/map/contains")) ||
             (resolved == "/std/collections/map/tryAt" &&
              !hasDeclaredDefinitionPath("/map/tryAt") &&
              !hasImportedDefinitionPath("/std/collections/map/tryAt")) ||
             (resolved == "/std/collections/map/at" &&
              !hasDeclaredDefinitionPath("/map/at") &&
              !hasImportedDefinitionPath("/std/collections/map/at")) ||
             (resolved == "/std/collections/map/at_unsafe" &&
              !hasDeclaredDefinitionPath("/map/at_unsafe") &&
              !hasImportedDefinitionPath("/std/collections/map/at_unsafe"))) &&
            !hasDeclaredDefinitionPath(resolved) &&
            !keepBuiltinIndexedArgsPackMapMethod) {
          isBuiltinMethod = false;
        }
        std::string builtinVectorReceiverCollection;
        if (isBuiltinMethod && resolved == "/vector/count" &&
            defMap_.find(resolved) == defMap_.end() &&
            !hasImportedDefinitionPath(resolved) &&
            !hasDeclaredDefinitionPath("/std/collections/vector/count") &&
            !hasImportedDefinitionPath("/std/collections/vector/count") &&
            !hasDeclaredDefinitionPath("/std/collections/vectorCount") &&
            !hasImportedDefinitionPath("/std/collections/vectorCount") &&
            !hasNamedArguments(expr.argNames) &&
            (expr.args.front().kind == Expr::Kind::Name ||
             (expr.args.front().kind == Expr::Kind::Call &&
              getBuiltinCollectionName(expr.args.front(), builtinVectorReceiverCollection) &&
              builtinVectorReceiverCollection == "vector"))) {
          std::string elemType;
          if (resolveVectorTarget(expr.args.front(), elemType)) {
            error_ = "unknown method: /vector/count";
            return false;
          }
        }
        if (!isBuiltinMethod && defMap_.find(resolved) == defMap_.end() &&
            isVectorBuiltinName(expr, "capacity") && !isStdNamespacedVectorCapacityCall) {
          promoteCapacityToBuiltinValidation(expr.args.front(), resolved, isBuiltinMethod, true);
        }
        if (!isBuiltinMethod && defMap_.find(resolved) == defMap_.end() &&
            !hasImportedDefinitionPath(resolved) && !hasBlockArgs) {
          if (const std::string diagnostic = experimentalGfxUnavailableMethodDiagnostic(resolved);
              !diagnostic.empty()) {
            error_ = diagnostic;
          } else {
            error_ = "unknown method: " + resolved;
          }
          return false;
        }
        resolvedMethod = isBuiltinMethod;
      } else {
        resolvedMethod = false;
      }
    }
    ExprCollectionCountCapacityDispatchContext collectionCountCapacityDispatchContext;
    collectionCountCapacityDispatchContext.isNamespacedVectorHelperCall =
        isNamespacedVectorHelperCall;
    collectionCountCapacityDispatchContext.namespacedHelper =
        namespacedHelper;
    collectionCountCapacityDispatchContext.isStdNamespacedVectorCountCall =
        isStdNamespacedVectorCountCall;
    collectionCountCapacityDispatchContext
        .shouldBuiltinValidateStdNamespacedVectorCountCall =
        shouldBuiltinValidateStdNamespacedVectorCountCall;
    collectionCountCapacityDispatchContext.isStdNamespacedMapCountCall =
        isStdNamespacedMapCountCall;
    collectionCountCapacityDispatchContext.isNamespacedVectorCountCall =
        isNamespacedVectorCountCall;
    collectionCountCapacityDispatchContext.isNamespacedMapCountCall =
        isNamespacedMapCountCall;
    collectionCountCapacityDispatchContext
        .isUnnamespacedMapCountFallbackCall =
        isUnnamespacedMapCountFallbackCall;
    collectionCountCapacityDispatchContext.isResolvedMapCountCall =
        isResolvedMapCountCall;
    collectionCountCapacityDispatchContext
        .prefersCanonicalVectorCountAliasDefinition =
        prefersCanonicalVectorCountAliasDefinition;
    collectionCountCapacityDispatchContext
        .isStdNamespacedVectorCapacityCall =
        isStdNamespacedVectorCapacityCall;
    collectionCountCapacityDispatchContext
        .shouldBuiltinValidateStdNamespacedVectorCapacityCall =
        shouldBuiltinValidateStdNamespacedVectorCapacityCall;
    collectionCountCapacityDispatchContext.isNamespacedVectorCapacityCall =
        isNamespacedVectorCapacityCall;
    collectionCountCapacityDispatchContext
        .isDirectStdNamespacedVectorCountWrapperMapTarget =
        isDirectStdNamespacedVectorCountWrapperMapTarget;
    collectionCountCapacityDispatchContext
        .hasStdNamespacedVectorCountAliasDefinition =
        hasStdNamespacedVectorCountAliasDefinition;
    collectionCountCapacityDispatchContext.shouldBuiltinValidateBareMapCountCall =
        shouldBuiltinValidateBareMapCountCall;
    collectionCountCapacityDispatchContext.resolveMapTarget =
        [&](const Expr &target) { return resolveMapTarget(target); };
    collectionCountCapacityDispatchContext
        .isArrayNamespacedVectorCountCompatibilityCall =
        [&](const Expr &target) {
          return this->isArrayNamespacedVectorCountCompatibilityCall(
              target, builtinCollectionDispatchResolvers);
        };
    collectionCountCapacityDispatchContext.tryRewriteBareVectorHelperCall =
        [&](const std::string &helperName, Expr &rewrittenOut) {
          return this->tryRewriteBareVectorHelperCall(
              expr, helperName, builtinCollectionDispatchResolvers,
              rewrittenOut);
        };
    collectionCountCapacityDispatchContext
        .promoteCapacityToBuiltinValidation =
        [&](const Expr &targetExpr, std::string &resolvedOut,
            bool &isBuiltinMethodOut, bool requireKnownCollection) {
          promoteCapacityToBuiltinValidation(targetExpr, resolvedOut,
                                             isBuiltinMethodOut,
                                             requireKnownCollection);
        };
    collectionCountCapacityDispatchContext
        .isNonCollectionStructCapacityTarget =
        [&](const std::string &resolvedPath) {
          return isNonCollectionStructCapacityTarget(resolvedPath);
        };
    bool handledCollectionCountCapacityTarget = false;
    std::optional<Expr> rewrittenCollectionCountCapacityCall;
    if (!resolveExprCollectionCountCapacityTarget(
            params, locals, expr, collectionCountCapacityDispatchContext,
            handledCollectionCountCapacityTarget,
            rewrittenCollectionCountCapacityCall, resolved, resolvedMethod,
            usedMethodTarget, hasMethodReceiverIndex, methodReceiverIndex)) {
      return false;
    }
    if (handledCollectionCountCapacityTarget &&
        rewrittenCollectionCountCapacityCall.has_value()) {
      return validateExpr(params, locals,
                          *rewrittenCollectionCountCapacityCall);
    }
    if (!expr.isMethodCall && isStdNamespacedVectorCountCall &&
        expr.args.size() == 1 &&
        !hasDeclaredDefinitionPath("/std/collections/vector/count") &&
        !hasImportedDefinitionPath("/std/collections/vector/count")) {
      std::string elemType;
      if (resolveStringTarget(expr.args.front()) || resolveArrayTarget(expr.args.front(), elemType) ||
          builtinCollectionDispatchResolvers.resolveExperimentalVectorTarget(
              expr.args.front(), elemType)) {
        error_ = "unknown call target: /std/collections/vector/count";
        return false;
      }
    }
    Expr rewrittenVectorHelperCall;
    if (!expr.isMethodCall &&
        !hasNamedArguments(expr.argNames) &&
        expr.args.size() >= 1 &&
        ((expr.namespacePrefix.empty() &&
          (expr.name == "at" || expr.name == "at_unsafe")) ||
         resolved == "/std/collections/vectorAt" ||
         resolved == "/std/collections/vectorAtUnsafe")) {
      const bool isUnsafeHelper =
          expr.name == "at_unsafe" ||
          resolved == "/std/collections/vector/at_unsafe" ||
          resolved == "/vector/at_unsafe" ||
          resolved == "/std/collections/vectorAtUnsafe";
      const std::string helperName = isUnsafeHelper ? "at_unsafe" : "at";
      auto resolvesExperimentalVectorValueReceiverForBareAccess =
          [&](const Expr &receiverExpr, std::string &elemTypeOut) -> bool {
        elemTypeOut.clear();
        std::string receiverTypeText;
        if (!inferQueryExprTypeText(receiverExpr, params, locals, receiverTypeText)) {
          return false;
        }
        BindingInfo inferredBinding;
        const std::string normalizedType = normalizeBindingTypeName(receiverTypeText);
        std::string base;
        std::string argText;
        if (splitTemplateTypeName(normalizedType, base, argText)) {
          inferredBinding.typeName = normalizeBindingTypeName(base);
          inferredBinding.typeTemplateArg = argText;
        } else {
          inferredBinding.typeName = normalizedType;
          inferredBinding.typeTemplateArg.clear();
        }
        const std::string normalizedBase = normalizeBindingTypeName(inferredBinding.typeName);
        if (normalizedBase == "Reference" || normalizedBase == "Pointer") {
          return false;
        }
        return extractExperimentalVectorElementType(inferredBinding, elemTypeOut);
      };
      std::string experimentalElemType;
      if (resolvesExperimentalVectorValueReceiverForBareAccess(expr.args.front(), experimentalElemType)) {
        Expr rewrittenMethodCall = expr;
        rewrittenMethodCall.isMethodCall = true;
        rewrittenMethodCall.name = helperName;
        rewrittenMethodCall.namespacePrefix.clear();
        return validateExpr(params, locals, rewrittenMethodCall);
      }
      bool isBuiltinMethod = false;
      std::string methodResolved;
      if (resolveMethodTarget(params, locals, expr.namespacePrefix, expr.args.front(), helperName,
                              methodResolved, isBuiltinMethod) &&
          !isBuiltinMethod && hasResolvableDefinitionTarget(methodResolved)) {
        Expr rewrittenMethodCall = expr;
        rewrittenMethodCall.isMethodCall = true;
        rewrittenMethodCall.name = helperName;
        rewrittenMethodCall.namespacePrefix.clear();
        return validateExpr(params, locals, rewrittenMethodCall);
      }
      Expr rewrittenBareVectorHelperCall;
      if (this->tryRewriteBareVectorHelperCall(
              expr, helperName, builtinCollectionDispatchResolvers, rewrittenBareVectorHelperCall)) {
        return validateExpr(params, locals, rewrittenBareVectorHelperCall);
      }
    } else if (this->tryRewriteBareVectorHelperCall(
                   expr, "at", builtinCollectionDispatchResolvers, rewrittenVectorHelperCall) ||
               this->tryRewriteBareVectorHelperCall(
                   expr, "at_unsafe", builtinCollectionDispatchResolvers, rewrittenVectorHelperCall)) {
      return validateExpr(params, locals, rewrittenVectorHelperCall);
    }
    ExprCollectionAccessDispatchContext collectionAccessDispatchContext;
    collectionAccessDispatchContext.isNamespacedVectorHelperCall =
        isNamespacedVectorHelperCall;
    collectionAccessDispatchContext.isNamespacedMapHelperCall =
        isNamespacedMapHelperCall;
    collectionAccessDispatchContext.namespacedHelper = namespacedHelper;
    collectionAccessDispatchContext.shouldBuiltinValidateBareMapContainsCall =
        shouldBuiltinValidateBareMapContainsCall;
    collectionAccessDispatchContext.shouldBuiltinValidateBareMapAccessCall =
        shouldBuiltinValidateBareMapAccessCall;
    collectionAccessDispatchContext.resolveArrayTarget =
        [&](const Expr &target, std::string &elemTypeOut) {
          return resolveArrayTarget(target, elemTypeOut);
        };
    collectionAccessDispatchContext.resolveVectorTarget =
        [&](const Expr &target, std::string &elemTypeOut) {
          return resolveVectorTarget(target, elemTypeOut);
        };
    collectionAccessDispatchContext.resolveSoaVectorTarget =
        [&](const Expr &target, std::string &elemTypeOut) {
          return resolveSoaVectorTarget(target, elemTypeOut);
        };
    collectionAccessDispatchContext.resolveStringTarget =
        [&](const Expr &target) { return resolveStringTarget(target); };
    collectionAccessDispatchContext.resolveMapTarget =
        [&](const Expr &target) { return resolveMapTarget(target); };
    collectionAccessDispatchContext.hasResolvableMapHelperPath =
        [&](const std::string &path) {
          return this->hasResolvableMapHelperPath(path);
        };
    collectionAccessDispatchContext.isIndexedArgsPackMapReceiverTarget =
        [&](const Expr &target) {
          return this->isIndexedArgsPackMapReceiverTarget(
              target, builtinCollectionDispatchResolvers);
        };
    bool handledCollectionAccessTarget = false;
    if (!resolveExprCollectionAccessTarget(
            params, locals, expr, collectionAccessDispatchContext,
            handledCollectionAccessTarget, resolved, resolvedMethod,
            usedMethodTarget, hasMethodReceiverIndex,
            methodReceiverIndex)) {
      return false;
    }
    if (usedMethodTarget && !resolvedMethod) {
      auto defIt = defMap_.find(resolved);
      if (defIt != defMap_.end() && isStaticHelperDefinition(*defIt->second)) {
        error_ = "static helper does not accept method-call syntax: " + resolved;
        return false;
      }
    }
    if (!validateExprBodyArguments(params, locals, expr, resolved, resolvedMethod,
                                   builtinCollectionDispatchResolverAdapters,
                                   enclosingStatements, statementIndex)) {
      return false;
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
      if (!currentValidationContext_.definitionIsCompute) {
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
    if (!resolvedMethod && resolved == "/file_error/why" && defMap_.find(resolved) == defMap_.end()) {
      resolvedMethod = true;
    }
    const bool isStdlibBufferLoadWrapperCall =
        resolved.rfind("/std/gfx/Buffer/load", 0) == 0 ||
        resolved.rfind("/std/gfx/experimental/Buffer/load", 0) == 0;
    const bool isStdlibBufferStoreWrapperCall =
        resolved.rfind("/std/gfx/Buffer/store", 0) == 0 ||
        resolved.rfind("/std/gfx/experimental/Buffer/store", 0) == 0;
    if (!currentValidationContext_.definitionIsCompute &&
        (defMap_.find(resolved) != defMap_.end() ||
         hasDeclaredDefinitionPath(resolved) ||
         hasImportedDefinitionPath(resolved)) &&
        (isStdlibBufferLoadWrapperCall || isStdlibBufferStoreWrapperCall)) {
      error_ = isStdlibBufferLoadWrapperCall ? "buffer_load requires a compute definition"
                                             : "buffer_store requires a compute definition";
      return false;
    }

    ExprNamedArgumentBuiltinContext namedArgumentBuiltinContext;
    namedArgumentBuiltinContext.hasVectorHelperCallResolution =
        hasVectorHelperCallResolution;
    namedArgumentBuiltinContext.isNamedArgsPackMethodAccessCall =
        [&](const Expr &target) {
          return this->isNamedArgsPackMethodAccessCall(
              target, builtinCollectionDispatchResolvers);
        };
    namedArgumentBuiltinContext.isNamedArgsPackWrappedFileBuiltinAccessCall =
        [&](const Expr &target) {
          return this->isNamedArgsPackWrappedFileBuiltinAccessCall(
              target, builtinCollectionDispatchResolvers);
        };
    namedArgumentBuiltinContext.isArrayNamespacedVectorCountCompatibilityCall =
        [&](const Expr &target) {
          return this->isArrayNamespacedVectorCountCompatibilityCall(
              target, builtinCollectionDispatchResolvers);
        };
    if (!validateExprNamedArguments(params, locals, expr, resolved,
                                    resolvedMethod,
                                    namedArgumentBuiltinContext)) {
      return false;
    }
    auto it = defMap_.find(resolved);
    if (it == defMap_.end() || resolvedMethod) {
      ExprTryBuiltinContext tryBuiltinContext;
      tryBuiltinContext.getDirectMapHelperCompatibilityPath =
          [&](const Expr &target) {
            return this->directMapHelperCompatibilityPath(
                target, params, locals,
                builtinCollectionDispatchResolverAdapters);
          };
      tryBuiltinContext.isIndexedArgsPackMapReceiverTarget =
          [&](const Expr &target) {
            return this->isIndexedArgsPackMapReceiverTarget(
                target, builtinCollectionDispatchResolvers);
          };
      bool handledTryBuiltin = false;
      if (!validateExprTryBuiltin(params, locals, expr, tryBuiltinContext,
                                  handledTryBuiltin)) {
        return false;
      }
      if (handledTryBuiltin) {
        return true;
      }
      ExprResultFileBuiltinContext resultFileBuiltinContext;
      resultFileBuiltinContext.isNamedArgsPackWrappedFileBuiltinAccessCall =
          [&](const Expr &target) {
            return this->isNamedArgsPackWrappedFileBuiltinAccessCall(
                target, builtinCollectionDispatchResolvers);
          };
      resultFileBuiltinContext.isStringExpr = [&](const Expr &target) {
        return this->isStringExprForArgumentValidation(
            target, builtinCollectionDispatchResolvers);
      };
      bool handledResultFileBuiltin = false;
      if (!validateExprResultFileBuiltins(params, locals, expr, resolved,
                                          resolvedMethod, resultFileBuiltinContext,
                                          handledResultFileBuiltin)) {
        return false;
      }
      if (handledResultFileBuiltin) {
        return true;
      }
      bool handledGpuBufferBuiltin = false;
      if (!validateExprGpuBufferBuiltins(params, locals, expr,
                                         handledGpuBufferBuiltin)) {
        return false;
      }
      if (handledGpuBufferBuiltin) {
        return true;
      }
      std::string logicalResolvedMethod = resolved;
      if (resolvedMethod) {
        std::string canonicalExperimentalMapHelperResolved;
        if (this->canonicalizeExperimentalMapHelperResolvedPath(
                resolved, canonicalExperimentalMapHelperResolved)) {
          logicalResolvedMethod = canonicalExperimentalMapHelperResolved;
        }
      }
      if (resolvedMethod && (logicalResolvedMethod == "/array/count" ||
                             logicalResolvedMethod == "/vector/count" ||
                             logicalResolvedMethod == "/std/collections/vector/count" ||
                             logicalResolvedMethod == "/soa_vector/count" || logicalResolvedMethod == "/string/count" ||
                             logicalResolvedMethod == "/map/count" ||
                             logicalResolvedMethod == "/std/collections/map/count")) {
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
        if (logicalResolvedMethod == "/map/count" ||
            logicalResolvedMethod == "/std/collections/map/count") {
          if (!resolveMapTarget(expr.args.front())) {
            if (!validateExpr(params, locals, expr.args.front())) {
              return false;
            }
            error_ = "count requires map target";
            return false;
          }
        }
        if (!validateExpr(params, locals, expr.args.front())) {
          return false;
        }
        return true;
      }
      if (expr.isMethodCall && resolvedMethod &&
          (logicalResolvedMethod == "/std/collections/map/contains" ||
           logicalResolvedMethod == "/std/collections/map/tryAt" ||
           logicalResolvedMethod == "/std/collections/map/at" ||
           logicalResolvedMethod == "/std/collections/map/at_unsafe")) {
        auto isCanonicalMapTypeText = [&](const std::string &typeText) {
          std::string normalizedType = normalizeBindingTypeName(typeText);
          while (true) {
            std::string base;
            std::string argText;
            if (!splitTemplateTypeName(normalizedType, base, argText)) {
              return false;
            }
            base = normalizeBindingTypeName(base);
            if (base == "Reference" || base == "Pointer") {
              std::vector<std::string> args;
              if (!splitTopLevelTemplateArgs(argText, args) || args.size() != 1) {
                return false;
              }
              normalizedType = normalizeBindingTypeName(args.front());
              continue;
            }
            return base == "std/collections/map" || base == "/std/collections/map";
          }
        };
        auto usesCanonicalMapReceiver = [&](const Expr &receiverExpr) {
          auto bindingTypeText = [](const BindingInfo &binding) {
            if (binding.typeName == "Reference" || binding.typeName == "Pointer") {
              return binding.typeName + "<" + binding.typeTemplateArg + ">";
            }
            if (binding.typeTemplateArg.empty()) {
              return binding.typeName;
            }
            return binding.typeName + "<" + binding.typeTemplateArg + ">";
          };
          if (receiverExpr.kind == Expr::Kind::Name) {
            if (const BindingInfo *paramBinding = findParamBinding(params, receiverExpr.name)) {
              return isCanonicalMapTypeText(bindingTypeText(*paramBinding));
            }
            if (auto it = locals.find(receiverExpr.name); it != locals.end()) {
              return isCanonicalMapTypeText(bindingTypeText(it->second));
            }
          }
          BindingInfo inferredBinding;
          if (inferBindingTypeFromInitializer(receiverExpr, params, locals, inferredBinding)) {
            return isCanonicalMapTypeText(bindingTypeText(inferredBinding));
          }
          std::string typeText;
          return inferQueryExprTypeText(receiverExpr, params, locals, typeText) &&
                 isCanonicalMapTypeText(typeText);
        };
        auto setCanonicalMapKeyMismatch = [&](const Expr &receiverExpr,
                                              const std::string &helperName,
                                              const std::string &mapKeyType) {
          if ((logicalResolvedMethod == "/std/collections/map/at" ||
               logicalResolvedMethod == "/std/collections/map/at_unsafe") &&
              usesCanonicalMapReceiver(receiverExpr)) {
            error_ = "argument type mismatch for " + logicalResolvedMethod + " parameter key";
            return;
          }
          if (normalizeBindingTypeName(mapKeyType) == "string") {
            error_ = helperName + " requires string map key";
          } else {
            error_ = helperName + " requires map key type " + mapKeyType;
          }
        };
        const std::string helperName =
            logicalResolvedMethod.substr(logicalResolvedMethod.find_last_of('/') + 1);
        if (!expr.templateArgs.empty()) {
          error_ = helperName + " does not accept template arguments";
          return false;
        }
        if (expr.hasBodyArguments || !expr.bodyArguments.empty()) {
          error_ = helperName + " does not accept block arguments";
          return false;
        }
        if (expr.args.size() != 2) {
          error_ = "argument count mismatch for builtin " + helperName;
          return false;
        }
        const Expr &receiverExpr = expr.args.front();
        const Expr &keyExpr = expr.args[1];
        std::string mapKeyType;
        if (!this->resolveMapKeyType(receiverExpr, builtinCollectionDispatchResolvers, mapKeyType)) {
          if (!validateExpr(params, locals, receiverExpr)) {
            return false;
          }
          error_ = helperName + " requires map target";
          return false;
        }
        if (!mapKeyType.empty()) {
          if (normalizeBindingTypeName(mapKeyType) == "string") {
            if (!this->isStringExprForArgumentValidation(
                    keyExpr, builtinCollectionDispatchResolvers)) {
              setCanonicalMapKeyMismatch(receiverExpr, helperName, mapKeyType);
              return false;
            }
          } else {
            ReturnKind keyKind = returnKindForTypeName(normalizeBindingTypeName(mapKeyType));
            if (keyKind != ReturnKind::Unknown) {
              if (resolveStringTarget(keyExpr)) {
                setCanonicalMapKeyMismatch(receiverExpr, helperName, mapKeyType);
                return false;
              }
              ReturnKind candidateKind = inferExprReturnKind(keyExpr, params, locals);
              if (candidateKind != ReturnKind::Unknown && candidateKind != keyKind) {
                setCanonicalMapKeyMismatch(receiverExpr, helperName, mapKeyType);
                return false;
              }
            }
          }
        }
        if (!validateExpr(params, locals, receiverExpr) ||
            !validateExpr(params, locals, keyExpr)) {
          return false;
        }
        return true;
      }
      if (!resolvedMethod && isVectorBuiltinName(expr, "count") &&
          !this->isArrayNamespacedVectorCountCompatibilityCall(expr, builtinCollectionDispatchResolvers) &&
          (!shouldBuiltinValidateStdNamespacedVectorCountCall && !isStdNamespacedVectorCountCall) &&
          !isNamespacedMapCountCall && !isResolvedMapCountCall &&
          !this->isUnnamespacedMapCountBuiltinFallbackCall(
              expr, params, locals, builtinCollectionDispatchResolverAdapters) &&
          it == defMap_.end()) {
        if (!shouldBuiltinValidateBareMapCountCall) {
          Expr rewrittenMapHelperCall;
          if (this->tryRewriteBareMapHelperCall(
                  expr, "count", builtinCollectionDispatchResolvers, rewrittenMapHelperCall)) {
            return validateExpr(params, locals, rewrittenMapHelperCall);
          }
        }
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
      if (resolvedMethod &&
          (resolved == "/vector/capacity" ||
           resolved == "/std/collections/vector/capacity")) {
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
      if (!resolvedMethod && isVectorBuiltinName(expr, "capacity") &&
          (!shouldBuiltinValidateStdNamespacedVectorCapacityCall && !isStdNamespacedVectorCapacityCall) &&
          it == defMap_.end()) {
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
      ExprMapSoaBuiltinContext mapSoaBuiltinContext;
      mapSoaBuiltinContext.shouldBuiltinValidateBareMapContainsCall =
          shouldBuiltinValidateBareMapContainsCall;
      mapSoaBuiltinContext.resolveMapKeyType =
          [&](const Expr &target, std::string &mapKeyTypeOut) {
            return this->resolveMapKeyType(
                target, builtinCollectionDispatchResolvers, mapKeyTypeOut);
          };
      mapSoaBuiltinContext.resolveVectorTarget =
          [&](const Expr &target, std::string &elemTypeOut) {
            return resolveVectorTarget(target, elemTypeOut);
          };
      mapSoaBuiltinContext.resolveSoaVectorTarget =
          [&](const Expr &target, std::string &elemTypeOut) {
            return resolveSoaVectorTarget(target, elemTypeOut);
          };
      mapSoaBuiltinContext.resolveStringTarget =
          [&](const Expr &target) {
            return this->isStringExprForArgumentValidation(
                target, builtinCollectionDispatchResolvers);
          };
      mapSoaBuiltinContext.bareMapHelperOperandIndices =
          [&](const Expr &target, size_t &receiverIndexOut,
              size_t &keyIndexOut) {
            return this->bareMapHelperOperandIndices(
                target, builtinCollectionDispatchResolvers,
                receiverIndexOut, keyIndexOut);
          };
      mapSoaBuiltinContext.isNamedArgsPackMethodAccessCall =
          [&](const Expr &target) {
            return this->isNamedArgsPackMethodAccessCall(
                target, builtinCollectionDispatchResolvers);
          };
      mapSoaBuiltinContext.isNamedArgsPackWrappedFileBuiltinAccessCall =
          [&](const Expr &target) {
            return this->isNamedArgsPackWrappedFileBuiltinAccessCall(
                target, builtinCollectionDispatchResolvers);
          };
      bool handledMapSoaBuiltin = false;
      if (!validateExprMapSoaBuiltins(
              params, locals, expr, resolved, resolvedMethod,
              mapSoaBuiltinContext, handledMapSoaBuiltin)) {
        return false;
      }
      if (handledMapSoaBuiltin) {
        return true;
      }
      std::string builtinName;
      bool handledNumericBuiltin = false;
      if (!validateNumericBuiltinExpr(params, locals, expr, handledNumericBuiltin)) {
        return false;
      }
      if (handledNumericBuiltin) {
        return true;
      }
      ExprCollectionAccessValidationContext collectionAccessValidationContext;
      collectionAccessValidationContext.isStdNamespacedVectorAccessCall =
          isStdNamespacedVectorAccessCall;
      collectionAccessValidationContext
          .shouldAllowStdAccessCompatibilityFallback =
          shouldAllowStdAccessCompatibilityFallback;
      collectionAccessValidationContext.hasStdNamespacedVectorAccessDefinition =
          hasStdNamespacedVectorAccessDefinition;
      collectionAccessValidationContext.isStdNamespacedMapAccessCall =
          isStdNamespacedMapAccessCall;
      collectionAccessValidationContext.hasStdNamespacedMapAccessDefinition =
          hasStdNamespacedMapAccessDefinition;
      collectionAccessValidationContext.shouldBuiltinValidateBareMapAccessCall =
          shouldBuiltinValidateBareMapAccessCall;
      collectionAccessValidationContext.resolveArgsPackAccessTarget =
          [&](const Expr &target, std::string &elemTypeOut) {
            return resolveArgsPackAccessTarget(target, elemTypeOut);
          };
      collectionAccessValidationContext.resolveVectorTarget =
          [&](const Expr &target, std::string &elemTypeOut) {
            return resolveVectorTarget(target, elemTypeOut);
          };
      collectionAccessValidationContext.resolveExperimentalVectorValueTarget =
          [&](const Expr &target, std::string &elemTypeOut) {
            return builtinCollectionDispatchResolvers
                .resolveExperimentalVectorValueTarget != nullptr &&
                   builtinCollectionDispatchResolvers
                       .resolveExperimentalVectorValueTarget(target, elemTypeOut);
          };
      collectionAccessValidationContext.resolveArrayTarget =
          [&](const Expr &target, std::string &elemTypeOut) {
            return resolveArrayTarget(target, elemTypeOut);
          };
      collectionAccessValidationContext.resolveStringTarget =
          [&](const Expr &target) { return resolveStringTarget(target); };
      collectionAccessValidationContext.resolveMapKeyType =
          [&](const Expr &target, std::string &mapKeyTypeOut) {
            return this->resolveMapKeyType(
                target, builtinCollectionDispatchResolvers, mapKeyTypeOut);
          };
      collectionAccessValidationContext.resolveExperimentalMapTarget =
          [&](const Expr &target, std::string &keyTypeOut,
              std::string &valueTypeOut) {
            return resolveExperimentalMapTarget(target, keyTypeOut,
                                                valueTypeOut);
          };
      collectionAccessValidationContext.isIndexedArgsPackMapReceiverTarget =
          [&](const Expr &target) {
            return this->isIndexedArgsPackMapReceiverTarget(
                target, builtinCollectionDispatchResolvers);
          };
      collectionAccessValidationContext.isNamedArgsPackMethodAccessCall =
          [&](const Expr &target) {
            return this->isNamedArgsPackMethodAccessCall(
                target, builtinCollectionDispatchResolvers);
          };
      collectionAccessValidationContext
          .isNamedArgsPackWrappedFileBuiltinAccessCall =
          [&](const Expr &target) {
            return this->isNamedArgsPackWrappedFileBuiltinAccessCall(
                target, builtinCollectionDispatchResolvers);
          };
      collectionAccessValidationContext.isMapLikeBareAccessReceiverTarget =
          [&](const Expr &target) {
            return this->isMapLikeBareAccessReceiver(
                target, params, locals, builtinCollectionDispatchResolvers);
          };
      collectionAccessValidationContext.isNonCollectionStructAccessTarget =
          [&](const std::string &targetPath) {
            return isNonCollectionStructAccessTarget(targetPath);
          };
      collectionAccessValidationContext.tryRewriteBareMapHelperCall =
          [&](const Expr &target, const std::string &helperName,
              Expr &rewrittenOut) {
            return this->tryRewriteBareMapHelperCall(
                target, helperName, builtinCollectionDispatchResolvers,
                rewrittenOut);
          };
      bool handledCollectionAccessFallback = false;
      if (!validateExprCollectionAccessFallbacks(
              params, locals, expr, resolved, resolvedMethod,
              collectionAccessValidationContext,
              handledCollectionAccessFallback)) {
        return false;
      }
      if (handledCollectionAccessFallback) {
        return true;
      }
      if (!expr.isMethodCall && getBuiltinArrayAccessName(expr, builtinName) && expr.args.size() == 2 &&
          !hasNamedArguments(expr.argNames)) {
        if (!this->isMapLikeBareAccessReceiver(
                expr.args.front(), params, locals,
                builtinCollectionDispatchResolvers) &&
            this->isMapLikeBareAccessReceiver(
                expr.args[1], params, locals,
                builtinCollectionDispatchResolvers)) {
          Expr rewrittenMapAccessCall = expr;
          std::swap(rewrittenMapAccessCall.args[0], rewrittenMapAccessCall.args[1]);
          return validateExpr(params, locals, rewrittenMapAccessCall);
        }
      }
      bool handledScalarPointerMemoryBuiltin = false;
      if (!validateExprScalarPointerMemoryBuiltins(
              params, locals, expr, handledScalarPointerMemoryBuiltin)) {
        return false;
      }
      if (handledScalarPointerMemoryBuiltin) {
        return true;
      }
      bool handledCollectionLiteralBuiltin = false;
      if (!validateExprCollectionLiteralBuiltins(params, locals, expr,
                                                handledCollectionLiteralBuiltin)) {
        return false;
      }
      if (handledCollectionLiteralBuiltin) {
        return true;
      }
      if (!expr.isMethodCall && getBuiltinArrayAccessName(expr, builtinName) &&
          expr.args.size() == 2) {
        const size_t receiverIndex = this->mapHelperReceiverIndex(expr, builtinCollectionDispatchResolvers);
        if (receiverIndex < expr.args.size()) {
          const Expr &receiverExpr = expr.args[receiverIndex];
          std::string elemType;
          std::string keyType;
          std::string valueType;
          const bool hasCollectionReceiver =
              resolveArgsPackAccessTarget(receiverExpr, elemType) ||
              resolveVectorTarget(receiverExpr, elemType) ||
              resolveArrayTarget(receiverExpr, elemType) ||
              resolveStringTarget(receiverExpr) ||
              resolveMapTarget(receiverExpr) ||
              resolveExperimentalMapTarget(receiverExpr, keyType, valueType);
          if (!hasCollectionReceiver) {
            bool isBuiltinMethod = false;
            std::string methodResolved;
            if (resolveMethodTarget(params, locals, expr.namespacePrefix, receiverExpr, builtinName, methodResolved,
                                    isBuiltinMethod) &&
                !methodResolved.empty()) {
              error_ = "unknown method: " + methodResolved;
            } else {
              error_ = builtinName + " requires array, vector, map, or string target";
            }
            return false;
          }
        }
      }
      auto hasActiveBorrowForBinding = [&](const std::string &name,
                                           const std::string &ignoreBorrowName = std::string()) -> bool {
        if (currentValidationContext_.definitionIsUnsafe) {
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
          if (currentValidationContext_.endedReferenceBorrows.count(borrowName) > 0) {
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
        if (currentValidationContext_.movedBindings.count(target.name) > 0) {
          error_ = "use-after-move: " + target.name;
          return false;
        }
        currentValidationContext_.movedBindings.insert(target.name);
        return true;
      }
      if (isAssignCall(expr)) {
        if (expr.args.size() != 2) {
          error_ = "assign requires exactly two arguments";
          return false;
        }
        const Expr &target = expr.args.front();
        const bool targetIsName = target.kind == Expr::Kind::Name;
        auto isLifecycleHelperPath = [](const std::string &fullPath) -> bool {
          static const std::array<std::string_view, 10> suffixes = {
              "/Create",       "/Destroy",       "/Copy",       "/Move",       "/CreateStack",
              "/DestroyStack", "/CreateHeap",    "/DestroyHeap","/CreateBuffer","/DestroyBuffer"};
          for (std::string_view suffix : suffixes) {
            if (fullPath.size() < suffix.size()) {
              continue;
            }
            if (fullPath.compare(fullPath.size() - suffix.size(), suffix.size(), suffix.data(), suffix.size()) == 0) {
              return true;
            }
          }
          return false;
        };
        auto isNamedFieldTarget = [&](const Expr &candidate, std::string_view rootName) -> bool {
          if (candidate.kind != Expr::Kind::Call || !candidate.isFieldAccess || candidate.args.size() != 1) {
            return false;
          }
          const Expr *receiver = &candidate.args.front();
          while (receiver->kind == Expr::Kind::Call && receiver->isFieldAccess && receiver->args.size() == 1) {
            receiver = &receiver->args.front();
          }
          return receiver->kind == Expr::Kind::Name && receiver->name == rootName;
        };
        auto resolveFieldTargetRootName = [&](const Expr &candidate, std::string &rootNameOut) -> bool {
          if (candidate.kind != Expr::Kind::Call || !candidate.isFieldAccess || candidate.args.size() != 1) {
            return false;
          }
          const Expr *receiver = &candidate.args.front();
          while (receiver->kind == Expr::Kind::Call && receiver->isFieldAccess && receiver->args.size() == 1) {
            receiver = &receiver->args.front();
          }
          if (receiver->kind != Expr::Kind::Name) {
            return false;
          }
          rootNameOut = receiver->name;
          return true;
        };
        auto validateMutableFieldAccessTarget = [&](const Expr &fieldTarget) -> bool {
          if (fieldTarget.kind != Expr::Kind::Call || !fieldTarget.isFieldAccess || fieldTarget.args.size() != 1) {
            error_ = "assign target must be a mutable binding";
            return false;
          }
          if (!validateExpr(params, locals, fieldTarget.args.front())) {
            return false;
          }
          BindingInfo fieldBinding;
          if (!this->resolveStructFieldBinding(
                  params, locals, fieldTarget.args.front(), fieldTarget.name, fieldBinding)) {
            if (error_.empty()) {
              error_ = "assign target must be a mutable binding";
            }
            return false;
          }
          std::string fieldTargetRootName;
          const bool allowMutableReceiverFieldWrite =
              resolveFieldTargetRootName(fieldTarget, fieldTargetRootName) &&
              isMutableBinding(params, locals, fieldTargetRootName);
          const bool allowLifecycleFieldWrite =
              !fieldBinding.isMutable && isNamedFieldTarget(fieldTarget, "this") &&
              isLifecycleHelperPath(currentValidationContext_.definitionPath);
          if (!fieldBinding.isMutable && !allowLifecycleFieldWrite && !allowMutableReceiverFieldWrite) {
            error_ = "assign target must be a mutable binding";
            return false;
          }
          if (fieldTarget.args.front().kind == Expr::Kind::Name &&
              hasActiveBorrowForBinding(fieldTarget.args.front().name)) {
            formatBorrowedBindingError(fieldTarget.args.front().name, fieldTarget.args.front().name);
            return false;
          }
          return true;
        };
        if (target.kind == Expr::Kind::Name) {
          if (!isMutableBinding(params, locals, target.name)) {
            error_ = "assign target must be a mutable binding: " + target.name;
            return false;
          }
          if (hasActiveBorrowForBinding(target.name)) {
            formatBorrowedBindingError(target.name, target.name);
            return false;
          }
        } else if (target.kind == Expr::Kind::Call && target.isFieldAccess) {
          if (!validateMutableFieldAccessTarget(target)) {
            return false;
          }
        } else if (target.kind == Expr::Kind::Call) {
          std::string accessName;
          if (getBuiltinArrayAccessName(target, accessName) && target.args.size() == 2) {
            const Expr &collectionTarget = target.args.front();
            std::string elemType;
            if (!(resolveVectorTarget(collectionTarget, elemType) || resolveArrayTarget(collectionTarget, elemType))) {
              error_ = "assign target must be a mutable binding";
              return false;
            }
            if (collectionTarget.kind == Expr::Kind::Name) {
              if (!isMutableBinding(params, locals, collectionTarget.name)) {
                error_ = "assign target must be a mutable binding: " + collectionTarget.name;
                return false;
              }
              if (hasActiveBorrowForBinding(collectionTarget.name)) {
                formatBorrowedBindingError(collectionTarget.name, collectionTarget.name);
                return false;
              }
            } else if (collectionTarget.kind == Expr::Kind::Call && collectionTarget.isFieldAccess) {
              if (!validateMutableFieldAccessTarget(collectionTarget)) {
                return false;
              }
            } else {
              error_ = "assign target must be a mutable binding";
              return false;
            }
            if (!validateExpr(params, locals, target)) {
              return false;
            }
          } else {
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
          if (currentValidationContext_.definitionIsUnsafe &&
              isUnsafeReferenceExpr(params, locals, expr.args[1])) {
            std::string escapeSink;
            bool hasEscapeSink = false;
            if (pointerExpr.kind == Expr::Kind::Name) {
              hasEscapeSink = resolveReferenceEscapeSink(
                  params, locals, pointerExpr.name, escapeSink);
            }
            if (!hasEscapeSink) {
              std::string locationRootName;
              if (resolveLocationRootBindingName(pointerExpr, locationRootName)) {
                hasEscapeSink = resolveReferenceEscapeSink(
                    params, locals, locationRootName, escapeSink);
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
              if (reportReferenceAssignmentEscape(
                      params, locals, escapeSink, expr.args[1])) {
                return false;
              }
            }
          }
          if (!currentValidationContext_.definitionIsUnsafe) {
            std::string escapeSink;
            bool hasEscapeSink = false;
            if (pointerExpr.kind == Expr::Kind::Name) {
              hasEscapeSink = resolveReferenceEscapeSink(
                  params, locals, pointerExpr.name, escapeSink);
            }
            if (!hasEscapeSink) {
              std::string locationRootName;
              if (resolveLocationRootBindingName(pointerExpr, locationRootName)) {
                hasEscapeSink = resolveReferenceEscapeSink(
                    params, locals, locationRootName, escapeSink);
              }
            }
            if (!hasEscapeSink && !pointerBorrowRoot.empty()) {
              if (const BindingInfo *rootParam = findParamBinding(params, pointerBorrowRoot);
                  rootParam != nullptr && rootParam->typeName == "Reference") {
                hasEscapeSink = true;
                escapeSink = pointerBorrowRoot;
              }
            }
            if (hasEscapeSink &&
                reportReferenceAssignmentEscape(params, locals, escapeSink,
                                               expr.args[1])) {
              return false;
            }
          }
          if (!validateExpr(params, locals, pointerExpr)) {
            return false;
          }
          }
        } else {
          error_ = "assign target must be a mutable binding";
          return false;
        }
        if (currentValidationContext_.definitionIsUnsafe && targetIsName &&
            isUnsafeReferenceExpr(params, locals, expr.args[1])) {
          std::string escapeSink;
          if (resolveReferenceEscapeSink(params, locals, target.name,
                                         escapeSink)) {
            if (reportReferenceAssignmentEscape(params, locals, escapeSink,
                                                expr.args[1])) {
              return false;
            }
          }
        }
        if (!currentValidationContext_.definitionIsUnsafe && targetIsName) {
          std::string escapeSink;
          if (resolveReferenceEscapeSink(params, locals, target.name,
                                         escapeSink) &&
              reportReferenceAssignmentEscape(params, locals, escapeSink,
                                              expr.args[1])) {
            return false;
          }
        }
        if (!validateExpr(params, locals, expr.args[1])) {
          return false;
        }
        if (targetIsName) {
          currentValidationContext_.movedBindings.erase(target.name);
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
        if (!isNumericExpr(params, locals, target)) {
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
      if (!expr.isMethodCall && expr.name.find('/') == std::string::npos) {
        const BindingInfo *callableBinding = findParamBinding(params, expr.name);
        if (callableBinding == nullptr) {
          auto localIt = locals.find(expr.name);
          if (localIt != locals.end()) {
            callableBinding = &localIt->second;
          }
        }
        if (callableBinding != nullptr && callableBinding->typeName == "lambda") {
          if (hasNamedArguments(expr.argNames)) {
            error_ = "named arguments not supported for lambda calls";
            return false;
          }
          if (!expr.templateArgs.empty()) {
            error_ = "lambda calls do not accept template arguments";
            return false;
          }
          if (expr.hasBodyArguments || !expr.bodyArguments.empty()) {
            error_ = "lambda calls do not accept block arguments";
            return false;
          }
          for (const auto &arg : expr.args) {
            if (!validateExpr(params, locals, arg)) {
              return false;
            }
          }
          return true;
        }
      }
      if (!expr.isMethodCall &&
          (resolved == "/std/collections/vector/count" || resolved == "/vector/count") &&
          expr.args.size() == 1 && defMap_.find(resolved) == defMap_.end()) {
        std::string elemType;
        std::string arrayElemType;
        const bool resolvesVector = resolveVectorTarget(expr.args.front(), elemType);
        std::string experimentalElemType;
        const bool resolvesExperimentalVector =
            builtinCollectionDispatchResolvers.resolveExperimentalVectorTarget(
                expr.args.front(), experimentalElemType);
        const bool resolvesArray = resolveArrayTarget(expr.args.front(), arrayElemType);
        const bool resolvesString = resolveStringTarget(expr.args.front());
        const bool resolvesMap = resolveMapTarget(expr.args.front());
        if (!resolvesVector && !resolvesExperimentalVector && !resolvesArray && !resolvesString) {
          if (!validateExpr(params, locals, expr.args.front())) {
            return false;
          }
          if (resolvesMap || resolveMapTarget(expr.args.front())) {
            error_ = "unknown call target: /std/collections/vector/count";
            return false;
          }
          error_ = "count requires vector target";
          return false;
        }
        if (!hasDeclaredDefinitionPath("/vector/count") &&
            !hasDeclaredDefinitionPath("/std/collections/vector/count") &&
            !hasImportedDefinitionPath("/std/collections/vector/count") &&
            (resolvesVector || resolvesExperimentalVector)) {
          error_ = "unknown call target: /std/collections/vector/count";
          return false;
        }
        if (resolved == "/std/collections/vector/count" &&
            hasImportedDefinitionPath("/std/collections/vector/count") &&
            (resolvesVector || resolvesExperimentalVector)) {
          return validateExpr(params, locals, expr.args.front());
        }
      }
      if (!expr.isMethodCall &&
          (resolved == "/std/collections/vector/capacity" || resolved == "/vector/capacity") &&
          expr.args.size() == 1 && defMap_.find(resolved) == defMap_.end()) {
        std::string elemType;
        const bool resolvesVector = resolveVectorTarget(expr.args.front(), elemType);
        std::string experimentalElemType;
        const bool resolvesExperimentalVector =
            builtinCollectionDispatchResolvers.resolveExperimentalVectorTarget(
                expr.args.front(), experimentalElemType);
        if (!resolvesVector && !resolvesExperimentalVector) {
          if (!validateExpr(params, locals, expr.args.front())) {
            return false;
          }
          if (resolveArrayTarget(expr.args.front(), elemType)) {
            error_ = "unknown call target: /std/collections/vector/capacity";
            return false;
          }
          if (resolveMapTarget(expr.args.front())) {
            error_ = "unknown call target: /std/collections/vector/capacity";
            return false;
          }
          error_ = "capacity requires vector target";
          return false;
        }
        if (!hasDeclaredDefinitionPath("/vector/capacity") &&
            !hasDeclaredDefinitionPath("/std/collections/vector/capacity") &&
            !hasImportedDefinitionPath("/std/collections/vector/capacity") &&
            (resolvesVector || resolvesExperimentalVector)) {
          error_ = "unknown call target: /std/collections/vector/capacity";
          return false;
        }
        if (resolved == "/std/collections/vector/capacity" &&
            hasImportedDefinitionPath("/std/collections/vector/capacity")) {
          return validateExpr(params, locals, expr.args.front());
        }
      }
      if (!expr.isMethodCall && getBuiltinArrayAccessName(expr, builtinName) && expr.args.size() == 2 &&
          !hasNamedArguments(expr.argNames)) {
        if (!this->isMapLikeBareAccessReceiver(
                expr.args.front(), params, locals,
                builtinCollectionDispatchResolvers) &&
            this->isMapLikeBareAccessReceiver(
                expr.args[1], params, locals,
                builtinCollectionDispatchResolvers)) {
          Expr rewrittenMapAccessCall = expr;
          std::swap(rewrittenMapAccessCall.args[0], rewrittenMapAccessCall.args[1]);
          return validateExpr(params, locals, rewrittenMapAccessCall);
        }
      }
      if (!expr.isMethodCall && isSimpleCallName(expr, "contains") && !shouldBuiltinValidateBareMapContainsCall) {
        Expr rewrittenMapHelperCall;
        if (this->tryRewriteBareMapHelperCall(
                expr, "contains", builtinCollectionDispatchResolvers, rewrittenMapHelperCall)) {
          return validateExpr(params, locals, rewrittenMapHelperCall);
        }
      }
      if (!expr.isMethodCall && isSimpleCallName(expr, "tryAt") && !shouldBuiltinValidateBareMapTryAtCall) {
        Expr rewrittenMapHelperCall;
        if (this->tryRewriteBareMapHelperCall(
                expr, "tryAt", builtinCollectionDispatchResolvers, rewrittenMapHelperCall)) {
          return validateExpr(params, locals, rewrittenMapHelperCall);
        }
      }
      if (!expr.isMethodCall && getBuiltinArrayAccessName(expr, builtinName) &&
          !shouldBuiltinValidateBareMapAccessCall) {
        Expr rewrittenMapHelperCall;
        if (this->tryRewriteBareMapHelperCall(
                expr, builtinName, builtinCollectionDispatchResolvers, rewrittenMapHelperCall)) {
          return validateExpr(params, locals, rewrittenMapHelperCall);
        }
      }
      auto resolveMapKeyTypeWithInference = [&](const Expr &receiverExpr, std::string &mapKeyTypeOut) {
        if (this->resolveMapKeyType(receiverExpr, builtinCollectionDispatchResolvers, mapKeyTypeOut)) {
          return true;
        }
        std::string receiverTypeText;
        std::string mapValueType;
        return inferQueryExprTypeText(receiverExpr, params, locals, receiverTypeText) &&
               extractMapKeyValueTypesFromTypeText(receiverTypeText, mapKeyTypeOut, mapValueType);
      };
      if (!expr.isMethodCall && isSimpleCallName(expr, "contains") && expr.args.size() == 2 &&
          (shouldBuiltinValidateBareMapContainsCall ||
           this->isMapLikeBareAccessReceiver(
               expr.args[this->mapHelperReceiverIndex(expr, builtinCollectionDispatchResolvers)],
               params, locals, builtinCollectionDispatchResolvers) ||
           this->isIndexedArgsPackMapReceiverTarget(expr.args.front(), builtinCollectionDispatchResolvers))) {
        size_t receiverIndex = 0;
        size_t keyIndex = 1;
        const bool hasBareMapOperands = this->bareMapHelperOperandIndices(
            expr, builtinCollectionDispatchResolvers, receiverIndex, keyIndex);
        const Expr &receiverExpr = hasBareMapOperands ? expr.args[receiverIndex] : expr.args.front();
        const Expr &keyExpr = hasBareMapOperands ? expr.args[keyIndex] : expr.args[1];
        std::string mapKeyType;
        if (!resolveMapKeyTypeWithInference(receiverExpr, mapKeyType)) {
          if (!validateExpr(params, locals, receiverExpr)) {
            return false;
          }
          error_ = "contains requires map target";
          return false;
        }
        if (!mapKeyType.empty()) {
          if (normalizeBindingTypeName(mapKeyType) == "string") {
            if (!this->isStringExprForArgumentValidation(keyExpr, builtinCollectionDispatchResolvers)) {
              error_ = "contains requires string map key";
              return false;
            }
          } else {
            ReturnKind keyKind = returnKindForTypeName(normalizeBindingTypeName(mapKeyType));
            if (keyKind != ReturnKind::Unknown) {
              if (resolveStringTarget(keyExpr)) {
                error_ = "contains requires map key type " + mapKeyType;
                return false;
              }
              ReturnKind candidateKind = inferExprReturnKind(keyExpr, params, locals);
              if (candidateKind != ReturnKind::Unknown && candidateKind != keyKind) {
                error_ = "contains requires map key type " + mapKeyType;
                return false;
              }
            }
          }
        }
        if (!validateExpr(params, locals, expr.args.front()) ||
            !validateExpr(params, locals, expr.args[1])) {
          return false;
        }
        return true;
      }
      if (((expr.isMethodCall && resolved == "/std/collections/map/tryAt") ||
           (!expr.isMethodCall &&
            (isSimpleCallName(expr, "tryAt") || resolved == "/std/collections/map/tryAt"))) &&
          expr.args.size() == 2 &&
          (shouldBuiltinValidateBareMapTryAtCall ||
           this->isMapLikeBareAccessReceiver(
               expr.args[this->mapHelperReceiverIndex(expr, builtinCollectionDispatchResolvers)],
               params, locals, builtinCollectionDispatchResolvers) ||
           this->isIndexedArgsPackMapReceiverTarget(
               expr.args[this->mapHelperReceiverIndex(expr, builtinCollectionDispatchResolvers)],
               builtinCollectionDispatchResolvers))) {
        size_t receiverIndex = 0;
        size_t keyIndex = 1;
        const bool hasBareMapOperands = this->bareMapHelperOperandIndices(
            expr, builtinCollectionDispatchResolvers, receiverIndex, keyIndex);
        const Expr &receiverExpr = hasBareMapOperands ? expr.args[receiverIndex] : expr.args.front();
        const Expr &keyExpr = hasBareMapOperands ? expr.args[keyIndex] : expr.args[1];
        std::string mapKeyType;
        if (!resolveMapKeyTypeWithInference(receiverExpr, mapKeyType)) {
          if (!validateExpr(params, locals, receiverExpr)) {
            return false;
          }
          error_ = "tryAt requires map target";
          return false;
        }
        if (!mapKeyType.empty()) {
          if (normalizeBindingTypeName(mapKeyType) == "string") {
            if (!this->isStringExprForArgumentValidation(keyExpr, builtinCollectionDispatchResolvers)) {
              error_ = "tryAt requires string map key";
              return false;
            }
          } else {
            ReturnKind keyKind = returnKindForTypeName(normalizeBindingTypeName(mapKeyType));
            if (keyKind != ReturnKind::Unknown) {
              if (resolveStringTarget(keyExpr)) {
                error_ = "tryAt requires map key type " + mapKeyType;
                return false;
              }
              ReturnKind candidateKind = inferExprReturnKind(keyExpr, params, locals);
              if (candidateKind != ReturnKind::Unknown && candidateKind != keyKind) {
                error_ = "tryAt requires map key type " + mapKeyType;
                return false;
              }
            }
          }
        }
        if (!validateExpr(params, locals, expr.args.front()) ||
            !validateExpr(params, locals, expr.args[1])) {
          return false;
        }
        return true;
      }
      if (!expr.isMethodCall && getBuiltinArrayAccessName(expr, builtinName) &&
          expr.args.size() == 2 &&
          (shouldBuiltinValidateBareMapAccessCall ||
           this->isMapLikeBareAccessReceiver(
               expr.args[this->mapHelperReceiverIndex(expr, builtinCollectionDispatchResolvers)],
               params, locals, builtinCollectionDispatchResolvers) ||
           this->isIndexedArgsPackMapReceiverTarget(
               expr.args[this->mapHelperReceiverIndex(expr, builtinCollectionDispatchResolvers)],
               builtinCollectionDispatchResolvers))) {
        auto setMapKeyMismatch = [&](const Expr &receiverExpr,
                                     const std::string &mapKeyType) {
          (void)receiverExpr;
          if (expr.name.rfind("/std/collections/map/", 0) == 0 ||
              expr.namespacePrefix == "/std/collections/map" ||
              expr.namespacePrefix == "std/collections/map") {
            error_ = "argument type mismatch for /std/collections/map/" + builtinName + " parameter key";
            return;
          }
          if (normalizeBindingTypeName(mapKeyType) == "string") {
            error_ = builtinName + " requires string map key";
          } else {
            error_ = builtinName + " requires map key type " + mapKeyType;
          }
        };
        size_t receiverIndex = 0;
        size_t keyIndex = 1;
        const bool hasBareMapOperands = this->bareMapHelperOperandIndices(
            expr, builtinCollectionDispatchResolvers, receiverIndex, keyIndex);
        const Expr &receiverExpr = hasBareMapOperands ? expr.args[receiverIndex] : expr.args.front();
        const Expr &keyExpr = hasBareMapOperands ? expr.args[keyIndex] : expr.args[1];
        std::string mapKeyType;
        if (resolveMapKeyTypeWithInference(receiverExpr, mapKeyType)) {
          if (!mapKeyType.empty()) {
            if (normalizeBindingTypeName(mapKeyType) == "string") {
              if (!this->isStringExprForArgumentValidation(keyExpr, builtinCollectionDispatchResolvers)) {
                setMapKeyMismatch(receiverExpr, mapKeyType);
                return false;
              }
            } else {
              ReturnKind keyKind = returnKindForTypeName(normalizeBindingTypeName(mapKeyType));
              if (keyKind != ReturnKind::Unknown) {
                if (resolveStringTarget(keyExpr)) {
                  setMapKeyMismatch(receiverExpr, mapKeyType);
                  return false;
                }
                ReturnKind indexKind = inferExprReturnKind(keyExpr, params, locals);
                if (indexKind != ReturnKind::Unknown && indexKind != keyKind) {
                  setMapKeyMismatch(receiverExpr, mapKeyType);
                  return false;
                }
              }
            }
          }
          if (!validateExpr(params, locals, expr.args.front()) ||
              !validateExpr(params, locals, expr.args[1])) {
            return false;
          }
          return true;
        }
      }
      if (!expr.isMethodCall && getBuiltinArrayAccessName(expr, builtinName) &&
          expr.args.size() == 2 && defMap_.find(resolved) == defMap_.end() &&
          hasImportedDefinitionPath("/std/collections/map/" + builtinName)) {
        auto setMapKeyMismatch = [&](const std::string &receiverTypeText,
                                     const std::string &mapKeyType) {
          (void)receiverTypeText;
          if (expr.name.rfind("/std/collections/map/", 0) == 0 ||
              expr.namespacePrefix == "/std/collections/map" ||
              expr.namespacePrefix == "std/collections/map") {
            error_ = "argument type mismatch for /std/collections/map/" + builtinName + " parameter key";
            return;
          }
          if (normalizeBindingTypeName(mapKeyType) == "string") {
            error_ = builtinName + " requires string map key";
          } else {
            error_ = builtinName + " requires map key type " + mapKeyType;
          }
        };
        size_t receiverIndex = 0;
        size_t keyIndex = 1;
        const bool hasBareMapOperands = this->bareMapHelperOperandIndices(
            expr, builtinCollectionDispatchResolvers, receiverIndex, keyIndex);
        const Expr &receiverExpr = hasBareMapOperands ? expr.args[receiverIndex] : expr.args.front();
        const Expr &keyExpr = hasBareMapOperands ? expr.args[keyIndex] : expr.args[1];
        std::string receiverTypeText;
        std::string mapKeyType;
        std::string mapValueType;
        if (inferQueryExprTypeText(receiverExpr, params, locals, receiverTypeText) &&
            extractMapKeyValueTypesFromTypeText(receiverTypeText, mapKeyType, mapValueType)) {
          if (!mapKeyType.empty()) {
            if (normalizeBindingTypeName(mapKeyType) == "string") {
              if (!this->isStringExprForArgumentValidation(keyExpr, builtinCollectionDispatchResolvers)) {
                setMapKeyMismatch(receiverTypeText, mapKeyType);
                return false;
              }
            } else {
              ReturnKind keyKind = returnKindForTypeName(normalizeBindingTypeName(mapKeyType));
              if (keyKind != ReturnKind::Unknown) {
                if (resolveStringTarget(keyExpr)) {
                  setMapKeyMismatch(receiverTypeText, mapKeyType);
                  return false;
                }
                ReturnKind indexKind = inferExprReturnKind(keyExpr, params, locals);
                if (indexKind != ReturnKind::Unknown && indexKind != keyKind) {
                  setMapKeyMismatch(receiverTypeText, mapKeyType);
                  return false;
                }
              }
            }
          }
          if (!validateExpr(params, locals, expr.args.front()) ||
              !validateExpr(params, locals, expr.args[1])) {
            return false;
          }
          return true;
        }
      }
      error_ = "unknown call target: " + formatUnknownCallTarget(expr);
      return false;
    }
    const auto &calleeParams = paramsByDef_[resolved];
    const std::string diagnosticResolved = diagnosticCallTargetPath(resolved);
    const ExprArgumentValidationContext argumentValidationContext{
        .callExpr = &expr,
        .resolved = &resolved,
        .diagnosticResolved = &diagnosticResolved,
        .params = &params,
        .locals = &locals,
        .dispatchResolvers = &builtinCollectionDispatchResolvers,
    };
    if (calleeParams.empty() && structNames_.count(resolved) > 0) {
      if (expr.args.empty() && !hasNamedArguments(expr.argNames) && !expr.hasBodyArguments &&
          expr.bodyArguments.empty()) {
        if (const std::string diagnostic = experimentalGfxUnavailableConstructorDiagnostic(expr, resolved);
            !diagnostic.empty()) {
          error_ = diagnostic;
          return false;
        }
      }
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
        if (!SemanticsValidator::resolveStructFieldBinding(*it->second, stmt, field.binding)) {
          return false;
        }
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
        if (error_.find("argument count mismatch") != std::string::npos) {
          error_ = "argument count mismatch for " + diagnosticResolved;
        }
        return false;
      }
      std::vector<const Expr *> orderedArgs;
      std::string orderError;
      if (!buildOrderedArguments(fieldParams, expr.args, expr.argNames, orderedArgs, orderError)) {
        if (orderError.find("argument count mismatch") != std::string::npos) {
          error_ = "argument count mismatch for " + diagnosticResolved;
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
      for (size_t argIndex = 0; argIndex < orderedArgs.size() && argIndex < fieldParams.size(); ++argIndex) {
        const Expr *arg = orderedArgs[argIndex];
        if (arg == nullptr || explicitArgs.count(arg) == 0) {
          continue;
        }
        const ParameterInfo &fieldParam = fieldParams[argIndex];
        const std::string expectedTypeText = this->expectedBindingTypeText(fieldParam.binding);
        if (!this->validateArgumentTypeAgainstParam(
                *arg, fieldParam, fieldParam.binding.typeName, expectedTypeText, argumentValidationContext)) {
          return false;
        }
      }
      return true;
    }
    Expr reorderedCallExpr;
    Expr trimmedTypeNamespaceCallExpr;
    const std::vector<Expr> *orderedCallArgs = &expr.args;
    const std::vector<std::optional<std::string>> *orderedCallArgNames = &expr.argNames;
    if (this->isTypeNamespaceMethodCall(params, locals, expr, resolved)) {
      trimmedTypeNamespaceCallExpr = expr;
      trimmedTypeNamespaceCallExpr.args.erase(trimmedTypeNamespaceCallExpr.args.begin());
      if (!trimmedTypeNamespaceCallExpr.argNames.empty()) {
        trimmedTypeNamespaceCallExpr.argNames.erase(trimmedTypeNamespaceCallExpr.argNames.begin());
      }
      orderedCallArgs = &trimmedTypeNamespaceCallExpr.args;
      orderedCallArgNames = &trimmedTypeNamespaceCallExpr.argNames;
    } else if (hasMethodReceiverIndex && methodReceiverIndex > 0 && methodReceiverIndex < expr.args.size()) {
      reorderedCallExpr = expr;
      std::swap(reorderedCallExpr.args[0], reorderedCallExpr.args[methodReceiverIndex]);
      if (reorderedCallExpr.argNames.size() < reorderedCallExpr.args.size()) {
        reorderedCallExpr.argNames.resize(reorderedCallExpr.args.size());
      }
      std::swap(reorderedCallExpr.argNames[0], reorderedCallExpr.argNames[methodReceiverIndex]);
      orderedCallArgs = &reorderedCallExpr.args;
      orderedCallArgNames = &reorderedCallExpr.argNames;
    }
    if (!validateNamedArgumentsAgainstParams(calleeParams, *orderedCallArgNames, error_)) {
      if (error_.find("argument count mismatch") != std::string::npos) {
        error_ = "argument count mismatch for " + diagnosticResolved;
      }
      return false;
    }
    std::vector<const Expr *> orderedArgs;
    std::vector<const Expr *> packedArgs;
    size_t packedParamIndex = calleeParams.size();
    std::string orderError;
    if (!buildOrderedArguments(calleeParams,
                               *orderedCallArgs,
                               *orderedCallArgNames,
                               orderedArgs,
                               packedArgs,
                               packedParamIndex,
                               orderError)) {
      if (orderError.find("argument count mismatch") != std::string::npos) {
        error_ = "argument count mismatch for " + diagnosticResolved;
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
    for (const auto *arg : packedArgs) {
      if (!arg) {
        continue;
      }
      if (!validateExpr(params, locals, *arg)) {
        return false;
      }
    }
    for (size_t paramIndex = 0; paramIndex < calleeParams.size(); ++paramIndex) {
      const ParameterInfo &param = calleeParams[paramIndex];
      if (paramIndex == packedParamIndex) {
        std::string packElementTypeText;
        if (!getArgsPackElementType(param.binding, packElementTypeText)) {
          continue;
        }
        std::string packElementTypeName = packElementTypeText;
        std::string packBase;
        std::string packArgs;
        if (splitTemplateTypeName(packElementTypeText, packBase, packArgs)) {
          packElementTypeName = packBase;
        }
        if (!this->validateArgumentsForParameter(
                param,
                packElementTypeName,
                packElementTypeText,
                packedArgs,
                argumentValidationContext)) {
          return false;
        }
        continue;
      }
      const Expr *arg = paramIndex < orderedArgs.size() ? orderedArgs[paramIndex] : nullptr;
      if (arg == nullptr) {
        continue;
      }
      const std::string &expectedTypeName = param.binding.typeName;
      const std::string expectedTypeText = this->expectedBindingTypeText(param.binding);
      if (!this->validateArgumentTypeAgainstParam(
              *arg, param, expectedTypeName, expectedTypeText, argumentValidationContext)) {
        return false;
      }
    }
    auto isReferenceTypeText = [](const std::string &typeName, const std::string &typeText) {
      if (normalizeBindingTypeName(typeName) == "Reference") {
        return true;
      }
      std::string base;
      std::string argText;
      return splitTemplateTypeName(typeText, base, argText) && normalizeBindingTypeName(base) == "Reference";
    };
    bool calleeIsUnsafe = false;
    for (const auto &transform : it->second->transforms) {
      if (transform.name == "unsafe") {
        calleeIsUnsafe = true;
        break;
      }
    }
    if (currentValidationContext_.definitionIsUnsafe && !calleeIsUnsafe) {
      for (size_t i = 0; i < calleeParams.size(); ++i) {
        const ParameterInfo &param = calleeParams[i];
        if (i == packedParamIndex) {
          std::string packElementTypeText;
          if (!getArgsPackElementType(param.binding, packElementTypeText)) {
            continue;
          }
          std::string packElementTypeName = packElementTypeText;
          std::string packBase;
          std::string packArgs;
          if (splitTemplateTypeName(packElementTypeText, packBase, packArgs)) {
            packElementTypeName = packBase;
          }
          if (!isReferenceTypeText(packElementTypeName, packElementTypeText)) {
            continue;
          }
          for (const Expr *arg : packedArgs) {
            if (!arg || !isUnsafeReferenceExpr(params, locals, *arg)) {
              continue;
            }
            error_ = "unsafe reference escapes across safe boundary to " + resolved;
            return false;
          }
          continue;
        }
        const Expr *arg = i < orderedArgs.size() ? orderedArgs[i] : nullptr;
        if (arg == nullptr) {
          continue;
        }
        const std::string expectedTypeText = param.binding.typeTemplateArg.empty()
                                                 ? param.binding.typeName
                                                 : param.binding.typeName + "<" + param.binding.typeTemplateArg + ">";
        if (!isReferenceTypeText(param.binding.typeName, expectedTypeText)) {
          continue;
        }
        if (!isUnsafeReferenceExpr(params, locals, *arg)) {
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
