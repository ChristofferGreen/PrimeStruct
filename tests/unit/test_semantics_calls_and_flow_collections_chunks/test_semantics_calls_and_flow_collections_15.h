TEST_CASE("field-bound experimental map compatibility count calls remain accepted") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_map/*

Holder() {
  [Map<string, i32>] values{mapPair<string, i32>("left"raw_utf8, 4i32, "right"raw_utf8, 7i32)}
}

[effects(heap_alloc), return<int>]
main() {
  [Holder] holder{Holder()}
  return(/map/count(holder.values))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("field-bound experimental map stdlib namespaced methods keep removed-path diagnostics") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_map/*

Holder() {
  [Map<string, i32>] values{mapPair<string, i32>("left"raw_utf8, 4i32, "right"raw_utf8, 7i32)}
}

[effects(heap_alloc), return<int>]
main() {
  [Holder] holder{Holder()}
  return(holder.values./std/collections/map/count())
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown method: /map/count") != std::string::npos);
}

TEST_CASE("stdlib map constructor assignments accept explicit experimental map struct fields") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_map/*

Holder() {
  [Map<string, i32> mut] primary{mapNew<string, i32>()}
  [Map<string, i32> mut] secondary{mapNew<string, i32>()}
}

[effects(heap_alloc), return<int>]
main() {
  [Holder mut] holder{Holder()}
  assign(holder.primary, /std/collections/map/map("left"raw_utf8, 4i32, "right"raw_utf8, 7i32))
  assign(holder.secondary, /std/collections/mapPair("extra"raw_utf8, 9i32, "other"raw_utf8, 2i32))
  return(plus(/std/collections/map/at(holder.primary, "left"raw_utf8),
              /std/collections/map/at(holder.secondary, "extra"raw_utf8)))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("stdlib map constructor assignments keep mismatch diagnostics on experimental map struct fields") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_map/*

Holder() {
  [Map<string, i32> mut] values{mapNew<string, i32>()}
}

[effects(heap_alloc), return<int>]
main() {
  [Holder mut] holder{Holder()}
  assign(holder.values, /std/collections/mapPair("left"raw_utf8, 4i32, "wrong"raw_utf8, false))
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("implicit template arguments conflict on /std/collections/mapPair") != std::string::npos);
}

TEST_CASE("helper-wrapped map constructor assignments accept inferred experimental map struct fields") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_map/*

[return<T> effects(heap_alloc)]
wrapValues<T>([T] values) {
  return(values)
}

Holder() {
  values{wrapValues(mapNew<string, i32>())}
}

[effects(heap_alloc), return<int>]
main() {
  [Holder mut] holder{Holder()}
  assign(holder.values, wrapValues(/std/collections/mapPair("left"raw_utf8, 4i32, "right"raw_utf8, 7i32)))
  return(/std/collections/map/at(holder.values, "left"raw_utf8))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("helper-wrapped map constructor assignments keep mismatch diagnostics on inferred experimental map struct fields") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_map/*

[return<T> effects(heap_alloc)]
wrapValues<T>([T] values) {
  return(values)
}

Holder() {
  values{wrapValues(mapNew<string, i32>())}
}

[effects(heap_alloc), return<int>]
main() {
  [Holder mut] holder{Holder()}
  assign(holder.values, wrapValues(/std/collections/mapPair("left"raw_utf8, 4i32, "wrong"raw_utf8, false)))
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("implicit template arguments conflict on /std/collections/mapPair") != std::string::npos);
}

TEST_CASE("helper-wrapped Result.ok payload assignments accept explicit experimental map result struct fields") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_map/*

[return<T> effects(heap_alloc)]
wrapStatus<T>([T] status) {
  return(status)
}

Holder() {
  [Result<Map<string, i32>, ContainerError> mut] status{Result.ok(mapNew<string, i32>())}
}

[effects(io_err)]
unexpectedWrappedExperimentalResultMapFieldAssignError([ContainerError] err) {
  [Result<ContainerError>] current{err.code}
  print_line_error(Result.why(current))
}

[return<Result<int, ContainerError>> effects(io_out, heap_alloc)
 on_error<ContainerError, /unexpectedWrappedExperimentalResultMapFieldAssignError>]
main() {
  [Holder mut] holder{Holder()}
  assign(holder.status, wrapStatus(Result.ok(/std/collections/mapPair("left"raw_utf8, 4i32,
                                                                     "right"raw_utf8, 7i32))))
  [Map<string, i32>] values{try(holder.status)}
  return(Result.ok(plus(/std/collections/map/count(values),
                        /std/collections/map/at(values, "left"raw_utf8))))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.empty());
}

TEST_CASE("helper-wrapped Result.ok payload assignments keep mismatch diagnostics on explicit experimental map result struct fields") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_map/*

[return<T> effects(heap_alloc)]
wrapStatus<T>([T] status) {
  return(status)
}

Holder() {
  [Result<Map<string, i32>, ContainerError> mut] status{Result.ok(mapNew<string, i32>())}
}

[effects(io_err)]
unexpectedWrappedExperimentalResultMapFieldAssignError([ContainerError] err) {
  [Result<ContainerError>] current{err.code}
  print_line_error(Result.why(current))
}

[return<Result<int, ContainerError>> effects(io_out, heap_alloc)
 on_error<ContainerError, /unexpectedWrappedExperimentalResultMapFieldAssignError>]
main() {
  [Holder mut] holder{Holder()}
  assign(holder.status, wrapStatus(Result.ok(/std/collections/mapPair("left"raw_utf8, 4i32,
                                                                     "wrong"raw_utf8, false))))
  return(Result.ok(0i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.find("implicit template arguments conflict on /std/collections/mapPair") != std::string::npos);
}

TEST_CASE("helper-wrapped map constructor assignments accept dereferenced experimental map struct fields") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_map/*

[return<T> effects(heap_alloc)]
wrapValues<T>([T] values) {
  return(values)
}

Holder() {
  [Map<string, i32> mut] values{mapNew<string, i32>()}
}

[return<Reference<Holder>>]
borrowHolder([Reference<Holder>] holder) {
  return(holder)
}

[effects(heap_alloc), return<int>]
main() {
  [Holder mut] holder{Holder()}
  assign(dereference(borrowHolder(location(holder))).values,
         wrapValues(/std/collections/mapPair("left"raw_utf8, 4i32,
                                             "right"raw_utf8, 7i32)))
  return(/std/collections/map/at(holder.values, "left"raw_utf8))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.empty());
}

TEST_CASE("helper-wrapped dereferenced map field assignments keep mismatch diagnostics") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_map/*

[return<T> effects(heap_alloc)]
wrapValues<T>([T] values) {
  return(values)
}

Holder() {
  [Map<string, i32> mut] values{mapNew<string, i32>()}
}

[return<Reference<Holder>>]
borrowHolder([Reference<Holder>] holder) {
  return(holder)
}

[effects(heap_alloc), return<int>]
main() {
  [Holder mut] holder{Holder()}
  assign(dereference(borrowHolder(location(holder))).values,
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

TEST_CASE("helper-wrapped Result.ok assignments accept dereferenced experimental result map struct fields") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_map/*

[return<T> effects(heap_alloc)]
wrapStatus<T>([T] status) {
  return(status)
}

Holder() {
  [Result<Map<string, i32>, ContainerError> mut] status{Result.ok(mapNew<string, i32>())}
}

[return<Reference<Holder>>]
borrowHolder([Reference<Holder>] holder) {
  return(holder)
}

[effects(io_err)]
unexpectedWrappedExperimentalResultMapDerefFieldAssignError([ContainerError] err) {
  [Result<ContainerError>] current{err.code}
  print_line_error(Result.why(current))
}

[return<Result<int, ContainerError>> effects(io_out, heap_alloc)
 on_error<ContainerError, /unexpectedWrappedExperimentalResultMapDerefFieldAssignError>]
main() {
  [Holder mut] holder{Holder()}
  assign(dereference(borrowHolder(location(holder))).status,
         wrapStatus(Result.ok(/std/collections/mapPair("left"raw_utf8, 4i32,
                                                      "right"raw_utf8, 7i32))))
  [Map<string, i32>] values{try(holder.status)}
  return(Result.ok(plus(/std/collections/map/count(values),
                        /std/collections/map/at(values, "left"raw_utf8))))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.empty());
}

TEST_CASE("helper-wrapped dereferenced Result.ok field assignments keep mismatch diagnostics") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_map/*

[return<T> effects(heap_alloc)]
wrapStatus<T>([T] status) {
  return(status)
}

Holder() {
  [Result<Map<string, i32>, ContainerError> mut] status{Result.ok(mapNew<string, i32>())}
}

[return<Reference<Holder>>]
borrowHolder([Reference<Holder>] holder) {
  return(holder)
}

[effects(io_err)]
unexpectedWrappedExperimentalResultMapDerefFieldAssignError([ContainerError] err) {
  [Result<ContainerError>] current{err.code}
  print_line_error(Result.why(current))
}

[return<Result<int, ContainerError>> effects(io_out, heap_alloc)
 on_error<ContainerError, /unexpectedWrappedExperimentalResultMapDerefFieldAssignError>]
main() {
  [Holder mut] holder{Holder()}
  assign(dereference(borrowHolder(location(holder))).status,
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

TEST_CASE("stdlib namespaced map helpers keep Comparable diagnostics on experimental map value receivers") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_map/*

[struct]
Key() {
  [i32] value{0i32}
}

[effects(heap_alloc), return<int>]
main() {
  [Map<Key, i32>] values{mapNew<Key, i32>()}
  if(/std/collections/map/contains(values, Key(1i32)),
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

TEST_CASE("map compatibility contains call requires explicit alias definition") {
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

TEST_CASE("map compatibility tryAt call requires explicit alias definition") {
  const std::string source = R"(
[effects(heap_alloc), return<Result<i32, ContainerError>>]
/std/collections/map/tryAt([map<i32, i32>] values, [i32] key) {
  return(Result.ok(17i32))
}

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 4i32)}
  return(try(/map/tryAt(values, 1i32)))
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
