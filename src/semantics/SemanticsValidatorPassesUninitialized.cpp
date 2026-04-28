#include "SemanticsValidator.h"

#include <algorithm>
#include <functional>
#include <optional>
#include <vector>

namespace primec::semantics {

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

  std::function<bool(const Expr &, std::string &)> resolveLocationRootBindingName;
  resolveLocationRootBindingName = [&](const Expr &pointerExpr, std::string &rootNameOut) -> bool {
    rootNameOut.clear();
    if (pointerExpr.kind == Expr::Kind::Name) {
      auto resolveNamedRoot = [&](const BindingInfo &binding) -> bool {
        const std::string normalizedType = normalizeBindingTypeName(binding.typeName);
        if ((normalizedType == "Reference" || normalizedType == "Pointer") &&
            !binding.referenceRoot.empty()) {
          rootNameOut = binding.referenceRoot;
          return true;
        }
        return false;
      };
      for (const auto &param : params) {
        if (param.name == pointerExpr.name && resolveNamedRoot(param.binding)) {
          return true;
        }
      }
      for (const auto &stmt : statements) {
        if (!stmt.isBinding || stmt.name != pointerExpr.name || stmt.args.size() != 1) {
          continue;
        }
        BindingInfo info;
        std::optional<std::string> restrictType;
        std::string error;
        if (!parseBindingInfo(stmt, stmt.namespacePrefix, structNames_, importAliases_, info,
                              restrictType, error, &sumNames_)) {
          return false;
        }
        const std::string normalizedType = normalizeBindingTypeName(info.typeName);
        if (normalizedType != "Reference" && normalizedType != "Pointer") {
          return false;
        }
        return resolveLocationRootBindingName(stmt.args.front(), rootNameOut);
      }
      return false;
    }
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
    const std::string resolvedPath = resolveCalleePath(pointerExpr);
    if (resolvedPath.empty()) {
      return false;
    }
    auto defIt = defMap_.find(resolvedPath);
    if (defIt == defMap_.end() || defIt->second == nullptr) {
      return false;
    }
    const Definition &helperDef = *defIt->second;
    const Expr *returnedValueExpr = nullptr;
    for (const auto &stmt : helperDef.statements) {
      if (isReturnCall(stmt) && stmt.args.size() == 1) {
        returnedValueExpr = &stmt.args.front();
      }
    }
    if (helperDef.returnExpr.has_value()) {
      returnedValueExpr = &*helperDef.returnExpr;
    }
    if (returnedValueExpr == nullptr || returnedValueExpr->kind != Expr::Kind::Name) {
      return false;
    }
    auto paramsIt = paramsByDef_.find(resolvedPath);
    if (paramsIt == paramsByDef_.end()) {
      return false;
    }
    std::string orderedArgError;
    std::vector<const Expr *> orderedArgs;
    if (!buildOrderedArguments(paramsIt->second, pointerExpr.args, pointerExpr.argNames, orderedArgs, orderedArgError)) {
      return false;
    }
    for (size_t index = 0; index < paramsIt->second.size() && index < orderedArgs.size(); ++index) {
      const ParameterInfo &param = paramsIt->second[index];
      const Expr *arg = orderedArgs[index];
      if (arg == nullptr || param.name != returnedValueExpr->name) {
        continue;
      }
      const std::string normalizedParamType = normalizeBindingTypeName(param.binding.typeName);
      if (normalizedParamType != "Reference" && normalizedParamType != "Pointer") {
        return false;
      }
      return resolveLocationRootBindingName(*arg, rootNameOut);
    }
    return false;
  };

  auto applyStorageCall = [&](const std::string &callName,
                              const Expr &target,
                              std::unordered_map<std::string, BindingInfo> &localsIn,
                              StateMap &statesIn,
                              bool consumeBorrow) -> std::optional<std::string> {
    std::string targetName;
    if (target.kind == Expr::Kind::Name) {
      targetName = target.name;
    } else if (target.kind == Expr::Kind::Call) {
      std::string pointerBuiltinName;
      if (!getBuiltinPointerName(target, pointerBuiltinName) || pointerBuiltinName != "dereference" ||
          target.args.size() != 1) {
        return std::nullopt;
      }
      if (target.args.front().kind == Expr::Kind::Name) {
        const std::string &referenceName = target.args.front().name;
        auto localIt = localsIn.find(referenceName);
        if (localIt != localsIn.end()) {
          const std::string normalizedType = normalizeBindingTypeName(localIt->second.typeName);
          if ((normalizedType == "Reference" || normalizedType == "Pointer") &&
              !localIt->second.referenceRoot.empty()) {
            targetName = localIt->second.referenceRoot;
          }
        }
        if (targetName.empty() && !resolveLocationRootBindingName(target.args.front(), targetName)) {
          return std::nullopt;
        }
      } else if (!resolveLocationRootBindingName(target.args.front(), targetName)) {
        return std::nullopt;
      }
    } else {
      return std::nullopt;
    }
    auto bindingIt = localsIn.find(targetName);
    if (bindingIt == localsIn.end() || !isUninitializedBinding(bindingIt->second)) {
      return std::nullopt;
    }
    auto stateIt = statesIn.find(targetName);
    UninitState current = stateIt == statesIn.end() ? UninitState::Unknown : stateIt->second;
    if (callName == "init") {
      if (current != UninitState::Uninitialized) {
        return "init requires uninitialized storage: " + targetName;
      }
      statesIn[targetName] = UninitState::Initialized;
      return std::nullopt;
    }
    if (callName == "drop") {
      if (current != UninitState::Initialized) {
        return "drop requires initialized storage: " + targetName;
      }
      statesIn[targetName] = UninitState::Uninitialized;
      return std::nullopt;
    }
    if (callName == "take") {
      if (current != UninitState::Initialized) {
        return "take requires initialized storage: " + targetName;
      }
      statesIn[targetName] = UninitState::Uninitialized;
      return std::nullopt;
    }
    if (callName == "borrow") {
      if (current != UninitState::Initialized) {
        return "borrow requires initialized storage: " + targetName;
      }
      statesIn[targetName] = consumeBorrow ? UninitState::Uninitialized : UninitState::Initialized;
      return std::nullopt;
    }
    return std::nullopt;
  };

  struct FlowResult {
    std::optional<std::string> error;
    bool terminated = false;
  };

  struct LocalBindingScope {
    std::unordered_map<std::string, BindingInfo> &locals;
    std::vector<std::string> insertedNames;

    explicit LocalBindingScope(
        std::unordered_map<std::string, BindingInfo> &localsIn)
        : locals(localsIn) {}

    void add(const std::string &name, BindingInfo info) {
      auto [it, inserted] = locals.try_emplace(name, std::move(info));
      if (inserted) {
        insertedNames.push_back(it->first);
      }
    }

    ~LocalBindingScope() {
      for (auto it = insertedNames.rbegin(); it != insertedNames.rend(); ++it) {
        locals.erase(*it);
      }
    }
  };

  std::function<std::optional<std::string>(const Expr &,
                                           std::unordered_map<std::string, BindingInfo> &,
                                           StateMap &)>
      applyExprEffects;

  std::function<std::optional<std::string>(const std::vector<Expr> &,
                                           std::unordered_map<std::string, BindingInfo> &,
                                           StateMap &)>
      analyzeValueBlock;

  std::function<FlowResult(const Expr &,
                           std::unordered_map<std::string, BindingInfo> &,
                           StateMap &)>
      analyzeStatement;

  std::function<FlowResult(const std::vector<Expr> &,
                           std::unordered_map<std::string, BindingInfo> &,
                           StateMap &)>
      analyzeStatements;

  analyzeStatements = [&](const std::vector<Expr> &stmts,
                          std::unordered_map<std::string, BindingInfo> &localsIn,
                          StateMap &statesIn) -> FlowResult {
    for (const auto &stmt : stmts) {
      FlowResult result = analyzeStatement(stmt, localsIn, statesIn);
      if (result.error.has_value()) {
        return result;
      }
      if (result.terminated) {
        return result;
      }
    }
    return {};
  };

  analyzeValueBlock = [&](const std::vector<Expr> &stmts,
                          std::unordered_map<std::string, BindingInfo> &localsIn,
                          StateMap &statesIn) -> std::optional<std::string> {
    LocalBindingScope localScope(localsIn);
    if (stmts.empty()) {
      return std::nullopt;
    }
    for (size_t i = 0; i < stmts.size(); ++i) {
      const Expr &stmt = stmts[i];
      const bool isLast = (i + 1 == stmts.size());
      if (stmt.isBinding) {
        if (stmt.args.size() == 1) {
          if (auto err = applyExprEffects(stmt.args.front(), localsIn, statesIn)) {
            return err;
          }
        }
        BindingInfo info;
        std::optional<std::string> restrictType;
        std::string error;
        if (!parseBindingInfo(stmt, stmt.namespacePrefix, structNames_, importAliases_, info,
                              restrictType, error, &sumNames_)) {
          return error;
        }
        const std::string normalizedType = normalizeBindingTypeName(info.typeName);
        if ((normalizedType == "Reference" || normalizedType == "Pointer") && stmt.args.size() == 1) {
          std::string referenceRoot;
          if (resolveLocationRootBindingName(stmt.args.front(), referenceRoot)) {
            info.referenceRoot = std::move(referenceRoot);
          }
        }
        const bool uninitializedBinding = isUninitializedBinding(info);
        localScope.add(stmt.name, std::move(info));
        if (uninitializedBinding) {
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
          return std::nullopt;
        }
        continue;
      }
      if (auto err = applyExprEffects(stmt, localsIn, statesIn)) {
        return err;
      }
      if (isLast) {
        return std::nullopt;
      }
    }
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
        if (auto err = applyStorageCall(expr.name, expr.args.front(), locals, states, false)) {
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
          LocalBindingScope thenScope(locals);
          if (auto err = analyzeValueBlock(thenArg.bodyArguments, locals, thenStates)) {
            return err;
          }
        } else if (auto err = applyExprEffects(thenArg, locals, thenStates)) {
          return err;
        }
        if (elseArg.kind == Expr::Kind::Call && (elseArg.hasBodyArguments || !elseArg.bodyArguments.empty())) {
          LocalBindingScope elseScope(locals);
          if (auto err = analyzeValueBlock(elseArg.bodyArguments, locals, elseStates)) {
            return err;
          }
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
        LocalBindingScope bodyScope(locals);
        if (auto err = analyzeValueBlock(expr.bodyArguments, locals, states)) {
          return err;
        }
      }
      return std::nullopt;
    };

    if (stmt.isBinding) {
      if (stmt.args.size() == 1) {
        if (auto err = applyExprEffects(stmt.args.front(), localsIn, statesIn)) {
          return {err, false};
        }
      }
      BindingInfo info;
      std::optional<std::string> restrictType;
      std::string error;
      if (!parseBindingInfo(stmt, stmt.namespacePrefix, structNames_, importAliases_, info,
                            restrictType, error, &sumNames_)) {
        return {error, false};
      }
      const std::string normalizedType = normalizeBindingTypeName(info.typeName);
      if ((normalizedType == "Reference" || normalizedType == "Pointer") && stmt.args.size() == 1) {
        std::string referenceRoot;
        if (resolveLocationRootBindingName(stmt.args.front(), referenceRoot)) {
          info.referenceRoot = std::move(referenceRoot);
        }
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
        if (!arg.isMethodCall &&
            (isSimpleCallName(arg, "take") || isSimpleCallName(arg, "borrow")) &&
            !arg.args.empty()) {
          if (auto err =
                  applyStorageCall(arg.name, arg.args.front(), localsIn,
                                   statesIn, true)) {
            return {err, false};
          }
          continue;
        }
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
      StateMap thenStates = statesIn;
      StateMap elseStates = statesIn;
      FlowResult thenResult;
      FlowResult elseResult;
      if (thenArg.kind == Expr::Kind::Call && (thenArg.hasBodyArguments || !thenArg.bodyArguments.empty())) {
        LocalBindingScope thenScope(localsIn);
        thenResult = analyzeStatements(thenArg.bodyArguments, localsIn, thenStates);
        if (thenResult.error.has_value()) {
          return thenResult;
        }
      }
      if (elseArg.kind == Expr::Kind::Call && (elseArg.hasBodyArguments || !elseArg.bodyArguments.empty())) {
        LocalBindingScope elseScope(localsIn);
        elseResult = analyzeStatements(elseArg.bodyArguments, localsIn, elseStates);
        if (elseResult.error.has_value()) {
          return elseResult;
        }
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
                               std::unordered_map<std::string, BindingInfo> &localsBase,
                               const StateMap &statesBase,
                               StateMap &statesOut,
                               bool &terminatedOut) -> FlowResult {
      if (body.kind == Expr::Kind::Call && (body.hasBodyArguments || !body.bodyArguments.empty())) {
        StateMap bodyStates = statesBase;
        LocalBindingScope bodyScope(localsBase);
        FlowResult result = analyzeStatements(body.bodyArguments, localsBase, bodyStates);
        statesOut = std::move(bodyStates);
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
      LocalBindingScope forScope(localsIn);
      auto &loopLocals = localsIn;
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
            StateMap bodyStates = iterStates;
            FlowResult result;
            {
              LocalBindingScope bodyScope(localsIn);
              result = analyzeStatements(stmt.bodyArguments, localsIn, bodyStates);
            }
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
        StateMap bodyStates = loopHead;
        FlowResult result;
        {
          LocalBindingScope bodyScope(localsIn);
          result = analyzeStatements(stmt.bodyArguments, localsIn, bodyStates);
        }
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
      StateMap bodyStates = statesIn;
      FlowResult result;
      {
        LocalBindingScope bodyScope(localsIn);
        result = analyzeStatements(stmt.bodyArguments, localsIn, bodyStates);
      }
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
  FlowResult result = analyzeStatements(statements, locals, states);
  if (result.error.has_value()) {
    return result.error;
  }
  if (result.terminated) {
    return std::nullopt;
  }
  if (auto name = firstNonUninitialized(states)) {
    return "uninitialized storage must be dropped before end of scope: " + *name;
  }
  return std::nullopt;
}

}  // namespace primec::semantics
