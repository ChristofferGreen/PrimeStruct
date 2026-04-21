#include <filesystem>
#include <fstream>
#include <iterator>

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

TEST_CASE("bare map count call requires imported canonical helper or explicit definition") {
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

TEST_CASE("bare map count call rejects compatibility alias when canonical helper is absent") {
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
  CHECK(error.find("unknown call target: /std/collections/map/count") != std::string::npos);
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

TEST_CASE("experimental map custom comparable struct keys validate through canonical map helpers") {
  const std::string source = R"(
import /std/collections/experimental_map/*

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
  [Map<Key, i32>] values{mapPair<Key, i32>(Key(2i32), 7i32, Key(5i32), 11i32)}
  [i32 mut] total{mapCount<Key, i32>(values)}
  assign(total, plus(total, mapAt<Key, i32>(values, Key(2i32))))
  assign(total, plus(total, mapAtUnsafe<Key, i32>(values, Key(5i32))))
  if(mapContains<Key, i32>(values, Key(2i32)),
     then() { assign(total, plus(total, 1i32)) },
     else() { })
  return(total)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("experimental map method-call sugar validates on the real Map struct") {
  const std::string source = R"(
import /std/collections/experimental_map/*

[return<int> effects(heap_alloc)]
main() {
  [Map<string, i32>] values{mapPair<string, i32>("left"raw_utf8, 4i32, "right"raw_utf8, 7i32)}
  return(values.count())
}
  )";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("experimental map Ref helper calls accept borrowed Map references") {
  const std::string source = R"(
import /std/collections/experimental_map/*

[return<int> effects(heap_alloc)]
main() {
  [Map<string, i32>] values{mapPair<string, i32>("left"raw_utf8, 4i32, "right"raw_utf8, 7i32)}
  [Reference<Map<string, i32>>] ref{location(values)}
  [i32 mut] total{mapCountRef<string, i32>(ref)}
  assign(total, plus(total, mapAtRef<string, i32>(ref, "left"raw_utf8)))
  assign(total, plus(total, mapAtUnsafeRef<string, i32>(ref, "right"raw_utf8)))
  if(mapContainsRef<string, i32>(ref, "left"raw_utf8),
     then() { assign(total, plus(total, 1i32)) },
     else() { })
  return(total)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("experimental map borrowed method-call sugar validates") {
  const std::string source = R"(
import /std/collections/experimental_map/*

[return<int> effects(heap_alloc)]
main() {
  [Map<string, i32>] values{mapPair<string, i32>("left"raw_utf8, 4i32, "right"raw_utf8, 7i32)}
  [Reference<Map<string, i32>>] ref{location(values)}
  return(ref.count())
}
  )";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("experimental map insert helpers validate on value and borrowed mutation surfaces") {
  const std::string source = R"(
import /std/collections/experimental_map/*

[effects(heap_alloc), return<int>]
main() {
  [Map<string, i32> mut] values{mapSingle<string, i32>("left"raw_utf8, 4i32)}
  mapInsert<string, i32>(values, "right"raw_utf8, 7i32)
  [Reference<Map<string, i32>> mut] ref{location(values)}
  mapInsertRef<string, i32>(ref, "third"raw_utf8, 11i32)
  return(plus(mapCount<string, i32>(values),
              plus(mapAt<string, i32>(values, "left"raw_utf8),
                   plus(mapAtRef<string, i32>(ref, "right"raw_utf8),
                        mapAt<string, i32>(values, "third"raw_utf8)))))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("experimental map ownership-sensitive values validate through experimental storage") {
  const std::string source = R"(
import /std/collections/experimental_map/*

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
  [Map<string, Owned> mut] values{mapSingle<string, Owned>("left"raw_utf8, Owned(4i32))}
  mapInsert<string, Owned>(values, "right"raw_utf8, Owned(7i32))
  mapInsert<string, Owned>(values, "left"raw_utf8, Owned(9i32))
  [Reference<Map<string, Owned>> mut] ref{location(values)}
  mapInsertRef<string, Owned>(ref, "third"raw_utf8, Owned(11i32))
  return(plus(mapCount<string, Owned>(values),
              plus(mapAt<string, Owned>(values, "left"raw_utf8).value,
                   plus(mapAtRef<string, Owned>(ref, "right"raw_utf8).value,
                        mapAtUnsafeRef<string, Owned>(ref, "third"raw_utf8).value))))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("canonical namespaced map insert validates on explicit experimental map bindings") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_map/*

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
  [Map<string, Owned> mut] values{mapSingle<string, Owned>("left"raw_utf8, Owned(4i32))}
  /std/collections/map/insert<string, Owned>(values, "right"raw_utf8, Owned(7i32))
  /std/collections/map/insert<string, Owned>(values, "left"raw_utf8, Owned(9i32))
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
  values.insert("left"raw_utf8, Owned(4i32))
  values.insert("right"raw_utf8, Owned(7i32))
  values.insert("left"raw_utf8, Owned(9i32))
  return(values.count())
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("experimental map value methods validate ownership-sensitive values through Map helpers") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_map/*

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
  [Map<string, Owned> mut] values{mapSingle<string, Owned>("left"raw_utf8, Owned(4i32))}
  values.insert("right"raw_utf8, Owned(7i32))
  values.insert("left"raw_utf8, Owned(9i32))
  values.insert("third"raw_utf8, Owned(11i32))
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

TEST_CASE("experimental map borrowed methods validate ownership-sensitive values through reference helpers") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_map/*

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

[return<Reference<Map<string, Owned>>>]
borrowExperimentalMap([Reference<Map<string, Owned>>] values) {
  return(values)
}

[effects(io_err)]
unexpectedExperimentalMapReferenceMethodError([ContainerError] err) {
  [Result<ContainerError>] status{err.code}
  print_line_error(Result.why(status))
}

[return<Result<int, ContainerError>> effects(io_out, heap_alloc) on_error<ContainerError, /unexpectedExperimentalMapReferenceMethodError>]
main() {
  [Map<string, Owned> mut] values{mapSingle<string, Owned>("left"raw_utf8, Owned(4i32))}
  [Reference<Map<string, Owned>> mut] ref{borrowExperimentalMap(location(values))}
  ref.insert("right"raw_utf8, Owned(7i32))
  ref.insert("left"raw_utf8, Owned(9i32))
  ref.insert("third"raw_utf8, Owned(11i32))
  [Owned] found{try(ref.tryAt("left"raw_utf8))}
  [Result<Owned, ContainerError>] missing{ref.tryAt("missing"raw_utf8)}
  [i32 mut] total{plus(ref.count(), found.value)}
  assign(total, plus(total, ref.at("right"raw_utf8).value))
  assign(total, plus(total, ref.at_unsafe("third"raw_utf8).value))
  if(ref.contains("right"raw_utf8),
     then() { assign(total, plus(total, 1i32)) },
     else() { })
  if(not(ref.contains("missing"raw_utf8)),
     then() { assign(total, plus(total, 2i32)) },
     else() { })
  return(Result.ok(total))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("canonical map wrappers reject non-relocation-trivial value growth") {
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
  [map<string, Owned>] values{mapPair<string, Owned>("left"raw_utf8, Owned(4i32), "right"raw_utf8, Owned(7i32))}
  return(mapCount<string, Owned>(values))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find(
            "map literal requires relocation-trivial map value type until container move/reallocation semantics "
            "are implemented: Owned") != std::string::npos);
}

TEST_CASE("canonical stdlib map wrappers resolve through explicit namespaced helpers") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [map<string, i32>] values{map<string, i32>("left"raw_utf8, 4i32, "right"raw_utf8, 7i32)}
  [i32 mut] total{mapCount<string, i32>(values)}
  assign(total, plus(total, mapAt<string, i32>(values, "left"raw_utf8)))
  assign(total, plus(total, mapAtUnsafe<string, i32>(values, "right"raw_utf8)))
  if(mapContains<string, i32>(values, "left"raw_utf8),
     then() { assign(total, plus(total, 1i32)) },
     else() { })
  return(total)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
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
            "  return(/std/collections/experimental_map/mapCount<K, V>(values))") !=
        std::string::npos);
  CHECK(source.find(
            "/std/collections/map/contains<K, V>([map<K, V>] values, [K] key) {\n"
            "  return(/std/collections/experimental_map/mapContains<K, V>(values, key))") !=
        std::string::npos);
  CHECK(source.find(
            "/std/collections/map/tryAt<K, V>([map<K, V>] values, [K] key) {\n"
            "  return(/std/collections/experimental_map/mapTryAt<K, V>(values, key))") !=
        std::string::npos);
  CHECK(source.find(
            "/std/collections/map/at<K, V>([map<K, V>] values, [K] key) {\n"
            "  return(/std/collections/experimental_map/mapAt<K, V>(values, key))") !=
        std::string::npos);
  CHECK(source.find(
            "/std/collections/map/at_unsafe<K, V>([map<K, V>] values, [K] key) {\n"
            "  return(/std/collections/experimental_map/mapAtUnsafe<K, V>(values, key))") !=
        std::string::npos);

  CHECK(source.find("return(/std/collections/mapCount<K, V>(values))") ==
        std::string::npos);
  CHECK(source.find("return(/std/collections/mapContains<K, V>(values, key))") ==
        std::string::npos);
  CHECK(source.find("return(/std/collections/mapTryAt<K, V>(values, key))") ==
        std::string::npos);
  CHECK(source.find("return(/std/collections/mapAt<K, V>(values, key))") ==
        std::string::npos);
  CHECK(source.find("return(/std/collections/mapAtUnsafe<K, V>(values, key))") ==
        std::string::npos);
}

TEST_CASE("experimental map bracket access stays unsupported on value and borrowed call receivers") {
  const std::string source = R"(
import /std/collections/experimental_map/*

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
import /std/collections/experimental_map/*

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
import /std/collections/experimental_map/*

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

TEST_CASE("experimental map missing comparable trait includes builtin map key rejects") {
  const std::string source = R"(
import /std/collections/experimental_map/*

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
  [Map<Key, i32>] values{mapSingle<Key, i32>(Key(1i32), 4i32)}
  return(mapCount<Key, i32>(values))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("trait constraint not satisfied") != std::string::npos);
  CHECK(error.find("Comparable</Key> requires less_than(/Key, /Key) -> bool") != std::string::npos);
}

TEST_CASE("experimental map methods include builtin map key rejects on the real Map struct") {
  const std::string source = R"(
import /std/collections/experimental_map/*

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
  [Map<Key, i32>] values{mapNew<Key, i32>()}
  if(values.contains(Key(1i32)),
     then() { return(1i32) },
     else() { return(0i32) })
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("trait constraint not satisfied") != std::string::npos);
  CHECK(error.find("Comparable</Key> requires less_than(/Key, /Key) -> bool") != std::string::npos);
}

TEST_CASE("experimental map Ref helper calls include builtin map key rejects") {
  const std::string source = R"(
import /std/collections/experimental_map/*

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
  [Map<Key, i32>] values{mapNew<Key, i32>()}
  [Reference<Map<Key, i32>>] ref{location(values)}
  if(mapContainsRef<Key, i32>(ref, Key(1i32)),
     then() { return(1i32) },
     else() { return(0i32) })
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("trait constraint not satisfied") != std::string::npos);
  CHECK(error.find("Comparable</Key> requires less_than(/Key, /Key) -> bool") != std::string::npos);
}

TEST_CASE("experimental map borrowed methods include builtin map key rejects") {
  const std::string source = R"(
import /std/collections/experimental_map/*

[struct]
Key() {
  [i32] value{0i32}
}

[return<bool>]
/Key/equal([Key] left, [Key] right) {
  return(equal(left.value, right.value))
}

[return<Reference<Map<Key, i32>>>]
borrowExperimentalMap([Reference<Map<Key, i32>>] values) {
  return(values)
}

[effects(heap_alloc), return<int>]
main() {
  [Map<Key, i32>] values{mapNew<Key, i32>()}
  [Reference<Map<Key, i32>>] ref{borrowExperimentalMap(location(values))}
  if(ref.contains(Key(1i32)),
     then() { return(1i32) },
     else() { return(0i32) })
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("trait constraint not satisfied") != std::string::npos);
  CHECK(error.find("Comparable</Key> requires less_than(/Key, /Key) -> bool") != std::string::npos);
}

TEST_CASE("experimental map insert helper calls include builtin map key rejects") {
  const std::string source = R"(
import /std/collections/experimental_map/*

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
  [Map<Key, i32> mut] values{mapNew<Key, i32>()}
  mapInsert<Key, i32>(values, Key(1i32), 4i32)
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("trait constraint not satisfied") != std::string::npos);
  CHECK(error.find("Comparable</Key> requires less_than(/Key, /Key) -> bool") != std::string::npos);
}

TEST_CASE("experimental map borrowed insert methods include builtin map key rejects") {
  const std::string source = R"(
import /std/collections/experimental_map/*

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
  [Map<Key, i32> mut] values{mapNew<Key, i32>()}
  [Reference<Map<Key, i32>> mut] ref{location(values)}
  ref.insert(Key(1i32), 4i32)
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("trait constraint not satisfied") != std::string::npos);
  CHECK(error.find("Comparable</Key> requires less_than(/Key, /Key) -> bool") != std::string::npos);
}

TEST_SUITE_END();
