#include "test_compile_run_helpers.h"

#include "test_compile_run_native_backend_core_helpers.h"

#if PRIMESTRUCT_NATIVE_CORE_ENABLED
TEST_SUITE_BEGIN("primestruct.compile.run.native_backend.core");

TEST_CASE("native materializes variadic borrowed map packs with indexed count methods") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
score_refs([args<Reference</std/collections/map<i32, i32>>>] values) {
  return(plus(values[0i32].count(), values[2i32].count()))
}

[return<int>]
forward([args<Reference</std/collections/map<i32, i32>>>] values) {
  return(score_refs([spread] values))
}

[return<int>]
forward_mixed([args<Reference</std/collections/map<i32, i32>>>] values) {
  [/std/collections/map<i32, i32>] extra{map<i32, i32>(1i32, 2i32)}
  [Reference</std/collections/map<i32, i32>>] extra_ref{location(extra)}
  return(score_refs(extra_ref, [spread] values))
}

[return<int>]
main() {
  [/std/collections/map<i32, i32>] a0{map<i32, i32>(1i32, 2i32, 3i32, 4i32)}
  [/std/collections/map<i32, i32>] a1{map<i32, i32>(5i32, 6i32)}
  [/std/collections/map<i32, i32>] a2{map<i32, i32>(7i32, 8i32, 9i32, 10i32, 11i32, 12i32)}
  [Reference</std/collections/map<i32, i32>>] r0{location(a0)}
  [Reference</std/collections/map<i32, i32>>] r1{location(a1)}
  [Reference</std/collections/map<i32, i32>>] r2{location(a2)}

  [/std/collections/map<i32, i32>] b0{map<i32, i32>(13i32, 14i32)}
  [/std/collections/map<i32, i32>] b1{map<i32, i32>(15i32, 16i32, 17i32, 18i32)}
  [/std/collections/map<i32, i32>] b2{map<i32, i32>(19i32, 20i32)}
  [Reference</std/collections/map<i32, i32>>] s0{location(b0)}
  [Reference</std/collections/map<i32, i32>>] s1{location(b1)}
  [Reference</std/collections/map<i32, i32>>] s2{location(b2)}

  [/std/collections/map<i32, i32>] c0{map<i32, i32>(21i32, 22i32, 23i32, 24i32)}
  [/std/collections/map<i32, i32>] c1{map<i32, i32>(25i32, 26i32, 27i32, 28i32, 29i32, 30i32)}
  [Reference</std/collections/map<i32, i32>>] t0{location(c0)}
  [Reference</std/collections/map<i32, i32>>] t1{location(c1)}

  return(plus(score_refs(r0, r1, r2),
              plus(forward(s0, s1, s2),
                   forward_mixed(t0, t1))))
}
)";
  const std::string srcPath = writeTemp("compile_native_variadic_args_borrowed_map.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_variadic_args_borrowed_map").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 11);
}

TEST_CASE("native materializes variadic borrowed map packs with indexed dereference count methods") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
score_refs([args<Reference</std/collections/map<i32, i32>>>] values) {
  return(plus(dereference(at(values, 0i32)).count(),
              dereference(at(values, 2i32)).count()))
}

[return<int>]
forward([args<Reference</std/collections/map<i32, i32>>>] values) {
  return(score_refs([spread] values))
}

[return<int>]
forward_mixed([args<Reference</std/collections/map<i32, i32>>>] values) {
  [/std/collections/map<i32, i32>] extra{map<i32, i32>(1i32, 2i32)}
  [Reference</std/collections/map<i32, i32>>] extra_ref{location(extra)}
  return(score_refs(extra_ref, [spread] values))
}

[return<int>]
main() {
  [/std/collections/map<i32, i32>] a0{map<i32, i32>(1i32, 2i32, 3i32, 4i32)}
  [/std/collections/map<i32, i32>] a1{map<i32, i32>(5i32, 6i32)}
  [/std/collections/map<i32, i32>] a2{map<i32, i32>(7i32, 8i32, 9i32, 10i32, 11i32, 12i32)}
  [Reference</std/collections/map<i32, i32>>] r0{location(a0)}
  [Reference</std/collections/map<i32, i32>>] r1{location(a1)}
  [Reference</std/collections/map<i32, i32>>] r2{location(a2)}

  [/std/collections/map<i32, i32>] b0{map<i32, i32>(13i32, 14i32)}
  [/std/collections/map<i32, i32>] b1{map<i32, i32>(15i32, 16i32, 17i32, 18i32)}
  [/std/collections/map<i32, i32>] b2{map<i32, i32>(19i32, 20i32)}
  [Reference</std/collections/map<i32, i32>>] s0{location(b0)}
  [Reference</std/collections/map<i32, i32>>] s1{location(b1)}
  [Reference</std/collections/map<i32, i32>>] s2{location(b2)}

  [/std/collections/map<i32, i32>] c0{map<i32, i32>(21i32, 22i32, 23i32, 24i32)}
  [/std/collections/map<i32, i32>] c1{map<i32, i32>(25i32, 26i32, 27i32, 28i32, 29i32, 30i32)}
  [Reference</std/collections/map<i32, i32>>] t0{location(c0)}
  [Reference</std/collections/map<i32, i32>>] t1{location(c1)}

  return(plus(score_refs(r0, r1, r2),
              plus(forward(s0, s1, s2),
                   forward_mixed(t0, t1))))
}
)";
  const std::string srcPath = writeTemp("compile_native_variadic_args_borrowed_map_deref_count.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_variadic_args_borrowed_map_deref_count").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 11);
}

TEST_CASE("native materializes variadic borrowed map packs with indexed dereference lookup helpers") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
score_refs([args<Reference</std/collections/map<i32, i32>>>] values) {
  [auto] head{dereference(at(values, 0i32)).at_unsafe(3i32)}
  if(dereference(at(values, 2i32)).contains(11i32),
     then(){ return(plus(head, dereference(at(values, 2i32)).at(11i32))) },
     else(){ return(0i32) })
}

[return<int>]
forward([args<Reference</std/collections/map<i32, i32>>>] values) {
  return(score_refs([spread] values))
}

[return<int>]
forward_mixed([args<Reference</std/collections/map<i32, i32>>>] values) {
  [/std/collections/map<i32, i32>] extra{map<i32, i32>(19i32, 20i32, 3i32, 4i32)}
  [Reference</std/collections/map<i32, i32>>] extra_ref{location(extra)}
  return(score_refs(extra_ref, [spread] values))
}

[return<int>]
main() {
  [/std/collections/map<i32, i32>] a0{map<i32, i32>(1i32, 2i32, 3i32, 4i32)}
  [/std/collections/map<i32, i32>] a1{map<i32, i32>(5i32, 6i32)}
  [/std/collections/map<i32, i32>] a2{map<i32, i32>(7i32, 8i32, 9i32, 10i32, 11i32, 12i32)}
  [Reference</std/collections/map<i32, i32>>] r0{location(a0)}
  [Reference</std/collections/map<i32, i32>>] r1{location(a1)}
  [Reference</std/collections/map<i32, i32>>] r2{location(a2)}

  [/std/collections/map<i32, i32>] b0{map<i32, i32>(13i32, 14i32, 3i32, 4i32)}
  [/std/collections/map<i32, i32>] b1{map<i32, i32>(15i32, 16i32)}
  [/std/collections/map<i32, i32>] b2{map<i32, i32>(17i32, 18i32, 11i32, 12i32)}
  [Reference</std/collections/map<i32, i32>>] s0{location(b0)}
  [Reference</std/collections/map<i32, i32>>] s1{location(b1)}
  [Reference</std/collections/map<i32, i32>>] s2{location(b2)}

  [/std/collections/map<i32, i32>] c0{map<i32, i32>(21i32, 22i32)}
  [/std/collections/map<i32, i32>] c1{map<i32, i32>(23i32, 24i32, 11i32, 12i32)}
  [Reference</std/collections/map<i32, i32>>] t0{location(c0)}
  [Reference</std/collections/map<i32, i32>>] t1{location(c1)}

  return(plus(score_refs(r0, r1, r2),
              plus(forward(s0, s1, s2),
                   forward_mixed(t0, t1))))
}
)";
  const std::string srcPath = writeTemp("compile_native_variadic_args_borrowed_map_deref_lookup.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_variadic_args_borrowed_map_deref_lookup").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 48);
}

TEST_CASE("native rejects variadic borrowed map packs with indexed tryAt inference") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
score_refs([args<Reference</std/collections/map<i32, i32>>>] values) {
  [auto] head{/std/collections/map/at_unsafe<i32, i32>(at(values, 0i32), 3i32)}
  [auto] tailMissing{
      Result.error(/std/collections/map/tryAt<i32, i32>(at(values, minus(count(values), 1i32)), 99i32))}
  return(if(tailMissing,
            then(){ return(plus(head, /std/collections/map/at_unsafe<i32, i32>(at(values, minus(count(values), 1i32)), 11i32))) },
            else(){ return(0i32) }))
}

[return<int>]
forward([args<Reference</std/collections/map<i32, i32>>>] values) {
  return(score_refs([spread] values))
}

[return<int>]
forward_mixed([args<Reference</std/collections/map<i32, i32>>>] values) {
  [/std/collections/map<i32, i32>] extra{map<i32, i32>(1i32, 2i32, 3i32, 8i32)}
  [Reference</std/collections/map<i32, i32>>] extra_ref{location(extra)}
  return(score_refs(extra_ref, [spread] values))
}

[return<int>]
main() {
  [/std/collections/map<i32, i32>] a0{map<i32, i32>(1i32, 2i32, 3i32, 4i32)}
  [/std/collections/map<i32, i32>] a1{map<i32, i32>(5i32, 6i32)}
  [/std/collections/map<i32, i32>] a2{map<i32, i32>(7i32, 8i32, 9i32, 10i32, 11i32, 12i32)}
  [Reference</std/collections/map<i32, i32>>] r0{location(a0)}
  [Reference</std/collections/map<i32, i32>>] r1{location(a1)}
  [Reference</std/collections/map<i32, i32>>] r2{location(a2)}

  [/std/collections/map<i32, i32>] b0{map<i32, i32>(1i32, 2i32, 3i32, 6i32)}
  [/std/collections/map<i32, i32>] b1{map<i32, i32>(5i32, 6i32)}
  [/std/collections/map<i32, i32>] b2{map<i32, i32>(7i32, 8i32, 9i32, 10i32, 11i32, 14i32)}
  [Reference</std/collections/map<i32, i32>>] s0{location(b0)}
  [Reference</std/collections/map<i32, i32>>] s1{location(b1)}
  [Reference</std/collections/map<i32, i32>>] s2{location(b2)}

  [/std/collections/map<i32, i32>] c0{map<i32, i32>(7i32, 8i32, 9i32, 10i32, 11i32, 16i32)}
  [/std/collections/map<i32, i32>] c1{map<i32, i32>(11i32, 16i32, 13i32, 14i32)}
  [Reference</std/collections/map<i32, i32>>] t0{location(c0)}
  [Reference</std/collections/map<i32, i32>>] t1{location(c1)}

  return(plus(score_refs(r0, r1, r2),
              plus(forward(s0, s1, s2),
                   forward_mixed(t0, t1))))
}
)";
  const std::string srcPath = writeTemp("compile_native_variadic_args_borrowed_map_tryat.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_native_variadic_args_borrowed_map_tryat.err").string();

  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  const std::string diagnostics = readFile(errPath);
  CHECK(diagnostics.find(
            "native backend only supports arithmetic/comparison/clamp/min/max/abs/sign/saturate/convert/pointer/assign/increment/decrement calls in expressions") !=
        std::string::npos);
  CHECK(diagnostics.find("call=/std/collections/map/at_unsafe") != std::string::npos);
}

TEST_CASE("native materializes variadic pointer map packs with indexed count methods") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
score_ptrs([args<Pointer</std/collections/map<i32, i32>>>] values) {
  return(plus(values[0i32].count(), values[2i32].count()))
}

[return<int>]
forward([args<Pointer</std/collections/map<i32, i32>>>] values) {
  return(score_ptrs([spread] values))
}

[return<int>]
forward_mixed([args<Pointer</std/collections/map<i32, i32>>>] values) {
  [/std/collections/map<i32, i32>] extra{map<i32, i32>(1i32, 2i32)}
  [Pointer</std/collections/map<i32, i32>>] extra_ptr{location(extra)}
  return(score_ptrs(extra_ptr, [spread] values))
}

[return<int>]
main() {
  [/std/collections/map<i32, i32>] a0{map<i32, i32>(1i32, 2i32, 3i32, 4i32)}
  [/std/collections/map<i32, i32>] a1{map<i32, i32>(5i32, 6i32)}
  [/std/collections/map<i32, i32>] a2{map<i32, i32>(7i32, 8i32, 9i32, 10i32, 11i32, 12i32)}
  [Pointer</std/collections/map<i32, i32>>] r0{location(a0)}
  [Pointer</std/collections/map<i32, i32>>] r1{location(a1)}
  [Pointer</std/collections/map<i32, i32>>] r2{location(a2)}

  [/std/collections/map<i32, i32>] b0{map<i32, i32>(13i32, 14i32)}
  [/std/collections/map<i32, i32>] b1{map<i32, i32>(15i32, 16i32, 17i32, 18i32)}
  [/std/collections/map<i32, i32>] b2{map<i32, i32>(19i32, 20i32)}
  [Pointer</std/collections/map<i32, i32>>] s0{location(b0)}
  [Pointer</std/collections/map<i32, i32>>] s1{location(b1)}
  [Pointer</std/collections/map<i32, i32>>] s2{location(b2)}

  [/std/collections/map<i32, i32>] c0{map<i32, i32>(21i32, 22i32, 23i32, 24i32)}
  [/std/collections/map<i32, i32>] c1{map<i32, i32>(25i32, 26i32, 27i32, 28i32, 29i32, 30i32)}
  [Pointer</std/collections/map<i32, i32>>] t0{location(c0)}
  [Pointer</std/collections/map<i32, i32>>] t1{location(c1)}

  return(plus(score_ptrs(r0, r1, r2),
              plus(forward(s0, s1, s2),
                   forward_mixed(t0, t1))))
}
)";
  const std::string srcPath = writeTemp("compile_native_variadic_args_pointer_map.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_variadic_args_pointer_map").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 11);
}

TEST_CASE("native materializes variadic pointer map packs with indexed dereference count methods") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
score_ptrs([args<Pointer</std/collections/map<i32, i32>>>] values) {
  return(plus(dereference(at(values, 0i32)).count(),
              dereference(at(values, 2i32)).count()))
}

[return<int>]
forward([args<Pointer</std/collections/map<i32, i32>>>] values) {
  return(score_ptrs([spread] values))
}

[return<int>]
forward_mixed([args<Pointer</std/collections/map<i32, i32>>>] values) {
  [/std/collections/map<i32, i32>] extra{map<i32, i32>(1i32, 2i32)}
  [Pointer</std/collections/map<i32, i32>>] extra_ptr{location(extra)}
  return(score_ptrs(extra_ptr, [spread] values))
}

[return<int>]
main() {
  [/std/collections/map<i32, i32>] a0{map<i32, i32>(1i32, 2i32, 3i32, 4i32)}
  [/std/collections/map<i32, i32>] a1{map<i32, i32>(5i32, 6i32)}
  [/std/collections/map<i32, i32>] a2{map<i32, i32>(7i32, 8i32, 9i32, 10i32, 11i32, 12i32)}
  [Pointer</std/collections/map<i32, i32>>] r0{location(a0)}
  [Pointer</std/collections/map<i32, i32>>] r1{location(a1)}
  [Pointer</std/collections/map<i32, i32>>] r2{location(a2)}

  [/std/collections/map<i32, i32>] b0{map<i32, i32>(13i32, 14i32)}
  [/std/collections/map<i32, i32>] b1{map<i32, i32>(15i32, 16i32, 17i32, 18i32)}
  [/std/collections/map<i32, i32>] b2{map<i32, i32>(19i32, 20i32)}
  [Pointer</std/collections/map<i32, i32>>] s0{location(b0)}
  [Pointer</std/collections/map<i32, i32>>] s1{location(b1)}
  [Pointer</std/collections/map<i32, i32>>] s2{location(b2)}

  [/std/collections/map<i32, i32>] c0{map<i32, i32>(21i32, 22i32, 23i32, 24i32)}
  [/std/collections/map<i32, i32>] c1{map<i32, i32>(25i32, 26i32, 27i32, 28i32, 29i32, 30i32)}
  [Pointer</std/collections/map<i32, i32>>] t0{location(c0)}
  [Pointer</std/collections/map<i32, i32>>] t1{location(c1)}

  return(plus(score_ptrs(r0, r1, r2),
              plus(forward(s0, s1, s2),
                   forward_mixed(t0, t1))))
}
)";
  const std::string srcPath = writeTemp("compile_native_variadic_args_pointer_map_deref_count.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_variadic_args_pointer_map_deref_count").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 11);
}

TEST_CASE("native materializes variadic pointer map packs with indexed lookup helpers") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
score_ptrs([args<Pointer</std/collections/map<i32, i32>>>] values) {
  [auto] head{/std/collections/map/at_unsafe<i32, i32>(at(values, 0i32), 3i32)}
  if(at(values, 2i32).contains(11i32),
     then(){ return(plus(head, at(values, 2i32).at(11i32))) },
     else(){ return(0i32) })
}

[return<int>]
forward([args<Pointer</std/collections/map<i32, i32>>>] values) {
  return(score_ptrs([spread] values))
}

[return<int>]
forward_mixed([args<Pointer</std/collections/map<i32, i32>>>] values) {
  [/std/collections/map<i32, i32>] extra{map<i32, i32>(19i32, 20i32, 3i32, 4i32)}
  [Pointer</std/collections/map<i32, i32>>] extra_ptr{location(extra)}
  return(score_ptrs(extra_ptr, [spread] values))
}

[return<int>]
main() {
  [/std/collections/map<i32, i32>] a0{map<i32, i32>(1i32, 2i32, 3i32, 4i32)}
  [/std/collections/map<i32, i32>] a1{map<i32, i32>(5i32, 6i32)}
  [/std/collections/map<i32, i32>] a2{map<i32, i32>(7i32, 8i32, 9i32, 10i32, 11i32, 12i32)}
  [Pointer</std/collections/map<i32, i32>>] r0{location(a0)}
  [Pointer</std/collections/map<i32, i32>>] r1{location(a1)}
  [Pointer</std/collections/map<i32, i32>>] r2{location(a2)}

  [/std/collections/map<i32, i32>] b0{map<i32, i32>(13i32, 14i32, 3i32, 4i32)}
  [/std/collections/map<i32, i32>] b1{map<i32, i32>(15i32, 16i32)}
  [/std/collections/map<i32, i32>] b2{map<i32, i32>(17i32, 18i32, 11i32, 12i32)}
  [Pointer</std/collections/map<i32, i32>>] s0{location(b0)}
  [Pointer</std/collections/map<i32, i32>>] s1{location(b1)}
  [Pointer</std/collections/map<i32, i32>>] s2{location(b2)}

  [/std/collections/map<i32, i32>] c0{map<i32, i32>(21i32, 22i32)}
  [/std/collections/map<i32, i32>] c1{map<i32, i32>(23i32, 24i32, 11i32, 12i32)}
  [Pointer</std/collections/map<i32, i32>>] t0{location(c0)}
  [Pointer</std/collections/map<i32, i32>>] t1{location(c1)}

  return(plus(score_ptrs(r0, r1, r2),
              plus(forward(s0, s1, s2),
                   forward_mixed(t0, t1))))
}
)";
  const std::string srcPath = writeTemp("compile_native_variadic_args_pointer_map_lookup.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_variadic_args_pointer_map_lookup").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 48);
}

TEST_CASE("native materializes variadic pointer map packs with indexed dereference lookup helpers") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
score_ptrs([args<Pointer</std/collections/map<i32, i32>>>] values) {
  [auto] head{dereference(at(values, 0i32)).at_unsafe(3i32)}
  if(dereference(at(values, 2i32)).contains(11i32),
     then(){ return(plus(head, dereference(at(values, 2i32)).at(11i32))) },
     else(){ return(0i32) })
}

[return<int>]
forward([args<Pointer</std/collections/map<i32, i32>>>] values) {
  return(score_ptrs([spread] values))
}

[return<int>]
forward_mixed([args<Pointer</std/collections/map<i32, i32>>>] values) {
  [/std/collections/map<i32, i32>] extra{map<i32, i32>(19i32, 20i32, 3i32, 4i32)}
  [Pointer</std/collections/map<i32, i32>>] extra_ptr{location(extra)}
  return(score_ptrs(extra_ptr, [spread] values))
}

[return<int>]
main() {
  [/std/collections/map<i32, i32>] a0{map<i32, i32>(1i32, 2i32, 3i32, 4i32)}
  [/std/collections/map<i32, i32>] a1{map<i32, i32>(5i32, 6i32)}
  [/std/collections/map<i32, i32>] a2{map<i32, i32>(7i32, 8i32, 9i32, 10i32, 11i32, 12i32)}
  [Pointer</std/collections/map<i32, i32>>] r0{location(a0)}
  [Pointer</std/collections/map<i32, i32>>] r1{location(a1)}
  [Pointer</std/collections/map<i32, i32>>] r2{location(a2)}

  [/std/collections/map<i32, i32>] b0{map<i32, i32>(13i32, 14i32, 3i32, 4i32)}
  [/std/collections/map<i32, i32>] b1{map<i32, i32>(15i32, 16i32)}
  [/std/collections/map<i32, i32>] b2{map<i32, i32>(17i32, 18i32, 11i32, 12i32)}
  [Pointer</std/collections/map<i32, i32>>] s0{location(b0)}
  [Pointer</std/collections/map<i32, i32>>] s1{location(b1)}
  [Pointer</std/collections/map<i32, i32>>] s2{location(b2)}

  [/std/collections/map<i32, i32>] c0{map<i32, i32>(21i32, 22i32)}
  [/std/collections/map<i32, i32>] c1{map<i32, i32>(23i32, 24i32, 11i32, 12i32)}
  [Pointer</std/collections/map<i32, i32>>] t0{location(c0)}
  [Pointer</std/collections/map<i32, i32>>] t1{location(c1)}

  return(plus(score_ptrs(r0, r1, r2),
              plus(forward(s0, s1, s2),
                   forward_mixed(t0, t1))))
}
)";
  const std::string srcPath = writeTemp("compile_native_variadic_args_pointer_map_deref_lookup.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_variadic_args_pointer_map_deref_lookup").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 48);
}

TEST_CASE("native rejects variadic pointer map packs with indexed tryAt inference") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
score_ptrs([args<Pointer</std/collections/map<i32, i32>>>] values) {
  [auto] head{/std/collections/map/at_unsafe<i32, i32>(at(values, 0i32), 3i32)}
  [auto] tailMissing{
      Result.error(/std/collections/map/tryAt<i32, i32>(at(values, minus(count(values), 1i32)), 99i32))}
  return(if(tailMissing,
            then(){ return(plus(head, /std/collections/map/at_unsafe<i32, i32>(at(values, minus(count(values), 1i32)), 11i32))) },
            else(){ return(0i32) }))
}

[return<int>]
forward([args<Pointer</std/collections/map<i32, i32>>>] values) {
  return(score_ptrs([spread] values))
}

[return<int>]
forward_mixed([args<Pointer</std/collections/map<i32, i32>>>] values) {
  [/std/collections/map<i32, i32>] extra{map<i32, i32>(1i32, 2i32, 3i32, 8i32)}
  [Pointer</std/collections/map<i32, i32>>] extra_ptr{location(extra)}
  return(score_ptrs(extra_ptr, [spread] values))
}

[return<int>]
main() {
  [/std/collections/map<i32, i32>] a0{map<i32, i32>(1i32, 2i32, 3i32, 4i32)}
  [/std/collections/map<i32, i32>] a1{map<i32, i32>(5i32, 6i32)}
  [/std/collections/map<i32, i32>] a2{map<i32, i32>(7i32, 8i32, 9i32, 10i32, 11i32, 12i32)}
  [Pointer</std/collections/map<i32, i32>>] r0{location(a0)}
  [Pointer</std/collections/map<i32, i32>>] r1{location(a1)}
  [Pointer</std/collections/map<i32, i32>>] r2{location(a2)}

  [/std/collections/map<i32, i32>] b0{map<i32, i32>(1i32, 2i32, 3i32, 6i32)}
  [/std/collections/map<i32, i32>] b1{map<i32, i32>(5i32, 6i32)}
  [/std/collections/map<i32, i32>] b2{map<i32, i32>(7i32, 8i32, 9i32, 10i32, 11i32, 14i32)}
  [Pointer</std/collections/map<i32, i32>>] s0{location(b0)}
  [Pointer</std/collections/map<i32, i32>>] s1{location(b1)}
  [Pointer</std/collections/map<i32, i32>>] s2{location(b2)}

  [/std/collections/map<i32, i32>] c0{map<i32, i32>(7i32, 8i32, 9i32, 10i32, 11i32, 16i32)}
  [/std/collections/map<i32, i32>] c1{map<i32, i32>(11i32, 16i32, 13i32, 14i32)}
  [Pointer</std/collections/map<i32, i32>>] t0{location(c0)}
  [Pointer</std/collections/map<i32, i32>>] t1{location(c1)}

  return(plus(score_ptrs(r0, r1, r2),
              plus(forward(s0, s1, s2),
                   forward_mixed(t0, t1))))
}
)";
  const std::string srcPath = writeTemp("compile_native_variadic_args_pointer_map_tryat.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_native_variadic_args_pointer_map_tryat.err").string();

  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  const std::string diagnostics = readFile(errPath);
  CHECK(diagnostics.find(
            "native backend only supports arithmetic/comparison/clamp/min/max/abs/sign/saturate/convert/pointer/assign/increment/decrement calls in expressions") !=
        std::string::npos);
  CHECK(diagnostics.find("call=/std/collections/map/at_unsafe") != std::string::npos);
}

TEST_CASE("native materializes variadic pointer vector packs with indexed count methods") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
score_ptrs([args<Pointer<vector<i32>>>] values) {
  [vector<i32>] head{dereference(at(values, 0i32))}
  [vector<i32>] tail{dereference(at(values, 2i32))}
  return(plus(/std/collections/vector/count(head),
              /std/collections/vector/count(tail)))
}

[return<int>]
forward([args<Pointer<vector<i32>>>] values) {
  return(score_ptrs([spread] values))
}

[effects(heap_alloc), return<int>]
forward_mixed([args<Pointer<vector<i32>>>] values) {
  [vector<i32>] extra{vectorSingle<i32>(1i32)}
  [Pointer<vector<i32>>] extra_ptr{location(extra)}
  return(score_ptrs(extra_ptr, [spread] values))
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] a0{vector<i32>(1i32, 2i32)}
  [vector<i32>] a1{vectorSingle<i32>(3i32)}
  [vector<i32>] a2{vector<i32>(4i32, 5i32, 6i32, 7i32)}
  [Pointer<vector<i32>>] r0{location(a0)}
  [Pointer<vector<i32>>] r1{location(a1)}
  [Pointer<vector<i32>>] r2{location(a2)}

  [vector<i32>] b0{vectorSingle<i32>(8i32)}
  [vector<i32>] b1{vector<i32>(9i32, 10i32)}
  [vector<i32>] b2{vector<i32>(11i32, 12i32, 13i32)}
  [Pointer<vector<i32>>] s0{location(b0)}
  [Pointer<vector<i32>>] s1{location(b1)}
  [Pointer<vector<i32>>] s2{location(b2)}

  [vector<i32>] c0{vector<i32>(14i32, 15i32)}
  [vector<i32>] c1{vector<i32>(16i32, 17i32, 18i32, 19i32, 20i32)}
  [Pointer<vector<i32>>] t0{location(c0)}
  [Pointer<vector<i32>>] t1{location(c1)}

  return(plus(score_ptrs(r0, r1, r2),
              plus(forward(s0, s1, s2),
                   forward_mixed(t0, t1))))
}
)";
  const std::string srcPath = writeTemp("compile_native_variadic_args_pointer_vector.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_variadic_args_pointer_vector").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 16);
}

TEST_CASE("native materializes variadic pointer vector packs with indexed dereference capacity methods") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
score_ptrs([args<Pointer<vector<i32>>>] values) {
  [vector<i32>] head{dereference(at(values, 0i32))}
  [vector<i32>] tail{dereference(at(values, 2i32))}
  return(plus(/std/collections/vector/capacity(head),
              /std/collections/vector/capacity(tail)))
}

[return<int>]
forward([args<Pointer<vector<i32>>>] values) {
  return(score_ptrs([spread] values))
}

[effects(heap_alloc), return<int>]
forward_mixed([args<Pointer<vector<i32>>>] values) {
  [vector<i32>] extra{vectorSingle<i32>(1i32)}
  [Pointer<vector<i32>>] extra_ptr{location(extra)}
  return(score_ptrs(extra_ptr, [spread] values))
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] a0{vector<i32>(1i32, 2i32)}
  [vector<i32>] a1{vectorSingle<i32>(3i32)}
  [vector<i32>] a2{vector<i32>(4i32, 5i32, 6i32, 7i32)}
  [Pointer<vector<i32>>] r0{location(a0)}
  [Pointer<vector<i32>>] r1{location(a1)}
  [Pointer<vector<i32>>] r2{location(a2)}

  [vector<i32>] b0{vectorSingle<i32>(8i32)}
  [vector<i32>] b1{vector<i32>(9i32, 10i32)}
  [vector<i32>] b2{vector<i32>(11i32, 12i32, 13i32)}
  [Pointer<vector<i32>>] s0{location(b0)}
  [Pointer<vector<i32>>] s1{location(b1)}
  [Pointer<vector<i32>>] s2{location(b2)}

  [vector<i32>] c0{vector<i32>(14i32, 15i32)}
  [vector<i32>] c1{vector<i32>(16i32, 17i32, 18i32, 19i32, 20i32)}
  [Pointer<vector<i32>>] t0{location(c0)}
  [Pointer<vector<i32>>] t1{location(c1)}

  return(plus(score_ptrs(r0, r1, r2),
              plus(forward(s0, s1, s2),
                   forward_mixed(t0, t1))))
}
)";
  const std::string srcPath = writeTemp("compile_native_variadic_args_pointer_vector_deref_capacity.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_variadic_args_pointer_vector_deref_capacity").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 16);
}

TEST_CASE("native keeps vector constructor and literal parity across direct and canonical forms") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] literal{vector<i32>{1i32, 2i32, 3i32}}
  [vector<i32>] directCtor{vector<i32>(4i32, 5i32, 6i32)}
  [vector<i32>] canonicalCtor{/std/collections/vector/vector<i32>(7i32, 8i32, 9i32)}
  [vector<i32>] canonicalEmpty{/std/collections/vector/vector<i32>()}
  return(
      plus(
          plus(/std/collections/vector/count<i32>(literal),
               /std/collections/vector/count<i32>(directCtor)),
          plus(
              plus(/std/collections/vector/count<i32>(canonicalCtor),
                   /std/collections/vector/count<i32>(canonicalEmpty)),
              plus(
                  plus(/std/collections/vector/at_unsafe<i32>(literal, 2i32),
                       /std/collections/vector/at_unsafe<i32>(directCtor, 1i32)),
                  /std/collections/vector/at_unsafe<i32>(canonicalCtor, 0i32)))))
}
)";
  const std::string srcPath = writeTemp("compile_native_vector_ctor_literal_parity.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_vector_ctor_literal_parity").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 24);
}

TEST_CASE("native keeps map constructor and literal parity across direct and canonical forms") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] literal{map<i32, i32>{1i32=11i32, 2i32=13i32}}
  [map<i32, i32>] directCtor{map<i32, i32>(3i32, 17i32, 4i32, 19i32)}
  [map<i32, i32>] canonicalCtor{/std/collections/map/map<i32, i32>(
      /std/collections/map/entry<i32, i32>(5i32, 23i32),
      /std/collections/map/entry<i32, i32>(6i32, 29i32))}
  [map<i32, i32>] canonicalSingle{/std/collections/map/map<i32, i32>(
      /std/collections/map/entry<i32, i32>(7i32, 31i32))}
  return(
      plus(
          plus(/std/collections/map/count<i32, i32>(literal),
               /std/collections/map/count<i32, i32>(directCtor)),
          plus(
              plus(/std/collections/map/count<i32, i32>(canonicalCtor),
                   /std/collections/map/count<i32, i32>(canonicalSingle)),
              plus(
                  /std/collections/map/at_unsafe<i32, i32>(literal, 1i32),
                  plus(
                      /std/collections/map/at_unsafe<i32, i32>(directCtor, 4i32),
                      plus(
                          /std/collections/map/at_unsafe<i32, i32>(canonicalCtor, 6i32),
                          /std/collections/map/at_unsafe<i32, i32>(canonicalSingle, 7i32)))))))
}
)";
  const std::string srcPath = writeTemp("compile_native_map_ctor_literal_parity.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_map_ctor_literal_parity").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 97);
}

TEST_SUITE_END();
#endif
