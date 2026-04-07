#include <string>
#include <vector>

#include "third_party/doctest.h"

#include "primec/CompilePipeline.h"
#include "test_ir_pipeline_helpers.h"

TEST_SUITE_BEGIN("primestruct.ir.pipeline.type_resolution_parity");

namespace {

struct TypeResolverPipelineSnapshot {
  bool ok = false;
  primec::CompilePipelineErrorStage errorStage = primec::CompilePipelineErrorStage::None;
  std::string error;
  primec::CompilePipelineDiagnosticInfo diagnosticInfo;
};

TypeResolverPipelineSnapshot runTypeResolverPipelineSnapshot(const std::string &source,
                                                            const std::string &entry = "/main") {
  TypeResolverPipelineSnapshot snapshot;
  PreparedCompilePipelineIrForTesting prepared;
  primec::CompilePipelineDiagnosticInfo diagnosticInfo;
  snapshot.ok = prepareIrThroughCompilePipeline(
      source, entry, "vm", prepared, snapshot.error, &diagnosticInfo);
  snapshot.errorStage = prepared.errorStage;
  snapshot.diagnosticInfo = std::move(diagnosticInfo);
  return snapshot;
}

bool diagnosticReportContainsMessage(const primec::DiagnosticSinkReport &report, const std::string &substring) {
  if (report.message.find(substring) != std::string::npos) {
    return true;
  }
  for (const auto &record : report.records) {
    if (record.message.find(substring) != std::string::npos) {
      return true;
    }
  }
  return false;
}

bool diagnosticReportContainsRelatedLabel(const primec::DiagnosticSinkReport &report, const std::string &label) {
  for (const auto &related : report.relatedSpans) {
    if (related.label.find(label) != std::string::npos) {
      return true;
    }
  }
  for (const auto &record : report.records) {
    for (const auto &related : record.relatedSpans) {
      if (related.label.find(label) != std::string::npos) {
        return true;
      }
    }
  }
  return false;
}

} // namespace

TEST_CASE("default type resolver keeps vm pipeline behavior stable across graph corpus") {
  struct GraphCase {
    std::string name;
    std::string source;
    bool expectSuccess = false;
    std::string errorSubstring = {};
    bool expectDiagnosticSnapshot = true;
  };

  const std::vector<GraphCase> cases = {
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
          true,
      },
      {
          "unresolved_auto",
          R"(
[return<auto>]
main() {
  [i32] value{1i32}
}
)",
          false,
          "unable to infer return type on /main",
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
          false,
          "conflicting return types on /main",
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
          true,
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
          true,
      },
      {
          "direct_local_auto_vector_helper_call_success",
          R"(
/vector/count([vector<i32>] values) {
  return(17i32)
}

[return<vector<i32>> effects(heap_alloc)]
makeValues() {
  [vector<i32>] values{vector<i32>(1i32, 2i32, 3i32)}
  return(values)
}

[return<i32> effects(heap_alloc)]
main() {
  [auto] values{makeValues()}
  return(/vector/count(values))
}
)",
          true,
      },
      {
          "direct_local_auto_vector_helper_method_success",
          R"(
/vector/count([vector<i32>] values) {
  return(17i32)
}

[return<vector<i32>> effects(heap_alloc)]
makeValues() {
  [vector<i32>] values{vector<i32>(1i32, 2i32, 3i32)}
  return(values)
}

[return<i32> effects(heap_alloc)]
main() {
  [auto] values{makeValues()}
  return(values./vector/count())
}
)",
          true,
      },
      {
          "query_local_auto_scalar_success",
          R"(
[return<int>]
main() {
  [auto] value{
    if(true,
      then(){ return(4i32) },
      else(){ return(7i32) })
  }
  return(value)
}
)",
          true,
      },
      {
          "query_local_auto_deleted_array_method",
          R"(
[return<array<i32>>]
valuesA() {
  return(array<i32>(1i32, 2i32))
}

[return<array<i32>>]
valuesB() {
  return(array<i32>(3i32, 4i32))
}

[return<i32>]
main() {
  [auto] values{
    if(true,
      then(){ return(valuesA()) },
      else(){ return(valuesB()) })
  }
  return(values./array/count())
}
)",
          false,
          "unknown method: /array/count",
      },
      {
          "query_local_auto_deleted_array_call",
          R"(
[return<array<i32>>]
valuesA() {
  return(array<i32>(1i32, 2i32))
}

[return<array<i32>>]
valuesB() {
  return(array<i32>(3i32, 4i32))
}

[return<i32>]
main() {
  [auto] values{
    if(true,
      then(){ return(valuesA()) },
      else(){ return(valuesB()) })
  }
  return(/array/count(values))
}
)",
          false,
          "unknown call target: /array/count",
      },
      {
          "query_local_auto_vector_helper_call_boundary",
          R"(
/vector/count([vector<i32>] values) {
  return(17i32)
}

[return<vector<i32>> effects(heap_alloc)]
valuesA() {
  [vector<i32>] values{vector<i32>(1i32, 2i32)}
  return(values)
}

[return<vector<i32>> effects(heap_alloc)]
valuesB() {
  [vector<i32>] values{vector<i32>(3i32, 4i32)}
  return(values)
}

[return<i32> effects(heap_alloc)]
main() {
  [auto] values{
    if(true,
      then(){ return(valuesA()) },
      else(){ return(valuesB()) })
  }
  return(/vector/count(values))
}
)",
          true,
      },
      {
          "query_local_auto_vector_helper_method_boundary",
          R"(
/vector/count([vector<i32>] values) {
  return(17i32)
}

[return<vector<i32>> effects(heap_alloc)]
valuesA() {
  [vector<i32>] values{vector<i32>(1i32, 2i32)}
  return(values)
}

[return<vector<i32>> effects(heap_alloc)]
valuesB() {
  [vector<i32>] values{vector<i32>(3i32, 4i32)}
  return(values)
}

[return<i32> effects(heap_alloc)]
main() {
  [auto] values{
    if(true,
      then(){ return(valuesA()) },
      else(){ return(valuesB()) })
  }
  return(values./vector/count())
}
)",
          true,
      },
      {
          "result_try_local_auto_error_type_boundary",
          R"(
MyError {
}

[return<void>]
unexpectedError([MyError] err) {
}

[return<Result<int, MyError>>]
lookup() {
  return(Result.ok(4i32))
}

[return<Result<int, MyError>> on_error<MyError, /unexpectedError>]
main() {
  [auto] selected{try(lookup())}
  return(Result.ok(selected))
}
)",
          false,
          "on_error requires int-backed error type",
          false,
      },
      {
          "query_try_local_auto_stdlib_semantic_boundary",
          R"(
import /std/collections/*

unexpectedStatusError([ContainerError] err) {
  [Result<ContainerError>] status{err.code}
}

[return<Result<int, ContainerError>>]
lookupA() {
  return(Result.ok(4i32))
}

[return<Result<int, ContainerError>>]
lookupB() {
  return(Result.ok(7i32))
}

[return<Result<int, ContainerError>>]
wrapStatus() {
  [auto] status{
    if(true,
      then(){ return(lookupA()) },
      else(){ return(lookupB()) })
  }
  return(status)
}

[return<Result<int, ContainerError>> on_error<ContainerError, /unexpectedStatusError>]
main() {
  [auto] selected{try(wrapStatus())}
  return(Result.ok(selected))
}
)",
          false,
          "unknown call target: do",
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
          false,
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
          true,
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
          false,
          "unresolved or ambiguous omitted struct field envelope: /Shape/center",
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
          false,
      },
      {
          "query_result_return_binding",
          R"(
import /std/collections/*

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

unexpectedWrapStatusError([ContainerError] err) {
  [Result<ContainerError>] status{err.code}
}

[return<Result<int, ContainerError>> on_error<ContainerError, /unexpectedWrapStatusError>]
main() {
  [auto] values{try(wrapStatus())}
  return(Result.ok(count(values)))
}
)",
          false,
          "unknown method: /array/count",
      },
      {
          "query_map_receiver_type_text",
          R"(
import /std/collections/*
import /std/collections/experimental_map/*

[return<auto> effects(heap_alloc)]
selectValues() {
  if(true,
    then(){ return(/std/collections/map/map("left"raw_utf8, 4i32, "right"raw_utf8, 7i32)) },
    else(){ return(/std/collections/mapPair("left"raw_utf8, 4i32, "right"raw_utf8, 7i32)) })
}

[return<i32> effects(heap_alloc)]
main() {
  return(plus(selectValues().count(), selectValues().at("left"raw_utf8)))
}
)",
          false,
          "unknown call target: drop",
      },
      {
          "infer_map_value_return_kind",
          R"(
import /std/collections/*
import /std/collections/experimental_map/*

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
          false,
          "unknown call target: drop",
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
          false,
          "unknown method: /vector/at",
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
          false,
      },
      {
          "borrowed_soa_vector_auto_return",
          R"(
import /std/collections/*

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
          false,
          "unknown call target: do",
      },
  };

  for (const GraphCase &testCase : cases) {
    CAPTURE(testCase.name);
    const TypeResolverPipelineSnapshot snapshot = runTypeResolverPipelineSnapshot(testCase.source);

    CHECK(snapshot.ok == testCase.expectSuccess);
    if (testCase.expectSuccess) {
      CHECK(snapshot.errorStage == primec::CompilePipelineErrorStage::None);
      CHECK(snapshot.error.empty());
    } else {
      CHECK_FALSE(snapshot.error.empty());
      CHECK(snapshot.error.find(testCase.errorSubstring) != std::string::npos);
      if (testCase.expectDiagnosticSnapshot) {
        CHECK(diagnosticReportContainsMessage(snapshot.diagnosticInfo, testCase.errorSubstring));
      }
    }
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

  const TypeResolverPipelineSnapshot graph = runTypeResolverPipelineSnapshot(source);

  CHECK_FALSE(graph.ok);
  CHECK(graph.error == "return type inference cycle requires explicit annotations on /alpha, /beta");
  CHECK(diagnosticReportContainsMessage(
      graph.diagnosticInfo, "return type inference cycle requires explicit annotations on /alpha, /beta"));
  CHECK(diagnosticReportContainsRelatedLabel(graph.diagnosticInfo, "cycle member: /alpha"));
  CHECK(diagnosticReportContainsRelatedLabel(graph.diagnosticInfo, "cycle member: /beta"));
}

TEST_CASE("graph type resolver still surfaces vm recursive-call lowering limits") {
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

  const TypeResolverPipelineSnapshot graph = runTypeResolverPipelineSnapshot(source);

  CHECK_FALSE(graph.ok);
  CHECK_FALSE(graph.error.empty());
}

TEST_SUITE_END();
