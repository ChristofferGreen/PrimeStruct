#include "third_party/doctest.h"

#include "test_semantics_helpers.h"

TEST_SUITE_BEGIN("primestruct.semantics.bindings.assignments");

TEST_CASE("collections validate") {
  const std::string source = R"(
[return<int>]
main() {
  array<i32>(1i32, 2i32, 3i32)
  map<i32, i32>(1i32, 10i32, 2i32, 20i32)
  return(1i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("array requires one template argument") {
  const std::string source = R"(
[return<int>]
main() {
  array(1i32, 2i32)
  return(1i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("array literal requires exactly one template argument") != std::string::npos);
}

TEST_CASE("array rejects envelope-level length template arg") {
  const std::string source = R"(
[return<int>]
main() {
  array<i32, i32>(1i32, 2i32)
  return(1i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("array<T, N> is unsupported; use array<T> (runtime-count array)") != std::string::npos);
}

TEST_CASE("map requires even number of arguments") {
  const std::string source = R"(
[return<int>]
main() {
  map<i32, i32>(1i32, 10i32, 2i32)
  return(1i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("map literal requires an even number of arguments") != std::string::npos);
}

TEST_CASE("assign to mutable binding succeeds") {
  const std::string source = R"(
[return<int>]
main() {
  [i32 mut] value{1i32}
  assign(value, 4i32)
  return(value)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("assign to immutable binding fails") {
  const std::string source = R"(
[return<int>]
main() {
  [i32] value{1i32}
  assign(value, 2i32)
  return(value)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("assign target must be a mutable binding") != std::string::npos);
}

TEST_CASE("assign allowed in expression context") {
  const std::string source = R"(
[return<int>]
main() {
  [i32 mut] value{1i32}
  return(assign(value, 2i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("assign to inferred mutable binding from convert call succeeds") {
  const std::string source = R"(
[return<int>]
main() {
  [mut] current{convert<u64>(1i32)}
  assign(current, plus(current, 2u64))
  return(convert<int>(current))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("assign to indexed vector element succeeds") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32, 2i32)}
  [i32] index{1i32}
  assign(values[index], 7i32)
  return(values[1i32])
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("assign to indexed vector field succeeds") {
  const std::string source = R"(
import /std/collections/*

[struct]
Box {
  [vector<i32> mut] values
}

[effects(heap_alloc), return<int>]
main() {
  [Box mut] box{Box(vector<i32>(1i32, 2i32))}
  [i32] index{1i32}
  assign(box.values[index], 9i32)
  return(box.values[1i32])
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("pointer binding and dereference assignment validate") {
  const std::string source = R"(
[return<int>]
main() {
  [i32 mut] value{5i32}
  [Pointer<i32> mut] ptr{location(value)}
  assign(dereference(ptr), 4i32)
  return(value)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("reference participates in arithmetic") {
  const std::string source = R"(
[return<int>]
main() {
  [i32 mut] value{5i32}
  [Reference<i32> mut] ref{location(value)}
  assign(ref, plus(ref, 2i32))
  return(plus(1i32, ref))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("location accepts reference binding") {
  const std::string source = R"(
[return<int>]
main() {
  [i32 mut] value{8i32}
  [Reference<i32> mut] ref{location(value)}
  [Pointer<i32>] ptr{location(ref)}
  return(dereference(ptr))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("dereference requires pointer or reference") {
  const std::string source = R"(
[return<int>]
main() {
  [i32] value{5i32}
  return(dereference(value))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("dereference requires a pointer or reference") != std::string::npos);
}

TEST_CASE("literal statement validates") {
  const std::string source = R"(
[return<int>]
main() {
  1i32
  return(2i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("pointer assignment requires mutable binding") {
  const std::string source = R"(
[return<int>]
main() {
  [i32 mut] value{5i32}
  [Pointer<i32>] ptr{location(value)}
  assign(dereference(ptr), 4i32)
  return(value)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("assign target must be a mutable binding") != std::string::npos);
}

TEST_CASE("assign through non-pointer dereference fails") {
  const std::string source = R"(
[return<int>]
main() {
  [i32 mut] value{1i32}
  assign(dereference(value), 3i32)
  return(value)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("assign target must be a mutable pointer binding") != std::string::npos);
}

TEST_CASE("reference binding assigns to target") {
  const std::string source = R"(
[return<int>]
main() {
  [i32 mut] value{5i32}
  [Reference<i32> mut] ref{location(value)}
  assign(ref, 4i32)
  return(value)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("reference assignment requires mutable binding") {
  const std::string source = R"(
[return<int>]
main() {
  [i32 mut] value{5i32}
  [Reference<i32>] ref{location(value)}
  assign(ref, 4i32)
  return(value)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("assign target must be a mutable binding") != std::string::npos);
}

TEST_CASE("reference binding requires location") {
  const std::string source = R"(
[return<int>]
main() {
  [Reference<i32>] ref{5i32}
  return(1i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("Reference bindings require location") != std::string::npos);
}

TEST_CASE("reference binding rejects location pointer type mismatch") {
  const std::string source = R"(
[return<int>]
main() {
  [i32 mut] value{5i32}
  [Pointer<i32>] ptr{location(value)}
  [Reference<i32>] ref{location(ptr)}
  return(1i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("Reference binding type mismatch") != std::string::npos);
}

TEST_CASE("reference binding rejects location reference type mismatch") {
  const std::string source = R"(
[return<int>]
main() {
  [i64 mut] value{5i64}
  [Reference<i64>] base{location(value)}
  [Reference<i32>] ref{location(base)}
  return(1i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("Reference binding type mismatch") != std::string::npos);
}

TEST_CASE("safe scope accepts pointer to reference conversion initializer at semantics time") {
  const std::string source = R"(
[return<int>]
main() {
  [i32 mut] value{5i32}
  [Pointer<i32>] ptr{location(value)}
  [Reference<i32>] ref{ptr}
  return(1i32)
}
  )";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_SUITE_END();
