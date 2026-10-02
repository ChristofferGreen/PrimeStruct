#include "SemanticsValidator.h"
#include "StdlibCollectionSurfaceHelpers.h"
#include "SemanticsValidatorInferCollectionCompatibilityInternal.h"
#include "primec/ir/StdlibCollectionPaths.h"
#include "primec/frontend/StringLiteral.h"
#include "primec/support/CollectionHelperNames.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <functional>
#include <iomanip>
#include <optional>
#include <sstream>
#include <unordered_set>
#include "SemanticsValidatorExprCallState.h"

namespace primec::semantics {

bool SemanticsValidator::validateExpr(const std::vector<ParameterInfo> &params,
                                      const std::unordered_map<std::string, BindingInfo> &locals,
                                      const Expr &expr,
                                      const std::vector<Expr> *enclosingStatements,
                                      size_t statementIndex,
                                      bool expressionIsStatementContext) {
  ExprContextScope exprScope(*this, expr);
  observeLocalMapSize(locals.size());
  if (expr.kind == Expr::Kind::Call) {
    observeCallVisited();
  }
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
    if (currentValidationState_.compileTimeTypeLocals.count(expr.name) > 0) {
      return failExprRootDiagnostic("type local is not a runtime value: " + expr.name);
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
    ValidateExprCallState st;
    st.enclosingStatements = enclosingStatements;
    st.statementIndex = statementIndex;
    st.expressionIsStatementContext = expressionIsStatementContext;
    if (validateExprCallPhase1(params, locals, expr, st) == PhaseStatus::Done) {
      return st.result;
    }
    if (validateExprCallPhase2(params, locals, expr, st) == PhaseStatus::Done) {
      return st.result;
    }
    if (validateExprCallPhase3(params, locals, expr, st) == PhaseStatus::Done) {
      return st.result;
    }
    if (validateExprCallPhase4(params, locals, expr, st) == PhaseStatus::Done) {
      return st.result;
    }
    if (validateExprCallPhase5(params, locals, expr, st) == PhaseStatus::Done) {
      return st.result;
    }
    if (validateExprCallPhase6(params, locals, expr, st) == PhaseStatus::Done) {
      return st.result;
    }
    if (validateExprCallPhase7(params, locals, expr, st) == PhaseStatus::Done) {
      return st.result;
    }
    if (validateExprCallPhase8(params, locals, expr, st) == PhaseStatus::Done) {
      return st.result;
    }
  }
  return false;
}

PhaseStatus SemanticsValidator::validateExprCallPhase1([[maybe_unused]] const std::vector<ParameterInfo> &params, [[maybe_unused]] const std::unordered_map<std::string, BindingInfo> &locals, [[maybe_unused]] const Expr &expr, ValidateExprCallState &st) {
  [[maybe_unused]] auto &enclosingStatements = st.enclosingStatements;
  [[maybe_unused]] auto &statementIndex = st.statementIndex;
  [[maybe_unused]] auto &expressionIsStatementContext = st.expressionIsStatementContext;
  [[maybe_unused]] auto publishExprRootDiagnostic = [&]() -> bool {
    captureExprContext(expr);
    return publishCurrentStructuredDiagnosticNow();
  };
  [[maybe_unused]] auto failExprRootDiagnostic = [&](std::string message) -> bool {
    return failExprDiagnostic(expr, std::move(message));
  };
    if (expr.isBinding) {
      return st.done(failExprRootDiagnostic("binding not allowed in expression context"));
    }
    if (isIfCall(expr)) {
      return st.done(validateIfExpr(params, locals, expr));
    }
    if (auto noImportSoaDiagnostic =
            noImportSoaHelperCallDiagnostic(expr, params, locals)) {
      return st.done(failExprRootDiagnostic(std::move(*noImportSoaDiagnostic)));
    }
    if (isTaskTypeCarrierExpr(expr)) {
      return st.done(validateTaskTypeCarrierExpr(
          params, locals, expr, enclosingStatements, statementIndex));
    }
    if (isTaskSpawnExpr(expr)) {
      return st.done(validateTaskSpawnExpr(
          params, locals, expr, enclosingStatements, statementIndex));
    }
    if (isTaskWaitExpr(expr)) {
      return st.done(validateTaskWaitExpr(params, locals, expr));
    }
    if (!validateTaskHandleArgumentEscapes(params, locals, expr)) {
      return st.done(false);
    }
    if (isSimpleCallName(expr, "move")) {
      bool handledMoveBuiltin = false;
      if (!validateExprMutationBorrowBuiltins(
              params, locals, expr, handledMoveBuiltin)) {
        return st.done(false);
      }
      if (handledMoveBuiltin) {
        return st.done(true);
      }
    }
    bool handledExplicitSumConstructor = false;
    if (!validateExplicitSumConstructorExpr(params,
                                            locals,
                                            expr,
                                            handledExplicitSumConstructor)) {
      return st.done(false);
    }
    if (handledExplicitSumConstructor) {
      return st.done(true);
    }
    if (expr.isMethodCall &&
        (expr.name == "count" || collection_helpers::isGetHelperName(expr.name) || expr.name == "ref") &&
        hasVisibleDefinitionPathForCurrentImports(collection_helpers::kRootedSoaPrefix + expr.name)) {
      for (const Expr &arg : expr.args) {
        if (!validateExpr(params, locals, arg, enclosingStatements,
                          statementIndex)) {
          return st.done(false);
        }
      }
      return st.done(true);
    }
    if (!expr.isMethodCall && !expr.isFieldAccess && expr.args.size() == 2 &&
        expr.args.front().kind == Expr::Kind::Call) {
      const std::string canonicalSoaGetPath =
          canonicalizeLegacySoaGetHelperPath(expr.name);
      std::string soaGetHelper;
      if (isLegacyOrCanonicalSoaHelperPath(canonicalSoaGetPath, "get")) {
        soaGetHelper = "get";
      } else if (isLegacyOrCanonicalSoaHelperPath(canonicalSoaGetPath, collection_helpers::kGetRef)) {
        soaGetHelper = collection_helpers::kGetRef;
      }
      const bool usesCanonicalSoaGetSurface =
          expr.name.rfind(collection_paths::modulePrefix(
                              collection_paths::kLegacySoaVectorFolder),
                          0) == 0 ||
          expr.namespacePrefix ==
              collection_paths::moduleRootBare(collection_paths::kLegacySoaVectorFolder) ||
          expr.namespacePrefix ==
              collection_paths::moduleRoot(collection_paths::kLegacySoaVectorFolder);
      if (!soaGetHelper.empty() && usesCanonicalSoaGetSurface) {
        std::string receiverTypeText;
        const bool receiverIsExperimentalSoa =
            inferQueryExprTypeText(expr.args.front(), params, locals,
                                   receiverTypeText) &&
            (receiverTypeText.find(collection_paths::kSoaVectorTypeName) !=
                 std::string::npos ||
             receiverTypeText.find(collection_paths::kExperimentalSoaVectorFolder) !=
                 std::string::npos);
        if (!receiverIsExperimentalSoa) {
          return st.done(failExprRootDiagnostic(soaUnavailableMethodDiagnostic(
              collection_paths::memberPath(collection_paths::kLegacySoaVectorFolder,
                                           soaGetHelper))));
        }
        if (expr.hasBodyArguments || !expr.bodyArguments.empty()) {
          return st.done(failExprRootDiagnostic(soaGetHelper +
                                        " does not accept block arguments"));
        }
        if (!expr.templateArgs.empty()) {
          return st.done(failExprRootDiagnostic(soaGetHelper +
                                        " does not accept template arguments"));
        }
        const Expr &indexExpr = expr.args[1];
        if (indexExpr.kind == Expr::Kind::BoolLiteral ||
            indexExpr.kind == Expr::Kind::FloatLiteral ||
            indexExpr.kind == Expr::Kind::StringLiteral) {
          return st.done(failExprRootDiagnostic(soaGetHelper +
                                        " requires integer index"));
        }
        return st.done(validateExpr(params, locals, expr.args.front(),
                            enclosingStatements, statementIndex) &&
               validateExpr(params, locals, expr.args[1],
                            enclosingStatements, statementIndex));
      }
    }
    if (expr.isFieldAccess && expr.args.size() == 1 &&
        expr.args.front().kind == Expr::Kind::Call) {
      const auto elementAccessHelper =
          builtinSoaAccessHelperName(expr.args.front(), params, locals);
      if (elementAccessHelper.has_value() &&
          (*elementAccessHelper == "get" ||
           *elementAccessHelper == collection_helpers::kGetRef ||
           *elementAccessHelper == "ref" ||
           *elementAccessHelper == collection_helpers::kRefRef)) {
        if (auto noImportSoaDiagnostic = noImportSoaHelperCallDiagnostic(
                expr.args.front(), params, locals)) {
          return st.done(failExprDiagnostic(expr.args.front(),
                                    std::move(*noImportSoaDiagnostic)));
        }
        for (const Expr &arg : expr.args.front().args) {
          if (!validateExpr(params, locals, arg, enclosingStatements,
                            statementIndex)) {
            return st.done(false);
          }
        }
        return st.done(true);
      }
    }
  st.pendingFieldViewNameFromRewrittenHelper = [&]() -> std::optional<std::string> {
      if (expr.kind != Expr::Kind::Call || expr.args.size() < 2 ||
          expr.args[1].kind != Expr::Kind::Literal) {
        return std::nullopt;
      }
      const std::string resolvedPath = resolveCalleePath(expr);
      const bool isFieldViewHelper =
          isExperimentalSoaFieldViewHelperPath(resolvedPath);
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
        if (!fieldStmt.isBinding || isCompileTimeTypeBinding(fieldStmt) ||
            isStaticField) {
          continue;
        }
        if (currentFieldIndex == fieldIndex) {
          return fieldStmt.name;
        }
        ++currentFieldIndex;
      }
      return std::nullopt;
    };
  [[maybe_unused]] auto &pendingFieldViewNameFromRewrittenHelper = st.pendingFieldViewNameFromRewrittenHelper;
  return PhaseStatus::Continue;
}

PhaseStatus SemanticsValidator::validateExprCallPhase2([[maybe_unused]] const std::vector<ParameterInfo> &params, [[maybe_unused]] const std::unordered_map<std::string, BindingInfo> &locals, [[maybe_unused]] const Expr &expr, ValidateExprCallState &st) {
  [[maybe_unused]] auto &enclosingStatements = st.enclosingStatements;
  [[maybe_unused]] auto &statementIndex = st.statementIndex;
  [[maybe_unused]] auto &expressionIsStatementContext = st.expressionIsStatementContext;
  [[maybe_unused]] auto publishExprRootDiagnostic = [&]() -> bool {
    captureExprContext(expr);
    return publishCurrentStructuredDiagnosticNow();
  };
  [[maybe_unused]] auto failExprRootDiagnostic = [&](std::string message) -> bool {
    return failExprDiagnostic(expr, std::move(message));
  };
  [[maybe_unused]] auto &pendingFieldViewNameFromRewrittenHelper = st.pendingFieldViewNameFromRewrittenHelper;
    if (!hasNamedArguments(expr.argNames)) {
      if (const auto pendingFieldName =
              pendingFieldViewNameFromRewrittenHelper()) {
        std::string fieldViewPath = resolveCalleePath(expr);
        if (const size_t suffix = fieldViewPath.find("__");
            suffix != std::string::npos) {
          fieldViewPath.erase(suffix);
        }
        if (fieldViewPath == collection_helpers::kCanonicalSoaFieldView) {
          for (const Expr &arg : expr.args) {
            if (!validateExpr(params, locals, arg, enclosingStatements,
                              statementIndex)) {
              return st.done(false);
            }
          }
          return st.done(true);
        }
        if (!expr.args.empty() && expr.args.front().kind == Expr::Kind::Call) {
          const auto elementAccessHelper =
              builtinSoaAccessHelperName(expr.args.front(), params, locals);
          if (elementAccessHelper.has_value() &&
              (*elementAccessHelper == "get" ||
               *elementAccessHelper == collection_helpers::kGetRef ||
               *elementAccessHelper == "ref" ||
               *elementAccessHelper == collection_helpers::kRefRef)) {
            for (const Expr &arg : expr.args.front().args) {
              if (!validateExpr(params, locals, arg, enclosingStatements,
                                statementIndex)) {
                return st.done(false);
              }
            }
            return st.done(true);
          }
        }
        return st.done(failExprRootDiagnostic(soaUnavailableMethodDiagnostic(
            soaFieldViewHelperPath(*pendingFieldName))));
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
      return isExperimentalSoaColumnFieldSchemaHelperPath(
          currentValidationState_.context.definitionPath);
    };
    if (isPendingSoaSchemaHelperCall()) {
      for (const Expr &arg : expr.args) {
        if (!validateExpr(params, locals, arg, enclosingStatements, statementIndex)) {
          return st.done(false);
        }
      }
      return st.done(true);
    }
    if (!hasNamedArguments(expr.argNames)) {
      if (const auto pendingPath =
              builtinSoaDirectPendingHelperPath(expr, params, locals)) {
        std::string pendingFieldName;
        if (splitSoaFieldViewHelperPath(*pendingPath, &pendingFieldName)) {
          return st.done(failExprRootDiagnostic(
              soaUnavailableMethodDiagnostic(*pendingPath)));
        }
      }
    }
  [[maybe_unused]] auto &effectScope = st.effectScope;
    if (!expr.transforms.empty()) {
      std::unordered_set<std::string> executionEffects;
      if (!resolveExecutionEffects(expr, executionEffects)) {
        return st.done(false);
      }
      effectScope.emplace(*this, std::move(executionEffects));
    }
    if (isMatchCall(expr)) {
      Expr expanded;
      if (!lowerMatchToIf(expr, expanded, error_)) {
        return st.done(false);
      }
      return st.done(validateExpr(params, locals, expanded));
    }
    if (isPickCall(expr)) {
      return st.done(validatePickExpr(params, locals, expr));
    }
    if (isIfCall(expr)) {
      return st.done(validateIfExpr(params, locals, expr));
    }
    if (!expr.isMethodCall && isSimpleCallName(expr, "uninitialized")) {
      if (hasNamedArguments(expr.argNames)) {
        return st.done(failExprRootDiagnostic("named arguments not supported for builtin calls"));
      }
      if (expr.hasBodyArguments || !expr.bodyArguments.empty()) {
        return st.done(failExprRootDiagnostic("uninitialized does not accept block arguments"));
      }
      if (!expr.args.empty()) {
        return st.done(failExprRootDiagnostic("uninitialized does not accept arguments"));
      }
      if (expr.templateArgs.size() != 1) {
        return st.done(failExprRootDiagnostic("uninitialized requires exactly one template argument"));
      }
      return st.done(true);
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
          return st.done(failExprRootDiagnostic("named arguments not supported for builtin calls"));
        }
        if (!expr.templateArgs.empty()) {
          return st.done(failExprRootDiagnostic(name + " does not accept template arguments"));
        }
        if (expr.hasBodyArguments || !expr.bodyArguments.empty()) {
          return st.done(failExprRootDiagnostic(name + " does not accept block arguments"));
        }
        const size_t expectedArgs = (name == "init") ? 2 : 1;
        if (expr.args.size() != expectedArgs) {
          return st.done(failExprRootDiagnostic(
              name + " requires exactly " + std::to_string(expectedArgs) + " argument" +
              (expectedArgs == 1 ? "" : "s")));
        }
        if (name == "init" || name == "drop") {
          return st.done(failExprRootDiagnostic(name + " is only supported as a statement"));
        }
        for (const auto &arg : expr.args) {
          if (!validateExpr(params, locals, arg)) {
            return st.done(false);
          }
        }
        if (!isUninitializedStorage(expr.args.front())) {
          return st.done(failExprRootDiagnostic(name + " requires uninitialized<T> storage"));
        }
        return st.done(true);
      }
    }
    if (isBuiltinBlockCall(expr) && expr.hasBodyArguments) {
      return st.done(validateBlockExpr(params, locals, expr));
    }
    if (isBuiltinBlockCall(expr)) {
      return st.done(failExprRootDiagnostic("block requires block arguments"));
    }
    if (isLoopCall(expr)) {
      return st.done(failExprRootDiagnostic("loop is only supported as a statement"));
    }
    if (isWhileCall(expr)) {
      return st.done(failExprRootDiagnostic("while is only supported as a statement"));
    }
    if (isForCall(expr)) {
      return st.done(failExprRootDiagnostic("for is only supported as a statement"));
    }
    if (isRepeatCall(expr)) {
      return st.done(failExprRootDiagnostic("repeat is only supported as a statement"));
    }
    if (isSimpleCallName(expr, "dispatch")) {
      return st.done(failExprRootDiagnostic("dispatch is only supported as a statement"));
    }
    if (isSimpleCallName(expr, "buffer_store")) {
      return st.done(failExprRootDiagnostic("buffer_store is only supported as a statement"));
    }
  return PhaseStatus::Continue;
}

} // namespace primec::semantics
