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

TEST_CASE("bare map count call validates without imported canonical helper") {
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

TEST_CASE("bare map count call resolves through compatibility alias when canonical helper is absent") {
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
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
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

TEST_CASE("bare map contains call validates without imported canonical helper") {
  const std::string source = R"(
[return<bool>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 2i32)}
  return(contains(values, 1i32))
}
  )";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.empty());
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

TEST_CASE("bare map contains call resolves through compatibility alias when canonical helper is absent") {
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
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
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

TEST_CASE("experimental map custom comparable struct keys currently reject through canonical map helpers") {
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
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("map requires builtin Comparable key type (i32, i64, u64, f32, f64, bool, or string): Key") !=
        std::string::npos);
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
  CHECK(error.find("Comparable") != std::string::npos);
  CHECK(error.find("builtin Comparable key type") != std::string::npos);
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
  CHECK(error.find("Comparable") != std::string::npos);
  CHECK(error.find("builtin Comparable key type") != std::string::npos);
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
  CHECK(error.find("Comparable") != std::string::npos);
  CHECK(error.find("builtin Comparable key type") != std::string::npos);
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
  CHECK(error.find("Comparable") != std::string::npos);
  CHECK(error.find("builtin Comparable key type") != std::string::npos);
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
  CHECK(error.find("Comparable") != std::string::npos);
  CHECK(error.find("builtin Comparable key type") != std::string::npos);
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
  CHECK(error.find("Comparable") != std::string::npos);
  CHECK(error.find("builtin Comparable key type") != std::string::npos);
}

TEST_CASE("container error contract shape validates through Result.why") {
  const std::string source = R"(
[struct]
ContainerError() {
  [i32] code{0i32}
}

namespace ContainerError {
  [return<string>]
  why([ContainerError] err) {
    return("container error"utf8)
  }
}

[return<ContainerError>]
containerMissingKey() {
  return(ContainerError(1i32))
}

[return<void>]
main() {
  [Result<ContainerError>] status{ containerMissingKey().code }
  [Result<ContainerError>] unknown{ 99i32 }
  [string] message{ Result.why(status) }
  [auto] fallback{ Result.why(unknown) }
  return()
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("containerErrorResult helper validates for value-carrying container results") {
  const std::string source = R"(
[struct]
ContainerError() {
  [i32] code{0i32}
}

[return<ContainerError>]
containerMissingKey() {
  return(ContainerError(1i32))
}

[return<Result<T, ContainerError>>]
containerErrorResult<T>([ContainerError] err) {
  return(multiply(convert<i64>(err.code), 4294967296i64))
}

[return<Result<i32, ContainerError>>]
main() {
  return(containerErrorResult<i32>(containerMissingKey()))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("mapTryAt helper validates for supported i32 container result payloads") {
  const std::string source = R"(
import /std/collections/*

[return<Result<V, ContainerError>>]
mapTryAt<K, V>([map<K, V>] values, [K] key) {
  if(/std/collections/map/contains(values, key),
     then() { return(Result.ok(/std/collections/map/at_unsafe(values, key))) },
     else() { return(containerErrorResult<V>(containerMissingKey())) })
}

[return<Result<i32, ContainerError>>]
main() {
  [map<string, i32>] values{map<string, i32>("left"raw_utf8, 7i32)}
  return(mapTryAt<string, i32>(values, "left"raw_utf8))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("mapTryAt helper validates for supported bool container result payloads") {
  const std::string source = R"(
import /std/collections/*

[return<Result<V, ContainerError>>]
mapTryAt<K, V>([map<K, V>] values, [K] key) {
  if(/std/collections/map/contains(values, key),
     then() { return(Result.ok(/std/collections/map/at_unsafe(values, key))) },
     else() { return(containerErrorResult<V>(containerMissingKey())) })
}

[return<Result<bool, ContainerError>>]
main() {
  [map<string, bool>] values{map<string, bool>("left"raw_utf8, true)}
  return(mapTryAt<string, bool>(values, "left"raw_utf8))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("mapTryAt helper validates for supported string container result payloads") {
  const std::string source = R"(
import /std/collections/*

[return<Result<V, ContainerError>>]
mapTryAt<K, V>([map<K, V>] values, [K] key) {
  if(/std/collections/map/contains(values, key),
     then() { return(Result.ok(/std/collections/map/at_unsafe(values, key))) },
     else() { return(containerErrorResult<V>(containerMissingKey())) })
}

[return<Result<string, ContainerError>>]
main() {
  [map<string, string>] values{map<string, string>("left"raw_utf8, "alpha"utf8)}
  return(mapTryAt<string, string>(values, "left"raw_utf8))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("bare vector count call resolves through imported stdlib helper") {
  const std::string source = R"(
import /std/collections/*

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

TEST_CASE("bare vector count call requires imported stdlib helper or explicit definition") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32)}
  return(count(values))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /std/collections/vector/count") != std::string::npos);
}

TEST_CASE("bare vector count wrapper call resolves through imported stdlib helper") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<vector<i32>>]
wrapVector() {
  return(vector<i32>(1i32, 2i32))
}

[effects(heap_alloc), return<int>]
main() {
  return(count(wrapVector()))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("bare vector count wrapper call requires imported stdlib helper or explicit definition") {
  const std::string source = R"(
[effects(heap_alloc), return<vector<i32>>]
wrapVector() {
  return(vector<i32>(1i32, 2i32))
}

[effects(heap_alloc), return<int>]
main() {
  return(count(wrapVector()))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /std/collections/vector/count") != std::string::npos);
}

TEST_CASE("bare vector capacity wrapper call resolves through imported stdlib helper") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<vector<i32>>]
wrapVector() {
  return(vector<i32>(1i32, 2i32))
}

[effects(heap_alloc), return<int>]
main() {
  return(capacity(wrapVector()))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("bare vector capacity wrapper call requires imported stdlib helper or explicit definition") {
  const std::string source = R"(
[effects(heap_alloc), return<vector<i32>>]
wrapVector() {
  return(vector<i32>(1i32, 2i32))
}

[effects(heap_alloc), return<int>]
main() {
  return(capacity(wrapVector()))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /std/collections/vector/capacity") != std::string::npos);
}

TEST_CASE("count helper validates on soa_vector binding") {
  const std::string source = R"(
Particle() {
  [i32] x{1i32}
}

[return<int>]
main() {
  [soa_vector<Particle>] values{soa_vector<Particle>()}
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

TEST_CASE("count method on vector binding accepts same-path helper") {
  const std::string source = R"(
[return<int>]
/vector/count([vector<i32>] values) {
  return(7i32)
}

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

TEST_CASE("count method on vector binding requires helper") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32)}
  return(values.count())
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown method: /vector/count") != std::string::npos);
}

TEST_CASE("count method validates on soa_vector binding") {
  const std::string source = R"(
Particle() {
  [i32] x{1i32}
}

[return<int>]
main() {
  [soa_vector<Particle>] values{soa_vector<Particle>()}
  return(values.count())
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("get helper validates on soa_vector binding") {
  const std::string source = R"(
Particle() {
  [i32] x{1i32}
}

[return<int>]
main() {
  [soa_vector<Particle>] values{soa_vector<Particle>()}
  get(values, 0i32)
  return(0i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("get method validates on soa_vector binding") {
  const std::string source = R"(
Particle() {
  [i32] x{1i32}
}

[return<int>]
main() {
  [soa_vector<Particle>] values{soa_vector<Particle>()}
  values.get(0i32)
  return(0i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("get helper rejects non-soa target") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32)}
  get(values, 0i32)
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("get requires soa_vector target") != std::string::npos);
}

TEST_CASE("ref helper validates on soa_vector binding") {
  const std::string source = R"(
Particle() {
  [i32] x{1i32}
}

[return<int>]
main() {
  [soa_vector<Particle>] values{soa_vector<Particle>()}
  ref(values, 0i32)
  return(0i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("ref method validates on soa_vector binding") {
  const std::string source = R"(
Particle() {
  [i32] x{1i32}
}

[return<int>]
main() {
  [soa_vector<Particle>] values{soa_vector<Particle>()}
  values.ref(0i32)
  return(0i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("ref helper rejects non-soa target") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32)}
  ref(values, 0i32)
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("ref requires soa_vector target") != std::string::npos);
}

TEST_CASE("to_soa helper validates on vector binding") {
  const std::string source = R"(
Particle() {
  [i32] x{1i32}
}

[return<int>]
main() {
  [vector<Particle>] values{vector<Particle>()}
  to_soa(values)
  return(0i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("to_aos helper validates on soa_vector binding") {
  const std::string source = R"(
Particle() {
  [i32] x{1i32}
}

[return<int>]
main() {
  [soa_vector<Particle>] values{soa_vector<Particle>()}
  to_aos(values)
  return(0i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("to_aos validates on borrowed indexed soa_vector receiver") {
  const std::string source = R"(
import /std/collections/*

Particle() {
  [i32] x{1i32}
}

[return<int>]
/score([args<Reference<soa_vector<Particle>>>] values) {
  return(count(to_aos(dereference(values[0i32]))))
}

[return<int>]
main() {
  return(0i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("to_soa and to_aos helpers compose for explicit conversion") {
  const std::string source = R"(
import /std/collections/*

Particle() {
  [i32] x{1i32}
}

[return<int>]
main() {
  [vector<Particle>] values{vector<Particle>()}
  return(count(to_aos(to_soa(values))))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("to_soa helper rejects non-vector target") {
  const std::string source = R"(
Particle() {
  [i32] x{1i32}
}

[return<int>]
main() {
  [soa_vector<Particle>] values{soa_vector<Particle>()}
  to_soa(values)
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("to_soa requires vector target") != std::string::npos);
}

TEST_CASE("to_aos helper rejects non-soa target") {
  const std::string source = R"(
Particle() {
  [i32] x{1i32}
}

[return<int>]
main() {
  [vector<Particle>] values{vector<Particle>()}
  to_aos(values)
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("to_aos requires soa_vector target") != std::string::npos);
}

TEST_CASE("aos and soa containers do not implicitly convert") {
  const std::string source = R"(
Particle() {
  [i32] x{1i32}
}

[return<int>]
/consumeSoa([soa_vector<Particle>] values) {
  return(0i32)
}

[return<int>]
main() {
  [vector<Particle>] values{vector<Particle>()}
  return(consumeSoa(values))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("argument type mismatch") != std::string::npos);
  CHECK(error.find("/consumeSoa") != std::string::npos);
}

TEST_CASE("ecs style soa_vector update loop validates with deferred structural phase") {
  const std::string source = R"(
Particle() {
  [i32] x{0i32}
  [i32] y{0i32}
}

[effects(heap_alloc), return<void>]
/simulateStep([soa_vector<Particle> mut] particles, [vector<Particle> mut] spawnQueue) {
  [i32 mut] i{0i32}
  while(less_than(i, count(particles))) {
    get(particles, i)
    assign(i, plus(i, 1i32))
  }

  [soa_vector<Particle>] stagedSpawns{to_soa(spawnQueue)}
  reserve(particles, plus(count(particles), count(stagedSpawns)))
}

[effects(heap_alloc), return<int>]
main() {
  [soa_vector<Particle> mut] particles{soa_vector<Particle>()}
  [vector<Particle> mut] spawnQueue{vector<Particle>()}
  simulateStep(particles, spawnQueue)
  return(0i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("ecs style update loop still requires explicit aos to soa conversion") {
  const std::string source = R"(
Particle() {
  [i32] x{0i32}
  [i32] y{0i32}
}

[effects(heap_alloc), return<void>]
/simulateStep([soa_vector<Particle> mut] particles) {
  [i32 mut] i{0i32}
  while(less_than(i, count(particles))) {
    get(particles, i)
    assign(i, plus(i, 1i32))
  }
}

[effects(heap_alloc), return<int>]
main() {
  [vector<Particle> mut] particles{vector<Particle>()}
  simulateStep(particles)
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("argument type mismatch") != std::string::npos);
  CHECK(error.find("/simulateStep") != std::string::npos);
}

TEST_CASE("to_soa and to_aos reject named arguments for builtin calls") {
  const auto checkNamedArgs = [](const std::string &callExpr) {
    const std::string source =
        "Particle() {\n"
        "  [i32] x{1i32}\n"
        "}\n\n"
        "[return<int>]\n"
        "main() {\n"
        "  [vector<Particle>] values{vector<Particle>()}\n"
        "  [soa_vector<Particle>] packed{soa_vector<Particle>()}\n"
        "  " + callExpr + "\n"
        "  return(0i32)\n"
        "}\n";
    std::string error;
    CHECK_FALSE(validateProgram(source, "/main", error));
    CHECK(error.find("named arguments not supported for builtin calls") != std::string::npos);
  };

  checkNamedArgs("to_soa([values] values)");
  checkNamedArgs("to_aos([values] packed)");
}

TEST_CASE("soa_vector get and ref reject named arguments for builtin calls") {
  const auto checkNamedArgs = [](const std::string &callExpr) {
    const std::string source =
        "Particle() {\n"
        "  [i32] x{1i32}\n"
        "}\n\n"
        "[return<int>]\n"
        "main() {\n"
        "  [soa_vector<Particle>] values{soa_vector<Particle>()}\n"
        "  " + callExpr + "\n"
        "  return(0i32)\n"
        "}\n";
    std::string error;
    CHECK_FALSE(validateProgram(source, "/main", error));
    CHECK(error.find("named arguments not supported for builtin calls") != std::string::npos);
  };

  checkNamedArgs("get([index] 0i32, [values] values)");
  checkNamedArgs("ref([index] 0i32, [values] values)");
}

TEST_CASE("soa_vector get helper call-form accepts labeled named receiver") {
  const std::string source = R"(
Particle() {
  [i32] x{1i32}
}

[effects(heap_alloc), return<int>]
/soa_vector/get([soa_vector<Particle>] values, [vector<i32>] index) {
  return(7i32)
}

[effects(heap_alloc), return<int>]
main() {
  [soa_vector<Particle>] values{soa_vector<Particle>()}
  [vector<i32>] index{vector<i32>(0i32)}
  return(get([index] index, [values] values))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("soa_vector get helper call-form does not fall back to non-receiver label") {
  const std::string source = R"(
Particle() {
  [i32] x{1i32}
}

[effects(heap_alloc), return<int>]
/soa_vector/get([vector<i32>] values, [soa_vector<Particle>] index) {
  return(8i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(0i32)}
  [soa_vector<Particle>] index{soa_vector<Particle>()}
  return(get([index] index, [values] values))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("named arguments not supported for builtin calls") != std::string::npos);
}

TEST_CASE("soa_vector field-view method reports unsupported diagnostic") {
  const std::string source = R"(
Particle() {
  [i32] x{1i32}
}

[return<int>]
main() {
  [soa_vector<Particle>] values{soa_vector<Particle>()}
  values.x()
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("soa_vector field views are not implemented yet: x") != std::string::npos);
}

TEST_CASE("soa_vector field-view call-form reports unsupported diagnostic") {
  const std::string source = R"(
Particle() {
  [i32] x{1i32}
}

[return<int>]
main() {
  [soa_vector<Particle>] values{soa_vector<Particle>()}
  x(values)
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("soa_vector field views are not implemented yet: x") != std::string::npos);
}

TEST_CASE("soa_vector field-view index syntax reports unsupported diagnostic") {
  const std::string source = R"(
Particle() {
  [i32] x{1i32}
}

[return<int>]
main() {
  [soa_vector<Particle>] values{soa_vector<Particle>()}
  return(values.x()[0i32])
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("soa_vector field views are not implemented yet: x") != std::string::npos);
}

TEST_CASE("soa_vector field-view builtin rejects named arguments") {
  const std::string source = R"(
Particle() {
  [i32] x{1i32}
}

[return<int>]
main() {
  [soa_vector<Particle>] values{soa_vector<Particle>()}
  values.x([index] 0i32)
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("named arguments not supported for builtin calls") != std::string::npos);
}

TEST_CASE("soa_vector field-view call-form falls back to user helper") {
  const std::string source = R"(
Particle() {
  [i32] x{1i32}
}

[return<int>]
/soa_vector/x([soa_vector<Particle>] values) {
  return(7i32)
}

[return<int>]
main() {
  [soa_vector<Particle>] values{soa_vector<Particle>()}
  return(x(values))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("soa_vector field-view method falls back to user helper") {
  const std::string source = R"(
Particle() {
  [i32] x{1i32}
}

[return<int>]
/soa_vector/x([soa_vector<Particle>] values, [i32] index) {
  return(index)
}

[return<int>]
main() {
  [soa_vector<Particle>] values{soa_vector<Particle>()}
  return(values.x(0i32))
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

TEST_CASE("count method validates on map binding with explicit alias helper") {
  const std::string source = R"(
[return<int>]
/map/count([map<i32, i32>] values) {
  return(7i32)
}

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

TEST_CASE("count wrapper temporaries infer i32 for chained methods") {
  const std::string source = R"(
wrapText() {
  [string] text{"abc"utf8}
  return(text)
}

[return<int>]
/i32/tag([i32] value) {
  return(plus(value, 40i32))
}

[return<int>]
main() {
  return(plus(count(wrapText()).tag(), wrapText().count().tag()))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("wrapper vector count method resolves through imported stdlib helper") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc)]
wrapVector() {
  [vector<i32>] values{vector<i32>(1i32, 2i32, 3i32)}
  return(values)
}

[effects(heap_alloc), return<int>]
main() {
  return(wrapVector().count())
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("wrapper vector count method requires helper") {
  const std::string source = R"(
[effects(heap_alloc)]
wrapVector() {
  [vector<i32>] values{vector<i32>(1i32, 2i32, 3i32)}
  return(values)
}

[effects(heap_alloc), return<int>]
main() {
  return(wrapVector().count())
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("count wrapper temporary chained method reports i32 path diagnostics") {
  const std::string source = R"(
wrapText() {
  [string] text{"abc"utf8}
  return(text)
}

[return<int>]
main() {
  return(count(wrapText()).missing_tag())
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown method: /i32/missing_tag") != std::string::npos);
}

TEST_CASE("access wrapper temporaries infer i32 for chained methods") {
  const std::string source = R"(
wrapText() {
  [string] text{"abc"utf8}
  return(text)
}

[return<int>]
/i32/tag([i32] value) {
  return(plus(value, 10i32))
}

[return<int>]
main() {
  return(plus(at(wrapText(), 1i32).tag(), wrapText().at_unsafe(2i32).tag()))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("namespaced access wrapper temporaries infer i32 for chained methods") {
  const std::string source = R"(
import /std/collections/*

wrapText() {
  [string] text{"abc"utf8}
  return(text)
}

[return<int>]
/i32/tag([i32] value) {
  return(plus(value, 10i32))
}

[return<int>]
main() {
  return(plus(/std/collections/vector/at(wrapText(), 1i32).tag(),
              /std/collections/vector/at_unsafe(wrapText(), 2i32).tag()))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("argument type mismatch for /std/collections/vector/at") != std::string::npos);
}

TEST_CASE("reordered access wrapper temporaries infer i32 for chained methods") {
  const std::string source = R"(
[return<map<string, i32>>]
wrapMapText() {
  [map<string, i32>] values{map<string, i32>("only"raw_utf8, 21i32)}
  return(values)
}

[return<int>]
/map/at_unsafe([map<string, i32>] values, [string] key) {
  return(values.at_unsafe(key))
}

[return<int>]
/i32/tag([i32] value) {
  return(plus(value, 1i32))
}

[return<int>]
main() {
  return(at_unsafe("only"raw_utf8, wrapMapText()).tag())
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("access wrapper temporary chained method reports i32 path diagnostics") {
  const std::string source = R"(
wrapText() {
  [string] text{"abc"utf8}
  return(text)
}

[return<int>]
main() {
  return(at(wrapText(), 1i32).missing_tag())
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown method: /i32/missing_tag") != std::string::npos);
}

TEST_CASE("namespaced access wrapper temporary chained method reports i32 path diagnostics") {
  const std::string source = R"(
import /std/collections/*

wrapText() {
  [string] text{"abc"utf8}
  return(text)
}

[return<int>]
main() {
  return(/std/collections/vector/at(wrapText(), 1i32).missing_tag())
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown method: /i32/missing_tag") != std::string::npos);
}

TEST_CASE("slash-method access wrapper temporaries infer i32 for chained methods") {
  const std::string source = R"(
import /std/collections/*

wrapText() {
  [string] text{"abc"utf8}
  return(text)
}

[return<int>]
/i32/tag([i32] value) {
  return(plus(value, 10i32))
}

[return<int>]
main() {
  return(plus(wrapText()./std/collections/vector/at(1i32).tag(),
              wrapText()./vector/at_unsafe(2i32).tag()))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("slash-method access wrapper temporary chained method reports i32 path diagnostics") {
  const std::string source = R"(
import /std/collections/*

wrapText() {
  [string] text{"abc"utf8}
  return(text)
}

[return<int>]
main() {
  return(wrapText()./std/collections/vector/at(1i32).missing_tag())
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown method: /i32/missing_tag") != std::string::npos);
}

TEST_CASE("map wrapper temporary count call validates target classification") {
  const std::string source = R"(
import /std/collections/*

wrapMapAuto() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 2i32)}
  return(values)
}

[return<int>]
main() {
  return(/std/collections/map/count(wrapMapAuto()))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("stdlib namespaced vector count validates on wrapper temporary vector target") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc)]
wrapVectorAuto() {
  [vector<i32>] values{vector<i32>(1i32, 2i32)}
  return(values)
}

[return<int>]
main() {
  return(/std/collections/vector/count(wrapVectorAuto()))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("stdlib namespaced vector count wrapper temporary vector target requires helper") {
  const std::string source = R"(
[effects(heap_alloc)]
wrapVectorAuto() {
  [vector<i32>] values{vector<i32>(1i32, 2i32)}
  return(values)
}

[return<int>]
main() {
  return(/std/collections/vector/count(wrapVectorAuto()))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /std/collections/vector/count") != std::string::npos);
}

TEST_CASE("stdlib namespaced vector capacity wrapper temporary vector target requires helper") {
  const std::string source = R"(
[effects(heap_alloc)]
wrapVectorAuto() {
  [vector<i32>] values{vector<i32>(1i32, 2i32)}
  return(values)
}

[return<int>]
main() {
  return(/std/collections/vector/capacity(wrapVectorAuto()))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /std/collections/vector/capacity") != std::string::npos);
}

TEST_CASE("stdlib namespaced vector count rejects wrapper temporary map target") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc)]
wrapMapAuto() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 2i32)}
  return(values)
}

[return<int>]
main() {
  return(/std/collections/vector/count(wrapMapAuto()))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /std/collections/vector/count") != std::string::npos);
}

TEST_CASE("map wrapper temporary access call validates map target classification") {
  const std::string source = R"(
import /std/collections/*

wrapMapAuto() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 2i32)}
  return(values)
}

[return<int>]
main() {
  [i32] first{/std/collections/map/at(wrapMapAuto(), 1i32)}
  [i32] second{/std/collections/map/at_unsafe(wrapMapAuto(), 1i32)}
  return(plus(first, second))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("map wrapper temporary unsafe access validates direct stdlib helper") {
  const std::string source = R"(
import /std/collections/*

wrapMapAuto() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 2i32)}
  return(values)
}

[return<int>]
main() {
  return(/std/collections/map/at_unsafe(wrapMapAuto(), 1i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("map wrapper temporary access keeps canonical key diagnostics") {
  const std::string source = R"(
import /std/collections/*

wrapMapAuto() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 2i32)}
  return(values)
}

[return<int>]
main() {
  return(/std/collections/map/at(wrapMapAuto(), true))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("argument type mismatch for /std/collections/map/at parameter key") !=
        std::string::npos);
}

TEST_CASE("declared canonical map access positional reorder keeps key diagnostics") {
  const std::string source = R"(
[return<int>]
/std/collections/map/at([/std/collections/map<string, i32>] values, [string] key) {
  return(86i32)
}

[return<int>]
main() {
  [/std/collections/map<string, i32>] values{map<string, i32>("only"raw_utf8, 2i32)}
  return(/std/collections/map/at(1i32, values))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("argument type mismatch for /std/collections/map/at parameter key") !=
        std::string::npos);
}

TEST_CASE("map wrapper temporary capacity reports vector target diagnostic") {
  const std::string source = R"(
wrapMapAuto() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 2i32)}
  return(values)
}

[return<int>]
main() {
  return(capacity(wrapMapAuto()))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("capacity requires vector target") != std::string::npos);
}

TEST_CASE("count preserves receiver call-target diagnostics") {
  const std::string source = R"(
[return<int>]
main() {
  return(count(missing()))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target") != std::string::npos);
}

TEST_CASE("capacity preserves receiver call-target diagnostics") {
  const std::string source = R"(
[return<int>]
main() {
  return(capacity(missing()))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target") != std::string::npos);
}

TEST_CASE("at preserves receiver call-target diagnostics") {
  const std::string source = R"(
[return<int>]
main() {
  at(missing(), 0i32)
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target") != std::string::npos);
}

TEST_CASE("bare vector capacity call resolves through imported stdlib helper") {
  const std::string source = R"(
import /std/collections/*

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

TEST_CASE("bare vector capacity call requires imported stdlib helper or explicit definition") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32)}
  return(capacity(values))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /std/collections/vector/capacity") != std::string::npos);
}

TEST_CASE("vector capacity compatibility alias rejects template arguments") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32)}
  return(/vector/capacity<i32>(values))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /vector/capacity") != std::string::npos);
}

TEST_CASE("vector capacity compatibility alias rejects block arguments") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32)}
  return(/vector/capacity(values) { 1i32 })
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /vector/capacity") != std::string::npos);
}

TEST_CASE("vector capacity compatibility alias rejects wrong argument count") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32)}
  return(/vector/capacity(values, 1i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /vector/capacity") != std::string::npos);
}

TEST_CASE("capacity method on vector binding validates without imported stdlib helper") {
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

TEST_CASE("capacity wrapper temporaries infer i32 for chained methods") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc)]
wrapVector() {
  [vector<i32>] values{vector<i32>(1i32, 2i32, 3i32)}
  return(values)
}

[return<int>]
/i32/tag([i32] value) {
  return(plus(value, 50i32))
}

[effects(heap_alloc), return<int>]
main() {
  return(plus(capacity(wrapVector()).tag(), wrapVector().capacity().tag()))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("wrapper vector capacity method resolves through imported stdlib helper") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc)]
wrapVector() {
  [vector<i32>] values{vector<i32>(1i32, 2i32, 3i32)}
  return(values)
}

[effects(heap_alloc), return<int>]
main() {
  return(wrapVector().capacity())
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("wrapper vector capacity method requires helper") {
  const std::string source = R"(
[effects(heap_alloc)]
wrapVector() {
  [vector<i32>] values{vector<i32>(1i32, 2i32, 3i32)}
  return(values)
}

[effects(heap_alloc), return<int>]
main() {
  return(wrapVector().capacity())
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

TEST_CASE("capacity method rejects array target") {
  const std::string source = R"(
[return<int>]
main() {
  [array<i32>] values{array<i32>(1i32, 2i32)}
  return(values.capacity())
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("capacity requires vector target") != std::string::npos);
}

TEST_CASE("capacity method keeps unknown method on non-collection receiver") {
  const std::string source = R"(
Counter {
  [i32] value{0i32}
}

[return<int>]
main() {
  [Counter] counter{Counter()}
  return(counter.capacity())
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown method: /Counter/capacity") != std::string::npos);
}

TEST_CASE("capacity method keeps unknown method on non-collection wrapper temporary") {
  const std::string source = R"(
Counter {
  [i32] value{0i32}
}

makeCounter() {
  [Counter] counter{Counter()}
  return(counter)
}

[return<int>]
main() {
  return(makeCounter().capacity())
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown method: /Counter/capacity") != std::string::npos);
}

TEST_CASE("capacity call keeps unknown method on non-collection receiver") {
  const std::string source = R"(
Counter {
  [i32] value{0i32}
}

[return<int>]
main() {
  [Counter] counter{Counter()}
  return(capacity(counter))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown method: /Counter/capacity") != std::string::npos);
}

TEST_CASE("capacity call keeps unknown method on non-collection wrapper temporary") {
  const std::string source = R"(
Counter {
  [i32] value{0i32}
}

makeCounter() {
  [Counter] counter{Counter()}
  return(counter)
}

[return<int>]
main() {
  return(capacity(makeCounter()))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown method: /Counter/capacity") != std::string::npos);
}

TEST_CASE("at call keeps unknown method on non-collection receiver") {
  const std::string source = R"(
Counter {
  [i32] value{0i32}
}

[return<int>]
main() {
  [Counter] counter{Counter()}
  at(counter, 0i32)
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown method: /Counter/at") != std::string::npos);
}

TEST_CASE("at call keeps unknown method on non-collection wrapper temporary") {
  const std::string source = R"(
Counter {
  [i32] value{0i32}
}

makeCounter() {
  [Counter] counter{Counter()}
  return(counter)
}

[return<int>]
main() {
  at(makeCounter(), 0i32)
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown method: /Counter/at") != std::string::npos);
}

TEST_CASE("at_unsafe call keeps unknown method on non-collection wrapper temporary") {
  const std::string source = R"(
Counter {
  [i32] value{0i32}
}

makeCounter() {
  [Counter] counter{Counter()}
  return(counter)
}

[return<int>]
main() {
  at_unsafe(makeCounter(), 0i32)
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown method: /Counter/at_unsafe") != std::string::npos);
}

TEST_CASE("push call keeps unknown method on non-collection receiver") {
  const std::string source = R"(
Counter {
  [i32] value{0i32}
}

[effects(heap_alloc), return<int>]
main() {
  [Counter mut] counter{Counter()}
  push(counter, 1i32)
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown method: /Counter/push") != std::string::npos);
}

TEST_CASE("reserve method keeps unknown method on wrapper temporary") {
  const std::string source = R"(
Counter {
  [i32] value{0i32}
}

makeCounter() {
  [Counter mut] counter{Counter()}
  return(counter)
}

[effects(heap_alloc), return<int>]
main() {
  makeCounter().reserve(2i32)
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown method: /Counter/reserve") != std::string::npos);
}

TEST_CASE("at method validates on array binding") {
  const std::string source = R"(
[return<int>]
main() {
  [array<i32>] values{array<i32>(1i32, 2i32)}
  return(values.at(1i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("at_unsafe call validates on map binding") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 2i32)}
  return(/std/collections/map/at_unsafe(values, 1i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("capacity method keeps user-defined array helper precedence") {
  const std::string source = R"(
[return<int>]
/array/capacity([array<i32>] values) {
  return(count(values))
}

[return<int>]
main() {
  [array<i32>] values{array<i32>(1i32, 2i32)}
  return(values.capacity())
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("capacity call keeps user-defined array helper precedence") {
  const std::string source = R"(
[return<int>]
/array/capacity([array<i32>] values) {
  return(plus(count(values), 33i32))
}

[return<int>]
main() {
  [array<i32>] values{array<i32>(1i32, 2i32)}
  return(capacity(values))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("count method keeps user-defined array helper precedence") {
  const std::string source = R"(
[return<int>]
/array/count([array<i32>] values) {
  return(plus(count(values), 10i32))
}

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

TEST_CASE("count call keeps user-defined array helper precedence") {
  const std::string source = R"(
[return<int>]
/array/count([array<i32>] values) {
  return(plus(count(values), 11i32))
}

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

TEST_CASE("count call keeps user-defined map helper precedence") {
  const std::string source = R"(
[return<int>]
/map/count([map<i32, i32>] values) {
  return(plus(count(values), 12i32))
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

TEST_CASE("count method keeps user-defined map helper precedence") {
  const std::string source = R"(
[return<int>]
/map/count([map<i32, i32>] values) {
  return(93i32)
}

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

TEST_CASE("count call keeps user-defined string helper precedence") {
  const std::string source = R"(
[return<int>]
/string/count([string] values) {
  return(94i32)
}

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

TEST_CASE("count method keeps user-defined string helper precedence") {
  const std::string source = R"(
[return<int>]
/string/count([string] values) {
  return(95i32)
}

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

TEST_CASE("count method keeps user-defined vector helper precedence") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/vector/count([vector<i32>] values) {
  return(97i32)
}

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

TEST_CASE("capacity method keeps user-defined vector helper precedence") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/vector/capacity([vector<i32>] values) {
  return(20i32)
}

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

TEST_CASE("count call keeps user-defined vector helper precedence") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/vector/count([vector<i32>] values) {
  return(21i32)
}

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

TEST_CASE("capacity call keeps user-defined vector helper precedence") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/vector/capacity([vector<i32>] values) {
  return(20i32)
}

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

TEST_CASE("bare vector count auto inference resolves through imported stdlib helper") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32)}
  [auto] inferred{count(values)}
  return(inferred)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("bare vector count auto inference requires imported stdlib helper or explicit definition") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32)}
  [auto] inferred{count(values)}
  return(inferred)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK_FALSE(error.empty());
}

TEST_CASE("bare vector capacity auto inference resolves through imported stdlib helper") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32)}
  [auto] inferred{capacity(values)}
  return(inferred)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("bare vector capacity auto inference requires imported stdlib helper or explicit definition") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32)}
  [auto] inferred{capacity(values)}
  return(inferred)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK_FALSE(error.empty());
}

TEST_CASE("at call keeps user-defined array helper precedence") {
  const std::string source = R"(
[return<int>]
/array/at([array<i32>] values, [i32] index) {
  return(73i32)
}

[return<int>]
main() {
  [array<i32>] values{array<i32>(1i32, 2i32)}
  return(at(values, 1i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("at_unsafe call keeps user-defined map helper precedence") {
  const std::string source = R"(
[return<int>]
/map/at_unsafe([map<i32, i32>] values, [i32] key) {
  return(74i32)
}

[return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 2i32)}
  return(at_unsafe(values, 1i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("at method keeps user-defined array helper precedence") {
  const std::string source = R"(
[return<int>]
/array/at([array<i32>] values, [i32] index) {
  return(75i32)
}

[return<int>]
main() {
  [array<i32>] values{array<i32>(1i32, 2i32)}
  return(values.at(1i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("at_unsafe call keeps user-defined array helper precedence") {
  const std::string source = R"(
[return<int>]
/array/at_unsafe([array<i32>] values, [i32] index) {
  return(85i32)
}

[return<int>]
main() {
  [array<i32>] values{array<i32>(1i32, 2i32)}
  return(at_unsafe(values, 1i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("at_unsafe method keeps user-defined array helper precedence") {
  const std::string source = R"(
[return<int>]
/array/at_unsafe([array<i32>] values, [i32] index) {
  return(86i32)
}

[return<int>]
main() {
  [array<i32>] values{array<i32>(1i32, 2i32)}
  return(values.at_unsafe(1i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("at_unsafe method keeps user-defined map helper precedence") {
  const std::string source = R"(
[return<int>]
/map/at_unsafe([map<i32, i32>] values, [i32] key) {
  return(76i32)
}

[return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 2i32)}
  return(values.at_unsafe(1i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("at call keeps user-defined map helper precedence") {
  const std::string source = R"(
[return<int>]
/map/at([map<i32, i32>] values, [i32] key) {
  return(67i32)
}

[return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 2i32)}
  return(at(values, 1i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("at method keeps user-defined map helper precedence") {
  const std::string source = R"(
[return<int>]
/map/at([map<i32, i32>] values, [i32] key) {
  return(65i32)
}

[return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 2i32)}
  return(values.at(1i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("at call keeps user-defined vector helper precedence") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/vector/at([vector<i32>] values, [i32] index) {
  return(77i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32)}
  return(at(values, 1i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("at method keeps user-defined vector helper precedence") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/vector/at([vector<i32>] values, [i32] index) {
  return(78i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32)}
  return(values.at(1i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("at_unsafe call keeps user-defined string helper precedence") {
  const std::string source = R"(
[return<int>]
/string/at_unsafe([string] values, [i32] index) {
  return(79i32)
}

[return<int>]
main() {
  [string] text{"abc"utf8}
  return(at_unsafe(text, 1i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("at_unsafe method keeps user-defined string helper precedence") {
  const std::string source = R"(
[return<int>]
/string/at_unsafe([string] values, [i32] index) {
  return(80i32)
}

[return<int>]
main() {
  [string] text{"abc"utf8}
  return(text.at_unsafe(1i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("at_unsafe call keeps user-defined vector helper precedence") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/vector/at_unsafe([vector<i32>] values, [i32] index) {
  return(81i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32)}
  return(at_unsafe(values, 1i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("at_unsafe method keeps user-defined vector helper precedence") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/vector/at_unsafe([vector<i32>] values, [i32] index) {
  return(82i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32)}
  return(values.at_unsafe(1i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("at call keeps user-defined string helper precedence") {
  const std::string source = R"(
[return<int>]
/string/at([string] values, [i32] index) {
  return(83i32)
}

[return<int>]
main() {
  [string] text{"abc"utf8}
  return(at(text, 1i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("at method keeps user-defined string helper precedence") {
  const std::string source = R"(
[return<int>]
/string/at([string] values, [i32] index) {
  return(84i32)
}

[return<int>]
main() {
  [string] text{"abc"utf8}
  return(text.at(1i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("vector push alias requires mutable vector binding") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32)}
  /vector/push(values, 2i32)
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK_FALSE(error.empty());
}

TEST_CASE("vector push alias requires heap_alloc effect") {
  const std::string source = R"(
[return<int>]
main() {
  [vector<i32> mut] values{vector<i32>()}
  /vector/push(values, 2i32)
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK_FALSE(error.empty());
}

TEST_CASE("push on array reports vector binding before effect requirement") {
  const auto checkInvalidPush = [](const std::string &stmtText) {
    const std::string source =
        "[return<int>]\n"
        "main() {\n"
        "  [array<i32> mut] values{array<i32>(1i32)}\n"
        "  " +
        stmtText +
        "\n"
        "  return(0i32)\n"
        "}\n";
    std::string error;
    CHECK_FALSE(validateProgram(source, "/main", error));
    CHECK(error.find("push requires vector binding") != std::string::npos);
  };

  checkInvalidPush("push(values, 2i32)");
  checkInvalidPush("values.push(2i32)");
}

TEST_CASE("bare vector push validates through imported stdlib helper") {
  const std::string source = R"(
import /std/collections/*

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

TEST_CASE("push rejects non-relocation-trivial vector element types") {
  const std::string source = R"(
import /std/collections/*

[struct]
Mover() {
  [i32] value{1i32}

  [mut]
  Move([Reference<Self>] other) {
    assign(this, other)
  }
}

[effects(heap_alloc), return<int>]
main() {
  [vector<Mover> mut] values{vector<Mover>(Mover())}
  push(values, Mover())
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find(
            "push requires relocation-trivial vector element type until container move/reallocation semantics are "
            "implemented: Mover") != std::string::npos);
}

TEST_CASE("push allows relocation-trivial string vector elements") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<string> mut] values{vector<string>("left"raw_utf8)}
  push(values, "right"raw_utf8)
  return(count(values))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("experimental vector ownership-sensitive helpers accept non-trivial elements") {
  const std::string source = R"(
import /std/collections/experimental_vector/*

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
  [Vector<Owned> mut] values{vectorPair<Owned>(Owned(10i32), Owned(20i32))}
  vectorPush<Owned>(values, Owned(30i32))
  vectorReserve<Owned>(values, 6i32)
  [i32] picked{plus(vectorAt<Owned>(values, 0i32).value, vectorAtUnsafe<Owned>(values, 2i32).value)}
  vectorPop<Owned>(values)
  vectorRemoveAt<Owned>(values, 0i32)
  vectorRemoveSwap<Owned>(values, 0i32)
  vectorClear<Owned>(values)
  return(picked)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("canonical vector helpers route experimental vector receivers onto stdlib vector storage") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_vector/*

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

[effects(heap_alloc), return<Vector<Owned>>]
wrapValues() {
  return(vectorPair<Owned>(Owned(10i32), Owned(20i32)))
}

[effects(heap_alloc), return<int>]
main() {
  [Vector<Owned> mut] values{wrapValues()}
  /std/collections/vectorPush<Owned>(values, Owned(30i32))
  /std/collections/vectorReserve<Owned>(values, 6i32)
  [i32 mut] total{/std/collections/vectorCount<Owned>(values)}
  assign(total, plus(total, /std/collections/vectorCapacity<Owned>(values)))
  assign(total, plus(total, /std/collections/vectorAt<Owned>(values, 0i32).value))
  assign(total, plus(total, /std/collections/vectorAtUnsafe<Owned>(values, 2i32).value))
  /std/collections/vector/push<Owned>(values, Owned(40i32))
  assign(total, plus(total, /std/collections/vector/count<Owned>(values)))
  assign(total, plus(total, values.at(3i32).value))
  /std/collections/vector/remove_at<Owned>(values, 1i32)
  /std/collections/vectorRemoveSwap<Owned>(values, 0i32)
  values.pop()
  /std/collections/vector/clear<Owned>(values)
  return(plus(total, values.count()))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("canonical vector helpers still require stdlib imports for experimental vector receivers") {
  const std::string source = R"(
import /std/collections/experimental_vector/*

[effects(heap_alloc), return<int>]
main() {
  [Vector<i32>] values{vectorPair<i32>(4i32, 5i32)}
  return(/std/collections/vector/count<i32>(values))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /std/collections/vector/count") != std::string::npos);
}

TEST_CASE("push on mutable vector field access reports vector-binding diagnostics") {
  const std::string source = R"(
import /std/collections/*

[struct]
Buffer() {
  [vector<i32> mut] values{vector<i32>()}
}

namespace Buffer {
  [effects(heap_alloc), return<void>]
  append([Buffer mut] self, [i32] value) {
    push(self.values, value)
  }
}

[effects(heap_alloc), return<int>]
main() {
  [Buffer mut] buffer{Buffer()}
  buffer.append(2i32)
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("push requires vector binding") != std::string::npos);
}

TEST_CASE("push validates on mutable soa_vector parameter") {
  const std::string source = R"(
Particle() {
  [i32] x{1i32}
}

[effects(heap_alloc), return<void>]
touch([soa_vector<Particle> mut] values) {
  push(values, Particle(2i32))
}

[return<int>]
main() {
  return(0i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("collection syntax parity validates for call and method forms") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] viaCall{vector<i32>(10i32, 20i32, 30i32)}
  [vector<i32> mut] viaMethod{vector<i32>(10i32, 20i32, 30i32)}
  pop(viaCall)
  viaMethod.pop()
  reserve(viaCall, 3i32)
  viaMethod.reserve(3i32)
  push(viaCall, 40i32)
  viaMethod.push(40i32)
  return(plus(
      plus(at(viaCall, 2i32), viaMethod.at(2i32)),
      plus(viaCall[2i32], viaMethod[2i32])))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("collection indexing syntax parity keeps integer-index diagnostics") {
  const auto checkInvalidIndex = [](const std::string &exprText) {
    const std::string source =
        "[return<int>]\n"
        "main() {\n"
        "  [array<i32>] values{array<i32>(1i32, 2i32)}\n"
        "  return(" +
        exprText +
        ")\n"
        "}\n";
    std::string error;
    CHECK_FALSE(validateProgram(source, "/main", error));
    CHECK(error.find("at requires integer index") != std::string::npos);
  };

  checkInvalidIndex("at(values, \"oops\"utf8)");
  checkInvalidIndex("values.at(\"oops\"utf8)");
  checkInvalidIndex("values[\"oops\"utf8]");
}

TEST_CASE("bare vector push requires imported stdlib helper before template specialization") {
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
  CHECK_FALSE(error.empty());
}

TEST_CASE("bare vector push requires imported stdlib helper before arity validation") {
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
  CHECK_FALSE(error.empty());
}

TEST_CASE("push call keeps user-defined vector helper precedence") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<void>]
/vector/push([vector<i32> mut] values, [i32] value) {
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32, 2i32)}
  push(values, 3i32)
  return(count(values))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("push method keeps user-defined vector helper precedence") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<void>]
/vector/push([vector<i32> mut] values, [i32] value) {
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32, 2i32)}
  values.push(3i32)
  return(count(values))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("vector reserve alias requires mutable vector binding") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32)}
  /vector/reserve(values, 8i32)
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK_FALSE(error.empty());
}

TEST_CASE("vector reserve alias requires heap_alloc effect") {
  const std::string source = R"(
[return<int>]
main() {
  [vector<i32> mut] values{vector<i32>()}
  /vector/reserve(values, 8i32)
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK_FALSE(error.empty());
}

TEST_CASE("reserve on array reports vector binding before effect requirement") {
  const auto checkInvalidReserve = [](const std::string &stmtText) {
    const std::string source =
        "[return<int>]\n"
        "main() {\n"
        "  [array<i32> mut] values{array<i32>(1i32)}\n"
        "  " +
        stmtText +
        "\n"
        "  return(0i32)\n"
        "}\n";
    std::string error;
    CHECK_FALSE(validateProgram(source, "/main", error));
    CHECK(error.find("reserve requires vector binding") != std::string::npos);
  };

  checkInvalidReserve("reserve(values, 8i32)");
  checkInvalidReserve("values.reserve(8i32)");
}

TEST_CASE("vector reserve alias requires integer capacity") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32)}
  /vector/reserve(values, "hi"utf8)
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK_FALSE(error.empty());
}

TEST_CASE("reserve rejects bool capacity in call and method forms") {
  const auto checkInvalidReserve = [](const std::string &stmtText) {
    const std::string source =
        "[effects(heap_alloc), return<int>]\n"
        "main() {\n"
        "  [vector<i32> mut] values{vector<i32>(1i32, 2i32)}\n"
        "  " +
        stmtText +
        "\n"
        "  return(0i32)\n"
        "}\n";
    std::string error;
    CHECK_FALSE(validateProgram(source, "/main", error));
    CHECK_FALSE(error.empty());
  };

  checkInvalidReserve("/vector/reserve(values, true)");
  checkInvalidReserve("values.reserve(true)");
}

TEST_CASE("bare vector reserve requires imported stdlib helper before template specialization") {
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
  CHECK_FALSE(error.empty());
}

TEST_CASE("bare vector reserve requires imported stdlib helper before arity validation") {
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
  CHECK_FALSE(error.empty());
}

TEST_CASE("bare vector reserve validates through imported stdlib helper") {
  const std::string source = R"(
import /std/collections/*

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

TEST_CASE("reserve rejects nested non-relocation-trivial vector element types") {
  const std::string source = R"(
import /std/collections/*

[struct]
Mover() {
  [i32] value{1i32}

  [mut]
  Move([Reference<Self>] other) {
    assign(this, other)
  }
}

[struct]
Wrapper() {
  [Mover] value{Mover()}
}

[effects(heap_alloc), return<int>]
main() {
  [vector<Wrapper> mut] values{vector<Wrapper>(Wrapper())}
  reserve(values, 4i32)
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find(
            "reserve requires relocation-trivial vector element type until container move/reallocation semantics are "
            "implemented: Wrapper") != std::string::npos);
}

TEST_CASE("reserve validates on mutable soa_vector parameter") {
  const std::string source = R"(
Particle() {
  [i32] x{1i32}
}

[effects(heap_alloc), return<void>]
touch([soa_vector<Particle> mut] values) {
  reserve(values, 8i32)
}

[return<int>]
main() {
  return(0i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("pop still rejects soa_vector helper target") {
  const std::string source = R"(
Particle() {
  [i32] x{1i32}
}

[return<void>]
touch([soa_vector<Particle> mut] values) {
  pop(values)
}

[return<int>]
main() {
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("pop requires vector binding") != std::string::npos);
}

TEST_CASE("reserve call keeps user-defined vector helper precedence") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<void>]
/vector/reserve([vector<i32> mut] values, [i32] capacity) {
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32, 2i32)}
  reserve(values, 3i32)
  return(count(values))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("reserve method keeps user-defined vector helper precedence") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<void>]
/vector/reserve([vector<i32> mut] values, [i32] capacity) {
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32, 2i32)}
  values.reserve(3i32)
  return(count(values))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("capacity method keeps same-path vector helper precedence") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
/vector/capacity([vector<i32>] values) {
  return(77i32)
}

[effects(heap_alloc), return<vector<i32>>]
wrapVector() {
  return(vector<i32>(1i32, 2i32, 3i32))
}

[effects(heap_alloc), return<int>]
main() {
  return(wrapVector().capacity())
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("vector pop alias requires mutable vector binding") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32)}
  /vector/pop(values)
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK_FALSE(error.empty());
}

TEST_CASE("bare vector pop validates through imported stdlib helper") {
  const std::string source = R"(
import /std/collections/*

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

TEST_CASE("pop rejects non-drop-trivial vector element types") {
  const std::string source = R"(
import /std/collections/*

[struct]
Owned() {
  [i32] value{1i32}

  Destroy() {
  }
}

[effects(heap_alloc), return<int>]
main() {
  [vector<Owned> mut] values{vector<Owned>(Owned())}
  pop(values)
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("pop requires drop-trivial vector element type until container drop semantics are implemented: Owned") !=
        std::string::npos);
}

TEST_CASE("pop allows drop-trivial string vector elements") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<string> mut] values{vector<string>("left"raw_utf8, "right"raw_utf8)}
  pop(values)
  return(count(values))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("bare vector pop requires imported stdlib helper before block-arg validation") {
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
  CHECK_FALSE(error.empty());
}

TEST_CASE("bare vector pop requires imported stdlib helper before template specialization") {
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
  CHECK_FALSE(error.empty());
}

TEST_CASE("bare vector pop requires imported stdlib helper before arity validation") {
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
  CHECK_FALSE(error.empty());
}

TEST_CASE("pop call keeps user-defined vector helper precedence") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<void>]
/vector/pop([vector<i32> mut] values) {
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32, 2i32)}
  pop(values)
  return(count(values))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("pop method keeps user-defined vector helper precedence") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<void>]
/vector/pop([vector<i32> mut] values) {
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32, 2i32)}
  values.pop()
  return(count(values))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("vector clear alias requires mutable vector binding") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32)}
  /vector/clear(values)
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK_FALSE(error.empty());
}

TEST_CASE("bare vector clear validates through imported stdlib helper") {
  const std::string source = R"(
import /std/collections/*

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

TEST_CASE("clear rejects vector elements with nested drop requirements") {
  const std::string source = R"(
import /std/collections/*

[struct]
Owned() {
  [i32] value{1i32}

  Destroy() {
  }
}

[struct]
Wrapper() {
  [Owned] value{Owned()}
}

[effects(heap_alloc), return<int>]
main() {
  [vector<Wrapper> mut] values{vector<Wrapper>(Wrapper())}
  clear(values)
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("clear requires drop-trivial vector element type until container drop semantics are implemented: Wrapper") !=
        std::string::npos);
}

TEST_CASE("clear call keeps user-defined vector helper precedence") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<void>]
/vector/clear([vector<i32> mut] values) {
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32, 2i32)}
  clear(values)
  return(count(values))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("clear method keeps user-defined vector helper precedence") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<void>]
/vector/clear([vector<i32> mut] values) {
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32, 2i32)}
  values.clear()
  return(count(values))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("bare vector clear requires imported stdlib helper before template specialization") {
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
  CHECK_FALSE(error.empty());
}

TEST_CASE("bare vector clear requires imported stdlib helper before arity validation") {
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
  CHECK_FALSE(error.empty());
}

TEST_CASE("vector remove_at alias requires mutable vector binding") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32)}
  /vector/remove_at(values, 0i32)
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK_FALSE(error.empty());
}

TEST_CASE("vector remove_at alias requires integer index") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32)}
  /vector/remove_at(values, "hi"utf8)
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK_FALSE(error.empty());
}

TEST_CASE("remove_at rejects bool index in call and method forms") {
  const auto checkInvalidRemoveAt = [](const std::string &stmtText) {
    const std::string source =
        "[effects(heap_alloc), return<int>]\n"
        "main() {\n"
        "  [vector<i32> mut] values{vector<i32>(1i32, 2i32)}\n"
        "  " +
        stmtText +
        "\n"
        "  return(0i32)\n"
        "}\n";
    std::string error;
    CHECK_FALSE(validateProgram(source, "/main", error));
    CHECK_FALSE(error.empty());
  };

  checkInvalidRemoveAt("/vector/remove_at(values, true)");
  checkInvalidRemoveAt("values.remove_at(true)");
}

TEST_CASE("bare vector remove_at requires imported stdlib helper before template specialization") {
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
  CHECK_FALSE(error.empty());
}

TEST_CASE("bare vector remove_at requires imported stdlib helper before arity validation") {
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
  CHECK_FALSE(error.empty());
}

TEST_CASE("remove_at rejects non-drop-trivial vector element types") {
  const std::string source = R"(
import /std/collections/*

[struct]
Owned() {
  [i32] value{1i32}

  Destroy() {
  }
}

[effects(heap_alloc), return<int>]
main() {
  [vector<Owned> mut] values{vector<Owned>(Owned(), Owned())}
  remove_at(values, 0i32)
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find(
            "remove_at requires drop-trivial vector element type until container drop semantics are implemented: "
            "Owned") != std::string::npos);
}

TEST_CASE("remove_at rejects nested non-relocation-trivial vector element types") {
  const std::string source = R"(
import /std/collections/*

[struct]
Mover() {
  [i32] value{1i32}

  [mut]
  Move([Reference<Self>] other) {
    assign(this, other)
  }
}

[struct]
Wrapper() {
  [Mover] value{Mover()}
}

[effects(heap_alloc), return<int>]
main() {
  [vector<Wrapper> mut] values{vector<Wrapper>(Wrapper(), Wrapper())}
  remove_at(values, 0i32)
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find(
            "remove_at requires relocation-trivial vector element type until container move/reallocation semantics "
            "are implemented: Wrapper") != std::string::npos);
}

TEST_CASE("remove_at call keeps user-defined vector helper precedence") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<void>]
/vector/remove_at([vector<i32> mut] values, [i32] index) {
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32, 2i32)}
  remove_at(values, 1i32)
  return(count(values))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("remove_at method keeps user-defined vector helper precedence") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<void>]
/vector/remove_at([vector<i32> mut] values, [i32] index) {
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32, 2i32)}
  values.remove_at(1i32)
  return(count(values))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("vector remove_swap alias requires integer index") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32)}
  /vector/remove_swap(values, "hi"utf8)
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK_FALSE(error.empty());
}

TEST_CASE("remove_swap rejects bool index in call and method forms") {
  const auto checkInvalidRemoveSwap = [](const std::string &stmtText) {
    const std::string source =
        "[effects(heap_alloc), return<int>]\n"
        "main() {\n"
        "  [vector<i32> mut] values{vector<i32>(1i32, 2i32)}\n"
        "  " +
        stmtText +
        "\n"
        "  return(0i32)\n"
        "}\n";
    std::string error;
    CHECK_FALSE(validateProgram(source, "/main", error));
    CHECK_FALSE(error.empty());
  };

  checkInvalidRemoveSwap("/vector/remove_swap(values, true)");
  checkInvalidRemoveSwap("values.remove_swap(true)");
}

TEST_CASE("vector remove_swap alias requires mutable vector binding") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32)}
  /vector/remove_swap(values, 1i32)
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK_FALSE(error.empty());
}

TEST_CASE("bare vector remove_swap requires imported stdlib helper before template specialization") {
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
  CHECK_FALSE(error.empty());
}

TEST_CASE("bare vector remove_swap requires imported stdlib helper before arity validation") {
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
  CHECK_FALSE(error.empty());
}

TEST_CASE("remove_swap rejects non-drop-trivial vector element types") {
  const std::string source = R"(
import /std/collections/*

[struct]
Owned() {
  [i32] value{1i32}

  Destroy() {
  }
}

[effects(heap_alloc), return<int>]
main() {
  [vector<Owned> mut] values{vector<Owned>(Owned(), Owned())}
  remove_swap(values, 0i32)
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find(
            "remove_swap requires drop-trivial vector element type until container drop semantics are implemented: "
            "Owned") != std::string::npos);
}

TEST_CASE("remove_swap rejects nested non-relocation-trivial vector element types") {
  const std::string source = R"(
import /std/collections/*

[struct]
Mover() {
  [i32] value{1i32}

  [mut]
  Move([Reference<Self>] other) {
    assign(this, other)
  }
}

[struct]
Wrapper() {
  [Mover] value{Mover()}
}

[effects(heap_alloc), return<int>]
main() {
  [vector<Wrapper> mut] values{vector<Wrapper>(Wrapper(), Wrapper())}
  remove_swap(values, 0i32)
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find(
            "remove_swap requires relocation-trivial vector element type until container move/reallocation semantics "
            "are implemented: Wrapper") != std::string::npos);
}

TEST_CASE("bare vector remove_swap validates through imported stdlib helper") {
  const std::string source = R"(
import /std/collections/*

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

TEST_CASE("remove_swap call keeps user-defined vector helper precedence") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<void>]
/vector/remove_swap([vector<i32> mut] values, [i32] index) {
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32, 2i32)}
  remove_swap(values, 1i32)
  return(count(values))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("remove_swap method keeps user-defined vector helper precedence") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<void>]
/vector/remove_swap([vector<i32> mut] values, [i32] index) {
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32, 2i32)}
  values.remove_swap(1i32)
  return(count(values))
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

TEST_CASE("vector helper named args on array targets report vector binding") {
  const auto checkInvalidStatement = [](const std::string &stmtText) {
    const std::string source =
        "[effects(heap_alloc), return<int>]\n"
        "main() {\n"
        "  [array<i32> mut] values{array<i32>(1i32, 2i32)}\n"
        "  " +
        stmtText +
        "\n"
        "  return(0i32)\n"
        "}\n";
    std::string error;
    CHECK_FALSE(validateProgram(source, "/main", error));
    CHECK(error.find("requires vector binding") != std::string::npos);
  };

  checkInvalidStatement("push([values] values, [value] 3i32)");
  checkInvalidStatement("reserve([values] values, [capacity] 8i32)");
  checkInvalidStatement("remove_at([values] values, [index] 0i32)");
  checkInvalidStatement("remove_swap([values] values, [index] 0i32)");
  checkInvalidStatement("pop([values] values)");
  checkInvalidStatement("clear([values] values)");
  checkInvalidStatement("values.push([value] 3i32)");
  checkInvalidStatement("values.reserve([capacity] 8i32)");
  checkInvalidStatement("values.remove_at([index] 0i32)");
  checkInvalidStatement("values.remove_swap([index] 0i32)");
}

TEST_CASE("bare vector mutator named args reject builtin call syntax even through imported stdlib helpers") {
  const auto checkInvalidStatement = [](const std::string &stmtText) {
    const std::string source =
        "import /std/collections/*\n\n"
        "[effects(heap_alloc), return<int>]\n"
        "main() {\n"
        "  [vector<i32> mut] values{vector<i32>(1i32, 2i32)}\n"
        "  " +
        stmtText +
        "\n"
        "  return(0i32)\n"
        "}\n";
    std::string error;
    CHECK_FALSE(validateProgram(source, "/main", error));
    CHECK(error.find("named arguments not supported for builtin calls") != std::string::npos);
  };

  checkInvalidStatement("push([value] 3i32, [values] values)");
  checkInvalidStatement("reserve([capacity] 8i32, [values] values)");
  checkInvalidStatement("remove_at([index] 0i32, [values] values)");
  checkInvalidStatement("remove_swap([index] 0i32, [values] values)");
  checkInvalidStatement("pop([values] values)");
  checkInvalidStatement("clear([values] values)");
}

TEST_CASE("bare vector mutator named args require imported stdlib helpers") {
  const auto checkInvalidStatement = [](const std::string &stmtText) {
    const std::string source =
        "[effects(heap_alloc), return<int>]\n"
        "main() {\n"
        "  [vector<i32> mut] values{vector<i32>(1i32, 2i32)}\n"
        "  " +
        stmtText +
        "\n"
        "  return(0i32)\n"
        "}\n";
    std::string error;
    CHECK_FALSE(validateProgram(source, "/main", error));
    CHECK(error.find("named arguments not supported for builtin calls") != std::string::npos);
  };

  checkInvalidStatement("push([value] 3i32, [values] values)");
  checkInvalidStatement("reserve([capacity] 8i32, [values] values)");
  checkInvalidStatement("remove_at([index] 0i32, [values] values)");
  checkInvalidStatement("remove_swap([index] 0i32, [values] values)");
  checkInvalidStatement("pop([values] values)");
  checkInvalidStatement("clear([values] values)");
}

TEST_CASE("vector helper expressions with named arguments stay statement-only") {
  struct HelperCase {
    const char *name;
    const char *args;
  };
  const HelperCase helpers[] = {
      {"push", "[values] values, [value] 2i32"},
      {"pop", "[values] values"},
      {"reserve", "[values] values, [capacity] 8i32"},
      {"clear", "[values] values"},
      {"remove_at", "[values] values, [index] 0i32"},
      {"remove_swap", "[values] values, [index] 0i32"},
  };
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

TEST_CASE("vector helper method expressions with named arguments require helper resolution") {
  struct HelperCase {
    const char *name;
    const char *args;
  };
  const HelperCase helpers[] = {
      {"push", "[value] 2i32"},
      {"reserve", "[capacity] 8i32"},
      {"remove_at", "[index] 0i32"},
      {"remove_swap", "[index] 0i32"},
  };
  for (const auto &helper : helpers) {
    CAPTURE(helper.name);
    const std::string source =
        "[effects(heap_alloc), return<int>]\n"
        "main() {\n"
        "  [vector<i32> mut] values{vector<i32>(1i32)}\n"
        "  return(values." +
        std::string(helper.name) + "(" + helper.args + "))\n"
        "}\n";
    std::string error;
    CHECK_FALSE(validateProgram(source, "/main", error));
    CHECK_FALSE(error.empty());
  }
}

TEST_CASE("namespaced vector helper statement form is accepted") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32)}
  /vector/push(values, 2i32)
  return(0i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("namespaced vector helper expression form stays statement-only") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32)}
  return(/vector/push(values, 2i32))
}
  )";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /vector/push") != std::string::npos);
}

TEST_CASE("stdlib namespaced vector helper accepts statement form") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32)}
  /std/collections/vector/push(values, 2i32)
  return(count(values))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("namespaced vector mutator statement helpers are accepted") {
  struct HelperCase {
    const char *name;
    const char *args;
  };
  const HelperCase helpers[] = {
      {"push", "values, 2i32"},
      {"pop", "values"},
      {"reserve", "values, 8i32"},
      {"clear", "values"},
      {"remove_at", "values, 0i32"},
      {"remove_swap", "values, 0i32"},
  };
  const char *prefixes[] = {"/std/collections/vector/"};
  for (const auto &helper : helpers) {
    for (const auto *prefix : prefixes) {
      CAPTURE(helper.name);
      CAPTURE(prefix);
      const std::string source =
          "import /std/collections/*\n\n"
          "[effects(heap_alloc), return<int>]\n"
          "main() {\n"
          "  [vector<i32> mut] values{vector<i32>(1i32, 2i32)}\n"
          "  " + std::string(prefix) + helper.name + "(" + helper.args + ")\n"
          "  return(count(values))\n"
          "}\n";
      std::string error;
      CHECK(validateProgram(source, "/main", error));
      CHECK(error.empty());
    }
  }
}

TEST_CASE("namespaced vector mutator expression helpers stay statement-only") {
  struct HelperCase {
    const char *name;
    const char *args;
  };
  const HelperCase helpers[] = {
      {"push", "values, 2i32"},
      {"pop", "values"},
      {"reserve", "values, 8i32"},
      {"clear", "values"},
      {"remove_at", "values, 0i32"},
      {"remove_swap", "values, 0i32"},
  };
  const char *prefixes[] = {"/vector/"};
  for (const auto &helper : helpers) {
    for (const auto *prefix : prefixes) {
      CAPTURE(helper.name);
      CAPTURE(prefix);
      const std::string source =
          "[effects(heap_alloc), return<int>]\n"
          "main() {\n"
          "  [vector<i32> mut] values{vector<i32>(1i32, 2i32)}\n"
          "  return(" + std::string(prefix) + helper.name + "(" + helper.args + "))\n"
          "}\n";
      std::string error;
      CHECK_FALSE(validateProgram(source, "/main", error));
      CHECK(error.find("unknown call target: " + std::string(prefix) + helper.name) != std::string::npos);
    }
  }
}

TEST_CASE("stdlib namespaced vector helper statement rejects compatibility helper definition fallback") {
  const std::string source = R"(
[effects(heap_alloc), return<void>]
/vector/push([vector<i32> mut] values, [i32] value) {
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32)}
  /std/collections/vector/push(2i32, values)
  return(count(values))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK_FALSE(error.empty());
}

TEST_CASE("stdlib namespaced vector push accepts named arguments through imported stdlib helper") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32)}
  /std/collections/vector/push([value] 2i32, [values] values)
  return(count(values))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("stdlib namespaced vector pop accepts named arguments through imported stdlib helper") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32, 2i32)}
  /std/collections/vector/pop([values] values)
  return(count(values))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("stdlib namespaced vector reserve accepts named arguments through imported stdlib helper") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32, 2i32)}
  /std/collections/vector/reserve([capacity] 8i32, [values] values)
  return(plus(count(values), capacity(values)))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("stdlib namespaced vector push requires imported stdlib helper") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32)}
  /std/collections/vector/push(values, 2i32)
  return(count(values))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /std/collections/vector/push") != std::string::npos);
}

TEST_CASE("stdlib namespaced vector pop requires imported stdlib helper") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32, 2i32)}
  /std/collections/vector/pop(values)
  return(count(values))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /std/collections/vector/pop") != std::string::npos);
}

TEST_CASE("stdlib namespaced vector reserve requires imported stdlib helper") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32, 2i32)}
  /std/collections/vector/reserve(values, 8i32)
  return(count(values))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /std/collections/vector/reserve") != std::string::npos);
}

TEST_CASE("stdlib namespaced vector clear accepts named arguments through imported stdlib helper") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32, 2i32)}
  /std/collections/vector/clear([values] values)
  return(count(values))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("stdlib namespaced vector remove_at accepts named arguments through imported stdlib helper") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32, 2i32, 3i32)}
  /std/collections/vector/remove_at([index] 1i32, [values] values)
  return(plus(count(values), at(values, 1i32)))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("stdlib namespaced vector remove_swap accepts named arguments through imported stdlib helper") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32, 2i32, 3i32)}
  /std/collections/vector/remove_swap([index] 0i32, [values] values)
  return(plus(count(values), at(values, 0i32)))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("stdlib namespaced vector clear requires imported stdlib helper") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32, 2i32)}
  /std/collections/vector/clear(values)
  return(count(values))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /std/collections/vector/clear") != std::string::npos);
}

TEST_CASE("stdlib namespaced vector remove_at requires imported stdlib helper") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32, 2i32, 3i32)}
  /std/collections/vector/remove_at(values, 1i32)
  return(count(values))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /std/collections/vector/remove_at") != std::string::npos);
}

TEST_CASE("stdlib namespaced vector remove_swap requires imported stdlib helper") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32, 2i32, 3i32)}
  /std/collections/vector/remove_swap(values, 1i32)
  return(count(values))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /std/collections/vector/remove_swap") != std::string::npos);
}

TEST_CASE("stdlib namespaced vector clear method requires imported stdlib helper") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32, 2i32)}
  values./std/collections/vector/clear()
  return(count(values))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown method: /std/collections/vector/clear") != std::string::npos);
}

TEST_CASE("stdlib namespaced vector push method requires imported stdlib helper") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32, 2i32)}
  values./std/collections/vector/push(3i32)
  return(count(values))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown method: /std/collections/vector/push") != std::string::npos);
}

TEST_CASE("stdlib namespaced vector pop method requires imported stdlib helper") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32, 2i32)}
  values./std/collections/vector/pop()
  return(count(values))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown method: /std/collections/vector/pop") != std::string::npos);
}

TEST_CASE("stdlib namespaced vector reserve method requires imported stdlib helper") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32, 2i32)}
  values./std/collections/vector/reserve(4i32)
  return(count(values))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown method: /std/collections/vector/reserve") != std::string::npos);
}

TEST_CASE("stdlib namespaced vector remove_at method requires imported stdlib helper") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32, 2i32, 3i32)}
  values./std/collections/vector/remove_at(1i32)
  return(count(values))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown method: /std/collections/vector/remove_at") != std::string::npos);
}

TEST_CASE("stdlib namespaced vector remove_swap method requires imported stdlib helper") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32, 2i32, 3i32)}
  values./std/collections/vector/remove_swap(1i32)
  return(count(values))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown method: /std/collections/vector/remove_swap") != std::string::npos);
}

TEST_CASE("stdlib namespaced vector push expression requires imported stdlib helper") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32)}
  return(/std/collections/vector/push(values, 2i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /std/collections/vector/push") != std::string::npos);
}

TEST_CASE("stdlib namespaced vector push named args require imported stdlib helper in expressions") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32)}
  return(/std/collections/vector/push([values] values, [value] 2i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("push") != std::string::npos);
}

TEST_CASE("array namespaced vector mutator alias named args stay statement-only in expressions") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32)}
  return(/array/push([values] values, [value] 2i32))
}
  )";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("push is only supported as a statement") != std::string::npos);
}

TEST_CASE("namespaced vector helper duplicate named args stay statement-only in expressions") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32)}
  return(/vector/push([values] values, [values] values))
}
  )";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /vector/push") != std::string::npos);
}

TEST_CASE("array namespaced vector mutator alias duplicate named args stay statement-only in expressions") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32)}
  return(/array/push([values] values, [values] values))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("only supported as a statement") != std::string::npos);
}

TEST_CASE("array namespaced vector mutator alias statement is rejected") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32)}
  /array/push(values, 2i32)
  return(count(values))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("push is only supported as a statement") != std::string::npos);
}

TEST_CASE("vector mutator alias block args keep builtin diagnostics") {
  const std::string source = R"(
[return<int>]
/std/collections/vector/push([vector<i32> mut] values, [i32] value) {
  return(value)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32)}
  /vector/push(values, 2i32) { 1i32 }
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("push does not accept block arguments") != std::string::npos);
}

TEST_CASE("stdlib namespaced vector helper duplicate named args stay statement-only in expressions") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32)}
  return(/std/collections/vector/push([values] values, [values] values))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("push") != std::string::npos);
}

TEST_CASE("vector helper method expression resolves through canonical stdlib helper") {
  const std::string source = R"(
[return<int>]
/std/collections/vector/push([vector<i32> mut] values, [i32] value) {
  return(value)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32)}
  return(values.push(2i32))
}
  )";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("push is only supported as a statement") != std::string::npos);
}

TEST_CASE("vector helper method expression reports canonical helper argument mismatch") {
  const std::string source = R"(
[return<int>]
/std/collections/vector/push([vector<i32> mut] values, [bool] value) {
  return(convert<int>(value))
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32)}
  return(values.push(2i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("push is only supported as a statement") != std::string::npos);
}

TEST_CASE("vector namespaced helper reordered expression stays statement-only") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/std/collections/vector/push([vector<i32> mut] values, [i32] value) {
  return(value)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32)}
  return(/vector/push(2i32, values))
}
  )";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /vector/push") != std::string::npos);
}

TEST_CASE("stdlib namespaced helper reordered expression rejects compatibility receiver fallback") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/std/collections/vector/push([vector<i32> mut] values, [i32] value) {
  return(value)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32)}
  return(/std/collections/vector/push(2i32, values))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("argument type mismatch") != std::string::npos);
  CHECK(error.find("/std/collections/vector/push") != std::string::npos);
  CHECK(error.find("parameter") != std::string::npos);
}

TEST_CASE("stdlib namespaced helper reordered expression keeps compatibility reject diagnostics") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/std/collections/vector/push([vector<i32> mut] values, [i32] value) {
  return(value)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32)}
  return(/std/collections/vector/push(true, values))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("argument type mismatch") != std::string::npos);
  CHECK(error.find("/std/collections/vector/push") != std::string::npos);
  CHECK(error.find("parameter") != std::string::npos);
}

TEST_CASE("vector namespaced helper reordered statement keeps vector-binding diagnostics") {
  const std::string source = R"(
[effects(heap_alloc), return<void>]
/std/collections/vector/push([vector<i32> mut] values, [i32] value) {
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32)}
  /vector/push(2i32, values)
  return(count(values))
}
  )";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK_FALSE(error.empty());
}

TEST_CASE("stdlib namespaced helper reordered statement rejects compatibility receiver fallback") {
  const std::string source = R"(
[effects(heap_alloc), return<void>]
/std/collections/vector/push([vector<i32> mut] values, [i32] value) {
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32)}
  /std/collections/vector/push(2i32, values)
  return(count(values))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("argument type mismatch") != std::string::npos);
  CHECK(error.find("/std/collections/vector/push") != std::string::npos);
  CHECK(error.find("parameter") != std::string::npos);
}

TEST_CASE("stdlib namespaced helper reordered statement keeps compatibility reject diagnostics") {
  const std::string source = R"(
[effects(heap_alloc), return<void>]
/std/collections/vector/push([vector<i32> mut] values, [i32] value) {
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32)}
  /std/collections/vector/push(true, values)
  return(count(values))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("argument type mismatch") != std::string::npos);
  CHECK(error.find("/std/collections/vector/push") != std::string::npos);
  CHECK(error.find("parameter") != std::string::npos);
}

TEST_CASE("stdlib namespaced vector constructor resolves through imported stdlib helper") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [auto] values{/std/collections/vector/vector<i32>(4i32, 5i32)}
  return(/std/collections/vector/count(values))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("stdlib namespaced vector constructor accepts named arguments through imported stdlib helper") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [auto] values{/std/collections/vector/vector<i32>([first] 4i32, [second] 5i32)}
  return(/std/collections/vector/count(values))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("stdlib namespaced vector constructor supports named-argument temporary receivers") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  return(plus(/std/collections/vector/count(/std/collections/vector/vector<i32>([second] 5i32, [first] 4i32)),
              /std/collections/vector/vector<i32>([second] 8i32, [first] 7i32).count()))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("stdlib namespaced vector constructor rejects explicit builtin vector binding") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{/std/collections/vector/vector<i32>(4i32, 5i32)}
  return(count(values))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("mismatch") != std::string::npos);
}

TEST_CASE("stdlib namespaced vector constructor rejects named-argument explicit builtin vector binding") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{/std/collections/vector/vector<i32>([second] 5i32, [first] 4i32)}
  return(count(values))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("mismatch") != std::string::npos);
}

TEST_CASE("stdlib namespaced vector constructor requires imported stdlib helper") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [auto] values{/std/collections/vector/vector<i32>(4i32, 5i32)}
  return(count(values))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /std/collections/vector/vector") != std::string::npos);
}

TEST_CASE("array namespaced vector constructor alias is rejected") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{/array/vector<i32>(4i32, 5i32)}
  return(count(values))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /array/vector") != std::string::npos);
}

TEST_CASE("array namespaced vector constructor alias named arguments keep unknown-call-target diagnostics") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{/array/vector<i32>([first] 4i32, [second] 5i32)}
  return(count(values))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /array/vector") != std::string::npos);
}

TEST_CASE("stdlib namespaced vector access and count helpers are builtin-alias validated") {
  const std::string source = R"(
import /std/collections/*
[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(4i32, 5i32)}
  [i32] c{/std/collections/vector/count(values)}
  [i32] k{/std/collections/vector/capacity(values)}
  [i32] first{/std/collections/vector/at(values, 0i32)}
  [i32] second{/std/collections/vector/at_unsafe(values, 1i32)}
  return(plus(plus(c, k), plus(first, second)))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("stdlib namespaced vector helpers accept explicit Vector bindings") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_vector/*

[effects(heap_alloc), return<int>]
main() {
  [Vector<i32> mut] values{/std/collections/vector/vector<i32>(4i32, 5i32)}
  /std/collections/vector/push(values, 6i32)
  [i32] c{/std/collections/vector/count(values)}
  [i32] k{/std/collections/vector/capacity(values)}
  [i32] first{/std/collections/vector/at(values, 0i32)}
  [i32] second{/std/collections/vector/at_unsafe(values, 2i32)}
  /std/collections/vector/pop(values)
  return(plus(plus(plus(c, k), plus(first, second)), /std/collections/vector/count(values)))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("stdlib wrapper vector helpers accept explicit Vector bindings") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_vector/*

[effects(heap_alloc), return<int>]
main() {
  [Vector<i32> mut] values{/std/collections/vector/vector<i32>(4i32, 5i32)}
  vectorReserve<i32>(values, 6i32)
  vectorPush<i32>(values, 6i32)
  [i32 mut] total{plus(vectorCount<i32>(values), vectorCapacity<i32>(values))}
  assign(total, plus(total, vectorAt<i32>(values, 0i32)))
  assign(total, plus(total, vectorAtUnsafe<i32>(values, 2i32)))
  vectorPop<i32>(values)
  vectorRemoveAt<i32>(values, 0i32)
  vectorPush<i32>(values, 9i32)
  vectorRemoveSwap<i32>(values, 0i32)
  vectorClear<i32>(values)
  return(total)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("stdlib wrapper vector helpers reject explicit Vector type mismatch") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_vector/*

[effects(heap_alloc), return<int>]
main() {
  [Vector<i32>] values{/std/collections/vector/vector<i32>(4i32, 5i32)}
  return(vectorCount<bool>(values))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("mismatch") != std::string::npos);
}

TEST_CASE("stdlib wrapper vector constructors accept explicit Vector destinations") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_vector/*

[return<T> effects(heap_alloc)]
wrapValues<T>([T] values) {
  return(values)
}

[return<Vector<i32>> effects(heap_alloc)]
buildValues() {
  return(/std/collections/vectorPair<i32>(2i32, 4i32))
}

[return<i32> effects(heap_alloc)]
scoreValues([Vector<i32>] values) {
  return(plus(/std/collections/vector/count<i32>(values), /std/collections/vector/at_unsafe<i32>(values, 1i32)))
}

[effects(heap_alloc), return<int>]
main() {
  [Vector<i32> mut] direct{/std/collections/vectorPair<i32>(4i32, 5i32)}
  [Vector<i32>] wrapped{wrapValues(/std/collections/vectorSingle<i32>(6i32))}
  [Vector<i32> mut] assigned{/std/collections/vectorNew<i32>()}
  assign(assigned, buildValues())
  /std/collections/vector/push<i32>(direct, 7i32)
  [i32 mut] total{scoreValues(/std/collections/vectorPair<i32>(1i32, 3i32))}
  assign(total, plus(total, /std/collections/vector/count<i32>(direct)))
  assign(total, plus(total, /std/collections/vector/at<i32>(direct, 0i32)))
  assign(total, plus(total, /std/collections/vector/at_unsafe<i32>(direct, 2i32)))
  assign(total, plus(total, /std/collections/vector/at<i32>(wrapped, 0i32)))
  assign(total, plus(total, /std/collections/vector/count<i32>(assigned)))
  assign(total, plus(total, /std/collections/vector/at_unsafe<i32>(assigned, 1i32)))
  return(total)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("stdlib wrapper vector constructors keep mismatch diagnostics on explicit Vector destinations") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_vector/*

[return<T> effects(heap_alloc)]
wrapValues<T>([T] values) {
  return(values)
}

[return<Vector<i32>> effects(heap_alloc)]
buildValues() {
  return(/std/collections/vectorPair<i32>(2i32, false))
}

[effects(heap_alloc), return<int>]
main() {
  [Vector<i32>] values{wrapValues(buildValues())}
  return(/std/collections/vector/count<i32>(values))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("mismatch") != std::string::npos);
}

TEST_CASE("stdlib wrapper vector constructors infer experimental auto locals and auto returns") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_vector/*

[return<auto> effects(heap_alloc)]
buildValues([bool] wrapped) {
  if(wrapped,
    then() {
      return(/std/collections/vectorPair(11i32, 13i32))
    },
    else() {
      return(/std/collections/vectorSingle(19i32))
    })
}

[effects(heap_alloc), return<int>]
main() {
  [auto mut] values{/std/collections/vectorNew<i32>()}
  /std/collections/vector/push<i32>(values, 3i32)
  [auto mut] built{buildValues(true)}
  /std/collections/vector/push<i32>(built, 17i32)
  [auto] single{buildValues(false)}
  return(plus(/std/collections/vector/count<i32>(values),
      plus(/std/collections/vector/at<i32>(built, 2i32),
          /std/collections/vector/at_unsafe<i32>(single, 0i32))))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.empty());
}

TEST_CASE("helper-wrapped vector constructors infer experimental auto locals") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_vector/*

[return<T> effects(heap_alloc)]
wrapValues<T>([T] values) {
  return(values)
}

[effects(heap_alloc), return<int>]
main() {
  [auto mut] values{wrapValues(/std/collections/vectorPair(4i32, 7i32))}
  /std/collections/vector/push<i32>(values, 9i32)
  return(plus(/std/collections/vector/count<i32>(values), /std/collections/vector/at_unsafe<i32>(values, 2i32)))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.empty());
}

TEST_CASE("helper-wrapped vector constructor auto inference keeps template conflict diagnostics") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_vector/*

[return<T> effects(heap_alloc)]
wrapValues<T>([T] values) {
  return(values)
}

[effects(heap_alloc), return<int>]
main() {
  [auto] values{wrapValues(/std/collections/vectorPair(2i32, false))}
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.find("implicit template arguments conflict on /std/collections/vectorPair") != std::string::npos);
}

TEST_CASE("implicit vector constructor auto inference keeps template conflict diagnostics") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_vector/*

[effects(heap_alloc), return<int>]
main() {
  [auto] values{/std/collections/vectorPair(2i32, false)}
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.find("implicit template arguments conflict on /std/collections/vectorPair") != std::string::npos);
}

TEST_CASE("canonical vector helpers accept direct constructor receivers") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_vector/*

[effects(heap_alloc), return<int>]
main() {
  [i32 mut] total{plus(/std/collections/vector/count(/std/collections/vectorPair(11i32, 13i32)),
                       /std/collections/vector/at(/std/collections/vectorPair(17i32, 19i32), 1i32))}
  assign(total, plus(total, vectorAt<i32>(/std/collections/vectorPair(23i32, 29i32), 0i32)))
  return(total)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.empty());
}

TEST_CASE("canonical vector helpers keep mismatch diagnostics on direct constructor receivers") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_vector/*

[effects(heap_alloc), return<int>]
main() {
  return(/std/collections/vector/count(/std/collections/vectorPair(2i32, false)))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.find("implicit template arguments conflict on /std/collections/vectorPair") != std::string::npos);
}

TEST_CASE("experimental vector methods accept direct constructor receivers") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_vector/*

[effects(heap_alloc), return<int>]
main() {
  [i32 mut] total{plus(/std/collections/vectorPair(31i32, 37i32).count(),
                       /std/collections/vectorPair(41i32, 43i32).at_unsafe(1i32))}
  assign(total, plus(total, /std/collections/vectorPair(47i32, 53i32).at(0i32)))
  return(total)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.empty());
}

TEST_CASE("experimental vector methods keep mismatch diagnostics on direct constructor receivers") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_vector/*

[effects(heap_alloc), return<int>]
main() {
  return(/std/collections/vectorPair(2i32, false).count())
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.find("implicit template arguments conflict on /std/collections/vectorPair") != std::string::npos);
}

TEST_CASE("helper-wrapped canonical vector helpers accept direct constructor receivers") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_vector/*

[return<T> effects(heap_alloc)]
wrapValues<T>([T] values) {
  return(values)
}

[effects(heap_alloc), return<int>]
main() {
  [i32 mut] total{plus(/std/collections/vector/count(wrapValues(/std/collections/vectorPair(11i32, 13i32))),
                       /std/collections/vector/at(wrapValues(/std/collections/vectorPair(17i32, 19i32)), 1i32))}
  assign(total, plus(total, vectorAt<i32>(wrapValues(/std/collections/vectorPair(23i32, 29i32)), 0i32)))
  return(total)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.empty());
}

TEST_CASE("helper-wrapped canonical vector helpers keep mismatch diagnostics on direct constructor receivers") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_vector/*

[return<T> effects(heap_alloc)]
wrapValues<T>([T] values) {
  return(values)
}

[effects(heap_alloc), return<int>]
main() {
  return(/std/collections/vector/count(wrapValues(/std/collections/vectorPair(2i32, false))))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.find("implicit template arguments conflict on /std/collections/vectorPair") != std::string::npos);
}

TEST_CASE("helper-wrapped experimental vector methods accept direct constructor receivers") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_vector/*

[return<T> effects(heap_alloc)]
wrapValues<T>([T] values) {
  return(values)
}

[effects(heap_alloc), return<int>]
main() {
  [i32 mut] total{plus(wrapValues(/std/collections/vectorPair(31i32, 37i32)).count(),
                       wrapValues(/std/collections/vectorPair(41i32, 43i32)).at_unsafe(1i32))}
  assign(total, plus(total, wrapValues(/std/collections/vectorPair(47i32, 53i32)).at(0i32)))
  return(total)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.empty());
}

TEST_CASE("helper-wrapped experimental vector methods keep mismatch diagnostics on direct constructor receivers") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_vector/*

[return<T> effects(heap_alloc)]
wrapValues<T>([T] values) {
  return(values)
}

[effects(heap_alloc), return<int>]
main() {
  return(wrapValues(/std/collections/vectorPair(2i32, false)).count())
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.find("implicit template arguments conflict on /std/collections/vectorPair") != std::string::npos);
}

TEST_CASE("stdlib namespaced map constructor resolves through imported stdlib helper") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{/std/collections/map/map<i32, i32>(1i32, 4i32, 2i32, 5i32)}
  return(/std/collections/map/at(values, 1i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("stdlib namespaced map constructor requires imported stdlib helper") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{/std/collections/map/map<i32, i32>(1i32, 4i32, 2i32, 5i32)}
  return(count(values))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /std/collections/map/map") != std::string::npos);
}

TEST_CASE("imported stdlib namespaced map helpers accept ordinary named arguments") {
  const std::string source = R"(
import /std/collections/*

[effects(io_err)]
unexpectedMapNamedArgsError([ContainerError] err) {
  [Result<ContainerError>] status{err.code}
  print_line_error(Result.why(status))
}

[return<Result<int, ContainerError>> effects(io_out, heap_alloc) on_error<ContainerError, /unexpectedMapNamedArgsError>]
main() {
  [map<string, i32>] values{/std/collections/map/map<string, i32>(
      [secondKey] "right"raw_utf8, [secondValue] 7i32, [firstKey] "left"raw_utf8, [firstValue] 4i32)}
  [i32] found{try(/std/collections/map/tryAt<string, i32>([key] "left"raw_utf8, [values] values))}
  [i32 mut] total{/std/collections/map/count<string, i32>([values] values)}
  assign(total, plus(total, /std/collections/map/at<string, i32>([key] "right"raw_utf8, [values] values)))
  assign(total, plus(total, /std/collections/map/at_unsafe<string, i32>([key] "left"raw_utf8, [values] values)))
  if(/std/collections/map/contains<string, i32>([key] "right"raw_utf8, [values] values),
     then() { assign(total, plus(total, found)) },
     else() { })
  return(Result.ok(total))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("canonical namespaced map helpers reject borrowed experimental map receivers") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_map/*

[return<Reference<Map<string, i32>>>]
borrowExperimentalMap([Reference<Map<string, i32>>] values) {
  return(values)
}

[effects(heap_alloc), return<i32>]
main() {
  [Map<string, i32>] values{mapPair<string, i32>("left"raw_utf8, 4i32, "right"raw_utf8, 7i32)}
  [Reference<Map<string, i32>>] ref{borrowExperimentalMap(location(values))}
  return(/std/collections/map/count<string, i32>(ref))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /std/collections/map/count") != std::string::npos);
}

TEST_CASE("canonical namespaced map _ref helpers accept borrowed experimental map receivers") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_map/*

[return<Reference<Map<string, i32>>>]
borrowExperimentalMap([Reference<Map<string, i32>>] values) {
  return(values)
}

[effects(io_err)]
unexpectedCanonicalExperimentalMapBorrowedRefError([ContainerError] err) {
  [Result<ContainerError>] status{err.code}
  print_line_error(Result.why(status))
}

[return<Result<int, ContainerError>> effects(io_out, heap_alloc) on_error<ContainerError, /unexpectedCanonicalExperimentalMapBorrowedRefError>]
main() {
  [Map<string, i32>] values{mapPair<string, i32>("left"raw_utf8, 4i32, "right"raw_utf8, 7i32)}
  [Reference<Map<string, i32>>] ref{borrowExperimentalMap(location(values))}
  [i32] found{try(/std/collections/map/tryAt_ref<string, i32>(ref, "left"raw_utf8))}
  [i32 mut] total{plus(/std/collections/map/count_ref<string, i32>(ref), found)}
  assign(total, plus(total, /std/collections/map/at_ref<string, i32>(ref, "left"raw_utf8)))
  assign(total, plus(total, /std/collections/map/at_unsafe_ref<string, i32>(ref, "right"raw_utf8)))
  if(/std/collections/map/contains_ref<string, i32>(ref, "left"raw_utf8),
     then() { assign(total, plus(total, 1i32)) },
     else() { })
  return(Result.ok(total))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("canonical namespaced map helpers keep unknown-call diagnostics for borrowed experimental map receivers") {
  const std::string source = R"(
import /std/collections/*
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
  if(/std/collections/map/contains<Key, i32>(ref, Key(1i32)),
     then() { return(1i32) },
     else() { return(0i32) })
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.find("unknown call target: /std/collections/map/contains") != std::string::npos);
}

TEST_CASE("canonical namespaced map _ref helpers include builtin map key rejects for borrowed experimental map receivers") {
  const std::string source = R"(
import /std/collections/*
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
  if(/std/collections/map/contains_ref<Key, i32>(ref, Key(1i32)),
     then() { return(1i32) },
     else() { return(0i32) })
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("Comparable") != std::string::npos);
  CHECK(error.find("builtin Comparable key type") != std::string::npos);
}

TEST_CASE("imported stdlib namespaced map constructor keeps mismatch diagnostics") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [map<string, i32>] values{/std/collections/map/map<string, i32>("left"raw_utf8, 4i32, "right"raw_utf8, false)}
  return(/std/collections/map/count<string, i32>(values))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("mismatch") != std::string::npos);
}

TEST_CASE("stdlib namespaced map count validates without imported stdlib helper") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 4i32, 2i32, 5i32)}
  return(/std/collections/map/count(values))
}
  )";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("stdlib namespaced map count builtin rejects template args without imported helper") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 4i32, 2i32, 5i32)}
  return(/std/collections/map/count<i32, i32>(values))
}
  )";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("count does not accept template arguments") != std::string::npos);
}

TEST_CASE("stdlib namespaced map count builtin rejects template args through compile pipeline") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 4i32, 2i32, 5i32)}
  return(/std/collections/map/count<i32, i32>(values))
}
  )";
  std::string error;
  CHECK_FALSE(validateProgramForCompileTarget(source, "/main", "vm", "", error));
  CHECK(error.find("count does not accept template arguments") != std::string::npos);
}

TEST_CASE("stdlib namespaced map contains requires imported stdlib helper or explicit definition") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 4i32, 2i32, 5i32)}
  [bool] found{/std/collections/map/contains(values, 1i32)}
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /std/collections/map/contains") != std::string::npos);
}

TEST_CASE("stdlib namespaced map tryAt requires imported stdlib helper or explicit definition") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 4i32, 2i32, 5i32)}
  return(try(/std/collections/map/tryAt(values, 1i32)))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /std/collections/map/tryAt") != std::string::npos);
}

TEST_CASE("stdlib namespaced map access helpers accept imported stdlib wrappers") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 4i32, 2i32, 5i32)}
  [i32] first{/std/collections/map/at(values, 1i32)}
  [i32] second{/std/collections/map/at_unsafe(values, 2i32)}
  return(plus(first, second))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.empty());
}

TEST_CASE("collected diagnostics ignore imported canonical map access helper calls") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 4i32, 2i32, 5i32)}
  [i32] aliasCount{/std/collections/map/count(values)}
  [i32] first{/std/collections/map/at(values, 1i32)}
  return(plus(aliasCount, first))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("collected diagnostics keep unknown target for unsupported canonical map helper calls") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 4i32)}
  return(/std/collections/map/missing(values))
}
)";
  primec::SemanticDiagnosticInfo diagnosticInfo;
  std::string error;
  CHECK_FALSE(validateProgramCollectingDiagnostics(source, "/main", error, diagnosticInfo));
  CHECK(error.find("unknown call target: /std/collections/map/missing") != std::string::npos);
  REQUIRE(diagnosticInfo.records.size() >= 1);
  bool foundUnknownTarget = false;
  for (const auto &record : diagnosticInfo.records) {
    if (record.message.find("unknown call target: /std/collections/map/missing") != std::string::npos) {
      foundUnknownTarget = true;
      break;
    }
  }
  CHECK(foundUnknownTarget);
}

TEST_CASE("stdlib namespaced map helpers accept canonical map references") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [/std/collections/map<i32, i32>] values{map<i32, i32>(1i32, 4i32, 2i32, 5i32)}
  [Reference</std/collections/map<i32, i32>>] ref{location(values)}
  [i32] c{/std/collections/map/count(ref)}
  [i32] first{/std/collections/map/at(ref, 1i32)}
  [i32] second{/std/collections/map/at_unsafe(ref, 2i32)}
  return(plus(c, plus(first, second)))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("stdlib namespaced map helpers accept experimental map value receivers") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_map/*

[effects(io_err)]
unexpectedCanonicalExperimentalMapValueError([ContainerError] err) {
  [Result<ContainerError>] status{err.code}
  print_line_error(Result.why(status))
}

[return<Result<int, ContainerError>> effects(io_out, heap_alloc) on_error<ContainerError, /unexpectedCanonicalExperimentalMapValueError>]
main() {
  [Map<string, i32>] values{mapPair<string, i32>("left"raw_utf8, 4i32, "right"raw_utf8, 7i32)}
  [i32] found{try(/std/collections/map/tryAt(values, "left"raw_utf8))}
  [i32 mut] total{plus(/std/collections/map/count(values), found)}
  assign(total, plus(total, /std/collections/map/at(values, "left"raw_utf8)))
  assign(total, plus(total, /std/collections/map/at_unsafe(values, "right"raw_utf8)))
  if(/std/collections/map/contains(values, "left"raw_utf8),
     then() { assign(total, plus(total, 1i32)) },
     else() { })
  return(Result.ok(total))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("stdlib wrapper map helpers accept experimental map value receivers") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_map/*

[effects(io_err)]
unexpectedWrapperExperimentalMapValueError([ContainerError] err) {
  [Result<ContainerError>] status{err.code}
  print_line_error(Result.why(status))
}

[return<Result<int, ContainerError>> effects(io_out, heap_alloc) on_error<ContainerError, /unexpectedWrapperExperimentalMapValueError>]
main() {
  [Map<string, i32>] values{mapPair<string, i32>("left"raw_utf8, 4i32, "right"raw_utf8, 7i32)}
  [i32] found{try(/std/collections/mapTryAt(values, "left"raw_utf8))}
  [i32 mut] total{plus(/std/collections/mapCount(values), found)}
  assign(total, plus(total, /std/collections/mapAt(values, "left"raw_utf8)))
  assign(total, plus(total, /std/collections/mapAtUnsafe(values, "right"raw_utf8)))
  if(/std/collections/mapContains(values, "left"raw_utf8),
     then() { assign(total, plus(total, 1i32)) },
     else() { })
  return(Result.ok(total))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("stdlib wrapper map helpers accept experimental constructor receivers") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_map/*

[effects(io_err)]
unexpectedWrapperExperimentalMapReceiverError([ContainerError] err) {
  [Result<ContainerError>] status{err.code}
  print_line_error(Result.why(status))
}

[return<Result<int, ContainerError>> effects(io_out, heap_alloc) on_error<ContainerError, /unexpectedWrapperExperimentalMapReceiverError>]
main() {
  [i32] found{try(/std/collections/mapTryAt(/std/collections/map/map("left"raw_utf8, 4i32, "right"raw_utf8, 7i32), "left"raw_utf8))}
  [i32 mut] total{plus(/std/collections/mapCount(/std/collections/mapPair("left"raw_utf8, 4i32, "right"raw_utf8, 7i32)), found)}
  assign(total, plus(total, /std/collections/mapAt(/std/collections/mapPair("extra"raw_utf8, 9i32, "other"raw_utf8, 2i32), "extra"raw_utf8)))
  assign(total, plus(total, /std/collections/mapAtUnsafe(/std/collections/map/map("bonus"raw_utf8, 5i32, "keep"raw_utf8, 1i32), "bonus"raw_utf8)))
  if(/std/collections/mapContains(/std/collections/mapPair("left"raw_utf8, 4i32, "right"raw_utf8, 7i32), "right"raw_utf8),
     then() { assign(total, plus(total, 1i32)) },
     else() { })
  return(Result.ok(total))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("stdlib wrapper map helpers keep constructor mismatch diagnostics on experimental constructor receivers") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_map/*

[effects(heap_alloc), return<int>]
main() {
  return(/std/collections/mapCount(/std/collections/mapPair("left"raw_utf8, 4i32, "wrong"raw_utf8, false)))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.find("implicit template arguments conflict on /std/collections/mapPair") != std::string::npos);
}

TEST_CASE("stdlib namespaced map constructor accepts explicit experimental map bindings") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_map/*

[effects(io_err)]
unexpectedCanonicalExperimentalMapConstructorError([ContainerError] err) {
  [Result<ContainerError>] status{err.code}
  print_line_error(Result.why(status))
}

[return<Result<int, ContainerError>> effects(io_out, heap_alloc) on_error<ContainerError, /unexpectedCanonicalExperimentalMapConstructorError>]
main() {
  [Map<string, i32>] values{/std/collections/map/map<string, i32>("left"raw_utf8, 4i32, "right"raw_utf8, 7i32)}
  [i32] found{try(/std/collections/map/tryAt(values, "left"raw_utf8))}
  [i32 mut] total{plus(/std/collections/map/count(values), found)}
  assign(total, plus(total, /std/collections/map/at(values, "left"raw_utf8)))
  assign(total, plus(total, /std/collections/map/at_unsafe(values, "right"raw_utf8)))
  if(/std/collections/map/contains(values, "left"raw_utf8),
     then() { assign(total, plus(total, 1i32)) },
     else() { })
  return(Result.ok(total))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.empty());
}

TEST_CASE("stdlib namespaced map constructor keeps mismatch diagnostics on explicit experimental map bindings") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_map/*

[effects(heap_alloc), return<int>]
main() {
  [Map<string, i32>] values{/std/collections/map/map<string, i32>("left"raw_utf8, 4i32, "right"raw_utf8, false)}
  return(/std/collections/map/count(values))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("mismatch") != std::string::npos);
}

TEST_CASE("stdlib namespaced map constructor accepts explicit experimental map returns") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_map/*

[effects(io_err)]
unexpectedCanonicalExperimentalMapReturnError([ContainerError] err) {
  [Result<ContainerError>] status{err.code}
  print_line_error(Result.why(status))
}

[return<Map<string, i32>> effects(heap_alloc)]
buildValues() {
  return(/std/collections/map/map<string, i32>("left"raw_utf8, 4i32, "right"raw_utf8, 7i32))
}

[return<Result<int, ContainerError>> effects(io_out, heap_alloc) on_error<ContainerError, /unexpectedCanonicalExperimentalMapReturnError>]
main() {
  [Map<string, i32>] values{buildValues()}
  [i32] found{try(/std/collections/map/tryAt(values, "left"raw_utf8))}
  [i32 mut] total{plus(/std/collections/map/count(values), found)}
  assign(total, plus(total, /std/collections/map/at(values, "left"raw_utf8)))
  assign(total, plus(total, /std/collections/map/at_unsafe(values, "right"raw_utf8)))
  if(/std/collections/map/contains(values, "left"raw_utf8),
     then() { assign(total, plus(total, 1i32)) },
     else() { })
  return(Result.ok(total))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.empty());
}

TEST_CASE("stdlib namespaced map constructor keeps mismatch diagnostics on explicit experimental map returns") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_map/*

[return<Map<string, i32>> effects(heap_alloc)]
buildValues() {
  return(/std/collections/map/map<string, i32>("left"raw_utf8, 4i32, "right"raw_utf8, false))
}

[effects(heap_alloc), return<int>]
main() {
  [Map<string, i32>] values{buildValues()}
  return(/std/collections/map/count(values))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("mismatch") != std::string::npos);
}

TEST_CASE("stdlib namespaced map constructor accepts explicit experimental map parameters") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_map/*

[effects(io_err)]
unexpectedCanonicalExperimentalMapParameterError([ContainerError] err) {
  [Result<ContainerError>] status{err.code}
  print_line_error(Result.why(status))
}

[return<Result<int, ContainerError>> effects(io_out, heap_alloc) on_error<ContainerError, /unexpectedCanonicalExperimentalMapParameterError>]
scoreValues([Map<string, i32>] values) {
  [i32] found{try(/std/collections/map/tryAt(values, "left"raw_utf8))}
  [i32 mut] total{plus(/std/collections/map/count(values), found)}
  assign(total, plus(total, /std/collections/map/at(values, "left"raw_utf8)))
  assign(total, plus(total, /std/collections/map/at_unsafe(values, "right"raw_utf8)))
  if(/std/collections/map/contains(values, "left"raw_utf8),
     then() { assign(total, plus(total, 1i32)) },
     else() { })
  return(Result.ok(total))
}

[return<Result<int, ContainerError>> effects(io_out, heap_alloc) on_error<ContainerError, /unexpectedCanonicalExperimentalMapParameterError>]
main() {
  return(scoreValues(/std/collections/map/map<string, i32>("left"raw_utf8, 4i32, "right"raw_utf8, 7i32)))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("stdlib namespaced map constructor keeps mismatch diagnostics on explicit experimental map parameters") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_map/*

[effects(heap_alloc), return<int>]
scoreValues([Map<string, i32>] values) {
  return(/std/collections/map/count(values))
}

[effects(heap_alloc), return<int>]
main() {
  return(scoreValues(/std/collections/map/map<string, i32>("left"raw_utf8, 4i32, "right"raw_utf8, false)))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("mismatch") != std::string::npos);
}

#if 0  // TODO: re-enable after experimental map wrapper routing is validated end-to-end.
TEST_CASE("helper-wrapped map constructors accept explicit experimental map parameters") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_map/*

Holder() {}

[return<T> effects(heap_alloc)]
wrapValues<T>([T] values) {
  return(values)
}

[effects(io_err)]
unexpectedWrappedExperimentalMapParameterError([ContainerError] err) {
  [Result<ContainerError>] status{err.code}
  print_line_error(Result.why(status))
}

[return<Result<int, ContainerError>> effects(io_out, heap_alloc) on_error<ContainerError, /unexpectedWrappedExperimentalMapParameterError>]
scoreValues([Map<string, i32>] values) {
  [i32] found{try(/std/collections/map/tryAt(values, "left"raw_utf8))}
  [i32 mut] total{plus(/std/collections/map/count(values), found)}
  assign(total, plus(total, /std/collections/map/at(values, "left"raw_utf8)))
  return(Result.ok(total))
}

[return<Result<int, ContainerError>> effects(io_out, heap_alloc) on_error<ContainerError, /unexpectedWrappedExperimentalMapParameterError>]
/Holder/score([Holder] self, [Map<string, i32>] values) {
  [i32] bonus{try(/std/collections/map/tryAt(values, "bonus"raw_utf8))}
  return(Result.ok(plus(/std/collections/map/count(values), bonus)))
}

[return<Result<int, ContainerError>> effects(io_out, heap_alloc) on_error<ContainerError, /unexpectedWrappedExperimentalMapParameterError>]
main() {
  [Holder] holder{Holder()}
  return(Result.ok(plus(try(scoreValues(wrapValues(/std/collections/map/map("left"raw_utf8, 4i32, "right"raw_utf8, 7i32)))),
                        try(holder.score(wrapValues(/std/collections/mapPair("bonus"raw_utf8, 5i32, "other"raw_utf8, 2i32)))))))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.empty());
}

TEST_CASE("helper-wrapped map constructors keep mismatch diagnostics on explicit experimental map parameters") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_map/*

[return<T> effects(heap_alloc)]
wrapValues<T>([T] values) {
  return(values)
}

[effects(heap_alloc), return<int>]
scoreValues([Map<string, i32>] values) {
  return(/std/collections/map/count(values))
}

[effects(heap_alloc), return<int>]
main() {
  return(scoreValues(wrapValues(/std/collections/mapPair("left"raw_utf8, 4i32, "wrong"raw_utf8, false))))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.find("implicit template arguments conflict on /std/collections/mapPair") != std::string::npos);
}

TEST_CASE("helper-wrapped Result.ok payloads accept explicit experimental map parameters") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_map/*

[return<T> effects(heap_alloc)]
wrapStatus<T>([T] status) {
  return(status)
}

[effects(io_err)]
unexpectedWrappedExperimentalResultMapParameterError([ContainerError] err) {
  [Result<ContainerError>] status{err.code}
  print_line_error(Result.why(status))
}

[return<Result<int, ContainerError>> effects(io_out, heap_alloc)
 on_error<ContainerError, /unexpectedWrappedExperimentalResultMapParameterError>]
scoreStatus([Result<Map<string, i32>, ContainerError>] status) {
  [Map<string, i32>] values{try(status)}
  return(Result.ok(plus(/std/collections/map/count(values),
                        /std/collections/map/at(values, "left"raw_utf8))))
}

[return<Result<int, ContainerError>> effects(io_out, heap_alloc)
 on_error<ContainerError, /unexpectedWrappedExperimentalResultMapParameterError>]
main() {
  return(scoreStatus(wrapStatus(Result.ok(/std/collections/mapPair("left"raw_utf8, 4i32,
                                                                  "right"raw_utf8, 7i32)))))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.empty());
}

TEST_CASE("helper-wrapped Result.ok payloads keep mismatch diagnostics on explicit experimental map parameters") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_map/*

[return<T> effects(heap_alloc)]
wrapStatus<T>([T] status) {
  return(status)
}

[effects(io_err)]
unexpectedWrappedExperimentalResultMapParameterError([ContainerError] err) {
  [Result<ContainerError>] status{err.code}
  print_line_error(Result.why(status))
}

[return<Result<int, ContainerError>> effects(io_out, heap_alloc)
 on_error<ContainerError, /unexpectedWrappedExperimentalResultMapParameterError>]
scoreStatus([Result<Map<string, i32>, ContainerError>] status) {
  [Map<string, i32>] values{try(status)}
  return(Result.ok(/std/collections/map/count(values)))
}

[return<Result<int, ContainerError>> effects(io_out, heap_alloc)
 on_error<ContainerError, /unexpectedWrappedExperimentalResultMapParameterError>]
main() {
  return(scoreStatus(wrapStatus(Result.ok(/std/collections/mapPair("left"raw_utf8, 4i32,
                                                                  "wrong"raw_utf8, false)))))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.find("implicit template arguments conflict on /std/collections/mapPair") != std::string::npos);
}

TEST_CASE("stdlib wrapper mapPair constructor accepts explicit experimental map bindings") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_map/*

[effects(io_err)]
unexpectedWrapperExperimentalMapConstructorError([ContainerError] err) {
  [Result<ContainerError>] status{err.code}
  print_line_error(Result.why(status))
}

[return<Result<int, ContainerError>> effects(io_out, heap_alloc) on_error<ContainerError, /unexpectedWrapperExperimentalMapConstructorError>]
main() {
  [Map<string, i32>] values{/std/collections/mapPair<string, i32>("left"raw_utf8, 4i32, "right"raw_utf8, 7i32)}
  [i32] found{try(/std/collections/map/tryAt(values, "left"raw_utf8))}
  [i32 mut] total{plus(/std/collections/map/count(values), found)}
  assign(total, plus(total, /std/collections/map/at(values, "left"raw_utf8)))
  assign(total, plus(total, /std/collections/map/at_unsafe(values, "right"raw_utf8)))
  if(/std/collections/map/contains(values, "left"raw_utf8),
     then() { assign(total, plus(total, 1i32)) },
     else() { })
  return(Result.ok(total))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("stdlib wrapper mapPair constructor keeps mismatch diagnostics on explicit experimental map bindings") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_map/*

[effects(heap_alloc), return<int>]
main() {
  [Map<string, i32>] values{/std/collections/mapPair<string, i32>("left"raw_utf8, 4i32, "right"raw_utf8, false)}
  return(/std/collections/map/count(values))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("mismatch") != std::string::npos);
}

TEST_CASE("helper-wrapped map constructors accept explicit experimental map bindings") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_map/*

[return<T> effects(heap_alloc)]
wrapValues<T>([T] values) {
  return(values)
}

[effects(io_err)]
unexpectedWrappedExperimentalMapBindingError([ContainerError] err) {
  [Result<ContainerError>] status{err.code}
  print_line_error(Result.why(status))
}

[return<Result<int, ContainerError>> effects(io_out, heap_alloc)
 on_error<ContainerError, /unexpectedWrappedExperimentalMapBindingError>]
main() {
  [Map<string, i32>] values{wrapValues(/std/collections/map/map("left"raw_utf8, 4i32, "right"raw_utf8, 7i32))}
  [i32] found{try(/std/collections/map/tryAt(values, "left"raw_utf8))}
  [i32 mut] total{plus(/std/collections/map/count(values), found)}
  assign(total, plus(total, /std/collections/map/at(values, "right"raw_utf8)))
  return(Result.ok(total))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.empty());
}

TEST_CASE("helper-wrapped map constructors keep mismatch diagnostics on explicit experimental map bindings") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_map/*

[return<T> effects(heap_alloc)]
wrapValues<T>([T] values) {
  return(values)
}

[effects(heap_alloc), return<int>]
main() {
  [Map<string, i32>] values{wrapValues(/std/collections/mapPair("left"raw_utf8, 4i32, "wrong"raw_utf8, false))}
  return(/std/collections/map/count(values))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.find("implicit template arguments conflict on /std/collections/mapPair") != std::string::npos);
}

TEST_CASE("helper-wrapped Result.ok payloads accept explicit experimental map bindings") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_map/*

[return<T> effects(heap_alloc)]
wrapStatus<T>([T] status) {
  return(status)
}

[effects(io_err)]
unexpectedWrappedExperimentalResultMapBindingError([ContainerError] err) {
  [Result<ContainerError>] status{err.code}
  print_line_error(Result.why(status))
}

[return<Result<int, ContainerError>> effects(io_out, heap_alloc)
 on_error<ContainerError, /unexpectedWrappedExperimentalResultMapBindingError>]
main() {
  [Result<Map<string, i32>, ContainerError>] status{
      wrapStatus(Result.ok(/std/collections/map/map("left"raw_utf8, 4i32, "right"raw_utf8, 7i32)))}
  [Map<string, i32>] values{try(status)}
  return(Result.ok(plus(/std/collections/map/count(values),
                        /std/collections/map/at(values, "right"raw_utf8))))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.empty());
}

TEST_CASE("helper-wrapped Result.ok payloads keep mismatch diagnostics on explicit experimental map bindings") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_map/*

[return<T> effects(heap_alloc)]
wrapStatus<T>([T] status) {
  return(status)
}

[effects(io_err)]
unexpectedWrappedExperimentalResultMapBindingError([ContainerError] err) {
  [Result<ContainerError>] status{err.code}
  print_line_error(Result.why(status))
}

[return<Result<int, ContainerError>> effects(io_out, heap_alloc)
 on_error<ContainerError, /unexpectedWrappedExperimentalResultMapBindingError>]
main() {
  [Result<Map<string, i32>, ContainerError>] status{
      wrapStatus(Result.ok(/std/collections/mapPair("left"raw_utf8, 4i32, "wrong"raw_utf8, false)))}
  [Map<string, i32>] values{try(status)}
  return(Result.ok(/std/collections/map/count(values)))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.find("implicit template arguments conflict on /std/collections/mapPair") != std::string::npos);
}

TEST_CASE("helper-wrapped Result.ok payload assignments accept explicit experimental map result targets") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_map/*

[return<T> effects(heap_alloc)]
wrapStatus<T>([T] status) {
  return(status)
}

[effects(io_err)]
unexpectedWrappedExperimentalResultMapAssignError([ContainerError] err) {
  [Result<ContainerError>] status{err.code}
  print_line_error(Result.why(status))
}

[return<Result<int, ContainerError>> effects(io_out, heap_alloc)
 on_error<ContainerError, /unexpectedWrappedExperimentalResultMapAssignError>]
main() {
  [Result<Map<string, i32>, ContainerError> mut] status{Result.ok(mapNew<string, i32>())}
  assign(status, wrapStatus(Result.ok(/std/collections/mapPair("left"raw_utf8, 4i32,
                                                              "right"raw_utf8, 7i32))))
  [Map<string, i32>] values{try(status)}
  return(Result.ok(plus(/std/collections/map/count(values),
                        /std/collections/map/at(values, "right"raw_utf8))))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.empty());
}

TEST_CASE("helper-wrapped Result.ok payload assignments keep mismatch diagnostics on explicit experimental map result targets") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_map/*

[return<T> effects(heap_alloc)]
wrapStatus<T>([T] status) {
  return(status)
}

[effects(io_err)]
unexpectedWrappedExperimentalResultMapAssignError([ContainerError] err) {
  [Result<ContainerError>] status{err.code}
  print_line_error(Result.why(status))
}

[return<Result<int, ContainerError>> effects(io_out, heap_alloc)
 on_error<ContainerError, /unexpectedWrappedExperimentalResultMapAssignError>]
main() {
  [Result<Map<string, i32>, ContainerError> mut] status{Result.ok(mapNew<string, i32>())}
  assign(status, wrapStatus(Result.ok(/std/collections/mapPair("left"raw_utf8, 4i32,
                                                              "wrong"raw_utf8, false))))
  return(Result.ok(0i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.find("implicit template arguments conflict on /std/collections/mapPair") != std::string::npos);
}

TEST_CASE("helper-wrapped map constructors accept experimental map dereference assignment targets") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_map/*

[return<T> effects(heap_alloc)]
wrapValues<T>([T] values) {
  return(values)
}

[return<Reference<Map<string, i32>>>]
borrowValues([Reference<Map<string, i32>>] values) {
  return(values)
}

[effects(heap_alloc), return<int>]
main() {
  [Map<string, i32> mut] values{mapNew<string, i32>()}
  assign(dereference(borrowValues(location(values))),
         wrapValues(/std/collections/mapPair("left"raw_utf8, 4i32,
                                             "right"raw_utf8, 7i32)))
  return(plus(/std/collections/map/count(values),
              /std/collections/map/at(values, "right"raw_utf8)))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.empty());
}

TEST_CASE("helper-wrapped map dereference assignments keep mismatch diagnostics") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_map/*

[return<T> effects(heap_alloc)]
wrapValues<T>([T] values) {
  return(values)
}

[return<Reference<Map<string, i32>>>]
borrowValues([Reference<Map<string, i32>>] values) {
  return(values)
}

[effects(heap_alloc), return<int>]
main() {
  [Map<string, i32> mut] values{mapNew<string, i32>()}
  assign(dereference(borrowValues(location(values))),
         wrapValues(/std/collections/mapPair("left"raw_utf8, 4i32,
                                             "wrong"raw_utf8, false)))
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.find("implicit template arguments conflict on /std/collections/mapPair") != std::string::npos);
}

TEST_CASE("helper-wrapped Result.ok payloads accept experimental map result dereference targets") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_map/*

[return<T> effects(heap_alloc)]
wrapStatus<T>([T] status) {
  return(status)
}

[return<Reference<Result<Map<string, i32>, ContainerError>>>]
borrowStatus([Reference<Result<Map<string, i32>, ContainerError>>] status) {
  return(status)
}

[effects(io_err)]
unexpectedWrappedExperimentalResultMapDerefAssignError([ContainerError] err) {
  [Result<ContainerError>] current{err.code}
  print_line_error(Result.why(current))
}

[return<Result<int, ContainerError>> effects(io_out, heap_alloc)
 on_error<ContainerError, /unexpectedWrappedExperimentalResultMapDerefAssignError>]
main() {
  [Result<Map<string, i32>, ContainerError> mut] status{Result.ok(mapNew<string, i32>())}
  assign(dereference(borrowStatus(location(status))),
         wrapStatus(Result.ok(/std/collections/mapPair("left"raw_utf8, 4i32,
                                                      "right"raw_utf8, 7i32))))
  [Map<string, i32>] values{try(status)}
  return(Result.ok(plus(/std/collections/map/count(values),
                        /std/collections/map/at(values, "left"raw_utf8))))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.empty());
}

TEST_CASE("helper-wrapped Result.ok dereference assignments keep mismatch diagnostics") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_map/*

[return<T> effects(heap_alloc)]
wrapStatus<T>([T] status) {
  return(status)
}

[return<Reference<Result<Map<string, i32>, ContainerError>>>]
borrowStatus([Reference<Result<Map<string, i32>, ContainerError>>] status) {
  return(status)
}

[effects(io_err)]
unexpectedWrappedExperimentalResultMapDerefAssignError([ContainerError] err) {
  [Result<ContainerError>] current{err.code}
  print_line_error(Result.why(current))
}

[return<Result<int, ContainerError>> effects(io_out, heap_alloc)
 on_error<ContainerError, /unexpectedWrappedExperimentalResultMapDerefAssignError>]
main() {
  [Result<Map<string, i32>, ContainerError> mut] status{Result.ok(mapNew<string, i32>())}
  assign(dereference(borrowStatus(location(status))),
         wrapStatus(Result.ok(/std/collections/mapPair("left"raw_utf8, 4i32,
                                                      "wrong"raw_utf8, false))))
  return(Result.ok(0i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.find("implicit template arguments conflict on /std/collections/mapPair") != std::string::npos);
}

TEST_CASE("helper-wrapped map constructors accept experimental map uninitialized storage") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_map/*

[return<T> effects(heap_alloc)]
wrapValues<T>([T] values) {
  return(values)
}

[effects(heap_alloc), return<int>]
main() {
  [uninitialized<Map<string, i32>> mut] storage{uninitialized<Map<string, i32>>()}
  init(storage, wrapValues(/std/collections/mapPair("left"raw_utf8, 4i32,
                                                    "right"raw_utf8, 7i32)))
  [Map<string, i32>] values{take(storage)}
  return(plus(/std/collections/map/count(values),
              /std/collections/map/at(values, "right"raw_utf8)))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.empty());
}

TEST_CASE("helper-wrapped map constructor uninitialized storage keeps mismatch diagnostics") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_map/*

[return<T> effects(heap_alloc)]
wrapValues<T>([T] values) {
  return(values)
}

[effects(heap_alloc), return<int>]
main() {
  [uninitialized<Map<string, i32>> mut] storage{uninitialized<Map<string, i32>>()}
  init(storage, wrapValues(/std/collections/mapPair("left"raw_utf8, 4i32,
                                                    "wrong"raw_utf8, false)))
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.find("implicit template arguments conflict on /std/collections/mapPair") != std::string::npos);
}

TEST_CASE("helper-wrapped Result.ok payloads accept experimental map result uninitialized storage") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_map/*

[return<T> effects(heap_alloc)]
wrapStatus<T>([T] status) {
  return(status)
}

[effects(heap_alloc), return<int>]
main() {
  [uninitialized<Result<Map<string, i32>, ContainerError>> mut] storage{
      uninitialized<Result<Map<string, i32>, ContainerError>>()}
  init(storage, wrapStatus(Result.ok(/std/collections/mapPair("left"raw_utf8, 4i32,
                                                             "right"raw_utf8, 7i32))))
  [Result<Map<string, i32>, ContainerError>] status{take(storage)}
  return(0i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.empty());
}

TEST_CASE("helper-wrapped Result.ok uninitialized storage keeps mismatch diagnostics") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_map/*

[return<T> effects(heap_alloc)]
wrapStatus<T>([T] status) {
  return(status)
}

[effects(heap_alloc), return<int>]
main() {
  [uninitialized<Result<Map<string, i32>, ContainerError>> mut] storage{
      uninitialized<Result<Map<string, i32>, ContainerError>>()}
  init(storage, wrapStatus(Result.ok(/std/collections/mapPair("left"raw_utf8, 4i32,
                                                             "wrong"raw_utf8, false))))
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.find("implicit template arguments conflict on /std/collections/mapPair") != std::string::npos);
}

TEST_CASE("helper-wrapped map constructors accept dereferenced experimental map uninitialized storage") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_map/*

[return<T> effects(heap_alloc)]
wrapValues<T>([T] values) {
  return(values)
}

[return<Reference<uninitialized<Map<string, i32>>>>]
borrowStorage([Reference<uninitialized<Map<string, i32>>>] storage) {
  return(storage)
}

[effects(heap_alloc), return<int>]
main() {
  [uninitialized<Map<string, i32>> mut] storage{uninitialized<Map<string, i32>>()}
  init(dereference(borrowStorage(location(storage))),
       wrapValues(/std/collections/mapPair("left"raw_utf8, 4i32,
                                           "right"raw_utf8, 7i32)))
  [Map<string, i32>] values{take(storage)}
  return(plus(/std/collections/map/count(values),
              /std/collections/map/at(values, "right"raw_utf8)))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.empty());
}

TEST_CASE("helper-wrapped dereferenced map storage keeps mismatch diagnostics") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_map/*

[return<T> effects(heap_alloc)]
wrapValues<T>([T] values) {
  return(values)
}

[return<Reference<uninitialized<Map<string, i32>>>>]
borrowStorage([Reference<uninitialized<Map<string, i32>>>] storage) {
  return(storage)
}

[effects(heap_alloc), return<int>]
main() {
  [uninitialized<Map<string, i32>> mut] storage{uninitialized<Map<string, i32>>()}
  init(dereference(borrowStorage(location(storage))),
       wrapValues(/std/collections/mapPair("left"raw_utf8, 4i32,
                                           "wrong"raw_utf8, false)))
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.find("implicit template arguments conflict on /std/collections/mapPair") != std::string::npos);
}

TEST_CASE("helper-wrapped Result.ok payloads accept dereferenced experimental result storage") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_map/*

[return<T> effects(heap_alloc)]
wrapStatus<T>([T] status) {
  return(status)
}

[return<Reference<uninitialized<Result<Map<string, i32>, ContainerError>>>>]
borrowStorage([Reference<uninitialized<Result<Map<string, i32>, ContainerError>>>] storage) {
  return(storage)
}

[effects(heap_alloc), return<int>]
main() {
  [uninitialized<Result<Map<string, i32>, ContainerError>> mut] storage{
      uninitialized<Result<Map<string, i32>, ContainerError>>()}
  init(dereference(borrowStorage(location(storage))),
       wrapStatus(Result.ok(/std/collections/mapPair("left"raw_utf8, 4i32,
                                                    "right"raw_utf8, 7i32))))
  [Result<Map<string, i32>, ContainerError>] status{take(storage)}
  return(0i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.empty());
}

TEST_CASE("helper-wrapped dereferenced Result.ok storage keeps mismatch diagnostics") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_map/*

[return<T> effects(heap_alloc)]
wrapStatus<T>([T] status) {
  return(status)
}

[return<Reference<uninitialized<Result<Map<string, i32>, ContainerError>>>>]
borrowStorage([Reference<uninitialized<Result<Map<string, i32>, ContainerError>>>] storage) {
  return(storage)
}

[effects(heap_alloc), return<int>]
main() {
  [uninitialized<Result<Map<string, i32>, ContainerError>> mut] storage{
      uninitialized<Result<Map<string, i32>, ContainerError>>()}
  init(dereference(borrowStorage(location(storage))),
       wrapStatus(Result.ok(/std/collections/mapPair("left"raw_utf8, 4i32,
                                                    "wrong"raw_utf8, false))))
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.find("implicit template arguments conflict on /std/collections/mapPair") != std::string::npos);
}

TEST_CASE("helper-wrapped map constructors accept experimental map struct storage fields") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_map/*

[return<T> effects(heap_alloc)]
wrapValues<T>([T] values) {
  return(values)
}

Holder() {
  [uninitialized<Map<string, i32>> mut] storage{uninitialized<Map<string, i32>>()}
}

[effects(heap_alloc), return<int>]
main() {
  [Holder mut] holder{Holder()}
  init(holder.storage, wrapValues(/std/collections/mapPair("left"raw_utf8, 4i32,
                                                           "right"raw_utf8, 7i32)))
  [Map<string, i32>] values{take(holder.storage)}
  return(plus(/std/collections/map/count(values),
              /std/collections/map/at(values, "right"raw_utf8)))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.empty());
}

TEST_CASE("helper-wrapped map struct storage fields keep mismatch diagnostics") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_map/*

[return<T> effects(heap_alloc)]
wrapValues<T>([T] values) {
  return(values)
}

Holder() {
  [uninitialized<Map<string, i32>> mut] storage{uninitialized<Map<string, i32>>()}
}

[effects(heap_alloc), return<int>]
main() {
  [Holder mut] holder{Holder()}
  init(holder.storage, wrapValues(/std/collections/mapPair("left"raw_utf8, 4i32,
                                                           "wrong"raw_utf8, false)))
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.find("implicit template arguments conflict on /std/collections/mapPair") != std::string::npos);
}

TEST_CASE("helper-wrapped Result.ok payloads accept experimental result struct storage fields") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_map/*

[return<T> effects(heap_alloc)]
wrapStatus<T>([T] status) {
  return(status)
}

Holder() {
  [uninitialized<Result<Map<string, i32>, ContainerError>> mut] status{
      uninitialized<Result<Map<string, i32>, ContainerError>>()}
}

[effects(heap_alloc), return<int>]
main() {
  [Holder mut] holder{Holder()}
  init(holder.status, wrapStatus(Result.ok(/std/collections/mapPair("left"raw_utf8, 4i32,
                                                                    "right"raw_utf8, 7i32))))
  [Result<Map<string, i32>, ContainerError>] status{take(holder.status)}
  return(0i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.empty());
}

TEST_CASE("helper-wrapped Result.ok struct storage fields keep mismatch diagnostics") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_map/*

[return<T> effects(heap_alloc)]
wrapStatus<T>([T] status) {
  return(status)
}

Holder() {
  [uninitialized<Result<Map<string, i32>, ContainerError>> mut] status{
      uninitialized<Result<Map<string, i32>, ContainerError>>()}
}

[effects(heap_alloc), return<int>]
main() {
  [Holder mut] holder{Holder()}
  init(holder.status, wrapStatus(Result.ok(/std/collections/mapPair("left"raw_utf8, 4i32,
                                                                    "wrong"raw_utf8, false))))
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.find("implicit template arguments conflict on /std/collections/mapPair") != std::string::npos);
}

TEST_CASE("helper-wrapped map constructors accept dereferenced experimental map struct storage fields") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_map/*

[return<T> effects(heap_alloc)]
wrapValues<T>([T] values) {
  return(values)
}

Holder() {
  [uninitialized<Map<string, i32>> mut] storage{uninitialized<Map<string, i32>>()}
}

[return<Reference<Holder>>]
borrowHolder([Reference<Holder>] holder) {
  return(holder)
}

[effects(heap_alloc), return<int>]
main() {
  [Holder mut] holder{Holder()}
  init(dereference(borrowHolder(location(holder))).storage,
       wrapValues(/std/collections/mapPair("left"raw_utf8, 4i32,
                                           "right"raw_utf8, 7i32)))
  [Map<string, i32>] values{take(holder.storage)}
  return(plus(/std/collections/map/count(values),
              /std/collections/map/at(values, "right"raw_utf8)))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.empty());
}

TEST_CASE("helper-wrapped dereferenced map struct storage fields keep mismatch diagnostics") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_map/*

[return<T> effects(heap_alloc)]
wrapValues<T>([T] values) {
  return(values)
}

Holder() {
  [uninitialized<Map<string, i32>> mut] storage{uninitialized<Map<string, i32>>()}
}

[return<Reference<Holder>>]
borrowHolder([Reference<Holder>] holder) {
  return(holder)
}

[effects(heap_alloc), return<int>]
main() {
  [Holder mut] holder{Holder()}
  init(dereference(borrowHolder(location(holder))).storage,
       wrapValues(/std/collections/mapPair("left"raw_utf8, 4i32,
                                           "wrong"raw_utf8, false)))
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.find("implicit template arguments conflict on /std/collections/mapPair") != std::string::npos);
}

TEST_CASE("helper-wrapped Result.ok payloads accept dereferenced result struct storage fields") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_map/*

[return<T> effects(heap_alloc)]
wrapStatus<T>([T] status) {
  return(status)
}

Holder() {
  [uninitialized<Result<Map<string, i32>, ContainerError>> mut] status{
      uninitialized<Result<Map<string, i32>, ContainerError>>()}
}

[return<Reference<Holder>>]
borrowHolder([Reference<Holder>] holder) {
  return(holder)
}

[effects(heap_alloc), return<int>]
main() {
  [Holder mut] holder{Holder()}
  init(dereference(borrowHolder(location(holder))).status,
       wrapStatus(Result.ok(/std/collections/mapPair("left"raw_utf8, 4i32,
                                                    "right"raw_utf8, 7i32))))
  [Result<Map<string, i32>, ContainerError>] status{take(holder.status)}
  return(0i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.empty());
}

TEST_CASE("helper-wrapped dereferenced Result.ok struct storage fields keep mismatch diagnostics") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_map/*

[return<T> effects(heap_alloc)]
wrapStatus<T>([T] status) {
  return(status)
}

Holder() {
  [uninitialized<Result<Map<string, i32>, ContainerError>> mut] status{
      uninitialized<Result<Map<string, i32>, ContainerError>>()}
}

[return<Reference<Holder>>]
borrowHolder([Reference<Holder>] holder) {
  return(holder)
}

[effects(heap_alloc), return<int>]
main() {
  [Holder mut] holder{Holder()}
  init(dereference(borrowHolder(location(holder))).status,
       wrapStatus(Result.ok(/std/collections/mapPair("left"raw_utf8, 4i32,
                                                    "wrong"raw_utf8, false))))
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.find("implicit template arguments conflict on /std/collections/mapPair") != std::string::npos);
}

TEST_CASE("helper-wrapped Result.ok payloads infer experimental result auto bindings") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_map/*

[return<T> effects(heap_alloc)]
wrapStatus<T>([T] status) {
  return(status)
}

[effects(heap_alloc), return<int>]
main() {
  [auto] status{wrapStatus(Result.ok(/std/collections/map/map("left"raw_utf8, 4i32,
                                                             "right"raw_utf8, 7i32)))}
  return(0i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.empty());
}

TEST_CASE("helper-wrapped Result.ok auto binding inference keeps template conflict diagnostics") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_map/*

[return<T> effects(heap_alloc)]
wrapStatus<T>([T] status) {
  return(status)
}

[effects(heap_alloc), return<int>]
main() {
  [auto] status{wrapStatus(Result.ok(/std/collections/mapPair("left"raw_utf8, 4i32,
                                                             "wrong"raw_utf8, false)))}
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.find("implicit template arguments conflict on /std/collections/mapPair") != std::string::npos);
}

TEST_CASE("stdlib wrapper mapPair constructor accepts explicit experimental map returns") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_map/*

[effects(io_err)]
unexpectedWrapperExperimentalMapReturnError([ContainerError] err) {
  [Result<ContainerError>] status{err.code}
  print_line_error(Result.why(status))
}

[return<Map<string, i32>> effects(heap_alloc)]
buildValues() {
  return(/std/collections/mapPair<string, i32>("left"raw_utf8, 4i32, "right"raw_utf8, 7i32))
}

[return<Result<int, ContainerError>> effects(io_out, heap_alloc) on_error<ContainerError, /unexpectedWrapperExperimentalMapReturnError>]
main() {
  [Map<string, i32>] values{buildValues()}
  [i32] found{try(/std/collections/map/tryAt(values, "left"raw_utf8))}
  [i32 mut] total{plus(/std/collections/map/count(values), found)}
  assign(total, plus(total, /std/collections/map/at(values, "left"raw_utf8)))
  assign(total, plus(total, /std/collections/map/at_unsafe(values, "right"raw_utf8)))
  if(/std/collections/map/contains(values, "left"raw_utf8),
     then() { assign(total, plus(total, 1i32)) },
     else() { })
  return(Result.ok(total))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("stdlib wrapper mapPair constructor keeps mismatch diagnostics on explicit experimental map returns") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_map/*

[return<Map<string, i32>> effects(heap_alloc)]
buildValues() {
  return(/std/collections/mapPair<string, i32>("left"raw_utf8, 4i32, "right"raw_utf8, false))
}

[effects(heap_alloc), return<int>]
main() {
  [Map<string, i32>] values{buildValues()}
  return(/std/collections/map/count(values))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("mismatch") != std::string::npos);
}

TEST_CASE("stdlib wrapper mapPair constructor accepts explicit experimental map parameters") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_map/*

[effects(io_err)]
unexpectedWrapperExperimentalMapParameterError([ContainerError] err) {
  [Result<ContainerError>] status{err.code}
  print_line_error(Result.why(status))
}

[return<Result<int, ContainerError>> effects(io_out, heap_alloc) on_error<ContainerError, /unexpectedWrapperExperimentalMapParameterError>]
scoreValues([Map<string, i32>] values) {
  [i32] found{try(/std/collections/map/tryAt(values, "left"raw_utf8))}
  [i32 mut] total{plus(/std/collections/map/count(values), found)}
  assign(total, plus(total, /std/collections/map/at(values, "left"raw_utf8)))
  assign(total, plus(total, /std/collections/map/at_unsafe(values, "right"raw_utf8)))
  if(/std/collections/map/contains(values, "left"raw_utf8),
     then() { assign(total, plus(total, 1i32)) },
     else() { })
  return(Result.ok(total))
}

[return<Result<int, ContainerError>> effects(io_out, heap_alloc) on_error<ContainerError, /unexpectedWrapperExperimentalMapParameterError>]
main() {
  return(scoreValues(/std/collections/mapPair<string, i32>("left"raw_utf8, 4i32, "right"raw_utf8, 7i32)))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("stdlib wrapper mapPair constructor keeps mismatch diagnostics on explicit experimental map parameters") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_map/*

[effects(heap_alloc), return<int>]
scoreValues([Map<string, i32>] values) {
  return(/std/collections/map/count(values))
}

[effects(heap_alloc), return<int>]
main() {
  return(scoreValues(/std/collections/mapPair<string, i32>("left"raw_utf8, 4i32, "right"raw_utf8, false)))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("mismatch") != std::string::npos);
}

TEST_CASE("map constructor assigns into explicit experimental map targets") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_map/*

[effects(io_err)]
unexpectedExperimentalMapAssignError([ContainerError] err) {
  [Result<ContainerError>] status{err.code}
  print_line_error(Result.why(status))
}

[return<Result<int, ContainerError>> effects(io_out, heap_alloc) on_error<ContainerError, /unexpectedExperimentalMapAssignError>]
scoreValues([Map<string, i32>] values) {
  [i32] found{try(/std/collections/map/tryAt(values, "left"raw_utf8))}
  [i32 mut] total{plus(/std/collections/map/count(values), found)}
  assign(total, plus(total, /std/collections/map/at(values, "left"raw_utf8)))
  assign(total, plus(total, /std/collections/map/at_unsafe(values, "right"raw_utf8)))
  if(/std/collections/map/contains(values, "left"raw_utf8),
     then() { assign(total, plus(total, 1i32)) },
     else() { })
  return(Result.ok(total))
}

[return<Result<int, ContainerError>> effects(io_out, heap_alloc) on_error<ContainerError, /unexpectedExperimentalMapAssignError>]
replaceAndScore([Map<string, i32> mut] values) {
  assign(values, /std/collections/mapPair<string, i32>("left"raw_utf8, 4i32, "right"raw_utf8, 7i32))
  return(scoreValues(values))
}

[return<Result<int, ContainerError>> effects(io_out, heap_alloc) on_error<ContainerError, /unexpectedExperimentalMapAssignError>]
main() {
  [Map<string, i32> mut] values{mapNew<string, i32>()}
  [Map<string, i32> mut] other{mapNew<string, i32>()}
  assign(values, /std/collections/map/map<string, i32>("left"raw_utf8, 4i32, "right"raw_utf8, 7i32))
  [i32] localScore{try(scoreValues(values))}
  [i32] paramScore{try(replaceAndScore(other))}
  return(Result.ok(plus(localScore, paramScore)))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("canonical map constructor assignment keeps mismatch diagnostics on explicit experimental map bindings") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_map/*

[effects(heap_alloc), return<int>]
main() {
  [Map<string, i32> mut] values{mapNew<string, i32>()}
  assign(values, /std/collections/map/map<string, i32>("left"raw_utf8, 4i32, "right"raw_utf8, false))
  return(/std/collections/map/count(values))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("mismatch") != std::string::npos);
}

TEST_CASE("wrapper map constructor assignment keeps mismatch diagnostics on explicit experimental map parameters") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_map/*

[effects(heap_alloc), return<int>]
replaceValues([Map<string, i32> mut] values) {
  assign(values, /std/collections/mapPair<string, i32>("left"raw_utf8, 4i32, "right"raw_utf8, false))
  return(/std/collections/map/count(values))
}

[effects(heap_alloc), return<int>]
main() {
  [Map<string, i32> mut] values{mapNew<string, i32>()}
  return(replaceValues(values))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("mismatch") != std::string::npos);
}

TEST_CASE("helper-wrapped map constructor assignments accept explicit experimental map targets") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_map/*

[return<T> effects(heap_alloc)]
wrapValues<T>([T] values) {
  return(values)
}

[effects(heap_alloc), return<int>]
main() {
  [Map<string, i32> mut] values{mapNew<string, i32>()}
  assign(values, wrapValues(/std/collections/map/map("left"raw_utf8, 4i32, "right"raw_utf8, 7i32)))
  return(/std/collections/map/at(values, "left"raw_utf8))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("helper-wrapped map constructor assignments keep mismatch diagnostics on explicit experimental map targets") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_map/*

[return<T> effects(heap_alloc)]
wrapValues<T>([T] values) {
  return(values)
}

[effects(heap_alloc), return<int>]
main() {
  [Map<string, i32> mut] values{mapNew<string, i32>()}
  assign(values, wrapValues(/std/collections/mapPair("left"raw_utf8, 4i32, "wrong"raw_utf8, false)))
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("implicit template arguments conflict on /std/collections/mapPair") != std::string::npos);
}

TEST_CASE("implicit map constructors infer experimental auto locals and auto returns") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_map/*

[return<auto> effects(heap_alloc)]
buildValues() {
  return(/std/collections/mapPair("left"raw_utf8, 4i32, "right"raw_utf8, 7i32))
}

[effects(io_err)]
unexpectedExperimentalMapAutoError([ContainerError] err) {
  [Result<ContainerError>] status{err.code}
  print_line_error(Result.why(status))
}

[return<Result<int, ContainerError>> effects(io_out, heap_alloc) on_error<ContainerError, /unexpectedExperimentalMapAutoError>]
main() {
  [auto mut] values{/std/collections/map/map("seed"raw_utf8, 1i32)}
  mapInsert<string, i32>(values, "left"raw_utf8, 4i32)
  mapInsert<string, i32>(values, "right"raw_utf8, 7i32)
  [auto mut] built{buildValues()}
  mapInsert<string, i32>(built, "extra"raw_utf8, 9i32)
  [i32] left{try(/std/collections/map/tryAt(values, "left"raw_utf8))}
  [i32] extra{try(/std/collections/map/tryAt(built, "extra"raw_utf8))}
  return(Result.ok(plus(plus(/std/collections/map/count(values), /std/collections/map/count(built)), plus(left, extra))))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("helper-wrapped map constructors infer experimental auto locals") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_map/*

[return<T> effects(heap_alloc)]
wrapValues<T>([T] values) {
  return(values)
}

[effects(io_err)]
unexpectedWrappedExperimentalMapAutoError([ContainerError] err) {
  [Result<ContainerError>] status{err.code}
  print_line_error(Result.why(status))
}

[return<Result<int, ContainerError>> effects(io_out, heap_alloc)
 on_error<ContainerError, /unexpectedWrappedExperimentalMapAutoError>]
main() {
  [auto mut] values{wrapValues(/std/collections/map/map("seed"raw_utf8, 1i32))}
  mapInsert<string, i32>(values, "left"raw_utf8, 4i32)
  mapInsert<string, i32>(values, "right"raw_utf8, 7i32)
  [i32] left{try(/std/collections/map/tryAt(values, "left"raw_utf8))}
  [i32] right{try(/std/collections/map/tryAt(values, "right"raw_utf8))}
  return(Result.ok(plus(/std/collections/map/count(values), plus(left, right))))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.empty());
}

TEST_CASE("helper-wrapped map constructor auto inference keeps template conflict diagnostics") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_map/*

[return<T> effects(heap_alloc)]
wrapValues<T>([T] values) {
  return(values)
}

[effects(heap_alloc), return<int>]
main() {
  [auto] values{wrapValues(/std/collections/map/map("left"raw_utf8, 4i32, "right"raw_utf8, false))}
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.find("implicit template arguments conflict on /std/collections/mapPair") != std::string::npos);
}

TEST_CASE("implicit map constructor auto inference keeps template conflict diagnostics") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_map/*

[effects(heap_alloc), return<int>]
main() {
  [auto] values{/std/collections/map/map("left"raw_utf8, 4i32, "right"raw_utf8, false)}
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("implicit template arguments conflict on /std/collections/mapPair") != std::string::npos);
}

TEST_CASE("inferred experimental map returns rewrite canonical constructors") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_map/*

[return<auto> effects(heap_alloc)]
buildValues() {
  return(/std/collections/map/map("left"raw_utf8, 4i32, "right"raw_utf8, 7i32))
}

[effects(io_err)]
unexpectedInferredExperimentalMapReturnError([ContainerError] err) {
  [Result<ContainerError>] status{err.code}
  print_line_error(Result.why(status))
}

[return<Result<int, ContainerError>> effects(io_out, heap_alloc) on_error<ContainerError, /unexpectedInferredExperimentalMapReturnError>]
main() {
  [Map<string, i32> mut] values{buildValues()}
  mapInsert<string, i32>(values, "extra"raw_utf8, 9i32)
  [i32] left{try(/std/collections/map/tryAt(values, "left"raw_utf8))}
  [i32] extra{try(/std/collections/map/tryAt(values, "extra"raw_utf8))}
  return(Result.ok(plus(/std/collections/map/count(values), plus(left, extra))))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("inferred experimental map returns keep constructor mismatch diagnostics") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_map/*

[return<auto> effects(heap_alloc)]
buildValues() {
  return(/std/collections/map/map("left"raw_utf8, 4i32, "right"raw_utf8, false))
}

[effects(heap_alloc), return<int>]
main() {
  [Map<string, i32>] values{buildValues()}
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("implicit template arguments conflict on /std/collections/mapPair") != std::string::npos);
}

TEST_CASE("block-bodied inferred experimental map returns rewrite constructors") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_map/*

[return<auto> effects(heap_alloc)]
buildValues([bool] useCanonical) {
  if(useCanonical,
     then() { /std/collections/map/map("left"raw_utf8, 4i32, "right"raw_utf8, 7i32) },
     else() { /std/collections/mapPair("left"raw_utf8, 4i32, "other"raw_utf8, 2i32) })
}

[effects(io_err)]
unexpectedBlockExperimentalMapReturnError([ContainerError] err) {
  [Result<ContainerError>] status{err.code}
  print_line_error(Result.why(status))
}

[return<Result<int, ContainerError>> effects(io_out, heap_alloc) on_error<ContainerError, /unexpectedBlockExperimentalMapReturnError>]
main() {
  [Map<string, i32> mut] values{buildValues(true)}
  mapInsert<string, i32>(values, "extra"raw_utf8, 9i32)
  [i32] left{try(/std/collections/map/tryAt(values, "left"raw_utf8))}
  [i32] extra{try(/std/collections/map/tryAt(values, "extra"raw_utf8))}
  return(Result.ok(plus(/std/collections/map/count(values), plus(left, extra))))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("block-bodied inferred experimental map returns keep mismatch diagnostics") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_map/*

[return<auto> effects(heap_alloc)]
buildValues([bool] useCanonical) {
  if(useCanonical,
     then() { /std/collections/map/map("left"raw_utf8, 4i32, "right"raw_utf8, 7i32) },
     else() { /std/collections/mapPair("left"raw_utf8, 4i32, "other"raw_utf8, false) })
}

[effects(heap_alloc), return<int>]
main() {
  [Map<string, i32>] values{buildValues(false)}
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("implicit template arguments conflict on /std/collections/mapPair") != std::string::npos);
}

TEST_CASE("auto bindings inside inferred experimental map return blocks rewrite constructors") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_map/*

[return<auto> effects(heap_alloc)]
buildValues([bool] useCanonical) {
  if(useCanonical,
     then() {
       [auto mut] values{/std/collections/map/map("left"raw_utf8, 4i32, "right"raw_utf8, 7i32)}
       mapInsert<string, i32>(values, "extra"raw_utf8, 9i32)
       values
     },
     else() {
       [auto mut] values{/std/collections/mapPair("left"raw_utf8, 4i32, "other"raw_utf8, 2i32)}
       values
     })
}

[effects(io_err)]
unexpectedAutoBlockExperimentalMapReturnError([ContainerError] err) {
  [Result<ContainerError>] status{err.code}
  print_line_error(Result.why(status))
}

[return<Result<int, ContainerError>> effects(io_out, heap_alloc) on_error<ContainerError, /unexpectedAutoBlockExperimentalMapReturnError>]
main() {
  [Map<string, i32>] values{buildValues(true)}
  [i32] left{try(/std/collections/map/tryAt(values, "left"raw_utf8))}
  [i32] extra{try(/std/collections/map/tryAt(values, "extra"raw_utf8))}
  return(Result.ok(plus(/std/collections/map/count(values), plus(left, extra))))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("auto bindings inside inferred experimental map return blocks keep mismatch diagnostics") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_map/*

[return<auto> effects(heap_alloc)]
buildValues([bool] useCanonical) {
  if(useCanonical,
     then() {
       [auto] values{/std/collections/map/map("left"raw_utf8, 4i32, "right"raw_utf8, 7i32)}
       values
     },
     else() {
       [auto] values{/std/collections/mapPair("left"raw_utf8, 4i32, "other"raw_utf8, false)}
       values
     })
}

[effects(heap_alloc), return<int>]
main() {
  [Map<string, i32>] values{buildValues(false)}
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("implicit template arguments conflict on /std/collections/mapPair") != std::string::npos);
}

TEST_CASE("helper-wrapped inferred experimental map returns rewrite nested constructor arguments") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_map/*

[return<T> effects(heap_alloc)]
wrapValues<T>([T] values) {
  return(values)
}

[return<auto> effects(heap_alloc)]
buildValues([bool] useCanonical) {
  if(useCanonical,
     then() { return(wrapValues(/std/collections/map/map("left"raw_utf8, 4i32, "right"raw_utf8, 7i32))) },
     else() { return(wrapValues(/std/collections/mapPair("extra"raw_utf8, 9i32, "bonus"raw_utf8, 5i32))) })
}

[effects(io_err)]
unexpectedWrappedExperimentalMapReturnError([ContainerError] err) {
  [Result<ContainerError>] status{err.code}
  print_line_error(Result.why(status))
}

[return<Result<int, ContainerError>> effects(io_out, heap_alloc) on_error<ContainerError, /unexpectedWrappedExperimentalMapReturnError>]
main() {
  [Map<string, i32>] canonical{buildValues(true)}
  [Map<string, i32>] wrapped{buildValues(false)}
  [i32] left{try(/std/collections/map/tryAt(canonical, "left"raw_utf8))}
  [i32] bonus{try(/std/collections/map/tryAt(wrapped, "bonus"raw_utf8))}
  return(Result.ok(plus(/std/collections/map/count(canonical), plus(left, bonus))))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("helper-wrapped inferred experimental map returns keep nested constructor mismatch diagnostics") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_map/*

[return<T> effects(heap_alloc)]
wrapValues<T>([T] values) {
  return(values)
}

[return<auto> effects(heap_alloc)]
buildValues() {
  return(wrapValues(/std/collections/mapPair("left"raw_utf8, 4i32, "wrong"raw_utf8, false)))
}

[effects(heap_alloc), return<int>]
main() {
  [Map<string, i32>] values{buildValues()}
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("implicit template arguments conflict on /std/collections/mapPair") != std::string::npos);
}

TEST_CASE("double helper-wrapped inferred experimental map returns rewrite nested constructor arguments") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_map/*

[return<T> effects(heap_alloc)]
wrapOnce<T>([T] values) {
  return(values)
}

[return<T> effects(heap_alloc)]
wrapTwice<T>([T] values) {
  return(wrapOnce(values))
}

[return<auto> effects(heap_alloc)]
buildValues([bool] useCanonical) {
  if(useCanonical,
     then() { return(wrapTwice(/std/collections/map/map("left"raw_utf8, 4i32, "right"raw_utf8, 7i32))) },
     else() { return(wrapTwice(/std/collections/mapPair("extra"raw_utf8, 9i32, "bonus"raw_utf8, 5i32))) })
}

[effects(io_err)]
unexpectedDoubleWrappedExperimentalMapReturnError([ContainerError] err) {
  [Result<ContainerError>] status{err.code}
  print_line_error(Result.why(status))
}

[return<Result<int, ContainerError>> effects(io_out, heap_alloc)
 on_error<ContainerError, /unexpectedDoubleWrappedExperimentalMapReturnError>]
main() {
  [Map<string, i32>] canonical{buildValues(true)}
  [Map<string, i32>] wrapped{buildValues(false)}
  [i32] left{try(/std/collections/map/tryAt(canonical, "left"raw_utf8))}
  [i32] bonus{try(/std/collections/map/tryAt(wrapped, "bonus"raw_utf8))}
  return(Result.ok(plus(/std/collections/map/count(canonical), plus(left, bonus))))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("double helper-wrapped inferred experimental map returns keep nested constructor mismatch diagnostics") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_map/*

[return<T> effects(heap_alloc)]
wrapOnce<T>([T] values) {
  return(values)
}

[return<T> effects(heap_alloc)]
wrapTwice<T>([T] values) {
  return(wrapOnce(values))
}

[return<auto> effects(heap_alloc)]
buildValues() {
  return(wrapTwice(/std/collections/mapPair("left"raw_utf8, 4i32, "wrong"raw_utf8, false)))
}

[effects(heap_alloc), return<int>]
main() {
  [Map<string, i32>] values{buildValues()}
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("implicit template arguments conflict on /std/collections/mapPair") != std::string::npos);
}

TEST_CASE("inferred experimental map call receivers resolve method sugar") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_map/*

[return<auto> effects(heap_alloc)]
buildValues([bool] useCanonical) {
  if(useCanonical,
     then() { /std/collections/map/map("left"raw_utf8, 4i32, "right"raw_utf8, 7i32) },
     else() { /std/collections/mapPair("left"raw_utf8, 4i32, "other"raw_utf8, 2i32) })
}

[effects(io_err)]
unexpectedInferredExperimentalMapReceiverError([ContainerError] err) {
  [Result<ContainerError>] status{err.code}
  print_line_error(Result.why(status))
}

[return<Result<int, ContainerError>> effects(io_out, heap_alloc) on_error<ContainerError, /unexpectedInferredExperimentalMapReceiverError>]
main() {
  [i32] left{try(buildValues(true).tryAt("left"raw_utf8))}
  [i32 mut] total{plus(buildValues(true).count(), left)}
  if(buildValues(true).contains("right"raw_utf8),
     then() { assign(total, plus(total, 1i32)) },
     else() { })
  return(Result.ok(total))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("inferred experimental map call receivers keep mismatch diagnostics") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_map/*

[return<auto> effects(heap_alloc)]
buildValues([bool] useCanonical) {
  if(useCanonical,
     then() { /std/collections/map/map("left"raw_utf8, 4i32, "right"raw_utf8, 7i32) },
     else() { /std/collections/mapPair("left"raw_utf8, 4i32, "other"raw_utf8, false) })
}

[effects(heap_alloc), return<int>]
main() {
  return(buildValues(false).count())
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("implicit template arguments conflict on /std/collections/mapPair") != std::string::npos);
}

TEST_CASE("stdlib map constructors accept explicit experimental map struct fields") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_map/*

[struct]
Holder() {
  [Map<string, i32>] primary{}
  [Map<string, i32>] secondary{}
}

[effects(io_err)]
unexpectedExperimentalMapStructFieldError([ContainerError] err) {
  [Result<ContainerError>] status{err.code}
  print_line_error(Result.why(status))
}

[return<Result<int, ContainerError>> effects(io_out, heap_alloc) on_error<ContainerError, /unexpectedExperimentalMapStructFieldError>]
main() {
  [Holder] holder{Holder(/std/collections/map/map("left"raw_utf8, 4i32, "right"raw_utf8, 7i32),
                        /std/collections/mapPair("other"raw_utf8, 2i32, "extra"raw_utf8, 9i32))}
  [i32] left{try(/std/collections/map/tryAt(holder.primary, "left"raw_utf8))}
  [i32] extra{try(/std/collections/map/tryAt(holder.secondary, "extra"raw_utf8))}
  return(Result.ok(plus(left, extra)))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("stdlib map constructors keep mismatch diagnostics on experimental map struct fields") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_map/*

[struct]
Holder() {
  [Map<string, i32>] values{}
}

[effects(heap_alloc), return<int>]
main() {
  [Holder] holder{Holder(/std/collections/mapPair("left"raw_utf8, 4i32, "wrong"raw_utf8, false))}
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("implicit template arguments conflict on /std/collections/mapPair") != std::string::npos);
}

TEST_CASE("stdlib map constructors accept inferred experimental map struct fields") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_map/*

[struct]
Holder() {
  primary{mapNew<string, i32>()}
  secondary{mapNew<string, i32>()}
}

[effects(heap_alloc), return<int>]
main() {
  [Holder mut] holder{Holder(/std/collections/map/map("left"raw_utf8, 4i32, "right"raw_utf8, 7i32))}
  assign(holder.secondary, /std/collections/mapPair("extra"raw_utf8, 9i32, "other"raw_utf8, 2i32))
  return(plus(/std/collections/map/at(holder.primary, "left"raw_utf8),
              /std/collections/map/at(holder.secondary, "extra"raw_utf8)))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("stdlib map constructors keep mismatch diagnostics on inferred experimental map struct fields") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_map/*

[struct]
Holder() {
  values{mapNew<string, i32>()}
}

[effects(heap_alloc), return<int>]
main() {
  [Holder mut] holder{Holder()}
  assign(holder.values, /std/collections/mapPair("left"raw_utf8, 4i32, "wrong"raw_utf8, false))
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("implicit template arguments conflict on /std/collections/mapPair") != std::string::npos);
}

TEST_CASE("helper-wrapped inferred experimental map struct fields rewrite constructors") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_map/*

[return<T> effects(heap_alloc)]
wrapValues<T>([T] values) {
  return(values)
}

[struct]
Holder() {
  primary{wrapValues(mapNew<string, i32>())}
  secondary{wrapValues(mapNew<string, i32>())}
}

[effects(heap_alloc), return<int>]
main() {
  [Holder mut] holder{Holder(/std/collections/map/map("left"raw_utf8, 4i32, "right"raw_utf8, 7i32))}
  assign(holder.secondary, /std/collections/mapPair("extra"raw_utf8, 9i32, "other"raw_utf8, 2i32))
  return(plus(/std/collections/map/at(holder.primary, "left"raw_utf8),
              /std/collections/map/at(holder.secondary, "extra"raw_utf8)))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("helper-wrapped inferred experimental map struct fields keep mismatch diagnostics") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_map/*

[return<T> effects(heap_alloc)]
wrapValues<T>([T] values) {
  return(values)
}

[struct]
Holder() {
  values{wrapValues(mapNew<string, i32>())}
}

[effects(heap_alloc), return<int>]
main() {
  [Holder mut] holder{Holder()}
  assign(holder.values, /std/collections/mapPair("left"raw_utf8, 4i32, "wrong"raw_utf8, false))
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("implicit template arguments conflict on /std/collections/mapPair") != std::string::npos);
}

TEST_CASE("helper-wrapped inferred experimental result map struct fields rewrite constructors") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_map/*

[return<T> effects(heap_alloc)]
wrapStatus<T>([T] status) {
  return(status)
}

[struct]
Holder() {
  status{wrapStatus(Result.ok(mapNew<string, i32>()))}
}

[effects(heap_alloc), return<int>]
main() {
  [Holder] holder{Holder(wrapStatus(Result.ok(/std/collections/mapPair("left"raw_utf8, 4i32,
                                                                      "right"raw_utf8, 7i32))))}
  return(0i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.empty());
}

TEST_CASE("helper-wrapped inferred experimental result map struct fields keep mismatch diagnostics") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_map/*

[return<T> effects(heap_alloc)]
wrapStatus<T>([T] status) {
  return(status)
}

[struct]
Holder() {
  status{wrapStatus(Result.ok(mapNew<string, i32>()))}
}

[effects(heap_alloc), return<int>]
main() {
  [Holder] holder{Holder(wrapStatus(Result.ok(/std/collections/mapPair("left"raw_utf8, 4i32,
                                                                      "wrong"raw_utf8, false))))}
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.find("implicit template arguments conflict on /std/collections/mapPair") != std::string::npos);
}

TEST_CASE("stdlib map constructors accept experimental map method-call parameters") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_map/*

Holder() {}

[return<int> effects(heap_alloc)]
/Holder/score([Holder] self, [Map<string, i32>] values) {
  return(plus(/std/collections/map/count(values),
              /std/collections/map/at_unsafe(values, "left"raw_utf8)))
}

[effects(heap_alloc), return<int>]
main() {
  [Holder] holder{Holder()}
  return(plus(holder.score(/std/collections/map/map("left"raw_utf8, 4i32, "right"raw_utf8, 7i32)),
              holder.score(/std/collections/mapPair("left"raw_utf8, 2i32, "extra"raw_utf8, 9i32))))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("stdlib map constructors keep mismatch diagnostics on experimental map method-call parameters") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_map/*

Holder() {}

[return<int> effects(heap_alloc)]
/Holder/score([Holder] self, [Map<string, i32>] values) {
  return(/std/collections/map/count(values))
}

[effects(heap_alloc), return<int>]
main() {
  [Holder] holder{Holder()}
  return(holder.score(/std/collections/mapPair("left"raw_utf8, 4i32, "wrong"raw_utf8, false)))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("implicit template arguments conflict on /std/collections/mapPair") != std::string::npos);
}

TEST_CASE("stdlib map constructors accept inferred experimental map default parameters") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_map/*

Holder() {}

[return<int> effects(heap_alloc)]
scoreValues([auto mut] values{mapNew<string, i32>()}) {
  mapInsert<string, i32>(values, "extra"raw_utf8, 9i32)
  return(plus(/std/collections/map/count(values),
              /std/collections/map/at(values, "left"raw_utf8)))
}

[return<int> effects(heap_alloc)]
/Holder/score([Holder] self, [auto mut] values{mapNew<string, i32>()}) {
  mapInsert<string, i32>(values, "bonus"raw_utf8, 5i32)
  return(plus(/std/collections/map/count(values),
              /std/collections/map/at(values, "extra"raw_utf8)))
}

[effects(heap_alloc), return<int>]
main() {
  [Holder] holder{Holder()}
  return(plus(scoreValues(/std/collections/map/map("left"raw_utf8, 4i32, "right"raw_utf8, 7i32)),
              holder.score(/std/collections/mapPair("left"raw_utf8, 2i32, "extra"raw_utf8, 9i32))))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("stdlib map constructors keep mismatch diagnostics on inferred experimental map default parameters") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_map/*

[return<int> effects(heap_alloc)]
scoreValues([auto mut] values{mapNew<string, i32>()}) {
  mapInsert<string, i32>(values, "extra"raw_utf8, 9i32)
  return(/std/collections/map/count(values))
}

[effects(heap_alloc), return<int>]
main() {
  return(scoreValues(/std/collections/mapPair("left"raw_utf8, 4i32, "wrong"raw_utf8, false)))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("implicit template arguments conflict on /std/collections/mapPair") != std::string::npos);
}

TEST_CASE("helper-wrapped map constructors accept inferred experimental map default parameters") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_map/*

Holder() {}

[return<T> effects(heap_alloc)]
wrapValues<T>([T] values) {
  return(values)
}

[return<int> effects(heap_alloc)]
scoreValues([auto mut] values{mapNew<string, i32>()}) {
  mapInsert<string, i32>(values, "extra"raw_utf8, 9i32)
  return(plus(/std/collections/map/count(values),
              /std/collections/map/at(values, "left"raw_utf8)))
}

[return<int> effects(heap_alloc)]
/Holder/score([Holder] self, [auto mut] values{mapNew<string, i32>()}) {
  mapInsert<string, i32>(values, "bonus"raw_utf8, 5i32)
  return(plus(/std/collections/map/count(values),
              /std/collections/map/at(values, "extra"raw_utf8)))
}

[effects(heap_alloc), return<int>]
main() {
  [Holder] holder{Holder()}
  return(plus(scoreValues(wrapValues(/std/collections/map/map("left"raw_utf8, 4i32, "right"raw_utf8, 7i32))),
              holder.score(wrapValues(/std/collections/mapPair("left"raw_utf8, 2i32, "extra"raw_utf8, 9i32)))))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("helper-wrapped map constructors keep mismatch diagnostics on inferred experimental map default parameters") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_map/*

[return<T> effects(heap_alloc)]
wrapValues<T>([T] values) {
  return(values)
}

[return<int> effects(heap_alloc)]
scoreValues([auto mut] values{mapNew<string, i32>()}) {
  mapInsert<string, i32>(values, "extra"raw_utf8, 9i32)
  return(/std/collections/map/count(values))
}

[effects(heap_alloc), return<int>]
main() {
  return(scoreValues(wrapValues(/std/collections/mapPair("left"raw_utf8, 4i32, "wrong"raw_utf8, false))))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.find("implicit template arguments conflict on /std/collections/mapPair") != std::string::npos);
}

TEST_CASE("helper-wrapped inferred experimental map default parameters rewrite constructors") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_map/*

Holder() {}

[return<T> effects(heap_alloc)]
wrapValues<T>([T] values) {
  return(values)
}

[return<int> effects(heap_alloc)]
scoreValues([auto mut] values{wrapValues(mapNew<string, i32>())}) {
  mapInsert<string, i32>(values, "extra"raw_utf8, 9i32)
  return(plus(/std/collections/map/count(values),
              /std/collections/map/at(values, "left"raw_utf8)))
}

[return<int> effects(heap_alloc)]
/Holder/score([Holder] self, [auto mut] values{wrapValues(mapNew<string, i32>())}) {
  mapInsert<string, i32>(values, "bonus"raw_utf8, 5i32)
  return(plus(/std/collections/map/count(values),
              /std/collections/map/at(values, "extra"raw_utf8)))
}

[effects(heap_alloc), return<int>]
main() {
  [Holder] holder{Holder()}
  return(plus(scoreValues(/std/collections/map/map("left"raw_utf8, 4i32, "right"raw_utf8, 7i32)),
              holder.score(/std/collections/mapPair("left"raw_utf8, 2i32, "extra"raw_utf8, 9i32))))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("helper-wrapped inferred experimental map default parameters keep mismatch diagnostics") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_map/*

[return<T> effects(heap_alloc)]
wrapValues<T>([T] values) {
  return(values)
}

[return<int> effects(heap_alloc)]
scoreValues([auto mut] values{wrapValues(mapNew<string, i32>())}) {
  mapInsert<string, i32>(values, "extra"raw_utf8, 9i32)
  return(/std/collections/map/count(values))
}

[effects(heap_alloc), return<int>]
main() {
  return(scoreValues(/std/collections/mapPair("left"raw_utf8, 4i32, "wrong"raw_utf8, false)))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("implicit template arguments conflict on /std/collections/mapPair") != std::string::npos);
}

TEST_CASE("canonical map helpers accept direct experimental constructor receivers") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_map/*

[effects(io_err)]
unexpectedExperimentalMapHelperReceiverError([ContainerError] err) {
  [Result<ContainerError>] status{err.code}
  print_line_error(Result.why(status))
}

[return<Result<int, ContainerError>> effects(io_out, heap_alloc) on_error<ContainerError, /unexpectedExperimentalMapHelperReceiverError>]
main() {
  [i32] found{try(/std/collections/map/tryAt(/std/collections/map/map("left"raw_utf8, 4i32, "right"raw_utf8, 7i32),
                                            "left"raw_utf8))}
  [i32 mut] total{plus(/std/collections/map/count(/std/collections/mapPair("left"raw_utf8, 4i32, "right"raw_utf8, 7i32)),
                          found)}
  assign(total, plus(total, /std/collections/map/at(/std/collections/mapPair("extra"raw_utf8, 9i32, "other"raw_utf8, 2i32),
                                                    "extra"raw_utf8)))
  assign(total, plus(total, /std/collections/map/at_unsafe(/std/collections/mapPair("bonus"raw_utf8, 5i32, "keep"raw_utf8, 1i32),
                                                           "bonus"raw_utf8)))
  if(/std/collections/map/contains(/std/collections/mapPair("left"raw_utf8, 4i32, "right"raw_utf8, 7i32),
                                   "right"raw_utf8),
     then() { assign(total, plus(total, 1i32)) },
     else() { })
  return(Result.ok(total))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("canonical map helpers keep mismatch diagnostics on direct constructor receivers") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_map/*

[effects(heap_alloc), return<int>]
main() {
  return(/std/collections/map/count(/std/collections/mapPair("left"raw_utf8, 4i32, "wrong"raw_utf8, false)))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("implicit template arguments conflict on /std/collections/mapPair") != std::string::npos);
}

TEST_CASE("helper-wrapped inferred experimental result map default parameters rewrite constructors") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_map/*

[return<T> effects(heap_alloc)]
wrapStatus<T>([T] status) {
  return(status)
}

[effects(heap_alloc), return<int>]
scoreStatus([auto] status{wrapStatus(Result.ok(mapNew<string, i32>()))}) {
  return(0i32)
}

[effects(heap_alloc), return<int>]
main() {
  return(scoreStatus(wrapStatus(Result.ok(/std/collections/mapPair("left"raw_utf8, 4i32,
                                                                  "right"raw_utf8, 7i32)))))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.empty());
}

TEST_CASE("helper-wrapped inferred experimental result map default parameters keep mismatch diagnostics") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_map/*

[return<T> effects(heap_alloc)]
wrapStatus<T>([T] status) {
  return(status)
}

[effects(heap_alloc), return<int>]
scoreStatus([auto] status{wrapStatus(Result.ok(mapNew<string, i32>()))}) {
  return(0i32)
}

[effects(heap_alloc), return<int>]
main() {
  return(scoreStatus(wrapStatus(Result.ok(/std/collections/mapPair("left"raw_utf8, 4i32,
                                                                  "wrong"raw_utf8, false)))))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.find("implicit template arguments conflict on /std/collections/mapPair") != std::string::npos);
}

TEST_CASE("helper-wrapped canonical map helpers accept direct experimental constructor receivers") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_map/*

[return<T> effects(heap_alloc)]
wrapValues<T>([T] values) {
  return(values)
}

[effects(io_err)]
unexpectedWrappedExperimentalMapHelperReceiverError([ContainerError] err) {
  [Result<ContainerError>] status{err.code}
  print_line_error(Result.why(status))
}

[return<Result<int, ContainerError>> effects(io_out, heap_alloc)
 on_error<ContainerError, /unexpectedWrappedExperimentalMapHelperReceiverError>]
main() {
  [i32] found{try(/std/collections/map/tryAt(wrapValues(/std/collections/map/map("left"raw_utf8, 4i32,
                                                                                  "right"raw_utf8, 7i32)),
                                            "left"raw_utf8))}
  [i32 mut] total{plus(/std/collections/map/count(wrapValues(/std/collections/mapPair("left"raw_utf8, 4i32,
                                                                                      "right"raw_utf8, 7i32))),
                          found)}
  assign(total, plus(total, /std/collections/map/at(wrapValues(/std/collections/mapPair("extra"raw_utf8, 9i32,
                                                                                        "other"raw_utf8, 2i32)),
                                                    "extra"raw_utf8)))
  assign(total, plus(total, /std/collections/map/at_unsafe(wrapValues(/std/collections/mapPair("bonus"raw_utf8, 5i32,
                                                                                               "keep"raw_utf8, 1i32)),
                                                           "bonus"raw_utf8)))
  if(/std/collections/map/contains(wrapValues(/std/collections/mapPair("left"raw_utf8, 4i32,
                                                                       "right"raw_utf8, 7i32)),
                                   "right"raw_utf8),
     then() { assign(total, plus(total, 1i32)) },
     else() { })
  return(Result.ok(total))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("helper-wrapped canonical map helpers keep mismatch diagnostics on direct constructor receivers") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_map/*

[return<T> effects(heap_alloc)]
wrapValues<T>([T] values) {
  return(values)
}

[effects(heap_alloc), return<int>]
main() {
  return(/std/collections/map/count(wrapValues(/std/collections/mapPair("left"raw_utf8, 4i32,
                                                                        "wrong"raw_utf8, false))))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("implicit template arguments conflict on /std/collections/mapPair") != std::string::npos);
}

TEST_CASE("experimental map methods accept direct constructor receivers") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_map/*

[effects(io_err)]
unexpectedExperimentalMapMethodReceiverError([ContainerError] err) {
  [Result<ContainerError>] status{err.code}
  print_line_error(Result.why(status))
}

[return<Result<int, ContainerError>> effects(io_out, heap_alloc) on_error<ContainerError, /unexpectedExperimentalMapMethodReceiverError>]
main() {
  [i32] found{try(/std/collections/map/map("left"raw_utf8, 4i32, "right"raw_utf8, 7i32).tryAt("left"raw_utf8))}
  [i32 mut] total{plus(/std/collections/mapPair("left"raw_utf8, 4i32, "right"raw_utf8, 7i32).count(), found)}
  assign(total, plus(total, /std/collections/mapPair("extra"raw_utf8, 9i32, "other"raw_utf8, 2i32).at("extra"raw_utf8)))
  assign(total, plus(total, /std/collections/mapPair("bonus"raw_utf8, 5i32, "keep"raw_utf8, 1i32).at_unsafe("bonus"raw_utf8)))
  if(/std/collections/mapPair("left"raw_utf8, 4i32, "right"raw_utf8, 7i32).contains("right"raw_utf8),
     then() { assign(total, plus(total, 1i32)) },
     else() { })
  return(Result.ok(total))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("experimental map methods keep mismatch diagnostics on direct constructor receivers") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_map/*

[effects(heap_alloc), return<int>]
main() {
  return(/std/collections/mapPair("left"raw_utf8, 4i32, "wrong"raw_utf8, false).count())
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("implicit template arguments conflict on /std/collections/mapPair") != std::string::npos);
}

TEST_CASE("helper-wrapped experimental map methods accept direct constructor receivers") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_map/*

[return<T> effects(heap_alloc)]
wrapValues<T>([T] values) {
  return(values)
}

[effects(io_err)]
unexpectedWrappedExperimentalMapMethodReceiverError([ContainerError] err) {
  [Result<ContainerError>] status{err.code}
  print_line_error(Result.why(status))
}

[return<Result<int, ContainerError>> effects(io_out, heap_alloc)
 on_error<ContainerError, /unexpectedWrappedExperimentalMapMethodReceiverError>]
main() {
  [i32] found{try(wrapValues(/std/collections/map/map("left"raw_utf8, 4i32, "right"raw_utf8, 7i32))
                  .tryAt("left"raw_utf8))}
  [i32 mut] total{plus(wrapValues(/std/collections/mapPair("left"raw_utf8, 4i32, "right"raw_utf8, 7i32)).count(),
                          found)}
  assign(total, plus(total, wrapValues(/std/collections/mapPair("extra"raw_utf8, 9i32, "other"raw_utf8, 2i32))
                                .at("extra"raw_utf8)))
  assign(total, plus(total, wrapValues(/std/collections/mapPair("bonus"raw_utf8, 5i32, "keep"raw_utf8, 1i32))
                                .at_unsafe("bonus"raw_utf8)))
  if(wrapValues(/std/collections/mapPair("left"raw_utf8, 4i32, "right"raw_utf8, 7i32)).contains("right"raw_utf8),
     then() { assign(total, plus(total, 1i32)) },
     else() { })
  return(Result.ok(total))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("helper-wrapped experimental map methods keep mismatch diagnostics on direct constructor receivers") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_map/*

[return<T> effects(heap_alloc)]
wrapValues<T>([T] values) {
  return(values)
}

[effects(heap_alloc), return<int>]
main() {
  return(wrapValues(/std/collections/mapPair("left"raw_utf8, 4i32, "wrong"raw_utf8, false)).count())
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("implicit template arguments conflict on /std/collections/mapPair") != std::string::npos);
}

TEST_CASE("field-bound experimental map methods accept struct field receivers") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_map/*

[effects(io_err)]
unexpectedFieldExperimentalMapMethodReceiverError([ContainerError] err) {
  [Result<ContainerError>] status{err.code}
  print_line_error(Result.why(status))
}

Holder() {
  [Map<string, i32>] values{mapPair<string, i32>("left"raw_utf8, 4i32, "right"raw_utf8, 7i32)}
}

[return<Result<int, ContainerError>> effects(io_out, heap_alloc)
 on_error<ContainerError, /unexpectedFieldExperimentalMapMethodReceiverError>]
main() {
  [Holder] holder{Holder()}
  [i32] found{try(holder.values.tryAt("left"raw_utf8))}
  [i32 mut] total{plus(holder.values.count(), found)}
  assign(total, plus(total, holder.values.at("right"raw_utf8)))
  return(Result.ok(total))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("field-bound experimental map wrapper helpers accept struct field receivers") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_map/*

Holder() {
  [Map<string, i32>] values{mapPair<string, i32>("left"raw_utf8, 4i32, "right"raw_utf8, 7i32)}
}

[effects(heap_alloc), return<int>]
main() {
  [Holder] holder{Holder()}
  return(plus(/std/collections/mapCount(holder.values),
              /std/collections/mapAt(holder.values, "left"raw_utf8)))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("field-bound experimental map bare count fallback validates") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_map/*

Holder() {
  [Map<string, i32>] values{mapPair<string, i32>("left"raw_utf8, 4i32, "right"raw_utf8, 7i32)}
}

[effects(heap_alloc), return<int>]
main() {
  [Holder] holder{Holder()}
  return(count(holder.values))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("field-bound experimental map bare at body arguments validate") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_map/*

[return<int>]
/std/collections/map/at([Map<string, i32>] values, [string] key) {
  return(90i32)
}

Holder() {
  [Map<string, i32>] values{mapPair<string, i32>("left"raw_utf8, 4i32, "right"raw_utf8, 7i32)}
}

[effects(heap_alloc), return<int>]
main() {
  [Holder] holder{Holder()}
  at(holder.values, "left"raw_utf8) { 1i32 }
  return(0i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("field-bound experimental map bare at expression body arguments validate") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_map/*

[return<int>]
/std/collections/map/at([Map<string, i32>] values, [string] key) {
  return(90i32)
}

Holder() {
  [Map<string, i32>] values{mapPair<string, i32>("left"raw_utf8, 4i32, "right"raw_utf8, 7i32)}
}

[effects(heap_alloc), return<int>]
main() {
  [Holder] holder{Holder()}
  return(at(holder.values, "left"raw_utf8) { 1i32 })
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("field-bound experimental map bare at expression body arguments keep canonical mismatch diagnostics") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_map/*

[return<int>]
/std/collections/map/at([Map<string, i32>] values) {
  return(90i32)
}

Holder() {
  [Map<string, i32>] values{mapPair<string, i32>("left"raw_utf8, 4i32, "right"raw_utf8, 7i32)}
}

[effects(heap_alloc), return<int>]
main() {
  [Holder] holder{Holder()}
  return(at(holder.values, "left"raw_utf8) { 1i32 })
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("argument count mismatch for /std/collections/map/at") != std::string::npos);
}

TEST_CASE("field-bound experimental map compatibility count calls keep removed-path diagnostics") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_map/*

/std/collections/map/count([map<string, i32>] values) {
  return(17i32)
}

Holder() {
  [Map<string, i32>] values{mapPair<string, i32>("left"raw_utf8, 4i32, "right"raw_utf8, 7i32)}
}

[effects(heap_alloc), return<int>]
main() {
  [Holder] holder{Holder()}
  return(/map/count(holder.values))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /map/count") != std::string::npos);
}

TEST_CASE("field-bound experimental map stdlib namespaced methods keep removed-path diagnostics") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_map/*

/std/collections/map/count([map<string, i32>] values) {
  return(17i32)
}

Holder() {
  [Map<string, i32>] values{mapPair<string, i32>("left"raw_utf8, 4i32, "right"raw_utf8, 7i32)}
}

[effects(heap_alloc), return<int>]
main() {
  [Holder] holder{Holder()}
  return(holder.values./std/collections/map/count())
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown method: /map/count") != std::string::npos);
}

TEST_CASE("stdlib map constructor assignments accept explicit experimental map struct fields") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_map/*

Holder() {
  [Map<string, i32> mut] primary{mapNew<string, i32>()}
  [Map<string, i32> mut] secondary{mapNew<string, i32>()}
}

[effects(heap_alloc), return<int>]
main() {
  [Holder mut] holder{Holder()}
  assign(holder.primary, /std/collections/map/map("left"raw_utf8, 4i32, "right"raw_utf8, 7i32))
  assign(holder.secondary, /std/collections/mapPair("extra"raw_utf8, 9i32, "other"raw_utf8, 2i32))
  return(plus(/std/collections/map/at(holder.primary, "left"raw_utf8),
              /std/collections/map/at(holder.secondary, "extra"raw_utf8)))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("stdlib map constructor assignments keep mismatch diagnostics on experimental map struct fields") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_map/*

Holder() {
  [Map<string, i32> mut] values{mapNew<string, i32>()}
}

[effects(heap_alloc), return<int>]
main() {
  [Holder mut] holder{Holder()}
  assign(holder.values, /std/collections/mapPair("left"raw_utf8, 4i32, "wrong"raw_utf8, false))
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("implicit template arguments conflict on /std/collections/mapPair") != std::string::npos);
}

TEST_CASE("helper-wrapped map constructor assignments accept inferred experimental map struct fields") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_map/*

[return<T> effects(heap_alloc)]
wrapValues<T>([T] values) {
  return(values)
}

Holder() {
  values{wrapValues(mapNew<string, i32>())}
}

[effects(heap_alloc), return<int>]
main() {
  [Holder mut] holder{Holder()}
  assign(holder.values, wrapValues(/std/collections/mapPair("left"raw_utf8, 4i32, "right"raw_utf8, 7i32)))
  return(/std/collections/map/at(holder.values, "left"raw_utf8))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("helper-wrapped map constructor assignments keep mismatch diagnostics on inferred experimental map struct fields") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_map/*

[return<T> effects(heap_alloc)]
wrapValues<T>([T] values) {
  return(values)
}

Holder() {
  values{wrapValues(mapNew<string, i32>())}
}

[effects(heap_alloc), return<int>]
main() {
  [Holder mut] holder{Holder()}
  assign(holder.values, wrapValues(/std/collections/mapPair("left"raw_utf8, 4i32, "wrong"raw_utf8, false)))
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("implicit template arguments conflict on /std/collections/mapPair") != std::string::npos);
}

TEST_CASE("helper-wrapped Result.ok payload assignments accept explicit experimental map result struct fields") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_map/*

[return<T> effects(heap_alloc)]
wrapStatus<T>([T] status) {
  return(status)
}

Holder() {
  [Result<Map<string, i32>, ContainerError> mut] status{Result.ok(mapNew<string, i32>())}
}

[effects(io_err)]
unexpectedWrappedExperimentalResultMapFieldAssignError([ContainerError] err) {
  [Result<ContainerError>] current{err.code}
  print_line_error(Result.why(current))
}

[return<Result<int, ContainerError>> effects(io_out, heap_alloc)
 on_error<ContainerError, /unexpectedWrappedExperimentalResultMapFieldAssignError>]
main() {
  [Holder mut] holder{Holder()}
  assign(holder.status, wrapStatus(Result.ok(/std/collections/mapPair("left"raw_utf8, 4i32,
                                                                     "right"raw_utf8, 7i32))))
  [Map<string, i32>] values{try(holder.status)}
  return(Result.ok(plus(/std/collections/map/count(values),
                        /std/collections/map/at(values, "left"raw_utf8))))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.empty());
}

TEST_CASE("helper-wrapped Result.ok payload assignments keep mismatch diagnostics on explicit experimental map result struct fields") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_map/*

[return<T> effects(heap_alloc)]
wrapStatus<T>([T] status) {
  return(status)
}

Holder() {
  [Result<Map<string, i32>, ContainerError> mut] status{Result.ok(mapNew<string, i32>())}
}

[effects(io_err)]
unexpectedWrappedExperimentalResultMapFieldAssignError([ContainerError] err) {
  [Result<ContainerError>] current{err.code}
  print_line_error(Result.why(current))
}

[return<Result<int, ContainerError>> effects(io_out, heap_alloc)
 on_error<ContainerError, /unexpectedWrappedExperimentalResultMapFieldAssignError>]
main() {
  [Holder mut] holder{Holder()}
  assign(holder.status, wrapStatus(Result.ok(/std/collections/mapPair("left"raw_utf8, 4i32,
                                                                     "wrong"raw_utf8, false))))
  return(Result.ok(0i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.find("implicit template arguments conflict on /std/collections/mapPair") != std::string::npos);
}

TEST_CASE("helper-wrapped map constructor assignments accept dereferenced experimental map struct fields") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_map/*

[return<T> effects(heap_alloc)]
wrapValues<T>([T] values) {
  return(values)
}

Holder() {
  [Map<string, i32> mut] values{mapNew<string, i32>()}
}

[return<Reference<Holder>>]
borrowHolder([Reference<Holder>] holder) {
  return(holder)
}

[effects(heap_alloc), return<int>]
main() {
  [Holder mut] holder{Holder()}
  assign(dereference(borrowHolder(location(holder))).values,
         wrapValues(/std/collections/mapPair("left"raw_utf8, 4i32,
                                             "right"raw_utf8, 7i32)))
  return(/std/collections/map/at(holder.values, "left"raw_utf8))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.empty());
}

TEST_CASE("helper-wrapped dereferenced map field assignments keep mismatch diagnostics") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_map/*

[return<T> effects(heap_alloc)]
wrapValues<T>([T] values) {
  return(values)
}

Holder() {
  [Map<string, i32> mut] values{mapNew<string, i32>()}
}

[return<Reference<Holder>>]
borrowHolder([Reference<Holder>] holder) {
  return(holder)
}

[effects(heap_alloc), return<int>]
main() {
  [Holder mut] holder{Holder()}
  assign(dereference(borrowHolder(location(holder))).values,
         wrapValues(/std/collections/mapPair("left"raw_utf8, 4i32,
                                             "wrong"raw_utf8, false)))
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.find("implicit template arguments conflict on /std/collections/mapPair") != std::string::npos);
}

TEST_CASE("helper-wrapped Result.ok assignments accept dereferenced experimental result map struct fields") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_map/*

[return<T> effects(heap_alloc)]
wrapStatus<T>([T] status) {
  return(status)
}

Holder() {
  [Result<Map<string, i32>, ContainerError> mut] status{Result.ok(mapNew<string, i32>())}
}

[return<Reference<Holder>>]
borrowHolder([Reference<Holder>] holder) {
  return(holder)
}

[effects(io_err)]
unexpectedWrappedExperimentalResultMapDerefFieldAssignError([ContainerError] err) {
  [Result<ContainerError>] current{err.code}
  print_line_error(Result.why(current))
}

[return<Result<int, ContainerError>> effects(io_out, heap_alloc)
 on_error<ContainerError, /unexpectedWrappedExperimentalResultMapDerefFieldAssignError>]
main() {
  [Holder mut] holder{Holder()}
  assign(dereference(borrowHolder(location(holder))).status,
         wrapStatus(Result.ok(/std/collections/mapPair("left"raw_utf8, 4i32,
                                                      "right"raw_utf8, 7i32))))
  [Map<string, i32>] values{try(holder.status)}
  return(Result.ok(plus(/std/collections/map/count(values),
                        /std/collections/map/at(values, "left"raw_utf8))))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.empty());
}

TEST_CASE("helper-wrapped dereferenced Result.ok field assignments keep mismatch diagnostics") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_map/*

[return<T> effects(heap_alloc)]
wrapStatus<T>([T] status) {
  return(status)
}

Holder() {
  [Result<Map<string, i32>, ContainerError> mut] status{Result.ok(mapNew<string, i32>())}
}

[return<Reference<Holder>>]
borrowHolder([Reference<Holder>] holder) {
  return(holder)
}

[effects(io_err)]
unexpectedWrappedExperimentalResultMapDerefFieldAssignError([ContainerError] err) {
  [Result<ContainerError>] current{err.code}
  print_line_error(Result.why(current))
}

[return<Result<int, ContainerError>> effects(io_out, heap_alloc)
 on_error<ContainerError, /unexpectedWrappedExperimentalResultMapDerefFieldAssignError>]
main() {
  [Holder mut] holder{Holder()}
  assign(dereference(borrowHolder(location(holder))).status,
         wrapStatus(Result.ok(/std/collections/mapPair("left"raw_utf8, 4i32,
                                                      "wrong"raw_utf8, false))))
  return(Result.ok(0i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.find("implicit template arguments conflict on /std/collections/mapPair") != std::string::npos);
}

#endif
TEST_CASE("stdlib namespaced map helpers keep Comparable diagnostics on experimental map value receivers") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_map/*

[struct]
Key() {
  [i32] value{0i32}
}

[effects(heap_alloc), return<int>]
main() {
  [Map<Key, i32>] values{mapNew<Key, i32>()}
  if(/std/collections/map/contains(values, Key(1i32)),
     then() { return(1i32) },
     else() { return(0i32) })
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("Comparable") != std::string::npos);
}

TEST_CASE("stdlib namespaced map helpers keep canonical key diagnostics on map references") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [/std/collections/map<i32, i32>] values{map<i32, i32>(1i32, 4i32)}
  [Reference</std/collections/map<i32, i32>>] ref{location(values)}
  return(/std/collections/map/at(ref, true))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("argument type mismatch for /std/collections/map/at") != std::string::npos);
  CHECK(error.find("parameter key") != std::string::npos);
}

TEST_CASE("map compatibility count call requires explicit alias definition") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/std/collections/map/count([map<i32, i32>] values) {
  return(17i32)
}

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 4i32)}
  return(/map/count(values))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /map/count") != std::string::npos);
}

TEST_CASE("map compatibility count auto inference requires explicit alias definition") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/std/collections/map/count([map<i32, i32>] values) {
  return(17i32)
}

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 4i32)}
  [auto] inferred{/map/count(values)}
  return(inferred)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /map/count") != std::string::npos);
}

TEST_CASE("map compatibility contains call requires explicit alias definition") {
  const std::string source = R"(
[effects(heap_alloc), return<bool>]
/std/collections/map/contains([map<i32, i32>] values, [i32] key) {
  return(false)
}

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 4i32)}
  [bool] found{/map/contains(values, 1i32)}
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /map/contains") != std::string::npos);
}

TEST_CASE("map compatibility contains call keeps explicit alias precedence") {
  const std::string source = R"(
[effects(heap_alloc), return<bool>]
/map/contains([map<i32, i32>] values, [i32] key) {
  return(false)
}

[effects(heap_alloc), return<bool>]
/std/collections/map/contains([map<i32, i32>] values, [bool] key) {
  return(true)
}

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 4i32)}
  [bool] found{/map/contains(values, 1i32)}
  return(0i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("map compatibility tryAt call requires explicit alias definition") {
  const std::string source = R"(
[effects(heap_alloc), return<Result<i32, ContainerError>>]
/std/collections/map/tryAt([map<i32, i32>] values, [i32] key) {
  return(Result.ok(17i32))
}

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 4i32)}
  return(try(/map/tryAt(values, 1i32)))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /map/tryAt") != std::string::npos);
}

TEST_CASE("map compatibility tryAt call keeps explicit alias precedence") {
  const std::string source = R"(
[effects(heap_alloc), return<Result<i32, ContainerError>>]
/map/tryAt([map<i32, i32>] values, [i32] key) {
  return(Result.ok(19i32))
}

[effects(heap_alloc), return<Result<i32, ContainerError>>]
/std/collections/map/tryAt([map<i32, i32>] values, [bool] key) {
  return(Result.ok(7i32))
}

[effects(heap_alloc), return<Result<i32, ContainerError>>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 4i32)}
  return(/map/tryAt(values, 1i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("map wrapper temporary tryAt call validates canonical target classification") {
  const std::string source = R"(
[struct]
ContainerError() {
  [i32] code{0i32}
}

namespace ContainerError {
  [return<string>]
  why([ContainerError] err) {
    return("container error"utf8)
  }
}

[effects(io_err)]
unexpectedMapWrapperTryAtError([ContainerError] err) {}

[effects(heap_alloc), return<map<i32, i32>>]
wrapMap() {
  return(map<i32, i32>(1i32, 4i32))
}

[effects(heap_alloc), return<Result<i32, ContainerError>>]
/std/collections/map/tryAt([map<i32, i32>] values, [i32] key) {
  return(Result.ok(17i32))
}

[return<Result<int, ContainerError>> effects(io_out, heap_alloc) on_error<ContainerError, /unexpectedMapWrapperTryAtError>]
main() {
  return(Result.ok(try(/std/collections/map/tryAt(wrapMap(), 1i32))))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("map wrapper temporary tryAt auto inference rejects explicit helper binding as try input") {
  const std::string source = R"(
[struct]
ContainerError() {
  [i32] code{0i32}
}

namespace ContainerError {
  [return<string>]
  why([ContainerError] err) {
    return("container error"utf8)
  }
}

[effects(io_err)]
unexpectedMapWrapperTryAtError([ContainerError] err) {}

[effects(heap_alloc), return<map<i32, i32>>]
wrapMap() {
  return(map<i32, i32>(1i32, 4i32))
}

[effects(heap_alloc), return<Result<i32, ContainerError>>]
/std/collections/map/tryAt([map<i32, i32>] values, [i32] key) {
  return(Result.ok(17i32))
}

[return<Result<int, ContainerError>> effects(io_out, heap_alloc) on_error<ContainerError, /unexpectedMapWrapperTryAtError>]
main() {
  [auto] inferred{/std/collections/map/tryAt(wrapMap(), 1i32)}
  return(Result.ok(try(inferred)))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("try requires Result argument") != std::string::npos);
}

TEST_CASE("map wrapper temporary tryAt call requires canonical helper definition") {
  const std::string source = R"(
[struct]
ContainerError() {
  [i32] code{0i32}
}

namespace ContainerError {
  [return<string>]
  why([ContainerError] err) {
    return("container error"utf8)
  }
}

[effects(io_err)]
unexpectedMapWrapperTryAtError([ContainerError] err) {}

[effects(heap_alloc), return<map<i32, i32>>]
wrapMap() {
  return(map<i32, i32>(1i32, 4i32))
}

[return<Result<int, ContainerError>> effects(io_out, heap_alloc) on_error<ContainerError, /unexpectedMapWrapperTryAtError>]
main() {
  return(Result.ok(try(/std/collections/map/tryAt(wrapMap(), 1i32))))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /std/collections/map/tryAt") != std::string::npos);
}

TEST_CASE("map wrapper temporary tryAt auto inference keeps builtin map result fallback without helper") {
  const std::string source = R"(
[struct]
ContainerError() {
  [i32] code{0i32}
}

namespace ContainerError {
  [return<string>]
  why([ContainerError] err) {
    return("container error"utf8)
  }
}

[effects(io_err)]
unexpectedMapWrapperTryAtError([ContainerError] err) {}

[effects(heap_alloc), return<map<i32, i32>>]
wrapMap() {
  return(map<i32, i32>(1i32, 4i32))
}

[return<Result<int, ContainerError>> effects(io_out, heap_alloc) on_error<ContainerError, /unexpectedMapWrapperTryAtError>]
main() {
  [auto] inferred{/std/collections/map/tryAt(wrapMap(), 1i32)}
  return(Result.ok(try(inferred)))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("map wrapper temporary contains call validates canonical target classification") {
  const std::string source = R"(
[effects(heap_alloc), return<map<i32, i32>>]
wrapMap() {
  return(map<i32, i32>(1i32, 4i32))
}

[effects(heap_alloc), return<bool>]
/std/collections/map/contains([map<i32, i32>] values, [i32] key) {
  return(true)
}

[effects(heap_alloc), return<bool>]
main() {
  return(/std/collections/map/contains(wrapMap(), 1i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("map wrapper temporary contains auto inference uses canonical helper return") {
  const std::string source = R"(
[effects(heap_alloc), return<map<i32, i32>>]
wrapMap() {
  return(map<i32, i32>(1i32, 4i32))
}

[effects(heap_alloc), return<bool>]
/std/collections/map/contains([map<i32, i32>] values, [i32] key) {
  return(true)
}

[effects(heap_alloc), return<bool>]
main() {
  [auto] inferred{/std/collections/map/contains(wrapMap(), 1i32)}
  return(inferred)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("map wrapper temporary contains call requires canonical helper definition") {
  const std::string source = R"(
[effects(heap_alloc), return<map<i32, i32>>]
wrapMap() {
  return(map<i32, i32>(1i32, 4i32))
}

[effects(heap_alloc), return<bool>]
main() {
  return(/std/collections/map/contains(wrapMap(), 1i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /std/collections/map/contains") != std::string::npos);
}

TEST_CASE("map wrapper temporary contains auto inference requires canonical helper definition") {
  const std::string source = R"(
[effects(heap_alloc), return<map<i32, i32>>]
wrapMap() {
  return(map<i32, i32>(1i32, 4i32))
}

[effects(heap_alloc), return<bool>]
main() {
  [auto] inferred{/std/collections/map/contains(wrapMap(), 1i32)}
  return(inferred)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /std/collections/map/contains") != std::string::npos);
}

TEST_CASE("map compatibility at call requires explicit alias definition") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/std/collections/map/at([map<i32, i32>] values, [i32] index) {
  return(17i32)
}

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 4i32)}
  return(/map/at(values, 1i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /map/at") != std::string::npos);
}

TEST_CASE("map compatibility at call keeps explicit alias precedence") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/map/at([map<i32, i32>] values, [i32] index) {
  return(19i32)
}

[effects(heap_alloc), return<int>]
/std/collections/map/at([map<i32, i32>] values, [bool] index) {
  return(17i32)
}

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 4i32)}
  return(/map/at(values, 1i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("map compatibility at_unsafe auto inference requires explicit alias definition") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/std/collections/map/at_unsafe([map<i32, i32>] values, [i32] index) {
  return(17i32)
}

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 4i32)}
  [auto] inferred{/map/at_unsafe(values, 1i32)}
  return(inferred)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /map/at_unsafe") != std::string::npos);
}

TEST_CASE("map compatibility at_unsafe keeps explicit alias precedence") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/map/at_unsafe([map<i32, i32>] values, [i32] index) {
  return(23i32)
}

[effects(heap_alloc), return<int>]
/std/collections/map/at_unsafe([map<i32, i32>] values, [bool] index) {
  return(17i32)
}

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 4i32)}
  [auto] inferred{/map/at_unsafe(values, 1i32)}
  return(inferred)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("stdlib namespaced map constructor does not resolve map alias helper fallback") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/map/map<T, U>([T] key, [U] value) {
  return(77i32)
}

[effects(heap_alloc), return<int>]
main() {
  return(/std/collections/map/map<i32, i32>(1i32, 2i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /std/collections/map/map") != std::string::npos);
}

TEST_CASE("map unnamespaced count call resolves through canonical helper") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/std/collections/map/count([map<i32, i32>] values) {
  return(17i32)
}

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 4i32)}
  return(count(values))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("map unnamespaced count auto inference resolves through canonical helper") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/std/collections/map/count([map<i32, i32>] values) {
  return(17i32)
}

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 4i32)}
  [auto] inferred{count(values)}
  return(inferred)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("map unnamespaced count call resolves through compatibility alias when canonical helper is absent") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/map/count([map<i32, i32>] values) {
  return(19i32)
}

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 4i32)}
  return(count(values))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("map unnamespaced count call validates without imported canonical helper") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 4i32)}
  return(count(values))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("map unnamespaced count auto inference stays stable") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 4i32)}
  [auto] inferred{count(values)}
  return(inferred)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("bare map at call resolves through canonical helper") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/std/collections/map/at([map<i32, i32>] values, [i32] index) {
  return(17i32)
}

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 4i32)}
  return(at(values, 1i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("bare map at wrapper call resolves through canonical helper") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/std/collections/map/at([map<i32, i32>] values, [i32] index) {
  return(17i32)
}

[effects(heap_alloc), return<map<i32, i32>>]
wrapMap() {
  return(map<i32, i32>(1i32, 4i32))
}

[effects(heap_alloc), return<int>]
main() {
  return(at(wrapMap(), 1i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("bare map at wrapper call validates builtin wrapper receiver without helpers") {
  const std::string source = R"(
[effects(heap_alloc), return<map<i32, i32>>]
wrapMap() {
  return(map<i32, i32>(1i32, 4i32))
}

[effects(heap_alloc), return<int>]
main() {
  return(at(wrapMap(), 1i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("bare map at call resolves through compatibility helper when canonical helper is absent") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/map/at([map<i32, i32>] values, [i32] index) {
  return(17i32)
}

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 4i32)}
  return(at(values, 1i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("bare map at call prefers canonical helper over compatibility alias") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/std/collections/map/at([map<i32, i32>] values, [i32] index) {
  return(17i32)
}

[effects(heap_alloc), return<int>]
/map/at([map<i32, i32>] values, [i32] index) {
  return(99i32)
}

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 4i32)}
  return(at(values, 1i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("bare map at_unsafe auto inference resolves through canonical helper") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/std/collections/map/at_unsafe([map<i32, i32>] values, [i32] index) {
  return(17i32)
}

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 4i32)}
  [auto] inferred{at_unsafe(values, 1i32)}
  return(inferred)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("bare map at_unsafe auto inference resolves through compatibility helper when canonical helper is absent") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/map/at_unsafe([map<i32, i32>] values, [i32] index) {
  return(17i32)
}

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 4i32)}
  [auto] inferred{at_unsafe(values, 1i32)}
  return(inferred)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("bare map at_unsafe auto inference prefers canonical helper over compatibility alias") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/std/collections/map/at_unsafe([map<i32, i32>] values, [i32] index) {
  return(17i32)
}

[effects(heap_alloc), return<int>]
/map/at_unsafe([map<i32, i32>] values, [i32] index) {
  return(99i32)
}

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 4i32)}
  [auto] inferred{at_unsafe(values, 1i32)}
  return(inferred)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("bare vector at call resolves through imported stdlib helper") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 4i32)}
  return(at(values, 1i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("bare vector at wrapper call resolves through same-path helper") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/vector/at([vector<i32>] values, [i32] index) {
  return(17i32)
}

[effects(heap_alloc), return<vector<i32>>]
wrapVector() {
  return(vector<i32>(1i32, 4i32))
}

[effects(heap_alloc), return<int>]
main() {
  return(at(wrapVector(), 1i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("bare vector at call prefers canonical helper over compatibility alias") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/std/collections/vector/at([vector<i32>] values, [i32] index) {
  return(17i32)
}

[effects(heap_alloc), return<int>]
/vector/at([vector<i32>] values, [i32] index) {
  return(99i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 4i32)}
  return(at(values, 1i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("bare vector at call rejects named arguments even through imported stdlib helper") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 4i32)}
  return(at([index] 1i32, [values] values))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK_FALSE(error.empty());
}

TEST_CASE("bare vector at call requires imported stdlib helper or explicit definition") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 4i32)}
  return(at(values, 1i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /std/collections/vector/at") != std::string::npos);
}

TEST_CASE("bare vector at auto inference resolves through imported stdlib helper") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 4i32)}
  [auto] inferred{at(values, 1i32)}
  return(inferred)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("bare vector at auto inference requires imported stdlib helper or explicit definition") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 4i32)}
  [auto] inferred{at(values, 1i32)}
  return(inferred)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /std/collections/vector/at") != std::string::npos);
}

TEST_CASE("bare vector at_unsafe auto inference resolves through imported stdlib helper") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 4i32)}
  [auto] inferred{at_unsafe(values, 1i32)}
  return(inferred)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("bare vector at_unsafe auto inference requires imported stdlib helper or explicit definition") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 4i32)}
  [auto] inferred{at_unsafe(values, 1i32)}
  return(inferred)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /std/collections/vector/at_unsafe") != std::string::npos);
}

TEST_CASE("variadic wrapped FileError packs still reject named free builtin at calls") {
  const std::string source = R"(
[return<int>]
score_refs([args<Reference<FileError>>] values) {
  [auto] head{at([values] values, [index] 0i32).why()}
  return(count(head))
}

[return<int>]
main() {
  [FileError] value{13i32}
  [Reference<FileError>] ref{location(value)}
  return(score_refs(ref))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("named arguments not supported for builtin calls") != std::string::npos);
}

TEST_CASE("variadic wrapped File handle packs accept canonical named free builtin at receivers") {
  const std::string source = R"(
[effects(file_write)]
swallow_file_error([FileError] err) {}

[return<int> effects(file_write) on_error<FileError, /swallow_file_error>]
score_refs([args<Reference<File<Write>>>] values) {
  at([values] values, [index] 0i32).write_line("alpha"utf8)?
  at([values] values, [index] minus(count(values), 1i32)).write_line("omega"utf8)?
  at([values] values, [index] 0i32).flush()?
  at([values] values, [index] minus(count(values), 1i32)).flush()?
  return(plus(count(values), 10i32))
}

[return<int> effects(file_write) on_error<FileError, /swallow_file_error>]
forward_refs([args<Reference<File<Write>>>] values) {
  return(score_refs([spread] values))
}

[return<int> effects(file_write) on_error<FileError, /swallow_file_error>]
forward_refs_mixed([args<Reference<File<Write>>>] values) {
  [File<Write>] extra{File<Write>("extra_named_ref.txt"utf8)?}
  [Reference<File<Write>>] extra_ref{location(extra)}
  return(score_refs(extra_ref, [spread] values))
}

[return<int> effects(file_write) on_error<FileError, /swallow_file_error>]
score_ptrs([args<Pointer<File<Write>>>] values) {
  at([values] values, [index] 0i32).write_line("alpha"utf8)?
  at([values] values, [index] minus(count(values), 1i32)).write_line("omega"utf8)?
  at([values] values, [index] 0i32).flush()?
  at([values] values, [index] minus(count(values), 1i32)).flush()?
  return(plus(count(values), 10i32))
}

[return<int> effects(file_write) on_error<FileError, /swallow_file_error>]
forward_ptrs([args<Pointer<File<Write>>>] values) {
  return(score_ptrs([spread] values))
}

[return<int> effects(file_write) on_error<FileError, /swallow_file_error>]
forward_ptrs_mixed([args<Pointer<File<Write>>>] values) {
  [File<Write>] extra{File<Write>("extra_named_ptr.txt"utf8)?}
  [Pointer<File<Write>>] extra_ptr{location(extra)}
  return(score_ptrs(extra_ptr, [spread] values))
}

[return<int> effects(file_write) on_error<FileError, /swallow_file_error>]
main() {
  [File<Write>] a0{File<Write>("a0_named.txt"utf8)?}
  [File<Write>] a1{File<Write>("a1_named.txt"utf8)?}
  [Reference<File<Write>>] r0{location(a0)}
  [Reference<File<Write>>] r1{location(a1)}

  [File<Write>] b0{File<Write>("b0_named.txt"utf8)?}
  [File<Write>] b1{File<Write>("b1_named.txt"utf8)?}
  [Reference<File<Write>>] s0{location(b0)}
  [Reference<File<Write>>] s1{location(b1)}

  [File<Write>] c0{File<Write>("c0_named.txt"utf8)?}
  [Reference<File<Write>>] t0{location(c0)}

  [File<Write>] d0{File<Write>("d0_named.txt"utf8)?}
  [File<Write>] d1{File<Write>("d1_named.txt"utf8)?}
  [Pointer<File<Write>>] p0{location(d0)}
  [Pointer<File<Write>>] p1{location(d1)}

  [File<Write>] e0{File<Write>("e0_named.txt"utf8)?}
  [File<Write>] e1{File<Write>("e1_named.txt"utf8)?}
  [Pointer<File<Write>>] q0{location(e0)}
  [Pointer<File<Write>>] q1{location(e1)}

  [File<Write>] f0{File<Write>("f0_named.txt"utf8)?}
  [Pointer<File<Write>>] u0{location(f0)}

  return(plus(score_refs(r0, r1),
              plus(forward_refs(s0, s1),
                   plus(forward_refs_mixed(t0),
                        plus(score_ptrs(p0, p1),
                             plus(forward_ptrs(q0, q1),
                                  forward_ptrs_mixed(u0)))))))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.empty());
}

TEST_CASE("variadic wrapped read File handle packs accept canonical named free builtin at read_byte try") {
  const std::string source = R"(
[effects(file_read)]
swallow_file_error([FileError] err) {}

[return<int> effects(file_read) on_error<FileError, /swallow_file_error>]
score_refs([args<Reference<File<Read>>>] values) {
  [i32 mut] first{0i32}
  [i32 mut] last{0i32}
  at([values] values, [index] 0i32).read_byte(first)?
  at([values] values, [index] minus(count(values), 1i32)).read_byte(last)?
  return(plus(plus(first, last), count(values)))
}

[return<int> effects(file_read) on_error<FileError, /swallow_file_error>]
forward_refs([args<Reference<File<Read>>>] values) {
  return(score_refs([spread] values))
}

[return<int> effects(file_read) on_error<FileError, /swallow_file_error>]
forward_refs_mixed([args<Reference<File<Read>>>] values) {
  [File<Read>] extra{File<Read>("extra_named_ref.txt"utf8)?}
  [Reference<File<Read>>] extra_ref{location(extra)}
  return(score_refs(extra_ref, [spread] values))
}

[return<int> effects(file_read) on_error<FileError, /swallow_file_error>]
score_ptrs([args<Pointer<File<Read>>>] values) {
  [i32 mut] first{0i32}
  [i32 mut] last{0i32}
  at([values] values, [index] 0i32).read_byte(first)?
  at([values] values, [index] minus(count(values), 1i32)).read_byte(last)?
  return(plus(plus(first, last), count(values)))
}

[return<int> effects(file_read) on_error<FileError, /swallow_file_error>]
forward_ptrs([args<Pointer<File<Read>>>] values) {
  return(score_ptrs([spread] values))
}

[return<int> effects(file_read) on_error<FileError, /swallow_file_error>]
forward_ptrs_mixed([args<Pointer<File<Read>>>] values) {
  [File<Read>] extra{File<Read>("extra_named_ptr.txt"utf8)?}
  [Pointer<File<Read>>] extra_ptr{location(extra)}
  return(score_ptrs(extra_ptr, [spread] values))
}

[return<int> effects(file_read) on_error<FileError, /swallow_file_error>]
main() {
  [File<Read>] a0{File<Read>("a0_named.txt"utf8)?}
  [File<Read>] a1{File<Read>("a1_named.txt"utf8)?}
  [Reference<File<Read>>] r0{location(a0)}
  [Reference<File<Read>>] r1{location(a1)}

  [File<Read>] b0{File<Read>("b0_named.txt"utf8)?}
  [File<Read>] b1{File<Read>("b1_named.txt"utf8)?}
  [Reference<File<Read>>] s0{location(b0)}
  [Reference<File<Read>>] s1{location(b1)}

  [File<Read>] c0{File<Read>("c0_named.txt"utf8)?}
  [Reference<File<Read>>] t0{location(c0)}

  [File<Read>] d0{File<Read>("d0_named.txt"utf8)?}
  [File<Read>] d1{File<Read>("d1_named.txt"utf8)?}
  [Pointer<File<Read>>] p0{location(d0)}
  [Pointer<File<Read>>] p1{location(d1)}

  [File<Read>] e0{File<Read>("e0_named.txt"utf8)?}
  [File<Read>] e1{File<Read>("e1_named.txt"utf8)?}
  [Pointer<File<Read>>] q0{location(e0)}
  [Pointer<File<Read>>] q1{location(e1)}

  [File<Read>] f0{File<Read>("f0_named.txt"utf8)?}
  [Pointer<File<Read>>] u0{location(f0)}

  return(plus(score_refs(r0, r1),
              plus(forward_refs(s0, s1),
                   plus(forward_refs_mixed(t0),
                        plus(score_ptrs(p0, p1),
                             plus(forward_ptrs(q0, q1),
                                  forward_ptrs_mixed(u0)))))))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.empty());
}

TEST_CASE("canonical map helper bodies reject conflicting imported count alias") {
  const std::string source = R"(
import /util

namespace util {
  [public effects(heap_alloc), return<int>]
  count([map<i32, i32>] values) {
    return(99i32)
  }
}

[effects(heap_alloc), return<int>]
/std/collections/map/count([map<i32, i32>] values) {
  return(17i32)
}

[effects(heap_alloc), return<int>]
/std/collections/map/size([map<i32, i32>] values) {
  return(count(values))
}

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 4i32)}
  return(/std/collections/map/size(values))
}
  )";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("import creates name conflict: count") != std::string::npos);
}

TEST_CASE("canonical map helper bodies keep import conflict ahead of missing canonical count fallback") {
  const std::string source = R"(
import /util

namespace util {
  [public effects(heap_alloc), return<int>]
  count([map<i32, i32>] values) {
    return(99i32)
  }
}

[effects(heap_alloc), return<int>]
/std/collections/map/size([map<i32, i32>] values) {
  return(count(values))
}

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 4i32)}
  return(/std/collections/map/size(values))
}
  )";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("import creates name conflict: count") != std::string::npos);
}

TEST_CASE("canonical map helper bodies keep import conflict ahead of missing canonical access fallback") {
  const std::string source = R"(
import /util

namespace util {
  [public effects(heap_alloc), return<int>]
  at([map<i32, i32>] values, [i32] key) {
    return(99i32)
  }
}

[effects(heap_alloc), return<int>]
/std/collections/map/first([map<i32, i32>] values) {
  return(at(values, 1i32))
}

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 4i32)}
  return(/std/collections/map/first(values))
}
  )";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK_FALSE(error.empty());
}

TEST_CASE("bare map at call validates without imported canonical helper") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 4i32)}
  return(at(values, 1i32))
}
  )";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("bare map at_unsafe auto inference stays stable") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 4i32)}
  [auto] inferred{at_unsafe(values, 1i32)}
  return(inferred)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("map namespaced count method keeps slash-path rejection diagnostics") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/std/collections/map/count([map<i32, i32>] values) {
  return(17i32)
}

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 4i32)}
  return(values./map/count())
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown method: /map/count") != std::string::npos);
}

TEST_CASE("map stdlib namespaced count method keeps slash-path rejection diagnostics") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/std/collections/map/count([map<i32, i32>] values) {
  return(17i32)
}

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 4i32)}
  return(values./std/collections/map/count())
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown method: /map/count") != std::string::npos);
}

TEST_CASE("map stdlib namespaced count method auto inference keeps stable slash-path diagnostics") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/std/collections/map/count([map<i32, i32>] values) {
  return(17i32)
}

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 4i32)}
  [auto] inferred{values./std/collections/map/count()}
  return(inferred)
}
  )";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK_FALSE(error.empty());
}

TEST_CASE("bare map count method validates without imported canonical helper") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 4i32)}
  return(values.count())
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("bare map count method auto inference stays stable") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 4i32)}
  [auto] inferred{values.count()}
  return(inferred)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("bare map contains method validates without imported canonical helper") {
  const std::string source = R"(
[effects(heap_alloc), return<bool>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 4i32)}
  return(values.contains(1i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("bare map tryAt method auto inference keeps deterministic unknown method diagnostics") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 4i32)}
  [auto] inferred{values.tryAt(1i32)}
  return(try(inferred))
}
  )";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK_FALSE(error.empty());
}

TEST_CASE("bare map access methods validate without imported canonical helpers") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 4i32, 2i32, 5i32)}
  return(plus(values.at(1i32), values.at_unsafe(2i32)))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("bare map access method auto inference stays stable") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 4i32)}
  [auto] inferred{values.at_unsafe(1i32)}
  return(inferred)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("map namespaced at method keeps slash-path rejection diagnostics") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/std/collections/map/at([map<i32, i32>] values, [i32] index) {
  return(17i32)
}

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 4i32)}
  return(values./map/at(1i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown method: /map/at") != std::string::npos);
}

TEST_CASE("map namespaced at_unsafe method auto inference keeps slash-path rejection diagnostics") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/std/collections/map/at_unsafe([map<i32, i32>] values, [i32] index) {
  return(17i32)
}

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 4i32)}
  [auto] inferred{values./map/at_unsafe(1i32)}
  return(inferred)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown method: /map/at_unsafe") != std::string::npos);
}

TEST_CASE("map stdlib namespaced at_unsafe method keeps slash-path rejection diagnostics") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/std/collections/map/at_unsafe([map<i32, i32>] values, [i32] index) {
  return(17i32)
}

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 4i32)}
  return(values./std/collections/map/at_unsafe(1i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown method: /map/at_unsafe") != std::string::npos);
}

TEST_CASE("stdlib canonical map contains and tryAt helpers resolve in method-call sugar") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<bool>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 4i32)}
  [Result<i32, ContainerError>] found{values.tryAt(1i32)}
  return(values.contains(1i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("stdlib canonical map helpers resolve in method-call sugar") {
  const std::string source = R"(
[return<int>]
/std/collections/map/count([map<i32, i32>] values, [bool] marker) {
  return(91i32)
}

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 4i32)}
  return(values.count(true))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("stdlib canonical map helper resolves method-call sugar for slash return type") {
  const std::string source = R"(
[return<int>]
/std/collections/map/count([map<i32, i32>] values) {
  return(73i32)
}

[effects(heap_alloc), return</std/collections/map<i32, i32>>]
makeValues() {
  return(map<i32, i32>(1i32, 4i32))
}

[effects(heap_alloc), return<int>]
main() {
  return(makeValues().count())
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("wrapper-returned canonical map method falls back to alias helper when canonical helper is absent") {
  const std::string source = R"(
[return<int>]
/map/count([map<i32, i32>] values) {
  return(71i32)
}

[effects(heap_alloc), return</std/collections/map<i32, i32>>]
makeValues() {
  return(map<i32, i32>(1i32, 4i32))
}

[effects(heap_alloc), return<int>]
main() {
  return(makeValues().count())
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("wrapper-returned canonical map method validates builtin wrapper receiver without helpers") {
  const std::string source = R"(
[effects(heap_alloc), return</std/collections/map<i32, i32>>]
makeValues() {
  return(map<i32, i32>(1i32, 4i32))
}

[effects(heap_alloc), return<int>]
main() {
  return(makeValues().count())
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("stdlib canonical map slash return type keeps canonical method diagnostics") {
  const std::string source = R"(
[return<int>]
/std/collections/map/count([map<i32, i32>] values, [bool] marker) {
  return(73i32)
}

[effects(heap_alloc), return</std/collections/map<i32, i32>>]
makeValues() {
  return(map<i32, i32>(1i32, 4i32))
}

[effects(heap_alloc), return<int>]
main() {
  return(makeValues().count())
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("argument count mismatch for /std/collections/map/count") != std::string::npos);
}

TEST_CASE("stdlib canonical map count method auto inference falls back to canonical helper return") {
  const std::string source = R"(
[return<bool>]
/std/collections/map/count([map<i32, i32>] values) {
  return(false)
}

[effects(heap_alloc), return<bool>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 4i32)}
  [auto] inferred{values.count()}
  return(inferred)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("stdlib canonical map count method auto inference keeps return mismatch diagnostics") {
  const std::string source = R"(
[return<bool>]
/std/collections/map/count([map<i32, i32>] values) {
  return(false)
}

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 4i32)}
  [auto] inferred{values.count()}
  return(inferred)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("return type mismatch") != std::string::npos);
  CHECK(error.find("expected i32") != std::string::npos);
}

TEST_CASE("stdlib canonical map count method auto inference keeps canonical precedence over alias helper") {
  const std::string source = R"(
[return<int>]
/map/count([map<i32, i32>] values) {
  return(41i32)
}

[return<bool>]
/std/collections/map/count([map<i32, i32>] values) {
  return(false)
}

[effects(heap_alloc), return<bool>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 4i32)}
  [auto] inferred{values.count()}
  return(inferred)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("stdlib canonical map count method auto inference keeps canonical mismatch diagnostics over alias helper") {
  const std::string source = R"(
[return<int>]
/map/count([map<i32, i32>] values) {
  return(41i32)
}

[return<bool>]
/std/collections/map/count([map<i32, i32>] values) {
  return(false)
}

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 4i32)}
  [auto] inferred{values.count()}
  return(inferred)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("return type mismatch") != std::string::npos);
  CHECK(error.find("expected i32") != std::string::npos);
}

TEST_CASE("stdlib canonical map count call auto inference keeps canonical precedence over alias helper") {
  const std::string source = R"(
[return<int>]
/map/count([map<i32, i32>] values) {
  return(41i32)
}

[return<bool>]
/std/collections/map/count([map<i32, i32>] values) {
  return(false)
}

[effects(heap_alloc), return<bool>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 4i32)}
  [auto] inferred{count(values)}
  return(inferred)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("stdlib canonical map count call auto inference keeps canonical mismatch diagnostics over alias helper") {
  const std::string source = R"(
[return<int>]
/map/count([map<i32, i32>] values) {
  return(41i32)
}

[return<bool>]
/std/collections/map/count([map<i32, i32>] values) {
  return(false)
}

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 4i32)}
  [auto] inferred{count(values)}
  return(inferred)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("return type mismatch") != std::string::npos);
  CHECK(error.find("expected i32") != std::string::npos);
}

TEST_CASE("stdlib canonical map access count shadow keeps canonical precedence over alias helper") {
  const std::string source = R"(
[return<int>]
/map/at([map<i32, string>] values, [i32] key) {
  return(41i32)
}

[return<string>]
/std/collections/map/at([map<i32, string>] values, [i32] key) {
  return("hello"utf8)
}

[return<int>]
/map/at_unsafe([map<i32, string>] values, [i32] key) {
  return(42i32)
}

[return<string>]
/std/collections/map/at_unsafe([map<i32, string>] values, [i32] key) {
  return("hello"utf8)
}

[return<int>]
/string/count([string] values) {
  return(5i32)
}

[effects(heap_alloc), return<int>]
main() {
  [map<i32, string>] values{map<i32, string>(1i32, "value"utf8)}
  return(plus(values.at(1i32).count(), values.at_unsafe(1i32).count()))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("stdlib canonical map access count shadow currently validates mixed canonical and alias returns") {
  const std::string source = R"(
[return<string>]
/map/at([map<i32, string>] values, [i32] key) {
  return("hello"utf8)
}

[return<int>]
/std/collections/map/at([map<i32, string>] values, [i32] key) {
  return(41i32)
}

[return<string>]
/map/at_unsafe([map<i32, string>] values, [i32] key) {
  return("hello"utf8)
}

[return<int>]
/std/collections/map/at_unsafe([map<i32, string>] values, [i32] key) {
  return(42i32)
}

[return<int>]
/string/count([string] values) {
  return(5i32)
}

[effects(heap_alloc), return<int>]
main() {
  [map<i32, string>] values{map<i32, string>(1i32, "value"utf8)}
  return(plus(values.at(1i32).count(), values.at_unsafe(1i32).count()))
}
  )";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("rejects stdlib canonical vector helper method-precedence forwarding in method-call sugar") {
  const std::string source = R"(
[return<int>]
/std/collections/vector/count([vector<i32>] values, [bool] marker) {
  return(90i32)
}

[return<int>]
/std/collections/vector/at([vector<i32>] values, [bool] index) {
  return(40i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  return(plus(values.count(true), values.at(true)))
}
  )";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK_FALSE(error.empty());
}

TEST_CASE("array namespaced vector helper alias rejects method-call sugar auto inference") {
  const std::string source = R"(
[return<bool>]
/std/collections/vector/count([vector<i32>] values, [bool] marker) {
  return(false)
}

[effects(heap_alloc), return<bool>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  [auto] inferred{values./array/count(true)}
  return(inferred)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("argument count mismatch for builtin count") != std::string::npos);
}

TEST_CASE("array namespaced vector helper alias method-call inference keeps unknown-method diagnostics") {
  const std::string source = R"(
[return<bool>]
/std/collections/vector/count([vector<i32>] values, [bool] marker) {
  return(false)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  [auto] inferred{values./array/count(true)}
  return(inferred)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("argument count mismatch for builtin count") != std::string::npos);
}

TEST_CASE("vector namespaced helper alias rejects method-call sugar auto inference") {
  const std::string source = R"(
[return<bool>]
/std/collections/vector/count([vector<i32>] values, [bool] marker) {
  return(false)
}

[effects(heap_alloc), return<bool>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  [auto] inferred{values./vector/count(true)}
  return(inferred)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("argument count mismatch for builtin count") != std::string::npos);
}

TEST_CASE("vector namespaced capacity alias rejects method-call sugar auto inference") {
  const std::string source = R"(
[return<bool>]
/std/collections/vector/capacity([vector<i32>] values, [bool] marker) {
  return(false)
}

[effects(heap_alloc), return<bool>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  [auto] inferred{values./vector/capacity(true)}
  return(inferred)
}
  )";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK_FALSE(error.empty());
}

TEST_CASE("stdlib namespaced vector helper alias rejects method-call sugar auto inference") {
  const std::string source = R"(
[return<bool>]
/std/collections/vector/count([vector<i32>] values, [bool] marker) {
  return(false)
}

[effects(heap_alloc), return<bool>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  [auto] inferred{values./std/collections/vector/count(true)}
  return(inferred)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("argument count mismatch for builtin count") != std::string::npos);
}

TEST_CASE("stdlib namespaced vector access alias rejects method-call sugar auto inference") {
  const std::string source = R"(
[return<bool>]
/std/collections/vector/at([vector<i32>] values, [bool] index) {
  return(false)
}

[effects(heap_alloc), return<bool>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  [auto] inferred{values./std/collections/vector/at(true)}
  return(inferred)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("at requires integer index") != std::string::npos);
}

TEST_CASE("stdlib namespaced vector helper alias method-call inference keeps unknown-method diagnostics") {
  const std::string source = R"(
[return<bool>]
/std/collections/vector/count([vector<i32>] values, [bool] marker) {
  return(false)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  [auto] inferred{values./std/collections/vector/count(true)}
  return(inferred)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("argument count mismatch for builtin count") != std::string::npos);
}

TEST_CASE("stdlib namespaced vector count method local same-path overload set rejects duplicate definitions") {
  const std::string source = R"(
[return<int>]
/std/collections/vector/count([map<i32, i32>] values) {
  return(31i32)
}

[return<int>]
/std/collections/vector/count([array<i32>] values) {
  return(32i32)
}

[return<int>]
/std/collections/vector/count([string] values) {
  return(33i32)
}

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 2i32)}
  [array<i32>] items{array<i32>(1i32, 2i32, 3i32)}
  [string] text{"abc"raw_utf8}
  return(plus(plus(values./std/collections/vector/count(),
                    items./std/collections/vector/count()),
              text./std/collections/vector/count()))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("duplicate definition: /std/collections/vector/count") != std::string::npos);
}

TEST_CASE("stdlib namespaced vector count method rejects map receiver without helper") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 2i32)}
  return(values./std/collections/vector/count())
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown method: /std/collections/vector/count") != std::string::npos);
}

TEST_CASE("stdlib namespaced vector count method rejects array receiver without helper") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [array<i32>] values{array<i32>(1i32, 2i32, 3i32)}
  return(values./std/collections/vector/count())
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown method: /std/collections/vector/count") != std::string::npos);
}

TEST_CASE("stdlib namespaced vector count method rejects string receiver without helper") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [string] value{"abc"raw_utf8}
  return(value./std/collections/vector/count())
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown method: /std/collections/vector/count") != std::string::npos);
}

TEST_CASE("stdlib namespaced vector capacity method local same-path overload set rejects duplicate definitions") {
  const std::string source = R"(
[return<int>]
/std/collections/vector/capacity([map<i32, i32>] values) {
  return(41i32)
}

[return<int>]
/std/collections/vector/capacity([array<i32>] values) {
  return(42i32)
}

[return<int>]
/std/collections/vector/capacity([string] values) {
  return(43i32)
}

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 2i32)}
  [array<i32>] items{array<i32>(1i32, 2i32, 3i32)}
  [string] text{"abc"raw_utf8}
  return(plus(plus(values./std/collections/vector/capacity(),
                    items./std/collections/vector/capacity()),
              text./std/collections/vector/capacity()))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("duplicate definition: /std/collections/vector/capacity") != std::string::npos);
}

TEST_CASE("stdlib namespaced vector capacity method rejects map receiver without helper") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 2i32)}
  return(values./std/collections/vector/capacity())
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown method: /std/collections/vector/capacity") != std::string::npos);
}

TEST_CASE("stdlib namespaced vector capacity method rejects array receiver without helper") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [array<i32>] values{array<i32>(1i32, 2i32, 3i32)}
  return(values./std/collections/vector/capacity())
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown method: /std/collections/vector/capacity") != std::string::npos);
}

TEST_CASE("stdlib namespaced vector capacity method rejects string receiver without helper") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [string] value{"abc"raw_utf8}
  return(value./std/collections/vector/capacity())
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown method: /std/collections/vector/capacity") != std::string::npos);
}

TEST_CASE("stdlib namespaced vector capacity method rejects wrapper map receiver without helper") {
  const std::string source = R"(
[return<map<i32, i32>>]
wrapMap() {
  return(map<i32, i32>(1i32, 2i32))
}

[return<int>]
main() {
  return(wrapMap()./std/collections/vector/capacity())
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown method: /std/collections/vector/capacity") != std::string::npos);
}

TEST_CASE("vector namespaced capacity method rejects wrapper map receiver without helper") {
  const std::string source = R"(
[return<map<i32, i32>>]
wrapMap() {
  return(map<i32, i32>(1i32, 2i32))
}

[return<int>]
main() {
  return(wrapMap()./vector/capacity())
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown method: /vector/capacity") != std::string::npos);
}

TEST_CASE("array namespaced slash method spelling rejects statement body arguments") {
  const std::string source = R"(
[return<int>]
/std/collections/vector/count([vector<i32>] values, [bool] marker) {
  return(90i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  values./array/count(true) { 1i32 }
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("block arguments require a definition target") != std::string::npos);
}

TEST_CASE("stdlib namespaced vector helper alias rejects statement body arguments") {
  const std::string source = R"(
[return<int>]
/std/collections/vector/count([vector<i32>] values, [bool] marker) {
  return(90i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  values./std/collections/vector/count(true) { 1i32 }
  return(0i32)
  }
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("block arguments require a definition target") != std::string::npos);
}

TEST_CASE("array namespaced slash method spelling block-arg diagnostics keep divide target") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  values./array/count(true) { 1i32 }
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("block arguments require a definition target") != std::string::npos);
}

TEST_CASE("array namespaced vector helper call form rejects statement body arguments") {
  const std::string source = R"(
[return<int>]
/std/collections/vector/count([vector<i32>] values, [bool] marker) {
  return(90i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  /array/count(values, true) { 1i32 }
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("block arguments require a definition target: /array/count") != std::string::npos);
}

TEST_CASE("vector namespaced helper call form rejects statement body arguments") {
  const std::string source = R"(
[return<int>]
/std/collections/vector/count([vector<i32>] values, [bool] marker) {
  return(90i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  /vector/count(values, true) { 1i32 }
  return(0i32)
  }
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("block arguments require a definition target: /vector/count") != std::string::npos);
}

TEST_CASE("stdlib canonical vector helper call form rejects compatibility fallback for statement body arguments") {
  const std::string source = R"(
[return<int>]
/vector/count([vector<i32>] values, [bool] marker) {
  return(90i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  /std/collections/vector/count(values, true) { 1i32 }
  return(0i32)
  }
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("block arguments require a definition target: /std/collections/vector/count") !=
        std::string::npos);
}

TEST_CASE("array namespaced vector helper call form rejects expression body arguments") {
  const std::string source = R"(
[return<int>]
/std/collections/vector/count([vector<i32>] values, [bool] marker) {
  return(90i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  return(/array/count(values, true) { 1i32 })
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /array/count") != std::string::npos);
}

TEST_CASE("vector namespaced helper call form expression body arguments currently pass semantics") {
  const std::string source = R"(
[return<int>]
/std/collections/vector/count([vector<i32>] values, [bool] marker) {
  return(90i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  return(/vector/count(values, true) { 1i32 })
  }
  )";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /vector/count") != std::string::npos);
}

TEST_CASE("stdlib canonical vector helper call-form expression body arguments reject compatibility fallback") {
  const std::string source = R"(
[return<bool>]
/vector/count([vector<i32>] values, [bool] marker) {
  return(false)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  return(/std/collections/vector/count(values, true) { 1i32 })
  }
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /std/collections/vector/count") != std::string::npos);
}

TEST_CASE("array namespaced slash method pointer receiver diagnostics keep divide target") {
  const std::string source = R"(
[return<int>]
main() {
  [i32] value{1i32}
  [Pointer<i32>] ptr{location(value)}
  ptr./array/count(true) { 1i32 }
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("block arguments require a definition target") != std::string::npos);
}

TEST_CASE("stdlib namespaced method body-arg diagnostics normalize pointer receiver target") {
  const std::string source = R"(
[return<int>]
main() {
  [i32] value{1i32}
  [Pointer<i32>] ptr{location(value)}
  ptr./std/collections/vector/count(true) { 1i32 }
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("block arguments require a definition target: /Pointer/count") != std::string::npos);
}

TEST_CASE("array namespaced method expression body-arg diagnostics normalize pointer receiver target") {
  const std::string source = R"(
[return<int>]
main() {
  [i32] value{1i32}
  [Pointer<i32>] ptr{location(value)}
  return(ptr./array/count(true) { 1i32 })
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("block arguments require a definition target: /Pointer/count") != std::string::npos);
}

TEST_CASE("stdlib namespaced method expression body-arg diagnostics normalize pointer receiver target") {
  const std::string source = R"(
[return<int>]
main() {
  [i32] value{1i32}
  [Pointer<i32>] ptr{location(value)}
  return(ptr./std/collections/vector/count(true) { 1i32 })
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("block arguments require a definition target: /Pointer/count") != std::string::npos);
}

TEST_CASE("array namespaced slash method reference receiver diagnostics keep divide target") {
  const std::string source = R"(
[return<int>]
main() {
  [i32] value{1i32}
  [Reference<i32>] ref{location(value)}
  ref./array/count(true) { 1i32 }
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("block arguments require a definition target") != std::string::npos);
}

TEST_CASE("stdlib namespaced method expression body-arg diagnostics normalize reference receiver target") {
  const std::string source = R"(
[return<int>]
main() {
  [i32] value{1i32}
  [Reference<i32>] ref{location(value)}
  return(ref./std/collections/vector/count(true) { 1i32 })
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("block arguments require a definition target: /Reference/count") != std::string::npos);
}

TEST_CASE("array namespaced slash method temporary pointer diagnostics keep divide target") {
  const std::string source = R"(
[return<int>]
main() {
  [i32] value{1i32}
  location(value)./array/count(true) { 1i32 }
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("block arguments require a definition target") != std::string::npos);
}

TEST_CASE("stdlib namespaced method expression body-arg diagnostics normalize temporary pointer receiver target") {
  const std::string source = R"(
[return<int>]
main() {
  [i32] value{1i32}
  return(location(value)./std/collections/vector/count(true) { 1i32 })
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("block arguments require a definition target: /Pointer/count") != std::string::npos);
}

TEST_CASE("array namespaced method body-arg diagnostics normalize helper-returned reference receiver target") {
  const std::string source = R"(
[return<Reference<i32>>]
borrow([Reference<i32>] ref) {
  return(ref)
}

[return<int>]
main() {
  [i32] value{1i32}
  borrow(location(value)).count(true) { 1i32 }
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("block arguments require a definition target: /Reference/count") != std::string::npos);
}

TEST_CASE("stdlib namespaced method expression body-arg diagnostics normalize helper-returned reference receiver target") {
  const std::string source = R"(
[return<Reference<i32>>]
borrow([Reference<i32>] ref) {
  return(ref)
}

[return<int>]
main() {
  [i32] value{1i32}
  return(borrow(location(value)).count(true) { 1i32 })
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("block arguments require a definition target: /Reference/count") != std::string::npos);
}

TEST_CASE("array namespaced method body-arg diagnostics normalize canonical-fallback helper receiver target") {
  const std::string source = R"(
[return<Reference<i32>>]
/std/collections/vector/borrow([Reference<i32>] ref) {
  return(ref)
}

[return<int>]
main() {
  [i32] value{1i32}
  /vector/borrow(location(value)).count(true) { 1i32 }
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("block arguments require a definition target: /Reference/count") != std::string::npos);
}

TEST_CASE("stdlib namespaced method expression body-arg diagnostics normalize canonical-fallback helper receiver target") {
  const std::string source = R"(
[return<Reference<i32>>]
/std/collections/vector/borrow([Reference<i32>] ref) {
  return(ref)
}

[return<int>]
main() {
  [i32] value{1i32}
  return(/vector/borrow(location(value)).count(true) { 1i32 })
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("block arguments require a definition target: /Reference/count") != std::string::npos);
}

TEST_CASE("array namespaced method expression body-arg infers helper-returned reference target") {
  const std::string source = R"(
[return<Reference<i32>>]
/std/collections/vector/borrow([Reference<i32>] ref) {
  return(ref)
}

[return<int>]
/Reference/count([Reference<i32>] self, [bool] marker) {
  return(7i32)
}

[return<int>]
main() {
  [i32] value{1i32}
  return(/vector/borrow(location(value)).count(true) { 1i32 })
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("array namespaced method expression body-arg helper-returned reference keeps canonical diagnostics") {
  const std::string source = R"(
[return<Reference<i32>>]
/std/collections/vector/borrow([Reference<i32>] ref) {
  return(ref)
}

[return<int>]
/Reference/count([Reference<i32>] self, [i32] marker) {
  return(7i32)
}

[return<int>]
main() {
  [i32] value{1i32}
  return(/vector/borrow(location(value)).count(true) { 1i32 })
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("argument type mismatch for /Reference/count parameter marker") != std::string::npos);
}

TEST_CASE("map method expression body-arg infers canonical helper on referenced wrapper receiver") {
  const std::string source = R"(
[return<Reference</std/collections/map<i32, i32>>>]
borrowMap([Reference</std/collections/map<i32, i32>>] values) {
  return(values)
}

[return<int>]
/std/collections/map/count([Reference</std/collections/map<i32, i32>>] values, [bool] marker) {
  return(7i32)
}

[return<bool>]
/Reference/count([Reference</std/collections/map<i32, i32>>] self, [bool] marker) {
  return(false)
}

[return<int>]
main() {
  [/std/collections/map<i32, i32>] values{map<i32, i32>(1i32, 4i32)}
  return(borrowMap(location(values)).count(true) { 1i32 })
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("map method expression body-arg referenced wrapper keeps canonical diagnostics") {
  const std::string source = R"(
[return<Reference</std/collections/map<i32, i32>>>]
borrowMap([Reference</std/collections/map<i32, i32>>] values) {
  return(values)
}

[return<int>]
/std/collections/map/count([Reference</std/collections/map<i32, i32>>] values, [i32] marker) {
  return(7i32)
}

[return<int>]
/Reference/count([Reference</std/collections/map<i32, i32>>] self, [bool] marker) {
  return(9i32)
}

[return<int>]
main() {
  [/std/collections/map<i32, i32>] values{map<i32, i32>(1i32, 4i32)}
  return(borrowMap(location(values)).count(true) { 1i32 })
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("argument type mismatch for /std/collections/map/count parameter marker") !=
        std::string::npos);
}

TEST_CASE("templated canonical map count wrapper method sugar rejects without explicit alias") {
  const std::string source = R"(
[effects(heap_alloc), return</std/collections/map<i32, i32>>]
wrapValues() {
  return(map<i32, i32>(1i32, 2i32))
}

[return<int>]
/std/collections/map/count<K, V>([map<K, V>] values, [bool] marker) {
  return(7i32)
}

[return<int>]
main() {
  return(wrapValues().count(1i32))
}
  )";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK_FALSE(error.empty());
}

TEST_CASE("bare map helper statement body arguments still validate") {
  const std::string source = R"(
[return<int>]
/map/count([map<i32, i32>] values, [bool] marker) {
  return(90i32)
}

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(5i32, 6i32)}
  values.count(true) { 1i32 }
  return(0i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("bare map helper statement body arguments fall back to canonical helper target") {
  const std::string source = R"(
[return<int>]
/std/collections/map/count([map<i32, i32>] values, [bool] marker) {
  return(90i32)
}

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(5i32, 6i32)}
  values.count(true) { 1i32 }
  return(0i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("bare map helper statement body arguments keep canonical mismatch diagnostics") {
  const std::string source = R"(
[return<int>]
/std/collections/map/count([map<i32, i32>] values) {
  return(90i32)
}

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(5i32, 6i32)}
  values.count(true) { 1i32 }
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("argument count mismatch for /std/collections/map/count") != std::string::npos);
}

TEST_CASE("wrapper-returned bare map helper statement body arguments fall back to canonical helper target") {
  const std::string source = R"(
[return<map<i32, i32>>]
wrapValues() {
  return(map<i32, i32>(5i32, 6i32))
}

[return<int>]
/std/collections/map/count([map<i32, i32>] values, [bool] marker) {
  return(90i32)
}

[effects(heap_alloc), return<int>]
main() {
  wrapValues().count(true) { 1i32 }
  return(0i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("wrapper-returned bare map helper statement body arguments keep canonical mismatch diagnostics") {
  const std::string source = R"(
[return<map<i32, i32>>]
wrapValues() {
  return(map<i32, i32>(5i32, 6i32))
}

[return<int>]
/std/collections/map/count([map<i32, i32>] values) {
  return(90i32)
}

[effects(heap_alloc), return<int>]
main() {
  wrapValues().count(true) { 1i32 }
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("argument count mismatch for /std/collections/map/count") != std::string::npos);
}

TEST_CASE("reference-wrapped map helper receiver statement body arguments fall back to canonical helper target") {
  const std::string source = R"(
[return<Reference</std/collections/map<i32, i32>>>]
borrowMap([Reference</std/collections/map<i32, i32>>] values) {
  return(values)
}

[return<int>]
/std/collections/map/count([Reference</std/collections/map<i32, i32>>] values, [bool] marker) {
  return(90i32)
}

[effects(heap_alloc), return<int>]
main() {
  [/std/collections/map<i32, i32>] values{map<i32, i32>(5i32, 6i32)}
  borrowMap(location(values)).count(true) { 1i32 }
  return(0i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("reference-wrapped map helper receiver statement body arguments keep canonical mismatch diagnostics") {
  const std::string source = R"(
[return<Reference</std/collections/map<i32, i32>>>]
borrowMap([Reference</std/collections/map<i32, i32>>] values) {
  return(values)
}

[return<int>]
/std/collections/map/count([Reference</std/collections/map<i32, i32>>] values) {
  return(90i32)
}

[effects(heap_alloc), return<int>]
main() {
  [/std/collections/map<i32, i32>] values{map<i32, i32>(5i32, 6i32)}
  borrowMap(location(values)).count(true) { 1i32 }
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("argument count mismatch for /std/collections/map/count") != std::string::npos);
}

TEST_CASE("map namespaced count method statement body arguments keep slash-path diagnostics") {
  const std::string source = R"(
[return<int>]
/std/collections/map/count([map<i32, i32>] values, [bool] marker) {
  return(90i32)
}

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(5i32, 6i32)}
  values./std/collections/map/count(true) { 1i32 }
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("block arguments require a definition target: /map/count") != std::string::npos);
}

TEST_CASE("map namespaced at method statement body arguments keep slash-path diagnostics") {
  const std::string source = R"(
[return<int>]
/std/collections/map/at([map<i32, i32>] values, [i32] key) {
  return(90i32)
}

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(5i32, 6i32)}
  values./map/at(5i32) { 1i32 }
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("block arguments require a definition target: /map/at") != std::string::npos);
}

TEST_CASE("map stdlib call form statement body arguments use canonical helper target") {
  const std::string source = R"(
[return<int>]
/std/collections/map/at_unsafe([map<i32, i32>] values, [i32] key) {
  return(90i32)
}

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(5i32, 6i32)}
  /std/collections/map/at_unsafe(values, 5i32) { 1i32 }
  return(0i32)
}
  )";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("bare map call form statement body arguments fall back to canonical helper target") {
  const std::string source = R"(
[return<int>]
/std/collections/map/count([map<i32, i32>] values, [bool] marker) {
  return(90i32)
}

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(5i32, 6i32)}
  count(values, true) { 1i32 }
  return(0i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("bare map call form statement body arguments keep canonical mismatch diagnostics") {
  const std::string source = R"(
[return<int>]
/std/collections/map/count([map<i32, i32>] values) {
  return(90i32)
}

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(5i32, 6i32)}
  count(values, true) { 1i32 }
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("argument count mismatch for /std/collections/map/count") != std::string::npos);
}

TEST_CASE("bare map helper expression body arguments still validate") {
  const std::string source = R"(
[return<int>]
/map/count([map<i32, i32>] values, [bool] marker) {
  return(90i32)
}

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(5i32, 6i32)}
  return(values.count(true) { 1i32 })
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("bare map call form expression body arguments fall back to canonical helper target") {
  const std::string source = R"(
[return<int>]
/std/collections/map/count([map<i32, i32>] values, [bool] marker) {
  return(90i32)
}

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(5i32, 6i32)}
  return(count(values, true) { 1i32 })
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("bare map call form expression body arguments keep canonical mismatch diagnostics") {
  const std::string source = R"(
[return<int>]
/std/collections/map/count([map<i32, i32>] values) {
  return(90i32)
}

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(5i32, 6i32)}
  return(count(values, true) { 1i32 })
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("argument count mismatch for /std/collections/map/count") != std::string::npos);
}

TEST_CASE("reference-wrapped map helper receiver expression body arguments fall back to canonical helper target") {
  const std::string source = R"(
[return<Reference</std/collections/map<i32, i32>>>]
borrowMap([Reference</std/collections/map<i32, i32>>] values) {
  return(values)
}

[return<int>]
/std/collections/map/count([Reference</std/collections/map<i32, i32>>] values, [bool] marker) {
  return(90i32)
}

[effects(heap_alloc), return<int>]
main() {
  [/std/collections/map<i32, i32>] values{map<i32, i32>(5i32, 6i32)}
  return(borrowMap(location(values)).count(true) { 1i32 })
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("reference-wrapped map helper receiver expression body arguments keep canonical mismatch diagnostics") {
  const std::string source = R"(
[return<Reference</std/collections/map<i32, i32>>>]
borrowMap([Reference</std/collections/map<i32, i32>>] values) {
  return(values)
}

[return<int>]
/std/collections/map/count([Reference</std/collections/map<i32, i32>>] values) {
  return(90i32)
}

[effects(heap_alloc), return<int>]
main() {
  [/std/collections/map<i32, i32>] values{map<i32, i32>(5i32, 6i32)}
  return(borrowMap(location(values)).count(true) { 1i32 })
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("argument count mismatch for /std/collections/map/count") != std::string::npos);
}

TEST_CASE("map namespaced count method expression body arguments keep slash-path diagnostics") {
  const std::string source = R"(
[return<int>]
/std/collections/map/count([map<i32, i32>] values, [bool] marker) {
  return(90i32)
}

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(5i32, 6i32)}
  return(values./std/collections/map/count(true) { 1i32 })
}
  )";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK_FALSE(error.empty());
}

TEST_CASE("map namespaced at method expression body arguments keep slash-path diagnostics") {
  const std::string source = R"(
[return<int>]
/std/collections/map/at([map<i32, i32>] values, [i32] key) {
  return(90i32)
}

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(5i32, 6i32)}
  return(values./map/at(5i32) { 1i32 })
}
  )";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK_FALSE(error.empty());
}

TEST_CASE("map stdlib call form expression body arguments use canonical helper target") {
  const std::string source = R"(
[return<int>]
/std/collections/map/at_unsafe([map<i32, i32>] values, [i32] key) {
  return(90i32)
}

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(5i32, 6i32)}
  return(/std/collections/map/at_unsafe(values, 5i32) { 1i32 })
}
  )";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("vector namespaced access alias chained method rejects canonical struct-return forwarding") {
  const std::string source = R"(
Marker {
  [i32] value
}

[return<Marker>]
/std/collections/vector/at([vector<i32>] values, [i32] index) {
  return(Marker(index))
}

[return<int>]
/Marker/tag([Marker] self) {
  return(self.value)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  return(/vector/at(values, 2i32).tag())
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown method: /i32/tag") != std::string::npos);
}

TEST_CASE("vector namespaced access alias chained method keeps removed-alias diagnostics") {
  const std::string source = R"(
Marker {
  [i32] value
}

[return<Marker>]
/std/collections/vector/at([vector<i32>] values, [i32] index) {
  return(Marker(index))
}

[return<int>]
/i32/tag([i32] self, [bool] marker) {
  return(self)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  return(/vector/at(values, 2i32).tag(1i32))
}
  )";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /vector/at") != std::string::npos);
}

TEST_CASE("vector namespaced access alias field expression keeps struct receiver diagnostics") {
  const std::string source = R"(
Marker {
  [i32] value
}

[return<Marker>]
/std/collections/vector/at([vector<i32>] values, [i32] index) {
  return(Marker(index))
}

[return<auto>]
project([vector<i32>] values) {
  return(/vector/at(values, 2i32).value)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  return(project(values))
}
  )";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /vector/at") != std::string::npos);
}

TEST_CASE("vector canonical access call keeps primitive receiver diagnostics") {
  const std::string source = R"(
Marker {
  [i32] value
}

[return<Marker>]
/std/collections/vector/at([vector<i32>] values, [i32] index) {
  return(Marker(index))
}

[return<int>]
/Marker/tag([Marker] self) {
  return(self.value)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  return(/std/collections/vector/at(values, 2i32).tag())
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown method: /i32/tag") != std::string::npos);
}

TEST_CASE("vector canonical unsafe access call forwards field inference") {
  const std::string source = R"(
Marker {
  [i32] value
}

[return<Marker>]
/std/collections/vector/at_unsafe([vector<i32>] values, [i32] index) {
  return(Marker(index))
}

[return<auto>]
project([vector<i32>] values) {
  return(/std/collections/vector/at_unsafe(values, 2i32).value)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  return(project(values))
}
  )";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("map namespaced access call keeps canonical struct-return forwarding") {
  const std::string source = R"(
Marker {
  [i32] value
}

[return<Marker>]
/std/collections/map/at([map<i32, i32>] values, [i32] key) {
  return(Marker(key))
}

[return<int>]
/Marker/tag([Marker] self) {
  return(self.value)
}

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(2i32, 7i32)}
  return(/std/collections/map/at(values, 2i32).tag())
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("map namespaced access alias chained method requires explicit alias definition") {
  const std::string source = R"(
Marker {
  [i32] value
}

[return<Marker>]
/std/collections/map/at([map<i32, i32>] values, [i32] key) {
  return(Marker(key))
}

[return<int>]
/Marker/tag([Marker] self) {
  return(self.value)
}

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(2i32, 7i32)}
  return(/map/at(values, 2i32).tag())
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /map/at") != std::string::npos);
}

TEST_CASE("map namespaced access alias chained method rejects before downstream tag diagnostics") {
  const std::string source = R"(
Marker {
  [i32] value
}

[return<Marker>]
/std/collections/map/at([map<i32, i32>] values, [i32] key) {
  return(Marker(key))
}

[return<int>]
/Marker/tag([Marker] self, [bool] marker) {
  return(self.value)
}

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(2i32, 7i32)}
  return(/map/at(values, 2i32).tag(1i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /map/at") != std::string::npos);
}

TEST_CASE("map namespaced unsafe access alias chained method requires explicit alias definition") {
  const std::string source = R"(
Marker {
  [i32] value
}

[return<Marker>]
/std/collections/map/at_unsafe([map<i32, i32>] values, [i32] key) {
  return(Marker(key))
}

[return<int>]
/Marker/tag([Marker] self) {
  return(self.value)
}

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(2i32, 7i32)}
  return(/map/at_unsafe(values, 2i32).tag())
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /map/at_unsafe") != std::string::npos);
}

TEST_CASE("map namespaced unsafe access alias chained method rejects before downstream tag diagnostics") {
  const std::string source = R"(
Marker {
  [i32] value
}

[return<Marker>]
/std/collections/map/at_unsafe([map<i32, i32>] values, [i32] key) {
  return(Marker(key))
}

[return<int>]
/i32/tag([i32] self, [bool] marker) {
  return(self)
}

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(2i32, 7i32)}
  return(/map/at_unsafe(values, 2i32).tag(1i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /map/at_unsafe") != std::string::npos);
}

TEST_CASE("vector namespaced access alias field expression keeps removed-alias diagnostics") {
  const std::string source = R"(
Marker {
  [i32] value
}

[return<Marker>]
/std/collections/vector/at([vector<i32>] values, [i32] index) {
  return(Marker(index))
}

[return<int>]
/i32/tag([i32] self, [bool] marker) {
  return(self)
}

[return<auto>]
project([vector<i32>] values) {
  return(/vector/at(values, 2i32).value.tag(1i32))
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  return(project(values))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK_FALSE(error.empty());
}

TEST_CASE("vector constructor alias call infers canonical helper return kind") {
  const std::string source = R"(
Marker {
  [i32] value
}

[return<Marker>]
/std/collections/vector/vector([i32] seed) {
  return(Marker(seed))
}

[return<int>]
/Marker/tag([Marker] self) {
  return(self.value)
}

[return<auto>]
project() {
  return(/vector/vector(9i32).tag())
}

[effects(heap_alloc), return<int>]
main() {
  return(project())
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("vector constructor alias call keeps canonical helper diagnostics") {
  const std::string source = R"(
Marker {
  [i32] value
}

[return<Marker>]
/std/collections/vector/vector([i32] seed) {
  return(Marker(seed))
}

[return<int>]
/Marker/tag([Marker] self, [bool] marker) {
  return(self.value)
}

[return<auto>]
project() {
  return(/vector/vector(9i32).tag(1i32))
}

[effects(heap_alloc), return<int>]
main() {
  return(project())
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("argument type mismatch for /Marker/tag parameter marker") != std::string::npos);
}

TEST_CASE("vector method alias access rejects canonical struct-return forwarding") {
  const std::string source = R"(
Marker {
  [i32] value
}

[return<Marker>]
/std/collections/vector/at([vector<i32>] values, [i32] index) {
  return(Marker(index))
}

[return<int>]
/Marker/tag([Marker] self) {
  return(self.value)
}

[return<auto>]
project([vector<i32>] values) {
  return(values./vector/at(2i32).tag())
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  return(project(values))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown method: /i32/tag") != std::string::npos);
}

TEST_CASE("vector method alias access field expression keeps struct receiver diagnostics") {
  const std::string source = R"(
Marker {
  [i32] value
}

[return<Marker>]
/std/collections/vector/at([vector<i32>] values, [i32] index) {
  return(Marker(index))
}

[return<auto>]
project([vector<i32>] values) {
  return(values./vector/at(2i32).value)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  return(project(values))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("field access requires struct receiver") != std::string::npos);
}

TEST_CASE("vector unsafe method alias access struct method chain keeps primitive receiver diagnostics") {
  const std::string source = R"(
Marker {
  [i32] value
}

[return<Marker>]
/std/collections/vector/at_unsafe([vector<i32>] values, [i32] index) {
  return(Marker(index))
}

[return<int>]
/Marker/tag([Marker] self) {
  return(self.value)
}

[return<auto>]
project([vector<i32>] values) {
  return(values./vector/at_unsafe(2i32).tag())
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  return(project(values))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown method: /i32/tag") != std::string::npos);
}

TEST_CASE("vector unsafe method alias access field expression keeps struct receiver diagnostics") {
  const std::string source = R"(
Marker {
  [i32] value
}

[return<Marker>]
/std/collections/vector/at_unsafe([vector<i32>] values, [i32] index) {
  return(Marker(index))
}

[return<auto>]
project([vector<i32>] values) {
  return(values./vector/at_unsafe(2i32).value)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  return(project(values))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("field access requires struct receiver") != std::string::npos);
}

TEST_CASE("vector method access with alias and canonical struct helpers infers the alias return") {
  const std::string source = R"(
AliasMarker {
  [i32] value
}

CanonicalMarker {
  [i32] value
}

[return<AliasMarker>]
/vector/at([vector<i32>] values, [i32] index) {
  return(AliasMarker(plus(index, 40i32)))
}

[return<CanonicalMarker>]
/std/collections/vector/at([vector<i32>] values, [i32] index) {
  return(CanonicalMarker(index))
}

[return<auto>]
project([vector<i32>] values) {
  return(values.at(2i32).value)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  return(project(values))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("vector method access reports current receiver diagnostics over canonical helper") {
  const std::string source = R"(
Marker {
  [i32] value
}

[return<Marker>]
/std/collections/vector/at([vector<i32>] values, [i32] index) {
  return(Marker(index))
}

[return<int>]
/Marker/tag([Marker] self) {
  return(self.value)
}

[return<auto>]
project([vector<i32>] values) {
  return(values.at(2i32).tag())
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  return(project(values))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK_FALSE(error.empty());
}

TEST_CASE("vector unsafe method access with canonical helper does not infer auto return") {
  const std::string source = R"(
Marker {
  [i32] value
}

[return<Marker>]
/std/collections/vector/at_unsafe([vector<i32>] values, [i32] index) {
  return(Marker(index))
}

[return<auto>]
project([vector<i32>] values) {
  return(values.at_unsafe(2i32).value)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  return(project(values))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK_FALSE(error.empty());
}

TEST_CASE("map method access keeps canonical struct-return forwarding") {
  const std::string source = R"(
Marker {
  [i32] value
}

[return<Marker>]
/std/collections/map/at([map<i32, i32>] values, [i32] key) {
  return(Marker(key))
}

[return<int>]
/Marker/tag([Marker] self) {
  return(self.value)
}

[return<auto>]
project([map<i32, i32>] values) {
  return(values.at(2i32).tag())
}

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(2i32, 7i32)}
  return(project(values))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("map method access field expression keeps canonical struct-return forwarding") {
  const std::string source = R"(
Marker {
  [i32] value
}

[return<Marker>]
/std/collections/map/at([map<i32, i32>] values, [i32] key) {
  return(Marker(key))
}

[return<auto>]
project([map<i32, i32>] values) {
  return(values.at(2i32).value)
}

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(2i32, 7i32)}
  return(project(values))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("map method access prefers canonical struct-return over alias helper") {
  const std::string source = R"(
CanonicalMarker {
  [i32] value
}

AliasMarker {
  [i32] value
}

[return<AliasMarker>]
/map/at([map<i32, i32>] values, [i32] key) {
  return(AliasMarker(plus(key, 40i32)))
}

[return<CanonicalMarker>]
/std/collections/map/at([map<i32, i32>] values, [i32] key) {
  return(CanonicalMarker(key))
}

[return<int>]
/CanonicalMarker/tag([CanonicalMarker] self) {
  return(self.value)
}

[return<auto>]
project([map<i32, i32>] values) {
  return(values.at(2i32).tag())
}

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(2i32, 7i32)}
  return(project(values))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("map method access reports canonical builtin result type over alias helper") {
  const std::string source = R"(
Marker {
  [i32] value
}

[return<Marker>]
/map/at([map<i32, i32>] values, [i32] key) {
  return(Marker(plus(key, 40i32)))
}

[return<i32>]
/std/collections/map/at([map<i32, i32>] values, [i32] key) {
  return(key)
}

[return<int>]
/Marker/tag([Marker] self) {
  return(self.value)
}

[return<auto>]
project([map<i32, i32>] values) {
  return(values.at(2i32).tag())
}

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(2i32, 7i32)}
  return(project(values))
}
  )";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown method: /i32/tag") != std::string::npos);
}

TEST_CASE("map method alias access keeps primitive receiver diagnostics during inference") {
  const std::string source = R"(
Marker {
  [i32] value
}

[return<Marker>]
/std/collections/map/at([map<i32, i32>] values, [i32] key) {
  return(Marker(key))
}

[return<int>]
/Marker/tag([Marker] self) {
  return(self.value)
}

[return<auto>]
project([map<i32, i32>] values) {
  return(values./std/collections/map/at(2i32).tag())
}

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(2i32, 7i32)}
  return(project(values))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unable to infer return type on /project") != std::string::npos);
}

TEST_CASE("map method alias access keeps primitive argument diagnostics during inference") {
  const std::string source = R"(
Marker {
  [i32] value
}

[return<Marker>]
/std/collections/map/at([map<i32, i32>] values, [i32] key) {
  return(Marker(key))
}

[return<int>]
/i32/tag([i32] self, [bool] marker) {
  return(self)
}

[return<auto>]
project([map<i32, i32>] values) {
  return(values./std/collections/map/at(2i32).tag(1i32))
}

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(2i32, 7i32)}
  return(project(values))
}
  )";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unable to infer return type on /project") != std::string::npos);
}

TEST_CASE("wrapper-returned map method alias access keeps primitive receiver diagnostics during inference") {
  const std::string source = R"(
[effects(heap_alloc), return</std/collections/map<i32, i32>>]
wrapMap() {
  return(map<i32, i32>(2i32, 7i32))
}

[return<int>]
/i32/tag([i32] self) {
  return(plus(self, 40i32))
}

[return<auto>]
project() {
  return(wrapMap()./std/collections/map/at(2i32).tag())
}

[effects(heap_alloc), return<int>]
main() {
  return(project())
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unable to infer return type on /project") != std::string::npos);
}

TEST_CASE("wrapper-returned map method alias access keeps primitive argument diagnostics during inference") {
  const std::string source = R"(
Marker {
  [i32] value
}

[effects(heap_alloc), return</std/collections/map<i32, i32>>]
wrapMap() {
  return(map<i32, i32>(2i32, 7i32))
}

[return<Marker>]
/std/collections/map/at([map<i32, i32>] values, [i32] key) {
  return(Marker(key))
}

[return<int>]
/i32/tag([i32] self, [bool] marker) {
  return(self)
}

[return<auto>]
project() {
  return(wrapMap()./std/collections/map/at(2i32).tag(1i32))
}

[effects(heap_alloc), return<int>]
main() {
  return(project())
}
  )";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unable to infer return type on /project") != std::string::npos);
}

TEST_CASE("std-namespaced vector method alias access keeps primitive receiver diagnostics during inference") {
  const std::string source = R"(
Marker {
  [i32] value
}

[return<Marker>]
/std/collections/vector/at([vector<i32>] values, [i32] index) {
  return(Marker(index))
}

[return<int>]
/Marker/tag([Marker] self) {
  return(self.value)
}

[return<auto>]
project([vector<i32>] values) {
  return(values./std/collections/vector/at(2i32).tag())
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  return(project(values))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown method: /i32/tag") != std::string::npos);
}

TEST_CASE("std-namespaced vector method alias access keeps primitive argument diagnostics during inference") {
  const std::string source = R"(
Marker {
  [i32] value
}

[return<Marker>]
/std/collections/vector/at([vector<i32>] values, [i32] index) {
  return(Marker(index))
}

[return<int>]
/i32/tag([i32] self, [bool] marker) {
  return(self)
}

[return<auto>]
project([vector<i32>] values) {
  return(values./std/collections/vector/at(2i32).tag(1i32))
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  return(project(values))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("argument type mismatch for /i32/tag parameter marker") != std::string::npos);
}

TEST_CASE("vector method alias access keeps removed-alias diagnostics") {
  const std::string source = R"(
Marker {
  [i32] value
}

[return<Marker>]
/std/collections/vector/at([vector<i32>] values, [i32] index) {
  return(Marker(index))
}

[return<int>]
/i32/tag([i32] self, [bool] marker) {
  return(self)
}

[return<auto>]
project([vector<i32>] values) {
  return(values./vector/at(2i32).tag(1i32))
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  return(project(values))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("argument type mismatch for /i32/tag parameter marker") != std::string::npos);
}

TEST_CASE("vector alias access auto wrapper keeps primitive receiver diagnostics") {
  const std::string source = R"(
Marker {
  [i32] value
}

[return<int>]
/vector/at([vector<i32>] values, [i32] index) {
  return(index)
}

[return<Marker>]
/std/collections/vector/at([vector<i32>] values, [i32] index) {
  return(Marker(index))
}

[return<auto>]
project([vector<i32>] values) {
  return(/vector/at(values, 2i32))
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  return(project(values).tag())
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown method: /i32/tag") != std::string::npos);
}

TEST_CASE("vector alias access auto wrapper keeps primitive argument diagnostics") {
  const std::string source = R"(
Marker {
  [i32] value
}

[return<int>]
/vector/at([vector<i32>] values, [i32] index) {
  return(index)
}

[return<Marker>]
/std/collections/vector/at([vector<i32>] values, [i32] index) {
  return(Marker(index))
}

[return<int>]
/i32/tag([i32] self, [bool] marker) {
  return(self)
}

[return<auto>]
project([vector<i32>] values) {
  return(/vector/at(values, 2i32))
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  return(project(values).tag(1i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("argument type mismatch for /i32/tag parameter marker") != std::string::npos);
}

TEST_CASE("templated stdlib canonical vector helpers reject method-call sugar template args on count") {
  const std::string source = R"(
[return<int>]
/std/collections/vector/count<T>([vector<T>] values, [bool] marker) {
  return(90i32)
}

[return<int>]
/std/collections/vector/at<T>([vector<T>] values, [i32] index) {
  return(plus(index, 40i32))
}

  [effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  return(plus(values.count<i32>(true), values.at<i32>(2i32)))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown method: /vector/count") != std::string::npos);
}

TEST_CASE("templated slash-path vector helper methods stay on unknown method diagnostics") {
  const std::string source = R"(
[return<int>]
/std/collections/vector/count<T>([vector<T>] values, [bool] marker) {
  return(90i32)
}

[return<int>]
/std/collections/vector/at<T>([vector<T>] values, [i32] index) {
  return(plus(index, 40i32))
}

  [effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  return(plus(values./vector/count<i32>(true), values./std/collections/vector/at<i32>(2i32)))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown method: /vector/count") != std::string::npos);
}

TEST_CASE("templated slash-path vector helper arity failures stay on unknown method diagnostics") {
  const std::string source = R"(
[return<int>]
/std/collections/vector/count<T>([vector<T>] values, [bool] marker) {
  return(90i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  return(values./vector/count<i32>())
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown method: /vector/count") != std::string::npos);
}

TEST_CASE("templated stdlib canonical vector helper reports current argument mismatch diagnostics") {
  const std::string source = R"(
[return<int>]
/std/collections/vector/count<T>([vector<T>] values, [bool] marker) {
  return(90i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  return(values.count<i32>())
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK_FALSE(error.empty());
}

TEST_CASE("vector namespaced call aliases validate count and access builtins") {
  const std::string source = R"(
[return<int>]
/vector/count([vector<i32>] values) {
  return(90i32)
}

[return<int>]
/vector/at([vector<i32>] values, [i32] index) {
  return(plus(index, 40i32))
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  return(plus(/vector/count(values), /vector/at(values, 2i32)))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("vector namespaced count alias keeps canonical helper diagnostics") {
  const std::string source = R"(
[return<int>]
/vector/count([vector<i32>] values, [bool] marker) {
  return(90i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  return(/vector/count(values))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("argument count mismatch") != std::string::npos);
  CHECK(error.find("/vector/count") != std::string::npos);
}

TEST_CASE("vector namespaced count alias falls back to canonical helper return") {
  const std::string source = R"(
[return<bool>]
/vector/count([vector<i32>] values, [bool] marker) {
  return(false)
}

[effects(heap_alloc), return<bool>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  return(/vector/count(values, true))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("vector namespaced call alias with explicit template args keeps builtin diagnostics") {
  const std::string source = R"(
[return<int>]
/std/collections/vector/count<T>([vector<T>] values, [bool] marker) {
  return(90i32)
}

  [effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  return(/vector/count<i32>(values, true))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /vector/count") != std::string::npos);
}

TEST_CASE("vector namespaced count alias arity mismatch rejects compatibility template forwarding") {
  const std::string source = R"(
[return<int>]
/vector/count([vector<i32>] values) {
  return(7i32)
}

[return<int>]
/std/collections/vector/count<T>([vector<T>] values, [bool] marker) {
  return(90i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  return(plus(/vector/count(values, true), values.count(true)))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("argument count mismatch") != std::string::npos);
}

TEST_CASE("vector namespaced alias rejects compatibility template forwarding on bool type mismatch with same arity") {
  const std::string source = R"(
[return<int>]
/vector/count([vector<i32>] values, [i32] marker) {
  return(7i32)
}

[return<int>]
/std/collections/vector/count<T>([vector<T>] values, [bool] marker) {
  return(90i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  return(plus(/vector/count(values, true), values.count(true)))
}
  )";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK_FALSE(error.empty());
}

TEST_CASE("vector namespaced alias rejects compatibility template forwarding on non-bool type mismatch with same arity") {
  const std::string source = R"(
[return<int>]
/vector/count([vector<i32>] values, [string] marker) {
  return(7i32)
}

[return<int>]
/std/collections/vector/count<T>([vector<T>] values, [i32] marker) {
  return(90i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  return(plus(/vector/count(values, 1i32), values.count(1i32)))
}
  )";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK_FALSE(error.empty());
}

TEST_CASE("vector namespaced alias rejects compatibility template forwarding on struct type mismatch with same arity") {
  const std::string source = R"(
MarkerA() {}
MarkerB() {}

[return<int>]
/vector/count([vector<i32>] values, [MarkerA] marker) {
  return(7i32)
}

[return<int>]
/std/collections/vector/count<T>([vector<T>] values, [MarkerB] marker) {
  return(90i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  [MarkerB] marker{MarkerB()}
  return(plus(/vector/count(values, marker), values.count(marker)))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("argument type mismatch for /vector/count parameter marker") != std::string::npos);
}

TEST_CASE("vector namespaced struct mismatch fallback keeps compatibility diagnostics") {
  const std::string source = R"(
MarkerA() {}
MarkerB() {}
MarkerC() {}

[return<int>]
/vector/count([vector<i32>] values, [MarkerA] marker) {
  return(7i32)
}

[return<int>]
/std/collections/vector/count<T>([vector<T>] values, [MarkerC] marker) {
  return(90i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  [MarkerB] marker{MarkerB()}
  return(values.count(marker))
}
  )";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK_FALSE(error.empty());
}

TEST_CASE("vector namespaced alias rejects compatibility template forwarding for struct mismatch from constructor temporary") {
  const std::string source = R"(
MarkerA() {}
MarkerB() {}

[return<int>]
/vector/count([vector<i32>] values, [MarkerA] marker) {
  return(7i32)
}

[return<int>]
/std/collections/vector/count<T>([vector<T>] values, [MarkerB] marker) {
  return(90i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  return(plus(/vector/count(values, MarkerB()), values.count(MarkerB())))
}
  )";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK_FALSE(error.empty());
}

TEST_CASE("vector namespaced constructor temporary mismatch keeps compatibility diagnostics") {
  const std::string source = R"(
MarkerA() {}
MarkerB() {}
MarkerC() {}

[return<int>]
/vector/count([vector<i32>] values, [MarkerA] marker) {
  return(7i32)
}

[return<int>]
/std/collections/vector/count<T>([vector<T>] values, [MarkerC] marker) {
  return(90i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  return(values.count(MarkerB()))
}
  )";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK_FALSE(error.empty());
}

TEST_CASE("vector namespaced alias rejects compatibility template forwarding for struct mismatch from method-call temporary") {
  const std::string source = R"(
MarkerA() {}
MarkerB() {}
Holder() {}

[return<MarkerB>]
/Holder/fetch([Holder] self) {
  return(MarkerB())
}

[return<int>]
/vector/count([vector<i32>] values, [MarkerA] marker) {
  return(7i32)
}

[return<int>]
/std/collections/vector/count<T>([vector<T>] values, [MarkerB] marker) {
  return(90i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  [Holder] holder{Holder()}
  return(plus(/vector/count(values, holder.fetch()), values.count(holder.fetch())))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("argument type mismatch for /vector/count parameter marker") != std::string::npos);
}

TEST_CASE("vector namespaced method-call temporary mismatch keeps compatibility diagnostics") {
  const std::string source = R"(
MarkerA() {}
MarkerB() {}
MarkerC() {}
Holder() {}

[return<MarkerB>]
/Holder/fetch([Holder] self) {
  return(MarkerB())
}

[return<int>]
/vector/count([vector<i32>] values, [MarkerA] marker) {
  return(7i32)
}

[return<int>]
/std/collections/vector/count<T>([vector<T>] values, [MarkerC] marker) {
  return(90i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  [Holder] holder{Holder()}
  return(values.count(holder.fetch()))
}
  )";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK_FALSE(error.empty());
}

TEST_CASE("vector namespaced alias rejects compatibility template forwarding for struct mismatch from chained method-call temporary") {
  const std::string source = R"(
MarkerA() {}
MarkerB() {}
Middle() {}
Holder() {}

[return<Middle>]
/Holder/first([Holder] self) {
  return(Middle())
}

[return<MarkerB>]
/Middle/next([Middle] self) {
  return(MarkerB())
}

[return<int>]
/vector/count([vector<i32>] values, [MarkerA] marker) {
  return(7i32)
}

[return<int>]
/std/collections/vector/count<T>([vector<T>] values, [MarkerB] marker) {
  return(90i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  [Holder] holder{Holder()}
  return(plus(/vector/count(values, holder.first().next()),
              values.count(holder.first().next())))
}
  )";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK_FALSE(error.empty());
}

TEST_CASE("vector namespaced chained method-call mismatch keeps compatibility diagnostics") {
  const std::string source = R"(
MarkerA() {}
MarkerB() {}
MarkerC() {}
Middle() {}
Holder() {}

[return<Middle>]
/Holder/first([Holder] self) {
  return(Middle())
}

[return<MarkerB>]
/Middle/next([Middle] self) {
  return(MarkerB())
}

[return<int>]
/vector/count([vector<i32>] values, [MarkerA] marker) {
  return(7i32)
}

[return<int>]
/std/collections/vector/count<T>([vector<T>] values, [MarkerC] marker) {
  return(90i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  [Holder] holder{Holder()}
  return(values.count(holder.first().next()))
}
  )";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK_FALSE(error.empty());
}

TEST_CASE("vector namespaced alias rejects compatibility template forwarding on array envelope element mismatch") {
  const std::string source = R"(
[return<int>]
/vector/count([vector<i32>] values, [array<i32>] marker) {
  return(7i32)
}

[return<int>]
/std/collections/vector/count<T>([vector<T>] values, [array<i64>] marker) {
  return(90i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  [array<i64>] marker{array<i64>(1i64)}
  return(plus(/vector/count(values, marker), values.count(marker)))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("argument type mismatch for /vector/count parameter marker") != std::string::npos);
}

TEST_CASE("vector namespaced array envelope mismatch keeps compatibility diagnostics") {
  const std::string source = R"(
[return<int>]
/vector/count([vector<i32>] values, [array<i32>] marker) {
  return(7i32)
}

[return<int>]
/std/collections/vector/count<T>([vector<T>] values, [array<bool>] marker) {
  return(90i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  [array<i64>] marker{array<i64>(1i64)}
  return(values.count(marker))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("argument type mismatch") != std::string::npos);
  CHECK(error.find("/vector/count") != std::string::npos);
}

TEST_CASE("vector namespaced alias rejects compatibility template forwarding on map envelope mismatch") {
  const std::string source = R"(
[return<int>]
/vector/count([vector<i32>] values, [map<i32, string>] marker) {
  return(7i32)
}

[return<int>]
/std/collections/vector/count<T>([vector<T>] values, [map<i32, i64>] marker) {
  return(90i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  [map<i32, i64>] marker{map<i32, i64>(1i32, 2i64)}
  return(plus(/vector/count(values, marker), values.count(marker)))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("argument type mismatch for /vector/count parameter marker") != std::string::npos);
}

TEST_CASE("vector namespaced map envelope mismatch keeps compatibility diagnostics") {
  const std::string source = R"(
[return<int>]
/vector/count([vector<i32>] values, [map<i32, string>] marker) {
  return(7i32)
}

[return<int>]
/std/collections/vector/count<T>([vector<T>] values, [map<i32, bool>] marker) {
  return(90i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  [map<i32, i64>] marker{map<i32, i64>(1i32, 2i64)}
  return(values.count(marker))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("argument type mismatch") != std::string::npos);
  CHECK(error.find("/vector/count") != std::string::npos);
}

TEST_CASE("vector namespaced alias rejects compatibility template forwarding on map envelope mismatch from call return") {
  const std::string source = R"(
[effects(heap_alloc), return<map<i32, i64>>]
makeMarker() {
  return(map<i32, i64>(1i32, 2i64))
}

[return<int>]
/vector/count([vector<i32>] values, [map<i32, string>] marker) {
  return(7i32)
}

[return<int>]
/std/collections/vector/count<T>([vector<T>] values, [map<i32, i64>] marker) {
  return(90i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  return(plus(/vector/count(values, makeMarker()), values.count(makeMarker())))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("argument type mismatch for /vector/count parameter marker") != std::string::npos);
}

TEST_CASE("vector namespaced map call-return mismatch keeps compatibility diagnostics") {
  const std::string source = R"(
[effects(heap_alloc), return<map<i32, i64>>]
makeMarker() {
  return(map<i32, i64>(1i32, 2i64))
}

[return<int>]
/vector/count([vector<i32>] values, [map<i32, string>] marker) {
  return(7i32)
}

[return<int>]
/std/collections/vector/count<T>([vector<T>] values, [map<i32, bool>] marker) {
  return(90i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  return(values.count(makeMarker()))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("argument type mismatch") != std::string::npos);
  CHECK(error.find("/vector/count") != std::string::npos);
}

TEST_CASE("vector namespaced alias rejects compatibility template forwarding on primitive mismatch from call return") {
  const std::string source = R"(
[return<i32>]
makeMarker() {
  return(1i32)
}

[return<int>]
/vector/count([vector<i32>] values, [string] marker) {
  return(7i32)
}

[return<int>]
/std/collections/vector/count<T>([vector<T>] values, [i32] marker) {
  return(90i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  return(plus(/vector/count(values, makeMarker()), values.count(makeMarker())))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("argument type mismatch for /vector/count parameter marker") != std::string::npos);
}

TEST_CASE("vector namespaced primitive call-return mismatch keeps compatibility diagnostics") {
  const std::string source = R"(
[return<i32>]
makeMarker() {
  return(1i32)
}

[return<int>]
/vector/count([vector<i32>] values, [string] marker) {
  return(7i32)
}

[return<int>]
/std/collections/vector/count<T>([vector<T>] values, [bool] marker) {
  return(90i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  return(values.count(makeMarker()))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("argument type mismatch") != std::string::npos);
  CHECK(error.find("/vector/count") != std::string::npos);
}

TEST_CASE("vector namespaced alias rejects compatibility template forwarding when unknown expected meets primitive call return") {
  const std::string source = R"(
Marker() {}

[return<i32>]
makeMarker() {
  return(1i32)
}

[return<int>]
/vector/count([vector<i32>] values, [Marker] marker) {
  return(7i32)
}

[return<int>]
/std/collections/vector/count<T>([vector<T>] values, [i32] marker) {
  return(90i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  return(plus(/vector/count(values, makeMarker()), values.count(makeMarker())))
}
  )";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("argument type mismatch for /vector/count parameter marker") != std::string::npos);
}

TEST_CASE("vector namespaced unknown expected primitive call return keeps compatibility diagnostics") {
  const std::string source = R"(
Marker() {}

[return<i32>]
makeMarker() {
  return(1i32)
}

[return<int>]
/vector/count([vector<i32>] values, [Marker] marker) {
  return(7i32)
}

[return<int>]
/std/collections/vector/count<T>([vector<T>] values, [bool] marker) {
  return(90i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  return(values.count(makeMarker()))
}
  )";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("argument type mismatch for /vector/count parameter marker") != std::string::npos);
}

TEST_CASE("vector namespaced alias rejects compatibility template forwarding when unknown expected meets primitive binding") {
  const std::string source = R"(
Marker() {}

[return<int>]
/vector/count([vector<i32>] values, [Marker] marker) {
  return(7i32)
}

[return<int>]
/std/collections/vector/count<T>([vector<T>] values, [i32] marker) {
  return(90i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  [i32] marker{1i32}
  return(plus(/vector/count(values, marker), values.count(marker)))
}
  )";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("argument type mismatch for /vector/count parameter marker") != std::string::npos);
}

TEST_CASE("vector namespaced unknown expected primitive binding keeps compatibility diagnostics") {
  const std::string source = R"(
Marker() {}

[return<int>]
/vector/count([vector<i32>] values, [Marker] marker) {
  return(7i32)
}

[return<int>]
/std/collections/vector/count<T>([vector<T>] values, [bool] marker) {
  return(90i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  [i32] marker{1i32}
  return(values.count(marker))
}
  )";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("argument type mismatch for /vector/count parameter marker") != std::string::npos);
}

TEST_CASE("vector namespaced alias rejects compatibility template forwarding when unknown expected meets vector envelope binding") {
  const std::string source = R"(
Marker() {}

[return<int>]
/vector/count([vector<i32>] values, [Marker] marker) {
  return(7i32)
}

[return<int>]
/std/collections/vector/count<T>([vector<T>] values, [vector<i32>] marker) {
  return(90i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  [vector<i32>] marker{vector<i32>(1i32)}
  return(plus(/vector/count(values, marker), values.count(marker)))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("argument type mismatch for /vector/count parameter marker") != std::string::npos);
}

TEST_CASE("vector namespaced unknown expected vector envelope keeps compatibility diagnostics") {
  const std::string source = R"(
Marker() {}

[return<int>]
/vector/count([vector<i32>] values, [Marker] marker) {
  return(7i32)
}

[return<int>]
/std/collections/vector/count<T>([vector<T>] values, [vector<bool>] marker) {
  return(90i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  [vector<i32>] marker{vector<i32>(1i32)}
  return(values.count(marker))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("argument type mismatch") != std::string::npos);
  CHECK(error.find("/vector/count") != std::string::npos);
}

TEST_CASE("vector namespaced count alias arity fallback keeps compatibility diagnostics") {
  const std::string source = R"(
[return<int>]
/vector/count([vector<i32>] values) {
  return(7i32)
}

[return<int>]
/std/collections/vector/count<T>([vector<T>] values, [bool] marker) {
  return(90i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  return(values.count(1i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("argument count mismatch") != std::string::npos);
}

TEST_CASE("vector namespaced non-bool mismatch fallback keeps compatibility diagnostics") {
  const std::string source = R"(
[return<int>]
/vector/count([vector<i32>] values, [string] marker) {
  return(7i32)
}

[return<int>]
/std/collections/vector/count<T>([vector<T>] values, [bool] marker) {
  return(90i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  return(values.count(1i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("argument type mismatch") != std::string::npos);
  CHECK(error.find("/vector/count") != std::string::npos);
}

TEST_CASE("vector namespaced bool mismatch fallback keeps compatibility diagnostics") {
  const std::string source = R"(
[return<int>]
/vector/count([vector<i32>] values, [i32] marker) {
  return(7i32)
}

[return<int>]
/std/collections/vector/count<T>([vector<T>] values, [string] marker) {
  return(90i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  return(values.count(true))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("argument type mismatch") != std::string::npos);
  CHECK(error.find("/vector/count") != std::string::npos);
}

TEST_CASE("vector namespaced count alias named arguments reject compatibility template forwarding") {
  const std::string source = R"(
[return<int>]
/vector/count([vector<i32>] values) {
  return(7i32)
}

[return<int>]
/std/collections/vector/count<T>([vector<T>] values, [bool] marker) {
  return(90i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  return(plus(/vector/count([values] values, [marker] true),
              values.count([marker] true)))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown named argument: marker") != std::string::npos);
}

TEST_CASE("vector namespaced count alias named arguments keep compatibility diagnostics") {
  const std::string source = R"(
[return<int>]
/vector/count([vector<i32>] values) {
  return(7i32)
}

[return<int>]
/std/collections/vector/count<T>([vector<T>] values, [bool] marker) {
  return(90i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  return(values.count([marker] 1i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown named argument: marker") != std::string::npos);
}

TEST_CASE("vector namespaced capacity alias arity mismatch rejects compatibility template forwarding") {
  const std::string source = R"(
[return<int>]
/vector/capacity([vector<i32>] values) {
  return(7i32)
}

[return<int>]
/std/collections/vector/capacity<T>([vector<T>] values, [bool] marker) {
  return(90i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  return(plus(/vector/capacity(values, true), values.capacity(true)))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("argument count mismatch") != std::string::npos);
}

TEST_CASE("vector namespaced capacity alias type mismatch rejects compatibility template forwarding") {
  const std::string source = R"(
[return<int>]
/vector/capacity([vector<i32>] values, [i32] marker) {
  return(7i32)
}

[return<int>]
/std/collections/vector/capacity<T>([vector<T>] values, [bool] marker) {
  return(90i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  return(plus(/vector/capacity(values, true), values.capacity(true)))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("argument type mismatch for /vector/capacity parameter marker") != std::string::npos);
}

TEST_CASE("vector namespaced capacity alias named arguments reject compatibility template forwarding") {
  const std::string source = R"(
[return<int>]
/vector/capacity([vector<i32>] values) {
  return(7i32)
}

[return<int>]
/std/collections/vector/capacity<T>([vector<T>] values, [bool] marker) {
  return(90i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  return(plus(/vector/capacity([values] values, [marker] true),
              values.capacity([marker] true)))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown named argument: marker") != std::string::npos);
}

TEST_CASE("vector namespaced at alias arity mismatch rejects compatibility template forwarding") {
  const std::string source = R"(
[return<int>]
/vector/at([vector<i32>] values) {
  return(7i32)
}

[return<int>]
/std/collections/vector/at<T>([vector<T>] values, [i32] index) {
  return(90i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  return(plus(/vector/at(values, 0i32), values.at(0i32)))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("argument count mismatch") != std::string::npos);
}

TEST_CASE("vector namespaced at alias type mismatch rejects compatibility template forwarding") {
  const std::string source = R"(
[return<int>]
/vector/at([vector<i32>] values, [string] index) {
  return(7i32)
}

[return<int>]
/std/collections/vector/at<T>([vector<T>] values, [i32] index) {
  return(90i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  return(plus(/vector/at(values, 0i32), values.at(0i32)))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("argument type mismatch for /vector/at parameter index") != std::string::npos);
}

TEST_CASE("vector namespaced at alias named arguments reject compatibility template forwarding") {
  const std::string source = R"(
[return<int>]
/vector/at([vector<i32>] values) {
  return(7i32)
}

[return<int>]
/std/collections/vector/at<T>([vector<T>] values, [i32] index) {
  return(90i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  return(plus(/vector/at([values] values, [index] 0i32),
              values.at([index] 0i32)))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown named argument: index") != std::string::npos);
}

TEST_CASE("vector namespaced at_unsafe alias arity mismatch rejects compatibility template forwarding") {
  const std::string source = R"(
[return<int>]
/vector/at_unsafe([vector<i32>] values) {
  return(7i32)
}

[return<int>]
/std/collections/vector/at_unsafe<T>([vector<T>] values, [i32] index) {
  return(90i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  return(plus(/vector/at_unsafe(values, 0i32), values.at_unsafe(0i32)))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("argument count mismatch") != std::string::npos);
}

TEST_CASE("vector namespaced at_unsafe alias type mismatch rejects compatibility template forwarding") {
  const std::string source = R"(
[return<int>]
/vector/at_unsafe([vector<i32>] values, [string] index) {
  return(7i32)
}

[return<int>]
/std/collections/vector/at_unsafe<T>([vector<T>] values, [i32] index) {
  return(90i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  return(plus(/vector/at_unsafe(values, 0i32), values.at_unsafe(0i32)))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("argument type mismatch for /vector/at_unsafe parameter index") != std::string::npos);
}

TEST_CASE("vector namespaced at_unsafe alias named arguments reject compatibility template forwarding") {
  const std::string source = R"(
[return<int>]
/vector/at_unsafe([vector<i32>] values) {
  return(7i32)
}

[return<int>]
/std/collections/vector/at_unsafe<T>([vector<T>] values, [i32] index) {
  return(90i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  return(plus(/vector/at_unsafe([values] values, [index] 0i32),
              values.at_unsafe([index] 0i32)))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown named argument: index") != std::string::npos);
}

TEST_CASE("array namespaced templated count alias is rejected") {
  const std::string source = R"(
[return<int>]
/array/count([vector<i32>] values) {
  return(7i32)
}

[return<int>]
/std/collections/vector/count<T>([vector<T>] values, [bool] marker) {
  return(90i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  return(/array/count<i32>(values, true))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("template arguments are only supported on templated definitions: /array/count") !=
        std::string::npos);
}

TEST_CASE("array namespaced templated count alias missing marker is rejected") {
  const std::string source = R"(
[return<int>]
/array/count([vector<i32>] values) {
  return(7i32)
}

[return<int>]
/std/collections/vector/count<T>([vector<T>] values, [bool] marker) {
  return(90i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  return(/array/count<i32>(values))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("template arguments are only supported on templated definitions: /array/count") !=
        std::string::npos);
}

TEST_CASE("stdlib namespaced templated vector count call rejects compatibility alias fallback") {
  const std::string source = R"(
[return<int>]
/vector/count([vector<i32>] values) {
  return(7i32)
}

[return<int>]
/array/count<T>([vector<T>] values, [bool] marker) {
  return(90i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  return(/std/collections/vector/count<i32>(values, true))
}
  )";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /std/collections/vector/count") != std::string::npos);
}

TEST_CASE("stdlib namespaced templated vector count arity keeps builtin template-argument diagnostics") {
  const std::string source = R"(
[return<int>]
/vector/count([vector<i32>] values) {
  return(7i32)
}

[return<int>]
/array/count<T>([vector<T>] values, [bool] marker) {
  return(90i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  return(/std/collections/vector/count<i32>(values))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /std/collections/vector/count") != std::string::npos);
}

TEST_CASE("vector namespaced templated alias call keeps builtin template diagnostics") {
  const std::string source = R"(
[return<int>]
/vector/count([vector<i32>] values) {
  return(7i32)
}

[return<int>]
/std/collections/vector/count<T>([vector<T>] values, [bool] marker) {
  return(90i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  return(/vector/count<i32>(values, true))
}
  )";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("template arguments are only supported on templated definitions: /vector/count") !=
        std::string::npos);
}

TEST_CASE("vector method templated call reports canonical argument mismatch past non-templated helper") {
  const std::string source = R"(
[return<int>]
/vector/count([vector<i32>] values) {
  return(7i32)
}

[return<int>]
/std/collections/vector/count<T>([vector<T>] values, [bool] marker) {
  return(90i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  return(values.count<i32>())
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK_FALSE(error.empty());
}

TEST_CASE("wrapper temporary templated vector method call rejects compatibility template forwarding") {
  const std::string source = R"(
[return<int>]
/vector/count([vector<i32>] values) {
  return(7i32)
}

[return<int>]
/std/collections/vector/count<T>([vector<T>] values, [bool] marker) {
  return(90i32)
}

[effects(heap_alloc), return<vector<i32>>]
wrapValues() {
  return(vector<i32>(5i32, 6i32, 7i32))
}

[effects(heap_alloc), return<int>]
main() {
  return(wrapValues().count<i32>(true))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("template arguments are only supported on templated definitions: /vector/count") !=
        std::string::npos);
}

TEST_CASE("wrapper temporary templated vector method arity mismatch reports canonical argument mismatch") {
  const std::string source = R"(
[return<int>]
/vector/count([vector<i32>] values) {
  return(7i32)
}

[return<int>]
/std/collections/vector/count<T>([vector<T>] values, [bool] marker) {
  return(90i32)
}

[effects(heap_alloc), return<vector<i32>>]
wrapValues() {
  return(vector<i32>(5i32, 6i32, 7i32))
}

[effects(heap_alloc), return<int>]
main() {
  return(wrapValues().count<i32>())
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK_FALSE(error.empty());
}

TEST_CASE("vector namespaced alias keeps builtin count diagnostics when only canonical templated helper exists") {
  const std::string source = R"(
[return<int>]
/std/collections/vector/count<T>([vector<T>] values, [bool] marker) {
  return(90i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  return(/vector/count(values, true))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /vector/count") != std::string::npos);
}

TEST_CASE("vector namespaced count capacity and access helpers validate as builtin aliases") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(4i32, 5i32)}
  [i32] c{/vector/count(values)}
  [i32] k{/vector/capacity(values)}
  [i32] first{/vector/at(values, 0i32)}
  [i32] second{/vector/at_unsafe(values, 1i32)}
  return(plus(plus(c, k), plus(first, second)))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /vector/count") != std::string::npos);
}

TEST_CASE("stdlib namespaced vector count rejects template arguments as builtin alias") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(4i32, 5i32)}
  return(/std/collections/vector/count<i32>(values))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /std/collections/vector/count") != std::string::npos);
}

TEST_CASE("vector namespaced capacity rejects template arguments as builtin alias") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(4i32, 5i32)}
  return(/vector/capacity<i32>(values))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /vector/capacity") != std::string::npos);
}

TEST_CASE("stdlib namespaced vector count accepts named arguments through imported stdlib helper") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(4i32, 5i32)}
  return(/std/collections/vector/count([values] values))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("stdlib namespaced vector count requires imported stdlib helper or explicit definition") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(4i32, 5i32)}
  return(/std/collections/vector/count(values))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /std/collections/vector/count") != std::string::npos);
}

TEST_CASE("stdlib namespaced vector count map target without helper reports unknown target") {
  const std::string source = R"(
[return<map<i32, i32>>]
wrapMap() {
  return(map<i32, i32>(1i32, 2i32, 3i32, 4i32))
}

[effects(heap_alloc), return<int>]
main() {
  return(/std/collections/vector/count(wrapMap()))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /std/collections/vector/count") != std::string::npos);
}

TEST_CASE("stdlib namespaced vector count accepts same-path helper on map target") {
  const std::string source = R"(
[return<int>]
/std/collections/vector/count([map<i32, i32>] values) {
  return(53i32)
}

[return<map<i32, i32>>]
wrapMap() {
  return(map<i32, i32>(1i32, 2i32, 3i32, 4i32))
}

[effects(heap_alloc), return<int>]
main() {
  return(/std/collections/vector/count(wrapMap()))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("vector namespaced count accepts same-path helper on map target") {
  const std::string source = R"(
[return<int>]
/vector/count([map<i32, i32>] values) {
  return(43i32)
}

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 2i32)}
  return(/vector/count(values))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("vector namespaced count map target without helper reports unknown target") {
  const std::string source = R"(
[return<map<i32, i32>>]
wrapMap() {
  return(map<i32, i32>(1i32, 2i32, 3i32, 4i32))
}

[effects(heap_alloc), return<int>]
main() {
  return(/vector/count(wrapMap()))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /vector/count") != std::string::npos);
}

TEST_CASE("stdlib namespaced vector count accepts same-path helper on string target") {
  const std::string source = R"(
[return<int>]
/std/collections/vector/count([string] values) {
  return(54i32)
}

[return<string>]
wrapText() {
  return("abc"raw_utf8)
}

[return<int>]
main() {
  return(/std/collections/vector/count(wrapText()))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("stdlib namespaced vector count string target without helper reports unknown target") {
  const std::string source = R"(
[return<string>]
wrapText() {
  return("abc"raw_utf8)
}

[return<int>]
main() {
  return(/std/collections/vector/count(wrapText()))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /std/collections/vector/count") != std::string::npos);
}

TEST_CASE("vector namespaced count accepts same-path helper on string target") {
  const std::string source = R"(
[return<int>]
/vector/count([string] values) {
  return(44i32)
}

[return<string>]
wrapText() {
  return("abc"raw_utf8)
}

[return<int>]
main() {
  return(/vector/count(wrapText()))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("vector namespaced count string target without helper reports unknown target") {
  const std::string source = R"(
[return<string>]
wrapText() {
  return("abc"raw_utf8)
}

[return<int>]
main() {
  return(/vector/count(wrapText()))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /vector/count") != std::string::npos);
}

TEST_CASE("stdlib namespaced vector count accepts same-path helper on array target") {
  const std::string source = R"(
[return<int>]
/std/collections/vector/count([array<i32>] values) {
  return(55i32)
}

[return<array<i32>>]
wrapArray() {
  return(array<i32>(1i32, 2i32, 3i32))
}

[return<int>]
main() {
  return(/std/collections/vector/count(wrapArray()))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("stdlib namespaced vector count array target without helper reports unknown target") {
  const std::string source = R"(
[return<array<i32>>]
wrapArray() {
  return(array<i32>(1i32, 2i32, 3i32))
}

[return<int>]
main() {
  return(/std/collections/vector/count(wrapArray()))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /std/collections/vector/count") != std::string::npos);
}

TEST_CASE("vector namespaced count accepts same-path helper on array target") {
  const std::string source = R"(
[return<int>]
/vector/count([array<i32>] values) {
  return(45i32)
}

[return<array<i32>>]
wrapArray() {
  return(array<i32>(1i32, 2i32, 3i32))
}

[return<int>]
main() {
  return(/vector/count(wrapArray()))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("vector namespaced count array target without helper reports unknown target") {
  const std::string source = R"(
[return<array<i32>>]
wrapArray() {
  return(array<i32>(1i32, 2i32, 3i32))
}

[return<int>]
main() {
  return(/vector/count(wrapArray()))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /vector/count") != std::string::npos);
}

TEST_CASE("vector namespaced count slash method accepts same-path helper on string target") {
  const std::string source = R"(
[return<int>]
/vector/count([string] values) {
  return(46i32)
}

[return<string>]
wrapText() {
  return("abc"raw_utf8)
}

[return<int>]
main() {
  return(wrapText()./vector/count())
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("vector namespaced count slash method accepts same-path helper on map target") {
  const std::string source = R"(
[return<int>]
/vector/count([map<i32, i32>] values) {
  return(47i32)
}

[return<map<i32, i32>>]
wrapMap() {
  return(map<i32, i32>(1i32, 2i32))
}

[return<int>]
main() {
  return(wrapMap()./vector/count())
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("vector namespaced count slash method map target without helper reports unknown method") {
  const std::string source = R"(
[return<map<i32, i32>>]
wrapMap() {
  return(map<i32, i32>(1i32, 2i32))
}

[return<int>]
main() {
  return(wrapMap()./vector/count())
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown method: /vector/count") != std::string::npos);
}

TEST_CASE("vector namespaced count slash method string target without helper reports unknown method") {
  const std::string source = R"(
[return<string>]
wrapText() {
  return("abc"raw_utf8)
}

[return<int>]
main() {
  return(wrapText()./vector/count())
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("vector namespaced count slash method accepts same-path helper on array target") {
  const std::string source = R"(
[return<int>]
/vector/count([array<i32>] values) {
  return(47i32)
}

[return<array<i32>>]
wrapArray() {
  return(array<i32>(1i32, 2i32, 3i32))
}

[return<int>]
main() {
  return(wrapArray()./vector/count())
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("vector namespaced count slash method array target without helper reports unknown method") {
  const std::string source = R"(
[return<array<i32>>]
wrapArray() {
  return(array<i32>(1i32, 2i32, 3i32))
}

[return<int>]
main() {
  return(wrapArray()./vector/count())
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("vector namespaced count accepts same-path helper on wrapper vector target") {
  const std::string source = R"(
[return<int>]
/vector/count([vector<i32>] values) {
  return(44i32)
}

[effects(heap_alloc), return<vector<i32>>]
wrapVector() {
  return(vector<i32>(1i32, 2i32))
}

[effects(heap_alloc), return<int>]
main() {
  return(/vector/count(wrapVector()))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("vector namespaced count wrapper vector target without helper reports unknown target") {
  const std::string source = R"(
[effects(heap_alloc), return<vector<i32>>]
wrapVector() {
  return(vector<i32>(1i32, 2i32))
}

[effects(heap_alloc), return<int>]
main() {
  return(/vector/count(wrapVector()))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /vector/count") != std::string::npos);
}

TEST_CASE("vector namespaced count rejects named arguments as builtin alias") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(4i32, 5i32)}
  return(/vector/count([values] values))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /vector/count") != std::string::npos);
}

TEST_CASE("array namespaced vector count call rejects named arguments via unknown target") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(4i32, 5i32)}
  return(/array/count([values] values))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /array/count") != std::string::npos);
}

TEST_CASE("array namespaced vector count builtin alias call is rejected") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(4i32, 5i32)}
  return(/array/count(values))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /array/count") != std::string::npos);
}

TEST_CASE("stdlib namespaced vector capacity accepts named arguments through imported stdlib helper") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(4i32, 5i32)}
  return(/std/collections/vector/capacity([values] values))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("stdlib namespaced vector capacity requires imported stdlib helper or explicit definition") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(4i32, 5i32)}
  return(/std/collections/vector/capacity(values))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /std/collections/vector/capacity") != std::string::npos);
}

TEST_CASE("stdlib namespaced vector capacity accepts same-path helper on map target") {
  const std::string source = R"(
[return<int>]
/std/collections/vector/capacity([map<i32, i32>] values) {
  return(41i32)
}

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 2i32)}
  return(/std/collections/vector/capacity(values))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("vector namespaced capacity accepts same-path helper on map target") {
  const std::string source = R"(
[return<int>]
/vector/capacity([map<i32, i32>] values) {
  return(42i32)
}

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 2i32)}
  return(/vector/capacity(values))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("vector namespaced capacity accepts same-path helper on array target") {
  const std::string source = R"(
[return<int>]
/vector/capacity([array<i32>] values) {
  return(43i32)
}

[effects(heap_alloc), return<int>]
main() {
  [array<i32>] values{array<i32>(1i32, 2i32)}
  return(/vector/capacity(values))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("vector namespaced capacity accepts same-path helper on wrapper vector target") {
  const std::string source = R"(
[return<int>]
/vector/capacity([vector<i32>] values) {
  return(44i32)
}

[effects(heap_alloc), return<vector<i32>>]
wrapVector() {
  return(vector<i32>(1i32, 2i32))
}

[effects(heap_alloc), return<int>]
main() {
  return(/vector/capacity(wrapVector()))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("vector namespaced capacity wrapper vector target without helper reports unknown target") {
  const std::string source = R"(
[effects(heap_alloc), return<vector<i32>>]
wrapVector() {
  return(vector<i32>(1i32, 2i32))
}

[effects(heap_alloc), return<int>]
main() {
  return(/vector/capacity(wrapVector()))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /vector/capacity") != std::string::npos);
}

TEST_CASE("vector namespaced capacity rejects named arguments as builtin alias") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(4i32, 5i32)}
  return(/vector/capacity([values] values))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /vector/capacity") != std::string::npos);
}

TEST_CASE("array namespaced vector capacity alias rejects named arguments with unknown-target diagnostics") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(4i32, 5i32)}
  return(/array/capacity([values] values))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /array/capacity") != std::string::npos);
}

TEST_CASE("array namespaced vector capacity alias call is rejected") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(4i32, 5i32)}
  return(/array/capacity(values))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /array/capacity") != std::string::npos);
}

TEST_CASE("namespaced vector count and capacity allow named args for user helper receiver") {
  const std::string source = R"(
Counter {}

[return<int>]
/Counter/count([Counter] values) {
  return(4i32)
}

[return<int>]
/Counter/capacity([Counter] values) {
  return(5i32)
}

[return<int>]
main() {
  [Counter] values{Counter()}
  return(plus(/std/collections/vector/count([values] values),
              /vector/capacity([values] values)))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /vector/capacity") != std::string::npos);
}

TEST_CASE("stdlib namespaced vector access helper accepts named arguments through imported stdlib helper") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(4i32, 5i32)}
  return(plus(/std/collections/vector/at([values] values, [index] 0i32),
              /std/collections/vector/at_unsafe([index] 1i32, [values] values)))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("stdlib namespaced vector access helper requires imported stdlib helper") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(4i32, 5i32)}
  return(/std/collections/vector/at(values, 0i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /std/collections/vector/at") != std::string::npos);
}

TEST_CASE("stdlib namespaced vector at_unsafe helper requires imported stdlib helper") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(4i32, 5i32)}
  return(/std/collections/vector/at_unsafe(values, 0i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /std/collections/vector/at_unsafe") != std::string::npos);
}

TEST_CASE("vector namespaced access helper rejects named arguments") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(4i32, 5i32)}
  return(/vector/at([values] values, [index] 0i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /vector/at") != std::string::npos);
}

TEST_CASE("vector namespaced at_unsafe helper rejects named arguments") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(4i32, 5i32)}
  return(/vector/at_unsafe([values] values, [index] 0i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /vector/at_unsafe") != std::string::npos);
}

TEST_CASE("stdlib namespaced vector capacity keeps non-vector target diagnostics") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 2i32)}
  return(/std/collections/vector/capacity(values))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /std/collections/vector/capacity") != std::string::npos);
}

TEST_CASE("vector namespaced capacity keeps non-vector target diagnostics") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 2i32)}
  return(/vector/capacity(values))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /vector/capacity") != std::string::npos);
}

TEST_CASE("vector namespaced capacity array target without helper reports unknown target") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [array<i32>] values{array<i32>(1i32, 2i32)}
  return(/vector/capacity(values))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /vector/capacity") != std::string::npos);
}

TEST_CASE("namespaced vector helper with named arguments is statement-only in expressions") {
  struct HelperCase {
    const char *name;
    const char *args;
  };
  const HelperCase helpers[] = {
      {"push", "[values] values, [value] 2i32"},
      {"reserve", "[values] values, [capacity] 8i32"},
      {"remove_at", "[values] values, [index] 0i32"},
      {"remove_swap", "[values] values, [index] 0i32"},
  };
  for (const auto &helper : helpers) {
    CAPTURE(helper.name);
    const std::string source =
        "[effects(heap_alloc), return<int>]\n"
        "main() {\n"
        "  [vector<i32> mut] values{vector<i32>(1i32)}\n"
        "  return(/vector/" +
        std::string(helper.name) + "(" + helper.args + "))\n"
        "}\n";
    std::string error;
    CHECK_FALSE(validateProgram(source, "/main", error));
    CHECK(error.find("unknown call target: /vector/" + std::string(helper.name)) != std::string::npos);
  }
}

TEST_CASE("array namespaced vector helper with named arguments is statement-only in expressions") {
  struct HelperCase {
    const char *name;
    const char *args;
  };
  const HelperCase helpers[] = {
      {"push", "[values] values, [value] 2i32"},
      {"reserve", "[values] values, [capacity] 8i32"},
      {"remove_at", "[values] values, [index] 0i32"},
      {"remove_swap", "[values] values, [index] 0i32"},
  };
  for (const auto &helper : helpers) {
    CAPTURE(helper.name);
    const std::string source =
        "[effects(heap_alloc), return<int>]\n"
        "main() {\n"
        "  [vector<i32> mut] values{vector<i32>(1i32)}\n"
        "  return(/array/" +
        std::string(helper.name) + "(" + helper.args + "))\n"
        "}\n";
    std::string error;
    CHECK_FALSE(validateProgram(source, "/main", error));
    CHECK(error.find("only supported as a statement") != std::string::npos);
  }
}

TEST_CASE("user definition named push is not treated as builtin vector helper") {
  const std::string source = R"(
[return<void>]
push([i32] value) {
}

[return<int>]
main() {
  push([value] 1i32)
  return(0i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("struct push helper statement call is not treated as builtin vector helper") {
  const std::string source = R"(
Counter {
  [i32 mut] value{0i32}
  push([i32] next) {
    assign(this.value, next)
  }
}

[return<int>]
main() {
  [Counter mut] counter{Counter()}
  counter.push(7i32)
  return(counter.value)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("struct push helper expression call is not treated as builtin vector helper") {
  const std::string source = R"(
Counter {
  [i32 mut] value{0i32}
  [return<int>]
  push([i32] next) {
    assign(this.value, next)
    return(this.value)
  }
}

[return<int>]
main() {
  [Counter mut] counter{Counter()}
  return(counter.push(7i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("struct push helper assign to immutable field is rejected") {
  const std::string source = R"(
Counter {
  [i32] value{0i32}
  push([i32] next) {
    assign(this.value, next)
  }
}

[return<int>]
main() {
  [Counter mut] counter{Counter()}
  counter.push(7i32)
  return(counter.value)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("assign target must be a mutable binding") != std::string::npos);
}

TEST_CASE("struct push helper wrapper temporary statement call is not treated as builtin vector helper") {
  const std::string source = R"(
Counter {
  push([i32] next) {
  }
}

[return<Counter>]
makeCounter() {
  [Counter] counter{Counter()}
  return(counter)
}

[return<int>]
main() {
  makeCounter().push(7i32)
  return(0i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("struct push helper wrapper temporary expression call is not treated as builtin vector helper") {
  const std::string source = R"(
Counter {
  [return<int>]
  push([i32] next) {
    return(next)
  }
}

[return<Counter>]
makeCounter() {
  [Counter] counter{Counter()}
  return(counter)
}

[return<int>]
main() {
  return(makeCounter().push(7i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("user definition named push can be used in expressions") {
  const std::string source = R"(
[return<int>]
push([i32] value) {
  return(value)
}

[return<int>]
main() {
  return(push(3i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("vector helper call-form expression uses user-defined method target") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/vector/push([vector<i32> mut] values, [i32] value) {
  return(value)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32)}
  return(push(values, 3i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("vector helper call-form expression user shadow accepts positional reordered arguments") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/vector/push([vector<i32> mut] values, [i32] value) {
  return(value)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32)}
  return(push(3i32, values))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("vector helper call-form expression user shadow accepts bool positional reordered arguments") {
  const std::string source = R"(
[effects(heap_alloc), return<bool>]
/vector/push([vector<i32> mut] values, [bool] value) {
  return(value)
}

[effects(heap_alloc), return<bool>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32)}
  return(push(true, values))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("vector helper call-form expression stays statement-only without user helper") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32)}
  return(push(values, 3i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("push is only supported as a statement") != std::string::npos);
}

TEST_CASE("vector helper expression skips temp-leading positional receiver probing") {
  const std::string source = R"(
Counter {
}

[return<Counter>]
makeCounter() {
  return(Counter())
}

[effects(heap_alloc), return<Counter>]
/vector/push([vector<Counter> mut] values, [Counter] value) {
  return(value)
}

[effects(heap_alloc), return<Counter>]
main() {
  [vector<Counter> mut] values{vector<Counter>(Counter())}
  return(push(makeCounter(), values))
}
  )";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("push is only supported as a statement") != std::string::npos);
}

TEST_CASE("vector helper call-form expression user shadow accepts named arguments") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/vector/push([vector<i32> mut] values, [i32] value) {
  return(value)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32)}
  return(push([value] 3i32, [values] values))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("vector helper call-form expression user shadow keeps duplicate named diagnostics") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/vector/push([vector<i32> mut] values, [i32] value) {
  return(value)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32)}
  return(push([values] values, [values] values))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("duplicate named argument: values") != std::string::npos);
}

TEST_CASE("vector helper call-form expression prefers labeled named receiver") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/vector/push([vector<i32> mut] values, [string] value) {
  return(11i32)
}

[effects(heap_alloc), return<int>]
/string/push([vector<i32> mut] values, [string] value) {
  return(99i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32)}
  [string] payload{"payload"raw_utf8}
  return(push([value] payload, [values] values))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("vector helper call-form expression infers auto binding from labeled receiver helper") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/vector/push([vector<i32> mut] values, [string] value) {
  return(11i32)
}

[effects(heap_alloc), return<int>]
/string/push([vector<i32> mut] values, [string] value) {
  return(99i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32)}
  [string] payload{"payload"raw_utf8}
  [auto] inferred{push([value] payload, [values] values)}
  return(inferred)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("vector stdlib namespaced helper auto inference follows alias precedence") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/vector/push([vector<i32> mut] values, [string] value) {
  return(11i32)
}

[effects(heap_alloc), return<bool>]
/std/collections/vector/push([vector<i32> mut] values, [string] value) {
  return(false)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32)}
  [string] payload{"tag"raw_utf8}
  [auto] inferred{/std/collections/vector/push([value] payload, [values] values)}
  return(inferred)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("vector stdlib namespaced helper auto inference keeps explicit parameter order") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/vector/push([vector<i32> mut] values, [string] value) {
  return(11i32)
}

[effects(heap_alloc), return<bool>]
/std/collections/vector/push([vector<i32> mut] values, [string] value) {
  return(false)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32)}
  [string] payload{"tag"raw_utf8}
  [auto] inferred{/std/collections/vector/push(payload, values)}
  return(inferred)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("argument type mismatch for /std/collections/vector/push parameter values") !=
        std::string::npos);
}

TEST_CASE("vector stdlib namespaced helper keeps explicit parameter diagnostics over return mismatch") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/vector/push([vector<i32> mut] values, [string] value) {
  return(11i32)
}

[effects(heap_alloc), return<bool>]
/std/collections/vector/push([vector<i32> mut] values, [string] value) {
  return(false)
}

[effects(heap_alloc), return<bool>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32)}
  [string] payload{"tag"raw_utf8}
  [auto] inferred{/std/collections/vector/push(payload, values)}
  return(inferred)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("argument type mismatch for /std/collections/vector/push parameter values") !=
        std::string::npos);
}

TEST_CASE("vector stdlib namespaced push auto inference uses canonical helper definition") {
  const std::string source = R"(
[effects(heap_alloc), return<bool>]
/std/collections/vector/push([vector<i32> mut] values, [string] value) {
  return(false)
}

[effects(heap_alloc), return<bool>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32)}
  [string] payload{"tag"raw_utf8}
  [auto] inferred{/std/collections/vector/push([value] payload, [values] values)}
  return(inferred)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("vector namespaced count-capacity call-form requires explicit vector helper definitions") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(4i32, 5i32)}
  [auto] inferredCount{/vector/count(values)}
  [auto] inferredCapacity{/std/collections/vector/capacity(values)}
  return(plus(inferredCount, inferredCapacity))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /vector/count") != std::string::npos);
}

TEST_CASE("vector namespaced count auto inference rejects map target without helper") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 2i32)}
  [auto] inferred{/vector/count(values)}
  return(inferred)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /vector/count") != std::string::npos);
}

TEST_CASE("vector namespaced capacity auto inference rejects map target without helper") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 2i32)}
  [auto] inferred{/vector/capacity(values)}
  return(inferred)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /vector/capacity") != std::string::npos);
}

TEST_CASE("vector stdlib namespaced count helper auto inference keeps canonical precedence") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/vector/count([vector<i32>] values) {
  return(12i32)
}

[effects(heap_alloc), return<bool>]
/std/collections/vector/count([vector<i32>] values) {
  return(false)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32)}
  [auto] inferred{/std/collections/vector/count(values)}
  return(inferred)
}
  )";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("return type mismatch") != std::string::npos);
  CHECK(error.find("expected i32") != std::string::npos);
}

TEST_CASE("vector stdlib namespaced count helper auto inference keeps inferred return mismatch diagnostics") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/vector/count([vector<i32>] values) {
  return(12i32)
}

[effects(heap_alloc), return<bool>]
/std/collections/vector/count([vector<i32>] values) {
  return(false)
}

[effects(heap_alloc), return<bool>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32)}
  [auto] inferred{/std/collections/vector/count(values)}
  return(inferred)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("vector stdlib namespaced count helper auto inference falls back to canonical helper return") {
  const std::string source = R"(
[effects(heap_alloc), return<bool>]
/std/collections/vector/count([vector<i32>] values) {
  return(false)
}

[effects(heap_alloc), return<bool>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32)}
  [auto] inferred{/std/collections/vector/count(values)}
  return(inferred)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("vector stdlib namespaced count auto inference rejects compatibility alias precedence for non-builtin arity") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/vector/count([vector<i32>] values, [bool] marker) {
  return(12i32)
}

[effects(heap_alloc), return<bool>]
/std/collections/vector/count([vector<i32>] values, [bool] marker) {
  return(false)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32)}
  [auto] inferred{/std/collections/vector/count(values, true)}
  return(inferred)
}
  )";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("return type mismatch") != std::string::npos);
  CHECK(error.find("expected i32") != std::string::npos);
}

TEST_CASE("vector stdlib namespaced count auto inference keeps non-builtin arity mismatch diagnostics") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/vector/count([vector<i32>] values, [bool] marker) {
  return(12i32)
}

[effects(heap_alloc), return<bool>]
/std/collections/vector/count([vector<i32>] values, [bool] marker) {
  return(false)
}

[effects(heap_alloc), return<bool>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32)}
  [auto] inferred{/std/collections/vector/count(values, true)}
  return(inferred)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("vector stdlib namespaced count auto inference non-builtin arity falls back to canonical helper return") {
  const std::string source = R"(
[effects(heap_alloc), return<bool>]
/std/collections/vector/count([vector<i32>] values, [bool] marker) {
  return(false)
}

[effects(heap_alloc), return<bool>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32)}
  [auto] inferred{/std/collections/vector/count(values, true)}
  return(inferred)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("vector stdlib namespaced count expression keeps canonical precedence") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/vector/count([vector<i32>] values) {
  return(12i32)
}

[effects(heap_alloc), return<bool>]
/std/collections/vector/count([vector<i32>] values) {
  return(false)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32)}
  return(/std/collections/vector/count(values))
}
  )";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("return type mismatch") != std::string::npos);
  CHECK(error.find("expected i32") != std::string::npos);
}

TEST_CASE("vector stdlib namespaced count expression keeps return mismatch diagnostics") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/vector/count([vector<i32>] values) {
  return(12i32)
}

[effects(heap_alloc), return<bool>]
/std/collections/vector/count([vector<i32>] values) {
  return(false)
}

[effects(heap_alloc), return<bool>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32)}
  return(/std/collections/vector/count(values))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("vector stdlib namespaced count expression falls back to canonical helper return") {
  const std::string source = R"(
[effects(heap_alloc), return<bool>]
/std/collections/vector/count([vector<i32>] values) {
  return(false)
}

[effects(heap_alloc), return<bool>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32)}
  return(/std/collections/vector/count(values))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("vector stdlib namespaced count expression rejects compatibility alias precedence for non-builtin arity") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/vector/count([vector<i32>] values, [bool] marker) {
  return(12i32)
}

[effects(heap_alloc), return<bool>]
/std/collections/vector/count([vector<i32>] values, [bool] marker) {
  return(false)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32)}
  return(/std/collections/vector/count(values, true))
}
  )";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("return type mismatch") != std::string::npos);
  CHECK(error.find("expected i32") != std::string::npos);
}

TEST_CASE("vector stdlib namespaced count expression keeps non-builtin arity mismatch diagnostics") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/vector/count([vector<i32>] values, [bool] marker) {
  return(12i32)
}

[effects(heap_alloc), return<bool>]
/std/collections/vector/count([vector<i32>] values, [bool] marker) {
  return(false)
}

[effects(heap_alloc), return<bool>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32)}
  return(/std/collections/vector/count(values, true))
}
  )";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("vector stdlib namespaced count expression non-builtin arity falls back to canonical helper return") {
  const std::string source = R"(
[effects(heap_alloc), return<bool>]
/std/collections/vector/count([vector<i32>] values, [bool] marker) {
  return(false)
}

[effects(heap_alloc), return<bool>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32)}
  return(/std/collections/vector/count(values, true))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("vector stdlib namespaced count expression rejects non-builtin compatibility alias fallback") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/vector/count([vector<i32>] values, [bool] marker) {
  return(27i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32)}
  return(/std/collections/vector/count(values, true))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /std/collections/vector/count") != std::string::npos);
}

TEST_CASE("vector stdlib namespaced count expression compatibility alias fallback reports unknown target") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/vector/count([vector<i32>] values, [bool] marker) {
  return(27i32)
}

[effects(heap_alloc), return<bool>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32)}
  return(/std/collections/vector/count(values, true))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /std/collections/vector/count") != std::string::npos);
}

TEST_CASE("vector stdlib namespaced count auto inference rejects non-builtin compatibility arity fallback") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/vector/count([vector<i32>] values, [bool] marker) {
  return(27i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32)}
  [auto] inferred{/std/collections/vector/count(values, true)}
  return(inferred)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("vector namespaced count non-builtin arity rejects array helper fallback") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/array/count([vector<i32>] values, [bool] marker) {
  return(31i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32)}
  return(/vector/count(values, true))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /vector/count") != std::string::npos);
}

TEST_CASE("vector namespaced count non-builtin arity diagnostics report builtin mismatch") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/array/count([vector<i32>] values, [bool] marker) {
  return(31i32)
}

[effects(heap_alloc), return<bool>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32)}
  return(/vector/count(values, true))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /vector/count") != std::string::npos);
}

TEST_CASE("vector namespaced count auto inference non-builtin arity rejects array helper fallback") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/array/count([vector<i32>] values, [bool] marker) {
  return(31i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32)}
  [auto] inferred{/vector/count(values, true)}
  return(inferred)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /vector/count") != std::string::npos);
}

TEST_CASE("map stdlib namespaced count expression keeps canonical helper return precedence") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/map/count([map<i32, i32>] values, [bool] marker) {
  return(12i32)
}

[effects(heap_alloc), return<bool>]
/std/collections/map/count([map<i32, i32>] values, [bool] marker) {
  return(false)
}

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 2i32)}
  return(/std/collections/map/count(values, true))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("return type mismatch") != std::string::npos);
  CHECK(error.find("expected i32") != std::string::npos);
}

TEST_CASE("map stdlib namespaced count expression succeeds when canonical helper matches return type") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/map/count([map<i32, i32>] values, [bool] marker) {
  return(12i32)
}

[effects(heap_alloc), return<bool>]
/std/collections/map/count([map<i32, i32>] values, [bool] marker) {
  return(false)
}

[effects(heap_alloc), return<bool>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 2i32)}
  return(/std/collections/map/count(values, true))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("map stdlib namespaced count expression non-builtin arity falls back to canonical helper return") {
  const std::string source = R"(
[effects(heap_alloc), return<bool>]
/std/collections/map/count([map<i32, i32>] values, [bool] marker) {
  return(false)
}

[effects(heap_alloc), return<bool>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 2i32)}
  return(/std/collections/map/count(values, true))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("map stdlib namespaced count expression falls back to map alias helper") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/map/count([map<i32, i32>] values, [bool] marker) {
  return(41i32)
}

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 2i32)}
  return(/std/collections/map/count(values, true))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("map stdlib namespaced count expression fallback keeps map alias diagnostics") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/map/count([map<i32, i32>] values) {
  return(41i32)
}

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 2i32)}
  return(/std/collections/map/count(values, true))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("argument count mismatch for /map/count") != std::string::npos);
}

TEST_CASE("map compatibility count call keeps explicit alias precedence with canonical templated helper present") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/map/count([map<i32, i32>] values) {
  return(41i32)
}

[effects(heap_alloc), return<bool>]
/std/collections/map/count<K, V>([map<K, V>] values, [bool] marker) {
  return(false)
}

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 2i32)}
  return(/map/count(values))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("map compatibility count call keeps alias mismatch diagnostics with canonical templated helper present") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/map/count([map<i32, i32>] values) {
  return(41i32)
}

[effects(heap_alloc), return<bool>]
/std/collections/map/count<K, V>([map<K, V>] values, [bool] marker) {
  return(false)
}

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 2i32)}
  return(/map/count(values, true))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("argument count mismatch for /map/count") != std::string::npos);
}

TEST_CASE("map stdlib namespaced count expression infers templated alias helper fallback") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/map/count<K, V>([map<K, V>] values, [bool] marker) {
  return(41i32)
}

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 2i32)}
  return(/std/collections/map/count(values, true))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("argument count mismatch for builtin count") != std::string::npos);
}

TEST_CASE("map compatibility count call does not inherit canonical templated helper") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/std/collections/map/count<K, V>([map<K, V>] values, [bool] marker) {
  return(41i32)
}

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 2i32)}
  return(/map/count(values, true))
}
  )";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK_FALSE(error.empty());
}

TEST_CASE("map compatibility explicit-template count call keeps alias precedence with canonical templated helper") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/map/count<K, V>([map<K, V>] values, [bool] marker) {
  return(41i32)
}

[effects(heap_alloc), return<bool>]
/std/collections/map/count<K, V>([map<K, V>] values, [bool] marker) {
  return(false)
}

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 2i32)}
  return(/map/count<i32, i32>(values, true))
}
  )";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("map compatibility explicit-template count call keeps non-templated alias diagnostics") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/map/count([map<i32, i32>] values, [bool] marker) {
  return(41i32)
}

[effects(heap_alloc), return<bool>]
/std/collections/map/count<K, V>([map<K, V>] values, [bool] marker) {
  return(false)
}

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 2i32)}
  return(/map/count<i32, i32>(values, true))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("template arguments are only supported on templated definitions: /map/count") != std::string::npos);
}

TEST_CASE("map compatibility explicit-template count method keeps non-templated alias diagnostics") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/map/count([map<i32, i32>] values, [bool] marker) {
  return(41i32)
}

[effects(heap_alloc), return<bool>]
/std/collections/map/count<K, V>([map<K, V>] values, [bool] marker) {
  return(false)
}

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 2i32)}
  return(values.count<i32, i32>(true))
}
  )";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK_FALSE(error.empty());
}

TEST_CASE("wrapper reference templated map count method rejects without explicit alias") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/Reference/count([Reference</std/collections/map<i32, i32>>] self) {
  return(7i32)
}

[effects(heap_alloc), return<int>]
/std/collections/map/count<K, V>([Reference</std/collections/map<K, V>>] values, [bool] marker) {
  return(41i32)
}

[effects(heap_alloc), return<Reference</std/collections/map<i32, i32>>>]
borrowValues([Reference</std/collections/map<i32, i32>>] values) {
  return(values)
}

[effects(heap_alloc), return<int>]
main() {
  [/std/collections/map<i32, i32>] values{map<i32, i32>(1i32, 2i32)}
  return(borrowValues(location(values)).count<i32, i32>(true))
}
  )";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK_FALSE(error.empty());
}

TEST_CASE("wrapper reference templated map count method keeps canonical diagnostics") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/Reference/count<K, V>([Reference</std/collections/map<K, V>>] self, [bool] marker) {
  return(7i32)
}

[effects(heap_alloc), return<bool>]
/std/collections/map/count([Reference</std/collections/map<i32, i32>>] values, [bool] marker) {
  return(false)
}

[effects(heap_alloc), return<Reference</std/collections/map<i32, i32>>>]
borrowValues([Reference</std/collections/map<i32, i32>>] values) {
  return(values)
}

[effects(heap_alloc), return<int>]
main() {
  [/std/collections/map<i32, i32>] values{map<i32, i32>(1i32, 2i32)}
  return(borrowValues(location(values)).count<i32, i32>(true))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("return type mismatch: expected i32") != std::string::npos);
}

TEST_CASE("map slash-path explicit-template count method stays on unknown method diagnostic") {
  const std::string source = R"(
[effects(heap_alloc), return<bool>]
/std/collections/map/count<K, V>([map<K, V>] values, [bool] marker) {
  return(false)
}

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 2i32)}
  return(values./map/count<i32, i32>(true))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown method: /map/count") != std::string::npos);
}

TEST_CASE("map canonical slash-path explicit-template access method stays on unknown method diagnostic") {
  const std::string source = R"(
[effects(heap_alloc), return<i32>]
/std/collections/map/at<K, V>([map<K, V>] values, [i32] key) {
  return(key)
}

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 2i32)}
  return(values./std/collections/map/at<i32, i32>(1i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown method: /map/at") != std::string::npos);
}

TEST_CASE("map canonical explicit-template count call keeps canonical non-templated diagnostics") {
  const std::string source = R"(
[effects(heap_alloc), return<bool>]
/std/collections/map/count([map<i32, i32>] values, [bool] marker) {
  return(false)
}

[effects(heap_alloc), return<int>]
/map/count<K, V>([map<K, V>] values, [bool] marker) {
  return(41i32)
}

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 2i32)}
  return(/std/collections/map/count<i32, i32>(values, true))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("template arguments are only supported on templated definitions: /std/collections/map/count") !=
        std::string::npos);
}

TEST_CASE("map canonical implicit-template count call keeps canonical non-templated diagnostics") {
  const std::string source = R"(
[effects(heap_alloc), return<bool>]
/std/collections/map/count([map<i32, i32>] values, [bool] marker) {
  return(false)
}

[effects(heap_alloc), return<int>]
/map/count<K, V>([map<K, V>] values) {
  return(41i32)
}

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 2i32)}
  return(/std/collections/map/count(values))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("argument count mismatch for /std/collections/map/count") != std::string::npos);
}

TEST_CASE("map canonical implicit-template count call infers wrapper slash return envelope") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/std/collections/map/count<K, V>([map<K, V>] values, [bool] marker) {
  return(41i32)
}

[effects(heap_alloc), return</std/collections/map<i32, i32>>]
wrapValues() {
  return(map<i32, i32>(1i32, 2i32))
}

[effects(heap_alloc), return<int>]
main() {
  return(/std/collections/map/count(wrapValues(), true))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("map canonical implicit-template count wrapper slash return keeps canonical diagnostics") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/std/collections/map/count<K, V>([map<K, V>] values, [bool] marker) {
  return(41i32)
}

[effects(heap_alloc), return</std/collections/map<i32, i32>>]
wrapValues() {
  return(map<i32, i32>(1i32, 2i32))
}

[effects(heap_alloc), return<int>]
main() {
  return(/std/collections/map/count(wrapValues(), 1i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("argument type mismatch for /std/collections/map/count") != std::string::npos);
}

TEST_CASE("map canonical wrapper auto local preserves collection template info") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return</std/collections/map<i32, i32>>]
wrapValues() {
  return(map<i32, i32>(1i32, 4i32, 2i32, 5i32))
}

[effects(heap_alloc), return<int>]
main() {
  [auto] values{wrapValues()}
  return(plus(plus(/std/collections/map/count(values), /std/collections/map/at(values, 1i32)),
              plus(/std/collections/map/at_unsafe(values, 2i32), values[1i32])))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("map canonical wrapper auto local keeps builtin count diagnostics") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return</std/collections/map<i32, i32>>]
wrapValues() {
  return(map<i32, i32>(1i32, 4i32))
}

[effects(heap_alloc), return<int>]
main() {
  [auto] values{wrapValues()}
  return(/std/collections/map/count(values, true))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("argument count mismatch for /std/collections/map/count") != std::string::npos);
}

TEST_CASE("map canonical reference wrapper auto local preserves collection template info") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<Reference</std/collections/map<i32, i32>>>]
borrowValues([Reference</std/collections/map<i32, i32>>] values) {
  return(values)
}

[effects(heap_alloc), return<int>]
main() {
  [/std/collections/map<i32, i32>] source{map<i32, i32>(1i32, 4i32, 2i32, 5i32)}
  [auto] values{borrowValues(location(source))}
  return(plus(plus(/std/collections/map/count(values), /std/collections/map/at(values, 1i32)),
              plus(/std/collections/map/at_unsafe(values, 2i32), values[1i32])))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("map canonical reference wrapper auto local keeps key diagnostics") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<Reference</std/collections/map<i32, i32>>>]
borrowValues([Reference</std/collections/map<i32, i32>>] values) {
  return(values)
}

[effects(heap_alloc), return<int>]
main() {
  [/std/collections/map<i32, i32>] source{map<i32, i32>(1i32, 4i32)}
  [auto] values{borrowValues(location(source))}
  return(/std/collections/map/at(values, true))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("argument type mismatch for /std/collections/map/at") != std::string::npos);
}

TEST_CASE("canonical map borrowed receiver validates direct stdlib access") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<Reference</std/collections/map<i32, i32>>>]
borrowValues([Reference</std/collections/map<i32, i32>>] values) {
  return(values)
}

[effects(heap_alloc), return<int>]
main() {
  [/std/collections/map<i32, i32>] source{map<i32, i32>(1i32, 4i32, 2i32, 5i32)}
  return(/std/collections/map/at(borrowValues(location(source)), 1i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("canonical map borrowed receiver keeps parameter key diagnostics") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<Reference</std/collections/map<i32, i32>>>]
borrowValues([Reference</std/collections/map<i32, i32>>] values) {
  return(values)
}

[effects(heap_alloc), return<int>]
main() {
  [/std/collections/map<i32, i32>] source{map<i32, i32>(1i32, 4i32)}
  return(/std/collections/map/at(borrowValues(location(source)), true))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("argument type mismatch for /std/collections/map/at parameter key") !=
        std::string::npos);
}

TEST_CASE("canonical map borrowed receiver validates direct stdlib contains") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<Reference</std/collections/map<i32, i32>>>]
borrowValues([Reference</std/collections/map<i32, i32>>] values) {
  return(values)
}

[effects(heap_alloc), return<int>]
main() {
  [/std/collections/map<i32, i32>] source{map<i32, i32>(1i32, 4i32, 2i32, 5i32)}
  if(/std/collections/map/contains(borrowValues(location(source)), 1i32),
     then(){ return(1i32) },
     else(){ return(0i32) })
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("canonical map borrowed receiver keeps contains key diagnostics") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<Reference</std/collections/map<i32, i32>>>]
borrowValues([Reference</std/collections/map<i32, i32>>] values) {
  return(values)
}

[effects(heap_alloc), return<int>]
main() {
  [/std/collections/map<i32, i32>] source{map<i32, i32>(1i32, 4i32)}
  if(/std/collections/map/contains(borrowValues(location(source)), true),
     then(){ return(1i32) },
     else(){ return(0i32) })
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("argument type mismatch for /std/collections/map/contains") !=
        std::string::npos);
}

TEST_CASE("canonical map borrowed receiver validates direct stdlib tryAt") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<Reference</std/collections/map<i32, i32>>>]
borrowValues([Reference</std/collections/map<i32, i32>>] values) {
  return(values)
}

[effects(heap_alloc), return<int>]
main() {
  [/std/collections/map<i32, i32>] source{map<i32, i32>(1i32, 4i32, 2i32, 5i32)}
  [Result<i32, ContainerError>] found{/std/collections/map/tryAt(borrowValues(location(source)), 1i32)}
  return(0i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("canonical map borrowed receiver keeps tryAt key diagnostics") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<Reference</std/collections/map<i32, i32>>>]
borrowValues([Reference</std/collections/map<i32, i32>>] values) {
  return(values)
}

[effects(heap_alloc), return<int>]
main() {
  [/std/collections/map<i32, i32>] source{map<i32, i32>(1i32, 4i32)}
  [Result<i32, ContainerError>] found{/std/collections/map/tryAt(borrowValues(location(source)), true)}
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("argument type mismatch for /std/collections/map/tryAt") !=
        std::string::npos);
}

TEST_CASE("explicit canonical map binding keeps builtin helper validation") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [/std/collections/map<i32, i32>] values{map<i32, i32>(1i32, 4i32, 2i32, 5i32)}
  return(plus(plus(/std/collections/map/count(values), /std/collections/map/at(values, 1i32)),
              plus(/std/collections/map/at_unsafe(values, 2i32), values[1i32])))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("explicit canonical map parameter keeps builtin helper validation") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
sumValues([/std/collections/map<i32, i32>] values) {
  [auto] first{/std/collections/map/at(values, 1i32)}
  [auto] second{/std/collections/map/at_unsafe(values, 2i32)}
  [auto] total{/std/collections/map/count(values)}
  return(plus(plus(total, first), plus(second, values[1i32])))
}

[effects(heap_alloc), return<int>]
main() {
  return(sumValues(map<i32, i32>(1i32, 4i32, 2i32, 5i32)))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("explicit canonical map parameter keeps builtin key diagnostics") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
sumValues([/std/collections/map<i32, i32>] values) {
  return(/std/collections/map/at(values, true))
}

[effects(heap_alloc), return<int>]
main() {
  return(sumValues(map<i32, i32>(1i32, 4i32)))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("argument type mismatch for /std/collections/map/at") != std::string::npos);
}

TEST_CASE("explicit canonical map parameter keeps print statement string validation") {
  const std::string source = R"(
import /std/collections/*

[effects(io_out), return<int>]
showValue([/std/collections/map<i32, string>] values) {
  print_line(values[1i32])
  return(0i32)
}

[effects(io_out), return<int>]
main() {
  return(showValue(map<i32, string>(1i32, "hello"utf8)))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("canonical map reference parameter keeps print statement string validation") {
  const std::string source = R"(
import /std/collections/*

[effects(io_out), return<int>]
showValue([Reference</std/collections/map<i32, string>>] values) {
  print_line(values[1i32])
  return(0i32)
}

[effects(io_out), return<int>]
main() {
  [/std/collections/map<i32, string>] values{map<i32, string>(1i32, "hello"utf8)}
  [Reference</std/collections/map<i32, string>>] ref{location(values)}
  return(showValue(ref))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("wrapper-returned canonical map keeps print statement string validation") {
  const std::string source = R"(
import /std/collections/*

[return</std/collections/map<i32, string>>]
wrapMap() {
  return(map<i32, string>(1i32, "hello"utf8))
}

[effects(io_out), return<int>]
showValue() {
  print_line(wrapMap()[1i32])
  return(0i32)
}

[effects(io_out), return<int>]
main() {
  return(showValue())
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("wrapper-returned referenced canonical map keeps print statement string validation") {
  const std::string source = R"(
import /std/collections/*

[return<Reference</std/collections/map<i32, string>>>]
borrowMap([Reference</std/collections/map<i32, string>>] values) {
  return(values)
}

[effects(io_out), return<int>]
showValue() {
  [/std/collections/map<i32, string>] values{map<i32, string>(1i32, "hello"utf8)}
  print_line(borrowMap(location(values))[1i32])
  return(0i32)
}

[effects(io_out), return<int>]
main() {
  return(showValue())
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("canonical map reference parameter keeps if string branch compatibility") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
showValue([Reference</std/collections/map<i32, string>>] values) {
  [string] message{if(true, then(){ values[1i32] }, else(){ "fallback"utf8 })}
  return(count(message))
}

[return<int>]
main() {
  [/std/collections/map<i32, string>] values{map<i32, string>(1i32, "hello"utf8)}
  [Reference</std/collections/map<i32, string>>] ref{location(values)}
  return(showValue(ref))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("wrapper-returned canonical map keeps if string branch compatibility") {
  const std::string source = R"(
import /std/collections/*

[return</std/collections/map<i32, string>>]
wrapMap() {
  return(map<i32, string>(1i32, "hello"utf8))
}

[return<int>]
showValue() {
  [string] message{if(true, then(){ wrapMap()[1i32] }, else(){ "fallback"utf8 })}
  return(count(message))
}

[return<int>]
main() {
  return(showValue())
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("wrapper-returned referenced canonical map keeps if string branch compatibility") {
  const std::string source = R"(
import /std/collections/*

[return<Reference</std/collections/map<i32, string>>>]
borrowMap([Reference</std/collections/map<i32, string>>] values) {
  return(values)
}

[return<int>]
showValue() {
  [/std/collections/map<i32, string>] values{map<i32, string>(1i32, "hello"utf8)}
  [string] message{if(true, then(){ borrowMap(location(values))[1i32] }, else(){ "fallback"utf8 })}
  return(count(message))
}

[return<int>]
main() {
  return(showValue())
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("explicit canonical map parameter keeps pathspace string diagnostics") {
  const std::string source = R"(
[effects(pathspace_take), return<int>]
useValue([/std/collections/map<i32, i32>] values) {
  take(values[1i32])
  return(0i32)
}

[effects(pathspace_take), return<int>]
main() {
  return(useValue(map<i32, i32>(1i32, 4i32)))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("take requires string path argument") != std::string::npos);
}

TEST_CASE("canonical map reference parameter keeps pathspace string diagnostics") {
  const std::string source = R"(
[effects(pathspace_take), return<int>]
useValue([Reference</std/collections/map<i32, i32>>] values) {
  take(values[1i32])
  return(0i32)
}

[effects(pathspace_take), return<int>]
main() {
  [/std/collections/map<i32, i32>] values{map<i32, i32>(1i32, 4i32)}
  [Reference</std/collections/map<i32, i32>>] ref{location(values)}
  return(useValue(ref))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("take requires string path argument") != std::string::npos);
}

TEST_CASE("wrapper-returned canonical map keeps pathspace string diagnostics") {
  const std::string source = R"(
[effects(heap_alloc), return</std/collections/map<i32, i32>>]
wrapMap() {
  return(map<i32, i32>(1i32, 4i32))
}

[effects(pathspace_take), return<int>]
useValue() {
  take(wrapMap()[1i32])
  return(0i32)
}

[effects(pathspace_take), return<int>]
main() {
  return(useValue())
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("take requires string path argument") != std::string::npos);
}

TEST_CASE("wrapper-returned referenced canonical map keeps pathspace string diagnostics") {
  const std::string source = R"(
[return<Reference</std/collections/map<i32, i32>>>]
borrowMap([Reference</std/collections/map<i32, i32>>] values) {
  return(values)
}

[effects(pathspace_take), return<int>]
useValue() {
  [/std/collections/map<i32, i32>] values{map<i32, i32>(1i32, 4i32)}
  take(borrowMap(location(values))[1i32])
  return(0i32)
}

[effects(pathspace_take), return<int>]
main() {
  return(useValue())
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("take requires string path argument") != std::string::npos);
}

TEST_CASE("canonical map reference parameter keeps if string branch diagnostics") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
showValue([Reference</std/collections/map<i32, i32>>] values) {
  [string] message{if(true, then(){ values[1i32] }, else(){ "fallback"utf8 })}
  return(0i32)
}

[return<int>]
main() {
  [/std/collections/map<i32, i32>] values{map<i32, i32>(1i32, 4i32)}
  [Reference</std/collections/map<i32, i32>>] ref{location(values)}
  return(showValue(ref))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("if branches must return compatible types") != std::string::npos);
}

TEST_CASE("wrapper-returned canonical map keeps if string branch diagnostics") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return</std/collections/map<i32, i32>>]
wrapMap() {
  return(map<i32, i32>(1i32, 4i32))
}

[return<int>]
showValue() {
  [string] message{if(true, then(){ wrapMap()[1i32] }, else(){ "fallback"utf8 })}
  return(0i32)
}

[return<int>]
main() {
  return(showValue())
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("if branches must return compatible types") != std::string::npos);
}

TEST_CASE("wrapper-returned referenced canonical map keeps if string branch diagnostics") {
  const std::string source = R"(
import /std/collections/*

[return<Reference</std/collections/map<i32, i32>>>]
borrowMap([Reference</std/collections/map<i32, i32>>] values) {
  return(values)
}

[return<int>]
showValue() {
  [/std/collections/map<i32, i32>] values{map<i32, i32>(1i32, 4i32)}
  [string] message{if(true, then(){ borrowMap(location(values))[1i32] }, else(){ "fallback"utf8 })}
  return(0i32)
}

[return<int>]
main() {
  return(showValue())
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("if branches must return compatible types") != std::string::npos);
}

TEST_CASE("wrapper-returned referenced canonical map keeps builtin key diagnostics") {
  const std::string source = R"(
import /std/collections/*

[return<Reference</std/collections/map<i32, i32>>>]
borrowMap([Reference</std/collections/map<i32, i32>>] values) {
  return(values)
}

[return<int>]
main() {
  [/std/collections/map<i32, i32>] values{map<i32, i32>(1i32, 4i32)}
  return(borrowMap(location(values))[true])
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("at requires map key type i32") != std::string::npos);
}

TEST_CASE("wrapper-returned canonical map access count call auto inference keeps string helper shadow") {
  const std::string source = R"(
[return<bool>]
/string/count([string] values) {
  return(false)
}

[return<string>]
/std/collections/map/at([map<i32, string>] values, [i32] key) {
  return("abc"utf8)
}

[return</std/collections/map<i32, string>>]
wrapMap() {
  return(map<i32, string>(1i32, "hello"utf8))
}

[return<bool>]
main() {
  [auto] inferred{count(wrapMap()[1i32])}
  return(inferred)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("wrapper-returned canonical map method access count keeps builtin string helper shadow") {
  const std::string source = R"(
[return<int>]
/string/count([string] values) {
  return(91i32)
}

[return<i32>]
/std/collections/map/at([map<i32, string>] values, [i32] key) {
  return(7i32)
}

[return<i32>]
/std/collections/map/at_unsafe([map<i32, string>] values, [i32] key) {
  return(8i32)
}

[return</std/collections/map<i32, string>>]
wrapMap() {
  return(map<i32, string>(1i32, "hello"utf8))
}

[return<int>]
main() {
  return(plus(wrapMap().at(1i32).count(),
              wrapMap().at_unsafe(1i32).count()))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("wrapper-returned direct canonical map access count keeps primitive diagnostics") {
  const std::string source = R"(
[return<int>]
/string/count([string] values) {
  return(91i32)
}

[return<string>]
/std/collections/map/at([map<i32, i32>] values, [i32] key) {
  return("abc"utf8)
}

[return</std/collections/map<i32, i32>>]
wrapMap() {
  return(map<i32, i32>(1i32, 4i32))
}

[return<int>]
main() {
  return(count(/std/collections/map/at(wrapMap(), 1i32)))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("field-bound map access count keeps builtin string fallback") {
  const std::string source = R"(
[return<int>]
/string/count([string] values) {
  return(91i32)
}

Holder() {
  [map<i32, string>] values{map<i32, string>(1i32, "abc"utf8)}
}

[return<int>]
main() {
  [Holder] holder{Holder()}
  return(count(holder.values.at(1i32)))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("wrapper-returned canonical map method access count keeps primitive diagnostics") {
  const std::string source = R"(
[return<int>]
/string/count([string] values) {
  return(91i32)
}

[return<string>]
/std/collections/map/at([map<i32, i32>] values, [i32] key) {
  return("abc"utf8)
}

[return<string>]
/std/collections/map/at_unsafe([map<i32, i32>] values, [i32] key) {
  return("abc"utf8)
}

[return</std/collections/map<i32, i32>>]
wrapMap() {
  return(map<i32, i32>(1i32, 4i32))
}

[return<int>]
main() {
  return(plus(wrapMap().at(1i32).count(),
              wrapMap().at_unsafe(1i32).count()))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("argument type mismatch for /string/count parameter values: expected string") !=
        std::string::npos);
}

TEST_CASE("canonical vector access count call keeps builtin string helper shadow") {
  const std::string source = R"(
[return<bool>]
/string/count([string] values) {
  return(false)
}

[return<i32>]
/std/collections/vector/at([vector<string>] values, [i32] index) {
  return(7i32)
}

[effects(heap_alloc), return<bool>]
main() {
  [vector<string>] values{vector<string>("hello"utf8)}
  [auto] inferred{count(/std/collections/vector/at(values, 0i32))}
  return(inferred)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("canonical vector unsafe access count call keeps primitive diagnostics") {
  const std::string source = R"(
[return<int>]
/string/count([string] values) {
  return(91i32)
}

[return<string>]
/std/collections/vector/at_unsafe([vector<i32>] values, [i32] index) {
  return("abc"utf8)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32)}
  return(count(/std/collections/vector/at_unsafe(values, 0i32)))
}
  )";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("canonical vector method access count keeps builtin string fallback") {
  const std::string source = R"(
[return<int>]
/string/count([string] values) {
  return(91i32)
}

[return<i32>]
/std/collections/vector/at([vector<string>] values, [i32] index) {
  return(7i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<string>] values{vector<string>("hello"utf8)}
  return(values.at(0i32).count())
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("canonical vector unsafe method access count keeps primitive receiver diagnostics") {
  const std::string source = R"(
[return<int>]
/string/count([string] values) {
  return(91i32)
}

[return<string>]
/std/collections/vector/at_unsafe([vector<i32>] values, [i32] index) {
  return("abc"utf8)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32)}
  return(values.at_unsafe(0i32).count())
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown method: /i32/count") != std::string::npos);
}

TEST_CASE("slash-method vector access count keeps builtin string fallback") {
  const std::string source = R"(
[return<int>]
/string/count([string] values) {
  return(91i32)
}

[return<i32>]
/std/collections/vector/at([vector<string>] values, [i32] index) {
  return(7i32)
}

[return<i32>]
/std/collections/vector/at_unsafe([vector<string>] values, [i32] index) {
  return(7i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<string>] values{vector<string>("hello"utf8)}
  return(plus(values./vector/at(0i32).count(),
              values./std/collections/vector/at_unsafe(0i32).count()))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("slash-method vector access count keeps primitive diagnostics") {
  const std::string source = R"(
[return<int>]
/string/count([string] values) {
  return(91i32)
}

[return<string>]
/std/collections/vector/at([vector<i32>] values, [i32] index) {
  return("abc"utf8)
}

[return<string>]
/std/collections/vector/at_unsafe([vector<i32>] values, [i32] index) {
  return("abc"utf8)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32)}
  return(plus(values./vector/at(0i32).count(),
              values./std/collections/vector/at_unsafe(0i32).count()))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown method: /i32/count") != std::string::npos);
}

TEST_CASE("wrapper-returned vector access count keeps builtin string helper shadow") {
  const std::string source = R"(
[return<bool>]
/string/count([string] values) {
  return(false)
}

[return<i32>]
/std/collections/vector/at([vector<string>] values, [i32] index) {
  return(7i32)
}

[return<i32>]
/std/collections/vector/at_unsafe([vector<string>] values, [i32] index) {
  return(7i32)
}

[effects(heap_alloc), return<vector<string>>]
wrapValues() {
  return(vector<string>("hello"utf8))
}

[return<bool>]
main() {
  [auto] direct{count(/vector/at(wrapValues(), 0i32))}
  [auto] method{wrapValues().at_unsafe(0i32).count()}
  return(method)
}
  )";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /vector/at") != std::string::npos);
}

TEST_CASE("wrapper-returned vector access count keeps primitive diagnostics") {
  const std::string source = R"(
[return<int>]
/string/count([string] values) {
  return(91i32)
}

[return<string>]
/std/collections/vector/at([vector<i32>] values, [i32] index) {
  return("abc"utf8)
}

[return<string>]
/std/collections/vector/at_unsafe([vector<i32>] values, [i32] index) {
  return("abc"utf8)
}

[effects(heap_alloc), return<vector<i32>>]
wrapValues() {
  return(vector<i32>(1i32))
}

[return<int>]
main() {
  return(plus(count(/vector/at(wrapValues(), 0i32)),
              wrapValues().at_unsafe(0i32).count()))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown method: /i32/count") != std::string::npos);
}

TEST_CASE("wrapper-returned canonical map access count call auto inference keeps string helper mismatch diagnostics") {
  const std::string source = R"(
import /std/collections/*

[return<bool>]
/string/count([string] values) {
  return(false)
}

[return</std/collections/map<i32, string>>]
wrapMap() {
  return(map<i32, string>(1i32, "hello"utf8))
}

[return<int>]
main() {
  [auto] inferred{count(wrapMap()[1i32])}
  return(inferred)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("return type mismatch") != std::string::npos);
  CHECK(error.find("expected i32") != std::string::npos);
}

TEST_CASE("wrapper-returned referenced canonical map access count call auto inference keeps string helper shadow") {
  const std::string source = R"(
[return<bool>]
/string/count([string] values) {
  return(false)
}

[return<string>]
/std/collections/map/at([map<i32, string>] values, [i32] key) {
  return("abc"utf8)
}

[return<Reference</std/collections/map<i32, string>>>]
borrowMap([Reference</std/collections/map<i32, string>>] values) {
  return(values)
}

[return<bool>]
main() {
  [/std/collections/map<i32, string>] values{map<i32, string>(1i32, "hello"utf8)}
  [auto] inferred{count(borrowMap(location(values))[1i32])}
  return(inferred)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("wrapper-returned referenced canonical map access count call auto inference keeps string helper mismatch diagnostics") {
  const std::string source = R"(
import /std/collections/*

[return<bool>]
/string/count([string] values) {
  return(false)
}

[return<Reference</std/collections/map<i32, string>>>]
borrowMap([Reference</std/collections/map<i32, string>>] values) {
  return(values)
}

[return<int>]
main() {
  [/std/collections/map<i32, string>] values{map<i32, string>(1i32, "hello"utf8)}
  [auto] inferred{count(borrowMap(location(values))[1i32])}
  return(inferred)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("return type mismatch") != std::string::npos);
  CHECK(error.find("expected i32") != std::string::npos);
}

TEST_CASE("wrapper-returned slash-method map access count keeps primitive diagnostics") {
  const std::string source = R"(
[return<int>]
/string/count([string] values) {
  return(91i32)
}

[return<string>]
/std/collections/map/at([map<i32, i32>] values, [i32] key) {
  return("abc"utf8)
}

[return<string>]
/std/collections/map/at_unsafe([map<i32, i32>] values, [i32] key) {
  return("abc"utf8)
}

[effects(heap_alloc), return</std/collections/map<i32, i32>>]
wrapMap() {
  return(map<i32, i32>(1i32, 4i32))
}

[return<int>]
main() {
  return(count(wrapMap()./std/collections/map/at(1i32)))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown method: /map/at") != std::string::npos);
}

TEST_CASE("map stdlib namespaced count expression inferred template fallback keeps alias diagnostics") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/map/count<K, V>([map<K, V>] values, [bool] marker) {
  return(41i32)
}

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 2i32)}
  return(/std/collections/map/count(values, 1i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("argument count mismatch for builtin count") != std::string::npos);
}

TEST_CASE("map stdlib namespaced count auto inference keeps canonical helper return precedence") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/map/count([map<i32, i32>] values, [bool] marker) {
  return(12i32)
}

[effects(heap_alloc), return<bool>]
/std/collections/map/count([map<i32, i32>] values, [bool] marker) {
  return(false)
}

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 2i32)}
  [auto] inferred{/std/collections/map/count(values, true)}
  return(inferred)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("return type mismatch") != std::string::npos);
  CHECK(error.find("expected i32") != std::string::npos);
}

TEST_CASE("map stdlib namespaced count auto inference falls back to map alias helper") {
  const std::string source = R"(
[effects(heap_alloc), return<bool>]
/map/count([map<i32, i32>] values, [bool] marker) {
  return(false)
}

[effects(heap_alloc), return<bool>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 2i32)}
  [auto] inferred{/std/collections/map/count(values, true)}
  return(inferred)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("map stdlib namespaced count auto inference succeeds when canonical helper matches return type") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/map/count([map<i32, i32>] values, [bool] marker) {
  return(12i32)
}

[effects(heap_alloc), return<bool>]
/std/collections/map/count([map<i32, i32>] values, [bool] marker) {
  return(false)
}

[effects(heap_alloc), return<bool>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 2i32)}
  [auto] inferred{/std/collections/map/count(values, true)}
  return(inferred)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("map stdlib namespaced count statement falls back to map alias helper") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/map/count([map<i32, i32>] values, [bool] marker) {
  return(41i32)
}

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 2i32)}
  /std/collections/map/count(values, true)
  return(0i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("map stdlib namespaced count auto inference non-builtin arity falls back to canonical helper return") {
  const std::string source = R"(
[effects(heap_alloc), return<bool>]
/std/collections/map/count([map<i32, i32>] values, [bool] marker) {
  return(false)
}

[effects(heap_alloc), return<bool>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 2i32)}
  [auto] inferred{/std/collections/map/count(values, true)}
  return(inferred)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("vector stdlib namespaced capacity expression keeps canonical precedence") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/vector/capacity([vector<i32>] values) {
  return(12i32)
}

[effects(heap_alloc), return<bool>]
/std/collections/vector/capacity([vector<i32>] values) {
  return(false)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32)}
  return(/std/collections/vector/capacity(values))
}
  )";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("return type mismatch") != std::string::npos);
  CHECK(error.find("expected i32") != std::string::npos);
}

TEST_CASE("vector stdlib namespaced capacity expression keeps return mismatch diagnostics") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/vector/capacity([vector<i32>] values) {
  return(12i32)
}

[effects(heap_alloc), return<bool>]
/std/collections/vector/capacity([vector<i32>] values) {
  return(false)
}

[effects(heap_alloc), return<bool>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32)}
  return(/std/collections/vector/capacity(values))
}
  )";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("vector stdlib namespaced capacity expression uses canonical helper definition") {
  const std::string source = R"(
[effects(heap_alloc), return<bool>]
/std/collections/vector/capacity([vector<i32>] values) {
  return(false)
}

[effects(heap_alloc), return<bool>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32)}
  return(/std/collections/vector/capacity(values))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("vector stdlib namespaced capacity expression rejects compatibility alias precedence for non-builtin arity") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/vector/capacity([vector<i32>] values, [bool] marker) {
  return(12i32)
}

[effects(heap_alloc), return<bool>]
/std/collections/vector/capacity([vector<i32>] values, [bool] marker) {
  return(false)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32)}
  return(/std/collections/vector/capacity(values, true))
}
  )";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("return type mismatch") != std::string::npos);
  CHECK(error.find("expected i32") != std::string::npos);
}

TEST_CASE("vector stdlib namespaced capacity expression keeps non-builtin arity mismatch diagnostics") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/vector/capacity([vector<i32>] values, [bool] marker) {
  return(12i32)
}

[effects(heap_alloc), return<bool>]
/std/collections/vector/capacity([vector<i32>] values, [bool] marker) {
  return(false)
}

[effects(heap_alloc), return<bool>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32)}
  return(/std/collections/vector/capacity(values, true))
}
  )";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("vector stdlib namespaced capacity expression uses canonical helper definition for non-builtin arity") {
  const std::string source = R"(
[effects(heap_alloc), return<bool>]
/std/collections/vector/capacity([vector<i32>] values, [bool] marker) {
  return(false)
}

[effects(heap_alloc), return<bool>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32)}
  return(/std/collections/vector/capacity(values, true))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("vector stdlib namespaced access expression follows alias precedence") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/vector/at([vector<i32>] values, [i32] index) {
  return(12i32)
}

[effects(heap_alloc), return<bool>]
/std/collections/vector/at([vector<i32>] values, [i32] index) {
  return(false)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32)}
  return(/std/collections/vector/at(values, 0i32))
}
  )";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("vector stdlib namespaced access expression reports alias return mismatch") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/vector/at([vector<i32>] values, [i32] index) {
  return(12i32)
}

[effects(heap_alloc), return<bool>]
/std/collections/vector/at([vector<i32>] values, [i32] index) {
  return(false)
}

[effects(heap_alloc), return<bool>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32)}
  return(/std/collections/vector/at(values, 0i32))
}
  )";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("return type mismatch") != std::string::npos);
  CHECK(error.find("expected bool") != std::string::npos);
}

TEST_CASE("vector stdlib namespaced access expression uses canonical helper definition") {
  const std::string source = R"(
[effects(heap_alloc), return<bool>]
/std/collections/vector/at([vector<i32>] values, [i32] index) {
  return(false)
}

[effects(heap_alloc), return<bool>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32)}
  return(/std/collections/vector/at(values, 0i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("vector stdlib namespaced access expression uses alias precedence for non-builtin arity") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/vector/at([vector<i32>] values) {
  return(12i32)
}

[effects(heap_alloc), return<bool>]
/std/collections/vector/at([vector<i32>] values) {
  return(false)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32)}
  return(/std/collections/vector/at(values))
}
  )";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("vector stdlib namespaced access expression reports alias mismatch for non-builtin arity") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/vector/at([vector<i32>] values) {
  return(12i32)
}

[effects(heap_alloc), return<bool>]
/std/collections/vector/at([vector<i32>] values) {
  return(false)
}

[effects(heap_alloc), return<bool>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32)}
  return(/std/collections/vector/at(values))
}
  )";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("return type mismatch") != std::string::npos);
  CHECK(error.find("expected bool") != std::string::npos);
}

TEST_CASE("vector stdlib namespaced access expression uses canonical helper definition for non-builtin arity") {
  const std::string source = R"(
[effects(heap_alloc), return<bool>]
/std/collections/vector/at([vector<i32>] values) {
  return(false)
}

[effects(heap_alloc), return<bool>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32)}
  return(/std/collections/vector/at(values))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("vector namespaced capacity auto inference keeps non-vector target diagnostics") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 2i32)}
  [auto] inferred{/std/collections/vector/capacity(values)}
  return(inferred)
}
  )";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /vector/capacity") != std::string::npos);
}

TEST_CASE("access helper call-form expression infers auto binding from labeled receiver helper") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/vector/get([vector<i32>] values, [string] index) {
  return(12i32)
}

[effects(heap_alloc), return<int>]
/string/get([string] values, [vector<i32>] index) {
  return(98i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32)}
  [string] index{"payload"raw_utf8}
  [auto] inferred{get([index] index, [values] values)}
  return(inferred)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("vector stdlib namespaced access helper auto inference follows alias precedence") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/vector/at([vector<i32>] values, [i32] index) {
  return(12i32)
}

[effects(heap_alloc), return<bool>]
/std/collections/vector/at([vector<i32>] values, [i32] index) {
  return(false)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32)}
  [auto] inferred{/std/collections/vector/at([index] 0i32, [values] values)}
  return(inferred)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("vector stdlib namespaced access helper auto inference reports alias mismatch") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/vector/at([vector<i32>] values, [i32] index) {
  return(12i32)
}

[effects(heap_alloc), return<bool>]
/std/collections/vector/at([vector<i32>] values, [i32] index) {
  return(false)
}

[effects(heap_alloc), return<bool>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32)}
  [auto] inferred{/std/collections/vector/at([index] 0i32, [values] values)}
  return(inferred)
}
  )";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("return type mismatch") != std::string::npos);
  CHECK(error.find("expected bool") != std::string::npos);
}

TEST_CASE("vector stdlib namespaced access helper auto inference uses canonical helper definition") {
  const std::string source = R"(
[effects(heap_alloc), return<bool>]
/std/collections/vector/at([vector<i32>] values, [i32] index) {
  return(false)
}

[effects(heap_alloc), return<bool>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32)}
  [auto] inferred{/std/collections/vector/at([index] 0i32, [values] values)}
  return(inferred)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("map stdlib namespaced access helper auto inference keeps receiver helper precedence") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/map/at([map<i32, i32>] values, [i32] key) {
  return(12i32)
}

[effects(heap_alloc), return<bool>]
/std/collections/map/at([map<i32, i32>] values, [i32] key) {
  return(false)
}

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 2i32)}
  [auto] inferred{/std/collections/map/at([index] 1i32, [values] values)}
  return(inferred)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("map stdlib namespaced access helper auto inference keeps inferred return mismatch diagnostics") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/map/at([map<i32, i32>] values, [i32] key) {
  return(12i32)
}

[effects(heap_alloc), return<bool>]
/std/collections/map/at([map<i32, i32>] values, [i32] key) {
  return(false)
}

[effects(heap_alloc), return<bool>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 2i32)}
  [auto] inferred{/std/collections/map/at([index] 1i32, [values] values)}
  return(inferred)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("return type mismatch") != std::string::npos);
  CHECK(error.find("expected bool") != std::string::npos);
}

TEST_CASE("map stdlib namespaced access helper auto inference falls back to canonical helper return") {
  const std::string source = R"(
[effects(heap_alloc), return<bool>]
/std/collections/map/at([map<i32, i32>] values, [i32] key) {
  return(false)
}

[effects(heap_alloc), return<bool>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 2i32)}
  [auto] inferred{/std/collections/map/at([index] 1i32, [values] values)}
  return(inferred)
}
  )";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
}

TEST_CASE("stdlib namespaced map at validates without imported stdlib helper") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 2i32)}
  return(/std/collections/map/at(values, 1i32))
}
  )";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
}

TEST_CASE("stdlib namespaced map at_unsafe validates without imported stdlib helper") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 2i32)}
  [auto] inferred{/std/collections/map/at_unsafe(values, 1i32)}
  return(inferred)
}
  )";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
}

TEST_CASE("soa access helper call-form expression infers auto binding from labeled receiver helper") {
  const std::string source = R"(
Particle() {
  [i32] x{1i32}
}

[effects(heap_alloc), return<int>]
/soa_vector/get([soa_vector<Particle>] values, [vector<i32>] index) {
  return(13i32)
}

[effects(heap_alloc), return<bool>]
/vector/get([vector<i32>] values, [soa_vector<Particle>] index) {
  return(false)
}

[effects(heap_alloc), return<int>]
main() {
  [soa_vector<Particle>] values{soa_vector<Particle>()}
  [vector<i32>] index{vector<i32>(0i32)}
  [auto] inferred{get([index] index, [values] values)}
  return(inferred)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("vector helper call-form expression builtin stays statement-only with named arguments") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32)}
  return(push([values] values, [value] 3i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("only supported as a statement") != std::string::npos);
}

TEST_CASE("vector helper statement call-form user shadow accepts named arguments") {
  const std::string source = R"(
[effects(heap_alloc), return<void>]
/vector/push([vector<i32> mut] values, [i32] value) {
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32)}
  push([value] 3i32, [values] values)
  return(0i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("vector helper statement prefers labeled named receiver") {
  const std::string source = R"(
Particle {
  [i32] id{0i32}
}

[effects(heap_alloc), return<void>]
/vector/push([vector<i32> mut] values, [soa_vector<Particle>] value) {
}

[effects(heap_alloc), return<void>]
/soa_vector/push([soa_vector<Particle> mut] values, [i32] value) {
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32)}
  [soa_vector<Particle>] payload{soa_vector<Particle>()}
  push([value] payload, [values] values)
  return(0i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("vector helper statement labeled receiver does not fall back to non-receiver label") {
  const std::string source = R"(
Particle {
  [i32] id{0i32}
}

[effects(heap_alloc), return<void>]
/soa_vector/push([vector<i32> mut] values, [soa_vector<Particle>] value) {
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32)}
  [soa_vector<Particle>] payload{soa_vector<Particle>()}
  push([value] payload, [values] values)
  return(count(values))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("named arguments not supported for builtin calls") != std::string::npos);
}

TEST_CASE("vector helper statement skips temp-leading positional receiver probing") {
  const std::string source = R"(
Counter {
}

[return<Counter>]
makeCounter() {
  return(Counter())
}

[effects(heap_alloc), return<void>]
/vector/push([vector<Counter> mut] values, [Counter] value) {
}

[effects(heap_alloc), return<int>]
main() {
  [vector<Counter> mut] values{vector<Counter>(Counter())}
  push(makeCounter(), values)
  return(0i32)
}
  )";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown method: /Counter/push") != std::string::npos);
}

TEST_CASE("vector helper statement validates on variadic vector pack receivers") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
mutate([args<vector<i32>>] values) {
  push(at(values, 0i32), 9i32)
  values[1i32].clear()
  values[2i32].remove_swap(0i32)
  return(plus(count(at(values, 0i32)),
              plus(capacity(values[1i32]),
                   count(values[2i32]))))
}

[effects(heap_alloc), return<int>]
main() {
  return(mutate(vector<i32>(1i32),
                vector<i32>(2i32, 3i32),
                vector<i32>(4i32, 5i32)))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("vector helper statement validates on dereferenced variadic vector pack receivers") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
mutate_refs([args<Reference<vector<i32>>>] values) {
  push(dereference(at(values, 0i32)), 9i32)
  dereference(at(values, 1i32)).clear()
  dereference(at(values, 2i32)).remove_swap(0i32)
  return(plus(count(dereference(at(values, 0i32))),
              plus(capacity(dereference(at(values, 1i32))),
                   count(dereference(at(values, 2i32))))))
}

[effects(heap_alloc), return<int>]
mutate_ptrs([args<Pointer<vector<i32>>>] values) {
  push(dereference(at(values, 0i32)), 9i32)
  dereference(at(values, 1i32)).clear()
  dereference(at(values, 2i32)).remove_swap(0i32)
  return(plus(count(dereference(at(values, 0i32))),
              plus(capacity(dereference(at(values, 1i32))),
                   count(dereference(at(values, 2i32))))))
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] a0{vector<i32>(1i32)}
  [vector<i32> mut] a1{vector<i32>(2i32, 3i32)}
  [vector<i32> mut] a2{vector<i32>(4i32, 5i32)}
  [vector<i32> mut] b0{vector<i32>(1i32)}
  [vector<i32> mut] b1{vector<i32>(2i32, 3i32)}
  [vector<i32> mut] b2{vector<i32>(4i32, 5i32)}
  [Reference<vector<i32>>] r0{location(a0)}
  [Reference<vector<i32>>] r1{location(a1)}
  [Reference<vector<i32>>] r2{location(a2)}
  [Pointer<vector<i32>>] p0{location(b0)}
  [Pointer<vector<i32>>] p1{location(b1)}
  [Pointer<vector<i32>>] p2{location(b2)}
  return(plus(mutate_refs(r0, r1, r2),
              mutate_ptrs(p0, p1, p2)))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("vector helper statement user shadow resolves on variadic vector pack receivers") {
  const std::string source = R"(
[return<void>]
/vector/push([vector<i32> mut] values, [i32] value) {
}

[return<int>]
mutate([args<vector<i32>>] values) {
  push(at(values, 0i32), 9i32)
  values[0i32].push(3i32)
  return(0i32)
}

[effects(heap_alloc), return<int>]
main() {
  return(mutate(vector<i32>(1i32)))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("vector at call-form helper shadow accepts reordered named arguments") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/vector/at([vector<i32>] values, [i32] index) {
  return(77i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32)}
  return(at([index] 1i32, [values] values))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("at call-form helper shadow prefers labeled named receiver") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/vector/at([vector<i32>] values, [vector<i32>] index) {
  return(90i32)
}

[effects(heap_alloc), return<int>]
/map/at([map<string, i32>] values, [vector<i32>] index) {
  return(91i32)
}

[effects(heap_alloc), return<int>]
main() {
  [map<string, i32>] values{map<string, i32>("only"raw_utf8, 2i32)}
  [vector<i32>] index{vector<i32>(0i32)}
  return(at([index] index, [values] values))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("at call-form labeled receiver does not fall back to non-receiver label") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/vector/at([vector<i32>] values, [vector<i32>] index) {
  return(92i32)
}

[effects(heap_alloc), return<int>]
main() {
  [map<string, i32>] values{map<string, i32>("only"raw_utf8, 2i32)}
  [vector<i32>] index{vector<i32>(0i32)}
  return(at([index] index, [values] values))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("named arguments not supported for builtin calls") != std::string::npos);
}

TEST_CASE("vector at call-form helper shadow accepts positional reordered arguments") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/vector/at([vector<i32>] values, [i32] index) {
  return(79i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32)}
  return(at(1i32, values))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("map at call-form helper shadow accepts string positional reordered arguments") {
  const std::string source = R"(
[return<int>]
/map/at([map<string, i32>] values, [string] key) {
  return(81i32)
}

[return<int>]
main() {
  [map<string, i32>] values{map<string, i32>("only"raw_utf8, 2i32)}
  return(at("only"raw_utf8, values))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("map at call-form helper shadow prefers later map receiver over leading string") {
  const std::string source = R"(
[return<int>]
/map/at([map<string, i32>] values, [string] key) {
  return(83i32)
}

[return<int>]
/string/at([string] values, [map<string, i32>] key) {
  return(84i32)
}

[return<int>]
main() {
  [map<string, i32>] values{map<string, i32>("only"raw_utf8, 2i32)}
  return(at("only"raw_utf8, values))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("map at_unsafe call-form helper shadow accepts string positional reordered arguments") {
  const std::string source = R"(
[return<int>]
/map/at_unsafe([map<string, i32>] values, [string] key) {
  return(82i32)
}

[return<int>]
main() {
  [map<string, i32>] values{map<string, i32>("only"raw_utf8, 2i32)}
  return(at_unsafe("only"raw_utf8, values))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("vector at_unsafe call-form helper shadow accepts reordered named arguments") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/vector/at_unsafe([vector<i32>] values, [i32] index) {
  return(78i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32)}
  return(at_unsafe([index] 1i32, [values] values))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("vector at call-form helper shadow skips temp-leading positional probing") {
  const std::string source = R"(
Counter {
}

[return<Counter>]
makeCounter() {
  return(Counter())
}

[effects(heap_alloc), return<Counter>]
/vector/at([vector<Counter>] values, [Counter] index) {
  return(index)
}

[effects(heap_alloc), return<Counter>]
main() {
  [vector<Counter>] values{vector<Counter>(Counter())}
  return(at(makeCounter(), values))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown method: /Counter/at") != std::string::npos);
}

TEST_CASE("user definition named push with positional args is not treated as builtin") {
  const std::string source = R"(
[return<int>]
push([i32] left, [i32] right) {
  return(plus(left, right))
}

[return<int>]
main() {
  push(1i32, 2i32)
  return(push(3i32, 4i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("user definition named push accepts named arguments") {
  const std::string source = R"(
[return<int>]
push([i32] value) {
  return(value)
}

[return<int>]
main() {
  return(push([value] 3i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("vector method helper in expressions requires imported stdlib helper or explicit definition") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32)}
  return(values.push(2i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("push is only supported as a statement") != std::string::npos);
}

TEST_CASE("bare vector clear named args require imported stdlib helper") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32)}
  clear([values] values)
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("named arguments not supported for builtin calls") != std::string::npos);
}

TEST_CASE("user definition named count accepts named arguments") {
  const std::string source = R"(
[return<int>]
count([i32] value) {
  return(value)
}

[return<int>]
main() {
  return(count([value] 5i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("user definition named capacity accepts named arguments") {
  const std::string source = R"(
[return<int>]
capacity([i32] value) {
  return(value)
}

[return<int>]
main() {
  return(capacity([value] 7i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("capacity builtin still rejects named arguments") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32)}
  return(capacity([value] values))
}
  )";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /std/collections/vector/capacity") != std::string::npos);
}

TEST_CASE("user definition named at accepts named arguments") {
  const std::string source = R"(
[return<int>]
at([i32] value, [i32] index) {
  return(plus(value, index))
}

[return<int>]
main() {
  return(at([value] 3i32, [index] 2i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("user definition named at_unsafe accepts named arguments") {
  const std::string source = R"(
[return<int>]
at_unsafe([i32] value, [i32] index) {
  return(minus(value, index))
}

[return<int>]
main() {
  return(at_unsafe([value] 7i32, [index] 2i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("array access builtin still rejects named arguments") {
  const std::string source = R"(
[return<int>]
main() {
  [array<i32>] values{array<i32>(1i32, 2i32)}
  return(at([value] values, [index] 0i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("named arguments not supported for builtin calls") != std::string::npos);
}

TEST_CASE("user definition named vector accepts named arguments") {
  const std::string source = R"(
[return<int>]
vector([i32] value) {
  return(value)
}

[return<int>]
main() {
  return(vector([value] 9i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("user definition named map accepts named arguments") {
  const std::string source = R"(
[return<int>]
map([i32] key, [i32] value) {
  return(plus(key, value))
}

[return<int>]
main() {
  return(map([key] 4i32, [value] 6i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("user definition named array accepts named arguments") {
  const std::string source = R"(
[return<int>]
array([i32] value) {
  return(value)
}

[return<int>]
main() {
  return(array([value] 2i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("user definition named vector accepts block arguments") {
  const std::string source = R"(
[return<int>]
vector([i32] value) {
  return(value)
}

[return<int>]
main() {
  [i32 mut] result{0i32}
  vector(9i32) {
    assign(result, 4i32)
  }
  return(result)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("user definition named array accepts block arguments") {
  const std::string source = R"(
[return<int>]
array<T>([T] value) {
  return(1i32)
}

[return<int>]
main() {
  [i32 mut] result{0i32}
  array<i32>(2i32) {
    assign(result, 5i32)
  }
  return(result)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("user definition named map accepts block arguments") {
  const std::string source = R"(
[return<int>]
map([i32] key, [i32] value) {
  return(plus(key, value))
}

[return<int>]
main() {
  [i32 mut] result{0i32}
  map(4i32, 6i32) {
    assign(result, 7i32)
  }
  return(result)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("collection builtin still rejects named arguments") {
  const std::string source = R"(
[return<int>]
main() {
  return(vector([value] 1i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("named arguments not supported for builtin calls") != std::string::npos);
}

TEST_CASE("builtin at validates on array literal call target") {
  const std::string source = R"(
[return<int>]
main() {
  return(at(array<i32>(4i32, 5i32), 1i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("builtin at validates on canonical map call target") {
  const std::string source = R"(
[return<int>]
main() {
  return(at(map<i32, i32>(7i32, 8i32), 7i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("builtin at rewrites positional reordered canonical map receiver") {
  const std::string source = R"(
[return<int>]
main() {
  return(at(7i32, map<i32, i32>(7i32, 8i32)))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("user definition named vector call is not treated as builtin collection target") {
  const std::string source = R"(
[return<int>]
vector<T>([T] value) {
  return(1i32)
}

[return<int>]
main() {
  return(at(vector<i32>(9i32), 0i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("at requires array, vector, map, or string target") != std::string::npos);
}

TEST_CASE("user definition named map call is not treated as builtin collection target") {
  const std::string source = R"(
[return<int>]
map<K, V>([K] key, [V] value) {
  return(1i32)
}

[return<int>]
main() {
  return(at(map<i32, i32>(7i32, 8i32), 0i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("at requires array, vector, map, or string target") != std::string::npos);
}

TEST_CASE("user definition named Map reordered receiver is not treated as builtin experimental map target") {
  const std::string source = R"(
[return<int>]
Map<K, V>([K] key, [V] value) {
  return(1i32)
}

[return<int>]
main() {
  return(at(0i32, Map<i32, i32>(7i32, 8i32)))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("at requires array, vector, map, or string target") != std::string::npos);
}

TEST_CASE("user definition named Map call is not treated as builtin experimental map target") {
  const std::string source = R"(
[return<int>]
Map<K, V>([K] key, [V] value) {
  return(1i32)
}

[return<int>]
main() {
  return(at(Map<i32, i32>(7i32, 8i32), 0i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown method: /i32/at") != std::string::npos);
}

TEST_SUITE_END();
