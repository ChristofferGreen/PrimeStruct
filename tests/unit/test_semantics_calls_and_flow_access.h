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
  CHECK(error.find("block arguments require a definition target") != std::string::npos);
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

TEST_CASE("method calls support templated definitions") {
  const std::string source = R"(
namespace i32 {
  [return<i32>]
  wrap<T>([i32] self) {
    return(self)
  }
}

[return<i32>]
main() {
  [i32] value{1i32}
  return(value.wrap<i32>())
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("templated method calls infer receiver types") {
  const std::string source = R"(
namespace i32 {
  [return<i32>]
  wrap<T>([i32] self) {
    return(self)
  }
}

[return<i32>]
compute([i32] input) {
  [mut] value{input}
  return(value.wrap<i32>())
}

[return<i32>]
main() {
  return(compute(1i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("method calls reject template args for block-inferred receiver") {
  const std::string source = R"(
namespace i32 {
  [return<i32>]
  inc([i32] self) {
    return(plus(self, 1i32))
  }
}

[return<i32>]
main() {
  [mut] value{1i32 2i32}
  return(value.inc<i32>())
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("template arguments are only supported on templated definitions: /i32/inc") != std::string::npos);
}

TEST_CASE("method calls reject template args on non-template defs") {
  const std::string source = R"(
namespace i32 {
  [return<i32>]
  inc([i32] self) {
    return(plus(self, 1i32))
  }
}

[return<i32>]
main() {
  [i32] value{1i32}
  return(value.inc<i32>())
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("template arguments are only supported on templated definitions: /i32/inc") != std::string::npos);
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

TEST_CASE("method calls resolve on struct constructor expressions") {
  const std::string source = R"(
[struct]
Foo() {
  [i32] value{1i32}
}

[return<int>]
/Foo/ping([Foo] self) {
  return(9i32)
}

[return<int>]
main() {
  return(Foo().ping())
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
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
  CHECK(error.find("block arguments require a definition target") != std::string::npos);
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
  CHECK(error.find("block arguments require a definition target") != std::string::npos);
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
  CHECK(error.find("block arguments require a definition target") != std::string::npos);
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
