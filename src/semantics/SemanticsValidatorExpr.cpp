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
    ExprCollectionDispatchSetup collectionDispatchSetup;
    if (!prepareExprCollectionDispatchSetup(
            params,
            locals,
            expr,
            builtinCollectionDispatchResolvers,
            builtinCollectionDispatchResolverAdapters,
            resolved,
            collectionDispatchSetup)) {
      return false;
    }
    ExprMethodResolutionContext methodResolutionContext;
    methodResolutionContext.hasVectorHelperCallResolution =
        hasVectorHelperCallResolution;
    methodResolutionContext.promoteCapacityToBuiltinValidation =
        [&](const Expr &targetExpr,
            std::string &resolvedOut,
            bool &isBuiltinMethodOut,
            bool requireKnownCollection) {
          promoteCapacityToBuiltinValidation(
              targetExpr, resolvedOut, isBuiltinMethodOut, requireKnownCollection);
        };
    methodResolutionContext.unavailableMethodDiagnostic =
        [&](const std::string &resolvedPath) {
          return experimentalGfxUnavailableMethodDiagnostic(resolvedPath);
        };
    if (!validateExprMethodCallTarget(
            params,
            locals,
            expr,
            methodResolutionContext,
            builtinCollectionDispatchResolvers,
            builtinCollectionDispatchResolverAdapters,
            resolved,
            resolvedMethod,
            usedMethodTarget,
            hasMethodReceiverIndex,
            methodReceiverIndex)) {
      return false;
    }
    ExprCollectionCountCapacityDispatchContext collectionCountCapacityDispatchContext;
    collectionCountCapacityDispatchContext.isNamespacedVectorHelperCall =
        collectionDispatchSetup.isNamespacedVectorHelperCall;
    collectionCountCapacityDispatchContext.namespacedHelper =
        collectionDispatchSetup.namespacedHelper;
    collectionCountCapacityDispatchContext.isStdNamespacedVectorCountCall =
        collectionDispatchSetup.isStdNamespacedVectorCountCall;
    collectionCountCapacityDispatchContext
        .shouldBuiltinValidateStdNamespacedVectorCountCall =
        collectionDispatchSetup.shouldBuiltinValidateStdNamespacedVectorCountCall;
    collectionCountCapacityDispatchContext.isStdNamespacedMapCountCall =
        collectionDispatchSetup.isStdNamespacedMapCountCall;
    collectionCountCapacityDispatchContext.isNamespacedVectorCountCall =
        collectionDispatchSetup.isNamespacedVectorCountCall;
    collectionCountCapacityDispatchContext.isNamespacedMapCountCall =
        collectionDispatchSetup.isNamespacedMapCountCall;
    collectionCountCapacityDispatchContext
        .isUnnamespacedMapCountFallbackCall =
        collectionDispatchSetup.isUnnamespacedMapCountFallbackCall;
    collectionCountCapacityDispatchContext.isResolvedMapCountCall =
        collectionDispatchSetup.isResolvedMapCountCall;
    collectionCountCapacityDispatchContext
        .prefersCanonicalVectorCountAliasDefinition =
        collectionDispatchSetup.prefersCanonicalVectorCountAliasDefinition;
    collectionCountCapacityDispatchContext
        .isStdNamespacedVectorCapacityCall =
        collectionDispatchSetup.isStdNamespacedVectorCapacityCall;
    collectionCountCapacityDispatchContext
        .shouldBuiltinValidateStdNamespacedVectorCapacityCall =
        collectionDispatchSetup
            .shouldBuiltinValidateStdNamespacedVectorCapacityCall;
    collectionCountCapacityDispatchContext.isNamespacedVectorCapacityCall =
        collectionDispatchSetup.isNamespacedVectorCapacityCall;
    collectionCountCapacityDispatchContext
        .isDirectStdNamespacedVectorCountWrapperMapTarget =
        collectionDispatchSetup.isDirectStdNamespacedVectorCountWrapperMapTarget;
    collectionCountCapacityDispatchContext
        .hasStdNamespacedVectorCountAliasDefinition =
        collectionDispatchSetup.hasStdNamespacedVectorCountAliasDefinition;
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
    ExprDirectCollectionFallbackContext directCollectionFallbackContext;
    directCollectionFallbackContext.isStdNamespacedVectorCountCall =
        collectionDispatchSetup.isStdNamespacedVectorCountCall;
    directCollectionFallbackContext.dispatchResolvers =
        &builtinCollectionDispatchResolvers;
    std::optional<Expr> rewrittenDirectCollectionFallbackCall;
    if (!validateExprDirectCollectionFallbacks(
            params,
            locals,
            expr,
            resolved,
            directCollectionFallbackContext,
            rewrittenDirectCollectionFallbackCall)) {
      return false;
    }
    if (rewrittenDirectCollectionFallbackCall.has_value()) {
      return validateExpr(params, locals,
                          *rewrittenDirectCollectionFallbackCall);
    }
    ExprCollectionAccessDispatchContext collectionAccessDispatchContext;
    collectionAccessDispatchContext.isNamespacedVectorHelperCall =
        collectionDispatchSetup.isNamespacedVectorHelperCall;
    collectionAccessDispatchContext.isNamespacedMapHelperCall =
        collectionDispatchSetup.isNamespacedMapHelperCall;
    collectionAccessDispatchContext.namespacedHelper =
        collectionDispatchSetup.namespacedHelper;
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
    ExprLateBuiltinContext lateBuiltinContext;
    lateBuiltinContext.tryBuiltinContext.getDirectMapHelperCompatibilityPath =
        [&](const Expr &target) {
          return this->directMapHelperCompatibilityPath(
              target, params, locals,
              builtinCollectionDispatchResolverAdapters);
        };
    lateBuiltinContext.tryBuiltinContext.isIndexedArgsPackMapReceiverTarget =
        [&](const Expr &target) {
          return this->isIndexedArgsPackMapReceiverTarget(
              target, builtinCollectionDispatchResolvers);
        };
    lateBuiltinContext.resultFileBuiltinContext
        .isNamedArgsPackWrappedFileBuiltinAccessCall =
        [&](const Expr &target) {
          return this->isNamedArgsPackWrappedFileBuiltinAccessCall(
              target, builtinCollectionDispatchResolvers);
        };
    lateBuiltinContext.resultFileBuiltinContext.isStringExpr =
        [&](const Expr &target) {
          return this->isStringExprForArgumentValidation(
              target, builtinCollectionDispatchResolvers);
        };
    bool handledLateBuiltin = false;
    if (!validateExprLateBuiltins(params, locals, expr, resolved,
                                  resolvedMethod, lateBuiltinContext,
                                  handledLateBuiltin)) {
      return false;
    }
    if (handledLateBuiltin) {
      return true;
    }
    ExprCountCapacityMapBuiltinContext countCapacityMapBuiltinContext;
    countCapacityMapBuiltinContext
        .shouldBuiltinValidateStdNamespacedVectorCountCall =
        collectionDispatchSetup.shouldBuiltinValidateStdNamespacedVectorCountCall;
    countCapacityMapBuiltinContext.isStdNamespacedVectorCountCall =
        collectionDispatchSetup.isStdNamespacedVectorCountCall;
    countCapacityMapBuiltinContext.shouldBuiltinValidateBareMapCountCall =
        shouldBuiltinValidateBareMapCountCall;
    countCapacityMapBuiltinContext.isNamespacedMapCountCall =
        collectionDispatchSetup.isNamespacedMapCountCall;
    countCapacityMapBuiltinContext.isResolvedMapCountCall =
        collectionDispatchSetup.isResolvedMapCountCall;
    countCapacityMapBuiltinContext
        .shouldBuiltinValidateStdNamespacedVectorCapacityCall =
        collectionDispatchSetup
            .shouldBuiltinValidateStdNamespacedVectorCapacityCall;
    countCapacityMapBuiltinContext.isStdNamespacedVectorCapacityCall =
        collectionDispatchSetup.isStdNamespacedVectorCapacityCall;
    countCapacityMapBuiltinContext.resolveVectorTarget =
        [&](const Expr &target, std::string &elemTypeOut) {
          return resolveVectorTarget(target, elemTypeOut);
        };
    countCapacityMapBuiltinContext.resolveMapTarget =
        [&](const Expr &target) { return resolveMapTarget(target); };
    countCapacityMapBuiltinContext.dispatchResolverAdapters =
        &builtinCollectionDispatchResolverAdapters;
    countCapacityMapBuiltinContext.dispatchResolvers =
        &builtinCollectionDispatchResolvers;
    bool handledCountCapacityMapBuiltin = false;
    if (!validateExprCountCapacityMapBuiltins(
            params, locals, expr, resolved, resolvedMethod,
            countCapacityMapBuiltinContext, handledCountCapacityMapBuiltin)) {
      return false;
    }
    if (handledCountCapacityMapBuiltin) {
      return true;
    }
    if (it == defMap_.end() || resolvedMethod) {
      ExprLateMapSoaBuiltinContext lateMapSoaBuiltinContext;
      lateMapSoaBuiltinContext.shouldBuiltinValidateBareMapContainsCall =
          shouldBuiltinValidateBareMapContainsCall;
      lateMapSoaBuiltinContext.resolveVectorTarget =
          [&](const Expr &target, std::string &elemTypeOut) {
            return resolveVectorTarget(target, elemTypeOut);
          };
      lateMapSoaBuiltinContext.resolveSoaVectorTarget =
          [&](const Expr &target, std::string &elemTypeOut) {
            return resolveSoaVectorTarget(target, elemTypeOut);
          };
      lateMapSoaBuiltinContext.dispatchResolvers =
          &builtinCollectionDispatchResolvers;
      bool handledMapSoaBuiltin = false;
      if (!validateExprLateMapSoaBuiltins(
              params, locals, expr, resolved, resolvedMethod,
              lateMapSoaBuiltinContext, handledMapSoaBuiltin)) {
        return false;
      }
      if (handledMapSoaBuiltin) {
        return true;
      }
      ExprLateFallbackBuiltinContext lateFallbackBuiltinContext;
      lateFallbackBuiltinContext
          .collectionAccessFallbackContext.isStdNamespacedVectorAccessCall =
          collectionDispatchSetup.isStdNamespacedVectorAccessCall;
      lateFallbackBuiltinContext.collectionAccessFallbackContext
          .shouldAllowStdAccessCompatibilityFallback =
          collectionDispatchSetup.shouldAllowStdAccessCompatibilityFallback;
      lateFallbackBuiltinContext.collectionAccessFallbackContext
          .hasStdNamespacedVectorAccessDefinition =
          collectionDispatchSetup.hasStdNamespacedVectorAccessDefinition;
      lateFallbackBuiltinContext
          .collectionAccessFallbackContext.isStdNamespacedMapAccessCall =
          collectionDispatchSetup.isStdNamespacedMapAccessCall;
      lateFallbackBuiltinContext
          .collectionAccessFallbackContext.hasStdNamespacedMapAccessDefinition =
          collectionDispatchSetup.hasStdNamespacedMapAccessDefinition;
      lateFallbackBuiltinContext.collectionAccessFallbackContext
          .shouldBuiltinValidateBareMapAccessCall =
          shouldBuiltinValidateBareMapAccessCall;
      lateFallbackBuiltinContext
          .collectionAccessFallbackContext.isNonCollectionStructAccessTarget =
          [&](const std::string &targetPath) {
            return isNonCollectionStructAccessTarget(targetPath);
          };
      lateFallbackBuiltinContext
          .collectionAccessFallbackContext.dispatchResolvers =
          &builtinCollectionDispatchResolvers;
      lateFallbackBuiltinContext.dispatchResolvers =
          &builtinCollectionDispatchResolvers;
      bool handledLateFallbackBuiltin = false;
      if (!validateExprLateFallbackBuiltins(
              params, locals, expr, resolved, resolvedMethod,
              lateFallbackBuiltinContext, handledLateFallbackBuiltin)) {
        return false;
      }
      if (handledLateFallbackBuiltin) {
        return true;
      }
      bool handledMutationBorrowBuiltin = false;
      if (!validateExprMutationBorrowBuiltins(
              params, locals, expr, handledMutationBorrowBuiltin)) {
        return false;
      }
      if (handledMutationBorrowBuiltin) {
        return true;
      }
      ExprLateCallCompatibilityContext lateCallCompatibilityContext;
      lateCallCompatibilityContext.dispatchResolvers =
          &builtinCollectionDispatchResolvers;
      bool handledLateCallCompatibility = false;
      if (!validateExprLateCallCompatibility(
              params, locals, expr, resolved,
              lateCallCompatibilityContext, handledLateCallCompatibility)) {
        return false;
      }
      if (handledLateCallCompatibility) {
        return true;
      }
      ExprLateMapAccessBuiltinContext lateMapAccessBuiltinContext;
      lateMapAccessBuiltinContext.dispatchResolvers =
          &builtinCollectionDispatchResolvers;
      lateMapAccessBuiltinContext.shouldBuiltinValidateBareMapContainsCall =
          shouldBuiltinValidateBareMapContainsCall;
      lateMapAccessBuiltinContext.shouldBuiltinValidateBareMapTryAtCall =
          shouldBuiltinValidateBareMapTryAtCall;
      lateMapAccessBuiltinContext.shouldBuiltinValidateBareMapAccessCall =
          shouldBuiltinValidateBareMapAccessCall;
      bool handledLateMapAccessBuiltin = false;
      if (!validateExprLateMapAccessBuiltins(
              params, locals, expr, resolved, lateMapAccessBuiltinContext,
              handledLateMapAccessBuiltin)) {
        return false;
      }
      if (handledLateMapAccessBuiltin) {
        return true;
      }
      if (expr.isMethodCall &&
          (expr.name == "count" || expr.name == "contains" ||
           expr.name == "tryAt" || expr.name == "at" ||
           expr.name == "at_unsafe") &&
          !expr.args.empty() && resolveMapTarget(expr.args.front())) {
        const std::string canonicalMapMethodTarget =
            "/std/collections/map/" + expr.name;
        const std::string aliasMapMethodTarget =
            "/map/" + expr.name;
        Expr rewrittenMapMethodCall = expr;
        rewrittenMapMethodCall.isMethodCall = false;
        rewrittenMapMethodCall.namespacePrefix.clear();
        if (hasDeclaredDefinitionPath(canonicalMapMethodTarget) ||
            hasImportedDefinitionPath(canonicalMapMethodTarget)) {
          rewrittenMapMethodCall.name = canonicalMapMethodTarget;
        } else if (hasDeclaredDefinitionPath(aliasMapMethodTarget) ||
                   hasImportedDefinitionPath(aliasMapMethodTarget)) {
          rewrittenMapMethodCall.name = aliasMapMethodTarget;
        } else {
          rewrittenMapMethodCall.name = canonicalMapMethodTarget;
        }
        return validateExpr(params, locals, rewrittenMapMethodCall);
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
    std::string resolvedStructConstructorZeroArgDiagnostic;
    if (calleeParams.empty() && structNames_.count(resolved) > 0 &&
        expr.args.empty() && !hasNamedArguments(expr.argNames) &&
        !expr.hasBodyArguments && expr.bodyArguments.empty()) {
      resolvedStructConstructorZeroArgDiagnostic =
          experimentalGfxUnavailableConstructorDiagnostic(expr, resolved);
    }
    ExprResolvedStructConstructorContext resolvedStructConstructorContext{
        .isResolvedStructConstructorCall =
            calleeParams.empty() && structNames_.count(resolved) > 0,
        .resolvedDefinition = it->second,
        .argumentValidationContext = &argumentValidationContext,
        .diagnosticResolved = &diagnosticResolved,
        .zeroArgDiagnostic = &resolvedStructConstructorZeroArgDiagnostic,
    };
    bool handledResolvedStructConstructor = false;
    if (!validateExprResolvedStructConstructorCall(
            params, locals, expr, resolved, resolvedStructConstructorContext,
            handledResolvedStructConstructor)) {
      return false;
    }
    if (handledResolvedStructConstructor) {
      return true;
    }
    ExprResolvedCallArgumentContext resolvedCallArgumentContext{
        .resolvedDefinition = it->second,
        .calleeParams = &calleeParams,
        .argumentValidationContext = &argumentValidationContext,
        .diagnosticResolved = &diagnosticResolved,
        .hasMethodReceiverIndex = hasMethodReceiverIndex,
        .methodReceiverIndex = methodReceiverIndex,
    };
    bool handledResolvedCallArguments = false;
    if (!validateExprResolvedCallArguments(
            params, locals, expr, resolved, resolvedCallArgumentContext,
            handledResolvedCallArguments)) {
      return false;
    }
    if (handledResolvedCallArguments) {
      return true;
    }
  }
  return false;
}

} // namespace primec::semantics
