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
  auto hasDefinitionFamilyPath = [&](std::string_view path) {
    const std::string pathText(path);
    if (pathExists(pathText)) {
      return true;
    }
    const std::string templatedPrefix = pathText + "<";
    const std::string specializedPrefix = pathText + "__t";
    for (const auto &def : program_.definitions) {
      if (def.fullPath == path || def.fullPath.rfind(templatedPrefix, 0) == 0 ||
          def.fullPath.rfind(specializedPrefix, 0) == 0) {
        return true;
      }
    }
    return false;
  };
  if (candidatePath == "/equal" || candidatePath == "equal") {
    std::string reflectedStructEqualityHelperPath;
    if (resolveReflectedStructEqualityHelperPath(params,
                                                 locals,
                                                 expr,
                                                 "equal",
                                                 reflectedStructEqualityHelperPath) &&
        pathExists(reflectedStructEqualityHelperPath)) {
      return reflectedStructEqualityHelperPath;
    }
  }
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
    return overloadFamilyBasePaths_.count(path) > 0;
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
  auto isMapEntryConstructorPath = [](const std::string &path) {
    return path == "/std/collections/map/entry" ||
           path.rfind("/std/collections/map/entry__", 0) == 0 ||
           path == "/map/entry" ||
           path.rfind("/map/entry__", 0) == 0 ||
           path == "/std/collections/experimental_map/entry" ||
           path.rfind("/std/collections/experimental_map/entry__", 0) == 0;
  };
  auto explicitCallPath = [](const Expr &callExpr) -> std::string {
    if (callExpr.kind != Expr::Kind::Call || callExpr.isMethodCall ||
        callExpr.name.empty()) {
      return {};
    }
    if (callExpr.name.front() == '/') {
      return callExpr.name;
    }
    std::string prefix = callExpr.namespacePrefix;
    if (!prefix.empty() && prefix.front() != '/') {
      prefix.insert(prefix.begin(), '/');
    }
    if (prefix.empty()) {
      return "/" + callExpr.name;
    }
    return prefix + "/" + callExpr.name;
  };
  auto isMapEntryConstructorExpr = [&](const Expr &argExpr) {
    if (argExpr.kind != Expr::Kind::Call || argExpr.isMethodCall) {
      return false;
    }
    const std::string resolvedArgPath = resolveCalleePath(argExpr);
    if (isMapEntryConstructorPath(resolvedArgPath)) {
      return true;
    }
    return isMapEntryConstructorPath(explicitCallPath(argExpr));
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
    if (ambiguousSpecializationBasePaths_.count(basePath) > 0) {
      return std::string{};
    }
    const auto specializationIt = uniqueSpecializationPathByBase_.find(basePath);
    if (specializationIt == uniqueSpecializationPathByBase_.end()) {
      return std::string{};
    }
    return pathExists(specializationIt->second) ? specializationIt->second : std::string{};
  };
  auto stripSamePathSoaSpecializationSuffix = [](std::string path) {
    const size_t lastSlash = path.find_last_of('/');
    const size_t specializationSuffix = path.find("__t");
    if (specializationSuffix != std::string::npos &&
        lastSlash != std::string::npos &&
        specializationSuffix > lastSlash) {
      path.erase(specializationSuffix);
    }
    return path;
  };
  auto canonicalSamePathSoaHelperBase = [&](std::string_view path) -> std::string {
    const std::string strippedPath =
        stripSamePathSoaSpecializationSuffix(std::string(path));
    if (strippedPath.rfind("/std/collections/experimental_soa_vector/SoaVector__", 0) == 0) {
      const size_t lastSlash = strippedPath.find_last_of('/');
      const std::string helperName =
          lastSlash == std::string::npos ? std::string{} : strippedPath.substr(lastSlash + 1);
      if (helperName == "count" || helperName == "count_ref" ||
          helperName == "get" || helperName == "get_ref" ||
          helperName == "ref" || helperName == "ref_ref" ||
          helperName == "to_aos" || helperName == "to_aos_ref" ||
          helperName == "push" || helperName == "reserve") {
        return preferredSoaHelperTargetForCollectionType(helperName, "/soa_vector");
      }
    }
    const std::string canonicalToAosPath =
        canonicalizeLegacySoaToAosHelperPath(strippedPath);
    if (canonicalToAosPath == "/std/collections/soa_vector/to_aos" ||
        canonicalToAosPath == "/std/collections/soa_vector/to_aos_ref") {
      return canonicalToAosPath;
    }
    const std::string canonicalGetPath =
        canonicalizeLegacySoaGetHelperPath(strippedPath);
    if (canonicalGetPath == "/std/collections/soa_vector/count" ||
        canonicalGetPath == "/std/collections/soa_vector/count_ref" ||
        canonicalGetPath == "/std/collections/soa_vector/get" ||
        canonicalGetPath == "/std/collections/soa_vector/get_ref") {
      return canonicalGetPath;
    }
    const std::string canonicalRefPath =
        canonicalizeLegacySoaRefHelperPath(strippedPath);
    if (canonicalRefPath == "/std/collections/soa_vector/ref" ||
        canonicalRefPath == "/std/collections/soa_vector/ref_ref") {
      return canonicalRefPath;
    }
    if (strippedPath == "/soa_vector/push" ||
        strippedPath == "/std/collections/soa_vector/push" ||
        strippedPath == "/soa_vector/reserve" ||
        strippedPath == "/std/collections/soa_vector/reserve") {
      return strippedPath;
    }
    return {};
  };

  const bool hasTemplateOverloads = hasOverloadFamily(candidatePath);
  const bool isTypeNamespaceCall = isTypeNamespaceMethodCall(params, locals, expr, candidatePath);
  const bool hasExplicitReceiverBinding =
      expr.isMethodCall && !expr.args.empty() &&
      expr.args.front().kind == Expr::Kind::Name &&
      (findParamBinding(params, expr.args.front().name) != nullptr ||
       locals.count(expr.args.front().name) > 0);
  const std::string mapConstructorBasePath = [&]() -> std::string {
    if (candidatePath == "/std/collections/map/map" ||
        candidatePath.rfind("/std/collections/map/map__ov", 0) == 0) {
      return "/std/collections/map/map";
    }
    if (candidatePath == "/map" || candidatePath.rfind("/map__ov", 0) == 0) {
      return "/map";
    }
    return {};
  }();
  const bool preferDirectMapConstructorCandidate =
      !mapConstructorBasePath.empty() &&
      !expr.isMethodCall &&
      !expr.args.empty() &&
      std::all_of(expr.args.begin(), expr.args.end(), [&](const Expr &arg) {
        return isMapEntryConstructorExpr(arg);
      });
  auto buildBaseCandidates = [&](auto &baseCandidates) {
    baseCandidates.clear();
    if (preferDirectMapConstructorCandidate) {
      appendIfMissing(baseCandidates, overloadCandidatePath(mapConstructorBasePath, 1));
      appendIfMissing(baseCandidates, mapConstructorBasePath);
    }
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
    if (!expr.templateArgs.empty()) {
      if (const std::string preferredTemplateBearingSamePathCandidate =
              canonicalSamePathSoaHelperBase(candidatePath);
          !preferredTemplateBearingSamePathCandidate.empty() &&
          pathExists(preferredTemplateBearingSamePathCandidate)) {
        return preferredTemplateBearingSamePathCandidate;
      }
    }
    const auto preferredMethodLikeSamePathSoaHelperCandidate = [&]() -> std::string {
      if (!expr.isMethodCall) {
        return {};
      }
      std::string canonicalCountPath(candidatePath);
      const size_t countTemplateSuffix = canonicalCountPath.find("__t");
      if (countTemplateSuffix != std::string::npos) {
        canonicalCountPath.erase(countTemplateSuffix);
      }
      if (canonicalCountPath == "/soa_vector/count") {
        canonicalCountPath = "/std/collections/soa_vector/count";
      } else if (canonicalCountPath == "/soa_vector/count_ref") {
        canonicalCountPath = "/std/collections/soa_vector/count_ref";
      }
      if (canonicalCountPath == "/std/collections/soa_vector/count" &&
          pathExists("/soa_vector/count")) {
        return "/soa_vector/count";
      }
      if (canonicalCountPath == "/std/collections/soa_vector/count_ref" &&
          pathExists("/soa_vector/count_ref")) {
        return "/soa_vector/count_ref";
      }
      const std::string canonicalGetPath =
          canonicalizeLegacySoaGetHelperPath(candidatePath);
      if (canonicalGetPath == "/std/collections/soa_vector/get" &&
          pathExists("/soa_vector/get")) {
        return "/soa_vector/get";
      }
      if (canonicalGetPath == "/std/collections/soa_vector/get_ref" &&
          pathExists("/soa_vector/get_ref")) {
        return "/soa_vector/get_ref";
      }
      const std::string canonicalRefPath =
          canonicalizeLegacySoaRefHelperPath(candidatePath);
      if (canonicalRefPath == "/std/collections/soa_vector/ref" &&
          pathExists("/soa_vector/ref")) {
        return "/soa_vector/ref";
      }
      if (canonicalRefPath == "/std/collections/soa_vector/ref_ref" &&
          pathExists("/soa_vector/ref_ref")) {
        return "/soa_vector/ref_ref";
      }
      const std::string canonicalToAosPath =
          canonicalizeLegacySoaToAosHelperPath(candidatePath);
      if (canonicalToAosPath == "/std/collections/soa_vector/to_aos" &&
          pathExists("/to_aos")) {
        return "/to_aos";
      }
      if (canonicalToAosPath == "/std/collections/soa_vector/to_aos_ref" &&
          pathExists("/to_aos_ref")) {
        return "/to_aos_ref";
      }
      if (candidatePath == "/std/collections/soa_vector/push" &&
          pathExists("/soa_vector/push")) {
        return "/soa_vector/push";
      }
      if (candidatePath == "/std/collections/soa_vector/reserve" &&
          pathExists("/soa_vector/reserve")) {
        return "/soa_vector/reserve";
      }
      return {};
    }();
    if (!preferredMethodLikeSamePathSoaHelperCandidate.empty()) {
      return preferredMethodLikeSamePathSoaHelperCandidate;
    }
    const std::string samePathSoaCandidateBase =
        canonicalSamePathSoaHelperBase(candidatePath);
    if (!samePathSoaCandidateBase.empty() &&
        hasDefinitionFamilyPath(samePathSoaCandidateBase)) {
      return samePathSoaCandidateBase;
    }
    for (const auto &basePath : baseCandidates) {
      const std::string samePathSoaHelperBase =
          canonicalSamePathSoaHelperBase(basePath);
      if (!samePathSoaHelperBase.empty() &&
          hasDefinitionFamilyPath(samePathSoaHelperBase)) {
        return samePathSoaHelperBase;
      }
      if (const std::string specializedPath = specializedPathForBase(basePath);
          !specializedPath.empty()) {
        return specializedPath;
      }
      if (hasDefinitionFamilyPath(basePath)) {
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
