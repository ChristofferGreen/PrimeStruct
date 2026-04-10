#include "SemanticsValidator.h"

#include <algorithm>
#include <cctype>
#include <iomanip>
#include <limits>
#include <sstream>

namespace primec::semantics {

bool SemanticsValidator::validateExprEarlyPointerBuiltin(
    const std::vector<ParameterInfo> &params,
    const std::unordered_map<std::string, BindingInfo> &locals,
    const Expr &expr,
    const ExprDispatchBootstrap &dispatchBootstrap,
    bool &handledOut) {
  handledOut = false;

  auto isAcceptedLocationTarget = [&](const Expr &target, const auto &self) -> bool {
    if (target.kind == Expr::Kind::Name) {
      return !isEntryArgsName(target.name) &&
             (locals.count(target.name) != 0 || isParam(params, target.name));
    }
    if (target.kind != Expr::Kind::Call) {
      return false;
    }
    if (dispatchBootstrap.isDeclaredPointerLikeCall != nullptr &&
        dispatchBootstrap.isDeclaredPointerLikeCall(target)) {
      return true;
    }
    if (target.isFieldAccess && target.args.size() == 1) {
      return self(target.args.front(), self);
    }
    std::string indexedElemType;
    const auto &dispatchResolvers = dispatchBootstrap.dispatchResolvers;
    const bool resolvesWrappedIndexedArgsPackElement =
        ((dispatchResolvers.resolveIndexedArgsPackElementType != nullptr &&
          dispatchResolvers.resolveIndexedArgsPackElementType(target, indexedElemType)) ||
         (dispatchResolvers.resolveWrappedIndexedArgsPackElementType != nullptr &&
          dispatchResolvers.resolveWrappedIndexedArgsPackElementType(target, indexedElemType)) ||
         (dispatchResolvers.resolveDereferencedIndexedArgsPackElementType != nullptr &&
          dispatchResolvers.resolveDereferencedIndexedArgsPackElementType(target, indexedElemType)));
    if (!resolvesWrappedIndexedArgsPackElement) {
      return false;
    }
    return unwrapReferencePointerTypeText(indexedElemType) != indexedElemType;
  };

  std::string earlyPointerBuiltin;
  if (!getBuiltinPointerName(expr, earlyPointerBuiltin)) {
    return true;
  }

  handledOut = true;
  auto failExprCallResolution = [&](std::string message) -> bool {
    return failExprDiagnostic(expr, std::move(message));
  };
  if (hasNamedArguments(expr.argNames)) {
    return failExprCallResolution("named arguments not supported for builtin calls");
  }
  if (!expr.templateArgs.empty()) {
    return failExprCallResolution("pointer helpers do not accept template arguments");
  }
  if (expr.hasBodyArguments || !expr.bodyArguments.empty()) {
    return failExprCallResolution("pointer helpers do not accept block arguments");
  }
  if (expr.args.size() != 1) {
    return failExprCallResolution("argument count mismatch for builtin " + earlyPointerBuiltin);
  }
  if (earlyPointerBuiltin == "location") {
    const Expr &target = expr.args.front();
    if (!isAcceptedLocationTarget(target, isAcceptedLocationTarget)) {
      return failExprCallResolution("location requires a local binding");
    }
  }
  if (earlyPointerBuiltin == "dereference" &&
      !isPointerLikeExpr(expr.args.front(), params, locals) &&
      !dispatchBootstrap.isDeclaredPointerLikeCall(expr.args.front())) {
    return failExprCallResolution("dereference requires a pointer or reference");
  }
  return validateExpr(params, locals, expr.args.front());
}

std::string SemanticsValidator::resolveExprConcreteCallPath(
    const std::vector<ParameterInfo> &params,
    const std::unordered_map<std::string, BindingInfo> &locals,
    const Expr &expr,
    const std::string &candidatePath) const {
  const Definition *activeDefinition = currentDefinitionContext_;
  const Execution *activeExecution = currentExecutionContext_;
  const bool hasScopedOwner = activeDefinition != nullptr || activeExecution != nullptr;
  if (hasScopedOwner &&
      (callTargetResolutionScratch_.definitionOwner != activeDefinition ||
       callTargetResolutionScratch_.executionOwner != activeExecution)) {
    callTargetResolutionScratch_.resetArena();
    callTargetResolutionScratch_.definitionOwner = activeDefinition;
    callTargetResolutionScratch_.executionOwner = activeExecution;
  } else if (!hasScopedOwner &&
             (!callTargetResolutionScratch_.definitionFamilyPathCache.empty() ||
              !callTargetResolutionScratch_.overloadFamilyPathCache.empty() ||
              !callTargetResolutionScratch_.overloadFamilyPrefixCache.empty() ||
              !callTargetResolutionScratch_.specializationPrefixCache.empty() ||
              !callTargetResolutionScratch_.overloadCandidatePathCache.empty() ||
              !callTargetResolutionScratch_.normalizedMethodNameCache.empty() ||
              !callTargetResolutionScratch_.explicitRemovedMethodPathCache.empty() ||
              !callTargetResolutionScratch_.concreteCallBaseCandidates.empty() ||
              callTargetResolutionScratch_.definitionOwner != nullptr ||
              callTargetResolutionScratch_.executionOwner != nullptr)) {
    callTargetResolutionScratch_.resetArena();
  }

  auto pathExists = [&](const std::string &path) {
    return defMap_.count(path) > 0 || paramsByDef_.count(path) > 0;
  };
  auto overloadFamilyPrefix = [&](const std::string &path) {
    if (!hasScopedOwner) {
      return path + "__ov";
    }
    const SymbolId pathKey = callTargetResolutionScratch_.keyInterner.intern(path);
    if (pathKey == InvalidSymbolId) {
      return path + "__ov";
    }
    if (const auto cacheIt = callTargetResolutionScratch_.overloadFamilyPrefixCache.find(pathKey);
        cacheIt != callTargetResolutionScratch_.overloadFamilyPrefixCache.end()) {
      return cacheIt->second;
    }
    auto [insertIt, inserted] =
        callTargetResolutionScratch_.overloadFamilyPrefixCache.emplace(pathKey, path + "__ov");
    (void)inserted;
    return insertIt->second;
  };
  auto specializationPrefix = [&](const std::string &basePath) {
    if (!hasScopedOwner) {
      return basePath + "__t";
    }
    const SymbolId basePathKey = callTargetResolutionScratch_.keyInterner.intern(basePath);
    if (basePathKey == InvalidSymbolId) {
      return basePath + "__t";
    }
    if (const auto cacheIt = callTargetResolutionScratch_.specializationPrefixCache.find(basePathKey);
        cacheIt != callTargetResolutionScratch_.specializationPrefixCache.end()) {
      return cacheIt->second;
    }
    auto [insertIt, inserted] =
        callTargetResolutionScratch_.specializationPrefixCache.emplace(basePathKey, basePath + "__t");
    (void)inserted;
    return insertIt->second;
  };
  auto overloadCandidatePath = [&](const std::string &path, size_t parameterCount) {
    if (!hasScopedOwner || parameterCount > static_cast<size_t>(std::numeric_limits<uint32_t>::max())) {
      return overloadFamilyPrefix(path) + std::to_string(parameterCount);
    }
    const SymbolId pathKey = callTargetResolutionScratch_.keyInterner.intern(path);
    if (pathKey == InvalidSymbolId) {
      return overloadFamilyPrefix(path) + std::to_string(parameterCount);
    }
    const CallTargetResolutionScratch::SymbolIndexKey key{
        pathKey, static_cast<uint32_t>(parameterCount)};
    if (const auto cacheIt = callTargetResolutionScratch_.overloadCandidatePathCache.find(key);
        cacheIt != callTargetResolutionScratch_.overloadCandidatePathCache.end()) {
      return cacheIt->second;
    }
    const std::string cachedPath = overloadFamilyPrefix(path) + std::to_string(parameterCount);
    auto [insertIt, inserted] =
        callTargetResolutionScratch_.overloadCandidatePathCache.emplace(key, cachedPath);
    (void)inserted;
    return insertIt->second;
  };
  auto computeHasOverloadFamily = [&](const std::string &path) {
    const std::string overloadPrefix = overloadFamilyPrefix(path);
    for (const auto &[candidate, defPtr] : defMap_) {
      (void)defPtr;
      if (candidate.rfind(overloadPrefix, 0) == 0) {
        return true;
      }
    }
    for (const auto &[candidate, candidateParams] : paramsByDef_) {
      (void)candidateParams;
      if (candidate.rfind(overloadPrefix, 0) == 0) {
        return true;
      }
    }
    return false;
  };
  auto hasOverloadFamily = [&](const std::string &path) {
    if (!hasScopedOwner) {
      return computeHasOverloadFamily(path);
    }
    const SymbolId pathKey = callTargetResolutionScratch_.keyInterner.intern(path);
    if (pathKey != InvalidSymbolId) {
      if (const auto cacheIt = callTargetResolutionScratch_.overloadFamilyPathCache.find(pathKey);
          cacheIt != callTargetResolutionScratch_.overloadFamilyPathCache.end()) {
        return cacheIt->second;
      }
    }
    const bool hasOverloads = computeHasOverloadFamily(path);
    if (pathKey != InvalidSymbolId) {
      callTargetResolutionScratch_.overloadFamilyPathCache.emplace(pathKey, hasOverloads);
    }
    return hasOverloads;
  };
  auto appendIfMissing = [](auto &paths, const std::string &path) {
    if (path.empty()) {
      return;
    }
    if (std::find(paths.begin(), paths.end(), path) == paths.end()) {
      paths.push_back(path);
    }
  };
  auto fnv1a64 = [](const std::string &text) {
    uint64_t hash = 1469598103934665603ULL;
    for (unsigned char c : text) {
      hash ^= static_cast<uint64_t>(c);
      hash *= 1099511628211ULL;
    }
    return hash;
  };
  auto stripWhitespace = [](const std::string &text) {
    std::string out;
    out.reserve(text.size());
    for (char c : text) {
      if (!std::isspace(static_cast<unsigned char>(c))) {
        out.push_back(c);
      }
    }
    return out;
  };
  const std::string templateHashSuffix = [&]() {
    if (expr.templateArgs.empty()) {
      return std::string{};
    }
    std::ostringstream out;
    out << std::hex << fnv1a64(stripWhitespace(joinTemplateArgs(expr.templateArgs)));
    return out.str();
  }();
  auto specializedPathForBase = [&](const std::string &basePath) {
    if (expr.templateArgs.empty()) {
      return std::string{};
    }
    const std::string specializationPrefixText = specializationPrefix(basePath);
    const std::string directCandidate = specializationPrefixText + templateHashSuffix;
    if (pathExists(directCandidate)) {
      return directCandidate;
    }
    std::string singleCandidate;
    auto considerCandidate = [&](const std::string &candidate) {
      if (candidate.rfind(specializationPrefixText, 0) != 0) {
        return;
      }
      if (!singleCandidate.empty() && singleCandidate != candidate) {
        singleCandidate.clear();
        singleCandidate = "__ambiguous__";
        return;
      }
      singleCandidate = candidate;
    };
    for (const auto &[candidate, defPtr] : defMap_) {
      (void)defPtr;
      considerCandidate(candidate);
      if (singleCandidate == "__ambiguous__") {
        return std::string{};
      }
    }
    for (const auto &[candidate, candidateParams] : paramsByDef_) {
      (void)candidateParams;
      considerCandidate(candidate);
      if (singleCandidate == "__ambiguous__") {
        return std::string{};
      }
    }
    return singleCandidate == "__ambiguous__" ? std::string{} : singleCandidate;
  };

  const bool hasTemplateOverloads = hasOverloadFamily(candidatePath);
  const bool isTypeNamespaceCall = isTypeNamespaceMethodCall(params, locals, expr, candidatePath);
  const bool hasExplicitReceiverBinding =
      expr.isMethodCall && !expr.args.empty() &&
      expr.args.front().kind == Expr::Kind::Name &&
      (findParamBinding(params, expr.args.front().name) != nullptr ||
       locals.count(expr.args.front().name) > 0);
  auto buildBaseCandidates = [&](auto &baseCandidates) {
    baseCandidates.clear();
    if (hasTemplateOverloads) {
      if (isTypeNamespaceCall && !expr.args.empty()) {
        appendIfMissing(baseCandidates, overloadCandidatePath(candidatePath, expr.args.size() - 1));
      } else if (expr.isMethodCall) {
        const size_t parameterCount =
            hasExplicitReceiverBinding ? expr.args.size() : expr.args.size() + 1;
        appendIfMissing(baseCandidates, overloadCandidatePath(candidatePath, parameterCount));
      } else {
        appendIfMissing(baseCandidates, overloadCandidatePath(candidatePath, expr.args.size()));
      }
    }
    appendIfMissing(baseCandidates, candidatePath);
  };
  auto resolveFromBaseCandidates = [&](const auto &baseCandidates) {
    for (const auto &basePath : baseCandidates) {
      if (const std::string specializedPath = specializedPathForBase(basePath);
          !specializedPath.empty()) {
        return specializedPath;
      }
      if (pathExists(basePath)) {
        return basePath;
      }
    }
    return baseCandidates.empty() ? candidatePath : baseCandidates.front();
  };

  if (hasScopedOwner) {
    auto &baseCandidates = callTargetResolutionScratch_.concreteCallBaseCandidates;
    buildBaseCandidates(baseCandidates);
    return resolveFromBaseCandidates(baseCandidates);
  }
  std::vector<std::string> baseCandidates;
  buildBaseCandidates(baseCandidates);
  return resolveFromBaseCandidates(baseCandidates);
}

} // namespace primec::semantics
