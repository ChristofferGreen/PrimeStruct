#include "third_party/doctest.h"

#include "test_semantics_helpers.h"

TEST_SUITE_BEGIN("primestruct.semantics.calls_flow.control");

TEST_CASE("repeat rejects missing count") {
  const std::string source = R"(
[return<int>]
main() {
  repeat() {
  }
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("repeat requires exactly one argument") != std::string::npos);
}

TEST_CASE("repeat requires block arguments") {
  const std::string source = R"(
[return<int>]
main() {
  repeat(2i32)
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("repeat requires block arguments") != std::string::npos);
}

TEST_CASE("repeat rejects template arguments") {
  const std::string source = R"(
[return<int>]
main() {
  repeat<i32>(2i32) { 1i32 }
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("repeat does not accept template arguments") != std::string::npos);
}

TEST_CASE("loop rejects named arguments") {
  const std::string source = R"(
[return<int>]
main() {
  loop([count] 2i32, do(){ })
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("named arguments not supported for builtin calls") != std::string::npos);
}

TEST_CASE("loop body rejects named arguments") {
  const std::string source = R"(
[return<int>]
main() {
  [i32 mut] total{0i32}
  loop(2i32, body([step] 1i32) {
    assign(total, plus(total, 1i32))
  })
  return(total)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("loop body requires a block envelope") != std::string::npos);
}

TEST_CASE("while rejects named arguments") {
  const std::string source = R"(
[return<int>]
main() {
  while([condition] true, do(){ })
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("named arguments not supported for builtin calls") != std::string::npos);
}

TEST_CASE("for rejects named arguments") {
  const std::string source = R"(
[return<int>]
main() {
  for([init] [i32 mut] i{0i32}, [condition] less_than(i, 2i32), [step] assign(i, plus(i, 1i32)), do(){ })
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("named arguments not supported for builtin calls") != std::string::npos);
}

TEST_CASE("for accepts comma separators") {
  const std::string source = R"(
[return<int>]
main() {
  [i32 mut] total{0i32}
  for([i32 mut] i{0i32}, less_than(i, 2i32), assign(i, plus(i, 1i32))) {
    assign(total, plus(total, i))
  }
  return(total)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("loop rejects template arguments") {
  const std::string source = R"(
[return<int>]
main() {
  loop<i32>(2i32) { }
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("loop does not accept template arguments") != std::string::npos);
}

TEST_CASE("while rejects template arguments") {
  const std::string source = R"(
[return<int>]
main() {
  while<bool>(true) { }
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("while does not accept template arguments") != std::string::npos);
}

TEST_CASE("for rejects template arguments") {
  const std::string source = R"(
[return<int>]
main() {
  for<i32>([i32 mut] i{0i32} less_than(i, 2i32) assign(i, plus(i, 1i32))) {
  }
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("for does not accept template arguments") != std::string::npos);
}

TEST_CASE("brace constructor values validate") {
  const std::string source = R"(
[return<int>]
pick([i32] value) {
  return(value)
}

[return<int>]
main() {
  return(pick{ 3i32 })
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("loop while for validate") {
  const std::string source = R"(
[return<int>]
main() {
  [i32 mut] total{0i32}
  [i32 mut] i{0i32}
  loop(2i32) {
    assign(total, plus(total, 1i32))
  }
  while(less_than(i 3i32)) {
    assign(total, plus(total, i))
    assign(i, plus(i, 1i32))
  }
  for([i32 mut] j{0i32} less_than(j 2i32) assign(j, plus(j, 1i32))) {
    assign(total, plus(total, j))
  }
  return(total)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("loop accepts i64 count") {
  const std::string source = R"(
[return<int>]
main() {
  [i32 mut] total{0i32}
  loop(2i64) {
    assign(total, plus(total, 1i32))
  }
  return(total)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("loop accepts u64 count") {
  const std::string source = R"(
[return<int>]
main() {
  [i32 mut] total{0i32}
  loop(2u64) {
    assign(total, plus(total, 1i32))
  }
  return(total)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("for accepts semicolon separators") {
  const std::string source = R"(
[return<int>]
main() {
  [i32 mut] total{0i32}
  for([i32 mut] i{0i32}; less_than(i, 3i32); assign(i, plus(i, 1i32))) {
    assign(total, plus(total, i))
  }
  return(total)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("loop rejected in value blocks") {
  const std::string source = R"(
[return<int>]
main() {
  value{
    loop(2i32) { }
    7i32
  }
  return(value)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("loop is only supported as a statement") != std::string::npos);
}

TEST_CASE("loop rejected in single-item value blocks") {
  const std::string source = R"(
[return<int>]
main() {
  value{ loop(1i32) { } }
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("loop is only supported as a statement") != std::string::npos);
}

TEST_CASE("while rejected in value blocks") {
  const std::string source = R"(
[return<int>]
main() {
  value{
    while(true) { }
    1i32
  }
  return(value)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("while is only supported as a statement") != std::string::npos);
}

TEST_CASE("for rejected in value blocks") {
  const std::string source = R"(
[return<int>]
main() {
  value{
    for([i32 mut] i{0i32} less_than(i, 2i32) assign(i, plus(i, 1i32))) { }
    1i32
  }
  return(value)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("for is only supported as a statement") != std::string::npos);
}

TEST_CASE("for condition accepts binding") {
  const std::string source = R"(
[return<int>]
main() {
  [i32 mut] total{0i32}
  for([i32 mut] i{0i32} [bool] keep{less_than(i, 2i32)} assign(i, plus(i, 1i32))) {
    if(keep, then(){ assign(total, plus(total, 1i32)) }, else(){ assign(total, plus(total, 0i32)) })
  }
  return(total)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("for step accepts binding") {
  const std::string source = R"(
[return<int>]
main() {
  [i32 mut] total{0i32}
  for([i32 mut] i{0i32} less_than(i, 2i32) [i32] step{1i32}) {
    assign(total, plus(total, step))
    assign(i, plus(i, step))
  }
  return(total)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("for condition binding requires bool in source syntax") {
  const std::string source = R"(
[return<int>]
main() {
  for([i32] i{0i32} [i32] keep{1i32} assign(i, plus(i, 1i32))) {
  }
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("for condition requires bool") != std::string::npos);
}

TEST_CASE("for requires init condition step and body") {
  const std::string source = R"(
[return<int>]
main() {
  for([i32 mut] i{0i32} less_than(i, 2i32) assign(i, plus(i, 1i32)))
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("for requires init, condition, step, and body") != std::string::npos);
}

TEST_CASE("loop rejects non-block body") {
  const std::string source = R"(
[return<int>]
main() {
  loop(2i32, plus(1i32, 2i32))
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("loop body requires a block envelope") != std::string::npos);
}

TEST_CASE("loop rejects negative count") {
  const std::string source = R"(
[return<int>]
main() {
  loop(-1i32) { }
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("loop count must be non-negative") != std::string::npos);
}

TEST_CASE("loop rejects non-integer count") {
  const std::string source = R"(
[return<int>]
main() {
  loop(1.5f32) { }
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("loop count requires integer") != std::string::npos);
}

TEST_CASE("loop requires count and body") {
  const std::string source = R"(
[return<int>]
main() {
  loop(2i32)
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("loop requires count and body") != std::string::npos);
}

TEST_CASE("while condition requires bool") {
  const std::string source = R"(
[return<int>]
main() {
  while(1i32) { }
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("while condition requires bool") != std::string::npos);
}

TEST_CASE("while requires condition and body") {
  const std::string source = R"(
[return<int>]
main() {
  while(true)
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("while requires condition and body") != std::string::npos);
}

TEST_CASE("loop blocks ignore definition name collisions") {
  const std::string source = R"(
[return<void>]
branch() {
  return()
}

[return<void>]
stepper() {
  return()
}

[return<int>]
main() {
  [i32 mut] total{0i32}
  loop(2i32, branch() {
    assign(total, plus(total, 1i32))
  })
  while(less_than(total, 3i32), branch() {
    assign(total, plus(total, 1i32))
  })
  for([i32 mut] i{0i32}, less_than(i, 2i32), assign(i, plus(i, 1i32)), stepper() {
    assign(total, plus(total, i))
  })
  return(total)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("shared_scope rejects non-loop statements") {
  const std::string source = R"(
[return<int>]
ping() {
  return(0i32)
}

[return<int>]
main() {
  [shared_scope] ping()
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("shared_scope is only valid on loop/while/for statements") != std::string::npos);
}

TEST_CASE("shared_scope rejects arguments") {
  const std::string source = R"(
[return<int>]
main() {
  [shared_scope(1)] loop(1i32) { }
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("shared_scope does not accept arguments") != std::string::npos);
}

TEST_CASE("shared_scope rejects template arguments") {
  const std::string source = R"(
[return<int>]
main() {
  [shared_scope<i32>] loop(1i32) { }
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("shared_scope does not accept template arguments") != std::string::npos);
}

TEST_CASE("shared_scope requires loop body envelope") {
  const std::string source = R"(
[return<int>]
main() {
  [shared_scope] loop(1i32, do())
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("shared_scope requires loop body in do() { ... }") != std::string::npos);
}

TEST_CASE("shared_scope hoists while bindings") {
  const std::string source = R"(
[return<int>]
main() {
  [i32 mut] total{0i32}
  [i32 mut] i{0i32}
  [shared_scope]
  while(less_than(i, 3i32)) {
    [i32 mut] acc{0i32}
    assign(acc, plus(acc, 1i32))
    assign(total, plus(total, acc))
    assign(i, plus(i, 1i32))
  }
  return(total)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("reference participates in signedness checks") {
  const std::string source = R"(
[return<int>]
main() {
  [u64 mut] value{1u64}
  [Reference<u64>] ref{location(value)}
  return(plus(ref, 1i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("mixed signed/unsigned") != std::string::npos);
}

#include "test_semantics_calls_and_flow_control_blocks.h"

TEST_SUITE_END();
