TEST_CASE("semantics validator rebuilds base contexts behind explicit validation state") {
  const std::filesystem::path cwd = std::filesystem::current_path();
  std::filesystem::path headerPath = cwd / "src" / "semantics" / "SemanticsValidator.h";
  std::filesystem::path sourcePath = cwd / "src" / "semantics" / "SemanticsValidator.cpp";
  std::filesystem::path buildPath = cwd / "src" / "semantics" / "SemanticsValidatorBuild.cpp";
  std::filesystem::path validationContextPath =
      cwd / "src" / "semantics" / "SemanticsValidatorValidationContext.cpp";
  if (!std::filesystem::exists(headerPath)) {
    headerPath = cwd.parent_path() / "src" / "semantics" / "SemanticsValidator.h";
    sourcePath = cwd.parent_path() / "src" / "semantics" / "SemanticsValidator.cpp";
    buildPath = cwd.parent_path() / "src" / "semantics" / "SemanticsValidatorBuild.cpp";
    validationContextPath =
        cwd.parent_path() / "src" / "semantics" / "SemanticsValidatorValidationContext.cpp";
  }
  REQUIRE(std::filesystem::exists(headerPath));
  REQUIRE(std::filesystem::exists(sourcePath));
  REQUIRE(std::filesystem::exists(buildPath));
  REQUIRE(std::filesystem::exists(validationContextPath));

  const std::string header = readTextFile(headerPath);
  const std::string source = readTextFile(sourcePath);
  const std::string build = readTextFile(buildPath);
  const std::string validationContext = readTextFile(validationContextPath);
  CHECK(header.find("std::unordered_map<std::string, ValidationContext> definitionValidationContexts_") ==
        std::string::npos);
  CHECK(header.find("std::unordered_map<std::string, ValidationContext> executionValidationContexts_") ==
        std::string::npos);
  CHECK(header.find("struct ValidationState {") != std::string::npos);
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
  CHECK(validationApi.find("struct TypeResolutionGraphSnapshotNode") == std::string::npos);
  CHECK(validationApi.find("buildTypeResolutionGraphForTesting") == std::string::npos);
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
  CHECK(lowerInference.find("const SemanticProductTargetAdapter *semanticProductTargets = nullptr;") !=
        std::string::npos);
  CHECK(lowerLocals.find("const SemanticProgram *semanticProgram,") != std::string::npos);
  CHECK(resultHelpers.find("using InferExprKindWithLocalsFn") != std::string::npos);
  CHECK(resultHelpers.find("struct PackedResultStructPayloadInfo") != std::string::npos);
  CHECK(resultHelpers.find("inferPackedResultStructType(") != std::string::npos);
  CHECK(resultHelpers.find("resolvePackedResultStructPayloadInfo(") != std::string::npos);
  CHECK(setupType.find("const SemanticProductTargetAdapter *semanticProductTargets,") !=
        std::string::npos);
  CHECK(setupType.find("resolveMethodCallDefinitionFromExpr(") != std::string::npos);
}

TEST_CASE("public lowerer testing umbrella keeps alias owners ahead of users") {
  const std::filesystem::path cwd = std::filesystem::current_path();
  std::filesystem::path irLowererHelpersHeaderPath = cwd / "include" / "primec" / "testing" / "IrLowererHelpers.h";
  if (!std::filesystem::exists(irLowererHelpersHeaderPath)) {
    irLowererHelpersHeaderPath =
        cwd.parent_path() / "include" / "primec" / "testing" / "IrLowererHelpers.h";
  }
  REQUIRE(std::filesystem::exists(irLowererHelpersHeaderPath));

  const std::string irLowererHelpersHeader = readTextFile(irLowererHelpersHeaderPath);

  const auto setupTypePos = irLowererHelpersHeader.find("IrLowererSetupTypeHelpers.h");
  const auto callDispatchPos = irLowererHelpersHeader.find("IrLowererCallDispatchHelpers.h");
  const auto statementBindingPos = irLowererHelpersHeader.find("IrLowererStatementBindingHelpers.h");
  const auto countAccessPos = irLowererHelpersHeader.find("IrLowererCountAccessHelpers.h");
  const auto resultHelpersPos = irLowererHelpersHeader.find("IrLowererResultHelpers.h");
  const auto stringCallPos = irLowererHelpersHeader.find("IrLowererStringCallHelpers.h");
  const auto operatorArithmeticPos = irLowererHelpersHeader.find("IrLowererOperatorArithmeticHelpers.h");
  const auto lowerExprEmitSetupPos = irLowererHelpersHeader.find("IrLowererLowerExprEmitSetup.h");
  const auto lowerInferencePos = irLowererHelpersHeader.find("IrLowererLowerInferenceSetup.h");

  REQUIRE(setupTypePos != std::string::npos);
  REQUIRE(callDispatchPos != std::string::npos);
  REQUIRE(statementBindingPos != std::string::npos);
  REQUIRE(countAccessPos != std::string::npos);
  REQUIRE(resultHelpersPos != std::string::npos);
  REQUIRE(stringCallPos != std::string::npos);
  REQUIRE(operatorArithmeticPos != std::string::npos);
  REQUIRE(lowerExprEmitSetupPos != std::string::npos);
  REQUIRE(lowerInferencePos != std::string::npos);

  CHECK(setupTypePos < callDispatchPos);
  CHECK(statementBindingPos < countAccessPos);
  CHECK(statementBindingPos < lowerInferencePos);
  CHECK(resultHelpersPos < operatorArithmeticPos);
  CHECK(stringCallPos < operatorArithmeticPos);
  CHECK(operatorArithmeticPos < lowerExprEmitSetupPos);
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
  CHECK(callDispatch.find("resolveCallPathFromScopeWithoutImportAliases(") != std::string::npos);
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
  CHECK(snapshot.find("REQUIRE(lowerer.lower(semanticAst, &semanticProgram, \"/main\", defaults, defaults, semanticModule, error));") !=
        std::string::npos);
  CHECK(snapshot.find("fallbackModule") == std::string::npos);
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
  std::filesystem::path irInferenceSetupPath =
      cwd / "src" / "ir_lowerer" / "IrLowererLowerSetupInference.h";
  std::filesystem::path irLowerEffectsPath = cwd / "src" / "ir_lowerer" / "IrLowererLowerEffects.cpp";
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
    irInferenceSetupPath = cwd.parent_path() / "src" / "ir_lowerer" / "IrLowererLowerSetupInference.h";
    irLowerEffectsPath = cwd.parent_path() / "src" / "ir_lowerer" / "IrLowererLowerEffects.cpp";
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
  REQUIRE(std::filesystem::exists(irInferenceSetupPath));
  REQUIRE(std::filesystem::exists(irLowerEffectsPath));
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
  const std::string irInferenceSetup = readTextFile(irInferenceSetupPath);
  const std::string irLowerEffects = readTextFile(irLowerEffectsPath);
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
  CHECK(semanticProduct.find("struct SemanticProgramCallableSummary") != std::string::npos);
  CHECK(semanticProduct.find("struct SemanticProgramTypeMetadata") != std::string::npos);
  CHECK(semanticProduct.find("struct SemanticProgramStructFieldMetadata") != std::string::npos);
  CHECK(semanticProduct.find("struct SemanticProgramBindingFact") != std::string::npos);
  CHECK(semanticProduct.find("struct SemanticProgramReturnFact") != std::string::npos);
  CHECK(semanticProduct.find("struct SemanticProgramLocalAutoFact") != std::string::npos);
  CHECK(semanticProduct.find("struct SemanticProgramQueryFact") != std::string::npos);
  CHECK(semanticProduct.find("struct SemanticProgramTryFact") != std::string::npos);
  CHECK(semanticProduct.find("struct SemanticProgramOnErrorFact") != std::string::npos);
  CHECK(semanticProduct.find("struct SemanticProgramDefinition") != std::string::npos);
  CHECK(semanticProduct.find("struct SemanticProgramExecution") != std::string::npos);
  CHECK(semanticProduct.find("struct SemanticProgram") != std::string::npos);
  CHECK(semanticProduct.find("std::string formatSemanticProgram(const SemanticProgram &semanticProgram);") !=
        std::string::npos);
  CHECK(semanticProduct.find("std::vector<SemanticProgramDirectCallTarget> directCallTargets;") !=
        std::string::npos);
  CHECK(semanticProduct.find("std::vector<SemanticProgramMethodCallTarget> methodCallTargets;") !=
        std::string::npos);
  CHECK(semanticProduct.find("std::vector<SemanticProgramBridgePathChoice> bridgePathChoices;") !=
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
  CHECK(irCallHelpers.find("SemanticProductTargetAdapter semanticProductTargets{};") != std::string::npos);
  CHECK(irCallResolution.find("buildSemanticProductTargetAdapter(semanticProgram)") != std::string::npos);
  CHECK(irCallResolution.find("findSemanticProductDirectCallTarget(semanticProductTargets, expr)") !=
        std::string::npos);
  CHECK(irCallResolution.find("findSemanticProductBridgePathChoice(semanticProductTargets, expr)") !=
        std::string::npos);
  CHECK(irCallResolution.find("resolveCallPathFromScopeWithoutImportAliases(expr, defMap)") !=
        std::string::npos);
  CHECK(irCallResolution.find("return resolveCallPathFromScope(expr, defMap, importAliases);") ==
        std::string::npos);
  CHECK(irMethodResolution.find("findSemanticProductMethodCallTarget(*semanticProductTargets, callExpr)") !=
        std::string::npos);
  CHECK(irMethodResolution.find("const auto &semanticAwareImportAliases =") != std::string::npos);
  CHECK(irMethodResolution.find("resolveMethodReceiverTarget(*receiver,") != std::string::npos);
  CHECK(irMethodResolution.find("semanticAwareImportAliases,") != std::string::npos);
  CHECK(irMethodResolution.find("resolveMethodCallDefinitionFromExpr(*receiver,") != std::string::npos);
  CHECK(irMethodResolution.find("resolveMethodCallDefinitionFromExpr(*receiver,\n"
                                "                                                        localsIn,\n"
                                "                                                        isArrayCountCall,\n"
                                "                                                        isVectorCapacityCall,\n"
                                "                                                        isEntryArgsName,\n"
                                "                                                        importAliases,") ==
        std::string::npos);
  CHECK(irInferenceSetup.find(".semanticProductTargets = &callResolutionAdapters.semanticProductTargets,") !=
        std::string::npos);
  CHECK(semanticTargetAdapterHeader.find("struct SemanticProductTargetAdapter") != std::string::npos);
  CHECK(semanticTargetAdapterHeader.find("bool hasSemanticProduct = false;") != std::string::npos);
  CHECK(semanticTargetAdapterHeader.find("const SemanticProgramCallableSummary *findSemanticProductCallableSummary(") !=
        std::string::npos);
  CHECK(semanticTargetAdapterHeader.find("std::unordered_map<std::string, const SemanticProgramOnErrorFact *> onErrorFactsByDefinitionPath;") !=
        std::string::npos);
  CHECK(semanticTargetAdapterHeader.find("const SemanticProgramOnErrorFact *findSemanticProductOnErrorFact(") !=
        std::string::npos);
  CHECK(semanticTargetAdapterHeader.find("std::unordered_map<std::string, const SemanticProgramTypeMetadata *> typeMetadataByPath;") !=
        std::string::npos);
  CHECK(semanticTargetAdapterHeader.find("std::vector<const SemanticProgramTypeMetadata *> orderedStructTypeMetadata;") !=
        std::string::npos);
  CHECK(semanticTargetAdapterHeader.find("std::unordered_map<std::string, std::vector<const SemanticProgramStructFieldMetadata *>>") !=
        std::string::npos);
  CHECK(semanticTargetAdapterHeader.find("const SemanticProgramTypeMetadata *findSemanticProductTypeMetadata(") !=
        std::string::npos);
  CHECK(semanticTargetAdapterHeader.find("findSemanticProductStructFieldMetadata(") !=
        std::string::npos);
  CHECK(semanticTargetAdapterHeader.find("const SemanticProgramReturnFact *findSemanticProductReturnFact(") !=
        std::string::npos);
  CHECK(semanticTargetAdapterHeader.find("std::unordered_map<uint64_t, const SemanticProgramBindingFact *> bindingFactsByExpr;") !=
        std::string::npos);
  CHECK(semanticTargetAdapterHeader.find("const SemanticProgramBindingFact *findSemanticProductBindingFact(") !=
        std::string::npos);
  CHECK(semanticTargetAdapterSource.find("buildSemanticProductTargetAdapter(const SemanticProgram *semanticProgram)") !=
        std::string::npos);
  CHECK(semanticTargetAdapterSource.find("adapter.hasSemanticProduct = true;") != std::string::npos);
  CHECK(semanticTargetAdapterSource.find("adapter.callableSummariesByPath.reserve(") != std::string::npos);
  CHECK(semanticTargetAdapterSource.find("adapter.onErrorFactsByDefinitionPath.reserve(semanticProgram->onErrorFacts.size())") !=
        std::string::npos);
  CHECK(semanticTargetAdapterSource.find("adapter.typeMetadataByPath.reserve(semanticProgram->typeMetadata.size())") !=
        std::string::npos);
  CHECK(semanticTargetAdapterSource.find("adapter.orderedStructTypeMetadata.reserve(semanticProgram->typeMetadata.size())") !=
        std::string::npos);
  CHECK(semanticTargetAdapterSource.find("adapter.orderedStructTypeMetadata.push_back(&entry);") !=
        std::string::npos);
  CHECK(semanticTargetAdapterSource.find("adapter.structFieldMetadataByStructPath.reserve(semanticProgram->structFieldMetadata.size())") !=
        std::string::npos);
  CHECK(semanticTargetAdapterSource.find("std::stable_sort(entries.begin(),") != std::string::npos);
  CHECK(semanticTargetAdapterSource.find("adapter.returnFactsByDefinitionPath.reserve(") != std::string::npos);
  CHECK(semanticTargetAdapterSource.find("adapter.bindingFactsByExpr.reserve(semanticProgram->bindingFacts.size())") !=
        std::string::npos);
  CHECK(semanticTargetAdapterSource.find("expr.semanticNodeId") != std::string::npos);
  CHECK(semanticTargetAdapterSource.find("insert_or_assign(entry.semanticNodeId, &entry)") != std::string::npos);
  CHECK(semanticTargetAdapterSource.find("const SemanticProgramTypeMetadata *findSemanticProductTypeMetadata(") !=
        std::string::npos);
  CHECK(semanticTargetAdapterSource.find("findSemanticProductStructFieldMetadata(") !=
        std::string::npos);
  CHECK(lowerImportsStructsSetupHeader.find("const SemanticProgram *semanticProgram,") != std::string::npos);
  CHECK(lowerImportsStructsSetupSource.find("buildSemanticProductTargetAdapter(semanticProgram)") !=
        std::string::npos);
  CHECK(lowerImportsStructsSetupSource.find("buildDefinitionMapAndStructNames(program.definitions, defMapOut, structNamesOut, &semanticProductTargets);") !=
        std::string::npos);
  CHECK(lowerImportsStructsSetupSource.find("program, defMapOut, &semanticProductTargets, computeStructLayout, outModule.structLayouts, errorOut") !=
        std::string::npos);
  CHECK(lowerImportsStructsSetupSource.find("&semanticProductTargets,") != std::string::npos);
  CHECK(lowerSetupImportsStructs.find("program, semanticProgram, out, defMap, structNames, importAliases, structFieldInfoByName, error") !=
        std::string::npos);
  CHECK(structTypeHelpersHeader.find("const SemanticProductTargetAdapter *semanticProductTargets = nullptr") !=
        std::string::npos);
  CHECK(structTypeHelpersSource.find("if (semanticProductTargets != nullptr && !semanticProductTargets->typeMetadataByPath.empty()) {") !=
        std::string::npos);
  CHECK(structTypeHelpersSource.find("isStructLikeSemanticProductCategory(typeMetadata->category)") !=
        std::string::npos);
  CHECK(structFieldBindingHelpersHeader.find("const SemanticProductTargetAdapter *semanticProductTargets,") !=
        std::string::npos);
  CHECK(structFieldBindingHelpersSource.find("if (!isStructDefinition(def, semanticProductTargets)) {") !=
        std::string::npos);
  CHECK(structFieldBindingHelpersSource.find("layoutFieldBindingFromSemanticProduct(") !=
        std::string::npos);
  CHECK(structFieldBindingHelpersSource.find("findSemanticProductStructFieldMetadata(*semanticProductTargets, def.fullPath)") !=
        std::string::npos);
  CHECK(structFieldBindingHelpersHeader.find("bool resolveStructLayoutFieldBinding(") !=
        std::string::npos);
  CHECK(structLayoutHelpersHeader.find("const SemanticProgramTypeMetadata *typeMetadata,") !=
        std::string::npos);
  CHECK(structLayoutHelpersHeader.find("const std::unordered_map<std::string, const Definition *> &defMap,") !=
        std::string::npos);
  CHECK(structLayoutHelpersHeader.find("const SemanticProductTargetAdapter *semanticProductTargets,") !=
        std::string::npos);
  CHECK(structLayoutHelpersSource.find("typeMetadata != nullptr && typeMetadata->hasExplicitAlignment") !=
        std::string::npos);
  CHECK(structLayoutHelpersSource.find("findSemanticProductTypeMetadata(*semanticProductTargets, def.fullPath)") !=
        std::string::npos);
  CHECK(structLayoutHelpersSource.find("if (semanticProductTargets != nullptr && !semanticProductTargets->orderedStructTypeMetadata.empty()) {") !=
        std::string::npos);
  CHECK(structLayoutHelpersSource.find("const auto defIt = defMap.find(typeMetadata->fullPath);") !=
        std::string::npos);
  CHECK(structLayoutHelpersSource.find("if (!isStructDefinition(def, semanticProductTargets)) {") !=
        std::string::npos);
  CHECK(callAccessHelpersHeader.find("struct SemanticProductTargetAdapter;") !=
        std::string::npos);
  CHECK(callAccessHelpersHeader.find("struct Definition;") != std::string::npos);
  CHECK(callAccessHelpersHeader.find("bool emitMapLookupContains(") != std::string::npos);
  CHECK(callAccessHelpersHeader.find("bool isStructDefinition(const Definition &def, const SemanticProductTargetAdapter *semanticProductTargets);") !=
        std::string::npos);
  CHECK(bindingTypeHelpersHeader.find("BindingTypeAdapters makeBindingTypeAdapters(const SemanticProgram *semanticProgram = nullptr);") !=
        std::string::npos);
  CHECK(bindingTypeHelpersSource.find("findSemanticProductBindingFact(semanticProductTargets, expr)") !=
        std::string::npos);
  CHECK(statementBindingHelpersHeader.find("const ResolveDefinitionCallForStatementFn &resolveDefinitionCall);") !=
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

  const auto lowerEntrySetupHeader =
      readFile("include/primec/testing/ir_lowerer_helpers/IrLowererLowerEntrySetup.h");
  CHECK(lowerEntrySetupHeader.find("struct Definition;") != std::string::npos);
  CHECK(lowerEntrySetupHeader.find("struct Program;") != std::string::npos);
  CHECK(lowerEntrySetupHeader.find("struct SemanticProgram;") != std::string::npos);

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

  const auto resultHelpersHeader =
      readFile("include/primec/testing/ir_lowerer_helpers/IrLowererResultHelpers.h");
  CHECK(resultHelpersHeader.find("struct Definition;") != std::string::npos);
  CHECK(resultHelpersHeader.find("struct ReturnInfo;") != std::string::npos);
  CHECK(resultHelpersHeader.find("struct StructSlotLayoutInfo;") != std::string::npos);

  const auto runtimeErrorHelpersHeader =
      readFile("include/primec/testing/ir_lowerer_helpers/IrLowererRuntimeErrorHelpers.h");
  CHECK(runtimeErrorHelpersHeader.find("struct IrFunction;") != std::string::npos);

  const auto statementBindingHelpersHeader =
      readFile("include/primec/testing/ir_lowerer_helpers/IrLowererStatementBindingHelpers.h");
  CHECK(statementBindingHelpersHeader.find("struct Definition;") != std::string::npos);
  CHECK(statementBindingHelpersHeader.find("struct ResultReturnInfo;") != std::string::npos);
  CHECK(statementBindingHelpersHeader.find("struct ReturnInfo;") != std::string::npos);

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
  CHECK(countAccessHelpersSource.find("entry.scopePath != entryDef.fullPath || entry.siteKind != \"parameter\"") !=
        std::string::npos);
  CHECK(countAccessHelpersSource.find("entryParamFact->bindingTypeText != \"array<string>\"") !=
        std::string::npos);
  CHECK(onErrorHelpersSource.find("buildEntryCountAccessSetup(entryDef, semanticProgram, out.countAccessSetup, error)") !=
        std::string::npos);
  CHECK(onErrorHelpersSource.find("findSemanticProductOnErrorFact(semanticProductTargets, def.fullPath)") !=
        std::string::npos);
  CHECK(onErrorHelpersSource.find("missing semantic-product on_error fact: ") !=
        std::string::npos);
  CHECK(onErrorHelpersSource.find("buildOnErrorByDefinition(program, semanticProgram,") !=
        std::string::npos);
  CHECK(statementCallHelpersHeader.find("const SemanticProgram *semanticProgram,") !=
        std::string::npos);
  CHECK(statementCallHelpersHeader.find("FunctionTableFinalizationResult finalizeEntryFunctionTableAndLowerCallables(\n"
                                        "    const Program &program,\n"
                                        "    const Definition &entryDef,\n"
                                        "    const SemanticProgram *semanticProgram,") !=
        std::string::npos);
  CHECK(statementCallHelpersSource.find("buildSemanticProductTargetAdapter(semanticProgram)") !=
        std::string::npos);
  CHECK(statementCallHelpersSource.find("const auto definitionsByPath = buildDefinitionBodyLookup(program);") !=
        std::string::npos);
  CHECK(statementCallHelpersSource.find("if (semanticProgram != nullptr && !semanticProgram->definitions.empty()) {") !=
        std::string::npos);
  CHECK(statementCallHelpersSource.find("semantic product definition missing AST body: ") !=
        std::string::npos);
  CHECK(statementCallHelpersSource.find("findSemanticProductCallableSummary(semanticProductTargets, def.fullPath)") !=
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
  CHECK(irLowerEffects.find("findSemanticProductCallableSummary(semanticProductTargets, entryPath)") !=
        std::string::npos);
  CHECK(irLowerEffects.find("missing semantic-product callable summary: ") !=
        std::string::npos);
  CHECK(irReturnInference.find("findSemanticProductCallableSummary(semanticProductTargets, entryPath)") !=
        std::string::npos);
  CHECK(irReturnInference.find("findSemanticProductReturnFact(semanticProductTargets, entryPath)") !=
        std::string::npos);
  CHECK(irReturnInference.find("missing semantic-product return fact: ") !=
        std::string::npos);
  CHECK(irLowerEffects.find("return validateProgramEffects(program, nullptr, entryPath, defaultEffects, entryDefaultEffects, error);") !=
        std::string::npos);
  CHECK(irLowerEffects.find("findSemanticProductCallableSummary(*semanticProductTargetsPtr, fullPath)") !=
        std::string::npos);
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
  CHECK(semanticsValidate.find("validator.directCallTargetSnapshotForSemanticProduct()") != std::string::npos);
  CHECK(semanticsValidate.find("validator.methodCallTargetSnapshotForSemanticProduct()") != std::string::npos);
  CHECK(semanticsValidate.find("validator.bridgePathChoiceSnapshotForSemanticProduct()") != std::string::npos);
  CHECK(semanticsValidate.find("validator.callableSummarySnapshotForSemanticProduct()") != std::string::npos);
  CHECK(semanticsValidate.find("validator.typeMetadataSnapshotForSemanticProduct()") != std::string::npos);
  CHECK(semanticsValidate.find("validator.structFieldMetadataSnapshotForSemanticProduct()") !=
        std::string::npos);
  CHECK(semanticsValidate.find("validator.bindingFactSnapshotForSemanticProduct()") != std::string::npos);
  CHECK(semanticsValidate.find("validator.returnFactSnapshotForSemanticProduct()") != std::string::npos);
  CHECK(semanticsValidate.find("validator.localAutoFactSnapshotForSemanticProduct()") != std::string::npos);
  CHECK(semanticsValidate.find("validator.queryFactSnapshotForSemanticProduct()") != std::string::npos);
  CHECK(semanticsValidate.find("validator.tryFactSnapshotForSemanticProduct()") != std::string::npos);
  CHECK(semanticsValidate.find("validator.onErrorFactSnapshotForSemanticProduct()") != std::string::npos);
  CHECK(semanticsValidate.find("*semanticProgramOut = buildSemanticProgram(program, entryPath);") ==
        std::string::npos);
  CHECK(semanticsValidate.find("*semanticProgramOut = buildSemanticProgram(program, entryPath, validator);") !=
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

  const std::string semanticProduct = readRepoFile("include/primec/SemanticProduct.h");
  const std::string compilePipelineHeader = readRepoFile("include/primec/CompilePipeline.h");
  const std::string semanticsHeader = readRepoFile("include/primec/Semantics.h");
  const std::string irPreparationHeader = readRepoFile("include/primec/IrPreparation.h");
  const std::string irLowererHeader = readRepoFile("include/primec/IrLowerer.h");
  const std::string compilePipelineSource = readRepoFile("src/CompilePipeline.cpp");
  const std::string irPreparationSource = readRepoFile("src/IrPreparation.cpp");
  const std::string irLowererEntrySetup =
      readRepoFile("src/ir_lowerer/IrLowererLowerSetupEntryEffects.h");
  const std::string semanticsValidate = readRepoFile("src/semantics/SemanticsValidate.cpp");
  const std::string semanticTargetAdapterHeader =
      readRepoFile("src/ir_lowerer/IrLowererSemanticProductTargetAdapters.h");
  const std::string semanticTargetAdapterSource =
      readRepoFile("src/ir_lowerer/IrLowererSemanticProductTargetAdapters.cpp");
  const std::string irCallResolution = readRepoFile("src/ir_lowerer/IrLowererCallResolution.cpp");
  const std::string irMethodResolution =
      readRepoFile("src/ir_lowerer/IrLowererSetupTypeMethodCallResolution.cpp");
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
  CHECK(semanticProduct.find("struct SemanticProgramStructFieldMetadata") != std::string::npos);
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

  CHECK(compilePipelineSource.find("output.semanticProgram = std::move(semanticProgram);") !=
        std::string::npos);
  CHECK(compilePipelineSource.find("output.hasSemanticProgram = true;") != std::string::npos);
  CHECK(compilePipelineSource.find("output.failure.stage = stage;") != std::string::npos);
  CHECK(compilePipelineSource.find("output.failure.message = message;") != std::string::npos);
  CHECK(semanticsValidate.find("*semanticProgramOut = buildSemanticProgram(program, entryPath, validator);") !=
        std::string::npos);

  CHECK(semanticTargetAdapterHeader.find("struct SemanticProductTargetAdapter") != std::string::npos);
  CHECK(semanticTargetAdapterHeader.find("bool hasSemanticProduct = false;") != std::string::npos);
  CHECK(semanticTargetAdapterHeader.find("findSemanticProductOnErrorFact(") != std::string::npos);
  CHECK(semanticTargetAdapterHeader.find("findSemanticProductStructFieldMetadata(") !=
        std::string::npos);
  CHECK(semanticTargetAdapterSource.find("buildSemanticProductTargetAdapter(const SemanticProgram *semanticProgram)") !=
        std::string::npos);
  CHECK(semanticTargetAdapterSource.find("adapter.hasSemanticProduct = true;") != std::string::npos);

  CHECK(irCallResolution.find("resolveCallPathFromScopeWithoutImportAliases(expr, defMap)") !=
        std::string::npos);
  CHECK(irMethodResolution.find("const auto &semanticAwareImportAliases =") != std::string::npos);
  CHECK(statementCallHelpersHeader.find("const SemanticProgram *semanticProgram,") !=
        std::string::npos);
  CHECK(functionTableStepHeader.find("const SemanticProgram *semanticProgram = nullptr;") !=
        std::string::npos);
  CHECK(callAccessHelpersHeader.find("bool emitMapLookupContains(") != std::string::npos);
  CHECK(callAccessHelpersHeader.find("bool emitBuiltinCanonicalMapInsertOverwriteOrPending(") !=
        std::string::npos);
  CHECK(structLayoutHelpersHeader.find("const SemanticProductTargetAdapter *semanticProductTargets,") !=
        std::string::npos);

  CHECK(primecMain.find("describeCompilePipelineFailure(pipelineOutput)") != std::string::npos);
  CHECK(primevmMain.find("describeCompilePipelineFailure(pipelineOutput)") != std::string::npos);
}
