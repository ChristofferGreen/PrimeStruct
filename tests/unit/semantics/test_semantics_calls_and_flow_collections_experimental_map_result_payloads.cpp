#include "test_semantics_helpers.h"

namespace {

void checkInitValueTypeMismatch(const std::string &error) {
  CHECK((error.find("init value type mismatch") != std::string::npos ||
         error.find("argument type mismatch") != std::string::npos));
}

} // namespace

TEST_SUITE_BEGIN("primestruct.semantics.calls_flow.collections");

TEST_CASE("helper-wrapped map constructor mismatch reports parameter diagnostics") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/map/*

[return<T> effects(heap_alloc)]
wrapValues<T>([T] values) {
  return(values)
}

[effects(heap_alloc), return<int>]
scoreValues([map<string, i32>] values) {
  return(/std/collections/map/count(values))
}

[effects(heap_alloc), return<int>]
main() {
  return(scoreValues(wrapValues(/std/collections/map/map<string, i32>("left"raw_utf8, 4i32, "wrong"raw_utf8, false))))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.find("argument type mismatch for /std/collections/map/map parameter secondValue") !=
        std::string::npos);
}

TEST_CASE("helper-wrapped Result.ok payloads validate on explicit canonical map parameters") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/map/*

[return<T> effects(heap_alloc)]
wrapStatus<T>([T] status) {
  return(status)
}

[effects(io_err)]
unexpectedWrappedCanonicalResultMapParameterError([ContainerError] err) {
  [Result<ContainerError>] status{err.code}
  print_line_error(Result.why(status))
}

[return<Result<int, ContainerError>> effects(io_out, heap_alloc)
 on_error<ContainerError, /unexpectedWrappedCanonicalResultMapParameterError>]
scoreStatus([Result<map<string, i32>, ContainerError>] status) {
  [map<string, i32>] values{try(status)}
  return(Result.ok(plus(/std/collections/map/count(values),
                        /std/collections/map/at(values, "left"raw_utf8))))
}

[return<Result<int, ContainerError>> effects(io_out, heap_alloc)
 on_error<ContainerError, /unexpectedWrappedCanonicalResultMapParameterError>]
main() {
  return(scoreStatus(wrapStatus(Result.ok(/std/collections/map/map<string, i32>("left"raw_utf8, 4i32,
                                                                  "right"raw_utf8, 7i32)))))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.empty());
}

TEST_CASE("helper-wrapped Result.ok payloads validate before template conflict on explicit canonical map parameters") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/map/*

[return<T> effects(heap_alloc)]
wrapStatus<T>([T] status) {
  return(status)
}

[effects(io_err)]
unexpectedWrappedCanonicalResultMapParameterError([ContainerError] err) {
  [Result<ContainerError>] status{err.code}
  print_line_error(Result.why(status))
}

[return<Result<int, ContainerError>> effects(io_out, heap_alloc)
 on_error<ContainerError, /unexpectedWrappedCanonicalResultMapParameterError>]
scoreStatus([Result<map<string, i32>, ContainerError>] status) {
  [map<string, i32>] values{try(status)}
  return(Result.ok(/std/collections/map/count(values)))
}

[return<Result<int, ContainerError>> effects(io_out, heap_alloc)
 on_error<ContainerError, /unexpectedWrappedCanonicalResultMapParameterError>]
main() {
  return(scoreStatus(wrapStatus(Result.ok(/std/collections/map/map<string, i32>("left"raw_utf8, 4i32,
                                                                  "wrong"raw_utf8, false)))))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("argument type mismatch") != std::string::npos);
}

TEST_CASE("stdlib wrapper map constructor validates on explicit canonical map bindings") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/map/*

[effects(io_err)]
unexpectedWrapperCanonicalMapConstructorError([ContainerError] err) {
  [Result<ContainerError>] status{err.code}
  print_line_error(Result.why(status))
}

[return<Result<int, ContainerError>> effects(io_out, heap_alloc) on_error<ContainerError, /unexpectedWrapperCanonicalMapConstructorError>]
main() {
  [map<string, i32>] values{/std/collections/map/map<string, i32>("left"raw_utf8, 4i32, "right"raw_utf8, 7i32)}
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

TEST_CASE("stdlib wrapper map constructor count validates on explicit canonical map bindings") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/map/*

[effects(heap_alloc), return<int>]
main() {
  [map<string, i32>] values{/std/collections/map/map<string, i32>("left"raw_utf8, 4i32, "right"raw_utf8, false)}
  return(/std/collections/map/count(values))
}
  )";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("argument type mismatch") != std::string::npos);
}

TEST_CASE("helper-wrapped map constructors validate on explicit canonical map bindings") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/map/*

[return<T> effects(heap_alloc)]
wrapValues<T>([T] values) {
  return(values)
}

[effects(io_err)]
unexpectedWrappedCanonicalMapBindingError([ContainerError] err) {
  [Result<ContainerError>] status{err.code}
  print_line_error(Result.why(status))
}

[return<Result<int, ContainerError>> effects(io_out, heap_alloc)
 on_error<ContainerError, /unexpectedWrappedCanonicalMapBindingError>]
main() {
  [map<string, i32>] values{wrapValues(/std/collections/map/map<string, i32>("left"raw_utf8, 4i32, "right"raw_utf8, 7i32))}
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

TEST_CASE("helper-wrapped map constructor counts validate on explicit canonical map bindings") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/map/*

[return<T> effects(heap_alloc)]
wrapValues<T>([T] values) {
  return(values)
}

[effects(heap_alloc), return<int>]
main() {
  [map<string, i32>] values{wrapValues(/std/collections/map/map<string, i32>("left"raw_utf8, 4i32, "wrong"raw_utf8, false))}
  return(/std/collections/map/count(values))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("argument type mismatch") != std::string::npos);
}

TEST_CASE("helper-wrapped Result.ok payloads validate on explicit canonical map bindings") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/map/*

[return<T> effects(heap_alloc)]
wrapStatus<T>([T] status) {
  return(status)
}

[effects(io_err)]
unexpectedWrappedCanonicalResultMapBindingError([ContainerError] err) {
  [Result<ContainerError>] status{err.code}
  print_line_error(Result.why(status))
}

[return<Result<int, ContainerError>> effects(io_out, heap_alloc)
 on_error<ContainerError, /unexpectedWrappedCanonicalResultMapBindingError>]
main() {
  [Result<map<string, i32>, ContainerError>] status{
      wrapStatus(Result.ok(/std/collections/map/map<string, i32>("left"raw_utf8, 4i32, "right"raw_utf8, 7i32)))}
  [map<string, i32>] values{try(status)}
  return(Result.ok(plus(/std/collections/map/count(values),
                        /std/collections/map/at(values, "right"raw_utf8))))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.empty());
}

TEST_CASE("helper-wrapped Result.ok payloads validate before mismatch on explicit canonical map bindings") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/map/*

[return<T> effects(heap_alloc)]
wrapStatus<T>([T] status) {
  return(status)
}

[effects(io_err)]
unexpectedWrappedCanonicalResultMapBindingError([ContainerError] err) {
  [Result<ContainerError>] status{err.code}
  print_line_error(Result.why(status))
}

[return<Result<int, ContainerError>> effects(io_out, heap_alloc)
 on_error<ContainerError, /unexpectedWrappedCanonicalResultMapBindingError>]
main() {
  [Result<map<string, i32>, ContainerError>] status{
      wrapStatus(Result.ok(/std/collections/map/map<string, i32>("left"raw_utf8, 4i32, "wrong"raw_utf8, false)))}
  [map<string, i32>] values{try(status)}
  return(Result.ok(/std/collections/map/count(values)))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("argument type mismatch") != std::string::npos);
}

TEST_CASE("helper-wrapped Result.ok payload assignments validate on explicit canonical map result targets") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/map/*

[return<T> effects(heap_alloc)]
wrapStatus<T>([T] status) {
  return(status)
}

[effects(io_err)]
unexpectedWrappedCanonicalResultMapAssignError([ContainerError] err) {
  [Result<ContainerError>] status{err.code}
  print_line_error(Result.why(status))
}

[return<Result<int, ContainerError>> effects(io_out, heap_alloc)
 on_error<ContainerError, /unexpectedWrappedCanonicalResultMapAssignError>]
main() {
  [Result<map<string, i32>, ContainerError> mut] status{Result.ok(/std/collections/map/map<string, i32>("seed"raw_utf8, 0i32))}
  assign(status, wrapStatus(Result.ok(/std/collections/map/map<string, i32>("left"raw_utf8, 4i32,
                                                              "right"raw_utf8, 7i32))))
  [map<string, i32>] values{try(status)}
  return(Result.ok(plus(/std/collections/map/count(values),
                        /std/collections/map/at(values, "right"raw_utf8))))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.empty());
}

TEST_CASE("helper-wrapped Result.ok payload assignments validate explicit canonical map result targets") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/map/*

[return<T> effects(heap_alloc)]
wrapStatus<T>([T] status) {
  return(status)
}

[effects(io_err)]
unexpectedWrappedCanonicalResultMapAssignError([ContainerError] err) {
  [Result<ContainerError>] status{err.code}
  print_line_error(Result.why(status))
}

[return<Result<int, ContainerError>> effects(io_out, heap_alloc)
 on_error<ContainerError, /unexpectedWrappedCanonicalResultMapAssignError>]
main() {
  [Result<map<string, i32>, ContainerError> mut] status{Result.ok(/std/collections/map/map<string, i32>("seed"raw_utf8, 0i32))}
  assign(status, wrapStatus(Result.ok(/std/collections/map/map<string, i32>("left"raw_utf8, 4i32,
                                                              "wrong"raw_utf8, false))))
  return(Result.ok(0i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("argument type mismatch") != std::string::npos);
}

TEST_CASE("helper-wrapped map constructors validate on canonical map dereference assignment targets") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/map/*

[return<T> effects(heap_alloc)]
wrapValues<T>([T] values) {
  return(values)
}

[return<Reference<map<string, i32>>>]
borrowValues([Reference<map<string, i32>>] values) {
  return(values)
}

[effects(heap_alloc), return<int>]
main() {
  [map<string, i32> mut] values{/std/collections/map/map<string, i32>("seed"raw_utf8, 0i32)}
  assign(dereference(borrowValues(location(values))),
         wrapValues(/std/collections/map/map<string, i32>("left"raw_utf8, 4i32,
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

TEST_CASE("helper-wrapped map dereference assignments validate") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/map/*

[return<T> effects(heap_alloc)]
wrapValues<T>([T] values) {
  return(values)
}

[return<Reference<map<string, i32>>>]
borrowValues([Reference<map<string, i32>>] values) {
  return(values)
}

[effects(heap_alloc), return<int>]
main() {
  [map<string, i32> mut] values{/std/collections/map/map<string, i32>("seed"raw_utf8, 0i32)}
  assign(dereference(borrowValues(location(values))),
         wrapValues(/std/collections/map/map<string, i32>("left"raw_utf8, 4i32,
                                             "wrong"raw_utf8, false)))
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("argument type mismatch") != std::string::npos);
}

TEST_CASE("helper-wrapped Result.ok payloads validate on canonical map result dereference targets") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/map/*

[return<T> effects(heap_alloc)]
wrapStatus<T>([T] status) {
  return(status)
}

[return<Reference<Result<map<string, i32>, ContainerError>>>]
borrowStatus([Reference<Result<map<string, i32>, ContainerError>>] status) {
  return(status)
}

[effects(io_err)]
unexpectedWrappedCanonicalResultMapDerefAssignError([ContainerError] err) {
  [Result<ContainerError>] current{err.code}
  print_line_error(Result.why(current))
}

[return<Result<int, ContainerError>> effects(io_out, heap_alloc)
 on_error<ContainerError, /unexpectedWrappedCanonicalResultMapDerefAssignError>]
main() {
  [Result<map<string, i32>, ContainerError> mut] status{Result.ok(/std/collections/map/map<string, i32>("seed"raw_utf8, 0i32))}
  assign(dereference(borrowStatus(location(status))),
         wrapStatus(Result.ok(/std/collections/map/map<string, i32>("left"raw_utf8, 4i32,
                                                      "right"raw_utf8, 7i32))))
  [map<string, i32>] values{try(status)}
  return(Result.ok(plus(/std/collections/map/count(values),
                        /std/collections/map/at(values, "left"raw_utf8))))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.empty());
}

TEST_CASE("helper-wrapped Result.ok dereference assignments validate") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/map/*

[return<T> effects(heap_alloc)]
wrapStatus<T>([T] status) {
  return(status)
}

[return<Reference<Result<map<string, i32>, ContainerError>>>]
borrowStatus([Reference<Result<map<string, i32>, ContainerError>>] status) {
  return(status)
}

[effects(io_err)]
unexpectedWrappedCanonicalResultMapDerefAssignError([ContainerError] err) {
  [Result<ContainerError>] current{err.code}
  print_line_error(Result.why(current))
}

[return<Result<int, ContainerError>> effects(io_out, heap_alloc)
 on_error<ContainerError, /unexpectedWrappedCanonicalResultMapDerefAssignError>]
main() {
  [Result<map<string, i32>, ContainerError> mut] status{Result.ok(/std/collections/map/map<string, i32>("seed"raw_utf8, 0i32))}
  assign(dereference(borrowStatus(location(status))),
         wrapStatus(Result.ok(/std/collections/map/map<string, i32>("left"raw_utf8, 4i32,
                                                      "wrong"raw_utf8, false))))
  return(Result.ok(0i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("argument type mismatch") != std::string::npos);
}

TEST_CASE("helper-wrapped map constructors reject canonical map uninitialized storage mismatch") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/map/*

[return<T> effects(heap_alloc)]
wrapValues<T>([T] values) {
  return(values)
}

[effects(heap_alloc), return<int>]
main() {
  [uninitialized<map<string, i32>> mut] storage{uninitialized<map<string, i32>>()}
  init(storage, wrapValues(/std/collections/map/map<string, i32>("left"raw_utf8, 4i32,
                                                    "right"raw_utf8, 7i32)))
  [map<string, i32>] values{take(storage)}
  return(plus(/std/collections/map/count(values),
              /std/collections/map/at(values, "right"raw_utf8)))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  INFO(error);
  checkInitValueTypeMismatch(error);
}

TEST_CASE("helper-wrapped map constructor uninitialized storage keeps init mismatch diagnostics") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/map/*

[return<T> effects(heap_alloc)]
wrapValues<T>([T] values) {
  return(values)
}

[effects(heap_alloc), return<int>]
main() {
  [uninitialized<map<string, i32>> mut] storage{uninitialized<map<string, i32>>()}
  init(storage, wrapValues(/std/collections/map/map<string, i32>("left"raw_utf8, 4i32,
                                                    "wrong"raw_utf8, false)))
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  INFO(error);
  checkInitValueTypeMismatch(error);
}

TEST_CASE("helper-wrapped Result.ok canonical map result uninitialized storage keeps init mismatch") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/map/*

[return<T> effects(heap_alloc)]
wrapStatus<T>([T] status) {
  return(status)
}

[effects(heap_alloc), return<int>]
main() {
  [uninitialized<Result<map<string, i32>, ContainerError>> mut] storage{
      uninitialized<Result<map<string, i32>, ContainerError>>()}
  init(storage, wrapStatus(Result.ok(/std/collections/map/map<string, i32>("left"raw_utf8, 4i32,
                                                             "right"raw_utf8, 7i32))))
  [Result<map<string, i32>, ContainerError>] status{take(storage)}
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  INFO(error);
  checkInitValueTypeMismatch(error);
}

TEST_CASE("helper-wrapped Result.ok uninitialized storage keeps init mismatch diagnostics") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/map/*

[return<T> effects(heap_alloc)]
wrapStatus<T>([T] status) {
  return(status)
}

[effects(heap_alloc), return<int>]
main() {
  [uninitialized<Result<map<string, i32>, ContainerError>> mut] storage{
      uninitialized<Result<map<string, i32>, ContainerError>>()}
  init(storage, wrapStatus(Result.ok(/std/collections/map/map<string, i32>("left"raw_utf8, 4i32,
                                                             "wrong"raw_utf8, false))))
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  INFO(error);
  checkInitValueTypeMismatch(error);
}

TEST_CASE("helper-wrapped map constructors on dereferenced canonical map uninitialized storage keep init mismatch") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/map/*

[return<T> effects(heap_alloc)]
wrapValues<T>([T] values) {
  return(values)
}

[return<Reference<uninitialized<map<string, i32>>>>]
borrowStorage([Reference<uninitialized<map<string, i32>>>] storage) {
  return(storage)
}

[effects(heap_alloc), return<int>]
main() {
  [uninitialized<map<string, i32>> mut] storage{uninitialized<map<string, i32>>()}
  init(dereference(borrowStorage(location(storage))),
       wrapValues(/std/collections/map/map<string, i32>("left"raw_utf8, 4i32,
                                           "right"raw_utf8, 7i32)))
  [map<string, i32>] values{take(storage)}
  return(plus(/std/collections/map/count(values),
              /std/collections/map/at(values, "right"raw_utf8)))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  INFO(error);
  checkInitValueTypeMismatch(error);
}

TEST_CASE("helper-wrapped dereferenced map storage keeps init mismatch diagnostics") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/map/*

[return<T> effects(heap_alloc)]
wrapValues<T>([T] values) {
  return(values)
}

[return<Reference<uninitialized<map<string, i32>>>>]
borrowStorage([Reference<uninitialized<map<string, i32>>>] storage) {
  return(storage)
}

[effects(heap_alloc), return<int>]
main() {
  [uninitialized<map<string, i32>> mut] storage{uninitialized<map<string, i32>>()}
  init(dereference(borrowStorage(location(storage))),
       wrapValues(/std/collections/map/map<string, i32>("left"raw_utf8, 4i32,
                                           "wrong"raw_utf8, false)))
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  INFO(error);
  checkInitValueTypeMismatch(error);
}

TEST_SUITE_END();
