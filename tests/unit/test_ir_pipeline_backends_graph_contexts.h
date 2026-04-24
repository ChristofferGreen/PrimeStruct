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
  CHECK(graphHeader.find("struct TypeResolutionGraphNode") != std::string::npos);
  CHECK(graphHeader.find("struct TypeResolutionGraphEdge") != std::string::npos);
  CHECK(graphHeader.find("struct TypeResolutionGraph") != std::string::npos);
  CHECK(graphHeader.find("buildTypeResolutionGraphForProgram") != std::string::npos);
  CHECK(graphHeader.find("formatTypeResolutionGraph") != std::string::npos);
  CHECK(graphSource.find("TypeResolutionGraphBuilder") != std::string::npos);
  CHECK(graphSource.find("prepareProgramForTypeResolutionAnalysis(program, entryPath, semanticTransforms, error)") !=
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

#if 0
TEST_CASE("compile pipeline publishes an initial semantic product shell") {
  const std::filesystem::path cwd = std::filesystem::current_path();
  std::filesystem::path semanticProductPath = cwd / "include" / "primec" / "SemanticProduct.h";
  std::filesystem::path compilePipelineHeaderPath = cwd / "include" / "primec" / "CompilePipeline.h";
  std::filesystem::path semanticsHeaderPath = cwd / "include" / "primec" / "Semantics.h";
  std::filesystem::path irPreparationHeaderPath = cwd / "include" / "primec" / "IrPreparation.h";
  std::filesystem::path irLowererHeaderPath = cwd / "include" / "primec" / "IrLowerer.h";
  std::filesystem::path compilePipelineSourcePath = cwd / "src" / "CompilePipeline.cpp";
  std::filesystem::path semanticsValidatePath = cwd / "src" / "semantics" / "SemanticsValidate.cpp";
  std::filesystem::path irPreparationSourcePath = cwd / "src" / "IrPreparation.cpp";
  std::filesystem::path irEntrySetupHeaderPath = cwd / "src" / "ir_lowerer" / "IrLowererLowerEntrySetup.h";
  std::filesystem::path irEntrySetupSourcePath = cwd / "src" / "ir_lowerer" / "IrLowererLowerEntrySetup.cpp";
  std::filesystem::path irLowererEntryPath = cwd / "src" / "ir_lowerer" / "IrLowererLowerSetupEntryEffects.h";
  std::filesystem::path irCallHelpersPath = cwd / "src" / "ir_lowerer" / "IrLowererCallHelpers.h";
  std::filesystem::path irCallResolutionPath = cwd / "src" / "ir_lowerer" / "IrLowererCallResolution.cpp";
  std::filesystem::path irMethodResolutionPath =
      cwd / "src" / "ir_lowerer" / "IrLowererSetupTypeMethodCallResolution.cpp";
  std::filesystem::path irSetupTypeCollectionHelpersPath =
      cwd / "src" / "ir_lowerer" / "IrLowererSetupTypeCollectionHelpers.cpp";
  std::filesystem::path irReceiverTargetHelpersPath =
      cwd / "src" / "ir_lowerer" / "IrLowererSetupTypeReceiverTargetHelpers.cpp";
  std::filesystem::path irSetupTypeReturnKindHelpersPath =
      cwd / "src" / "ir_lowerer" / "IrLowererSetupTypeReturnKindHelpers.cpp";
  std::filesystem::path irInferenceSetupPath =
      cwd / "src" / "ir_lowerer" / "IrLowererLowerSetupInference.h";
  std::filesystem::path irLowerEffectsPath = cwd / "src" / "ir_lowerer" / "IrLowererLowerEffects.cpp";
  std::filesystem::path irGpuEffectsPath = cwd / "src" / "ir_lowerer" / "IrLowererGpuEffects.cpp";
  std::filesystem::path irNativeEffectsPath = cwd / "src" / "ir_lowerer" / "IrLowererNativeEffects.cpp";
  std::filesystem::path irVmEffectsPath = cwd / "src" / "ir_lowerer" / "IrLowererVmEffects.cpp";
  std::filesystem::path irReturnInferencePath =
      cwd / "src" / "ir_lowerer" / "IrLowererReturnInferenceHelpers.cpp";
  std::filesystem::path semanticTargetAdapterHeaderPath =
      cwd / "src" / "ir_lowerer" / "IrLowererSemanticProductTargetAdapters.h";
  std::filesystem::path semanticTargetAdapterSourcePath =
      cwd / "src" / "ir_lowerer" / "IrLowererSemanticProductTargetAdapters.cpp";
  std::filesystem::path lowerImportsStructsSetupHeaderPath =
      cwd / "src" / "ir_lowerer" / "IrLowererLowerImportsStructsSetup.h";
  std::filesystem::path lowerImportsStructsSetupSourcePath =
      cwd / "src" / "ir_lowerer" / "IrLowererLowerImportsStructsSetup.cpp";
  std::filesystem::path lowerSetupImportsStructsPath =
      cwd / "src" / "ir_lowerer" / "IrLowererLowerSetupImportsStructs.h";
  std::filesystem::path structTypeHelpersHeaderPath =
      cwd / "src" / "ir_lowerer" / "IrLowererStructTypeHelpers.h";
  std::filesystem::path structTypeHelpersSourcePath =
      cwd / "src" / "ir_lowerer" / "IrLowererStructTypeHelpers.cpp";
  std::filesystem::path structFieldBindingHelpersHeaderPath =
      cwd / "src" / "ir_lowerer" / "IrLowererStructFieldBindingHelpers.h";
  std::filesystem::path structFieldBindingHelpersSourcePath =
      cwd / "src" / "ir_lowerer" / "IrLowererStructFieldBindingHelpers.cpp";
  std::filesystem::path structLayoutHelpersHeaderPath =
      cwd / "src" / "ir_lowerer" / "IrLowererStructLayoutHelpers.h";
  std::filesystem::path structLayoutHelpersSourcePath =
      cwd / "src" / "ir_lowerer" / "IrLowererStructLayoutHelpers.cpp";
  std::filesystem::path bindingTypeHelpersHeaderPath =
      cwd / "src" / "ir_lowerer" / "IrLowererBindingTypeHelpers.h";
  std::filesystem::path bindingTypeHelpersSourcePath =
      cwd / "src" / "ir_lowerer" / "IrLowererBindingTypeHelpers.cpp";
  std::filesystem::path setupMathHelpersHeaderPath =
      cwd / "src" / "ir_lowerer" / "IrLowererSetupMathHelpers.h";
  std::filesystem::path setupMathHelpersSourcePath =
      cwd / "src" / "ir_lowerer" / "IrLowererSetupMathHelpers.cpp";
  std::filesystem::path countAccessHelpersHeaderPath =
      cwd / "src" / "ir_lowerer" / "IrLowererCountAccessHelpers.h";
  std::filesystem::path countAccessHelpersSourcePath =
      cwd / "src" / "ir_lowerer" / "IrLowererCountAccessHelpers.cpp";
  std::filesystem::path onErrorHelpersSourcePath =
      cwd / "src" / "ir_lowerer" / "IrLowererOnErrorHelpers.cpp";
  std::filesystem::path statementCallHelpersHeaderPath =
      cwd / "src" / "ir_lowerer" / "IrLowererStatementCallHelpers.h";
  std::filesystem::path statementCallHelpersSourcePath =
      cwd / "src" / "ir_lowerer" / "IrLowererStatementCallHelpers.cpp";
  std::filesystem::path functionTableStepHeaderPath =
      cwd / "src" / "ir_lowerer" / "IrLowererLowerStatementsFunctionTableStep.h";
  std::filesystem::path functionTableStepSourcePath =
      cwd / "src" / "ir_lowerer" / "IrLowererLowerStatementsFunctionTableStep.cpp";
  std::filesystem::path lowerStatementsCallsPath =
      cwd / "src" / "ir_lowerer" / "IrLowererLowerStatementsCalls.h";
  std::filesystem::path uninitializedTypeHelpersPath =
      cwd / "src" / "ir_lowerer" / "IrLowererUninitializedTypeHelpers.h";
  std::filesystem::path uninitializedSetupBuildersPath =
      cwd / "src" / "ir_lowerer" / "IrLowererUninitializedSetupBuilders.cpp";
  std::filesystem::path primecMainPath = cwd / "src" / "main.cpp";
  std::filesystem::path primevmMainPath = cwd / "src" / "primevm_main.cpp";
  if (!std::filesystem::exists(semanticProductPath)) {
    semanticProductPath = cwd.parent_path() / "include" / "primec" / "SemanticProduct.h";
    compilePipelineHeaderPath = cwd.parent_path() / "include" / "primec" / "CompilePipeline.h";
    semanticsHeaderPath = cwd.parent_path() / "include" / "primec" / "Semantics.h";
    irPreparationHeaderPath = cwd.parent_path() / "include" / "primec" / "IrPreparation.h";
    irLowererHeaderPath = cwd.parent_path() / "include" / "primec" / "IrLowerer.h";
    compilePipelineSourcePath = cwd.parent_path() / "src" / "CompilePipeline.cpp";
    semanticsValidatePath = cwd.parent_path() / "src" / "semantics" / "SemanticsValidate.cpp";
    irPreparationSourcePath = cwd.parent_path() / "src" / "IrPreparation.cpp";
    irEntrySetupHeaderPath = cwd.parent_path() / "src" / "ir_lowerer" / "IrLowererLowerEntrySetup.h";
    irEntrySetupSourcePath = cwd.parent_path() / "src" / "ir_lowerer" / "IrLowererLowerEntrySetup.cpp";
    irLowererEntryPath = cwd.parent_path() / "src" / "ir_lowerer" / "IrLowererLowerSetupEntryEffects.h";
    irCallHelpersPath = cwd.parent_path() / "src" / "ir_lowerer" / "IrLowererCallHelpers.h";
    irCallResolutionPath = cwd.parent_path() / "src" / "ir_lowerer" / "IrLowererCallResolution.cpp";
    irMethodResolutionPath =
        cwd.parent_path() / "src" / "ir_lowerer" / "IrLowererSetupTypeMethodCallResolution.cpp";
    irSetupTypeCollectionHelpersPath =
        cwd.parent_path() / "src" / "ir_lowerer" / "IrLowererSetupTypeCollectionHelpers.cpp";
    irReceiverTargetHelpersPath =
        cwd.parent_path() / "src" / "ir_lowerer" / "IrLowererSetupTypeReceiverTargetHelpers.cpp";
    irSetupTypeReturnKindHelpersPath =
        cwd.parent_path() / "src" / "ir_lowerer" / "IrLowererSetupTypeReturnKindHelpers.cpp";
    irInferenceSetupPath = cwd.parent_path() / "src" / "ir_lowerer" / "IrLowererLowerSetupInference.h";
    irLowerEffectsPath = cwd.parent_path() / "src" / "ir_lowerer" / "IrLowererLowerEffects.cpp";
    irGpuEffectsPath = cwd.parent_path() / "src" / "ir_lowerer" / "IrLowererGpuEffects.cpp";
    irNativeEffectsPath = cwd.parent_path() / "src" / "ir_lowerer" / "IrLowererNativeEffects.cpp";
    irVmEffectsPath = cwd.parent_path() / "src" / "ir_lowerer" / "IrLowererVmEffects.cpp";
    irReturnInferencePath =
        cwd.parent_path() / "src" / "ir_lowerer" / "IrLowererReturnInferenceHelpers.cpp";
    semanticTargetAdapterHeaderPath =
        cwd.parent_path() / "src" / "ir_lowerer" / "IrLowererSemanticProductTargetAdapters.h";
    semanticTargetAdapterSourcePath =
        cwd.parent_path() / "src" / "ir_lowerer" / "IrLowererSemanticProductTargetAdapters.cpp";
    lowerImportsStructsSetupHeaderPath =
        cwd.parent_path() / "src" / "ir_lowerer" / "IrLowererLowerImportsStructsSetup.h";
    lowerImportsStructsSetupSourcePath =
        cwd.parent_path() / "src" / "ir_lowerer" / "IrLowererLowerImportsStructsSetup.cpp";
    lowerSetupImportsStructsPath =
        cwd.parent_path() / "src" / "ir_lowerer" / "IrLowererLowerSetupImportsStructs.h";
    structTypeHelpersHeaderPath =
        cwd.parent_path() / "src" / "ir_lowerer" / "IrLowererStructTypeHelpers.h";
    structTypeHelpersSourcePath =
        cwd.parent_path() / "src" / "ir_lowerer" / "IrLowererStructTypeHelpers.cpp";
    structFieldBindingHelpersHeaderPath =
        cwd.parent_path() / "src" / "ir_lowerer" / "IrLowererStructFieldBindingHelpers.h";
    structFieldBindingHelpersSourcePath =
        cwd.parent_path() / "src" / "ir_lowerer" / "IrLowererStructFieldBindingHelpers.cpp";
    structLayoutHelpersHeaderPath =
        cwd.parent_path() / "src" / "ir_lowerer" / "IrLowererStructLayoutHelpers.h";
    structLayoutHelpersSourcePath =
        cwd.parent_path() / "src" / "ir_lowerer" / "IrLowererStructLayoutHelpers.cpp";
    bindingTypeHelpersHeaderPath =
        cwd.parent_path() / "src" / "ir_lowerer" / "IrLowererBindingTypeHelpers.h";
    bindingTypeHelpersSourcePath =
        cwd.parent_path() / "src" / "ir_lowerer" / "IrLowererBindingTypeHelpers.cpp";
    setupMathHelpersHeaderPath =
        cwd.parent_path() / "src" / "ir_lowerer" / "IrLowererSetupMathHelpers.h";
    setupMathHelpersSourcePath =
        cwd.parent_path() / "src" / "ir_lowerer" / "IrLowererSetupMathHelpers.cpp";
    countAccessHelpersHeaderPath =
        cwd.parent_path() / "src" / "ir_lowerer" / "IrLowererCountAccessHelpers.h";
    countAccessHelpersSourcePath =
        cwd.parent_path() / "src" / "ir_lowerer" / "IrLowererCountAccessHelpers.cpp";
    onErrorHelpersSourcePath =
        cwd.parent_path() / "src" / "ir_lowerer" / "IrLowererOnErrorHelpers.cpp";
    statementCallHelpersHeaderPath =
        cwd.parent_path() / "src" / "ir_lowerer" / "IrLowererStatementCallHelpers.h";
    statementCallHelpersSourcePath =
        cwd.parent_path() / "src" / "ir_lowerer" / "IrLowererStatementCallHelpers.cpp";
    functionTableStepHeaderPath =
        cwd.parent_path() / "src" / "ir_lowerer" / "IrLowererLowerStatementsFunctionTableStep.h";
    functionTableStepSourcePath =
        cwd.parent_path() / "src" / "ir_lowerer" / "IrLowererLowerStatementsFunctionTableStep.cpp";
    lowerStatementsCallsPath =
        cwd.parent_path() / "src" / "ir_lowerer" / "IrLowererLowerStatementsCalls.h";
    uninitializedTypeHelpersPath =
        cwd.parent_path() / "src" / "ir_lowerer" / "IrLowererUninitializedTypeHelpers.h";
    uninitializedSetupBuildersPath =
        cwd.parent_path() / "src" / "ir_lowerer" / "IrLowererUninitializedSetupBuilders.cpp";
    primecMainPath = cwd.parent_path() / "src" / "main.cpp";
    primevmMainPath = cwd.parent_path() / "src" / "primevm_main.cpp";
  }
  REQUIRE(std::filesystem::exists(semanticProductPath));
  REQUIRE(std::filesystem::exists(compilePipelineHeaderPath));
  REQUIRE(std::filesystem::exists(semanticsHeaderPath));
  REQUIRE(std::filesystem::exists(irPreparationHeaderPath));
  REQUIRE(std::filesystem::exists(irLowererHeaderPath));
  REQUIRE(std::filesystem::exists(compilePipelineSourcePath));
  REQUIRE(std::filesystem::exists(semanticsValidatePath));
  REQUIRE(std::filesystem::exists(irPreparationSourcePath));
  REQUIRE(std::filesystem::exists(irEntrySetupHeaderPath));
  REQUIRE(std::filesystem::exists(irEntrySetupSourcePath));
  REQUIRE(std::filesystem::exists(irLowererEntryPath));
  REQUIRE(std::filesystem::exists(irCallHelpersPath));
  REQUIRE(std::filesystem::exists(irCallResolutionPath));
  REQUIRE(std::filesystem::exists(irMethodResolutionPath));
  REQUIRE(std::filesystem::exists(irSetupTypeCollectionHelpersPath));
  REQUIRE(std::filesystem::exists(irReceiverTargetHelpersPath));
  REQUIRE(std::filesystem::exists(irSetupTypeReturnKindHelpersPath));
  REQUIRE(std::filesystem::exists(irInferenceSetupPath));
  REQUIRE(std::filesystem::exists(irLowerEffectsPath));
  REQUIRE(std::filesystem::exists(irGpuEffectsPath));
  REQUIRE(std::filesystem::exists(irNativeEffectsPath));
  REQUIRE(std::filesystem::exists(irVmEffectsPath));
  REQUIRE(std::filesystem::exists(irReturnInferencePath));
  REQUIRE(std::filesystem::exists(semanticTargetAdapterHeaderPath));
  REQUIRE(std::filesystem::exists(semanticTargetAdapterSourcePath));
  REQUIRE(std::filesystem::exists(lowerImportsStructsSetupHeaderPath));
  REQUIRE(std::filesystem::exists(lowerImportsStructsSetupSourcePath));
  REQUIRE(std::filesystem::exists(lowerSetupImportsStructsPath));
  REQUIRE(std::filesystem::exists(structTypeHelpersHeaderPath));
  REQUIRE(std::filesystem::exists(structTypeHelpersSourcePath));
  REQUIRE(std::filesystem::exists(structFieldBindingHelpersHeaderPath));
  REQUIRE(std::filesystem::exists(structFieldBindingHelpersSourcePath));
  REQUIRE(std::filesystem::exists(structLayoutHelpersHeaderPath));
  REQUIRE(std::filesystem::exists(structLayoutHelpersSourcePath));
  REQUIRE(std::filesystem::exists(bindingTypeHelpersHeaderPath));
  REQUIRE(std::filesystem::exists(bindingTypeHelpersSourcePath));
  REQUIRE(std::filesystem::exists(setupMathHelpersHeaderPath));
  REQUIRE(std::filesystem::exists(setupMathHelpersSourcePath));
  REQUIRE(std::filesystem::exists(countAccessHelpersHeaderPath));
  REQUIRE(std::filesystem::exists(countAccessHelpersSourcePath));
  REQUIRE(std::filesystem::exists(onErrorHelpersSourcePath));
  REQUIRE(std::filesystem::exists(statementCallHelpersHeaderPath));
  REQUIRE(std::filesystem::exists(statementCallHelpersSourcePath));
  REQUIRE(std::filesystem::exists(functionTableStepHeaderPath));
  REQUIRE(std::filesystem::exists(functionTableStepSourcePath));
  REQUIRE(std::filesystem::exists(lowerStatementsCallsPath));
  REQUIRE(std::filesystem::exists(uninitializedTypeHelpersPath));
  REQUIRE(std::filesystem::exists(uninitializedSetupBuildersPath));
  REQUIRE(std::filesystem::exists(primecMainPath));
  REQUIRE(std::filesystem::exists(primevmMainPath));

  const std::string semanticProduct = readTextFile(semanticProductPath);
  const std::string compilePipelineHeader = readTextFile(compilePipelineHeaderPath);
  const std::string semanticsHeader = readTextFile(semanticsHeaderPath);
  const std::string irPreparationHeader = readTextFile(irPreparationHeaderPath);
  const std::string irLowererHeader = readTextFile(irLowererHeaderPath);
  const std::string compilePipelineSource = readTextFile(compilePipelineSourcePath);
  const std::string semanticsValidate = readTextFile(semanticsValidatePath);
  const std::string irPreparationSource = readTextFile(irPreparationSourcePath);
  const std::string irEntrySetupHeader = readTextFile(irEntrySetupHeaderPath);
  const std::string irEntrySetupSource = readTextFile(irEntrySetupSourcePath);
  const std::string irLowererEntry = readTextFile(irLowererEntryPath);
  const std::string irCallHelpers = readTextFile(irCallHelpersPath);
  const std::string irCallResolution = readTextFile(irCallResolutionPath);
  const std::string irMethodResolution = readTextFile(irMethodResolutionPath);
  const std::string irSetupTypeCollectionHelpers = readTextFile(irSetupTypeCollectionHelpersPath);
  const std::string irReceiverTargetHelpers = readTextFile(irReceiverTargetHelpersPath);
  const std::string irSetupTypeReturnKindHelpers = readTextFile(irSetupTypeReturnKindHelpersPath);
  const std::string irInferenceSetup = readTextFile(irInferenceSetupPath);
  const std::string irLowerEffects = readTextFile(irLowerEffectsPath);
  const std::string irGpuEffects = readTextFile(irGpuEffectsPath);
  const std::string irNativeEffects = readTextFile(irNativeEffectsPath);
  const std::string irVmEffects = readTextFile(irVmEffectsPath);
  const std::string irReturnInference = readTextFile(irReturnInferencePath);
  const std::string semanticTargetAdapterHeader = readTextFile(semanticTargetAdapterHeaderPath);
  const std::string semanticTargetAdapterSource = readTextFile(semanticTargetAdapterSourcePath);
  const std::string lowerImportsStructsSetupHeader = readTextFile(lowerImportsStructsSetupHeaderPath);
  const std::string lowerImportsStructsSetupSource = readTextFile(lowerImportsStructsSetupSourcePath);
  const std::string lowerSetupImportsStructs = readTextFile(lowerSetupImportsStructsPath);
  const std::string structTypeHelpersHeader = readTextFile(structTypeHelpersHeaderPath);
  const std::string structTypeHelpersSource = readTextFile(structTypeHelpersSourcePath);
  const std::string structFieldBindingHelpersHeader = readTextFile(structFieldBindingHelpersHeaderPath);
  const std::string structFieldBindingHelpersSource = readTextFile(structFieldBindingHelpersSourcePath);
  const std::string structLayoutHelpersHeader = readTextFile(structLayoutHelpersHeaderPath);
  const std::string structLayoutHelpersSource = readTextFile(structLayoutHelpersSourcePath);
  const std::string bindingTypeHelpersHeader = readTextFile(bindingTypeHelpersHeaderPath);
  const std::string bindingTypeHelpersSource = readTextFile(bindingTypeHelpersSourcePath);
  const std::string setupMathHelpersHeader = readTextFile(setupMathHelpersHeaderPath);
  const std::string setupMathHelpersSource = readTextFile(setupMathHelpersSourcePath);
  const std::string countAccessHelpersHeader = readTextFile(countAccessHelpersHeaderPath);
  const std::string countAccessHelpersSource = readTextFile(countAccessHelpersSourcePath);
  const std::string onErrorHelpersSource = readTextFile(onErrorHelpersSourcePath);
  const std::string statementCallHelpersHeader = readTextFile(statementCallHelpersHeaderPath);
  const std::string statementCallHelpersSource = readTextFile(statementCallHelpersSourcePath);
  const std::string functionTableStepHeader = readTextFile(functionTableStepHeaderPath);
  const std::string functionTableStepSource = readTextFile(functionTableStepSourcePath);
  const std::string lowerStatementsCalls = readTextFile(lowerStatementsCallsPath);
  const std::string uninitializedTypeHelpers = readTextFile(uninitializedTypeHelpersPath);
  const std::string uninitializedSetupBuilders = readTextFile(uninitializedSetupBuildersPath);
  const std::string primecMain = readTextFile(primecMainPath);
  const std::string primevmMain = readTextFile(primevmMainPath);
  CHECK(semanticProduct.find("struct SemanticProgramDirectCallTarget") != std::string::npos);
  CHECK(semanticProduct.find("struct SemanticProgramMethodCallTarget") != std::string::npos);
  CHECK(semanticProduct.find("struct SemanticProgramBridgePathChoice") != std::string::npos);
  CHECK(semanticProduct.find("std::optional<StdlibSurfaceId> stdlibSurfaceId;") !=
        std::string::npos);
  CHECK(semanticProduct.find("struct SemanticProgramCallableSummary") != std::string::npos);
  CHECK(semanticProduct.find("struct SemanticProgramTypeMetadata") != std::string::npos);
  CHECK(semanticProduct.find("struct SemanticProgramStructFieldMetadata") != std::string::npos);
  CHECK(semanticProduct.find("struct SemanticProgramBindingFact") != std::string::npos);
  CHECK(semanticProduct.find("struct SemanticProgramReturnFact") != std::string::npos);
  CHECK(semanticProduct.find("struct SemanticProgramLocalAutoFact") != std::string::npos);
  CHECK(semanticProduct.find("struct SemanticProgramQueryFact") != std::string::npos);
  CHECK(semanticProduct.find("struct SemanticProgramTryFact") != std::string::npos);
  CHECK(semanticProduct.find("struct SemanticProgramOnErrorFact") != std::string::npos);
  CHECK(semanticProduct.find("std::vector<std::string> boundArgTexts;") != std::string::npos);
  CHECK(semanticProduct.find("struct SemanticProgramDefinition") != std::string::npos);
  CHECK(semanticProduct.find("struct SemanticProgramExecution") != std::string::npos);
  CHECK(semanticProduct.find("struct SemanticProgram") != std::string::npos);
  CHECK(semanticProduct.find("std::string formatSemanticProgram(const SemanticProgram &semanticProgram);") !=
        std::string::npos);
  CHECK(semanticProduct.find("std::vector<SemanticProgramDirectCallTarget> directCallTargets;") !=
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
  CHECK(semanticProduct.find("std::vector<SemanticProgramMethodCallTarget> methodCallTargets;") !=
        std::string::npos);
  CHECK(semanticProduct.find("std::vector<SemanticProgramBridgePathChoice> bridgePathChoices;") !=
        std::string::npos);
  CHECK(semanticProduct.find("std::unordered_map<uint64_t, StdlibSurfaceId> directCallStdlibSurfaceIdsByExpr;") !=
        std::string::npos);
  CHECK(semanticProduct.find("std::unordered_map<uint64_t, StdlibSurfaceId> methodCallStdlibSurfaceIdsByExpr;") !=
        std::string::npos);
  CHECK(semanticProduct.find("std::unordered_map<uint64_t, StdlibSurfaceId> bridgePathChoiceStdlibSurfaceIdsByExpr;") !=
        std::string::npos);
  CHECK(semanticProduct.find("std::vector<SemanticProgramCallableSummary> callableSummaries;") !=
        std::string::npos);
  CHECK(semanticProduct.find("std::vector<SemanticProgramTypeMetadata> typeMetadata;") !=
        std::string::npos);
  CHECK(semanticProduct.find("std::vector<SemanticProgramStructFieldMetadata> structFieldMetadata;") !=
        std::string::npos);
  CHECK(semanticProduct.find("std::vector<SemanticProgramBindingFact> bindingFacts;") !=
        std::string::npos);
  CHECK(semanticProduct.find("std::vector<SemanticProgramReturnFact> returnFacts;") !=
        std::string::npos);
  CHECK(semanticProduct.find("std::vector<SemanticProgramLocalAutoFact> localAutoFacts;") !=
        std::string::npos);
  CHECK(semanticProduct.find("std::vector<SemanticProgramQueryFact> queryFacts;") !=
        std::string::npos);
  CHECK(semanticProduct.find("std::vector<SemanticProgramTryFact> tryFacts;") !=
        std::string::npos);
  CHECK(semanticProduct.find("std::vector<SemanticProgramOnErrorFact> onErrorFacts;") !=
        std::string::npos);
  CHECK(compilePipelineHeader.find("SemanticProgram semanticProgram;") != std::string::npos);
  CHECK(compilePipelineHeader.find("bool hasSemanticProgram = false;") != std::string::npos);
  CHECK(compilePipelineHeader.find("struct CompilePipelineFailure") != std::string::npos);
  CHECK(compilePipelineHeader.find("CompilePipelineFailure failure;") != std::string::npos);
  CHECK(compilePipelineHeader.find("bool hasFailure = false;") != std::string::npos);
  CHECK(semanticsHeader.find("SemanticProgram *semanticProgramOut = nullptr") != std::string::npos);
  CHECK(irPreparationHeader.find("const SemanticProgram *semanticProgram,") != std::string::npos);
  CHECK(irLowererHeader.find("const SemanticProgram *semanticProgram,") != std::string::npos);
  CHECK(compilePipelineSource.find("SemanticProgram semanticProgram;") != std::string::npos);
  CHECK(compilePipelineSource.find("&semanticProgram") != std::string::npos);
  CHECK(compilePipelineSource.find("output.semanticProgram = std::move(semanticProgram);") != std::string::npos);
  CHECK(compilePipelineSource.find("output.hasSemanticProgram = true;") != std::string::npos);
  CHECK(compilePipelineSource.find("output.failure.stage = stage;") != std::string::npos);
  CHECK(compilePipelineSource.find("output.failure.message = message;") != std::string::npos);
  CHECK(compilePipelineSource.find("output.hasFailure = true;") != std::string::npos);
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
  CHECK(compilePipelineSource.find("CompilePipelineImportStageState importStage;") !=
        std::string::npos);
  CHECK(compilePipelineSource.find("CompilePipelinePreParseStageState preParseStage;") !=
        std::string::npos);
  CHECK(compilePipelineSource.find("CompilePipelineParsedProgramStageState parsedStage;") !=
        std::string::npos);
  const size_t semanticProductPublishPos =
      compilePipelineSource.find("output.semanticProgram = std::move(semanticProgram);");
  const size_t graphicsValidationPos =
      compilePipelineSource.find("if (!validateGraphicsBackendSupport(output.program, options, error, &capturedDiagnosticInfo)) {");
  REQUIRE(semanticProductPublishPos != std::string::npos);
  REQUIRE(graphicsValidationPos != std::string::npos);
  CHECK(semanticProductPublishPos < graphicsValidationPos);
  CHECK(irPreparationSource.find("bool prepareIrModule(Program &program,") != std::string::npos);
  CHECK(irPreparationSource.find("const SemanticProgram *semanticProgram,") != std::string::npos);
  CHECK(irPreparationSource.find("lowerer.lower(program,") != std::string::npos);
  CHECK(irPreparationSource.find("semanticProgram,") != std::string::npos);
  CHECK(irEntrySetupHeader.find("const SemanticProgram *semanticProgram,") != std::string::npos);
  CHECK(irEntrySetupSource.find("(void)semanticProgram;") == std::string::npos);
  CHECK(irEntrySetupSource.find("semantic product entry path mismatch: expected ") != std::string::npos);
  CHECK(irLowererEntry.find("runLowerEntrySetup(program,") != std::string::npos);
  CHECK(irLowererEntry.find("semanticProgram,") != std::string::npos);
  CHECK(irEntrySetupSource.find("validateSemanticProductDirectCallCoverage(context.program,") ==
        std::string::npos);
  CHECK(irEntrySetupSource.find("validateSemanticProductBridgePathCoverage(context.program,") ==
        std::string::npos);
  CHECK(irEntrySetupSource.find("buildDefinitionMapAndStructNames(program.definitions, defMap, unusedStructNames, nullptr);") ==
        std::string::npos);
  CHECK(irEntrySetupSource.find("buildImportAliasesFromProgram(program.imports, program.definitions, defMap);") ==
        std::string::npos);
  CHECK(irEntrySetupSource.find("{\"routing.direct-call\", \"directCallTargets[].resolvedPathId\"") ==
        std::string::npos);
  CHECK(irEntrySetupSource.find("{\"routing.bridge-path\", \"bridgePathChoices[].helperNameId\"") ==
        std::string::npos);
  CHECK(irEntrySetupSource.find("{\"routing.method-call\", \"methodCallTargets[].resolvedPathId\"") ==
        std::string::npos);
  CHECK(irEntrySetupSource.find("validateSemanticProductMethodCallCoverage(program, semanticProgram, error)") ==
        std::string::npos);
  CHECK(irEntrySetupSource.find("validateSemanticProductBindingCoverage(context.program, context.semanticProgram, error)") !=
        std::string::npos);
  CHECK(irEntrySetupSource.find("validateSemanticProductResultMetadataCompleteness(semanticProgram, error)") !=
        std::string::npos);
  CHECK(irEntrySetupSource.find("validateNativeNoSoftwareNumericTypes(semanticProgram, error)") !=
        std::string::npos);
  CHECK(irEntrySetupSource.find("validateNativeNoRuntimeReflectionQueries(semanticProgram, error)") !=
        std::string::npos);
  CHECK(irEntrySetupSource.find("validateGpuNoSoftwareNumericTypes(semanticProgram, error)") !=
        std::string::npos);
  CHECK(irEntrySetupSource.find("validateGpuNoRuntimeReflectionQueries(semanticProgram, error)") !=
        std::string::npos);
  CHECK(irEntrySetupSource.find("validateNoSoftwareNumericTypes(semanticProgram, error)") ==
        std::string::npos);
  CHECK(irEntrySetupSource.find("validateNoRuntimeReflectionQueries(semanticProgram, error)") ==
        std::string::npos);
  CHECK(irEntrySetupSource.find("kSemanticProductCompletenessMatrix") != std::string::npos);
  CHECK(irEntrySetupSource.find("validateSemanticProductCompletenessMatrix(program, *entryDefOut, semanticProgram, error)") !=
        std::string::npos);
  CHECK(irEntrySetupSource.find("validateEntryParameterFactFamily") != std::string::npos);
  CHECK(irEntrySetupSource.find("resolveEntryArgsParameter(") != std::string::npos);
  CHECK(irEntrySetupSource.find("validateOnErrorFactFamily") != std::string::npos);
  CHECK(irEntrySetupSource.find("buildOnErrorByDefinition(context.program,") != std::string::npos);
  CHECK(irCallHelpers.find("SemanticProductTargetAdapter semanticProductTargets{};") != std::string::npos);
  CHECK(irCallHelpers.find("const SemanticProgram *semanticProgram = nullptr;") != std::string::npos);
  CHECK(irCallResolution.find("buildSemanticProductTargetAdapter(semanticProgram)") != std::string::npos);
  CHECK(irCallResolution.find("adapters.semanticProgram = semanticProgram;") != std::string::npos);
  CHECK(irCallResolution.find("findSemanticProductDirectCallTarget(semanticProgram, expr)") !=
        std::string::npos);
  CHECK(irCallResolution.find("findSemanticProductBridgePathChoice(semanticProgram, expr)") !=
        std::string::npos);
  CHECK(irCallResolution.find("if (expr.isMethodCall) {\n        if (const std::string resolvedPath =\n                findSemanticProductMethodCallTarget(semanticProgram, expr);") !=
        std::string::npos);
  CHECK(irCallResolution.find("return resolveCallPathWithoutSemanticFallbackProbes(expr);") !=
        std::string::npos);
  CHECK(irMethodResolution.find("findSemanticProductMethodCallTarget(semanticProgram, callExpr)") !=
        std::string::npos);
  CHECK(irMethodResolution.find("findSemanticProductBridgePathChoice(semanticProgram, callExpr)") !=
        std::string::npos);
  CHECK(irMethodResolution.find("matchesGeneratedDefinitionFamilyPath(") !=
        std::string::npos);
  CHECK(irMethodResolution.find("buildReceiverMethodTargetPath(") !=
        std::string::npos);
  CHECK(irMethodResolution.find("missing semantic-product method-call target: ") !=
        std::string::npos);
  CHECK(irMethodResolution.find("semantic-product method-call target missing lowered definition: ") !=
        std::string::npos);
  CHECK(irMethodResolution.find(
            "const bool\n"
            "          routesExplicitVectorCountMethodThroughMapMethodTarget =\n"
            "              requestsExplicitVectorCountMethod &&\n"
            "              (normalizeCollectionHelperPath(resolvedPath) == \"/map/count\" ||\n"
            "               normalizeCollectionHelperPath(resolvedPath) ==\n"
                    "                   \"/std/collections/map/count\");") !=
        std::string::npos);
  CHECK(irMethodResolution.find(
            "      const std::string preferredResolvedPath =\n"
            "          !explicitVectorCountBridgePath.empty()\n"
            "              ? explicitVectorCountBridgePath\n"
            "              : resolvedPath;\n"
            "      if (const Definition *resolvedDef =\n"
            "              resolveLoweredDefinitionPath(preferredResolvedPath);") !=
        std::string::npos);
  CHECK(irMethodResolution.find(
            "      const std::string receiverMethodTargetPath =\n"
            "          buildReceiverMethodTargetPath(targetPath, explicitMethodPath);") !=
        std::string::npos);
  CHECK(irMethodResolution.find(
            "      const std::string normalizedReceiverMethodTargetPath =\n"
            "            buildReceiverMethodTargetPath(normalizedTargetPath,\n"
            "                                          explicitMethodPath);") !=
        std::string::npos);
  CHECK(irMethodResolution.find(
            "      if (requestsExplicitVectorCountMethod) {\n"
            "        if (!directResolvedPath.empty() &&\n"
            "            directResolvedPath != preferredResolvedPath) {\n"
            "          if (const Definition *directResolvedDef =\n"
            "                  resolveLoweredDefinitionPath(directResolvedPath);\n"
            "              directResolvedDef != nullptr) {\n"
            "            return directResolvedDef;\n"
            "          }\n"
            "        }\n"
            "        if (const Definition *explicitMethodDef =\n"
            "                resolveLoweredDefinitionPath(explicitMethodPath);\n"
            "            explicitMethodDef != nullptr) {\n"
            "          return explicitMethodDef;\n"
            "        }\n"
            "      }") !=
        std::string::npos);
  CHECK(irMethodResolution.find(
            "    const std::string directResolvedPath =\n"
            "        findSemanticProductDirectCallTarget(semanticProgram, callExpr);") !=
        std::string::npos);
  CHECK(irMethodResolution.find(
            "      const std::string bridgeResolvedPath =\n"
            "          findSemanticProductBridgePathChoice(semanticProgram, callExpr);\n"
            "      const std::string preferredFallbackResolvedPath =\n"
            "          !bridgeResolvedPath.empty() ? bridgeResolvedPath : directResolvedPath;\n"
            "      if (!preferredFallbackResolvedPath.empty()) {\n"
            "        if (const Definition *resolvedDef =\n"
            "                resolveLoweredDefinitionPath(preferredFallbackResolvedPath);\n"
            "            resolvedDef != nullptr) {\n"
            "          return resolvedDef;\n"
            "        }\n"
            "        errorOut.clear();") !=
        std::string::npos);
  CHECK(irMethodResolution.find(
            "              matchesGeneratedDefinitionFamilyPath(candidatePath, path)) {") !=
        std::string::npos);
  CHECK(irMethodResolution.find(
            "            matchesGeneratedDefinitionFamilyPath(candidatePath, candidate)) {") !=
        std::string::npos);
  CHECK(irMethodResolution.find("candidatePath.rfind(path + \"__t\", 0) == 0") ==
        std::string::npos);
  CHECK(irMethodResolution.find("candidatePath.rfind(candidate + \"__t\", 0) == 0") ==
        std::string::npos);
  CHECK(irMethodResolution.find(
            "  const std::string explicitMethodPath = describeMethodCallExpr(callExpr);") !=
        std::string::npos);
  CHECK(irMethodResolution.find(
            "  const bool isExplicitRemovedVectorMethodAlias =\n"
            "      isExplicitRemovedVectorMethodAliasPath(explicitMethodPath);") !=
        std::string::npos);
  CHECK(irMethodResolution.find(
            "  const bool isExplicitMapMethodAlias =\n"
            "      isExplicitMapMethodAliasPath(explicitMethodPath);") !=
        std::string::npos);
  CHECK(irMethodResolution.find(
            "  const bool isExplicitMapContainsOrTryAtMethod =\n"
            "      isExplicitMapContainsOrTryAtMethodPath(explicitMethodPath);") !=
        std::string::npos);
  CHECK(irMethodResolution.find("std::string semanticTargetLookupError;") ==
        std::string::npos);
  CHECK(irCallResolution.find("std::string resolveScopedCallPathForHelperMatching(const Expr &expr)") !=
        std::string::npos);
  CHECK(irCallResolution.find(
            "    const std::string scopedCallPath =\n"
            "        resolveScopedCallPathForHelperMatching(callExpr);\n"
            "    if (isExplicitMapContainsOrTryAtMethodPath(scopedCallPath) &&\n"
            "        normalizeCollectionHelperPath(scopedCallPath) ==") !=
        std::string::npos);
  CHECK(irMethodResolution.find("const auto &semanticAwareImportAliases =") != std::string::npos);
  CHECK(irMethodResolution.find("resolveMethodReceiverTarget(*receiver,") != std::string::npos);
  CHECK(irMethodResolution.find("explicitMethodPath,\n"
                                "                                   semanticAwareImportAliases,") !=
        std::string::npos);
  CHECK(irMethodResolution.find("semanticAwareImportAliases,") != std::string::npos);
  CHECK(irMethodResolution.find("resolveMethodDefinitionFromReceiverTarget(\n"
                                "      explicitMethodPath, typeName, resolvedTypePath, defMap, lookupError);") !=
        std::string::npos);
  CHECK(irMethodResolution.find("resolveMethodDefinitionFromReceiverTarget(\n"
                                "          explicitMethodPath, \"\", receiverTypeName, defMap, errorOutRef);") !=
        std::string::npos);
  CHECK(irSetupTypeCollectionHelpers.find(
            "std::string normalized = rebuildScopedCollectionHelperPath(expr);") !=
        std::string::npos);
  CHECK(irSetupTypeCollectionHelpers.find("std::string normalized = expr.name;") ==
        std::string::npos);
  CHECK(irReceiverTargetHelpers.find("std::string resolveScopedMethodPath(const Expr &expr)") !=
        std::string::npos);
  CHECK(irReceiverTargetHelpers.find(
            "const std::string scopedMethodPath = resolveScopedMethodPath(callExpr);") !=
        std::string::npos);
  CHECK(irReceiverTargetHelpers.find(
            "const bool isExplicitRemovedVectorMethodAlias = isExplicitRemovedVectorMethodAliasPath(scopedMethodPath);") !=
        std::string::npos);
  CHECK(irReceiverTargetHelpers.find(
            "const bool isExplicitMapMethodAlias = isExplicitMapMethodAliasPath(scopedMethodPath);") !=
        std::string::npos);
  CHECK(irReceiverTargetHelpers.find(
            "isExplicitMapContainsOrTryAtMethodPath(scopedMethodPath);") !=
        std::string::npos);
  CHECK(irReceiverTargetHelpers.find(
            "errorOut = \"unknown method target for \" + scopedMethodPath;") !=
        std::string::npos);
  CHECK(irReceiverTargetHelpers.find(
            "const std::string scopedReceiverMethodPath = resolveScopedMethodPath(receiverCallExpr);") !=
        std::string::npos);
  CHECK(irReceiverTargetHelpers.find(
            "(isExplicitMapMethodAliasPath(scopedReceiverMethodPath) ||\n"
            "        isExplicitMapContainsOrTryAtMethodPath(scopedReceiverMethodPath))) ||") !=
        std::string::npos);
  CHECK(irReceiverTargetHelpers.find(
            "(isExplicitMapMethodAliasPath(receiverCallExpr.name) ||\n"
            "        isExplicitMapContainsOrTryAtMethodPath(receiverCallExpr.name))) ||") ==
        std::string::npos);
  CHECK(irMethodResolution.find("resolveMethodCallDefinitionFromExpr(*receiver,") != std::string::npos);
  CHECK(irMethodResolution.find("resolveMethodCallDefinitionFromExpr(*receiver,\n"
                                "                                                        localsIn,\n"
                                "                                                        isArrayCountCall,\n"
                                "                                                        isVectorCapacityCall,\n"
                                "                                                        isEntryArgsName,\n"
                                "                                                        importAliases,") ==
        std::string::npos);
  CHECK(irMethodResolution.find("errorOut =\n        \"semantic-product method-call target missing lowered definition: \" +") !=
        std::string::npos);
  CHECK(irMethodResolution.find("resolvedPath;\n    return nullptr;") !=
        std::string::npos);
  CHECK(irSetupTypeReturnKindHelpers.find("std::string resolveScopedCallPath(const Expr &expr)") !=
        std::string::npos);
  CHECK(irSetupTypeReturnKindHelpers.find(
            "const std::string scopedCallPath = resolveScopedCallPath(callExpr);") !=
        std::string::npos);
  CHECK(irSetupTypeReturnKindHelpers.find(
            "appendCandidates(collectionHelperPathCandidates(scopedCallPath));") !=
        std::string::npos);
  CHECK(irSetupTypeReturnKindHelpers.find(
            "if (isExplicitRemovedVectorMethodAliasPath(scopedCallPath)) {") !=
        std::string::npos);
  CHECK(irSetupTypeReturnKindHelpers.find(
            "for (const auto &candidatePath : collectionHelperPathCandidates(scopedCallPath)) {") !=
        std::string::npos);
  CHECK(irSetupTypeReturnKindHelpers.find(
            "!isAllowedResolvedVectorDirectCallPath(scopedCallPath, callee->fullPath) ||\n"
            "        !isAllowedResolvedMapDirectCallPath(scopedCallPath, callee->fullPath)) {") !=
        std::string::npos);
  CHECK(irInferenceSetup.find(".semanticProgram = callResolutionAdapters.semanticProgram,") !=
        std::string::npos);
  CHECK(irInferenceSetup.find(".semanticIndex = &callResolutionAdapters.semanticProductTargets.semanticIndex,") !=
        std::string::npos);
  CHECK(semanticTargetAdapterHeader.find("struct SemanticProductTargetAdapter") != std::string::npos);
  CHECK(semanticTargetAdapterHeader.find("bool hasSemanticProduct = false;") != std::string::npos);
  CHECK(semanticTargetAdapterHeader.find("std::string findSemanticProductDirectCallTarget(const SemanticProgram *semanticProgram, const Expr &expr);") !=
        std::string::npos);
  CHECK(semanticTargetAdapterHeader.find("std::string findSemanticProductMethodCallTarget(const SemanticProgram *semanticProgram, const Expr &expr);") !=
        std::string::npos);
  CHECK(semanticTargetAdapterHeader.find("std::string findSemanticProductBridgePathChoice(const SemanticProgram *semanticProgram, const Expr &expr);") !=
        std::string::npos);
  CHECK(semanticTargetAdapterHeader.find("const SemanticProgramCallableSummary *findSemanticProductCallableSummary(\n    const SemanticProgram *semanticProgram,") !=
        std::string::npos);
  CHECK(semanticTargetAdapterHeader.find("const SemanticProgramCallableSummary *findSemanticProductCallableSummary(") !=
        std::string::npos);
  CHECK(semanticTargetAdapterHeader.find("SemanticProductIndex buildSemanticProductIndex(const SemanticProgram *semanticProgram);") !=
        std::string::npos);
  CHECK(semanticTargetAdapterHeader.find("std::unordered_map<uint64_t, const SemanticProgramOnErrorFact *> onErrorFactsByDefinitionId;") !=
        std::string::npos);
  CHECK(semanticTargetAdapterHeader.find("std::unordered_map<SymbolId, const SemanticProgramOnErrorFact *> onErrorFactsByDefinitionPathId;") !=
        std::string::npos);
  CHECK(semanticTargetAdapterHeader.find("std::unordered_map<std::string, const SemanticProgramOnErrorFact *> onErrorFactsByDefinitionPath;") ==
        std::string::npos);
  CHECK(semanticTargetAdapterHeader.find("const SemanticProgramOnErrorFact *findSemanticProductOnErrorFactBySemanticId(\n    const SemanticProductIndex &semanticIndex,") !=
        std::string::npos);
  CHECK(semanticTargetAdapterHeader.find("const SemanticProgramOnErrorFact *findSemanticProductOnErrorFact(\n    const SemanticProgram *semanticProgram,\n    const SemanticProductIndex &semanticIndex,") !=
        std::string::npos);
  CHECK(semanticTargetAdapterHeader.find("const SemanticProgramOnErrorFact *findSemanticProductOnErrorFact(") !=
        std::string::npos);
  CHECK(semanticProduct.find("const SemanticProgramTypeMetadata *semanticProgramLookupTypeMetadata(") !=
        std::string::npos);
  CHECK(semanticProduct.find("semanticProgramStructTypeMetadataView(const SemanticProgram &semanticProgram)") !=
        std::string::npos);
  CHECK(semanticProduct.find("semanticProgramStructFieldMetadataView(const SemanticProgram &semanticProgram,") !=
        std::string::npos);
  CHECK(semanticTargetAdapterHeader.find("std::unordered_map<std::string_view, const SemanticProgramTypeMetadata *> typeMetadataByPath;") ==
        std::string::npos);
  CHECK(semanticTargetAdapterHeader.find("std::vector<const SemanticProgramTypeMetadata *> orderedStructTypeMetadata;") ==
        std::string::npos);
  CHECK(semanticTargetAdapterHeader.find("std::unordered_map<std::string_view, std::vector<const SemanticProgramStructFieldMetadata *>>") ==
        std::string::npos);
  CHECK(semanticTargetAdapterHeader.find("const SemanticProgramTypeMetadata *findSemanticProductTypeMetadata(") ==
        std::string::npos);
  CHECK(semanticTargetAdapterHeader.find("findSemanticProductStructFieldMetadata(") ==
        std::string::npos);
  CHECK(semanticTargetAdapterHeader.find("std::unordered_map<uint64_t, const SemanticProgramReturnFact *> returnFactsByDefinitionId;") !=
        std::string::npos);
  CHECK(semanticTargetAdapterHeader.find("std::unordered_map<SymbolId, const SemanticProgramReturnFact *> returnFactsByDefinitionPathId;") !=
        std::string::npos);
  CHECK(semanticTargetAdapterHeader.find("const SemanticProgramReturnFact *findSemanticProductReturnFactBySemanticId(\n    const SemanticProductIndex &semanticIndex,") !=
        std::string::npos);
  CHECK(semanticTargetAdapterHeader.find("const SemanticProgramReturnFact *findSemanticProductReturnFact(\n    const SemanticProgram *semanticProgram,\n    const SemanticProductIndex &semanticIndex,") !=
        std::string::npos);
  CHECK(semanticTargetAdapterHeader.find("const SemanticProgramPublishedRoutingLookups *publishedRoutingLookups = nullptr;") !=
        std::string::npos);
  CHECK(semanticProduct.find("std::unordered_map<uint64_t, std::size_t> onErrorFactIndicesByDefinitionId;") !=
        std::string::npos);
  CHECK(semanticProduct.find("std::unordered_map<SymbolId, std::size_t> onErrorFactIndicesByDefinitionPathId;") !=
        std::string::npos);
  CHECK(semanticProduct.find("std::unordered_map<uint64_t, std::size_t> localAutoFactIndicesByExpr;") !=
        std::string::npos);
  CHECK(semanticProduct.find("std::unordered_map<uint64_t, std::size_t> localAutoFactIndicesByInitPathAndBindingNameId;") !=
        std::string::npos);
  CHECK(semanticProduct.find("std::unordered_map<uint64_t, std::size_t> queryFactIndicesByExpr;") !=
        std::string::npos);
  CHECK(semanticProduct.find("std::unordered_map<uint64_t, std::size_t> queryFactIndicesByResolvedPathAndCallNameId;") !=
        std::string::npos);
  CHECK(semanticProduct.find("std::unordered_map<uint64_t, std::size_t> tryFactIndicesByExpr;") !=
        std::string::npos);
  CHECK(semanticProduct.find("std::unordered_map<uint64_t, std::size_t> tryFactIndicesByOperandPathAndSource;") !=
        std::string::npos);
  CHECK(semanticProduct.find("const SemanticProgramOnErrorFact *semanticProgramLookupPublishedOnErrorFactByDefinitionPathId(") !=
        std::string::npos);
  CHECK(semanticProduct.find("const SemanticProgramLocalAutoFact *semanticProgramLookupPublishedLocalAutoFactByInitializerPathAndBindingNameId(") !=
        std::string::npos);
  CHECK(semanticProduct.find("const SemanticProgramQueryFact *semanticProgramLookupPublishedQueryFactByResolvedPathAndCallNameId(") !=
        std::string::npos);
  CHECK(semanticProduct.find("const SemanticProgramTryFact *semanticProgramLookupPublishedTryFactByOperandPathAndSource(") !=
        std::string::npos);
  CHECK(semanticTargetAdapterHeader.find("std::optional<StdlibSurfaceId> findSemanticProductDirectCallStdlibSurfaceId(") !=
        std::string::npos);
  CHECK(semanticTargetAdapterHeader.find("std::optional<StdlibSurfaceId> findSemanticProductMethodCallStdlibSurfaceId(") !=
        std::string::npos);
  CHECK(semanticTargetAdapterHeader.find("std::optional<StdlibSurfaceId> findSemanticProductBridgePathChoiceStdlibSurfaceId(") !=
        std::string::npos);
  CHECK(semanticTargetAdapterHeader.find("SemanticProductIndex semanticIndex;") !=
        std::string::npos);
  CHECK(semanticTargetAdapterHeader.find("std::unordered_map<uint64_t, SymbolId> directCallTargetIdsByExpr;") ==
        std::string::npos);
  CHECK(semanticTargetAdapterHeader.find("std::unordered_map<uint64_t, SymbolId> methodCallTargetIdsByExpr;") ==
        std::string::npos);
  CHECK(semanticTargetAdapterHeader.find("std::unordered_map<uint64_t, SymbolId> bridgePathChoiceIdsByExpr;") ==
        std::string::npos);
  CHECK(semanticTargetAdapterHeader.find("std::unordered_map<SymbolId, const SemanticProgramCallableSummary *> callableSummariesByPathId;") ==
        std::string::npos);
  CHECK(semanticTargetAdapterHeader.find("const SemanticProgramReturnFact *findSemanticProductReturnFact(") !=
        std::string::npos);
  CHECK(semanticTargetAdapterHeader.find("const SemanticProgramLocalAutoFact *findSemanticProductLocalAutoFactBySemanticId(\n    const SemanticProductIndex &semanticIndex,") !=
        std::string::npos);
  CHECK(semanticTargetAdapterHeader.find("std::unordered_map<uint64_t, const SemanticProgramLocalAutoFact *> localAutoFactsByExpr;") !=
        std::string::npos);
  CHECK(semanticTargetAdapterHeader.find("std::unordered_map<uint64_t, const SemanticProgramLocalAutoFact *> localAutoFactsByInitPathAndBindingNameId;") !=
        std::string::npos);
  CHECK(semanticTargetAdapterHeader.find("const SemanticProgramLocalAutoFact *findSemanticProductLocalAutoFact(\n    const SemanticProgram *semanticProgram,\n    const SemanticProductIndex &semanticIndex,") !=
        std::string::npos);
  CHECK(semanticTargetAdapterHeader.find("std::unordered_map<uint64_t, const SemanticProgramQueryFact *> queryFactsByExpr;") !=
        std::string::npos);
  CHECK(semanticTargetAdapterHeader.find("std::unordered_map<uint64_t, const SemanticProgramQueryFact *> queryFactsByResolvedPathAndCallNameId;") !=
        std::string::npos);
  CHECK(semanticTargetAdapterHeader.find("const SemanticProgramQueryFact *findSemanticProductQueryFactBySemanticId(\n    const SemanticProductIndex &semanticIndex,") !=
        std::string::npos);
  CHECK(semanticTargetAdapterHeader.find("const SemanticProgramQueryFact *findSemanticProductQueryFact(\n    const SemanticProgram *semanticProgram,\n    const SemanticProductIndex &semanticIndex,") !=
        std::string::npos);
  CHECK(semanticTargetAdapterHeader.find("std::unordered_map<uint64_t, const SemanticProgramTryFact *> tryFactsByExpr;") !=
        std::string::npos);
  CHECK(semanticTargetAdapterHeader.find("std::unordered_map<uint64_t, const SemanticProgramTryFact *> tryFactsByOperandPathAndSource;") !=
        std::string::npos);
  CHECK(semanticTargetAdapterHeader.find("const SemanticProgramTryFact *findSemanticProductTryFactBySemanticId(\n    const SemanticProductIndex &semanticIndex,") !=
        std::string::npos);
  CHECK(semanticTargetAdapterHeader.find("const SemanticProgramTryFact *findSemanticProductTryFact(\n    const SemanticProgram *semanticProgram,\n    const SemanticProductIndex &semanticIndex,") !=
        std::string::npos);
  CHECK(semanticTargetAdapterHeader.find("const SemanticProgramLocalAutoFact *findSemanticProductLocalAutoFact(") !=
        std::string::npos);
  CHECK(semanticTargetAdapterHeader.find("const SemanticProgramQueryFact *findSemanticProductQueryFact(") !=
        std::string::npos);
  CHECK(semanticTargetAdapterHeader.find("const SemanticProgramTryFact *findSemanticProductTryFact(") !=
        std::string::npos);
  CHECK(semanticTargetAdapterHeader.find("std::unordered_map<uint64_t, const SemanticProgramBindingFact *> bindingFactsByExpr;") !=
        std::string::npos);
  CHECK(semanticTargetAdapterHeader.find("const SemanticProgramBindingFact *findSemanticProductBindingFact(const SemanticProductIndex &semanticIndex,") !=
        std::string::npos);
  CHECK(semanticTargetAdapterHeader.find("const SemanticProgramBindingFact *findSemanticProductBindingFact(") !=
        std::string::npos);
  CHECK(semanticTargetAdapterSource.find("buildSemanticProductTargetAdapter(const SemanticProgram *semanticProgram)") !=
        std::string::npos);
  CHECK(semanticTargetAdapterSource.find("SemanticProductIndex buildSemanticProductIndex(const SemanticProgram *semanticProgram)") !=
        std::string::npos);
  CHECK(semanticTargetAdapterSource.find("adapter.hasSemanticProduct = true;") != std::string::npos);
  CHECK(semanticTargetAdapterSource.find("adapter.publishedRoutingLookups = &semanticProgram->publishedRoutingLookups;") !=
        std::string::npos);
  CHECK(semanticTargetAdapterSource.find("adapter.semanticIndex = buildSemanticProductIndex(semanticProgram);") !=
        std::string::npos);
  CHECK(semanticTargetAdapterSource.find("semanticProgramLookupPublishedDirectCallTargetId(*adapter.semanticProgram,") !=
        std::string::npos);
  CHECK(semanticTargetAdapterSource.find("semanticProgramLookupPublishedMethodCallTargetId(*adapter.semanticProgram,") !=
        std::string::npos);
  CHECK(semanticTargetAdapterSource.find("semanticProgramLookupPublishedBridgePathChoiceId(*adapter.semanticProgram,") !=
        std::string::npos);
  CHECK(semanticTargetAdapterSource.find("semanticProgramLookupPublishedDirectCallTargetStdlibSurfaceId(") !=
        std::string::npos);
  CHECK(semanticTargetAdapterSource.find("semanticProgramLookupPublishedMethodCallTargetStdlibSurfaceId(") !=
        std::string::npos);
  CHECK(semanticTargetAdapterSource.find("semanticProgramLookupPublishedBridgePathChoiceStdlibSurfaceId(") !=
        std::string::npos);
  CHECK(semanticTargetAdapterSource.find("semanticProgramLookupPublishedCallableSummary(*adapter.semanticProgram, fullPath)") !=
        std::string::npos);
  CHECK(semanticTargetAdapterSource.find("semanticProgramDirectCallTargetView(*semanticProgram)") ==
        std::string::npos);
  CHECK(semanticTargetAdapterSource.find("semanticProgramMethodCallTargetView(*semanticProgram)") ==
        std::string::npos);
  CHECK(semanticTargetAdapterSource.find("semanticProgramBridgePathChoiceView(*semanticProgram)") ==
        std::string::npos);
  CHECK(semanticTargetAdapterSource.find("semanticProgramCallableSummaryView(*semanticProgram)") ==
        std::string::npos);
  CHECK(semanticTargetAdapterSource.find("semanticProgramOnErrorFactView(*semanticProgram)") !=
        std::string::npos);
  CHECK(semanticTargetAdapterSource.find("publishedRoutingLookups.onErrorFactIndicesByDefinitionId.empty()") !=
        std::string::npos);
  CHECK(semanticTargetAdapterSource.find("publishedRoutingLookups.localAutoFactIndicesByExpr.empty()") !=
        std::string::npos);
  CHECK(semanticTargetAdapterSource.find("publishedRoutingLookups.queryFactIndicesByExpr.empty()") !=
        std::string::npos);
  CHECK(semanticTargetAdapterSource.find("publishedRoutingLookups.tryFactIndicesByExpr.empty()") !=
        std::string::npos);
  CHECK(semanticTargetAdapterSource.find("findDefinitionScopedSemanticFact(semanticIndex.onErrorFactsByDefinitionId, definition)") !=
        std::string::npos);
  CHECK(semanticTargetAdapterSource.find("semanticProgramLookupPublishedOnErrorFactByDefinitionPathId(") !=
        std::string::npos);
  CHECK(semanticTargetAdapterSource.find("index.onErrorFactsByDefinitionPathId.reserve(onErrorFacts.size())") !=
        std::string::npos);
  CHECK(semanticTargetAdapterSource.find("adapter.onErrorFactsByDefinitionId.reserve(onErrorFacts.size())") ==
        std::string::npos);
  CHECK(semanticTargetAdapterSource.find("adapter.onErrorFactsByDefinitionPathId.reserve(onErrorFacts.size())") ==
        std::string::npos);
  CHECK(semanticTargetAdapterSource.find("adapter.onErrorFactsByDefinitionPath.reserve(semanticProgram->onErrorFacts.size())") ==
        std::string::npos);
  CHECK(semanticTargetAdapterSource.find("const auto onErrorFacts = semanticProgramOnErrorFactView(*adapter.semanticProgram);") ==
        std::string::npos);
  CHECK(semanticTargetAdapterSource.find("if (entry->definitionPathId == *definitionPathId)") ==
        std::string::npos);
  CHECK(semanticTargetAdapterSource.find("buildDirectCallIndex(index);") == std::string::npos);
  CHECK(semanticTargetAdapterSource.find("buildMethodCallIndex(index);") == std::string::npos);
  CHECK(semanticTargetAdapterSource.find("buildBridgePathChoiceIndex(index);") == std::string::npos);
  CHECK(semanticTargetAdapterSource.find("semanticProgramLookupCallTargetStringId(*semanticProgram, definition.fullPath)") !=
        std::string::npos);
  CHECK(semanticTargetAdapterSource.find("adapter.typeMetadataByPath.reserve(semanticProgram->typeMetadata.size())") ==
        std::string::npos);
  CHECK(semanticTargetAdapterSource.find("adapter.orderedStructTypeMetadata.reserve(semanticProgram->typeMetadata.size())") ==
        std::string::npos);
  CHECK(semanticTargetAdapterSource.find("adapter.orderedStructTypeMetadata.push_back(&entry);") ==
        std::string::npos);
  CHECK(semanticTargetAdapterSource.find("adapter.structFieldMetadataByStructPath.reserve(semanticProgram->structFieldMetadata.size())") ==
        std::string::npos);
  CHECK(semanticTargetAdapterSource.find("std::stable_sort(entries.begin(),") == std::string::npos);
  CHECK(semanticTargetAdapterSource.find("semanticProgramReturnFactView(*semanticProgram)") !=
        std::string::npos);
  CHECK(semanticTargetAdapterSource.find("index.returnFactsByDefinitionId.reserve(returnFacts.size())") !=
        std::string::npos);
  CHECK(semanticTargetAdapterSource.find("index.returnFactsByDefinitionPathId.reserve(returnFacts.size())") !=
        std::string::npos);
  CHECK(semanticTargetAdapterSource.find("adapter.returnFactsByDefinitionPath.reserve(") == std::string::npos);
  CHECK(semanticTargetAdapterSource.find("findDefinitionScopedSemanticFact(semanticIndex.returnFactsByDefinitionId,") !=
        std::string::npos);
  CHECK(semanticTargetAdapterSource.find("adapter.returnFactsByDefinitionId.reserve(returnFacts.size())") ==
        std::string::npos);
  CHECK(semanticTargetAdapterSource.find("adapter.returnFactsByDefinitionPathId.reserve(returnFacts.size())") ==
        std::string::npos);
  CHECK(semanticTargetAdapterSource.find("semanticProgramLookupCallTargetStringId(*semanticProgram, definition.fullPath)") !=
        std::string::npos);
  CHECK(semanticTargetAdapterSource.find("semanticProgramLocalAutoFactView(*semanticProgram)") !=
        std::string::npos);
  CHECK(semanticTargetAdapterSource.find("index.localAutoFactsByInitPathAndBindingNameId.reserve(localAutoFacts.size())") !=
        std::string::npos);
  CHECK(semanticTargetAdapterSource.find("semanticIndex.localAutoFactsByInitPathAndBindingNameId.find(") !=
        std::string::npos);
  CHECK(semanticTargetAdapterSource.find("adapter.localAutoFactsByExpr.reserve(localAutoFacts.size())") ==
        std::string::npos);
  CHECK(semanticTargetAdapterSource.find("adapter.localAutoFactsByInitPathAndBindingNameId.reserve(localAutoFacts.size())") ==
        std::string::npos);
  CHECK(semanticTargetAdapterSource.find("const auto localAutoFacts = semanticProgramLocalAutoFactView(*adapter.semanticProgram);") ==
        std::string::npos);
  CHECK(semanticTargetAdapterSource.find("resolveLocalAutoInitializerPathId(semanticProgram, expr)") !=
        std::string::npos);
  CHECK(semanticTargetAdapterSource.find("semanticProgramLookupPublishedLocalAutoFactByInitializerPathAndBindingNameId(") !=
        std::string::npos);
  CHECK(semanticTargetAdapterSource.find("semanticProgramQueryFactView(*semanticProgram)") !=
        std::string::npos);
  CHECK(semanticTargetAdapterSource.find("index.queryFactsByResolvedPathAndCallNameId.reserve(queryFacts.size())") !=
        std::string::npos);
  CHECK(semanticTargetAdapterSource.find("adapter.semanticIndex.queryFactsByResolvedPathAndCallNameId.find(") !=
        std::string::npos);
  CHECK(semanticTargetAdapterSource.find("adapter.queryFactsByExpr.reserve(queryFacts.size())") ==
        std::string::npos);
  CHECK(semanticTargetAdapterSource.find("adapter.queryFactsByResolvedPathAndCallNameId.reserve(queryFacts.size())") ==
        std::string::npos);
  CHECK(semanticTargetAdapterSource.find("const auto queryFacts = semanticProgramQueryFactView(*adapter.semanticProgram);") ==
        std::string::npos);
  CHECK(semanticTargetAdapterSource.find("resolveSemanticExprPathId(adapter, expr)") !=
        std::string::npos);
  CHECK(semanticTargetAdapterSource.find("semanticProgramLookupPublishedQueryFactByResolvedPathAndCallNameId(") !=
        std::string::npos);
  CHECK(semanticTargetAdapterSource.find("semanticProgramTryFactView(*semanticProgram)") !=
        std::string::npos);
  CHECK(semanticTargetAdapterSource.find("index.tryFactsByOperandPathAndSource.reserve(tryFacts.size())") !=
        std::string::npos);
  CHECK(semanticTargetAdapterSource.find("adapter.semanticIndex.tryFactsByOperandPathAndSource.find(") !=
        std::string::npos);
  CHECK(semanticTargetAdapterSource.find("adapter.tryFactsByExpr.reserve(tryFacts.size())") ==
        std::string::npos);
  CHECK(semanticTargetAdapterSource.find("adapter.tryFactsByOperandPathAndSource.reserve(tryFacts.size())") ==
        std::string::npos);
  CHECK(semanticTargetAdapterSource.find("const auto tryFacts = semanticProgramTryFactView(*adapter.semanticProgram);") ==
        std::string::npos);
  CHECK(semanticTargetAdapterSource.find("makeTryFactOperandPathSourceKey(*operandPathId, expr.sourceLine, expr.sourceColumn)") !=
        std::string::npos);
  CHECK(semanticTargetAdapterSource.find("semanticProgramLookupPublishedTryFactByOperandPathAndSource(") !=
        std::string::npos);
  CHECK(semanticTargetAdapterSource.find("semanticProgramBindingFactView(*semanticProgram)") !=
        std::string::npos);
  CHECK(semanticTargetAdapterSource.find("findExpressionScopedSemanticFact(semanticIndex.bindingFactsByExpr, expr)") !=
        std::string::npos);
  CHECK(semanticTargetAdapterSource.find("adapter.bindingFactsByExpr.reserve(bindingFacts.size())") ==
        std::string::npos);
  CHECK(semanticTargetAdapterSource.find("definition.fullPath.empty()") == std::string::npos);
  CHECK(semanticTargetAdapterSource.find("expr.semanticNodeId") != std::string::npos);
  CHECK(semanticTargetAdapterSource.find("insert_or_assign(entry.semanticNodeId, &entry)") != std::string::npos);
  CHECK(semanticTargetAdapterSource.find("std::string findSemanticProductDirectCallTarget(const SemanticProgram *semanticProgram, const Expr &expr)") !=
        std::string::npos);
  CHECK(semanticTargetAdapterSource.find("std::string findSemanticProductMethodCallTarget(const SemanticProgram *semanticProgram, const Expr &expr)") !=
        std::string::npos);
  CHECK(semanticTargetAdapterSource.find("std::string findSemanticProductBridgePathChoice(const SemanticProgram *semanticProgram, const Expr &expr)") !=
        std::string::npos);
  CHECK(semanticTargetAdapterSource.find("const SemanticProgramCallableSummary *findSemanticProductCallableSummary(const SemanticProgram *semanticProgram,") !=
        std::string::npos);
  CHECK(semanticTargetAdapterSource.find("const SemanticProgramTypeMetadata *findSemanticProductTypeMetadata(") ==
        std::string::npos);
  CHECK(semanticTargetAdapterSource.find("findSemanticProductStructFieldMetadata(") ==
        std::string::npos);
  CHECK(lowerImportsStructsSetupHeader.find("const SemanticProgram *semanticProgram,") != std::string::npos);
  CHECK(lowerImportsStructsSetupSource.find("buildSemanticProductTargetAdapter(semanticProgram)") ==
        std::string::npos);
  CHECK(lowerImportsStructsSetupSource.find("buildDefinitionMapAndStructNames(program.definitions, defMapOut, structNamesOut, semanticProgram);") !=
        std::string::npos);
  CHECK(lowerImportsStructsSetupSource.find("program,\n          structNamesOut,\n          resolveStructTypePath,\n          defMapOut,\n          importAliasesOut,\n          semanticProgram,") !=
        std::string::npos);
  CHECK(lowerImportsStructsSetupSource.find("semanticProgram,\n          layout,\n          layoutError);") !=
        std::string::npos);
  CHECK(lowerImportsStructsSetupSource.find("program, defMapOut, semanticProgram, computeStructLayout, outModule.structLayouts, errorOut") !=
        std::string::npos);
  CHECK(lowerImportsStructsSetupSource.find("&semanticProductTargets,") == std::string::npos);
  CHECK(lowerSetupImportsStructs.find("program, semanticProgram, out, defMap, structNames, importAliases, structFieldInfoByName, error") !=
        std::string::npos);
  CHECK(structTypeHelpersHeader.find("const SemanticProgram *semanticProgram = nullptr") !=
        std::string::npos);
  CHECK(structTypeHelpersSource.find("const auto structTypeMetadata = semanticProgramStructTypeMetadataView(*semanticProgram);") !=
        std::string::npos);
  CHECK(structTypeHelpersSource.find("if (semanticProductTargets != nullptr && !semanticProductTargets->typeMetadataByPath.empty()) {") ==
        std::string::npos);
  CHECK(structTypeHelpersSource.find("isStructLikeSemanticProductCategory(typeMetadata->category)") !=
        std::string::npos);
  CHECK(structFieldBindingHelpersHeader.find("const SemanticProgram *semanticProgram,") !=
        std::string::npos);
  CHECK(structFieldBindingHelpersSource.find("if (!isStructDefinition(def, semanticProgram)) {") !=
        std::string::npos);
  CHECK(structFieldBindingHelpersSource.find("layoutFieldBindingFromSemanticProduct(") !=
        std::string::npos);
  CHECK(structFieldBindingHelpersSource.find("semanticProgramStructFieldMetadataView(*semanticProgram, def.fullPath)") !=
        std::string::npos);
  CHECK(structFieldBindingHelpersSource.find("findSemanticProductStructFieldMetadata(*semanticProductTargets, def.fullPath)") ==
        std::string::npos);
  CHECK(structFieldBindingHelpersHeader.find("bool resolveStructLayoutFieldBinding(") !=
        std::string::npos);
  CHECK(structLayoutHelpersHeader.find("const SemanticProgramTypeMetadata *typeMetadata,") !=
        std::string::npos);
  CHECK(structLayoutHelpersHeader.find("const std::unordered_map<std::string, const Definition *> &defMap,") !=
        std::string::npos);
  CHECK(structLayoutHelpersHeader.find("const SemanticProgram *semanticProgram,") !=
        std::string::npos);
  CHECK(structLayoutHelpersSource.find("typeMetadata != nullptr && typeMetadata->hasExplicitAlignment") !=
        std::string::npos);
  CHECK(structLayoutHelpersSource.find("semanticProgramLookupTypeMetadata(*semanticProgram, def.fullPath)") !=
        std::string::npos);
  CHECK(structLayoutHelpersSource.find("const auto structTypeMetadata = semanticProgramStructTypeMetadataView(*semanticProgram);") !=
        std::string::npos);
  CHECK(structLayoutHelpersSource.find("findSemanticProductTypeMetadata(*semanticProductTargets, def.fullPath)") ==
        std::string::npos);
  CHECK(structLayoutHelpersSource.find("if (semanticProductTargets != nullptr && !semanticProductTargets->orderedStructTypeMetadata.empty()) {") ==
        std::string::npos);
  CHECK(structLayoutHelpersSource.find("const auto defIt = defMap.find(typeMetadata->fullPath);") !=
        std::string::npos);
  CHECK(structLayoutHelpersSource.find("if (!isStructDefinition(def, semanticProgram)) {") !=
        std::string::npos);
  CHECK(callAccessHelpersHeader.find("struct SemanticProductTargetAdapter;") !=
        std::string::npos);
  CHECK(callAccessHelpersHeader.find("struct Definition;") != std::string::npos);
  CHECK(callAccessHelpersHeader.find("bool emitMapLookupContains(") != std::string::npos);
  CHECK(callAccessHelpersHeader.find("bool isStructDefinition(const Definition &def, const SemanticProgram *semanticProgram);") !=
        std::string::npos);
  CHECK(callAccessHelpersHeader.find("bool isStructDefinition(const Definition &def, const SemanticProductTargetAdapter *semanticProductTargets);") !=
        std::string::npos);
  CHECK(bindingTypeHelpersHeader.find("bool validateSemanticProductBindingCoverage(const Program &program,") !=
        std::string::npos);
  CHECK(bindingTypeHelpersHeader.find("BindingTypeAdapters makeBindingTypeAdapters(const SemanticProgram *semanticProgram = nullptr);") !=
        std::string::npos);
  CHECK(bindingTypeHelpersSource.find("findSemanticProductBindingFact(semanticIndex, expr)") !=
        std::string::npos);
  CHECK(bindingTypeHelpersSource.find("findSemanticProductLocalAutoFactBySemanticId(semanticIndex, expr)") !=
        std::string::npos);
  CHECK(bindingTypeHelpersSource.find("requiresSemanticBindingFact(semanticProgram, expr)") !=
        std::string::npos);
  CHECK(statementBindingHelpersHeader.find("const ResolveDefinitionCallForStatementFn &resolveDefinitionCall,") !=
        std::string::npos);
  CHECK(statementBindingHelpersHeader.find("const SemanticProgram *semanticProgram = nullptr,") !=
        std::string::npos);
  CHECK(statementBindingHelpersHeader.find("const SemanticProductIndex *semanticIndex = nullptr);") !=
        std::string::npos);
  CHECK(statementBindingHelpersHeader.find("const SemanticProductTargetAdapter *semanticProductTargets);") !=
        std::string::npos);
  CHECK(statementBindingHelpersHeader.find("bool isPointerMemoryIntrinsicCall(const Expr &expr);") !=
        std::string::npos);
  CHECK(statementBindingHelpersHeader.find("bool inferPointerMemoryIntrinsicTargetsUninitializedStorage(const Expr &expr, const LocalMap &localsIn);") !=
        std::string::npos);
  CHECK(lowerReturnCallsSetupHeader.find("struct IrFunction;") != std::string::npos);

  const auto lowerInlineCallContextSetupStepHeader = readFile(
      "include/primec/testing/ir_lowerer_helpers/IrLowererLowerInlineCallContextSetupStep.h");
  CHECK(lowerInlineCallContextSetupStepHeader.find("struct IrFunction;") != std::string::npos);
  CHECK(lowerInlineCallContextSetupStepHeader.find("struct ReturnInfo;") != std::string::npos);

  const auto lowerStatementsEntryStatementStepHeader = readFile(
      "include/primec/testing/ir_lowerer_helpers/IrLowererLowerStatementsEntryStatementStep.h");
  CHECK(lowerStatementsEntryStatementStepHeader.find("struct IrFunction;") != std::string::npos);

  const auto lowerInlineCallReturnValueStepHeader = readFile(
      "include/primec/testing/ir_lowerer_helpers/IrLowererLowerInlineCallReturnValueStep.h");
  CHECK(lowerInlineCallReturnValueStepHeader.find("struct IrFunction;") != std::string::npos);

  const auto lowerInlineCallStatementStepHeader = readFile(
      "include/primec/testing/ir_lowerer_helpers/IrLowererLowerInlineCallStatementStep.h");
  CHECK(lowerInlineCallStatementStepHeader.find("struct IrFunction;") != std::string::npos);

  const auto lowerInlineCallCleanupStepHeader = readFile(
      "include/primec/testing/ir_lowerer_helpers/IrLowererLowerInlineCallCleanupStep.h");
  CHECK(lowerInlineCallCleanupStepHeader.find("struct IrFunction;") != std::string::npos);

  const auto lowerStatementsFunctionTableStepHeader = readFile(
      "include/primec/testing/ir_lowerer_helpers/IrLowererLowerStatementsFunctionTableStep.h");
  CHECK(lowerStatementsFunctionTableStepHeader.find("struct Program;") != std::string::npos);
  CHECK(lowerStatementsFunctionTableStepHeader.find("struct Definition;") != std::string::npos);
  CHECK(lowerStatementsFunctionTableStepHeader.find("struct SemanticProgram;") != std::string::npos);
  CHECK(lowerStatementsFunctionTableStepHeader.find("struct IrFunction;") != std::string::npos);
  CHECK(lowerStatementsFunctionTableStepHeader.find("struct ReturnInfo;") != std::string::npos);

  const auto bindingTypeHelpersHeader =
      readFile("include/primec/testing/ir_lowerer_helpers/IrLowererBindingTypeHelpers.h");
  CHECK(bindingTypeHelpersHeader.find("struct SemanticProgram;") != std::string::npos);

  const auto callDispatchHelpersHeader =
      readFile("include/primec/testing/ir_lowerer_helpers/IrLowererCallDispatchHelpers.h");
  CHECK(callDispatchHelpersHeader.find("struct Definition;") != std::string::npos);
  CHECK(callDispatchHelpersHeader.find("struct SemanticProgram;") != std::string::npos);
  CHECK(callDispatchHelpersHeader.find("struct SemanticProductTargetAdapter;") != std::string::npos);

  const auto countAccessHelpersHeader =
      readFile("include/primec/testing/ir_lowerer_helpers/IrLowererCountAccessHelpers.h");
  CHECK(countAccessHelpersHeader.find("struct Definition;") != std::string::npos);
  CHECK(countAccessHelpersHeader.find("struct SemanticProgram;") != std::string::npos);

  const auto flowHelpersHeader = readFile("include/primec/testing/ir_lowerer_helpers/IrLowererFlowHelpers.h");
  CHECK(flowHelpersHeader.find("struct Definition;") != std::string::npos);
  CHECK(flowHelpersHeader.find("struct ReturnInfo;") != std::string::npos);

  const auto inlineCallContextHelpersHeader =
      readFile("include/primec/testing/ir_lowerer_helpers/IrLowererInlineCallContextHelpers.h");
  CHECK(inlineCallContextHelpersHeader.find("struct Definition;") != std::string::npos);
  CHECK(inlineCallContextHelpersHeader.find("struct OnErrorHandler;") != std::string::npos);
  CHECK(inlineCallContextHelpersHeader.find("struct ResultReturnInfo;") != std::string::npos);
  CHECK(inlineCallContextHelpersHeader.find("struct ReturnInfo;") != std::string::npos);
  CHECK(inlineCallContextHelpersHeader.find("OnErrorByDefinition") == std::string::npos);

  const auto lowerEffectsHeader = readFile("include/primec/testing/ir_lowerer_helpers/IrLowererLowerEffects.h");
  CHECK(lowerEffectsHeader.find("struct Definition;") != std::string::npos);
  CHECK(lowerEffectsHeader.find("struct Program;") != std::string::npos);
  CHECK(lowerEffectsHeader.find("struct SemanticProgram;") != std::string::npos);
  CHECK(lowerEffectsHeader.find("validateGpuNoSoftwareNumericTypes(") == std::string::npos);
  CHECK(lowerEffectsHeader.find("validateGpuProgramEffects(") == std::string::npos);
  CHECK(lowerEffectsHeader.find("validateNativeNoSoftwareNumericTypes(") == std::string::npos);
  CHECK(lowerEffectsHeader.find("validateNativeProgramEffects(") == std::string::npos);
  const auto gpuEffectsHeader = readFile("include/primec/testing/ir_lowerer_helpers/IrLowererGpuEffects.h");
  CHECK(gpuEffectsHeader.find("validateGpuNoSoftwareNumericTypes(") != std::string::npos);
  CHECK(gpuEffectsHeader.find("validateGpuNoRuntimeReflectionQueries(") != std::string::npos);
  CHECK(gpuEffectsHeader.find("validateGpuProgramEffects(") != std::string::npos);
  const auto nativeEffectsHeader = readFile("include/primec/testing/ir_lowerer_helpers/IrLowererNativeEffects.h");
  CHECK(nativeEffectsHeader.find("validateNativeNoSoftwareNumericTypes(") != std::string::npos);
  CHECK(nativeEffectsHeader.find("validateNativeNoRuntimeReflectionQueries(") != std::string::npos);
  CHECK(nativeEffectsHeader.find("validateNativeProgramEffects(") != std::string::npos);
  const auto vmEffectsHeader = readFile("include/primec/testing/ir_lowerer_helpers/IrLowererVmEffects.h");
  CHECK(vmEffectsHeader.find("validateVmProgramEffects(") != std::string::npos);

  const auto lowerEntrySetupHeader =
      readFile("include/primec/testing/ir_lowerer_helpers/IrLowererLowerEntrySetup.h");
  CHECK(lowerEntrySetupHeader.find("struct Definition;") != std::string::npos);
  CHECK(lowerEntrySetupHeader.find("struct Program;") != std::string::npos);
  CHECK(lowerEntrySetupHeader.find("struct SemanticProgram;") != std::string::npos);
  CHECK(lowerEntrySetupHeader.find("::primec::IrValidationTarget validationTarget,") != std::string::npos);

  const auto lowerImportsStructsSetupHeader =
      readFile("include/primec/testing/ir_lowerer_helpers/IrLowererLowerImportsStructsSetup.h");
  CHECK(lowerImportsStructsSetupHeader.find("struct Definition;") != std::string::npos);
  CHECK(lowerImportsStructsSetupHeader.find("struct LayoutFieldBinding;") != std::string::npos);
  CHECK(lowerImportsStructsSetupHeader.find("struct Program;") != std::string::npos);
  CHECK(lowerImportsStructsSetupHeader.find("struct SemanticProgram;") != std::string::npos);

  const auto lowerInferenceSetupHeader =
      readFile("include/primec/testing/ir_lowerer_helpers/IrLowererLowerInferenceSetup.h");
  CHECK(lowerInferenceSetupHeader.find("struct Definition;") != std::string::npos);
  CHECK(lowerInferenceSetupHeader.find("struct Program;") != std::string::npos);
  CHECK(lowerInferenceSetupHeader.find("struct ReturnInfo;") != std::string::npos);
  CHECK(lowerInferenceSetupHeader.find("struct SemanticProductTargetAdapter;") != std::string::npos);

  const auto onErrorHelpersHeader =
      readFile("include/primec/testing/ir_lowerer_helpers/IrLowererOnErrorHelpers.h");
  CHECK(onErrorHelpersHeader.find("struct Program;") != std::string::npos);
  CHECK(onErrorHelpersHeader.find("struct Definition;") != std::string::npos);
  CHECK(onErrorHelpersHeader.find("struct OnErrorHandler;") != std::string::npos);
  CHECK(onErrorHelpersHeader.find("struct SemanticProgram;") != std::string::npos);

  const auto returnInferenceHelpersHeader =
      readFile("include/primec/testing/ir_lowerer_helpers/IrLowererReturnInferenceHelpers.h");
  CHECK(returnInferenceHelpersHeader.find("struct Definition;") != std::string::npos);
  CHECK(returnInferenceHelpersHeader.find("struct ResultReturnInfo;") != std::string::npos);
  CHECK(returnInferenceHelpersHeader.find("struct ReturnInfo;") != std::string::npos);
  CHECK(returnInferenceHelpersHeader.find("struct SemanticProgram;") != std::string::npos);

  const auto setupTypeHelpersHeader =
      readFile("include/primec/testing/ir_lowerer_helpers/IrLowererSetupTypeHelpers.h");
  CHECK(setupTypeHelpersHeader.find("struct Definition;") != std::string::npos);
  CHECK(setupTypeHelpersHeader.find("struct ReturnInfo;") != std::string::npos);
  CHECK(setupTypeHelpersHeader.find("struct SemanticProductTargetAdapter;") != std::string::npos);

  CHECK(statementCallHelpersHeader.find("struct Definition;") != std::string::npos);
  CHECK(statementCallHelpersHeader.find("struct IrFunction;") != std::string::npos);
  CHECK(statementCallHelpersHeader.find("struct OnErrorHandler;") != std::string::npos);
  CHECK(statementCallHelpersHeader.find("struct Program;") != std::string::npos);
  CHECK(statementCallHelpersHeader.find("struct ResultReturnInfo;") != std::string::npos);
  CHECK(statementCallHelpersHeader.find("struct ReturnInfo;") != std::string::npos);
  CHECK(statementCallHelpersHeader.find("struct SemanticProgram;") != std::string::npos);

  const auto structLayoutHelpersHeader =
      readFile("include/primec/testing/ir_lowerer_helpers/IrLowererStructLayoutHelpers.h");
  CHECK(structLayoutHelpersHeader.find("struct Definition;") != std::string::npos);
  CHECK(structLayoutHelpersHeader.find("struct IrStructLayout;") != std::string::npos);
  CHECK(structLayoutHelpersHeader.find("struct LayoutFieldBinding;") != std::string::npos);
  CHECK(structLayoutHelpersHeader.find("struct Program;") != std::string::npos);
  CHECK(structLayoutHelpersHeader.find("struct SemanticProgram;") != std::string::npos);

  const auto structTypeHelpersHeader =
      readFile("include/primec/testing/ir_lowerer_helpers/IrLowererStructTypeHelpers.h");
  CHECK(structTypeHelpersHeader.find("struct Definition;") != std::string::npos);
  CHECK(structTypeHelpersHeader.find("struct LayoutFieldBinding;") != std::string::npos);
  CHECK(structTypeHelpersHeader.find("struct Program;") != std::string::npos);

  const auto lowerLocalsSetupHeader =
      readFile("include/primec/testing/ir_lowerer_helpers/IrLowererLowerLocalsSetup.h");
  CHECK(lowerLocalsSetupHeader.find("struct Definition;") != std::string::npos);
  CHECK(lowerLocalsSetupHeader.find("struct IrFunction;") != std::string::npos);
  CHECK(lowerLocalsSetupHeader.find("struct LayoutFieldBinding;") != std::string::npos);
  CHECK(lowerLocalsSetupHeader.find("struct Program;") != std::string::npos);
  CHECK(lowerLocalsSetupHeader.find("struct SemanticProgram;") != std::string::npos);

  const auto lowerSetupStageHeader =
      readFile("include/primec/testing/ir_lowerer_helpers/IrLowererLowerSetupStage.h");
  CHECK(lowerSetupStageHeader.find("struct Definition;") != std::string::npos);
  CHECK(lowerSetupStageHeader.find("struct IrFunction;") != std::string::npos);
  CHECK(lowerSetupStageHeader.find("struct Program;") != std::string::npos);
  CHECK(lowerSetupStageHeader.find("struct SemanticProgram;") != std::string::npos);
  CHECK(lowerSetupStageHeader.find("struct LowerSetupStageInput;") != std::string::npos);
  CHECK(lowerSetupStageHeader.find("struct LowerSetupStageState;") != std::string::npos);
  CHECK(lowerSetupStageHeader.find("bool runLowerSetupStage(") != std::string::npos);

  const auto lowerReturnEmitStageHeader =
      readFile("include/primec/testing/ir_lowerer_helpers/IrLowererLowerReturnEmitStage.h");
  CHECK(lowerReturnEmitStageHeader.find("struct Definition;") != std::string::npos);
  CHECK(lowerReturnEmitStageHeader.find("struct LowerSetupStageState;") != std::string::npos);
  CHECK(lowerReturnEmitStageHeader.find("struct LowerReturnEmitInlineContext;") != std::string::npos);
  CHECK(lowerReturnEmitStageHeader.find("struct LowerReturnEmitStageInput;") != std::string::npos);
  CHECK(lowerReturnEmitStageHeader.find("struct LowerReturnEmitStageState;") != std::string::npos);
  CHECK(lowerReturnEmitStageHeader.find("bool runLowerReturnEmitStage(") != std::string::npos);

  const auto lowerStatementsCallsStepHeader =
      readFile("include/primec/testing/ir_lowerer_helpers/IrLowererLowerStatementsCallsStep.h");
  CHECK(lowerStatementsCallsStepHeader.find("struct Definition;") != std::string::npos);
  CHECK(lowerStatementsCallsStepHeader.find("struct ReturnInfo;") != std::string::npos);

  const auto lowerStatementsEntryExecutionStepHeader = readFile(
      "include/primec/testing/ir_lowerer_helpers/IrLowererLowerStatementsEntryExecutionStep.h");
  CHECK(lowerStatementsEntryExecutionStepHeader.find("struct Definition;") != std::string::npos);
  CHECK(lowerStatementsEntryExecutionStepHeader.find("struct OnErrorHandler;") !=
        std::string::npos);
  CHECK(lowerStatementsEntryExecutionStepHeader.find("struct ResultReturnInfo;") !=
        std::string::npos);

  const auto lowerStatementsSourceMapStepHeader =
      readFile("include/primec/testing/ir_lowerer_helpers/IrLowererLowerStatementsSourceMapStep.h");
  CHECK(lowerStatementsSourceMapStepHeader.find("struct Definition;") != std::string::npos);

  const auto lowerStatementsCallsStageHeader =
      readFile("include/primec/testing/ir_lowerer_helpers/IrLowererLowerStatementsCallsStage.h");
  CHECK(lowerStatementsCallsStageHeader.find("struct Definition;") != std::string::npos);
  CHECK(lowerStatementsCallsStageHeader.find("struct IrFunction;") != std::string::npos);
  CHECK(lowerStatementsCallsStageHeader.find("struct Program;") != std::string::npos);
  CHECK(lowerStatementsCallsStageHeader.find("struct SemanticProgram;") != std::string::npos);
  CHECK(lowerStatementsCallsStageHeader.find("struct LowerStatementsCallsStageInput;") !=
        std::string::npos);
  CHECK(lowerStatementsCallsStageHeader.find("bool runLowerStatementsCallsStage(") !=
        std::string::npos);

  const auto resultHelpersHeader =
      readFile("include/primec/testing/ir_lowerer_helpers/IrLowererResultHelpers.h");
  const auto resultHelpersSource = readFile("src/ir_lowerer/IrLowererResultHelpers.cpp");
  const auto resultInternalHeader = readFile("src/ir_lowerer/IrLowererResultInternal.h");
  const auto resultMetadataHelpersSource = readFile("src/ir_lowerer/IrLowererResultMetadataHelpers.cpp");
  const auto lowerInferenceDispatchSource = readFile("src/ir_lowerer/IrLowererLowerInferenceDispatchSetup.cpp");
  const auto lowerEmitExprTryHelpers = readFile("src/ir_lowerer/IrLowererLowerEmitExprTryHelpers.h");
  CHECK(resultHelpersHeader.find("struct Definition;") != std::string::npos);
  CHECK(resultHelpersHeader.find("struct ReturnInfo;") != std::string::npos);
  CHECK(resultHelpersHeader.find("struct StructSlotLayoutInfo;") != std::string::npos);
  CHECK(resultHelpersHeader.find("struct SemanticProductIndex;") != std::string::npos);
  CHECK(resultHelpersHeader.find("const SemanticProgram *semanticProgram = nullptr,") !=
        std::string::npos);
  CHECK(resultHelpersHeader.find("const SemanticProductIndex *semanticIndex = nullptr,") !=
        std::string::npos);
  CHECK(resultHelpersHeader.find("std::string *errorOut = nullptr") != std::string::npos);
  CHECK(resultHelpersHeader.find("bool validateSemanticProductResultMetadataCompleteness(") !=
        std::string::npos);
  CHECK(resultHelpersSource.find("findSemanticProductQueryFactBySemanticId(*semanticIndex, expr)") !=
        std::string::npos);
  CHECK(resultHelpersSource.find("missing semantic-product query fact: ") != std::string::npos);
  CHECK(resultHelpersSource.find("missing semantic-product callable result metadata: ") !=
        std::string::npos);
  CHECK(resultHelpersSource.find("missing semantic-product return binding type: ") !=
        std::string::npos);
  CHECK(resultHelpersSource.find("incomplete semantic-product query fact: ") != std::string::npos);
  CHECK(resultHelpersSource.find("incomplete semantic-product try fact: try") != std::string::npos);
  CHECK(resultInternalHeader.find("const SemanticProductTargetAdapter *semanticProductTargets") ==
        std::string::npos);
  CHECK(resultInternalHeader.find("const SemanticProgram *semanticProgram = nullptr,") !=
        std::string::npos);
  CHECK(resultInternalHeader.find("const SemanticProductIndex *semanticIndex = nullptr,") !=
        std::string::npos);
  CHECK(resultMetadataHelpersSource.find("const SemanticProductTargetAdapter *semanticProductTargets") ==
        std::string::npos);
  CHECK(resultMetadataHelpersSource.find("findSemanticProductBindingFact(*semanticIndex, valueExpr)") !=
        std::string::npos);
  CHECK(resultMetadataHelpersSource.find("findSemanticProductQueryFactBySemanticId(*semanticIndex, valueExpr)") !=
        std::string::npos);
  CHECK(lowerInferenceDispatchSource.find("findSemanticProductTryFactBySemanticId(*semanticIndex, tryExpr)") !=
        std::string::npos);
  CHECK(lowerInferenceDispatchSource.find("missing semantic-product try fact: try") !=
        std::string::npos);
  CHECK(lowerEmitExprTryHelpers.find(
            "findSemanticProductTryFactBySemanticId(callResolutionAdapters.semanticProductTargets.semanticIndex, expr)") !=
        std::string::npos);
  CHECK(lowerEmitExprTryHelpers.find(
            "findSemanticProductQueryFactBySemanticId(\n"
            "                    callResolutionAdapters.semanticProductTargets.semanticIndex,\n"
            "                    operandExpr)") != std::string::npos);
  CHECK(lowerEmitExprTryHelpers.find(
            "ir_lowerer::getBuiltinComparisonName(*candidateValueExpr, builtinComparisonName)") !=
        std::string::npos);
  CHECK(lowerEmitExprTryHelpers.find("resultInfoOut.valueKind = LocalInfo::ValueKind::Bool;") !=
        std::string::npos);

  const auto runtimeErrorHelpersHeader =
      readFile("include/primec/testing/ir_lowerer_helpers/IrLowererRuntimeErrorHelpers.h");
  CHECK(runtimeErrorHelpersHeader.find("struct IrFunction;") != std::string::npos);

  const auto statementBindingHelpersHeader =
      readFile("include/primec/testing/ir_lowerer_helpers/IrLowererStatementBindingHelpers.h");
  CHECK(statementBindingHelpersHeader.find("struct Definition;") != std::string::npos);
  CHECK(statementBindingHelpersHeader.find("struct ResultReturnInfo;") != std::string::npos);
  CHECK(statementBindingHelpersHeader.find("struct ReturnInfo;") != std::string::npos);
  CHECK(statementBindingHelpersHeader.find("struct SemanticProductIndex;") != std::string::npos);
  CHECK(statementBindingHelpersHeader.find("struct SemanticProductTargetAdapter;") !=
        std::string::npos);
  CHECK(statementBindingHelpersHeader.find("const SemanticProgram *semanticProgram = nullptr,") !=
        std::string::npos);
  CHECK(statementBindingHelpersHeader.find("const SemanticProductIndex *semanticIndex = nullptr);") !=
        std::string::npos);

  const auto structFieldBindingHelpersHeader =
      readFile("include/primec/testing/ir_lowerer_helpers/IrLowererStructFieldBindingHelpers.h");
  CHECK(structFieldBindingHelpersHeader.find("struct Definition;") != std::string::npos);
  CHECK(structFieldBindingHelpersHeader.find("struct Program;") != std::string::npos);

  const auto structReturnPathHelpersHeader =
      readFile("include/primec/testing/ir_lowerer_helpers/IrLowererStructReturnPathHelpers.h");
  CHECK(structReturnPathHelpersHeader.find("struct Definition;") != std::string::npos);
  CHECK(structReturnPathHelpersHeader.find("struct LayoutFieldBinding;") != std::string::npos);

  const auto uninitializedTypeHelpersHeader =
      readFile("include/primec/testing/ir_lowerer_helpers/IrLowererUninitializedTypeHelpers.h");
  CHECK(uninitializedTypeHelpersHeader.find("struct Definition;") != std::string::npos);
  CHECK(uninitializedTypeHelpersHeader.find("struct IrFunction;") != std::string::npos);
  CHECK(uninitializedTypeHelpersHeader.find("struct Program;") != std::string::npos);
  CHECK(operatorConversionsAndCallsHelpersHeader.find("struct Definition;") !=
        std::string::npos);
  CHECK(operatorConversionsAndCallsHelpersHeader.find("struct LayoutFieldBinding;") !=
        std::string::npos);
  CHECK(structLayoutHelpersHeader.find("struct SemanticProgramTypeMetadata;") !=
        std::string::npos);
  CHECK(statementCallHelpersHeader.find("struct StructSlotLayoutInfo;") !=
        std::string::npos);
  const auto inlineParamHelpersHeader =
      readFile("include/primec/testing/ir_lowerer_helpers/IrLowererInlineParamHelpers.h");
  CHECK(inlineParamHelpersHeader.find("struct Definition;") != std::string::npos);
  CHECK(inlineParamHelpersHeader.find("struct StructSlotLayoutInfo;") !=
        std::string::npos);
  const auto inlineStructArgHelpersHeader =
      readFile("include/primec/testing/ir_lowerer_helpers/IrLowererInlineStructArgHelpers.h");
  CHECK(inlineStructArgHelpersHeader.find("struct Definition;") != std::string::npos);
  CHECK(inlineStructArgHelpersHeader.find("struct StructSlotLayoutInfo;") !=
        std::string::npos);
  const auto lowerInlineCallActiveContextStepHeader = readFile(
      "include/primec/testing/ir_lowerer_helpers/IrLowererLowerInlineCallActiveContextStep.h");
  CHECK(lowerInlineCallActiveContextStepHeader.find("struct Definition;") != std::string::npos);
  CHECK(countAccessHelpersHeader.find("const SemanticProgram *semanticProgram,") != std::string::npos);
  CHECK(countAccessHelpersSource.find("resolveEntryArgsParameter(const Definition &entryDef,\n"
                                      "                               const SemanticProgram *semanticProgram,") !=
        std::string::npos);
  CHECK(countAccessHelpersSource.find("bool isExplicitVectorCountMethodCall(const Expr &expr)") !=
        std::string::npos);
  CHECK(countAccessHelpersSource.find("if (isExplicitVectorCountMethodCall(expr)) {\n"
                                      "    return false;\n"
                                      "  }") !=
        std::string::npos);
  CHECK(countAccessHelpersSource.find("if (isExplicitVectorCountMethodCall(expr)) {\n"
                                      "    return CountAccessCallEmitResult::NotHandled;\n"
                                      "  }") !=
        std::string::npos);
  CHECK(countAccessHelpersSource.find("entry.scopePath != entryDef.fullPath || entry.siteKind != \"parameter\"") !=
        std::string::npos);
  CHECK(countAccessHelpersSource.find("entryParamFact->bindingTypeText != \"array<string>\"") !=
        std::string::npos);
  CHECK(countAccessHelpersSource.find("missing semantic-product entry parameter fact: ") !=
        std::string::npos);
  CHECK(onErrorHelpersSource.find("buildEntryCountAccessSetup(entryDef, semanticProgram, out.countAccessSetup, error)") !=
        std::string::npos);
  CHECK(onErrorHelpersSource.find("findSemanticProductOnErrorFact(semanticProgram, semanticIndex, def)") !=
        std::string::npos);
  CHECK(onErrorHelpersSource.find("missing semantic-product on_error fact: ") !=
        std::string::npos);
  CHECK(onErrorHelpersSource.find("missing semantic-product on_error handler path id: ") !=
        std::string::npos);
  CHECK(onErrorHelpersSource.find("fact.boundArgTexts.size() != fact.boundArgCount") !=
        std::string::npos);
  CHECK(onErrorHelpersSource.find("for (const auto &argText : fact.boundArgTexts)") !=
        std::string::npos);
  CHECK(onErrorHelpersSource.find("findOnErrorTransform(") == std::string::npos);
  CHECK(onErrorHelpersSource.find("buildOnErrorByDefinition(program, semanticProgram,") !=
        std::string::npos);
  CHECK(statementCallHelpersHeader.find("const SemanticProgram *semanticProgram,") !=
        std::string::npos);
  CHECK(statementCallHelpersHeader.find("FunctionTableFinalizationResult finalizeEntryFunctionTableAndLowerCallables(\n"
                                        "    const Program &program,\n"
                                        "    const Definition &entryDef,\n"
                                        "    const SemanticProgram *semanticProgram,") !=
        std::string::npos);
  CHECK(statementCallHelpersSource.find("buildSemanticProductTargetAdapter(semanticProgram)") ==
        std::string::npos);
  CHECK(statementCallHelpersSource.find("const auto definitionsByPath = buildDefinitionBodyLookup(program);") !=
        std::string::npos);
  CHECK(statementCallHelpersSource.find("if (semanticProgram != nullptr && !semanticProgram->definitions.empty()) {") !=
        std::string::npos);
  CHECK(statementCallHelpersSource.find("semantic product definition missing AST body: ") !=
        std::string::npos);
  CHECK(statementCallHelpersSource.find("findSemanticProductCallableSummary(semanticProgram, def.fullPath)") !=
        std::string::npos);
  CHECK(statementCallHelpersSource.find("missing semantic-product callable summary: ") !=
        std::string::npos);
  CHECK(functionTableStepHeader.find("const SemanticProgram *semanticProgram = nullptr;") !=
        std::string::npos);
  CHECK(functionTableStepSource.find("input.semanticProgram,") != std::string::npos);
  CHECK(lowerStatementsCalls.find(".semanticProgram = semanticProgram,") != std::string::npos);
  CHECK(setupMathHelpersHeader.find("SetupMathAndBindingAdapters makeSetupMathAndBindingAdapters(bool hasMathImport,") !=
        std::string::npos);
  CHECK(setupMathHelpersHeader.find("struct SemanticProgram;") != std::string::npos);
  CHECK(setupMathHelpersHeader.find("const SemanticProgram *semanticProgram = nullptr);") !=
        std::string::npos);
  CHECK(setupMathHelpersSource.find("adapters.bindingTypeAdapters = makeBindingTypeAdapters(semanticProgram);") !=
        std::string::npos);
  CHECK(uninitializedTypeHelpers.find("bool buildSetupMathTypeStructAndUninitializedResolutionSetup(") !=
        std::string::npos);
  CHECK(uninitializedTypeHelpers.find("const SemanticProgram *semanticProgram,") != std::string::npos);
  CHECK(uninitializedSetupBuilders.find("makeSetupMathAndBindingAdapters(hasMathImport, semanticProgram)") !=
        std::string::npos);
  CHECK(irLowerEffects.find("findSemanticProductCallableSummary(semanticProgram, entryPath)") !=
        std::string::npos);
  CHECK(irLowerEffects.find("missing semantic-product callable summary: ") !=
        std::string::npos);
  CHECK(irLowerEffects.find("validateProgramEffectsForBackendSurface(") !=
        std::string::npos);
  CHECK(irLowerEffects.find("std::string(backendSurfaceName) + \" does not support effect: \"") !=
        std::string::npos);
  CHECK(irLowerEffects.find("validateNativeProgramEffects(") ==
        std::string::npos);
  CHECK(irLowerEffects.find("validateNoSoftwareNumericTypesForBackendSurface(") !=
        std::string::npos);
  CHECK(irLowerEffects.find("validateNoRuntimeReflectionQueriesForBackendSurface(") !=
        std::string::npos);
  CHECK(irReturnInference.find("findSemanticProductCallableSummary(semanticProgram, entryPath)") !=
        std::string::npos);
  CHECK(irReturnInference.find("findSemanticProductReturnFact(semanticProgram, semanticIndex, entryDef)") !=
        std::string::npos);
  CHECK(irReturnInference.find("missing semantic-product return fact: ") !=
        std::string::npos);
  CHECK(irLowerEffects.find("findSemanticProductCallableSummary(semanticProgram, fullPath)") !=
        std::string::npos);
  CHECK(irEntrySetupSource.find("validationTarget == IrValidationTarget::Vm") !=
        std::string::npos);
  CHECK(irEntrySetupSource.find("validationTarget == IrValidationTarget::Glsl") !=
        std::string::npos);
  CHECK(irEntrySetupSource.find("validationTarget == IrValidationTarget::Native") !=
        std::string::npos);
  CHECK(irEntrySetupSource.find("validateGpuProgramEffects(") !=
        std::string::npos);
  CHECK(irEntrySetupSource.find("validateNativeProgramEffects(") !=
        std::string::npos);
  CHECK(irEntrySetupSource.find("validateVmProgramEffects(") != std::string::npos);
  CHECK(irGpuEffects.find("validateNoSoftwareNumericTypesForBackendSurface(") !=
        std::string::npos);
  CHECK(irGpuEffects.find("validateNoRuntimeReflectionQueriesForBackendSurface(") !=
        std::string::npos);
  CHECK(irGpuEffects.find("return validateProgramEffectsForBackendSurface(") !=
        std::string::npos);
  CHECK(irGpuEffects.find("\"gpu backend\"") != std::string::npos);
  CHECK(irNativeEffects.find("validateNoSoftwareNumericTypesForBackendSurface(") !=
        std::string::npos);
  CHECK(irNativeEffects.find("validateNoRuntimeReflectionQueriesForBackendSurface(") !=
        std::string::npos);
  CHECK(irNativeEffects.find("return validateProgramEffectsForBackendSurface(") !=
        std::string::npos);
  CHECK(irNativeEffects.find("\"native backend\"") != std::string::npos);
  CHECK(irVmEffects.find("return validateProgramEffectsForBackendSurface(") !=
        std::string::npos);
  CHECK(irVmEffects.find("\"vm backend\"") != std::string::npos);
  CHECK(primecMain.find("pipelineOutput.hasSemanticProgram ? &pipelineOutput.semanticProgram : nullptr") !=
        std::string::npos);
  CHECK(primecMain.find("describeCompilePipelineFailure(pipelineOutput)") != std::string::npos);
  CHECK(primecMain.find("prepareIrModule(program, semanticProgram, options, validationTarget, ir, prepFailure)") !=
        std::string::npos);
  CHECK(primevmMain.find("pipelineOutput.hasSemanticProgram ? &pipelineOutput.semanticProgram : nullptr") !=
        std::string::npos);
  CHECK(primevmMain.find("describeCompilePipelineFailure(pipelineOutput)") != std::string::npos);
  CHECK(primevmMain.find("prepareIrModule(program, semanticProgram, options, primec::IrValidationTarget::Vm, ir, irFailure)") !=
        std::string::npos);
  CHECK(semanticsValidate.find("SemanticProgram buildSemanticProgram(const Program &program,") !=
        std::string::npos);
  const std::size_t buildSemanticProgramStart =
      semanticsValidate.find("SemanticProgram buildSemanticProgram(const Program &program,");
  const std::size_t semanticProgramFactCountStart =
      semanticsValidate.find("uint64_t semanticProgramFactCount(const SemanticProgram &semanticProgram) {");
  REQUIRE(buildSemanticProgramStart != std::string::npos);
  REQUIRE(semanticProgramFactCountStart != std::string::npos);
  REQUIRE(buildSemanticProgramStart < semanticProgramFactCountStart);
  const std::string buildSemanticProgramBody = semanticsValidate.substr(
      buildSemanticProgramStart, semanticProgramFactCountStart - buildSemanticProgramStart);

  CHECK(buildSemanticProgramBody.find("auto publicationSurface =") != std::string::npos);
  CHECK(buildSemanticProgramBody.find("validator.takeSemanticPublicationSurfaceForSemanticProduct(buildConfig)") !=
        std::string::npos);
  CHECK(buildSemanticProgramBody.find("semantics::buildSemanticProgramFromPublicationSurface(") !=
        std::string::npos);
  CHECK(buildSemanticProgramBody.find("validator.takeCollectedDirectCallTargetsForSemanticProduct()") ==
        std::string::npos);
  CHECK(buildSemanticProgramBody.find("validator.takeCollectedMethodCallTargetsForSemanticProduct()") ==
        std::string::npos);
  CHECK(buildSemanticProgramBody.find("validator.takeCollectedBridgePathChoicesForSemanticProduct()") ==
        std::string::npos);
  CHECK(buildSemanticProgramBody.find("directCallTargetSnapshotsForBridgeDerivation") == std::string::npos);
  CHECK(buildSemanticProgramBody.find("validator.takeCollectedCallableSummariesForSemanticProduct()") ==
        std::string::npos);
  CHECK(buildSemanticProgramBody.find("validator.typeMetadataSnapshotForSemanticProduct()") ==
        std::string::npos);
  CHECK(buildSemanticProgramBody.find("validator.structFieldMetadataSnapshotForSemanticProduct()") ==
        std::string::npos);
  CHECK(buildSemanticProgramBody.find("validator.bindingFactSnapshotForSemanticProduct()") ==
        std::string::npos);
  CHECK(buildSemanticProgramBody.find("validator.returnFactSnapshotForSemanticProduct()") ==
        std::string::npos);
  CHECK(buildSemanticProgramBody.find("validator.localAutoFactSnapshotForSemanticProduct()") ==
        std::string::npos);
  CHECK(buildSemanticProgramBody.find("validator.queryFactSnapshotForSemanticProduct()") ==
        std::string::npos);
  CHECK(buildSemanticProgramBody.find("validator.tryFactSnapshotForSemanticProduct()") ==
        std::string::npos);
  CHECK(buildSemanticProgramBody.find("validator.onErrorFactSnapshotForSemanticProduct()") ==
        std::string::npos);
  CHECK(buildSemanticProgramBody.find("ensureModuleResolvedArtifacts(") == std::string::npos);
  CHECK(buildSemanticProgramBody.find("semanticProgram.moduleResolvedArtifacts.reserve(") ==
        std::string::npos);
  CHECK(buildSemanticProgramBody.find("std::sort(semanticProgram.moduleResolvedArtifacts.begin(),") ==
        std::string::npos);
  CHECK(semanticsValidate.find("*semanticProgramOut = buildSemanticProgram(program, entryPath);") ==
        std::string::npos);
  CHECK(semanticsValidate.find("*semanticProgramOut = buildSemanticProgram(program, entryPath, validator, semanticProductBuildConfig);") !=
        std::string::npos);
}
#endif

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
  const std::string semanticProductSource = readRepoFile("src/SemanticProduct.cpp");
  const std::string semanticsValidate = readRepoFile("src/semantics/SemanticsValidate.cpp");
  const std::string semanticsSnapshots = readRepoFile("src/semantics/SemanticsValidatorSnapshots.cpp");
  const std::string semanticTargetAdapterHeader =
      readRepoFile("src/ir_lowerer/IrLowererSemanticProductTargetAdapters.h");
  const std::string semanticTargetAdapterSource =
      readRepoFile("src/ir_lowerer/IrLowererSemanticProductTargetAdapters.cpp");
  const std::string irCallResolution = readRepoFile("src/ir_lowerer/IrLowererCallResolution.cpp");
  const std::string irMethodResolution =
      readRepoFile("src/ir_lowerer/IrLowererSetupTypeMethodCallResolution.cpp");
  const std::string bindingTypeHelpersSource =
      readRepoFile("src/ir_lowerer/IrLowererBindingTypeHelpers.cpp");
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

  CHECK(semanticProduct.find("struct SemanticProgramDirectCallTarget") != std::string::npos);
  CHECK(semanticProduct.find("struct SemanticProgramMethodCallTarget") != std::string::npos);
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

  CHECK(compilePipelineHeader.find("SemanticProgram semanticProgram;") != std::string::npos);
  CHECK(compilePipelineHeader.find("CompilePipelineFailure failure;") != std::string::npos);
  CHECK(compilePipelineHeader.find("bool hasSemanticProgram = false;") != std::string::npos);
  CHECK(compilePipelineHeader.find("bool hasFailure = false;") != std::string::npos);
  CHECK(semanticsHeader.find("SemanticProgram *semanticProgramOut = nullptr") != std::string::npos);
  CHECK(irPreparationHeader.find("const SemanticProgram *semanticProgram,") != std::string::npos);
  CHECK(irLowererHeader.find("const SemanticProgram *semanticProgram,") != std::string::npos);
  CHECK(irLowererHeader.find("return lower(program, nullptr, entryPath, defaultEffects, entryDefaultEffects, out, error, diagnosticInfo);") ==
        std::string::npos);
  CHECK(irPreparationSource.find("semantic product is required for IR preparation") != std::string::npos);
  CHECK(irLowererEntrySetup.find("semantic product is required for IR lowering") != std::string::npos);
  CHECK(cmake.find("src/semantics/SemanticPublicationBuilders.cpp") != std::string::npos);
  CHECK(cmake.find("src/semantics/SemanticsWorkerSymbolMerge.cpp") != std::string::npos);

  CHECK(compilePipelineSource.find("output.semanticProgram = std::move(semanticProgram);") !=
        std::string::npos);
  CHECK(compilePipelineSource.find("output.hasSemanticProgram = true;") != std::string::npos);
  CHECK(compilePipelineSource.find("output.failure.stage = stage;") != std::string::npos);
  CHECK(compilePipelineSource.find("output.failure.message = message;") != std::string::npos);
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
  CHECK(semanticsValidate.find("*semanticProgramOut = buildSemanticProgram(program, entryPath, validator, semanticProductBuildConfig);") !=
        std::string::npos);
  CHECK(semanticsValidate.find("#include \"SemanticPublicationBuilders.h\"") != std::string::npos);
  CHECK(semanticsValidate.find("buildSemanticProgramFromPublicationSurface(") != std::string::npos);
  CHECK(semanticPublicationBuildersSource.find("normalizeSemanticModulePathKey(") != std::string::npos);
  CHECK(semanticProductSource.find("if (semanticProgram.publishedStorageFrozen) {") !=
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
  CHECK(semanticPublicationBuildersSource.find("state.semanticProgram.publishedRoutingLookups.callableSummaryIndicesByPathId.insert_or_assign(") !=
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

  CHECK(semanticTargetAdapterHeader.find("struct SemanticProductTargetAdapter") != std::string::npos);
  CHECK(semanticTargetAdapterHeader.find("bool hasSemanticProduct = false;") != std::string::npos);
  CHECK(semanticTargetAdapterHeader.find("const SemanticProgram *semanticProgram = nullptr;") !=
        std::string::npos);
  CHECK(semanticTargetAdapterHeader.find("methodCallTargetPathsById") == std::string::npos);
  CHECK(semanticTargetAdapterHeader.find("methodCallTargetIdsByPath") == std::string::npos);
  CHECK(semanticTargetAdapterHeader.find("findSemanticProductOnErrorFact(") != std::string::npos);
  CHECK(semanticTargetAdapterHeader.find("findSemanticProductStructFieldMetadata(") ==
        std::string::npos);
  CHECK(semanticTargetAdapterSource.find("buildSemanticProductTargetAdapter(const SemanticProgram *semanticProgram)") !=
        std::string::npos);
  CHECK(semanticTargetAdapterSource.find("adapter.hasSemanticProduct = true;") != std::string::npos);
  CHECK(semanticTargetAdapterSource.find("adapter.semanticProgram = semanticProgram;") !=
        std::string::npos);
  CHECK(semanticTargetAdapterSource.find("entry->resolvedPathId == InvalidSymbolId") != std::string::npos);
  CHECK(semanticTargetAdapterSource.find("semanticProgramLookupPublishedBridgePathChoiceId(*semanticProgram,") !=
        std::string::npos);
  CHECK(semanticTargetAdapterSource.find("semanticProgramResolveCallTargetString(*semanticProgram, *pathId)") !=
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
  CHECK(irCallResolution.find("const std::string fallbackResolvedPath =\n          resolveCallPathFromPublishedLookups(expr, semanticProgram);") !=
        std::string::npos);
  CHECK(irCallResolution.find("isResidualBridgeHelperPath(resolvedPath)") !=
        std::string::npos);
  CHECK(irCallResolution.find("isResidualBridgeHelperPath(fallbackResolvedPath) &&") !=
        std::string::npos);
  CHECK(irCallResolution.find("resolvesToPublishedDefinitionFamilyTarget(semanticProgram, fallbackResolvedPath)") !=
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
  CHECK(bindingTypeHelpersSource.find("bool validateSemanticProductLocalAutoCoverage(const Program &program,") !=
        std::string::npos);
  CHECK(bindingTypeHelpersSource.find("findSemanticProductLocalAutoFactBySemanticId(semanticIndex, expr)") !=
        std::string::npos);
  CHECK(bindingTypeHelpersSource.find("missing semantic-product local-auto fact: ") != std::string::npos);
  CHECK(bindingTypeHelpersSource.find("missing semantic-product local-auto initializer path id: ") !=
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

  CHECK(primecMain.find("describeCompilePipelineFailure(pipelineOutput)") != std::string::npos);
  CHECK(primevmMain.find("describeCompilePipelineFailure(pipelineOutput)") != std::string::npos);
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
  const std::string semanticProductHeader =
      readTextFile(root / "include" / "primec" / "SemanticProduct.h");
  const std::string semanticProductSource =
      readTextFile(root / "src" / "SemanticProduct.cpp");
  const std::string semanticTargetAdapterSource =
      readTextFile(root / "src" / "ir_lowerer" / "IrLowererSemanticProductTargetAdapters.cpp");
  const std::string irCallResolutionSource =
      readTextFile(root / "src" / "ir_lowerer" / "IrLowererCallResolution.cpp");

  const std::size_t methodStart =
      semanticProductHeader.find("struct SemanticProgramMethodCallTarget {");
  const std::size_t bridgeStart =
      semanticProductHeader.find("struct SemanticProgramBridgePathChoice {");
  REQUIRE(methodStart != std::string::npos);
  REQUIRE(bridgeStart != std::string::npos);
  REQUIRE(methodStart < bridgeStart);

  const std::string methodBody =
      semanticProductHeader.substr(methodStart, bridgeStart - methodStart);
  CHECK(methodBody.find("SymbolId resolvedPathId = InvalidSymbolId;") !=
        std::string::npos);
  CHECK(methodBody.find("std::string resolvedPath;") == std::string::npos);

  CHECK(semanticProductSource.find("semanticProgramMethodCallTargetResolvedPath(") !=
        std::string::npos);
  CHECK(semanticProductSource.find("resolvedPath.empty() ? entry.resolvedPath") ==
        std::string::npos);
  CHECK(semanticTargetAdapterSource.find("entry->resolvedPathId == InvalidSymbolId") !=
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

TEST_CASE("lower inference return info uses direct semantic return facts") {
  const std::filesystem::path cwd = std::filesystem::current_path();
  const std::filesystem::path root =
      std::filesystem::exists(cwd / "include" / "primec" / "SemanticProduct.h")
          ? cwd
          : cwd.parent_path();
  const std::string irLowererReturnInfoHelpers =
      readTextFile(root / "src" / "ir_lowerer" / "IrLowererLowerInferenceReturnInfoHelpers.cpp");

  CHECK(irLowererReturnInfoHelpers.find("bool buildSemanticProductReturnInfo(const LowerInferenceReturnInfoSetupInput &input,\n"
                                        "                                    const SemanticProgram &semanticProgram,\n"
                                        "                                    const SemanticProductIndex &semanticIndex,") !=
        std::string::npos);
  CHECK(irLowererReturnInfoHelpers.find("const SemanticProductTargetAdapter &semanticProductTargets") ==
        std::string::npos);
  CHECK(irLowererReturnInfoHelpers.find("findSemanticProductCallableSummary(&semanticProgram, definition.fullPath)") !=
        std::string::npos);
  CHECK(irLowererReturnInfoHelpers.find("findSemanticProductReturnFact(&semanticProgram, semanticIndex, definition)") !=
        std::string::npos);
  CHECK(irLowererReturnInfoHelpers.find("*input.semanticProgram,") !=
        std::string::npos);
  CHECK(irLowererReturnInfoHelpers.find("*input.semanticIndex,") !=
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

TEST_CASE("semantic snapshot query projections reuse semantic-product fact caches") {
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

  const std::size_t queryFactStart =
      semanticsSnapshots.find("SemanticsValidator::queryFactSnapshotForSemanticProduct() {");
  const std::size_t tryFactStart =
      semanticsSnapshots.find("SemanticsValidator::tryFactSnapshotForSemanticProduct()");
  const std::size_t queryCacheHelperStart =
      semanticsSnapshotLocals.find("SemanticsValidator::ensureQuerySnapshotFactCaches() {");
  REQUIRE(queryFactStart != std::string::npos);
  REQUIRE(tryFactStart != std::string::npos);
  REQUIRE(queryCacheHelperStart != std::string::npos);
  REQUIRE(graphLocalAutoHeader.find("struct GraphLocalAutoKey {") != std::string::npos);
  REQUIRE(queryFactStart < tryFactStart);

  const std::string queryFactBody = semanticsSnapshots.substr(queryFactStart, tryFactStart - queryFactStart);
  CHECK(queryFactBody.find("ensureQuerySnapshotFactCaches();") != std::string::npos);
  CHECK(queryFactBody.find("return std::exchange(queryFactSnapshotCache_, {});") != std::string::npos);

  const std::string queryCacheHelperBody = semanticsSnapshotLocals.substr(queryCacheHelperStart);
  CHECK(queryCacheHelperBody.find("forEachInferredQuerySnapshot(") != std::string::npos);
  CHECK(queryCacheHelperBody.find("queryFactSnapshotCache_.push_back(") != std::string::npos);
  CHECK(queryCacheHelperBody.find("std::stable_sort(queryFactSnapshotCache_.begin(),") != std::string::npos);
  CHECK(queryCacheHelperBody.find("if (left.scopePath != right.scopePath)") != std::string::npos);
  CHECK(queryCacheHelperBody.find("if (left.sourceLine != right.sourceLine)") != std::string::npos);
  CHECK(queryCacheHelperBody.find("if (left.sourceColumn != right.sourceColumn)") != std::string::npos);
  CHECK(queryCacheHelperBody.find("if (left.callName != right.callName)") != std::string::npos);
  CHECK(queryCacheHelperBody.find("return left.resolvedPath < right.resolvedPath;") != std::string::npos);

  CHECK(semanticsHeader.find("queryCallTypeSnapshotForTesting") == std::string::npos);
  CHECK(semanticsHeader.find("queryBindingSnapshotForTesting") == std::string::npos);
  CHECK(semanticsHeader.find("queryResultTypeSnapshotForTesting") == std::string::npos);
  CHECK(semanticsHeader.find("queryReceiverBindingSnapshotForTesting") == std::string::npos);
  CHECK(semanticsHeader.find("struct QueryCallTypeSnapshotEntry {") == std::string::npos);
  CHECK(semanticsHeader.find("struct QueryBindingSnapshotEntry {") == std::string::npos);
  CHECK(semanticsHeader.find("struct QueryResultTypeSnapshotEntry {") == std::string::npos);
  CHECK(semanticsHeader.find("struct QueryReceiverBindingSnapshotEntry {") == std::string::npos);
  CHECK(semanticsValidate.find("validator.queryFactSnapshotForSemanticProduct()") != std::string::npos);
  CHECK(semanticsValidate.find("validator.queryCallTypeSnapshotForTesting()") == std::string::npos);
  CHECK(semanticsValidate.find("validator.queryBindingSnapshotForTesting()") == std::string::npos);
  CHECK(semanticsValidate.find("validator.queryResultTypeSnapshotForTesting()") == std::string::npos);
  CHECK(semanticsValidate.find("validator.queryReceiverBindingSnapshotForTesting()") == std::string::npos);
  CHECK(semanticsValidate.find("if (entry.typeText.empty()) {") != std::string::npos);
  CHECK(semanticsValidate.find("if (entry.binding.typeName.empty()) {") != std::string::npos);
  CHECK(semanticsValidate.find("if (!entry.hasResultType) {") != std::string::npos);
  CHECK(semanticsValidate.find("if (entry.receiverBinding.typeName.empty()) {") != std::string::npos);
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
  REQUIRE(collectHelperStart != std::string::npos);
  REQUIRE(callableSummaryStart != std::string::npos);
  REQUIRE(ensureOnErrorStart != std::string::npos);
  REQUIRE(typeMetadataStart != std::string::npos);
  REQUIRE(onErrorStart != std::string::npos);
  REQUIRE(collectHelperStart < callableSummaryStart);
  REQUIRE(callableSummaryStart < typeMetadataStart);
  REQUIRE(ensureOnErrorStart < typeMetadataStart);

  CHECK(semanticsHeader.find("bool mergedWorkerCallableSummariesValid_ = false;") !=
        std::string::npos);
  CHECK(semanticsHeader.find("bool mergedWorkerOnErrorFactsValid_ = false;") !=
        std::string::npos);
  CHECK(semanticsHeader.find("bool mergedWorkerPublicationSeedStringsValid_ = false;") !=
        std::string::npos);
  CHECK(semanticsHeader.find("std::vector<CollectedCallableSummaryEntry> mergedWorkerCallableSummaries_;") !=
        std::string::npos);
  CHECK(semanticsHeader.find("std::vector<OnErrorSnapshotEntry> mergedWorkerOnErrorFacts_;") !=
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
  CHECK(semanticsSnapshots.find("SemanticsValidator::sortCollectedOnErrorSnapshots(") !=
        std::string::npos);

  const std::string collectHelperBody =
      semanticsSnapshots.substr(collectHelperStart, callableSummaryStart - collectHelperStart);
  CHECK(collectHelperBody.find("if (mergedWorkerCallableSummariesValid_) {") != std::string::npos);
  CHECK(collectHelperBody.find("collectedCallableSummaries_ = mergedWorkerCallableSummaries_;") !=
        std::string::npos);
  CHECK(collectHelperBody.find("rebindCollectedCallableSummarySemanticNodeIds(collectedCallableSummaries_);") !=
        std::string::npos);
  CHECK(collectHelperBody.find("collectCallableSummaryEntriesForStableRange(") !=
        std::string::npos);
  CHECK(collectHelperBody.find("collectExecutionCallableSummaryEntries(collectedCallableSummaries_);") !=
        std::string::npos);
  CHECK(collectHelperBody.find("sortCollectedCallableSummaries(collectedCallableSummaries_);") !=
        std::string::npos);
  CHECK(collectHelperBody.find("collectedCallableSummaries_.push_back(") != std::string::npos);
  CHECK(semanticsSnapshots.find("if (left.fullPath != right.fullPath)") != std::string::npos);
  CHECK(semanticsSnapshots.find("return left.isExecution < right.isExecution;") !=
        std::string::npos);
  CHECK(semanticsDefinitionPasses.find("std::vector<CollectedCallableSummaryEntry> callableSummaries;") !=
        std::string::npos);
  CHECK(semanticsDefinitionPasses.find("worker.collectCallableSummaryEntriesForStableRange(") !=
        std::string::npos);
  CHECK(semanticsDefinitionPasses.find("mergeWorkerCallableSummaries") != std::string::npos);
  CHECK(semanticsDefinitionPasses.find("collectExecutionCallableSummaryEntries(mergedWorkerCallableSummaries_);") !=
        std::string::npos);
  CHECK(semanticsDefinitionPasses.find("sortCollectedCallableSummaries(mergedWorkerCallableSummaries_);") !=
        std::string::npos);
  CHECK(semanticsDefinitionPasses.find("mergedWorkerCallableSummariesValid_ = true;") !=
        std::string::npos);
  CHECK(semanticsDefinitionPasses.find("std::vector<OnErrorSnapshotEntry> onErrorFacts;") !=
        std::string::npos);
  CHECK(semanticsDefinitionPasses.find("WorkerSymbolInternerSnapshot publicationStringSnapshot;") !=
        std::string::npos);
  CHECK(semanticsDefinitionPasses.find("appendSemanticPublicationStringOrigins(") !=
        std::string::npos);
  CHECK(semanticsDefinitionPasses.find("worker.collectOnErrorSnapshotEntriesForStableRange(") !=
        std::string::npos);
  CHECK(semanticsDefinitionPasses.find("mergeWorkerOnErrorFacts") != std::string::npos);
  CHECK(semanticsDefinitionPasses.find("sortCollectedOnErrorSnapshots(mergedWorkerOnErrorFacts_);") !=
        std::string::npos);
  CHECK(semanticsDefinitionPasses.find("mergedWorkerOnErrorFactsValid_ = true;") !=
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
  CHECK(ensureOnErrorBody.find("if (mergedWorkerOnErrorFactsValid_) {") != std::string::npos);
  CHECK(ensureOnErrorBody.find("onErrorSnapshotCache_ = mergedWorkerOnErrorFacts_;") !=
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
