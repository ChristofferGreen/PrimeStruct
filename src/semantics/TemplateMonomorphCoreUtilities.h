#pragma once

bool isNonTypeTransformName(const std::string &name) {
  return name == "return" || name == "effects" || name == "capabilities" || name == "type" ||
         name == "mut" || name == "copy" ||
         name == "restrict" || name == "align_bytes" || name == "align_kbytes" || name == "on_error" ||
         name == "struct" || name == "enum" || name == "unsafe" ||
         name == "pod" || name == "handle" || name == "gpu_lane" || name == "no_padding" ||
         name == "platform_independent_padding" || name == "public" || name == "private" ||
         name == "static" || name == "single_type_to_return" || name == "stack" || name == "heap" || name == "buffer" ||
         name == "Additive" || name == "Multiplicative" || name == "Comparable" || name == "Indexable";
}

bool isBuiltinTemplateContainer(const std::string &name) {
  return name == "array" || name == "vector" || name == "soa" || name == "map" || name == "Result" ||
         name == "File" || isBuiltinTemplateTypeName(name);
}

std::string templateMonomorphSoaReceiverTypeName() {
  return internalSoaCollectionTypeName();
}

bool isTemplateMonomorphSoaReceiverType(std::string_view receiverTypeName) {
  return receiverTypeName == templateMonomorphSoaReceiverTypeName();
}

std::string templateMonomorphSamePathSoaHelperPrefix(bool leadingSlash = true) {
  std::string prefix = samePathSoaHelperTargetPath("");
  if (!leadingSlash && !prefix.empty() && prefix.front() == '/') {
    prefix.erase(prefix.begin());
  }
  return prefix;
}

std::string templateMonomorphCompatibilitySoaHelperPrefix(bool leadingSlash = true) {
  std::string prefix = compatibilitySoaHelperTargetPath("");
  prefix += "/";
  if (!leadingSlash && !prefix.empty() && prefix.front() == '/') {
    prefix.erase(prefix.begin());
  }
  return prefix;
}

std::string templateMonomorphPublicSoaHelperPrefix(bool leadingSlash = true) {
  std::string prefix = publicSoaHelperTargetPath("");
  prefix += "/";
  if (!leadingSlash && !prefix.empty() && prefix.front() == '/') {
    prefix.erase(prefix.begin());
  }
  return prefix;
}

std::string templateMonomorphExperimentalSoaHelperPrefix() {
  std::string prefix = experimentalSoaStorageTypePath(true);
  const size_t lastSlash = prefix.find_last_of('/');
  if (lastSlash != std::string::npos) {
    prefix.erase(lastSlash + 1);
  }
  return prefix;
}

std::string templateMonomorphSoaToAosHelperName(bool borrowed = false) {
  std::string name = "to";
  name += "_";
  name += "aos";
  if (borrowed) {
    name += "_ref";
  }
  return name;
}

std::string templateMonomorphSoaToSoaHelperName() {
  std::string name = "to";
  name += "_";
  name += "soa";
  return name;
}

bool stripTemplateMonomorphSoaHelperPrefix(std::string_view path,
                                           std::string &helperNameOut,
                                           bool leadingSlash = true) {
  const std::string samePathPrefix =
      templateMonomorphSamePathSoaHelperPrefix(leadingSlash);
  if (path.rfind(samePathPrefix, 0) == 0) {
    helperNameOut = std::string(path.substr(samePathPrefix.size()));
    return true;
  }
  const std::string compatibilityPrefix =
      templateMonomorphCompatibilitySoaHelperPrefix(leadingSlash);
  if (path.rfind(compatibilityPrefix, 0) == 0) {
    helperNameOut = std::string(path.substr(compatibilityPrefix.size()));
    return true;
  }
  const std::string publicPrefix =
      templateMonomorphPublicSoaHelperPrefix(leadingSlash);
  if (path.rfind(publicPrefix, 0) == 0) {
    helperNameOut = std::string(path.substr(publicPrefix.size()));
    return true;
  }
  return false;
}

bool isTemplateMonomorphMapImportAlias(std::string_view name) {
  const primec::StdlibSurfaceMetadata *metadata =
      keyValueConstructorSurfaceMetadataLocal();
  if (metadata == nullptr) {
    return false;
  }
  const std::string normalizedName = std::string(trimLeadingSlash(name));
  for (std::string_view alias : metadata->importAliasSpellings) {
    if (normalizedName == trimLeadingSlash(alias)) {
      return true;
    }
  }
  return false;
}

bool isTemplateMonomorphMapConstructorCallPath(std::string_view path) {
  const primec::StdlibSurfaceMetadata *metadata =
      keyValueConstructorSurfaceMetadataLocal();
  if (metadata == nullptr) {
    return false;
  }
  const std::string normalizedPath =
      stripCollectionConstructorSuffixes(std::string(path));
  std::string memberName;
  if (resolveKeyValueConstructorMemberPath(normalizedPath, memberName) &&
      memberName == "map") {
    return true;
  }
  const std::string unrootedPath = std::string(trimLeadingSlash(normalizedPath));
  for (std::string_view alias : metadata->importAliasSpellings) {
    if (unrootedPath == trimLeadingSlash(alias)) {
      return true;
    }
  }
  return false;
}

bool isTemplateMonomorphMapEntryConstructorPath(std::string path) {
  const size_t generatedSuffix = path.find("__");
  if (generatedSuffix != std::string::npos) {
    path.erase(generatedSuffix);
  }
  return metadataBackedKeyValueHelperMethodName(path) == "entry" ||
         isExperimentalCollectionConstructorPathLocal(path, "map", "entry");
}

std::string normalizeBuiltinCollectionTemplateBase(const std::string &name) {
  if (name == "array" || name == "/array") {
    return "array";
  }
  if (name == "vector" || name == "/vector" ||
      trimLeadingSlash(name) ==
          trimLeadingSlash(canonicalVectorCompatibilityPrefixOrFallback())) {
    return "vector";
  }
  if (name == "soa" || name == "/soa" ||
      name == publicSoaHelperTargetPath("") ||
      name == trimLeadingSlash(publicSoaHelperTargetPath(""))) {
    return templateMonomorphSoaReceiverTypeName();
  }
  if (isTemplateMonomorphMapImportAlias(name)) {
    return "map";
  }
  return {};
}

bool isBuiltinCollectionTemplateBase(const std::string &name, size_t argumentCount) {
  const std::string normalized = normalizeBuiltinCollectionTemplateBase(name);
  if (normalized.empty()) {
    return false;
  }
  if ((normalized == "array" || normalized == "vector" ||
       isTemplateMonomorphSoaReceiverType(normalized)) &&
      argumentCount == 1) {
    return true;
  }
  return normalized == "map" && argumentCount == 2;
}

bool importPathCoversTarget(const std::string &importPath, const std::string &targetPath) {
  if (importPath.empty() || importPath.front() != '/') {
    return false;
  }
  if (importPath == targetPath) {
    return true;
  }
  if (importPath == canonicalVectorCompatibilityPrefixOrFallback() ||
      isTemplateMonomorphMapImportAlias(importPath) ||
      importPath == publicSoaHelperTargetPath("")) {
    return targetPath.rfind(importPath + "/", 0) == 0;
  }
  if (importPath.size() >= 2 && importPath.compare(importPath.size() - 2, 2, "/*") == 0) {
    const std::string prefix = importPath.substr(0, importPath.size() - 2);
    return targetPath == prefix || targetPath.rfind(prefix + "/", 0) == 0;
  }
  return false;
}

std::string helperOverloadInternalPath(const std::string &publicPath, size_t parameterCount) {
  return publicPath + "__ov" + std::to_string(parameterCount);
}

std::string helperOverloadDefinitionKey(const Definition &def) {
  return def.fullPath + "#" + std::to_string(def.sourceLine) + ":" +
         std::to_string(def.sourceColumn) + ":" +
         std::to_string(def.parameters.size());
}

bool definitionHasRequireTransform(const Definition &def) {
  for (const auto &transform : def.transforms) {
    if (transform.name == "require") {
      return true;
    }
  }
  return false;
}

std::string genericTypeOverloadInternalPath(const std::string &publicPath,
                                            size_t templateParameterCount) {
  return publicPath + "__arity" + std::to_string(templateParameterCount);
}

std::string helperOverloadDisplayPath(const std::string &path, const Context &ctx) {
  auto directIt = ctx.helperOverloadInternalToPublic.find(path);
  if (directIt != ctx.helperOverloadInternalToPublic.end()) {
    return directIt->second;
  }
  auto typeDirectIt = ctx.genericTypeOverloadInternalToPublic.find(path);
  if (typeDirectIt != ctx.genericTypeOverloadInternalToPublic.end()) {
    return typeDirectIt->second;
  }
  for (const auto &[internalPath, publicPath] : ctx.helperOverloadInternalToPublic) {
    if (path.rfind(internalPath + "__t", 0) == 0) {
      return publicPath;
    }
  }
  for (const auto &[internalPath, publicPath] : ctx.genericTypeOverloadInternalToPublic) {
    if (path.rfind(internalPath + "__t", 0) == 0) {
      return publicPath;
    }
  }
  return path;
}

bool selectGenericTypeOverloadPath(const std::string &resolvedPath,
                                   size_t templateArgumentCount,
                                   const Context &ctx,
                                   std::string &pathOut,
                                   std::string &error) {
  pathOut = resolvedPath;
  auto familyIt = ctx.genericTypeOverloads.find(resolvedPath);
  if (familyIt == ctx.genericTypeOverloads.end()) {
    return true;
  }
  for (const auto &entry : familyIt->second) {
    if (entry.templateParameterCount == templateArgumentCount) {
      pathOut = entry.internalPath;
      return true;
    }
  }
  std::ostringstream expected;
  bool firstExpected = true;
  for (const auto &entry : familyIt->second) {
    if (!firstExpected) {
      expected << " or ";
    }
    firstExpected = false;
    expected << entry.templateParameterCount;
  }
  error = "template argument count mismatch for " + resolvedPath +
          ": expected " + expected.str() + ", got " +
          std::to_string(templateArgumentCount);
  return false;
}

std::string bindingTypeTextForRequirementOverloadSelection(const BindingInfo &binding) {
  if (binding.typeTemplateArg.empty()) {
    return binding.typeName;
  }
  return binding.typeName + "<" + binding.typeTemplateArg + ">";
}

std::optional<std::string>
requirementOverloadArgumentTypeText(const Expr &arg,
                                    const LocalTypeMap *locals,
                                    const std::vector<ParameterInfo> *params) {
  switch (arg.kind) {
  case Expr::Kind::Literal:
    if (arg.isUnsigned) {
      return arg.intWidth == 64 ? std::optional<std::string>("u64")
                                : std::optional<std::string>("u32");
    }
    return arg.intWidth == 64 ? std::optional<std::string>("i64")
                              : std::optional<std::string>("i32");
  case Expr::Kind::BoolLiteral:
    return std::string("bool");
  case Expr::Kind::FloatLiteral:
    return arg.floatWidth == 64 ? std::optional<std::string>("f64")
                                : std::optional<std::string>("f32");
  case Expr::Kind::StringLiteral:
    return std::string("string");
  case Expr::Kind::Name:
    if (locals != nullptr) {
      auto localIt = locals->find(arg.name);
      if (localIt != locals->end()) {
        return bindingTypeTextForRequirementOverloadSelection(localIt->second);
      }
    }
    if (params != nullptr) {
      for (const auto &param : *params) {
        if (param.name == arg.name) {
          return bindingTypeTextForRequirementOverloadSelection(param.binding);
        }
      }
    }
    return std::nullopt;
  case Expr::Kind::Call:
    return std::nullopt;
  }
  return std::nullopt;
}

std::string requirementOverloadSourceLocation(int line, int column) {
  if (line <= 0) {
    return "unknown";
  }
  return std::to_string(line) + ":" + std::to_string(column > 0 ? column : 1);
}

std::string requirementOverloadExprDisplayText(const Expr &expr) {
  switch (expr.kind) {
  case Expr::Kind::Literal: {
    std::string suffix;
    if (expr.isUnsigned) {
      suffix = expr.intWidth == 64 ? "u64" : "u32";
    } else {
      suffix = expr.intWidth == 64 ? "i64" : "i32";
    }
    return std::to_string(expr.literalValue) + suffix;
  }
  case Expr::Kind::BoolLiteral:
    return expr.boolValue ? "true" : "false";
  case Expr::Kind::FloatLiteral:
    return expr.floatValue + (expr.floatWidth == 64 ? "f64" : "f32");
  case Expr::Kind::StringLiteral:
    return "\"" + expr.stringValue + "\"";
  case Expr::Kind::Name:
    return expr.name.empty() ? "<anonymous>" : expr.name;
  case Expr::Kind::Call:
    return (expr.sourceName.empty() ? expr.name : expr.sourceName) + "(...)";
  }
  return "<expr>";
}

std::string formatRequirementOverloadArgumentFacts(
    const Expr &expr,
    const LocalTypeMap *locals,
    const std::vector<ParameterInfo> *params) {
  std::ostringstream out;
  out << "concrete inferred argument facts:";
  if (expr.args.empty()) {
    out << "\n- none";
    return out.str();
  }
  for (std::size_t i = 0; i < expr.args.size(); ++i) {
    const Expr &arg = expr.args[i];
    const std::optional<std::string> typeText =
        requirementOverloadArgumentTypeText(arg, locals, params);
    out << "\n- arg" << i << " type="
        << (typeText.has_value() ? *typeText : std::string("unknown"))
        << " expr=" << requirementOverloadExprDisplayText(arg)
        << " at "
        << requirementOverloadSourceLocation(arg.sourceLine,
                                             arg.sourceColumn);
  }
  return out.str();
}

std::string formatRequirementOverloadCallSite(const Expr &expr,
                                              const std::string &resolvedPath) {
  const std::string callee =
      expr.sourceName.empty() ? (expr.name.empty() ? resolvedPath : expr.name)
                              : expr.sourceName;
  return "call site: " + callee + " at " +
         requirementOverloadSourceLocation(expr.sourceLine, expr.sourceColumn);
}

std::string formatRequirementOverloadPredicateSummary(
    const RequirementPredicateFactDraft &fact,
    std::string_view originalArgument,
    int sourceLine,
    int sourceColumn) {
  const std::string predicate =
      fact.predicateName.empty() ? std::string(originalArgument)
                                 : fact.predicateName;
  std::ostringstream out;
  out << predicate << " at "
      << requirementOverloadSourceLocation(sourceLine, sourceColumn) << ": "
      << fact.evaluationDiagnostic;
  if (!fact.sourceText.empty()) {
    out << " (source: " << fact.sourceText << ")";
  }
  return out.str();
}

struct RequirementOverloadViability {
  bool viable = true;
  bool decisive = false;
  std::vector<std::string> diagnostics;
  std::vector<std::string> satisfiedDiagnostics;
};

RequirementOverloadViability evaluateRequirementOverloadViability(
    const Definition &def,
    const Expr &expr,
    const Context &ctx,
    const LocalTypeMap *locals,
    const std::vector<ParameterInfo> *params) {
  RequirementOverloadViability result;
  if (!definitionHasRequireTransform(def)) {
    result.decisive = true;
    return result;
  }
  RequirementPredicateDefinitionContext requirementContext;
  requirementContext.definitionPath = def.fullPath;
  requirementContext.namespacePrefix = def.namespacePrefix;
  requirementContext.templateArgs = def.templateArgs;
  requirementContext.importAliases = ctx.importAliases;

  for (std::size_t i = 0; i < def.parameters.size(); ++i) {
    ParameterInfo param;
    param.name = def.parameters[i].name;
    extractExplicitBindingType(def.parameters[i], param.binding);
    if (i < expr.args.size()) {
      const std::optional<std::string> argType =
          requirementOverloadArgumentTypeText(expr.args[i], locals, params);
      if (argType.has_value() &&
          std::find(def.templateArgs.begin(),
                    def.templateArgs.end(),
                    param.binding.typeName) != def.templateArgs.end()) {
        param.binding.typeName = *argType;
        param.binding.typeTemplateArg.clear();
      }
    }
    requirementContext.params.push_back(std::move(param));
  }

  for (const auto &transform : def.transforms) {
    if (transform.name != "require") {
      continue;
    }
    const int sourceLine = transform.sourceLine > 0 ? transform.sourceLine
                                                    : def.sourceLine;
    const int sourceColumn = transform.sourceColumn > 0 ? transform.sourceColumn
                                                        : def.sourceColumn;
    for (const auto &argument : transform.arguments) {
      RequirementPredicateFactDraft fact =
          buildRequirementPredicateFactDraft(argument,
                                             sourceLine,
                                             sourceColumn,
                                             requirementContext);
      if (fact.evaluationDiagnostic.find("deferred for unresolved") !=
          std::string::npos) {
        continue;
      }
      result.decisive = true;
      if (fact.evaluationOutcome == "satisfied") {
        result.satisfiedDiagnostics.push_back(
            formatRequirementOverloadPredicateSummary(fact,
                                                      argument,
                                                      sourceLine,
                                                      sourceColumn));
        continue;
      }
      result.viable = false;
      result.diagnostics.push_back(
          formatRequirementOverloadPredicateSummary(fact,
                                                    argument,
                                                    sourceLine,
                                                    sourceColumn));
    }
  }
  return result;
}

std::string formatRequirementOverloadCandidateSummary(
    const HelperOverloadEntry &entry,
    const Definition *def,
    const Context &ctx,
    const RequirementOverloadViability *viability) {
  std::ostringstream out;
  out << helperOverloadDisplayPath(entry.internalPath, ctx);
  if (def != nullptr) {
    out << " at "
        << requirementOverloadSourceLocation(def->sourceLine,
                                             def->sourceColumn);
  }

  if (viability != nullptr && !viability->diagnostics.empty()) {
    out << "\n  rejected by:";
    for (const std::string &diagnostic : viability->diagnostics) {
      out << "\n  - " << diagnostic;
    }
  } else if (viability != nullptr &&
             !viability->satisfiedDiagnostics.empty()) {
    out << "\n  requirements satisfied:";
    for (const std::string &diagnostic : viability->satisfiedDiagnostics) {
      out << "\n  - " << diagnostic;
    }
  } else {
    out << "\n  requirements: none or deferred";
  }
  return out.str();
}

std::string selectRequirementAwareHelperOverloadPath(
    const Expr &expr,
    const std::string &resolvedPath,
    const Context &ctx,
    const std::vector<const HelperOverloadEntry *> &candidates,
    const LocalTypeMap *locals,
    const std::vector<ParameterInfo> *params) {
  if (candidates.size() <= 1) {
    return candidates.empty() ? resolvedPath : candidates.front()->internalPath;
  }
  bool hasRequirementCandidate = false;
  for (const auto *entry : candidates) {
    if (entry != nullptr && entry->hasRequirementTransform) {
      hasRequirementCandidate = true;
      break;
    }
  }
  if (!hasRequirementCandidate) {
    return candidates.front()->internalPath;
  }

  std::vector<const HelperOverloadEntry *> viable;
  std::vector<std::string> rejected;
  std::vector<std::string> viableSummaries;
  for (const auto *entry : candidates) {
    if (entry == nullptr) {
      continue;
    }
    auto defIt = ctx.sourceDefs.find(entry->internalPath);
    if (defIt == ctx.sourceDefs.end()) {
      viable.push_back(entry);
      viableSummaries.push_back(
          formatRequirementOverloadCandidateSummary(*entry,
                                                    nullptr,
                                                    ctx,
                                                    nullptr));
      continue;
    }
    RequirementOverloadViability viability =
        evaluateRequirementOverloadViability(defIt->second,
                                             expr,
                                             ctx,
                                             locals,
                                             params);
    if (viability.viable) {
      viable.push_back(entry);
      viableSummaries.push_back(formatRequirementOverloadCandidateSummary(
          *entry, &defIt->second, ctx, &viability));
      continue;
    }
    rejected.push_back(formatRequirementOverloadCandidateSummary(
        *entry, &defIt->second, ctx, &viability));
  }

  if (viable.size() == 1) {
    return viable.front()->internalPath;
  }
  std::sort(rejected.begin(), rejected.end());
  std::sort(viableSummaries.begin(), viableSummaries.end());
  std::ostringstream error;
  error << formatRequirementOverloadCallSite(expr, resolvedPath) << '\n';
  if (viable.empty()) {
    error << "no viable requirement overload for " << resolvedPath << '\n';
    if (!rejected.empty()) {
      error << "rejected candidates:";
      for (const std::string &candidate : rejected) {
        error << "\n- " << candidate;
      }
      error << '\n';
    }
    error << formatRequirementOverloadArgumentFacts(expr, locals, params)
          << '\n';
    error << "hint: pass values or types that satisfy exactly one "
             "constrained overload, or call a more specific helper name.";
  } else {
    error << "ambiguous requirement overload for " << resolvedPath << '\n';
    error << "viable candidates:";
    for (const std::string &candidate : viableSummaries) {
      error << "\n- " << candidate;
    }
    error << '\n'
          << formatRequirementOverloadArgumentFacts(expr, locals, params)
          << '\n';
    error << "hint: make the overload requirements mutually exclusive, or "
             "call a clearer helper name.";
  }
  ctx.requirementOverloadSelectionError = error.str();
  return resolvedPath;
}

std::string selectHelperOverloadPath(const Expr &expr,
                                     const std::string &resolvedPath,
                                     const Context &ctx,
                                     const LocalTypeMap *locals = nullptr,
                                     const std::vector<ParameterInfo> *params = nullptr) {
  ctx.requirementOverloadSelectionError.clear();
  auto familyIt = ctx.helperOverloads.find(resolvedPath);
  if (familyIt == ctx.helperOverloads.end()) {
    return resolvedPath;
  }
  auto explicitCallPath = [](const Expr &callExpr) -> std::string {
    if (callExpr.kind != Expr::Kind::Call || callExpr.isMethodCall || callExpr.name.empty()) {
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
  auto isKeyValueEntryConstructorExpr = [&](const Expr &argExpr) {
    if (argExpr.kind != Expr::Kind::Call || argExpr.isMethodCall) {
      return false;
    }
    return isTemplateMonomorphMapEntryConstructorPath(explicitCallPath(argExpr));
  };
  const bool preferKeyValueEntryArgsPackOverload =
      !expr.isMethodCall &&
      isTemplateMonomorphMapConstructorCallPath(resolvedPath) &&
      !expr.args.empty() &&
      ([&]() {
        for (const Expr &argExpr : expr.args) {
          if (!isKeyValueEntryConstructorExpr(argExpr)) {
            return false;
          }
        }
        return true;
      })();
  if (preferKeyValueEntryArgsPackOverload) {
    std::vector<const HelperOverloadEntry *> candidates;
    for (const auto &entry : familyIt->second) {
      if (entry.parameterCount == 1) {
        candidates.push_back(&entry);
      }
    }
    if (!candidates.empty()) {
      return selectRequirementAwareHelperOverloadPath(
          expr, resolvedPath, ctx, candidates, locals, params);
    }
  }
  std::vector<size_t> candidateCounts;
  if (expr.isMethodCall) {
    // Most method-call paths still carry the receiver explicitly in expr.args,
    // but some monomorphized internal helper paths have already peeled it
    // away. Prefer the raw call shape first and fall back to the self-counted
    // arity for those stripped-receiver paths.
    candidateCounts.push_back(expr.args.size());
    candidateCounts.push_back(expr.args.size() + 1);
  } else {
    candidateCounts.push_back(expr.args.size());
  }
  for (size_t candidateCount : candidateCounts) {
    std::vector<const HelperOverloadEntry *> candidates;
    for (const auto &entry : familyIt->second) {
      if (entry.parameterCount == candidateCount) {
        candidates.push_back(&entry);
      }
    }
    if (!candidates.empty()) {
      return selectRequirementAwareHelperOverloadPath(
          expr, resolvedPath, ctx, candidates, locals, params);
    }
  }
  for (size_t candidateCount : candidateCounts) {
    std::vector<const HelperOverloadEntry *> candidates;
    for (const auto &entry : familyIt->second) {
      if (entry.isVariadic && candidateCount >= entry.variadicMinArgumentCount) {
        candidates.push_back(&entry);
      }
    }
    if (!candidates.empty()) {
      return selectRequirementAwareHelperOverloadPath(
          expr, resolvedPath, ctx, candidates, locals, params);
    }
  }
  return resolvedPath;
}

bool resolveHelperOverloadDefinitionIdentity(const Definition &def,
                                             const Context &ctx,
                                             std::string &internalPathOut,
                                             std::string &nameOut) {
  internalPathOut = def.fullPath;
  nameOut = def.name;
  auto familyIt = ctx.helperOverloads.find(def.fullPath);
  if (familyIt == ctx.helperOverloads.end()) {
    return false;
  }
  const std::string sourceKey = helperOverloadDefinitionKey(def);
  auto identityIt = ctx.helperOverloadDefinitionIdentity.find(sourceKey);
  if (identityIt != ctx.helperOverloadDefinitionIdentity.end()) {
    internalPathOut = identityIt->second;
    const size_t slash = internalPathOut.find_last_of('/');
    nameOut = slash == std::string::npos ? internalPathOut : internalPathOut.substr(slash + 1);
    return true;
  }
  const size_t parameterCount = def.parameters.size();
  for (const auto &entry : familyIt->second) {
    if (entry.parameterCount != parameterCount) {
      continue;
    }
    internalPathOut = entry.internalPath;
    const size_t slash = internalPathOut.find_last_of('/');
    nameOut = slash == std::string::npos ? internalPathOut : internalPathOut.substr(slash + 1);
    return true;
  }
  return false;
}

bool resolveGenericTypeOverloadDefinitionIdentity(const Definition &def,
                                                  const Context &ctx,
                                                  std::string &internalPathOut,
                                                  std::string &nameOut) {
  internalPathOut = def.fullPath;
  nameOut = def.name;
  auto familyIt = ctx.genericTypeOverloads.find(def.fullPath);
  if (familyIt == ctx.genericTypeOverloads.end()) {
    return false;
  }
  const size_t templateParameterCount = def.templateArgs.size();
  for (const auto &entry : familyIt->second) {
    if (entry.templateParameterCount != templateParameterCount) {
      continue;
    }
    internalPathOut = entry.internalPath;
    const size_t slash = internalPathOut.find_last_of('/');
    nameOut = slash == std::string::npos ? internalPathOut : internalPathOut.substr(slash + 1);
    return true;
  }
  return false;
}

std::string trimWhitespace(const std::string &text) {
  size_t start = 0;
  while (start < text.size() && std::isspace(static_cast<unsigned char>(text[start]))) {
    ++start;
  }
  size_t end = text.size();
  while (end > start && std::isspace(static_cast<unsigned char>(text[end - 1]))) {
    --end;
  }
  return text.substr(start, end - start);
}

std::string stripWhitespace(const std::string &text) {
  std::string out;
  out.reserve(text.size());
  for (char c : text) {
    if (!std::isspace(static_cast<unsigned char>(c))) {
      out.push_back(c);
    }
  }
  return out;
}
