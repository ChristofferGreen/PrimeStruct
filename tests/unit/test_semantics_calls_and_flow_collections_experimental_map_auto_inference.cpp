#include "test_semantics_helpers.h"

TEST_SUITE_BEGIN("primestruct.semantics.calls_flow.collections");

namespace {

void checkImplicitMapConflict(const std::string& error) {
  CHECK(error.find("implicit template arguments conflict") != std::string::npos);
  CHECK(error.find("map") != std::string::npos);
}

void checkImplicitMapPairConflict(const std::string& error) {
  CHECK(error.find("implicit template arguments conflict") != std::string::npos);
  CHECK(error.find("mapPair") != std::string::npos);
}

} // namespace

TEST_CASE("helper-wrapped map constructors infer experimental auto locals") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_map/*

[return<T> effects(heap_alloc)]
wrapValues<T>([T] values) {
  return(values)
}

[effects(io_err)]
unexpectedWrappedExperimentalMapAutoError([ContainerError] err) {
  [Result<ContainerError>] status{err.code}
  print_line_error(Result.why(status))
}

[return<Result<int, ContainerError>> effects(io_out, heap_alloc)
 on_error<ContainerError, /unexpectedWrappedExperimentalMapAutoError>]
main() {
  [auto mut] values{wrapValues(/std/collections/map/map("seed"raw_utf8, 1i32))}
  mapInsert<string, i32>(values, "left"raw_utf8, 4i32)
  mapInsert<string, i32>(values, "right"raw_utf8, 7i32)
  [i32] left{try(/std/collections/map/tryAt(values, "left"raw_utf8))}
  [i32] right{try(/std/collections/map/tryAt(values, "right"raw_utf8))}
  return(Result.ok(plus(/std/collections/map/count(values), plus(left, right))))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.empty());
}

TEST_CASE("helper-wrapped map constructor auto inference keeps template conflict diagnostics") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_map/*

[return<T> effects(heap_alloc)]
wrapValues<T>([T] values) {
  return(values)
}

[effects(heap_alloc), return<int>]
main() {
  [auto] values{wrapValues(/std/collections/map/map("left"raw_utf8, 4i32, "right"raw_utf8, false))}
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  INFO(error);
  checkImplicitMapConflict(error);
}

TEST_CASE("implicit map constructor auto inference keeps template conflict diagnostics") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_map/*

[effects(heap_alloc), return<int>]
main() {
  [auto] values{/std/collections/map/map("left"raw_utf8, 4i32, "right"raw_utf8, false)}
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  checkImplicitMapConflict(error);
}

TEST_CASE("inferred experimental map returns rewrite canonical constructors") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_map/*

[return<auto> effects(heap_alloc)]
buildValues() {
  return(/std/collections/map/map("left"raw_utf8, 4i32, "right"raw_utf8, 7i32))
}

[effects(io_err)]
unexpectedInferredExperimentalMapReturnError([ContainerError] err) {
  [Result<ContainerError>] status{err.code}
  print_line_error(Result.why(status))
}

[return<Result<int, ContainerError>> effects(io_out, heap_alloc) on_error<ContainerError, /unexpectedInferredExperimentalMapReturnError>]
main() {
  [Map<string, i32> mut] values{buildValues()}
  mapInsert<string, i32>(values, "extra"raw_utf8, 9i32)
  [i32] left{try(/std/collections/map/tryAt(values, "left"raw_utf8))}
  [i32] extra{try(/std/collections/map/tryAt(values, "extra"raw_utf8))}
  return(Result.ok(plus(/std/collections/map/count(values), plus(left, extra))))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("inferred experimental map returns keep constructor mismatch diagnostics") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_map/*

[return<auto> effects(heap_alloc)]
buildValues() {
  return(/std/collections/map/map("left"raw_utf8, 4i32, "right"raw_utf8, false))
}

[effects(heap_alloc), return<int>]
main() {
  [Map<string, i32>] values{buildValues()}
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  checkImplicitMapConflict(error);
}

TEST_CASE("block-bodied inferred experimental map returns rewrite constructors") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_map/*

[return<auto> effects(heap_alloc)]
buildValues([bool] useCanonical) {
  if(useCanonical,
     then() { /std/collections/map/map("left"raw_utf8, 4i32, "right"raw_utf8, 7i32) },
     else() { /std/collections/mapPair("left"raw_utf8, 4i32, "other"raw_utf8, 2i32) })
}

[effects(io_err)]
unexpectedBlockExperimentalMapReturnError([ContainerError] err) {
  [Result<ContainerError>] status{err.code}
  print_line_error(Result.why(status))
}

[return<Result<int, ContainerError>> effects(io_out, heap_alloc) on_error<ContainerError, /unexpectedBlockExperimentalMapReturnError>]
main() {
  [Map<string, i32> mut] values{buildValues(true)}
  mapInsert<string, i32>(values, "extra"raw_utf8, 9i32)
  [i32] left{try(/std/collections/map/tryAt(values, "left"raw_utf8))}
  [i32] extra{try(/std/collections/map/tryAt(values, "extra"raw_utf8))}
  return(Result.ok(plus(/std/collections/map/count(values), plus(left, extra))))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("block-bodied inferred experimental map returns keep mismatch diagnostics") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_map/*

[return<auto> effects(heap_alloc)]
buildValues([bool] useCanonical) {
  if(useCanonical,
     then() { /std/collections/map/map("left"raw_utf8, 4i32, "right"raw_utf8, 7i32) },
     else() { /std/collections/mapPair("left"raw_utf8, 4i32, "other"raw_utf8, false) })
}

[effects(heap_alloc), return<int>]
main() {
  [Map<string, i32>] values{buildValues(false)}
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  checkImplicitMapPairConflict(error);
}

TEST_CASE("auto bindings inside inferred experimental map return blocks rewrite constructors") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_map/*

[return<auto> effects(heap_alloc)]
buildValues([bool] useCanonical) {
  if(useCanonical,
     then() {
       [auto mut] values{/std/collections/map/map("left"raw_utf8, 4i32, "right"raw_utf8, 7i32)}
       mapInsert<string, i32>(values, "extra"raw_utf8, 9i32)
       values
     },
     else() {
       [auto mut] values{/std/collections/mapPair("left"raw_utf8, 4i32, "other"raw_utf8, 2i32)}
       values
     })
}

[effects(io_err)]
unexpectedAutoBlockExperimentalMapReturnError([ContainerError] err) {
  [Result<ContainerError>] status{err.code}
  print_line_error(Result.why(status))
}

[return<Result<int, ContainerError>> effects(io_out, heap_alloc) on_error<ContainerError, /unexpectedAutoBlockExperimentalMapReturnError>]
main() {
  [Map<string, i32>] values{buildValues(true)}
  [i32] left{try(/std/collections/map/tryAt(values, "left"raw_utf8))}
  [i32] extra{try(/std/collections/map/tryAt(values, "extra"raw_utf8))}
  return(Result.ok(plus(/std/collections/map/count(values), plus(left, extra))))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("auto bindings inside inferred experimental map return blocks keep mismatch diagnostics") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_map/*

[return<auto> effects(heap_alloc)]
buildValues([bool] useCanonical) {
  if(useCanonical,
     then() {
       [auto] values{/std/collections/map/map("left"raw_utf8, 4i32, "right"raw_utf8, 7i32)}
       values
     },
     else() {
       [auto] values{/std/collections/mapPair("left"raw_utf8, 4i32, "other"raw_utf8, false)}
       values
     })
}

[effects(heap_alloc), return<int>]
main() {
  [Map<string, i32>] values{buildValues(false)}
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  checkImplicitMapPairConflict(error);
}

TEST_CASE("helper-wrapped inferred experimental map returns rewrite nested constructor arguments") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_map/*

[return<T> effects(heap_alloc)]
wrapValues<T>([T] values) {
  return(values)
}

[return<auto> effects(heap_alloc)]
buildValues([bool] useCanonical) {
  if(useCanonical,
     then() { return(wrapValues(/std/collections/map/map("left"raw_utf8, 4i32, "right"raw_utf8, 7i32))) },
     else() { return(wrapValues(/std/collections/mapPair("extra"raw_utf8, 9i32, "bonus"raw_utf8, 5i32))) })
}

[effects(io_err)]
unexpectedWrappedExperimentalMapReturnError([ContainerError] err) {
  [Result<ContainerError>] status{err.code}
  print_line_error(Result.why(status))
}

[return<Result<int, ContainerError>> effects(io_out, heap_alloc) on_error<ContainerError, /unexpectedWrappedExperimentalMapReturnError>]
main() {
  [Map<string, i32>] canonical{buildValues(true)}
  [Map<string, i32>] wrapped{buildValues(false)}
  [i32] left{try(/std/collections/map/tryAt(canonical, "left"raw_utf8))}
  [i32] bonus{try(/std/collections/map/tryAt(wrapped, "bonus"raw_utf8))}
  return(Result.ok(plus(/std/collections/map/count(canonical), plus(left, bonus))))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("helper-wrapped inferred experimental map returns keep nested constructor mismatch diagnostics") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_map/*

[return<T> effects(heap_alloc)]
wrapValues<T>([T] values) {
  return(values)
}

[return<auto> effects(heap_alloc)]
buildValues() {
  return(wrapValues(/std/collections/mapPair("left"raw_utf8, 4i32, "wrong"raw_utf8, false)))
}

[effects(heap_alloc), return<int>]
main() {
  [Map<string, i32>] values{buildValues()}
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  checkImplicitMapPairConflict(error);
}

TEST_CASE("double helper-wrapped inferred experimental map returns rewrite nested constructor arguments") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_map/*

[return<T> effects(heap_alloc)]
wrapOnce<T>([T] values) {
  return(values)
}

[return<T> effects(heap_alloc)]
wrapTwice<T>([T] values) {
  return(wrapOnce(values))
}

[return<auto> effects(heap_alloc)]
buildValues([bool] useCanonical) {
  if(useCanonical,
     then() { return(wrapTwice(/std/collections/map/map("left"raw_utf8, 4i32, "right"raw_utf8, 7i32))) },
     else() { return(wrapTwice(/std/collections/mapPair("extra"raw_utf8, 9i32, "bonus"raw_utf8, 5i32))) })
}

[effects(io_err)]
unexpectedDoubleWrappedExperimentalMapReturnError([ContainerError] err) {
  [Result<ContainerError>] status{err.code}
  print_line_error(Result.why(status))
}

[return<Result<int, ContainerError>> effects(io_out, heap_alloc)
 on_error<ContainerError, /unexpectedDoubleWrappedExperimentalMapReturnError>]
main() {
  [Map<string, i32>] canonical{buildValues(true)}
  [Map<string, i32>] wrapped{buildValues(false)}
  [i32] left{try(/std/collections/map/tryAt(canonical, "left"raw_utf8))}
  [i32] bonus{try(/std/collections/map/tryAt(wrapped, "bonus"raw_utf8))}
  return(Result.ok(plus(/std/collections/map/count(canonical), plus(left, bonus))))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("double helper-wrapped inferred experimental map returns keep nested constructor mismatch diagnostics") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_map/*

[return<T> effects(heap_alloc)]
wrapOnce<T>([T] values) {
  return(values)
}

[return<T> effects(heap_alloc)]
wrapTwice<T>([T] values) {
  return(wrapOnce(values))
}

[return<auto> effects(heap_alloc)]
buildValues() {
  return(wrapTwice(/std/collections/mapPair("left"raw_utf8, 4i32, "wrong"raw_utf8, false)))
}

[effects(heap_alloc), return<int>]
main() {
  [Map<string, i32>] values{buildValues()}
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  checkImplicitMapPairConflict(error);
}

TEST_CASE("inferred experimental map call receivers resolve method sugar") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_map/*

[return<auto> effects(heap_alloc)]
buildValues([bool] useCanonical) {
  if(useCanonical,
     then() { /std/collections/map/map("left"raw_utf8, 4i32, "right"raw_utf8, 7i32) },
     else() { /std/collections/mapPair("left"raw_utf8, 4i32, "other"raw_utf8, 2i32) })
}

[effects(io_err)]
unexpectedInferredExperimentalMapReceiverError([ContainerError] err) {
  [Result<ContainerError>] status{err.code}
  print_line_error(Result.why(status))
}

[return<Result<int, ContainerError>> effects(io_out, heap_alloc) on_error<ContainerError, /unexpectedInferredExperimentalMapReceiverError>]
main() {
  [i32] left{try(buildValues(true).tryAt("left"raw_utf8))}
  [i32 mut] total{plus(buildValues(true).count(), left)}
  if(buildValues(true).contains("right"raw_utf8),
     then() { assign(total, plus(total, 1i32)) },
     else() { })
  return(Result.ok(total))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("inferred experimental map call receivers keep mismatch diagnostics") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_map/*

[return<auto> effects(heap_alloc)]
buildValues([bool] useCanonical) {
  if(useCanonical,
     then() { /std/collections/map/map("left"raw_utf8, 4i32, "right"raw_utf8, 7i32) },
     else() { /std/collections/mapPair("left"raw_utf8, 4i32, "other"raw_utf8, false) })
}

[effects(heap_alloc), return<int>]
main() {
  return(buildValues(false).count())
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  checkImplicitMapPairConflict(error);
}

TEST_CASE("stdlib map constructors accept explicit experimental map struct fields") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_map/*

[struct]
Holder() {
  [Map<string, i32>] primary{mapNew<string, i32>()}
  [Map<string, i32>] secondary{mapNew<string, i32>()}
}

[effects(io_err)]
unexpectedExperimentalMapStructFieldError([ContainerError] err) {
  [Result<ContainerError>] status{err.code}
  print_line_error(Result.why(status))
}

[return<Result<int, ContainerError>> effects(io_out, heap_alloc) on_error<ContainerError, /unexpectedExperimentalMapStructFieldError>]
main() {
  [Holder] holder{Holder(/std/collections/map/map("left"raw_utf8, 4i32, "right"raw_utf8, 7i32),
                        /std/collections/mapPair("other"raw_utf8, 2i32, "extra"raw_utf8, 9i32))}
  [i32] left{try(/std/collections/map/tryAt<string, i32>(holder.primary, "left"raw_utf8))}
  [i32] extra{try(/std/collections/map/tryAt<string, i32>(holder.secondary, "extra"raw_utf8))}
  return(Result.ok(plus(left, extra)))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("stdlib map constructors keep mismatch diagnostics on experimental map struct fields") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_map/*

[struct]
Holder() {
  [Map<string, i32>] values{}
}

[effects(heap_alloc), return<int>]
main() {
  [Holder] holder{Holder(/std/collections/mapPair("left"raw_utf8, 4i32, "wrong"raw_utf8, false))}
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  checkImplicitMapPairConflict(error);
}

TEST_CASE("stdlib map constructors accept inferred experimental map struct fields") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_map/*

[struct]
Holder() {
  primary{mapNew<string, i32>()}
  secondary{mapNew<string, i32>()}
}

[effects(heap_alloc), return<int>]
main() {
  [Holder mut] holder{Holder(/std/collections/map/map("left"raw_utf8, 4i32, "right"raw_utf8, 7i32))}
  assign(holder.secondary, /std/collections/mapPair("extra"raw_utf8, 9i32, "other"raw_utf8, 2i32))
  return(plus(/std/collections/map/at(holder.primary, "left"raw_utf8),
              /std/collections/map/at(holder.secondary, "extra"raw_utf8)))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("stdlib map constructors keep mismatch diagnostics on inferred experimental map struct fields") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_map/*

[struct]
Holder() {
  values{mapNew<string, i32>()}
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
  checkImplicitMapPairConflict(error);
}

TEST_CASE("helper-wrapped inferred experimental map struct fields validate") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_map/*

[return<T> effects(heap_alloc)]
wrapValues<T>([T] values) {
  return(values)
}

[struct]
Holder() {
  primary{wrapValues(mapNew<string, i32>())}
  secondary{wrapValues(mapNew<string, i32>())}
}

[effects(heap_alloc), return<int>]
main() {
  [Holder mut] holder{Holder(/std/collections/map/map("left"raw_utf8, 4i32, "right"raw_utf8, 7i32))}
  assign(holder.secondary, /std/collections/mapPair("extra"raw_utf8, 9i32, "other"raw_utf8, 2i32))
  return(plus(/std/collections/map/at(holder.primary, "left"raw_utf8),
              /std/collections/map/at(holder.secondary, "extra"raw_utf8)))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.empty());
}

TEST_CASE("helper-wrapped inferred experimental map struct fields keep mismatch diagnostics") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_map/*

[return<T> effects(heap_alloc)]
wrapValues<T>([T] values) {
  return(values)
}

[struct]
Holder() {
  values{wrapValues(mapNew<string, i32>())}
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
  INFO(error);
  checkImplicitMapPairConflict(error);
}

TEST_CASE("helper-wrapped inferred experimental result map struct fields validate") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_map/*

[return<T> effects(heap_alloc)]
wrapStatus<T>([T] status) {
  return(status)
}

[struct]
Holder() {
  status{wrapStatus(Result.ok(mapNew<string, i32>()))}
}

[effects(heap_alloc), return<int>]
main() {
  [Holder] holder{Holder(wrapStatus(Result.ok(/std/collections/mapPair("left"raw_utf8, 4i32,
                                                                      "right"raw_utf8, 7i32))))}
  return(0i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.empty());
}

TEST_CASE("helper-wrapped inferred experimental result map struct fields keep mismatch diagnostics") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_map/*

[return<T> effects(heap_alloc)]
wrapStatus<T>([T] status) {
  return(status)
}

[struct]
Holder() {
  status{wrapStatus(Result.ok(mapNew<string, i32>()))}
}

[effects(heap_alloc), return<int>]
main() {
  [Holder] holder{Holder(wrapStatus(Result.ok(/std/collections/mapPair("left"raw_utf8, 4i32,
                                                                      "wrong"raw_utf8, false))))}
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  INFO(error);
  checkImplicitMapPairConflict(error);
}

TEST_SUITE_END();
