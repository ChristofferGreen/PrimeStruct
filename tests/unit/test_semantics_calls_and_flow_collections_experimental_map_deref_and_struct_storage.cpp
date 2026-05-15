#include "test_semantics_helpers.h"

TEST_SUITE_BEGIN("primestruct.semantics.calls_flow.collections");

namespace {

void checkStdlibMapPairConstructorMismatch(const std::string &error) {
  CHECK((error.find("argument type mismatch") != std::string::npos ||
         error.find("implicit template arguments conflict") != std::string::npos));
  CHECK(error.find("/std/collections/") != std::string::npos);
  CHECK(error.find("map") != std::string::npos);
}

void checkCanonicalMapConstructorMismatch(const std::string &error) {
  CHECK((error.find("argument type mismatch") != std::string::npos ||
         error.find("implicit template arguments conflict") != std::string::npos));
  CHECK(error.find("/std/collections/map/map") != std::string::npos);
}

} // namespace

TEST_CASE("helper-wrapped Result.ok payloads accept dereferenced canonical result storage") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/internal_map/*

[return<T> effects(heap_alloc)]
wrapStatus<T>([T] status) {
  return(status)
}

[return<Reference<uninitialized<Result<map<string, i32>, ContainerError>>>>]
borrowStorage([Reference<uninitialized<Result<map<string, i32>, ContainerError>>>] storage) {
  return(storage)
}

[effects(heap_alloc), return<int>]
main() {
  [uninitialized<Result<map<string, i32>, ContainerError>> mut] storage{
      uninitialized<Result<map<string, i32>, ContainerError>>()}
  init(dereference(borrowStorage(location(storage))),
       wrapStatus(Result.ok(/std/collections/map/map("left"raw_utf8, 4i32,
                                                    "right"raw_utf8, 7i32))))
  [Result<map<string, i32>, ContainerError>] status{take(storage)}
  return(0i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.empty());
}

TEST_CASE("map constructors keep arg-pack count when soa helpers are imported compatibility") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/internal_map/*
import /std/collections/soa_vector/*

[return<T> effects(heap_alloc)]
wrapValues<T>([T] values) {
  return(values)
}

[effects(heap_alloc), return<int>]
main() {
  [map<string, i32>] values{wrapValues(/std/collections/map/map("left"raw_utf8, 4i32,
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

TEST_CASE("map constructor mismatch wins over imported soa count alias compatibility") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/internal_map/*
import /std/collections/soa_vector/*

[return<T> effects(heap_alloc)]
wrapValues<T>([T] values) {
  return(values)
}

[effects(heap_alloc), return<int>]
main() {
  [map<string, i32>] values{wrapValues(/std/collections/map/map("left"raw_utf8, 4i32,
                                                                "wrong"raw_utf8, false))}
  return(/std/collections/map/count(values))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  INFO(error);
  checkStdlibMapPairConstructorMismatch(error);
}

TEST_CASE("helper-wrapped dereferenced Result.ok storage keeps mismatch diagnostics") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/internal_map/*

[return<T> effects(heap_alloc)]
wrapStatus<T>([T] status) {
  return(status)
}

[return<Reference<uninitialized<Result<map<string, i32>, ContainerError>>>>]
borrowStorage([Reference<uninitialized<Result<map<string, i32>, ContainerError>>>] storage) {
  return(storage)
}

[effects(heap_alloc), return<int>]
main() {
  [uninitialized<Result<map<string, i32>, ContainerError>> mut] storage{
      uninitialized<Result<map<string, i32>, ContainerError>>()}
  init(dereference(borrowStorage(location(storage))),
       wrapStatus(Result.ok(/std/collections/map/map("left"raw_utf8, 4i32,
                                                    "wrong"raw_utf8, false))))
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  INFO(error);
  checkStdlibMapPairConstructorMismatch(error);
}

TEST_CASE("helper-wrapped map constructors accept canonical map struct storage fields") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/internal_map/*

[return<T> effects(heap_alloc)]
wrapValues<T>([T] values) {
  return(values)
}

Holder() {
  [uninitialized<map<string, i32>> mut] storage{uninitialized<map<string, i32>>()}
}

[effects(heap_alloc), return<int>]
main() {
  [Holder mut] holder{Holder()}
  init(holder.storage, wrapValues(/std/collections/map/map("left"raw_utf8, 4i32,
                                                           "right"raw_utf8, 7i32)))
  [map<string, i32>] values{take(holder.storage)}
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
import /std/collections/internal_map/*

[return<T> effects(heap_alloc)]
wrapValues<T>([T] values) {
  return(values)
}

Holder() {
  [uninitialized<map<string, i32>> mut] storage{uninitialized<map<string, i32>>()}
}

[effects(heap_alloc), return<int>]
main() {
  [Holder mut] holder{Holder()}
  init(holder.storage, wrapValues(/std/collections/map/map("left"raw_utf8, 4i32,
                                                           "wrong"raw_utf8, false)))
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  INFO(error);
  checkStdlibMapPairConstructorMismatch(error);
}

TEST_CASE("helper-wrapped Result.ok payloads accept canonical result struct storage fields") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/internal_map/*

[return<T> effects(heap_alloc)]
wrapStatus<T>([T] status) {
  return(status)
}

Holder() {
  [uninitialized<Result<map<string, i32>, ContainerError>> mut] status{
      uninitialized<Result<map<string, i32>, ContainerError>>()}
}

[effects(heap_alloc), return<int>]
main() {
  [Holder mut] holder{Holder()}
  init(holder.status, wrapStatus(Result.ok(/std/collections/map/map("left"raw_utf8, 4i32,
                                                                    "right"raw_utf8, 7i32))))
  [Result<map<string, i32>, ContainerError>] status{take(holder.status)}
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
import /std/collections/internal_map/*

[return<T> effects(heap_alloc)]
wrapStatus<T>([T] status) {
  return(status)
}

Holder() {
  [uninitialized<Result<map<string, i32>, ContainerError>> mut] status{
      uninitialized<Result<map<string, i32>, ContainerError>>()}
}

[effects(heap_alloc), return<int>]
main() {
  [Holder mut] holder{Holder()}
  init(holder.status, wrapStatus(Result.ok(/std/collections/map/map("left"raw_utf8, 4i32,
                                                                    "wrong"raw_utf8, false))))
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  INFO(error);
  checkStdlibMapPairConstructorMismatch(error);
}

TEST_CASE("helper-wrapped map constructors accept dereferenced canonical map struct storage fields") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/internal_map/*

[return<T> effects(heap_alloc)]
wrapValues<T>([T] values) {
  return(values)
}

Holder() {
  [uninitialized<map<string, i32>> mut] storage{uninitialized<map<string, i32>>()}
}

[return<Reference<Holder>>]
borrowHolder([Reference<Holder>] holder) {
  return(holder)
}

[effects(heap_alloc), return<int>]
main() {
  [Holder mut] holder{Holder()}
  init(dereference(borrowHolder(location(holder))).storage,
       wrapValues(/std/collections/map/map("left"raw_utf8, 4i32,
                                           "right"raw_utf8, 7i32)))
  [map<string, i32>] values{take(holder.storage)}
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
import /std/collections/internal_map/*

[return<T> effects(heap_alloc)]
wrapValues<T>([T] values) {
  return(values)
}

Holder() {
  [uninitialized<map<string, i32>> mut] storage{uninitialized<map<string, i32>>()}
}

[return<Reference<Holder>>]
borrowHolder([Reference<Holder>] holder) {
  return(holder)
}

[effects(heap_alloc), return<int>]
main() {
  [Holder mut] holder{Holder()}
  init(dereference(borrowHolder(location(holder))).storage,
       wrapValues(/std/collections/map/map("left"raw_utf8, 4i32,
                                           "wrong"raw_utf8, false)))
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  INFO(error);
  checkStdlibMapPairConstructorMismatch(error);
}

TEST_CASE("map constructors accept dereferenced canonical map storage references") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/internal_map/*

[effects(heap_alloc), return<int>]
main() {
  [uninitialized<map<string, i32>> mut] storage{uninitialized<map<string, i32>>()}
  [Reference<uninitialized<map<string, i32>>>] ref{location(storage)}
  init(dereference(ref), /std/collections/map/map("left"raw_utf8, 5i32,
                                                  "right"raw_utf8, 8i32))
  [map<string, i32>] values{take(storage)}
  return(plus(/std/collections/map/count(values),
              /std/collections/map/at(values, "left"raw_utf8)))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.empty());
}

TEST_CASE("dereferenced canonical map storage references keep mismatch diagnostics") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/internal_map/*

[effects(heap_alloc), return<int>]
main() {
  [uninitialized<map<string, i32>> mut] storage{uninitialized<map<string, i32>>()}
  [Reference<uninitialized<map<string, i32>>>] ref{location(storage)}
  init(dereference(ref), /std/collections/map/map("left"raw_utf8, 5i32,
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
import /std/collections/internal_map/*

[return<T> effects(heap_alloc)]
wrapStatus<T>([T] status) {
  return(status)
}

Holder() {
  [uninitialized<Result<map<string, i32>, ContainerError>> mut] status{
      uninitialized<Result<map<string, i32>, ContainerError>>()}
}

[return<Reference<Holder>>]
borrowHolder([Reference<Holder>] holder) {
  return(holder)
}

[effects(heap_alloc), return<int>]
main() {
  [Holder mut] holder{Holder()}
  init(dereference(borrowHolder(location(holder))).status,
       wrapStatus(Result.ok(/std/collections/map/map("left"raw_utf8, 4i32,
                                                    "right"raw_utf8, 7i32))))
  [Result<map<string, i32>, ContainerError>] status{take(holder.status)}
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
import /std/collections/internal_map/*

[return<T> effects(heap_alloc)]
wrapStatus<T>([T] status) {
  return(status)
}

Holder() {
  [uninitialized<Result<map<string, i32>, ContainerError>> mut] status{
      uninitialized<Result<map<string, i32>, ContainerError>>()}
}

[return<Reference<Holder>>]
borrowHolder([Reference<Holder>] holder) {
  return(holder)
}

[effects(heap_alloc), return<int>]
main() {
  [Holder mut] holder{Holder()}
  init(dereference(borrowHolder(location(holder))).status,
       wrapStatus(Result.ok(/std/collections/map/map("left"raw_utf8, 4i32,
                                                    "wrong"raw_utf8, false))))
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  INFO(error);
  checkStdlibMapPairConstructorMismatch(error);
}

TEST_CASE("helper-wrapped Result.ok payloads infer canonical result auto bindings") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/internal_map/*

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
import /std/collections/internal_map/*

[return<T> effects(heap_alloc)]
wrapStatus<T>([T] status) {
  return(status)
}

[effects(heap_alloc), return<int>]
main() {
  [auto] status{wrapStatus(Result.ok(/std/collections/map/map("left"raw_utf8, 4i32,
                                                             "wrong"raw_utf8, false)))}
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  INFO(error);
  checkStdlibMapPairConstructorMismatch(error);
}

TEST_CASE("stdlib wrapper map constructor accepts explicit canonical map returns") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/internal_map/*

[effects(io_err)]
unexpectedWrapperCanonicalMapReturnError([ContainerError] err) {
  [Result<ContainerError>] status{err.code}
  print_line_error(Result.why(status))
}

[return<map<string, i32>> effects(heap_alloc)]
buildValues() {
  return(/std/collections/map/map<string, i32>("left"raw_utf8, 4i32, "right"raw_utf8, 7i32))
}

[return<Result<int, ContainerError>> effects(io_out, heap_alloc) on_error<ContainerError, /unexpectedWrapperCanonicalMapReturnError>]
main() {
  [map<string, i32>] values{buildValues()}
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

TEST_CASE("stdlib wrapper map constructor keeps mismatch diagnostics on explicit canonical map returns") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/internal_map/*

[return<map<string, i32>> effects(heap_alloc)]
buildValues() {
  return(/std/collections/map/map<string, i32>("left"raw_utf8, 4i32, "right"raw_utf8, false))
}

[effects(heap_alloc), return<int>]
main() {
  [map<string, i32>] values{buildValues()}
  return(/std/collections/map/count(values))
}
  )";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  INFO(error);
  checkStdlibMapPairConstructorMismatch(error);
}

TEST_CASE("stdlib wrapper map constructor accepts explicit canonical map parameters") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/internal_map/*

[effects(io_err)]
unexpectedWrapperCanonicalMapParameterError([ContainerError] err) {
  [Result<ContainerError>] status{err.code}
  print_line_error(Result.why(status))
}

[return<Result<int, ContainerError>> effects(io_out, heap_alloc) on_error<ContainerError, /unexpectedWrapperCanonicalMapParameterError>]
scoreValues([map<string, i32>] values) {
  [i32] found{try(/std/collections/map/tryAt(values, "left"raw_utf8))}
  [i32 mut] total{plus(/std/collections/map/count(values), found)}
  assign(total, plus(total, /std/collections/map/at(values, "left"raw_utf8)))
  assign(total, plus(total, /std/collections/map/at_unsafe(values, "right"raw_utf8)))
  if(/std/collections/map/contains(values, "left"raw_utf8),
     then() { assign(total, plus(total, 1i32)) },
     else() { })
  return(Result.ok(total))
}

[return<Result<int, ContainerError>> effects(io_out, heap_alloc) on_error<ContainerError, /unexpectedWrapperCanonicalMapParameterError>]
main() {
  return(scoreValues(/std/collections/map/map<string, i32>("left"raw_utf8, 4i32, "right"raw_utf8, 7i32)))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("stdlib wrapper map constructor keeps mismatch diagnostics on explicit canonical map parameters") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/internal_map/*

[effects(heap_alloc), return<int>]
scoreValues([map<string, i32>] values) {
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
  checkStdlibMapPairConstructorMismatch(error);
}

TEST_CASE("map constructor assigns into explicit canonical map targets") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/internal_map/*

[effects(io_err)]
unexpectedCanonicalMapAssignError([ContainerError] err) {
  [Result<ContainerError>] status{err.code}
  print_line_error(Result.why(status))
}

[return<Result<int, ContainerError>> effects(io_out, heap_alloc) on_error<ContainerError, /unexpectedCanonicalMapAssignError>]
scoreValues([map<string, i32>] values) {
  [i32] found{try(/std/collections/map/tryAt(values, "left"raw_utf8))}
  [i32 mut] total{plus(/std/collections/map/count(values), found)}
  assign(total, plus(total, /std/collections/map/at(values, "left"raw_utf8)))
  assign(total, plus(total, /std/collections/map/at_unsafe(values, "right"raw_utf8)))
  if(/std/collections/map/contains(values, "left"raw_utf8),
     then() { assign(total, plus(total, 1i32)) },
     else() { })
  return(Result.ok(total))
}

[return<Result<int, ContainerError>> effects(io_out, heap_alloc) on_error<ContainerError, /unexpectedCanonicalMapAssignError>]
replaceAndScore([map<string, i32> mut] values) {
  assign(values, /std/collections/map/map<string, i32>("left"raw_utf8, 4i32, "right"raw_utf8, 7i32))
  return(scoreValues(values))
}

[return<Result<int, ContainerError>> effects(io_out, heap_alloc) on_error<ContainerError, /unexpectedCanonicalMapAssignError>]
main() {
  [map<string, i32> mut] values{/std/collections/map/map<string, i32>("seed"raw_utf8, 0i32)}
  [map<string, i32> mut] other{/std/collections/map/map<string, i32>("seed"raw_utf8, 0i32)}
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

TEST_CASE("canonical map constructor assignment keeps mismatch diagnostics on explicit canonical map bindings") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/internal_map/*

[effects(heap_alloc), return<int>]
main() {
  [map<string, i32> mut] values{/std/collections/map/map<string, i32>("seed"raw_utf8, 0i32)}
  assign(values, /std/collections/map/map<string, i32>("left"raw_utf8, 4i32, "right"raw_utf8, false))
  return(/std/collections/map/count(values))
}
  )";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  INFO(error);
  checkCanonicalMapConstructorMismatch(error);
}

TEST_CASE("wrapper map constructor assignment keeps mismatch diagnostics on explicit canonical map parameters") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/internal_map/*

[effects(heap_alloc), return<int>]
replaceValues([map<string, i32> mut] values) {
  assign(values, /std/collections/map/map<string, i32>("left"raw_utf8, 4i32, "right"raw_utf8, false))
  return(/std/collections/map/count(values))
}

[effects(heap_alloc), return<int>]
main() {
  [map<string, i32> mut] values{/std/collections/map/map<string, i32>("seed"raw_utf8, 0i32)}
  return(replaceValues(values))
}
  )";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  INFO(error);
  checkStdlibMapPairConstructorMismatch(error);
}

TEST_CASE("helper-wrapped map constructor assignments accept explicit canonical map targets") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/internal_map/*

[return<T> effects(heap_alloc)]
wrapValues<T>([T] values) {
  return(values)
}

[effects(heap_alloc), return<int>]
main() {
  [map<string, i32> mut] values{/std/collections/map/map<string, i32>("seed"raw_utf8, 0i32)}
  assign(values, wrapValues(/std/collections/map/map("left"raw_utf8, 4i32, "right"raw_utf8, 7i32)))
  return(/std/collections/map/at(values, "left"raw_utf8))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("helper-wrapped map constructor assignments keep mismatch diagnostics on explicit canonical map targets") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/internal_map/*

[return<T> effects(heap_alloc)]
wrapValues<T>([T] values) {
  return(values)
}

[effects(heap_alloc), return<int>]
main() {
  [map<string, i32> mut] values{/std/collections/map/map<string, i32>("seed"raw_utf8, 0i32)}
  assign(values, wrapValues(/std/collections/map/map("left"raw_utf8, 4i32, "wrong"raw_utf8, false)))
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  INFO(error);
  checkStdlibMapPairConstructorMismatch(error);
}

TEST_CASE("implicit map constructors infer canonical auto locals and auto returns") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/internal_map/*

[return<auto> effects(heap_alloc)]
buildValues() {
  return(/std/collections/map/map<string, i32>("left"raw_utf8, 4i32, "right"raw_utf8, 7i32))
}

[effects(io_err)]
unexpectedCanonicalMapAutoError([ContainerError] err) {
  [Result<ContainerError>] status{err.code}
  print_line_error(Result.why(status))
}

[return<Result<int, ContainerError>> effects(io_out, heap_alloc) on_error<ContainerError, /unexpectedCanonicalMapAutoError>]
main() {
  [auto mut] values{/std/collections/map/map<string, i32>("seed"raw_utf8, 1i32)}
  /std/collections/map/insert<string, i32>(values, "left"raw_utf8, 4i32)
  /std/collections/map/insert<string, i32>(values, "right"raw_utf8, 7i32)
  [auto mut] built{buildValues()}
  /std/collections/map/insert<string, i32>(built, "extra"raw_utf8, 9i32)
  [i32] left{/std/collections/map/at(values, "left"raw_utf8)}
  [i32] extra{/std/collections/map/at(built, "extra"raw_utf8)}
  return(Result.ok(plus(plus(/std/collections/map/count(values), /std/collections/map/count(built)), plus(left, extra))))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_SUITE_END();
