#include "third_party/doctest.h"

#include "test_semantics_helpers.h"

TEST_SUITE_BEGIN("primestruct.semantics.bindings.pointers");

TEST_CASE("pointer helpers validate") {
  const std::string source = R"(
[return<int>]
main() {
  [i32] value{1i32}
  return(dereference(location(value)))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("dereference accepts helper-returned pointer calls") {
  const std::string source = R"(
[return<Pointer<i32>>]
forward([Pointer<i32>] ptr) {
  return(ptr)
}

[return<int>]
main() {
  [i32] value{7i32}
  return(dereference(forward(location(value))))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("dereference auto inference accepts helper-returned pointer calls") {
  const std::string source = R"(
[return<Pointer<i32>>]
forward([Pointer<i32>] ptr) {
  return(ptr)
}

[return<int>]
main() {
  [i32] value{7i32}
  [auto] inferred{dereference(forward(location(value)))}
  return(inferred)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("dereference rejects helper-returned non-pointer calls") {
  const std::string source = R"(
[return<int>]
value() {
  return(7i32)
}

[return<int>]
main() {
  return(dereference(value()))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("dereference requires a pointer or reference") !=
        std::string::npos);
}

TEST_CASE("dereference auto inference rejects helper-returned non-pointer calls") {
  const std::string source = R"(
[return<int>]
value() {
  return(7i32)
}

[return<int>]
main() {
  [auto] inferred{dereference(value())}
  return(inferred)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("binding initializer validateExpr failed") !=
        std::string::npos);
}

TEST_CASE("pointer helpers reject template arguments") {
  const std::string source = R"(
[return<int>]
main() {
  [i32] value{1i32}
  return(dereference<i32>(location(value)))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("pointer helpers do not accept template arguments") != std::string::npos);
}

TEST_CASE("pointer helpers reject block arguments") {
  const std::string source = R"(
[return<int>]
main() {
  [i32] value{1i32}
  return(dereference(location(value)) { 1i32 })
}
  )";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("pointer helpers do not accept block arguments") != std::string::npos);
}

TEST_CASE("memory intrinsics validate with heap_alloc effect") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [mut] ptr{/std/intrinsics/memory/alloc<i32>(4i32)}
  assign(dereference(ptr), 7i32)
  [mut] grown{/std/intrinsics/memory/realloc(ptr, 8i32)}
  /std/intrinsics/memory/free(grown)
  return(7i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("pointer targets allow top-level uninitialized storage") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [Pointer<uninitialized<i32>>] ptr{/std/intrinsics/memory/alloc<uninitialized<i32>>(1i32)}
  init(dereference(ptr), 7i32)
  [i32] out{take(dereference(ptr))}
  /std/intrinsics/memory/free(ptr)
  return(out)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("reference targets allow top-level uninitialized storage") {
  const std::string source = R"(
[return<int>]
main() {
  [uninitialized<i32>] storage{uninitialized<i32>()}
  [Reference<uninitialized<i32>>] ref{location(storage)}
  init(dereference(ref), 7i32)
  return(take(dereference(ref)))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("variadic pointer packs allow top-level uninitialized storage elements") {
  const std::string source = R"(
[return<int>]
score_ptrs([args<Pointer<uninitialized<i32>>>] values) {
  init(dereference(values.at(0i32)), 7i32)
  return(take(dereference(values.at_unsafe(0i32))))
}

[return<int>]
main() {
  [uninitialized<i32>] storage{uninitialized<i32>()}
  [Pointer<uninitialized<i32>>] ptr{location(storage)}
  return(score_ptrs(ptr))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("variadic reference packs allow top-level uninitialized storage elements") {
  const std::string source = R"(
[return<int>]
score_refs([args<Reference<uninitialized<i32>>>] values) {
  init(dereference(values[0i32]), 7i32)
  return(take(dereference(values.at_unsafe(0i32))))
}

[return<int>]
main() {
  [uninitialized<i32>] storage{uninitialized<i32>()}
  [Reference<uninitialized<i32>>] ref{location(storage)}
  return(score_refs(ref))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("memory at validates checked pointer access") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [mut] ptr{/std/intrinsics/memory/alloc<i32>(2i32)}
  [mut] second{/std/intrinsics/memory/at(ptr, 1i32, 2i32)}
  assign(dereference(second), 7i32)
  /std/intrinsics/memory/free(ptr)
  return(7i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("memory at rejects non-pointer target") {
  const std::string source = R"(
[return<int>]
main() {
  /std/intrinsics/memory/at(1i32, 0i32, 1i32)
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("at requires pointer target") != std::string::npos);
}

TEST_CASE("memory at rejects mismatched index and count kinds") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [Pointer<i32>] ptr{/std/intrinsics/memory/alloc<i32>(2i32)}
  /std/intrinsics/memory/at(ptr, 1i32, 2i64)
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("at requires matching integer index and element count kinds") != std::string::npos);
}

TEST_CASE("memory at_unsafe validates unchecked pointer access") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [mut] ptr{/std/intrinsics/memory/alloc<i32>(2i32)}
  [mut] second{/std/intrinsics/memory/at_unsafe(ptr, 1i32)}
  assign(dereference(second), 7i32)
  /std/intrinsics/memory/free(ptr)
  return(7i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("memory at_unsafe rejects non-pointer target") {
  const std::string source = R"(
[return<int>]
main() {
  /std/intrinsics/memory/at_unsafe(1i32, 0i32)
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("at_unsafe requires pointer target") != std::string::npos);
}

TEST_CASE("memory at_unsafe rejects template arguments") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [Pointer<i32>] ptr{/std/intrinsics/memory/alloc<i32>(2i32)}
  /std/intrinsics/memory/at_unsafe<i32>(ptr, 1i32)
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("at_unsafe does not accept template arguments") != std::string::npos);
}

TEST_CASE("alloc requires heap_alloc effect") {
  const std::string source = R"(
[return<int>]
main() {
  [Pointer<i32>] ptr{/std/intrinsics/memory/alloc<i32>(4i32)}
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("alloc requires heap_alloc effect") != std::string::npos);
}

TEST_CASE("alloc rejects unsupported pointer target type") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [Pointer<array<i32>>] ptr{/std/intrinsics/memory/alloc<array<i32>>(1i32)}
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unsupported pointer target type: array<i32>") != std::string::npos);
}

TEST_CASE("realloc requires pointer target") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  /std/intrinsics/memory/realloc(1i32, 2i32)
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("realloc requires pointer target") != std::string::npos);
}

TEST_CASE("free rejects template arguments") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [Pointer<i32>] ptr{/std/intrinsics/memory/alloc<i32>(1i32)}
  /std/intrinsics/memory/free<i32>(ptr)
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("free does not accept template arguments") != std::string::npos);
}

TEST_CASE("binding array type requires one template argument") {
  const std::string source = R"(
[return<int>]
main() {
  [array<i32, i32>] value{array<i32>(1i32)}
  return(1i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("array<T, N> is unsupported; use array<T> (runtime-count array)") != std::string::npos);
}

TEST_CASE("binding map type requires two template arguments") {
  const std::string source = R"(
[return<int>]
main() {
  [map<i32>] value{map<i32, i32>(1i32, 2i32)}
  return(1i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("map requires exactly two template arguments") != std::string::npos);
}

TEST_CASE("binding canonical map type requires two template arguments") {
  const std::string source = R"(
[return<int>]
main() {
  [/std/collections/map<i32>] value{map<i32, i32>(1i32, 2i32)}
  return(1i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("map requires exactly two template arguments") != std::string::npos);
}

TEST_CASE("pointer bindings require template arguments") {
  const std::string source = R"(
[return<int>]
main() {
  [i32] value{1i32}
  [Pointer] ptr{location(value)}
  return(1i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("Pointer requires a template argument") != std::string::npos);
}

TEST_CASE("pointer bindings accept struct targets") {
  const std::string source = R"(
[struct]
Foo() {
  [i32] value{1i32}
}

[return<int>]
main() {
  [Foo] value{Foo()}
  [Pointer<Foo>] ptr{location(value)}
  return(1i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("reference bindings require template arguments") {
  const std::string source = R"(
[return<int>]
main() {
  [i32] value{1i32}
  [Reference] ref{location(value)}
  return(1i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("Reference requires a template argument") != std::string::npos);
}

TEST_CASE("reference bindings accept struct targets") {
  const std::string source = R"(
[struct]
Foo() {
  [i32] value{1i32}
}

[return<int>]
main() {
  [Foo] value{Foo()}
  [Reference<Foo>] ref{location(value)}
  return(1i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("reference bindings accept array targets") {
  const std::string source = R"(
[return<int>]
main() {
  [array<i32>] values{array<i32>(1i32, 2i32)}
  [Reference<array<i32>>] ref{location(values)}
  return(at(ref, 0i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("reference bindings accept Buffer targets") {
  const std::string source = R"(
[effects(gpu_dispatch) return<int>]
main() {
  [Buffer<i32>] values{/std/gpu/buffer<i32>(2i32)}
  [Reference<Buffer<i32>>] ref{location(values)}
  return(1i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("pointer bindings accept Buffer targets") {
  const std::string source = R"(
[effects(gpu_dispatch) return<int>]
main() {
  [Buffer<i32>] values{/std/gpu/buffer<i32>(2i32)}
  [Pointer<Buffer<i32>>] ptr{location(values)}
  return(1i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("pointer bindings accept array targets") {
  const std::string source = R"(
[return<int>]
main() {
  [array<i32>] values{array<i32>(1i32, 2i32)}
  [Pointer<array<i32>>] ptr{location(values)}
  return(1i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("reference bindings reject array without element type") {
  const std::string source = R"(
[return<int>]
main() {
  [array<i32>] values{array<i32>(1i32, 2i32)}
  [Reference<array>] ref{location(values)}
  return(1i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unsupported reference target type") != std::string::npos);
}

TEST_CASE("pointer bindings reject array without element type") {
  const std::string source = R"(
[return<int>]
main() {
  [array<i32>] values{array<i32>(1i32, 2i32)}
  [Pointer<array>] ptr{location(values)}
  return(1i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unsupported pointer target type") != std::string::npos);
}

TEST_CASE("pointer bindings reject unknown targets") {
  const std::string source = R"(
[return<int>]
main() {
  [i32] value{1i32}
  [Pointer<Missing>] ptr{location(value)}
  return(1i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unsupported pointer target type") != std::string::npos);
}

TEST_CASE("reference bindings reject unknown targets") {
  const std::string source = R"(
[return<int>]
main() {
  [i32] value{1i32}
  [Reference<Missing>] ref{location(value)}
  return(1i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unsupported reference target type") != std::string::npos);
}

TEST_CASE("location requires local binding name") {
  const std::string source = R"(
[return<int>]
main() {
  return(dereference(location(plus(1i32, 2i32))))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("location requires a local binding") != std::string::npos);
}

TEST_CASE("location accepts parameters") {
  const std::string source = R"(
[return<int>]
read([i32] x) {
  [Pointer<i32>] ptr{location(x)}
  return(dereference(ptr))
}

[return<int>]
main() {
  return(read(3i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("dereference accepts pointer parameters") {
  const std::string source = R"(
[return<int>]
read([Pointer<i32>] ptr) {
  return(dereference(ptr))
}

[return<int>]
main() {
  [i32] value{7i32}
  return(read(location(value)))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("pointer arithmetic accepts pointer parameters") {
  const std::string source = R"(
[return<int>]
offset([Pointer<i32>] ptr) {
  return(dereference(plus(ptr, 0i32)))
}

[return<int>]
main() {
  [i32] value{9i32}
  return(offset(location(value)))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("assign allows mutable parameters") {
  const std::string source = R"(
[return<int>]
increment([i32 mut] value) {
  assign(value, plus(value, 1i32))
  return(value)
}

[return<int>]
main() {
  return(increment(4i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("assign allows mutable pointer parameters") {
  const std::string source = R"(
[return<int>]
write([Pointer<i32> mut] ptr) {
  assign(dereference(ptr), 9i32)
  return(dereference(ptr))
}

[return<int>]
main() {
  [i32 mut] value{3i32}
  return(write(location(value)))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("binding allows templated type") {
  const std::string source = R"(
[return<int>]
main() {
  [handle<Texture>] value{1i32}
  return(1i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("binding allows struct types") {
  const std::string source = R"(
[struct]
thing() {
  [i32] value{1i32}
}

[return<int>]
main() {
  [thing] item{1i32}
  return(1i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("binding allows untagged struct definitions") {
  const std::string source = R"(
thing() {
  [i32] value{1i32}
}

[return<int>]
main() {
  [thing] item{thing([value] 2i32)}
  return(1i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("struct constructor accepts named arguments") {
  const std::string source = R"(
[struct]
thing() {
  [i32] value{1i32}
  [i32] count{2i32}
}

[return<int>]
main() {
  thing([count] 3i32, [value] 4i32)
  return(1i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("struct constructor allows positional after labels") {
  const std::string source = R"(
[struct]
thing() {
  [i32] value{1i32}
  [i32] count{2i32}
}

[return<int>]
main() {
  thing([count] 3i32, 4i32)
  return(1i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("struct constructor allows defaulted fields") {
  const std::string source = R"(
[struct]
thing() {
  [i32] value{1i32}
  [i32] count{2i32}
}

[return<int>]
main() {
  thing([value] 5i32)
  return(1i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("struct constructor accepts named arguments while skipping static fields") {
  const std::string source = R"(
[struct]
thing() {
  [static i32] shared{1i32}
  [i32] value{2i32}
  [i32] count{3i32}
}

[return<int>]
main() {
  thing([count] 4i32, [value] 5i32)
  return(1i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("struct constructor rejects extra arguments") {
  const std::string source = R"(
[struct]
thing() {
  [i32] value{1i32}
}

[return<int>]
main() {
  thing(1i32, 2i32)
  return(1i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("argument count mismatch") != std::string::npos);
}

TEST_CASE("struct constructor keeps argument mismatch when static fields are skipped") {
  const std::string source = R"(
[struct]
thing() {
  [static i32] shared{1i32}
  [i32] value{2i32}
}

[return<int>]
main() {
  thing(1i32, 2i32)
  return(1i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("argument count mismatch") != std::string::npos);
}

TEST_CASE("wrapper-returned struct constructor validates in resolved helper arguments") {
  const std::string source = R"(
[struct]
thing() {
  [i32] value{1i32}
  [i32] count{2i32}
}

[return<thing>]
buildThing() {
  return(thing([count] 4i32, [value] 3i32))
}

[return<int>]
use([thing] item) {
  return(item.count)
}

[return<int>]
main() {
  return(use(buildThing()))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("wrapper-returned struct constructor keeps resolved helper mismatch diagnostics") {
  const std::string source = R"(
[struct]
thing() {
  [i32] value{1i32}
}

[return<thing>]
buildThing() {
  return(thing([value] 3i32))
}

[return<int>]
use([thing] item, [i32] extra) {
  return(extra)
}

[return<int>]
main() {
  return(use(buildThing()))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("argument count mismatch for /use") != std::string::npos);
}

TEST_CASE("struct brace constructor works in arguments") {
  const std::string source = R"(
[struct]
thing() {
  [i32] value{1i32}
}

[return<int>]
use([thing] item) {
  return(1i32)
}

[return<int>]
main() {
  return(use(thing{ 2i32 }))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("struct brace constructor accepts return value blocks") {
  const std::string source = R"(
[struct]
thing() {
  [i32] value{1i32}
}

[return<int>]
use([thing] item) {
  return(1i32)
}

[return<int>]
main() {
  return(use(thing{
    return(2i32)
  }))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("struct brace constructor uses last expression") {
  const std::string source = R"(
[struct]
thing() {
  [i32] value{1i32}
}

[return<int>]
use([thing] item) {
  return(1i32)
}

[return<int>]
main() {
  return(use(thing{
    [i32] temp{1i32}
    plus(temp, 2i32)
  }))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("binding initializer accepts labeled arguments") {
  const std::string source = R"(
[struct]
thing() {
  [i32] value{1i32}
  [i32] count{2i32}
}

[return<int>]
main() {
  [thing] item{thing([count] 3i32 [value] 4i32)}
  return(1i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("binding initializer accepts return value blocks") {
  const std::string source = R"(
[return<int>]
main() {
  [i32] value{
    [i32] temp{1i32}
    return(plus(temp, 2i32))
  }
  return(value)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("binding initializer rejects named args for builtins") {
  const std::string source = R"(
[return<int>]
main() {
  [array<i32>] values{array<i32>([first] 1i32)}
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("named arguments not supported for builtin calls") != std::string::npos);
}

TEST_CASE("binding initializer allows struct constructor block") {
  const std::string source = R"(
[struct]
thing() {
  [i32] value{1i32}
}

[return<int>]
main() {
  [thing] item{block(){ thing() }}
  return(1i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("binding initializer allows struct constructor if") {
  const std::string source = R"(
[struct]
thing() {
  [i32] value{1i32}
}

[return<int>]
main() {
  [thing] item{if(true, then(){ thing() }, else(){ thing() })}
  return(1i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("struct constructor rejects unknown named arguments") {
  const std::string source = R"(
[struct]
thing() {
  [i32] value{1i32}
}

[return<int>]
main() {
  thing([missing] 2i32)
  return(1i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown named argument: missing") != std::string::npos);
}

TEST_CASE("binding resolves struct types in namespace") {
  const std::string source = R"(
namespace demo {
  [struct]
  widget() {
    [i32] value{1i32}
  }

  [return<int>]
  main() {
    [widget] item{1i32}
    return(1i32)
  }
}
)";
  std::string error;
  CHECK(validateProgram(source, "/demo/main", error));
  CHECK(error.empty());
}

TEST_CASE("binding allows pointer types") {
  const std::string source = R"(
[return<int>]
main() {
  [i32 mut] value{1i32}
  [Pointer<i32>] ptr{location(value)}
  [Reference<i32>] ref{location(value)}
  return(1i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("i64 and u64 bindings validate") {
  const std::string sourceSigned = R"(
[return<i64>]
main() {
  [i64] value{9i64}
  return(value)
}
)";
  std::string error;
  CHECK(validateProgram(sourceSigned, "/main", error));
  CHECK(error.empty());

  const std::string sourceUnsigned = R"(
[return<u64>]
main() {
  [u64] value{10u64}
  return(value)
}
)";
  CHECK(validateProgram(sourceUnsigned, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("pointer plus validates") {
  const std::string source = R"(
[return<int>]
main() {
  [i32] value{3i32}
  return(dereference(plus(location(value), 0i32)))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("pointer minus validates") {
  const std::string source = R"(
[return<int>]
main() {
  [i32] first{3i32}
  [i32] second{4i32}
  return(dereference(minus(location(second), 16i32)))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("pointer minus accepts u64 offsets") {
  const std::string source = R"(
[return<int>]
main() {
  [i32] first{3i32}
  [i32] second{4i32}
  return(dereference(minus(location(second), 16u64)))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("pointer plus accepts i64 offsets") {
  const std::string source = R"(
[return<int>]
main() {
  [i32] value{3i32}
  return(dereference(plus(location(value), 16i64)))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("pointer minus accepts i64 offsets") {
  const std::string source = R"(
[return<int>]
main() {
  [i32] first{3i32}
  [i32] second{4i32}
  return(dereference(minus(location(second), 16i64)))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("pointer minus rejects pointer + pointer") {
  const std::string source = R"(
[return<int>]
main() {
  [i32] first{3i32}
  [i32] second{4i32}
  return(minus(location(first), location(second)))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("pointer arithmetic does not support pointer + pointer") != std::string::npos);
}

TEST_CASE("pointer minus rejects pointer on right") {
  const std::string source = R"(
[return<int>]
main() {
  [i32] value{3i32}
  [Pointer<i32>] ptr{location(value)}
  return(minus(1i32, ptr))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("pointer arithmetic requires pointer on the left") != std::string::npos);
}

TEST_CASE("pointer plus accepts reference locations") {
  const std::string source = R"(
[return<int>]
main() {
  [i32] first{3i32}
  [i32] second{4i32}
  [Reference<i32>] ref{location(first)}
  return(dereference(plus(location(ref), 16i32)))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("pointer plus rejects pointer + pointer") {
  const std::string source = R"(
[return<int>]
main() {
  [i32] value{3i32}
  [Pointer<i32>] ptr{location(value)}
  return(plus(location(value), ptr))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("pointer arithmetic does not support pointer + pointer") != std::string::npos);
}

TEST_CASE("pointer plus rejects pointer on right") {
  const std::string source = R"(
[return<int>]
main() {
  [i32] value{3i32}
  [Pointer<i32>] ptr{location(value)}
  return(plus(1i32, ptr))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("pointer arithmetic requires pointer on the left") != std::string::npos);
}

TEST_CASE("pointer plus rejects non-integer offset") {
  const std::string source = R"(
[return<int>]
main() {
  [i32] value{3i32}
  return(plus(location(value), true))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("pointer arithmetic requires integer offset") != std::string::npos);
}

TEST_SUITE_END();
