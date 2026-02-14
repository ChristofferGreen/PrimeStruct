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

TEST_CASE("statement call with block arguments rejects bindings") {
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
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("binding not allowed in execution body") != std::string::npos);
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

TEST_CASE("block expression without parens validates") {
  const std::string source = R"(
[return<bool>]
main() {
  return(block{
    [mut] value{"hello"utf8}
    equal(value, "hello"utf8)
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
  return(block{ })
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
  return(block{
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
  return(if(true, 1i32, 2i32) { 3i32 })
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("if does not accept trailing block arguments") != std::string::npos);
}

TEST_CASE("if rejects named arguments") {
  const std::string source = R"(
[return<int>]
main() {
  return(if([cond] true, [then] 1i32, [alt] 2i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("named arguments not supported for builtin calls") != std::string::npos);
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

TEST_CASE("unknown identifier fails") {
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

TEST_CASE("namespace blocks may be reopened") {
  const std::string source = R"(
namespace demo {
  [return<int>]
  foo() {
    return(1i32)
  }
}

namespace demo {
  [return<int>]
  bar() {
    return(foo())
  }
}

[return<int>]
main() {
  return(/demo/bar())
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("string literal method count validates") {
  const std::string source = R"(
[return<int>]
main() {
  return("hi"utf8.count())
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("duplicate named arguments fail") {
  const std::string source = R"(
[return<int>]
foo([i32] a, [i32] b) {
  return(a)
}

[return<int>]
main() {
  return(foo([a] 1i32, [a] 2i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("duplicate named argument") != std::string::npos);
}

TEST_CASE("array literal validates") {
  const std::string source = R"(
[return<int>]
use([array<i32>] x) {
  return(1i32)
}

[return<int>]
main() {
  return(use(array<i32>(1i32, 2i32)))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("array literal validates bool elements") {
  const std::string source = R"(
[return<int>]
main() {
  array<bool>(true, false)
  return(1i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("vector literal validates") {
  const std::string source = R"(
[return<int>]
use([vector<i32>] x) {
  return(1i32)
}

[effects(heap_alloc), return<int>]
main() {
  return(use(vector<i32>(1i32, 2i32)))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("vector literal validates bool elements") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  vector<bool>(true, false)
  return(1i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("vector literal requires heap_alloc effect") {
  const std::string source = R"(
[return<int>]
main() {
  vector<i32>(1i32)
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("vector literal requires heap_alloc effect") != std::string::npos);
}

TEST_CASE("vector literal allows heap_alloc effect") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  vector<i32>(1i32)
  return(0i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("vector literal allows empty without heap_alloc") {
  const std::string source = R"(
[return<int>]
main() {
  vector<i32>()
  return(0i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("named arguments match parameters") {
  const std::string source = R"(
[return<int>]
foo([i32] a, [i32] b) {
  return(a)
}

[return<int>]
main() {
  return(foo([a] 1i32, [b] 2i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("named arguments with reordered params match") {
  const std::string source = R"(
[return<int>]
foo([i32] a, [i32] b) {
  return(a)
}

[return<int>]
main() {
  return(foo([b] 2i32, [a] 1i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("named arguments allow positional fill after labels") {
  const std::string source = R"(
[return<int>]
foo([i32] a, [i32] b, [i32] c) {
  return(plus(plus(a, b), c))
}

[return<int>]
main() {
  return(foo([b] 2i32, 1i32, 3i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("named arguments on method calls allow builtin names") {
  const std::string source = R"(
[struct]
Foo() {
  [i32] value{1i32}
}

[return<int>]
/Foo/plus([Foo] self, [i32] rhs) {
  return(rhs)
}

[return<int>]
main() {
  [Foo] item{1i32}
  return(item.plus([rhs] 2i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("named arguments reject builtin method calls") {
  const std::string source = R"(
[return<int>]
main() {
  return(array<i32>(1i32, 2i32).count([value] 0i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("named arguments not supported for builtin calls") != std::string::npos);
}

TEST_CASE("named arguments rejected for assign builtin") {
  const std::string source = R"(
[return<int>]
main() {
  [i32 mut] value{1i32}
  return(assign([target] value, [value] 2i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("named arguments not supported for builtin calls") != std::string::npos);
}

TEST_CASE("named arguments allow user-defined builtin name") {
  const std::string source = R"(
[return<int>]
plus([i32] left, [i32] right) {
  return(left)
}

[return<int>]
main() {
  return(plus([right] 2i32, [left] 1i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_SUITE_END();

TEST_SUITE_BEGIN("primestruct.semantics.calls_flow.collections");

TEST_CASE("count builtin validates on array literals") {
  const std::string source = R"(
[return<int>]
main() {
  return(count(array<i32>(1i32, 2i32)))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("count helper validates on array binding") {
  const std::string source = R"(
[return<int>]
main() {
  [array<i32>] values{array<i32>(1i32, 2i32)}
  return(count(values))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("count builtin validates on map literals") {
  const std::string source = R"(
[return<int>]
main() {
  return(count(map<i32, i32>(1i32, 2i32)))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("count helper validates on map binding") {
  const std::string source = R"(
[return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 2i32)}
  return(count(values))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("count helper validates on vector binding") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32)}
  return(count(values))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("count builtin validates on method calls") {
  const std::string source = R"(
[return<int>]
main() {
  return(array<i32>(1i32, 2i32).count())
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("count method validates on vector binding") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32)}
  return(values.count())
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("count rejects vector named arguments") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32)}
  return(values.count([value] 0i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("named arguments not supported for builtin calls") != std::string::npos);
}

TEST_CASE("count method validates on array binding") {
  const std::string source = R"(
[return<int>]
main() {
  [array<i32>] values{array<i32>(1i32, 2i32)}
  return(values.count())
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("count method validates on map binding") {
  const std::string source = R"(
[return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 2i32)}
  return(values.count())
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("count method validates on string binding") {
  const std::string source = R"(
[return<int>]
main() {
  [string] text{"abc"utf8}
  return(text.count())
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("capacity builtin validates on vector binding") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32)}
  return(capacity(values))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("capacity rejects template arguments") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32)}
  return(capacity<i32>(values))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("capacity does not accept template arguments") != std::string::npos);
}

TEST_CASE("capacity rejects block arguments") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32)}
  return(capacity(values) { 1i32 })
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("block arguments are only supported on statement calls") != std::string::npos);
}

TEST_CASE("capacity rejects wrong argument count") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32)}
  return(capacity(values, 1i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("argument count mismatch for builtin capacity") != std::string::npos);
}

TEST_CASE("capacity method validates on vector binding") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32)}
  return(values.capacity())
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("capacity method rejects extra arguments") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32)}
  return(values.capacity(1i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("argument count mismatch for builtin capacity") != std::string::npos);
}

TEST_CASE("capacity rejects non-vector target") {
  const std::string source = R"(
[return<int>]
main() {
  return(capacity(1i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("capacity requires vector target") != std::string::npos);
}

TEST_CASE("push requires mutable vector binding") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32)}
  push(values, 2i32)
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("push requires mutable vector binding") != std::string::npos);
}

TEST_CASE("push requires heap_alloc effect") {
  const std::string source = R"(
[return<int>]
main() {
  [vector<i32> mut] values{vector<i32>()}
  push(values, 2i32)
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("push requires heap_alloc effect") != std::string::npos);
}

TEST_CASE("push validates on mutable vector binding") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32)}
  push(values, 2i32)
  return(0i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("push rejects template arguments") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32)}
  push<i32>(values, 2i32)
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("push does not accept template arguments") != std::string::npos);
}

TEST_CASE("push rejects wrong argument count") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32)}
  push(values)
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("push requires exactly two arguments") != std::string::npos);
}

TEST_CASE("reserve requires mutable vector binding") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32)}
  reserve(values, 8i32)
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("reserve requires mutable vector binding") != std::string::npos);
}

TEST_CASE("reserve requires heap_alloc effect") {
  const std::string source = R"(
[return<int>]
main() {
  [vector<i32> mut] values{vector<i32>()}
  reserve(values, 8i32)
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("reserve requires heap_alloc effect") != std::string::npos);
}

TEST_CASE("reserve requires integer capacity") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32)}
  reserve(values, "hi"utf8)
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("reserve requires integer capacity") != std::string::npos);
}

TEST_CASE("reserve rejects template arguments") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32)}
  reserve<i32>(values, 8i32)
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("reserve does not accept template arguments") != std::string::npos);
}

TEST_CASE("reserve rejects wrong argument count") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32)}
  reserve(values)
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("reserve requires exactly two arguments") != std::string::npos);
}

TEST_CASE("reserve validates on mutable vector binding") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32)}
  reserve(values, 8i32)
  return(0i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("pop requires mutable vector binding") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32)}
  pop(values)
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("pop requires mutable vector binding") != std::string::npos);
}

TEST_CASE("pop validates on mutable vector binding") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32)}
  pop(values)
  return(0i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("pop rejects block arguments") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32)}
  pop(values) { 1i32 }
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("pop does not accept block arguments") != std::string::npos);
}

TEST_CASE("pop rejects template arguments") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32)}
  pop<i32>(values)
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("pop does not accept template arguments") != std::string::npos);
}

TEST_CASE("pop rejects wrong argument count") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32)}
  pop(values, 1i32)
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("pop requires exactly one argument") != std::string::npos);
}

TEST_CASE("clear requires mutable vector binding") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32)}
  clear(values)
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("clear requires mutable vector binding") != std::string::npos);
}

TEST_CASE("clear validates on mutable vector binding") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32)}
  clear(values)
  return(0i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("clear rejects template arguments") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32)}
  clear<i32>(values)
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("clear does not accept template arguments") != std::string::npos);
}

TEST_CASE("clear rejects wrong argument count") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32)}
  clear(values, 1i32)
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("clear requires exactly one argument") != std::string::npos);
}

TEST_CASE("remove_at requires mutable vector binding") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32)}
  remove_at(values, 0i32)
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("remove_at requires mutable vector binding") != std::string::npos);
}

TEST_CASE("remove_at requires integer index") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32)}
  remove_at(values, "hi"utf8)
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("remove_at requires integer index") != std::string::npos);
}

TEST_CASE("remove_at rejects template arguments") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32)}
  remove_at<i32>(values, 0i32)
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("remove_at does not accept template arguments") != std::string::npos);
}

TEST_CASE("remove_at rejects wrong argument count") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32)}
  remove_at(values)
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("remove_at requires exactly two arguments") != std::string::npos);
}

TEST_CASE("remove_swap requires integer index") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32)}
  remove_swap(values, "hi"utf8)
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("remove_swap requires integer index") != std::string::npos);
}

TEST_CASE("remove_swap requires mutable vector binding") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32)}
  remove_swap(values, 1i32)
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("remove_swap requires mutable vector binding") != std::string::npos);
}

TEST_CASE("remove_swap rejects template arguments") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32, 2i32)}
  remove_swap<i32>(values, 1i32)
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("remove_swap does not accept template arguments") != std::string::npos);
}

TEST_CASE("remove_swap rejects wrong argument count") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32, 2i32)}
  remove_swap(values)
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("remove_swap requires exactly two arguments") != std::string::npos);
}

TEST_CASE("remove_swap validates on mutable vector binding") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32, 2i32)}
  remove_swap(values, 1i32)
  return(0i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("vector helpers are statement-only in expressions") {
  struct HelperCase {
    const char *name;
    const char *args;
  };
  const HelperCase helpers[] = {
      {"push", "values, 2i32"},       {"pop", "values"},       {"reserve", "values, 8i32"},
      {"clear", "values"},            {"remove_at", "values, 0i32"}, {"remove_swap", "values, 0i32"}};
  for (const auto &helper : helpers) {
    CAPTURE(helper.name);
    const std::string source =
        "[effects(heap_alloc), return<int>]\n"
        "main() {\n"
        "  [vector<i32> mut] values{vector<i32>(1i32)}\n"
        "  return(" +
        std::string(helper.name) + "(" + helper.args + "))\n"
        "}\n";
    std::string error;
    CHECK_FALSE(validateProgram(source, "/main", error));
    CHECK(error.find("only supported as a statement") != std::string::npos);
  }
}

TEST_SUITE_END();

TEST_SUITE_BEGIN("primestruct.semantics.calls_flow.access");

TEST_CASE("count helper validates on string binding") {
  const std::string source = R"(
[return<int>]
main() {
  [string] text{"abc"utf8}
  return(count(text))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("count builtin rejects block arguments") {
  const std::string source = R"(
[return<int>]
main() {
  return(count(array<i32>(1i32, 2i32)) { 1i32 })
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("block arguments are only supported on statement calls") != std::string::npos);
}

TEST_CASE("count builtin rejects missing argument") {
  const std::string source = R"(
[return<int>]
main() {
  return(count())
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("argument count mismatch for builtin count") != std::string::npos);
}

TEST_CASE("count rejects non-collection target") {
  const std::string source = R"(
[return<int>]
main() {
  return(count(1i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown method: /i32/count") != std::string::npos);
}

TEST_CASE("count forwards to type method") {
  const std::string source = R"(
[struct]
Foo() {
  [i32] value{1i32}
}

[return<int>]
/Foo/count([Foo] self) {
  return(7i32)
}

[return<int>]
main() {
  [Foo] item{1i32}
  return(count(item))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("count rejects missing type method") {
  const std::string source = R"(
[struct]
Foo() {
  [i32] value{1i32}
}

[return<int>]
main() {
  [Foo] item{1i32}
  return(count(item))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown method: /Foo/count") != std::string::npos);
}

TEST_CASE("array method calls resolve to definitions") {
  const std::string source = R"(
[return<int>]
/array/first([array<i32>] items) {
  return(items[0i32])
}

[return<int>]
main() {
  [array<i32>] items{array<i32>(4i32, 7i32)}
  return(items.first())
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("map method calls resolve to definitions") {
  const std::string source = R"(
[return<int>]
/map/size([map<i32, i32>] items) {
  return(count(items))
}

[return<int>]
main() {
  [map<i32, i32>] items{map<i32, i32>(1i32, 2i32)}
  return(items.size())
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("vector method calls resolve to definitions") {
  const std::string source = R"(
[return<int>]
/vector/size([vector<i32>] items) {
  return(count(items))
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] items{vector<i32>(1i32, 2i32)}
  return(items.size())
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("method calls reject pointer receivers") {
  const std::string source = R"(
[return<int>]
main() {
  [i32] value{1i32}
  [Pointer<i32>] ptr{location(value)}
  return(ptr.count())
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown method target for count") != std::string::npos);
}

TEST_CASE("unknown method calls fail") {
  const std::string source = R"(
[struct]
Foo() {
  [i32] value{1i32}
}

[return<int>]
main() {
  [Foo] item{Foo()}
  return(item.missing())
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown method: /Foo/missing") != std::string::npos);
}

TEST_CASE("count builtin rejects template arguments") {
  const std::string source = R"(
[return<int>]
main() {
  return(count<i32>(array<i32>(1i32, 2i32)))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("count does not accept template arguments") != std::string::npos);
}

TEST_CASE("count method rejects template arguments") {
  const std::string source = R"(
[return<int>]
main() {
  return(array<i32>(1i32, 2i32).count<i32>())
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("count does not accept template arguments") != std::string::npos);
}

TEST_CASE("count method rejects block arguments") {
  const std::string source = R"(
[return<int>]
main() {
  return(array<i32>(1i32, 2i32).count() { 1i32 })
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("block arguments are only supported on statement calls") != std::string::npos);
}

TEST_CASE("array access rejects template arguments") {
  const std::string source = R"(
[return<int>]
main() {
  return(at<i32>(array<i32>(1i32), 0i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("at does not accept template arguments") != std::string::npos);
}

TEST_CASE("array literal access validates") {
  const std::string source = R"(
[return<int>]
main() {
  return(at(array<i32>(1i32, 2i32), 0i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("array access rejects argument count mismatch") {
  const std::string source = R"(
[return<int>]
main() {
  return(at(array<i32>(1i32)))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("argument count mismatch for builtin at") != std::string::npos);
}

TEST_CASE("array access rejects block arguments") {
  const std::string source = R"(
[return<int>]
main() {
  return(at(array<i32>(1i32), 0i32) { 1i32 })
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("block arguments are only supported on statement calls") != std::string::npos);
}

TEST_CASE("unsafe array access rejects template arguments") {
  const std::string source = R"(
[return<int>]
main() {
  return(at_unsafe<i32>(array<i32>(1i32), 0i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("at_unsafe does not accept template arguments") != std::string::npos);
}

TEST_CASE("unsafe array access rejects argument count mismatch") {
  const std::string source = R"(
[return<int>]
main() {
  return(at_unsafe(array<i32>(1i32), 0i32, 1i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("argument count mismatch for builtin at_unsafe") != std::string::npos);
}

TEST_CASE("unsafe array access rejects block arguments") {
  const std::string source = R"(
[return<int>]
main() {
  return(at_unsafe(array<i32>(1i32), 0i32) { 1i32 })
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("block arguments are only supported on statement calls") != std::string::npos);
}

TEST_CASE("array access rejects non-integer index") {
  const std::string source = R"(
[return<int>]
main() {
  return(at(array<i32>(1i32, 2i32), "nope"utf8))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("at requires integer index") != std::string::npos);
}

TEST_CASE("unsafe array access rejects non-integer index") {
  const std::string source = R"(
[return<int>]
main() {
  return(at_unsafe(array<i32>(1i32, 2i32), "nope"utf8))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("at_unsafe requires integer index") != std::string::npos);
}

TEST_CASE("string access rejects non-integer index") {
  const std::string source = R"(
[return<int>]
main() {
  [string] text{"hello"utf8}
  return(at(text, "nope"utf8))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("at requires integer index") != std::string::npos);
}

TEST_CASE("string access validates integer index") {
  const std::string source = R"(
[return<int>]
main() {
  [string] text{"hello"utf8}
  return(at(text, 0i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("string literal access rejects non-integer index") {
  const std::string source = R"(
[return<int>]
main() {
  return(at("hello"utf8, "nope"utf8))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("at requires integer index") != std::string::npos);
}

TEST_CASE("string literal access validates integer index") {
  const std::string source = R"(
[return<int>]
main() {
  return(at("hello"utf8, 1i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("unsafe string access rejects non-integer index") {
  const std::string source = R"(
[return<int>]
main() {
  [string] text{"hello"utf8}
  return(at_unsafe(text, "nope"utf8))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("at_unsafe requires integer index") != std::string::npos);
}

TEST_CASE("unsafe string access validates integer index") {
  const std::string source = R"(
[return<int>]
main() {
  [string] text{"hello"utf8}
  return(at_unsafe(text, 1i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("unsafe string literal access rejects non-integer index") {
  const std::string source = R"(
[return<int>]
main() {
  return(at_unsafe("hello"utf8, "nope"utf8))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("at_unsafe requires integer index") != std::string::npos);
}

TEST_CASE("unsafe string literal access validates integer index") {
  const std::string source = R"(
[return<int>]
main() {
  return(at_unsafe("hello"utf8, 0i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("array access rejects non-collection target") {
  const std::string source = R"(
[return<int>]
main() {
  return(at(1i32, 0i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("at requires array, vector, map, or string target") != std::string::npos);
}

TEST_CASE("unsafe array access rejects non-collection target") {
  const std::string source = R"(
[return<int>]
main() {
  return(at_unsafe(1i32, 0i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("at_unsafe requires array, vector, map, or string target") != std::string::npos);
}

TEST_SUITE_END();

TEST_SUITE_BEGIN("primestruct.semantics.calls_flow.named_args");

TEST_CASE("unknown named argument fails") {
  const std::string source = R"(
[return<int>]
foo([i32] a, [i32] b) {
  return(a)
}

[return<int>]
main() {
  return(foo([c] 1i32, [b] 2i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown named argument") != std::string::npos);
}

TEST_CASE("named argument duplicates positional parameter fails") {
  const std::string source = R"(
[return<int>]
foo([i32] a, [i32] b) {
  return(a)
}

[return<int>]
main() {
  return(foo(1i32, [a] 2i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("named argument duplicates parameter") != std::string::npos);
}

TEST_CASE("execution named arguments match parameters") {
  const std::string source = R"(
[return<int>]
main() {
  return(1i32)
}

[return<int>]
job([i32] a, [i32] b) {
  return(a)
}

job([a] 1i32, [b] 2i32)
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("execution named arguments allow positional fill after labels") {
  const std::string source = R"(
[return<int>]
main() {
  return(1i32)
}

[return<int>]
job([i32] a, [i32] b) {
  return(plus(a, b))
}

job([b] 2i32, 1i32)
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("execution arguments reject named builtins") {
  const std::string source = R"(
[return<int>]
main() {
  return(1i32)
}

[return<int>]
job([array<i32>] values) {
  return(values.count())
}

job(array<i32>([first] 1i32))
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("named arguments not supported for builtin calls") != std::string::npos);
}

TEST_CASE("named arguments not allowed on builtin calls") {
  const std::string source = R"(
[return<int>]
main() {
  return(plus([left] 1i32, [right] 2i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("named arguments not supported for builtin calls") != std::string::npos);
}

TEST_SUITE_END();

TEST_SUITE_BEGIN("primestruct.semantics.calls_flow.numeric_builtins");

TEST_CASE("convert builtin validates") {
  const std::string source = R"(
[return<int>]
main() {
  return(convert<int>(1.5f))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("convert missing template arg fails") {
  const std::string source = R"(
[return<int>]
main() {
  return(convert(1i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("convert requires exactly one template argument") != std::string::npos);
}

TEST_CASE("convert rejects missing argument") {
  const std::string source = R"(
[return<int>]
main() {
  return(convert<int>())
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("argument count mismatch for builtin convert") != std::string::npos);
}

TEST_CASE("convert rejects extra arguments") {
  const std::string source = R"(
[return<int>]
main() {
  return(convert<int>(1i32, 2i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("argument count mismatch for builtin convert") != std::string::npos);
}

TEST_CASE("convert unsupported template arg fails") {
  const std::string source = R"(
[return<int>]
main() {
  return(convert<u32>(1i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unsupported convert target type") != std::string::npos);
}

TEST_CASE("convert rejects software numeric target") {
  const std::string source = R"(
[return<int>]
main() {
  return(convert<decimal>(1i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("software numeric types are not supported yet") != std::string::npos);
}

TEST_CASE("convert rejects integer target") {
  const std::string source = R"(
[return<int>]
main() {
  return(convert<integer>(1i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("software numeric types are not supported yet") != std::string::npos);
}

TEST_CASE("convert<bool> accepts u64 literal") {
  const std::string source = R"(
[return<bool>]
main() {
  return(convert<bool>(1u64))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("convert<bool> accepts float operand") {
  const std::string source = R"(
[return<bool>]
main() {
  return(convert<bool>(1.5f))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("convert rejects string operand") {
  const std::string source = R"(
[return<int>]
main() {
  return(convert<int>("hi"utf8))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("convert requires numeric or bool operand") != std::string::npos);
}

TEST_CASE("abs rejects non-numeric operand") {
  const std::string source = R"(
import /math/*
[return<int>]
main() {
  return(abs("hi"utf8))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("abs requires numeric operand") != std::string::npos);
}

TEST_CASE("sign rejects non-numeric operand") {
  const std::string source = R"(
import /math/*
[return<int>]
main() {
  return(sign("hi"utf8))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("sign requires numeric operand") != std::string::npos);
}

TEST_CASE("saturate rejects non-numeric operand") {
  const std::string source = R"(
import /math/*
[return<int>]
main() {
  return(saturate("hi"utf8))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("saturate requires numeric operand") != std::string::npos);
}

TEST_CASE("pow accepts integer operands") {
  const std::string source = R"(
import /math/*
[return<int>]
main() {
  return(pow(2i32, 3i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("rounding math builtins validate") {
  const std::string source = R"(
import /math/*
[return<float>]
main() {
  return(floor(1.25f))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("float predicate math builtins validate") {
  const std::string source = R"(
import /math/*
[return<bool>]
main() {
  return(is_finite(1.0f))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("min rejects non-numeric operand") {
  const std::string source = R"(
import /math/*
[return<int>]
main() {
  return(min("hi"utf8, 1i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("min requires numeric operands") != std::string::npos);
}

TEST_CASE("max rejects non-numeric operand") {
  const std::string source = R"(
import /math/*
[return<int>]
main() {
  return(max(1i32, "hi"utf8))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("max requires numeric operands") != std::string::npos);
}

TEST_CASE("clamp rejects non-numeric operand") {
  const std::string source = R"(
import /math/*
[return<int>]
main() {
  return(clamp("hi"utf8, 0i32, 2i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("clamp requires numeric operands") != std::string::npos);
}

TEST_SUITE_END();

TEST_SUITE_BEGIN("primestruct.semantics.calls_flow.comparisons_literals");

TEST_CASE("string comparisons validate") {
  const std::string source = R"(
[return<bool>]
main() {
  return(equal("alpha"utf8, "beta"utf8))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("string comparisons reject mixed types") {
  const std::string source = R"(
[return<bool>]
main() {
  return(equal("alpha"utf8, 1i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("comparisons do not support mixed string/numeric operands") != std::string::npos);
}

TEST_CASE("arithmetic operators reject bool operands") {
  const std::string source = R"(
[return<int>]
main() {
  return(plus(true, 1i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("arithmetic operators require numeric operands") != std::string::npos);
}

TEST_CASE("comparisons reject non-numeric operands") {
  const std::string source = R"(
thing() {
  [i32] value{1i32}
}

[return<bool>]
main() {
  [thing] item{1i32}
  return(equal(item, item))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("comparisons require numeric, bool, or string operands") != std::string::npos);
}

TEST_CASE("comparisons allow bool with signed integers") {
  const std::string source = R"(
[return<bool>]
main() {
  return(equal(true, 1i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("comparisons reject bool with unsigned") {
  const std::string source = R"(
[return<bool>]
main() {
  return(equal(true, 1u64))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("comparisons do not support mixed signed/unsigned operands") != std::string::npos);
}

TEST_CASE("comparisons reject mixed int and float operands") {
  const std::string source = R"(
[return<bool>]
main() {
  return(greater_than(1i32, 2.5f))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("comparisons do not support mixed int/float operands") != std::string::npos);
}

TEST_CASE("map literal validates") {
  const std::string source = R"(
[return<int>]
use([i32] x) {
  return(1i32)
}

[return<int>]
main() {
  return(use(map<i32, i32>(1i32, 2i32)))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("map literal validates bool keys and values") {
  const std::string source = R"(
[return<int>]
main() {
  map<bool, bool>(true, false)
  return(1i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("array literal missing template arg fails") {
  const std::string source = R"(
[return<int>]
use([array<i32>] x) {
  return(1i32)
}

[return<int>]
main() {
  return(use(array(1i32)))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("array literal requires exactly one template argument") != std::string::npos);
}

TEST_CASE("vector literal missing template arg fails") {
  const std::string source = R"(
[return<int>]
use([vector<i32>] x) {
  return(1i32)
}

[effects(heap_alloc), return<int>]
main() {
  return(use(vector(1i32)))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("vector literal requires exactly one template argument") != std::string::npos);
}

TEST_CASE("array literal type mismatch fails") {
  const std::string source = R"(
[return<int>]
use([array<i32>] x) {
  return(1i32)
}

[return<int>]
main() {
  return(use(array<i32>(1i32, "hi"utf8)))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("array literal requires element type i32") != std::string::npos);
}

TEST_CASE("vector literal type mismatch fails") {
  const std::string source = R"(
[return<int>]
use([vector<i32>] x) {
  return(1i32)
}

[effects(heap_alloc), return<int>]
main() {
  return(use(vector<i32>(1i32, "hi"utf8)))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("vector literal requires element type i32") != std::string::npos);
}

TEST_CASE("array literal rejects software numeric type") {
  const std::string source = R"(
[return<int>]
main() {
  array<decimal>(1.0f)
  return(1i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("software numeric types are not supported yet") != std::string::npos);
}

TEST_CASE("vector literal rejects software numeric type") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  vector<decimal>(1.0f)
  return(1i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("software numeric types are not supported yet") != std::string::npos);
}

TEST_CASE("map literal missing template args fails") {
  const std::string source = R"(
[return<int>]
use([i32] x) {
  return(1i32)
}

[return<int>]
main() {
  return(use(map(1i32, 2i32)))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("map literal requires exactly two template arguments") != std::string::npos);
}

TEST_CASE("map literal rejects software numeric types") {
  const std::string source = R"(
[return<int>]
main() {
  map<integer, i32>(1i32, 2i32)
  return(1i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("software numeric types are not supported yet") != std::string::npos);
}

TEST_CASE("map literal key type mismatch fails") {
  const std::string source = R"(
[return<int>]
use([i32] x) {
  return(1i32)
}

[return<int>]
main() {
  return(use(map<i32, i32>("a"utf8, 2i32)))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("map literal requires key type i32") != std::string::npos);
}

TEST_CASE("map literal value type mismatch fails") {
  const std::string source = R"(
[return<int>]
use([i32] x) {
  return(1i32)
}

[return<int>]
main() {
  return(use(map<string, i32>("a"utf8, "b"utf8)))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("map literal requires value type i32") != std::string::npos);
}

TEST_CASE("map literal requires even argument count") {
  const std::string source = R"(
[return<int>]
use([i32] x) {
  return(1i32)
}

[return<int>]
main() {
  return(use(map<i32, i32>(1i32, 2i32, 3i32)))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("map literal requires an even number of arguments") != std::string::npos);
}

TEST_CASE("map access validates key type") {
  const std::string source = R"(
[return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 2i32)}
  return(at(values, "nope"utf8))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("at requires map key type i32") != std::string::npos);
}

TEST_CASE("map literal access validates") {
  const std::string source = R"(
[return<int>]
main() {
  return(at_unsafe(map<i32, i32>(1i32, 2i32), 1i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("map literal access validates for string keys") {
  const std::string source = R"(
[return<int>]
main() {
  return(at_unsafe(map<string, i32>("a"utf8, 2i32), "a"utf8))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("unsafe map access validates key type") {
  const std::string source = R"(
[return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 2i32)}
  return(at_unsafe(values, "nope"utf8))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("at_unsafe requires map key type i32") != std::string::npos);
}

TEST_CASE("string map access rejects numeric index") {
  const std::string source = R"(
[return<int>]
main() {
  [map<string, i32>] values{map<string, i32>("a"utf8, 3i32)}
  return(at(values, 1i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("at requires string map key") != std::string::npos);
}

TEST_CASE("unsafe string map access rejects numeric index") {
  const std::string source = R"(
[return<int>]
main() {
  [map<string, i32>] values{map<string, i32>("a"utf8, 3i32)}
  return(at_unsafe(values, 1i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("at_unsafe requires string map key") != std::string::npos);
}

TEST_CASE("map access accepts matching key type") {
  const std::string source = R"(
[return<int>]
main() {
  [map<string, i32>] values{map<string, i32>("a"utf8, 3i32)}
  return(at(values, "a"utf8))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("map access accepts string key expression") {
  const std::string source = R"(
[return<int>]
main() {
  [map<string, i32>] values{map<string, i32>("a"utf8, 1i32)}
  [map<i32, string>] keys{map<i32, string>(1i32, "a"utf8)}
  return(at(values, at(keys, 1i32)))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("unsafe map access accepts string key expression") {
  const std::string source = R"(
[return<int>]
main() {
  [map<string, i32>] values{map<string, i32>("a"utf8, 1i32)}
  [map<i32, string>] keys{map<i32, string>(1i32, "a"utf8)}
  return(at_unsafe(values, at(keys, 1i32)))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_SUITE_END();

TEST_SUITE_BEGIN("primestruct.semantics.calls_flow.effects");

TEST_CASE("boolean literal validates") {
  const std::string source = R"(
[return<bool>]
main() {
  return(false)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("if statement sugar validates") {
  const std::string source = R"(
[return<int>]
main() {
  [i32 mut] value{1i32}
  if(true) {
    assign(value, 2i32)
  } else {
    assign(value, 3i32)
  }
  return(value)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("return inside if block validates") {
  const std::string source = R"(
[return<int>]
main() {
  if(true) {
    return(2i32)
  } else {
    return(3i32)
  }
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("if blocks ignore colliding then definition") {
  const std::string source = R"(
[return<void>]
then() {
  return()
}

[return<int>]
main() {
  if(true) {
    return(1i32)
  } else {
    return(2i32)
  }
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("missing return on some control paths fails") {
  const std::string source = R"(
[return<int>]
main() {
  if(true, then(){ return(2i32) }, else(){ })
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("not all control paths return") != std::string::npos);
}


TEST_CASE("return after partial if validates") {
  const std::string source = R"(
[return<int>]
main() {
  if(true, then(){ return(2i32) }, else(){ })
  return(3i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("return not allowed in execution body") {
  const std::string source = R"(
[return<int>]
main() {
  return(1i32)
}

[return<int>]
execute_repeat([i32] x) {
  return(x)
}

execute_repeat(1i32) { return(2i32) }
)";
  std::string error;
  CHECK_FALSE(parseProgramWithError(source, error));
  CHECK(error.find("return not allowed in execution body") != std::string::npos);
}

TEST_CASE("print requires io_out effect") {
  const std::string source = R"(
[effects(io_err)]
main() {
  print("hello"utf8)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("io_out") != std::string::npos);
}

TEST_CASE("execution effects must be subset of definition effects") {
  const std::string source = R"(
[return<void>]
noop() {
}

[effects(io_out) return<void>]
main() {
  [effects(io_err)] noop()
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("execution effects must be a subset of enclosing effects") != std::string::npos);
}

TEST_CASE("execution effects scope vector literals") {
  const std::string source = R"(
[effects(heap_alloc io_out) return<void>]
main() {
  [effects(io_out)] vector<i32>(1i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("vector literal requires heap_alloc effect") != std::string::npos);
}

TEST_CASE("implicit default effects allow print") {
  const std::string source = R"(
main() {
  print_line("hello"utf8)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("print_error requires io_err effect") {
  const std::string source = R"(
[effects(io_out)]
main() {
  print_error("oops"utf8)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("io_err") != std::string::npos);
}

TEST_CASE("print_line_error requires io_err effect") {
  const std::string source = R"(
[effects(io_out)]
main() {
  print_line_error("oops"utf8)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("io_err") != std::string::npos);
}

TEST_CASE("notify requires pathspace_notify effect") {
  const std::string source = R"(
main() {
  notify("/events/test"utf8, 1i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("pathspace_notify") != std::string::npos);
}

TEST_CASE("notify rejects non-string path argument") {
  const std::string source = R"(
[effects(pathspace_notify)]
main() {
  notify(1i32, 2i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("requires string path argument") != std::string::npos);
}

TEST_CASE("notify accepts string array access") {
  const std::string source = R"(
[effects(pathspace_notify)]
main() {
  [array<string>] values{array<string>("a"utf8)}
  notify(values[0i32], 1i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("notify accepts string map access") {
  const std::string source = R"(
[effects(pathspace_notify)]
main() {
  [map<i32, string>] values{map<i32, string>(1i32, "/events/test"utf8)}
  notify(values[1i32], 1i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("notify rejects template arguments") {
  const std::string source = R"(
[effects(pathspace_notify)]
main() {
  notify<i32>("/events/test"utf8, 1i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("notify does not accept template arguments") != std::string::npos);
}

TEST_CASE("notify rejects argument count mismatch") {
  const std::string source = R"(
[effects(pathspace_notify)]
main() {
  notify("/events/test"utf8)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("notify requires exactly 2 arguments") != std::string::npos);
}

TEST_CASE("notify rejects block arguments") {
  const std::string source = R"(
[effects(pathspace_notify)]
main() {
  notify("/events/test"utf8, 1i32) { 2i32 }
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("notify does not accept block arguments") != std::string::npos);
}

TEST_CASE("insert requires pathspace_insert effect") {
  const std::string source = R"(
main() {
  insert("/events/test"utf8, 1i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("pathspace_insert") != std::string::npos);
}

TEST_CASE("insert rejects template arguments") {
  const std::string source = R"(
[effects(pathspace_insert)]
main() {
  insert<i32>("/events/test"utf8, 1i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("insert does not accept template arguments") != std::string::npos);
}

TEST_CASE("insert rejects non-string path argument") {
  const std::string source = R"(
[effects(pathspace_insert)]
main() {
  insert(1i32, 2i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("requires string path argument") != std::string::npos);
}

TEST_CASE("insert accepts string array access") {
  const std::string source = R"(
[effects(pathspace_insert)]
main() {
  [array<string>] values{array<string>("/events/test"utf8)}
  insert(values[0i32], 1i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("insert rejects block arguments") {
  const std::string source = R"(
[effects(pathspace_insert)]
main() {
  insert("/events/test"utf8, 1i32) { 2i32 }
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("insert does not accept block arguments") != std::string::npos);
}

TEST_CASE("insert rejects argument count mismatch") {
  const std::string source = R"(
[effects(pathspace_insert)]
main() {
  insert("/events/test"utf8)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("insert requires exactly 2 arguments") != std::string::npos);
}

TEST_CASE("insert not allowed in expression context") {
  const std::string source = R"(
[return<int> effects(pathspace_insert)]
main() {
  return(insert("/events/test"utf8, 1i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("statement-only") != std::string::npos);
}

TEST_CASE("take requires pathspace_take effect") {
  const std::string source = R"(
main() {
  take("/events/test"utf8)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("pathspace_take") != std::string::npos);
}

TEST_CASE("take rejects template arguments") {
  const std::string source = R"(
[effects(pathspace_take)]
main() {
  take<i32>("/events/test"utf8)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("take does not accept template arguments") != std::string::npos);
}

TEST_CASE("take rejects non-string path argument") {
  const std::string source = R"(
[effects(pathspace_take)]
main() {
  take(1i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("requires string path argument") != std::string::npos);
}

TEST_CASE("take accepts string map access") {
  const std::string source = R"(
[effects(pathspace_take)]
main() {
  [map<i32, string>] values{map<i32, string>(1i32, "/events/test"utf8)}
  take(values[1i32])
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("take rejects block arguments") {
  const std::string source = R"(
[effects(pathspace_take)]
main() {
  take("/events/test"utf8) { 1i32 }
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("take does not accept block arguments") != std::string::npos);
}

TEST_CASE("take rejects argument count mismatch") {
  const std::string source = R"(
[effects(pathspace_take)]
main() {
  take()
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("take requires exactly 1 argument") != std::string::npos);
}

TEST_CASE("take not allowed in expression context") {
  const std::string source = R"(
[return<int> effects(pathspace_take)]
main() {
  return(take("/events/test"utf8))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("statement-only") != std::string::npos);
}

TEST_CASE("notify not allowed in expression context") {
  const std::string source = R"(
[return<int> effects(pathspace_notify)]
main() {
  return(notify("/events/test"utf8, 1i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("statement-only") != std::string::npos);
}

TEST_CASE("string literal rejects unknown suffix") {
  const std::string source = R"(
[effects(io_out)]
main() {
  print("hello"utf16)
}
)";
  primec::Lexer lexer(source);
  primec::Parser parser(lexer.tokenize());
  primec::Program program;
  std::string error;
  CHECK_FALSE(parser.parse(program, error));
  CHECK(error.find("unknown string literal suffix") != std::string::npos);
}

TEST_CASE("ascii string literal rejects non-ASCII characters") {
  const std::string source = R"(
[effects(io_out)]
main() {
  print("héllo"ascii)
}
)";
  primec::Lexer lexer(source);
  primec::Parser parser(lexer.tokenize());
  primec::Program program;
  std::string error;
  CHECK_FALSE(parser.parse(program, error));
  CHECK(error.find("ascii string literal contains non-ASCII characters") != std::string::npos);
}

TEST_CASE("string literal rejects unknown escape sequences") {
  const std::string source = R"(
[effects(io_out)]
main() {
  print("hello\q"utf8)
}
)";
  primec::Lexer lexer(source);
  primec::Parser parser(lexer.tokenize());
  primec::Program program;
  std::string error;
  CHECK_FALSE(parser.parse(program, error));
  CHECK(error.find("unknown escape sequence") != std::string::npos);
}

TEST_CASE("print not allowed in expression context") {
  const std::string source = R"(
[return<int> effects(io_out)]
main() {
  return(print_line("hello"utf8))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("only supported as a statement") != std::string::npos);
}

TEST_CASE("print rejects pointer argument") {
  const std::string source = R"(
[effects(io_out) return<int>]
main() {
  [i32] value{1i32}
  print(location(value))
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("requires a numeric/bool or string literal/binding argument") != std::string::npos);
}

TEST_CASE("print rejects collection argument") {
  const std::string source = R"(
[effects(io_out)]
main() {
  print(array<i32>(1i32, 2i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("requires a numeric/bool or string literal/binding argument") != std::string::npos);
}

TEST_CASE("print rejects missing arguments") {
  const std::string source = R"(
[effects(io_out)]
main() {
  print()
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("print requires exactly one argument") != std::string::npos);
}

TEST_CASE("print_line rejects block arguments") {
  const std::string source = R"(
[effects(io_out)]
main() {
  print_line(1i32) { 2i32 }
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("print_line does not accept block arguments") != std::string::npos);
}

TEST_CASE("print_line rejects missing arguments") {
  const std::string source = R"(
[effects(io_out)]
main() {
  print_line()
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("print_line requires exactly one argument") != std::string::npos);
}

TEST_CASE("print_error rejects missing arguments") {
  const std::string source = R"(
[effects(io_err)]
main() {
  print_error()
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("print_error requires exactly one argument") != std::string::npos);
}

TEST_CASE("print_error rejects block arguments") {
  const std::string source = R"(
[effects(io_err)]
main() {
  print_error("oops"utf8) { 1i32 }
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("print_error does not accept block arguments") != std::string::npos);
}

TEST_CASE("print_line_error rejects missing arguments") {
  const std::string source = R"(
[effects(io_err)]
main() {
  print_line_error()
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("print_line_error requires exactly one argument") != std::string::npos);
}

TEST_CASE("print_line_error rejects block arguments") {
  const std::string source = R"(
[effects(io_err)]
main() {
  print_line_error("oops"utf8) { 1i32 }
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("print_line_error does not accept block arguments") != std::string::npos);
}

TEST_CASE("array literal rejects block arguments") {
  const std::string source = R"(
[return<int>]
main() {
  array<i32>(1i32) { 2i32 }
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("array literal does not accept block arguments") != std::string::npos);
}

TEST_CASE("map literal rejects block arguments") {
  const std::string source = R"(
[return<int>]
main() {
  map<i32, i32>(1i32, 2i32) { 3i32 }
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("map literal does not accept block arguments") != std::string::npos);
}

TEST_CASE("vector literal rejects block arguments") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  vector<i32>(1i32, 2i32) { 3i32 }
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("vector literal does not accept block arguments") != std::string::npos);
}

TEST_CASE("print accepts string array access") {
  const std::string source = R"(
[effects(io_out)]
main() {
  [array<string>] values{array<string>("hi"utf8)}
  print_line(values[0i32])
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("print accepts string map access") {
  const std::string source = R"(
[effects(io_out)]
main() {
  [map<i32, string>] values{map<i32, string>(1i32, "hi"utf8)}
  print_line(values[1i32])
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("print rejects struct block value") {
  const std::string source = R"(
[struct]
Thing() {
  [i32] value{1i32}
}

[effects(io_out)]
main() {
  print_line(block(){
    [Thing] item{Thing()}
    item
  })
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("requires a numeric/bool or string literal/binding argument") != std::string::npos);
}

TEST_CASE("print accepts string binding") {
  const std::string source = R"(
[effects(io_out)]
main() {
  [string] greeting{"hi"utf8}
  print_line(greeting)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("print accepts bool binding") {
  const std::string source = R"(
[effects(io_out)]
main() {
  [bool] ready{true}
  print_line(ready)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("print accepts bool literal") {
  const std::string source = R"(
[effects(io_out)]
main() {
  print_line(true)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("print_error accepts bool binding") {
  const std::string source = R"(
[effects(io_err)]
main() {
  [bool] ready{true}
  print_error(ready)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("print_line_error accepts bool binding") {
  const std::string source = R"(
[effects(io_err)]
main() {
  [bool] ready{true}
  print_line_error(ready)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("default effects allow print") {
  const std::string source = R"(
[return<void>]
main() {
  print_line("hello"utf8)
}
)";
  std::string error;
  CHECK(validateProgramWithDefaults(source, "/main", {"io_out"}, error));
  CHECK(error.empty());
}

TEST_CASE("default effects allow print in execution body") {
  const std::string source = R"(
[return<void>]
execute_repeat([i32] count) {
}

[return<void>]
main() {
}

execute_repeat(1i32) {
  print_line("hello"utf8)
}
)";
  std::string error;
  CHECK(validateProgramWithDefaults(source, "/main", {"io_out"}, error));
  CHECK(error.empty());
}

TEST_CASE("default effects allow print_error") {
  const std::string source = R"(
[return<void>]
main() {
  print_line_error("oops"utf8)
}
)";
  std::string error;
  CHECK(validateProgramWithDefaults(source, "/main", {"io_err"}, error));
  CHECK(error.empty());
}

TEST_CASE("default effects allow vector literal") {
  const std::string source = R"(
[return<int>]
main() {
  vector<i32>(1i32)
  return(0i32)
}
)";
  std::string error;
  CHECK(validateProgramWithDefaults(source, "/main", {"heap_alloc"}, error));
  CHECK(error.empty());
}

TEST_CASE("default effects reject invalid names") {
  const std::string source = R"(
[return<void>]
main() {
}
)";
  std::string error;
  CHECK_FALSE(validateProgramWithDefaults(source, "/main", {"bad-effect"}, error));
  CHECK(error.find("invalid default effect: bad-effect") != std::string::npos);
}

TEST_SUITE_END();
