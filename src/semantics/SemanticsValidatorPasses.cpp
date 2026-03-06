#include "SemanticsValidator.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <functional>
#include <iterator>
#include <limits>
#include <optional>

namespace primec::semantics {

namespace {

struct HelperSuffixInfo {
  std::string_view suffix;
  std::string_view placement;
};

bool isLifecycleHelperName(const std::string &fullPath) {
  static const std::array<HelperSuffixInfo, 10> suffixes = {{
      {"Create", ""},
      {"Destroy", ""},
      {"Copy", ""},
      {"Move", ""},
      {"CreateStack", "stack"},
      {"DestroyStack", "stack"},
      {"CreateHeap", "heap"},
      {"DestroyHeap", "heap"},
      {"CreateBuffer", "buffer"},
      {"DestroyBuffer", "buffer"},
  }};
  for (const auto &info : suffixes) {
    const std::string_view suffix = info.suffix;
    if (fullPath.size() < suffix.size() + 1) {
      continue;
    }
    const size_t suffixStart = fullPath.size() - suffix.size();
    if (fullPath[suffixStart - 1] != '/') {
      continue;
    }
    if (fullPath.compare(suffixStart, suffix.size(), suffix.data(), suffix.size()) != 0) {
      continue;
    }
    return true;
  }
  return false;
}

} // namespace

std::unordered_set<std::string> SemanticsValidator::resolveEffects(const std::vector<Transform> &transforms,
                                                                    bool isEntry) const {
  bool sawEffects = false;
  std::unordered_set<std::string> effects;
  for (const auto &transform : transforms) {
    if (transform.name != "effects") {
      continue;
    }
    sawEffects = true;
    effects.clear();
    for (const auto &arg : transform.arguments) {
      effects.insert(arg);
    }
  }
  if (!sawEffects) {
    effects = isEntry ? entryDefaultEffectSet_ : defaultEffectSet_;
  }
  return effects;
}

bool SemanticsValidator::validateCapabilitiesSubset(const std::vector<Transform> &transforms,
                                                    const std::string &context) {
  bool sawCapabilities = false;
  std::unordered_set<std::string> capabilities;
  for (const auto &transform : transforms) {
    if (transform.name != "capabilities") {
      continue;
    }
    sawCapabilities = true;
    capabilities.clear();
    for (const auto &arg : transform.arguments) {
      capabilities.insert(arg);
    }
  }
  if (!sawCapabilities) {
    return true;
  }
  for (const auto &capability : capabilities) {
    if (activeEffects_.count(capability) == 0) {
      error_ = "capability requires matching effect on " + context + ": " + capability;
      return false;
    }
  }
  return true;
}

bool SemanticsValidator::resolveExecutionEffects(const Expr &expr, std::unordered_set<std::string> &effectsOut) {
  effectsOut = activeEffects_;
  bool sawEffects = false;
  bool sawCapabilities = false;
  std::unordered_set<std::string> capabilities;
  std::string context = resolveCalleePath(expr);
  if (context.empty()) {
    context = expr.name.empty() ? "<execution>" : expr.name;
  }
  for (const auto &transform : expr.transforms) {
    if (transform.name == "effects") {
      if (sawEffects) {
        error_ = "duplicate effects transform on " + context;
        return false;
      }
      sawEffects = true;
      if (!validateEffectsTransform(transform, context, error_)) {
        return false;
      }
      effectsOut.clear();
      for (const auto &arg : transform.arguments) {
        effectsOut.insert(arg);
      }
    } else if (transform.name == "capabilities") {
      if (sawCapabilities) {
        error_ = "duplicate capabilities transform on " + context;
        return false;
      }
      sawCapabilities = true;
      if (!validateCapabilitiesTransform(transform, context, error_)) {
        return false;
      }
      capabilities.clear();
      for (const auto &arg : transform.arguments) {
        capabilities.insert(arg);
      }
    } else if (transform.name == "return") {
      error_ = "return transform not allowed on executions: " + context;
      return false;
    } else if (transform.name == "on_error") {
      error_ = "on_error transform is not allowed on executions: " + context;
      return false;
    } else if (transform.name == "mut") {
      error_ = "mut transform is not allowed on executions: " + context;
      return false;
    } else if (transform.name == "unsafe") {
      error_ = "unsafe transform is not allowed on executions: " + context;
      return false;
    } else if (transform.name == "copy") {
      error_ = "copy transform is not allowed on executions: " + context;
      return false;
    } else if (transform.name == "restrict") {
      error_ = "restrict transform is not allowed on executions: " + context;
      return false;
    } else if (transform.name == "stack" || transform.name == "heap" || transform.name == "buffer") {
      error_ = "placement transforms are not supported: " + context;
      return false;
    } else if (transform.name == "align_bytes" || transform.name == "align_kbytes") {
      error_ = "alignment transforms are not supported on executions: " + context;
      return false;
    } else if (transform.name == "no_padding" || transform.name == "platform_independent_padding") {
      error_ = "layout transforms are not supported on executions: " + context;
      return false;
    } else if (isBindingQualifierName(transform.name)) {
      error_ = "binding visibility/static transforms are only valid on bindings: " + context;
      return false;
    } else if (isStructTransformName(transform.name)) {
      error_ = "struct transforms are not allowed on executions: " + context;
      return false;
    }
  }
  if (sawEffects) {
    for (const auto &effect : effectsOut) {
      if (activeEffects_.count(effect) == 0) {
        error_ = "execution effects must be a subset of enclosing effects on " + context + ": " + effect;
        return false;
      }
    }
  }
  if (sawCapabilities) {
    for (const auto &capability : capabilities) {
      if (effectsOut.count(capability) == 0) {
        error_ = "capability requires matching effect on " + context + ": " + capability;
        return false;
      }
    }
  }
  return true;
}

bool SemanticsValidator::validateDefinitions() {
  std::vector<SemanticDiagnosticRecord> collectedRecords;
  const bool collectDiagnostics = collectDiagnostics_ && diagnosticInfo_ != nullptr;
  auto resetCollectedState = [&]() {
    if (!collectDiagnostics) {
      return;
    }
    diagnosticInfo_->line = 0;
    diagnosticInfo_->column = 0;
    diagnosticInfo_->relatedSpans.clear();
  };
  auto pushCollectedRecord = [&]() {
    if (!collectDiagnostics || error_.empty()) {
      return;
    }
    SemanticDiagnosticRecord record;
    record.message = error_;
    record.line = diagnosticInfo_->line;
    record.column = diagnosticInfo_->column;
    record.relatedSpans = diagnosticInfo_->relatedSpans;
    collectedRecords.push_back(std::move(record));
    error_.clear();
    resetCollectedState();
  };
  auto isBuiltinCall = [&](const Expr &expr) -> bool {
    if (expr.isMethodCall) {
      return false;
    }
    if (isIfCall(expr) || isMatchCall(expr) || isLoopCall(expr) || isWhileCall(expr) || isForCall(expr) ||
        isRepeatCall(expr) || isReturnCall(expr) || isBlockCall(expr)) {
      return true;
    }
    std::string builtinName;
    return getBuiltinOperatorName(expr, builtinName) || getBuiltinComparisonName(expr, builtinName) ||
           getBuiltinMutationName(expr, builtinName) ||
           getBuiltinClampName(expr, builtinName, allowMathBareName(expr.name)) ||
           getBuiltinMinMaxName(expr, builtinName, allowMathBareName(expr.name)) ||
           getBuiltinAbsSignName(expr, builtinName, allowMathBareName(expr.name)) ||
           getBuiltinSaturateName(expr, builtinName, allowMathBareName(expr.name)) ||
           getBuiltinMathName(expr, builtinName, allowMathBareName(expr.name)) ||
           getBuiltinGpuName(expr, builtinName) || getBuiltinConvertName(expr, builtinName) ||
           getBuiltinArrayAccessName(expr, builtinName) || getBuiltinPointerName(expr, builtinName) ||
           getBuiltinCollectionName(expr, builtinName);
  };
  auto collectDefinitionIntraBodyCallDiagnostics = [&](const Definition &def,
                                                        std::vector<SemanticDiagnosticRecord> &out) {
    const auto &definitionParams = paramsByDef_[def.fullPath];
    std::unordered_map<std::string, BindingInfo> definitionLocals;
    definitionLocals.reserve(definitionParams.size());
    for (const auto &param : definitionParams) {
      definitionLocals.emplace(param.name, param.binding);
    }
    auto appendDefinitionRecord = [&](const Expr &expr, std::string message) {
      SemanticDiagnosticRecord record;
      record.message = std::move(message);
      if (expr.sourceLine > 0 && expr.sourceColumn > 0) {
        record.line = expr.sourceLine;
        record.column = expr.sourceColumn;
      }
      if (def.sourceLine > 0 && def.sourceColumn > 0) {
        SemanticDiagnosticRelatedSpan related;
        related.line = def.sourceLine;
        related.column = def.sourceColumn;
        related.label = "definition: " + def.fullPath;
        record.relatedSpans.push_back(std::move(related));
      }
      out.push_back(std::move(record));
    };
    auto isFlowEffectDiagnosticMessage = [](const std::string &message) {
      return message.rfind("execution effects must be a subset of enclosing effects on ", 0) == 0 ||
             message.rfind("capability requires matching effect on ", 0) == 0 ||
             message.rfind("duplicate effects transform on ", 0) == 0 ||
             message.rfind("duplicate capabilities transform on ", 0) == 0;
    };
    auto collectResolvedCallArgumentDiagnostic = [&](const Expr &expr, const std::string &resolved) -> bool {
      auto appendArgumentTypeMismatch = [&](const std::string &paramName,
                                            const std::string &expectedType,
                                            const std::string &actualType) {
        appendDefinitionRecord(expr,
                               "argument type mismatch for " + resolved + " parameter " + paramName + ": expected " +
                                   expectedType + " got " + actualType);
      };
      std::string message;
      if (!validateNamedArguments(expr.args, expr.argNames, resolved, message)) {
        appendDefinitionRecord(expr, std::move(message));
        return true;
      }
      if (structNames_.count(resolved) > 0) {
        return false;
      }
      auto paramsIt = paramsByDef_.find(resolved);
      if (paramsIt == paramsByDef_.end()) {
        return false;
      }
      const auto &calleeParams = paramsIt->second;
      if (!validateNamedArgumentsAgainstParams(calleeParams, expr.argNames, message)) {
        appendDefinitionRecord(expr, std::move(message));
        return true;
      }
      std::vector<const Expr *> orderedArgs;
      std::string orderError;
      if (!buildOrderedArguments(calleeParams, expr.args, expr.argNames, orderedArgs, orderError)) {
        if (orderError.find("argument count mismatch") != std::string::npos) {
          message = "argument count mismatch for " + resolved;
        } else {
          message = std::move(orderError);
        }
        appendDefinitionRecord(expr, std::move(message));
        return true;
      }
      std::unordered_set<const Expr *> explicitArgs;
      explicitArgs.reserve(expr.args.size());
      for (const auto &arg : expr.args) {
        explicitArgs.insert(&arg);
      }
      for (size_t paramIndex = 0; paramIndex < orderedArgs.size() && paramIndex < calleeParams.size(); ++paramIndex) {
        const Expr *arg = orderedArgs[paramIndex];
        if (arg == nullptr || explicitArgs.count(arg) == 0) {
          continue;
        }
        const ParameterInfo &param = calleeParams[paramIndex];
        const std::string &expectedTypeName = param.binding.typeName;
        if (expectedTypeName.empty() || expectedTypeName == "auto") {
          continue;
        }
        ReturnKind expectedKind = returnKindForTypeName(expectedTypeName);
        if (expectedKind != ReturnKind::Unknown) {
          ReturnKind actualKind = inferExprReturnKind(*arg, definitionParams, definitionLocals);
          if (actualKind == ReturnKind::Unknown || actualKind == expectedKind) {
            continue;
          }
          const std::string expectedName = typeNameForReturnKind(expectedKind);
          const std::string actualName = typeNameForReturnKind(actualKind);
          if (expectedName.empty() || actualName.empty()) {
            continue;
          }
          appendArgumentTypeMismatch(param.name, expectedName, actualName);
          continue;
        }
        const std::string expectedStructPath =
            resolveStructTypePath(expectedTypeName, expr.namespacePrefix, structNames_);
        if (expectedStructPath.empty()) {
          continue;
        }
        const std::string actualStructPath = inferStructReturnPath(*arg, definitionParams, definitionLocals);
        if (!actualStructPath.empty()) {
          if (actualStructPath != expectedStructPath) {
            appendArgumentTypeMismatch(param.name, expectedStructPath, actualStructPath);
          }
          continue;
        }
        ReturnKind actualKind = inferExprReturnKind(*arg, definitionParams, definitionLocals);
        if (actualKind == ReturnKind::Unknown || actualKind == ReturnKind::Array) {
          continue;
        }
        const std::string actualName = typeNameForReturnKind(actualKind);
        if (actualName.empty()) {
          continue;
        }
        appendArgumentTypeMismatch(param.name, expectedStructPath, actualName);
      }
      return false;
    };

    std::function<void(const Expr &)> scanExpr;
    scanExpr = [&](const Expr &expr) {
      if (expr.kind == Expr::Kind::Call) {
        std::optional<EffectScope> effectScope;
        if (!expr.isBinding && !expr.transforms.empty()) {
          std::unordered_set<std::string> executionEffects;
          if (!resolveExecutionEffects(expr, executionEffects)) {
            const std::string effectError = error_;
            error_.clear();
            if (isFlowEffectDiagnosticMessage(effectError)) {
              appendDefinitionRecord(expr, effectError);
            }
            return;
          }
          effectScope.emplace(*this, std::move(executionEffects));
        }
        if (!expr.name.empty() && !isBuiltinCall(expr)) {
          const std::string resolved = resolveCalleePath(expr);
          if (defMap_.count(resolved) == 0) {
            appendDefinitionRecord(expr, "unknown call target: " + expr.name);
          } else {
            collectResolvedCallArgumentDiagnostic(expr, resolved);
          }
        }
      }
      for (const auto &arg : expr.args) {
        scanExpr(arg);
      }
      for (const auto &arg : expr.bodyArguments) {
        scanExpr(arg);
      }
    };

    for (const auto &stmt : def.statements) {
      scanExpr(stmt);
    }
    if (def.returnExpr.has_value()) {
      scanExpr(*def.returnExpr);
    }
  };

  auto validateDefinition = [&](const Definition &def) -> bool {
    DefinitionContextScope definitionScope(*this, def);
    ValidationContextScope validationContextScope(*this, buildDefinitionValidationContext(def));
    auto isStructDefinition = [&](const Definition &candidate) {
      for (const auto &transform : candidate.transforms) {
        if (isStructTransformName(transform.name)) {
          return true;
        }
      }
      return false;
    };
    if (!validateCapabilitiesSubset(def.transforms, def.fullPath)) {
      return false;
    }
    std::unordered_map<std::string, BindingInfo> locals;
    const auto &defParams = paramsByDef_[def.fullPath];
    for (const auto &param : defParams) {
      locals.emplace(param.name, param.binding);
    }
    for (const auto &param : defParams) {
      if (param.defaultExpr == nullptr) {
        continue;
      }
      if (!validateExpr(defParams, locals, *param.defaultExpr)) {
        return false;
      }
    }
    ReturnKind kind = ReturnKind::Unknown;
    auto kindIt = returnKinds_.find(def.fullPath);
    if (kindIt != returnKinds_.end()) {
      kind = kindIt->second;
    }
    if (isLifecycleHelperName(def.fullPath) && kind != ReturnKind::Void) {
      error_ = "lifecycle helpers must return void: " + def.fullPath;
      return false;
    }
    currentResultType_.reset();
    for (const auto &transform : def.transforms) {
      if (transform.name == "return" && transform.templateArgs.size() == 1) {
        ResultTypeInfo resultInfo;
        if (resolveResultTypeFromTypeName(transform.templateArgs.front(), resultInfo)) {
          currentResultType_ = resultInfo;
          break;
        }
      }
    }
    std::optional<OnErrorHandler> onErrorHandler;
    if (!parseOnErrorTransform(def.transforms, def.namespacePrefix, def.fullPath, onErrorHandler)) {
      return false;
    }
    if (onErrorHandler.has_value() && (!currentResultType_.has_value() || !currentResultType_->isResult)) {
      error_ = "on_error requires Result return type on " + def.fullPath;
      return false;
    }
    if (onErrorHandler.has_value() &&
        !errorTypesMatch(onErrorHandler->errorType, currentResultType_->errorType, def.namespacePrefix)) {
      error_ = "on_error error type mismatch on " + def.fullPath;
      return false;
    }
    if (onErrorHandler.has_value()) {
      OnErrorScope onErrorScope(*this, std::nullopt);
      for (const auto &arg : onErrorHandler->boundArgs) {
        if (!validateExpr(defParams, locals, arg)) {
          return false;
        }
      }
    }
    OnErrorScope onErrorScope(*this, onErrorHandler);
    bool sawReturn = false;
    for (size_t stmtIndex = 0; stmtIndex < def.statements.size(); ++stmtIndex) {
      const Expr &stmt = def.statements[stmtIndex];
      if (!validateStatement(defParams,
                             locals,
                             stmt,
                             kind,
                             true,
                             true,
                             &sawReturn,
                             def.namespacePrefix,
                             &def.statements,
                             stmtIndex)) {
        return false;
      }
      expireReferenceBorrowsForRemainder(defParams, locals, def.statements, stmtIndex + 1);
    }
    if (kind != ReturnKind::Void && !isStructDefinition(def)) {
      bool allPathsReturn = blockAlwaysReturns(def.statements);
      if (!allPathsReturn) {
        if (sawReturn) {
          error_ = "not all control paths return in " + def.fullPath;
        } else {
          error_ = "missing return statement in " + def.fullPath;
        }
        return false;
      }
    }
    bool shouldCheckUninitialized = (kind == ReturnKind::Void);
    if (kind != ReturnKind::Void && !isStructDefinition(def)) {
      shouldCheckUninitialized = blockAlwaysReturns(def.statements);
    }
    if (shouldCheckUninitialized) {
      std::optional<std::string> uninitError =
          validateUninitializedDefiniteState(defParams, def.statements);
      if (uninitError.has_value()) {
        error_ = *uninitError;
        return false;
      }
    }
    return true;
  };

  for (const auto &def : program_.definitions) {
    resetCollectedState();
    if (collectDiagnostics) {
      ValidationContextScope validationContextScope(*this, buildDefinitionValidationContext(def));
      std::vector<SemanticDiagnosticRecord> intraDefinitionRecords;
      collectDefinitionIntraBodyCallDiagnostics(def, intraDefinitionRecords);
      if (!intraDefinitionRecords.empty()) {
        if (error_.empty()) {
          error_ = intraDefinitionRecords.front().message;
        }
        collectedRecords.insert(collectedRecords.end(),
                                std::make_move_iterator(intraDefinitionRecords.begin()),
                                std::make_move_iterator(intraDefinitionRecords.end()));
        continue;
      }
    }
    if (!validateDefinition(def)) {
      if (!collectDiagnostics) {
        return false;
      }
      pushCollectedRecord();
    }
  }

  if (collectDiagnostics && !collectedRecords.empty()) {
    diagnosticInfo_->records = std::move(collectedRecords);
    if (!diagnosticInfo_->records.empty()) {
      diagnosticInfo_->line = diagnosticInfo_->records.front().line;
      diagnosticInfo_->column = diagnosticInfo_->records.front().column;
      diagnosticInfo_->relatedSpans = diagnosticInfo_->records.front().relatedSpans;
      error_ = diagnosticInfo_->records.front().message;
    }
    return false;
  }

  return true;
}

std::optional<std::string> SemanticsValidator::validateUninitializedDefiniteState(
    const std::vector<ParameterInfo> &params,
    const std::vector<Expr> &statements) {
  enum class UninitState { Uninitialized, Initialized, Unknown };
  using StateMap = std::unordered_map<std::string, UninitState>;

  auto isUninitializedBinding = [](const BindingInfo &binding) -> bool {
    return binding.typeName == "uninitialized" && !binding.typeTemplateArg.empty();
  };

  auto firstNonUninitialized = [&](const StateMap &states) -> std::optional<std::string> {
    std::vector<std::string> names;
    names.reserve(states.size());
    for (const auto &entry : states) {
      if (entry.second != UninitState::Uninitialized) {
        names.push_back(entry.first);
      }
    }
    if (names.empty()) {
      return std::nullopt;
    }
    std::sort(names.begin(), names.end());
    return names.front();
  };

  auto mergeStates = [&](const StateMap &base, const StateMap &left, const StateMap &right) -> StateMap {
    StateMap merged = base;
    for (const auto &entry : base) {
      const auto leftIt = left.find(entry.first);
      const auto rightIt = right.find(entry.first);
      if (leftIt == left.end() || rightIt == right.end()) {
        merged[entry.first] = UninitState::Unknown;
        continue;
      }
      merged[entry.first] = (leftIt->second == rightIt->second) ? leftIt->second : UninitState::Unknown;
    }
    return merged;
  };

  auto statesEqual = [](const StateMap &left, const StateMap &right) -> bool {
    if (left.size() != right.size()) {
      return false;
    }
    for (const auto &entry : left) {
      const auto it = right.find(entry.first);
      if (it == right.end() || it->second != entry.second) {
        return false;
      }
    }
    return true;
  };

  auto loopIterationLimit = [](const StateMap &statesIn) -> size_t {
    if (statesIn.empty()) {
      return 2;
    }
    return statesIn.size() * 3 + 2;
  };

  auto integerLiteralCount = [](const Expr &expr) -> std::optional<uint64_t> {
    if (expr.kind != Expr::Kind::Literal) {
      return std::nullopt;
    }
    if (expr.isUnsigned) {
      return expr.literalValue;
    }
    if (expr.intWidth == 32) {
      const int32_t value = static_cast<int32_t>(expr.literalValue);
      if (value < 0) {
        return std::nullopt;
      }
      return static_cast<uint64_t>(value);
    }
    const int64_t value = static_cast<int64_t>(expr.literalValue);
    if (value < 0) {
      return std::nullopt;
    }
    return static_cast<uint64_t>(value);
  };

  auto boundedLoopCount = [&](const Expr &countExpr) -> std::optional<size_t> {
    constexpr uint64_t kMaxBoundedCount = 1;
    std::optional<uint64_t> count = integerLiteralCount(countExpr);
    if (!count.has_value() || *count > kMaxBoundedCount) {
      return std::nullopt;
    }
    return static_cast<size_t>(*count);
  };

  auto boundedRepeatCount = [&](const Expr &countExpr) -> std::optional<size_t> {
    if (countExpr.kind == Expr::Kind::BoolLiteral) {
      return countExpr.boolValue ? static_cast<size_t>(1) : static_cast<size_t>(0);
    }
    return boundedLoopCount(countExpr);
  };

  auto applyStorageCall = [&](const std::string &callName,
                              const Expr &target,
                              std::unordered_map<std::string, BindingInfo> &localsIn,
                              StateMap &statesIn,
                              bool consumeBorrow) -> std::optional<std::string> {
    if (target.kind != Expr::Kind::Name) {
      return std::nullopt;
    }
    auto bindingIt = localsIn.find(target.name);
    if (bindingIt == localsIn.end() || !isUninitializedBinding(bindingIt->second)) {
      return std::nullopt;
    }
    auto stateIt = statesIn.find(target.name);
    UninitState current = stateIt == statesIn.end() ? UninitState::Unknown : stateIt->second;
    if (callName == "init") {
      if (current != UninitState::Uninitialized) {
        return "init requires uninitialized storage: " + target.name;
      }
      statesIn[target.name] = UninitState::Initialized;
      return std::nullopt;
    }
    if (callName == "drop") {
      if (current != UninitState::Initialized) {
        return "drop requires initialized storage: " + target.name;
      }
      statesIn[target.name] = UninitState::Uninitialized;
      return std::nullopt;
    }
    if (callName == "take") {
      if (current != UninitState::Initialized) {
        return "take requires initialized storage: " + target.name;
      }
      statesIn[target.name] = UninitState::Uninitialized;
      return std::nullopt;
    }
    if (callName == "borrow") {
      if (current != UninitState::Initialized) {
        return "borrow requires initialized storage: " + target.name;
      }
      statesIn[target.name] = consumeBorrow ? UninitState::Uninitialized : UninitState::Initialized;
      return std::nullopt;
    }
    return std::nullopt;
  };

  struct FlowResult {
    std::optional<std::string> error;
    bool terminated = false;
  };

  std::function<std::optional<std::string>(const Expr &,
                                           std::unordered_map<std::string, BindingInfo> &,
                                           StateMap &)>
      applyExprEffects;

  std::function<std::optional<std::string>(const std::vector<Expr> &,
                                           std::unordered_map<std::string, BindingInfo>,
                                           StateMap,
                                           StateMap &)>
      analyzeValueBlock;

  std::function<FlowResult(const Expr &,
                           std::unordered_map<std::string, BindingInfo> &,
                           StateMap &)>
      analyzeStatement;

  std::function<FlowResult(const std::vector<Expr> &,
                           std::unordered_map<std::string, BindingInfo>,
                           StateMap,
                           StateMap &)>
      analyzeStatements;

  analyzeStatements = [&](const std::vector<Expr> &stmts,
                          std::unordered_map<std::string, BindingInfo> localsIn,
                          StateMap statesIn,
                          StateMap &statesOut) -> FlowResult {
    for (const auto &stmt : stmts) {
      FlowResult result = analyzeStatement(stmt, localsIn, statesIn);
      if (result.error.has_value()) {
        return result;
      }
      if (result.terminated) {
        statesOut = std::move(statesIn);
        return result;
      }
    }
    statesOut = std::move(statesIn);
    return {};
  };

  analyzeValueBlock = [&](const std::vector<Expr> &stmts,
                          std::unordered_map<std::string, BindingInfo> localsIn,
                          StateMap statesIn,
                          StateMap &statesOut) -> std::optional<std::string> {
    if (stmts.empty()) {
      statesOut = std::move(statesIn);
      return std::nullopt;
    }
    for (size_t i = 0; i < stmts.size(); ++i) {
      const Expr &stmt = stmts[i];
      const bool isLast = (i + 1 == stmts.size());
      if (stmt.isBinding) {
        BindingInfo info;
        std::optional<std::string> restrictType;
        std::string error;
        if (!parseBindingInfo(stmt, stmt.namespacePrefix, structNames_, importAliases_, info, restrictType, error)) {
          return error;
        }
        localsIn.emplace(stmt.name, info);
        if (isUninitializedBinding(info)) {
          statesIn[stmt.name] = UninitState::Uninitialized;
        }
        continue;
      }
      if (isReturnCall(stmt)) {
        for (const auto &arg : stmt.args) {
          if (auto err = applyExprEffects(arg, localsIn, statesIn)) {
            return err;
          }
        }
        statesOut = std::move(statesIn);
        return std::nullopt;
      }
      if (!stmt.isMethodCall &&
          (isSimpleCallName(stmt, "init") || isSimpleCallName(stmt, "drop") ||
           isSimpleCallName(stmt, "take") || isSimpleCallName(stmt, "borrow")) &&
          !stmt.args.empty()) {
        if (auto err = applyStorageCall(stmt.name, stmt.args.front(), localsIn, statesIn, false)) {
          return err;
        }
        if (isLast) {
          statesOut = std::move(statesIn);
          return std::nullopt;
        }
        continue;
      }
      if (auto err = applyExprEffects(stmt, localsIn, statesIn)) {
        return err;
      }
      if (isLast) {
        statesOut = std::move(statesIn);
        return std::nullopt;
      }
    }
    statesOut = std::move(statesIn);
    return std::nullopt;
  };

  analyzeStatement = [&](const Expr &stmt,
                         std::unordered_map<std::string, BindingInfo> &localsIn,
                         StateMap &statesIn) -> FlowResult {
    applyExprEffects = [&](const Expr &expr,
                           std::unordered_map<std::string, BindingInfo> &locals,
                           StateMap &states) -> std::optional<std::string> {
      if (expr.kind != Expr::Kind::Call) {
        return std::nullopt;
      }
      if (!expr.isMethodCall &&
          (isSimpleCallName(expr, "take") || isSimpleCallName(expr, "borrow")) &&
          !expr.args.empty()) {
        if (auto err = applyStorageCall(expr.name, expr.args.front(), locals, states, true)) {
          return err;
        }
      }
      if (isMatchCall(expr)) {
        Expr expanded;
        std::string error;
        if (!lowerMatchToIf(expr, expanded, error)) {
          return error;
        }
        return applyExprEffects(expanded, locals, states);
      }
      if (isIfCall(expr) && expr.args.size() == 3) {
        if (auto err = applyExprEffects(expr.args[0], locals, states)) {
          return err;
        }
        StateMap thenStates = states;
        StateMap elseStates = states;
        const Expr &thenArg = expr.args[1];
        const Expr &elseArg = expr.args[2];
        if (thenArg.kind == Expr::Kind::Call && (thenArg.hasBodyArguments || !thenArg.bodyArguments.empty())) {
          StateMap branchStates;
          if (auto err = analyzeValueBlock(thenArg.bodyArguments, locals, thenStates, branchStates)) {
            return err;
          }
          thenStates = std::move(branchStates);
        } else if (auto err = applyExprEffects(thenArg, locals, thenStates)) {
          return err;
        }
        if (elseArg.kind == Expr::Kind::Call && (elseArg.hasBodyArguments || !elseArg.bodyArguments.empty())) {
          StateMap branchStates;
          if (auto err = analyzeValueBlock(elseArg.bodyArguments, locals, elseStates, branchStates)) {
            return err;
          }
          elseStates = std::move(branchStates);
        } else if (auto err = applyExprEffects(elseArg, locals, elseStates)) {
          return err;
        }
        states = mergeStates(states, thenStates, elseStates);
        return std::nullopt;
      }
      for (const auto &arg : expr.args) {
        if (auto err = applyExprEffects(arg, locals, states)) {
          return err;
        }
      }
      if (expr.hasBodyArguments || !expr.bodyArguments.empty()) {
        StateMap bodyStates;
        if (auto err = analyzeValueBlock(expr.bodyArguments, locals, states, bodyStates)) {
          return err;
        }
        states = std::move(bodyStates);
      }
      return std::nullopt;
    };

    if (stmt.isBinding) {
      BindingInfo info;
      std::optional<std::string> restrictType;
      std::string error;
      if (!parseBindingInfo(stmt, stmt.namespacePrefix, structNames_, importAliases_, info, restrictType, error)) {
        return {error, false};
      }
      localsIn.emplace(stmt.name, info);
      if (isUninitializedBinding(info)) {
        statesIn[stmt.name] = UninitState::Uninitialized;
      }
      return {};
    }
    if (stmt.kind != Expr::Kind::Call) {
      return {};
    }
    if (isReturnCall(stmt)) {
      for (const auto &arg : stmt.args) {
        if (auto err = applyExprEffects(arg, localsIn, statesIn)) {
          return {err, false};
        }
      }
      if (auto name = firstNonUninitialized(statesIn)) {
        return {"return requires uninitialized storage to be dropped: " + *name, false};
      }
      return {std::nullopt, true};
    }
    if (isMatchCall(stmt)) {
      Expr expanded;
      std::string error;
      if (!lowerMatchToIf(stmt, expanded, error)) {
        return {error, false};
      }
      return analyzeStatement(expanded, localsIn, statesIn);
    }
    if (isIfCall(stmt) && stmt.args.size() == 3) {
      if (auto err = applyExprEffects(stmt.args.front(), localsIn, statesIn)) {
        return {err, false};
      }
      const Expr &thenArg = stmt.args[1];
      const Expr &elseArg = stmt.args[2];
      StateMap thenStates;
      StateMap elseStates;
      FlowResult thenResult;
      FlowResult elseResult;
      if (thenArg.kind == Expr::Kind::Call && (thenArg.hasBodyArguments || !thenArg.bodyArguments.empty())) {
        thenResult = analyzeStatements(thenArg.bodyArguments, localsIn, statesIn, thenStates);
        if (thenResult.error.has_value()) {
          return thenResult;
        }
      } else {
        thenStates = statesIn;
      }
      if (elseArg.kind == Expr::Kind::Call && (elseArg.hasBodyArguments || !elseArg.bodyArguments.empty())) {
        elseResult = analyzeStatements(elseArg.bodyArguments, localsIn, statesIn, elseStates);
        if (elseResult.error.has_value()) {
          return elseResult;
        }
      } else {
        elseStates = statesIn;
      }
      if (thenResult.terminated && elseResult.terminated) {
        return {std::nullopt, true};
      }
      if (thenResult.terminated && !elseResult.terminated) {
        statesIn = elseStates;
        return {};
      }
      if (!thenResult.terminated && elseResult.terminated) {
        statesIn = thenStates;
        return {};
      }
      statesIn = mergeStates(statesIn, thenStates, elseStates);
      return {};
    }
    auto analyzeLoopBody = [&](const Expr &body,
                               const std::unordered_map<std::string, BindingInfo> &localsBase,
                               const StateMap &statesBase,
                               StateMap &statesOut,
                               bool &terminatedOut) -> FlowResult {
      if (body.kind == Expr::Kind::Call && (body.hasBodyArguments || !body.bodyArguments.empty())) {
        FlowResult result = analyzeStatements(body.bodyArguments, localsBase, statesBase, statesOut);
        terminatedOut = result.terminated;
        return result;
      }
      statesOut = statesBase;
      terminatedOut = false;
      return {};
    };
    if (isLoopCall(stmt)) {
      if (stmt.args.size() >= 2) {
        if (auto err = applyExprEffects(stmt.args.front(), localsIn, statesIn)) {
          return {err, false};
        }
        if (auto exactIterations = boundedLoopCount(stmt.args.front()); exactIterations.has_value()) {
          StateMap iterStates = statesIn;
          for (size_t i = 0; i < *exactIterations; ++i) {
            StateMap bodyStates;
            bool bodyTerminated = false;
            FlowResult result = analyzeLoopBody(stmt.args[1], localsIn, iterStates, bodyStates, bodyTerminated);
            if (result.error.has_value()) {
              return result;
            }
            if (bodyTerminated) {
              statesIn = iterStates;
              return {};
            }
            iterStates = std::move(bodyStates);
          }
          statesIn = std::move(iterStates);
          return {};
        }
        StateMap loopHead = statesIn;
        StateMap exitStates = loopHead;
        const size_t maxIterations = loopIterationLimit(loopHead);
        for (size_t i = 0; i < maxIterations; ++i) {
          StateMap bodyStates;
          bool bodyTerminated = false;
          FlowResult result = analyzeLoopBody(stmt.args[1], localsIn, loopHead, bodyStates, bodyTerminated);
          if (result.error.has_value()) {
            return result;
          }
          if (bodyTerminated) {
            statesIn = exitStates;
            return {};
          }
          exitStates = mergeStates(exitStates, exitStates, bodyStates);
          StateMap nextHead = mergeStates(loopHead, loopHead, bodyStates);
          if (statesEqual(nextHead, loopHead)) {
            statesIn = exitStates;
            return {};
          }
          loopHead = std::move(nextHead);
        }
        statesIn = exitStates;
      }
      return {};
    }
    if (isWhileCall(stmt)) {
      if (stmt.args.size() >= 2) {
        StateMap loopHead = statesIn;
        StateMap exitStates = statesIn;
        bool hasExitStates = false;
        const size_t maxIterations = loopIterationLimit(loopHead);
        for (size_t i = 0; i < maxIterations; ++i) {
          StateMap conditionStates = loopHead;
          if (auto err = applyExprEffects(stmt.args.front(), localsIn, conditionStates)) {
            return {err, false};
          }
          if (!hasExitStates) {
            exitStates = conditionStates;
            hasExitStates = true;
          } else {
            exitStates = mergeStates(exitStates, exitStates, conditionStates);
          }
          StateMap bodyStates;
          bool bodyTerminated = false;
          FlowResult result = analyzeLoopBody(stmt.args[1], localsIn, conditionStates, bodyStates, bodyTerminated);
          if (result.error.has_value()) {
            return result;
          }
          if (bodyTerminated) {
            statesIn = exitStates;
            return {};
          }
          StateMap nextHead = mergeStates(loopHead, loopHead, bodyStates);
          if (statesEqual(nextHead, loopHead)) {
            statesIn = exitStates;
            return {};
          }
          loopHead = std::move(nextHead);
        }
        if (hasExitStates) {
          statesIn = exitStates;
        }
      }
      return {};
    }
    if (isForCall(stmt)) {
      StateMap loopStates = statesIn;
      std::unordered_map<std::string, BindingInfo> loopLocals = localsIn;
      auto applyForCondition = [&](const Expr &condExpr,
                                   std::unordered_map<std::string, BindingInfo> &condLocals,
                                   StateMap &condStates) -> FlowResult {
        if (condExpr.isBinding) {
          return analyzeStatement(condExpr, condLocals, condStates);
        }
        if (auto err = applyExprEffects(condExpr, condLocals, condStates)) {
          return {err, false};
        }
        return {};
      };
      if (stmt.args.size() >= 1) {
        FlowResult result = analyzeStatement(stmt.args[0], loopLocals, loopStates);
        if (result.error.has_value()) {
          return result;
        }
      }
      if (stmt.args.size() < 2) {
        statesIn = loopStates;
        return {};
      }
      FlowResult initialCondition = applyForCondition(stmt.args[1], loopLocals, loopStates);
      if (initialCondition.error.has_value()) {
        return initialCondition;
      }
      StateMap exitStates = loopStates;
      if (stmt.args.size() >= 4) {
        StateMap bodyStates;
        bool bodyTerminated = false;
        FlowResult result = analyzeLoopBody(stmt.args[3], loopLocals, loopStates, bodyStates, bodyTerminated);
        if (result.error.has_value()) {
          return result;
        }
        if (!bodyTerminated) {
          StateMap iterStates = bodyStates;
          if (stmt.args.size() >= 3) {
            FlowResult stepResult = analyzeStatement(stmt.args[2], loopLocals, iterStates);
            if (stepResult.error.has_value()) {
              return stepResult;
            }
            if (stepResult.terminated) {
              statesIn = exitStates;
              return {};
            }
          }
          FlowResult nextCondition = applyForCondition(stmt.args[1], loopLocals, iterStates);
          if (nextCondition.error.has_value()) {
            return nextCondition;
          }
          if (!nextCondition.terminated) {
            exitStates = mergeStates(exitStates, exitStates, iterStates);
          }
        }
      }
      statesIn = exitStates;
      return {};
    }
    if (isRepeatCall(stmt)) {
      if (!stmt.args.empty()) {
        if (auto err = applyExprEffects(stmt.args.front(), localsIn, statesIn)) {
          return {err, false};
        }
        if (auto exactIterations = boundedRepeatCount(stmt.args.front()); exactIterations.has_value()) {
          StateMap iterStates = statesIn;
          for (size_t i = 0; i < *exactIterations; ++i) {
            StateMap bodyStates;
            FlowResult result = analyzeStatements(stmt.bodyArguments, localsIn, iterStates, bodyStates);
            if (result.error.has_value()) {
              return result;
            }
            if (result.terminated) {
              statesIn = iterStates;
              return {};
            }
            iterStates = std::move(bodyStates);
          }
          statesIn = std::move(iterStates);
          return {};
        }
      }
      StateMap loopHead = statesIn;
      StateMap exitStates = loopHead;
      const size_t maxIterations = loopIterationLimit(loopHead);
      for (size_t i = 0; i < maxIterations; ++i) {
        StateMap bodyStates;
        FlowResult result = analyzeStatements(stmt.bodyArguments, localsIn, loopHead, bodyStates);
        if (result.error.has_value()) {
          return result;
        }
        if (result.terminated) {
          statesIn = exitStates;
          return {};
        }
        exitStates = mergeStates(exitStates, exitStates, bodyStates);
        StateMap nextHead = mergeStates(loopHead, loopHead, bodyStates);
        if (statesEqual(nextHead, loopHead)) {
          statesIn = exitStates;
          return {};
        }
        loopHead = std::move(nextHead);
      }
      statesIn = exitStates;
      return {};
    }
    if (stmt.hasBodyArguments || !stmt.bodyArguments.empty()) {
      StateMap bodyStates;
      FlowResult result = analyzeStatements(stmt.bodyArguments, localsIn, statesIn, bodyStates);
      if (result.error.has_value()) {
        return result;
      }
      if (!result.terminated) {
        for (const auto &entry : statesIn) {
          auto it = bodyStates.find(entry.first);
          if (it != bodyStates.end()) {
            statesIn[entry.first] = it->second;
          }
        }
      }
      return {};
    }
    if (!stmt.isMethodCall &&
        (isSimpleCallName(stmt, "init") || isSimpleCallName(stmt, "drop") ||
         isSimpleCallName(stmt, "take") || isSimpleCallName(stmt, "borrow")) &&
        !stmt.args.empty()) {
      const std::string name = stmt.name;
      if (auto err = applyStorageCall(name, stmt.args.front(), localsIn, statesIn, false)) {
        return {err, false};
      }
      return {};
    }
    return {};
  };

  std::unordered_map<std::string, BindingInfo> locals;
  locals.reserve(params.size());
  for (const auto &param : params) {
    locals.emplace(param.name, param.binding);
  }
  StateMap states;
  StateMap finalStates;
  FlowResult result = analyzeStatements(statements, locals, states, finalStates);
  if (result.error.has_value()) {
    return result.error;
  }
  if (result.terminated) {
    return std::nullopt;
  }
  if (auto name = firstNonUninitialized(finalStates)) {
    return "uninitialized storage must be dropped before end of scope: " + *name;
  }
  return std::nullopt;
}

bool SemanticsValidator::validateExecutions() {
  std::vector<SemanticDiagnosticRecord> collectedRecords;
  const bool collectDiagnostics = collectDiagnostics_ && diagnosticInfo_ != nullptr;
  auto resetCollectedState = [&]() {
    if (!collectDiagnostics) {
      return;
    }
    diagnosticInfo_->line = 0;
    diagnosticInfo_->column = 0;
    diagnosticInfo_->relatedSpans.clear();
  };
  auto pushCollectedRecord = [&]() {
    if (!collectDiagnostics || error_.empty()) {
      return;
    }
    SemanticDiagnosticRecord record;
    record.message = error_;
    record.line = diagnosticInfo_->line;
    record.column = diagnosticInfo_->column;
    record.relatedSpans = diagnosticInfo_->relatedSpans;
    collectedRecords.push_back(std::move(record));
    error_.clear();
    resetCollectedState();
  };
  auto isBuiltinCall = [&](const Expr &expr) -> bool {
    if (expr.isMethodCall) {
      return false;
    }
    if (isIfCall(expr) || isMatchCall(expr) || isLoopCall(expr) || isWhileCall(expr) || isForCall(expr) ||
        isRepeatCall(expr) || isReturnCall(expr) || isBlockCall(expr)) {
      return true;
    }
    std::string builtinName;
    return getBuiltinOperatorName(expr, builtinName) || getBuiltinComparisonName(expr, builtinName) ||
           getBuiltinMutationName(expr, builtinName) ||
           getBuiltinClampName(expr, builtinName, allowMathBareName(expr.name)) ||
           getBuiltinMinMaxName(expr, builtinName, allowMathBareName(expr.name)) ||
           getBuiltinAbsSignName(expr, builtinName, allowMathBareName(expr.name)) ||
           getBuiltinSaturateName(expr, builtinName, allowMathBareName(expr.name)) ||
           getBuiltinMathName(expr, builtinName, allowMathBareName(expr.name)) ||
           getBuiltinGpuName(expr, builtinName) || getBuiltinConvertName(expr, builtinName) ||
           getBuiltinArrayAccessName(expr, builtinName) || getBuiltinPointerName(expr, builtinName) ||
           getBuiltinCollectionName(expr, builtinName);
  };
  auto collectExecutionIntraBodyCallDiagnostics = [&](const Execution &exec,
                                                      std::vector<SemanticDiagnosticRecord> &out) {
    const std::vector<ParameterInfo> executionParams;
    const std::unordered_map<std::string, BindingInfo> executionLocals;
    auto appendExecutionRecord = [&](const Expr &expr, std::string message) {
      SemanticDiagnosticRecord record;
      record.message = std::move(message);
      if (expr.sourceLine > 0 && expr.sourceColumn > 0) {
        record.line = expr.sourceLine;
        record.column = expr.sourceColumn;
      }
      if (exec.sourceLine > 0 && exec.sourceColumn > 0) {
        SemanticDiagnosticRelatedSpan related;
        related.line = exec.sourceLine;
        related.column = exec.sourceColumn;
        related.label = "execution: " + exec.fullPath;
        record.relatedSpans.push_back(std::move(related));
      }
      out.push_back(std::move(record));
    };
    auto isFlowEffectDiagnosticMessage = [](const std::string &message) {
      return message.rfind("execution effects must be a subset of enclosing effects on ", 0) == 0 ||
             message.rfind("capability requires matching effect on ", 0) == 0 ||
             message.rfind("duplicate effects transform on ", 0) == 0 ||
             message.rfind("duplicate capabilities transform on ", 0) == 0;
    };
    auto collectResolvedCallArgumentDiagnostic = [&](const Expr &expr, const std::string &resolved) -> bool {
      auto appendArgumentTypeMismatch = [&](const std::string &paramName,
                                            const std::string &expectedType,
                                            const std::string &actualType) {
        appendExecutionRecord(expr,
                              "argument type mismatch for " + resolved + " parameter " + paramName + ": expected " +
                                  expectedType + " got " + actualType);
      };
      std::string message;
      if (!validateNamedArguments(expr.args, expr.argNames, resolved, message)) {
        appendExecutionRecord(expr, std::move(message));
        return true;
      }
      if (structNames_.count(resolved) > 0) {
        return false;
      }
      auto paramsIt = paramsByDef_.find(resolved);
      if (paramsIt == paramsByDef_.end()) {
        return false;
      }
      const auto &calleeParams = paramsIt->second;
      if (!validateNamedArgumentsAgainstParams(calleeParams, expr.argNames, message)) {
        appendExecutionRecord(expr, std::move(message));
        return true;
      }
      std::vector<const Expr *> orderedArgs;
      std::string orderError;
      if (!buildOrderedArguments(calleeParams, expr.args, expr.argNames, orderedArgs, orderError)) {
        if (orderError.find("argument count mismatch") != std::string::npos) {
          message = "argument count mismatch for " + resolved;
        } else {
          message = std::move(orderError);
        }
        appendExecutionRecord(expr, std::move(message));
        return true;
      }
      std::unordered_set<const Expr *> explicitArgs;
      explicitArgs.reserve(expr.args.size());
      for (const auto &arg : expr.args) {
        explicitArgs.insert(&arg);
      }
      for (size_t paramIndex = 0; paramIndex < orderedArgs.size() && paramIndex < calleeParams.size(); ++paramIndex) {
        const Expr *arg = orderedArgs[paramIndex];
        if (arg == nullptr || explicitArgs.count(arg) == 0) {
          continue;
        }
        const ParameterInfo &param = calleeParams[paramIndex];
        const std::string &expectedTypeName = param.binding.typeName;
        if (expectedTypeName.empty() || expectedTypeName == "auto") {
          continue;
        }
        ReturnKind expectedKind = returnKindForTypeName(expectedTypeName);
        if (expectedKind != ReturnKind::Unknown) {
          ReturnKind actualKind = inferExprReturnKind(*arg, executionParams, executionLocals);
          if (actualKind == ReturnKind::Unknown || actualKind == expectedKind) {
            continue;
          }
          const std::string expectedName = typeNameForReturnKind(expectedKind);
          const std::string actualName = typeNameForReturnKind(actualKind);
          if (expectedName.empty() || actualName.empty()) {
            continue;
          }
          appendArgumentTypeMismatch(param.name, expectedName, actualName);
          continue;
        }
        const std::string expectedStructPath =
            resolveStructTypePath(expectedTypeName, expr.namespacePrefix, structNames_);
        if (expectedStructPath.empty()) {
          continue;
        }
        const std::string actualStructPath = inferStructReturnPath(*arg, executionParams, executionLocals);
        if (!actualStructPath.empty()) {
          if (actualStructPath != expectedStructPath) {
            appendArgumentTypeMismatch(param.name, expectedStructPath, actualStructPath);
          }
          continue;
        }
        ReturnKind actualKind = inferExprReturnKind(*arg, executionParams, executionLocals);
        if (actualKind == ReturnKind::Unknown || actualKind == ReturnKind::Array) {
          continue;
        }
        const std::string actualName = typeNameForReturnKind(actualKind);
        if (actualName.empty()) {
          continue;
        }
        appendArgumentTypeMismatch(param.name, expectedStructPath, actualName);
      }
      return false;
    };

    std::function<void(const Expr &)> scanExpr;
    scanExpr = [&](const Expr &expr) {
      if (expr.kind == Expr::Kind::Call) {
        std::optional<EffectScope> effectScope;
        if (!expr.isBinding && !expr.transforms.empty()) {
          std::unordered_set<std::string> executionEffects;
          if (!resolveExecutionEffects(expr, executionEffects)) {
            const std::string effectError = error_;
            error_.clear();
            if (isFlowEffectDiagnosticMessage(effectError)) {
              appendExecutionRecord(expr, effectError);
            }
            return;
          }
          effectScope.emplace(*this, std::move(executionEffects));
        }
        if (!expr.name.empty() && !isBuiltinCall(expr)) {
          const std::string resolved = resolveCalleePath(expr);
          if (defMap_.count(resolved) == 0) {
            appendExecutionRecord(expr, "unknown call target: " + expr.name);
          } else {
            collectResolvedCallArgumentDiagnostic(expr, resolved);
          }
        }
      }
      for (const auto &arg : expr.args) {
        scanExpr(arg);
      }
      for (const auto &arg : expr.bodyArguments) {
        scanExpr(arg);
      }
    };

    for (const auto &arg : exec.arguments) {
      scanExpr(arg);
    }
    for (const auto &arg : exec.bodyArguments) {
      scanExpr(arg);
    }
  };

  auto validateExecution = [&](const Execution &exec) -> bool {
    ExecutionContextScope executionScope(*this, exec);
    ValidationContextScope validationContextScope(*this, buildExecutionValidationContext(exec));
    bool sawEffects = false;
    bool sawCapabilities = false;
    for (const auto &transform : exec.transforms) {
      if (transform.name == "return") {
        error_ = "return transform not allowed on executions: " + exec.fullPath;
        return false;
      }
      if (transform.name == "mut") {
        error_ = "mut transform is not allowed on executions: " + exec.fullPath;
        return false;
      }
      if (transform.name == "unsafe") {
        error_ = "unsafe transform is not allowed on executions: " + exec.fullPath;
        return false;
      }
      if (transform.name == "no_padding" || transform.name == "platform_independent_padding") {
        error_ = "layout transforms are not supported on executions: " + exec.fullPath;
        return false;
      }
      if (isBindingQualifierName(transform.name)) {
        error_ = "binding visibility/static transforms are only valid on bindings: " + exec.fullPath;
        return false;
      }
      if (transform.name == "copy") {
        error_ = "copy transform is not allowed on executions: " + exec.fullPath;
        return false;
      }
      if (transform.name == "restrict") {
        error_ = "restrict transform is not allowed on executions: " + exec.fullPath;
        return false;
      }
      if (transform.name == "stack" || transform.name == "heap" || transform.name == "buffer") {
        error_ = "placement transforms are not supported: " + exec.fullPath;
        return false;
      }
      if (transform.name == "effects") {
        if (sawEffects) {
          error_ = "duplicate effects transform on " + exec.fullPath;
          return false;
        }
        sawEffects = true;
        if (!validateEffectsTransform(transform, exec.fullPath, error_)) {
          return false;
        }
      } else if (transform.name == "capabilities") {
        if (sawCapabilities) {
          error_ = "duplicate capabilities transform on " + exec.fullPath;
          return false;
        }
        sawCapabilities = true;
        if (!validateCapabilitiesTransform(transform, exec.fullPath, error_)) {
          return false;
        }
      } else if (transform.name == "align_bytes" || transform.name == "align_kbytes") {
        error_ = "alignment transforms are not supported on executions: " + exec.fullPath;
        return false;
      } else if (isStructTransformName(transform.name)) {
        error_ = "struct transforms are not allowed on executions: " + exec.fullPath;
        return false;
      }
    }
    if (!validateCapabilitiesSubset(exec.transforms, exec.fullPath)) {
      return false;
    }
    Expr execTarget;
    if (!exec.name.empty()) {
      execTarget.name = exec.name;
      execTarget.namespacePrefix = exec.namespacePrefix;
    } else {
      execTarget.name = exec.fullPath;
    }
    const std::string resolvedPath = resolveCalleePath(execTarget);
    auto it = defMap_.find(resolvedPath);
    if (it == defMap_.end()) {
      error_ = "unknown execution target: " + resolvedPath;
      return false;
    }
    const std::unordered_set<std::string> targetEffects =
        resolveEffects(it->second->transforms, it->second->fullPath == entryPath_);
    for (const auto &effect : activeEffects_) {
      if (targetEffects.count(effect) == 0) {
        error_ = "execution effects must be a subset of enclosing effects on " + resolvedPath + ": " + effect;
        return false;
      }
    }
    if (!validateNamedArguments(exec.arguments, exec.argumentNames, resolvedPath, error_)) {
      return false;
    }
    const auto &execParams = paramsByDef_[resolvedPath];
    if (!validateNamedArgumentsAgainstParams(execParams, exec.argumentNames, error_)) {
      return false;
    }
    std::vector<const Expr *> orderedExecArgs;
    std::string orderError;
    if (!buildOrderedArguments(execParams, exec.arguments, exec.argumentNames, orderedExecArgs, orderError)) {
      if (orderError.find("argument count mismatch") != std::string::npos) {
        error_ = "argument count mismatch for " + resolvedPath;
      } else {
        error_ = orderError;
      }
      return false;
    }
    for (const auto *arg : orderedExecArgs) {
      if (!arg) {
        continue;
      }
      if (!validateExpr({}, std::unordered_map<std::string, BindingInfo>{}, *arg)) {
        return false;
      }
    }
    std::unordered_map<std::string, BindingInfo> execLocals;
    for (size_t bodyIndex = 0; bodyIndex < exec.bodyArguments.size(); ++bodyIndex) {
      const Expr &arg = exec.bodyArguments[bodyIndex];
      if (!validateStatement({},
                             execLocals,
                             arg,
                             ReturnKind::Unknown,
                             false,
                             true,
                             nullptr,
                             exec.namespacePrefix,
                             &exec.bodyArguments,
                             bodyIndex)) {
        return false;
      }
      expireReferenceBorrowsForRemainder({}, execLocals, exec.bodyArguments, bodyIndex + 1);
    }
    return true;
  };

  for (const auto &exec : program_.executions) {
    resetCollectedState();
    if (collectDiagnostics) {
      ValidationContextScope validationContextScope(*this, buildExecutionValidationContext(exec));
      std::vector<SemanticDiagnosticRecord> intraExecutionRecords;
      collectExecutionIntraBodyCallDiagnostics(exec, intraExecutionRecords);
      if (!intraExecutionRecords.empty()) {
        if (error_.empty()) {
          error_ = intraExecutionRecords.front().message;
        }
        collectedRecords.insert(collectedRecords.end(),
                                std::make_move_iterator(intraExecutionRecords.begin()),
                                std::make_move_iterator(intraExecutionRecords.end()));
        continue;
      }
    }
    if (!validateExecution(exec)) {
      if (!collectDiagnostics) {
        return false;
      }
      pushCollectedRecord();
    }
  }

  if (collectDiagnostics && !collectedRecords.empty()) {
    diagnosticInfo_->records = std::move(collectedRecords);
    if (!diagnosticInfo_->records.empty()) {
      diagnosticInfo_->line = diagnosticInfo_->records.front().line;
      diagnosticInfo_->column = diagnosticInfo_->records.front().column;
      diagnosticInfo_->relatedSpans = diagnosticInfo_->records.front().relatedSpans;
      error_ = diagnosticInfo_->records.front().message;
    }
    return false;
  }

  return true;
}

bool SemanticsValidator::validateEntry() {
  auto entryIt = defMap_.find(entryPath_);
  if (entryIt == defMap_.end()) {
    error_ = "missing entry definition " + entryPath_;
    return false;
  }
  const auto &entryParams = paramsByDef_[entryPath_];
  if (!entryParams.empty()) {
    if (entryParams.size() != 1) {
      error_ = "entry definition must take a single array<string> parameter: " + entryPath_;
      return false;
    }
    const ParameterInfo &param = entryParams.front();
    if (param.binding.typeName != "array" || param.binding.typeTemplateArg != "string") {
      error_ = "entry definition must take a single array<string> parameter: " + entryPath_;
      return false;
    }
    if (param.defaultExpr != nullptr) {
      error_ = "entry parameter does not allow a default value: " + entryPath_;
      return false;
    }
  }
  return true;
}

bool SemanticsValidator::validateStructLayouts() {
  struct LayoutInfo {
    uint32_t sizeBytes = 0;
    uint32_t alignmentBytes = 1;
  };
  auto isStructDefinition = [&](const Definition &def) {
    bool hasStruct = false;
    bool hasReturn = false;
    for (const auto &transform : def.transforms) {
      if (transform.name == "return") {
        hasReturn = true;
      }
      if (isStructTransformName(transform.name)) {
        hasStruct = true;
      }
    }
    if (hasStruct) {
      return true;
    }
    if (hasReturn || !def.parameters.empty() || def.hasReturnStatement || def.returnExpr.has_value()) {
      return false;
    }
    for (const auto &stmt : def.statements) {
      if (!stmt.isBinding) {
        return false;
      }
    }
    return true;
  };
  auto alignTo = [](uint32_t value, uint32_t alignment) -> uint32_t {
    if (alignment == 0) {
      return value;
    }
    uint32_t remainder = value % alignment;
    if (remainder == 0) {
      return value;
    }
    return value + (alignment - remainder);
  };
  auto extractAlignment = [&](const std::vector<Transform> &transforms,
                              const std::string &context,
                              uint32_t &alignmentOut,
                              bool &hasAlignment) -> bool {
    alignmentOut = 1;
    hasAlignment = false;
    for (const auto &transform : transforms) {
      if (transform.name != "align_bytes" && transform.name != "align_kbytes") {
        continue;
      }
      if (hasAlignment) {
        error_ = "duplicate " + transform.name + " transform on " + context;
        return false;
      }
      if (!validateAlignTransform(transform, context, error_)) {
        return false;
      }
      auto parsePositiveInt = [](const std::string &text, int &valueOut) -> bool {
        std::string digits = text;
        if (digits.size() > 3 && digits.compare(digits.size() - 3, 3, "i32") == 0) {
          digits.resize(digits.size() - 3);
        }
        if (digits.empty()) {
          return false;
        }
        int parsed = 0;
        for (char c : digits) {
          if (!std::isdigit(static_cast<unsigned char>(c))) {
            return false;
          }
          int digit = c - '0';
          if (parsed > (std::numeric_limits<int>::max() - digit) / 10) {
            return false;
          }
          parsed = parsed * 10 + digit;
        }
        if (parsed <= 0) {
          return false;
        }
        valueOut = parsed;
        return true;
      };
      int value = 0;
      if (!parsePositiveInt(transform.arguments[0], value)) {
        error_ = transform.name + " requires a positive integer argument";
        return false;
      }
      uint64_t bytes = static_cast<uint64_t>(value);
      if (transform.name == "align_kbytes") {
        bytes *= 1024ull;
      }
      if (bytes > std::numeric_limits<uint32_t>::max()) {
        error_ = transform.name + " alignment too large on " + context;
        return false;
      }
      alignmentOut = static_cast<uint32_t>(bytes);
      hasAlignment = true;
    }
    return true;
  };
  auto resolveStructTypePath = [&](const std::string &typeName, const std::string &namespacePrefix) -> std::string {
    if (!typeName.empty() && typeName[0] == '/') {
      return typeName;
    }
    std::string resolved = resolveTypePath(typeName, namespacePrefix);
    std::string current = namespacePrefix;
    while (true) {
      if (!current.empty()) {
        std::string direct = current + "/" + typeName;
        if (structNames_.count(direct) > 0) {
          return direct;
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
    return resolved;
  };

  std::unordered_map<std::string, LayoutInfo> layoutCache;
  std::unordered_set<std::string> layoutStack;

  std::function<bool(const Definition &, LayoutInfo &)> computeStructLayout;
  std::function<bool(const BindingInfo &, const std::string &, LayoutInfo &)> typeLayoutForBinding;

  computeStructLayout = [&](const Definition &def, LayoutInfo &out) -> bool {
    auto isStaticField = [&](const Expr &stmt) -> bool {
      for (const auto &transform : stmt.transforms) {
        if (transform.name == "static") {
          return true;
        }
      }
      return false;
    };
    auto cached = layoutCache.find(def.fullPath);
    if (cached != layoutCache.end()) {
      out = cached->second;
      return true;
    }
    if (!layoutStack.insert(def.fullPath).second) {
      error_ = "recursive struct layout not supported: " + def.fullPath;
      return false;
    }
    bool requireNoPadding = false;
    bool requirePlatformPadding = false;
    for (const auto &transform : def.transforms) {
      if (transform.name == "no_padding") {
        requireNoPadding = true;
      } else if (transform.name == "platform_independent_padding") {
        requirePlatformPadding = true;
      }
    }
    uint32_t structAlign = 1;
    uint32_t explicitStructAlign = 1;
    bool hasStructAlign = false;
    if (!extractAlignment(def.transforms, "struct " + def.fullPath, explicitStructAlign, hasStructAlign)) {
      return false;
    }
    uint32_t offset = 0;
    for (const auto &stmt : def.statements) {
      if (!stmt.isBinding) {
        continue;
      }
      const bool fieldIsStatic = isStaticField(stmt);
      if (fieldIsStatic) {
        continue;
      }
      BindingInfo binding;
      if (!resolveStructFieldBinding(def, stmt, binding)) {
        return false;
      }
      LayoutInfo fieldLayout;
      if (!typeLayoutForBinding(binding, def.namespacePrefix, fieldLayout)) {
        return false;
      }
      uint32_t explicitFieldAlign = 1;
      bool hasFieldAlign = false;
      const std::string fieldContext = "field " + def.fullPath + "/" + stmt.name;
      if (!extractAlignment(stmt.transforms, fieldContext, explicitFieldAlign, hasFieldAlign)) {
        return false;
      }
      if (hasFieldAlign && explicitFieldAlign < fieldLayout.alignmentBytes) {
        error_ = "alignment requirement on " + fieldContext + " is smaller than required alignment of " +
                 std::to_string(fieldLayout.alignmentBytes);
        return false;
      }
      uint32_t fieldAlign = hasFieldAlign ? std::max(explicitFieldAlign, fieldLayout.alignmentBytes)
                                          : fieldLayout.alignmentBytes;
      uint32_t *activeOffset = &offset;
      uint32_t alignedOffset = alignTo(*activeOffset, fieldAlign);
      if (requireNoPadding && alignedOffset != *activeOffset) {
        error_ = "no_padding disallows alignment padding on " + fieldContext;
        return false;
      }
      if (requirePlatformPadding && alignedOffset != *activeOffset && !hasFieldAlign) {
        error_ = "platform_independent_padding requires explicit alignment on " + fieldContext;
        return false;
      }
      *activeOffset = alignedOffset + fieldLayout.sizeBytes;
      if (!fieldIsStatic) {
        structAlign = std::max(structAlign, fieldAlign);
      }
    }
    if (hasStructAlign && explicitStructAlign < structAlign) {
      error_ = "alignment requirement on struct " + def.fullPath + " is smaller than required alignment of " +
               std::to_string(structAlign);
      return false;
    }
    structAlign = hasStructAlign ? std::max(structAlign, explicitStructAlign) : structAlign;
    uint32_t totalSize = alignTo(offset, structAlign);
    if (requireNoPadding && totalSize != offset) {
      error_ = "no_padding disallows trailing padding on struct " + def.fullPath;
      return false;
    }
    if (requirePlatformPadding && totalSize != offset && !hasStructAlign) {
      error_ = "platform_independent_padding requires explicit struct alignment on " + def.fullPath;
      return false;
    }
    LayoutInfo layout{totalSize, structAlign};
    layoutCache.emplace(def.fullPath, layout);
    layoutStack.erase(def.fullPath);
    out = layout;
    return true;
  };

  typeLayoutForBinding = [&](const BindingInfo &binding,
                             const std::string &namespacePrefix,
                             LayoutInfo &layoutOut) -> bool {
    std::string normalized = normalizeBindingTypeName(binding.typeName);
    if (normalized == "i32" || normalized == "int" || normalized == "f32") {
      layoutOut = {4u, 4u};
      return true;
    }
    if (normalized == "i64" || normalized == "u64" || normalized == "f64") {
      layoutOut = {8u, 8u};
      return true;
    }
    if (normalized == "bool") {
      layoutOut = {1u, 1u};
      return true;
    }
    if (normalized == "string") {
      layoutOut = {8u, 8u};
      return true;
    }
    if (binding.typeName == "Pointer" || binding.typeName == "Reference") {
      layoutOut = {8u, 8u};
      return true;
    }
    if (binding.typeName == "array" || binding.typeName == "vector" || binding.typeName == "map") {
      layoutOut = {8u, 8u};
      return true;
    }
    if (binding.typeName == "uninitialized") {
      if (binding.typeTemplateArg.empty()) {
        error_ = "uninitialized requires exactly one template argument";
        return false;
      }
      std::string base;
      std::string arg;
      BindingInfo inner;
      if (splitTemplateTypeName(binding.typeTemplateArg, base, arg)) {
        if (base == "uninitialized") {
          error_ = "nested uninitialized storage is not supported";
          return false;
        }
        inner.typeName = base;
        inner.typeTemplateArg = arg;
      } else {
        inner.typeName = binding.typeTemplateArg;
      }
      return typeLayoutForBinding(inner, namespacePrefix, layoutOut);
    }
    std::string structPath = resolveStructTypePath(binding.typeName, namespacePrefix);
    auto defIt = defMap_.find(structPath);
    if (defIt == defMap_.end()) {
      error_ = "unknown struct type for layout: " + binding.typeName;
      return false;
    }
    LayoutInfo nested;
    if (!computeStructLayout(*defIt->second, nested)) {
      return false;
    }
    layoutOut = nested;
    return true;
  };

  for (const auto &def : program_.definitions) {
    DefinitionContextScope definitionScope(*this, def);
    if (!isStructDefinition(def)) {
      continue;
    }
    LayoutInfo layout;
    if (!computeStructLayout(def, layout)) {
      return false;
    }
  }
  return true;
}

bool SemanticsValidator::validateOmittedBindingInitializer(const Expr &binding,
                                                           const BindingInfo &info,
                                                           const std::string &namespacePrefix) {
  if (!hasExplicitBindingTypeTransform(binding)) {
    error_ = "omitted initializer requires explicit struct type: " + binding.name;
    return false;
  }
  const std::string normalizedType = normalizeBindingTypeName(info.typeName);
  if (normalizedType == "vector") {
    std::vector<std::string> args;
    if (!splitTopLevelTemplateArgs(info.typeTemplateArg, args) || args.size() != 1) {
      error_ = "vector requires exactly one template argument";
      return false;
    }
    return true;
  }
  if (!info.typeTemplateArg.empty()) {
    error_ = "omitted initializer requires struct type: " + info.typeName;
    return false;
  }
  std::string structPath = resolveStructTypePath(info.typeName, namespacePrefix, structNames_);
  if (structPath.empty()) {
    auto importIt = importAliases_.find(info.typeName);
    if (importIt != importAliases_.end() && structNames_.count(importIt->second) > 0) {
      structPath = importIt->second;
    }
  }
  if (structPath.empty()) {
    error_ = "omitted initializer requires struct type: " + info.typeName;
    return false;
  }
  if (!hasStructZeroArgConstructor(structPath)) {
    error_ = "omitted initializer requires zero-arg constructor: " + structPath;
    return false;
  }
  if (!isOutsideEffectFreeStructConstructor(structPath)) {
    error_ = "omitted initializer requires effect-free zero-arg constructor: " + structPath;
    return false;
  }
  return true;
}

bool SemanticsValidator::hasStructZeroArgConstructor(const std::string &structPath) const {
  auto defIt = defMap_.find(structPath);
  if (defIt == defMap_.end() || !defIt->second) {
    return false;
  }
  auto isStaticField = [](const Expr &stmt) -> bool {
    for (const auto &transform : stmt.transforms) {
      if (transform.name == "static") {
        return true;
      }
    }
    return false;
  };
  const Definition &def = *defIt->second;
  std::unordered_set<std::string> missingFields;
  for (const auto &stmt : def.statements) {
    if (!stmt.isBinding || isStaticField(stmt)) {
      continue;
    }
    if (stmt.args.empty()) {
      missingFields.insert(stmt.name);
    }
  }
  if (missingFields.empty()) {
    return true;
  }
  const std::string createPath = structPath + "/Create";
  auto createIt = defMap_.find(createPath);
  if (createIt == defMap_.end() || !createIt->second) {
    return false;
  }
  const Definition &createDef = *createIt->second;
  std::string thisName = "this";
  auto paramsIt = paramsByDef_.find(createDef.fullPath);
  if (paramsIt != paramsByDef_.end() && !paramsIt->second.empty() && paramsIt->second.front().name == "this") {
    thisName = paramsIt->second.front().name;
  }

  struct FieldInitFlow {
    bool ok = true;
    bool fallsThrough = true;
    std::unordered_set<std::string> assigned;
  };

  auto allAssigned = [&](const std::unordered_set<std::string> &assigned) -> bool {
    for (const auto &field : missingFields) {
      if (assigned.count(field) == 0) {
        return false;
      }
    }
    return true;
  };
  auto intersectAssigned = [&](const std::unordered_set<std::string> &left,
                               const std::unordered_set<std::string> &right) -> std::unordered_set<std::string> {
    std::unordered_set<std::string> result;
    if (left.size() < right.size()) {
      for (const auto &field : left) {
        if (right.count(field) > 0) {
          result.insert(field);
        }
      }
    } else {
      for (const auto &field : right) {
        if (left.count(field) > 0) {
          result.insert(field);
        }
      }
    }
    return result;
  };
  auto isThisFieldTarget = [&](const Expr &target) -> bool {
    if (thisName.empty()) {
      return false;
    }
    if (!target.isFieldAccess || target.args.size() != 1) {
      return false;
    }
    const Expr &receiver = target.args.front();
    return receiver.kind == Expr::Kind::Name && receiver.name == thisName;
  };

  std::function<FieldInitFlow(const std::vector<Expr> &, const std::unordered_set<std::string> &)>
      analyzeStatements;
  std::function<FieldInitFlow(const Expr &, const std::unordered_set<std::string> &)> analyzeStatement;
  std::function<FieldInitFlow(const Expr &, const std::unordered_set<std::string> &)> analyzeBlockExpr;

  analyzeStatements = [&](const std::vector<Expr> &statements,
                          const std::unordered_set<std::string> &assignedIn) -> FieldInitFlow {
    std::unordered_set<std::string> current = assignedIn;
    bool fallsThrough = true;
    for (const auto &stmt : statements) {
      if (!fallsThrough) {
        break;
      }
      FieldInitFlow next = analyzeStatement(stmt, current);
      if (!next.ok) {
        return next;
      }
      fallsThrough = next.fallsThrough;
      current = std::move(next.assigned);
    }
    FieldInitFlow result;
    result.ok = true;
    result.fallsThrough = fallsThrough;
    result.assigned = std::move(current);
    return result;
  };

  analyzeBlockExpr = [&](const Expr &expr,
                         const std::unordered_set<std::string> &assignedIn) -> FieldInitFlow {
    if (expr.kind == Expr::Kind::Call && expr.hasBodyArguments) {
      return analyzeStatements(expr.bodyArguments, assignedIn);
    }
    return analyzeStatement(expr, assignedIn);
  };

  analyzeStatement = [&](const Expr &stmt,
                         const std::unordered_set<std::string> &assignedIn) -> FieldInitFlow {
    if (isReturnCall(stmt)) {
      if (!allAssigned(assignedIn)) {
        return FieldInitFlow{false, false, assignedIn};
      }
      return FieldInitFlow{true, false, assignedIn};
    }
    if (isMatchCall(stmt)) {
      Expr expanded;
      std::string error;
      if (!lowerMatchToIf(stmt, expanded, error)) {
        return FieldInitFlow{false, false, assignedIn};
      }
      return analyzeStatement(expanded, assignedIn);
    }
    if (isIfCall(stmt) && stmt.args.size() == 3) {
      FieldInitFlow thenFlow = analyzeBlockExpr(stmt.args[1], assignedIn);
      if (!thenFlow.ok) {
        return thenFlow;
      }
      FieldInitFlow elseFlow = analyzeBlockExpr(stmt.args[2], assignedIn);
      if (!elseFlow.ok) {
        return elseFlow;
      }
      FieldInitFlow result;
      result.ok = true;
      result.fallsThrough = thenFlow.fallsThrough || elseFlow.fallsThrough;
      if (thenFlow.fallsThrough && elseFlow.fallsThrough) {
        result.assigned = intersectAssigned(thenFlow.assigned, elseFlow.assigned);
      } else if (thenFlow.fallsThrough) {
        result.assigned = std::move(thenFlow.assigned);
      } else if (elseFlow.fallsThrough) {
        result.assigned = std::move(elseFlow.assigned);
      } else {
        result.assigned = assignedIn;
      }
      return result;
    }
    if (isLoopCall(stmt) || isWhileCall(stmt) || isForCall(stmt) || isRepeatCall(stmt)) {
      if (!stmt.args.empty()) {
        const Expr &body = stmt.args.back();
        if (body.kind == Expr::Kind::Call && body.hasBodyArguments) {
          FieldInitFlow bodyFlow = analyzeStatements(body.bodyArguments, assignedIn);
          if (!bodyFlow.ok) {
            return bodyFlow;
          }
        }
      }
      FieldInitFlow result;
      result.ok = true;
      result.fallsThrough = true;
      result.assigned = assignedIn;
      return result;
    }
    if (isBuiltinBlockCall(stmt) && stmt.hasBodyArguments) {
      return analyzeStatements(stmt.bodyArguments, assignedIn);
    }
    std::unordered_set<std::string> assignedOut = assignedIn;
    if (isAssignCall(stmt) && stmt.args.size() == 2) {
      const Expr &target = stmt.args.front();
      if (isThisFieldTarget(target) && missingFields.count(target.name) > 0) {
        assignedOut.insert(target.name);
      }
    }
    FieldInitFlow result;
    result.ok = true;
    result.fallsThrough = true;
    result.assigned = std::move(assignedOut);
    return result;
  };

  FieldInitFlow flow = analyzeStatements(createDef.statements, {});
  if (!flow.ok) {
    return false;
  }
  if (flow.fallsThrough && !allAssigned(flow.assigned)) {
    return false;
  }
  return true;
}

SemanticsValidator::EffectFreeSummary SemanticsValidator::effectFreeSummaryForDefinition(const Definition &def) {
  auto cached = effectFreeDefCache_.find(def.fullPath);
  if (cached != effectFreeDefCache_.end()) {
    return cached->second;
  }
  if (!effectFreeDefStack_.insert(def.fullPath).second) {
    return {};
  }

  EffectFreeSummary summary;
  bool hasCapabilities = false;
  for (const auto &transform : def.transforms) {
    if (transform.name == "capabilities" && !transform.arguments.empty()) {
      hasCapabilities = true;
      break;
    }
  }
  if (hasCapabilities) {
    effectFreeDefCache_.emplace(def.fullPath, summary);
    effectFreeDefStack_.erase(def.fullPath);
    return summary;
  }
  const std::unordered_set<std::string> effects = resolveEffects(def.transforms, def.fullPath == entryPath_);
  if (!effects.empty()) {
    effectFreeDefCache_.emplace(def.fullPath, summary);
    effectFreeDefStack_.erase(def.fullPath);
    return summary;
  }

  EffectFreeContext ctx;
  const auto &params = paramsByDef_[def.fullPath];
  ctx.params = &params;
  ctx.locals.reserve(params.size());
  ctx.paramNames.reserve(params.size());
  for (const auto &param : params) {
    ctx.locals.emplace(param.name, param.binding);
    ctx.paramNames.insert(param.name);
  }
  if (!params.empty() && params.front().name == "this") {
    ctx.thisName = params.front().name;
  }

  bool writesThis = false;
  for (const auto &stmt : def.statements) {
    if (!isOutsideEffectFreeStatement(stmt, ctx, writesThis)) {
      effectFreeDefCache_.emplace(def.fullPath, summary);
      effectFreeDefStack_.erase(def.fullPath);
      return summary;
    }
  }
  if (def.returnExpr.has_value()) {
    if (!isOutsideEffectFreeExpr(*def.returnExpr, ctx, writesThis)) {
      effectFreeDefCache_.emplace(def.fullPath, summary);
      effectFreeDefStack_.erase(def.fullPath);
      return summary;
    }
  }

  summary.effectFree = true;
  summary.writesThis = writesThis;
  effectFreeDefCache_.emplace(def.fullPath, summary);
  effectFreeDefStack_.erase(def.fullPath);
  return summary;
}

bool SemanticsValidator::isOutsideEffectFreeStructConstructor(const std::string &structPath) {
  auto cached = effectFreeStructCache_.find(structPath);
  if (cached != effectFreeStructCache_.end()) {
    return cached->second;
  }
  if (!effectFreeStructStack_.insert(structPath).second) {
    return false;
  }
  if (!hasStructZeroArgConstructor(structPath)) {
    effectFreeStructCache_.emplace(structPath, false);
    effectFreeStructStack_.erase(structPath);
    return false;
  }
  auto defIt = defMap_.find(structPath);
  if (defIt == defMap_.end() || !defIt->second) {
    effectFreeStructCache_.emplace(structPath, false);
    effectFreeStructStack_.erase(structPath);
    return false;
  }

  auto isStaticField = [](const Expr &stmt) -> bool {
    for (const auto &transform : stmt.transforms) {
      if (transform.name == "static") {
        return true;
      }
    }
    return false;
  };

  EffectFreeContext ctx;
  const std::vector<ParameterInfo> emptyParams;
  ctx.params = &emptyParams;
  bool writesThis = false;
  for (const auto &field : defIt->second->statements) {
    if (!field.isBinding || isStaticField(field)) {
      continue;
    }
    if (field.args.empty()) {
      continue;
    }
    if (!isOutsideEffectFreeExpr(field.args.front(), ctx, writesThis)) {
      effectFreeStructCache_.emplace(structPath, false);
      effectFreeStructStack_.erase(structPath);
      return false;
    }
  }

  const std::string createPath = structPath + "/Create";
  auto createIt = defMap_.find(createPath);
  if (createIt != defMap_.end() && createIt->second) {
    const EffectFreeSummary summary = effectFreeSummaryForDefinition(*createIt->second);
    if (!summary.effectFree) {
      effectFreeStructCache_.emplace(structPath, false);
      effectFreeStructStack_.erase(structPath);
      return false;
    }
  }

  effectFreeStructCache_.emplace(structPath, true);
  effectFreeStructStack_.erase(structPath);
  return true;
}

bool SemanticsValidator::isOutsideEffectFreeStatement(const Expr &stmt,
                                                      EffectFreeContext &ctx,
                                                      bool &writesThis) {
  if (stmt.isBinding) {
    if (ctx.locals.count(stmt.name) > 0) {
      return false;
    }
    BindingInfo info;
    std::optional<std::string> restrictType;
    std::string parseError;
    if (!parseBindingInfo(stmt, stmt.namespacePrefix, structNames_, importAliases_, info, restrictType, parseError)) {
      return false;
    }
    if (!stmt.args.empty()) {
      if (!isOutsideEffectFreeExpr(stmt.args.front(), ctx, writesThis)) {
        return false;
      }
    } else {
      if (!hasExplicitBindingTypeTransform(stmt)) {
        return false;
      }
      if (!info.typeTemplateArg.empty()) {
        return false;
      }
      std::string structPath = resolveStructTypePath(info.typeName, stmt.namespacePrefix, structNames_);
      if (structPath.empty()) {
        auto importIt = importAliases_.find(info.typeName);
        if (importIt != importAliases_.end() && structNames_.count(importIt->second) > 0) {
          structPath = importIt->second;
        }
      }
      if (structPath.empty()) {
        return false;
      }
      if (!hasStructZeroArgConstructor(structPath)) {
        return false;
      }
      if (!isOutsideEffectFreeStructConstructor(structPath)) {
        return false;
      }
    }
    ctx.locals.emplace(stmt.name, std::move(info));
    return true;
  }

  if (stmt.kind == Expr::Kind::Call) {
    if (isReturnCall(stmt)) {
      if (!stmt.args.empty() && !isOutsideEffectFreeExpr(stmt.args.front(), ctx, writesThis)) {
        return false;
      }
      return true;
    }
    if (isMatchCall(stmt)) {
      Expr expanded;
      std::string error;
      if (!lowerMatchToIf(stmt, expanded, error)) {
        return false;
      }
      return isOutsideEffectFreeStatement(expanded, ctx, writesThis);
    }
    if (isIfCall(stmt) && stmt.args.size() == 3) {
      if (!isOutsideEffectFreeExpr(stmt.args[0], ctx, writesThis)) {
        return false;
      }
      auto checkBranch = [&](const Expr &branch) -> bool {
        EffectFreeContext branchCtx = ctx;
        if (branch.kind == Expr::Kind::Call && isBuiltinBlockCall(branch) &&
            (branch.hasBodyArguments || !branch.bodyArguments.empty())) {
          for (const auto &bodyExpr : branch.bodyArguments) {
            if (!isOutsideEffectFreeStatement(bodyExpr, branchCtx, writesThis)) {
              return false;
            }
          }
          return true;
        }
        return isOutsideEffectFreeExpr(branch, branchCtx, writesThis);
      };
      if (!checkBranch(stmt.args[1]) || !checkBranch(stmt.args[2])) {
        return false;
      }
      return true;
    }
    if (isLoopCall(stmt) && stmt.args.size() == 2) {
      if (!isOutsideEffectFreeExpr(stmt.args[0], ctx, writesThis)) {
        return false;
      }
      EffectFreeContext bodyCtx = ctx;
      const Expr &body = stmt.args[1];
      if (body.kind == Expr::Kind::Call && isBuiltinBlockCall(body) &&
          (body.hasBodyArguments || !body.bodyArguments.empty())) {
        for (const auto &bodyStmt : body.bodyArguments) {
          if (!isOutsideEffectFreeStatement(bodyStmt, bodyCtx, writesThis)) {
            return false;
          }
        }
        return true;
      }
      return isOutsideEffectFreeExpr(body, bodyCtx, writesThis);
    }
    if (isWhileCall(stmt) && stmt.args.size() == 2) {
      if (!isOutsideEffectFreeExpr(stmt.args[0], ctx, writesThis)) {
        return false;
      }
      EffectFreeContext bodyCtx = ctx;
      const Expr &body = stmt.args[1];
      if (body.kind == Expr::Kind::Call && isBuiltinBlockCall(body) &&
          (body.hasBodyArguments || !body.bodyArguments.empty())) {
        for (const auto &bodyStmt : body.bodyArguments) {
          if (!isOutsideEffectFreeStatement(bodyStmt, bodyCtx, writesThis)) {
            return false;
          }
        }
        return true;
      }
      return isOutsideEffectFreeExpr(body, bodyCtx, writesThis);
    }
    if (isForCall(stmt) && stmt.args.size() == 4) {
      EffectFreeContext loopCtx = ctx;
      if (!isOutsideEffectFreeStatement(stmt.args[0], loopCtx, writesThis)) {
        return false;
      }
      if (!isOutsideEffectFreeExpr(stmt.args[1], loopCtx, writesThis)) {
        return false;
      }
      if (!isOutsideEffectFreeStatement(stmt.args[2], loopCtx, writesThis)) {
        return false;
      }
      EffectFreeContext bodyCtx = loopCtx;
      const Expr &body = stmt.args[3];
      if (body.kind == Expr::Kind::Call && isBuiltinBlockCall(body) &&
          (body.hasBodyArguments || !body.bodyArguments.empty())) {
        for (const auto &bodyStmt : body.bodyArguments) {
          if (!isOutsideEffectFreeStatement(bodyStmt, bodyCtx, writesThis)) {
            return false;
          }
        }
        return true;
      }
      return isOutsideEffectFreeExpr(body, bodyCtx, writesThis);
    }
  }

  return isOutsideEffectFreeExpr(stmt, ctx, writesThis);
}

bool SemanticsValidator::isOutsideEffectFreeExpr(const Expr &expr, EffectFreeContext &ctx, bool &writesThis) {
  if (expr.kind == Expr::Kind::Literal || expr.kind == Expr::Kind::BoolLiteral || expr.kind == Expr::Kind::FloatLiteral ||
      expr.kind == Expr::Kind::StringLiteral) {
    return true;
  }
  if (expr.kind == Expr::Kind::Name) {
    return true;
  }
  if (expr.kind != Expr::Kind::Call) {
    return false;
  }
  if (expr.isBinding) {
    return false;
  }

  for (const auto &transform : expr.transforms) {
    if (transform.name == "capabilities" && !transform.arguments.empty()) {
      return false;
    }
    if (transform.name == "effects" && !transform.arguments.empty()) {
      return false;
    }
  }

  if (expr.isFieldAccess) {
    if (expr.args.size() != 1) {
      return false;
    }
    return isOutsideEffectFreeExpr(expr.args.front(), ctx, writesThis);
  }

  if (expr.isMethodCall) {
    if (expr.args.empty()) {
      return false;
    }
    const Expr &receiver = expr.args.front();
    if (!isOutsideEffectFreeExpr(receiver, ctx, writesThis)) {
      return false;
    }
    std::string receiverStruct;
    if (receiver.kind == Expr::Kind::Name) {
      auto it = ctx.locals.find(receiver.name);
      if (it != ctx.locals.end()) {
        if (it->second.typeName == "Reference" || it->second.typeName == "Pointer") {
          receiverStruct = resolveStructTypePath(it->second.typeTemplateArg, receiver.namespacePrefix, structNames_);
          if (receiverStruct.empty()) {
            auto importIt = importAliases_.find(it->second.typeTemplateArg);
            if (importIt != importAliases_.end() && structNames_.count(importIt->second) > 0) {
              receiverStruct = importIt->second;
            }
          }
        } else {
          receiverStruct = resolveStructTypePath(it->second.typeName, receiver.namespacePrefix, structNames_);
          if (receiverStruct.empty()) {
            auto importIt = importAliases_.find(it->second.typeName);
            if (importIt != importAliases_.end() && structNames_.count(importIt->second) > 0) {
              receiverStruct = importIt->second;
            }
          }
        }
      }
    }
    if (receiverStruct.empty()) {
      return false;
    }
    const std::string methodPath = receiverStruct + "/" + expr.name;
    auto defIt = defMap_.find(methodPath);
    if (defIt == defMap_.end() || !defIt->second) {
      return false;
    }
    const EffectFreeSummary summary = effectFreeSummaryForDefinition(*defIt->second);
    if (!summary.effectFree) {
      return false;
    }
    if (summary.writesThis) {
      if (receiver.kind != Expr::Kind::Name) {
        return false;
      }
      auto bindingIt = ctx.locals.find(receiver.name);
      if (bindingIt == ctx.locals.end()) {
        return false;
      }
      if (bindingIt->second.typeName == "Reference" || bindingIt->second.typeName == "Pointer") {
        if (receiver.name != ctx.thisName) {
          return false;
        }
      }
      if (!bindingIt->second.isMutable) {
        return false;
      }
      if (ctx.paramNames.count(receiver.name) > 0 && receiver.name != ctx.thisName) {
        return false;
      }
    }
    for (size_t i = 1; i < expr.args.size(); ++i) {
      if (!isOutsideEffectFreeExpr(expr.args[i], ctx, writesThis)) {
        return false;
      }
    }
    return true;
  }

  if (isReturnCall(expr)) {
    if (!expr.args.empty()) {
      return isOutsideEffectFreeExpr(expr.args.front(), ctx, writesThis);
    }
    return true;
  }

  if (isMatchCall(expr)) {
    Expr expanded;
    std::string error;
    if (!lowerMatchToIf(expr, expanded, error)) {
      return false;
    }
    return isOutsideEffectFreeExpr(expanded, ctx, writesThis);
  }
  if (isIfCall(expr) && expr.args.size() == 3) {
    if (!isOutsideEffectFreeExpr(expr.args[0], ctx, writesThis)) {
      return false;
    }
    auto checkBranch = [&](const Expr &branch) -> bool {
      EffectFreeContext branchCtx = ctx;
      if (branch.kind == Expr::Kind::Call && isBuiltinBlockCall(branch) &&
          (branch.hasBodyArguments || !branch.bodyArguments.empty())) {
        for (const auto &bodyExpr : branch.bodyArguments) {
          if (!isOutsideEffectFreeStatement(bodyExpr, branchCtx, writesThis)) {
            return false;
          }
        }
        return true;
      }
      return isOutsideEffectFreeExpr(branch, branchCtx, writesThis);
    };
    return checkBranch(expr.args[1]) && checkBranch(expr.args[2]);
  }

  if (expr.hasBodyArguments || !expr.bodyArguments.empty()) {
    EffectFreeContext blockCtx = ctx;
    for (const auto &bodyExpr : expr.bodyArguments) {
      if (!isOutsideEffectFreeStatement(bodyExpr, blockCtx, writesThis)) {
        return false;
      }
    }
    if (isBuiltinBlockCall(expr) && expr.args.empty() && expr.templateArgs.empty() &&
        !hasNamedArguments(expr.argNames)) {
      return true;
    }
  }

  if (isAssignCall(expr) && expr.args.size() == 2) {
    const Expr &target = expr.args.front();
    if (target.kind == Expr::Kind::Name) {
      auto bindingIt = ctx.locals.find(target.name);
      if (bindingIt == ctx.locals.end()) {
        return false;
      }
      if (bindingIt->second.typeName == "Reference" || bindingIt->second.typeName == "Pointer") {
        if (target.name != ctx.thisName) {
          return false;
        }
      }
      if (!bindingIt->second.isMutable) {
        return false;
      }
      if (ctx.paramNames.count(target.name) > 0 && target.name != ctx.thisName) {
        return false;
      }
      if (target.name == ctx.thisName) {
        writesThis = true;
      }
      return isOutsideEffectFreeExpr(expr.args[1], ctx, writesThis);
    }
    auto isThisFieldTarget = [&](const Expr &candidate) -> bool {
      if (ctx.thisName.empty()) {
        return false;
      }
      const Expr *current = &candidate;
      while (current->isFieldAccess && current->args.size() == 1) {
        const Expr &receiver = current->args.front();
        if (receiver.kind == Expr::Kind::Name) {
          return receiver.name == ctx.thisName;
        }
        if (!receiver.isFieldAccess) {
          return false;
        }
        current = &receiver;
      }
      return false;
    };
    if (isThisFieldTarget(target)) {
      auto bindingIt = ctx.locals.find(ctx.thisName);
      if (bindingIt == ctx.locals.end()) {
        return false;
      }
      if (!bindingIt->second.isMutable) {
        return false;
      }
      writesThis = true;
      return isOutsideEffectFreeExpr(expr.args[1], ctx, writesThis);
    }
    return false;
  }

  std::string mutateName;
  if (getBuiltinMutationName(expr, mutateName) && expr.args.size() == 1) {
    const Expr &target = expr.args.front();
    if (target.kind != Expr::Kind::Name) {
      return false;
    }
    auto bindingIt = ctx.locals.find(target.name);
    if (bindingIt == ctx.locals.end()) {
      return false;
    }
    if (bindingIt->second.typeName == "Reference" || bindingIt->second.typeName == "Pointer") {
      if (target.name != ctx.thisName) {
        return false;
      }
    }
    if (!bindingIt->second.isMutable) {
      return false;
    }
    if (ctx.paramNames.count(target.name) > 0 && target.name != ctx.thisName) {
      return false;
    }
    if (target.name == ctx.thisName) {
      writesThis = true;
    }
    return true;
  }

  std::string builtinName;
  if (getBuiltinOperatorName(expr, builtinName) || getBuiltinComparisonName(expr, builtinName) ||
      getBuiltinClampName(expr, builtinName, true) || getBuiltinMinMaxName(expr, builtinName, true) ||
      getBuiltinAbsSignName(expr, builtinName, true) || getBuiltinSaturateName(expr, builtinName, true) ||
      getBuiltinMathName(expr, builtinName, true) || getBuiltinConvertName(expr, builtinName) ||
      getBuiltinArrayAccessName(expr, builtinName) || getBuiltinPointerName(expr, builtinName)) {
    for (const auto &arg : expr.args) {
      if (!isOutsideEffectFreeExpr(arg, ctx, writesThis)) {
        return false;
      }
    }
    return true;
  }
  if (getBuiltinCollectionName(expr, builtinName)) {
    if (builtinName != "array") {
      return false;
    }
    for (const auto &arg : expr.args) {
      if (!isOutsideEffectFreeExpr(arg, ctx, writesThis)) {
        return false;
      }
    }
    return true;
  }

  const std::string resolved = resolveCalleePath(expr);
  auto defIt = defMap_.find(resolved);
  if (defIt == defMap_.end() || !defIt->second) {
    return false;
  }
  if (structNames_.count(resolved) > 0) {
    if (!isOutsideEffectFreeStructConstructor(resolved)) {
      return false;
    }
  } else {
    const EffectFreeSummary summary = effectFreeSummaryForDefinition(*defIt->second);
    if (!summary.effectFree) {
      return false;
    }
  }
  for (const auto &arg : expr.args) {
    if (!isOutsideEffectFreeExpr(arg, ctx, writesThis)) {
      return false;
    }
  }
  return true;
}

} // namespace primec::semantics
