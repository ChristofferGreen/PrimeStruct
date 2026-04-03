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
  std::filesystem::path testApiPath = cwd / "include" / "primec" / "testing" / "SemanticsValidationHelpers.h";
  std::filesystem::path graphHeaderPath = cwd / "src" / "semantics" / "TypeResolutionGraph.h";
  std::filesystem::path graphSourcePath = cwd / "src" / "semantics" / "TypeResolutionGraph.cpp";
  std::filesystem::path pipelinePath = cwd / "src" / "CompilePipeline.cpp";
  std::filesystem::path primecMainPath = cwd / "src" / "main.cpp";
  std::filesystem::path primevmMainPath = cwd / "src" / "primevm_main.cpp";
  if (!std::filesystem::exists(cmakePath)) {
    cmakePath = cwd.parent_path() / "CMakeLists.txt";
    testApiPath = cwd.parent_path() / "include" / "primec" / "testing" / "SemanticsValidationHelpers.h";
    graphHeaderPath = cwd.parent_path() / "src" / "semantics" / "TypeResolutionGraph.h";
    graphSourcePath = cwd.parent_path() / "src" / "semantics" / "TypeResolutionGraph.cpp";
    pipelinePath = cwd.parent_path() / "src" / "CompilePipeline.cpp";
    primecMainPath = cwd.parent_path() / "src" / "main.cpp";
    primevmMainPath = cwd.parent_path() / "src" / "primevm_main.cpp";
  }
  REQUIRE(std::filesystem::exists(cmakePath));
  REQUIRE(std::filesystem::exists(testApiPath));
  REQUIRE(std::filesystem::exists(graphHeaderPath));
  REQUIRE(std::filesystem::exists(graphSourcePath));
  REQUIRE(std::filesystem::exists(pipelinePath));
  REQUIRE(std::filesystem::exists(primecMainPath));
  REQUIRE(std::filesystem::exists(primevmMainPath));

  const std::string cmake = readTextFile(cmakePath);
  const std::string testApi = readTextFile(testApiPath);
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
  CHECK(pipeline.find("dumpStage == \"type_graph\" || dumpStage == \"type-graph\"") != std::string::npos);
  CHECK(pipeline.find("semantics::buildTypeResolutionGraphForProgram(") != std::string::npos);
  CHECK(pipeline.find("output.dumpOutput = semantics::formatTypeResolutionGraph(graph);") != std::string::npos);
  CHECK(primecMain.find("[--dump-stage pre_ast|ast|ast-semantic|type-graph|ir]") != std::string::npos);
  CHECK(primevmMain.find("[--dump-stage pre_ast|ast|ast-semantic|type-graph|ir]") != std::string::npos);
}
