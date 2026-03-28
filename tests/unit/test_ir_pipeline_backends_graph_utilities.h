TEST_CASE("type resolver parity harness is wired through ir pipeline tests") {
  const std::filesystem::path cwd = std::filesystem::current_path();
  std::filesystem::path irPipelinePath = cwd / "tests" / "unit" / "test_ir_pipeline.cpp";
  std::filesystem::path parityHeaderPath = cwd / "tests" / "unit" / "test_ir_pipeline_type_resolution_parity.h";
  if (!std::filesystem::exists(irPipelinePath)) {
    irPipelinePath = cwd.parent_path() / "tests" / "unit" / "test_ir_pipeline.cpp";
    parityHeaderPath = cwd.parent_path() / "tests" / "unit" / "test_ir_pipeline_type_resolution_parity.h";
  }
  REQUIRE(std::filesystem::exists(irPipelinePath));
  REQUIRE(std::filesystem::exists(parityHeaderPath));

  const std::string irPipeline = readTextFile(irPipelinePath);
  const std::string parityHeader = readTextFile(parityHeaderPath);
  CHECK(irPipeline.find("TypeResolverPipelineSnapshot") != std::string::npos);
  CHECK(irPipeline.find("runTypeResolverPipelineSnapshot") != std::string::npos);
  CHECK(irPipeline.find("#include \"test_ir_pipeline_type_resolution_parity.h\"") != std::string::npos);
  CHECK(parityHeader.find("default type resolver keeps vm pipeline behavior stable across graph corpus") !=
        std::string::npos);
  CHECK(parityHeader.find("direct_call_local_auto_struct") != std::string::npos);
  CHECK(parityHeader.find("direct_call_local_auto_collection") != std::string::npos);
  CHECK(parityHeader.find("block_local_auto_struct") != std::string::npos);
  CHECK(parityHeader.find("if_local_auto_collection") != std::string::npos);
  CHECK(parityHeader.find("ambiguous_omitted_field_envelope") != std::string::npos);
  CHECK(parityHeader.find("query_collection_return_binding") != std::string::npos);
  CHECK(parityHeader.find("query_result_return_binding") != std::string::npos);
  CHECK(parityHeader.find("query_map_receiver_type_text") != std::string::npos);
  CHECK(parityHeader.find("infer_map_value_return_kind") != std::string::npos);
  CHECK(parityHeader.find("shared_collection_receiver_classifiers") != std::string::npos);
  CHECK(parityHeader.find("graph type resolver intentionally upgrades recursive cycle diagnostics") !=
        std::string::npos);
  CHECK(parityHeader.find("graph type resolver still surfaces vm recursive-call lowering limits") !=
        std::string::npos);
}

TEST_CASE("strongly connected component utility is wired through semantics testing api") {
  const std::filesystem::path cwd = std::filesystem::current_path();
  std::filesystem::path cmakePath = cwd / "CMakeLists.txt";
  std::filesystem::path testApiPath = cwd / "include" / "primec" / "testing" / "SemanticsValidationHelpers.h";
  std::filesystem::path sccHeaderPath = cwd / "src" / "semantics" / "StronglyConnectedComponents.h";
  std::filesystem::path sccSourcePath = cwd / "src" / "semantics" / "StronglyConnectedComponents.cpp";
  if (!std::filesystem::exists(cmakePath)) {
    cmakePath = cwd.parent_path() / "CMakeLists.txt";
    testApiPath = cwd.parent_path() / "include" / "primec" / "testing" / "SemanticsValidationHelpers.h";
    sccHeaderPath = cwd.parent_path() / "src" / "semantics" / "StronglyConnectedComponents.h";
    sccSourcePath = cwd.parent_path() / "src" / "semantics" / "StronglyConnectedComponents.cpp";
  }
  REQUIRE(std::filesystem::exists(cmakePath));
  REQUIRE(std::filesystem::exists(testApiPath));
  REQUIRE(std::filesystem::exists(sccHeaderPath));
  REQUIRE(std::filesystem::exists(sccSourcePath));

  const std::string cmake = readTextFile(cmakePath);
  const std::string testApi = readTextFile(testApiPath);
  const std::string sccHeader = readTextFile(sccHeaderPath);
  const std::string sccSource = readTextFile(sccSourcePath);
  CHECK(cmake.find("src/semantics/StronglyConnectedComponents.cpp") != std::string::npos);
  CHECK(cmake.find("primestruct.semantics.scc") != std::string::npos);
  CHECK(testApi.find("struct StronglyConnectedComponentsTestEdge") != std::string::npos);
  CHECK(testApi.find("struct StronglyConnectedComponentSnapshot") != std::string::npos);
  CHECK(testApi.find("struct StronglyConnectedComponentsSnapshot") != std::string::npos);
  CHECK(testApi.find("computeStronglyConnectedComponentsForTesting") != std::string::npos);
  CHECK(sccHeader.find("struct DirectedGraphEdge") != std::string::npos);
  CHECK(sccHeader.find("struct StronglyConnectedComponent") != std::string::npos);
  CHECK(sccHeader.find("struct StronglyConnectedComponents") != std::string::npos);
  CHECK(sccHeader.find("computeStronglyConnectedComponents") != std::string::npos);
  CHECK(sccSource.find("class TarjanSccBuilder") != std::string::npos);
  CHECK(sccSource.find("normalizeComponentOrder") != std::string::npos);
  CHECK(sccSource.find("computeStronglyConnectedComponentsForTesting") != std::string::npos);
}

TEST_CASE("condensation dag utility is wired through semantics testing api") {
  const std::filesystem::path cwd = std::filesystem::current_path();
  std::filesystem::path cmakePath = cwd / "CMakeLists.txt";
  std::filesystem::path testApiPath = cwd / "include" / "primec" / "testing" / "SemanticsValidationHelpers.h";
  std::filesystem::path dagHeaderPath = cwd / "src" / "semantics" / "CondensationDag.h";
  std::filesystem::path dagSourcePath = cwd / "src" / "semantics" / "CondensationDag.cpp";
  if (!std::filesystem::exists(cmakePath)) {
    cmakePath = cwd.parent_path() / "CMakeLists.txt";
    testApiPath = cwd.parent_path() / "include" / "primec" / "testing" / "SemanticsValidationHelpers.h";
    dagHeaderPath = cwd.parent_path() / "src" / "semantics" / "CondensationDag.h";
    dagSourcePath = cwd.parent_path() / "src" / "semantics" / "CondensationDag.cpp";
  }
  REQUIRE(std::filesystem::exists(cmakePath));
  REQUIRE(std::filesystem::exists(testApiPath));
  REQUIRE(std::filesystem::exists(dagHeaderPath));
  REQUIRE(std::filesystem::exists(dagSourcePath));

  const std::string cmake = readTextFile(cmakePath);
  const std::string testApi = readTextFile(testApiPath);
  const std::string dagHeader = readTextFile(dagHeaderPath);
  const std::string dagSource = readTextFile(dagSourcePath);
  CHECK(cmake.find("src/semantics/CondensationDag.cpp") != std::string::npos);
  CHECK(cmake.find("primestruct.semantics.condensation_dag") != std::string::npos);
  CHECK(testApi.find("struct CondensationDagNodeSnapshot") != std::string::npos);
  CHECK(testApi.find("struct CondensationDagEdgeSnapshot") != std::string::npos);
  CHECK(testApi.find("struct CondensationDagSnapshot") != std::string::npos);
  CHECK(testApi.find("computeCondensationDagForTesting") != std::string::npos);
  CHECK(dagHeader.find("struct CondensationDagNode") != std::string::npos);
  CHECK(dagHeader.find("struct CondensationDagEdge") != std::string::npos);
  CHECK(dagHeader.find("struct CondensationDag") != std::string::npos);
  CHECK(dagHeader.find("buildCondensationDag") != std::string::npos);
  CHECK(dagHeader.find("computeCondensationDag") != std::string::npos);
  CHECK(dagSource.find("computeStronglyConnectedComponents(nodeCount, edges)") != std::string::npos);
  CHECK(dagSource.find("collectCondensationEdgePairs") != std::string::npos);
  CHECK(dagSource.find("buildTopologicalComponentOrder") != std::string::npos);
  CHECK(dagSource.find("std::priority_queue<uint32_t, std::vector<uint32_t>, std::greater<uint32_t>>") !=
        std::string::npos);
}

