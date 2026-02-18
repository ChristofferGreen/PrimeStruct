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
