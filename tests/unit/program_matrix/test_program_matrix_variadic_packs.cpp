#include "program_matrix.h"

TEST_SUITE_BEGIN("primestruct.program_matrix.variadic_packs");

// Variadic `[args<T>]` packs of arrays, references, pointers, vectors and maps (docs/spec/language-core.md, variadic
// packs): indexed access, forwarding with `[spread]` and mixing a leading argument with a spread pack give the same
// exit code on every backend. The expected values are worked out by hand from each program's call tree.

namespace {

void runPack(const char *name, const char *source, int exitCode) {
  program_matrix::ProgramCase program;
  program.name = name;
  program.source = source;
  program.exitCode = exitCode;
  // Wasm runs the packs of arrays and of references and pointers to scalars and structs; the vector and map packs need the
  // heap.
  static const std::vector<std::string> wasmPrograms = {"variadic_map_pack_count",
                                                        "variadic_map_pack_try_at",
                                                        "variadic_borrowed_vector_pack",
                                                        "variadic_vector_pointer_pack",
                                                        "variadic_array_pack",
                                                        "variadic_borrowed_array_pack",
                                                        "variadic_scalar_pointer_pack",
                                                        "variadic_scalar_reference_pack",
                                                        "variadic_struct_reference_pack"};
  if (program_matrix::listed(wasmPrograms, name)) {
    program.extraConfigs = {"wasm-wasi"};
  }
  program_matrix::runProgramMatrix(program);
}

} // namespace

TEST_CASE("array packs keep their elements through forwarding and spreading") {
  runPack("variadic_array_pack",
          R"(
[return<int>]
score_arrays([args<array<i32>>] values) {
  return(plus(values[0i32].count(), values[2i32].count()))
}

[return<int>]
forward([args<array<i32>>] values) {
  return(score_arrays([spread] values))
}

[return<int>]
forward_mixed([args<array<i32>>] values) {
  return(score_arrays(array<i32>(1i32), [spread] values))
}

[return<int>]
main() {
  return(plus(score_arrays(array<i32>(1i32, 2i32), array<i32>(3i32), array<i32>(4i32, 5i32, 6i32)),
              plus(forward(array<i32>(7i32), array<i32>(8i32, 9i32), array<i32>(10i32)),
                   forward_mixed(array<i32>(11i32, 12i32), array<i32>(13i32, 14i32, 15i32)))))
}
)",
          11);
  runPack("variadic_borrowed_array_pack",
          R"(
[return<int>]
score_refs([args<Reference<array<i32>>>] values) {
  return(plus(values[0i32].count(), values[2i32].count()))
}

[return<int>]
forward([args<Reference<array<i32>>>] values) {
  return(score_refs([spread] values))
}

[return<int>]
forward_mixed([args<Reference<array<i32>>>] values) {
  [array<i32>] extra{array<i32>(1i32)}
  [Reference<array<i32>>] extra_ref{location(extra)}
  return(score_refs(extra_ref, [spread] values))
}

[return<int>]
main() {
  [array<i32>] a0{array<i32>(1i32, 2i32)}
  [array<i32>] a1{array<i32>(3i32)}
  [array<i32>] a2{array<i32>(4i32, 5i32, 6i32)}
  [Reference<array<i32>>] r0{location(a0)}
  [Reference<array<i32>>] r1{location(a1)}
  [Reference<array<i32>>] r2{location(a2)}

  [array<i32>] b0{array<i32>(7i32)}
  [array<i32>] b1{array<i32>(8i32, 9i32)}
  [array<i32>] b2{array<i32>(10i32)}
  [Reference<array<i32>>] s0{location(b0)}
  [Reference<array<i32>>] s1{location(b1)}
  [Reference<array<i32>>] s2{location(b2)}

  [array<i32>] c0{array<i32>(11i32, 12i32)}
  [array<i32>] c1{array<i32>(13i32, 14i32, 15i32)}
  [Reference<array<i32>>] t0{location(c0)}
  [Reference<array<i32>>] t1{location(c1)}

  return(plus(score_refs(r0, r1, r2),
              plus(forward(s0, s1, s2),
                   forward_mixed(t0, t1))))
}
)",
          11);
}

TEST_CASE("reference packs read scalars and struct fields through indexed dereference") {
  runPack("variadic_scalar_reference_pack",
          R"(
[return<Reference<i32>>]
borrow_ref([Reference<i32>] value) {
  return(value)
}

[return<int>]
score_refs([args<Reference<i32>>] values) {
  return(plus(dereference(values[0i32]), dereference(values[2i32])))
}

[return<int>]
forward([args<Reference<i32>>] values) {
  return(score_refs([spread] values))
}

[return<int>]
forward_mixed([args<Reference<i32>>] values) {
  [i32] extra{1i32}
  [Reference<i32>] extra_ref{location(extra)}
  return(score_refs(location(borrow_ref(extra_ref)), [spread] values))
}

[return<int>]
main() {
  [i32] a0{1i32}
  [i32] a1{2i32}
  [i32] a2{3i32}
  [Reference<i32>] r0{location(a0)}
  [Reference<i32>] r1{location(a1)}
  [Reference<i32>] r2{location(a2)}

  [i32] b0{4i32}
  [i32] b1{5i32}
  [i32] b2{6i32}
  [Reference<i32>] s0{location(b0)}
  [Reference<i32>] s1{location(b1)}
  [Reference<i32>] s2{location(b2)}

  [i32] c0{7i32}
  [i32] c1{8i32}
  [Reference<i32>] t0{location(c0)}
  [Reference<i32>] t1{location(c1)}

  return(plus(score_refs(location(borrow_ref(r0)),
                         location(borrow_ref(r1)),
                         location(borrow_ref(r2))),
              plus(forward(location(borrow_ref(s0)),
                           location(borrow_ref(s1)),
                           location(borrow_ref(s2))),
                   forward_mixed(location(borrow_ref(t0)),
                                 location(borrow_ref(t1))))))
}
)",
          23);
  runPack("variadic_struct_reference_pack",
          R"(
[struct]
Pair() {
  [i32] value{7i32}
}

[return<int>]
/Pair/score([Pair] self) {
  return(plus(self.value, 1i32))
}

[return<Reference<Pair>>]
borrow_ref([Reference<Pair>] value) {
  return(value)
}

[return<int>]
score_refs([args<Reference<Pair>>] values) {
  return(plus(values[0i32].value, values[2i32].score()))
}

[return<int>]
forward([args<Reference<Pair>>] values) {
  return(score_refs([spread] values))
}

[return<int>]
forward_mixed([args<Reference<Pair>>] values) {
  [Pair] extra{Pair{5i32}}
  [Reference<Pair>] extra_ref{location(extra)}
  return(score_refs(location(borrow_ref(extra_ref)), [spread] values))
}

[return<int>]
main() {
  [Pair] a0{Pair{7i32}}
  [Pair] a1{Pair{8i32}}
  [Pair] a2{Pair{9i32}}
  [Reference<Pair>] r0{location(a0)}
  [Reference<Pair>] r1{location(a1)}
  [Reference<Pair>] r2{location(a2)}

  [Pair] b0{Pair{11i32}}
  [Pair] b1{Pair{12i32}}
  [Pair] b2{Pair{13i32}}
  [Reference<Pair>] s0{location(b0)}
  [Reference<Pair>] s1{location(b1)}
  [Reference<Pair>] s2{location(b2)}

  [Pair] c0{Pair{15i32}}
  [Pair] c1{Pair{17i32}}
  [Reference<Pair>] t0{location(c0)}
  [Reference<Pair>] t1{location(c1)}

  return(plus(score_refs(location(borrow_ref(r0)),
                         location(borrow_ref(r1)),
                         location(borrow_ref(r2))),
              plus(forward(location(borrow_ref(s0)),
                           location(borrow_ref(s1)),
                           location(borrow_ref(s2))),
                   forward_mixed(location(borrow_ref(t0)),
                                 location(borrow_ref(t1))))))
}
)",
          65);
}

TEST_CASE("pointer packs of scalars and vectors read through indexed dereference") {
  runPack("variadic_scalar_pointer_pack",
          R"(
[return<int>]
score_ptrs([args<Pointer<i32>>] values) {
  return(plus(dereference(values[0i32]), dereference(values[2i32])))
}

[return<int>]
score_from_at([args<Reference<i32>>] values) {
  return(score_ptrs(location(at(values, 0i32)),
                    location(values.at(1i32)),
                    location(values.at_unsafe(2i32))))
}

[return<int>]
forward([args<Reference<i32>>] values) {
  return(score_from_at([spread] values))
}

[return<int>]
forward_mixed([args<Reference<i32>>] values) {
  [i32] extra{1i32}
  [Reference<i32>] extra_ref{location(extra)}
  return(score_ptrs(location(at(values, 0i32)),
                    location(extra_ref),
                    location(values.at_unsafe(1i32))))
}

[return<int>]
main() {
  [i32] a0{1i32}
  [i32] a1{2i32}
  [i32] a2{3i32}
  [Reference<i32>] r0{location(a0)}
  [Reference<i32>] r1{location(a1)}
  [Reference<i32>] r2{location(a2)}

  [i32] b0{4i32}
  [i32] b1{5i32}
  [i32] b2{6i32}
  [Reference<i32>] s0{location(b0)}
  [Reference<i32>] s1{location(b1)}
  [Reference<i32>] s2{location(b2)}

  [i32] c0{7i32}
  [i32] c1{8i32}
  [Reference<i32>] t0{location(c0)}
  [Reference<i32>] t1{location(c1)}

  return(plus(score_from_at(r0, r1, r2),
              plus(forward(s0, s1, s2),
                   forward_mixed(t0, t1))))
}
)",
          29);
  runPack("variadic_vector_pointer_pack",
          R"(
import /std/collections/*

[return<int>]
score_ptrs([args<Pointer<vector<i32>>>] values) {
  [auto] head{at_unsafe(dereference(at(values, 0i32)), 1i32)}
  return(plus(head, dereference(at(values, 2i32)).at(0i32)))
}

[return<int>]
forward([args<Pointer<vector<i32>>>] values) {
  return(score_ptrs([spread] values))
}

[effects(heap_alloc), return<int>]
forward_mixed([args<Pointer<vector<i32>>>] values) {
  [vector<i32>] extra{vector<i32>(1i32, 2i32)}
  [Pointer<vector<i32>>] extra_ptr{location(extra)}
  return(score_ptrs(extra_ptr, [spread] values))
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] a0{vector<i32>(1i32, 2i32)}
  [vector<i32>] a1{/std/collections/vector/vector<i32>(3i32)}
  [vector<i32>] a2{vector<i32>(4i32, 5i32, 6i32)}
  [Pointer<vector<i32>>] r0{location(a0)}
  [Pointer<vector<i32>>] r1{location(a1)}
  [Pointer<vector<i32>>] r2{location(a2)}

  [vector<i32>] b0{vector<i32>(7i32, 8i32)}
  [vector<i32>] b1{/std/collections/vector/vector<i32>(9i32)}
  [vector<i32>] b2{vector<i32>(10i32, 11i32)}
  [Pointer<vector<i32>>] s0{location(b0)}
  [Pointer<vector<i32>>] s1{location(b1)}
  [Pointer<vector<i32>>] s2{location(b2)}

  [vector<i32>] c0{/std/collections/vector/vector<i32>(12i32)}
  [vector<i32>] c1{vector<i32>(13i32, 14i32, 15i32)}
  [Pointer<vector<i32>>] t0{location(c0)}
  [Pointer<vector<i32>>] t1{location(c1)}

  return(plus(score_ptrs(r0, r1, r2),
              plus(forward(s0, s1, s2),
                   forward_mixed(t0, t1))))
}
)",
          39);
}

TEST_CASE("borrowed vector packs count their elements after forwarding") {
  runPack("variadic_borrowed_vector_pack",
          R"(
import /std/collections/*

[return<int>]
score_refs([args<Reference<vector<i32>>>] values) {
  [vector<i32>] head{dereference(at(values, 0i32))}
  [vector<i32>] tail{dereference(at(values, 2i32))}
  return(plus(/std/collections/vector/count(head),
              /std/collections/vector/count(tail)))
}

[return<int>]
forward([args<Reference<vector<i32>>>] values) {
  return(score_refs([spread] values))
}

[effects(heap_alloc), return<int>]
forward_mixed([args<Reference<vector<i32>>>] values) {
  [vector<i32>] extra{/std/collections/vector/vector<i32>(1i32)}
  [Reference<vector<i32>>] extra_ref{location(extra)}
  return(score_refs(extra_ref, [spread] values))
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] a0{vector<i32>(1i32, 2i32)}
  [vector<i32>] a1{/std/collections/vector/vector<i32>(3i32)}
  [vector<i32>] a2{vector<i32>(4i32, 5i32, 6i32, 7i32)}
  [Reference<vector<i32>>] r0{location(a0)}
  [Reference<vector<i32>>] r1{location(a1)}
  [Reference<vector<i32>>] r2{location(a2)}

  [vector<i32>] b0{/std/collections/vector/vector<i32>(8i32)}
  [vector<i32>] b1{vector<i32>(9i32, 10i32)}
  [vector<i32>] b2{vector<i32>(11i32, 12i32, 13i32)}
  [Reference<vector<i32>>] s0{location(b0)}
  [Reference<vector<i32>>] s1{location(b1)}
  [Reference<vector<i32>>] s2{location(b2)}

  [vector<i32>] c0{vector<i32>(14i32, 15i32)}
  [vector<i32>] c1{vector<i32>(16i32, 17i32, 18i32, 19i32, 20i32)}
  [Reference<vector<i32>>] t0{location(c0)}
  [Reference<vector<i32>>] t1{location(c1)}

  return(plus(score_refs(r0, r1, r2),
              plus(forward(s0, s1, s2),
                   forward_mixed(t0, t1))))
}
)",
          16);
}

TEST_CASE("map packs keep count and tryAt results through forwarding") {
  runPack("variadic_map_pack_try_at",
          R"(
import /std/collections/*

[return<int>]
score_maps([args<map<i32, i32>>] values) {
  [auto] head{/std/collections/map/at_unsafe<i32, i32>(at(values, 0i32), 3i32)}
  [auto] tailMissing{
      Result.error(/std/collections/map/tryAt<i32, i32>(at(values, minus(count(values), 1i32)), 99i32))}
  return(if(tailMissing,
            then(){ return(plus(head, /std/collections/map/at_unsafe<i32, i32>(at(values, minus(count(values), 1i32)), 11i32))) },
            else(){ return(0i32) }))
}

[return<int>]
forward([args<map<i32, i32>>] values) {
  return(score_maps([spread] values))
}

[return<int>]
forward_mixed([args<map<i32, i32>>] values) {
  return(score_maps(map<i32, i32>(1i32, 2i32, 3i32, 8i32), [spread] values))
}

[return<int>]
main() {
  return(plus(score_maps(map<i32, i32>(1i32, 2i32, 3i32, 4i32),
                         map<i32, i32>(5i32, 6i32),
                         map<i32, i32>(7i32, 8i32, 9i32, 10i32, 11i32, 12i32)),
              plus(forward(map<i32, i32>(1i32, 2i32, 3i32, 6i32),
                           map<i32, i32>(5i32, 6i32),
                           map<i32, i32>(7i32, 8i32, 9i32, 10i32, 11i32, 14i32)),
                   forward_mixed(map<i32, i32>(7i32, 8i32, 9i32, 10i32, 11i32, 16i32)))))
}
)",
          60);
  runPack("variadic_map_pack_count",
          R"(
import /std/collections/*
import /std/collections/map/*

[return<int> effects(heap_alloc)]
score_maps([args<map<i32, i32>>] values) {
  [map<i32, i32>] head{at(values, 0i32)}
  [map<i32, i32>] tail{at(values, 2i32)}
  return(plus(/std/collections/map/count<i32, i32>(head),
              /std/collections/map/count<i32, i32>(tail)))
}

[return<int> effects(heap_alloc)]
forward([args<map<i32, i32>>] values) {
  return(score_maps([spread] values))
}

[return<int> effects(heap_alloc)]
forward_mixed([args<map<i32, i32>>] values) {
  return(score_maps(map<i32, i32>(1i32, 2i32), [spread] values))
}

[return<int> effects(heap_alloc)]
main() {
  return(plus(score_maps(map<i32, i32>(1i32, 2i32, 3i32, 4i32),
                         map<i32, i32>(5i32, 6i32),
                         map<i32, i32>(7i32, 8i32, 9i32, 10i32, 11i32, 12i32)),
              plus(forward(map<i32, i32>(13i32, 14i32),
                           map<i32, i32>(15i32, 16i32, 17i32, 18i32),
                           map<i32, i32>(19i32, 20i32)),
                   forward_mixed(map<i32, i32>(21i32, 22i32, 23i32, 24i32),
                                 map<i32, i32>(25i32, 26i32, 27i32, 28i32, 29i32, 30i32)))))
}
)",
          11);
}

TEST_SUITE_END();
