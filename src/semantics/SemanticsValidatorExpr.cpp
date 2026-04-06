#include "SemanticsValidator.h"
#include "primec/StringLiteral.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <functional>
#include <iomanip>
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
  auto publishExprRootDiagnostic = [&]() -> bool {
    captureExprContext(expr);
    return publishCurrentStructuredDiagnosticNow();
  };
  auto failExprRootDiagnostic = [&](std::string message) -> bool {
    return failExprDiagnostic(expr, std::move(message));
  };
  if (expr.isLambda) {
    return validateLambdaExpr(params, locals, expr, enclosingStatements, statementIndex);
  }
  if (!allowEntryArgStringUse_) {
    if (isEntryArgsAccess(expr) || isEntryArgStringBinding(locals, expr)) {
      return failExprRootDiagnostic(
          "entry argument strings are only supported in print calls or string bindings");
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
      return publishExprRootDiagnostic();
    }
    if (parsed.encoding == StringEncoding::Ascii && !isAsciiText(parsed.decoded)) {
      return failExprRootDiagnostic("ascii string literal contains non-ASCII characters");
    }
    return true;
  }
  if (expr.kind == Expr::Kind::Name) {
    if (isParam(params, expr.name) || locals.count(expr.name) > 0) {
      if (currentValidationState_.movedBindings.count(expr.name) > 0) {
        return failExprRootDiagnostic("use-after-move: " + expr.name);
      }
      return true;
    }
    if (!allowMathBareName(expr.name) && expr.name.find('/') == std::string::npos &&
        isBuiltinMathConstant(expr.name, true)) {
      return failExprRootDiagnostic(
          "math constant requires import /std/math/* or /std/math/<name>: " + expr.name);
    }
    if (isBuiltinMathConstant(expr.name, allowMathBareName(expr.name))) {
      return true;
    }
    return failExprRootDiagnostic("unknown identifier: " + expr.name);
  }
  if (expr.kind == Expr::Kind::Call) {
    if (expr.isBinding) {
      return failExprRootDiagnostic("binding not allowed in expression context");
    }
    auto pendingFieldViewNameFromRewrittenHelper = [&]() -> std::optional<std::string> {
      if (expr.kind != Expr::Kind::Call || expr.args.size() < 2 ||
          expr.args[1].kind != Expr::Kind::Literal) {
        return std::nullopt;
      }
      const std::string resolvedPath = resolveCalleePath(expr);
      const bool isFieldViewHelper =
          resolvedPath.rfind(
              "/std/collections/experimental_soa_vector/soaVectorFieldView",
              0) == 0 ||
          resolvedPath.rfind(
              "/std/collections/experimental_soa_storage/soaColumnFieldViewUnsafe",
              0) == 0;
      if (!isFieldViewHelper) {
        return std::nullopt;
      }
      auto inferStructTypeText = [&]() -> std::optional<std::string> {
        if (!expr.templateArgs.empty()) {
          return expr.templateArgs.front();
        }
        if (expr.args.empty()) {
          return std::nullopt;
        }
        const Expr &receiverExpr = expr.args.front();
        auto extractReceiverStructType = [&](const BindingInfo &binding)
            -> std::optional<std::string> {
          std::string elemType;
          if (extractExperimentalSoaVectorElementType(binding, elemType)) {
            return elemType;
          }
          return std::nullopt;
        };
        if (receiverExpr.kind == Expr::Kind::Name) {
          if (const BindingInfo *paramBinding =
                  findParamBinding(params, receiverExpr.name)) {
            return extractReceiverStructType(*paramBinding);
          }
          auto localIt = locals.find(receiverExpr.name);
          if (localIt != locals.end()) {
            return extractReceiverStructType(localIt->second);
          }
        }
        BindingInfo receiverBinding;
        if (inferBindingTypeFromInitializer(receiverExpr, params, locals,
                                            receiverBinding)) {
          if (const auto elemType = extractReceiverStructType(receiverBinding)) {
            return elemType;
          }
        }
        std::string inferredTypeText;
        if (inferQueryExprTypeText(receiverExpr, params, locals, inferredTypeText)) {
          BindingInfo inferredBinding;
          std::string base;
          std::string argText;
          const std::string normalizedType =
              normalizeBindingTypeName(inferredTypeText);
          if (splitTemplateTypeName(normalizedType, base, argText)) {
            inferredBinding.typeName = normalizeBindingTypeName(base);
            inferredBinding.typeTemplateArg = argText;
          } else {
            inferredBinding.typeName = normalizedType;
            inferredBinding.typeTemplateArg.clear();
          }
          return extractReceiverStructType(inferredBinding);
        }
        return std::nullopt;
      };
      const auto structTypeText = inferStructTypeText();
      if (!structTypeText.has_value()) {
        return std::nullopt;
      }
      std::string currentNamespace;
      if (!currentValidationState_.context.definitionPath.empty()) {
        const size_t slash =
            currentValidationState_.context.definitionPath.find_last_of('/');
        if (slash != std::string::npos && slash > 0) {
          currentNamespace =
              currentValidationState_.context.definitionPath.substr(0, slash);
        }
      }
      const std::string lookupNamespace =
          !expr.namespacePrefix.empty() ? expr.namespacePrefix : currentNamespace;
      const std::string structPath = resolveStructTypePath(
          normalizeBindingTypeName(*structTypeText),
          lookupNamespace,
          structNames_);
      auto defIt = defMap_.find(structPath);
      if (structPath.empty() || defIt == defMap_.end() || defIt->second == nullptr) {
        return std::nullopt;
      }
      const size_t fieldIndex = static_cast<size_t>(expr.args[1].literalValue);
      size_t currentFieldIndex = 0;
      for (const auto &fieldStmt : defIt->second->statements) {
        bool isStaticField = false;
        for (const auto &transform : fieldStmt.transforms) {
          if (transform.name == "static") {
            isStaticField = true;
            break;
          }
        }
        if (!fieldStmt.isBinding || isStaticField) {
          continue;
        }
        if (currentFieldIndex == fieldIndex) {
          return fieldStmt.name;
        }
        ++currentFieldIndex;
      }
      return std::nullopt;
    };
    if (!hasNamedArguments(expr.argNames)) {
      if (const auto pendingFieldName =
              pendingFieldViewNameFromRewrittenHelper()) {
        return failExprRootDiagnostic(
            soaDirectPendingUnavailableMethodDiagnostic(
                soaFieldViewHelperPath(*pendingFieldName)));
      }
    }
    auto isPendingSoaSchemaHelperCall = [&]() {
      if (expr.isMethodCall || expr.name.empty()) {
        return false;
      }
      std::string normalizedName = expr.name;
      if (!normalizedName.empty() && normalizedName.front() == '/') {
        normalizedName.erase(normalizedName.begin());
      }
      const std::string normalizedNamespace =
          !expr.namespacePrefix.empty() && expr.namespacePrefix.front() == '/'
              ? expr.namespacePrefix.substr(1)
              : expr.namespacePrefix;
      const bool isQualifiedStructSchemaCall =
          normalizedName == "Struct/SoaSchemaFieldCount" ||
          normalizedName == "Struct/SoaSchemaElementStride" ||
          normalizedName == "Struct/SoaSchemaFieldOffset";
      const bool isSplitStructSchemaCall =
          normalizedNamespace == "Struct" &&
          (normalizedName == "SoaSchemaFieldCount" ||
           normalizedName == "SoaSchemaElementStride" ||
           normalizedName == "SoaSchemaFieldOffset");
      if (!isQualifiedStructSchemaCall && !isSplitStructSchemaCall) {
        return false;
      }
      const std::string &definitionPath = currentValidationState_.context.definitionPath;
      return definitionPath.rfind(
                 "/std/collections/experimental_soa_storage/soaColumnField", 0) == 0;
    };
    if (isPendingSoaSchemaHelperCall()) {
      for (const Expr &arg : expr.args) {
        if (!validateExpr(params, locals, arg, enclosingStatements, statementIndex)) {
          return false;
        }
      }
      return true;
    }
    if (!hasNamedArguments(expr.argNames)) {
      if (const auto pendingPath =
              builtinSoaDirectPendingHelperPath(expr, params, locals)) {
        std::string pendingFieldName;
        if (splitSoaFieldViewHelperPath(*pendingPath, &pendingFieldName)) {
          return failExprRootDiagnostic(
              soaDirectPendingUnavailableMethodDiagnostic(*pendingPath));
        }
      }
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
        return failExprRootDiagnostic("named arguments not supported for builtin calls");
      }
      if (expr.hasBodyArguments || !expr.bodyArguments.empty()) {
        return failExprRootDiagnostic("uninitialized does not accept block arguments");
      }
      if (!expr.args.empty()) {
        return failExprRootDiagnostic("uninitialized does not accept arguments");
      }
      if (expr.templateArgs.size() != 1) {
        return failExprRootDiagnostic("uninitialized requires exactly one template argument");
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
          return failExprRootDiagnostic("named arguments not supported for builtin calls");
        }
        if (!expr.templateArgs.empty()) {
          return failExprRootDiagnostic(name + " does not accept template arguments");
        }
        if (expr.hasBodyArguments || !expr.bodyArguments.empty()) {
          return failExprRootDiagnostic(name + " does not accept block arguments");
        }
        const size_t expectedArgs = (name == "init") ? 2 : 1;
        if (expr.args.size() != expectedArgs) {
          return failExprRootDiagnostic(
              name + " requires exactly " + std::to_string(expectedArgs) + " argument" +
              (expectedArgs == 1 ? "" : "s"));
        }
        if (name == "init" || name == "drop") {
          return failExprRootDiagnostic(name + " is only supported as a statement");
        }
        for (const auto &arg : expr.args) {
          if (!validateExpr(params, locals, arg)) {
            return false;
          }
        }
        if (!isUninitializedStorage(expr.args.front())) {
          return failExprRootDiagnostic(name + " requires uninitialized<T> storage");
        }
        return true;
      }
    }
    if (isBuiltinBlockCall(expr) && expr.hasBodyArguments) {
      return validateBlockExpr(params, locals, expr);
    }
    if (isBuiltinBlockCall(expr)) {
      return failExprRootDiagnostic("block requires block arguments");
    }
    if (isLoopCall(expr)) {
      return failExprRootDiagnostic("loop is only supported as a statement");
    }
    if (isWhileCall(expr)) {
      return failExprRootDiagnostic("while is only supported as a statement");
    }
    if (isForCall(expr)) {
      return failExprRootDiagnostic("for is only supported as a statement");
    }
    if (isRepeatCall(expr)) {
      return failExprRootDiagnostic("repeat is only supported as a statement");
    }
    if (isSimpleCallName(expr, "dispatch")) {
      return failExprRootDiagnostic("dispatch is only supported as a statement");
    }
    if (isSimpleCallName(expr, "buffer_store")) {
      return failExprRootDiagnostic("buffer_store is only supported as a statement");
    }
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
      return failExprRootDiagnostic("return not allowed in expression context");
    }
    auto isNonCtorWrapperReturnedBareVectorMethodWithoutHelper = [&]() {
      if (!expr.isMethodCall || expr.args.empty() ||
          (expr.name != "count" && expr.name != "capacity")) {
        return false;
      }
      const Expr &receiver = expr.args.front();
      if (receiver.kind != Expr::Kind::Call || receiver.isBinding || receiver.isMethodCall) {
        return false;
      }
      std::string receiverTypeText;
      if (!inferQueryExprTypeText(receiver, params, locals, receiverTypeText)) {
        return false;
      }
      std::string base;
      std::string argText;
      if (!splitTemplateTypeName(normalizeBindingTypeName(receiverTypeText), base, argText) ||
          normalizeBindingTypeName(base) != "vector") {
        return false;
      }
      const std::string resolvedReceiver = resolveCalleePath(receiver);
      auto matchesCtorPath = [&](std::string_view ctorPath) {
        return resolvedReceiver == ctorPath ||
               resolvedReceiver.rfind(std::string(ctorPath) + "__t", 0) == 0;
      };
      if (matchesCtorPath("/std/collections/vector/vector") ||
          matchesCtorPath("/std/collections/experimental_vector/vector") ||
          matchesCtorPath("/std/collections/experimental_vector/vectorNew") ||
          matchesCtorPath("/std/collections/experimental_vector/vectorSingle") ||
          matchesCtorPath("/std/collections/experimental_vector/vectorPair") ||
          matchesCtorPath("/std/collections/experimental_vector/vectorTriple") ||
          matchesCtorPath("/std/collections/experimental_vector/vectorQuad") ||
          matchesCtorPath("/std/collections/experimental_vector/vectorQuint") ||
          matchesCtorPath("/std/collections/experimental_vector/vectorSext") ||
          matchesCtorPath("/std/collections/experimental_vector/vectorSept") ||
          matchesCtorPath("/std/collections/experimental_vector/vectorOct")) {
        return false;
      }
      std::string builtinCollectionName;
      if (getBuiltinCollectionName(receiver, builtinCollectionName) &&
          builtinCollectionName == "vector") {
        return false;
      }
      const std::string methodPath = preferredBareVectorHelperTarget(expr.name);
      return !hasDeclaredDefinitionPath(methodPath) &&
             !hasImportedDefinitionPath(methodPath);
    };
    if (isNonCtorWrapperReturnedBareVectorMethodWithoutHelper()) {
      return failExprRootDiagnostic("unknown method: /vector/" + expr.name);
    }
    ExprDispatchBootstrap dispatchBootstrap;
    prepareExprDispatchBootstrap(params, locals, dispatchBootstrap);
    const bool shouldBuiltinValidateBareMapCountCall =
        shouldBuiltinValidateCurrentMapWrapperHelper("count");
    const bool shouldBuiltinValidateBareMapContainsCall =
        shouldBuiltinValidateCurrentMapWrapperHelper("contains");
    const bool shouldBuiltinValidateBareMapTryAtCall =
        shouldBuiltinValidateCurrentMapWrapperHelper("tryAt");
    const bool shouldBuiltinValidateBareMapAccessCall =
        shouldBuiltinValidateCurrentMapWrapperHelper("at") ||
        shouldBuiltinValidateCurrentMapWrapperHelper("at_unsafe");
    bool handledEarlyPointerBuiltin = false;
    if (!validateExprEarlyPointerBuiltin(
            params, locals, expr, dispatchBootstrap,
            handledEarlyPointerBuiltin)) {
      return false;
    }
    if (handledEarlyPointerBuiltin) {
      return true;
    }

    std::string resolved;
    ExprPreDispatchDirectCallContext preDispatchDirectCallContext;
    preDispatchDirectCallContext.dispatchBootstrap = &dispatchBootstrap;
    std::optional<Expr> rewrittenPreDispatchDirectCall;
    bool handledPreDispatchDirectCall = false;
    if (!validateExprPreDispatchDirectCalls(
            params,
            locals,
            expr,
            preDispatchDirectCallContext,
            resolved,
            rewrittenPreDispatchDirectCall,
            handledPreDispatchDirectCall)) {
      return false;
    }
    if (rewrittenPreDispatchDirectCall.has_value()) {
      return validateExpr(params, locals, *rewrittenPreDispatchDirectCall);
    }
    if (handledPreDispatchDirectCall) {
      return true;
    }
    ExprMethodCompatibilitySetup methodCompatibilitySetup;
    if (!prepareExprMethodCompatibilitySetup(
            params,
            locals,
            expr,
            dispatchBootstrap,
            hasVectorHelperCallResolution,
            vectorHelperCallResolvedPath,
            vectorHelperCallReceiverIndex,
            resolved,
            methodCompatibilitySetup)) {
      return false;
    }
    bool resolvedMethod = methodCompatibilitySetup.resolvedMethod;
    bool usedMethodTarget = methodCompatibilitySetup.usedMethodTarget;
    bool hasMethodReceiverIndex = methodCompatibilitySetup.hasMethodReceiverIndex;
    size_t methodReceiverIndex = methodCompatibilitySetup.methodReceiverIndex;
    if (expr.isFieldAccess) {
      return validateExprFieldAccess(params, locals, expr);
    }
    ExprCollectionDispatchSetup collectionDispatchSetup;
    if (!prepareExprCollectionDispatchSetup(
            params,
            locals,
            expr,
            dispatchBootstrap.dispatchResolvers,
            dispatchBootstrap.dispatchResolverAdapters,
            resolved,
            collectionDispatchSetup)) {
      return false;
    }
    ExprMethodResolutionContext methodResolutionContext;
    methodResolutionContext.hasVectorHelperCallResolution =
        hasVectorHelperCallResolution;
    methodResolutionContext.promoteCapacityToBuiltinValidation =
        methodCompatibilitySetup.promoteCapacityToBuiltinValidation;
    methodResolutionContext.unavailableMethodDiagnostic =
        methodCompatibilitySetup.unavailableMethodDiagnostic;
    if (!validateExprMethodCallTarget(
            params,
            locals,
            expr,
            methodResolutionContext,
            dispatchBootstrap.dispatchResolvers,
            dispatchBootstrap.dispatchResolverAdapters,
            resolved,
            resolvedMethod,
            usedMethodTarget,
            hasMethodReceiverIndex,
            methodReceiverIndex)) {
      if (error_.empty()) {
        auto exprKindName = [](Expr::Kind kind) -> const char * {
          switch (kind) {
          case Expr::Kind::Literal:
            return "Literal";
          case Expr::Kind::BoolLiteral:
            return "BoolLiteral";
          case Expr::Kind::FloatLiteral:
            return "FloatLiteral";
          case Expr::Kind::StringLiteral:
            return "StringLiteral";
          case Expr::Kind::Call:
            return "Call";
          case Expr::Kind::Name:
            return "Name";
          }
          return "Unknown";
        };
        std::string receiverSummary = "none";
        if (!expr.args.empty()) {
          const Expr &receiver = expr.args.front();
          receiverSummary = std::string(exprKindName(receiver.kind)) +
                            ":" + receiver.name +
                            " ns=" + receiver.namespacePrefix;
        }
        return failExprRootDiagnostic(
            "validateExprMethodCallTarget failed name=" + expr.name +
            " ns=" + expr.namespacePrefix +
            " resolved=" + resolved +
            " templateArgs=" + std::to_string(expr.templateArgs.size()) +
            " args=" + std::to_string(expr.args.size()) +
            " receiver=" + receiverSummary);
      }
      return false;
    }
    resolved = resolveExprConcreteCallPath(params, locals, expr, resolved);
    auto stripSpecializationSuffix = [](const std::string &path) -> std::string {
      const size_t suffix = path.find("__t");
      return suffix == std::string::npos ? path : path.substr(0, suffix);
    };
    auto isDestroyHelperPath = [&](const std::string &path) -> bool {
      const std::string base = stripSpecializationSuffix(path);
      static const std::array<std::string_view, 4> suffixes = {
          "/Destroy", "/DestroyStack", "/DestroyHeap", "/DestroyBuffer"};
      for (std::string_view suffix : suffixes) {
        if (base.size() >= suffix.size() &&
            base.compare(base.size() - suffix.size(), suffix.size(),
                         suffix.data(), suffix.size()) == 0) {
          return true;
        }
      }
      return false;
    };
    auto isSoaGrowthHelperPath = [&](const std::string &path) -> bool {
      const std::string base = stripSpecializationSuffix(path);
      if (base == "/soa_vector/push" || base == "/soa_vector/reserve" ||
          base == "/std/collections/soa_vector/push" ||
          base == "/std/collections/soa_vector/reserve") {
        return true;
      }
      const bool isExperimentalSoaPath =
          base.rfind("/std/collections/experimental_soa_vector/", 0) == 0;
      if (!isExperimentalSoaPath) {
        return false;
      }
      auto hasSuffix = [&](std::string_view suffix) {
        return base.size() >= suffix.size() &&
               base.compare(base.size() - suffix.size(), suffix.size(),
                            suffix.data(), suffix.size()) == 0;
      };
      return hasSuffix("/push") || hasSuffix("/reserve") ||
             hasSuffix("/soaVectorPush") || hasSuffix("/soaVectorReserve");
    };
    if (isDestroyHelperPath(resolved) || isSoaGrowthHelperPath(resolved)) {
      auto resolveNamedBinding =
          [&](const std::string &name) -> const BindingInfo * {
        if (const BindingInfo *paramBinding = findParamBinding(params, name)) {
          return paramBinding;
        }
        auto it = locals.find(name);
        if (it != locals.end()) {
          return &it->second;
        }
        return nullptr;
      };
      auto isSoaBorrowBinding = [&](const BindingInfo &binding) -> bool {
        if (binding.typeName == "soa_vector") {
          return true;
        }
        std::string elemType;
        if (extractExperimentalSoaVectorElementType(binding, elemType)) {
          return true;
        }
        const std::string normalizedType =
            normalizeBindingTypeName(binding.typeName);
        if ((normalizedType == "Reference" || normalizedType == "Pointer") &&
            !binding.typeTemplateArg.empty()) {
          std::string base;
          std::string arg;
          const std::string normalizedTarget =
              normalizeBindingTypeName(binding.typeTemplateArg);
          if (splitTemplateTypeName(normalizedTarget, base, arg)) {
            return normalizeBindingTypeName(base) == "soa_vector";
          }
          return normalizedTarget == "soa_vector";
        }
        return false;
      };
      auto isSoaFieldViewBindingType = [&](const BindingInfo &binding) -> bool {
        std::string normalized = normalizeBindingTypeName(binding.typeName);
        if (normalized.empty()) {
          return false;
        }
        std::string base;
        std::string arg;
        if (splitTemplateTypeName(normalized, base, arg)) {
          normalized = normalizeBindingTypeName(base);
        }
        return normalized == "SoaFieldView" ||
               normalized == "std/collections/experimental_soa_storage/SoaFieldView";
      };
      auto referenceRootForBorrowBinding =
          [&](const std::string &bindingName,
              const BindingInfo &binding) -> std::string {
        if (binding.typeName != "Reference" &&
            !isSoaFieldViewBindingType(binding)) {
          return "";
        }
        if (!binding.referenceRoot.empty()) {
          return binding.referenceRoot;
        }
        return bindingName;
      };
      auto hasActiveBorrowForRoot =
          [&](const std::string &borrowRoot,
              const std::string &ignoreBorrowName = std::string()) -> bool {
        if (borrowRoot.empty() ||
            currentValidationState_.context.definitionIsUnsafe) {
          return false;
        }
        auto hasBorrowFrom =
            [&](const std::string &bindingName,
                const BindingInfo &binding) -> bool {
          if (!ignoreBorrowName.empty() &&
              bindingName == ignoreBorrowName) {
            return false;
          }
          if (currentValidationState_.endedReferenceBorrows.count(bindingName) >
              0) {
            return false;
          }
          const std::string root =
              referenceRootForBorrowBinding(bindingName, binding);
          return !root.empty() && root == borrowRoot;
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
      auto resolveStandaloneSoaBorrowRoot =
          [&](const Expr &receiverExpr,
              std::string &borrowRootOut,
              std::string &ignoreBorrowNameOut) -> bool {
        borrowRootOut.clear();
        ignoreBorrowNameOut.clear();
        if (receiverExpr.kind == Expr::Kind::Name) {
          const BindingInfo *binding =
              resolveNamedBinding(receiverExpr.name);
          if (binding == nullptr || !isSoaBorrowBinding(*binding)) {
            return false;
          }
          const std::string normalizedType =
              normalizeBindingTypeName(binding->typeName);
          if (normalizedType == "Reference" || normalizedType == "Pointer") {
            ignoreBorrowNameOut = receiverExpr.name;
            if (!binding->referenceRoot.empty()) {
              borrowRootOut = binding->referenceRoot;
            } else {
              borrowRootOut = receiverExpr.name;
            }
            return true;
          }
          borrowRootOut = receiverExpr.name;
          return true;
        }
        if (receiverExpr.kind != Expr::Kind::Call) {
          return false;
        }
        std::string builtinName;
        if (getBuiltinPointerName(receiverExpr, builtinName) &&
            builtinName == "dereference" &&
            receiverExpr.args.size() == 1) {
          const Expr &pointerExpr = receiverExpr.args.front();
          if (pointerExpr.kind == Expr::Kind::Name) {
            const BindingInfo *binding =
                resolveNamedBinding(pointerExpr.name);
            if (binding == nullptr || binding->referenceRoot.empty() ||
                !isSoaBorrowBinding(*binding)) {
              return false;
            }
            ignoreBorrowNameOut = pointerExpr.name;
            borrowRootOut = binding->referenceRoot;
            return true;
          }
          if (getBuiltinPointerName(pointerExpr, builtinName) &&
              builtinName == "location" && pointerExpr.args.size() == 1 &&
              pointerExpr.args.front().kind == Expr::Kind::Name) {
            const BindingInfo *binding =
                resolveNamedBinding(pointerExpr.args.front().name);
            if (binding == nullptr || !isSoaBorrowBinding(*binding)) {
              return false;
            }
            borrowRootOut = pointerExpr.args.front().name;
            return true;
          }
        }
        return false;
      };
      const Expr *receiverExpr = nullptr;
      if (hasMethodReceiverIndex && methodReceiverIndex < expr.args.size()) {
        receiverExpr = &expr.args[methodReceiverIndex];
      } else if (expr.isMethodCall && !expr.args.empty()) {
        receiverExpr = &expr.args.front();
      } else if (!expr.args.empty()) {
        receiverExpr = &expr.args.front();
      }
      if (receiverExpr != nullptr) {
        std::string borrowRoot;
        std::string ignoreBorrowName;
        if (resolveStandaloneSoaBorrowRoot(*receiverExpr, borrowRoot,
                                           ignoreBorrowName) &&
            hasActiveBorrowForRoot(borrowRoot, ignoreBorrowName)) {
          const std::string borrowSink =
              !ignoreBorrowName.empty() ? ignoreBorrowName : borrowRoot;
          return failExprRootDiagnostic(
              "borrowed binding: " + borrowRoot + " (root: " + borrowRoot +
              ", sink: " + borrowSink + ")");
        }
      }
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
        dispatchBootstrap.resolveMapTarget;
    collectionCountCapacityDispatchContext
        .isArrayNamespacedVectorCountCompatibilityCall =
        [&](const Expr &target) {
          return this->isArrayNamespacedVectorCountCompatibilityCall(
              target, dispatchBootstrap.dispatchResolvers);
        };
    collectionCountCapacityDispatchContext.tryRewriteBareVectorHelperCall =
        [&](const std::string &helperName, Expr &rewrittenOut) {
          return this->tryRewriteBareVectorHelperCall(
              expr, helperName, dispatchBootstrap.dispatchResolvers,
              rewrittenOut);
        };
    collectionCountCapacityDispatchContext
        .promoteCapacityToBuiltinValidation =
        methodCompatibilitySetup.promoteCapacityToBuiltinValidation;
    collectionCountCapacityDispatchContext
        .isNonCollectionStructCapacityTarget =
        methodCompatibilitySetup.isNonCollectionStructCapacityTarget;
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
        &dispatchBootstrap.dispatchResolvers;
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
    prepareExprCollectionAccessDispatchContext(
        collectionDispatchSetup,
        shouldBuiltinValidateBareMapContainsCall,
        shouldBuiltinValidateBareMapAccessCall,
        dispatchBootstrap.dispatchResolvers,
        dispatchBootstrap.resolveMapTarget,
        collectionAccessDispatchContext);
    bool handledCollectionAccessTarget = false;
    if (!resolveExprCollectionAccessTarget(
            params, locals, expr, collectionAccessDispatchContext,
            handledCollectionAccessTarget, resolved, resolvedMethod,
            usedMethodTarget, hasMethodReceiverIndex,
            methodReceiverIndex)) {
      return false;
    }
    bool handledPostAccessPrecheck = false;
    if (!validateExprPostAccessPrechecks(
            params,
            locals,
            expr,
            resolved,
            resolvedMethod,
            usedMethodTarget,
            dispatchBootstrap.dispatchResolverAdapters,
            enclosingStatements,
            statementIndex,
            handledPostAccessPrecheck)) {
      return false;
    }
    if (handledPostAccessPrecheck) {
      return true;
    }

    ExprNamedArgumentBuiltinContext namedArgumentBuiltinContext;
    prepareExprNamedArgumentBuiltinContext(
        hasVectorHelperCallResolution,
        dispatchBootstrap.dispatchResolvers,
        namedArgumentBuiltinContext);
    if (!validateExprNamedArguments(params, locals, expr, resolved,
                                    resolvedMethod,
                                    namedArgumentBuiltinContext)) {
      if (error_.empty()) {
        return failExprRootDiagnostic("validateExprNamedArguments failed");
      }
      return false;
    }
    auto it = defMap_.find(resolved);
    ExprLateBuiltinContext lateBuiltinContext;
    prepareExprLateBuiltinContext(
        params,
        locals,
        dispatchBootstrap.dispatchResolverAdapters,
        dispatchBootstrap.dispatchResolvers,
        lateBuiltinContext);
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
    prepareExprCountCapacityMapBuiltinContext(
        collectionDispatchSetup.shouldBuiltinValidateStdNamespacedVectorCountCall,
        collectionDispatchSetup.isStdNamespacedVectorCountCall,
        shouldBuiltinValidateBareMapCountCall,
        collectionDispatchSetup.isNamespacedMapCountCall,
        collectionDispatchSetup.isResolvedMapCountCall,
        collectionDispatchSetup
            .shouldBuiltinValidateStdNamespacedVectorCapacityCall,
        collectionDispatchSetup.isStdNamespacedVectorCapacityCall,
        dispatchBootstrap.dispatchResolverAdapters,
        dispatchBootstrap.dispatchResolvers,
        countCapacityMapBuiltinContext);
    bool handledCountCapacityMapBuiltin = false;
    if (!validateExprCountCapacityMapBuiltins(
            params, locals, expr, resolved, resolvedMethod,
            countCapacityMapBuiltinContext, handledCountCapacityMapBuiltin)) {
      return false;
    }
    if (handledCountCapacityMapBuiltin) {
      return true;
    }
    const bool shouldLateValidateCanonicalSoaToAos =
        resolved.rfind("/std/collections/soa_vector/to_aos", 0) == 0;
    if (it == defMap_.end() || resolvedMethod || shouldLateValidateCanonicalSoaToAos) {
      ExprLateMapSoaBuiltinContext lateMapSoaBuiltinContext;
      prepareExprLateMapSoaBuiltinContext(
          shouldBuiltinValidateBareMapContainsCall,
          dispatchBootstrap.dispatchResolvers,
          lateMapSoaBuiltinContext);
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
      prepareExprLateFallbackBuiltinContext(
          collectionDispatchSetup.isStdNamespacedVectorAccessCall,
          collectionDispatchSetup.shouldAllowStdAccessCompatibilityFallback,
          collectionDispatchSetup.hasStdNamespacedVectorAccessDefinition,
          collectionDispatchSetup.isStdNamespacedMapAccessCall,
          collectionDispatchSetup.hasStdNamespacedMapAccessDefinition,
          shouldBuiltinValidateBareMapAccessCall,
          dispatchBootstrap.dispatchResolvers,
          lateFallbackBuiltinContext);
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
      prepareExprLateCallCompatibilityContext(
          dispatchBootstrap.dispatchResolvers,
          lateCallCompatibilityContext);
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
      prepareExprLateMapAccessBuiltinContext(
          dispatchBootstrap.dispatchResolvers,
          shouldBuiltinValidateBareMapContainsCall,
          shouldBuiltinValidateBareMapTryAtCall,
          shouldBuiltinValidateBareMapAccessCall,
          lateMapAccessBuiltinContext);
      bool handledLateMapAccessBuiltin = false;
      if (!validateExprLateMapAccessBuiltins(
              params, locals, expr, resolved, lateMapAccessBuiltinContext,
              handledLateMapAccessBuiltin)) {
        return false;
      }
      if (handledLateMapAccessBuiltin) {
        return true;
      }
      ExprLateUnknownTargetFallbackContext lateUnknownTargetFallbackContext;
      lateUnknownTargetFallbackContext.resolveMapTarget =
          dispatchBootstrap.resolveMapTarget;
      bool handledLateUnknownTargetFallback = false;
      if (!validateExprLateUnknownTargetFallbacks(
              params, locals, expr, lateUnknownTargetFallbackContext,
              handledLateUnknownTargetFallback)) {
        if (error_.empty()) {
          return failExprRootDiagnostic(
              "validateExprLateUnknownTargetFallbacks failed");
        }
        return false;
      }
      if (handledLateUnknownTargetFallback) {
        return true;
      }
    }
    const auto &calleeParams = paramsByDef_[resolved];
    ExprResolvedCallSetup resolvedCallSetup;
    prepareExprResolvedCallSetup(
        params,
        locals,
        expr,
        resolved,
        dispatchBootstrap.dispatchResolvers,
        *it->second,
        calleeParams,
        hasMethodReceiverIndex,
        methodReceiverIndex,
        resolvedCallSetup);
    bool handledResolvedStructConstructor = false;
    if (!validateExprResolvedStructConstructorCall(
            params, locals, expr, resolved,
            resolvedCallSetup.resolvedStructConstructorContext,
            handledResolvedStructConstructor)) {
      return false;
    }
    if (handledResolvedStructConstructor) {
      return true;
    }
    bool handledResolvedCallArguments = false;
    if (!validateExprResolvedCallArguments(
            params, locals, expr, resolved,
            resolvedCallSetup.resolvedCallArgumentContext,
            handledResolvedCallArguments)) {
      if (error_.empty()) {
        return failExprRootDiagnostic(
            "validateExprResolvedCallArguments failed");
      }
      return false;
    }
    if (handledResolvedCallArguments) {
      return true;
    }
  }
  return false;
}

} // namespace primec::semantics
