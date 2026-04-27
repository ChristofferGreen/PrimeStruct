#include "test_semantics_helpers.h"

TEST_SUITE_BEGIN("primestruct.semantics.calls_flow.collections");

namespace {

void checkStdlibMapPairConstructorMismatch(const std::string &error) {
  CHECK((error.find("argument type mismatch") != std::string::npos ||
         error.find("implicit template arguments conflict") != std::string::npos));
  CHECK(error.find("/std/collections/") != std::string::npos);
  CHECK(error.find("mapPair") != std::string::npos);
}

void checkCanonicalMapConstructorMismatch(const std::string &error) {
  CHECK((error.find("argument type mismatch") != std::string::npos ||
         error.find("implicit template arguments conflict") != std::string::npos));
  CHECK((error.find("/std/collections/map/map") != std::string::npos ||
         error.find("/std/collections/mapPair") != std::string::npos ||
         error.find("/std/collections/experimental_map/mapPair") != std::string::npos));
}

} // namespace

TEST_CASE("helper-wrapped Result.ok payloads accept dereferenced experimental result storage") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_map/*

[return<T> effects(heap_alloc)]
wrapStatus<T>([T] status) {
  return(status)
}

[return<Reference<uninitialized<Result<Map<string, i32>, ContainerError>>>>]
borrowStorage([Reference<uninitialized<Result<Map<string, i32>, ContainerError>>>] storage) {
  return(storage)
}

[effects(heap_alloc), return<int>]
main() {
  [uninitialized<Result<Map<string, i32>, ContainerError>> mut] storage{
      uninitialized<Result<Map<string, i32>, ContainerError>>()}
  init(dereference(borrowStorage(location(storage))),
       wrapStatus(Result.ok(/std/collections/mapPair("left"raw_utf8, 4i32,
                                                    "right"raw_utf8, 7i32))))
  [Result<Map<string, i32>, ContainerError>] status{take(storage)}
  return(0i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.empty());
}

TEST_CASE("map constructors keep arg-pack count when soa helpers are imported") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_map/*
import /std/collections/soa_vector/*

[return<T> effects(heap_alloc)]
wrapValues<T>([T] values) {
  return(values)
}

[effects(heap_alloc), return<int>]
main() {
  [Map<string, i32>] values{wrapValues(/std/collections/mapPair("left"raw_utf8, 4i32,
                                                                "right"raw_utf8, 7i32))}
  return(plus(/std/collections/map/count(values),
              /std/collections/map/at(values, "right"raw_utf8)))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.empty());
}

TEST_CASE("helper-wrapped dereferenced Result.ok storage keeps mismatch diagnostics") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_map/*

[return<T> effects(heap_alloc)]
wrapStatus<T>([T] status) {
  return(status)
}

[return<Reference<uninitialized<Result<Map<string, i32>, ContainerError>>>>]
borrowStorage([Reference<uninitialized<Result<Map<string, i32>, ContainerError>>>] storage) {
  return(storage)
}

[effects(heap_alloc), return<int>]
main() {
  [uninitialized<Result<Map<string, i32>, ContainerError>> mut] storage{
      uninitialized<Result<Map<string, i32>, ContainerError>>()}
  init(dereference(borrowStorage(location(storage))),
       wrapStatus(Result.ok(/std/collections/mapPair("left"raw_utf8, 4i32,
                                                    "wrong"raw_utf8, false))))
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  INFO(error);
  checkStdlibMapPairConstructorMismatch(error);
}

TEST_CASE("helper-wrapped map constructors accept experimental map struct storage fields") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_map/*

[return<T> effects(heap_alloc)]
wrapValues<T>([T] values) {
  return(values)
}

Holder() {
  [uninitialized<Map<string, i32>> mut] storage{uninitialized<Map<string, i32>>()}
}

[effects(heap_alloc), return<int>]
main() {
  [Holder mut] holder{Holder()}
  init(holder.storage, wrapValues(/std/collections/mapPair("left"raw_utf8, 4i32,
                                                           "right"raw_utf8, 7i32)))
  [Map<string, i32>] values{take(holder.storage)}
  return(plus(/std/collections/map/count(values),
              /std/collections/map/at(values, "right"raw_utf8)))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.empty());
}

TEST_CASE("helper-wrapped map struct storage fields keep mismatch diagnostics") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_map/*

[return<T> effects(heap_alloc)]
wrapValues<T>([T] values) {
  return(values)
}

Holder() {
  [uninitialized<Map<string, i32>> mut] storage{uninitialized<Map<string, i32>>()}
}

[effects(heap_alloc), return<int>]
main() {
  [Holder mut] holder{Holder()}
  init(holder.storage, wrapValues(/std/collections/mapPair("left"raw_utf8, 4i32,
                                                           "wrong"raw_utf8, false)))
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  INFO(error);
  checkStdlibMapPairConstructorMismatch(error);
}

TEST_CASE("helper-wrapped Result.ok payloads accept experimental result struct storage fields") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_map/*

[return<T> effects(heap_alloc)]
wrapStatus<T>([T] status) {
  return(status)
}

Holder() {
  [uninitialized<Result<Map<string, i32>, ContainerError>> mut] status{
      uninitialized<Result<Map<string, i32>, ContainerError>>()}
}

[effects(heap_alloc), return<int>]
main() {
  [Holder mut] holder{Holder()}
  init(holder.status, wrapStatus(Result.ok(/std/collections/mapPair("left"raw_utf8, 4i32,
                                                                    "right"raw_utf8, 7i32))))
  [Result<Map<string, i32>, ContainerError>] status{take(holder.status)}
  return(0i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.empty());
}

TEST_CASE("helper-wrapped Result.ok struct storage fields keep mismatch diagnostics") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_map/*

[return<T> effects(heap_alloc)]
wrapStatus<T>([T] status) {
  return(status)
}

Holder() {
  [uninitialized<Result<Map<string, i32>, ContainerError>> mut] status{
      uninitialized<Result<Map<string, i32>, ContainerError>>()}
}

[effects(heap_alloc), return<int>]
main() {
  [Holder mut] holder{Holder()}
  init(holder.status, wrapStatus(Result.ok(/std/collections/mapPair("left"raw_utf8, 4i32,
                                                                    "wrong"raw_utf8, false))))
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  INFO(error);
  checkStdlibMapPairConstructorMismatch(error);
}

TEST_CASE("helper-wrapped map constructors accept dereferenced experimental map struct storage fields") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_map/*

[return<T> effects(heap_alloc)]
wrapValues<T>([T] values) {
  return(values)
}

Holder() {
  [uninitialized<Map<string, i32>> mut] storage{uninitialized<Map<string, i32>>()}
}

[return<Reference<Holder>>]
borrowHolder([Reference<Holder>] holder) {
  return(holder)
}

[effects(heap_alloc), return<int>]
main() {
  [Holder mut] holder{Holder()}
  init(dereference(borrowHolder(location(holder))).storage,
       wrapValues(/std/collections/mapPair("left"raw_utf8, 4i32,
                                           "right"raw_utf8, 7i32)))
  [Map<string, i32>] values{take(holder.storage)}
  return(plus(/std/collections/map/count(values),
              /std/collections/map/at(values, "right"raw_utf8)))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.empty());
}

TEST_CASE("helper-wrapped dereferenced map struct storage fields keep mismatch diagnostics") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_map/*

[return<T> effects(heap_alloc)]
wrapValues<T>([T] values) {
  return(values)
}

Holder() {
  [uninitialized<Map<string, i32>> mut] storage{uninitialized<Map<string, i32>>()}
}

[return<Reference<Holder>>]
borrowHolder([Reference<Holder>] holder) {
  return(holder)
}

[effects(heap_alloc), return<int>]
main() {
  [Holder mut] holder{Holder()}
  init(dereference(borrowHolder(location(holder))).storage,
       wrapValues(/std/collections/mapPair("left"raw_utf8, 4i32,
                                           "wrong"raw_utf8, false)))
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  INFO(error);
  checkStdlibMapPairConstructorMismatch(error);
}

TEST_CASE("map constructors accept dereferenced experimental map storage references") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_map/*

[effects(heap_alloc), return<int>]
main() {
  [uninitialized<Map<string, i32>> mut] storage{uninitialized<Map<string, i32>>()}
  [Reference<uninitialized<Map<string, i32>>>] ref{location(storage)}
  init(dereference(ref), /std/collections/mapPair("left"raw_utf8, 5i32,
                                                  "right"raw_utf8, 8i32))
  [Map<string, i32>] values{take(storage)}
  return(plus(/std/collections/map/count(values),
              /std/collections/map/at(values, "left"raw_utf8)))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.empty());
}

TEST_CASE("dereferenced experimental map storage references keep mismatch diagnostics") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_map/*

[effects(heap_alloc), return<int>]
main() {
  [uninitialized<Map<string, i32>> mut] storage{uninitialized<Map<string, i32>>()}
  [Reference<uninitialized<Map<string, i32>>>] ref{location(storage)}
  init(dereference(ref), /std/collections/mapPair("left"raw_utf8, 5i32,
                                                  "wrong"raw_utf8, false))
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  INFO(error);
  checkStdlibMapPairConstructorMismatch(error);
}

TEST_CASE("helper-wrapped Result.ok payloads accept dereferenced result struct storage fields") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_map/*

[return<T> effects(heap_alloc)]
wrapStatus<T>([T] status) {
  return(status)
}

Holder() {
  [uninitialized<Result<Map<string, i32>, ContainerError>> mut] status{
      uninitialized<Result<Map<string, i32>, ContainerError>>()}
}

[return<Reference<Holder>>]
borrowHolder([Reference<Holder>] holder) {
  return(holder)
}

[effects(heap_alloc), return<int>]
main() {
  [Holder mut] holder{Holder()}
  init(dereference(borrowHolder(location(holder))).status,
       wrapStatus(Result.ok(/std/collections/mapPair("left"raw_utf8, 4i32,
                                                    "right"raw_utf8, 7i32))))
  [Result<Map<string, i32>, ContainerError>] status{take(holder.status)}
  return(0i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.empty());
}

TEST_CASE("helper-wrapped dereferenced Result.ok struct storage fields keep mismatch diagnostics") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_map/*

[return<T> effects(heap_alloc)]
wrapStatus<T>([T] status) {
  return(status)
}

Holder() {
  [uninitialized<Result<Map<string, i32>, ContainerError>> mut] status{
      uninitialized<Result<Map<string, i32>, ContainerError>>()}
}

[return<Reference<Holder>>]
borrowHolder([Reference<Holder>] holder) {
  return(holder)
}

[effects(heap_alloc), return<int>]
main() {
  [Holder mut] holder{Holder()}
  init(dereference(borrowHolder(location(holder))).status,
       wrapStatus(Result.ok(/std/collections/mapPair("left"raw_utf8, 4i32,
                                                    "wrong"raw_utf8, false))))
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  INFO(error);
  checkStdlibMapPairConstructorMismatch(error);
}

TEST_CASE("helper-wrapped Result.ok payloads infer experimental result auto bindings") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_map/*

[return<T> effects(heap_alloc)]
wrapStatus<T>([T] status) {
  return(status)
}

[effects(heap_alloc), return<int>]
main() {
  [auto] status{wrapStatus(Result.ok(/std/collections/map/map("left"raw_utf8, 4i32,
                                                             "right"raw_utf8, 7i32)))}
  return(0i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.empty());
}

TEST_CASE("helper-wrapped Result.ok auto binding inference keeps template conflict diagnostics") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_map/*

[return<T> effects(heap_alloc)]
wrapStatus<T>([T] status) {
  return(status)
}

[effects(heap_alloc), return<int>]
main() {
  [auto] status{wrapStatus(Result.ok(/std/collections/mapPair("left"raw_utf8, 4i32,
                                                             "wrong"raw_utf8, false)))}
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  INFO(error);
  checkStdlibMapPairConstructorMismatch(error);
}

TEST_CASE("stdlib wrapper mapPair constructor accepts explicit experimental map returns") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_map/*

[effects(io_err)]
unexpectedWrapperExperimentalMapReturnError([ContainerError] err) {
  [Result<ContainerError>] status{err.code}
  print_line_error(Result.why(status))
}

[return<Map<string, i32>> effects(heap_alloc)]
buildValues() {
  return(/std/collections/mapPair<string, i32>("left"raw_utf8, 4i32, "right"raw_utf8, 7i32))
}

[return<Result<int, ContainerError>> effects(io_out, heap_alloc) on_error<ContainerError, /unexpectedWrapperExperimentalMapReturnError>]
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
  CHECK(error.empty());
}

TEST_CASE("stdlib wrapper mapPair constructor keeps mismatch diagnostics on explicit experimental map returns") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_map/*

[return<Map<string, i32>> effects(heap_alloc)]
buildValues() {
  return(/std/collections/mapPair<string, i32>("left"raw_utf8, 4i32, "right"raw_utf8, false))
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
  checkStdlibMapPairConstructorMismatch(error);
}

TEST_CASE("stdlib wrapper mapPair constructor accepts explicit experimental map parameters") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_map/*

[effects(io_err)]
unexpectedWrapperExperimentalMapParameterError([ContainerError] err) {
  [Result<ContainerError>] status{err.code}
  print_line_error(Result.why(status))
}

[return<Result<int, ContainerError>> effects(io_out, heap_alloc) on_error<ContainerError, /unexpectedWrapperExperimentalMapParameterError>]
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

[return<Result<int, ContainerError>> effects(io_out, heap_alloc) on_error<ContainerError, /unexpectedWrapperExperimentalMapParameterError>]
main() {
  return(scoreValues(/std/collections/mapPair<string, i32>("left"raw_utf8, 4i32, "right"raw_utf8, 7i32)))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("stdlib wrapper mapPair constructor keeps mismatch diagnostics on explicit experimental map parameters") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_map/*

[effects(heap_alloc), return<int>]
scoreValues([Map<string, i32>] values) {
  return(/std/collections/map/count(values))
}

[effects(heap_alloc), return<int>]
main() {
  return(scoreValues(/std/collections/mapPair<string, i32>("left"raw_utf8, 4i32, "right"raw_utf8, false)))
}
  )";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  INFO(error);
  checkStdlibMapPairConstructorMismatch(error);
}

TEST_CASE("map constructor assigns into explicit experimental map targets") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_map/*

[effects(io_err)]
unexpectedExperimentalMapAssignError([ContainerError] err) {
  [Result<ContainerError>] status{err.code}
  print_line_error(Result.why(status))
}

[return<Result<int, ContainerError>> effects(io_out, heap_alloc) on_error<ContainerError, /unexpectedExperimentalMapAssignError>]
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

[return<Result<int, ContainerError>> effects(io_out, heap_alloc) on_error<ContainerError, /unexpectedExperimentalMapAssignError>]
replaceAndScore([Map<string, i32> mut] values) {
  assign(values, /std/collections/mapPair<string, i32>("left"raw_utf8, 4i32, "right"raw_utf8, 7i32))
  return(scoreValues(values))
}

[return<Result<int, ContainerError>> effects(io_out, heap_alloc) on_error<ContainerError, /unexpectedExperimentalMapAssignError>]
main() {
  [Map<string, i32> mut] values{mapNew<string, i32>()}
  [Map<string, i32> mut] other{mapNew<string, i32>()}
  assign(values, /std/collections/map/map<string, i32>("left"raw_utf8, 4i32, "right"raw_utf8, 7i32))
  [i32] localScore{try(scoreValues(values))}
  [i32] paramScore{try(replaceAndScore(other))}
  return(Result.ok(plus(localScore, paramScore)))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("canonical map constructor assignment keeps mismatch diagnostics on explicit experimental map bindings") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_map/*

[effects(heap_alloc), return<int>]
main() {
  [Map<string, i32> mut] values{mapNew<string, i32>()}
  assign(values, /std/collections/map/map<string, i32>("left"raw_utf8, 4i32, "right"raw_utf8, false))
  return(/std/collections/map/count(values))
}
  )";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  INFO(error);
  checkCanonicalMapConstructorMismatch(error);
}

TEST_CASE("wrapper map constructor assignment keeps mismatch diagnostics on explicit experimental map parameters") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_map/*

[effects(heap_alloc), return<int>]
replaceValues([Map<string, i32> mut] values) {
  assign(values, /std/collections/mapPair<string, i32>("left"raw_utf8, 4i32, "right"raw_utf8, false))
  return(/std/collections/map/count(values))
}

[effects(heap_alloc), return<int>]
main() {
  [Map<string, i32> mut] values{mapNew<string, i32>()}
  return(replaceValues(values))
}
  )";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  INFO(error);
  checkStdlibMapPairConstructorMismatch(error);
}

TEST_CASE("helper-wrapped map constructor assignments accept explicit experimental map targets") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_map/*

[return<T> effects(heap_alloc)]
wrapValues<T>([T] values) {
  return(values)
}

[effects(heap_alloc), return<int>]
main() {
  [Map<string, i32> mut] values{mapNew<string, i32>()}
  assign(values, wrapValues(/std/collections/map/map("left"raw_utf8, 4i32, "right"raw_utf8, 7i32)))
  return(/std/collections/map/at(values, "left"raw_utf8))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("helper-wrapped map constructor assignments keep mismatch diagnostics on explicit experimental map targets") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_map/*

[return<T> effects(heap_alloc)]
wrapValues<T>([T] values) {
  return(values)
}

[effects(heap_alloc), return<int>]
main() {
  [Map<string, i32> mut] values{mapNew<string, i32>()}
  assign(values, wrapValues(/std/collections/mapPair("left"raw_utf8, 4i32, "wrong"raw_utf8, false)))
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  INFO(error);
  checkStdlibMapPairConstructorMismatch(error);
}

TEST_CASE("implicit map constructors infer experimental auto locals and auto returns") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_map/*

[return<auto> effects(heap_alloc)]
buildValues() {
  return(/std/collections/mapPair("left"raw_utf8, 4i32, "right"raw_utf8, 7i32))
}

[effects(io_err)]
unexpectedExperimentalMapAutoError([ContainerError] err) {
  [Result<ContainerError>] status{err.code}
  print_line_error(Result.why(status))
}

[return<Result<int, ContainerError>> effects(io_out, heap_alloc) on_error<ContainerError, /unexpectedExperimentalMapAutoError>]
main() {
  [auto mut] values{/std/collections/map/map("seed"raw_utf8, 1i32)}
  mapInsert<string, i32>(values, "left"raw_utf8, 4i32)
  mapInsert<string, i32>(values, "right"raw_utf8, 7i32)
  [auto mut] built{buildValues()}
  mapInsert<string, i32>(built, "extra"raw_utf8, 9i32)
  [i32] left{try(/std/collections/map/tryAt(values, "left"raw_utf8))}
  [i32] extra{try(/std/collections/map/tryAt(built, "extra"raw_utf8))}
  return(Result.ok(plus(plus(/std/collections/map/count(values), /std/collections/map/count(built)), plus(left, extra))))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_SUITE_END();
