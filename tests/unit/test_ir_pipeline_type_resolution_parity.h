TEST_SUITE_BEGIN("primestruct.ir.pipeline.type_resolution_parity");

TEST_CASE("legacy and graph type resolvers keep diagnostics and vm ir aligned on parity corpus") {
  struct ParityCase {
    std::string name;
    std::string source;
  };

  const std::vector<ParityCase> cases = {
      {
          "acyclic_call_chain",
          R"(
[return<auto>]
leaf() {
  return(1i32)
}

[return<auto>]
wrap() {
  return(leaf())
}

[return<i32>]
main() {
  return(wrap())
}
)",
      },
      {
          "unresolved_auto",
          R"(
[return<auto>]
main() {
  [i32] value{1i32}
}
)",
      },
      {
          "conflicting_branch_returns",
          R"(
[return<auto>]
main() {
  if(true,
    then(){ return(1i32) },
    else(){ return(1.5f) })
}
)",
      },
  };

  for (const ParityCase &testCase : cases) {
    CAPTURE(testCase.name);
    const TypeResolverPipelineSnapshot legacy = runTypeResolverPipelineSnapshot(testCase.source, "legacy");
    const TypeResolverPipelineSnapshot graph = runTypeResolverPipelineSnapshot(testCase.source, "graph");

    CHECK(graph.ok == legacy.ok);
    CHECK(graph.errorStage == legacy.errorStage);
    CHECK(graph.error == legacy.error);
    CHECK(graph.diagnosticSnapshot == legacy.diagnosticSnapshot);
    CHECK(graph.serializedIr == legacy.serializedIr);
  }
}

TEST_CASE("graph type resolver intentionally corrects grounded mutual recursion") {
  const std::string source = R"(
[return<auto>]
alpha([bool] done{false}) {
  if(done,
    then(){ return(1i32) },
    else(){ return(beta(true)) })
}

[return<auto>]
beta([bool] done{false}) {
  if(done,
    then(){ return(1i32) },
    else(){ return(alpha(true)) })
}

[return<i32>]
main() {
  return(alpha())
}
)";

  const std::string annotatedSource = R"(
[return<i32>]
alpha([bool] done{false}) {
  if(done,
    then(){ return(1i32) },
    else(){ return(beta(true)) })
}

[return<i32>]
beta([bool] done{false}) {
  if(done,
    then(){ return(1i32) },
    else(){ return(alpha(true)) })
}

[return<i32>]
main() {
  return(alpha())
}
)";

  const TypeResolverPipelineSnapshot legacy = runTypeResolverPipelineSnapshot(source, "legacy");
  const TypeResolverPipelineSnapshot graph = runTypeResolverPipelineSnapshot(source, "graph");
  const TypeResolverPipelineSnapshot annotated = runTypeResolverPipelineSnapshot(annotatedSource, "legacy");

  CHECK_FALSE(legacy.ok);
  CHECK(legacy.error.find("return type inference requires explicit annotation") != std::string::npos);
  CHECK(legacy.serializedIr.empty());

  CHECK(graph.ok);
  CHECK(graph.error.empty());
  CHECK(graph.diagnosticSnapshot == snapshotDiagnosticReport(primec::DiagnosticSinkReport{}));
  CHECK_FALSE(graph.serializedIr.empty());

  CHECK(annotated.ok);
  CHECK(annotated.error.empty());
  CHECK(graph.serializedIr == annotated.serializedIr);
}

TEST_SUITE_END();
