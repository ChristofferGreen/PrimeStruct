#include "SemanticsValidator.h"
#include "MapConstructorHelpers.h"

#include <algorithm>
#include <cctype>
#include <iomanip>
#include <limits>
#include <sstream>
#include <string_view>

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
    if (const auto *metadata = mapHelperSurfaceMetadataLocal();
        metadata != nullptr) {
      const std::string entryPath =
          std::string(metadata->canonicalPath) + "/entry";
      if (path == entryPath || path.rfind(entryPath + "__", 0) == 0) {
        return true;
      }
    }
    return isExperimentalCollectionConstructorPathLocal(path, "map", "entry");
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
  const std::string directExplicitCallPath = explicitCallPath(expr);
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
    auto isUnsignedIntegerArgText = [](std::string_view text) {
      if (text.empty()) {
        return false;
      }
      if (text.size() > 2 && text[0] == '0' && (text[1] == 'x' || text[1] == 'X')) {
        for (size_t i = 2; i < text.size(); ++i) {
          if (text[i] == ',') {
            continue;
          }
          if (!std::isxdigit(static_cast<unsigned char>(text[i]))) {
            return false;
          }
        }
        return true;
      }
      for (char c : text) {
        if (c == ',') {
          continue;
        }
        if (!std::isdigit(static_cast<unsigned char>(c))) {
          return false;
        }
      }
      return true;
    };
    auto isIntegerTemplateArg = [&](size_t index) {
      if (index < expr.templateArgDetails.size() &&
          expr.templateArgDetails[index].text == expr.templateArgs[index]) {
        return expr.templateArgDetails[index].kind == TemplateArgumentKind::Integer;
      }
      return isUnsignedIntegerArgText(expr.templateArgs[index]);
    };
    std::ostringstream canonical;
    for (size_t i = 0; i < expr.templateArgs.size(); ++i) {
      if (i > 0) {
        canonical << ",";
      }
      canonical << (isIntegerTemplateArg(i) ? "int:" : "type:")
                << stripWhitespace(expr.templateArgs[i]);
    }
    std::ostringstream out;
    out << std::hex << fnv1a64(canonical.str());
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
    if (isExperimentalSoaVectorSpecializedTypePath(strippedPath)) {
      const size_t lastSlash = strippedPath.find_last_of('/');
      const std::string helperName =
          lastSlash == std::string::npos ? std::string{} : strippedPath.substr(lastSlash + 1);
      if (isSupportedCompatibilitySoaHelperName(helperName)) {
        return preferredSoaHelperTargetForCollectionType(
            helperName, internalSoaCollectionTypePath(true));
      }
    }
    const std::string canonicalToAosPath =
        canonicalizeLegacySoaToAosHelperPath(strippedPath);
    if (isCanonicalStdlibSoaHelperPath(canonicalToAosPath, "to" "_aos") ||
        isCanonicalStdlibSoaHelperPath(canonicalToAosPath, "to" "_aos_ref")) {
      return canonicalToAosPath;
    }
    const std::string canonicalGetPath =
        canonicalizeLegacySoaGetHelperPath(strippedPath);
    if (isCanonicalStdlibSoaHelperPath(canonicalGetPath, "count") ||
        isCanonicalStdlibSoaHelperPath(canonicalGetPath, "count_ref") ||
        isCanonicalStdlibSoaHelperPath(canonicalGetPath, "get") ||
        isCanonicalStdlibSoaHelperPath(canonicalGetPath, "get_ref")) {
      return canonicalGetPath;
    }
    const std::string canonicalRefPath =
        canonicalizeLegacySoaRefHelperPath(strippedPath);
    if (isCanonicalSoaRefLikeHelperPath(canonicalRefPath)) {
      return canonicalRefPath;
    }
    if (isLegacyOrCanonicalSoaHelperPath(strippedPath, "push") ||
        isLegacyOrCanonicalSoaHelperPath(strippedPath, "reserve")) {
      return strippedPath;
    }
    return {};
  };

  const bool hasTemplateOverloads = hasOverloadFamily(candidatePath);
  const bool isTypeNamespaceCall = isTypeNamespaceMethodCall(params, locals, expr, candidatePath);
  if (!expr.isMethodCall && expr.namespacePrefix.empty() &&
      expr.name.find('/') == std::string::npos && expr.args.size() == 1 &&
      !hasNamedArguments(expr.argNames) && expr.templateArgs.empty() &&
      !expr.hasBodyArguments && expr.bodyArguments.empty()) {
    auto primitiveReceiverTypeName = [&](const Expr &receiver) -> std::string {
      if (receiver.kind == Expr::Kind::Literal) {
        return "i32";
      }
      if (receiver.kind == Expr::Kind::BoolLiteral) {
        return "bool";
      }
      if (receiver.kind == Expr::Kind::FloatLiteral) {
        return "f64";
      }
      if (receiver.kind != Expr::Kind::Name) {
        return {};
      }
      const BindingInfo *binding = findParamBinding(params, receiver.name);
      if (binding == nullptr) {
        const auto localIt = locals.find(receiver.name);
        if (localIt != locals.end()) {
          binding = &localIt->second;
        }
      }
      if (binding == nullptr || !binding->typeTemplateArg.empty()) {
        return {};
      }
      const std::string normalizedType =
          normalizeBindingTypeName(binding->typeName);
      if (normalizedType == "i32" || normalizedType == "i64" ||
          normalizedType == "u64" || normalizedType == "bool" ||
          normalizedType == "f32" || normalizedType == "f64" ||
          normalizedType == "integer" || normalizedType == "decimal" ||
          normalizedType == "complex") {
        return normalizedType;
      }
      return {};
    };
    const std::string receiverTypeName =
        primitiveReceiverTypeName(expr.args.front());
    if (!receiverTypeName.empty()) {
      const std::string methodStylePath = "/" + receiverTypeName + "/" + expr.name;
      if (hasDefinitionFamilyPath(methodStylePath)) {
        return methodStylePath;
      }
    }
    const BindingInfo *collectionBinding = nullptr;
    if (expr.args.front().kind == Expr::Kind::Name) {
      collectionBinding = findParamBinding(params, expr.args.front().name);
      if (collectionBinding == nullptr) {
        const auto localIt = locals.find(expr.args.front().name);
        if (localIt != locals.end()) {
          collectionBinding = &localIt->second;
        }
      }
    }
    if (collectionBinding != nullptr) {
      std::string collectionTypeText = collectionBinding->typeName;
      if (!collectionBinding->typeTemplateArg.empty()) {
        collectionTypeText += "<" + collectionBinding->typeTemplateArg + ">";
      }
      const std::string collectionPath =
          inferMethodCollectionTypePathFromTypeText(collectionTypeText);
      if ((collectionPath == "/array" || collectionPath == "/string") &&
          (expr.name == "count" || expr.name == "capacity" ||
           expr.name == "at" || expr.name == "at_unsafe")) {
        const std::string collectionHelperPath = collectionPath + "/" + expr.name;
        if (hasDefinitionFamilyPath(collectionHelperPath)) {
          return collectionHelperPath;
        }
      }
    }
  }
  const bool hasExplicitReceiverBinding =
      expr.isMethodCall && !expr.args.empty() &&
      expr.args.front().kind == Expr::Kind::Name &&
      (findParamBinding(params, expr.args.front().name) != nullptr ||
       locals.count(expr.args.front().name) > 0);
  const std::string mapConstructorBasePath = [&]() -> std::string {
    const auto *metadata = mapConstructorSurfaceMetadataLocal();
    if (metadata == nullptr) {
      return {};
    }
    const std::string canonicalPath(metadata->canonicalPath);
    if (candidatePath == canonicalPath ||
        candidatePath.rfind(canonicalPath + "__ov", 0) == 0) {
      return canonicalPath;
    }
    for (std::string_view alias : metadata->importAliasSpellings) {
      if (alias.find('/') != std::string_view::npos) {
        continue;
      }
      const std::string rootedAlias = "/" + std::string(alias);
      if (candidatePath == rootedAlias ||
          candidatePath.rfind(rootedAlias + "__ov", 0) == 0) {
        return rootedAlias;
      }
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
      if (canonicalCountPath == samePathSoaHelperTargetPath("count")) {
        canonicalCountPath = compatibilitySoaHelperTargetPath("count");
      } else if (canonicalCountPath == samePathSoaHelperTargetPath("count_ref")) {
        canonicalCountPath = compatibilitySoaHelperTargetPath("count_ref");
      }
      if (isCanonicalStdlibSoaHelperPath(canonicalCountPath, "count") &&
          pathExists(samePathSoaHelperTargetPath("count"))) {
        return samePathSoaHelperTargetPath("count");
      }
      if (isCanonicalStdlibSoaHelperPath(canonicalCountPath, "count_ref") &&
          pathExists(samePathSoaHelperTargetPath("count_ref"))) {
        return samePathSoaHelperTargetPath("count_ref");
      }
      const std::string canonicalGetPath =
          canonicalizeLegacySoaGetHelperPath(candidatePath);
      if (isCanonicalStdlibSoaHelperPath(canonicalGetPath, "get") &&
          pathExists(samePathSoaHelperTargetPath("get"))) {
        return samePathSoaHelperTargetPath("get");
      }
      if (isCanonicalStdlibSoaHelperPath(canonicalGetPath, "get_ref") &&
          pathExists(samePathSoaHelperTargetPath("get_ref"))) {
        return samePathSoaHelperTargetPath("get_ref");
      }
      const std::string canonicalRefPath =
          canonicalizeLegacySoaRefHelperPath(candidatePath);
      if (isCanonicalStdlibSoaHelperPath(canonicalRefPath, "ref") &&
          pathExists(samePathSoaHelperTargetPath("ref"))) {
        return samePathSoaHelperTargetPath("ref");
      }
      if (isCanonicalStdlibSoaHelperPath(canonicalRefPath, "ref_ref") &&
          pathExists(samePathSoaHelperTargetPath("ref_ref"))) {
        return samePathSoaHelperTargetPath("ref_ref");
      }
      const std::string canonicalToAosPath =
          canonicalizeLegacySoaToAosHelperPath(candidatePath);
      if (isCanonicalStdlibSoaHelperPath(canonicalToAosPath, "to" "_aos") &&
          pathExists("/to" "_aos")) {
        return "/to" "_aos";
      }
      if (isCanonicalStdlibSoaHelperPath(canonicalToAosPath, "to" "_aos_ref") &&
          pathExists("/to" "_aos_ref")) {
        return "/to" "_aos_ref";
      }
      if (isCanonicalStdlibSoaHelperPath(candidatePath, "push") &&
          pathExists(samePathSoaHelperTargetPath("push"))) {
        return samePathSoaHelperTargetPath("push");
      }
      if (isCanonicalStdlibSoaHelperPath(candidatePath, "reserve") &&
          pathExists(samePathSoaHelperTargetPath("reserve"))) {
        return samePathSoaHelperTargetPath("reserve");
      }
      return {};
    }();
    if (!preferredMethodLikeSamePathSoaHelperCandidate.empty()) {
      return preferredMethodLikeSamePathSoaHelperCandidate;
    }
    const auto preferredDirectToAosSamePathHelperCandidate = [&]() -> std::string {
      if (expr.isMethodCall || !expr.templateArgs.empty()) {
        return {};
      }
      if ((candidatePath == "/to" "_aos" || candidatePath == "/to" "_aos_ref") &&
          hasDefinitionFamilyPath(candidatePath)) {
        return candidatePath;
      }
      return {};
    }();
    if (!preferredDirectToAosSamePathHelperCandidate.empty()) {
      return preferredDirectToAosSamePathHelperCandidate;
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
