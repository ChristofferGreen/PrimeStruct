#include "third_party/doctest.h"

#include "test_semantics_helpers.h"

TEST_SUITE_BEGIN("primestruct.semantics.bindings.control_flow");

TEST_CASE("borrow checker rejects assign before pointer alias use in repeat body") {
  const std::string source = R"(
[return<void>]
main() {
  [i32 mut] value{1i32}
  [Reference<i32>] ref{location(value)}
  [Pointer<i32>] ptr{location(ref)}
  assign(value, 2i32)
  repeat(1i32) {
    [i32] observed{dereference(ptr)}
  }
  return()
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("borrowed binding: value") != std::string::npos);
}

TEST_CASE("borrow checker allows assign after pointer alias use in repeat body") {
  const std::string source = R"(
[return<void>]
main() {
  [i32 mut] value{1i32}
  [Reference<i32>] ref{location(value)}
  [Pointer<i32>] ptr{location(ref)}
  repeat(1i32) {
    [i32] observed{dereference(ptr)}
  }
  assign(value, 2i32)
  return()
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("borrow checker rejects assign after pointer alias use across while iterations") {
  const std::string source = R"(
[return<void>]
main() {
  [i32 mut] value{1i32}
  [Reference<i32>] ref{location(value)}
  [Pointer<i32>] ptr{location(ref)}
  while(true, body(){
    [i32] observed{dereference(ptr)}
    assign(value, 2i32)
  })
  return()
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("borrowed binding: value") != std::string::npos);
}

TEST_CASE("borrow checker rejects assign after pointer alias use across for iterations") {
  const std::string source = R"(
[return<void>]
main() {
  [i32 mut] value{1i32}
  [Reference<i32>] ref{location(value)}
  [Pointer<i32>] ptr{location(ref)}
  [i32 mut] i{0i32}
  for(assign(i, 0i32), less_than(i, 3i32), assign(i, plus(i, 1i32)), body(){
    [i32] observed{dereference(ptr)}
    assign(value, 2i32)
  })
  return()
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("borrowed binding: value") != std::string::npos);
}

TEST_CASE("borrow checker rejects assign after pointer alias use across loop iterations") {
  const std::string source = R"(
[return<void>]
main() {
  [i32 mut] value{1i32}
  [Reference<i32>] ref{location(value)}
  [Pointer<i32>] ptr{location(ref)}
  loop(2i32, body(){
    [i32] observed{dereference(ptr)}
    assign(value, 2i32)
  })
  return()
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("borrowed binding: value") != std::string::npos);
}

TEST_CASE("borrow checker rejects assign after pointer alias use across repeat iterations") {
  const std::string source = R"(
[return<void>]
main() {
  [i32 mut] value{1i32}
  [Reference<i32>] ref{location(value)}
  [Pointer<i32>] ptr{location(ref)}
  repeat(2i32) {
    [i32] observed{dereference(ptr)}
    assign(value, 2i32)
  }
  return()
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("borrowed binding: value") != std::string::npos);
}

TEST_CASE("borrow checker allows assign after pointer alias use in single-iteration loop") {
  const std::string source = R"(
[return<void>]
main() {
  [i32 mut] value{1i32}
  [Reference<i32>] ref{location(value)}
  [Pointer<i32>] ptr{location(ref)}
  loop(1i32, body(){
    [i32] observed{dereference(ptr)}
    assign(value, 2i32)
  })
  return()
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("borrow checker allows assign after pointer alias use in single-iteration repeat") {
  const std::string source = R"(
[return<void>]
main() {
  [i32 mut] value{1i32}
  [Reference<i32>] ref{location(value)}
  [Pointer<i32>] ptr{location(ref)}
  repeat(true) {
    [i32] observed{dereference(ptr)}
    assign(value, 2i32)
  }
  return()
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("borrow checker rejects assign before pointer alias use in if branch") {
  const std::string source = R"(
[return<void>]
main() {
  [i32 mut] value{1i32}
  [Reference<i32>] ref{location(value)}
  [Pointer<i32>] ptr{location(ref)}
  assign(value, 2i32)
  if(true, then(){
    [i32] observed{dereference(ptr)}
  }, else(){
    [i32] fallback{0i32}
  })
  return()
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("borrowed binding: value") != std::string::npos);
}

TEST_CASE("borrow checker allows assign after pointer alias use in if branch") {
  const std::string source = R"(
[return<void>]
main() {
  [i32 mut] value{1i32}
  [Reference<i32>] ref{location(value)}
  [Pointer<i32>] ptr{location(ref)}
  if(true, then(){
    [i32] observed{dereference(ptr)}
  }, else(){
    [i32] fallback{0i32}
  })
  assign(value, 2i32)
  return()
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("borrow checker diagnostics include root and sink for pointer alias writes") {
  const std::string source = R"(
[return<void>]
main() {
  [i32 mut] value{1i32}
  [Reference<i32>] ref{location(value)}
  [Pointer<i32> mut] ptr{location(ref)}
  [Pointer<i32> mut] alias{ptr}
  assign(dereference(alias), 2i32)
  [i32] observed{dereference(ptr)}
  return()
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("borrowed binding: value") != std::string::npos);
  CHECK(error.find("root: value") != std::string::npos);
  CHECK(error.find("sink: alias") != std::string::npos);
}

TEST_CASE("borrow checker allows branch-local assign after last pointer alias use with no merge use") {
  const std::string source = R"(
[return<void>]
main() {
  [bool] cond{true}
  [i32 mut] value{1i32}
  [Reference<i32>] ref{location(value)}
  [Pointer<i32>] ptr{location(ref)}
  if(cond, then(){
    [i32] observed{dereference(ptr)}
    assign(value, 2i32)
  }, else(){
  })
  return()
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("borrow checker rejects branch-local assign before post-merge pointer alias use") {
  const std::string source = R"(
[return<void>]
main() {
  [bool] cond{true}
  [i32 mut] value{1i32}
  [Reference<i32>] ref{location(value)}
  [Pointer<i32>] ptr{location(ref)}
  if(cond, then(){
    [i32] observed{dereference(ptr)}
    assign(value, 2i32)
  }, else(){
  })
  [i32] later{dereference(ptr)}
  return()
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("borrowed binding: value") != std::string::npos);
}

TEST_CASE("borrow checker rejects match-branch assign before post-merge pointer alias use") {
  const std::string source = R"(
[return<void>]
main() {
  [i32] selector{0i32}
  [i32 mut] value{1i32}
  [Reference<i32>] ref{location(value)}
  [Pointer<i32>] ptr{location(ref)}
  match(selector, case(0i32) {
    [i32] observed{dereference(ptr)}
    assign(value, 2i32)
  }, else() {
  })
  [i32] later{dereference(ptr)}
  return()
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("borrowed binding: value") != std::string::npos);
}

TEST_CASE("borrow checker rejects body-block assign before post-block pointer alias use") {
  const std::string source = R"(
[return<void>]
execute_repeat([i32] count) {
  return()
}

[return<void>]
main() {
  [i32 mut] value{1i32}
  [Reference<i32>] ref{location(value)}
  [Pointer<i32>] ptr{location(ref)}
  execute_repeat(2i32) {
    [i32] observed{dereference(ptr)}
    assign(value, 2i32)
  }
  [i32] later{dereference(ptr)}
  return()
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("borrowed binding: value") != std::string::npos);
}

TEST_CASE("borrow checker allows body-block assign after last pointer alias use with no post-block use") {
  const std::string source = R"(
[return<void>]
execute_repeat([i32] count) {
  return()
}

[return<void>]
main() {
  [i32 mut] value{1i32}
  [Reference<i32>] ref{location(value)}
  [Pointer<i32>] ptr{location(ref)}
  execute_repeat(2i32) {
    [i32] observed{dereference(ptr)}
    assign(value, 2i32)
  }
  return()
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("borrow checker rejects lambda-capture assign before later pointer alias use") {
  const std::string source = R"(
[return<void>]
main() {
  [i32 mut] value{1i32}
  [Reference<i32>] ref{location(value)}
  [Pointer<i32>] ptr{location(ref)}
  [ref value, ref ref, ref ptr]([i32] x) {
    [i32] observed{dereference(ptr)}
    assign(value, 2i32)
    return(plus(x, observed))
  }
  [i32] later{dereference(ptr)}
  return()
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("borrowed binding: value") != std::string::npos);
}

TEST_CASE("borrow checker allows lambda-capture assign after last pointer alias use with no later use") {
  const std::string source = R"(
[return<void>]
main() {
  [i32 mut] value{1i32}
  [Reference<i32>] ref{location(value)}
  [Pointer<i32>] ptr{location(ref)}
  [ref value, ref ref, ref ptr]([i32] x) {
    [i32] observed{dereference(ptr)}
    assign(value, 2i32)
    return(plus(x, observed))
  }
  return()
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_SUITE_END();
