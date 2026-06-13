#include "test_semantics_helpers.h"

TEST_SUITE_BEGIN("primestruct.semantics.calls_flow.collections");

TEST_CASE("field-bound canonical map compatibility count calls require alias helpers") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/map/*

Holder() {
  [map<string, i32>] values{/std/collections/map/map<string, i32>("left"raw_utf8, 4i32, "right"raw_utf8, 7i32)}
}

[effects(heap_alloc), return<int>]
main() {
  [Holder] holder{Holder()}
  return(/map/count(holder.values))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /map/count") != std::string::npos);
}

TEST_CASE("field-bound canonical map stdlib namespaced count methods report retired count diagnostics") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/map/*

Holder() {
  [map<string, i32>] values{/std/collections/map/map<string, i32>("left"raw_utf8, 4i32, "right"raw_utf8, 7i32)}
}

[effects(heap_alloc), return<int>]
main() {
  [Holder] holder{Holder()}
  return(holder.values./std/collections/map/count())
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /map/count") != std::string::npos);
}

TEST_CASE("stdlib map constructor assignments accept explicit canonical map struct fields") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/map/*

Holder() {
  [map<string, i32> mut] primary{/std/collections/map/map<string, i32>("seed"raw_utf8, 0i32)}
  [map<string, i32> mut] secondary{/std/collections/map/map<string, i32>("seed"raw_utf8, 0i32)}
}

[effects(heap_alloc), return<int>]
main() {
  [Holder mut] holder{Holder()}
  assign(holder.primary, /std/collections/map/map<string, i32>("left"raw_utf8, 4i32, "right"raw_utf8, 7i32))
  assign(holder.secondary, /std/collections/map/map<string, i32>("extra"raw_utf8, 9i32, "other"raw_utf8, 2i32))
  return(plus(/std/collections/map/at(holder.primary, "left"raw_utf8),
              /std/collections/map/at(holder.secondary, "extra"raw_utf8)))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("stdlib map constructor assignments validate canonical map struct fields") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/map/*

Holder() {
  [map<string, i32> mut] values{/std/collections/map/map<string, i32>("seed"raw_utf8, 0i32)}
}

[effects(heap_alloc), return<int>]
main() {
  [Holder mut] holder{Holder()}
  assign(holder.values, /std/collections/map/map<string, i32>("left"raw_utf8, 4i32, "wrong"raw_utf8, false))
  return(0i32)
}
  )";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("helper-wrapped map constructor assignments report retired at diagnostics on inferred canonical map struct fields") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/map/*

[return<T> effects(heap_alloc)]
wrapValues<T>([T] values) {
  return(values)
}

Holder() {
  values{wrapValues(/std/collections/map/map<string, i32>("seed"raw_utf8, 0i32))}
}

[effects(heap_alloc), return<int>]
main() {
  [Holder mut] holder{Holder()}
  assign(holder.values, wrapValues(/std/collections/map/map<string, i32>("left"raw_utf8, 4i32, "right"raw_utf8, 7i32)))
  return(/std/collections/map/at<string, i32>(holder.values, "left"raw_utf8))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /map/at") != std::string::npos);
}

TEST_CASE("helper-wrapped map constructor assignments validate inferred canonical map struct fields") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/map/*

[return<T> effects(heap_alloc)]
wrapValues<T>([T] values) {
  return(values)
}

Holder() {
  values{wrapValues(/std/collections/map/map<string, i32>("seed"raw_utf8, 0i32))}
}

[effects(heap_alloc), return<int>]
main() {
  [Holder mut] holder{Holder()}
  assign(holder.values, wrapValues(/std/collections/map/map<string, i32>("left"raw_utf8, 4i32, "wrong"raw_utf8, false)))
  return(0i32)
}
  )";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("helper-wrapped Result.ok payload assignments report retired count diagnostics on explicit canonical map result struct fields") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/map/*

[return<T> effects(heap_alloc)]
wrapStatus<T>([T] status) {
  return(status)
}

Holder() {
  [Result<map<string, i32>, ContainerError> mut] status{Result.ok(/std/collections/map/map<string, i32>("seed"raw_utf8, 0i32))}
}

[effects(io_err)]
unexpectedWrappedCanonicalResultMapFieldAssignError([ContainerError] err) {
  [Result<ContainerError>] current{err.code}
  print_line_error(Result.why(current))
}

[return<Result<int, ContainerError>> effects(io_out, heap_alloc)
 on_error<ContainerError, /unexpectedWrappedCanonicalResultMapFieldAssignError>]
main() {
  [Holder mut] holder{Holder()}
  assign(holder.status, wrapStatus(Result.ok(/std/collections/map/map<string, i32>("left"raw_utf8, 4i32,
                                                                     "right"raw_utf8, 7i32))))
  [map<string, i32>] values{try(holder.status)}
  return(Result.ok(plus(/std/collections/map/count(values),
                        /std/collections/map/at(values, "left"raw_utf8))))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.find("unknown call target: /map/count") != std::string::npos);
}

TEST_CASE("helper-wrapped Result.ok payload assignments validate explicit canonical map result struct fields") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/map/*

[return<T> effects(heap_alloc)]
wrapStatus<T>([T] status) {
  return(status)
}

Holder() {
  [Result<map<string, i32>, ContainerError> mut] status{Result.ok(/std/collections/map/map<string, i32>("seed"raw_utf8, 0i32))}
}

[effects(io_err)]
unexpectedWrappedCanonicalResultMapFieldAssignError([ContainerError] err) {
  [Result<ContainerError>] current{err.code}
  print_line_error(Result.why(current))
}

[return<Result<int, ContainerError>> effects(io_out, heap_alloc)
 on_error<ContainerError, /unexpectedWrappedCanonicalResultMapFieldAssignError>]
main() {
  [Holder mut] holder{Holder()}
  assign(holder.status, wrapStatus(Result.ok(/std/collections/map/map<string, i32>("left"raw_utf8, 4i32,
                                                                     "wrong"raw_utf8, false))))
  return(Result.ok(0i32))
}
  )";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.empty());
}

TEST_CASE("helper-wrapped map constructor assignments accept dereferenced canonical map struct fields") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/map/*

[return<T> effects(heap_alloc)]
wrapValues<T>([T] values) {
  return(values)
}

Holder() {
  [map<string, i32> mut] values{/std/collections/map/map<string, i32>("seed"raw_utf8, 0i32)}
}

[return<Reference<Holder>>]
borrowHolder([Reference<Holder>] holder) {
  return(holder)
}

[effects(heap_alloc), return<int>]
main() {
  [Holder mut] holder{Holder()}
  assign(dereference(borrowHolder(location(holder))).values,
         wrapValues(/std/collections/map/map<string, i32>("left"raw_utf8, 4i32,
                                             "right"raw_utf8, 7i32)))
  return(/std/collections/map/at(holder.values, "left"raw_utf8))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.empty());
}

TEST_CASE("helper-wrapped dereferenced map field assignments validate") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/map/*

[return<T> effects(heap_alloc)]
wrapValues<T>([T] values) {
  return(values)
}

Holder() {
  [map<string, i32> mut] values{/std/collections/map/map<string, i32>("seed"raw_utf8, 0i32)}
}

[return<Reference<Holder>>]
borrowHolder([Reference<Holder>] holder) {
  return(holder)
}

[effects(heap_alloc), return<int>]
main() {
  [Holder mut] holder{Holder()}
  assign(dereference(borrowHolder(location(holder))).values,
         wrapValues(/std/collections/map/map<string, i32>("left"raw_utf8, 4i32,
                                             "wrong"raw_utf8, false)))
  return(0i32)
}
  )";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.empty());
}

TEST_CASE("helper-wrapped Result.ok assignments report retired count diagnostics on dereferenced canonical result map struct fields") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/map/*

[return<T> effects(heap_alloc)]
wrapStatus<T>([T] status) {
  return(status)
}

Holder() {
  [Result<map<string, i32>, ContainerError> mut] status{Result.ok(/std/collections/map/map<string, i32>("seed"raw_utf8, 0i32))}
}

[return<Reference<Holder>>]
borrowHolder([Reference<Holder>] holder) {
  return(holder)
}

[effects(io_err)]
unexpectedWrappedCanonicalResultMapDerefFieldAssignError([ContainerError] err) {
  [Result<ContainerError>] current{err.code}
  print_line_error(Result.why(current))
}

[return<Result<int, ContainerError>> effects(io_out, heap_alloc)
 on_error<ContainerError, /unexpectedWrappedCanonicalResultMapDerefFieldAssignError>]
main() {
  [Holder mut] holder{Holder()}
  assign(dereference(borrowHolder(location(holder))).status,
         wrapStatus(Result.ok(/std/collections/map/map<string, i32>("left"raw_utf8, 4i32,
                                                      "right"raw_utf8, 7i32))))
  [map<string, i32>] values{try(holder.status)}
  return(Result.ok(plus(/std/collections/map/count(values),
                        /std/collections/map/at(values, "left"raw_utf8))))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.find("unknown call target: /map/count") != std::string::npos);
}

TEST_CASE("helper-wrapped dereferenced Result.ok field assignments validate") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/map/*

[return<T> effects(heap_alloc)]
wrapStatus<T>([T] status) {
  return(status)
}

Holder() {
  [Result<map<string, i32>, ContainerError> mut] status{Result.ok(/std/collections/map/map<string, i32>("seed"raw_utf8, 0i32))}
}

[return<Reference<Holder>>]
borrowHolder([Reference<Holder>] holder) {
  return(holder)
}

[effects(io_err)]
unexpectedWrappedCanonicalResultMapDerefFieldAssignError([ContainerError] err) {
  [Result<ContainerError>] current{err.code}
  print_line_error(Result.why(current))
}

[return<Result<int, ContainerError>> effects(io_out, heap_alloc)
 on_error<ContainerError, /unexpectedWrappedCanonicalResultMapDerefFieldAssignError>]
main() {
  [Holder mut] holder{Holder()}
  assign(dereference(borrowHolder(location(holder))).status,
         wrapStatus(Result.ok(/std/collections/map/map<string, i32>("left"raw_utf8, 4i32,
                                                      "wrong"raw_utf8, false))))
  return(Result.ok(0i32))
}
  )";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.empty());
}

TEST_CASE("stdlib namespaced map helpers keep Comparable diagnostics on canonical map value receivers") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/map/*

[struct]
Key() {
  [i32] value{0i32}
}

[effects(heap_alloc), return<int>]
main() {
  [map<Key, i32>] values{mapNew<Key, i32>()}
  if(/std/collections/map/contains(values, Key{1i32}),
     then() { return(1i32) },
     else() { return(0i32) })
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("Comparable") != std::string::npos);
}

TEST_CASE("stdlib namespaced map helpers keep canonical key diagnostics on map references") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [/std/collections/map<i32, i32>] values{map<i32, i32>(1i32, 4i32)}
  [Reference</std/collections/map<i32, i32>>] ref{location(values)}
  return(/std/collections/map/at(ref, true))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("argument type mismatch for /std/collections/map/at") != std::string::npos);
  CHECK(error.find("parameter key") != std::string::npos);
}

TEST_CASE("map compatibility count call requires explicit alias definition") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/std/collections/map/count([map<i32, i32>] values) {
  return(17i32)
}

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 4i32)}
  return(/map/count(values))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /map/count") != std::string::npos);
}

TEST_CASE("map compatibility count auto inference requires explicit alias definition") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/std/collections/map/count([map<i32, i32>] values) {
  return(17i32)
}

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 4i32)}
  [auto] inferred{/map/count(values)}
  return(inferred)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /map/count") != std::string::npos);
}

TEST_CASE("map compatibility contains call rejects visible canonical definition") {
  const std::string source = R"(
[effects(heap_alloc), return<bool>]
/std/collections/map/contains([map<i32, i32>] values, [i32] key) {
  return(false)
}

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 4i32)}
  [bool] found{/map/contains(values, 1i32)}
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /map/contains") != std::string::npos);
}

TEST_CASE("map compatibility contains call keeps explicit alias precedence") {
  const std::string source = R"(
[effects(heap_alloc), return<bool>]
/map/contains([map<i32, i32>] values, [i32] key) {
  return(false)
}

[effects(heap_alloc), return<bool>]
/std/collections/map/contains([map<i32, i32>] values, [bool] key) {
  return(true)
}

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 4i32)}
  [bool] found{/map/contains(values, 1i32)}
  return(0i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("map compatibility tryAt call rejects visible canonical definition") {
  const std::string source = R"(
[effects(heap_alloc), return<Result<i32, ContainerError>>]
/std/collections/map/tryAt([map<i32, i32>] values, [i32] key) {
  return(Result.ok(17i32))
}

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 4i32)}
  [Result<i32, ContainerError>] found{/map/tryAt(values, 1i32)}
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /map/tryAt") != std::string::npos);
}

TEST_CASE("map compatibility tryAt call keeps explicit alias precedence") {
  const std::string source = R"(
[effects(heap_alloc), return<Result<i32, ContainerError>>]
/map/tryAt([map<i32, i32>] values, [i32] key) {
  return(Result.ok(19i32))
}

[effects(heap_alloc), return<Result<i32, ContainerError>>]
/std/collections/map/tryAt([map<i32, i32>] values, [bool] key) {
  return(Result.ok(7i32))
}

[effects(heap_alloc), return<Result<i32, ContainerError>>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 4i32)}
  return(/map/tryAt(values, 1i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("map wrapper temporary tryAt call validates canonical target classification") {
  const std::string source = R"(
[struct]
ContainerError() {
  [i32] code{0i32}
}

namespace ContainerError {
  [return<string>]
  why([ContainerError] err) {
    return("container error"utf8)
  }
}

[effects(io_err)]
unexpectedMapWrapperTryAtError([ContainerError] err) {}

[effects(heap_alloc), return<map<i32, i32>>]
wrapMap() {
  return(map<i32, i32>(1i32, 4i32))
}

[effects(heap_alloc), return<Result<i32, ContainerError>>]
/std/collections/map/tryAt([map<i32, i32>] values, [i32] key) {
  return(Result.ok(17i32))
}

[return<Result<int, ContainerError>> effects(io_out, heap_alloc) on_error<ContainerError, /unexpectedMapWrapperTryAtError>]
main() {
  return(Result.ok(try(/std/collections/map/tryAt(wrapMap(), 1i32))))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("map wrapper temporary tryAt auto inference accepts explicit helper binding as try input") {
  const std::string source = R"(
[struct]
ContainerError() {
  [i32] code{0i32}
}

namespace ContainerError {
  [return<string>]
  why([ContainerError] err) {
    return("container error"utf8)
  }
}

[effects(io_err)]
unexpectedMapWrapperTryAtError([ContainerError] err) {}

[effects(heap_alloc), return<map<i32, i32>>]
wrapMap() {
  return(map<i32, i32>(1i32, 4i32))
}

[effects(heap_alloc), return<Result<i32, ContainerError>>]
/std/collections/map/tryAt([map<i32, i32>] values, [i32] key) {
  return(Result.ok(17i32))
}

[return<Result<int, ContainerError>> effects(io_out, heap_alloc) on_error<ContainerError, /unexpectedMapWrapperTryAtError>]
main() {
  [auto] inferred{/std/collections/map/tryAt(wrapMap(), 1i32)}
  return(Result.ok(try(inferred)))
}
  )";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("map wrapper temporary tryAt call requires canonical helper definition") {
  const std::string source = R"(
[struct]
ContainerError() {
  [i32] code{0i32}
}

namespace ContainerError {
  [return<string>]
  why([ContainerError] err) {
    return("container error"utf8)
  }
}

[effects(io_err)]
unexpectedMapWrapperTryAtError([ContainerError] err) {}

[effects(heap_alloc), return<map<i32, i32>>]
wrapMap() {
  return(map<i32, i32>(1i32, 4i32))
}

[return<Result<int, ContainerError>> effects(io_out, heap_alloc) on_error<ContainerError, /unexpectedMapWrapperTryAtError>]
main() {
  return(Result.ok(try(/std/collections/map/tryAt(wrapMap(), 1i32))))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /std/collections/map/tryAt") != std::string::npos);
}

TEST_SUITE_END();
