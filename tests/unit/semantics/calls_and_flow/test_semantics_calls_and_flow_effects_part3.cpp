#include "third_party/doctest.h"

#include "../test_semantics_helpers.h"

// Callee effects at call sites (docs/spec/language-core.md, Effects): every effect a callee declares with
// `[effects(...)]` must be active in the caller.

TEST_SUITE_BEGIN("primestruct.semantics.calls_flow.effects");

TEST_CASE("std net call requires net_io effect in caller") {
  const std::string source = R"(
import /std/net/*

[return<int>]
main() {
  return(listen(0i32, 4i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("/std/net/listen requires net_io effect in /main") != std::string::npos);
}

TEST_CASE("std net call validates when caller declares net_io") {
  const std::string source = R"(
import /std/net/*

[return<int> effects(net_io)]
main() {
  return(listen(0i32, 4i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("plain call requires callee declared effects") {
  const std::string source = R"(
[return<int> effects(heap_alloc)]
make_count() {
  return(1i32)
}

[return<int> effects(io_out)]
main() {
  return(make_count())
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("/make_count requires heap_alloc effect in /main") != std::string::npos);
}

TEST_CASE("callee without effects transform needs no caller effects") {
  const std::string source = R"(
[return<int>]
add_one([i32] value) {
  return(plus(value, 1i32))
}

[return<int> effects(heap_alloc)]
main() {
  return(add_one(1i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("file_write in caller satisfies callee file_read") {
  const std::string source = R"(
[return<int> effects(file_read)]
read_size() {
  return(0i32)
}

[return<int> effects(file_write)]
main() {
  return(read_size())
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("method call requires callee declared effects") {
  const std::string source = R"(
Logger() {
  [i32] level{0i32}

  [return<int> effects(io_err)]
  report() {
    print_line_error("report"utf8)
    return(this.level)
  }
}

[return<int> effects(io_out)]
main() {
  [Logger] logger{Logger{}}
  return(logger.report())
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("/Logger/report requires io_err effect in /main") != std::string::npos);
}

TEST_CASE("method call validates when caller declares callee effects") {
  const std::string source = R"(
Logger() {
  [i32] level{0i32}

  [return<int> effects(io_err)]
  report() {
    print_line_error("report"utf8)
    return(this.level)
  }
}

[return<int> effects(io_out io_err)]
main() {
  [Logger] logger{Logger{}}
  return(logger.report())
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("monomorphized template call requires callee declared effects") {
  const std::string source = R"(
[return<T> effects(heap_alloc)]
keep<T>([T] value) {
  return(value)
}

[return<int> effects(io_out)]
main() {
  return(keep<i32>(3i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("/keep requires heap_alloc effect in /main") != std::string::npos);
}

TEST_CASE("lambda body is checked against enclosing definition effects") {
  const std::string source = R"(
[return<int> effects(task)]
work([i32] value) {
  return(value)
}

[return<int> effects(io_out)]
main() {
  [auto] run{[]([i32] value) { return(work(value)) }}
  return(run(1i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("/work requires task effect in a lambda in /main") != std::string::npos);
}

TEST_CASE("lambda body inherits enclosing definition effects") {
  const std::string source = R"(
[return<int> effects(task)]
work([i32] value) {
  return(value)
}

[return<int> effects(io_out task)]
main() {
  [auto] run{[]([i32] value) { return(work(value)) }}
  return(run(1i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("calling a Function value needs none of its closures' effects") {
  // The closure's io_err comes from main; the holder that calls it through Function declares only heap_alloc.
  const std::string source = R"(
import /std/collections/*

[struct]
Holder {
  [Function<i32, i32> mut] handler

  [mut return<i32> effects(heap_alloc)]
  run([i32] x) {
    return(this.handler(x))
  }
}

[return<int> effects(heap_alloc, io_err)]
main() {
  [Holder mut] holder{Holder{[handler] Function<i32, i32>{[]([i32] x) { return(x + 1i32) }}}}
  return(holder.run(1i32))
}
)";
  std::string error;
  CHECK_MESSAGE(validateProgram(source, "/main", error), error);
  CHECK(error.empty());
}

TEST_CASE("field access resolves the struct payload of a try and of a question mark") {
  const auto source = [](const std::string &field) {
    std::string text = R"(
[struct]
Pair() {
  [i32] left{0i32}
  [i32] right{0i32}
}

[return<Result<Pair, i32>>]
make_pair() {
  return(Result.ok(Pair{[left] 1i32, [right] 2i32}))
}

[effects(io_out)]
on_failure([i32] err) {
  print_line(err)
}

[return<i32> effects(io_out) on_error<i32, /on_failure>]
main() {
  [i32] a{try(make_pair()).FIELD}
  [i32] b{make_pair()?.FIELD}
  return(plus(a, b))
}
)";
    const size_t at = text.find("FIELD");
    text.replace(text.find("FIELD", at + 1), 5, field);
    text.replace(at, 5, field);
    return text;
  };
  std::string error;
  CHECK(validateProgram(source("right"), "/main", error));
  CHECK(error.empty());
  error.clear();
  const std::string missing = source("missing");
  const size_t tryOffset = missing.find("try(make_pair())");
  REQUIRE(tryOffset != std::string::npos);
  const int tryLine =
      1 + static_cast<int>(std::count(missing.begin(), missing.begin() + tryOffset, '\n'));
  primec::SemanticDiagnosticInfo diagnosticInfo;
  CHECK_FALSE(validateProgramReportingDiagnostic(missing, "/main", error, diagnosticInfo));
  CHECK(error.find("unknown field: missing") != std::string::npos);
  CHECK(diagnosticInfo.hasPrimarySpan);
  CHECK(diagnosticInfo.primarySpan.line == tryLine);
}

TEST_CASE("field access on a try of a scalar payload needs a struct receiver") {
  const std::string source = R"(
[return<Result<i32, i32>>]
make_value() {
  return(Result.ok(1i32))
}

[effects(io_out)]
on_failure([i32] err) {
  print_line(err)
}

[return<i32> effects(io_out) on_error<i32, /on_failure>]
main() {
  [i32] a{try(make_value()).left}
  return(a)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("field access requires struct receiver") != std::string::npos);
}

TEST_CASE("effects(none) declares a definition without effects") {
  const std::string source = R"(
[effects(none), return<int>]
twice([int] value) {
  return(value)
}

[effects(heap_alloc), return<int>]
main() {
  return(twice(2i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("effects(none) definition cannot print") {
  const std::string source = R"(
[effects(none), return<int>]
main() {
  print_line("hi"utf8)
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("print_line requires io_out effect") != std::string::npos);
}

TEST_CASE("effects(none) execution cannot print") {
  const std::string source = R"(
[return<int>]
main() {
  [effects(none)] print_line("hi"utf8)
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("print_line requires io_out effect") != std::string::npos);
}

TEST_SUITE_END();
