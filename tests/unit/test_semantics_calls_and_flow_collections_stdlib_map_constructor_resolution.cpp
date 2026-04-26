#include "test_semantics_helpers.h"

TEST_SUITE_BEGIN("primestruct.semantics.calls_flow.collections");

namespace {

void checkCanonicalMapConstructorMismatch(const std::string &error) {
  CHECK((error.find("argument type mismatch") != std::string::npos ||
         error.find("implicit template arguments conflict") != std::string::npos));
  CHECK((error.find("/std/collections/map/map") != std::string::npos ||
         error.find("/std/collections/mapPair") != std::string::npos));
}

} // namespace

TEST_CASE("stdlib namespaced map constructor resolves through imported stdlib helper") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{/std/collections/map/map<i32, i32>(1i32, 4i32, 2i32, 5i32)}
  return(/std/collections/map/at(values, 1i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("stdlib namespaced map constructor keeps same-path definition before wrapper fallback") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/std/collections/map/map<T, U>([T] key, [U] value) {
  return(41i32)
}

[effects(heap_alloc), return<int>]
/std/collections/mapSingle<T, U>([T] key, [U] value) {
  return(77i32)
}

[effects(heap_alloc), return<int>]
main() {
  return(/std/collections/map/map<i32, i32>(1i32, 2i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.empty());
}

TEST_CASE("stdlib namespaced map constructor requires imported stdlib helper") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{/std/collections/map/map<i32, i32>(1i32, 4i32, 2i32, 5i32)}
  return(count(values))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /std/collections/map/map") != std::string::npos);
}

TEST_CASE("imported stdlib namespaced map helpers accept ordinary named arguments") {
  const std::string source = R"(
import /std/collections/*

[effects(io_err)]
unexpectedMapNamedArgsError([ContainerError] err) {
  [Result<ContainerError>] status{err.code}
  print_line_error(Result.why(status))
}

[return<Result<int, ContainerError>> effects(io_out, heap_alloc) on_error<ContainerError, /unexpectedMapNamedArgsError>]
main() {
  [map<string, i32>] values{/std/collections/map/map<string, i32>(
      [secondKey] "right"raw_utf8, [secondValue] 7i32, [firstKey] "left"raw_utf8, [firstValue] 4i32)}
  [i32] found{try(/std/collections/map/tryAt<string, i32>([key] "left"raw_utf8, [values] values))}
  [i32 mut] total{/std/collections/map/count<string, i32>([values] values)}
  assign(total, plus(total, /std/collections/map/at<string, i32>([key] "right"raw_utf8, [values] values)))
  assign(total, plus(total, /std/collections/map/at_unsafe<string, i32>([key] "left"raw_utf8, [values] values)))
  if(/std/collections/map/contains<string, i32>([key] "right"raw_utf8, [values] values),
     then() { assign(total, plus(total, found)) },
     else() { })
  return(Result.ok(total))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("canonical namespaced map helpers reject borrowed experimental map receivers") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_map/*

[return<Reference<Map<string, i32>>>]
borrowExperimentalMap([Reference<Map<string, i32>>] values) {
  return(values)
}

[effects(heap_alloc), return<i32>]
main() {
  [Map<string, i32>] values{mapPair<string, i32>("left"raw_utf8, 4i32, "right"raw_utf8, 7i32)}
  [Reference<Map<string, i32>>] ref{borrowExperimentalMap(location(values))}
  return(/std/collections/map/count<string, i32>(ref))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /std/collections/map/count") != std::string::npos);
}

TEST_CASE("canonical namespaced map _ref helpers accept borrowed experimental map receivers") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_map/*

[return<Reference<Map<string, i32>>>]
borrowExperimentalMap([Reference<Map<string, i32>>] values) {
  return(values)
}

[effects(io_err)]
unexpectedCanonicalExperimentalMapBorrowedRefError([ContainerError] err) {
  [Result<ContainerError>] status{err.code}
  print_line_error(Result.why(status))
}

[return<Result<int, ContainerError>> effects(io_out, heap_alloc) on_error<ContainerError, /unexpectedCanonicalExperimentalMapBorrowedRefError>]
main() {
  [Map<string, i32>] values{mapPair<string, i32>("left"raw_utf8, 4i32, "right"raw_utf8, 7i32)}
  [Reference<Map<string, i32>> mut] ref{borrowExperimentalMap(location(values))}
  /std/collections/map/insert_ref<string, i32>(ref, "third"raw_utf8, 11i32)
  /std/collections/map/insert_ref<string, i32>(ref, "right"raw_utf8, 13i32)
  [i32] found{try(/std/collections/map/tryAt_ref<string, i32>(ref, "left"raw_utf8))}
  [i32 mut] total{plus(/std/collections/map/count_ref<string, i32>(ref), found)}
  assign(total, plus(total, /std/collections/map/at_ref<string, i32>(ref, "left"raw_utf8)))
  assign(total, plus(total, /std/collections/map/at_unsafe_ref<string, i32>(ref, "right"raw_utf8)))
  if(/std/collections/map/contains_ref<string, i32>(ref, "left"raw_utf8),
     then() { assign(total, plus(total, 1i32)) },
     else() { })
  if(not(/std/collections/map/contains_ref<string, i32>(ref, "missing"raw_utf8)),
     then() { assign(total, plus(total, 2i32)) },
     else() { })
  return(Result.ok(total))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("canonical namespaced map helpers keep unknown-call diagnostics for borrowed experimental map receivers") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_map/*

[struct]
Key() {
  [i32] value{0i32}
}

[return<bool>]
/Key/equal([Key] left, [Key] right) {
  return(equal(left.value, right.value))
}

[return<Reference<Map<Key, i32>>>]
borrowExperimentalMap([Reference<Map<Key, i32>>] values) {
  return(values)
}

[effects(heap_alloc), return<int>]
main() {
  [Map<Key, i32>] values{mapNew<Key, i32>()}
  [Reference<Map<Key, i32>>] ref{borrowExperimentalMap(location(values))}
  if(/std/collections/map/contains<Key, i32>(ref, Key(1i32)),
     then() { return(1i32) },
     else() { return(0i32) })
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.find("unknown call target: /std/collections/map/contains") != std::string::npos);
}

TEST_CASE("canonical namespaced map _ref helpers include builtin map key rejects for borrowed experimental map receivers") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_map/*

[struct]
Key() {
  [i32] value{0i32}
}

[return<bool>]
/Key/equal([Key] left, [Key] right) {
  return(equal(left.value, right.value))
}

[return<Reference<Map<Key, i32>>>]
borrowExperimentalMap([Reference<Map<Key, i32>>] values) {
  return(values)
}

[effects(heap_alloc), return<int>]
main() {
  [Map<Key, i32>] values{mapNew<Key, i32>()}
  [Reference<Map<Key, i32>>] ref{borrowExperimentalMap(location(values))}
  if(/std/collections/map/contains_ref<Key, i32>(ref, Key(1i32)),
     then() { return(1i32) },
     else() { return(0i32) })
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("trait constraint not satisfied") != std::string::npos);
  CHECK(error.find("Comparable</Key> requires less_than(/Key, /Key) -> bool") != std::string::npos);
}

TEST_CASE("imported stdlib namespaced map constructor keeps mismatch diagnostics") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [map<string, i32>] values{/std/collections/map/map<string, i32>("left"raw_utf8, 4i32, "right"raw_utf8, false)}
  return(/std/collections/map/count<string, i32>(values))
}
  )";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  INFO(error);
  checkCanonicalMapConstructorMismatch(error);
}

TEST_CASE("stdlib namespaced map count requires an explicit canonical helper definition") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 4i32, 2i32, 5i32)}
  return(/std/collections/map/count(values))
}
  )";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /std/collections/map/count") != std::string::npos);
}

TEST_CASE("stdlib namespaced map count builtin rejects template args without imported helper") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 4i32, 2i32, 5i32)}
  return(/std/collections/map/count<i32, i32>(values))
}
  )";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("count does not accept template arguments") != std::string::npos);
}

TEST_CASE("stdlib namespaced map count builtin rejects template args through compile pipeline") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 4i32, 2i32, 5i32)}
  return(/std/collections/map/count<i32, i32>(values))
}
  )";
  std::string error;
  CHECK_FALSE(validateProgramForCompileTarget(source, "/main", "vm", "", error));
  CHECK(error.find("count does not accept template arguments") != std::string::npos);
}

TEST_CASE("stdlib namespaced map contains requires imported stdlib helper or explicit definition") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 4i32, 2i32, 5i32)}
  [bool] found{/std/collections/map/contains(values, 1i32)}
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /std/collections/map/contains") != std::string::npos);
}

TEST_CASE("stdlib namespaced map tryAt requires imported stdlib helper or explicit definition") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 4i32, 2i32, 5i32)}
  return(try(/std/collections/map/tryAt(values, 1i32)))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /std/collections/map/tryAt") != std::string::npos);
}

TEST_CASE("stdlib namespaced map tryAt does not inherit alias-only helper definition") {
  const std::string source = R"(
[struct]
ContainerError() {
  [i32] code{0i32}
}

[effects(heap_alloc), return<Result<int, ContainerError>>]
/map/tryAt([map<i32, i32>] values, [i32] key) {
  return(Result.ok(17i32))
}

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 4i32, 2i32, 5i32)}
  [auto] inferred{/std/collections/map/tryAt(values, 1i32)}
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /std/collections/map/tryAt") != std::string::npos);
}

TEST_CASE("stdlib namespaced map access helpers accept imported stdlib wrappers") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 4i32, 2i32, 5i32)}
  [i32] first{/std/collections/map/at(values, 1i32)}
  [i32] second{/std/collections/map/at_unsafe(values, 2i32)}
  return(plus(first, second))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.empty());
}

TEST_CASE("collected diagnostics ignore imported canonical map access helper calls") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 4i32, 2i32, 5i32)}
  [i32] aliasCount{/std/collections/map/count(values)}
  [i32] first{/std/collections/map/at(values, 1i32)}
  return(plus(aliasCount, first))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("collected diagnostics keep unknown target for unsupported canonical map helper calls") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 4i32)}
  return(/std/collections/map/missing(values))
}
)";
  primec::SemanticDiagnosticInfo diagnosticInfo;
  std::string error;
  CHECK_FALSE(validateProgramCollectingDiagnostics(source, "/main", error, diagnosticInfo));
  CHECK(error.find("unknown call target: /std/collections/map/missing") != std::string::npos);
  REQUIRE(diagnosticInfo.records.size() >= 1);
  bool foundUnknownTarget = false;
  for (const auto &record : diagnosticInfo.records) {
    if (record.message.find("unknown call target: /std/collections/map/missing") != std::string::npos) {
      foundUnknownTarget = true;
      break;
    }
  }
  CHECK(foundUnknownTarget);
}

TEST_CASE("stdlib namespaced map helpers accept canonical map references") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [/std/collections/map<i32, i32>] values{map<i32, i32>(1i32, 4i32, 2i32, 5i32)}
  [Reference</std/collections/map<i32, i32>>] ref{location(values)}
  [i32] c{/std/collections/map/count(ref)}
  [i32] first{/std/collections/map/at(ref, 1i32)}
  [i32] second{/std/collections/map/at_unsafe(ref, 2i32)}
  return(plus(c, plus(first, second)))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("stdlib namespaced map helpers accept experimental map value receivers") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_map/*

[effects(io_err)]
unexpectedCanonicalExperimentalMapValueError([ContainerError] err) {
  [Result<ContainerError>] status{err.code}
  print_line_error(Result.why(status))
}

[return<Result<int, ContainerError>> effects(io_out, heap_alloc) on_error<ContainerError, /unexpectedCanonicalExperimentalMapValueError>]
main() {
  [Map<string, i32>] values{mapPair<string, i32>("left"raw_utf8, 4i32, "right"raw_utf8, 7i32)}
  [i32] found{try(/std/collections/map/tryAt(values, "left"raw_utf8))}
  [i32 mut] total{plus(/std/collections/map/count(values), found)}
  assign(total, plus(total, /std/collections/map/at(values, "left"raw_utf8)))
  assign(total, plus(total, /std/collections/map/at_unsafe(values, "right"raw_utf8)))
  if(/std/collections/map/contains(values, "left"raw_utf8),
     then() { assign(total, plus(total, 1i32)) },
     else() { })
  return(Result.ok(total))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("stdlib wrapper map helpers accept experimental map value receivers") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_map/*

[effects(io_err)]
unexpectedWrapperExperimentalMapValueError([ContainerError] err) {
  [Result<ContainerError>] status{err.code}
  print_line_error(Result.why(status))
}

[return<Result<int, ContainerError>> effects(io_out, heap_alloc) on_error<ContainerError, /unexpectedWrapperExperimentalMapValueError>]
main() {
  [Map<string, i32>] values{mapPair<string, i32>("left"raw_utf8, 4i32, "right"raw_utf8, 7i32)}
  [i32] found{try(/std/collections/mapTryAt(values, "left"raw_utf8))}
  [i32 mut] total{plus(/std/collections/mapCount(values), found)}
  assign(total, plus(total, /std/collections/mapAt(values, "left"raw_utf8)))
  assign(total, plus(total, /std/collections/mapAtUnsafe(values, "right"raw_utf8)))
  if(/std/collections/mapContains(values, "left"raw_utf8),
     then() { assign(total, plus(total, 1i32)) },
     else() { })
  return(Result.ok(total))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("stdlib wrapper map helpers accept experimental constructor receivers") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_map/*

[effects(io_err)]
unexpectedWrapperExperimentalMapReceiverError([ContainerError] err) {
  [Result<ContainerError>] status{err.code}
  print_line_error(Result.why(status))
}

[return<Result<int, ContainerError>> effects(io_out, heap_alloc) on_error<ContainerError, /unexpectedWrapperExperimentalMapReceiverError>]
main() {
  [i32] found{try(/std/collections/mapTryAt(/std/collections/map/map("left"raw_utf8, 4i32, "right"raw_utf8, 7i32), "left"raw_utf8))}
  [i32 mut] total{plus(/std/collections/mapCount(/std/collections/mapPair("left"raw_utf8, 4i32, "right"raw_utf8, 7i32)), found)}
  assign(total, plus(total, /std/collections/mapAt(/std/collections/mapPair("extra"raw_utf8, 9i32, "other"raw_utf8, 2i32), "extra"raw_utf8)))
  assign(total, plus(total, /std/collections/mapAtUnsafe(/std/collections/map/map("bonus"raw_utf8, 5i32, "keep"raw_utf8, 1i32), "bonus"raw_utf8)))
  if(/std/collections/mapContains(/std/collections/mapPair("left"raw_utf8, 4i32, "right"raw_utf8, 7i32), "right"raw_utf8),
     then() { assign(total, plus(total, 1i32)) },
     else() { })
  return(Result.ok(total))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("stdlib wrapper map helpers keep constructor mismatch diagnostics on experimental constructor receivers") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_map/*

[effects(heap_alloc), return<int>]
main() {
  return(/std/collections/mapCount(/std/collections/mapPair("left"raw_utf8, 4i32, "wrong"raw_utf8, false)))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.find("implicit template arguments conflict on /std/collections/mapPair") != std::string::npos);
}

TEST_CASE("stdlib namespaced map constructor accepts explicit experimental map bindings") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_map/*

[effects(io_err)]
unexpectedCanonicalExperimentalMapConstructorError([ContainerError] err) {
  [Result<ContainerError>] status{err.code}
  print_line_error(Result.why(status))
}

[return<Result<int, ContainerError>> effects(io_out, heap_alloc) on_error<ContainerError, /unexpectedCanonicalExperimentalMapConstructorError>]
main() {
  [Map<string, i32>] values{/std/collections/map/map<string, i32>("left"raw_utf8, 4i32, "right"raw_utf8, 7i32)}
  [i32] found{try(/std/collections/map/tryAt(values, "left"raw_utf8))}
  [i32 mut] total{plus(/std/collections/map/count(values), found)}
  assign(total, plus(total, /std/collections/map/at(values, "left"raw_utf8)))
  assign(total, plus(total, /std/collections/map/at_unsafe(values, "right"raw_utf8)))
  if(/std/collections/map/contains(values, "left"raw_utf8),
     then() { assign(total, plus(total, 1i32)) },
     else() { })
  return(Result.ok(total))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.empty());
}

TEST_CASE("stdlib namespaced map constructor keeps mismatch diagnostics on explicit experimental map bindings") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_map/*

[effects(heap_alloc), return<int>]
main() {
  [Map<string, i32>] values{/std/collections/map/map<string, i32>("left"raw_utf8, 4i32, "right"raw_utf8, false)}
  return(/std/collections/map/count(values))
}
  )";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  INFO(error);
  checkCanonicalMapConstructorMismatch(error);
}

TEST_CASE("stdlib namespaced map constructor accepts explicit experimental map returns") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_map/*

[effects(io_err)]
unexpectedCanonicalExperimentalMapReturnError([ContainerError] err) {
  [Result<ContainerError>] status{err.code}
  print_line_error(Result.why(status))
}

[return<Map<string, i32>> effects(heap_alloc)]
buildValues() {
  return(/std/collections/map/map<string, i32>("left"raw_utf8, 4i32, "right"raw_utf8, 7i32))
}

[return<Result<int, ContainerError>> effects(io_out, heap_alloc) on_error<ContainerError, /unexpectedCanonicalExperimentalMapReturnError>]
main() {
  [Map<string, i32>] values{buildValues()}
  [i32] found{try(/std/collections/map/tryAt(values, "left"raw_utf8))}
  [i32 mut] total{plus(/std/collections/map/count(values), found)}
  assign(total, plus(total, /std/collections/map/at(values, "left"raw_utf8)))
  assign(total, plus(total, /std/collections/map/at_unsafe(values, "right"raw_utf8)))
  if(/std/collections/map/contains(values, "left"raw_utf8),
     then() { assign(total, plus(total, 1i32)) },
     else() { })
  return(Result.ok(total))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.empty());
}

TEST_CASE("stdlib namespaced map constructor keeps mismatch diagnostics on explicit experimental map returns") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_map/*

[return<Map<string, i32>> effects(heap_alloc)]
buildValues() {
  return(/std/collections/map/map<string, i32>("left"raw_utf8, 4i32, "right"raw_utf8, false))
}

[effects(heap_alloc), return<int>]
main() {
  [Map<string, i32>] values{buildValues()}
  return(/std/collections/map/count(values))
}
  )";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  INFO(error);
  checkCanonicalMapConstructorMismatch(error);
}

TEST_CASE("stdlib namespaced map constructor accepts explicit experimental map parameters") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_map/*

[effects(io_err)]
unexpectedCanonicalExperimentalMapParameterError([ContainerError] err) {
  [Result<ContainerError>] status{err.code}
  print_line_error(Result.why(status))
}

[return<Result<int, ContainerError>> effects(io_out, heap_alloc) on_error<ContainerError, /unexpectedCanonicalExperimentalMapParameterError>]
scoreValues([Map<string, i32>] values) {
  [i32] found{try(/std/collections/map/tryAt(values, "left"raw_utf8))}
  [i32 mut] total{plus(/std/collections/map/count(values), found)}
  assign(total, plus(total, /std/collections/map/at(values, "left"raw_utf8)))
  assign(total, plus(total, /std/collections/map/at_unsafe(values, "right"raw_utf8)))
  if(/std/collections/map/contains(values, "left"raw_utf8),
     then() { assign(total, plus(total, 1i32)) },
     else() { })
  return(Result.ok(total))
}

[return<Result<int, ContainerError>> effects(io_out, heap_alloc) on_error<ContainerError, /unexpectedCanonicalExperimentalMapParameterError>]
main() {
  return(scoreValues(/std/collections/map/map<string, i32>("left"raw_utf8, 4i32, "right"raw_utf8, 7i32)))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("stdlib namespaced map constructor keeps mismatch diagnostics on explicit experimental map parameters") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_map/*

[effects(heap_alloc), return<int>]
scoreValues([Map<string, i32>] values) {
  return(/std/collections/map/count(values))
}

[effects(heap_alloc), return<int>]
main() {
  return(scoreValues(/std/collections/map/map<string, i32>("left"raw_utf8, 4i32, "right"raw_utf8, false)))
}
  )";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  INFO(error);
  checkCanonicalMapConstructorMismatch(error);
}

TEST_CASE("helper-wrapped map constructors accept explicit experimental map parameters") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_map/*

Holder() {}

[return<T> effects(heap_alloc)]
wrapValues<T>([T] values) {
  return(values)
}

[effects(io_err)]
unexpectedWrappedExperimentalMapParameterError([ContainerError] err) {
  [Result<ContainerError>] status{err.code}
  print_line_error(Result.why(status))
}

[return<Result<int, ContainerError>> effects(io_out, heap_alloc) on_error<ContainerError, /unexpectedWrappedExperimentalMapParameterError>]
scoreValues([Map<string, i32>] values) {
  [i32] found{try(/std/collections/map/tryAt(values, "left"raw_utf8))}
  [i32 mut] total{plus(/std/collections/map/count(values), found)}
  assign(total, plus(total, /std/collections/map/at(values, "left"raw_utf8)))
  return(Result.ok(total))
}

[return<Result<int, ContainerError>> effects(io_out, heap_alloc) on_error<ContainerError, /unexpectedWrappedExperimentalMapParameterError>]
/Holder/score([Holder] self, [Map<string, i32>] values) {
  [i32] bonus{try(/std/collections/map/tryAt(values, "bonus"raw_utf8))}
  return(Result.ok(plus(/std/collections/map/count(values), bonus)))
}

[return<Result<int, ContainerError>> effects(io_out, heap_alloc) on_error<ContainerError, /unexpectedWrappedExperimentalMapParameterError>]
main() {
  [Holder] holder{Holder()}
  return(Result.ok(plus(try(scoreValues(wrapValues(/std/collections/map/map("left"raw_utf8, 4i32, "right"raw_utf8, 7i32)))),
                        try(holder.score(wrapValues(/std/collections/mapPair("bonus"raw_utf8, 5i32, "other"raw_utf8, 2i32)))))))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.empty());
}

TEST_SUITE_END();
