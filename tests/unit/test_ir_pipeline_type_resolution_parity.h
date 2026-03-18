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
      {
          "direct_call_local_auto_struct",
          R"(
[struct]
Pair() {
  [i32] value{7i32}
}

[return<Pair>]
makePair() {
  return(Pair())
}

[return<i32>]
main() {
  [auto] pair{makePair()}
  return(pair.value)
}
)",
      },
      {
          "direct_call_local_auto_collection",
          R"(
[return<array<i32>>]
makeValues() {
  return(array<i32>(1i32, 2i32, 3i32))
}

[return<i32>]
main() {
  [auto] values{makeValues()}
  return(count(values))
}
)",
      },
      {
          "block_local_auto_struct",
          R"(
[struct]
Pair() {
  [i32] value{7i32}
}

[return<Pair>]
makePair() {
  return(Pair())
}

[return<i32>]
main() {
  [auto] pair{
    block {
      return(makePair())
    }
  }
  return(pair.value)
}
)",
      },
      {
          "if_local_auto_collection",
          R"(
[return<array<i32>>]
valuesA() {
  return(array<i32>(1i32, 2i32))
}

[return<array<i32>>]
valuesB() {
  return(array<i32>(3i32, 4i32, 5i32))
}

[return<i32>]
main() {
  [auto] values{
    if(true,
      then(){ return(valuesA()) },
      else(){ return(valuesB()) })
  }
  return(count(values))
}
)",
      },
      {
          "block_omitted_field_envelope_struct",
          R"(
[struct]
Vec3() {
  [i32] x{7i32}

  [return<i32>]
  getX() {
    return(this.x)
  }
}

[return<Vec3>]
makeCenter() {
  return(Vec3())
}

[struct]
Sphere() {
  center{
    block {
      return(makeCenter())
    }
  }
}

[return<i32>]
main() {
  [Sphere] shape{Sphere()}
  return(shape.center.getX())
}
)",
      },
      {
          "if_omitted_field_envelope_struct",
          R"(
[struct]
Vec3() {
  [i32] x{7i32}

  [return<i32>]
  getX() {
    return(this.x)
  }
}

[return<Vec3>]
leftCenter() {
  return(Vec3())
}

[return<Vec3>]
rightCenter() {
  return(Vec3())
}

[struct]
Sphere() {
  center{
    if(true,
      then(){ return(leftCenter()) },
      else(){ return(rightCenter()) })
  }
}

[return<i32>]
main() {
  [Sphere] shape{Sphere()}
  return(shape.center.getX())
}
)",
      },
      {
          "ambiguous_omitted_field_envelope",
          R"(
[struct]
Vec2() {
  [i32] x{1i32}
}

[struct]
Vec3() {
  [i32] x{2i32}
}

[struct]
Shape() {
  center{
    if(true,
      then(){ return(Vec2()) },
      else(){ return(Vec3()) })
  }
}

[return<i32>]
main() {
  return(0i32)
}
)",
      },
      {
          "query_collection_return_binding",
          R"(
[return<array<i32>>]
valuesA() {
  return(array<i32>(1i32, 2i32))
}

[return<array<i32>>]
valuesB() {
  return(array<i32>(3i32, 4i32))
}

[return<auto>]
wrapValues() {
  [auto] values{
    if(true,
      then(){ return(valuesA()) },
      else(){ return(valuesB()) })
  }
  return(values)
}

[return<i32>]
main() {
  return(count(wrapValues()))
}
)",
      },
      {
          "query_result_return_binding",
          R"(
[return<Result<array<i32>, ContainerError>>]
valuesOkA() {
  return(Result.ok(array<i32>(1i32, 2i32)))
}

[return<Result<array<i32>, ContainerError>>]
valuesOkB() {
  return(Result.ok(array<i32>(3i32, 4i32)))
}

[return<auto>]
wrapStatus() {
  [auto] status{
    if(true,
      then(){ return(valuesOkA()) },
      else(){ return(valuesOkB()) })
  }
  return(status)
}

[return<i32>]
main() {
  [auto] values{try(wrapStatus())}
  return(count(values))
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

TEST_CASE("graph type resolver intentionally upgrades recursive cycle diagnostics") {
  const std::string source = R"(
[return<auto>]
alpha() {
  return(beta())
}

[return<auto>]
beta() {
  return(alpha())
}

[return<i32>]
main() {
  return(alpha())
}
)";

  const TypeResolverPipelineSnapshot legacy = runTypeResolverPipelineSnapshot(source, "legacy");
  const TypeResolverPipelineSnapshot graph = runTypeResolverPipelineSnapshot(source, "graph");

  CHECK_FALSE(legacy.ok);
  CHECK(legacy.error.find("return type inference requires explicit annotation on /alpha") != std::string::npos);

  CHECK_FALSE(graph.ok);
  CHECK(graph.errorStage == legacy.errorStage);
  CHECK(graph.error == "return type inference cycle requires explicit annotations on /alpha, /beta");
  CHECK(graph.serializedIr.empty());
  CHECK(
      graph.diagnosticSnapshot ==
      "message=return type inference cycle requires explicit annotations on /alpha, /beta\n"
      "primary=:2:1:2:1:1\n"
      "related[0]=cycle member: /alpha@:2:1:2:1\n"
      "related[1]=cycle member: /beta@:7:1:7:1\n"
      "record[0]=return type inference cycle requires explicit annotations on /alpha, /beta@:2:1:2:1:1\n"
      "record_related[0][0]=cycle member: /alpha@:2:1:2:1\n"
      "record_related[0][1]=cycle member: /beta@:7:1:7:1\n");
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
