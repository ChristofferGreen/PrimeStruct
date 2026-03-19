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
      {
          "query_map_receiver_type_text",
          R"(
[return<auto> effects(heap_alloc)]
selectValues() {
  if(true,
    then(){ return(/std/collections/map/map("left"raw_utf8, 4i32, "right"raw_utf8, 7i32)) },
    else(){ return(/std/collections/mapPair("left"raw_utf8, 4i32, "right"raw_utf8, 7i32)) })
}

[return<Result<int, ContainerError>> effects(heap_alloc)]
main() {
  [i32] total{plus(selectValues().count(), try(selectValues().tryAt("left"raw_utf8)))}
  return(Result.ok(total))
}
)",
      },
      {
          "infer_map_value_return_kind",
          R"(
[return<auto> effects(heap_alloc)]
selectValues() {
  if(true,
    then(){ return(/std/collections/map/map("left"raw_utf8, 4i32, "right"raw_utf8, 7i32)) },
    else(){ return(/std/collections/mapPair("left"raw_utf8, 4i32, "right"raw_utf8, 7i32)) })
}

[return<auto> effects(heap_alloc)]
pickLeft() {
  return(selectValues().at("left"raw_utf8))
}

[return<i32> effects(heap_alloc)]
main() {
  return(pickLeft())
}
)",
      },
      {
          "shared_collection_receiver_classifiers",
          R"(
[return<int>]
packScore([args<string>] values) {
  return(plus(values.at(0i32).count(), values.at_unsafe(1i32).count()))
}

[return<int> effects(heap_alloc)]
vectorScore() {
  [vector<string>] values{vector<string>("alpha"raw_utf8, "beta"raw_utf8)}
  return(plus(values.at(0i32).count(), values.at_unsafe(1i32).count()))
}

[return<int> effects(heap_alloc)]
mapScore() {
  [map<i32, string>] values{map<i32, string>(1i32, "left"raw_utf8, 2i32, "right"raw_utf8)}
  return(plus(values.at(1i32).count(), values.at_unsafe(2i32).count()))
}

[return<int> effects(heap_alloc)]
main() {
  return(plus(packScore("ab"raw_utf8, "cde"raw_utf8),
              plus(vectorScore(), mapScore())))
}
)",
      },
      {
          "auto_collection_receiver_classifiers",
          R"(
[return<auto>]
packScore([args<string>] values) {
  return(plus(values.at(0i32).count(), values.at_unsafe(1i32).count()))
}

[return<auto> effects(heap_alloc)]
vectorScore() {
  [vector<string>] values{vector<string>("alpha"raw_utf8, "beta"raw_utf8)}
  return(plus(values.at(0i32).count(), values.at_unsafe(1i32).count()))
}

[return<auto> effects(heap_alloc)]
mapScore() {
  [map<i32, string>] values{map<i32, string>(1i32, "left"raw_utf8, 2i32, "right"raw_utf8)}
  return(plus(values.at(1i32).count(), values.at_unsafe(2i32).count()))
}

[return<i32> effects(heap_alloc)]
main() {
  return(plus(packScore("ab"raw_utf8, "cde"raw_utf8),
              plus(vectorScore(), mapScore())))
}
)",
      },
      {
          "borrowed_soa_vector_auto_return",
          R"(
Particle() {
  [i32] x{1i32}
}

[return<auto>]
scoreRefs([args<Reference<soa_vector<Particle>>>] values) {
  return(count(to_aos(dereference(values[0i32]))))
}

[return<i32>]
main() {
  return(0i32)
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
  CHECK(legacy.error.find("return type inference requires explicit annotation") != std::string::npos);

  CHECK_FALSE(graph.ok);
  CHECK(graph.errorStage == legacy.errorStage);
  CHECK(graph.error == "return type inference cycle requires explicit annotations on /alpha, /beta");
  CHECK(graph.serializedIr.empty());
  CHECK(graph.diagnosticSnapshot.find(
            "message=return type inference cycle requires explicit annotations on /alpha, /beta") !=
        std::string::npos);
  CHECK(graph.diagnosticSnapshot.find("cycle member: /alpha") != std::string::npos);
  CHECK(graph.diagnosticSnapshot.find("cycle member: /beta") != std::string::npos);
}

TEST_CASE("grounded mutual recursion currently diverges between legacy and graph vm pipelines") {
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

  const TypeResolverPipelineSnapshot legacy = runTypeResolverPipelineSnapshot(source, "legacy");
  const TypeResolverPipelineSnapshot graph = runTypeResolverPipelineSnapshot(source, "graph");

  CHECK_FALSE(legacy.ok);
  CHECK(legacy.error.find("return type inference requires explicit annotation on /beta") != std::string::npos);
  CHECK(legacy.serializedIr.empty());

  CHECK_FALSE(graph.ok);
  CHECK(graph.error.find("unable to infer return type on /alpha") != std::string::npos);
  CHECK(graph.serializedIr.empty());
}

TEST_SUITE_END();
