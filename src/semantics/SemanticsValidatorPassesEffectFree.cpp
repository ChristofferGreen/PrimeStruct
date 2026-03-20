#include "SemanticsValidator.h"

#include <cctype>
#include <functional>
#include <optional>
#include <unordered_set>
#include <vector>

namespace primec::semantics {

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
  bool hasExplicitEffects = false;
  std::unordered_set<std::string> explicitEffects;
  for (const auto &transform : def.transforms) {
    if (transform.name != "effects") {
      continue;
    }
    hasExplicitEffects = true;
    for (const auto &arg : transform.arguments) {
      explicitEffects.insert(arg);
    }
  }
  if (hasExplicitEffects && !explicitEffects.empty()) {
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
  auto normalizeCollectionMethodName = [](const std::string &receiverPath,
                                          std::string methodName) -> std::string {
    if (!methodName.empty() && methodName.front() == '/') {
      methodName.erase(methodName.begin());
    }
    if (receiverPath == "/vector" || receiverPath == "/array") {
      const std::string vectorPrefix = "vector/";
      const std::string arrayPrefix = "array/";
      const std::string stdVectorPrefix = "std/collections/vector/";
      if (methodName.rfind(vectorPrefix, 0) == 0) {
        return methodName.substr(vectorPrefix.size());
      }
      if (methodName.rfind(arrayPrefix, 0) == 0) {
        return methodName.substr(arrayPrefix.size());
      }
      if (methodName.rfind(stdVectorPrefix, 0) == 0) {
        return methodName.substr(stdVectorPrefix.size());
      }
    }
    if (receiverPath == "/map") {
      const std::string mapPrefix = "map/";
      const std::string stdMapPrefix = "std/collections/map/";
      if (methodName.rfind(mapPrefix, 0) == 0) {
        return methodName.substr(mapPrefix.size());
      }
      if (methodName.rfind(stdMapPrefix, 0) == 0) {
        return methodName.substr(stdMapPrefix.size());
      }
    }
    return methodName;
  };
  auto methodPathCandidatesForReceiver = [](const std::string &receiverPath,
                                            const std::string &methodName) -> std::vector<std::string> {
    if (receiverPath == "/vector") {
      if (methodName == "count") {
        return {"/vector/" + methodName,
                "/std/collections/vector/" + methodName};
      }
      return {"/vector/" + methodName,
              "/std/collections/vector/" + methodName,
              "/array/" + methodName};
    }
    if (receiverPath == "/array") {
      if (methodName == "count") {
        return {"/array/" + methodName};
      }
      return {"/array/" + methodName,
              "/vector/" + methodName,
              "/std/collections/vector/" + methodName};
    }
    if (receiverPath == "/map") {
      if (methodName == "count" || methodName == "at" || methodName == "at_unsafe") {
        return {"/std/collections/map/" + methodName,
                "/map/" + methodName};
      }
      return {"/map/" + methodName,
              "/std/collections/map/" + methodName};
    }
    return {receiverPath + "/" + methodName};
  };
  auto preferCollectionHelperPath = [&](const std::string &path) -> std::string {
    auto allowsArrayVectorCompatibilitySuffix = [](const std::string &suffix) {
      return suffix != "count" && suffix != "capacity" && suffix != "at" && suffix != "at_unsafe" &&
             suffix != "push" && suffix != "pop" && suffix != "reserve" && suffix != "clear" &&
             suffix != "remove_at" && suffix != "remove_swap";
    };
    auto allowsVectorStdlibCompatibilitySuffix = [](const std::string &suffix) {
      return suffix != "count" && suffix != "capacity" && suffix != "at" && suffix != "at_unsafe" &&
             suffix != "push" && suffix != "pop" && suffix != "reserve" && suffix != "clear" &&
             suffix != "remove_at" && suffix != "remove_swap";
    };
    std::string preferred = path;
    if (preferred.rfind("/array/", 0) == 0 && defMap_.count(preferred) == 0) {
      const std::string suffix = preferred.substr(std::string("/array/").size());
      if (allowsArrayVectorCompatibilitySuffix(suffix)) {
        const std::string vectorAlias = "/vector/" + suffix;
        if (defMap_.count(vectorAlias) > 0) {
          return vectorAlias;
        }
        const std::string stdlibAlias = "/std/collections/vector/" + suffix;
        if (defMap_.count(stdlibAlias) > 0) {
          return stdlibAlias;
        }
      }
    }
    if (preferred.rfind("/vector/", 0) == 0 && defMap_.count(preferred) == 0) {
      const std::string suffix = preferred.substr(std::string("/vector/").size());
      if (allowsVectorStdlibCompatibilitySuffix(suffix)) {
        const std::string stdlibAlias = "/std/collections/vector/" + suffix;
        if (defMap_.count(stdlibAlias) > 0) {
          preferred = stdlibAlias;
        } else {
          if (allowsArrayVectorCompatibilitySuffix(suffix)) {
            const std::string arrayAlias = "/array/" + suffix;
            if (defMap_.count(arrayAlias) > 0) {
              preferred = arrayAlias;
            }
          }
        }
      }
    }
    if (preferred.rfind("/std/collections/vector/", 0) == 0 && defMap_.count(preferred) == 0) {
      const std::string suffix = preferred.substr(std::string("/std/collections/vector/").size());
      if (allowsVectorStdlibCompatibilitySuffix(suffix)) {
        const std::string vectorAlias = "/vector/" + suffix;
        if (defMap_.count(vectorAlias) > 0) {
          preferred = vectorAlias;
        } else {
          if (allowsArrayVectorCompatibilitySuffix(suffix)) {
            const std::string arrayAlias = "/array/" + suffix;
            if (defMap_.count(arrayAlias) > 0) {
              preferred = arrayAlias;
            }
          }
        }
      }
    }
    if (preferred.rfind("/map/", 0) == 0 && defMap_.count(preferred) == 0) {
      const std::string suffix = preferred.substr(std::string("/map/").size());
      if (suffix != "count" && suffix != "contains" && suffix != "tryAt") {
        const std::string stdlibAlias = "/std/collections/map/" + suffix;
        if (defMap_.count(stdlibAlias) > 0) {
          preferred = stdlibAlias;
        }
      }
    }
    if (preferred.rfind("/std/collections/map/", 0) == 0 && defMap_.count(preferred) == 0) {
      const std::string suffix = preferred.substr(std::string("/std/collections/map/").size());
      if (suffix != "map" && suffix != "count" && suffix != "contains" && suffix != "tryAt" &&
          suffix != "at" && suffix != "at_unsafe") {
        const std::string mapAlias = "/map/" + suffix;
        if (defMap_.count(mapAlias) > 0) {
          preferred = mapAlias;
        }
      }
    }
    return preferred;
  };
  auto collectionHelperPathCandidates = [&](const std::string &path) {
    auto allowsArrayVectorCompatibilitySuffix = [](const std::string &suffix) {
      return suffix != "count" && suffix != "capacity" && suffix != "at" && suffix != "at_unsafe" &&
             suffix != "push" && suffix != "pop" && suffix != "reserve" && suffix != "clear" &&
             suffix != "remove_at" && suffix != "remove_swap";
    };
    auto allowsVectorStdlibCompatibilitySuffix = [](const std::string &suffix) {
      return suffix != "count" && suffix != "capacity" && suffix != "at" && suffix != "at_unsafe" &&
             suffix != "push" && suffix != "pop" && suffix != "reserve" && suffix != "clear" &&
             suffix != "remove_at" && suffix != "remove_swap";
    };
    std::vector<std::string> candidates;
    auto appendUnique = [&](const std::string &candidate) {
      if (candidate.empty()) {
        return;
      }
      for (const auto &existing : candidates) {
        if (existing == candidate) {
          return;
        }
      }
      candidates.push_back(candidate);
    };

    std::string normalizedPath = path;
    if (!normalizedPath.empty() && normalizedPath.front() != '/') {
      if (normalizedPath.rfind("array/", 0) == 0 || normalizedPath.rfind("vector/", 0) == 0 ||
          normalizedPath.rfind("std/collections/vector/", 0) == 0 || normalizedPath.rfind("map/", 0) == 0 ||
          normalizedPath.rfind("std/collections/map/", 0) == 0) {
        normalizedPath.insert(normalizedPath.begin(), '/');
      }
    }

    appendUnique(path);
    appendUnique(normalizedPath);
    if (normalizedPath.rfind("/array/", 0) == 0) {
      const std::string suffix = normalizedPath.substr(std::string("/array/").size());
      if (allowsArrayVectorCompatibilitySuffix(suffix)) {
        appendUnique("/vector/" + suffix);
        appendUnique("/std/collections/vector/" + suffix);
      }
    } else if (normalizedPath.rfind("/vector/", 0) == 0) {
      const std::string suffix = normalizedPath.substr(std::string("/vector/").size());
      if (allowsVectorStdlibCompatibilitySuffix(suffix)) {
        appendUnique("/std/collections/vector/" + suffix);
      }
      if (allowsArrayVectorCompatibilitySuffix(suffix)) {
        appendUnique("/array/" + suffix);
      }
    } else if (normalizedPath.rfind("/std/collections/vector/", 0) == 0) {
      const std::string suffix = normalizedPath.substr(std::string("/std/collections/vector/").size());
      if (allowsVectorStdlibCompatibilitySuffix(suffix)) {
        appendUnique("/vector/" + suffix);
      }
      if (allowsArrayVectorCompatibilitySuffix(suffix)) {
        appendUnique("/array/" + suffix);
      }
    } else if (normalizedPath.rfind("/map/", 0) == 0) {
      const std::string suffix = normalizedPath.substr(std::string("/map/").size());
      if (suffix != "count" && suffix != "contains" && suffix != "tryAt") {
        appendUnique("/std/collections/map/" + suffix);
      }
    } else if (normalizedPath.rfind("/std/collections/map/", 0) == 0) {
      const std::string suffix = normalizedPath.substr(std::string("/std/collections/map/").size());
      if (suffix != "map" && suffix != "count" && suffix != "contains" && suffix != "tryAt" &&
          suffix != "at" && suffix != "at_unsafe") {
        appendUnique("/map/" + suffix);
      }
    }
    return candidates;
  };
  std::function<std::string(const std::string &, const std::string &)> collectionPathFromType;
  collectionPathFromType = [&](const std::string &typeName, const std::string &typeTemplateArg) -> std::string {
    if (typeName == "string") {
      return "/string";
    }
    if ((typeName == "array" || typeName == "vector" || typeName == "soa_vector") && !typeTemplateArg.empty()) {
      return "/" + typeName;
    }
    if (isMapCollectionTypeName(typeName) && !typeTemplateArg.empty()) {
      return "/map";
    }
    std::string base;
    std::string argsText;
    if (!splitTemplateTypeName(typeName, base, argsText)) {
      return "";
    }
    std::vector<std::string> args;
    if (!splitTopLevelTemplateArgs(argsText, args)) {
      return "";
    }
    if ((base == "array" || base == "vector" || base == "soa_vector") && args.size() == 1) {
      return "/" + base;
    }
    if (isMapCollectionTypeName(base) && args.size() == 2) {
      return "/map";
    }
    if ((base == "Reference" || base == "Pointer") && args.size() == 1) {
      return collectionPathFromType(normalizeBindingTypeName(args.front()), "");
    }
    return "";
  };
  auto collectionPathFromBinding = [&](const BindingInfo &binding) -> std::string {
    if ((binding.typeName == "Reference" || binding.typeName == "Pointer") && !binding.typeTemplateArg.empty()) {
      return collectionPathFromType(normalizeBindingTypeName(binding.typeTemplateArg), "");
    }
    return collectionPathFromType(normalizeBindingTypeName(binding.typeName), binding.typeTemplateArg);
  };
  auto collectionPathFromCallExpr = [&](const Expr &callExpr) -> std::string {
    if (callExpr.kind != Expr::Kind::Call || callExpr.isMethodCall) {
      return "";
    }
    std::string builtinCollection;
    if (getBuiltinCollectionName(callExpr, builtinCollection)) {
      if (builtinCollection == "string") {
        return "/string";
      }
      if ((builtinCollection == "array" || builtinCollection == "vector" || builtinCollection == "soa_vector") &&
          callExpr.templateArgs.size() == 1) {
        return "/" + builtinCollection;
      }
      if (builtinCollection == "map" && callExpr.templateArgs.size() == 2) {
        return "/map";
      }
    }

    auto resolvedCandidates = collectionHelperPathCandidates(resolveCalleePath(callExpr));
    if (resolvedCandidates.empty()) {
      resolvedCandidates.push_back(resolveCalleePath(callExpr));
    }
    for (const auto &candidate : resolvedCandidates) {
      auto defIt = defMap_.find(candidate);
      if (defIt == defMap_.end() || !defIt->second) {
        continue;
      }
      for (const auto &transform : defIt->second->transforms) {
        if (transform.name != "return" || transform.templateArgs.size() != 1) {
          continue;
        }
        const std::string collectionPath =
            collectionPathFromType(normalizeBindingTypeName(transform.templateArgs.front()), "");
        if (!collectionPath.empty()) {
          return collectionPath;
        }
      }
    }
    return "";
  };
  auto resolveBareMapCallPath = [&](const Expr &callExpr) -> std::string {
    if (callExpr.isMethodCall || callExpr.args.empty()) {
      return "";
    }
    std::string helperName;
    std::string normalizedName = callExpr.name;
    if (!normalizedName.empty() && normalizedName.front() == '/') {
      normalizedName.erase(normalizedName.begin());
    }
    if (normalizedName == "count") {
      helperName = "count";
    } else if (normalizedName == "at") {
      helperName = "at";
    } else if (normalizedName == "at_unsafe") {
      helperName = "at_unsafe";
    } else {
      const std::string resolved = resolveCalleePath(callExpr);
      if (resolved == "/count") {
        helperName = "count";
      } else if (resolved == "/at") {
        helperName = "at";
      } else if (resolved == "/at_unsafe") {
        helperName = "at_unsafe";
      } else {
        return "";
      }
    }
    if (defMap_.count("/" + helperName) > 0) {
      return "";
    }
    std::vector<size_t> receiverIndices;
    auto appendReceiverIndex = [&](size_t index) {
      if (index >= callExpr.args.size()) {
        return;
      }
      for (size_t existing : receiverIndices) {
        if (existing == index) {
          return;
        }
      }
      receiverIndices.push_back(index);
    };
    if (hasNamedArguments(callExpr.argNames)) {
      bool foundValues = false;
      for (size_t i = 0; i < callExpr.args.size(); ++i) {
        if (i < callExpr.argNames.size() && callExpr.argNames[i].has_value() &&
            *callExpr.argNames[i] == "values") {
          appendReceiverIndex(i);
          foundValues = true;
          break;
        }
      }
      if (!foundValues) {
        for (size_t i = 0; i < callExpr.args.size(); ++i) {
          appendReceiverIndex(i);
        }
      }
    } else {
      for (size_t i = 0; i < callExpr.args.size(); ++i) {
        appendReceiverIndex(i);
      }
    }
    for (size_t receiverIndex : receiverIndices) {
      const Expr &receiver = callExpr.args[receiverIndex];
      if (receiver.kind == Expr::Kind::Name) {
        auto it = ctx.locals.find(receiver.name);
        if (it == ctx.locals.end()) {
          continue;
        }
        if (collectionPathFromBinding(it->second) == "/map") {
          return "/std/collections/map/" + helperName;
        }
      }
      if (collectionPathFromCallExpr(receiver) == "/map") {
        return "/std/collections/map/" + helperName;
      }
    }
    return "";
  };

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
        receiverStruct = collectionPathFromBinding(it->second);
        if (receiverStruct.empty() &&
            (it->second.typeName == "Reference" || it->second.typeName == "Pointer")) {
          receiverStruct = resolveStructTypePath(it->second.typeTemplateArg, receiver.namespacePrefix, structNames_);
          if (receiverStruct.empty()) {
            auto importIt = importAliases_.find(it->second.typeTemplateArg);
            if (importIt != importAliases_.end() && structNames_.count(importIt->second) > 0) {
              receiverStruct = importIt->second;
            }
          }
        } else if (receiverStruct.empty()) {
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
    if (receiverStruct.empty() && receiver.kind == Expr::Kind::Call) {
      receiverStruct = collectionPathFromCallExpr(receiver);
    }
    if (receiverStruct.empty()) {
      return false;
    }
    if (isExplicitRemovedCollectionMethodAlias(receiverStruct, expr.name)) {
      return false;
    }
    auto isTemplateCompatibleDefinition = [&](const Definition &def) -> bool {
      if (expr.templateArgs.empty()) {
        return true;
      }
      return !def.templateArgs.empty() && def.templateArgs.size() == expr.templateArgs.size();
    };
    const std::string normalizedMethodName = normalizeCollectionMethodName(receiverStruct, expr.name);
    const std::vector<std::string> methodCandidates =
        methodPathCandidatesForReceiver(receiverStruct, normalizedMethodName);
    std::string methodPath;
    bool hasMethodDefinitionCandidate = false;
    for (const auto &candidate : methodCandidates) {
      auto candidateIt = defMap_.find(candidate);
      if (candidateIt == defMap_.end() || !candidateIt->second) {
        continue;
      }
      if (isTemplateCompatibleDefinition(*candidateIt->second)) {
        methodPath = candidate;
        hasMethodDefinitionCandidate = true;
        break;
      }
    }
    if (!hasMethodDefinitionCandidate) {
      return false;
    }
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

  auto isTemplateCompatibleDefinition = [&](const Definition &def) -> bool {
    if (expr.templateArgs.empty()) {
      return true;
    }
    return !def.templateArgs.empty() && def.templateArgs.size() == expr.templateArgs.size();
  };
  std::string resolvedPath = resolveCalleePath(expr);
  const std::string bareMapCallPath = resolveBareMapCallPath(expr);
  if (!bareMapCallPath.empty()) {
    resolvedPath = bareMapCallPath;
  }
  if (isExplicitRemovedCollectionCallAlias(resolvedPath)) {
    return false;
  }
  const auto resolvedCandidates = collectionHelperPathCandidates(resolvedPath);
  const std::string resolved =
      resolvedCandidates.empty() ? preferCollectionHelperPath(resolvedPath) : resolvedCandidates.front();
  bool hasDefinitionCandidate = false;
  std::string selectedDefinitionPath;
  for (const auto &candidate : resolvedCandidates) {
    auto candidateIt = defMap_.find(candidate);
    if (candidateIt == defMap_.end() || !candidateIt->second) {
      continue;
    }
    if (!isTemplateCompatibleDefinition(*candidateIt->second)) {
      continue;
    }
    hasDefinitionCandidate = true;
    selectedDefinitionPath = candidate;
    break;
  }
  if (!hasDefinitionCandidate && resolvedCandidates.empty()) {
    auto preferredIt = defMap_.find(resolved);
    if (preferredIt != defMap_.end() && preferredIt->second &&
        isTemplateCompatibleDefinition(*preferredIt->second)) {
      hasDefinitionCandidate = true;
      selectedDefinitionPath = resolved;
    }
  }
  std::string builtinName;
  if (getBuiltinOperatorName(expr, builtinName) || getBuiltinComparisonName(expr, builtinName) ||
      getBuiltinClampName(expr, builtinName, true) || getBuiltinMinMaxName(expr, builtinName, true) ||
      getBuiltinAbsSignName(expr, builtinName, true) || getBuiltinSaturateName(expr, builtinName, true) ||
      getBuiltinMathName(expr, builtinName, true) || getBuiltinConvertName(expr, builtinName) ||
      getBuiltinArrayAccessName(expr, builtinName) || getBuiltinPointerName(expr, builtinName) ||
      (!expr.isMethodCall && isSimpleCallName(expr, "contains"))) {
    for (const auto &arg : expr.args) {
      if (!isOutsideEffectFreeExpr(arg, ctx, writesThis)) {
        return false;
      }
    }
    return true;
  }
  if (!hasDefinitionCandidate && getBuiltinCollectionName(expr, builtinName)) {
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

  auto defIt = defMap_.end();
  if (!selectedDefinitionPath.empty()) {
    defIt = defMap_.find(selectedDefinitionPath);
  } else if (resolvedCandidates.empty()) {
    defIt = defMap_.find(resolved);
  }
  if (defIt == defMap_.end() || !defIt->second) {
    return false;
  }
  const std::string resolvedDefinitionPath = selectedDefinitionPath.empty() ? resolved : selectedDefinitionPath;
  if (structNames_.count(resolvedDefinitionPath) > 0) {
    if (!isOutsideEffectFreeStructConstructor(resolvedDefinitionPath)) {
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

}  // namespace primec::semantics
