TEST_CASE("statement call with block arguments validates") {
  const std::string source = R"(
[return<void>]
execute_repeat([i32] count) {
  return()
}

[return<int>]
main() {
  [i32 mut] value{1i32}
  execute_repeat(2i32) {
    assign(value, 3i32)
  }
  return(value)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("block arguments require definition target") {
  const std::string source = R"(
[return<int>]
main() {
  plus(1i32, 2i32) { return(1i32) }
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("block arguments require a definition target") != std::string::npos);
}

TEST_CASE("statement call with block arguments accepts bindings") {
  const std::string source = R"(
[return<void>]
execute_repeat([i32] count) {
  return()
}

[return<int>]
main() {
  execute_repeat(2i32) {
    [i32] temp{3i32}
  }
  return(1i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("method call with block arguments targets definition") {
  const std::string source = R"(
[struct]
Thing() {
  [i32] value{1i32}
}

[return<void>]
/Thing/do([Thing] self) {
  return()
}

[return<int>]
main() {
  [Thing] item{Thing{}}
  item.do() {
    [i32] temp{2i32}
  }
  return(1i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("block binding infers string type") {
  const std::string source = R"(
[return<bool>]
main() {
  return(block(){
    [mut] value{"hello"utf8}
    equal(value, "hello"utf8)
  })
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("block expression with parens validates") {
  const std::string source = R"(
[return<bool>]
main() {
  return(block() {
    [mut] value{"hello"utf8}
    equal(value, "hello"utf8)
  })
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("block expression allows return value") {
  const std::string source = R"(
[return<int>]
main() {
  return(block() {
    return(2i32)
    3i32
  })
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("block expression requires block arguments") {
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

TEST_CASE("user-defined block call ignores builtin rules") {
  const std::string source = R"(
[return<int>]
block([i32] value) {
  return(value)
}

[return<int>]
main() {
  [i32 mut] result{block(1i32)}
  block(2i32) {
    assign(result, 3i32)
  }
  return(result)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("block expression rejects arguments") {
  const std::string source = R"(
[return<int>]
main() {
  return(block(1i32) { 2i32 })
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("block expression does not accept arguments") != std::string::npos);
}

TEST_CASE("block expression requires a value") {
  const std::string source = R"(
[return<int>]
main() {
  return(block() { })
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("block expression requires a value") != std::string::npos);
}

TEST_CASE("block expression must end with expression") {
  const std::string source = R"(
[return<int>]
main() {
  return(block() {
    [i32] value{1i32}
  })
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("block expression must end with an expression") != std::string::npos);
}

TEST_CASE("if missing else fails") {
  const std::string source = R"(
[return<int>]
main() {
  [i32 mut] value{1i32}
  if(true, then(){ assign(value, 2i32) })
  return(value)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("if requires condition") != std::string::npos);
}

TEST_CASE("if condition requires bool") {
  const std::string source = R"(
[return<int>]
main() {
  return(if(1i32, then(){ 1i32 }, else(){ 2i32 }))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("if condition requires bool") != std::string::npos);
}

TEST_CASE("if statement sugar accepts call arguments") {
  const std::string source = R"(
[return<int>]
main() {
  return(plus(if(true) { 1i32 } else { 2i32 }, 3i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("if rejects trailing block arguments") {
  const std::string source = R"(
[return<int>]
main() {
  return(if(true, then(){ 1i32 }, else(){ 2i32 }) { 3i32 })
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("if does not accept trailing block arguments") != std::string::npos);
}

TEST_CASE("if blocks ignore definition name collisions") {
  const std::string source = R"(
[return<void>]
branch() {
  return()
}

[return<void>]
other() {
  return()
}

[return<int>]
main() {
  return(if(true, branch(){ 1i32 }, other(){ 2i32 }))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("if requires branch envelopes") {
  const std::string source = R"(
[return<int>]
main() {
  return(if(true, 1i32, 2i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("if branches require block envelopes") != std::string::npos);
}

TEST_CASE("if rejects named arguments in source syntax") {
  const std::string source = R"(
[return<int>]
main() {
  return(if([cond] true, [then] then(){ 1i32 }, [alt] else(){ 2i32 }))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("named arguments not supported for builtin calls") != std::string::npos);
}

TEST_CASE("if rejects builtin string map access mixed with numeric branch") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
main() {
  [map<i32, string>] values{map<i32, string>(1i32, "one"utf8)}
  return(if(true, then(){ at(values, 1i32) }, else(){ 2i32 }))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("if branches must return compatible types") != std::string::npos);
}

TEST_CASE("if uses user-defined at return type for branch compatibility") {
  const std::string source = R"(
[return<int>]
at([map<i32, string>] values, [i32] key) {
  return(key)
}

[return<int>]
main() {
  [map<i32, string>] values{map<i32, string>(1i32, "one"utf8)}
  return(if(true, then(){ at(values, 1i32) }, else(){ 2i32 }))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("labeled arguments accept bracket syntax") {
  const std::string source = R"(
[return<int>]
sum3([i32] a, [i32] b, [i32] c) {
  return(plus(plus(a, b), c))
}

[return<int>]
main() {
  return(sum3(1i32 [c] 3i32 [b] 2i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("binding not allowed in expression") {
  const std::string source = R"(
[return<int>]
main() {
  return([i32] value{1i32})
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("binding not allowed in expression") != std::string::npos);
}

TEST_CASE("execution rejects binding transforms") {
  const std::string source = R"(
[return<void>]
ping() {
  return()
}

[return<int>]
main() {
  [mut] ping()
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("mut transform is not allowed on executions") != std::string::npos);
}

TEST_CASE("unknown identifier fails for bare name") {
  const std::string source = R"(
[return<int>]
main() {
  return(missing)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown identifier") != std::string::npos);
}

TEST_CASE("repeat is statement-only") {
  const std::string source = R"(
[return<int>]
main() {
  return(repeat(1i32) { })
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("repeat is only supported as a statement") != std::string::npos);
}

TEST_CASE("string literal count validates") {
  const std::string source = R"(
[return<int>]
main() {
  [i32] a{count("abc"utf8)}
  [i32] b{count("hi"utf8)}
  return(plus(a, b))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}
