#include "test_compile_run_helpers.h"

#include "test_compile_run_native_backend_core_helpers.h"

#if PRIMESTRUCT_NATIVE_CORE_ENABLED
TEST_SUITE_BEGIN("primestruct.compile.run.native_backend.core");

TEST_CASE("native materializes variadic array packs with indexed count methods") {
  const std::string source = R"(
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
)";
  const std::string srcPath = writeTemp("compile_native_variadic_args_array.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_variadic_args_array").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 48);
}

TEST_CASE("native materializes variadic borrowed array packs with indexed count methods") {
  const std::string source = R"(
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
)";
  const std::string srcPath = writeTemp("compile_native_variadic_args_borrowed_array.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_variadic_args_borrowed_array").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 11);
}

TEST_CASE("native materializes variadic borrowed array packs with indexed dereference access helpers") {
  const std::string source = R"(
[return<int>]
score_refs([args<Reference<array<i32>>>] values) {
  [auto] head{at_unsafe(dereference(at(values, 0i32)), 1i32)}
  return(plus(head, dereference(at(values, 2i32)).at(0i32)))
}

[return<int>]
forward([args<Reference<array<i32>>>] values) {
  return(score_refs([spread] values))
}

[return<int>]
forward_mixed([args<Reference<array<i32>>>] values) {
  [array<i32>] extra{array<i32>(1i32, 2i32)}
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

  [array<i32>] b0{array<i32>(7i32, 8i32)}
  [array<i32>] b1{array<i32>(9i32)}
  [array<i32>] b2{array<i32>(10i32, 11i32)}
  [Reference<array<i32>>] s0{location(b0)}
  [Reference<array<i32>>] s1{location(b1)}
  [Reference<array<i32>>] s2{location(b2)}

  [array<i32>] c0{array<i32>(12i32)}
  [array<i32>] c1{array<i32>(13i32, 14i32, 15i32)}
  [Reference<array<i32>>] t0{location(c0)}
  [Reference<array<i32>>] t1{location(c1)}

  return(plus(score_refs(r0, r1, r2),
              plus(forward(s0, s1, s2),
                   forward_mixed(t0, t1))))
}
)";
  const std::string srcPath = writeTemp("compile_native_variadic_args_borrowed_array_deref_access.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_variadic_args_borrowed_array_deref_access").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 96);
}

TEST_CASE("native materializes variadic pointer array packs with indexed count methods") {
  const std::string source = R"(
[return<int>]
score_ptrs([args<Pointer<array<i32>>>] values) {
  return(plus(values[0i32].count(), values[2i32].count()))
}

[return<int>]
forward([args<Pointer<array<i32>>>] values) {
  return(score_ptrs([spread] values))
}

[return<int>]
forward_mixed([args<Pointer<array<i32>>>] values) {
  [array<i32>] extra{array<i32>(1i32)}
  [Pointer<array<i32>>] extra_ptr{location(extra)}
  return(score_ptrs(extra_ptr, [spread] values))
}

[return<int>]
main() {
  [array<i32>] a0{array<i32>(1i32, 2i32)}
  [array<i32>] a1{array<i32>(3i32)}
  [array<i32>] a2{array<i32>(4i32, 5i32, 6i32)}
  [Pointer<array<i32>>] p0{location(a0)}
  [Pointer<array<i32>>] p1{location(a1)}
  [Pointer<array<i32>>] p2{location(a2)}

  [array<i32>] b0{array<i32>(7i32)}
  [array<i32>] b1{array<i32>(8i32, 9i32)}
  [array<i32>] b2{array<i32>(10i32)}
  [Pointer<array<i32>>] q0{location(b0)}
  [Pointer<array<i32>>] q1{location(b1)}
  [Pointer<array<i32>>] q2{location(b2)}

  [array<i32>] c0{array<i32>(11i32, 12i32)}
  [array<i32>] c1{array<i32>(13i32, 14i32, 15i32)}
  [Pointer<array<i32>>] r0{location(c0)}
  [Pointer<array<i32>>] r1{location(c1)}

  return(plus(score_ptrs(p0, p1, p2),
              plus(forward(q0, q1, q2),
                   forward_mixed(r0, r1))))
}
)";
  const std::string srcPath = writeTemp("compile_native_variadic_args_pointer_array.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_variadic_args_pointer_array").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 11);
}

TEST_CASE("native materializes variadic pointer array packs with indexed dereference access helpers") {
  const std::string source = R"(
[return<int>]
score_ptrs([args<Pointer<array<i32>>>] values) {
  [auto] head{at_unsafe(dereference(at(values, 0i32)), 1i32)}
  return(plus(head, dereference(at(values, 2i32)).at(0i32)))
}

[return<int>]
forward([args<Pointer<array<i32>>>] values) {
  return(score_ptrs([spread] values))
}

[return<int>]
forward_mixed([args<Pointer<array<i32>>>] values) {
  [array<i32>] extra{array<i32>(1i32, 2i32)}
  [Pointer<array<i32>>] extra_ptr{location(extra)}
  return(score_ptrs(extra_ptr, [spread] values))
}

[return<int>]
main() {
  [array<i32>] a0{array<i32>(1i32, 2i32)}
  [array<i32>] a1{array<i32>(3i32)}
  [array<i32>] a2{array<i32>(4i32, 5i32, 6i32)}
  [Pointer<array<i32>>] p0{location(a0)}
  [Pointer<array<i32>>] p1{location(a1)}
  [Pointer<array<i32>>] p2{location(a2)}

  [array<i32>] b0{array<i32>(7i32, 8i32)}
  [array<i32>] b1{array<i32>(9i32)}
  [array<i32>] b2{array<i32>(10i32, 11i32)}
  [Pointer<array<i32>>] q0{location(b0)}
  [Pointer<array<i32>>] q1{location(b1)}
  [Pointer<array<i32>>] q2{location(b2)}

  [array<i32>] c0{array<i32>(12i32)}
  [array<i32>] c1{array<i32>(13i32, 14i32, 15i32)}
  [Pointer<array<i32>>] r0{location(c0)}
  [Pointer<array<i32>>] r1{location(c1)}

  return(plus(score_ptrs(p0, p1, p2),
              plus(forward(q0, q1, q2),
                   forward_mixed(r0, r1))))
}
)";
  const std::string srcPath = writeTemp("compile_native_variadic_args_pointer_array_deref_access.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_variadic_args_pointer_array_deref_access").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 96);
}

TEST_CASE("native materializes variadic scalar reference packs with indexed dereference") {
  const std::string source = R"(
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
)";
  const std::string srcPath = writeTemp("compile_native_variadic_args_scalar_reference.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_variadic_args_scalar_reference").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 144);
}

TEST_CASE("native rejects variadic struct reference packs with indexed field and helper access") {
  const std::string source = R"(
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
  [Pair] extra{Pair(5i32)}
  [Reference<Pair>] extra_ref{location(extra)}
  return(score_refs(location(borrow_ref(extra_ref)), [spread] values))
}

[return<int>]
main() {
  [Pair] a0{Pair(7i32)}
  [Pair] a1{Pair(8i32)}
  [Pair] a2{Pair(9i32)}
  [Reference<Pair>] r0{location(a0)}
  [Reference<Pair>] r1{location(a1)}
  [Reference<Pair>] r2{location(a2)}

  [Pair] b0{Pair(11i32)}
  [Pair] b1{Pair(12i32)}
  [Pair] b2{Pair(13i32)}
  [Reference<Pair>] s0{location(b0)}
  [Reference<Pair>] s1{location(b1)}
  [Reference<Pair>] s2{location(b2)}

  [Pair] c0{Pair(15i32)}
  [Pair] c1{Pair(17i32)}
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
)";
  const std::string srcPath = writeTemp("compile_native_variadic_args_struct_reference.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_variadic_args_struct_reference").string();
  const std::string errPath =
      (testScratchPath("") / "primec_native_variadic_args_struct_reference.err").string();

  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("Native lowering error: unknown struct field: value") !=
        std::string::npos);
}

TEST_CASE("native rejects variadic struct pointer packs with indexed field and helper access") {
  const std::string source = R"(
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
score_ptrs([args<Pointer<Pair>>] values) {
  return(plus(values[0i32].value, values[2i32].score()))
}

[return<int>]
forward([args<Pointer<Pair>>] values) {
  return(score_ptrs([spread] values))
}

[return<int>]
forward_mixed([args<Pointer<Pair>>] values) {
  [Pair] extra{Pair(5i32)}
  [Reference<Pair>] extra_ref{location(extra)}
  return(score_ptrs(location(borrow_ref(extra_ref)), [spread] values))
}

[return<int>]
main() {
  [Pair] a0{Pair(7i32)}
  [Pair] a1{Pair(8i32)}
  [Pair] a2{Pair(9i32)}
  [Reference<Pair>] r0{location(a0)}
  [Reference<Pair>] r1{location(a1)}
  [Reference<Pair>] r2{location(a2)}

  [Pair] b0{Pair(11i32)}
  [Pair] b1{Pair(12i32)}
  [Pair] b2{Pair(13i32)}
  [Reference<Pair>] s0{location(b0)}
  [Reference<Pair>] s1{location(b1)}
  [Reference<Pair>] s2{location(b2)}

  [Pair] c0{Pair(15i32)}
  [Pair] c1{Pair(17i32)}
  [Reference<Pair>] t0{location(c0)}
  [Reference<Pair>] t1{location(c1)}

  return(plus(score_ptrs(location(borrow_ref(r0)),
                         location(borrow_ref(r1)),
                         location(borrow_ref(r2))),
              plus(forward(location(borrow_ref(s0)),
                           location(borrow_ref(s1)),
                           location(borrow_ref(s2))),
                   forward_mixed(location(borrow_ref(t0)),
                                 location(borrow_ref(t1))))))
}
)";
  const std::string srcPath = writeTemp("compile_native_variadic_args_struct_pointer.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_variadic_args_struct_pointer").string();
  const std::string errPath =
      (testScratchPath("") / "primec_native_variadic_args_struct_pointer.err").string();

  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("Native lowering error: unknown struct field: value") !=
        std::string::npos);
}

TEST_CASE("native materializes variadic scalar pointer packs with indexed dereference") {
  const std::string source = R"(
[return<Reference<i32>>]
borrow_ref([Reference<i32>] value) {
  return(value)
}

[return<int>]
score_ptrs([args<Pointer<i32>>] values) {
  return(plus(dereference(values[0i32]), dereference(values[2i32])))
}

[return<int>]
forward([args<Pointer<i32>>] values) {
  return(score_ptrs([spread] values))
}

[return<int>]
forward_mixed([args<Pointer<i32>>] values) {
  [i32] extra{1i32}
  [Reference<i32>] extra_ref{location(extra)}
  return(score_ptrs(location(borrow_ref(extra_ref)), [spread] values))
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

  return(plus(score_ptrs(location(borrow_ref(r0)),
                         location(borrow_ref(r1)),
                         location(borrow_ref(r2))),
              plus(forward(location(borrow_ref(s0)),
                           location(borrow_ref(s1)),
                           location(borrow_ref(s2))),
                   forward_mixed(location(borrow_ref(t0)),
                                 location(borrow_ref(t1))))))
}
)";
  const std::string srcPath = writeTemp("compile_native_variadic_args_scalar_pointer.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_variadic_args_scalar_pointer").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 144);
}

TEST_CASE("native rejects variadic scalar pointer packs from borrowed pack access") {
  const std::string source = R"(
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
)";
  const std::string srcPath = writeTemp("compile_native_variadic_args_scalar_pointer_pack_access.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() / "primec_native_variadic_args_scalar_pointer_pack_access").string();
  const std::string errPath =
      (testScratchPath("") / "primec_native_variadic_args_scalar_pointer_pack_access.err").string();

  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("Native lowering error: variadic parameter type mismatch") !=
        std::string::npos);
}


TEST_SUITE_END();
#endif
