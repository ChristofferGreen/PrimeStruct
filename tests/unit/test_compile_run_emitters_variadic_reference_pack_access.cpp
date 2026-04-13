#include "test_compile_run_helpers.h"
#include "test_compile_run_collection_conformance_helpers.h"
#include "test_compile_run_emitters_helpers.h"

#include "primec/testing/EmitterHelpers.h"

TEST_SUITE_BEGIN("primestruct.compile.run.emitters.cpp");

TEST_CASE("C++ emitter materializes variadic struct reference packs from borrowed pack reference fields") {
  const std::string source = R"(
[struct]
Pair() {
  [i32] value{0i32}
}

[return<int>]
/Pair/score([Pair] self) {
  return(plus(self.value, 1i32))
}

[struct]
Holder() {
  [mut Pair] pair_storage{Pair()}
  [Reference<Pair>] pair_ref{location(pair_storage)}
}

[return<int>]
score_refs([args<Reference<Pair>>] values) {
  return(plus(values[0i32].value, values[2i32].score()))
}

[return<int>]
score_from_fields([args<Reference<Holder>>] values) {
  return(score_refs(location(values.at(0i32).pair_ref),
                    location(at(values, 1i32).pair_ref),
                    location(values.at_unsafe(2i32).pair_ref)))
}

[return<int>]
forward([args<Reference<Holder>>] values) {
  return(score_from_fields([spread] values))
}

[return<int>]
forward_mixed([args<Reference<Holder>>] values) {
  [Pair] extra_value{Pair(5i32)}
  [Holder] extra
  assign(extra.pair_storage, extra_value)
  return(score_refs(location(extra.pair_ref),
                    location(values.at(0i32).pair_ref),
                    location(at(values, 1i32).pair_ref)))
}

[return<int>]
main() {
  [Pair] a0{Pair(7i32)}
  [Pair] a1{Pair(8i32)}
  [Pair] a2{Pair(9i32)}
  [Holder] h0
  [Holder] h1
  [Holder] h2
  assign(h0.pair_storage, a0)
  assign(h1.pair_storage, a1)
  assign(h2.pair_storage, a2)
  [Reference<Holder>] r0{location(h0)}
  [Reference<Holder>] r1{location(h1)}
  [Reference<Holder>] r2{location(h2)}

  [Pair] b0{Pair(11i32)}
  [Pair] b1{Pair(12i32)}
  [Pair] b2{Pair(13i32)}
  [Holder] i0
  [Holder] i1
  [Holder] i2
  assign(i0.pair_storage, b0)
  assign(i1.pair_storage, b1)
  assign(i2.pair_storage, b2)
  [Reference<Holder>] s0{location(i0)}
  [Reference<Holder>] s1{location(i1)}
  [Reference<Holder>] s2{location(i2)}

  [Pair] c0{Pair(15i32)}
  [Pair] c1{Pair(17i32)}
  [Holder] j0
  [Holder] j1
  assign(j0.pair_storage, c0)
  assign(j1.pair_storage, c1)
  [Reference<Holder>] t0{location(j0)}
  [Reference<Holder>] t1{location(j1)}

  return(plus(score_from_fields(r0, r1, r2),
              plus(forward(s0, s1, s2),
                   forward_mixed(t0, t1))))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_variadic_args_struct_reference_pack_reference_field.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() /
       "primec_cpp_variadic_args_struct_reference_pack_reference_field_exe")
          .string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 65);
}

TEST_CASE("C++ emitter materializes variadic pointer uninitialized scalar packs with indexed init and take") {
  const std::string source = R"(
[return<Reference<uninitialized<i32>>>]
borrow_ref([Reference<uninitialized<i32>>] value) {
  return(value)
}

[return<int>]
score_ptrs([args<Pointer<uninitialized<i32>>>] values) {
  init(dereference(values[0i32]), 2i32)
  init(dereference(values.at(1i32)), 3i32)
  init(dereference(values.at_unsafe(2i32)), 4i32)
  return(plus(take(dereference(values[0i32])),
              plus(take(dereference(values.at(1i32))),
                   take(dereference(values.at_unsafe(2i32))))))
}

[return<int>]
forward([args<Pointer<uninitialized<i32>>>] values) {
  return(score_ptrs([spread] values))
}

[return<int>]
forward_mixed([args<Pointer<uninitialized<i32>>>] values) {
  [uninitialized<i32>] extra{uninitialized<i32>()}
  [Reference<uninitialized<i32>>] extra_ref{location(extra)}
  return(score_ptrs(location(borrow_ref(extra_ref)), [spread] values))
}

[return<int>]
main() {
  [uninitialized<i32>] a0{uninitialized<i32>()}
  [uninitialized<i32>] a1{uninitialized<i32>()}
  [uninitialized<i32>] a2{uninitialized<i32>()}
  [Reference<uninitialized<i32>>] p0{location(a0)}
  [Reference<uninitialized<i32>>] p1{location(a1)}
  [Reference<uninitialized<i32>>] p2{location(a2)}

  [uninitialized<i32>] b0{uninitialized<i32>()}
  [uninitialized<i32>] b1{uninitialized<i32>()}
  [uninitialized<i32>] b2{uninitialized<i32>()}
  [Reference<uninitialized<i32>>] q0{location(b0)}
  [Reference<uninitialized<i32>>] q1{location(b1)}
  [Reference<uninitialized<i32>>] q2{location(b2)}

  [uninitialized<i32>] c0{uninitialized<i32>()}
  [uninitialized<i32>] c1{uninitialized<i32>()}
  [Reference<uninitialized<i32>>] r0{location(c0)}
  [Reference<uninitialized<i32>>] r1{location(c1)}

  return(plus(score_ptrs(location(borrow_ref(p0)), location(borrow_ref(p1)), location(borrow_ref(p2))),
              plus(forward(location(borrow_ref(q0)), location(borrow_ref(q1)), location(borrow_ref(q2))),
                   forward_mixed(location(borrow_ref(r0)), location(borrow_ref(r1))))))
}
)";
  const std::string srcPath = writeTemp("compile_cpp_variadic_args_pointer_uninitialized_scalar.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_cpp_variadic_args_pointer_uninitialized_scalar_exe")
          .string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 27);
}

TEST_CASE("C++ emitter materializes variadic pointer uninitialized struct packs from borrowed helper references") {
  const std::string source = R"(
[struct]
Pair() {
  [i32] left{0i32}
  [i32] right{0i32}
}

[return<Reference<uninitialized<Pair>>>]
borrow_ref([Reference<uninitialized<Pair>>] value) {
  return(value)
}

[return<int>]
score_ptrs([args<Pointer<uninitialized<Pair>>>] values) {
  init(dereference(values[0i32]), Pair(1i32, 2i32))
  init(dereference(values.at(1i32)), Pair(3i32, 4i32))
  init(dereference(values.at_unsafe(2i32)), Pair(5i32, 6i32))
  [Pair] first{take(dereference(values[0i32]))}
  [Pair] second{take(dereference(values.at(1i32)))}
  [Pair] third{take(dereference(values.at_unsafe(2i32)))}
  return(plus(first.left, plus(second.right, third.left)))
}

[return<int>]
forward([args<Pointer<uninitialized<Pair>>>] values) {
  return(score_ptrs([spread] values))
}

[return<int>]
forward_mixed([args<Pointer<uninitialized<Pair>>>] values) {
  [uninitialized<Pair>] extra{uninitialized<Pair>()}
  [Reference<uninitialized<Pair>>] extra_ref{location(extra)}
  return(score_ptrs(location(borrow_ref(extra_ref)), [spread] values))
}

[return<int>]
main() {
  [uninitialized<Pair>] a0{uninitialized<Pair>()}
  [uninitialized<Pair>] a1{uninitialized<Pair>()}
  [uninitialized<Pair>] a2{uninitialized<Pair>()}
  [Reference<uninitialized<Pair>>] p0{location(a0)}
  [Reference<uninitialized<Pair>>] p1{location(a1)}
  [Reference<uninitialized<Pair>>] p2{location(a2)}

  [uninitialized<Pair>] b0{uninitialized<Pair>()}
  [uninitialized<Pair>] b1{uninitialized<Pair>()}
  [uninitialized<Pair>] b2{uninitialized<Pair>()}
  [Reference<uninitialized<Pair>>] q0{location(b0)}
  [Reference<uninitialized<Pair>>] q1{location(b1)}
  [Reference<uninitialized<Pair>>] q2{location(b2)}

  [uninitialized<Pair>] c0{uninitialized<Pair>()}
  [uninitialized<Pair>] c1{uninitialized<Pair>()}
  [Reference<uninitialized<Pair>>] r0{location(c0)}
  [Reference<uninitialized<Pair>>] r1{location(c1)}

  return(plus(score_ptrs(location(borrow_ref(p0)), location(borrow_ref(p1)), location(borrow_ref(p2))),
              plus(forward(location(borrow_ref(q0)), location(borrow_ref(q1)), location(borrow_ref(q2))),
                   forward_mixed(location(borrow_ref(r0)), location(borrow_ref(r1))))))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_variadic_args_pointer_uninitialized_struct_helper_ref.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() /
       "primec_cpp_variadic_args_pointer_uninitialized_struct_helper_ref_exe")
          .string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 30);
}

TEST_CASE("C++ emitter materializes variadic borrowed uninitialized scalar packs with indexed init and take") {
  const std::string source = R"(
[return<int>]
score_refs([args<Reference<uninitialized<i32>>>] values) {
  init(dereference(values[0i32]), 2i32)
  init(dereference(values.at(1i32)), 3i32)
  init(dereference(values.at_unsafe(2i32)), 4i32)
  return(plus(take(dereference(values[0i32])),
              plus(take(dereference(values.at(1i32))),
                   take(dereference(values.at_unsafe(2i32))))))
}

[return<int>]
forward([args<Reference<uninitialized<i32>>>] values) {
  return(score_refs([spread] values))
}

[return<int>]
forward_mixed([args<Reference<uninitialized<i32>>>] values) {
  [uninitialized<i32>] extra{uninitialized<i32>()}
  return(score_refs(location(extra), [spread] values))
}

[return<int>]
main() {
  [uninitialized<i32>] a0{uninitialized<i32>()}
  [uninitialized<i32>] a1{uninitialized<i32>()}
  [uninitialized<i32>] a2{uninitialized<i32>()}

  [uninitialized<i32>] b0{uninitialized<i32>()}
  [uninitialized<i32>] b1{uninitialized<i32>()}
  [uninitialized<i32>] b2{uninitialized<i32>()}

  [uninitialized<i32>] c0{uninitialized<i32>()}
  [uninitialized<i32>] c1{uninitialized<i32>()}

  return(plus(score_refs(location(a0), location(a1), location(a2)),
              plus(forward(location(b0), location(b1), location(b2)),
                   forward_mixed(location(c0), location(c1)))))
}
)";
  const std::string srcPath = writeTemp("compile_cpp_variadic_args_reference_uninitialized_scalar.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() / "primec_cpp_variadic_args_reference_uninitialized_scalar_exe")
          .string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 27);
}

TEST_CASE("C++ emitter materializes variadic borrowed uninitialized struct packs with indexed init and take") {
  const std::string source = R"(
[struct]
Pair() {
  [i32] left{0i32}
  [i32] right{0i32}
}

[return<int>]
score_refs([args<Reference<uninitialized<Pair>>>] values) {
  init(dereference(values[0i32]), Pair(1i32, 2i32))
  init(dereference(values.at(1i32)), Pair(3i32, 4i32))
  init(dereference(values.at_unsafe(2i32)), Pair(5i32, 6i32))
  [Pair] first{take(dereference(values[0i32]))}
  [Pair] second{take(dereference(values.at(1i32)))}
  [Pair] third{take(dereference(values.at_unsafe(2i32)))}
  return(plus(first.left, plus(second.right, third.left)))
}

[return<int>]
forward([args<Reference<uninitialized<Pair>>>] values) {
  return(score_refs([spread] values))
}

[return<int>]
forward_mixed([args<Reference<uninitialized<Pair>>>] values) {
  [uninitialized<Pair>] extra{uninitialized<Pair>()}
  return(score_refs(location(extra), [spread] values))
}

[return<int>]
main() {
  [uninitialized<Pair>] a0{uninitialized<Pair>()}
  [uninitialized<Pair>] a1{uninitialized<Pair>()}
  [uninitialized<Pair>] a2{uninitialized<Pair>()}

  [uninitialized<Pair>] b0{uninitialized<Pair>()}
  [uninitialized<Pair>] b1{uninitialized<Pair>()}
  [uninitialized<Pair>] b2{uninitialized<Pair>()}

  [uninitialized<Pair>] c0{uninitialized<Pair>()}
  [uninitialized<Pair>] c1{uninitialized<Pair>()}

  return(plus(score_refs(location(a0), location(a1), location(a2)),
              plus(forward(location(b0), location(b1), location(b2)),
                   forward_mixed(location(c0), location(c1)))))
}
)";
  const std::string srcPath = writeTemp("compile_cpp_variadic_args_reference_uninitialized_struct.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() / "primec_cpp_variadic_args_reference_uninitialized_struct_exe")
          .string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 30);
}


TEST_SUITE_END();
