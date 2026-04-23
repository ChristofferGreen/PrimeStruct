#include "test_semantics_helpers.h"

TEST_SUITE_BEGIN("primestruct.semantics.calls_flow.collections");

namespace {

void checkMapPairTemplateConflict(const std::string &error) {
  CHECK(error.find("implicit template arguments conflict on ") !=
        std::string::npos);
  CHECK(error.find("mapPair") != std::string::npos);
}

} // namespace

TEST_CASE("stdlib map constructors accept experimental map method-call parameters") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_map/*

Holder() {}

[return<int> effects(heap_alloc)]
/Holder/score([Holder] self, [Map<string, i32>] values) {
  return(plus(/std/collections/map/count(values),
              /std/collections/map/at_unsafe(values, "left"raw_utf8)))
}

[effects(heap_alloc), return<int>]
main() {
  [Holder] holder{Holder()}
  return(plus(holder.score(/std/collections/map/map("left"raw_utf8, 4i32, "right"raw_utf8, 7i32)),
              holder.score(/std/collections/mapPair("left"raw_utf8, 2i32, "extra"raw_utf8, 9i32))))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("stdlib map constructors keep mismatch diagnostics on experimental map method-call parameters") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_map/*

Holder() {}

[return<int> effects(heap_alloc)]
/Holder/score([Holder] self, [Map<string, i32>] values) {
  return(/std/collections/map/count(values))
}

[effects(heap_alloc), return<int>]
main() {
  [Holder] holder{Holder()}
  return(holder.score(/std/collections/mapPair("left"raw_utf8, 4i32, "wrong"raw_utf8, false)))
}
  )";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  checkMapPairTemplateConflict(error);
}

TEST_CASE("stdlib map constructors accept inferred experimental map default parameters") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_map/*

Holder() {}

[return<int> effects(heap_alloc)]
scoreValues([auto mut] values{mapNew<string, i32>()}) {
  mapInsert<string, i32>(values, "extra"raw_utf8, 9i32)
  return(plus(/std/collections/map/count(values),
              /std/collections/map/at(values, "left"raw_utf8)))
}

[return<int> effects(heap_alloc)]
/Holder/score([Holder] self, [auto mut] values{mapNew<string, i32>()}) {
  mapInsert<string, i32>(values, "bonus"raw_utf8, 5i32)
  return(plus(/std/collections/map/count(values),
              /std/collections/map/at(values, "extra"raw_utf8)))
}

[effects(heap_alloc), return<int>]
main() {
  [Holder] holder{Holder()}
  return(plus(scoreValues(/std/collections/map/map("left"raw_utf8, 4i32, "right"raw_utf8, 7i32)),
              holder.score(/std/collections/mapPair("left"raw_utf8, 2i32, "extra"raw_utf8, 9i32))))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("stdlib map constructors keep mismatch diagnostics on inferred experimental map default parameters") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_map/*

[return<int> effects(heap_alloc)]
scoreValues([auto mut] values{mapNew<string, i32>()}) {
  mapInsert<string, i32>(values, "extra"raw_utf8, 9i32)
  return(/std/collections/map/count(values))
}

[effects(heap_alloc), return<int>]
main() {
  return(scoreValues(/std/collections/mapPair("left"raw_utf8, 4i32, "wrong"raw_utf8, false)))
}
  )";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  checkMapPairTemplateConflict(error);
}

TEST_CASE("helper-wrapped map constructors accept inferred experimental map default parameters") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_map/*

Holder() {}

[return<T> effects(heap_alloc)]
wrapValues<T>([T] values) {
  return(values)
}

[return<int> effects(heap_alloc)]
scoreValues([auto mut] values{mapNew<string, i32>()}) {
  mapInsert<string, i32>(values, "extra"raw_utf8, 9i32)
  return(plus(/std/collections/map/count(values),
              /std/collections/map/at(values, "left"raw_utf8)))
}

[return<int> effects(heap_alloc)]
/Holder/score([Holder] self, [auto mut] values{mapNew<string, i32>()}) {
  mapInsert<string, i32>(values, "bonus"raw_utf8, 5i32)
  return(plus(/std/collections/map/count(values),
              /std/collections/map/at(values, "extra"raw_utf8)))
}

[effects(heap_alloc), return<int>]
main() {
  [Holder] holder{Holder()}
  return(plus(scoreValues(wrapValues(/std/collections/map/map("left"raw_utf8, 4i32, "right"raw_utf8, 7i32))),
              holder.score(wrapValues(/std/collections/mapPair("left"raw_utf8, 2i32, "extra"raw_utf8, 9i32)))))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("helper-wrapped map constructors keep mismatch diagnostics on inferred experimental map default parameters") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_map/*

[return<T> effects(heap_alloc)]
wrapValues<T>([T] values) {
  return(values)
}

[return<int> effects(heap_alloc)]
scoreValues([auto mut] values{mapNew<string, i32>()}) {
  mapInsert<string, i32>(values, "extra"raw_utf8, 9i32)
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
  checkMapPairTemplateConflict(error);
}

TEST_CASE("helper-wrapped inferred experimental map default parameters rewrite constructors") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_map/*

Holder() {}

[return<T> effects(heap_alloc)]
wrapValues<T>([T] values) {
  return(values)
}

[return<int> effects(heap_alloc)]
scoreValues([auto mut] values{wrapValues(mapNew<string, i32>())}) {
  mapInsert<string, i32>(values, "extra"raw_utf8, 9i32)
  return(plus(/std/collections/map/count(values),
              /std/collections/map/at(values, "left"raw_utf8)))
}

[return<int> effects(heap_alloc)]
/Holder/score([Holder] self, [auto mut] values{wrapValues(mapNew<string, i32>())}) {
  mapInsert<string, i32>(values, "bonus"raw_utf8, 5i32)
  return(plus(/std/collections/map/count(values),
              /std/collections/map/at(values, "extra"raw_utf8)))
}

[effects(heap_alloc), return<int>]
main() {
  [Holder] holder{Holder()}
  return(plus(scoreValues(/std/collections/map/map("left"raw_utf8, 4i32, "right"raw_utf8, 7i32)),
              holder.score(/std/collections/mapPair("left"raw_utf8, 2i32, "extra"raw_utf8, 9i32))))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("helper-wrapped inferred experimental map default parameters keep mismatch diagnostics") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_map/*

[return<T> effects(heap_alloc)]
wrapValues<T>([T] values) {
  return(values)
}

[return<int> effects(heap_alloc)]
scoreValues([auto mut] values{wrapValues(mapNew<string, i32>())}) {
  mapInsert<string, i32>(values, "extra"raw_utf8, 9i32)
  return(/std/collections/map/count(values))
}

[effects(heap_alloc), return<int>]
main() {
  return(scoreValues(/std/collections/mapPair("left"raw_utf8, 4i32, "wrong"raw_utf8, false)))
}
  )";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  checkMapPairTemplateConflict(error);
}

TEST_CASE("canonical map helpers accept direct experimental constructor receivers") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_map/*

[effects(io_err)]
unexpectedExperimentalMapHelperReceiverError([ContainerError] err) {
  [Result<ContainerError>] status{err.code}
  print_line_error(Result.why(status))
}

[return<Result<int, ContainerError>> effects(io_out, heap_alloc) on_error<ContainerError, /unexpectedExperimentalMapHelperReceiverError>]
main() {
  [i32] found{try(/std/collections/map/tryAt(/std/collections/map/map("left"raw_utf8, 4i32, "right"raw_utf8, 7i32),
                                            "left"raw_utf8))}
  [i32 mut] total{plus(/std/collections/map/count(/std/collections/mapPair("left"raw_utf8, 4i32, "right"raw_utf8, 7i32)),
                          found)}
  assign(total, plus(total, /std/collections/map/at(/std/collections/mapPair("extra"raw_utf8, 9i32, "other"raw_utf8, 2i32),
                                                    "extra"raw_utf8)))
  assign(total, plus(total, /std/collections/map/at_unsafe(/std/collections/mapPair("bonus"raw_utf8, 5i32, "keep"raw_utf8, 1i32),
                                                           "bonus"raw_utf8)))
  if(/std/collections/map/contains(/std/collections/mapPair("left"raw_utf8, 4i32, "right"raw_utf8, 7i32),
                                   "right"raw_utf8),
     then() { assign(total, plus(total, 1i32)) },
     else() { })
  return(Result.ok(total))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("canonical map helpers keep mismatch diagnostics on direct constructor receivers") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_map/*

[effects(heap_alloc), return<int>]
main() {
  return(/std/collections/map/count(/std/collections/mapPair("left"raw_utf8, 4i32, "wrong"raw_utf8, false)))
}
  )";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  checkMapPairTemplateConflict(error);
}

TEST_CASE("helper-wrapped inferred experimental result map default parameters rewrite constructors") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_map/*

[return<T> effects(heap_alloc)]
wrapStatus<T>([T] status) {
  return(status)
}

[effects(heap_alloc), return<int>]
scoreStatus([auto] status{wrapStatus(Result.ok(mapNew<string, i32>()))}) {
  return(0i32)
}

[effects(heap_alloc), return<int>]
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

TEST_CASE("helper-wrapped inferred experimental result map default parameters keep mismatch diagnostics") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_map/*

[return<T> effects(heap_alloc)]
wrapStatus<T>([T] status) {
  return(status)
}

[effects(heap_alloc), return<int>]
scoreStatus([auto] status{wrapStatus(Result.ok(mapNew<string, i32>()))}) {
  return(0i32)
}

[effects(heap_alloc), return<int>]
main() {
  return(scoreStatus(wrapStatus(Result.ok(/std/collections/mapPair("left"raw_utf8, 4i32,
                                                                  "wrong"raw_utf8, false)))))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  INFO(error);
  checkMapPairTemplateConflict(error);
}

TEST_CASE("helper-wrapped canonical map helpers accept direct experimental constructor receivers") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_map/*

[return<T> effects(heap_alloc)]
wrapValues<T>([T] values) {
  return(values)
}

[effects(io_err)]
unexpectedWrappedExperimentalMapHelperReceiverError([ContainerError] err) {
  [Result<ContainerError>] status{err.code}
  print_line_error(Result.why(status))
}

[return<Result<int, ContainerError>> effects(io_out, heap_alloc)
 on_error<ContainerError, /unexpectedWrappedExperimentalMapHelperReceiverError>]
main() {
  [i32] found{try(/std/collections/map/tryAt(wrapValues(/std/collections/map/map("left"raw_utf8, 4i32,
                                                                                  "right"raw_utf8, 7i32)),
                                            "left"raw_utf8))}
  [i32 mut] total{plus(/std/collections/map/count(wrapValues(/std/collections/mapPair("left"raw_utf8, 4i32,
                                                                                      "right"raw_utf8, 7i32))),
                          found)}
  assign(total, plus(total, /std/collections/map/at(wrapValues(/std/collections/mapPair("extra"raw_utf8, 9i32,
                                                                                        "other"raw_utf8, 2i32)),
                                                    "extra"raw_utf8)))
  assign(total, plus(total, /std/collections/map/at_unsafe(wrapValues(/std/collections/mapPair("bonus"raw_utf8, 5i32,
                                                                                               "keep"raw_utf8, 1i32)),
                                                           "bonus"raw_utf8)))
  if(/std/collections/map/contains(wrapValues(/std/collections/mapPair("left"raw_utf8, 4i32,
                                                                       "right"raw_utf8, 7i32)),
                                   "right"raw_utf8),
     then() { assign(total, plus(total, 1i32)) },
     else() { })
  return(Result.ok(total))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("helper-wrapped canonical map helpers keep mismatch diagnostics on direct constructor receivers") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_map/*

[return<T> effects(heap_alloc)]
wrapValues<T>([T] values) {
  return(values)
}

[effects(heap_alloc), return<int>]
main() {
  return(/std/collections/map/count(wrapValues(/std/collections/mapPair("left"raw_utf8, 4i32,
                                                                        "wrong"raw_utf8, false))))
}
  )";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  checkMapPairTemplateConflict(error);
}

TEST_CASE("experimental map methods accept direct constructor receivers") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_map/*

[effects(io_err)]
unexpectedExperimentalMapMethodReceiverError([ContainerError] err) {
  [Result<ContainerError>] status{err.code}
  print_line_error(Result.why(status))
}

[return<Result<int, ContainerError>> effects(io_out, heap_alloc) on_error<ContainerError, /unexpectedExperimentalMapMethodReceiverError>]
main() {
  [i32] found{try(/std/collections/map/map("left"raw_utf8, 4i32, "right"raw_utf8, 7i32).tryAt("left"raw_utf8))}
  [i32 mut] total{plus(/std/collections/mapPair("left"raw_utf8, 4i32, "right"raw_utf8, 7i32).count(), found)}
  assign(total, plus(total, /std/collections/mapPair("extra"raw_utf8, 9i32, "other"raw_utf8, 2i32).at("extra"raw_utf8)))
  assign(total, plus(total, /std/collections/mapPair("bonus"raw_utf8, 5i32, "keep"raw_utf8, 1i32).at_unsafe("bonus"raw_utf8)))
  if(/std/collections/mapPair("left"raw_utf8, 4i32, "right"raw_utf8, 7i32).contains("right"raw_utf8),
     then() { assign(total, plus(total, 1i32)) },
     else() { })
  return(Result.ok(total))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("experimental map methods keep mismatch diagnostics on direct constructor receivers") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_map/*

[effects(heap_alloc), return<int>]
main() {
  return(/std/collections/mapPair("left"raw_utf8, 4i32, "wrong"raw_utf8, false).count())
}
  )";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  checkMapPairTemplateConflict(error);
}

TEST_CASE("helper-wrapped experimental map helpers accept direct constructor receivers") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_map/*

[return<T> effects(heap_alloc)]
wrapValues<T>([T] values) {
  return(values)
}

[effects(io_err)]
unexpectedWrappedExperimentalMapMethodReceiverError([ContainerError] err) {
  [Result<ContainerError>] status{err.code}
  print_line_error(Result.why(status))
}

[return<Result<int, ContainerError>> effects(io_out, heap_alloc)
 on_error<ContainerError, /unexpectedWrappedExperimentalMapMethodReceiverError>]
main() {
  [i32] found{try(/std/collections/map/tryAt<string, i32>(
      wrapValues(/std/collections/map/map("left"raw_utf8, 4i32, "right"raw_utf8, 7i32)),
      "left"raw_utf8))}
  [i32 mut] total{plus(/std/collections/map/count<string, i32>(
                           wrapValues(/std/collections/mapPair("left"raw_utf8, 4i32, "right"raw_utf8, 7i32))),
                       found)}
  assign(total, plus(total, /std/collections/map/at<string, i32>(
                                wrapValues(/std/collections/mapPair("extra"raw_utf8, 9i32,
                                                                    "other"raw_utf8, 2i32)),
                                "extra"raw_utf8)))
  if(/std/collections/map/contains<string, i32>(
         wrapValues(/std/collections/mapPair("left"raw_utf8, 4i32, "right"raw_utf8, 7i32)),
         "right"raw_utf8),
     then() { assign(total, plus(total, 1i32)) },
     else() { })
  return(Result.ok(total))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("helper-wrapped experimental map methods keep mismatch diagnostics on direct constructor receivers") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_map/*

[return<T> effects(heap_alloc)]
wrapValues<T>([T] values) {
  return(values)
}

[effects(heap_alloc), return<int>]
main() {
  return(wrapValues(/std/collections/mapPair("left"raw_utf8, 4i32, "wrong"raw_utf8, false)).count())
}
  )";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  checkMapPairTemplateConflict(error);
}

TEST_CASE("field-bound experimental map helpers accept struct field receivers") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_map/*

[effects(io_err)]
unexpectedFieldExperimentalMapMethodReceiverError([ContainerError] err) {
  [Result<ContainerError>] status{err.code}
  print_line_error(Result.why(status))
}

Holder() {
  [Map<string, i32>] values{mapPair<string, i32>("left"raw_utf8, 4i32, "right"raw_utf8, 7i32)}
}

[return<Result<int, ContainerError>> effects(io_out, heap_alloc)
 on_error<ContainerError, /unexpectedFieldExperimentalMapMethodReceiverError>]
main() {
  [Holder] holder{Holder()}
  [i32] found{try(/std/collections/map/tryAt<string, i32>(holder.values, "left"raw_utf8))}
  [i32 mut] total{plus(/std/collections/map/count<string, i32>(holder.values), found)}
  assign(total, plus(total, /std/collections/map/at<string, i32>(holder.values, "right"raw_utf8)))
  return(Result.ok(total))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("field-bound experimental map wrapper helpers accept struct field receivers") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_map/*

Holder() {
  [Map<string, i32>] values{mapPair<string, i32>("left"raw_utf8, 4i32, "right"raw_utf8, 7i32)}
}

[effects(heap_alloc), return<int>]
main() {
  [Holder] holder{Holder()}
  return(plus(/std/collections/mapCount(holder.values),
              /std/collections/mapAt(holder.values, "left"raw_utf8)))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("field-bound experimental map bare count fallback validates") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_map/*

Holder() {
  [Map<string, i32>] values{mapPair<string, i32>("left"raw_utf8, 4i32, "right"raw_utf8, 7i32)}
}

[effects(heap_alloc), return<int>]
main() {
  [Holder] holder{Holder()}
  return(count(holder.values))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("field-bound experimental map explicit at body arguments validate") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_map/*

Holder() {
  [Map<string, i32>] values{mapPair<string, i32>("left"raw_utf8, 4i32, "right"raw_utf8, 7i32)}
}

[effects(heap_alloc), return<int>]
main() {
  [Holder] holder{Holder()}
  /std/collections/map/at<string, i32>(holder.values, "left"raw_utf8) { 1i32 }
  return(0i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("field-bound experimental map explicit at expression body arguments validate") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_map/*

Holder() {
  [Map<string, i32>] values{mapPair<string, i32>("left"raw_utf8, 4i32, "right"raw_utf8, 7i32)}
}

[effects(heap_alloc), return<int>]
main() {
  [Holder] holder{Holder()}
  return(/std/collections/map/at<string, i32>(holder.values, "left"raw_utf8) { 1i32 })
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("field-bound experimental map explicit at expression body arguments keep mismatch diagnostics") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_map/*

Holder() {
  [Map<string, i32>] values{mapPair<string, i32>("left"raw_utf8, 4i32, "right"raw_utf8, 7i32)}
}

[effects(heap_alloc), return<int>]
main() {
  [Holder] holder{Holder()}
  return(/std/collections/map/at<string, i32>(holder.values) { 1i32 })
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("argument count mismatch for /std/collections/experimental_map/mapAt") != std::string::npos);
}

TEST_SUITE_END();
