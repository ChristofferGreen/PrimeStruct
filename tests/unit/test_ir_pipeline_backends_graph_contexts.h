TEST_CASE("semantics validator rebuilds base contexts behind explicit validation state") {
  const std::filesystem::path cwd = std::filesystem::current_path();
  std::filesystem::path headerPath = cwd / "src" / "semantics" / "SemanticsValidator.h";
  std::filesystem::path privateStatementsPath =
      cwd / "src" / "semantics" / "SemanticsValidatorPrivateStatements.h";
  std::filesystem::path sourcePath = cwd / "src" / "semantics" / "SemanticsValidator.cpp";
  std::filesystem::path buildPath = cwd / "src" / "semantics" / "SemanticsValidatorBuild.cpp";
  std::filesystem::path validationContextPath =
      cwd / "src" / "semantics" / "SemanticsValidatorValidationContext.cpp";
  if (!std::filesystem::exists(headerPath)) {
    headerPath = cwd.parent_path() / "src" / "semantics" / "SemanticsValidator.h";
    privateStatementsPath =
        cwd.parent_path() / "src" / "semantics" / "SemanticsValidatorPrivateStatements.h";
    sourcePath = cwd.parent_path() / "src" / "semantics" / "SemanticsValidator.cpp";
    buildPath = cwd.parent_path() / "src" / "semantics" / "SemanticsValidatorBuild.cpp";
    validationContextPath =
        cwd.parent_path() / "src" / "semantics" / "SemanticsValidatorValidationContext.cpp";
  }
  REQUIRE(std::filesystem::exists(headerPath));
  REQUIRE(std::filesystem::exists(privateStatementsPath));
  REQUIRE(std::filesystem::exists(sourcePath));
  REQUIRE(std::filesystem::exists(buildPath));
  REQUIRE(std::filesystem::exists(validationContextPath));

  const std::string header = readTextFile(headerPath);
  const std::string privateStatements = readTextFile(privateStatementsPath);
  const std::string source = readTextFile(sourcePath);
  const std::string build = readTextFile(buildPath);
  const std::string validationContext = readTextFile(validationContextPath);
  CHECK(header.find("std::unordered_map<std::string, ValidationContext> definitionValidationContexts_") ==
        std::string::npos);
  CHECK(header.find("std::unordered_map<std::string, ValidationContext> executionValidationContexts_") ==
        std::string::npos);
  CHECK(privateStatements.find("struct ValidationState {") != std::string::npos);
  CHECK(header.find("ValidationState currentValidationState_") != std::string::npos);
  CHECK(header.find("std::unordered_set<std::string> activeEffects_;") == std::string::npos);
  CHECK(header.find("std::unordered_set<std::string> movedBindings_;") == std::string::npos);
  CHECK(header.find("std::unordered_set<std::string> endedReferenceBorrows_;") == std::string::npos);
  CHECK(header.find("std::string currentDefinitionPath_;") == std::string::npos);
  CHECK(build.find("definitionValidationContexts_.try_emplace(def.fullPath, std::move(context));") ==
        std::string::npos);
  CHECK(build.find("executionValidationContexts_.try_emplace(exec.fullPath, makeExecutionValidationContext(exec));") ==
        std::string::npos);
  CHECK(build.find("if (!makeDefinitionValidationContext(def, context)) {") != std::string::npos);
  CHECK(validationContext.find("bool SemanticsValidator::makeDefinitionValidationState(const Definition &def, ValidationState &out) {") !=
        std::string::npos);
  CHECK(validationContext.find("ValidationState state;") != std::string::npos);
  CHECK(validationContext.find("return makeExecutionValidationState(exec);") != std::string::npos);
  CHECK(validationContext.find("if (!const_cast<SemanticsValidator *>(this)->makeDefinitionValidationContext(def, context)) {") !=
        std::string::npos);
  CHECK(validationContext.find("return makeExecutionValidationContext(exec);") != std::string::npos);
  CHECK(validationContext.find("definitionValidationContexts_.find(def.fullPath)") == std::string::npos);
  CHECK(validationContext.find("executionValidationContexts_.find(exec.fullPath)") == std::string::npos);
  CHECK(source.find("snapshotValidationContext()") == std::string::npos);
  CHECK(source.find("restoreValidationContext(") == std::string::npos);
}

TEST_CASE("call target resolution reuses scoped scratch cache") {
  const std::filesystem::path cwd = std::filesystem::current_path();
  std::filesystem::path headerPath = cwd / "src" / "semantics" / "SemanticsValidator.h";
  std::filesystem::path sourcePath = cwd / "src" / "semantics" / "SemanticsValidatorBuildCallResolution.cpp";
  std::filesystem::path concreteSourcePath =
      cwd / "src" / "semantics" / "SemanticsValidatorExprCallResolution.cpp";
  if (!std::filesystem::exists(headerPath)) {
    headerPath = cwd.parent_path() / "src" / "semantics" / "SemanticsValidator.h";
    sourcePath = cwd.parent_path() / "src" / "semantics" / "SemanticsValidatorBuildCallResolution.cpp";
    concreteSourcePath =
        cwd.parent_path() / "src" / "semantics" / "SemanticsValidatorExprCallResolution.cpp";
  }
  REQUIRE(std::filesystem::exists(headerPath));
  REQUIRE(std::filesystem::exists(sourcePath));
  REQUIRE(std::filesystem::exists(concreteSourcePath));

  const std::string header = readTextFile(headerPath);
  const std::string source = readTextFile(sourcePath);
  const std::string concreteSource = readTextFile(concreteSourcePath);
  CHECK(header.find("struct CallTargetResolutionScratch {") != std::string::npos);
  CHECK(header.find("using PmrBoolMap = std::pmr::unordered_map<SymbolId, bool>;") != std::string::npos);
  CHECK(header.find("PmrBoolMap definitionFamilyPathCache{&arenaResource};") != std::string::npos);
  CHECK(header.find("PmrBoolMap overloadFamilyPathCache{&arenaResource};") != std::string::npos);
  CHECK(header.find("using PmrIndexStringMap = std::pmr::unordered_map<SymbolIndexKey, std::string, SymbolIndexKeyHash>;") !=
        std::string::npos);
  CHECK(header.find("using PmrStringVector = std::pmr::vector<std::string>;") != std::string::npos);
  CHECK(header.find("PmrSymbolStringMap overloadFamilyPrefixCache{&arenaResource};") !=
        std::string::npos);
  CHECK(header.find("PmrSymbolStringMap specializationPrefixCache{&arenaResource};") !=
        std::string::npos);
  CHECK(header.find("PmrIndexStringMap overloadCandidatePathCache{&arenaResource};") !=
        std::string::npos);
  CHECK(header.find("PmrStringVector concreteCallBaseCandidates{&arenaResource};") != std::string::npos);
  CHECK(header.find("PmrSymbolStringMap rootedCallNamePathCache{&arenaResource};") !=
        std::string::npos);
  CHECK(header.find("PmrSymbolStringMap normalizedNamespacePrefixCache{&arenaResource};") !=
        std::string::npos);
  CHECK(header.find("PmrPairStringMap joinedCallPathCache{&arenaResource};") !=
        std::string::npos);
  CHECK(header.find("mutable CallTargetResolutionScratch callTargetResolutionScratch_;") != std::string::npos);
  CHECK(source.find("const bool hasScopedOwner = activeDefinition != nullptr || activeExecution != nullptr;") !=
        std::string::npos);
  CHECK(header.find("void resetArena() {") != std::string::npos);
  CHECK(source.find("callTargetResolutionScratch_.resetArena();") != std::string::npos);
  CHECK(source.find("callTargetResolutionScratch_.rootedCallNamePathCache.find(key)") !=
        std::string::npos);
  CHECK(source.find("callTargetResolutionScratch_.normalizedNamespacePrefixCache.find(key)") !=
        std::string::npos);
  CHECK(source.find("callTargetResolutionScratch_.joinedCallPathCache.find(key)") !=
        std::string::npos);
  CHECK(source.find("callTargetResolutionScratch_.definitionFamilyPathCache.emplace(pathKey, hasPath);") !=
        std::string::npos);
  CHECK(concreteSource.find("callTargetResolutionScratch_.overloadFamilyPathCache.find(pathKey)") !=
        std::string::npos);
  CHECK(concreteSource.find("callTargetResolutionScratch_.overloadFamilyPathCache.emplace(pathKey, hasOverloads);") !=
        std::string::npos);
  CHECK(concreteSource.find("callTargetResolutionScratch_.overloadFamilyPrefixCache.find(pathKey)") !=
        std::string::npos);
  CHECK(concreteSource.find("callTargetResolutionScratch_.specializationPrefixCache.find(basePathKey)") !=
        std::string::npos);
  CHECK(concreteSource.find("callTargetResolutionScratch_.overloadCandidatePathCache.find(key)") !=
        std::string::npos);
  CHECK(concreteSource.find("appendIfMissing(baseCandidates, overloadCandidatePath(candidatePath,") !=
        std::string::npos);
  CHECK(concreteSource.find("auto &baseCandidates = callTargetResolutionScratch_.concreteCallBaseCandidates;") !=
        std::string::npos);
}

TEST_CASE("method target resolution reuses scoped scratch cache") {
  const std::filesystem::path cwd = std::filesystem::current_path();
  std::filesystem::path headerPath = cwd / "src" / "semantics" / "SemanticsValidator.h";
  std::filesystem::path sourcePath = cwd / "src" / "semantics" / "SemanticsValidatorInferMethodResolution.cpp";
  if (!std::filesystem::exists(headerPath)) {
    headerPath = cwd.parent_path() / "src" / "semantics" / "SemanticsValidator.h";
    sourcePath = cwd.parent_path() / "src" / "semantics" / "SemanticsValidatorInferMethodResolution.cpp";
  }
  REQUIRE(std::filesystem::exists(headerPath));
  REQUIRE(std::filesystem::exists(sourcePath));

  const std::string header = readTextFile(headerPath);
  const std::string source = readTextFile(sourcePath);
  CHECK(header.find("PmrSymbolStringMap normalizedMethodNameCache{&arenaResource};") !=
        std::string::npos);
  CHECK(header.find("PmrPairStringMap explicitRemovedMethodPathCache{&arenaResource};") !=
        std::string::npos);
  CHECK(header.find("PmrMethodMemoMap methodTargetMemoCache{&arenaResource};") !=
        std::string::npos);
  CHECK(header.find("PmrStringVector methodReceiverResolutionCandidates{&arenaResource};") !=
        std::string::npos);
  CHECK(header.find("PmrSymbolStringMap canonicalReceiverAliasPathCache{&arenaResource};") !=
        std::string::npos);
  CHECK(header.find("bool methodTargetMemoizationEnabled_ = true;") !=
        std::string::npos);
  CHECK(header.find("void resetArena() {") != std::string::npos);
  CHECK(source.find("callTargetResolutionScratch_.resetArena();") != std::string::npos);
  CHECK(source.find("lookupNormalizedMethodName(methodName, normalizedMethodName)") !=
        std::string::npos);
  CHECK(source.find("callTargetResolutionScratch_.normalizedMethodNameCache.emplace(rawMethodNameKey,") !=
        std::string::npos);
  CHECK(source.find("callTargetResolutionScratch_.rootedCallNamePathCache.find(key)") !=
        std::string::npos);
  CHECK(source.find("callTargetResolutionScratch_.joinedCallPathCache.find(joinKey)") !=
        std::string::npos);
  CHECK(source.find("appendCanonicalReceiverResolutionCandidates") != std::string::npos);
  CHECK(source.find("preferVectorStdlibHelperPath(\"/std/collections/vector/\" + normalizedMethodName)") !=
        std::string::npos);
  CHECK(source.find("return joinMethodTarget(\"/std/collections/map\", helperSuffix);") !=
        std::string::npos);
  CHECK(source.find("return joinMethodTarget(\"/map\", helperSuffix);") !=
        std::string::npos);
  CHECK(source.find("callTargetResolutionScratch_.canonicalReceiverAliasPathCache.find(resolvedReceiverPathKey)") !=
        std::string::npos);
  CHECK(source.find("callTargetResolutionScratch_.canonicalReceiverAliasPathCache.emplace(") !=
        std::string::npos);
  CHECK(source.find("const std::string aliasPath = canonicalReceiverAliasPath(resolvedReceiverPath);") !=
        std::string::npos);
  CHECK(source.find("appendCanonicalReceiverResolutionCandidates(resolvedReceiverPath, appendResolvedCandidate);") !=
        std::string::npos);
  CHECK(source.find("const CallTargetResolutionScratch::SymbolPairKey cacheKey{methodNameKey, namespaceKey};") !=
        std::string::npos);
  CHECK(source.find("callTargetResolutionScratch_.explicitRemovedMethodPathCache.find(cacheKey)") !=
        std::string::npos);
  CHECK(source.find("callTargetResolutionScratch_.explicitRemovedMethodPathCache.emplace(cacheKey, explicitRemovedMethodPath);") !=
        std::string::npos);
  CHECK(source.find("return CallTargetResolutionScratch::MethodTargetMemoKey{") != std::string::npos);
  CHECK(source.find("callTargetResolutionScratch_.methodTargetMemoCache.find(*key)") !=
        std::string::npos);
  CHECK(source.find("!callTargetResolutionScratch_.methodReceiverResolutionCandidates.empty()") !=
        std::string::npos);
  CHECK(source.find("auto &resolvedCandidates = callTargetResolutionScratch_.methodReceiverResolutionCandidates;") !=
        std::string::npos);
  CHECK(source.find("if (!methodTargetMemoizationEnabled_) {") !=
        std::string::npos);
  CHECK(source.find("storeMethodTargetMemo(memoReceiverTypeText, success);") !=
        std::string::npos);
}

TEST_CASE("graph local auto facts use compact structured keys") {
  const std::filesystem::path cwd = std::filesystem::current_path();
  std::filesystem::path headerPath = cwd / "src" / "semantics" / "SemanticsValidator.h";
  std::filesystem::path graphLocalAutoHeaderPath =
      cwd / "src" / "semantics" / "SemanticsValidatorGraphLocalAuto.h";
  std::filesystem::path snapshotLocalsPath =
      cwd / "src" / "semantics" / "SemanticsValidatorSnapshotLocals.cpp";
  std::filesystem::path buildUtilityPath =
      cwd / "src" / "semantics" / "SemanticsValidatorBuildUtility.cpp";
  std::filesystem::path inferGraphPath =
      cwd / "src" / "semantics" / "SemanticsValidatorInferGraph.cpp";
  std::filesystem::path graphLocalAutoSourcePath =
      cwd / "src" / "semantics" / "SemanticsValidatorGraphLocalAuto.cpp";
  if (!std::filesystem::exists(headerPath)) {
    headerPath = cwd.parent_path() / "src" / "semantics" / "SemanticsValidator.h";
    graphLocalAutoHeaderPath =
        cwd.parent_path() / "src" / "semantics" / "SemanticsValidatorGraphLocalAuto.h";
    snapshotLocalsPath =
        cwd.parent_path() / "src" / "semantics" / "SemanticsValidatorSnapshotLocals.cpp";
    buildUtilityPath =
        cwd.parent_path() / "src" / "semantics" / "SemanticsValidatorBuildUtility.cpp";
    inferGraphPath =
        cwd.parent_path() / "src" / "semantics" / "SemanticsValidatorInferGraph.cpp";
    graphLocalAutoSourcePath =
        cwd.parent_path() / "src" / "semantics" / "SemanticsValidatorGraphLocalAuto.cpp";
  }
  REQUIRE(std::filesystem::exists(headerPath));
  REQUIRE(std::filesystem::exists(graphLocalAutoHeaderPath));
  REQUIRE(std::filesystem::exists(snapshotLocalsPath));
  REQUIRE(std::filesystem::exists(buildUtilityPath));
  REQUIRE(std::filesystem::exists(inferGraphPath));
  REQUIRE(std::filesystem::exists(graphLocalAutoSourcePath));

  const std::string header = readTextFile(headerPath);
  const std::string graphLocalAutoHeader = readTextFile(graphLocalAutoHeaderPath);
  const std::string snapshotLocals = readTextFile(snapshotLocalsPath);
  const std::string buildUtility = readTextFile(buildUtilityPath);
  const std::string inferGraph = readTextFile(inferGraphPath);
  const std::string graphLocalAutoSource = readTextFile(graphLocalAutoSourcePath);

  CHECK(graphLocalAutoHeader.find("struct GraphLocalAutoKey {") != std::string::npos);
  CHECK(graphLocalAutoHeader.find("SymbolId scopePathId = InvalidSymbolId;") !=
        std::string::npos);
  CHECK(header.find("mutable SymbolInterner graphLocalAutoScopePathInterner_;") != std::string::npos);
  CHECK(graphLocalAutoHeader.find("class GraphLocalAutoBenchmarkShadow {") !=
        std::string::npos);
  CHECK(graphLocalAutoHeader.find("struct LegacyShadowState {") != std::string::npos);
  CHECK(header.find("std::unique_ptr<GraphLocalAutoBenchmarkShadow> graphLocalAutoBenchmarkShadow_;") !=
        std::string::npos);
  CHECK(header.find("mutable std::unordered_set<std::string> graphLocalAutoLegacyKeyShadow_;") ==
        std::string::npos);
  CHECK(header.find("graphLocalAutoLegacyBindingShadow_") == std::string::npos);
  CHECK(header.find("graphLocalAutoLegacyInitializerResolvedPathShadow_") == std::string::npos);
  CHECK(header.find("graphLocalAutoLegacyInitializerBindingShadow_") == std::string::npos);
  CHECK(header.find("graphLocalAutoLegacyQuerySnapshotShadow_") == std::string::npos);
  CHECK(header.find("graphLocalAutoLegacyTryValueShadow_") == std::string::npos);
  CHECK(header.find("graphLocalAutoLegacyDirectCallPathShadow_") == std::string::npos);
  CHECK(header.find("graphLocalAutoLegacyDirectCallReturnKindShadow_") == std::string::npos);
  CHECK(header.find("graphLocalAutoLegacyMethodCallPathShadow_") == std::string::npos);
  CHECK(header.find("graphLocalAutoLegacyMethodCallReturnKindShadow_") == std::string::npos);
  CHECK(graphLocalAutoHeader.find("struct GraphLocalAutoFacts {") != std::string::npos);
  CHECK(graphLocalAutoHeader.find("bool hasBinding = false;") != std::string::npos);
  CHECK(header.find("std::unordered_map<GraphLocalAutoKey, GraphLocalAutoFacts, GraphLocalAutoKeyHash> graphLocalAutoFacts_;") !=
        std::string::npos);
  CHECK(snapshotLocals.find("const SymbolId scopePathId = graphLocalAutoScopePathInterner_.intern(scopePath);") !=
        std::string::npos);
  CHECK(snapshotLocals.find("if (graphLocalAutoBenchmarkShadow_ != nullptr) {") !=
        std::string::npos);
  CHECK(snapshotLocals.find("graphLocalAutoBenchmarkShadow_->noteLegacyKey(scopePath, sourceLine,") !=
        std::string::npos);
  CHECK(snapshotLocals.find("return GraphLocalAutoKey{scopePathId, sourceLine, sourceColumn};") !=
        std::string::npos);
  CHECK(buildUtility.find("graphLocalAutoBindingKey(scopePath, sourceLine, sourceColumn)") !=
        std::string::npos);
  CHECK(buildUtility.find(
            "  std::string resolvedPath = preferredCollectionHelperResolvedPath(expr);\n"
            "  if (resolvedPath.empty()) {\n"
            "    resolvedPath = resolveCalleePath(expr);\n"
            "  }\n"
            "  std::string canonicalResolvedPath = resolvedPath;\n"
            "  if (const size_t suffix = canonicalResolvedPath.find(\"__t\");\n"
            "      suffix != std::string::npos) {\n"
            "    canonicalResolvedPath.erase(suffix);\n"
            "  }\n"
            "  canonicalResolvedPath =\n"
            "      canonicalizeLegacySoaGetHelperPath(canonicalResolvedPath);\n"
            "  canonicalResolvedPath =\n"
            "      canonicalizeLegacySoaRefHelperPath(canonicalResolvedPath);\n"
            "  canonicalResolvedPath =\n"
            "      canonicalizeLegacySoaToAosHelperPath(canonicalResolvedPath);\n"
            "  const std::string &resolvedLookupPath =\n"
            "      !canonicalResolvedPath.empty() ? canonicalResolvedPath : resolvedPath;\n"
            "  return defMap_.count(resolvedLookupPath) == 0;\n") !=
        std::string::npos);
  CHECK(buildUtility.find("  return defMap_.count(resolveCalleePath(expr)) == 0;\n") ==
        std::string::npos);
  CHECK(inferGraph.find("graphLocalAutoScopePathInterner_.clear();") != std::string::npos);
  CHECK(inferGraph.find("if (graphLocalAutoBenchmarkShadow_ != nullptr) {") !=
        std::string::npos);
  CHECK(inferGraph.find("graphLocalAutoBenchmarkShadow_->clear();") !=
        std::string::npos);
  CHECK(inferGraph.find("graphLocalAutoFacts_.clear();") != std::string::npos);
  CHECK(inferGraph.find("graphLocalAutoFacts_.try_emplace(bindingKey);") != std::string::npos);
  CHECK(inferGraph.find("GraphLocalAutoFacts &fact = factIt->second;") != std::string::npos);
  CHECK(inferGraph.find("graphLocalAutoBenchmarkShadow_->rebuildFromFacts(graphLocalAutoFacts_);") !=
        std::string::npos);
  CHECK(graphLocalAutoSource.find("void GraphLocalAutoBenchmarkShadow::rebuildFromFacts(") !=
        std::string::npos);
  CHECK(graphLocalAutoSource.find("legacyState_.bindingShadow.reserve(graphLocalAutoFacts.size());") !=
        std::string::npos);
  CHECK(graphLocalAutoSource.find("for (const auto &[bindingKey, fact] : graphLocalAutoFacts) {") !=
        std::string::npos);
  CHECK(inferGraph.find("using PmrDependencyCountMap = std::pmr::unordered_map<GraphLocalAutoKey, size_t, GraphLocalAutoKeyHash>;") !=
        std::string::npos);
  CHECK(inferGraph.find("std::pmr::monotonic_buffer_resource arenaResource{") != std::string::npos);
  CHECK(inferGraph.find("PmrDependencyCountMap dependencyCountByBindingKey{&arenaResource};") != std::string::npos);
  CHECK(inferGraph.find("GraphLocalAutoDependencyScratch dependencyScratch;") != std::string::npos);
  CHECK(inferGraph.find("auto &dependencyCountByBindingKey = dependencyScratch.dependencyCountByBindingKey;") !=
        std::string::npos);
  CHECK(inferGraph.find("using StdDependencyCountMap = std::unordered_map<GraphLocalAutoKey, size_t, GraphLocalAutoKeyHash>;") !=
        std::string::npos);
  CHECK(inferGraph.find("const bool usePmrDependencyScratch = benchmarkGraphLocalAutoDependencyScratchPmrEnabled_;") !=
        std::string::npos);
  CHECK(inferGraph.find("if (!usePmrDependencyScratch) {") != std::string::npos);
  CHECK(inferGraph.find("stdDependencyCountByBindingKey.reserve(graph.nodes.size());") !=
        std::string::npos);
}

TEST_CASE("type resolution graph builder is wired through semantics testing api") {
  const std::filesystem::path cwd = std::filesystem::current_path();
  std::filesystem::path cmakePath = cwd / "CMakeLists.txt";
  std::filesystem::path testApiPath = cwd / "include" / "primec" / "testing" / "SemanticsGraphHelpers.h";
  std::filesystem::path validationApiPath = cwd / "include" / "primec" / "testing" / "SemanticsValidationHelpers.h";
  std::filesystem::path graphHeaderPath = cwd / "src" / "semantics" / "TypeResolutionGraph.h";
  std::filesystem::path graphSourcePath = cwd / "src" / "semantics" / "TypeResolutionGraph.cpp";
  std::filesystem::path pipelinePath = cwd / "src" / "CompilePipeline.cpp";
  std::filesystem::path primecMainPath = cwd / "src" / "main.cpp";
  std::filesystem::path primevmMainPath = cwd / "src" / "primevm_main.cpp";
  if (!std::filesystem::exists(cmakePath)) {
    cmakePath = cwd.parent_path() / "CMakeLists.txt";
    testApiPath = cwd.parent_path() / "include" / "primec" / "testing" / "SemanticsGraphHelpers.h";
    validationApiPath = cwd.parent_path() / "include" / "primec" / "testing" / "SemanticsValidationHelpers.h";
    graphHeaderPath = cwd.parent_path() / "src" / "semantics" / "TypeResolutionGraph.h";
    graphSourcePath = cwd.parent_path() / "src" / "semantics" / "TypeResolutionGraph.cpp";
    pipelinePath = cwd.parent_path() / "src" / "CompilePipeline.cpp";
    primecMainPath = cwd.parent_path() / "src" / "main.cpp";
    primevmMainPath = cwd.parent_path() / "src" / "primevm_main.cpp";
  }
  REQUIRE(std::filesystem::exists(cmakePath));
  REQUIRE(std::filesystem::exists(testApiPath));
  REQUIRE(std::filesystem::exists(validationApiPath));
  REQUIRE(std::filesystem::exists(graphHeaderPath));
  REQUIRE(std::filesystem::exists(graphSourcePath));
  REQUIRE(std::filesystem::exists(pipelinePath));
  REQUIRE(std::filesystem::exists(primecMainPath));
  REQUIRE(std::filesystem::exists(primevmMainPath));

  const std::string cmake = readTextFile(cmakePath);
  const std::string testApi = readTextFile(testApiPath);
  const std::string validationApi = readTextFile(validationApiPath);
  const std::string graphHeader = readTextFile(graphHeaderPath);
  const std::string graphSource = readTextFile(graphSourcePath);
  const std::string pipeline = readTextFile(pipelinePath);
  const std::string primecMain = readTextFile(primecMainPath);
  const std::string primevmMain = readTextFile(primevmMainPath);
  CHECK(cmake.find("src/semantics/TypeResolutionGraph.cpp") != std::string::npos);
  CHECK(cmake.find("primestruct.semantics.type_resolution_graph") != std::string::npos);
  CHECK(testApi.find("struct TypeResolutionGraphSnapshotNode") != std::string::npos);
  CHECK(testApi.find("struct TypeResolutionGraphSnapshotEdge") != std::string::npos);
  CHECK(testApi.find("struct TypeResolutionGraphInvalidationContractSnapshot") !=
        std::string::npos);
  CHECK(testApi.find("struct TypeResolutionGraphSnapshot") != std::string::npos);
  CHECK(testApi.find("buildTypeResolutionGraphForTesting") != std::string::npos);
  CHECK(testApi.find("dumpTypeResolutionGraphForTesting") != std::string::npos);
  CHECK(testApi.find("struct TemplatedFallbackQueryStateEnvelopeSnapshotForTesting") !=
        std::string::npos);
  CHECK(testApi.find("struct ExplicitTemplateArgResolutionFactForTesting") != std::string::npos);
  CHECK(testApi.find("struct ImplicitTemplateArgResolutionFactForTesting") != std::string::npos);
  CHECK(testApi.find("collectExplicitTemplateArgResolutionFactsForTesting") != std::string::npos);
  CHECK(testApi.find("collectImplicitTemplateArgResolutionFactsForTesting") != std::string::npos);
  CHECK(testApi.find("collectExplicitTemplateArgFactConsumptionMetricsForTesting") !=
        std::string::npos);
  CHECK(testApi.find("collectImplicitTemplateArgFactConsumptionMetricsForTesting") !=
        std::string::npos);
  CHECK(testApi.find("classifyTemplatedFallbackQueryTypeTextForTesting") != std::string::npos);
  CHECK(validationApi.find("struct TypeResolutionGraphSnapshotNode") == std::string::npos);
  CHECK(validationApi.find("buildTypeResolutionGraphForTesting") == std::string::npos);
  CHECK(validationApi.find("TemplatedFallbackQueryStateEnvelopeSnapshotForTesting") == std::string::npos);
  CHECK(validationApi.find("ExplicitTemplateArgResolutionFactForTesting") == std::string::npos);
  CHECK(validationApi.find("ImplicitTemplateArgResolutionFactForTesting") == std::string::npos);
  CHECK(validationApi.find("collectExplicitTemplateArgResolutionFactsForTesting") == std::string::npos);
  CHECK(validationApi.find("collectImplicitTemplateArgResolutionFactsForTesting") == std::string::npos);
  CHECK(validationApi.find("collectExplicitTemplateArgFactConsumptionMetricsForTesting") ==
        std::string::npos);
  CHECK(validationApi.find("collectImplicitTemplateArgFactConsumptionMetricsForTesting") ==
        std::string::npos);
  CHECK(validationApi.find("classifyTemplatedFallbackQueryTypeTextForTesting") == std::string::npos);
  CHECK(graphHeader.find("enum class TypeResolutionNodeKind") != std::string::npos);
  CHECK(graphHeader.find("enum class TypeResolutionEdgeKind") != std::string::npos);
  CHECK(graphHeader.find("enum class TypeResolutionGraphInvalidationEditFamily") !=
        std::string::npos);
  CHECK(graphHeader.find("typeResolutionGraphInvalidationContracts") !=
        std::string::npos);
  CHECK(graphHeader.find("struct TypeResolutionGraphNode") != std::string::npos);
  CHECK(graphHeader.find("struct TypeResolutionGraphEdge") != std::string::npos);
  CHECK(graphHeader.find("struct TypeResolutionGraph") != std::string::npos);
  CHECK(graphHeader.find("buildTypeResolutionGraphForProgram") != std::string::npos);
  CHECK(graphHeader.find("formatTypeResolutionGraph") != std::string::npos);
  CHECK(graphSource.find("TypeResolutionGraphBuilder") != std::string::npos);
  CHECK(graphSource.find("prepareProgramForTypeResolutionAnalysis(program, entryPath, semanticTransforms, error)") !=
        std::string::npos);
  CHECK(graphSource.find("typeResolutionGraphInvalidationContracts()") !=
        std::string::npos);
  CHECK(graphSource.find("buildTypeResolutionGraph(program)") != std::string::npos);
  CHECK(graphSource.find("buildTypeResolutionGraphForProgram(std::move(program), entryPath") != std::string::npos);
  CHECK(graphSource.find("out = formatTypeResolutionGraph(graph);") != std::string::npos);
  CHECK(cmake.find("src/semantics/TypeResolutionGraphPreparation.cpp") != std::string::npos);
  CHECK(pipeline.find("DumpStage::TypeGraph") != std::string::npos);
  CHECK(pipeline.find("DumpStage::SemanticProduct") != std::string::npos);
  CHECK(pipeline.find("dumpStage == \"semantic_product\" || dumpStage == \"semantic-product\"") !=
        std::string::npos);
  CHECK(pipeline.find("dumpStage == \"type_graph\" || dumpStage == \"type-graph\"") != std::string::npos);
  CHECK(pipeline.find("semantics::buildTypeResolutionGraphForProgram(") != std::string::npos);
  CHECK(pipeline.find("output.dumpOutput = semantics::formatTypeResolutionGraph(graph);") != std::string::npos);
  CHECK(pipeline.find("output.dumpOutput = formatSemanticProgram(output.semanticProgram);") !=
        std::string::npos);
  CHECK(primecMain.find("[--dump-stage pre_ast|ast|ast-semantic|semantic-product|type-graph|ir]") !=
        std::string::npos);
  CHECK(primevmMain.find("[--dump-stage pre_ast|ast|ast-semantic|semantic-product|type-graph|ir]") !=
        std::string::npos);
}

TEST_CASE("public semantic-product dump helper is available for pipeline tests") {
  const std::filesystem::path cwd = std::filesystem::current_path();
  std::filesystem::path helperPath = cwd / "include" / "primec" / "testing" / "CompilePipelineDumpHelpers.h";
  if (!std::filesystem::exists(helperPath)) {
    helperPath = cwd.parent_path() / "include" / "primec" / "testing" / "CompilePipelineDumpHelpers.h";
  }
  REQUIRE(std::filesystem::exists(helperPath));

  const std::string helper = readTextFile(helperPath);
  CHECK(helper.find("struct CompilePipelineBoundaryDumps") != std::string::npos);
  CHECK(helper.find("struct CompilePipelineBackendConformance") != std::string::npos);
  CHECK(helper.find("defaultCompilePipelineTestingEffects()") != std::string::npos);
  CHECK(helper.find("\"heap_alloc\"") != std::string::npos);
  CHECK(helper.find("captureCompilePipelineDumpForTesting(") == std::string::npos);
  CHECK(helper.find("captureSemanticBoundaryDumpsForTesting(") != std::string::npos);
  CHECK(helper.find("dumpStageRequiresSemanticProduct(") == std::string::npos);
  CHECK(helper.find("enum class CompilePipelineSemanticProductIntent") != std::string::npos);
  CHECK(helper.find("CompilePipelineSemanticProductIntent semanticProductIntent") != std::string::npos);
  CHECK(helper.find("applySemanticProductIntent(Options &options, CompilePipelineSemanticProductIntent intent)") !=
        std::string::npos);
  CHECK(helper.find("applySemanticProductIntent(options, semanticProductIntent);") !=
        std::string::npos);
  CHECK(helper.find("CompilePipelineSemanticProductIntent::SkipForNonConsumingPath") !=
        std::string::npos);
  CHECK(helper.find("CompilePipelineSemanticProductIntent::Require") != std::string::npos);
  CHECK(helper.find("detail::applySemanticProductIntent(") !=
        std::string::npos);
  CHECK(helper.find("struct CompilePipelinePreparedIr") == std::string::npos);
  CHECK(helper.find("prepareCompilePipelineIrForTesting(") == std::string::npos);
  CHECK(helper.find("runCompilePipelineBackendConformanceForTesting(") != std::string::npos);
}

TEST_CASE("core IR test helpers expose semantic-product-aware lowering") {
  const std::filesystem::path cwd = std::filesystem::current_path();
  std::filesystem::path helperPath = cwd / "tests" / "unit" / "test_ir_pipeline_helpers.h";
  if (!std::filesystem::exists(helperPath)) {
    helperPath = cwd.parent_path() / "tests" / "unit" / "test_ir_pipeline_helpers.h";
  }
  REQUIRE(std::filesystem::exists(helperPath));

  const std::string helper = readTextFile(helperPath);
  CHECK(helper.find("parseValidateAndLower(const std::string &source,") != std::string::npos);
  CHECK(helper.find("prepareIrThroughCompilePipeline(") != std::string::npos);
  CHECK(helper.find("SemanticProgram semanticProgram;") != std::string::npos);
  CHECK(helper.find("enum class CompilePipelineSemanticProductIntentForTesting") != std::string::npos);
  CHECK(helper.find("applyCompilePipelineSemanticProductIntentForTesting(") != std::string::npos);
  CHECK(helper.find("applyCompilePipelineSemanticProductIntentForTesting(options, semanticProductIntent);") !=
        std::string::npos);
  CHECK(helper.find("return lowerer.lower(program, &semanticProgram, \"/main\", defaultEffects, entryDefaultEffects, module, error);") !=
        std::string::npos);
}

TEST_CASE("public lowerer testing headers stay in sync with semantic-product helper declarations") {
  const std::filesystem::path cwd = std::filesystem::current_path();
  std::filesystem::path bindingTransformPath =
      cwd / "include" / "primec" / "testing" / "ir_lowerer_helpers" / "IrLowererBindingTransformHelpers.h";
  std::filesystem::path flowHelpersPath =
      cwd / "include" / "primec" / "testing" / "ir_lowerer_helpers" / "IrLowererFlowHelpers.h";
  std::filesystem::path inlineParamPath =
      cwd / "include" / "primec" / "testing" / "ir_lowerer_helpers" / "IrLowererInlineParamHelpers.h";
  std::filesystem::path lowerInferencePath =
      cwd / "include" / "primec" / "testing" / "ir_lowerer_helpers" / "IrLowererLowerInferenceSetup.h";
  std::filesystem::path lowerLocalsPath =
      cwd / "include" / "primec" / "testing" / "ir_lowerer_helpers" / "IrLowererLowerLocalsSetup.h";
  std::filesystem::path resultHelpersPath =
      cwd / "include" / "primec" / "testing" / "ir_lowerer_helpers" / "IrLowererResultHelpers.h";
  std::filesystem::path setupTypePath =
      cwd / "include" / "primec" / "testing" / "ir_lowerer_helpers" / "IrLowererSetupTypeHelpers.h";
  if (!std::filesystem::exists(bindingTransformPath)) {
    const std::filesystem::path root = cwd.parent_path();
    bindingTransformPath =
        root / "include" / "primec" / "testing" / "ir_lowerer_helpers" / "IrLowererBindingTransformHelpers.h";
    flowHelpersPath =
        root / "include" / "primec" / "testing" / "ir_lowerer_helpers" / "IrLowererFlowHelpers.h";
    inlineParamPath =
        root / "include" / "primec" / "testing" / "ir_lowerer_helpers" / "IrLowererInlineParamHelpers.h";
    lowerInferencePath =
        root / "include" / "primec" / "testing" / "ir_lowerer_helpers" / "IrLowererLowerInferenceSetup.h";
    lowerLocalsPath =
        root / "include" / "primec" / "testing" / "ir_lowerer_helpers" / "IrLowererLowerLocalsSetup.h";
    resultHelpersPath =
        root / "include" / "primec" / "testing" / "ir_lowerer_helpers" / "IrLowererResultHelpers.h";
    setupTypePath =
        root / "include" / "primec" / "testing" / "ir_lowerer_helpers" / "IrLowererSetupTypeHelpers.h";
  }
  REQUIRE(std::filesystem::exists(bindingTransformPath));
  REQUIRE(std::filesystem::exists(flowHelpersPath));
  REQUIRE(std::filesystem::exists(inlineParamPath));
  REQUIRE(std::filesystem::exists(lowerInferencePath));
  REQUIRE(std::filesystem::exists(lowerLocalsPath));
  REQUIRE(std::filesystem::exists(resultHelpersPath));
  REQUIRE(std::filesystem::exists(setupTypePath));

  const std::string bindingTransform = readTextFile(bindingTransformPath);
  const std::string flowHelpers = readTextFile(flowHelpersPath);
  const std::string inlineParam = readTextFile(inlineParamPath);
  const std::string lowerInference = readTextFile(lowerInferencePath);
  const std::string lowerLocals = readTextFile(lowerLocalsPath);
  const std::string resultHelpers = readTextFile(resultHelpersPath);
  const std::string setupType = readTextFile(setupTypePath);

  CHECK(bindingTransform.find("extractTopLevelUninitializedTypeText") != std::string::npos);
  CHECK(bindingTransform.find("unwrapTopLevelUninitializedTypeText") != std::string::npos);
  CHECK(flowHelpers.find("emitVectorMoveSlot(") != std::string::npos);
  CHECK(flowHelpers.find("emitDisarmTemporaryStructAfterCopy(") != std::string::npos);
  CHECK(flowHelpers.find("emitBuiltinCanonicalMapInsertOverwriteOrPending(") == std::string::npos);
  CHECK(inlineParam.find("using ResolveInlineParameterDefinitionCallFn") != std::string::npos);
  CHECK(inlineParam.find("const std::string &calleePath,") != std::string::npos);
  CHECK(lowerInference.find("const SemanticProgram *semanticProgram = nullptr;") !=
        std::string::npos);
  CHECK(lowerInference.find("const SemanticProductIndex *semanticIndex = nullptr;") !=
        std::string::npos);
  CHECK(lowerLocals.find("const SemanticProgram *semanticProgram,") != std::string::npos);
  CHECK(resultHelpers.find("using InferExprKindWithLocalsFn") != std::string::npos);
  CHECK(resultHelpers.find("struct PackedResultStructPayloadInfo") != std::string::npos);
  CHECK(resultHelpers.find("const SemanticProgram *semanticProgram = nullptr,") !=
        std::string::npos);
  CHECK(resultHelpers.find("const SemanticProductIndex *semanticIndex = nullptr,") !=
        std::string::npos);
  CHECK(resultHelpers.find("std::string *errorOut = nullptr") != std::string::npos);
  CHECK(resultHelpers.find("inferPackedResultStructType(") != std::string::npos);
  CHECK(resultHelpers.find("resolvePackedResultStructPayloadInfo(") != std::string::npos);
  CHECK(setupType.find("const SemanticProgram *semanticProgram,") !=
        std::string::npos);
  CHECK(setupType.find("resolveMethodCallDefinitionFromExpr(") != std::string::npos);

  std::filesystem::path semanticTargetsPath =
      cwd / "include" / "primec" / "testing" / "ir_lowerer_helpers" /
      "IrLowererSemanticProductTargetAdapters.h";
  if (!std::filesystem::exists(semanticTargetsPath)) {
    semanticTargetsPath = cwd.parent_path() / "include" / "primec" /
                          "testing" / "ir_lowerer_helpers" /
                          "IrLowererSemanticProductTargetAdapters.h";
  }
  REQUIRE(std::filesystem::exists(semanticTargetsPath));
  const std::string semanticTargets = readTextFile(semanticTargetsPath);
  CHECK(semanticTargets.find("findSemanticProductReturnFactByPath") !=
        std::string::npos);
  CHECK(semanticTargets.find("findSemanticProductBindingFactByScopeAndName") ==
        std::string::npos);
  CHECK(semanticTargets.find("findSemanticProductSumTypeMetadata") !=
        std::string::npos);
  CHECK(semanticTargets.find("findSemanticProductSumVariantMetadata") !=
        std::string::npos);
}

TEST_CASE("public lowerer testing umbrellas keep alias owners ahead of users") {
  const std::filesystem::path cwd = std::filesystem::current_path();
  std::filesystem::path irLowererHelpersHeaderPath = cwd / "include" / "primec" / "testing" / "IrLowererHelpers.h";
  std::filesystem::path irLowererStageContractsHeaderPath =
      cwd / "include" / "primec" / "testing" / "IrLowererStageContracts.h";
  if (!std::filesystem::exists(irLowererHelpersHeaderPath)) {
    irLowererHelpersHeaderPath =
        cwd.parent_path() / "include" / "primec" / "testing" / "IrLowererHelpers.h";
    irLowererStageContractsHeaderPath =
        cwd.parent_path() / "include" / "primec" / "testing" / "IrLowererStageContracts.h";
  }
  REQUIRE(std::filesystem::exists(irLowererHelpersHeaderPath));
  REQUIRE(std::filesystem::exists(irLowererStageContractsHeaderPath));

  const std::string irLowererHelpersHeader = readTextFile(irLowererHelpersHeaderPath);
  const std::string irLowererStageContractsHeader = readTextFile(irLowererStageContractsHeaderPath);

  const auto setupTypePos = irLowererHelpersHeader.find("IrLowererSetupTypeHelpers.h");
  const auto callDispatchPos = irLowererHelpersHeader.find("IrLowererCallDispatchHelpers.h");
  const auto statementBindingPos = irLowererHelpersHeader.find("IrLowererStatementBindingHelpers.h");
  const auto countAccessPos = irLowererHelpersHeader.find("IrLowererCountAccessHelpers.h");
  const auto resultHelpersPos = irLowererHelpersHeader.find("IrLowererResultHelpers.h");
  const auto stringCallPos = irLowererHelpersHeader.find("IrLowererStringCallHelpers.h");
  const auto operatorArithmeticPos = irLowererHelpersHeader.find("IrLowererOperatorArithmeticHelpers.h");
  const auto gpuEffectsPos = irLowererHelpersHeader.find("IrLowererGpuEffects.h");
  const auto nativeEffectsPos = irLowererHelpersHeader.find("IrLowererNativeEffects.h");
  const auto vmEffectsPos = irLowererHelpersHeader.find("IrLowererVmEffects.h");
  const auto lowerExprEmitSetupPos = irLowererHelpersHeader.find("IrLowererLowerExprEmitSetup.h");
  const auto lowerReturnCallsSetupPos = irLowererHelpersHeader.find("IrLowererLowerReturnCallsSetup.h");
  const auto lowerInferencePos = irLowererStageContractsHeader.find("IrLowererLowerInferenceSetup.h");
  const auto lowerSetupStagePos = irLowererStageContractsHeader.find("IrLowererLowerSetupStage.h");
  const auto lowerReturnEmitStagePos = irLowererStageContractsHeader.find("IrLowererLowerReturnEmitStage.h");
  const auto lowerStatementsCallsStagePos =
      irLowererStageContractsHeader.find("IrLowererLowerStatementsCallsStage.h");

  REQUIRE(setupTypePos != std::string::npos);
  REQUIRE(callDispatchPos != std::string::npos);
  REQUIRE(statementBindingPos != std::string::npos);
  REQUIRE(countAccessPos != std::string::npos);
  REQUIRE(resultHelpersPos != std::string::npos);
  REQUIRE(stringCallPos != std::string::npos);
  REQUIRE(operatorArithmeticPos != std::string::npos);
  REQUIRE(gpuEffectsPos != std::string::npos);
  REQUIRE(nativeEffectsPos != std::string::npos);
  REQUIRE(vmEffectsPos != std::string::npos);
  REQUIRE(lowerExprEmitSetupPos != std::string::npos);
  REQUIRE(lowerReturnCallsSetupPos != std::string::npos);
  REQUIRE(lowerInferencePos != std::string::npos);
  REQUIRE(lowerSetupStagePos != std::string::npos);
  REQUIRE(lowerReturnEmitStagePos != std::string::npos);
  REQUIRE(lowerStatementsCallsStagePos != std::string::npos);

  CHECK(setupTypePos < callDispatchPos);
  CHECK(statementBindingPos < countAccessPos);
  CHECK(resultHelpersPos < operatorArithmeticPos);
  CHECK(stringCallPos < operatorArithmeticPos);
  CHECK(operatorArithmeticPos < gpuEffectsPos);
  CHECK(gpuEffectsPos < lowerExprEmitSetupPos);
  CHECK(operatorArithmeticPos < lowerExprEmitSetupPos);
  CHECK(lowerExprEmitSetupPos < lowerReturnCallsSetupPos);
  CHECK(lowerInferencePos < lowerSetupStagePos);
  CHECK(lowerSetupStagePos < lowerReturnEmitStagePos);
  CHECK(lowerReturnEmitStagePos < lowerStatementsCallsStagePos);
  CHECK(irLowererHelpersHeader.find("IrLowererLowerInferenceSetup.h") == std::string::npos);
  CHECK(irLowererHelpersHeader.find("IrLowererLowerSetupStage.h") == std::string::npos);
  CHECK(irLowererHelpersHeader.find("IrLowererLowerReturnEmitStage.h") == std::string::npos);
  CHECK(irLowererHelpersHeader.find("IrLowererLowerStatementsCallsStage.h") == std::string::npos);
  CHECK(irLowererHelpersHeader.find("IrLowererLowerStatementsCallsStep.h") == std::string::npos);
  CHECK(irLowererHelpersHeader.find("IrLowererLowerStatementsEntryExecutionStep.h") == std::string::npos);
  CHECK(irLowererHelpersHeader.find("IrLowererLowerStatementsEntryStatementStep.h") == std::string::npos);
  CHECK(irLowererHelpersHeader.find("IrLowererLowerStatementsFunctionTableStep.h") == std::string::npos);
  CHECK(irLowererHelpersHeader.find("IrLowererLowerStatementsSourceMapStep.h") == std::string::npos);
  CHECK(irLowererStageContractsHeader.find("IrLowererLowerStatementsCallsStep.h") == std::string::npos);
  CHECK(irLowererStageContractsHeader.find("IrLowererLowerStatementsEntryExecutionStep.h") ==
        std::string::npos);
  CHECK(irLowererStageContractsHeader.find("IrLowererLowerStatementsEntryStatementStep.h") ==
        std::string::npos);
  CHECK(irLowererStageContractsHeader.find("IrLowererLowerStatementsFunctionTableStep.h") ==
        std::string::npos);
  CHECK(irLowererStageContractsHeader.find("IrLowererLowerStatementsSourceMapStep.h") ==
        std::string::npos);
}

TEST_CASE("public call dispatch testing header stays in sync with alias-policy helpers") {
  const std::filesystem::path cwd = std::filesystem::current_path();
  std::filesystem::path callDispatchPath =
      cwd / "include" / "primec" / "testing" / "ir_lowerer_helpers" / "IrLowererCallDispatchHelpers.h";
  if (!std::filesystem::exists(callDispatchPath)) {
    callDispatchPath =
        cwd.parent_path() / "include" / "primec" / "testing" / "ir_lowerer_helpers" /
        "IrLowererCallDispatchHelpers.h";
  }
  REQUIRE(std::filesystem::exists(callDispatchPath));

  const std::string callDispatch = readTextFile(callDispatchPath);
  CHECK(callDispatch.find("resolveCallPathFromScopeWithoutImportAliases(") == std::string::npos);
  CHECK(callDispatch.find("validateSemanticProductDirectCallCoverage(") != std::string::npos);
  CHECK(callDispatch.find("validateSemanticProductBridgePathCoverage(") != std::string::npos);
  CHECK(callDispatch.find("validateSemanticProductMethodCallCoverage(") != std::string::npos);
  CHECK(callDispatch.find("const LocalMap &localsIn,") != std::string::npos);
  CHECK(callDispatch.find("UnsupportedNativeCallResult emitUnsupportedNativeCallDiagnostic(") !=
        std::string::npos);
}

TEST_CASE("graph snapshot suite uses semantic-product-aware lowering only") {
  const std::filesystem::path cwd = std::filesystem::current_path();
  std::filesystem::path snapshotPath =
      cwd / "tests" / "unit" / "test_semantics_type_resolution_graph_snapshots.cpp";
  if (!std::filesystem::exists(snapshotPath)) {
    snapshotPath = cwd.parent_path() / "tests" / "unit" / "test_semantics_type_resolution_graph_snapshots.cpp";
  }
  REQUIRE(std::filesystem::exists(snapshotPath));

  const std::string snapshot = readTextFile(snapshotPath);
  CHECK(snapshot.find("REQUIRE(lowerer.lower(baselineAst, &semanticProgram,") !=
        std::string::npos);
  CHECK(snapshot.find("fallbackModule") == std::string::npos);
}

TEST_CASE("backend registry keeps semantic-product negative fixture families covered") {
  const std::filesystem::path cwd = std::filesystem::current_path();
  std::filesystem::path registryPath = cwd / "tests" / "unit" / "test_ir_pipeline_backends_registry.cpp";
  if (!std::filesystem::exists(registryPath)) {
    registryPath = cwd.parent_path() / "tests" / "unit" / "test_ir_pipeline_backends_registry.cpp";
  }
  REQUIRE(std::filesystem::exists(registryPath));

  const std::string registrySource = readTextFile(registryPath);
  CHECK(registrySource.find("missing semantic-product direct-call semantic id:") != std::string::npos);
  CHECK(registrySource.find("missing semantic-product method-call target:") != std::string::npos);
  CHECK(registrySource.find("ir lowerer rejects missing semantic-product bridge-path choices") !=
        std::string::npos);
  CHECK(registrySource.find("missing semantic-product binding fact:") != std::string::npos);
  CHECK(registrySource.find("missing semantic-product collection specialization:") !=
        std::string::npos);
  CHECK(registrySource.find("missing semantic-product local-auto fact:") != std::string::npos);
  CHECK(registrySource.find("missing semantic-product return fact:") != std::string::npos);
  CHECK(registrySource.find("missing semantic-product callable result metadata:") != std::string::npos);
  CHECK(registrySource.find("missing semantic-product return binding type:") != std::string::npos);
  CHECK(registrySource.find("missing semantic-product on_error fact:") != std::string::npos);
  CHECK(registrySource.find("missing semantic-product entry parameter fact:") != std::string::npos);
}

TEST_CASE("ir lowerer header exposes only semantic-product-aware lowering entrypoint") {
  const std::filesystem::path cwd = std::filesystem::current_path();
  std::filesystem::path irLowererHeaderPath = cwd / "include" / "primec" / "IrLowerer.h";
  if (!std::filesystem::exists(irLowererHeaderPath)) {
    irLowererHeaderPath = cwd.parent_path() / "include" / "primec" / "IrLowerer.h";
  }
  REQUIRE(std::filesystem::exists(irLowererHeaderPath));

  const std::string header = readTextFile(irLowererHeaderPath);
  CHECK(header.find("const SemanticProgram *semanticProgram") != std::string::npos);
  CHECK(header.find("return lower(program, nullptr, entryPath, defaultEffects, entryDefaultEffects, out, error, diagnosticInfo);") ==
        std::string::npos);
}

TEST_CASE("semantic product routing facts have dedicated public headers") {
  const std::filesystem::path cwd = std::filesystem::current_path();
  std::filesystem::path semanticProductPath = cwd / "include" / "primec" / "SemanticProduct.h";
  std::filesystem::path directCallFactsPath =
      cwd / "include" / "primec" / "semantic_product" / "DirectCallFacts.h";
  std::filesystem::path methodCallFactsPath =
      cwd / "include" / "primec" / "semantic_product" / "MethodCallFacts.h";
  if (!std::filesystem::exists(semanticProductPath)) {
    semanticProductPath = cwd.parent_path() / "include" / "primec" / "SemanticProduct.h";
    directCallFactsPath =
        cwd.parent_path() / "include" / "primec" / "semantic_product" / "DirectCallFacts.h";
    methodCallFactsPath =
        cwd.parent_path() / "include" / "primec" / "semantic_product" / "MethodCallFacts.h";
  }
  REQUIRE(std::filesystem::exists(semanticProductPath));
  REQUIRE(std::filesystem::exists(directCallFactsPath));
  REQUIRE(std::filesystem::exists(methodCallFactsPath));

  const std::string semanticProduct = readTextFile(semanticProductPath);
  const std::string directCallFacts = readTextFile(directCallFactsPath);
  const std::string methodCallFacts = readTextFile(methodCallFactsPath);
  CHECK(semanticProduct.find("#include \"primec/semantic_product/DirectCallFacts.h\"") !=
        std::string::npos);
  CHECK(semanticProduct.find("#include \"primec/semantic_product/MethodCallFacts.h\"") !=
        std::string::npos);
  CHECK(semanticProduct.find("struct SemanticProgramDirectCallTarget {") == std::string::npos);
  CHECK(semanticProduct.find("struct SemanticProgramMethodCallTarget {") == std::string::npos);
  CHECK(semanticProduct.find("std::vector<SemanticProgramDirectCallTarget> directCallTargets;") !=
        std::string::npos);
  CHECK(semanticProduct.find("std::vector<SemanticProgramMethodCallTarget> methodCallTargets;") !=
        std::string::npos);
  CHECK(directCallFacts.find("#include \"primec/SemanticProduct.h\"") == std::string::npos);
  CHECK(methodCallFacts.find("#include \"primec/SemanticProduct.h\"") == std::string::npos);
  CHECK(directCallFacts.find("struct SemanticProgramDirectCallTarget {") != std::string::npos);
  CHECK(methodCallFacts.find("struct SemanticProgramMethodCallTarget {") != std::string::npos);
  CHECK(directCallFacts.find("semanticProgramDirectCallTargetView") != std::string::npos);
  CHECK(methodCallFacts.find("semanticProgramMethodCallTargetView") != std::string::npos);
  CHECK(directCallFacts.find("semanticProgramDirectCallTargetResolvedPath") !=
        std::string::npos);
  CHECK(methodCallFacts.find("semanticProgramMethodCallTargetResolvedPath") !=
        std::string::npos);
}

TEST_CASE("semantic product routing fact v1 shape markers match public fields") {
  const std::filesystem::path cwd = std::filesystem::current_path();
  std::filesystem::path directCallFactsPath =
      cwd / "include" / "primec" / "semantic_product" / "DirectCallFacts.h";
  std::filesystem::path methodCallFactsPath =
      cwd / "include" / "primec" / "semantic_product" / "MethodCallFacts.h";
  if (!std::filesystem::exists(directCallFactsPath)) {
    directCallFactsPath =
        cwd.parent_path() / "include" / "primec" / "semantic_product" / "DirectCallFacts.h";
    methodCallFactsPath =
        cwd.parent_path() / "include" / "primec" / "semantic_product" / "MethodCallFacts.h";
  }
  REQUIRE(std::filesystem::exists(directCallFactsPath));
  REQUIRE(std::filesystem::exists(methodCallFactsPath));

  const auto trim = [](std::string value) {
    const std::size_t first = value.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) {
      return std::string{};
    }
    const std::size_t last = value.find_last_not_of(" \t\r\n");
    return value.substr(first, last - first + 1);
  };
  const auto extractStructShape = [&](const std::string &source, std::string_view structName) {
    const std::string structNeedle = "struct " + std::string(structName) + " {";
    const std::size_t structStart = source.find(structNeedle);
    if (structStart == std::string::npos) {
      return "missing struct " + std::string(structName);
    }
    const std::size_t bodyStart = source.find('\n', structStart);
    const std::size_t bodyEnd = source.find("\n};", bodyStart);
    if (bodyStart == std::string::npos || bodyEnd == std::string::npos) {
      return "missing struct body " + std::string(structName);
    }

    std::string shape;
    std::size_t lineStart = bodyStart + 1;
    while (lineStart < bodyEnd) {
      const std::size_t lineEnd = source.find('\n', lineStart);
      const std::size_t clampedLineEnd =
          lineEnd == std::string::npos || lineEnd > bodyEnd ? bodyEnd : lineEnd;
      const std::string line =
          trim(source.substr(lineStart, clampedLineEnd - lineStart));
      if (!line.empty()) {
        if (!shape.empty()) {
          shape += "|";
        }
        shape += line;
      }
      if (lineEnd == std::string::npos || lineEnd >= bodyEnd) {
        break;
      }
      lineStart = lineEnd + 1;
    }
    return shape;
  };
  const auto extractStringViewMarker = [&](const std::string &source, std::string_view markerName) {
    const std::size_t markerStart = source.find(markerName);
    if (markerStart == std::string::npos) {
      return "missing marker " + std::string(markerName);
    }
    const std::size_t initStart = source.find('=', markerStart);
    if (initStart == std::string::npos) {
      return "missing marker initializer " + std::string(markerName);
    }
    std::size_t initEnd = std::string::npos;
    bool inString = false;
    for (std::size_t i = initStart + 1; i < source.size(); ++i) {
      if (source[i] == '"') {
        inString = !inString;
      } else if (source[i] == ';' && !inString) {
        initEnd = i;
        break;
      }
    }
    if (initEnd == std::string::npos) {
      return "missing marker terminator " + std::string(markerName);
    }

    std::string marker;
    std::size_t searchStart = initStart;
    while (searchStart < initEnd) {
      const std::size_t quoteStart = source.find('"', searchStart);
      if (quoteStart == std::string::npos || quoteStart >= initEnd) {
        break;
      }
      const std::size_t quoteEnd = source.find('"', quoteStart + 1);
      if (quoteEnd == std::string::npos || quoteEnd > initEnd) {
        return "unterminated marker string " + std::string(markerName);
      }
      marker += source.substr(quoteStart + 1, quoteEnd - quoteStart - 1);
      searchStart = quoteEnd + 1;
    }
    return marker;
  };

  const std::string directCallFacts = readTextFile(directCallFactsPath);
  const std::string methodCallFacts = readTextFile(methodCallFactsPath);
  CHECK(extractStructShape(directCallFacts, "SemanticProgramDirectCallTarget") ==
        extractStringViewMarker(directCallFacts, "SemanticProgramDirectCallTargetContractV1Shape"));
  CHECK(extractStructShape(methodCallFacts, "SemanticProgramMethodCallTarget") ==
        extractStringViewMarker(methodCallFacts, "SemanticProgramMethodCallTargetContractV1Shape"));
}


TEST_CASE("compile pipeline publishes an initial semantic product shell") {
  const std::filesystem::path cwd = std::filesystem::current_path();
  const std::filesystem::path root =
      std::filesystem::exists(cwd / "include" / "primec" / "SemanticProduct.h") ? cwd : cwd.parent_path();
  auto readRepoFile = [&](const std::filesystem::path &relativePath) {
    const std::filesystem::path fullPath = root / relativePath;
    REQUIRE(std::filesystem::exists(fullPath));
    return readTextFile(fullPath);
  };

  const std::string cmake = readRepoFile("CMakeLists.txt");
  const std::string semanticProduct = readRepoFile("include/primec/SemanticProduct.h");
  const std::string directCallFacts =
      readRepoFile("include/primec/semantic_product/DirectCallFacts.h");
  const std::string methodCallFacts =
      readRepoFile("include/primec/semantic_product/MethodCallFacts.h");
  const std::string compilePipelineHeader = readRepoFile("include/primec/CompilePipeline.h");
  const std::string semanticsHeader = readRepoFile("include/primec/Semantics.h");
  const std::string irPreparationHeader = readRepoFile("include/primec/IrPreparation.h");
  const std::string irLowererHeader = readRepoFile("include/primec/IrLowerer.h");
  const std::string compilePipelineSource = readRepoFile("src/CompilePipeline.cpp");
  const std::string irPreparationSource = readRepoFile("src/IrPreparation.cpp");
  const std::string irLowererEntrySetup =
      readRepoFile("src/ir_lowerer/IrLowererLowerSetupEntryEffects.h");
  const std::string semanticPublicationBuildersHeader =
      readRepoFile("src/semantics/SemanticPublicationBuilders.h");
  const std::string semanticPublicationSurfaceHeader =
      readRepoFile("src/semantics/SemanticPublicationSurface.h");
  const std::string semanticPublicationBuildersSource =
      readRepoFile("src/semantics/SemanticPublicationBuilders.cpp");
  const std::string semanticPublicationOrchestrationHeader =
      readRepoFile("src/semantics/SemanticsValidationPublicationOrchestration.h");
  const std::string semanticPublicationOrchestrationSource =
      readRepoFile("src/semantics/SemanticsValidationPublicationOrchestration.cpp");
  const std::string semanticProductSource = readRepoFile("src/SemanticProduct.cpp");
  const std::string semanticsValidate = readRepoFile("src/semantics/SemanticsValidate.cpp");
  const std::string semanticsSnapshots = readRepoFile("src/semantics/SemanticsValidatorSnapshots.cpp");
  const std::string irCallResolution = readRepoFile("src/ir_lowerer/IrLowererCallResolution.cpp");
  const std::string irMethodResolution =
      readRepoFile("src/ir_lowerer/IrLowererSetupTypeMethodCallResolution.cpp");
  const std::string bindingTypeHelpersSource =
      readRepoFile("src/ir_lowerer/IrLowererBindingTypeHelpers.cpp");
  const std::string structLayoutHelpersSource =
      readRepoFile("src/ir_lowerer/IrLowererStructLayoutHelpers.cpp");
  const std::string irLowererResultHelpers =
      readRepoFile("src/ir_lowerer/IrLowererResultHelpers.cpp");
  const std::string statementCallHelpersHeader =
      readRepoFile("src/ir_lowerer/IrLowererStatementCallHelpers.h");
  const std::string functionTableStepHeader =
      readRepoFile("src/ir_lowerer/IrLowererLowerStatementsFunctionTableStep.h");
  const std::string callAccessHelpersHeader =
      readRepoFile("include/primec/testing/ir_lowerer_helpers/IrLowererCallAccessHelpers.h");
  const std::string structLayoutHelpersHeader =
      readRepoFile("include/primec/testing/ir_lowerer_helpers/IrLowererStructLayoutHelpers.h");
  const std::string primecMain = readRepoFile("src/main.cpp");
  const std::string primevmMain = readRepoFile("src/primevm_main.cpp");

  CHECK(semanticProduct.find("#include \"primec/semantic_product/DirectCallFacts.h\"") !=
        std::string::npos);
  CHECK(semanticProduct.find("#include \"primec/semantic_product/MethodCallFacts.h\"") !=
        std::string::npos);
  CHECK(semanticProduct.find("SemanticProductContractVersionV2 = 2") !=
        std::string::npos);
  CHECK(semanticProduct.find("enum class SemanticProgramFactOwnership") !=
        std::string::npos);
  CHECK(semanticProduct.find("struct SemanticProgramFactFamilyInfo") !=
        std::string::npos);
  CHECK(directCallFacts.find("struct SemanticProgramDirectCallTarget") != std::string::npos);
  CHECK(methodCallFacts.find("struct SemanticProgramMethodCallTarget") != std::string::npos);
  CHECK(semanticProduct.find("struct SemanticProgramBridgePathChoice") != std::string::npos);
  CHECK(semanticProduct.find("struct SemanticProgramModuleIdentity") != std::string::npos);
  CHECK(semanticProduct.find("struct SemanticProgramModuleResolvedArtifacts") != std::string::npos);
  CHECK(semanticProduct.find("struct SemanticProgramPublishedRoutingLookups") != std::string::npos);
  CHECK(semanticProduct.find("std::unordered_map<SymbolId, std::size_t> definitionIndicesByPathId;") !=
        std::string::npos);
  CHECK(semanticProduct.find("std::unordered_map<SymbolId, SymbolId> importAliasTargetPathIdsByNameId;") !=
        std::string::npos);
  CHECK(semanticProduct.find("std::vector<std::size_t> directCallTargetIndices;") !=
        std::string::npos);
  CHECK(semanticProduct.find("std::vector<std::size_t> methodCallTargetIndices;") !=
        std::string::npos);
  CHECK(semanticProduct.find("std::vector<std::size_t> bridgePathChoiceIndices;") !=
        std::string::npos);
  CHECK(semanticProduct.find("std::vector<std::size_t> callableSummaryIndices;") !=
        std::string::npos);
  CHECK(semanticProduct.find("std::vector<std::size_t> bindingFactIndices;") !=
        std::string::npos);
  CHECK(semanticProduct.find("std::vector<std::size_t> returnFactIndices;") !=
        std::string::npos);
  CHECK(semanticProduct.find("std::vector<std::size_t> localAutoFactIndices;") !=
        std::string::npos);
  CHECK(semanticProduct.find("std::vector<std::size_t> queryFactIndices;") !=
        std::string::npos);
  CHECK(semanticProduct.find("std::vector<std::size_t> tryFactIndices;") !=
        std::string::npos);
  CHECK(semanticProduct.find("std::vector<std::size_t> onErrorFactIndices;") !=
        std::string::npos);
  const size_t moduleArtifactsStart =
      semanticProduct.find("struct SemanticProgramModuleResolvedArtifacts {");
  REQUIRE(moduleArtifactsStart != std::string::npos);
  const size_t moduleArtifactsEnd = semanticProduct.find("};", moduleArtifactsStart);
  REQUIRE(moduleArtifactsEnd != std::string::npos);
  const std::string moduleArtifactsBlock =
      semanticProduct.substr(moduleArtifactsStart, moduleArtifactsEnd - moduleArtifactsStart);
  CHECK(moduleArtifactsBlock.find("std::vector<SemanticProgram") == std::string::npos);
  CHECK(semanticProduct.find("struct SemanticProgramStructFieldMetadata") != std::string::npos);
  CHECK(semanticProduct.find("std::vector<SemanticProgramModuleResolvedArtifacts> moduleResolvedArtifacts;") !=
        std::string::npos);
  CHECK(semanticProduct.find("SemanticProgramPublishedRoutingLookups publishedRoutingLookups;") !=
        std::string::npos);
  CHECK(semanticProduct.find("bool publishedStorageFrozen = false;") != std::string::npos);
  CHECK(semanticProduct.find("struct SemanticProgramPublishedLowererPreflightFacts") !=
        std::string::npos);
  CHECK(semanticProduct.find("SemanticProgramPublishedLowererPreflightFacts publishedLowererPreflightFacts;") !=
        std::string::npos);
  CHECK(semanticProduct.find("std::vector<SemanticProgramReturnFact> returnFacts;") != std::string::npos);
  CHECK(semanticProduct.find("std::vector<SemanticProgramLocalAutoFact> localAutoFacts;") != std::string::npos);
  CHECK(semanticProduct.find("std::vector<SemanticProgramQueryFact> queryFacts;") != std::string::npos);
  CHECK(semanticProduct.find("std::vector<SemanticProgramTryFact> tryFacts;") != std::string::npos);
  CHECK(semanticProduct.find("std::vector<SemanticProgramOnErrorFact> onErrorFacts;") != std::string::npos);
  CHECK(semanticProduct.find("semanticProgramLookupPublishedLowererSoftwareNumericType(") !=
        std::string::npos);
  CHECK(semanticProduct.find("semanticProgramLookupPublishedLowererRuntimeReflectionPath(") !=
        std::string::npos);
  CHECK(semanticProduct.find("semanticProgramLookupPublishedLowererRuntimeReflectionUsesObjectTable(") !=
        std::string::npos);
  CHECK(semanticProduct.find("semanticProgramBindingFactView(const SemanticProgram &semanticProgram);") !=
        std::string::npos);
  CHECK(semanticProduct.find("semanticProgramReturnFactView(const SemanticProgram &semanticProgram);") !=
        std::string::npos);
  CHECK(semanticProduct.find("semanticProgramOnErrorFactView(const SemanticProgram &semanticProgram);") !=
        std::string::npos);
  CHECK(semanticProduct.find("freezeSemanticProgramPublishedStorage(SemanticProgram &semanticProgram);") !=
        std::string::npos);
  CHECK(semanticProduct.find("semanticProgramPublishedStorageFrozen(const SemanticProgram &semanticProgram);") !=
        std::string::npos);
  CHECK(semanticProduct.find("std::string formatSemanticProgram(const SemanticProgram &semanticProgram);") !=
        std::string::npos);
  CHECK(semanticProduct.find("semanticProgramFactFamilyInfos()") !=
        std::string::npos);
  CHECK(semanticProduct.find("semanticProgramFactFamilyOwnership(std::string_view familyName)") !=
        std::string::npos);
  CHECK(semanticProduct.find(
            "semanticProgramFactFamilyIsSemanticProductOwned(std::string_view familyName)") !=
        std::string::npos);
  CHECK(semanticProduct.find(
            "semanticProgramFactFamilyIsAstProvenanceOwned(std::string_view familyName)") !=
        std::string::npos);

  CHECK(compilePipelineHeader.find("SemanticProgram semanticProgram;") != std::string::npos);
  CHECK(compilePipelineHeader.find("CompilePipelineFailure failure;") != std::string::npos);
  CHECK(compilePipelineHeader.find("bool hasSemanticProgram = false;") != std::string::npos);
  CHECK(compilePipelineHeader.find("bool hasFailure = false;") != std::string::npos);
  CHECK(compilePipelineHeader.find("struct CompilePipelineSuccessResult") != std::string::npos);
  CHECK(compilePipelineHeader.find("struct CompilePipelineFailureResult") != std::string::npos);
  CHECK(compilePipelineHeader.find("using CompilePipelineResult =") != std::string::npos);
  CHECK(compilePipelineHeader.find("std::variant<CompilePipelineSuccessResult, CompilePipelineFailureResult>") !=
        std::string::npos);
  CHECK(compilePipelineHeader.find("CompilePipelineResult runCompilePipelineResult(") !=
        std::string::npos);
  CHECK(semanticsHeader.find("SemanticProgram *semanticProgramOut = nullptr") != std::string::npos);
  CHECK(irPreparationHeader.find("const SemanticProgram *semanticProgram,") != std::string::npos);
  CHECK(irLowererHeader.find("const SemanticProgram *semanticProgram,") != std::string::npos);
  CHECK(irLowererHeader.find("return lower(program, nullptr, entryPath, defaultEffects, entryDefaultEffects, out, error, diagnosticInfo);") ==
        std::string::npos);
  CHECK(irPreparationSource.find("semantic product is required for IR preparation") != std::string::npos);
  CHECK(irLowererEntrySetup.find("semantic product is required for IR lowering") != std::string::npos);
  CHECK(cmake.find("src/semantics/SemanticPublicationBuilders.cpp") != std::string::npos);
  CHECK(cmake.find("src/semantics/SemanticsValidationPublicationOrchestration.cpp") !=
        std::string::npos);
  CHECK(cmake.find("src/semantics/SemanticsWorkerSymbolMerge.cpp") != std::string::npos);

  CHECK(compilePipelineSource.find("output.semanticProgram = std::move(semanticProgram);") !=
        std::string::npos);
  CHECK(compilePipelineSource.find("output.hasSemanticProgram = true;") != std::string::npos);
  CHECK(compilePipelineSource.find("output.failure.stage = stage;") != std::string::npos);
  CHECK(compilePipelineSource.find("output.failure.message = message;") != std::string::npos);
  CHECK(compilePipelineSource.find("CompilePipelineSuccessResult{std::move(output)}") !=
        std::string::npos);
  CHECK(compilePipelineSource.find("makeCompilePipelineFailureResult(") != std::string::npos);
  CHECK(compilePipelineSource.find("struct CompilePipelineImportStageState {") !=
        std::string::npos);
  CHECK(compilePipelineSource.find("struct CompilePipelinePreParseStageState {") !=
        std::string::npos);
  CHECK(compilePipelineSource.find("struct CompilePipelineParsedProgramStageState {") !=
        std::string::npos);
  CHECK(compilePipelineSource.find("bool runCompilePipelineImportStage(") !=
        std::string::npos);
  CHECK(compilePipelineSource.find("bool runCompilePipelineTransformStage(") !=
        std::string::npos);
  CHECK(compilePipelineSource.find("bool runCompilePipelineParseStage(") !=
        std::string::npos);
  const size_t preAstDumpPos =
      compilePipelineSource.find("if (dumpStage == DumpStage::PreAst) {");
  const size_t parseStagePos =
      compilePipelineSource.find("if (!runCompilePipelineParseStage(options,");
  REQUIRE(preAstDumpPos != std::string::npos);
  REQUIRE(parseStagePos != std::string::npos);
  CHECK(preAstDumpPos < parseStagePos);
  CHECK(semanticPublicationBuildersHeader.find("buildSemanticProgramFromPublicationSurface(") !=
        std::string::npos);
  CHECK(semanticPublicationBuildersHeader.find("#include \"SemanticPublicationSurface.h\"") !=
        std::string::npos);
  CHECK(semanticPublicationBuildersHeader.find("#include \"SemanticsValidator.h\"") ==
        std::string::npos);
  CHECK(semanticPublicationSurfaceHeader.find("struct SemanticPublicationSurface {") !=
        std::string::npos);
  CHECK(semanticPublicationSurfaceHeader.find("std::vector<std::string> callTargetSeedStrings;") !=
        std::string::npos);
  CHECK(semanticPublicationSurfaceHeader.find("std::vector<CollectedDirectCallTargetEntry> directCallTargets;") !=
        std::string::npos);
  CHECK(semanticPublicationSurfaceHeader.find("std::vector<OnErrorSnapshotEntry> onErrorFacts;") !=
        std::string::npos);
  CHECK(semanticPublicationBuildersSource.find("struct SemanticPublicationBuilderState {") !=
        std::string::npos);
  CHECK(semanticPublicationBuildersSource.find("initializeSemanticProgramPublicationShell(") !=
        std::string::npos);
  CHECK(semanticPublicationBuildersSource.find("preseedSemanticProgramCallTargetStrings(") !=
        std::string::npos);
  CHECK(semanticPublicationBuildersSource.find("publicationSurface.callTargetSeedStrings") !=
        std::string::npos);
  CHECK(semanticPublicationBuildersSource.find("publishRoutingLookupIndexes(") !=
        std::string::npos);
  CHECK(semanticPublicationBuildersSource.find("publishLowererPreflightFacts(") !=
        std::string::npos);
  CHECK(semanticPublicationBuildersSource.find("publishDirectCallTargetFacts(") !=
        std::string::npos);
  CHECK(semanticPublicationBuildersSource.find("publishMethodCallTargetFacts(") !=
        std::string::npos);
  CHECK(semanticPublicationBuildersSource.find("publishBridgePathChoiceFacts(") !=
        std::string::npos);
  CHECK(semanticPublicationBuildersSource.find("publishCallableSummaryFacts(") !=
        std::string::npos);
  CHECK(semanticPublicationBuildersSource.find("publishSemanticRoutingFamilies(") !=
        std::string::npos);
  CHECK(semanticPublicationBuildersSource.find("publishTypeMetadataFacts(") !=
        std::string::npos);
  CHECK(semanticPublicationBuildersSource.find("publishStructFieldMetadataFacts(") !=
        std::string::npos);
  CHECK(semanticPublicationBuildersSource.find("publishSemanticMetadataFamilies(") !=
        std::string::npos);
  CHECK(semanticPublicationBuildersSource.find("publishBindingFacts(") !=
        std::string::npos);
  CHECK(semanticPublicationBuildersSource.find("publishReturnFacts(") !=
        std::string::npos);
  CHECK(semanticPublicationBuildersSource.find("publishLocalAutoFacts(") !=
        std::string::npos);
  CHECK(semanticPublicationBuildersSource.find("publishQueryFacts(") !=
        std::string::npos);
  CHECK(semanticPublicationBuildersSource.find("publishTryFacts(") !=
        std::string::npos);
  CHECK(semanticPublicationBuildersSource.find("publishOnErrorFacts(") !=
        std::string::npos);
  CHECK(semanticPublicationBuildersSource.find("publishSemanticScopedFactFamilies(") !=
        std::string::npos);
  CHECK(semanticPublicationBuildersSource.find("finalizeSemanticModuleArtifacts(") !=
        std::string::npos);
  CHECK(semanticPublicationBuildersSource.find("freezeSemanticProgramPublishedStorage(state.semanticProgram);") !=
        std::string::npos);
  CHECK(semanticsValidate.find("semantics::publishSemanticProgramAfterValidation(program,") !=
        std::string::npos);
  CHECK(semanticsValidate.find("#include \"SemanticsValidationPublicationOrchestration.h\"") !=
        std::string::npos);
  CHECK(semanticsValidate.find("#include \"SemanticPublicationBuilders.h\"") ==
        std::string::npos);
  CHECK(semanticsValidate.find("buildSemanticProgramFromPublicationSurface(") ==
        std::string::npos);
  CHECK(semanticPublicationOrchestrationHeader.find(
            "publishSemanticProgramAfterValidation(") != std::string::npos);
  CHECK(semanticPublicationOrchestrationSource.find(
            "semanticProgramFactCountForValidationPublication(") != std::string::npos);
  CHECK(semanticPublicationOrchestrationSource.find(
            "benchmarkRuntime.phaseCounters->semanticProductBuild.callsVisited = 1;") !=
        std::string::npos);
  CHECK(semanticPublicationBuildersSource.find("normalizeSemanticModulePathKey(") != std::string::npos);
  CHECK(semanticProductSource.find("if (semanticProgram.publishedStorageFrozen) {") !=
        std::string::npos);
  CHECK(semanticProductSource.find("semanticProgramFactFamilyInfos()") !=
        std::string::npos);
  CHECK(semanticProductSource.find("\"definitions\",\n"
                                   "       SemanticProgramFactOwnership::AstProvenance") !=
        std::string::npos);
  CHECK(semanticProductSource.find("\"directCallTargets\",\n"
                                   "       SemanticProgramFactOwnership::SemanticProduct") !=
        std::string::npos);
  CHECK(semanticProductSource.find("\"publishedRoutingLookups\",\n"
                                   "       SemanticProgramFactOwnership::DerivedIndex") !=
        std::string::npos);
  CHECK(semanticProductSource.find("semanticProgramFactFamilyIsSemanticProductOwned(") !=
        std::string::npos);
  CHECK(semanticProductSource.find("semanticProgramFactFamilyIsAstProvenanceOwned(") !=
        std::string::npos);
  CHECK(primecMain.find("releaseSemanticProgramLookupMap(pipelineOutput.semanticProgram)") ==
        std::string::npos);
  CHECK(primevmMain.find("releaseSemanticProgramLookupMap(pipelineOutput.semanticProgram)") ==
        std::string::npos);
  CHECK(semanticPublicationBuildersSource.find("semanticSourceUnitImportMatchesPath(") !=
        std::string::npos);
  CHECK(semanticPublicationBuildersSource.find("semanticSourceUnitModuleKeyForPath(") !=
        std::string::npos);
  CHECK(semanticPublicationBuildersSource.find(
            "semanticSourceUnitImportOrderKeyForModuleKey(") !=
        std::string::npos);
  CHECK(semanticPublicationBuildersSource.find(
            "semanticSourceUnitModuleKeyForPath(\n"
            "        scopePath, semanticProgram.sourceImports, semanticProgram.imports)") !=
        std::string::npos);
  CHECK(semanticPublicationBuildersSource.find("semanticModuleKeyForPath(") == std::string::npos);
  CHECK(semanticPublicationBuildersSource.find("semanticProgram.moduleResolvedArtifacts.reserve(") !=
        std::string::npos);
  CHECK(semanticPublicationBuildersSource.find("ensureModuleResolvedArtifacts(") !=
        std::string::npos);
  CHECK(semanticPublicationBuildersSource.find("std::sort(state.semanticProgram.moduleResolvedArtifacts.begin(),") !=
        std::string::npos);
  CHECK(semanticPublicationBuildersSource.find("left.identity.moduleKey < right.identity.moduleKey") ==
        std::string::npos);
  CHECK(semanticPublicationBuildersSource.find(
            "semanticSourceUnitImportOrderKeyForModuleKey(\n"
            "                      left.identity.moduleKey,") !=
        std::string::npos);
  CHECK(semanticPublicationBuildersSource.find(
            "semanticSourceUnitImportOrderKeyForModuleKey(\n"
            "                      right.identity.moduleKey,") !=
        std::string::npos);
  CHECK(semanticPublicationBuildersSource.find("if (leftImportOrder != rightImportOrder)") !=
        std::string::npos);
  CHECK(semanticPublicationBuildersSource.find("return leftImportOrder < rightImportOrder;") !=
        std::string::npos);
  CHECK(semanticPublicationBuildersSource.find("ensureModuleResolvedArtifacts(snapshotEntry.scopePath)\n        .directCallTargetIndices.push_back(entryIndex);") !=
        std::string::npos);
  CHECK(semanticPublicationBuildersSource.find("ensureModuleResolvedArtifacts(snapshotEntry.scopePath)\n        .methodCallTargetIndices.push_back(entryIndex);") !=
        std::string::npos);
  CHECK(semanticPublicationBuildersSource.find("ensureModuleResolvedArtifacts(snapshotEntry.scopePath)\n        .bridgePathChoiceIndices.push_back(entryIndex);") !=
        std::string::npos);
  CHECK(semanticPublicationBuildersSource.find("ensureModuleResolvedArtifacts(snapshotEntry.fullPath)\n        .callableSummaryIndices.push_back(entryIndex);") !=
        std::string::npos);
  CHECK(semanticPublicationBuildersSource.find("state.semanticProgram.publishedRoutingLookups.directCallTargetIdsByExpr.insert_or_assign(") !=
        std::string::npos);
  CHECK(semanticPublicationBuildersSource.find("state.semanticProgram.publishedRoutingLookups.methodCallTargetIdsByExpr.insert_or_assign(") !=
        std::string::npos);
  CHECK(semanticPublicationBuildersSource.find("state.semanticProgram.publishedRoutingLookups.bridgePathChoiceIdsByExpr.insert_or_assign(") !=
        std::string::npos);
  CHECK(semanticPublicationBuildersSource.find("state.semanticProgram.publishedRoutingLookups.directCallStdlibSurfaceIdsByExpr.insert_or_assign(") !=
        std::string::npos);
  CHECK(semanticPublicationBuildersSource.find("state.semanticProgram.publishedRoutingLookups.methodCallStdlibSurfaceIdsByExpr.insert_or_assign(") !=
        std::string::npos);
  CHECK(semanticPublicationBuildersSource.find("state.semanticProgram.publishedRoutingLookups.bridgePathChoiceStdlibSurfaceIdsByExpr") !=
        std::string::npos);
  CHECK(semanticPublicationBuildersSource.find("routingLookups.definitionIndicesByPathId.insert_or_assign(") !=
        std::string::npos);
  CHECK(semanticPublicationBuildersSource.find("routingLookups.importAliasTargetPathIdsByNameId.try_emplace(") !=
        std::string::npos);
  CHECK(semanticPublicationBuildersSource.find("callableSummaryIndicesByPathId.try_emplace(") !=
        std::string::npos);
  CHECK(semanticPublicationBuildersSource.find("callableSummaryIndicesByPathId.insert_or_assign(") !=
        std::string::npos);
  CHECK(semanticPublicationBuildersSource.find("state.semanticProgram.publishedRoutingLookups.localAutoFactIndicesByExpr.insert_or_assign(") !=
        std::string::npos);
  CHECK(semanticPublicationBuildersSource.find("state.semanticProgram.publishedRoutingLookups.localAutoFactIndicesByInitPathAndBindingNameId") !=
        std::string::npos);
  CHECK(semanticPublicationBuildersSource.find("state.semanticProgram.publishedRoutingLookups.queryFactIndicesByExpr.insert_or_assign(") !=
        std::string::npos);
  CHECK(semanticPublicationBuildersSource.find("state.semanticProgram.publishedRoutingLookups.queryFactIndicesByResolvedPathAndCallNameId") !=
        std::string::npos);
  CHECK(semanticPublicationBuildersSource.find("state.semanticProgram.publishedRoutingLookups.tryFactIndicesByExpr.insert_or_assign(") !=
        std::string::npos);
  CHECK(semanticPublicationBuildersSource.find("state.semanticProgram.publishedRoutingLookups.tryFactIndicesByOperandPathAndSource") !=
        std::string::npos);
  CHECK(semanticPublicationBuildersSource.find("state.semanticProgram.publishedRoutingLookups.onErrorFactIndicesByDefinitionId.insert_or_assign(") !=
        std::string::npos);
  CHECK(semanticPublicationBuildersSource.find("state.semanticProgram.publishedRoutingLookups.onErrorFactIndicesByDefinitionPathId") !=
        std::string::npos);
  CHECK(semanticPublicationBuildersSource.find("facts.firstSoftwareNumericTypeId =\n            semanticProgramInternCallTargetString(state.semanticProgram, found);") !=
        std::string::npos);
  CHECK(semanticPublicationBuildersSource.find("facts.firstRuntimeReflectionPathId =\n        semanticProgramInternCallTargetString(state.semanticProgram, found);") !=
        std::string::npos);
  CHECK(semanticPublicationBuildersSource.find("facts.firstRuntimeReflectionPathIsObjectTable = isRuntimeReflectionPath(found);") !=
        std::string::npos);
  CHECK(semanticPublicationBuildersSource.find("auto &module = state.ensureModuleResolvedArtifacts(moduleScopePath);") !=
        std::string::npos);
  CHECK(semanticPublicationBuildersSource.find("module.bindingFactIndices.push_back(entryIndex);") !=
        std::string::npos);
  CHECK(semanticPublicationBuildersSource.find(
            "entry.initializerStdlibSurfaceId =\n        classifyPublishedStdlibSurfaceId(snapshotEntry.initializerResolvedPath);") !=
        std::string::npos);
  CHECK(semanticPublicationBuildersSource.find(
            "entry.initializerDirectCallStdlibSurfaceId =\n        classifyPublishedStdlibSurfaceId(snapshotEntry.initializerDirectCallResolvedPath);") !=
        std::string::npos);
  CHECK(semanticPublicationBuildersSource.find(
            "entry.initializerMethodCallStdlibSurfaceId =\n        classifyPublishedStdlibSurfaceId(snapshotEntry.initializerMethodCallResolvedPath);") !=
        std::string::npos);
  CHECK(semanticPublicationBuildersSource.find("ensureModuleResolvedArtifacts(entry.scopePath).directCallTargets.push_back(entry);") ==
        std::string::npos);
  CHECK(semanticPublicationBuildersSource.find("ensureModuleResolvedArtifacts(entry.scopePath).methodCallTargets.push_back(entry);") ==
        std::string::npos);
  CHECK(semanticPublicationBuildersSource.find("ensureModuleResolvedArtifacts(storedEntry.scopePath).bridgePathChoices.push_back(storedEntry);") ==
        std::string::npos);
  CHECK(semanticPublicationBuildersSource.find("ensureModuleResolvedArtifacts(snapshotEntry.fullPath).callableSummaries.push_back(") ==
        std::string::npos);
  CHECK(semanticPublicationBuildersSource.find("ensureModuleResolvedArtifacts(storedEntry.scopePath).bindingFacts.push_back(storedEntry);") ==
        std::string::npos);
  CHECK(semanticsSnapshots.find("forEachResolvedNonMethodCallSnapshot(") != std::string::npos);
  CHECK(semanticsSnapshots.find("collectPilotRoutingSemanticProductFacts()") !=
        std::string::npos);
  CHECK(semanticsSnapshots.find("findStdlibSurfaceMetadataByResolvedPath(resolvedPath)") !=
        std::string::npos);
  CHECK(semanticsSnapshots.find("resolveStdlibSurfaceMemberName(*metadata, resolvedPath)") !=
        std::string::npos);
  CHECK(semanticsSnapshots.find("return std::pair<std::string, std::string>(\"soa_vector\"") !=
        std::string::npos);
  CHECK(semanticsSnapshots.find("/std/collections/experimental_soa_vector/") !=
        std::string::npos);
  CHECK(semanticsSnapshots.find("takeSemanticPublicationSurfaceForSemanticProduct(") !=
        std::string::npos);
  CHECK(semanticsSnapshots.find("takeCollectedBridgePathChoicesForSemanticProduct()") !=
        std::string::npos);
  CHECK(semanticsSnapshots.find(
            "          const bool hasMethodCallInitializer =\n"
            "              expr.args.size() == 1 && expr.args.front().isMethodCall;") !=
        std::string::npos);
  CHECK(semanticsSnapshots.find(
            "          if (!fact.methodCallResolvedPath.empty()) {\n"
            "            initializerMethodCallResolvedPath = fact.methodCallResolvedPath;\n"
            "          } else if (hasMethodCallInitializer && !fact.initializerResolvedPath.empty()) {\n"
            "            initializerMethodCallResolvedPath = fact.initializerResolvedPath;\n"
            "          }") !=
        std::string::npos);
  CHECK(semanticsSnapshots.find(
            "          if (fact.hasMethodCallReturnKind) {\n"
            "            initializerMethodCallReturnKind = fact.methodCallReturnKind;\n"
            "          } else if (hasMethodCallInitializer && fact.hasInitializerBinding) {\n"
            "            initializerMethodCallReturnKind = returnKindForTypeName(\n"
            "                normalizeBindingTypeName(fact.initializerBinding.typeName));\n"
            "          }") !=
        std::string::npos);
  CHECK(semanticsSnapshots.find("forEachResolvedNonMethodCallSnapshot(") <
        semanticsSnapshots.find("collectPilotRoutingSemanticProductFacts()"));
  CHECK(semanticsSnapshots.find("forEachResolvedNonMethodCallSnapshot(") <
        semanticsSnapshots.find("takeCollectedBridgePathChoicesForSemanticProduct()"));
  CHECK(semanticsValidate.find("semantics::assignSemanticNodeIds(program);") != std::string::npos);
  CHECK(semanticsValidate.find("validator.invalidatePilotRoutingSemanticCollectors();") !=
        std::string::npos);
  CHECK(semanticsValidate.find("semantics::assignSemanticNodeIds(program);") <
        semanticsValidate.find("validator.invalidatePilotRoutingSemanticCollectors();"));
  CHECK(semanticPublicationBuildersSource.find("ensureModuleResolvedArtifacts(snapshotEntry.definitionPath).returnFactIndices.push_back(") !=
        std::string::npos);
  CHECK(semanticPublicationBuildersSource.find("ensureModuleResolvedArtifacts(snapshotEntry.scopePath).localAutoFactIndices.push_back(") !=
        std::string::npos);
  CHECK(semanticPublicationBuildersSource.find("module.queryFactIndices.push_back(entryIndex);") !=
        std::string::npos);
  CHECK(semanticPublicationBuildersSource.find("ensureModuleResolvedArtifacts(snapshotEntry.scopePath).tryFactIndices.push_back(") !=
        std::string::npos);
  CHECK(semanticPublicationBuildersSource.find("ensureModuleResolvedArtifacts(snapshotEntry.definitionPath).onErrorFactIndices.push_back(") !=
        std::string::npos);
  CHECK(semanticPublicationBuildersSource.find("ensureModuleResolvedArtifacts(snapshotEntry.definitionPath).returnFacts.push_back(entry);") ==
        std::string::npos);
  CHECK(semanticPublicationBuildersSource.find("ensureModuleResolvedArtifacts(snapshotEntry.scopePath).localAutoFacts.push_back(entry);") ==
        std::string::npos);
  CHECK(semanticPublicationBuildersSource.find("ensureModuleResolvedArtifacts(snapshotEntry.scopePath).queryFacts.push_back(entry);") ==
        std::string::npos);
  CHECK(semanticPublicationBuildersSource.find("ensureModuleResolvedArtifacts(storedEntry.definitionPath).returnFacts.push_back(storedEntry);") ==
        std::string::npos);
  CHECK(semanticPublicationBuildersSource.find("ensureModuleResolvedArtifacts(storedEntry.scopePath).localAutoFacts.push_back(storedEntry);") ==
        std::string::npos);
  CHECK(semanticPublicationBuildersSource.find("ensureModuleResolvedArtifacts(storedEntry.scopePath).queryFacts.push_back(storedEntry);") ==
        std::string::npos);
  CHECK(semanticPublicationBuildersSource.find("ensureModuleResolvedArtifacts(storedEntry.scopePath).tryFacts.push_back(storedEntry);") ==
        std::string::npos);
  CHECK(semanticPublicationBuildersSource.find("ensureModuleResolvedArtifacts(storedEntry.definitionPath).onErrorFacts.push_back(storedEntry);") ==
        std::string::npos);

  CHECK(irCallResolution.find("bool validateSemanticProductDirectCallCoverage(") !=
        std::string::npos);
  CHECK(irCallResolution.find("std::string resolveCallPathFromPublishedLookups(const Expr &expr,") !=
        std::string::npos);
  CHECK(irCallResolution.find("bool resolvesToPublishedDefinitionFamilyTarget(const SemanticProgram *semanticProgram,") !=
        std::string::npos);
  CHECK(irCallResolution.find("bridgePathChoicesByExpr.contains(expr.semanticNodeId)") !=
        std::string::npos);
  CHECK(irCallResolution.find("bool isPublishedCollectionBridgeStdlibSurfaceId(") !=
        std::string::npos);
  CHECK(irCallResolution.find("findSemanticProductDirectCallStdlibSurfaceId(semanticProgram, expr)") !=
        std::string::npos);
  CHECK(irCallResolution.find("findSemanticProductBridgePathChoiceStdlibSurfaceId(semanticProgram, expr)") !=
        std::string::npos);
  CHECK(irCallResolution.find("resolvePublishedSemanticStdlibSurfaceMemberName(") !=
        std::string::npos);
  CHECK(irCallResolution.find("StdlibSurfaceId::CollectionsVectorHelpers") !=
        std::string::npos);
  CHECK(irCallResolution.find("StdlibSurfaceId::CollectionsMapHelpers") !=
        std::string::npos);
  CHECK(irCallResolution.find("StdlibSurfaceId::CollectionsSoaVectorHelpers") !=
        std::string::npos);
  CHECK(irCallResolution.find("const std::string resolvedPath = resolveCallPathFromPublishedLookups(expr, semanticProgram);") !=
        std::string::npos);
  CHECK(irCallResolution.find("!isBridgeHelperCall(semanticProgram, expr, resolvedPath) &&") !=
        std::string::npos);
  CHECK(irCallResolution.find("!resolvesToPublishedDefinitionFamilyTarget(semanticProgram, resolvedPath)") !=
        std::string::npos);
  CHECK(irCallResolution.find("missing semantic-product direct-call target: ") != std::string::npos);
  CHECK(irCallResolution.find("bool validateSemanticProductBridgePathCoverage(") !=
        std::string::npos);
  CHECK(irCallResolution.find("missing semantic-product bridge helper name id: ") !=
        std::string::npos);
  CHECK(irCallResolution.find("missing semantic-product bridge-path choice: ") != std::string::npos);
  CHECK(irCallResolution.find("const std::string fallbackResolvedPath =\n          resolveCallPathFromPublishedLookups(expr, semanticProgram);") ==
        std::string::npos);
  CHECK(irCallResolution.find("isResidualBridgeHelperPath(resolvedPath)") !=
        std::string::npos);
  CHECK(irCallResolution.find("isResidualBridgeHelperPath(fallbackResolvedPath) &&") ==
        std::string::npos);
  CHECK(irCallResolution.find("resolvesToPublishedDefinitionFamilyTarget(semanticProgram, fallbackResolvedPath)") ==
        std::string::npos);
  CHECK(irCallResolution.find("bool validateSemanticProductMethodCallCoverage(const Program &program,") !=
        std::string::npos);
  CHECK(irCallResolution.find("missing semantic-product method-call resolved path id: ") !=
        std::string::npos);
  CHECK(irCallResolution.find("missing semantic-product method-call target: ") != std::string::npos);
  CHECK(bindingTypeHelpersSource.find("bool validateSemanticProductBindingCoverage(const Program &program,") !=
        std::string::npos);
  CHECK(bindingTypeHelpersSource.find("missing semantic-product binding fact: ") != std::string::npos);
  CHECK(bindingTypeHelpersSource.find("missing semantic-product binding resolved path id: ") !=
        std::string::npos);
  CHECK(bindingTypeHelpersSource.find(
            "bool validateSemanticProductCollectionSpecializationCoverage(") !=
        std::string::npos);
  CHECK(bindingTypeHelpersSource.find(
            "missing semantic-product collection specialization: ") !=
        std::string::npos);
  CHECK(bindingTypeHelpersSource.find("bool validateSemanticProductLocalAutoCoverage(const Program &program,") !=
        std::string::npos);
  CHECK(bindingTypeHelpersSource.find("findSemanticProductLocalAutoFactBySemanticId(semanticIndex, expr)") !=
        std::string::npos);
  CHECK(bindingTypeHelpersSource.find("missing semantic-product local-auto fact: ") != std::string::npos);
  CHECK(bindingTypeHelpersSource.find("missing semantic-product local-auto initializer path id: ") !=
        std::string::npos);
  CHECK(structLayoutHelpersSource.find("bool validateSemanticProductStructLayoutCoverage(const Program &program,") !=
        std::string::npos);
  CHECK(structLayoutHelpersSource.find("missing semantic-product type metadata: ") !=
        std::string::npos);
  CHECK(structLayoutHelpersSource.find("missing semantic-product struct provenance: ") !=
        std::string::npos);
  CHECK(irLowererResultHelpers.find("missing semantic-product return definition path id") !=
        std::string::npos);
  CHECK(irLowererResultHelpers.find("missing semantic-product query resolved path id: ") !=
        std::string::npos);
  CHECK(irLowererResultHelpers.find("missing semantic-product try operand resolved path id: try") !=
        std::string::npos);
  CHECK(irMethodResolution.find("const auto &semanticAwareImportAliases =") != std::string::npos);
  CHECK(statementCallHelpersHeader.find("const SemanticProgram *semanticProgram,") !=
        std::string::npos);
  CHECK(functionTableStepHeader.find("const SemanticProgram *semanticProgram = nullptr;") !=
        std::string::npos);
  CHECK(callAccessHelpersHeader.find("bool emitMapLookupContains(") != std::string::npos);
  CHECK(callAccessHelpersHeader.find("bool emitBuiltinCanonicalMapInsertOverwriteOrGrow(") !=
        std::string::npos);
  CHECK(structLayoutHelpersHeader.find("const ::primec::SemanticProgram *semanticProgram,") !=
        std::string::npos);

  CHECK(primecMain.find("std::get_if<primec::CompilePipelineFailureResult>(&runResult)") !=
        std::string::npos);
  CHECK(primecMain.find("describeCompilePipelineFailure(*pipelineFailure)") !=
        std::string::npos);
  CHECK(primecMain.find("describeCompilePipelineFailure(pipelineOutput)") ==
        std::string::npos);
  CHECK(primevmMain.find("std::get_if<primec::CompilePipelineFailureResult>(&pipelineResult)") !=
        std::string::npos);
  CHECK(primevmMain.find("describeCompilePipelineFailure(*pipelineFailure)") !=
        std::string::npos);
}

TEST_CASE("semantic snapshot shared traversal keeps direct and bridge ordering keys") {
  const std::filesystem::path cwd = std::filesystem::current_path();
  const std::filesystem::path root =
      std::filesystem::exists(cwd / "src" / "semantics" / "SemanticsValidatorSnapshots.cpp")
          ? cwd
          : cwd.parent_path();
  const std::string semanticsSnapshots =
      readTextFile(root / "src" / "semantics" / "SemanticsValidatorSnapshots.cpp");

  const std::size_t collectStart =
      semanticsSnapshots.find("SemanticsValidator::collectPilotRoutingSemanticProductFacts() {");
  const std::size_t takeSurfaceStart =
      semanticsSnapshots.find("SemanticsValidator::takeSemanticPublicationSurfaceForSemanticProduct(");
  const std::size_t directTakeStart =
      semanticsSnapshots.find("SemanticsValidator::takeCollectedDirectCallTargetsForSemanticProduct() {");
  const std::size_t bridgeTakeStart =
      semanticsSnapshots.find("SemanticsValidator::takeCollectedBridgePathChoicesForSemanticProduct() {");
  const std::size_t callableTakeStart =
      semanticsSnapshots.find("SemanticsValidator::takeCollectedCallableSummariesForSemanticProduct() {");
  REQUIRE(collectStart != std::string::npos);
  REQUIRE(takeSurfaceStart != std::string::npos);
  REQUIRE(directTakeStart != std::string::npos);
  REQUIRE(bridgeTakeStart != std::string::npos);
  REQUIRE(callableTakeStart != std::string::npos);
  REQUIRE(collectStart < takeSurfaceStart);
  REQUIRE(directTakeStart < bridgeTakeStart);
  REQUIRE(bridgeTakeStart < callableTakeStart);
  REQUIRE(callableTakeStart < takeSurfaceStart);

  const std::string collectBody = semanticsSnapshots.substr(collectStart, takeSurfaceStart - collectStart);
  CHECK(semanticsSnapshots.find("forEachResolvedNonMethodCallSnapshot(") != std::string::npos);
  CHECK(collectBody.find(
            "      std::string resolvedPath = preferredCollectionHelperResolvedPath(expr);\n"
            "      if (resolvedPath.empty()) {\n"
            "        resolvedPath = resolveCalleePath(expr);\n"
            "      }") !=
        std::string::npos);
  CHECK(collectBody.find(
            "        std::string canonicalResolvedPath = resolvedPath;\n"
            "        if (const size_t suffix = canonicalResolvedPath.find(\"__t\");\n"
            "            suffix != std::string::npos) {\n"
            "          canonicalResolvedPath.erase(suffix);\n"
            "        }\n"
            "        canonicalResolvedPath =\n"
            "            canonicalizeLegacySoaGetHelperPath(canonicalResolvedPath);\n"
            "        canonicalResolvedPath =\n"
            "            canonicalizeLegacySoaRefHelperPath(canonicalResolvedPath);\n"
            "        canonicalResolvedPath =\n"
            "            canonicalizeLegacySoaToAosHelperPath(canonicalResolvedPath);\n"
            "        if (!canonicalResolvedPath.empty()) {\n"
            "          resolvedPath = std::move(canonicalResolvedPath);\n"
            "        }\n") !=
        std::string::npos);
  CHECK(collectBody.find(
            "      std::string resolvedPath = resolveCalleePath(expr);\n"
            "      if (const std::string preferredCollectionPath =\n"
            "              preferredCollectionHelperResolvedPath(expr);\n"
            "          !preferredCollectionPath.empty()) {\n"
            "        resolvedPath = preferredCollectionPath;\n"
            "      }") ==
        std::string::npos);
  CHECK(semanticsSnapshots.find(
            "          const bool hasDirectCallInitializer =\n"
            "              expr.args.size() == 1 &&\n"
            "              expr.args.front().kind == Expr::Kind::Call &&\n"
            "              !expr.args.front().isMethodCall;") !=
        std::string::npos);
  CHECK(semanticsSnapshots.find(
            "          if (!fact.directCallResolvedPath.empty()) {\n"
            "            initializerDirectCallResolvedPath = fact.directCallResolvedPath;\n"
            "          } else if (hasDirectCallInitializer && !fact.initializerResolvedPath.empty()) {\n"
            "            initializerDirectCallResolvedPath = fact.initializerResolvedPath;\n"
            "          }") !=
        std::string::npos);
  CHECK(semanticsSnapshots.find(
            "          if (fact.hasDirectCallReturnKind) {\n"
            "            initializerDirectCallReturnKind = fact.directCallReturnKind;\n"
            "          } else if (hasDirectCallInitializer && fact.hasInitializerBinding) {\n"
            "            initializerDirectCallReturnKind = returnKindForTypeName(\n"
            "                normalizeBindingTypeName(fact.initializerBinding.typeName));\n"
            "          }") !=
        std::string::npos);
  CHECK(collectBody.find("collectedDirectCallTargets_.push_back(") != std::string::npos);
  CHECK(collectBody.find("QuerySnapshotData queryData;") != std::string::npos);
  CHECK(collectBody.find("inferQuerySnapshotData(defParams, activeLocals, expr, queryData)") !=
        std::string::npos);
  CHECK(collectBody.find("std::move(queryData.receiverBinding)") != std::string::npos);
  CHECK(collectBody.find("resolveMethodTarget(defParams,") == std::string::npos);
  CHECK(collectBody.find("collectedBridgePathChoices_.push_back(") != std::string::npos);
  CHECK(collectBody.find("std::stable_sort(collectedDirectCallTargets_.begin(),") != std::string::npos);
  CHECK(collectBody.find("std::stable_sort(collectedBridgePathChoices_.begin(),") != std::string::npos);
  CHECK(collectBody.find("if (left.scopePath != right.scopePath)") != std::string::npos);
  CHECK(collectBody.find("if (left.sourceLine != right.sourceLine)") != std::string::npos);
  CHECK(collectBody.find("if (left.sourceColumn != right.sourceColumn)") != std::string::npos);
  CHECK(collectBody.find("if (left.callName != right.callName)") != std::string::npos);
  CHECK(collectBody.find("if (left.collectionFamily != right.collectionFamily)") != std::string::npos);
  CHECK(collectBody.find("if (left.helperName != right.helperName)") != std::string::npos);
  CHECK(collectBody.find("return left.resolvedPath < right.resolvedPath;") != std::string::npos);
  CHECK(collectBody.find("return left.chosenPath < right.chosenPath;") != std::string::npos);

  const std::string takeSurfaceBody =
      semanticsSnapshots.substr(takeSurfaceStart, directTakeStart - takeSurfaceStart);
  CHECK(takeSurfaceBody.find("collectPilotRoutingSemanticProductFacts();") != std::string::npos);
  CHECK(takeSurfaceBody.find("surface.directCallTargets = std::exchange(collectedDirectCallTargets_, {});") !=
        std::string::npos);
  CHECK(takeSurfaceBody.find("surface.methodCallTargets = std::exchange(collectedMethodCallTargets_, {});") !=
        std::string::npos);
  CHECK(takeSurfaceBody.find("surface.bridgePathChoices = std::exchange(collectedBridgePathChoices_, {});") !=
        std::string::npos);
  CHECK(takeSurfaceBody.find("surface.callableSummaries = std::exchange(collectedCallableSummaries_, {});") !=
        std::string::npos);
  CHECK(takeSurfaceBody.find("invalidatePilotRoutingSemanticCollectors();") != std::string::npos);
  CHECK(takeSurfaceBody.find("surface.bindingFacts = bindingFactSnapshotForSemanticProduct();") !=
        std::string::npos);
  CHECK(takeSurfaceBody.find("surface.returnFacts = returnFactSnapshotForSemanticProduct();") !=
        std::string::npos);
  CHECK(takeSurfaceBody.find("surface.localAutoFacts = localAutoFactSnapshotForSemanticProduct();") !=
        std::string::npos);
  CHECK(takeSurfaceBody.find("surface.queryFacts = queryFactSnapshotForSemanticProduct();") !=
        std::string::npos);
  CHECK(takeSurfaceBody.find("surface.tryFacts = tryFactSnapshotForSemanticProduct();") !=
        std::string::npos);
  CHECK(takeSurfaceBody.find("surface.onErrorFacts = onErrorFactSnapshotForSemanticProduct();") !=
        std::string::npos);
  CHECK(takeSurfaceBody.find("releaseTransientSnapshotCaches();") != std::string::npos);

  const std::string directTakeBody = semanticsSnapshots.substr(directTakeStart, bridgeTakeStart - directTakeStart);
  CHECK(directTakeBody.find("collectPilotRoutingSemanticProductFacts();") != std::string::npos);
  CHECK(directTakeBody.find("return std::exchange(collectedDirectCallTargets_, {});") != std::string::npos);

  const std::string bridgeTakeBody = semanticsSnapshots.substr(bridgeTakeStart, callableTakeStart - bridgeTakeStart);
  CHECK(bridgeTakeBody.find("collectPilotRoutingSemanticProductFacts();") != std::string::npos);
  CHECK(bridgeTakeBody.find("return std::exchange(collectedBridgePathChoices_, {});") != std::string::npos);
}

TEST_CASE("semantic product bridge path choices use helperNameId without helperName shadow field") {
  const std::filesystem::path cwd = std::filesystem::current_path();
  const std::filesystem::path root =
      std::filesystem::exists(cwd / "include" / "primec" / "SemanticProduct.h")
          ? cwd
          : cwd.parent_path();
  const std::string semanticProductHeader =
      readTextFile(root / "include" / "primec" / "SemanticProduct.h");
  const std::string semanticProductSource =
      readTextFile(root / "src" / "SemanticProduct.cpp");

  const std::size_t bridgeStart =
      semanticProductHeader.find("struct SemanticProgramBridgePathChoice {");
  const std::size_t callableStart =
      semanticProductHeader.find("struct SemanticProgramCallableSummary {");
  REQUIRE(bridgeStart != std::string::npos);
  REQUIRE(callableStart != std::string::npos);
  REQUIRE(bridgeStart < callableStart);

  const std::string bridgeBody =
      semanticProductHeader.substr(bridgeStart, callableStart - bridgeStart);
  CHECK(bridgeBody.find("SymbolId helperNameId = InvalidSymbolId;") != std::string::npos);
  CHECK(bridgeBody.find("std::string helperName;") == std::string::npos);

  CHECK(semanticProductSource.find("semanticProgramBridgePathChoiceHelperName(") !=
        std::string::npos);
  CHECK(semanticProductSource.find("helperName.empty() ? entry.helperName") ==
        std::string::npos);
  const std::string irCallResolutionSource =
      readTextFile(root / "src" / "ir_lowerer" / "IrLowererCallResolution.cpp");
  CHECK(irCallResolutionSource.find("entry.helperNameId == InvalidSymbolId") !=
        std::string::npos);
}

TEST_CASE("semantic product method call targets use resolvedPathId without resolvedPath shadow field") {
  const std::filesystem::path cwd = std::filesystem::current_path();
  const std::filesystem::path root =
      std::filesystem::exists(cwd / "include" / "primec" / "SemanticProduct.h")
          ? cwd
          : cwd.parent_path();
  const std::string methodCallFactsHeader =
      readTextFile(root / "include" / "primec" / "semantic_product" / "MethodCallFacts.h");
  const std::string semanticProductSource =
      readTextFile(root / "src" / "SemanticProduct.cpp");
  const std::string irCallResolutionSource =
      readTextFile(root / "src" / "ir_lowerer" / "IrLowererCallResolution.cpp");

  const std::size_t methodStart =
      methodCallFactsHeader.find("struct SemanticProgramMethodCallTarget {");
  const std::size_t viewStart =
      methodCallFactsHeader.find("semanticProgramMethodCallTargetView(");
  REQUIRE(methodStart != std::string::npos);
  REQUIRE(viewStart != std::string::npos);
  REQUIRE(methodStart < viewStart);

  const std::string methodBody =
      methodCallFactsHeader.substr(methodStart, viewStart - methodStart);
  CHECK(methodBody.find("SymbolId resolvedPathId = InvalidSymbolId;") !=
        std::string::npos);
  CHECK(methodBody.find("std::string resolvedPath;") == std::string::npos);

  CHECK(semanticProductSource.find("semanticProgramMethodCallTargetResolvedPath(") !=
        std::string::npos);
  CHECK(semanticProductSource.find("resolvedPath.empty() ? entry.resolvedPath") ==
        std::string::npos);
  CHECK(irCallResolutionSource.find("missing semantic-product method-call resolved path id: ") !=
        std::string::npos);
}

TEST_CASE("semantic product callable summaries use fullPathId without fullPath shadow field") {
  const std::filesystem::path cwd = std::filesystem::current_path();
  const std::filesystem::path root =
      std::filesystem::exists(cwd / "include" / "primec" / "SemanticProduct.h")
          ? cwd
          : cwd.parent_path();
  const std::string semanticProductHeader =
      readTextFile(root / "include" / "primec" / "SemanticProduct.h");
  const std::string semanticProductSource =
      readTextFile(root / "src" / "SemanticProduct.cpp");
  const std::string irLowererResultHelpers =
      readTextFile(root / "src" / "ir_lowerer" / "IrLowererResultHelpers.cpp");
  const std::string irLowererReturnInfoHelpers =
      readTextFile(root / "src" / "ir_lowerer" / "IrLowererLowerInferenceReturnInfoHelpers.cpp");

  const std::size_t callableStart =
      semanticProductHeader.find("struct SemanticProgramCallableSummary {");
  const std::size_t typeMetadataStart =
      semanticProductHeader.find("struct SemanticProgramTypeMetadata {");
  REQUIRE(callableStart != std::string::npos);
  REQUIRE(typeMetadataStart != std::string::npos);
  REQUIRE(callableStart < typeMetadataStart);

  const std::string callableBody =
      semanticProductHeader.substr(callableStart, typeMetadataStart - callableStart);
  CHECK(callableBody.find("SymbolId fullPathId = InvalidSymbolId;") !=
        std::string::npos);
  CHECK(callableBody.find("std::string fullPath;") == std::string::npos);

  CHECK(semanticProductSource.find("semanticProgramCallableSummaryFullPath(") !=
        std::string::npos);
  CHECK(semanticProductSource.find("fullPath.empty() ? entry.fullPath") ==
        std::string::npos);

  CHECK(irLowererResultHelpers.find("summary->fullPathId == InvalidSymbolId") !=
        std::string::npos);
  CHECK(irLowererReturnInfoHelpers.find("entry->fullPathId == InvalidSymbolId || callablePath.empty()") !=
        std::string::npos);
}

TEST_CASE("semantic product binding facts use resolvedPathId without resolvedPath shadow field") {
  const std::filesystem::path cwd = std::filesystem::current_path();
  const std::filesystem::path root =
      std::filesystem::exists(cwd / "include" / "primec" / "SemanticProduct.h")
          ? cwd
          : cwd.parent_path();
  const std::string semanticProductHeader =
      readTextFile(root / "include" / "primec" / "SemanticProduct.h");
  const std::string semanticProductSource =
      readTextFile(root / "src" / "SemanticProduct.cpp");
  const std::string bindingTypeHelpersSource =
      readTextFile(root / "src" / "ir_lowerer" / "IrLowererBindingTypeHelpers.cpp");

  const std::size_t bindingStart =
      semanticProductHeader.find("struct SemanticProgramBindingFact {");
  const std::size_t returnStart =
      semanticProductHeader.find("struct SemanticProgramReturnFact {");
  REQUIRE(bindingStart != std::string::npos);
  REQUIRE(returnStart != std::string::npos);
  REQUIRE(bindingStart < returnStart);

  const std::string bindingBody =
      semanticProductHeader.substr(bindingStart, returnStart - bindingStart);
  CHECK(bindingBody.find("SymbolId resolvedPathId = InvalidSymbolId;") !=
        std::string::npos);
  CHECK(bindingBody.find("std::string resolvedPath;") == std::string::npos);

  CHECK(semanticProductSource.find("semanticProgramBindingFactResolvedPath(") !=
        std::string::npos);
  CHECK(semanticProductSource.find("resolvedPath.empty() ? entry.resolvedPath") ==
        std::string::npos);

  CHECK(bindingTypeHelpersSource.find("bindingFact->resolvedPathId == InvalidSymbolId ||") !=
        std::string::npos);
}

TEST_CASE("semantic product return facts use definitionPathId without definitionPath shadow field") {
  const std::filesystem::path cwd = std::filesystem::current_path();
  const std::filesystem::path root =
      std::filesystem::exists(cwd / "include" / "primec" / "SemanticProduct.h")
          ? cwd
          : cwd.parent_path();
  const std::string semanticProductHeader =
      readTextFile(root / "include" / "primec" / "SemanticProduct.h");
  const std::string semanticProductSource =
      readTextFile(root / "src" / "SemanticProduct.cpp");
  const std::string irLowererResultHelpers =
      readTextFile(root / "src" / "ir_lowerer" / "IrLowererResultHelpers.cpp");

  const std::size_t returnStart =
      semanticProductHeader.find("struct SemanticProgramReturnFact {");
  const std::size_t localAutoStart =
      semanticProductHeader.find("struct SemanticProgramLocalAutoFact {");
  REQUIRE(returnStart != std::string::npos);
  REQUIRE(localAutoStart != std::string::npos);
  REQUIRE(returnStart < localAutoStart);

  const std::string returnBody =
      semanticProductHeader.substr(returnStart, localAutoStart - returnStart);
  CHECK(returnBody.find("SymbolId definitionPathId = InvalidSymbolId;") !=
        std::string::npos);
  CHECK(returnBody.find("std::string definitionPath;") == std::string::npos);

  CHECK(semanticProductSource.find("semanticProgramReturnFactDefinitionPath(") !=
        std::string::npos);

  CHECK(irLowererResultHelpers.find("returnFact->definitionPathId == InvalidSymbolId") !=
        std::string::npos);
}

TEST_CASE("semantic product local auto facts use initializerResolvedPathId without initializerResolvedPath shadow field") {
  const std::filesystem::path cwd = std::filesystem::current_path();
  const std::filesystem::path root =
      std::filesystem::exists(cwd / "include" / "primec" / "SemanticProduct.h")
          ? cwd
          : cwd.parent_path();
  const std::string semanticProductHeader =
      readTextFile(root / "include" / "primec" / "SemanticProduct.h");
  const std::string semanticProductSource =
      readTextFile(root / "src" / "SemanticProduct.cpp");
  const std::string bindingTypeHelpersSource =
      readTextFile(root / "src" / "ir_lowerer" / "IrLowererBindingTypeHelpers.cpp");

  const std::size_t localAutoStart =
      semanticProductHeader.find("struct SemanticProgramLocalAutoFact {");
  const std::size_t queryStart =
      semanticProductHeader.find("struct SemanticProgramQueryFact {");
  REQUIRE(localAutoStart != std::string::npos);
  REQUIRE(queryStart != std::string::npos);
  REQUIRE(localAutoStart < queryStart);

  const std::string localAutoBody =
      semanticProductHeader.substr(localAutoStart, queryStart - localAutoStart);
  CHECK(localAutoBody.find("SymbolId initializerResolvedPathId = InvalidSymbolId;") !=
        std::string::npos);
  CHECK(localAutoBody.find("std::string initializerResolvedPath;") == std::string::npos);
  CHECK(localAutoBody.find("std::optional<StdlibSurfaceId> initializerStdlibSurfaceId;") !=
        std::string::npos);
  CHECK(localAutoBody.find(
            "std::optional<StdlibSurfaceId> initializerDirectCallStdlibSurfaceId;") !=
        std::string::npos);
  CHECK(localAutoBody.find(
            "std::optional<StdlibSurfaceId> initializerMethodCallStdlibSurfaceId;") !=
        std::string::npos);

  CHECK(semanticProductSource.find("semanticProgramLocalAutoFactInitializerResolvedPath(") !=
        std::string::npos);
  CHECK(semanticProductSource.find("initializerResolvedPath.empty() ? entry.initializerResolvedPath") ==
        std::string::npos);
  CHECK(semanticProductSource.find("initializer_stdlib_surface_id=") != std::string::npos);
  CHECK(semanticProductSource.find("initializer_direct_call_stdlib_surface_id=") !=
        std::string::npos);
  CHECK(semanticProductSource.find("initializer_method_call_stdlib_surface_id=") !=
        std::string::npos);

  CHECK(bindingTypeHelpersSource.find("localAutoFact->initializerResolvedPathId != InvalidSymbolId") !=
        std::string::npos);
}

TEST_CASE("semantic product query facts use resolvedPathId without resolvedPath shadow field") {
  const std::filesystem::path cwd = std::filesystem::current_path();
  const std::filesystem::path root =
      std::filesystem::exists(cwd / "include" / "primec" / "SemanticProduct.h")
          ? cwd
          : cwd.parent_path();
  const std::string semanticProductHeader =
      readTextFile(root / "include" / "primec" / "SemanticProduct.h");
  const std::string semanticProductSource =
      readTextFile(root / "src" / "SemanticProduct.cpp");
  const std::string irLowererResultHelpers =
      readTextFile(root / "src" / "ir_lowerer" / "IrLowererResultHelpers.cpp");

  const std::size_t queryStart =
      semanticProductHeader.find("struct SemanticProgramQueryFact {");
  const std::size_t tryStart =
      semanticProductHeader.find("struct SemanticProgramTryFact {");
  REQUIRE(queryStart != std::string::npos);
  REQUIRE(tryStart != std::string::npos);
  REQUIRE(queryStart < tryStart);

  const std::string queryBody =
      semanticProductHeader.substr(queryStart, tryStart - queryStart);
  CHECK(queryBody.find("SymbolId resolvedPathId = InvalidSymbolId;") !=
        std::string::npos);
  CHECK(queryBody.find("std::string resolvedPath;") == std::string::npos);

  CHECK(semanticProductSource.find("semanticProgramQueryFactResolvedPath(") !=
        std::string::npos);
  CHECK(semanticProductSource.find("resolvedPath.empty() ? entry.resolvedPath") ==
        std::string::npos);

  CHECK(irLowererResultHelpers.find("queryFact->resolvedPathId == InvalidSymbolId") !=
        std::string::npos);
}

TEST_CASE("semantic product try facts use operandResolvedPathId without operandResolvedPath shadow field") {
  const std::filesystem::path cwd = std::filesystem::current_path();
  const std::filesystem::path root =
      std::filesystem::exists(cwd / "include" / "primec" / "SemanticProduct.h")
          ? cwd
          : cwd.parent_path();
  const std::string semanticProductHeader =
      readTextFile(root / "include" / "primec" / "SemanticProduct.h");
  const std::string semanticProductSource =
      readTextFile(root / "src" / "SemanticProduct.cpp");
  const std::string irLowererResultHelpers =
      readTextFile(root / "src" / "ir_lowerer" / "IrLowererResultHelpers.cpp");

  const std::size_t tryStart =
      semanticProductHeader.find("struct SemanticProgramTryFact {");
  const std::size_t onErrorStart =
      semanticProductHeader.find("struct SemanticProgramOnErrorFact {");
  REQUIRE(tryStart != std::string::npos);
  REQUIRE(onErrorStart != std::string::npos);
  REQUIRE(tryStart < onErrorStart);

  const std::string tryBody =
      semanticProductHeader.substr(tryStart, onErrorStart - tryStart);
  CHECK(tryBody.find("SymbolId operandResolvedPathId = InvalidSymbolId;") !=
        std::string::npos);
  CHECK(tryBody.find("std::string operandResolvedPath;") == std::string::npos);

  CHECK(semanticProductSource.find("semanticProgramTryFactOperandResolvedPath(") !=
        std::string::npos);
  CHECK(semanticProductSource.find("operandResolvedPath.empty() ? entry.operandResolvedPath") ==
        std::string::npos);

  CHECK(irLowererResultHelpers.find("tryFact->operandResolvedPathId == InvalidSymbolId") !=
        std::string::npos);
}

TEST_CASE("semantic product on_error facts use handlerPathId without handlerPath shadow field") {
  const std::filesystem::path cwd = std::filesystem::current_path();
  const std::filesystem::path root =
      std::filesystem::exists(cwd / "include" / "primec" / "SemanticProduct.h")
          ? cwd
          : cwd.parent_path();
  const std::string semanticProductHeader =
      readTextFile(root / "include" / "primec" / "SemanticProduct.h");
  const std::string semanticProductSource =
      readTextFile(root / "src" / "SemanticProduct.cpp");
  const std::string onErrorHelpersSource =
      readTextFile(root / "src" / "ir_lowerer" / "IrLowererOnErrorHelpers.cpp");

  const std::size_t onErrorStart =
      semanticProductHeader.find("struct SemanticProgramOnErrorFact {");
  const std::size_t moduleIdentityStart =
      semanticProductHeader.find("struct SemanticProgramModuleIdentity {");
  REQUIRE(onErrorStart != std::string::npos);
  REQUIRE(moduleIdentityStart != std::string::npos);
  REQUIRE(onErrorStart < moduleIdentityStart);

  const std::string onErrorBody =
      semanticProductHeader.substr(onErrorStart, moduleIdentityStart - onErrorStart);
  CHECK(onErrorBody.find("SymbolId handlerPathId = InvalidSymbolId;") !=
        std::string::npos);
  CHECK(onErrorBody.find("std::string handlerPath;") == std::string::npos);

  CHECK(semanticProductSource.find("semanticProgramOnErrorFactHandlerPath(") !=
        std::string::npos);
  CHECK(semanticProductSource.find("handlerPath.empty() ? entry.handlerPath") ==
        std::string::npos);

  CHECK(onErrorHelpersSource.find("fact.handlerPathId == InvalidSymbolId") !=
        std::string::npos);
}

TEST_CASE("semantic product query projections expose stable public lookup keys") {
  const std::string source = R"(
[return<void>]
unexpectedError([i32] err) {
}

[return<Result<int, i32>>]
lookup() {
  return(Result.ok(4i32))
}

[return<i32> effects(heap_alloc) on_error<i32, /unexpectedError>]
main() {
  [auto] selected{try(lookup())}
  return(selected)
}
)";

  primec::testing::CompilePipelineBackendConformance conformance;
  std::string error;
  REQUIRE(primec::testing::runCompilePipelineBackendConformanceForTesting(
      source, "/main", "native", conformance, error));
  CHECK(error.empty());
  REQUIRE(conformance.output.hasSemanticProgram);
  const primec::SemanticProgram &semanticProgram =
      conformance.output.semanticProgram;

  const primec::SemanticProgramQueryFact *queryFact = nullptr;
  for (const auto *entry : primec::semanticProgramQueryFactView(semanticProgram)) {
    if (entry != nullptr && entry->scopePath == "/main" &&
        primec::semanticProgramQueryFactResolvedPath(semanticProgram, *entry) ==
            "/lookup") {
      queryFact = entry;
      break;
    }
  }
  REQUIRE(queryFact != nullptr);
  CHECK(queryFact->callName == "lookup");
  CHECK(queryFact->bindingTypeText == "Result<int, i32>");
  CHECK(queryFact->hasResultType);
  CHECK(queryFact->resultTypeHasValue);
  CHECK(queryFact->resultValueType == "int");
  CHECK(queryFact->resultErrorType == "i32");
  REQUIRE(queryFact->semanticNodeId != 0);
  REQUIRE(queryFact->resolvedPathId != primec::InvalidSymbolId);
  REQUIRE(queryFact->callNameId != primec::InvalidSymbolId);
  CHECK(primec::semanticProgramLookupPublishedQueryFactBySemanticId(
            semanticProgram, queryFact->semanticNodeId) == queryFact);
  CHECK(primec::semanticProgramLookupPublishedQueryFactByResolvedPathAndCallNameId(
            semanticProgram, queryFact->resolvedPathId, queryFact->callNameId) ==
        queryFact);

  const primec::SemanticProgramTryFact *tryFact = nullptr;
  for (const auto *entry : primec::semanticProgramTryFactView(semanticProgram)) {
    if (entry != nullptr && entry->scopePath == "/main" &&
        primec::semanticProgramTryFactOperandResolvedPath(semanticProgram, *entry) ==
            "/lookup") {
      tryFact = entry;
      break;
    }
  }
  REQUIRE(tryFact != nullptr);
  CHECK(tryFact->valueType == "int");
  CHECK(tryFact->errorType == "i32");
  CHECK(tryFact->onErrorHandlerPath == "/unexpectedError");
  REQUIRE(tryFact->semanticNodeId != 0);
  REQUIRE(tryFact->operandResolvedPathId != primec::InvalidSymbolId);
  CHECK(primec::semanticProgramLookupPublishedTryFactBySemanticId(
            semanticProgram, tryFact->semanticNodeId) == tryFact);
  CHECK(primec::semanticProgramLookupPublishedTryFactByOperandPathAndSource(
            semanticProgram,
            tryFact->operandResolvedPathId,
            tryFact->sourceLine,
            tryFact->sourceColumn) == tryFact);
}

TEST_CASE("semantic snapshot shared traversal keeps call and try ordering keys") {
  const std::filesystem::path cwd = std::filesystem::current_path();
  std::filesystem::path headerPath = cwd / "src" / "semantics" / "SemanticsValidator.h";
  const std::filesystem::path root =
      std::filesystem::exists(cwd / "src" / "semantics" / "SemanticsValidatorSnapshots.cpp")
          ? cwd
          : cwd.parent_path();
  if (!std::filesystem::exists(headerPath)) {
    headerPath = root / "src" / "semantics" / "SemanticsValidator.h";
  }
  REQUIRE(std::filesystem::exists(headerPath));
  const std::string semanticsHeader = readTextFile(headerPath);
  const std::string graphLocalAutoHeader =
      readTextFile(root / "src" / "semantics" / "SemanticsValidatorGraphLocalAuto.h");
  const std::string semanticsSnapshots =
      readTextFile(root / "src" / "semantics" / "SemanticsValidatorSnapshots.cpp");
  const std::string semanticsSnapshotLocals =
      readTextFile(root / "src" / "semantics" / "SemanticsValidatorSnapshotLocals.cpp");
  const std::string semanticsValidate =
      readTextFile(root / "src" / "semantics" / "SemanticsValidate.cpp");

  const std::size_t callBindingStart =
      semanticsSnapshots.find("SemanticsValidator::callBindingSnapshotForTesting() {");
  const std::size_t directCallStart =
      semanticsSnapshots.find("SemanticsValidator::takeCollectedDirectCallTargetsForSemanticProduct() {");
  const std::size_t tryFactStart =
      semanticsSnapshots.find("SemanticsValidator::tryFactSnapshotForSemanticProduct() {");
  const std::size_t onErrorFactStart =
      semanticsSnapshots.find("SemanticsValidator::onErrorFactSnapshotForSemanticProduct() {");
  const std::size_t callTryCacheHelperStart =
      semanticsSnapshotLocals.find("SemanticsValidator::ensureCallAndTrySnapshotFactCaches(");
  REQUIRE(callBindingStart != std::string::npos);
  REQUIRE(directCallStart != std::string::npos);
  REQUIRE(tryFactStart != std::string::npos);
  REQUIRE(onErrorFactStart != std::string::npos);
  REQUIRE(callTryCacheHelperStart != std::string::npos);
  REQUIRE(graphLocalAutoHeader.find("struct GraphLocalAutoKey {") != std::string::npos);
  REQUIRE(callBindingStart < directCallStart);
  REQUIRE(tryFactStart < onErrorFactStart);

  const std::string callBindingBody = semanticsSnapshots.substr(callBindingStart, directCallStart - callBindingStart);
  CHECK(callBindingBody.find("ensureCallAndTrySnapshotFactCaches(") != std::string::npos);
  CHECK(callBindingBody.find("return callBindingSnapshotCache_;") != std::string::npos);

  const std::string tryFactBody = semanticsSnapshots.substr(tryFactStart, onErrorFactStart - tryFactStart);
  CHECK(tryFactBody.find("ensureCallAndTrySnapshotFactCaches(") != std::string::npos);
  CHECK(tryFactBody.find("return std::exchange(tryValueSnapshotCache_, {});") != std::string::npos);

  const std::string callTryCacheHelperBody = semanticsSnapshotLocals.substr(callTryCacheHelperStart);
  CHECK(callTryCacheHelperBody.find("forEachLocalAwareSnapshotCall(") != std::string::npos);
  CHECK(callTryCacheHelperBody.find("callBindingSnapshotCache_.push_back(") != std::string::npos);
  CHECK(callTryCacheHelperBody.find("tryValueSnapshotCache_.push_back(") != std::string::npos);
  CHECK(callTryCacheHelperBody.find("std::stable_sort(callBindingSnapshotCache_.begin(),") != std::string::npos);
  CHECK(callTryCacheHelperBody.find("std::stable_sort(tryValueSnapshotCache_.begin(),") != std::string::npos);
  CHECK(callTryCacheHelperBody.find("if (left.scopePath != right.scopePath)") != std::string::npos);
  CHECK(callTryCacheHelperBody.find("if (left.sourceLine != right.sourceLine)") != std::string::npos);
  CHECK(callTryCacheHelperBody.find("if (left.sourceColumn != right.sourceColumn)") != std::string::npos);
  CHECK(callTryCacheHelperBody.find("return left.callName < right.callName;") != std::string::npos);
  CHECK(callTryCacheHelperBody.find("return left.operandResolvedPath < right.operandResolvedPath;") !=
        std::string::npos);

  CHECK(semanticsHeader.find("tryValueSnapshotForTesting") == std::string::npos);
  CHECK(semanticsValidate.find("validator.tryFactSnapshotForSemanticProduct()") != std::string::npos);
  CHECK(semanticsValidate.find("validator.tryValueSnapshotForTesting()") == std::string::npos);
}

TEST_CASE("semantic snapshot locals concrete-call canonicalization stays stable") {
  const std::filesystem::path cwd = std::filesystem::current_path();
  const std::filesystem::path root =
      std::filesystem::exists(cwd / "src" / "semantics" / "SemanticsValidatorSnapshotLocals.cpp")
          ? cwd
          : cwd.parent_path();
  const std::string semanticsSnapshotLocals =
      readTextFile(root / "src" / "semantics" / "SemanticsValidatorSnapshotLocals.cpp");

  CHECK(semanticsSnapshotLocals.find(
            "  out.resolvedPath = preferredCollectionHelperResolvedPath(expr);\n"
            "  if (out.resolvedPath.empty()) {\n"
            "    out.resolvedPath = resolveCalleePath(expr);\n"
            "  }\n") !=
        std::string::npos);
  CHECK(semanticsSnapshotLocals.find(
            "  if (!out.resolvedPath.empty()) {\n"
            "    const std::string concreteResolvedPath =\n"
            "        resolveExprConcreteCallPath(defParams, activeLocals, expr, out.resolvedPath);\n"
            "    if (!concreteResolvedPath.empty()) {\n"
            "      out.resolvedPath = concreteResolvedPath;\n"
            "    }\n"
            "    std::string canonicalResolvedPath = out.resolvedPath;\n"
            "    if (const size_t suffix = canonicalResolvedPath.find(\"__t\");\n"
            "        suffix != std::string::npos) {\n"
            "      canonicalResolvedPath.erase(suffix);\n"
            "    }\n"
            "    canonicalResolvedPath =\n"
            "        canonicalizeLegacySoaGetHelperPath(canonicalResolvedPath);\n"
            "    canonicalResolvedPath =\n"
            "        canonicalizeLegacySoaRefHelperPath(canonicalResolvedPath);\n"
            "    canonicalResolvedPath =\n"
            "        canonicalizeLegacySoaToAosHelperPath(canonicalResolvedPath);\n"
            "    if (!canonicalResolvedPath.empty()) {\n"
            "      out.resolvedPath = std::move(canonicalResolvedPath);\n"
            "    }\n"
            "  }") !=
        std::string::npos);
}

TEST_CASE("semantic snapshot method targets concrete-call canonicalization stays stable") {
  const std::filesystem::path cwd = std::filesystem::current_path();
  const std::filesystem::path root =
      std::filesystem::exists(cwd / "src" / "semantics" / "SemanticsValidatorSnapshots.cpp")
          ? cwd
          : cwd.parent_path();
  const std::string semanticsSnapshots =
      readTextFile(root / "src" / "semantics" / "SemanticsValidatorSnapshots.cpp");

  CHECK(semanticsSnapshots.find(
            "      resolvedPath = preferredCollectionHelperResolvedPath(expr);\n"
            "      if (resolvedPath.empty()) {\n"
            "        resolvedPath = resolveCalleePath(expr);\n"
            "      }\n") !=
        std::string::npos);
  CHECK(semanticsSnapshots.find(
            "    const std::string concreteResolvedPath =\n"
            "        resolveExprConcreteCallPath(defParams, activeLocals, expr, resolvedPath);\n"
            "    if (!concreteResolvedPath.empty()) {\n"
            "      resolvedPath = concreteResolvedPath;\n"
            "    }") !=
        std::string::npos);
  CHECK(semanticsSnapshots.find(
            "      resolvedPath = resolveCalleePath(expr);\n"
            "      if (resolvedPath.empty()) {\n"
            "        return;\n"
            "      }\n") ==
        std::string::npos);
}

TEST_CASE("semantic snapshot shared traversal keeps callable summary and on_error ordering keys") {
  const std::filesystem::path cwd = std::filesystem::current_path();
  std::filesystem::path headerPath = cwd / "src" / "semantics" / "SemanticsValidator.h";
  const std::filesystem::path root =
      std::filesystem::exists(cwd / "src" / "semantics" / "SemanticsValidatorSnapshots.cpp")
          ? cwd
          : cwd.parent_path();
  if (!std::filesystem::exists(headerPath)) {
    headerPath = root / "src" / "semantics" / "SemanticsValidator.h";
  }
  REQUIRE(std::filesystem::exists(headerPath));
  const std::string semanticsHeader = readTextFile(headerPath);
  const std::string semanticsSnapshots =
      readTextFile(root / "src" / "semantics" / "SemanticsValidatorSnapshots.cpp");
  const std::string semanticsDefinitionPasses =
      readTextFile(root / "src" / "semantics" / "SemanticsValidatorPassesDefinitions.cpp");
  const std::string semanticsValidate =
      readTextFile(root / "src" / "semantics" / "SemanticsValidate.cpp");

  const std::size_t collectHelperStart =
      semanticsSnapshots.find("SemanticsValidator::collectPilotRoutingSemanticProductFacts() {");
  const std::size_t callableSummaryStart =
      semanticsSnapshots.find("SemanticsValidator::takeCollectedCallableSummariesForSemanticProduct() {");
  const std::size_t ensureOnErrorStart =
      semanticsSnapshots.find("void SemanticsValidator::ensureOnErrorSnapshotFactCache() const {");
  const std::size_t typeMetadataStart =
      semanticsSnapshots.find("SemanticsValidator::typeMetadataSnapshotForSemanticProduct() const {");
  const std::size_t onErrorStart =
      semanticsSnapshots.find("SemanticsValidator::onErrorFactSnapshotForSemanticProduct() {");
  const std::size_t rebindHelperStart =
      semanticsSnapshots.find("void SemanticsValidator::rebindMergedWorkerPublicationFactSemanticNodeIds() {");
  REQUIRE(collectHelperStart != std::string::npos);
  REQUIRE(callableSummaryStart != std::string::npos);
  REQUIRE(ensureOnErrorStart != std::string::npos);
  REQUIRE(typeMetadataStart != std::string::npos);
  REQUIRE(onErrorStart != std::string::npos);
  REQUIRE(rebindHelperStart != std::string::npos);
  REQUIRE(collectHelperStart < callableSummaryStart);
  REQUIRE(callableSummaryStart < typeMetadataStart);
  REQUIRE(ensureOnErrorStart < typeMetadataStart);
  REQUIRE(rebindHelperStart < onErrorStart);

  CHECK(semanticsHeader.find("bool mergedWorkerPublicationFactsValid_ = false;") !=
        std::string::npos);
  CHECK(semanticsHeader.find("bool mergedWorkerPublicationFactSemanticNodeIdsCurrent_ = false;") !=
        std::string::npos);
  CHECK(semanticsHeader.find("bool mergedWorkerPublicationSeedStringsValid_ = false;") !=
        std::string::npos);
  CHECK(semanticsHeader.find("SemanticPublicationSurface mergedWorkerPublicationFacts_;") !=
        std::string::npos);
  CHECK(semanticsHeader.find("std::vector<std::string> mergedWorkerPublicationSeedStrings_;") !=
        std::string::npos);
  CHECK(semanticsSnapshots.find("SemanticsValidator::collectCallableSummaryEntriesForStableRange(") !=
        std::string::npos);
  CHECK(semanticsSnapshots.find("SemanticsValidator::collectExecutionCallableSummaryEntries(") !=
        std::string::npos);
  CHECK(semanticsSnapshots.find("SemanticsValidator::rebindCollectedCallableSummarySemanticNodeIds(") !=
        std::string::npos);
  CHECK(semanticsSnapshots.find("SemanticsValidator::sortCollectedCallableSummaries(") !=
        std::string::npos);
  CHECK(semanticsSnapshots.find("SemanticsValidator::collectOnErrorSnapshotEntriesForStableRange(") !=
        std::string::npos);
  CHECK(semanticsSnapshots.find("SemanticsValidator::collectDefinitionPublicationFactsForStableRange(") !=
        std::string::npos);
  CHECK(semanticsSnapshots.find("SemanticsValidator::rebindMergedWorkerPublicationFactSemanticNodeIds()") !=
        std::string::npos);
  CHECK(semanticsSnapshots.find("SemanticsValidator::sortCollectedOnErrorSnapshots(") !=
        std::string::npos);

  const std::string collectHelperBody =
      semanticsSnapshots.substr(collectHelperStart, callableSummaryStart - collectHelperStart);
  CHECK(collectHelperBody.find("const bool useMergedWorkerPublicationFacts = mergedWorkerPublicationFactsValid_;") !=
        std::string::npos);
  CHECK(collectHelperBody.find("rebindMergedWorkerPublicationFactSemanticNodeIds();") !=
        std::string::npos);
  CHECK(collectHelperBody.find("collectedDirectCallTargets_ = mergedWorkerPublicationFacts_.directCallTargets;") !=
        std::string::npos);
  CHECK(collectHelperBody.find("collectedMethodCallTargets_ = mergedWorkerPublicationFacts_.methodCallTargets;") !=
        std::string::npos);
  CHECK(collectHelperBody.find("collectedBridgePathChoices_ = mergedWorkerPublicationFacts_.bridgePathChoices;") !=
        std::string::npos);
  CHECK(collectHelperBody.find("collectedCallableSummaries_ = mergedWorkerPublicationFacts_.callableSummaries;") !=
        std::string::npos);
  CHECK(collectHelperBody.find("rebindCollectedCallableSummarySemanticNodeIds(collectedCallableSummaries_);") !=
        std::string::npos);
  CHECK(collectHelperBody.find("collectCallableSummaryEntriesForStableRange(") !=
        std::string::npos);
  CHECK(collectHelperBody.find("collectExecutionCallableSummaryEntries(collectedCallableSummaries_);") !=
        std::string::npos);
  CHECK(collectHelperBody.find("sortCollectedCallableSummaries(collectedCallableSummaries_);") !=
        std::string::npos);
  CHECK(semanticsSnapshots.find("if (left.fullPath != right.fullPath)") != std::string::npos);
  CHECK(semanticsSnapshots.find("return left.isExecution < right.isExecution;") !=
        std::string::npos);
  const std::string rebindHelperBody =
      semanticsSnapshots.substr(rebindHelperStart, onErrorStart - rebindHelperStart);
  CHECK(semanticsSnapshots.find("bool appendMissingEntriesBySnapshotKey(") !=
        std::string::npos);
  CHECK(rebindHelperBody.find("freshDirectCallTargets") != std::string::npos);
  CHECK(rebindHelperBody.find("freshQueryFacts") != std::string::npos);
  CHECK(rebindHelperBody.find("appendMissingEntriesBySnapshotKey(\n"
                              "          mergedWorkerPublicationFacts_.directCallTargets") !=
        std::string::npos);
  CHECK(rebindHelperBody.find("appendMissingEntriesBySnapshotKey(\n"
                              "          mergedWorkerPublicationFacts_.queryFacts") !=
        std::string::npos);
  CHECK(rebindHelperBody.find("directCallTargetSnapshotKey") != std::string::npos);
  CHECK(rebindHelperBody.find("queryFactSnapshotKey") != std::string::npos);
  CHECK(semanticsDefinitionPasses.find("struct WorkerChunkResult") ==
        std::string::npos);
  CHECK(semanticsDefinitionPasses.find("worker.collectDefinitionPublicationFactsForStableRange(") !=
        std::string::npos);
  CHECK(semanticsDefinitionPasses.find("mergeWorkerPublicationFacts") != std::string::npos);
  CHECK(semanticsDefinitionPasses.find("mergeWorkerCallableSummaries") == std::string::npos);
  CHECK(semanticsDefinitionPasses.find("mergeWorkerOnErrorFacts") == std::string::npos);
  CHECK(semanticsDefinitionPasses.find("collectExecutionCallableSummaryEntries(\n"
                                      "        mergedWorkerPublicationFacts_.callableSummaries);") !=
        std::string::npos);
  CHECK(semanticsDefinitionPasses.find("sortCollectedCallableSummaries(mergedWorkerPublicationFacts_.callableSummaries);") !=
        std::string::npos);
  CHECK(semanticsDefinitionPasses.find("mergedWorkerPublicationFactsValid_ = true;") !=
        std::string::npos);
  CHECK(semanticsDefinitionPasses.find("appendSemanticPublicationStringOrigins(") !=
        std::string::npos);
  CHECK(semanticsDefinitionPasses.find("sortCollectedOnErrorSnapshots(mergedWorkerPublicationFacts_.onErrorFacts);") !=
        std::string::npos);
  CHECK(semanticsDefinitionPasses.find("appendMoved(mergedWorkerPublicationFacts_.bindingFacts,") !=
        std::string::npos);
  CHECK(semanticsDefinitionPasses.find("appendMoved(mergedWorkerPublicationFacts_.queryFacts,") !=
        std::string::npos);
  CHECK(semanticsDefinitionPasses.find("appendMoved(mergedWorkerPublicationFacts_.tryFacts,") !=
        std::string::npos);
  CHECK(semanticsDefinitionPasses.find("appendMoved(mergedWorkerPublicationFacts_.onErrorFacts,") !=
        std::string::npos);
  CHECK(semanticsSnapshots.find("rebindSemanticNodeIdsBySnapshotKey(") !=
        std::string::npos);
  CHECK(semanticsDefinitionPasses.find("mergeWorkerPublicationSeedStrings") !=
        std::string::npos);
  CHECK(semanticsDefinitionPasses.find("SymbolInterner::mergeWorkerSnapshotsDeterministic(") !=
        std::string::npos);
  CHECK(semanticsDefinitionPasses.find("mergedWorkerPublicationSeedStrings_ =") !=
        std::string::npos);
  CHECK(semanticsDefinitionPasses.find("mergedWorkerPublicationSeedStringsValid_ = true;") !=
        std::string::npos);

  const std::string callableSummaryBody =
      semanticsSnapshots.substr(callableSummaryStart, typeMetadataStart - callableSummaryStart);
  CHECK(callableSummaryBody.find("collectPilotRoutingSemanticProductFacts();") != std::string::npos);
  CHECK(callableSummaryBody.find("return std::exchange(collectedCallableSummaries_, {});") !=
        std::string::npos);
  CHECK(semanticsSnapshots.find("surface.callTargetSeedStrings = mergedWorkerPublicationSeedStrings_;") !=
        std::string::npos);
  CHECK(semanticsSnapshots.find("appendSemanticPublicationStringOrigins(publicationStringInterner,") !=
        std::string::npos);

  const std::string ensureOnErrorBody =
      semanticsSnapshots.substr(ensureOnErrorStart, typeMetadataStart - ensureOnErrorStart);
  CHECK(ensureOnErrorBody.find("if (mergedWorkerPublicationFactsValid_) {") != std::string::npos);
  CHECK(ensureOnErrorBody.find("onErrorSnapshotCache_ = mergedWorkerPublicationFacts_.onErrorFacts;") !=
        std::string::npos);
  CHECK(ensureOnErrorBody.find("collectOnErrorSnapshotEntriesForStableRange(") !=
        std::string::npos);
  CHECK(ensureOnErrorBody.find("sortCollectedOnErrorSnapshots(onErrorSnapshotCache_);") !=
        std::string::npos);

  const std::string onErrorBody = semanticsSnapshots.substr(onErrorStart);
  CHECK(onErrorBody.find("ensureOnErrorSnapshotFactCache();") != std::string::npos);
  CHECK(onErrorBody.find("return std::exchange(onErrorSnapshotCache_, {});") != std::string::npos);

  CHECK(semanticsHeader.find("onErrorSnapshotForTesting") == std::string::npos);
  CHECK(semanticsHeader.find("validationContextSnapshotForTesting") == std::string::npos);
  CHECK(semanticsHeader.find("struct ValidationContextSnapshotEntry {") == std::string::npos);
  CHECK(semanticsValidate.find("validator.onErrorFactSnapshotForSemanticProduct()") != std::string::npos);
  CHECK(semanticsValidate.find("validator.onErrorSnapshotForTesting()") == std::string::npos);
  CHECK(semanticsValidate.find("validator.takeCollectedCallableSummariesForSemanticProduct()") != std::string::npos);
  CHECK(semanticsValidate.find("validator.validationContextSnapshotForTesting()") == std::string::npos);
  CHECK(semanticsValidate.find("if (entry.isExecution) {") != std::string::npos);
}

TEST_CASE("semantic snapshot traversal inventory avoids inactive next-candidate pointer") {
  const std::filesystem::path cwd = std::filesystem::current_path();
  const std::filesystem::path root =
      std::filesystem::exists(cwd / "docs" / "semantic_snapshot_traversal_inventory.md")
          ? cwd
          : cwd.parent_path();
  const std::filesystem::path inventoryPath =
      root / "docs" / "semantic_snapshot_traversal_inventory.md";
  REQUIRE(std::filesystem::exists(inventoryPath));

  const std::string inventory = readTextFile(inventoryPath);
  CHECK(inventory.find("`P2-11` implemented") != std::string::npos);
  CHECK(inventory.find("`TODO-4201` implemented") != std::string::npos);
  CHECK(inventory.find("method-call target publication now derives resolved-path and receiver") !=
        std::string::npos);
  CHECK(inventory.find("No active TODO currently targets additional semantic snapshot traversal merges.") !=
        std::string::npos);
  CHECK(inventory.find("Add a concrete traversal-churn TODO with acceptance and parity coverage") !=
        std::string::npos);
  CHECK(inventory.find("## Next Pair Candidate") == std::string::npos);
  CHECK(inventory.find("Likely next merge candidate") == std::string::npos);
  CHECK(inventory.find("The historical next candidate was") == std::string::npos);
}
