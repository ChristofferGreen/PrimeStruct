#include "test_compile_run_helpers.h"
#include "test_compile_run_collection_conformance_helpers.h"
#include "test_compile_run_emitters_helpers.h"

#include "primec/testing/EmitterHelpers.h"

TEST_SUITE_BEGIN("primestruct.compile.run.emitters.cpp");

TEST_CASE("C++ emitter materializes variadic scalar pointer packs from borrowed pack access") {
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
  const std::string srcPath = writeTemp("compile_cpp_variadic_args_scalar_pointer_pack_access.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() / "primec_cpp_variadic_args_scalar_pointer_pack_access_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 29);
}

TEST_CASE("C++ emitter materializes variadic struct pointer packs from borrowed pack access") {
  const std::string source = R"(
[struct]
Pair() {
  [i32] value{0i32}
}

[return<int>]
/Pair/score([Pair] self) {
  return(plus(self.value, 1i32))
}

[return<int>]
score_ptrs([args<Pointer<Pair>>] values) {
  return(plus(values[0i32].value, values[2i32].score()))
}

[return<int>]
score_from_at([args<Reference<Pair>>] values) {
  return(score_ptrs(location(values.at(0i32)),
                    location(at(values, 1i32)),
                    location(values.at_unsafe(2i32))))
}

[return<int>]
forward([args<Reference<Pair>>] values) {
  return(score_from_at([spread] values))
}

[return<int>]
forward_mixed([args<Reference<Pair>>] values) {
  [Pair] extra{Pair(5i32)}
  [Reference<Pair>] extra_ref{location(extra)}
  return(score_ptrs(location(values.at(0i32)),
                    location(extra_ref),
                    location(at(values, 1i32))))
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

  return(plus(score_from_at(r0, r1, r2),
              plus(forward(s0, s1, s2),
                   forward_mixed(t0, t1))))
}
)";
  const std::string srcPath = writeTemp("compile_cpp_variadic_args_struct_pointer_pack_access.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() / "primec_cpp_variadic_args_struct_pointer_pack_access_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 75);
}

TEST_CASE("C++ emitter materializes variadic scalar pointer packs from borrowed pack field access") {
  const std::string source = R"(
[struct]
Holder() {
  [i32] value{0i32}
}

[return<int>]
score_ptrs([args<Pointer<i32>>] values) {
  return(plus(dereference(values[0i32]), dereference(values[2i32])))
}

[return<int>]
score_from_fields([args<Reference<Holder>>] values) {
  return(score_ptrs(location(at(values, 0i32).value),
                    location(values.at(1i32).value),
                    location(values.at_unsafe(2i32).value)))
}

[return<int>]
forward([args<Reference<Holder>>] values) {
  return(score_from_fields([spread] values))
}

[return<int>]
forward_mixed([args<Reference<Holder>>] values) {
  [Holder] extra{Holder(1i32)}
  return(score_ptrs(location(values.at(0i32).value),
                    location(extra.value),
                    location(at(values, 1i32).value)))
}

[return<int>]
main() {
  [Holder] a0{Holder(1i32)}
  [Holder] a1{Holder(2i32)}
  [Holder] a2{Holder(3i32)}
  [Reference<Holder>] r0{location(a0)}
  [Reference<Holder>] r1{location(a1)}
  [Reference<Holder>] r2{location(a2)}

  [Holder] b0{Holder(4i32)}
  [Holder] b1{Holder(5i32)}
  [Holder] b2{Holder(6i32)}
  [Reference<Holder>] s0{location(b0)}
  [Reference<Holder>] s1{location(b1)}
  [Reference<Holder>] s2{location(b2)}

  [Holder] c0{Holder(7i32)}
  [Holder] c1{Holder(8i32)}
  [Reference<Holder>] t0{location(c0)}
  [Reference<Holder>] t1{location(c1)}

  return(plus(score_from_fields(r0, r1, r2),
              plus(forward(s0, s1, s2),
                   forward_mixed(t0, t1))))
}
)";
  const std::string srcPath = writeTemp("compile_cpp_variadic_args_scalar_pointer_pack_field_access.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() / "primec_cpp_variadic_args_scalar_pointer_pack_field_access_exe")
          .string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 29);
}

TEST_CASE("C++ emitter materializes variadic struct pointer packs from borrowed pack field access") {
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
  [Pair] pair{Pair()}
}

[return<int>]
score_ptrs([args<Pointer<Pair>>] values) {
  return(plus(values[0i32].value, values[2i32].score()))
}

[return<int>]
score_from_fields([args<Reference<Holder>>] values) {
  return(score_ptrs(location(values.at(0i32).pair),
                    location(at(values, 1i32).pair),
                    location(values.at_unsafe(2i32).pair)))
}

[return<int>]
forward([args<Reference<Holder>>] values) {
  return(score_from_fields([spread] values))
}

[return<int>]
forward_mixed([args<Reference<Holder>>] values) {
  [Holder] extra{Holder(Pair(5i32))}
  return(score_ptrs(location(values.at(0i32).pair),
                    location(extra.pair),
                    location(at(values, 1i32).pair)))
}

[return<int>]
main() {
  [Holder] a0{Holder(Pair(7i32))}
  [Holder] a1{Holder(Pair(8i32))}
  [Holder] a2{Holder(Pair(9i32))}
  [Reference<Holder>] r0{location(a0)}
  [Reference<Holder>] r1{location(a1)}
  [Reference<Holder>] r2{location(a2)}

  [Holder] b0{Holder(Pair(11i32))}
  [Holder] b1{Holder(Pair(12i32))}
  [Holder] b2{Holder(Pair(13i32))}
  [Reference<Holder>] s0{location(b0)}
  [Reference<Holder>] s1{location(b1)}
  [Reference<Holder>] s2{location(b2)}

  [Holder] c0{Holder(Pair(15i32))}
  [Holder] c1{Holder(Pair(17i32))}
  [Reference<Holder>] t0{location(c0)}
  [Reference<Holder>] t1{location(c1)}

  return(plus(score_from_fields(r0, r1, r2),
              plus(forward(s0, s1, s2),
                   forward_mixed(t0, t1))))
}
)";
  const std::string srcPath = writeTemp("compile_cpp_variadic_args_struct_pointer_pack_field_access.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() / "primec_cpp_variadic_args_struct_pointer_pack_field_access_exe")
          .string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 75);
}

TEST_CASE("C++ emitter materializes variadic scalar pointer packs from borrowed pack reference fields") {
  const std::string source = R"(
[struct]
Holder() {
  [mut i32] value_storage{0i32}
  [Reference<i32>] value_ref{location(value_storage)}
}

[return<int>]
score_ptrs([args<Pointer<i32>>] values) {
  return(plus(dereference(values[0i32]), dereference(values[2i32])))
}

[return<int>]
score_from_fields([args<Reference<Holder>>] values) {
  return(score_ptrs(location(at(values, 0i32).value_ref),
                    location(values.at(1i32).value_ref),
                    location(values.at_unsafe(2i32).value_ref)))
}

[return<int>]
forward([args<Reference<Holder>>] values) {
  return(score_from_fields([spread] values))
}

[return<int>]
forward_mixed([args<Reference<Holder>>] values) {
  [i32] extra_value{1i32}
  [Holder] extra
  assign(extra.value_storage, extra_value)
  return(score_ptrs(location(values.at(0i32).value_ref),
                    location(extra.value_ref),
                    location(at(values, 1i32).value_ref)))
}

[return<int>]
main() {
  [i32] a0{1i32}
  [i32] a1{2i32}
  [i32] a2{3i32}
  [Holder] h0
  [Holder] h1
  [Holder] h2
  assign(h0.value_storage, a0)
  assign(h1.value_storage, a1)
  assign(h2.value_storage, a2)
  [Reference<Holder>] r0{location(h0)}
  [Reference<Holder>] r1{location(h1)}
  [Reference<Holder>] r2{location(h2)}

  [i32] b0{4i32}
  [i32] b1{5i32}
  [i32] b2{6i32}
  [Holder] i0
  [Holder] i1
  [Holder] i2
  assign(i0.value_storage, b0)
  assign(i1.value_storage, b1)
  assign(i2.value_storage, b2)
  [Reference<Holder>] s0{location(i0)}
  [Reference<Holder>] s1{location(i1)}
  [Reference<Holder>] s2{location(i2)}

  [i32] c0{7i32}
  [i32] c1{8i32}
  [Holder] j0
  [Holder] j1
  assign(j0.value_storage, c0)
  assign(j1.value_storage, c1)
  [Reference<Holder>] t0{location(j0)}
  [Reference<Holder>] t1{location(j1)}

  return(plus(score_from_fields(r0, r1, r2),
              plus(forward(s0, s1, s2),
                   forward_mixed(t0, t1))))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_variadic_args_scalar_pointer_pack_reference_field.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() /
       "primec_cpp_variadic_args_scalar_pointer_pack_reference_field_exe")
          .string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 29);
}

TEST_CASE("C++ emitter materializes variadic scalar pointer packs from indexed dereference receiver reference fields") {
  const std::string source = R"(
[struct]
Holder() {
  [mut i32] value_storage{0i32}
  [Reference<i32>] value_ref{location(value_storage)}
}

[return<int>]
score_ptrs([args<Pointer<i32>>] values) {
  return(plus(dereference(values[0i32]), dereference(values[2i32])))
}

[return<int>]
score_from_deref_fields([args<Reference<Holder>>] values) {
  return(score_ptrs(location(dereference(values[0i32]).value_ref),
                    location(dereference(values.at(1i32)).value_ref),
                    location(dereference(values.at_unsafe(2i32)).value_ref)))
}

[return<int>]
forward([args<Reference<Holder>>] values) {
  return(score_from_deref_fields([spread] values))
}

[return<int>]
forward_mixed([args<Reference<Holder>>] values) {
  [i32] extra_value{1i32}
  [Holder] extra
  assign(extra.value_storage, extra_value)
  return(score_ptrs(location(dereference(values.at(0i32)).value_ref),
                    location(extra.value_ref),
                    location(dereference(at(values, 1i32)).value_ref)))
}

[return<int>]
main() {
  [i32] a0{1i32}
  [i32] a1{2i32}
  [i32] a2{3i32}
  [Holder] h0
  [Holder] h1
  [Holder] h2
  assign(h0.value_storage, a0)
  assign(h1.value_storage, a1)
  assign(h2.value_storage, a2)
  [Reference<Holder>] r0{location(h0)}
  [Reference<Holder>] r1{location(h1)}
  [Reference<Holder>] r2{location(h2)}

  [i32] b0{4i32}
  [i32] b1{5i32}
  [i32] b2{6i32}
  [Holder] i0
  [Holder] i1
  [Holder] i2
  assign(i0.value_storage, b0)
  assign(i1.value_storage, b1)
  assign(i2.value_storage, b2)
  [Reference<Holder>] s0{location(i0)}
  [Reference<Holder>] s1{location(i1)}
  [Reference<Holder>] s2{location(i2)}

  [i32] c0{7i32}
  [i32] c1{8i32}
  [Holder] j0
  [Holder] j1
  assign(j0.value_storage, c0)
  assign(j1.value_storage, c1)
  [Reference<Holder>] t0{location(j0)}
  [Reference<Holder>] t1{location(j1)}

  return(plus(score_from_deref_fields(r0, r1, r2),
              plus(forward(s0, s1, s2),
                   forward_mixed(t0, t1))))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_variadic_args_scalar_pointer_indexed_deref_receiver_ref_field.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() /
       "primec_cpp_variadic_args_scalar_pointer_indexed_deref_receiver_ref_field_exe")
          .string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 29);
}

TEST_CASE("C++ emitter materializes variadic struct pointer packs from borrowed pack reference fields") {
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
score_ptrs([args<Pointer<Pair>>] values) {
  return(plus(values[0i32].value, values[2i32].score()))
}

[return<int>]
score_from_fields([args<Reference<Holder>>] values) {
  return(score_ptrs(location(values.at(0i32).pair_ref),
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
  return(score_ptrs(location(values.at(0i32).pair_ref),
                    location(extra.pair_ref),
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
      writeTemp("compile_cpp_variadic_args_struct_pointer_pack_reference_field.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() /
       "primec_cpp_variadic_args_struct_pointer_pack_reference_field_exe")
          .string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 75);
}

TEST_CASE("C++ emitter materializes variadic scalar reference packs from borrowed pack reference fields") {
  const std::string source = R"(
[struct]
Holder() {
  [mut i32] value_storage{0i32}
  [Reference<i32>] value_ref{location(value_storage)}
}

[return<int>]
score_refs([args<Reference<i32>>] values) {
  return(plus(dereference(values[0i32]), dereference(values[2i32])))
}

[return<int>]
score_from_fields([args<Reference<Holder>>] values) {
  return(score_refs(location(at(values, 0i32).value_ref),
                    location(values.at(1i32).value_ref),
                    location(values.at_unsafe(2i32).value_ref)))
}

[return<int>]
forward([args<Reference<Holder>>] values) {
  return(score_from_fields([spread] values))
}

[return<int>]
forward_mixed([args<Reference<Holder>>] values) {
  [i32] extra_value{1i32}
  [Holder] extra
  assign(extra.value_storage, extra_value)
  return(score_refs(location(extra.value_ref),
                    location(values.at(0i32).value_ref),
                    location(at(values, 1i32).value_ref)))
}

[return<int>]
main() {
  [i32] a0{1i32}
  [i32] a1{2i32}
  [i32] a2{3i32}
  [Holder] h0
  [Holder] h1
  [Holder] h2
  assign(h0.value_storage, a0)
  assign(h1.value_storage, a1)
  assign(h2.value_storage, a2)
  [Reference<Holder>] r0{location(h0)}
  [Reference<Holder>] r1{location(h1)}
  [Reference<Holder>] r2{location(h2)}

  [i32] b0{4i32}
  [i32] b1{5i32}
  [i32] b2{6i32}
  [Holder] i0
  [Holder] i1
  [Holder] i2
  assign(i0.value_storage, b0)
  assign(i1.value_storage, b1)
  assign(i2.value_storage, b2)
  [Reference<Holder>] s0{location(i0)}
  [Reference<Holder>] s1{location(i1)}
  [Reference<Holder>] s2{location(i2)}

  [i32] c0{7i32}
  [i32] c1{8i32}
  [Holder] j0
  [Holder] j1
  assign(j0.value_storage, c0)
  assign(j1.value_storage, c1)
  [Reference<Holder>] t0{location(j0)}
  [Reference<Holder>] t1{location(j1)}

  return(plus(score_from_fields(r0, r1, r2),
              plus(forward(s0, s1, s2),
                   forward_mixed(t0, t1))))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_variadic_args_scalar_reference_pack_reference_field.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() /
       "primec_cpp_variadic_args_scalar_reference_pack_reference_field_exe")
          .string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 23);
}


TEST_SUITE_END();
