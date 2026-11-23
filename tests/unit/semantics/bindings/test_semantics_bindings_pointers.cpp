#include "third_party/doctest.h"

#include "../test_semantics_helpers.h"

#include <utility>
#include <vector>

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
  CHECK(error.find("dereference requires a pointer or reference") !=
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
[unsafe effects(heap_alloc), return<int>]
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
[unsafe effects(heap_alloc), return<int>]
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

TEST_CASE("pointer helper roots allow uninitialized borrow binding") {
  const std::string source = R"(
[unsafe return<Pointer<uninitialized<i32>>>]
slot([Pointer<uninitialized<i32>>] values, [i32] index) {
  return(/std/intrinsics/memory/at_unsafe(values, index))
}

[unsafe effects(heap_alloc)]
release([Pointer<uninitialized<i32>>] ptr) {
  /std/intrinsics/memory/free(ptr)
}

[effects(heap_alloc), return<int>]
main() {
  [Pointer<uninitialized<i32>>] ptr{/std/intrinsics/memory/alloc<uninitialized<i32>>(1i32)}
  [Pointer<uninitialized<i32>>] ptrSlot{slot(ptr, 0i32)}
  init(dereference(ptrSlot), 7i32)
  [Reference<i32>] ref{borrow(dereference(ptrSlot))}
  [i32] out{dereference(ref)}
  drop(dereference(ptrSlot))
  release(ptr)
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
[unsafe effects(heap_alloc), return<int>]
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
[unsafe effects(heap_alloc), return<int>]
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
[unsafe return<int>]
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
[unsafe effects(heap_alloc), return<int>]
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
[unsafe effects(heap_alloc), return<int>]
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
[unsafe effects(heap_alloc), return<int>]
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

TEST_CASE("binding canonical map type stays ordinary stdlib code") {
  const std::string source = R"(
[return<int>]
main() {
  [/std/collections/map<i32>] value{map<i32, i32>(1i32, 2i32)}
  return(1i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
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
  [Foo] value{Foo{}}
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
  [Foo] value{Foo{}}
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
  [thing] item{[value] 1i32}
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
  [thing] item{[value] 2i32}
  return(1i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("typed binding accepts empty lower-case constructor") {
  const std::string source = R"(
[struct]
thing() {
  [i32] value{1i32}
}

[return<int>]
main() {
  [thing] item{thing{}}
  return(item.value)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("struct brace constructor accepts named arguments") {
  const std::string source = R"(
[struct]
thing() {
  [i32] value{1i32}
  [i32] count{2i32}
}

[return<int>]
use([thing] item) {
  return(item.count)
}

[return<int>]
main() {
  return(use(thing{[count] 3i32, [value] 4i32}))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("struct brace constructor allows positional after labels") {
  const std::string source = R"(
[struct]
thing() {
  [i32] value{1i32}
  [i32] count{2i32}
}

[return<int>]
use([thing] item) {
  return(item.count)
}

[return<int>]
main() {
  return(use(thing{[count] 3i32, 4i32}))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("struct brace constructor allows defaulted fields") {
  const std::string source = R"(
[struct]
thing() {
  [i32] value{1i32}
  [i32] count{2i32}
}

[return<int>]
use([thing] item) {
  return(item.value)
}

[return<int>]
main() {
  return(use(thing{[value] 5i32}))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("struct brace constructor accepts named arguments while skipping static fields") {
  const std::string source = R"(
[struct]
thing() {
  [static i32] shared{1i32}
  [i32] value{2i32}
  [i32] count{3i32}
}

[return<int>]
use([thing] item) {
  return(item.count)
}

[return<int>]
main() {
  return(use(thing{[count] 4i32, [value] 5i32}))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("struct brace constructor rejects extra arguments") {
  const std::string source = R"(
[struct]
thing() {
  [i32] value{1i32}
}

[return<int>]
main() {
  thing{1i32, 2i32}
  return(1i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("argument count mismatch") != std::string::npos);
}

TEST_CASE("struct brace constructor keeps argument mismatch when static fields are skipped") {
  const std::string source = R"(
[struct]
thing() {
  [static i32] shared{1i32}
  [i32] value{2i32}
}

[return<int>]
main() {
  thing{1i32, 2i32}
  return(1i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("argument count mismatch") != std::string::npos);
}

TEST_CASE("struct call-shaped constructor rejects field mapping") {
  const std::string source = R"(
[struct]
thing() {
  [i32] value{1i32}
}

[return<int>]
main() {
  thing([value] 2i32)
  return(1i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("struct construction requires braces: /thing") !=
        std::string::npos);
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
  return(thing{[count] 4i32, [value] 3i32})
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
  return(thing{[value] 3i32})
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

TEST_CASE("struct brace constructor accepts multiple positional arguments") {
  const std::string source = R"(
[struct]
thing() {
  [i32] value{1i32}
  [i32] count{2i32}
}

[return<int>]
use([thing] item) {
  return(item.count)
}

[return<int>]
main() {
  return(use(thing{4i32, 3i32}))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("struct brace constructor rejects duplicate labels") {
  const std::string source = R"(
[struct]
thing() {
  [i32] value{1i32}
  [i32] count{2i32}
}

[return<int>]
use([thing] item) {
  return(item.value)
}

[return<int>]
main() {
  return(use(thing{[value] 3i32, [value] 4i32}))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("duplicate named argument: value") != std::string::npos);
}

TEST_CASE("struct brace constructor rejects too many positional entries") {
  const std::string source = R"(
[struct]
thing() {
  [i32] value{1i32}
}

[return<int>]
use([thing] item) {
  return(item.value)
}

[return<int>]
main() {
  return(use(thing{1i32, 2i32}))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("argument count mismatch") != std::string::npos);
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
    block(){
      [i32] temp{1i32}
      plus(temp, 2i32)
    },
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
  [thing] item{[count] 3i32, [value] 4i32}
  return(1i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("binding initializer accepts labeled struct-literal local bindings") {
  const std::string source = R"(
[struct]
Pair {
  [int] left{0i32}
  [int] right{0i32}
}

[return<int>]
main() {
  [Pair] pair{[left] 4i32, [right] 8i32}
  return(plus(pair.left, pair.right))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("binding initializer shorthand still rejects named args for builtins") {
  const std::string source = R"(
[return<int>]
main() {
  [array<i32>] values{[first] 1i32}
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(parseProgramWithError(source, error));
  CHECK(error.find("named arguments not supported for builtin calls") != std::string::npos);
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
  [thing] item{block(){ return(thing{}) }}
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
  [thing] item{if(true, then(){ return(thing{}) }, else(){ return(thing{}) })}
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
use([thing] item) {
  return(item.value)
}

[return<int>]
main() {
  return(use(thing{[missing] 2i32}))
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
    [widget] item{[value] 1i32}
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

TEST_CASE("semantics pointer plus accepts i64 offsets") {
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

TEST_CASE("raw heap operations require an unsafe definition") {
  // docs/spec/pointers-and-references.md: free, realloc, at_unsafe and reinterpret are unsafe.
  const std::vector<std::pair<std::string, std::string>> uses = {
      {"free", "/std/intrinsics/memory/free(ptr)"},
      {"realloc", "[Pointer<i32>] grown{/std/intrinsics/memory/realloc(ptr, 2i32)}"},
      {"at_unsafe", "[Pointer<i32>] slot{/std/intrinsics/memory/at_unsafe(ptr, 0i32)}"},
      {"reinterpret", "[Pointer<i64>] wide{/std/intrinsics/memory/reinterpret<i64>(ptr)}"},
  };
  for (const auto &[name, use] : uses) {
    INFO(name);
    for (const bool unsafeMain : {false, true}) {
      const std::string source = std::string("[") + (unsafeMain ? "unsafe " : "") +
                                 "effects(heap_alloc), return<int>]\n"
                                 "main() {\n"
                                 "  [Pointer<i32>] ptr{/std/intrinsics/memory/alloc<i32>(1i32)}\n"
                                 "  " +
                                 use +
                                 "\n"
                                 "  return(0i32)\n"
                                 "}\n";
      std::string error;
      if (unsafeMain) {
        CHECK(validateProgram(source, "/main", error));
        CHECK(error.empty());
      } else {
        CHECK_FALSE(validateProgram(source, "/main", error));
        CHECK(error.find(name + " requires an unsafe definition") != std::string::npos);
      }
    }
  }
}

TEST_CASE("unsafe_api definitions are only callable from unsafe code") {
  const std::string prelude = R"(
[unsafe_api effects(heap_alloc)]
release([Pointer<i32>] ptr) {
  /std/intrinsics/memory/free(ptr)
}

[unsafe effects(heap_alloc)]
wrapper([Pointer<i32>] ptr) {
  release(ptr)
}
)";
  const std::string safeCaller = prelude + R"(
[effects(heap_alloc), return<int>]
main() {
  [Pointer<i32>] ptr{/std/intrinsics/memory/alloc<i32>(1i32)}
  release(ptr)
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(safeCaller, "/main", error));
  CHECK(error.find("calling /release requires an unsafe definition") != std::string::npos);

  // An [unsafe] definition may call it, and safe code may call that definition.
  const std::string throughUnsafe = prelude + R"(
[effects(heap_alloc), return<int>]
main() {
  [Pointer<i32>] ptr{/std/intrinsics/memory/alloc<i32>(1i32)}
  wrapper(ptr)
  return(0i32)
}
)";
  error.clear();
  CHECK(validateProgram(throughUnsafe, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("pointer to a local cannot escape via return") {
  const std::string direct = R"(
[return<Pointer<i32>>]
leak() {
  [i32 mut] value{4i32}
  return(location(value))
}

[return<int>]
main() {
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(direct, "/main", error));
  CHECK(error.find("pointer escapes via return (root: value)") != std::string::npos);

  const std::string throughAlias = R"(
[return<Pointer<i32>>]
leak() {
  [i32 mut] value{4i32}
  [Pointer<i32>] ptr{location(value)}
  return(plus(ptr, 0i32))
}

[return<int>]
main() {
  return(0i32)
}
)";
  error.clear();
  CHECK_FALSE(validateProgram(throughAlias, "/main", error));
  CHECK(error.find("pointer escapes via return (root: value)") != std::string::npos);
}

TEST_CASE("pointer to a local cannot escape via assignment through a parameter") {
  const std::string throughMutParam = R"(
[return<void>]
leak([Pointer<i32> mut] out) {
  [i32 mut] value{4i32}
  assign(out, location(value))
}

[return<int>]
main() {
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(throughMutParam, "/main", error));
  CHECK(error.find("pointer escapes via assignment to out (root: value)") != std::string::npos);

  const std::string throughField = R"(
[struct]
Holder() {
  [i32 mut] dummy{0i32}
  [Pointer<i32> mut] target{location(dummy)}
}

[return<void>]
leak([Holder mut] holder) {
  [i32 mut] value{4i32}
  assign(holder.target, location(value))
}

[return<int>]
main() {
  return(0i32)
}
)";
  error.clear();
  CHECK_FALSE(validateProgram(throughField, "/main", error));
  CHECK(error.find("pointer escapes via assignment to holder (root: value)") != std::string::npos);
}

TEST_CASE("pointers that stay within their local's scope are accepted") {
  const std::string source = R"(
[return<Pointer<i32>>]
pass_through([Pointer<i32>] ptr) {
  return(ptr)
}

[return<void>]
keep_own_copy([Pointer<i32> copy mut] out) {
  [i32 mut] value{4i32}
  assign(out, location(value))
}

[unsafe return<Pointer<i32>>]
unsafe_leak() {
  [i32 mut] value{4i32}
  return(location(value))
}

[return<int>]
main() {
  [i32 mut] value{7i32}
  [Pointer<i32> mut] ptr{pass_through(location(value))}
  assign(dereference(ptr), 9i32)
  return(value)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("pointer to a local cannot escape into a local of an enclosing block") {
  const std::string direct = R"(
[return<int>]
main() {
  [i32 mut] outer{1i32}
  [Pointer<i32> mut] ptr{location(outer)}
  if(true) {
    [i32 mut] inner{2i32}
    assign(ptr, location(inner))
  }
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(direct, "/main", error));
  CHECK(error.find("pointer escapes via assignment to ptr (root: inner)") != std::string::npos);

  const std::string throughFieldAndAlias = R"(
[struct]
Holder() {
  [i32 mut] dummy{0i32}
  [Pointer<i32> mut] target{location(dummy)}
}

[return<int>]
main() {
  [Holder mut] holder{Holder{}}
  if(true) {
    [i32 mut] inner{2i32}
    [Pointer<i32>] alias{location(inner)}
    assign(holder.target, alias)
  }
  return(0i32)
}
)";
  error.clear();
  CHECK_FALSE(validateProgram(throughFieldAndAlias, "/main", error));
  CHECK(error.find("pointer escapes via assignment to holder (root: inner)") != std::string::npos);
}

TEST_CASE("pointer to a local cannot escape into a longer-lived container") {
  const std::string prelude = R"(
import /std/collections/*
import /std/collections/map/*

[struct]
Ref() {
  [i32 mut] dummy{0i32}
  [Pointer<i32> mut] at{location(dummy)}
}
)";
  const std::string outerVector = prelude + R"(
[effects(heap_alloc) return<int>]
main() {
  [Vector<Ref> mut] refs{vector<Ref>()}
  if(true) {
    [i32 mut] inner{2i32}
    vectorPush<Ref>(refs, Ref{0i32, location(inner)})
  }
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(outerVector, "/main", error));
  CHECK(error.find(
            "pointer escapes via argument to /std/collections/vector/vectorPush (root: inner)") !=
        std::string::npos);

  const std::string parameterVector = prelude + R"(
[effects(heap_alloc) return<void>]
fill([Vector<Ref> mut] out) {
  [i32 mut] inner{2i32}
  vectorPush<Ref>(out, Ref{0i32, location(inner)})
}

[effects(heap_alloc) return<int>]
main() {
  return(0i32)
}
)";
  error.clear();
  CHECK_FALSE(validateProgram(parameterVector, "/main", error));
  CHECK(error.find(
            "pointer escapes via argument to /std/collections/vector/vectorPush (root: inner)") !=
        std::string::npos);

  const std::string methodPush = prelude + R"(
[effects(heap_alloc) return<int>]
main() {
  [vector<Ref> mut] refs{vector<Ref>()}
  for([i32 mut] i{0i32}, i < 2i32, i++) {
    refs.push(Ref{0i32, location(i)})
  }
  return(0i32)
}
)";
  error.clear();
  CHECK_FALSE(validateProgram(methodPush, "/main", error));
  CHECK(error.find("pointer escapes via argument to push (root: i)") != std::string::npos);

  const std::string mapInsert = prelude + R"(
[effects(heap_alloc) return<int>]
main() {
  [Map<i32, Ref> mut] refs{mapSingle<i32, Ref>(0i32, Ref{})}
  if(true) {
    [i32 mut] inner{2i32}
    refs.insert(1i32, Ref{0i32, location(inner)})
  }
  return(0i32)
}
)";
  error.clear();
  CHECK_FALSE(validateProgram(mapInsert, "/main", error));
  CHECK(
      error.find("pointer escapes via argument to /std/collections/map/Map/insert (root: inner)") !=
      std::string::npos);
}

TEST_CASE("pointer to a local cannot escape through a move parameter the callee keeps") {
  const std::string source = R"(
[struct]
Holder() {
  [i32 mut] dummy{0i32}
  [Pointer<i32> mut] target{location(dummy)}

  [mut]
  keep([Pointer<i32> move] ptr) {
    assign(this.target, ptr)
  }
}

[return<void>]
keep_in([Holder mut] holder, [Pointer<i32> move] ptr) {
  assign(holder.target, ptr)
}

[return<int>]
main() {
  [Holder mut] holder{Holder{}}
  if(true) {
    [i32 mut] inner{2i32}
    __CALL__
  }
  return(0i32)
}
)";
  auto withCall = [&](const std::string &call) {
    std::string text = source;
    text.replace(text.find("__CALL__"), 8, call);
    return text;
  };
  std::string error;
  CHECK_FALSE(validateProgram(withCall("keep_in(holder, location(inner))"), "/main", error));
  CHECK(error.find("pointer escapes via argument to /keep_in (root: inner)") != std::string::npos);
  error.clear();
  CHECK_FALSE(validateProgram(withCall("holder.keep(location(inner))"), "/main", error));
  CHECK(error.find("pointer escapes via argument to /Holder/keep (root: inner)") !=
        std::string::npos);

  const std::string returned = R"(
[struct]
Ref() {
  [i32 mut] dummy{0i32}
  [Pointer<i32> mut] at{location(dummy)}
}

[return<Ref>]
leak() {
  [i32 mut] inner{2i32}
  return(Ref{0i32, location(inner)})
}

[return<int>]
main() {
  return(0i32)
}
)";
  error.clear();
  CHECK_FALSE(validateProgram(returned, "/main", error));
  CHECK(error.find("pointer escapes via return (root: inner)") != std::string::npos);
}

TEST_CASE("pointers kept within their local's block are accepted") {
  const std::string source = R"(
import /std/collections/*

[struct]
Ref() {
  [i32 mut] dummy{0i32}
  [Pointer<i32> mut] at{location(dummy)}
}

[effects(heap_alloc) return<int>]
main() {
  [i32 mut] value{4i32}
  [Pointer<i32> mut] ptr{location(value)}
  [Vector<Ref> mut] refs{vector<Ref>()}
  vectorPush<Ref>(refs, Ref{0i32, location(value)})
  if(true) {
    [i32 mut] inner{2i32}
    [Pointer<i32> mut] local{location(value)}
    assign(local, location(inner))
    [Vector<Ref> mut] innerRefs{vector<Ref>()}
    vectorPush<Ref>(innerRefs, Ref{0i32, location(inner)})
    vectorPush<Ref>(refs, Ref{0i32, location(value)})
    assign(ptr, location(value))
  }
  return(dereference(ptr))
}

[unsafe effects(heap_alloc) return<int>]
unsafe_keep() {
  [Vector<Ref> mut] refs{vector<Ref>()}
  [i32 mut] outer{1i32}
  [Pointer<i32> mut] ptr{location(outer)}
  if(true) {
    [i32 mut] inner{2i32}
    vectorPush<Ref>(refs, Ref{0i32, location(inner)})
    assign(ptr, location(inner))
  }
  return(0i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("vector storage helpers are only callable from unsafe definitions") {
  // docs/spec/type-system.md, Memory safety: a helper that frees or aliases a container's storage is `[unsafe_api]`.
  const std::vector<std::pair<std::string, std::string>> uses = {
      {"vectorBorrowSlot",
       "[Reference<i32>] slot{/std/collections/vector/vectorBorrowSlot<i32>(values, 0i32)}"},
      {"vectorSlotUnsafe",
       "[Pointer<uninitialized<i32>>] slot{/std/collections/vector/vectorSlotUnsafe<i32>(values, "
       "0i32)}"},
      {"vectorTakeSlot", "[i32] taken{/std/collections/vector/vectorTakeSlot<i32>(values, 0i32)}"},
      {"vectorDropSlot", "/std/collections/vector/vectorDropSlot<i32>(values, 0i32)"},
      {"vectorInitSlot", "/std/collections/vector/vectorInitSlot<i32>(values, 0i32, 3i32)"},
      {"vectorFreeStorage",
       "/std/collections/vector/vectorFreeStorage<i32>(/std/intrinsics/memory/"
       "alloc<uninitialized<i32>>(1i32))"},
  };
  for (const auto &[name, use] : uses) {
    INFO(name);
    for (const bool unsafeMain : {false, true}) {
      const std::string source = std::string("import /std/collections/*\n\n[") +
                                 (unsafeMain ? "unsafe " : "") +
                                 "effects(heap_alloc), return<int>]\n"
                                 "main() {\n"
                                 "  [/std/collections/vector/Vector<i32> mut] "
                                 "values{/std/collections/vector/vector<i32>(1i32)}\n"
                                 "  " +
                                 use +
                                 "\n"
                                 "  return(0i32)\n"
                                 "}\n";
      std::string error;
      if (unsafeMain) {
        CHECK(validateProgram(source, "/main", error));
        CHECK(error.empty());
      } else {
        CHECK_FALSE(validateProgram(source, "/main", error));
        CHECK(error.find("calling /std/collections/vector/" + name + "__") != std::string::npos);
        CHECK(error.find("requires an unsafe definition") != std::string::npos);
      }
    }
  }
}

TEST_CASE("soa slot helpers are only callable from unsafe definitions") {
  // docs/spec/type-system.md, Memory safety: the SoA slot helpers are `[unsafe_api]`.
  const std::vector<std::pair<std::string, std::string>> uses = {
      {"soaColumnSlotUnsafe",
       "[Pointer<uninitialized<i32>>] "
       "slot{/std/collections/soa_storage/soaColumnSlotUnsafe<i32>(column, "
       "0i32)}"},
      {"soaColumnBorrowSlot",
       "[Reference<i32>] slot{/std/collections/soa_storage/soaColumnBorrowSlot<i32>(column, "
       "0i32)}"},
      {"soaColumnTakeSlot",
       "[i32] taken{/std/collections/soa_storage/soaColumnTakeSlot<i32>(column, 0i32)}"},
      {"soaColumnDropSlot", "/std/collections/soa_storage/soaColumnDropSlot<i32>(column, 0i32)"},
      {"soaColumnInitSlot",
       "/std/collections/soa_storage/soaColumnInitSlot<i32>(column, 0i32, 3i32)"},
      {"soaColumnDropRange", "/std/collections/soa_storage/soaColumnDropRange<i32>(column, 0i32)"},
  };
  for (const auto &[name, use] : uses) {
    INFO(name);
    for (const bool unsafeMain : {false, true}) {
      const std::string source = std::string("import /std/collections/soa_storage/*\n\n[") +
                                 (unsafeMain ? "unsafe " : "") +
                                 "effects(heap_alloc), return<int>]\n"
                                 "main() {\n"
                                 "  [SoaColumn<i32> mut] column{soaColumnNew<i32>()}\n"
                                 "  soaColumnReserve<i32>(column, 1i32)\n"
                                 "  " +
                                 use +
                                 "\n"
                                 "  return(0i32)\n"
                                 "}\n";
      std::string error;
      if (unsafeMain) {
        CHECK(validateProgram(source, "/main", error));
        CHECK(error.empty());
      } else {
        CHECK_FALSE(validateProgram(source, "/main", error));
        CHECK(error.find("calling /std/collections/soa_storage/" + name + "__") !=
              std::string::npos);
        CHECK(error.find("requires an unsafe definition") != std::string::npos);
      }
    }
  }
}

TEST_CASE("public soa operations stay callable from safe definitions") {
  const std::string source = R"(
import /std/collections/soa_storage/*

[effects(heap_alloc), return<int>]
main() {
  [SoaColumn<i32> mut] column{soaColumnNew<i32>()}
  soaColumnReserve<i32>(column, 2i32)
  soaColumnPush<i32>(column, 2i32)
  soaColumnWrite<i32>(column, 0i32, 4i32)
  [i32] first{soaColumnRead<i32>(column, 0i32)}
  soaColumnClear<i32>(column)
  return(first)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("a pointer used after its root moves is rejected") {
  // docs/spec/type-system.md, Memory safety: a pointer to a binding is a borrow of it until the pointer's last use.
  const std::string prelude = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
take([vector<i32> move] items) {
  return(items.count())
}
)";
  const std::vector<std::pair<std::string, std::string>> rejected = {
      {"move builtin", "  take(move(v))\n  return(dereference(p).count())\n"},
      {"implicit move argument", "  take(v)\n  return(dereference(p).count())\n"},
  };
  for (const auto &[name, tail] : rejected) {
    INFO(name);
    const std::string source = prelude +
                               "\n[effects(heap_alloc), return<int>]\nmain() {\n"
                               "  [vector<i32> mut] v{vector<i32>(1i32, 2i32)}\n"
                               "  [Pointer<vector<i32>>] p{location(v)}\n" +
                               tail + "}\n";
    std::string error;
    CHECK_FALSE(validateProgram(source, "/main", error));
    CHECK(error.find("borrowed binding: v (root: v, sink: p)") != std::string::npos);
  }
}

TEST_CASE("a pointer that is not used after the move of its root is accepted") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
take([vector<i32> move] items) {
  return(items.count())
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] v{vector<i32>(1i32, 2i32)}
  [Pointer<vector<i32>>] p{location(v)}
  [i32] before{dereference(p).count()}
  [i32] moved{take(move(v))}
  [i32 mut] x{1i32}
  [Pointer<i32>] q{location(x)}
  x = 5i32
  return(plus(plus(before, moved), dereference(q)))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("a Reference alias is a borrow when its root is passed to a move parameter") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
take([vector<i32> move] items) {
  return(items.count())
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] v{vector<i32>(1i32, 2i32)}
  [Reference<vector<i32>>] r{location(v)}
  take(v)
  return(r.count())
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("borrowed binding: v (root: v, sink: r)") != std::string::npos);
}

TEST_CASE("a Reference argument changes the container a live view reads") {
  const std::string prelude = R"(
import /std/collections/*

[effects(heap_alloc), return<void>]
wipe([Reference<vector<i32>> mut] target) {
  dereference(target).clear()
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] v{vector<i32>(1i32, 2i32)}
  [vector<i32>] view{v}
)";
  const std::vector<std::pair<std::string, std::string>> rejected = {
      {"location argument", "  wipe(location(v))\n  return(view.count())\n}\n"},
      {"alias argument",
       "  [Reference<vector<i32>>] r{location(v)}\n  wipe(r)\n  return(view.count())\n}\n"},
  };
  for (const auto &[name, tail] : rejected) {
    INFO(name);
    std::string error;
    CHECK_FALSE(validateProgram(prelude + tail, "/main", error));
    CHECK(error.find("borrowed binding: v (root: v, sink: view)") != std::string::npos);
  }
  // The view's last use comes first, so the change is fine.
  std::string error;
  CHECK(validateProgram(prelude +
                            "  [i32] seen{view.count()}\n  wipe(location(v))\n  return(seen)\n}\n",
                        "/main",
                        error));
  CHECK(error.empty());
}

TEST_CASE("a plain Reference argument that the callee changes structurally ends a live view") {
  // docs/spec/pointers-and-references.md: a plain `Reference` may call `mut` helpers, so the callee's body decides.
  const std::string helpers = R"(
import /std/collections/*

[effects(heap_alloc), return<void>]
wipe([Reference<vector<i32>>] target) {
  dereference(target).clear()
}

[effects(heap_alloc), return<void>]
wipe_via_alias([Reference<vector<i32>>] target) {
  [Reference<vector<i32>>] alias{location(target)}
  alias.push(1i32)
}

[effects(heap_alloc), return<void>]
wipe_through_callee([Reference<vector<i32>>] target) {
  wipe(target)
}

[effects(heap_alloc), return<void>]
wipe_after_recursion([Reference<vector<i32>>] target, [i32] depth) {
  if(depth > 0i32) {
    wipe_after_recursion(target, minus(depth, 1i32))
  } else {
    target.clear()
  }
}

[return<int>]
count_ref([Reference<vector<i32>>] target) {
  return(dereference(target).count())
}

[return<int>]
count_through_callee([Reference<vector<i32>>] target) {
  return(count_ref(target))
}

[return<int>]
count_recursive([Reference<vector<i32>>] target, [i32] depth) {
  if(depth > 0i32) {
    return(count_recursive(target, minus(depth, 1i32)))
  }
  return(target.count())
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] v{vector<i32>(1i32, 2i32)}
  [vector<i32>] view{v}
)";
  const std::vector<std::pair<std::string, std::string>> rejected = {
      {"direct", "  wipe(location(v))\n  return(view.count())\n}\n"},
      {"alias local",
       "  [Reference<vector<i32>>] r{location(v)}\n  wipe(r)\n  return(view.count())\n}\n"},
      {"through alias in callee", "  wipe_via_alias(location(v))\n  return(view.count())\n}\n"},
      {"through callee", "  wipe_through_callee(location(v))\n  return(view.count())\n}\n"},
      {"recursion", "  wipe_after_recursion(location(v), 2i32)\n  return(view.count())\n}\n"},
  };
  for (const auto &[name, tail] : rejected) {
    INFO(name);
    std::string error;
    CHECK_FALSE(validateProgram(helpers + tail, "/main", error));
    CHECK(error.find("borrowed binding: v (root: v, sink: view)") != std::string::npos);
  }
  const std::vector<std::pair<std::string, std::string>> accepted = {
      {"read-only", "  [i32] n{count_ref(location(v))}\n  return(plus(n, view.count()))\n}\n"},
      {"read-only through callee",
       "  return(plus(count_through_callee(location(v)), view.count()))\n}\n"},
      {"read-only recursion",
       "  return(plus(count_recursive(location(v), 3i32), view.count()))\n}\n"},
      {"view ends first", "  [i32] seen{view.count()}\n  wipe(location(v))\n  return(seen)\n}\n"},
  };
  for (const auto &[name, tail] : accepted) {
    INFO(name);
    std::string error;
    CHECK(validateProgram(helpers + tail, "/main", error));
    CHECK(error.empty());
  }
}

TEST_CASE("a view of a dereferenced Reference parameter blocks changes through it") {
  // docs/spec/type-system.md, Memory safety: a view borrows the root of its source until its last use.
  const std::string prelude = R"(
import /std/collections/*

[struct]
Bag() {
  [vector<i32> mut] items{vector<i32>()}
}

[effects(heap_alloc), return<int>]
)";
  const std::vector<std::pair<std::string, std::string>> rejected = {
      {
          "clear",
          "shrink([Reference<vector<i32>>] r) {\n  [vector<i32>] view{dereference(r)}\n"
          "  dereference(r).clear()\n  return(view.count())\n}\n",
      },
      {"push",
       "shrink([Reference<vector<i32>>] r) {\n  [vector<i32>] view{dereference(r)}\n"
       "  dereference(r).push(1i32)\n  return(view.count())\n}\n"},
      {"field",
       "shrink([Reference<Bag>] bag) {\n  [vector<i32>] view{dereference(bag).items}\n"
       "  dereference(bag).items.clear()\n  return(view.count())\n}\n"},
  };
  const std::string mainDef = "\n[return<int>]\nmain() {\n  return(0i32)\n}\n";
  for (const auto &[name, definition] : rejected) {
    INFO(name);
    std::string error;
    CHECK_FALSE(validateProgram(prelude + definition + mainDef, "/main", error));
    CHECK(error.find("borrowed binding: ") != std::string::npos);
    CHECK(error.find("sink: view") != std::string::npos);
  }
  const std::vector<std::pair<std::string, std::string>> accepted = {
      {"last use first",
       "shrink([Reference<vector<i32>>] r) {\n  [vector<i32>] view{dereference(r)}\n"
       "  [i32] seen{view.count()}\n  dereference(r).clear()\n  return(seen)\n}\n"},
      {"mut view copies",
       "shrink([Reference<vector<i32>>] r) {\n  [vector<i32> mut] view{dereference(r)}\n"
       "  dereference(r).clear()\n  return(view.count())\n}\n"},
      {"read only use",
       "shrink([Reference<vector<i32>>] r) {\n  [vector<i32>] view{dereference(r)}\n"
       "  return(view.count() + dereference(r).count())\n}\n"},
  };
  for (const auto &[name, definition] : accepted) {
    INFO(name);
    std::string error;
    CHECK(validateProgram(prelude + definition + mainDef, "/main", error));
    CHECK(error.empty());
  }
}

TEST_CASE("a view of a Reference alias blocks changing the local it aliases") {
  // docs/spec/type-system.md, Memory safety: the view reads the aliased local's storage after the alias's last use.
  const std::string prelude = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32, 2i32)}
  [Reference<vector<i32>>] r{location(values)}
)";
  const std::vector<std::pair<std::string, std::string>> rejected = {
      {"clear",
       "  [vector<i32>] view{dereference(r)}\n  values.clear()\n  return(view.count())\n}\n"},
      {"push",
       "  [vector<i32>] view{dereference(r)}\n  values.push(3i32)\n  return(view.count())\n}\n"},
      {"assign",
       "  [vector<i32>] view{dereference(r)}\n  assign(values, vector<i32>(9i32))\n  "
       "return(view.count())\n}\n"},
  };
  for (const auto &[name, body] : rejected) {
    INFO(name);
    std::string error;
    CHECK_FALSE(validateProgram(prelude + body, "/main", error));
    CHECK(error.find("borrowed binding: values (root: values, sink: ") != std::string::npos);
  }
  const std::vector<std::pair<std::string, std::string>> accepted = {
      {"last use first",
       "  [vector<i32>] view{dereference(r)}\n  [i32] seen{view.count()}\n  values.clear()\n  "
       "return(seen)\n}\n"},
      {"mut view copies",
       "  [vector<i32> mut] view{dereference(r)}\n  values.clear()\n  return(view.count())\n}\n"},
      {"no view", "  values.clear()\n  return(dereference(r).count())\n}\n"},
  };
  for (const auto &[name, body] : accepted) {
    INFO(name);
    std::string error;
    CHECK(validateProgram(prelude + body, "/main", error));
    CHECK(error.empty());
  }
}

TEST_CASE("changing a borrowed local directly names the live view as the sink") {
  const std::string prelude = R"(
import /std/collections/*

[struct]
Bag() {
  [vector<i32> mut] items{vector<i32>()}
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] v{vector<i32>(1i32, 2i32)}
  [Bag mut] bag{Bag{}}
  [vector<i32>] view{v}
  [vector<i32>] bagView{bag.items}
)";
  const std::vector<std::tuple<std::string, std::string, std::string, std::string>> rejected = {
      {"assign", "  v = vector<i32>(9i32)\n  return(view.count())\n}\n", "v", "view"},
      {"assign call", "  assign(v, vector<i32>(9i32))\n  return(view.count())\n}\n", "v", "view"},
      {"move", "  [vector<i32> mut] other{move(v)}\n  return(view.count())\n}\n", "v", "view"},
      {"assign to a field",
       "  bag.items = vector<i32>(9i32)\n  return(bagView.count())\n}\n",
       "bag",
       "bagView"},
  };
  for (const auto &[name, body, root, sink] : rejected) {
    INFO(name);
    std::string error;
    CHECK_FALSE(validateProgram(prelude + body, "/main", error));
    CHECK(error.find("borrowed binding: " + root + " (root: " + root + ", sink: " + sink + ")") !=
          std::string::npos);
  }
}

TEST_CASE("a plain Reference parameter that replaces a container field ends a live view") {
  const std::string helpers = R"(
import /std/collections/*

[struct]
Bag() {
  [vector<i32> mut] items{vector<i32>()}
  [i32 mut] hits{0i32}
}

[struct]
Shelf() {
  [Bag mut] bag{Bag{}}
}

[effects(heap_alloc), return<void>]
reset([Reference<Bag>] target) {
  target.items = vector<i32>(7i32)
}

[effects(heap_alloc), return<void>]
reset_assign_call([Reference<Bag>] target) {
  assign(target.items, vector<i32>(7i32))
}

[effects(heap_alloc), return<void>]
reset_nested([Reference<Shelf>] target) {
  target.bag.items = vector<i32>(7i32)
}

[effects(heap_alloc), return<void>]
reset_through_callee([Reference<Bag>] target) {
  reset(target)
}

[return<void>]
bump([Reference<Bag>] target) {
  target.hits = 3i32
}

[return<int>]
read_items([Reference<Bag>] target) {
  return(target.items.count())
}

[effects(heap_alloc), return<int>]
main() {
  [Bag mut] bag{Bag{}}
  bag.items.push(1i32)
  [Shelf mut] shelf{Shelf{}}
  shelf.bag.items.push(1i32)
  [vector<i32>] view{bag.items}
  [vector<i32>] shelfView{shelf.bag.items}
)";
  const std::vector<std::tuple<std::string, std::string, std::string>> rejected = {
      {"field assignment", "  reset(location(bag))\n  return(view.count())\n}\n", "bag"},
      {"assign call", "  reset_assign_call(location(bag))\n  return(view.count())\n}\n", "bag"},
      {"through callee",
       "  reset_through_callee(location(bag))\n  return(view.count())\n}\n",
       "bag"},
      {"nested field",
       "  reset_nested(location(shelf))\n  return(shelfView.count())\n}\n",
       "shelf"},
  };
  for (const auto &[name, tail, root] : rejected) {
    INFO(name);
    std::string error;
    CHECK_FALSE(validateProgram(helpers + tail, "/main", error));
    CHECK(error.find("borrowed binding: " + root + " (root: " + root + ", sink: ") !=
          std::string::npos);
  }
  const std::vector<std::pair<std::string, std::string>> accepted = {
      {"scalar field",
       "  bump(location(bag))\n  return(plus(view.count(), shelfView.count()))\n}\n"},
      {"read only", "  return(plus(read_items(location(bag)), view.count()))\n}\n"},
      {"view ends first",
       "  [i32] seen{view.count()}\n  reset(location(bag))\n  return(plus(seen, "
       "shelfView.count()))\n}\n"},
  };
  for (const auto &[name, tail] : accepted) {
    INFO(name);
    std::string error;
    CHECK(validateProgram(helpers + tail, "/main", error));
    CHECK(error.empty());
  }
}

TEST_CASE("vector and map public helpers stay callable from safe code") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32, 2i32, 3i32)}
  values.push(4i32)
  values.remove_at(0i32)
  values.remove_swap(0i32)
  values.pop()
  [map<i32, i32> mut] table{map<i32, i32>()}
  table.insert(1i32, 5i32)
  table.insert(1i32, 9i32)
  return(plus(values[0i32], table.at(1i32)))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

namespace {

const char *PrivateFieldBoxPrelude = R"(
namespace box {
  [public struct]
  Box() {
    [private i32 mut] secret{7i32}
    [public i32 mut] open{1i32}

    [public return<int>]
    peek() {
      return(this.secret)
    }
  }

  [return<int>]
  sibling([/box/Box] b) {
    return(b.secret)
  }

  namespace inner {
    [return<int>]
    nested([/box/Box mut] b) {
      assign(b.secret, 3i32)
      return(b.secret)
    }
  }
}
)";

std::string privateFieldMain(const std::string &body) {
  return std::string(PrivateFieldBoxPrelude) +
         "\n[return<int>]\nmain() {\n  [/box/Box mut] b{/box/Box{}}\n" + body +
         "\n  return(0i32)\n}\n";
}

} // namespace

TEST_CASE("private fields are rejected outside the struct namespace") {
  // docs/spec/type-system.md, Field visibility.
  const std::vector<std::pair<std::string, std::string>> uses = {
      {"read", "  [i32] x{b.secret}"},
      {"write", "  assign(b.secret, 9i32)"},
      {"location", "  [Pointer<i32> mut] p{location(b.secret)}"},
      {"brace", "  [/box/Box] c{/box/Box{8i32, 2i32}}"},
  };
  for (const auto &[name, use] : uses) {
    INFO(name);
    std::string error;
    CHECK_FALSE(validateProgram(privateFieldMain(use), "/main", error));
    CHECK(error.find("private field is not accessible: /box/Box/secret") != std::string::npos);
  }
}

TEST_CASE("private fields stay usable inside the struct namespace") {
  std::string error;
  // Public fields, the struct body, a sibling definition and a nested namespace.
  CHECK(validateProgram(
      privateFieldMain("  assign(b.open, 4i32)\n  [i32] x{b.peek()}\n  [i32] y{/box/sibling(b)}\n  "
                       "[i32] z{/box/inner/nested(b)}\n  [/box/Box] c{/box/Box{[open] 2i32}}"),
      "/main",
      error));
  CHECK(error.empty());
}

TEST_CASE("container storage fields are private outside the collections namespace") {
  // docs/spec/type-system.md, Memory safety: `Vector`, `MapValue` and `SoaColumn` keep their storage private.
  const std::vector<std::pair<std::string, std::string>> uses = {
      {"/std/collections/vector/Vector/fieldCount",
       "  [/std/collections/vector/Vector<i32> mut] v{/std/collections/vector/vector<i32>(1i32)}\n"
       "  assign(v.fieldCount, 1000i32)"},
      {"/std/collections/vector/Vector/fieldCapacity",
       "  [/std/collections/vector/Vector<i32> mut] v{/std/collections/vector/vector<i32>(1i32)}\n"
       "  [i32] c{v.fieldCapacity}"},
      {"/std/collections/vector/Vector/data",
       "  [/std/collections/vector/Vector<i32> mut] v{/std/collections/vector/vector<i32>(1i32)}\n"
       "  [Pointer<uninitialized<i32>>] d{v.data}"},
      {"/std/collections/map/MapValue/keys",
       "  [/std/collections/map/MapValue<i32, i32> mut] m{/std/collections/map/map<i32, i32>(1i32, "
       "2i32)}\n"
       "  [/std/collections/vector/Vector<i32>] k{m.keys}"},
      {"/std/collections/map/MapValue/payloads",
       "  [/std/collections/map/MapValue<i32, i32> mut] m{/std/collections/map/map<i32, i32>(1i32, "
       "2i32)}\n"
       "  [/std/collections/vector/Vector<i32>] p{m.payloads}"},
      {"/std/collections/soa_storage/SoaColumn/count",
       "  [/std/collections/soa_storage/SoaColumn<i32>] "
       "c{/std/collections/soa_storage/SoaColumn<i32>{1i32, 1i32}}"},
  };
  for (const auto &[field, use] : uses) {
    INFO(field);
    const std::string source =
        "import /std/collections/*\n\n[effects(heap_alloc), return<int>]\nmain() {\n" + use +
        "\n  return(0i32)\n}\n";
    std::string error;
    CHECK_FALSE(validateProgram(source, "/main", error));
    CHECK(error.find("private field is not accessible: " + field) != std::string::npos);
  }
}

TEST_CASE("an empty brace stdlib Vector is a local's initializer") {
  // docs/spec/literals-and-composite-construction.md, Vector binding forms: every field of `Vector` has an
  // initializer, so `Vector<T>{}` is the empty vector.
  const std::string source = R"(import /std/collections/*

[struct]
Item() {
  [i32 mut] n{0i32}
}

[effects(heap_alloc), return<int>]
main() {
  [Vector<Item> mut] local{Vector<Item>{}}
  local.push(Item{3i32})
  [auto mut] inferred{Vector<i32>{}}
  inferred.push(4i32)
  return(local.count() + inferred.count())
}
)";
  std::string error;
  CHECK_MESSAGE(validateProgram(source, "/main", error), error);
  CHECK(error.empty());
}

TEST_CASE("an empty brace stdlib Vector is a field's initializer") {
  const std::string source = R"(import /std/collections/*

[struct]
Item() {
  [i32 mut] n{0i32}
}

[struct]
Bag() {
  [Vector<Item> mut] items{Vector<Item>{}}
}

[effects(heap_alloc), return<int>]
main() {
  [Bag mut] bag{Bag{}}
  bag.items.push(Item{3i32})
  return(bag.items.count())
}
)";
  std::string error;
  CHECK_MESSAGE(validateProgram(source, "/main", error), error);
  CHECK(error.empty());
}

TEST_CASE("a brace stdlib Vector with an entry names a private field") {
  // The entries of the braces are the storage fields, which are private: only the empty form builds a Vector.
  const std::string source = R"(import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [Vector<i32>] values{Vector<i32>{1i32}}
  return(0i32)
}
)";
  primec::SemanticDiagnosticInfo diagnosticInfo;
  std::string error;
  CHECK_FALSE(validateProgramReportingDiagnostic(source, "/main", error, diagnosticInfo));
  CHECK_MESSAGE(
      error.find("private field is not accessible: /std/collections/vector/Vector/fieldCount") !=
          std::string::npos,
      error);
  REQUIRE(diagnosticInfo.hasPrimarySpan);
  CHECK(diagnosticInfo.primarySpan.line == 5);
  CHECK(diagnosticInfo.primarySpan.column == 24);
}

namespace {

void checkPrivateDefinitionCall(const std::string &source,
                                const std::string &expected,
                                int line,
                                int column) {
  primec::SemanticDiagnosticInfo diagnosticInfo;
  std::string error;
  CHECK_FALSE(validateProgramReportingDiagnostic(source, "/main", error, diagnosticInfo));
  CHECK_MESSAGE(error.find(expected) != std::string::npos, error);
  REQUIRE(diagnosticInfo.hasPrimarySpan);
  CHECK(diagnosticInfo.primarySpan.line == line);
  CHECK(diagnosticInfo.primarySpan.column == column);
}

// A program may write definitions under `/std`; they follow the rule of the library's own.
const char *PrivateProbePrelude = R"(namespace std {
  namespace probe {
    [return<int>]
    hidden() {
      return(1i32)
    }

    [private return<int>]
    marked() {
      return(2i32)
    }

    [public return<int>]
    open() {
      return(hidden() + marked())
    }

    [public struct]
    Counter() {
      [i32 mut] total{5i32}

      [private return<int>]
      raw() {
        return(this.total)
      }

      [return<int>]
      peek() {
        return(this.raw())
      }
    }

    [public return<int>]
    sibling([Counter] counter) {
      return(counter.raw())
    }

    namespace inner {
      [public return<int>]
      nested() {
        return(/std/probe/hidden())
      }
    }
  }
}
)";

} // namespace

TEST_CASE("a private helper of the standard library is not callable by absolute path") {
  // docs/spec/backend-type-support.md, Exports: a `/std` definition without `[public]` is private to its namespace.
  const std::vector<std::pair<std::string, std::string>> uses = {
      {"vectorCheckShape", "  /std/collections/vector/vectorCheckShape<i32>(values)"},
      {"vectorCheckIndex", "  /std/collections/vector/vectorCheckIndex<i32>(values, 0i32)"},
      {"vectorReserveInternal",
       "  /std/collections/vector/vectorReserveInternal<i32>(values, 8i32)"},
  };
  for (const auto &[name, use] : uses) {
    INFO(name);
    const std::string source =
        "import /std/collections/*\n\n[effects(heap_alloc), return<int>]\nmain() {\n"
        "  [Vector<i32> mut] values{/std/collections/vector/vector<i32>(1i32)}\n" +
        use + "\n  return(0i32)\n}\n";
    checkPrivateDefinitionCall(
        source,
        "/std/collections/vector/" + name +
            " is private: only definitions in /std/collections/vector may call it",
        6,
        3);
  }
}

TEST_CASE("a private definition written under /std is not callable from outside its namespace") {
  const std::string hidden = std::string(PrivateProbePrelude) +
                             "\n[return<int>]\nmain() {\n  return(/std/probe/hidden())\n}\n";
  checkPrivateDefinitionCall(
      hidden, "/std/probe/hidden is private: only definitions in /std/probe may call it", 49, 10);
  const std::string marked = std::string(PrivateProbePrelude) +
                             "\n[return<int>]\nmain() {\n  return(/std/probe/marked())\n}\n";
  checkPrivateDefinitionCall(
      marked, "/std/probe/marked is private: only definitions in /std/probe may call it", 49, 10);
}

TEST_CASE("a private method of a /std struct is not callable from outside the struct's namespace") {
  const std::string source =
      std::string(PrivateProbePrelude) +
      "\n[return<int>]\nmain() {\n  [/std/probe/Counter] counter{/std/probe/Counter{}}\n"
      "  return(counter.raw())\n}\n";
  checkPrivateDefinitionCall(
      source,
      "/std/probe/Counter/raw is private: only definitions in /std/probe may call it",
      50,
      10);
}

TEST_CASE(
    "a private /std definition is callable from its namespace and the namespaces nested in it") {
  // The public definitions of the namespace, a nested namespace, the struct's own method and a sibling of the
  // struct reach the private ones; a method without a visibility transform is callable where its struct is.
  const std::string source =
      std::string(PrivateProbePrelude) +
      "\n[return<int>]\nmain() {\n  [/std/probe/Counter] counter{/std/probe/Counter{}}\n"
      "  [i32] a{/std/probe/open()}\n  [i32] b{/std/probe/inner/nested()}\n"
      "  [i32] c{counter.peek()}\n  [i32] d{/std/probe/sibling(counter)}\n"
      "  return(plus(plus(a, b), plus(c, d)))\n}\n";
  primec::SemanticDiagnosticInfo diagnosticInfo;
  std::string error;
  CHECK_MESSAGE(validateProgramReportingDiagnostic(source, "/main", error, diagnosticInfo), error);
  CHECK(error.empty());
}

TEST_CASE("a definition added to a library namespace calls its private helpers") {
  const std::string source = R"(import /std/collections/*

namespace std {
  namespace collections {
    namespace vector {
      [public return<int>]
      checked_count([Vector<i32>] values) {
        vectorCheckShape<i32>(values)
        return(vectorCount<i32>(values))
      }
    }
  }
}

[effects(heap_alloc), return<int>]
main() {
  [Vector<i32> mut] values{/std/collections/vector/vector<i32>(1i32)}
  return(/std/collections/vector/checked_count(values))
}
)";
  primec::SemanticDiagnosticInfo diagnosticInfo;
  std::string error;
  CHECK_MESSAGE(validateProgramReportingDiagnostic(source, "/main", error, diagnosticInfo), error);
  CHECK(error.empty());
}

TEST_CASE("outside /std a private definition stays callable within the compilation unit") {
  const std::string source = R"(namespace util {
  [return<int>]
  helper() {
    return(3i32)
  }

  [private return<int>]
  marked() {
    return(4i32)
  }
}

[return<int>]
main() {
  return(/util/helper() + /util/marked())
}
)";
  primec::SemanticDiagnosticInfo diagnosticInfo;
  std::string error;
  CHECK_MESSAGE(validateProgramReportingDiagnostic(source, "/main", error, diagnosticInfo), error);
  CHECK(error.empty());
}

TEST_SUITE_END();
