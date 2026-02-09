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

TEST_CASE("reference participates in signedness checks") {
  const std::string source = R"(
[return<int>]
main() {
  [u64 mut] value(1u64)
  [Reference<u64>] ref(location(value))
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
  [i32 mut] value(1i32)
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
    [i32] temp(3i32)
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
  return(block{
    [mut] value("hello"utf8)
    equal(value, "hello"utf8)
  })
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("if missing else fails") {
  const std::string source = R"(
[return<int>]
main() {
  [i32 mut] value(1i32)
  if(true, then{ assign(value, 2i32) })
  return(value)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("if requires condition") != std::string::npos);
}

TEST_CASE("binding not allowed in expression") {
  const std::string source = R"(
[return<int>]
main() {
  return([i32] value(1i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("binding not allowed in expression") != std::string::npos);
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
  [i32] a(count("abc"utf8))
  [i32] b(count("hi"utf8))
  return(plus(a, b))
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
  return(foo(a = 1i32, a = 2i32))
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

TEST_CASE("named arguments match parameters") {
  const std::string source = R"(
[return<int>]
foo([i32] a, [i32] b) {
  return(a)
}

[return<int>]
main() {
  return(foo(a = 1i32, b = 2i32))
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
  return(foo(b = 2i32, a = 1i32))
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
  [i32] value(1i32)
}

[return<int>]
/Foo/plus([Foo] self, [i32] rhs) {
  return(rhs)
}

[return<int>]
main() {
  [Foo] item(1i32)
  return(item.plus(rhs = 2i32))
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
  return(array<i32>(1i32, 2i32).count(value = 0i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("named arguments not supported for builtin calls") != std::string::npos);
}

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
  [array<i32>] values(array<i32>(1i32, 2i32))
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
  [map<i32, i32>] values(map<i32, i32>(1i32, 2i32))
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

TEST_CASE("count method validates on map binding") {
  const std::string source = R"(
[return<int>]
main() {
  [map<i32, i32>] values(map<i32, i32>(1i32, 2i32))
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
  [string] text("abc"utf8)
  return(text.count())
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("count helper validates on string binding") {
  const std::string source = R"(
[return<int>]
main() {
  [string] text("abc"utf8)
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
  [i32] value(1i32)
}

[return<int>]
/Foo/count([Foo] self) {
  return(7i32)
}

[return<int>]
main() {
  [Foo] item(1i32)
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
  [i32] value(1i32)
}

[return<int>]
main() {
  [Foo] item(1i32)
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
  [array<i32>] items(array<i32>(4i32, 7i32))
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
  [map<i32, i32>] items(map<i32, i32>(1i32, 2i32))
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
  [i32] value(1i32)
  [Pointer<i32>] ptr(location(value))
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
  [i32] value(1i32)
}

[return<int>]
main() {
  [Foo] item(Foo())
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
  [string] text("hello"utf8)
  return(at(text, "nope"utf8))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("at requires integer index") != std::string::npos);
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

TEST_CASE("unsafe string access rejects non-integer index") {
  const std::string source = R"(
[return<int>]
main() {
  [string] text("hello"utf8)
  return(at_unsafe(text, "nope"utf8))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("at_unsafe requires integer index") != std::string::npos);
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

TEST_CASE("array access rejects non-collection target") {
  const std::string source = R"(
[return<int>]
main() {
  return(at(1i32, 0i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("at requires array, map, or string target") != std::string::npos);
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
  CHECK(error.find("at_unsafe requires array, map, or string target") != std::string::npos);
}

TEST_CASE("unknown named argument fails") {
  const std::string source = R"(
[return<int>]
foo([i32] a, [i32] b) {
  return(a)
}

[return<int>]
main() {
  return(foo(c = 1i32, b = 2i32))
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
  return(foo(1i32, a = 2i32))
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

job(a = 1i32, b = 2i32)
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("named arguments not allowed on builtin calls") {
  const std::string source = R"(
[return<int>]
main() {
  return(plus(left = 1i32, right = 2i32))
}
)";
  primec::Lexer lexer(source);
  primec::Parser parser(lexer.tokenize());
  primec::Program program;
  std::string error;
  CHECK_FALSE(parser.parse(program, error));
  CHECK(error.find("named arguments not supported for builtin calls") != std::string::npos);
}

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

TEST_CASE("convert<bool> rejects float operand") {
  const std::string source = R"(
[return<bool>]
main() {
  return(convert<bool>(1.5f))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("convert<bool> requires integer or bool operand") != std::string::npos);
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
  [map<i32, i32>] values(map<i32, i32>(1i32, 2i32))
  return(at(values, "nope"utf8))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("at requires map key type i32") != std::string::npos);
}

TEST_CASE("unsafe map access validates key type") {
  const std::string source = R"(
[return<int>]
main() {
  [map<i32, i32>] values(map<i32, i32>(1i32, 2i32))
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
  [map<string, i32>] values(map<string, i32>("a"utf8, 3i32))
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
  [map<string, i32>] values(map<string, i32>("a"utf8, 3i32))
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
  [map<string, i32>] values(map<string, i32>("a"utf8, 3i32))
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
  [map<string, i32>] values(map<string, i32>("a"utf8, 1i32))
  [map<i32, string>] keys(map<i32, string>(1i32, "a"utf8))
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
  [map<string, i32>] values(map<string, i32>("a"utf8, 1i32))
  [map<i32, string>] keys(map<i32, string>(1i32, "a"utf8))
  return(at_unsafe(values, at(keys, 1i32)))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

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
  [i32 mut] value(1i32)
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

TEST_CASE("missing return on some control paths fails") {
  const std::string source = R"(
[return<int>]
main() {
  if(true, then{ return(2i32) }, else{ })
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
  if(true, then{ return(2i32) }, else{ })
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
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("return not allowed in execution body") != std::string::npos);
}

TEST_CASE("print requires io_out effect") {
  const std::string source = R"(
main() {
  print("hello"utf8)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("io_out") != std::string::npos);
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
  [array<string>] values(array<string>("a"utf8))
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
  [map<i32, string>] values(map<i32, string>(1i32, "/events/test"utf8))
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
  [array<string>] values(array<string>("/events/test"utf8))
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
  [map<i32, string>] values(map<i32, string>(1i32, "/events/test"utf8))
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
  [i32] value(1i32)
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
  array<i32>{1i32}
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
  map<i32, i32>{1i32, 2i32}
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("map literal does not accept block arguments") != std::string::npos);
}

TEST_CASE("print accepts string array access") {
  const std::string source = R"(
[effects(io_out)]
main() {
  [array<string>] values(array<string>("hi"utf8))
  print_line(values[0i32])
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
  [i32] value(1i32)
}

[effects(io_out)]
main() {
  print_line(block{
    [Thing] item(Thing())
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
  [string] greeting("hi"utf8)
  print_line(greeting)
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
