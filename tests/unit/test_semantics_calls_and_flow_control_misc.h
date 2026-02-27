TEST_SUITE_BEGIN("primestruct.semantics.calls_flow.control");

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

TEST_CASE("expression effects narrow active effects") {
  const std::string source = R"(
[effects(heap_alloc, io_out), return<int>]
main() {
  return([effects(io_out)] vector<i32>(1i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("vector literal requires heap_alloc effect") != std::string::npos);
}

TEST_CASE("expression effects must be subset of enclosing") {
  const std::string source = R"(
[return<int>]
main() {
  return([effects(pathspace_notify)] plus(1i32, 2i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("execution effects must be a subset of enclosing effects") != std::string::npos);
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

TEST_CASE("loop rejects negative i64 count") {
  const std::string source = R"(
[return<int>]
main() {
  loop(-1i64, do(){ })
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("loop count must be non-negative") != std::string::npos);
}

TEST_CASE("loop body propagates statement validation errors") {
  const std::string source = R"(
[return<int>]
main() {
  [i32] value{1i32}
  loop(1i32, do() {
    assign(value, 2i32)
  })
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("assign target must be a mutable binding") != std::string::npos);
}

TEST_CASE("loop rejects template arguments in suite") {
  const std::string source = R"(
[return<int>]
main() {
  loop<i32>(2i32, do(){ })
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("loop does not accept template arguments") != std::string::npos);
}

TEST_CASE("while rejects template arguments in suite") {
  const std::string source = R"(
[return<int>]
main() {
  while<bool>(true, do(){ })
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("while does not accept template arguments") != std::string::npos);
}

TEST_CASE("for rejects template arguments in suite") {
  const std::string source = R"(
[return<int>]
main() {
  for<i32>([i32 mut] i{0i32}, less_than(i, 2i32), assign(i, plus(i, 1i32)), do(){ })
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("for does not accept template arguments") != std::string::npos);
}

TEST_CASE("while requires condition and body in suite") {
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

TEST_CASE("for requires init condition step and body in suite") {
  const std::string source = R"(
[return<int>]
main() {
  for([i32 mut] i{0i32}, less_than(i, 2i32), assign(i, plus(i, 1i32)))
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("for requires init, condition, step, and body") != std::string::npos);
}

TEST_CASE("repeat rejects named arguments in suite") {
  const std::string source = R"(
[return<int>]
main() {
  repeat([count] 1i32) {
  }
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("named arguments not supported for builtin calls") != std::string::npos);
}

TEST_CASE("loop requires count and body in suite") {
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

TEST_CASE("loop rejects trailing block arguments in suite") {
  const std::string source = R"(
[return<int>]
main() {
  loop(1i32, do(){ }) {
    [i32] temp{0i32}
  }
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("loop does not accept trailing block arguments") != std::string::npos);
}

TEST_CASE("while rejects trailing block arguments in suite") {
  const std::string source = R"(
[return<int>]
main() {
  while(true, do(){ }) {
    [i32] temp{0i32}
  }
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("while does not accept trailing block arguments") != std::string::npos);
}

TEST_CASE("for rejects trailing block arguments in suite") {
  const std::string source = R"(
[return<int>]
main() {
  for([i32 mut] i{0i32}, less_than(i, 1i32), assign(i, plus(i, 1i32)), do(){ }) {
    [i32] temp{0i32}
  }
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("for does not accept trailing block arguments") != std::string::npos);
}

TEST_CASE("loop count requires integer in suite") {
  const std::string source = R"(
[return<int>]
main() {
  loop(1.5f32, do(){ })
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("loop count requires integer") != std::string::npos);
}

TEST_CASE("while condition requires bool in suite") {
  const std::string source = R"(
[return<int>]
main() {
  while(1i32, do(){ })
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("while condition requires bool") != std::string::npos);
}

TEST_CASE("for condition expression requires bool in suite") {
  const std::string source = R"(
[return<int>]
main() {
  for([i32 mut] i{0i32}, 1i32, assign(i, plus(i, 1i32)), do(){ })
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("for condition requires bool") != std::string::npos);
}

TEST_CASE("for condition binding requires bool in suite") {
  const std::string source = R"(
[return<int>]
main() {
  for([i32 mut] i{0i32} [i32] keep{1i32} assign(i, plus(i, 1i32))) {
  }
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("for condition requires bool") != std::string::npos);
}

TEST_CASE("loop count expression validation failure in suite") {
  const std::string source = R"(
[return<int>]
main() {
  loop(missingCount, do(){ })
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown identifier") != std::string::npos);
}

TEST_CASE("while condition expression validation failure in suite") {
  const std::string source = R"(
[return<int>]
main() {
  while(missingCond, do(){ })
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown identifier") != std::string::npos);
}

TEST_CASE("for condition expression validation failure in suite") {
  const std::string source = R"(
[return<int>]
main() {
  for([i32 mut] i{0i32}, missingCond, assign(i, plus(i, 1i32)), do(){ })
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown identifier") != std::string::npos);
}

TEST_SUITE_END();
