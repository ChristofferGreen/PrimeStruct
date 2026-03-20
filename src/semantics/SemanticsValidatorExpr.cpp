#include "SemanticsValidator.h"
#include "primec/StringLiteral.h"

#include <array>
#include <algorithm>
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
      for (const auto &importPath : program_.imports) {
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
    std::function<bool(const Expr &)> resolveMapTarget;
    resolveMapTarget = [&](const Expr &target) -> bool {
      std::string keyType;
      std::string valueType;
      return resolveMapTargetWithTypes(target, keyType, valueType);
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
      if (earlyPointerBuiltin == "dereference" && !isPointerLikeExpr(expr.args.front(), params, locals)) {
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
    if (!expr.isMethodCall) {
      std::string builtinAccessName;
      if (getBuiltinArrayAccessName(expr, builtinAccessName) && expr.args.size() == 2) {
        std::string keyType;
        std::string valueType;
        if (resolveExperimentalMapTarget(expr.args.front(), keyType, valueType)) {
          error_ = builtinAccessName + " requires integer index";
          return false;
        }
      }
    }
    bool resolvedMethod = false;
    bool usedMethodTarget = false;
    bool hasMethodReceiverIndex = false;
    size_t methodReceiverIndex = 0;
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
      if (receiverPath == "/array" || receiverPath == "/vector" || receiverPath == "/map" || receiverPath == "/string") {
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
      std::string typeReceiverPath;
      const bool typeNamespaceReceiver =
          this->isTypeNamespaceFieldReceiver(params, locals, expr.args.front(), typeReceiverPath);
      if (!typeNamespaceReceiver && !validateExpr(params, locals, expr.args.front())) {
        return false;
      }
      BindingInfo fieldBinding;
      if (!this->resolveStructFieldBinding(params, locals, expr.args.front(), expr.name, fieldBinding)) {
        if (error_.empty()) {
          std::string receiverStructPath;
          if (!this->resolveStructFieldReceiverPath(params, locals, expr.args.front(), receiverStructPath)) {
            error_ = "field access requires struct receiver";
          } else {
            error_ = "unknown field: " + expr.name;
          }
        }
        return false;
      }
      return true;
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
    const bool isResolvedMapAccessCall =
        !expr.isMethodCall && (resolved == "/map/at" || resolved == "/map/at_unsafe") &&
        !isMapNamespacedAccessCompatibilityCall;
    const bool prefersExplicitDirectMapAccessAliasDefinition =
        !expr.isMethodCall &&
        (((isNamespacedMapHelperCall && (namespacedHelper == "at" || namespacedHelper == "at_unsafe")) ||
          (((expr.namespacePrefix == "map") || (expr.namespacePrefix == "/map")) &&
           (expr.name == "at" || expr.name == "at_unsafe")))) &&
        hasDefinitionPath("/map/" +
                          ((expr.name == "at" || expr.name == "at_unsafe") ? expr.name : namespacedHelper));
    const bool shouldAllowStdAccessCompatibilityFallback = false;
    const bool isBuiltinAccessName =
        hasBuiltinAccessSpelling &&
        (!isStdNamespacedVectorAccessCall || shouldAllowStdAccessCompatibilityFallback ||
         hasStdNamespacedVectorAccessDefinition) &&
        !isStdNamespacedMapAccessCall && !isResolvedMapAccessCall;
    const bool isNamespacedVectorAccessCall =
        !isStdNamespacedVectorAccessCall && isBuiltinAccessName && isNamespacedVectorHelperCall &&
        (namespacedHelper == "at" || namespacedHelper == "at_unsafe") &&
        !hasDefinitionPath(resolved);
    const bool shouldSkipStdAccessMethodFallback = false;
    const bool isNamespacedMapAccessCall =
        isBuiltinAccessName && isNamespacedMapHelperCall &&
        (namespacedHelper == "at" || namespacedHelper == "at_unsafe") &&
        !prefersExplicitDirectMapAccessAliasDefinition &&
        !hasDefinitionPath(resolved);
    const bool isDirectStdNamespacedVectorCountWrapperMapTarget =
        !expr.isMethodCall && isStdNamespacedVectorCountCall && expr.args.size() == 1 &&
        expr.args.front().kind == Expr::Kind::Call && resolveMapTarget(expr.args.front());
    const bool hasStdNamespacedVectorCountAliasDefinition =
        hasDefinitionPath("/std/collections/vector/count") ||
        hasImportedDefinitionPath("/std/collections/vector/count");
    const bool prefersCanonicalVectorCountAliasDefinition =
        !expr.isMethodCall && resolved == "/vector/count" && !hasDefinitionPath(resolved) &&
        hasDefinitionPath("/std/collections/vector/count");
    if (prefersCanonicalVectorCountAliasDefinition) {
      resolved = "/std/collections/vector/count";
    }
    if (prefersExplicitDirectMapAccessAliasDefinition) {
      resolved = "/map/" + ((expr.name == "at" || expr.name == "at_unsafe") ? expr.name : namespacedHelper);
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
        if (!resolveMethodTarget(params, locals, expr.namespacePrefix, expr.args.front(), expr.name, resolved,
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
    } else if (hasNamedArguments(expr.argNames) &&
               expr.args.size() == 1 &&
               defMap_.find(resolved) == defMap_.end() &&
               isNamespacedVectorHelperCall &&
               (namespacedHelper == "count" || namespacedHelper == "capacity")) {
      bool isBuiltinMethod = false;
      std::string methodResolved;
      if (resolveMethodTarget(params, locals, expr.namespacePrefix, expr.args.front(), namespacedHelper,
                              methodResolved, isBuiltinMethod) &&
          !isBuiltinMethod && defMap_.find(methodResolved) != defMap_.end()) {
        usedMethodTarget = true;
        hasMethodReceiverIndex = true;
        methodReceiverIndex = 0;
        resolved = methodResolved;
        resolvedMethod = false;
      }
    } else if (isDirectStdNamespacedVectorCountWrapperMapTarget) {
      error_ = "template arguments required for /std/collections/vector/count";
      return false;
    }
    Expr rewrittenVectorHelperCall;
    if (this->tryRewriteBareVectorHelperCall(
            expr, "count", builtinCollectionDispatchResolvers, rewrittenVectorHelperCall) ||
        this->tryRewriteBareVectorHelperCall(
            expr, "capacity", builtinCollectionDispatchResolvers, rewrittenVectorHelperCall)) {
      return validateExpr(params, locals, rewrittenVectorHelperCall);
    }
    if ((isStdNamespacedVectorCountCall && expr.args.size() == 1 && resolveMapTarget(expr.args.front())) &&
               (defMap_.find("/std/collections/vector/count") == defMap_.end() ||
                hasImportedDefinitionPath("/std/collections/vector/count")) &&
               hasStdNamespacedVectorCountAliasDefinition) {
      error_ = "count requires vector target";
      return false;
    } else if (!hasNamedArguments(expr.argNames) &&
               (!isStdNamespacedVectorCountCall || shouldBuiltinValidateStdNamespacedVectorCountCall) &&
               (isVectorBuiltinName(expr, "count") || isStdNamespacedMapCountCall ||
                isNamespacedMapCountCall || isUnnamespacedMapCountFallbackCall || isResolvedMapCountCall) &&
               expr.args.size() == 1 &&
               !this->isArrayNamespacedVectorCountCompatibilityCall(expr, builtinCollectionDispatchResolvers) &&
               !prefersCanonicalVectorCountAliasDefinition &&
               ((defMap_.find(resolved) == defMap_.end() && !isStdNamespacedMapCountCall) ||
                (isNamespacedVectorCountCall && !isStdNamespacedVectorCountCall) ||
                isStdNamespacedMapCountCall || isNamespacedMapCountCall || isUnnamespacedMapCountFallbackCall ||
                isResolvedMapCountCall)) {
      usedMethodTarget = true;
      hasMethodReceiverIndex = true;
      methodReceiverIndex = 0;
      bool isBuiltinMethod = false;
      std::string methodResolved;
      if (isUnnamespacedMapCountFallbackCall &&
          !hasDeclaredDefinitionPath("/std/collections/map/count") &&
          !hasDeclaredDefinitionPath("/map/count") &&
          !hasImportedDefinitionPath("/std/collections/map/count") &&
          resolveMapTarget(expr.args.front())) {
        methodResolved = "/std/collections/map/count";
        isBuiltinMethod = true;
      } else if (!resolveMethodTarget(params, locals, expr.namespacePrefix, expr.args.front(), "count",
                                      methodResolved, isBuiltinMethod)) {
        // Preserve receiver diagnostics (for example unknown call target)
        // when collection-target resolution fails.
        (void)validateExpr(params, locals, expr.args.front());
        return false;
      }
      if (isBuiltinMethod && methodResolved == "/std/collections/map/count" &&
          !hasDeclaredDefinitionPath("/map/count") &&
          !hasImportedDefinitionPath("/std/collections/map/count") &&
          !hasDeclaredDefinitionPath("/std/collections/map/count") &&
          !shouldBuiltinValidateBareMapCountCall &&
          !resolveMapTarget(expr.args.front())) {
        error_ = "unknown call target: /std/collections/map/count";
        return false;
      }
      if (!isBuiltinMethod && defMap_.find(methodResolved) == defMap_.end()) {
        error_ = "unknown method: " + methodResolved;
        return false;
      }
      resolved = methodResolved;
      resolvedMethod = isBuiltinMethod;
    } else if (!hasNamedArguments(expr.argNames) &&
               (!isStdNamespacedVectorCountCall || shouldBuiltinValidateStdNamespacedVectorCountCall) &&
               ((isVectorBuiltinName(expr, "count") && isNamespacedVectorHelperCall &&
                 (!isStdNamespacedVectorCountCall || shouldBuiltinValidateStdNamespacedVectorCountCall)) ||
                isStdNamespacedMapCountCall || isNamespacedMapCountCall ||
                isUnnamespacedMapCountFallbackCall || isResolvedMapCountCall) &&
               !expr.args.empty() && expr.args.size() != 1 &&
               !prefersCanonicalVectorCountAliasDefinition &&
               (defMap_.find(resolved) != defMap_.end() || isStdNamespacedMapCountCall ||
                isNamespacedMapCountCall || isUnnamespacedMapCountFallbackCall || isResolvedMapCountCall)) {
      usedMethodTarget = true;
      hasMethodReceiverIndex = true;
      methodReceiverIndex = 0;
      bool isBuiltinMethod = false;
      std::string methodResolved;
      if (isUnnamespacedMapCountFallbackCall &&
          !hasDeclaredDefinitionPath("/std/collections/map/count") &&
          !hasDeclaredDefinitionPath("/map/count") &&
          !hasImportedDefinitionPath("/std/collections/map/count") &&
          resolveMapTarget(expr.args.front())) {
        methodResolved = "/std/collections/map/count";
        isBuiltinMethod = true;
      } else if (!resolveMethodTarget(params, locals, expr.namespacePrefix, expr.args.front(), "count",
                                      methodResolved, isBuiltinMethod)) {
        (void)validateExpr(params, locals, expr.args.front());
        return false;
      }
      if (isBuiltinMethod && methodResolved == "/std/collections/map/count" &&
          !hasDeclaredDefinitionPath("/map/count") &&
          !hasImportedDefinitionPath("/std/collections/map/count") &&
          !hasDeclaredDefinitionPath("/std/collections/map/count") &&
          !shouldBuiltinValidateBareMapCountCall &&
          !resolveMapTarget(expr.args.front())) {
        error_ = "unknown call target: /std/collections/map/count";
        return false;
      }
      if (!isBuiltinMethod && defMap_.find(methodResolved) == defMap_.end()) {
        error_ = "unknown method: " + methodResolved;
        return false;
      }
      resolved = methodResolved;
      resolvedMethod = isBuiltinMethod;
    } else if (!hasNamedArguments(expr.argNames) &&
               !shouldSkipStdCapacityMethodFallback &&
               (!isStdNamespacedVectorCapacityCall || shouldBuiltinValidateStdNamespacedVectorCapacityCall) &&
               isVectorBuiltinName(expr, "capacity") && isNamespacedVectorHelperCall && !expr.args.empty() &&
               expr.args.size() != 1 && defMap_.find(resolved) != defMap_.end()) {
      usedMethodTarget = true;
      hasMethodReceiverIndex = true;
      methodReceiverIndex = 0;
      bool isBuiltinMethod = false;
      std::string methodResolved;
      if (!resolveMethodTarget(params, locals, expr.namespacePrefix, expr.args.front(), "capacity",
                               methodResolved, isBuiltinMethod)) {
        (void)validateExpr(params, locals, expr.args.front());
        return false;
      }
      if (!isBuiltinMethod && defMap_.find(methodResolved) == defMap_.end()) {
        error_ = "unknown method: " + methodResolved;
        return false;
      }
      resolved = methodResolved;
      resolvedMethod = isBuiltinMethod;
    } else if (!hasNamedArguments(expr.argNames) &&
               !shouldSkipStdCapacityMethodFallback &&
               (!isStdNamespacedVectorCapacityCall || shouldBuiltinValidateStdNamespacedVectorCapacityCall) &&
               isVectorBuiltinName(expr, "capacity") && expr.args.size() == 1 &&
               (defMap_.find(resolved) == defMap_.end() || isNamespacedVectorCapacityCall)) {
      usedMethodTarget = true;
      hasMethodReceiverIndex = true;
      methodReceiverIndex = 0;
      bool isBuiltinMethod = false;
      std::string methodResolved;
      if (!resolveMethodTarget(params, locals, expr.namespacePrefix, expr.args.front(), "capacity",
                               methodResolved, isBuiltinMethod)) {
        // Preserve receiver diagnostics (for example unknown call target)
        // when collection-target resolution fails.
        (void)validateExpr(params, locals, expr.args.front());
        return false;
      }
      if (!isBuiltinMethod && defMap_.find(methodResolved) == defMap_.end()) {
        if (!isNonCollectionStructCapacityTarget(methodResolved)) {
          promoteCapacityToBuiltinValidation(expr.args.front(), methodResolved, isBuiltinMethod, false);
        }
      }
      if (!isBuiltinMethod && defMap_.find(methodResolved) == defMap_.end()) {
        error_ = "unknown method: " + methodResolved;
        return false;
      }
      resolved = methodResolved;
      resolvedMethod = isBuiltinMethod;
    } else if (this->tryRewriteBareVectorHelperCall(
                   expr, "at", builtinCollectionDispatchResolvers, rewrittenVectorHelperCall) ||
               this->tryRewriteBareVectorHelperCall(
                   expr, "at_unsafe", builtinCollectionDispatchResolvers, rewrittenVectorHelperCall)) {
      return validateExpr(params, locals, rewrittenVectorHelperCall);
    } else if (isBuiltinAccessName &&
               !(isStdNamespacedVectorAccessCall && hasNamedArguments(expr.argNames)) &&
               (defMap_.find(resolved) == defMap_.end() ||
                resolved == "/map/at" || resolved == "/map/at_unsafe" ||
                resolved == "/std/collections/map/at" || resolved == "/std/collections/map/at_unsafe" ||
                (isNamespacedVectorAccessCall && !shouldSkipStdAccessMethodFallback) ||
                isNamespacedMapAccessCall)) {
      std::vector<size_t> receiverIndices;
      auto appendReceiverIndex = [&](size_t index) {
        if (index >= expr.args.size()) {
          return;
        }
        for (size_t existing : receiverIndices) {
          if (existing == index) {
            return;
          }
        }
        receiverIndices.push_back(index);
      };
      const bool hasNamedArgs = hasNamedArguments(expr.argNames);
      if (hasNamedArgs) {
        bool hasValuesNamedReceiver = false;
        for (size_t i = 0; i < expr.args.size(); ++i) {
          if (i < expr.argNames.size() && expr.argNames[i].has_value() && *expr.argNames[i] == "values") {
            appendReceiverIndex(i);
            hasValuesNamedReceiver = true;
          }
        }
        if (!hasValuesNamedReceiver) {
          appendReceiverIndex(0);
          for (size_t i = 1; i < expr.args.size(); ++i) {
            appendReceiverIndex(i);
          }
        }
      } else {
        appendReceiverIndex(0);
      }
      auto isMapLikeAccessReceiverExpr = [&](const Expr &candidate) -> bool {
        if (resolveMapTarget(candidate)) {
          return true;
        }
        if (candidate.kind != Expr::Kind::Call) {
          return false;
        }
        auto defIt = defMap_.find(resolveCalleePath(candidate));
        if ((defIt == defMap_.end() || defIt->second == nullptr) &&
            !candidate.name.empty() && candidate.name.find('/') == std::string::npos) {
          defIt = defMap_.find("/" + candidate.name);
        }
        if (defIt != defMap_.end() && defIt->second != nullptr) {
          BindingInfo inferredReturn;
          if (inferDefinitionReturnBinding(*defIt->second, inferredReturn)) {
            const std::string inferredTypeText =
                inferredReturn.typeTemplateArg.empty()
                    ? inferredReturn.typeName
                    : inferredReturn.typeName + "<" + inferredReturn.typeTemplateArg + ">";
            if (returnsMapCollectionType(inferredTypeText)) {
              return true;
            }
          }
          for (const auto &transform : defIt->second->transforms) {
            if (transform.name == "return" && transform.templateArgs.size() == 1 &&
                returnsMapCollectionType(transform.templateArgs.front())) {
              return true;
            }
          }
        }
        std::string receiverTypeText;
        return inferQueryExprTypeText(candidate, params, locals, receiverTypeText) &&
               returnsMapCollectionType(receiverTypeText);
      };
      auto isCollectionAccessReceiverExpr = [&](const Expr &candidate) -> bool {
        std::string elemType;
        return resolveVectorTarget(candidate, elemType) || resolveArrayTarget(candidate, elemType) ||
               resolveStringTarget(candidate) || isMapLikeAccessReceiverExpr(candidate);
      };
      const bool probePositionalReorderedReceiver =
          !hasNamedArgs && expr.args.size() > 1 &&
          (expr.args.front().kind == Expr::Kind::Literal || expr.args.front().kind == Expr::Kind::BoolLiteral ||
           expr.args.front().kind == Expr::Kind::FloatLiteral || expr.args.front().kind == Expr::Kind::StringLiteral ||
           (expr.args.front().kind == Expr::Kind::Name &&
            !isCollectionAccessReceiverExpr(expr.args.front())));
      if (probePositionalReorderedReceiver) {
        for (size_t i = 1; i < expr.args.size(); ++i) {
          appendReceiverIndex(i);
        }
      }
      std::optional<std::string> deferredMethodResolved;
      bool deferredMethodIsBuiltin = false;
      for (size_t receiverIndex : receiverIndices) {
        const Expr &receiverCandidate = expr.args[receiverIndex];
        std::string elemType;
        if (!(resolveVectorTarget(receiverCandidate, elemType) || resolveArrayTarget(receiverCandidate, elemType) ||
              resolveStringTarget(receiverCandidate) || isMapLikeAccessReceiverExpr(receiverCandidate))) {
          continue;
        }
        usedMethodTarget = true;
        bool isBuiltinMethod = false;
        std::string methodResolved;
        if (!resolveMethodTarget(params, locals, expr.namespacePrefix, receiverCandidate, accessHelperName,
                                 methodResolved, isBuiltinMethod)) {
          // Preserve receiver diagnostics (for example unknown call target)
          // when collection-target resolution fails.
          (void)validateExpr(params, locals, receiverCandidate);
          return false;
        }
        if (!isBuiltinMethod && defMap_.find(methodResolved) == defMap_.end()) {
          error_ = "unknown method: " + methodResolved;
          return false;
        }
        if (probePositionalReorderedReceiver && receiverIndex == 0) {
          deferredMethodResolved = methodResolved;
          deferredMethodIsBuiltin = isBuiltinMethod;
          continue;
        }
        resolved = methodResolved;
        resolvedMethod = isBuiltinMethod;
        hasMethodReceiverIndex = true;
        methodReceiverIndex = receiverIndex;
        break;
      }
      if (!hasMethodReceiverIndex && deferredMethodResolved.has_value()) {
        usedMethodTarget = true;
        resolved = *deferredMethodResolved;
        resolvedMethod = deferredMethodIsBuiltin;
        hasMethodReceiverIndex = true;
        methodReceiverIndex = 0;
      }
      if (!hasMethodReceiverIndex && !expr.args.empty() &&
          (expr.args.front().kind == Expr::Kind::Name || expr.args.front().kind == Expr::Kind::Call)) {
        bool isBuiltinMethod = false;
        std::string methodResolved;
        if (resolveMethodTarget(params, locals, expr.namespacePrefix, expr.args.front(), accessHelperName,
                                methodResolved, isBuiltinMethod)) {
          if (isBuiltinMethod) {
            usedMethodTarget = true;
            hasMethodReceiverIndex = true;
            methodReceiverIndex = 0;
            resolved = methodResolved;
            resolvedMethod = true;
          } else {
            const size_t methodSlash = methodResolved.find_last_of('/');
            const bool hasStructReceiver = methodSlash != std::string::npos && methodSlash > 0 &&
                                           structNames_.count(methodResolved.substr(0, methodSlash)) > 0;
            if (hasStructReceiver) {
              usedMethodTarget = true;
              hasMethodReceiverIndex = true;
              methodReceiverIndex = 0;
              if (defMap_.find(methodResolved) == defMap_.end()) {
                error_ = "unknown method: " + methodResolved;
                return false;
              }
              resolved = methodResolved;
              resolvedMethod = false;
            }
          }
        } else if (
            this->resolveDirectCallTemporaryAccessReceiverPath(
                expr.args.front(), accessHelperName, methodResolved) ||
            this->resolveLeadingNonCollectionAccessReceiverPath(
                params,
                locals,
                expr.args.front(),
                accessHelperName,
                builtinCollectionDispatchResolvers,
                methodResolved)) {
          error_ = "unknown method: " + methodResolved;
          return false;
        }
      }
    } else if ((isSimpleCallName(expr, "get") || isSimpleCallName(expr, "ref")) && expr.args.size() == 2 &&
               defMap_.find(resolved) == defMap_.end()) {
      std::vector<size_t> receiverIndices;
      auto appendReceiverIndex = [&](size_t index) {
        if (index >= expr.args.size()) {
          return;
        }
        for (size_t existing : receiverIndices) {
          if (existing == index) {
            return;
          }
        }
        receiverIndices.push_back(index);
      };
      const bool hasNamedArgs = hasNamedArguments(expr.argNames);
      if (hasNamedArgs) {
        bool hasValuesNamedReceiver = false;
        for (size_t i = 0; i < expr.args.size(); ++i) {
          if (i < expr.argNames.size() && expr.argNames[i].has_value() && *expr.argNames[i] == "values") {
            appendReceiverIndex(i);
            hasValuesNamedReceiver = true;
          }
        }
        if (!hasValuesNamedReceiver) {
          appendReceiverIndex(0);
          for (size_t i = 1; i < expr.args.size(); ++i) {
            appendReceiverIndex(i);
          }
        }
      } else {
        appendReceiverIndex(0);
      }
      for (size_t receiverIndex : receiverIndices) {
        const Expr &receiverCandidate = expr.args[receiverIndex];
        std::string methodTarget;
        if (resolveVectorHelperMethodTarget(params, locals, receiverCandidate, expr.name, methodTarget)) {
          methodTarget = preferVectorStdlibHelperPath(methodTarget);
          if (defMap_.count(methodTarget) > 0) {
            usedMethodTarget = true;
            resolved = methodTarget;
            resolvedMethod = false;
            hasMethodReceiverIndex = true;
            methodReceiverIndex = receiverIndex;
            break;
          }
        }
        std::string elemType;
        if (!resolveSoaVectorTarget(receiverCandidate, elemType)) {
          continue;
        }
        usedMethodTarget = true;
        bool isBuiltinMethod = false;
        std::string methodResolved;
        if (!resolveMethodTarget(params, locals, expr.namespacePrefix, receiverCandidate, expr.name,
                                 methodResolved, isBuiltinMethod)) {
          (void)validateExpr(params, locals, receiverCandidate);
          return false;
        }
        if (!isBuiltinMethod && defMap_.find(methodResolved) == defMap_.end()) {
          error_ = "unknown method: " + methodResolved;
          return false;
        }
        resolved = methodResolved;
        resolvedMethod = isBuiltinMethod;
        hasMethodReceiverIndex = true;
        methodReceiverIndex = receiverIndex;
        break;
      }
    } else if (expr.args.size() == 1 && defMap_.find(resolved) == defMap_.end() &&
               !isSimpleCallName(expr, "to_soa") && !isSimpleCallName(expr, "to_aos")) {
      const Expr &receiverCandidate = expr.args.front();
      std::string elemType;
      if (resolveSoaVectorTarget(receiverCandidate, elemType)) {
        usedMethodTarget = true;
        bool isBuiltinMethod = false;
        std::string methodResolved;
        if (!resolveMethodTarget(params, locals, expr.namespacePrefix, receiverCandidate, expr.name,
                                 methodResolved, isBuiltinMethod)) {
          (void)validateExpr(params, locals, receiverCandidate);
          return false;
        }
        if (!isBuiltinMethod && defMap_.find(methodResolved) == defMap_.end()) {
          error_ = "unknown method: " + methodResolved;
          return false;
        }
        resolved = methodResolved;
        resolvedMethod = isBuiltinMethod;
        hasMethodReceiverIndex = true;
        methodReceiverIndex = 0;
      }
    } else if (!expr.isMethodCall && expr.args.size() == 2 && defMap_.find(resolved) == defMap_.end() &&
               (isSimpleCallName(expr, "contains") || getBuiltinArrayAccessName(expr, accessHelperName))) {
      const Expr &receiverCandidate = expr.args.front();
        std::string keyType;
        std::string valueType;
      const bool isMapReceiver = resolveMapTarget(receiverCandidate);
      const bool isExperimentalMapReceiver =
          resolveExperimentalMapTarget(receiverCandidate, keyType, valueType);
      if (isMapReceiver || isExperimentalMapReceiver) {
        if (getBuiltinArrayAccessName(expr, accessHelperName) && isExperimentalMapReceiver) {
          error_ = accessHelperName + " requires integer index";
          return false;
        }
        usedMethodTarget = true;
        bool isBuiltinMethod = false;
        std::string methodResolved;
        if (!resolveMethodTarget(params, locals, expr.namespacePrefix, receiverCandidate, expr.name,
                                 methodResolved, isBuiltinMethod)) {
          (void)validateExpr(params, locals, receiverCandidate);
          return false;
        }
        if (isBuiltinMethod) {
          if (((methodResolved == "/std/collections/map/contains" &&
                (hasDeclaredDefinitionPath("/map/contains") ||
                 hasDeclaredDefinitionPath("/std/collections/map/contains"))) ||
               (methodResolved == "/std/collections/map/at" &&
                (hasDeclaredDefinitionPath("/map/at") ||
                 hasDeclaredDefinitionPath("/std/collections/map/at"))) ||
               (methodResolved == "/std/collections/map/at_unsafe" &&
                (hasDeclaredDefinitionPath("/map/at_unsafe") ||
                 hasDeclaredDefinitionPath("/std/collections/map/at_unsafe")))) &&
              !(methodResolved == "/std/collections/map/contains" && shouldBuiltinValidateBareMapContainsCall) &&
              !((methodResolved == "/std/collections/map/at" ||
                 methodResolved == "/std/collections/map/at_unsafe") &&
                shouldBuiltinValidateBareMapAccessCall) &&
              !this->isIndexedArgsPackMapReceiverTarget(receiverCandidate, builtinCollectionDispatchResolvers)) {
            isBuiltinMethod = false;
          }
        }
        if (isBuiltinMethod) {
          if (methodResolved == "/std/collections/map/contains" &&
              !shouldBuiltinValidateBareMapContainsCall &&
              !this->isIndexedArgsPackMapReceiverTarget(receiverCandidate, builtinCollectionDispatchResolvers) &&
              !hasDeclaredDefinitionPath("/map/contains") &&
              !hasImportedDefinitionPath("/std/collections/map/contains") &&
              !hasDeclaredDefinitionPath("/std/collections/map/contains")) {
            error_ = "unknown call target: /std/collections/map/contains";
            return false;
          }
          if ((methodResolved == "/map/at" || methodResolved == "/map/at_unsafe" ||
               methodResolved == "/std/collections/map/at" || methodResolved == "/std/collections/map/at_unsafe") &&
              !shouldBuiltinValidateBareMapAccessCall &&
              !this->isIndexedArgsPackMapReceiverTarget(receiverCandidate, builtinCollectionDispatchResolvers) &&
              !hasDeclaredDefinitionPath("/map/" +
                                         std::string(methodResolved.find("unsafe") != std::string::npos ? "at_unsafe"
                                                                                                         : "at")) &&
              !hasImportedDefinitionPath("/std/collections/map/" +
                                         std::string(methodResolved.find("unsafe") != std::string::npos ? "at_unsafe"
                                                                                                         : "at")) &&
              !hasDeclaredDefinitionPath("/std/collections/map/" +
                                         std::string(methodResolved.find("unsafe") != std::string::npos ? "at_unsafe"
                                                                                                         : "at"))) {
            error_ = "unknown call target: /std/collections/map/" +
                     std::string(methodResolved.find("unsafe") != std::string::npos ? "at_unsafe" : "at");
            return false;
          }
        } else if (defMap_.find(methodResolved) == defMap_.end() &&
                   !this->hasResolvableMapHelperPath(methodResolved)) {
          error_ = "unknown method: " + methodResolved;
          return false;
        }
        resolved = methodResolved;
        resolvedMethod = isBuiltinMethod;
        hasMethodReceiverIndex = true;
        methodReceiverIndex = 0;
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
      std::string remappedRemovedMapBodyArgumentTarget;
      const bool remappedRemovedMapTarget =
          this->resolveRemovedMapBodyArgumentTarget(
              expr, resolved, params, locals, builtinCollectionDispatchResolverAdapters, remappedRemovedMapBodyArgumentTarget);
      if (remappedRemovedMapTarget) {
        resolved = remappedRemovedMapBodyArgumentTarget;
        resolvedMethod = false;
      } else if (!resolvedMethod && !this->shouldPreserveRemovedCollectionHelperPath(resolved)) {
        resolved = preferVectorStdlibHelperPath(resolved);
      }
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
      BorrowEndScope borrowScope(*this, currentValidationContext_.endedReferenceBorrows);
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

    if (hasNamedArguments(expr.argNames)) {
      auto resolveVectorMutatorName = [&](const std::string &name, std::string &helperOut) -> bool {
        std::string normalized = name;
        if (!normalized.empty() && normalized.front() == '/') {
          normalized.erase(normalized.begin());
        }
        if (normalized.rfind("vector/", 0) == 0) {
          normalized = normalized.substr(std::string("vector/").size());
        } else if (normalized.rfind("array/", 0) == 0) {
          normalized = normalized.substr(std::string("array/").size());
        } else if (normalized.rfind("std/collections/vector/", 0) == 0) {
          normalized = normalized.substr(std::string("std/collections/vector/").size());
        }
        if (normalized == "push" || normalized == "pop" || normalized == "reserve" || normalized == "clear" ||
            normalized == "remove_at" || normalized == "remove_swap") {
          helperOut = normalized;
          return true;
        }
        return false;
      };
      std::string vectorHelperName;
      if (resolveVectorMutatorName(expr.name, vectorHelperName) &&
          (resolvedMethod || defMap_.find(resolved) == defMap_.end())) {
        error_ = vectorHelperName + " is only supported as a statement";
        return false;
      }
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
      if (currentValidationContext_.definitionIsUnsafe && isUnsafeReferenceExpr(rhsExpr)) {
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
      ExprNamedArgumentBuiltinContext namedArgumentBuiltinContext;
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
      if (!validateExprNamedArgumentBuiltins(
              params, locals, expr, resolved, resolvedMethod,
              namedArgumentBuiltinContext)) {
        return false;
      }
      bool handledGpuBufferBuiltin = false;
      if (!validateExprGpuBufferBuiltins(params, locals, expr,
                                         handledGpuBufferBuiltin)) {
        return false;
      }
      if (handledGpuBufferBuiltin) {
        return true;
      }
      if (resolvedMethod && (resolved == "/array/count" || resolved == "/vector/count" ||
                             resolved == "/soa_vector/count" || resolved == "/string/count" ||
                             resolved == "/map/count" || resolved == "/std/collections/map/count")) {
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
        if (resolved == "/map/count" || resolved == "/std/collections/map/count") {
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
          if (currentValidationContext_.definitionIsUnsafe && isUnsafeReferenceExpr(expr.args[1])) {
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
          if (!currentValidationContext_.definitionIsUnsafe) {
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
          }
        } else {
          error_ = "assign target must be a mutable binding";
          return false;
        }
        if (currentValidationContext_.definitionIsUnsafe && targetIsName && isUnsafeReferenceExpr(expr.args[1])) {
          std::string escapeSink;
          if (resolveReferenceEscapeSink(target.name, escapeSink)) {
            if (reportReferenceAssignmentEscape(escapeSink, expr.args[1])) {
              return false;
            }
          }
        }
        if (!currentValidationContext_.definitionIsUnsafe && targetIsName) {
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
        const bool isAliasCount = resolved == "/vector/count";
        std::string elemType;
        if (!resolveVectorTarget(expr.args.front(), elemType) &&
            !resolveArrayTarget(expr.args.front(), elemType) &&
            !resolveStringTarget(expr.args.front())) {
          if (!validateExpr(params, locals, expr.args.front())) {
            return false;
          }
          if (resolveMapTarget(expr.args.front())) {
            error_ = isAliasCount ? "unknown call target: /vector/count"
                                  : "unknown call target: /std/collections/vector/count";
            return false;
          }
          error_ = "count requires vector target";
          return false;
        }
      }
      if (!expr.isMethodCall &&
          (resolved == "/std/collections/vector/capacity" || resolved == "/vector/capacity") &&
          expr.args.size() == 1 && defMap_.find(resolved) == defMap_.end()) {
        const bool isAliasCapacity = resolved == "/vector/capacity";
        std::string elemType;
        if (!resolveVectorTarget(expr.args.front(), elemType)) {
          if (!validateExpr(params, locals, expr.args.front())) {
            return false;
          }
          if (isAliasCapacity && resolveArrayTarget(expr.args.front(), elemType)) {
            error_ = "unknown call target: /vector/capacity";
            return false;
          }
          if (resolveMapTarget(expr.args.front())) {
            error_ = isAliasCapacity ? "unknown call target: /vector/capacity"
                                     : "unknown call target: /std/collections/vector/capacity";
            return false;
          }
          error_ = "capacity requires vector target";
          return false;
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
      if (!expr.isMethodCall && isSimpleCallName(expr, "contains") && expr.args.size() == 2 &&
          (shouldBuiltinValidateBareMapContainsCall ||
           this->isIndexedArgsPackMapReceiverTarget(expr.args.front(), builtinCollectionDispatchResolvers))) {
        size_t receiverIndex = 0;
        size_t keyIndex = 1;
        const bool hasBareMapOperands = this->bareMapHelperOperandIndices(
            expr, builtinCollectionDispatchResolvers, receiverIndex, keyIndex);
        const Expr &receiverExpr = hasBareMapOperands ? expr.args[receiverIndex] : expr.args.front();
        const Expr &keyExpr = hasBareMapOperands ? expr.args[keyIndex] : expr.args[1];
        std::string mapKeyType;
        if (!this->resolveMapKeyType(receiverExpr, builtinCollectionDispatchResolvers, mapKeyType)) {
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
        if (!this->resolveMapKeyType(receiverExpr, builtinCollectionDispatchResolvers, mapKeyType)) {
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
        if (this->resolveMapKeyType(receiverExpr, builtinCollectionDispatchResolvers, mapKeyType)) {
          if (!mapKeyType.empty()) {
            if (normalizeBindingTypeName(mapKeyType) == "string") {
              if (!this->isStringExprForArgumentValidation(keyExpr, builtinCollectionDispatchResolvers)) {
                error_ = builtinName + " requires string map key";
                return false;
              }
            } else {
              ReturnKind keyKind = returnKindForTypeName(normalizeBindingTypeName(mapKeyType));
              if (keyKind != ReturnKind::Unknown) {
                if (resolveStringTarget(keyExpr)) {
                  error_ = builtinName + " requires map key type " + mapKeyType;
                  return false;
                }
                ReturnKind indexKind = inferExprReturnKind(keyExpr, params, locals);
                if (indexKind != ReturnKind::Unknown && indexKind != keyKind) {
                  error_ = builtinName + " requires map key type " + mapKeyType;
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
            if (!arg || !isUnsafeReferenceExpr(*arg)) {
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
