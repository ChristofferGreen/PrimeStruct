#include "third_party/doctest.h"

#include "../test_semantics_helpers.h"

TEST_SUITE_BEGIN("primestruct.semantics.bindings.assignments");

TEST_CASE("collections validate") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  array<i32>{1i32, 2i32, 3i32}
  vector<i32>{1i32, 2i32, 3i32}
  map<i32, i32>(1i32, 10i32, 2i32, 20i32)
  return(1i32)
}
)";
  std::string error;
  primec::Program program;
  REQUIRE(validateProgramCapturingProgram(source, "/main", error, program));
  CHECK(error.empty());

  const primec::Definition *mainDef = nullptr;
  for (const auto &def : program.definitions) {
    if (def.fullPath == "/main") {
      mainDef = &def;
      break;
    }
  }
  REQUIRE(mainDef != nullptr);
  REQUIRE(mainDef->statements.size() >= 3);
  CHECK(mainDef->statements[0].isBraceConstructor);
  CHECK(mainDef->statements[0].name == "array");
  CHECK(mainDef->statements[1].isBraceConstructor);
  CHECK(mainDef->statements[1].name == "vector");
  CHECK_FALSE(mainDef->statements[2].isBraceConstructor);
  // TODO-4683: the pair constructor ladder was deleted and calls are
  // rewritten to the single variadic entries constructor (map__ov1) at
  // monomorph time, so this pair-shaped call now resolves there instead of
  // a dedicated pair overload (map__ov4, which no longer exists).
  CHECK(mainDef->statements[2].name == "/std/collections/map/map__ov1__ta77c4e1cde0d2ba9");
}

TEST_CASE("collection literals reject labeled entries") {
  const std::string source = R"(
[return<int>]
main() {
  array<i32>{[value] 1i32}
  return(1i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("named arguments not supported for builtin calls") != std::string::npos);
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

TEST_CASE("map constructor odd argument count uses ordinary diagnostics") {
  const std::string source = R"(
import /std/collections/map

[effects(heap_alloc), return<int>]
main() {
  /std/collections/map/map<i32, i32>(1i32, 10i32, 2i32)
  return(1i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("argument count mismatch") != std::string::npos);
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

TEST_CASE("assign to indexed inferred vector element succeeds") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [mut] values{vector<i32>()}
  /std/collections/vector/push(values, 1i32)
  /std/collections/vector/push(values, 2i32)
  [i32] index{1i32}
  assign(values[index], 7i32)
  return(values[1i32])
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("assign to namespaced indexed inferred vector element succeeds") {
  const std::string source = R"(
import /std/collections/*

namespace foo {
[effects(heap_alloc), return<int>]
work() {
  [mut] values{vector<i32>()}
  /std/collections/vector/push(values, 1i32)
  values[0i32] = 9i32
  return(values[0i32])
}
}

[effects(heap_alloc), return<int>]
main() {
  return(/foo/work())
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("assign to std-namespaced indexed inferred vector element succeeds") {
  const std::string source = R"(
import /std/collections/*

namespace std {
namespace image {
[effects(heap_alloc), return<int>]
work() {
  [mut] values{vector<i32>()}
  /std/collections/vector/push(values, 1i32)
  values[0i32] = 9i32
  return(values[0i32])
}
}
}

[effects(heap_alloc), return<int>]
main() {
  return(/std/image/work())
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

TEST_CASE("assign to namespaced indexed vector field succeeds") {
  const std::string source = R"(
import /std/collections/*

namespace shapes {
namespace grid {
[public struct]
Grid() {
  [vector<i32> mut] rectXs{vector<i32>()}
}

namespace Grid {
  [return<void>]
  assign_rect([Grid mut] self, [i32] nodeId, [i32] x) {
    self.rectXs[nodeId] = x
  }
}
}
}

[effects(heap_alloc), return<int>]
main() {
  [/shapes/grid/Grid mut] tree{/shapes/grid/Grid()}
  /std/collections/vector/push(tree.rectXs, 1i32)
  tree.assign_rect(0i32, 9i32)
  return(tree.rectXs[0i32])
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

namespace {

struct AssignDiagnostic {
  std::string message;
  int line = 0;
  int column = 0;
};

AssignDiagnostic firstAssignDiagnostic(const std::string &source) {
  std::string error;
  primec::SemanticDiagnosticInfo info;
  CHECK_FALSE(validateProgramCollectingDiagnostics(source, "/main", error, info));
  AssignDiagnostic found;
  if (!info.records.empty()) {
    found.message = info.records.front().message;
    found.line = info.records.front().primarySpan.line;
    found.column = info.records.front().primarySpan.column;
  }
  return found;
}

} // namespace

TEST_CASE("assigning a scalar of another type to a local is a diagnostic with a span") {
  const std::string source = R"(
[return<int>]
main() {
  [i64 mut] other{2i64}
  assign(other, 2.5f64)
  return(0i32)
}
)";
  const AssignDiagnostic found = firstAssignDiagnostic(source);
  CHECK(found.message.find("assign value type mismatch: expected i64, got f64") == 0);
  CHECK(found.line == 5);
  CHECK(found.column > 0);
}

TEST_CASE("assigning a bool, an f32 or an i32 to a local of another scalar type is a diagnostic") {
  SUBCASE("bool to i64") {
    const AssignDiagnostic found = firstAssignDiagnostic(R"(
[return<int>]
main() {
  [i64 mut] other{2i64}
  assign(other, true)
  return(0i32)
}
)");
    CHECK(found.message.find("assign value type mismatch: expected i64, got bool") == 0);
  }
  SUBCASE("f64 to f32") {
    const AssignDiagnostic found = firstAssignDiagnostic(R"(
[return<int>]
main() {
  [f32 mut] other{1.0f32}
  assign(other, 2.5f64)
  return(0i32)
}
)");
    CHECK(found.message.find("assign value type mismatch: expected f32, got f64") == 0);
  }
  SUBCASE("i32 to bool") {
    const AssignDiagnostic found = firstAssignDiagnostic(R"(
[return<int>]
main() {
  [bool mut] flag{true}
  assign(flag, 1i32)
  return(0i32)
}
)");
    CHECK(found.message.find("assign value type mismatch: expected bool, got i32") == 0);
  }
}

TEST_CASE("assigning a scalar of another type through a Reference parameter is a diagnostic") {
  const AssignDiagnostic found = firstAssignDiagnostic(R"(
[return<void>]
put([Reference<i64> mut] r) {
  assign(dereference(r), 2.5f64)
}

[return<int>]
main() {
  [i64 mut] target{1i64}
  put(target)
  return(0i32)
}
)");
  CHECK(found.message.find("assign value type mismatch: expected i64, got f64") == 0);
  CHECK(found.line == 4);
}

TEST_CASE("assigning a bool through a Reference parameter or a Reference field is a diagnostic") {
  SUBCASE("parameter") {
    const AssignDiagnostic found = firstAssignDiagnostic(R"(
[return<void>]
put([Reference<i64> mut] r) {
  assign(dereference(r), true)
}

[return<int>]
main() {
  [i64 mut] target{1i64}
  put(target)
  return(0i32)
}
)");
    CHECK(found.message.find("assign value type mismatch: expected i64, got bool") == 0);
  }
  SUBCASE("field") {
    const AssignDiagnostic found = firstAssignDiagnostic(R"(
[struct]
Keeper() {
  [Reference<i64> mut] kept

  [return<void>]
  flip() {
    assign(dereference(this.kept), true)
  }
}

[return<int>]
main() {
  [i64 mut] target{1i64}
  [Keeper mut] keeper{Keeper{location(target)}}
  keeper.flip()
  return(0i32)
}
)");
    CHECK(found.message.find("assign value type mismatch: expected i64, got bool") == 0);
  }
}

TEST_CASE("assigning a value of the target's scalar type validates for every scalar") {
  const std::string source = R"(
[return<void>]
put([Reference<i64> mut] r) {
  assign(dereference(r), 7i64)
}

[return<int>]
main() {
  [i32 mut] a{1i32}
  [i64 mut] b{1i64}
  [u64 mut] c{1u64}
  [u8 mut] d{1u8}
  [f32 mut] e{1.0f32}
  [f64 mut] f{1.0f64}
  [bool mut] g{true}
  assign(a, 2i32)
  assign(b, plus(2i64, b))
  assign(c, 2u64)
  assign(d, 2u8)
  assign(e, 2.5f32)
  assign(f, 2.5f64)
  assign(g, false)
  put(b)
  return(a)
}
)";
  std::string error;
  CHECK_MESSAGE(validateProgram(source, "/main", error), error);
}

TEST_SUITE_END();
