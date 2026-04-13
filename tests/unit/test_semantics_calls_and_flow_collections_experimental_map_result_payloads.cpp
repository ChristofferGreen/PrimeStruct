#include "test_semantics_helpers.h"

TEST_SUITE_BEGIN("primestruct.semantics.calls_flow.collections");

TEST_CASE("helper-wrapped map constructors keep mismatch diagnostics on explicit experimental map parameters") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_map/*

[return<T> effects(heap_alloc)]
wrapValues<T>([T] values) {
  return(values)
}

[effects(heap_alloc), return<int>]
scoreValues([Map<string, i32>] values) {
  return(/std/collections/map/count(values))
}

[effects(heap_alloc), return<int>]
main() {
  return(scoreValues(wrapValues(/std/collections/mapPair("left"raw_utf8, 4i32, "wrong"raw_utf8, false))))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.find("implicit template arguments conflict on /std/collections/mapPair") != std::string::npos);
}

TEST_CASE("helper-wrapped Result.ok payloads accept explicit experimental map parameters") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_map/*

[return<T> effects(heap_alloc)]
wrapStatus<T>([T] status) {
  return(status)
}

[effects(io_err)]
unexpectedWrappedExperimentalResultMapParameterError([ContainerError] err) {
  [Result<ContainerError>] status{err.code}
  print_line_error(Result.why(status))
}

[return<Result<int, ContainerError>> effects(io_out, heap_alloc)
 on_error<ContainerError, /unexpectedWrappedExperimentalResultMapParameterError>]
scoreStatus([Result<Map<string, i32>, ContainerError>] status) {
  [Map<string, i32>] values{try(status)}
  return(Result.ok(plus(/std/collections/map/count(values),
                        /std/collections/map/at(values, "left"raw_utf8))))
}

[return<Result<int, ContainerError>> effects(io_out, heap_alloc)
 on_error<ContainerError, /unexpectedWrappedExperimentalResultMapParameterError>]
main() {
  return(scoreStatus(wrapStatus(Result.ok(/std/collections/mapPair("left"raw_utf8, 4i32,
                                                                  "right"raw_utf8, 7i32)))))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.empty());
}

TEST_CASE("helper-wrapped Result.ok payloads keep mismatch diagnostics on explicit experimental map parameters") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_map/*

[return<T> effects(heap_alloc)]
wrapStatus<T>([T] status) {
  return(status)
}

[effects(io_err)]
unexpectedWrappedExperimentalResultMapParameterError([ContainerError] err) {
  [Result<ContainerError>] status{err.code}
  print_line_error(Result.why(status))
}

[return<Result<int, ContainerError>> effects(io_out, heap_alloc)
 on_error<ContainerError, /unexpectedWrappedExperimentalResultMapParameterError>]
scoreStatus([Result<Map<string, i32>, ContainerError>] status) {
  [Map<string, i32>] values{try(status)}
  return(Result.ok(/std/collections/map/count(values)))
}

[return<Result<int, ContainerError>> effects(io_out, heap_alloc)
 on_error<ContainerError, /unexpectedWrappedExperimentalResultMapParameterError>]
main() {
  return(scoreStatus(wrapStatus(Result.ok(/std/collections/mapPair("left"raw_utf8, 4i32,
                                                                  "wrong"raw_utf8, false)))))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.find("implicit template arguments conflict on /std/collections/mapPair") != std::string::npos);
}

TEST_CASE("stdlib wrapper mapPair constructor accepts explicit experimental map bindings") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_map/*

[effects(io_err)]
unexpectedWrapperExperimentalMapConstructorError([ContainerError] err) {
  [Result<ContainerError>] status{err.code}
  print_line_error(Result.why(status))
}

[return<Result<int, ContainerError>> effects(io_out, heap_alloc) on_error<ContainerError, /unexpectedWrapperExperimentalMapConstructorError>]
main() {
  [Map<string, i32>] values{/std/collections/mapPair<string, i32>("left"raw_utf8, 4i32, "right"raw_utf8, 7i32)}
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

TEST_CASE("stdlib wrapper mapPair constructor keeps mismatch diagnostics on explicit experimental map bindings") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_map/*

[effects(heap_alloc), return<int>]
main() {
  [Map<string, i32>] values{/std/collections/mapPair<string, i32>("left"raw_utf8, 4i32, "right"raw_utf8, false)}
  return(/std/collections/map/count(values))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("mismatch") != std::string::npos);
}

TEST_CASE("helper-wrapped map constructors accept explicit experimental map bindings") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_map/*

[return<T> effects(heap_alloc)]
wrapValues<T>([T] values) {
  return(values)
}

[effects(io_err)]
unexpectedWrappedExperimentalMapBindingError([ContainerError] err) {
  [Result<ContainerError>] status{err.code}
  print_line_error(Result.why(status))
}

[return<Result<int, ContainerError>> effects(io_out, heap_alloc)
 on_error<ContainerError, /unexpectedWrappedExperimentalMapBindingError>]
main() {
  [Map<string, i32>] values{wrapValues(/std/collections/map/map("left"raw_utf8, 4i32, "right"raw_utf8, 7i32))}
  [i32] found{try(/std/collections/map/tryAt(values, "left"raw_utf8))}
  [i32 mut] total{plus(/std/collections/map/count(values), found)}
  assign(total, plus(total, /std/collections/map/at(values, "right"raw_utf8)))
  return(Result.ok(total))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.empty());
}

TEST_CASE("helper-wrapped map constructors keep mismatch diagnostics on explicit experimental map bindings") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_map/*

[return<T> effects(heap_alloc)]
wrapValues<T>([T] values) {
  return(values)
}

[effects(heap_alloc), return<int>]
main() {
  [Map<string, i32>] values{wrapValues(/std/collections/mapPair("left"raw_utf8, 4i32, "wrong"raw_utf8, false))}
  return(/std/collections/map/count(values))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.find("implicit template arguments conflict on /std/collections/mapPair") != std::string::npos);
}

TEST_CASE("helper-wrapped Result.ok payloads accept explicit experimental map bindings") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_map/*

[return<T> effects(heap_alloc)]
wrapStatus<T>([T] status) {
  return(status)
}

[effects(io_err)]
unexpectedWrappedExperimentalResultMapBindingError([ContainerError] err) {
  [Result<ContainerError>] status{err.code}
  print_line_error(Result.why(status))
}

[return<Result<int, ContainerError>> effects(io_out, heap_alloc)
 on_error<ContainerError, /unexpectedWrappedExperimentalResultMapBindingError>]
main() {
  [Result<Map<string, i32>, ContainerError>] status{
      wrapStatus(Result.ok(/std/collections/map/map("left"raw_utf8, 4i32, "right"raw_utf8, 7i32)))}
  [Map<string, i32>] values{try(status)}
  return(Result.ok(plus(/std/collections/map/count(values),
                        /std/collections/map/at(values, "right"raw_utf8))))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.empty());
}

TEST_CASE("helper-wrapped Result.ok payloads keep mismatch diagnostics on explicit experimental map bindings") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_map/*

[return<T> effects(heap_alloc)]
wrapStatus<T>([T] status) {
  return(status)
}

[effects(io_err)]
unexpectedWrappedExperimentalResultMapBindingError([ContainerError] err) {
  [Result<ContainerError>] status{err.code}
  print_line_error(Result.why(status))
}

[return<Result<int, ContainerError>> effects(io_out, heap_alloc)
 on_error<ContainerError, /unexpectedWrappedExperimentalResultMapBindingError>]
main() {
  [Result<Map<string, i32>, ContainerError>] status{
      wrapStatus(Result.ok(/std/collections/mapPair("left"raw_utf8, 4i32, "wrong"raw_utf8, false)))}
  [Map<string, i32>] values{try(status)}
  return(Result.ok(/std/collections/map/count(values)))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.find("implicit template arguments conflict on /std/collections/mapPair") != std::string::npos);
}

TEST_CASE("helper-wrapped Result.ok payload assignments accept explicit experimental map result targets") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_map/*

[return<T> effects(heap_alloc)]
wrapStatus<T>([T] status) {
  return(status)
}

[effects(io_err)]
unexpectedWrappedExperimentalResultMapAssignError([ContainerError] err) {
  [Result<ContainerError>] status{err.code}
  print_line_error(Result.why(status))
}

[return<Result<int, ContainerError>> effects(io_out, heap_alloc)
 on_error<ContainerError, /unexpectedWrappedExperimentalResultMapAssignError>]
main() {
  [Result<Map<string, i32>, ContainerError> mut] status{Result.ok(mapNew<string, i32>())}
  assign(status, wrapStatus(Result.ok(/std/collections/mapPair("left"raw_utf8, 4i32,
                                                              "right"raw_utf8, 7i32))))
  [Map<string, i32>] values{try(status)}
  return(Result.ok(plus(/std/collections/map/count(values),
                        /std/collections/map/at(values, "right"raw_utf8))))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.empty());
}

TEST_CASE("helper-wrapped Result.ok payload assignments keep mismatch diagnostics on explicit experimental map result targets") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_map/*

[return<T> effects(heap_alloc)]
wrapStatus<T>([T] status) {
  return(status)
}

[effects(io_err)]
unexpectedWrappedExperimentalResultMapAssignError([ContainerError] err) {
  [Result<ContainerError>] status{err.code}
  print_line_error(Result.why(status))
}

[return<Result<int, ContainerError>> effects(io_out, heap_alloc)
 on_error<ContainerError, /unexpectedWrappedExperimentalResultMapAssignError>]
main() {
  [Result<Map<string, i32>, ContainerError> mut] status{Result.ok(mapNew<string, i32>())}
  assign(status, wrapStatus(Result.ok(/std/collections/mapPair("left"raw_utf8, 4i32,
                                                              "wrong"raw_utf8, false))))
  return(Result.ok(0i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.find("implicit template arguments conflict on /std/collections/mapPair") != std::string::npos);
}

TEST_CASE("helper-wrapped map constructors accept experimental map dereference assignment targets") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_map/*

[return<T> effects(heap_alloc)]
wrapValues<T>([T] values) {
  return(values)
}

[return<Reference<Map<string, i32>>>]
borrowValues([Reference<Map<string, i32>>] values) {
  return(values)
}

[effects(heap_alloc), return<int>]
main() {
  [Map<string, i32> mut] values{mapNew<string, i32>()}
  assign(dereference(borrowValues(location(values))),
         wrapValues(/std/collections/mapPair("left"raw_utf8, 4i32,
                                             "right"raw_utf8, 7i32)))
  return(plus(/std/collections/map/count(values),
              /std/collections/map/at(values, "right"raw_utf8)))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.empty());
}

TEST_CASE("helper-wrapped map dereference assignments keep mismatch diagnostics") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_map/*

[return<T> effects(heap_alloc)]
wrapValues<T>([T] values) {
  return(values)
}

[return<Reference<Map<string, i32>>>]
borrowValues([Reference<Map<string, i32>>] values) {
  return(values)
}

[effects(heap_alloc), return<int>]
main() {
  [Map<string, i32> mut] values{mapNew<string, i32>()}
  assign(dereference(borrowValues(location(values))),
         wrapValues(/std/collections/mapPair("left"raw_utf8, 4i32,
                                             "wrong"raw_utf8, false)))
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.find("implicit template arguments conflict on /std/collections/mapPair") != std::string::npos);
}

TEST_CASE("helper-wrapped Result.ok payloads accept experimental map result dereference targets") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_map/*

[return<T> effects(heap_alloc)]
wrapStatus<T>([T] status) {
  return(status)
}

[return<Reference<Result<Map<string, i32>, ContainerError>>>]
borrowStatus([Reference<Result<Map<string, i32>, ContainerError>>] status) {
  return(status)
}

[effects(io_err)]
unexpectedWrappedExperimentalResultMapDerefAssignError([ContainerError] err) {
  [Result<ContainerError>] current{err.code}
  print_line_error(Result.why(current))
}

[return<Result<int, ContainerError>> effects(io_out, heap_alloc)
 on_error<ContainerError, /unexpectedWrappedExperimentalResultMapDerefAssignError>]
main() {
  [Result<Map<string, i32>, ContainerError> mut] status{Result.ok(mapNew<string, i32>())}
  assign(dereference(borrowStatus(location(status))),
         wrapStatus(Result.ok(/std/collections/mapPair("left"raw_utf8, 4i32,
                                                      "right"raw_utf8, 7i32))))
  [Map<string, i32>] values{try(status)}
  return(Result.ok(plus(/std/collections/map/count(values),
                        /std/collections/map/at(values, "left"raw_utf8))))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.empty());
}

TEST_CASE("helper-wrapped Result.ok dereference assignments keep mismatch diagnostics") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_map/*

[return<T> effects(heap_alloc)]
wrapStatus<T>([T] status) {
  return(status)
}

[return<Reference<Result<Map<string, i32>, ContainerError>>>]
borrowStatus([Reference<Result<Map<string, i32>, ContainerError>>] status) {
  return(status)
}

[effects(io_err)]
unexpectedWrappedExperimentalResultMapDerefAssignError([ContainerError] err) {
  [Result<ContainerError>] current{err.code}
  print_line_error(Result.why(current))
}

[return<Result<int, ContainerError>> effects(io_out, heap_alloc)
 on_error<ContainerError, /unexpectedWrappedExperimentalResultMapDerefAssignError>]
main() {
  [Result<Map<string, i32>, ContainerError> mut] status{Result.ok(mapNew<string, i32>())}
  assign(dereference(borrowStatus(location(status))),
         wrapStatus(Result.ok(/std/collections/mapPair("left"raw_utf8, 4i32,
                                                      "wrong"raw_utf8, false))))
  return(Result.ok(0i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.find("implicit template arguments conflict on /std/collections/mapPair") != std::string::npos);
}

TEST_CASE("helper-wrapped map constructors accept experimental map uninitialized storage") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_map/*

[return<T> effects(heap_alloc)]
wrapValues<T>([T] values) {
  return(values)
}

[effects(heap_alloc), return<int>]
main() {
  [uninitialized<Map<string, i32>> mut] storage{uninitialized<Map<string, i32>>()}
  init(storage, wrapValues(/std/collections/mapPair("left"raw_utf8, 4i32,
                                                    "right"raw_utf8, 7i32)))
  [Map<string, i32>] values{take(storage)}
  return(plus(/std/collections/map/count(values),
              /std/collections/map/at(values, "right"raw_utf8)))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.empty());
}

TEST_CASE("helper-wrapped map constructor uninitialized storage keeps mismatch diagnostics") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_map/*

[return<T> effects(heap_alloc)]
wrapValues<T>([T] values) {
  return(values)
}

[effects(heap_alloc), return<int>]
main() {
  [uninitialized<Map<string, i32>> mut] storage{uninitialized<Map<string, i32>>()}
  init(storage, wrapValues(/std/collections/mapPair("left"raw_utf8, 4i32,
                                                    "wrong"raw_utf8, false)))
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.find("implicit template arguments conflict on /std/collections/mapPair") != std::string::npos);
}

TEST_CASE("helper-wrapped Result.ok payloads accept experimental map result uninitialized storage") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_map/*

[return<T> effects(heap_alloc)]
wrapStatus<T>([T] status) {
  return(status)
}

[effects(heap_alloc), return<int>]
main() {
  [uninitialized<Result<Map<string, i32>, ContainerError>> mut] storage{
      uninitialized<Result<Map<string, i32>, ContainerError>>()}
  init(storage, wrapStatus(Result.ok(/std/collections/mapPair("left"raw_utf8, 4i32,
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

TEST_CASE("helper-wrapped Result.ok uninitialized storage keeps mismatch diagnostics") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_map/*

[return<T> effects(heap_alloc)]
wrapStatus<T>([T] status) {
  return(status)
}

[effects(heap_alloc), return<int>]
main() {
  [uninitialized<Result<Map<string, i32>, ContainerError>> mut] storage{
      uninitialized<Result<Map<string, i32>, ContainerError>>()}
  init(storage, wrapStatus(Result.ok(/std/collections/mapPair("left"raw_utf8, 4i32,
                                                             "wrong"raw_utf8, false))))
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.find("implicit template arguments conflict on /std/collections/mapPair") != std::string::npos);
}

TEST_CASE("helper-wrapped map constructors accept dereferenced experimental map uninitialized storage") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_map/*

[return<T> effects(heap_alloc)]
wrapValues<T>([T] values) {
  return(values)
}

[return<Reference<uninitialized<Map<string, i32>>>>]
borrowStorage([Reference<uninitialized<Map<string, i32>>>] storage) {
  return(storage)
}

[effects(heap_alloc), return<int>]
main() {
  [uninitialized<Map<string, i32>> mut] storage{uninitialized<Map<string, i32>>()}
  init(dereference(borrowStorage(location(storage))),
       wrapValues(/std/collections/mapPair("left"raw_utf8, 4i32,
                                           "right"raw_utf8, 7i32)))
  [Map<string, i32>] values{take(storage)}
  return(plus(/std/collections/map/count(values),
              /std/collections/map/at(values, "right"raw_utf8)))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.empty());
}

TEST_CASE("helper-wrapped dereferenced map storage keeps mismatch diagnostics") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_map/*

[return<T> effects(heap_alloc)]
wrapValues<T>([T] values) {
  return(values)
}

[return<Reference<uninitialized<Map<string, i32>>>>]
borrowStorage([Reference<uninitialized<Map<string, i32>>>] storage) {
  return(storage)
}

[effects(heap_alloc), return<int>]
main() {
  [uninitialized<Map<string, i32>> mut] storage{uninitialized<Map<string, i32>>()}
  init(dereference(borrowStorage(location(storage))),
       wrapValues(/std/collections/mapPair("left"raw_utf8, 4i32,
                                           "wrong"raw_utf8, false)))
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.find("implicit template arguments conflict on /std/collections/mapPair") != std::string::npos);
}

TEST_SUITE_END();
