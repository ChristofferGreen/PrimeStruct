#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <vector>

#include "test_semantics_helpers.h"

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

TEST_CASE("bare map count call validates through builtin fallback without imported canonical helper") {
  const std::string source = R"(
[return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 2i32)}
  return(count(values))
}
  )";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /std/collections/map/count") != std::string::npos);
}

TEST_CASE("bare map count call resolves through canonical helper definition") {
  const std::string source = R"(
[return<int>]
/std/collections/map/count([map<i32, i32>] values) {
  return(17i32)
}

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

TEST_CASE("imported canonical map count validates builtin map method receivers") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return</std/collections/map/MapValue<K, V>>]
wrapMap<K, V>([K] key, [V] value) {
  return(map<K, V>(key, value))
}

[effects(heap_alloc), return<int>]
main() {
  [/std/collections/map/MapValue<string, i32>] values{
      map<string, i32>("left"raw_utf8, 10i32, "right"raw_utf8, 15i32)}
  return(plus(values.count(), wrapMap<string, i32>("only"raw_utf8, 4i32).count()))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("imported canonical map count keeps borrowed args pack count_ref diagnostics") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
score_refs([args<Reference</std/collections/map<i32, i32>>>] values) {
  return(plus(values[0i32].count(),
              dereference(at(values, 1i32)).count()))
}

[return<int>]
score_ptrs([args<Pointer</std/collections/map<i32, i32>>>] values) {
  return(plus(values[0i32].count(),
              dereference(at(values, 1i32)).count()))
}

[return<int>]
main() {
  [/std/collections/map<i32, i32>] left{map<i32, i32>(1i32, 2i32)}
  [/std/collections/map<i32, i32>] right{map<i32, i32>(3i32, 4i32, 5i32, 6i32)}
  [Reference</std/collections/map<i32, i32>>] left_ref{location(left)}
  [Reference</std/collections/map<i32, i32>>] right_ref{location(right)}
  [Pointer</std/collections/map<i32, i32>>] left_ptr{location(left)}
  [Pointer</std/collections/map<i32, i32>>] right_ptr{location(right)}
  return(plus(score_refs(left_ref, right_ref),
              score_ptrs(left_ptr, right_ptr)))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /std/collections/map/count_ref") != std::string::npos);
}

TEST_CASE("bare map count call rejects when only compatibility alias is present") {
  const std::string source = R"(
[return<int>]
/map/count([map<i32, i32>] values) {
  return(19i32)
}

[return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 2i32)}
  return(count(values))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: count") != std::string::npos);
}

TEST_CASE("bare map count call keeps explicit root helper precedence") {
  const std::string source = R"(
[return<int>]
/count([map<i32, i32>] values) {
  return(23i32)
}

[return<int>]
/std/collections/map/count([map<i32, i32>] values) {
  return(17i32)
}

[return<int>]
/map/count([map<i32, i32>] values) {
  return(19i32)
}

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

TEST_CASE("bare map contains call requires imported canonical helper or explicit definition") {
  const std::string source = R"(
[return<bool>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 2i32)}
  return(contains(values, 1i32))
}
  )";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /std/collections/map/contains") != std::string::npos);
}

TEST_CASE("bare map contains call resolves through canonical helper definition") {
  const std::string source = R"(
[return<bool>]
/std/collections/map/contains([map<i32, i32>] values, [i32] key) {
  return(false)
}

[return<bool>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 2i32)}
  return(contains(values, 1i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("bare map contains call rejects compatibility alias when canonical helper is absent") {
  const std::string source = R"(
[return<bool>]
/map/contains([map<i32, i32>] values, [i32] key) {
  return(false)
}

[return<bool>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 2i32)}
  return(contains(values, 1i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /std/collections/map/contains") != std::string::npos);
}

TEST_CASE("bare map contains call keeps explicit root helper precedence") {
  const std::string source = R"(
[return<bool>]
/contains([map<i32, i32>] values, [i32] key) {
  return(false)
}

[return<bool>]
/std/collections/map/contains([map<i32, i32>] values, [bool] key) {
  return(true)
}

[return<bool>]
/map/contains([map<i32, i32>] values, [bool] key) {
  return(true)
}

[return<bool>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 2i32)}
  return(contains(values, 1i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("bare map tryAt call requires imported canonical helper or explicit definition") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 2i32)}
  return(try(tryAt(values, 1i32)))
}
  )";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /std/collections/map/tryAt") != std::string::npos);
}

TEST_CASE("bare map tryAt call rejects compatibility helper fallback") {
  const std::string source = R"(
[effects(heap_alloc), return<Result<i32, ContainerError>>]
/map/tryAt([map<i32, i32>] values, [i32] key) {
  return(Result.ok(5i32))
}

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 2i32)}
  return(try(tryAt(values, 1i32)))
}
  )";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /std/collections/map/tryAt") != std::string::npos);
}

TEST_CASE("map binding rejects unsupported builtin Comparable key contract") {
  const std::string source = R"(
[return<int>]
main() {
  [map<array<i32>, i32>] values{map<array<i32>, i32>()}
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("map requires builtin Comparable key type (i32, i64, u64, f32, f64, bool, or string): array<i32>") !=
        std::string::npos);
}

TEST_CASE("inferred map binding rejects unsupported builtin Comparable key contract") {
  const std::string source = R"(
[return<int>]
main() {
  [mut] values{map<array<i32>, i32>()}
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("map requires builtin Comparable key type (i32, i64, u64, f32, f64, bool, or string): array<i32>") !=
        std::string::npos);
}

TEST_CASE("experimental map custom comparable struct keys keep canonical map helper diagnostics") {
  const std::string source = R"(
import /std/collections/internal_map/*

[struct]
Key() {
  [i32] value{0i32}
}

[return<bool>]
/Key/equal([Key] left, [Key] right) {
  return(equal(left.value, right.value))
}

[return<bool>]
/Key/less_than([Key] left, [Key] right) {
  return(less_than(left.value, right.value))
}

[effects(heap_alloc), return<int>]
main() {
  [Map<Key, i32>] values{mapPair<Key, i32>(Key{2i32}, 7i32, Key{5i32}, 11i32)}
  [i32 mut] total{/std/collections/map/count<Key, i32>(values)}
  assign(total, plus(total, /std/collections/map/at<Key, i32>(values, Key{2i32})))
  assign(total, plus(total, /std/collections/map/at_unsafe<Key, i32>(values, Key{5i32})))
  if(/std/collections/map/contains<Key, i32>(values, Key{2i32}),
     then() { assign(total, plus(total, 1i32)) },
     else() { })
  return(total)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("map requires builtin Comparable key type (i32, i64, u64, f32, f64, bool, or string): Key") !=
        std::string::npos);
}

TEST_CASE("experimental map method-call sugar keeps missing Map helper diagnostics") {
  const std::string source = R"(
import /std/collections/internal_map/*

[return<int> effects(heap_alloc)]
main() {
  [Map<string, i32>] values{mapPair<string, i32>("left"raw_utf8, 4i32, "right"raw_utf8, 7i32)}
  return(values.count())
}
  )";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /std/collections/map/count") !=
        std::string::npos);
}

TEST_CASE("canonical map Ref helper calls accept borrowed map references") {
  const std::string source = R"(
import /std/collections/*

[return<int> effects(heap_alloc)]
main() {
  [map<string, i32>] values{map<string, i32>("left"raw_utf8, 4i32, "right"raw_utf8, 7i32)}
  [Reference<map<string, i32>>] ref{location(values)}
  [i32 mut] total{/std/collections/map/count_ref<string, i32>(ref)}
  assign(total, plus(total, /std/collections/map/at_ref<string, i32>(ref, "left"raw_utf8)))
  assign(total, plus(total, /std/collections/map/at_unsafe_ref<string, i32>(ref, "right"raw_utf8)))
  if(/std/collections/map/contains_ref<string, i32>(ref, "left"raw_utf8),
     then() { assign(total, plus(total, 1i32)) },
     else() { })
  return(total)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("public stdlib map Ref wrappers validate through canonical borrowed helpers") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/internal_map/*

[return<int> effects(heap_alloc)]
main() {
  [map<string, i32> mut] values{map<string, i32>("left"raw_utf8, 4i32)}
  /std/collections/map/insert<string, i32>(values, "right"raw_utf8, 7i32)
  [Reference<map<string, i32>> mut] ref{location(values)}
  /std/collections/map/insert_ref<string, i32>(ref, "third"raw_utf8, 11i32)
  [Result<i32, ContainerError>] found{/std/collections/map/tryAt_ref<string, i32>(ref, "left"raw_utf8)}
  [Result<i32, ContainerError>] missing{/std/collections/map/tryAt_ref<string, i32>(ref, "missing"raw_utf8)}
  [i32 mut] total{/std/collections/map/count_ref<string, i32>(ref)}
  assign(total, plus(total, /std/collections/map/at_ref<string, i32>(ref, "right"raw_utf8)))
  assign(total, plus(total, /std/collections/map/at_unsafe_ref<string, i32>(ref, "third"raw_utf8)))
  if(/std/collections/map/contains_ref<string, i32>(ref, "left"raw_utf8),
     then() { assign(total, plus(total, 1i32)) },
     else() { })
  if(not(/std/collections/map/contains_ref<string, i32>(ref, "missing"raw_utf8)),
     then() { assign(total, plus(total, 2i32)) },
     else() { })
  return(total)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("canonical map borrowed method-call sugar rejects missing ref template inference") {
  const std::string source = R"(
import /std/collections/*

[return<int> effects(heap_alloc)]
main() {
  [map<string, i32>] values{map<string, i32>("left"raw_utf8, 4i32, "right"raw_utf8, 7i32)}
  [Reference<map<string, i32>>] ref{location(values)}
  return(ref.count())
}
  )";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /std/collections/map/count_ref") !=
        std::string::npos);
}

TEST_CASE("canonical map insert helpers validate on value and borrowed mutation surfaces") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [map<string, i32> mut] values{map<string, i32>("left"raw_utf8, 4i32)}
  /std/collections/map/insert<string, i32>(values, "right"raw_utf8, 7i32)
  [Reference<map<string, i32>> mut] ref{location(values)}
  /std/collections/map/insert_ref<string, i32>(ref, "third"raw_utf8, 11i32)
  return(plus(/std/collections/map/count<string, i32>(values),
              plus(/std/collections/map/at<string, i32>(values, "left"raw_utf8),
                   plus(/std/collections/map/at_ref<string, i32>(ref, "right"raw_utf8),
                        /std/collections/map/at<string, i32>(values, "third"raw_utf8)))))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("canonical map ownership-sensitive values validate through canonical helpers") {
  const std::string source = R"(
import /std/collections/*

[struct]
Owned() {
  [i32 mut] value{0i32}

  [mut]
  Move([Reference<Self>] other) {
    assign(this.value, other.value)
    assign(other.value, 0i32)
  }

  Destroy() {
  }
}

[effects(heap_alloc), return<int>]
main() {
  [map<string, Owned> mut] values{map<string, Owned>()}
  /std/collections/map/insert<string, Owned>(values, "left"raw_utf8, Owned{4i32})
  /std/collections/map/insert<string, Owned>(values, "right"raw_utf8, Owned{7i32})
  /std/collections/map/insert<string, Owned>(values, "left"raw_utf8, Owned{9i32})
  [Reference<map<string, Owned>> mut] ref{location(values)}
  /std/collections/map/insert_ref<string, Owned>(ref, "third"raw_utf8, Owned{11i32})
  return(plus(/std/collections/map/count<string, Owned>(values),
              plus(/std/collections/map/at<string, Owned>(values, "left"raw_utf8).value,
                   plus(/std/collections/map/at_ref<string, Owned>(ref, "right"raw_utf8).value,
                        /std/collections/map/at_unsafe_ref<string, Owned>(ref, "third"raw_utf8).value))))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("canonical namespaced map insert validates on explicit experimental map bindings") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/internal_map/*

[struct]
Owned() {
  [i32 mut] value{0i32}

  [mut]
  Move([Reference<Self>] other) {
    assign(this.value, other.value)
    assign(other.value, 0i32)
  }

  Destroy() {
  }
}

[effects(heap_alloc), return<int>]
main() {
  [Map<string, Owned> mut] values{mapSingle<string, Owned>("left"raw_utf8, Owned{4i32})}
  /std/collections/map/insert<string, Owned>(values, "right"raw_utf8, Owned{7i32})
  /std/collections/map/insert<string, Owned>(values, "left"raw_utf8, Owned{9i32})
  return(plus(/std/collections/map/count<string, Owned>(values),
              plus(/std/collections/map/at<string, Owned>(values, "left"raw_utf8).value,
                   /std/collections/map/at_unsafe<string, Owned>(values, "right"raw_utf8).value)))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("builtin canonical map insert method sugar validates before lowering ownership-sensitive values") {
  const std::string source = R"(
import /std/collections/*

[struct]
Owned() {
  [i32 mut] value{0i32}

  [mut]
  Move([Reference<Self>] other) {
    assign(this.value, other.value)
    assign(other.value, 0i32)
  }

  Destroy() {
  }
}

[effects(heap_alloc), return<int>]
main() {
  [map<string, Owned> mut] values{map<string, Owned>()}
  values.insert("left"raw_utf8, Owned{4i32})
  values.insert("right"raw_utf8, Owned{7i32})
  values.insert("left"raw_utf8, Owned{9i32})
  return(values.count())
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("constructor-backed builtin map insert method sugar avoids insert builtin rewrite") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32> mut] values{map<i32, i32>(1i32, 4i32)}
  values.insert(2i32, 7i32)
  values.insert(1i32, 9i32)
  return(values.count())
}
)";
  primec::Program program;
  std::string error;
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

  std::vector<std::string> callNames;
  auto collectCallNames = [&](const auto &self, const primec::Expr &expr) -> void {
    if (expr.kind == primec::Expr::Kind::Call) {
      callNames.push_back(expr.name);
    }
    for (const auto &arg : expr.args) {
      self(self, arg);
    }
    for (const auto &bodyArg : expr.bodyArguments) {
      self(self, bodyArg);
    }
  };

  for (const auto &stmt : mainDef->statements) {
    collectCallNames(collectCallNames, stmt);
  }
  if (mainDef->returnExpr.has_value()) {
    collectCallNames(collectCallNames, *mainDef->returnExpr);
  }

  CHECK(std::find(callNames.begin(),
                  callNames.end(),
                  "/std/collections/map/insert_builtin") == callNames.end());
  CHECK(std::none_of(callNames.begin(),
                     callNames.end(),
                     [](const std::string &name) {
                       return name.rfind("/std/collections/map/insert_builtin__", 0) == 0;
                     }));
}

TEST_CASE("canonical map value methods validate ownership-sensitive values through map helpers") {
  const std::string source = R"(
import /std/collections/*

[struct]
Owned() {
  [i32 mut] value{0i32}

  [mut]
  Move([Reference<Self>] other) {
    assign(this.value, other.value)
    assign(other.value, 0i32)
  }

  Destroy() {
  }
}

[effects(io_err)]
unexpectedExperimentalMapValueMethodError([ContainerError] err) {
  [Result<ContainerError>] status{err.code}
  print_line_error(Result.why(status))
}

[return<Result<int, ContainerError>> effects(io_out, heap_alloc) on_error<ContainerError, /unexpectedExperimentalMapValueMethodError>]
main() {
  [map<string, Owned> mut] values{map<string, Owned>()}
  values.insert("left"raw_utf8, Owned{4i32})
  values.insert("right"raw_utf8, Owned{7i32})
  values.insert("left"raw_utf8, Owned{9i32})
  values.insert("third"raw_utf8, Owned{11i32})
  [Owned] found{try(values.tryAt("left"raw_utf8))}
  [Result<Owned, ContainerError>] missing{values.tryAt("missing"raw_utf8)}
  [i32 mut] total{plus(values.count(), found.value)}
  assign(total, plus(total, values.at("right"raw_utf8).value))
  assign(total, plus(total, values.at_unsafe("third"raw_utf8).value))
  if(values.contains("right"raw_utf8),
     then() { assign(total, plus(total, 1i32)) },
     else() { })
  if(not(values.contains("missing"raw_utf8)),
     then() { assign(total, plus(total, 2i32)) },
     else() { })
  return(Result.ok(total))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("canonical map borrowed helper calls validate ownership-sensitive values through ref helpers") {
  const std::string source = R"(
import /std/collections/*

[struct]
Owned() {
  [i32 mut] value{0i32}

  [mut]
  Move([Reference<Self>] other) {
    assign(this.value, other.value)
    assign(other.value, 0i32)
  }

  Destroy() {
  }
}

[return<Reference<map<string, Owned>>>]
borrowMap([Reference<map<string, Owned>>] values) {
  return(values)
}

[effects(io_err)]
unexpectedExperimentalMapReferenceMethodError([ContainerError] err) {
  [Result<ContainerError>] status{err.code}
  print_line_error(Result.why(status))
}

[return<Result<int, ContainerError>> effects(io_out, heap_alloc) on_error<ContainerError, /unexpectedExperimentalMapReferenceMethodError>]
main() {
  [map<string, Owned> mut] values{map<string, Owned>()}
  values.insert("left"raw_utf8, Owned{4i32})
  [Reference<map<string, Owned>> mut] ref{borrowMap(location(values))}
  /std/collections/map/insert_ref<string, Owned>(ref, "right"raw_utf8, Owned{7i32})
  /std/collections/map/insert_ref<string, Owned>(ref, "left"raw_utf8, Owned{9i32})
  /std/collections/map/insert_ref<string, Owned>(ref, "third"raw_utf8, Owned{11i32})
  [Owned] found{try(/std/collections/map/tryAt_ref<string, Owned>(ref, "left"raw_utf8))}
  [Result<Owned, ContainerError>] missing{/std/collections/map/tryAt_ref<string, Owned>(ref, "missing"raw_utf8)}
  [i32 mut] total{plus(/std/collections/map/count_ref<string, Owned>(ref), found.value)}
  assign(total, plus(total, /std/collections/map/at_ref<string, Owned>(ref, "right"raw_utf8).value))
  assign(total, plus(total, /std/collections/map/at_unsafe_ref<string, Owned>(ref, "third"raw_utf8).value))
  if(/std/collections/map/contains_ref<string, Owned>(ref, "right"raw_utf8),
     then() { assign(total, plus(total, 1i32)) },
     else() { })
  if(not(/std/collections/map/contains_ref<string, Owned>(ref, "missing"raw_utf8)),
     then() { assign(total, plus(total, 2i32)) },
     else() { })
  return(Result.ok(total))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("canonical map wrappers accept ownership-sensitive value growth") {
  const std::string source = R"(
import /std/collections/*

[struct]
Owned() {
  [i32 mut] value{0i32}

  [mut]
  Move([Reference<Self>] other) {
    assign(this.value, other.value)
    assign(other.value, 0i32)
  }
}

[effects(heap_alloc), return<int>]
main() {
  [map<string, Owned>] values{mapPair<string, Owned>("left"raw_utf8, Owned{4i32}, "right"raw_utf8, Owned{7i32})}
  return(/std/collections/map/count<string, Owned>(values))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("canonical stdlib map wrappers resolve through explicit namespaced helpers") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [map<string, i32>] values{map<string, i32>("left"raw_utf8, 4i32, "right"raw_utf8, 7i32)}
  [Result<i32, ContainerError>] found{/std/collections/map/tryAt<string, i32>(values, "left"raw_utf8)}
  [Result<i32, ContainerError>] missing{/std/collections/map/tryAt<string, i32>(values, "missing"raw_utf8)}
  [i32 mut] total{/std/collections/map/count<string, i32>(values)}
  assign(total, plus(total, /std/collections/map/at<string, i32>(values, "left"raw_utf8)))
  assign(total, plus(total, /std/collections/map/at_unsafe<string, i32>(values, "right"raw_utf8)))
  if(/std/collections/map/contains<string, i32>(values, "left"raw_utf8),
     then() { assign(total, plus(total, 1i32)) },
     else() { })
  return(total)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("public stdlib map wrapper bridge is retired") {
  auto readText = [](const std::filesystem::path &path) {
    std::ifstream file(path);
    CHECK(file.is_open());
    if (!file.is_open()) {
      return std::string{};
    }
    return std::string((std::istreambuf_iterator<char>(file)),
                       std::istreambuf_iterator<char>());
  };

  const std::filesystem::path repoRoot =
      std::filesystem::exists(std::filesystem::path("stdlib"))
          ? std::filesystem::path(".")
          : std::filesystem::path("..");
  const std::filesystem::path collectionsStdlibPath =
      repoRoot / "stdlib" / "std" / "collections" / "collections.prime";

  REQUIRE(std::filesystem::exists(collectionsStdlibPath));
  const std::string source = readText(collectionsStdlibPath);

  CHECK(source.find("Retired compatibility umbrella.") != std::string::npos);
  CHECK(source.find("/std/collections/map/*") != std::string::npos);
  CHECK(source.find("mapCount") == std::string::npos);
  CHECK(source.find("mapContains") == std::string::npos);
  CHECK(source.find("mapTryAt") == std::string::npos);
  CHECK(source.find("mapAt") == std::string::npos);
  CHECK(source.find("mapInsert") == std::string::npos);
  CHECK(source.find("[public") == std::string::npos);
}

TEST_CASE("retired public stdlib map wrapper calls report their original target") {
  const std::vector<std::pair<std::string, std::string>> cases = {
      {"/std/collections/mapCount(1i32)", "/std/collections/mapCount"},
      {"/std/collections/mapContains(1i32, 2i32)", "/std/collections/mapContains"},
      {"/std/collections/mapTryAt(1i32, 2i32)", "/std/collections/mapTryAt"},
      {"/std/collections/mapAt(1i32, 2i32)", "/std/collections/mapAt"},
      {"/std/collections/mapAtUnsafe(1i32, 2i32)", "/std/collections/mapAtUnsafe"},
      {"/std/collections/mapInsert(1i32, 2i32, 3i32)", "/std/collections/mapInsert"}};

  for (const auto &[call, target] : cases) {
    const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  return()" + call + R"()
}
)";
    std::string error;
    CHECK_FALSE(validateProgram(source, "/main", error));
    INFO(call);
    INFO(error);
    CHECK(error.find("unknown call target: " + target) != std::string::npos);
  }
}

TEST_CASE("canonical stdlib map slash helpers avoid wrapper recursion") {
  auto readText = [](const std::filesystem::path &path) {
    std::ifstream file(path);
    CHECK(file.is_open());
    if (!file.is_open()) {
      return std::string{};
    }
    return std::string((std::istreambuf_iterator<char>(file)),
                       std::istreambuf_iterator<char>());
  };

  const std::filesystem::path repoRoot =
      std::filesystem::exists(std::filesystem::path("stdlib"))
          ? std::filesystem::path(".")
          : std::filesystem::path("..");
  const std::filesystem::path mapStdlibPath =
      repoRoot / "stdlib" / "std" / "collections" / "map.prime";

  REQUIRE(std::filesystem::exists(mapStdlibPath));
  const std::string source = readText(mapStdlibPath);

  CHECK(source.find(
            "/std/collections/map/count<K, V>([map<K, V>] values) {\n"
            "  return(/std/collections/internal_map/countImpl<K, V>(values))") !=
        std::string::npos);
  CHECK(source.find(
            "/std/collections/map/count_ref<K, V>([Reference<map<K, V>>] values) {\n"
            "  return(/std/collections/internal_map/countRefImpl<K, V>(values))") !=
        std::string::npos);
  CHECK(source.find(
            "/std/collections/map/contains<K, V>([map<K, V>] values, [K] key) {\n"
            "  return(/std/collections/internal_map/containsImpl<K, V>(values, key))") !=
        std::string::npos);
  CHECK(source.find("[public return<bool> Comparable<K>]\n/std/collections/map/contains<K, V>") !=
        std::string::npos);
  CHECK(source.find(
            "/std/collections/map/contains_ref<K, V>([Reference<map<K, V>>] values, [K] key) {\n"
            "  return(/std/collections/internal_map/containsRefImpl<K, V>(values, key))") !=
        std::string::npos);
  CHECK(source.find("[public return<bool> Comparable<K>]\n/std/collections/map/contains_ref<K, V>") !=
        std::string::npos);
  CHECK(source.find(
            "/std/collections/map/tryAt<K, V>([map<K, V>] values, [K] key) {\n"
            "  return(/std/collections/internal_map/tryAtImpl<K, V>(values, key))") !=
        std::string::npos);
  CHECK(source.find(
            "[public return<Result<V, /std/collections/ContainerError>> Comparable<K>]\n"
            "/std/collections/map/tryAt<K, V>") != std::string::npos);
  CHECK(source.find(
            "/std/collections/map/tryAt_ref<K, V>([Reference<map<K, V>>] values, [K] key) {\n"
            "  return(/std/collections/internal_map/tryAtRefImpl<K, V>(values, key))") !=
        std::string::npos);
  CHECK(source.find(
            "[public return<Result<V, /std/collections/ContainerError>> Comparable<K>]\n"
            "/std/collections/map/tryAt_ref<K, V>") != std::string::npos);
  CHECK(source.find(
            "/std/collections/map/at<K, V>([map<K, V>] values, [K] key) {\n"
            "  return(/std/collections/internal_map/atImpl<K, V>(values, key))") !=
        std::string::npos);
  CHECK(source.find("[public return<V> Comparable<K>]\n/std/collections/map/at<K, V>") !=
        std::string::npos);
  CHECK(source.find(
            "/std/collections/map/at_ref<K, V>([Reference<map<K, V>>] values, [K] key) {\n"
            "  return(/std/collections/internal_map/atRefImpl<K, V>(values, key))") !=
        std::string::npos);
  CHECK(source.find("[public return<V> Comparable<K>]\n/std/collections/map/at_ref<K, V>") !=
        std::string::npos);
  CHECK(source.find(
            "/std/collections/map/at_unsafe<K, V>([map<K, V>] values, [K] key) {\n"
            "  return(/std/collections/internal_map/atUnsafeImpl<K, V>(values, key))") !=
        std::string::npos);
  CHECK(source.find("[public return<V> Comparable<K>]\n/std/collections/map/at_unsafe<K, V>") !=
        std::string::npos);
  CHECK(source.find(
            "/std/collections/map/at_unsafe_ref<K, V>([Reference<map<K, V>>] values, [K] key) {\n"
            "  return(/std/collections/internal_map/atUnsafeRefImpl<K, V>(values, key))") !=
        std::string::npos);
  CHECK(source.find("[public return<V> Comparable<K>]\n/std/collections/map/at_unsafe_ref<K, V>") !=
        std::string::npos);
  CHECK(source.find(
            "/std/collections/map/insert_ref<K, V>([Reference<map<K, V>> mut] values, [K] key, [V] value) {\n"
            "  /std/collections/internal_map/insertRefImpl<K, V>(values, key, value)") !=
        std::string::npos);

  CHECK(source.find("return(/std/collections/map/count<K, V>(values))") ==
        std::string::npos);
  CHECK(source.find("return(/std/collections/map/count_ref<K, V>(values))") ==
        std::string::npos);
  CHECK(source.find("return(/std/collections/map/contains<K, V>(values, key))") ==
        std::string::npos);
  CHECK(source.find("return(/std/collections/map/contains_ref<K, V>(values, key))") ==
        std::string::npos);
  CHECK(source.find("return(/std/collections/map/tryAt<K, V>(values, key))") ==
        std::string::npos);
  CHECK(source.find("return(/std/collections/map/tryAt_ref<K, V>(values, key))") ==
        std::string::npos);
  CHECK(source.find("return(/std/collections/map/at<K, V>(values, key))") ==
        std::string::npos);
  CHECK(source.find("return(/std/collections/map/at_ref<K, V>(values, key))") ==
        std::string::npos);
  CHECK(source.find("return(/std/collections/map/at_unsafe<K, V>(values, key))") ==
        std::string::npos);
  CHECK(source.find("return(/std/collections/map/at_unsafe_ref<K, V>(values, key))") ==
        std::string::npos);
}

TEST_CASE("canonical map count wrapper ignores removed alias helper") {
  const std::string source = R"(
[effects(heap_alloc), return<bool>]
/map/count<K, V>([map<K, V>] values) {
  return(false)
}

[effects(heap_alloc), return<int>]
/std/collections/experimental_map/mapCount<K, V>([map<K, V>] values) {
  return(2i32)
}

[effects(heap_alloc), return<int>]
/std/collections/map/count<K, V>([map<K, V>] values) {
  return(/std/collections/experimental_map/mapCount<K, V>(values))
}

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 4i32)}
  return(/std/collections/map/count(values))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.empty());
}

TEST_CASE("canonical map access wrapper ignores removed alias helper") {
  const std::string source = R"(
[effects(heap_alloc), return<bool>]
/map/at<K, V>([map<K, V>] values, [K] key) {
  return(false)
}

[effects(heap_alloc), return<int>]
/std/collections/experimental_map/mapAt<K, V>([map<K, V>] values, [K] key) {
  return(4i32)
}

[effects(heap_alloc), return<int>]
/std/collections/map/at<K, V>([map<K, V>] values, [K] key) {
  return(/std/collections/experimental_map/mapAt<K, V>(values, key))
}

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 4i32)}
  return(/std/collections/map/at(values, 1i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.empty());
}

TEST_CASE("experimental map direct-import shim stays retired") {
  auto readText = [](const std::filesystem::path &path) {
    std::ifstream file(path);
    CHECK(file.is_open());
    if (!file.is_open()) {
      return std::string{};
    }
    return std::string((std::istreambuf_iterator<char>(file)),
                       std::istreambuf_iterator<char>());
  };

  const std::filesystem::path repoRoot =
      std::filesystem::exists(std::filesystem::path("stdlib"))
          ? std::filesystem::path(".")
          : std::filesystem::path("..");
  const std::filesystem::path experimentalMapStdlibPath =
      repoRoot / "stdlib" / "std" / "collections" / "experimental_map.prime";

  REQUIRE(std::filesystem::exists(experimentalMapStdlibPath));
  const std::string source = readText(experimentalMapStdlibPath);

  CHECK(source.find("Rejected direct-import shim") != std::string::npos);
  CHECK(source.find("import /std/collections/internal_map/*") != std::string::npos);
  CHECK(source.find("[public struct]") == std::string::npos);
  CHECK(source.find("/std/collections/map/count_ref") == std::string::npos);
  CHECK(source.find("/std/collections/map/insert_ref") == std::string::npos);
  CHECK(source.find("/Reference/count") == std::string::npos);
  CHECK(source.find("/Reference/insert") == std::string::npos);

  CHECK(source.find("/std/collections/map/count_ref<K, V>([Reference<Map<K, V>>] entries) {\n"
                    "    [Map<K, V>] values{dereference(entries)}\n"
                    "    return(/std/collections/map/count<K, V>(values))") ==
        std::string::npos);
  CHECK(source.find("/std/collections/map/insert<K, V>([Map<K, V> mut] entries, [K] key, [V] value) {\n"
                    "    [Reference<Map<K, V>> mut] ref{location(entries)}") ==
        std::string::npos);
  CHECK(source.find("/std/collections/map/count_ref<K, V>([Reference<Map<K, V>>] values)") ==
        std::string::npos);
  CHECK(source.find("return(mapBorrowedCount<K, V>(entries))") ==
        std::string::npos);
  CHECK(source.find("/std/collections/map/contains_ref<K, V>([Reference<Map<K, V>>] entries, [K] key) {\n"
                    "    [Map<K, V>] values{dereference(entries)}\n"
                    "    return(/std/collections/map/contains<K, V>(values, key))") ==
        std::string::npos);
  CHECK(source.find("/std/collections/map/contains_ref<K, V>([Reference<Map<K, V>>] values, [K] key)") ==
        std::string::npos);
  CHECK(source.find("return(mapBorrowedContains<K, V>(entries, key))") ==
        std::string::npos);
  CHECK(source.find("/std/collections/map/tryAt_ref<K, V>([Reference<Map<K, V>>] entries, [K] key) {\n"
                    "    [Map<K, V>] values{dereference(entries)}\n"
                    "    return(/std/collections/map/tryAt<K, V>(values, key))") ==
        std::string::npos);
  CHECK(source.find("/std/collections/map/tryAt_ref<K, V>([Reference<Map<K, V>>] values, [K] key)") ==
        std::string::npos);
  CHECK(source.find("return(mapBorrowedTryAt<K, V>(entries, key))") ==
        std::string::npos);
  CHECK(source.find("/std/collections/map/at_ref<K, V>([Reference<Map<K, V>>] entries, [K] key) {\n"
                    "    [Map<K, V>] values{dereference(entries)}\n"
                    "    return(/std/collections/map/at<K, V>(values, key))") ==
        std::string::npos);
  CHECK(source.find("/std/collections/map/at_ref<K, V>([Reference<Map<K, V>>] values, [K] key)") ==
        std::string::npos);
  CHECK(source.find("return(mapBorrowedAt<K, V>(entries, key))") ==
        std::string::npos);
  CHECK(source.find("/std/collections/map/at_unsafe_ref<K, V>([Reference<Map<K, V>>] entries, [K] key) {\n"
                    "    [Map<K, V>] values{dereference(entries)}\n"
                    "    return(/std/collections/map/at_unsafe<K, V>(values, key))") ==
        std::string::npos);
  CHECK(source.find("/std/collections/map/insert_ref<K, V>([Reference<Map<K, V>> mut] entries, [K] key, [V] value) {\n"
                    "    [Map<K, V> mut] values{dereference(entries)}\n"
                    "    /std/collections/map/insert<K, V>(values, key, value)\n"
                    "    assign(dereference(entries), values)") ==
        std::string::npos);
  CHECK(source.find("/std/collections/map/at_unsafe_ref<K, V>([Reference<Map<K, V>>] values, [K] key)") ==
        std::string::npos);
  CHECK(source.find("/std/collections/map/insert_ref<K, V>([Reference<Map<K, V>> mut] values, [K] key, [V] value)") ==
        std::string::npos);
  CHECK(source.find("return(mapBorrowedAtUnsafe<K, V>(entries, key))") ==
        std::string::npos);
}

TEST_CASE("experimental map bracket access stays unsupported on value and borrowed call receivers") {
  const std::string source = R"(
import /std/collections/internal_map/*

[return<Reference<Map<string, i32>>>]
borrowExperimentalMap([Reference<Map<string, i32>>] values) {
  return(values)
}

[effects(heap_alloc), return<int>]
main() {
  [Map<string, i32>] values{mapPair<string, i32>("left"raw_utf8, 4i32, "right"raw_utf8, 7i32)}
  return(plus(values["left"raw_utf8],
              borrowExperimentalMap(location(values))["right"raw_utf8]))
}
  )";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK_FALSE(error.empty());
}

TEST_CASE("wrapper-returned experimental map bracket access stays unsupported") {
  const std::string source = R"(
import /std/collections/internal_map/*

[effects(heap_alloc), return<Map<i32, string>>]
wrapMap() {
  return(mapSingle<i32, string>(1i32, "hello"utf8))
}

[effects(io_out), return<int>]
main() {
  print_line(wrapMap()[1i32])
  return(0i32)
}
  )";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK_FALSE(error.empty());
}

TEST_CASE("experimental map bracket access on borrowed calls fails before key diagnostics") {
  const std::string source = R"(
import /std/collections/internal_map/*

[return<Reference<Map<string, i32>>>]
borrowExperimentalMap([Reference<Map<string, i32>>] values) {
  return(values)
}

[effects(heap_alloc), return<int>]
main() {
  [Map<string, i32>] values{mapSingle<string, i32>("only"raw_utf8, 4i32)}
  return(borrowExperimentalMap(location(values))[true])
}
  )";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK_FALSE(error.empty());
}

TEST_CASE("canonical map missing comparable trait reports builtin key rejection") {
  const std::string source = R"(
import /std/collections/*

[struct]
Key() {
  [i32] value{0i32}
}

[return<bool>]
/Key/equal([Key] left, [Key] right) {
  return(equal(left.value, right.value))
}

[effects(heap_alloc), return<int>]
main() {
  [map<Key, i32>] values{map<Key, i32>(Key{1i32}, 4i32)}
  return(/std/collections/map/count<Key, i32>(values))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("map requires builtin Comparable key type (i32, i64, u64, f32, f64, bool, or string): Key") !=
        std::string::npos);
}

TEST_CASE("canonical map methods include builtin key rejection") {
  const std::string source = R"(
import /std/collections/*

[struct]
Key() {
  [i32] value{0i32}
}

[return<bool>]
/Key/equal([Key] left, [Key] right) {
  return(equal(left.value, right.value))
}

[effects(heap_alloc), return<int>]
main() {
  [map<Key, i32>] values{map<Key, i32>()}
  if(values.contains(Key{1i32}),
     then() { return(1i32) },
     else() { return(0i32) })
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("map requires builtin Comparable key type (i32, i64, u64, f32, f64, bool, or string): Key") !=
        std::string::npos);
}

TEST_CASE("canonical map Ref helper calls include builtin key rejection") {
  const std::string source = R"(
import /std/collections/*

[struct]
Key() {
  [i32] value{0i32}
}

[return<bool>]
/Key/equal([Key] left, [Key] right) {
  return(equal(left.value, right.value))
}

[effects(heap_alloc), return<int>]
main() {
  [map<Key, i32>] values{map<Key, i32>()}
  [Reference<map<Key, i32>>] ref{location(values)}
  if(/std/collections/map/contains_ref<Key, i32>(ref, Key{1i32}),
     then() { return(1i32) },
     else() { return(0i32) })
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("map requires builtin Comparable key type (i32, i64, u64, f32, f64, bool, or string): Key") !=
        std::string::npos);
}

TEST_CASE("canonical map borrowed methods include builtin key rejection") {
  const std::string source = R"(
import /std/collections/*

[struct]
Key() {
  [i32] value{0i32}
}

[return<bool>]
/Key/equal([Key] left, [Key] right) {
  return(equal(left.value, right.value))
}

[return<Reference<map<Key, i32>>>]
borrowMap([Reference<map<Key, i32>>] values) {
  return(values)
}

[effects(heap_alloc), return<int>]
main() {
  [map<Key, i32>] values{map<Key, i32>()}
  [Reference<map<Key, i32>>] ref{borrowMap(location(values))}
  if(ref.contains(Key{1i32}),
     then() { return(1i32) },
     else() { return(0i32) })
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("map requires builtin Comparable key type (i32, i64, u64, f32, f64, bool, or string): Key") !=
        std::string::npos);
}

TEST_CASE("canonical map insert helper calls include builtin key rejection") {
  const std::string source = R"(
import /std/collections/*

[struct]
Key() {
  [i32] value{0i32}
}

[return<bool>]
/Key/equal([Key] left, [Key] right) {
  return(equal(left.value, right.value))
}

[effects(heap_alloc), return<int>]
main() {
  [map<Key, i32> mut] values{map<Key, i32>()}
  /std/collections/map/insert<Key, i32>(values, Key{1i32}, 4i32)
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("map requires builtin Comparable key type (i32, i64, u64, f32, f64, bool, or string): Key") !=
        std::string::npos);
}

TEST_CASE("canonical map borrowed insert methods include builtin key rejection") {
  const std::string source = R"(
import /std/collections/*

[struct]
Key() {
  [i32] value{0i32}
}

[return<bool>]
/Key/equal([Key] left, [Key] right) {
  return(equal(left.value, right.value))
}

[effects(heap_alloc), return<int>]
main() {
  [map<Key, i32> mut] values{map<Key, i32>()}
  [Reference<map<Key, i32>> mut] ref{location(values)}
  ref.insert(Key{1i32}, 4i32)
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("map requires builtin Comparable key type (i32, i64, u64, f32, f64, bool, or string): Key") !=
        std::string::npos);
}

TEST_SUITE_END();
