#include "test_semantics_helpers.h"

TEST_SUITE_BEGIN("primestruct.semantics.calls_flow.collections");

namespace {

void checkMapPairTemplateConflict(const std::string &error) {
  CHECK((error.find("argument type mismatch") != std::string::npos ||
         error.find("implicit template arguments conflict on ") !=
             std::string::npos ||
         error.find("unable to infer implicit template arguments") !=
             std::string::npos));
  CHECK(error.find("map") != std::string::npos);
}

} // namespace

TEST_CASE("stdlib map constructors accept canonical map method-call parameters") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/internal_map/*

Holder() {}

[return<int> effects(heap_alloc)]
/Holder/score([Holder] self, [map<string, i32>] values) {
  return(plus(/std/collections/map/count(values),
              /std/collections/map/at_unsafe(values, "left"raw_utf8)))
}

[effects(heap_alloc), return<int>]
main() {
  [Holder] holder{Holder()}
  return(plus(holder.score(/std/collections/map/map<string, i32>("left"raw_utf8, 4i32, "right"raw_utf8, 7i32)),
              holder.score(/std/collections/map/map<string, i32>("left"raw_utf8, 2i32, "extra"raw_utf8, 9i32))))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("stdlib map constructors keep mismatch diagnostics on canonical map method-call parameters") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/internal_map/*

Holder() {}

[return<int> effects(heap_alloc)]
/Holder/score([Holder] self, [map<string, i32>] values) {
  return(/std/collections/map/count(values))
}

[effects(heap_alloc), return<int>]
main() {
  [Holder] holder{Holder()}
  return(holder.score(/std/collections/map/map<string, i32>("left"raw_utf8, 4i32, "wrong"raw_utf8, false)))
}
  )";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  checkMapPairTemplateConflict(error);
}

TEST_CASE("stdlib map constructors accept inferred canonical map default parameters") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/internal_map/*

Holder() {}

[return<int> effects(heap_alloc)]
scoreValues([auto mut] values{/std/collections/map/map<string, i32>("seed"raw_utf8, 0i32)}) {
  /std/collections/map/insert<string, i32>(values, "extra"raw_utf8, 9i32)
  return(plus(/std/collections/map/count(values),
              /std/collections/map/at(values, "left"raw_utf8)))
}

[return<int> effects(heap_alloc)]
/Holder/score([Holder] self, [auto mut] values{/std/collections/map/map<string, i32>("seed"raw_utf8, 0i32)}) {
  /std/collections/map/insert<string, i32>(values, "bonus"raw_utf8, 5i32)
  return(plus(/std/collections/map/count(values),
              /std/collections/map/at(values, "extra"raw_utf8)))
}

[effects(heap_alloc), return<int>]
main() {
  [Holder] holder{Holder()}
  return(plus(scoreValues(/std/collections/map/map<string, i32>("left"raw_utf8, 4i32, "right"raw_utf8, 7i32)),
              holder.score(/std/collections/map/map<string, i32>("left"raw_utf8, 2i32, "extra"raw_utf8, 9i32))))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("stdlib map constructors keep mismatch diagnostics on inferred canonical map default parameters") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/internal_map/*

[return<int> effects(heap_alloc)]
scoreValues([auto mut] values{/std/collections/map/map<string, i32>("seed"raw_utf8, 0i32)}) {
  /std/collections/map/insert<string, i32>(values, "extra"raw_utf8, 9i32)
  return(/std/collections/map/count(values))
}

[effects(heap_alloc), return<int>]
main() {
  return(scoreValues(/std/collections/map/map<string, i32>("left"raw_utf8, 4i32, "wrong"raw_utf8, false)))
}
  )";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  checkMapPairTemplateConflict(error);
}

TEST_CASE("helper-wrapped map constructors accept inferred canonical map default parameters") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/internal_map/*

Holder() {}

[return<T> effects(heap_alloc)]
wrapValues<T>([T] values) {
  return(values)
}

[return<int> effects(heap_alloc)]
scoreValues([auto mut] values{/std/collections/map/map<string, i32>("seed"raw_utf8, 0i32)}) {
  /std/collections/map/insert<string, i32>(values, "extra"raw_utf8, 9i32)
  return(plus(/std/collections/map/count(values),
              /std/collections/map/at(values, "left"raw_utf8)))
}

[return<int> effects(heap_alloc)]
/Holder/score([Holder] self, [auto mut] values{/std/collections/map/map<string, i32>("seed"raw_utf8, 0i32)}) {
  /std/collections/map/insert<string, i32>(values, "bonus"raw_utf8, 5i32)
  return(plus(/std/collections/map/count(values),
              /std/collections/map/at(values, "extra"raw_utf8)))
}

[effects(heap_alloc), return<int>]
main() {
  [Holder] holder{Holder()}
  return(plus(scoreValues(wrapValues(/std/collections/map/map<string, i32>("left"raw_utf8, 4i32, "right"raw_utf8, 7i32))),
              holder.score(wrapValues(/std/collections/map/map<string, i32>("left"raw_utf8, 2i32, "extra"raw_utf8, 9i32)))))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("helper-wrapped map constructors keep mismatch diagnostics on inferred canonical map default parameters") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/internal_map/*

[return<T> effects(heap_alloc)]
wrapValues<T>([T] values) {
  return(values)
}

[return<int> effects(heap_alloc)]
scoreValues([auto mut] values{/std/collections/map/map<string, i32>("seed"raw_utf8, 0i32)}) {
  /std/collections/map/insert<string, i32>(values, "extra"raw_utf8, 9i32)
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
  checkMapPairTemplateConflict(error);
}

TEST_CASE("helper-wrapped inferred canonical map default parameters validate") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/internal_map/*

Holder() {}

[return<T> effects(heap_alloc)]
wrapValues<T>([T] values) {
  return(values)
}

[return<int> effects(heap_alloc)]
scoreValues([auto mut] values{wrapValues(/std/collections/map/map<string, i32>("seed"raw_utf8, 0i32))}) {
  /std/collections/map/insert<string, i32>(values, "extra"raw_utf8, 9i32)
  return(plus(/std/collections/map/count(values),
              /std/collections/map/at(values, "left"raw_utf8)))
}

[return<int> effects(heap_alloc)]
/Holder/score([Holder] self, [auto mut] values{wrapValues(/std/collections/map/map<string, i32>("seed"raw_utf8, 0i32))}) {
  /std/collections/map/insert<string, i32>(values, "bonus"raw_utf8, 5i32)
  return(plus(/std/collections/map/count(values),
              /std/collections/map/at(values, "extra"raw_utf8)))
}

[effects(heap_alloc), return<int>]
main() {
  [Holder] holder{Holder()}
  return(plus(scoreValues(/std/collections/map/map<string, i32>("left"raw_utf8, 4i32, "right"raw_utf8, 7i32)),
              holder.score(/std/collections/map/map<string, i32>("left"raw_utf8, 2i32, "extra"raw_utf8, 9i32))))
}
  )";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.empty());
}

TEST_CASE("helper-wrapped inferred canonical map default parameters keep mismatch diagnostics") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/internal_map/*

[return<T> effects(heap_alloc)]
wrapValues<T>([T] values) {
  return(values)
}

[return<int> effects(heap_alloc)]
scoreValues([auto mut] values{wrapValues(/std/collections/map/map<string, i32>("seed"raw_utf8, 0i32))}) {
  /std/collections/map/insert<string, i32>(values, "extra"raw_utf8, 9i32)
  return(/std/collections/map/count(values))
}

[effects(heap_alloc), return<int>]
main() {
  return(scoreValues(/std/collections/map/map<string, i32>("left"raw_utf8, 4i32, "wrong"raw_utf8, false)))
}
  )";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  checkMapPairTemplateConflict(error);
}

TEST_CASE("canonical map helpers accept direct experimental constructor receivers") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/internal_map/*

[effects(io_err)]
unexpectedCanonicalMapHelperReceiverError([ContainerError] err) {
  [Result<ContainerError>] status{err.code}
  print_line_error(Result.why(status))
}

[return<Result<int, ContainerError>> effects(io_out, heap_alloc) on_error<ContainerError, /unexpectedCanonicalMapHelperReceiverError>]
main() {
  [i32] found{try(/std/collections/map/tryAt(/std/collections/map/map<string, i32>("left"raw_utf8, 4i32, "right"raw_utf8, 7i32),
                                            "left"raw_utf8))}
  [i32 mut] total{plus(/std/collections/map/count(/std/collections/map/map<string, i32>("left"raw_utf8, 4i32, "right"raw_utf8, 7i32)),
                          found)}
  assign(total, plus(total, /std/collections/map/at(/std/collections/map/map<string, i32>("extra"raw_utf8, 9i32, "other"raw_utf8, 2i32),
                                                    "extra"raw_utf8)))
  assign(total, plus(total, /std/collections/map/at_unsafe(/std/collections/map/map<string, i32>("bonus"raw_utf8, 5i32, "keep"raw_utf8, 1i32),
                                                           "bonus"raw_utf8)))
  if(/std/collections/map/contains(/std/collections/map/map<string, i32>("left"raw_utf8, 4i32, "right"raw_utf8, 7i32),
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
import /std/collections/internal_map/*

[effects(heap_alloc), return<int>]
main() {
  return(/std/collections/map/count(/std/collections/map/map<string, i32>("left"raw_utf8, 4i32, "wrong"raw_utf8, false)))
}
  )";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  checkMapPairTemplateConflict(error);
}

TEST_CASE("helper-wrapped inferred experimental result map default parameters validate") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/internal_map/*

[return<T> effects(heap_alloc)]
wrapStatus<T>([T] status) {
  return(status)
}

[effects(heap_alloc), return<int>]
scoreStatus([auto] status{wrapStatus(Result.ok(/std/collections/map/map<string, i32>("seed"raw_utf8, 0i32)))}) {
  return(0i32)
}

[effects(heap_alloc), return<int>]
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

TEST_CASE("helper-wrapped inferred experimental result map default parameters keep mismatch diagnostics") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/internal_map/*

[return<T> effects(heap_alloc)]
wrapStatus<T>([T] status) {
  return(status)
}

[effects(heap_alloc), return<int>]
scoreStatus([auto] status{wrapStatus(Result.ok(/std/collections/map/map<string, i32>("seed"raw_utf8, 0i32)))}) {
  return(0i32)
}

[effects(heap_alloc), return<int>]
main() {
  return(scoreStatus(wrapStatus(Result.ok(/std/collections/map/map<string, i32>("left"raw_utf8, 4i32,
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
import /std/collections/internal_map/*

[return<T> effects(heap_alloc)]
wrapValues<T>([T] values) {
  return(values)
}

[effects(io_err)]
unexpectedWrappedCanonicalMapHelperReceiverError([ContainerError] err) {
  [Result<ContainerError>] status{err.code}
  print_line_error(Result.why(status))
}

[return<Result<int, ContainerError>> effects(io_out, heap_alloc)
 on_error<ContainerError, /unexpectedWrappedCanonicalMapHelperReceiverError>]
main() {
  [i32] found{try(/std/collections/map/tryAt(wrapValues(/std/collections/map/map<string, i32>("left"raw_utf8, 4i32,
                                                                                  "right"raw_utf8, 7i32)),
                                            "left"raw_utf8))}
  [i32 mut] total{plus(/std/collections/map/count(wrapValues(/std/collections/map/map<string, i32>("left"raw_utf8, 4i32,
                                                                                      "right"raw_utf8, 7i32))),
                          found)}
  assign(total, plus(total, /std/collections/map/at(wrapValues(/std/collections/map/map<string, i32>("extra"raw_utf8, 9i32,
                                                                                        "other"raw_utf8, 2i32)),
                                                    "extra"raw_utf8)))
  assign(total, plus(total, /std/collections/map/at_unsafe(wrapValues(/std/collections/map/map<string, i32>("bonus"raw_utf8, 5i32,
                                                                                               "keep"raw_utf8, 1i32)),
                                                           "bonus"raw_utf8)))
  if(/std/collections/map/contains(wrapValues(/std/collections/map/map<string, i32>("left"raw_utf8, 4i32,
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
import /std/collections/internal_map/*

[return<T> effects(heap_alloc)]
wrapValues<T>([T] values) {
  return(values)
}

[effects(heap_alloc), return<int>]
main() {
  return(/std/collections/map/count(wrapValues(/std/collections/map/map<string, i32>("left"raw_utf8, 4i32,
                                                                        "wrong"raw_utf8, false))))
}
  )";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  checkMapPairTemplateConflict(error);
}

TEST_CASE("canonical map methods validate direct constructor receivers") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/internal_map/*

[effects(io_err)]
unexpectedCanonicalMapMethodReceiverError([ContainerError] err) {
  [Result<ContainerError>] status{err.code}
  print_line_error(Result.why(status))
}

[return<Result<int, ContainerError>> effects(io_out, heap_alloc) on_error<ContainerError, /unexpectedCanonicalMapMethodReceiverError>]
main() {
  [i32] found{try(/std/collections/map/map<string, i32>("left"raw_utf8, 4i32, "right"raw_utf8, 7i32).tryAt("left"raw_utf8))}
  [i32 mut] total{plus(/std/collections/map/map<string, i32>("left"raw_utf8, 4i32, "right"raw_utf8, 7i32).count(), found)}
  assign(total, plus(total, /std/collections/map/map<string, i32>("extra"raw_utf8, 9i32, "other"raw_utf8, 2i32).at("extra"raw_utf8)))
  assign(total, plus(total, /std/collections/map/map<string, i32>("bonus"raw_utf8, 5i32, "keep"raw_utf8, 1i32).at_unsafe("bonus"raw_utf8)))
  if(/std/collections/map/map<string, i32>("left"raw_utf8, 4i32, "right"raw_utf8, 7i32).contains("right"raw_utf8),
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

TEST_CASE("canonical map methods keep mismatch diagnostics on direct constructor receivers") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/internal_map/*

[effects(heap_alloc), return<int>]
main() {
  return(/std/collections/map/map<string, i32>("left"raw_utf8, 4i32, "wrong"raw_utf8, false).count())
}
  )";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  checkMapPairTemplateConflict(error);
}

TEST_CASE("helper-wrapped canonical map helpers accept direct constructor receivers") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/internal_map/*

[return<T> effects(heap_alloc)]
wrapValues<T>([T] values) {
  return(values)
}

[effects(io_err)]
unexpectedWrappedCanonicalMapMethodReceiverError([ContainerError] err) {
  [Result<ContainerError>] status{err.code}
  print_line_error(Result.why(status))
}

[return<Result<int, ContainerError>> effects(io_out, heap_alloc)
 on_error<ContainerError, /unexpectedWrappedCanonicalMapMethodReceiverError>]
main() {
  [i32] found{try(/std/collections/map/tryAt<string, i32>(
      wrapValues(/std/collections/map/map<string, i32>("left"raw_utf8, 4i32, "right"raw_utf8, 7i32)),
      "left"raw_utf8))}
  [i32 mut] total{plus(/std/collections/map/count<string, i32>(
                           wrapValues(/std/collections/map/map<string, i32>("left"raw_utf8, 4i32, "right"raw_utf8, 7i32))),
                       found)}
  assign(total, plus(total, /std/collections/map/at<string, i32>(
                                wrapValues(/std/collections/map/map<string, i32>("extra"raw_utf8, 9i32,
                                                                    "other"raw_utf8, 2i32)),
                                "extra"raw_utf8)))
  if(/std/collections/map/contains<string, i32>(
         wrapValues(/std/collections/map/map<string, i32>("left"raw_utf8, 4i32, "right"raw_utf8, 7i32)),
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

TEST_CASE("helper-wrapped canonical map methods keep mismatch diagnostics on direct constructor receivers") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/internal_map/*

[return<T> effects(heap_alloc)]
wrapValues<T>([T] values) {
  return(values)
}

[effects(heap_alloc), return<int>]
main() {
  return(wrapValues(/std/collections/map/map<string, i32>("left"raw_utf8, 4i32, "wrong"raw_utf8, false)).count())
}
  )";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  checkMapPairTemplateConflict(error);
}

TEST_CASE("field-bound canonical map helpers accept struct field receivers") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/internal_map/*

[effects(io_err)]
unexpectedFieldCanonicalMapMethodReceiverError([ContainerError] err) {
  [Result<ContainerError>] status{err.code}
  print_line_error(Result.why(status))
}

Holder() {
  [map<string, i32>] values{/std/collections/map/map<string, i32>("left"raw_utf8, 4i32, "right"raw_utf8, 7i32)}
}

[return<Result<int, ContainerError>> effects(io_out, heap_alloc)
 on_error<ContainerError, /unexpectedFieldCanonicalMapMethodReceiverError>]
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

TEST_CASE("field-bound canonical map wrapper helpers accept struct field receivers") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/internal_map/*

Holder() {
  [map<string, i32>] values{/std/collections/map/map<string, i32>("left"raw_utf8, 4i32, "right"raw_utf8, 7i32)}
}

[effects(heap_alloc), return<int>]
main() {
  [Holder] holder{Holder()}
  return(plus(/std/collections/map/count(holder.values),
              /std/collections/map/at(holder.values, "left"raw_utf8)))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("field-bound canonical map bare count fallback validates") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/internal_map/*

Holder() {
  [map<string, i32>] values{/std/collections/map/map<string, i32>("left"raw_utf8, 4i32, "right"raw_utf8, 7i32)}
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

TEST_CASE("field-bound canonical map explicit at body arguments validate") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/internal_map/*

Holder() {
  [map<string, i32>] values{/std/collections/map/map<string, i32>("left"raw_utf8, 4i32, "right"raw_utf8, 7i32)}
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

TEST_CASE("field-bound canonical map explicit at expression body arguments validate") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/internal_map/*

Holder() {
  [map<string, i32>] values{/std/collections/map/map<string, i32>("left"raw_utf8, 4i32, "right"raw_utf8, 7i32)}
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

TEST_CASE("field-bound canonical map explicit at expression body arguments keep mismatch diagnostics") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/internal_map/*

Holder() {
  [map<string, i32>] values{/std/collections/map/map<string, i32>("left"raw_utf8, 4i32, "right"raw_utf8, 7i32)}
}

[effects(heap_alloc), return<int>]
main() {
  [Holder] holder{Holder()}
  return(/std/collections/map/at<string, i32>(holder.values) { 1i32 })
}
  )";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.find("argument count mismatch") != std::string::npos);
  CHECK((error.find("mapAt") != std::string::npos ||
         error.find("/std/collections/map/at") != std::string::npos));
}

TEST_SUITE_END();
