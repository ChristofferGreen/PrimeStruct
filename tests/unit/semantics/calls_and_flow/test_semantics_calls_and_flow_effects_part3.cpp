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

TEST_SUITE_END();
