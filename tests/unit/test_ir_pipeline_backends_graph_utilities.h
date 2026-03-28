TEST_CASE("type resolver parity harness is wired through ir pipeline tests") {
  const std::filesystem::path cwd = std::filesystem::current_path();
  std::filesystem::path cmakePath = cwd / "CMakeLists.txt";
  std::filesystem::path paritySourcePath = cwd / "tests" / "unit" / "test_ir_pipeline_type_resolution_parity.cpp";
  std::filesystem::path parityHelperPath = cwd / "tests" / "unit" / "test_ir_pipeline_type_resolution_helpers.h";
  if (!std::filesystem::exists(cmakePath)) {
    cmakePath = cwd.parent_path() / "CMakeLists.txt";
    paritySourcePath = cwd.parent_path() / "tests" / "unit" / "test_ir_pipeline_type_resolution_parity.cpp";
    parityHelperPath = cwd.parent_path() / "tests" / "unit" / "test_ir_pipeline_type_resolution_helpers.h";
  }
  REQUIRE(std::filesystem::exists(cmakePath));
  REQUIRE(std::filesystem::exists(paritySourcePath));
  REQUIRE(std::filesystem::exists(parityHelperPath));

  const std::string cmake = readTextFile(cmakePath);
  const std::string paritySource = readTextFile(paritySourcePath);
  const std::string parityHelper = readTextFile(parityHelperPath);
  CHECK(cmake.find("tests/unit/test_ir_pipeline_type_resolution_parity.cpp") != std::string::npos);
  CHECK(parityHelper.find("struct TypeResolverPipelineSnapshot") != std::string::npos);
  CHECK(parityHelper.find("runTypeResolverPipelineSnapshot") != std::string::npos);
  CHECK(paritySource.find("default type resolver keeps vm pipeline behavior stable across graph corpus") !=
        std::string::npos);
  CHECK(paritySource.find("direct_call_local_auto_struct") != std::string::npos);
  CHECK(paritySource.find("direct_call_local_auto_collection") != std::string::npos);
  CHECK(paritySource.find("block_local_auto_struct") != std::string::npos);
  CHECK(paritySource.find("if_local_auto_collection") != std::string::npos);
  CHECK(paritySource.find("ambiguous_omitted_field_envelope") != std::string::npos);
  CHECK(paritySource.find("query_collection_return_binding") != std::string::npos);
  CHECK(paritySource.find("query_result_return_binding") != std::string::npos);
  CHECK(paritySource.find("query_map_receiver_type_text") != std::string::npos);
  CHECK(paritySource.find("infer_map_value_return_kind") != std::string::npos);
  CHECK(paritySource.find("shared_collection_receiver_classifiers") != std::string::npos);
  CHECK(paritySource.find("graph type resolver intentionally upgrades recursive cycle diagnostics") !=
        std::string::npos);
  CHECK(paritySource.find("graph type resolver still surfaces vm recursive-call lowering limits") !=
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
