#include "third_party/doctest.h"

#include "test_semantics_helpers.h"

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

TEST_CASE("count helper validates on variadic args parameter") {
  const std::string source = R"(
[return<int>]
collect(values...) {
  return(count(values))
}

[return<int>]
main() {
  return(collect(1i32, 2i32, 3i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("count method validates on variadic args parameter") {
  const std::string source = R"(
[return<int>]
collect(values...) {
  return(values.count())
}

[return<int>]
main() {
  return(collect(1i32, 2i32, 3i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("count on variadic args still rejects extra arguments") {
  const std::string source = R"(
[return<int>]
collect(values...) {
  return(count(values, 1i32))
}

[return<int>]
main() {
  return(collect(1i32, 2i32, 3i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("argument count mismatch for builtin count") != std::string::npos);
}

TEST_CASE("variadic args access helper validates") {
  const std::string source = R"(
[return<int>]
collect(values...) {
  return(at(values, 0i32))
}

[return<int>]
main() {
  return(collect(1i32, 2i32, 3i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("variadic args access method validates") {
  const std::string source = R"(
[return<int>]
collect(values...) {
  return(values.at(0i32))
}

[return<int>]
main() {
  return(collect(1i32, 2i32, 3i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("variadic args unsafe access method validates") {
  const std::string source = R"(
[return<int>]
collect(values...) {
  return(values.at_unsafe(0i32))
}

[return<int>]
main() {
  return(collect(1i32, 2i32, 3i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("variadic args index validates") {
  const std::string source = R"(
[return<int>]
collect(values...) {
  return(values[0i32])
}

[return<int>]
main() {
  return(collect(1i32, 2i32, 3i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("variadic args index supports string method resolution") {
  const std::string source = R"(
[return<int>]
collect([string] values...) {
  return(values[0i32].count())
}

[return<int>]
main() {
  return(collect("hello"utf8, "bye"utf8))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("variadic args index rejects non-integer index") {
  const std::string source = R"(
[return<int>]
collect(values...) {
  return(values["nope"utf8])
}

[return<int>]
main() {
  return(collect(1i32, 2i32, 3i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("at requires integer index") != std::string::npos);
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
  return(/std/collections/map/count(items))
}

[return<int>]
/std/collections/map/count([map<i32, i32>] items) {
  return(1i32)
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
import /std/collections/*

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

TEST_CASE("templated method call on user-defined vector call receiver resolves by return type") {
  const std::string source = R"(
namespace vector {
  [return<i32>]
  wrap([i32] self) {
    return(plus(self, 10i32))
  }
}

namespace i32 {
  [return<i32>]
  wrap<T>([i32] self) {
    return(self)
  }
}

[return<i32>]
vector([i32] value) {
  return(value)
}

[return<i32>]
main() {
  return(vector(1i32).wrap<i32>())
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
  CHECK(error.find("unknown method: /Pointer/count") != std::string::npos);
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

#include "test_semantics_calls_and_flow_access_indexing.h"
