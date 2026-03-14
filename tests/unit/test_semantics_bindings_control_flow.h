TEST_SUITE_BEGIN("primestruct.semantics.bindings.control_flow");

TEST_CASE("if validates block arguments") {
  const std::string source = R"(
[return<int>]
main() {
  [i32 mut] value{1i32}
  if(true, then(){
    [i32] temp{2i32}
    assign(value, temp)
  }, else(){ assign(value, 3i32) })
  return(value)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("if rejects float condition") {
  const std::string source = R"(
[return<int>]
main() {
  if(1.5f, then(){ return(1i32) }, else(){ return(2i32) })
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("if condition requires bool") != std::string::npos);
}

TEST_CASE("if expression rejects void blocks") {
  const std::string source = R"(
[return<void>]
noop() {
  return()
}

[return<int>]
main() {
  return(if(true, then(){ noop() }, else(){ 1i32 }))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("if branches must produce a value") != std::string::npos);
}

TEST_CASE("if expression rejects mixed string/numeric branches") {
  const std::string source = R"(
main() {
  [string] message{if(true, then(){ "hello"utf8 }, else(){ 1i32 })}
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("if branches must return compatible types") != std::string::npos);
}

TEST_CASE("if expression accepts nested string branches") {
  const std::string source = R"(
[return<string>]
describe([i32] code) {
  return(if(equal(code, 1i32),
            then(){ "one"utf8 },
            else(){ if(equal(code, 2i32), then(){ "two"utf8 }, else(){ "other"utf8 }) }))
}

[return<int>]
main() {
  [string] message{describe(2i32)}
  return(0i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("repeat validates block arguments") {
  const std::string source = R"(
[return<int>]
main() {
  [i32 mut] value{0i32}
  repeat(3i32) {
    assign(value, plus(value, 2i32))
  }
  return(value)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("repeat validates bool count") {
  const std::string source = R"(
[return<int>]
main() {
  [i32 mut] value{0i32}
  repeat(true) {
    assign(value, 7i32)
  }
  return(value)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("block expression validates and introduces scope") {
  const std::string source = R"(
[return<int>]
main() {
  [i32] value{block(){
    [i32] inner{1i32}
    plus(inner, 2i32)
  }}
  return(value)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("block expression allows explicit return value") {
  const std::string source = R"(
[return<int>]
main() {
  return(block(){
    [i32] inner{1i32}
    return(plus(inner, 2i32))
  })
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("block expression allows early return") {
  const std::string source = R"(
[return<int>]
main() {
  return(block(){
    return(3i32)
    4i32
  })
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("block expression rejects void return value") {
  const std::string source = R"(
[return<void>]
log() {
  return()
}

[return<int>]
main() {
  return(block(){
    return(log())
  })
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("block expression requires a value") != std::string::npos);
}

TEST_CASE("if expression allows return value in branch blocks") {
  const std::string source = R"(
[return<int>]
main() {
  return(if(true, then(){
    return(1i32)
  }, else(){
    return(2i32)
  }))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("block expression must end with value") {
  const std::string source = R"(
[return<int>]
main() {
  return(block(){
    [i32] inner{1i32}
  })
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("block expression must end with an expression") != std::string::npos);
}

TEST_CASE("block expression rejects void tail") {
  const std::string source = R"(
[return<void>]
log() {
  return()
}

[return<int>]
main() {
  return(block(){
    log()
  })
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("block expression requires a value") != std::string::npos);
}

TEST_CASE("block expression rejects arguments") {
  const std::string source = R"(
[return<int>]
main() {
  return(block(1i32){
    1i32
  })
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("block expression does not accept arguments") != std::string::npos);
}

TEST_CASE("block expression requires body arguments") {
  const std::string source = R"(
[return<int>]
main() {
  return(block())
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("block requires block arguments") != std::string::npos);
}

TEST_CASE("block statement rejects arguments") {
  const std::string source = R"(
[return<int>]
main() {
  block(1i32) { 2i32 }
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("block does not accept arguments") != std::string::npos);
}

TEST_CASE("block requires body arguments") {
  const std::string source = R"(
[return<int>]
main() {
  block()
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("block requires block arguments") != std::string::npos);
}

TEST_CASE("block scope does not leak bindings") {
  const std::string source = R"(
[return<int>]
main() {
  block(){
    [i32] inner{1i32}
    inner
  }
  return(inner)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown identifier") != std::string::npos);
}

TEST_CASE("block bindings infer primitive type from initializer expressions") {
  const std::string source = R"(
namespace i64 {
  [return<i64>]
  inc([i64] self) {
    return(plus(self, 1i64))
  }
}

[return<i64>]
main() {
  return(block(){
    [mut] value{plus(1i64, 2i64)}
    value.inc()
  })
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

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

TEST_CASE("repeat rejects float count") {
  const std::string source = R"(
[return<int>]
main() {
  repeat(1.5f) {
  }
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("repeat count requires integer or bool") != std::string::npos);
}

TEST_CASE("repeat accepts bool count") {
  const std::string source = R"(
[return<int>]
main() {
  [bool] enabled{true}
  repeat(enabled) {
  }
  return(0i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("repeat rejects string count") {
  const std::string source = R"(
[return<int>]
main() {
  repeat("nope"utf8) {
  }
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("repeat count requires integer or bool") != std::string::npos);
}

TEST_SUITE_END();
