#include "test_compile_run_helpers.h"
#include "test_compile_run_collection_conformance_helpers.h"
#include "test_compile_run_emitters_helpers.h"

#include "primec/testing/EmitterHelpers.h"

TEST_SUITE_BEGIN("primestruct.compile.run.emitters.cpp");

TEST_CASE("rejects wrapper canonical direct-call struct method chain forwarding in C++ emitter") {
  const std::string source = R"(
Marker {
  [i32] value
}

[effects(heap_alloc), return<vector<i32>>]
wrapValues() {
  return(vector<i32>(5i32, 6i32, 7i32))
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
  return(/std/collections/vector/at(wrapValues(), 2i32).tag())
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_wrapper_canonical_direct_struct_method_chain_forwarding.prime", source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_cpp_wrapper_canonical_direct_struct_method_chain_forwarding.err")
          .string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("unknown method: /i32/tag") != std::string::npos);
}

TEST_CASE("rejects wrapper canonical direct-call method receiver fallback without helper in C++ emitter") {
  const std::string source = R"(
namespace i32 {
  [return<int>]
  tag([i32] value) {
    return(plus(value, 40i32))
  }
}

[effects(heap_alloc), return<vector<i32>>]
wrapValues() {
  return(vector<i32>(5i32, 6i32, 7i32))
}

[effects(heap_alloc), return<int>]
main() {
  return(/std/collections/vector/at(wrapValues(), 1i32).tag())
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_wrapper_canonical_direct_method_receiver_fallback_reject.prime", source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_cpp_wrapper_canonical_direct_method_receiver_fallback_reject.err")
          .string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("unknown call target: /std/collections/vector/at") !=
        std::string::npos);
}

TEST_CASE("rejects wrapper canonical direct-call map method receiver fallback without helper in C++ emitter") {
  const std::string source = R"(
namespace i32 {
  [return<int>]
  tag([i32] value) {
    return(plus(value, 40i32))
  }
}

[effects(heap_alloc), return</std/collections/map<i32, i32>>]
wrapMap() {
  return(map<i32, i32>(1i32, 5i32, 2i32, 6i32))
}

[effects(heap_alloc), return<int>]
main() {
  return(/std/collections/map/at(wrapMap(), 1i32).tag())
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_wrapper_canonical_direct_map_method_receiver_fallback_reject.prime", source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_cpp_wrapper_canonical_direct_map_method_receiver_fallback_reject.err")
          .string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("unknown call target: /std/collections/map/at") !=
        std::string::npos);
}

TEST_CASE("rejects wrapper compatibility direct-call map receiver fallback without helper in C++ emitter") {
  const std::string source = R"(
namespace i32 {
  [return<int>]
  tag([i32] value) {
    return(plus(value, 40i32))
  }
}

[effects(heap_alloc), return</std/collections/map<i32, i32>>]
wrapMap() {
  return(map<i32, i32>(1i32, 5i32, 2i32, 6i32))
}

[effects(heap_alloc), return<int>]
main() {
  return(/map/at(wrapMap(), 1i32).tag())
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_wrapper_compatibility_direct_map_receiver_fallback_reject.prime", source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_cpp_wrapper_compatibility_direct_map_receiver_fallback_reject.err")
          .string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("unknown call target: /map/at") != std::string::npos);
}

TEST_CASE("C++ emitter keeps wrapper-returned vector direct-call string count forwarding") {
  const std::string source = R"(
[return<int>]
/string/count([string] values) {
  return(91i32)
}

[return<string>]
/std/collections/vector/at([vector<i32>] values, [i32] index) {
  return("abc"raw_utf8)
}

[return<string>]
/std/collections/vector/at_unsafe([vector<i32>] values, [i32] index) {
  return("abc"raw_utf8)
}

[effects(heap_alloc), return<vector<i32>>]
wrapValues() {
  return(vector<i32>(1i32))
}

[effects(heap_alloc), return<int>]
main() {
  return(plus(count(/vector/at(wrapValues(), 0i32)),
              wrapValues().at_unsafe(0i32).count()))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_wrapper_vector_access_direct_count_forwarding.prime", source);
  const std::string exePath =
      (testScratchPath("") /
       "primec_cpp_wrapper_vector_access_direct_count_forwarding_exe")
          .string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 182);
}

TEST_CASE("C++ emitter keeps vector alias direct-call count canonical wrapper return forwarding") {
  const std::string source = R"(
[effects(heap_alloc), return<vector<i32>>]
wrapValues() {
  return(vector<i32>(1i32))
}

[return<string>]
/std/collections/vector/at([vector<i32>] values, [i32] index) {
  return("abc"raw_utf8)
}

[effects(heap_alloc), return<int>]
main() {
  return(count(/vector/at(wrapValues(), 0i32)))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_vector_alias_access_direct_count_canonical_wrapper_forwarding.prime", source);
  const std::string exePath =
      (testScratchPath("") /
       "primec_cpp_vector_alias_access_direct_count_canonical_wrapper_forwarding_exe")
          .string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 91);
}

TEST_CASE("C++ emitter keeps primitive diagnostics on vector alias access count with canonical non-string wrapper return") {
  const std::string source = R"(
[effects(heap_alloc), return<vector<i32>>]
wrapValues() {
  return(vector<i32>(1i32))
}

[return<int>]
/std/collections/vector/at([vector<i32>] values, [i32] index) {
  return(7i32)
}

[effects(heap_alloc), return<int>]
main() {
  return(count(/vector/at(wrapValues(), 0i32)))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_vector_alias_access_count_canonical_wrapper_return_non_string_reject.prime", source);
  const std::string errPath = (testScratchPath("") /
                               "primec_cpp_vector_alias_access_count_canonical_wrapper_return_non_string_reject.err")
                                  .string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("unknown method: /i32/count") != std::string::npos);
}

TEST_CASE("rejects inferred wrapper string count arg mismatch in C++ emitter") {
  const std::string source = R"(
wrapText() {
  [string] text{"abc"raw_utf8}
  return(text)
}

[return<int>]
main() {
  [i32] callValue{count(wrapText(), 0i32)}
  [i32] methodValue{wrapText().count(1i32)}
  return(plus(callValue, methodValue))
}
)";
  const std::string srcPath = writeTemp("compile_cpp_inferred_wrapper_string_count_arg_mismatch.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_cpp_inferred_wrapper_string_count_arg_mismatch_exe")
          .string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 2);
}

TEST_CASE("rejects inferred wrapper string access index mismatch in C++ emitter") {
  const std::string source = R"(
wrapText() {
  [string] text{"abc"raw_utf8}
  return(text)
}

[return<int>]
main() {
  [i32] callValue{at(wrapText(), true)}
  [i32] methodValue{wrapText().at(true)}
  [i32] unsafeCall{at_unsafe(wrapText(), true)}
  [i32] unsafeMethod{wrapText().at_unsafe(true)}
  return(plus(callValue, plus(methodValue, plus(unsafeCall, unsafeMethod))))
}
)";
  const std::string srcPath = writeTemp("compile_cpp_inferred_wrapper_string_access_index_mismatch.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_cpp_inferred_wrapper_string_access_index_mismatch_exe")
          .string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 2);
}

TEST_CASE("compiles and runs user wrapper temporary access shadow precedence in C++ emitter") {
  const std::string source = R"(
[return<map<i32, i32>>]
wrapMap() {
  return(map<i32, i32>(1i32, 2i32))
}

[effects(heap_alloc), return<vector<i32>>]
wrapVector() {
  return(vector<i32>(3i32, 4i32))
}

[return<int>]
/map/at([map<i32, i32>] values, [i32] key) {
  return(80i32)
}

[return<int>]
/map/at_unsafe([map<i32, i32>] values, [i32] key) {
  return(81i32)
}

[effects(heap_alloc), return<int>]
/vector/at([vector<i32>] values, [i32] index) {
  return(82i32)
}

[effects(heap_alloc), return<int>]
/vector/at_unsafe([vector<i32>] values, [i32] index) {
  return(83i32)
}

[effects(heap_alloc), return<int>]
main() {
  return(plus(plus(plus(at(wrapMap(), 1i32), wrapMap().at(1i32)), wrapMap()[1i32]),
              plus(plus(at_unsafe(wrapMap(), 1i32), wrapMap().at_unsafe(1i32)),
                   plus(plus(plus(at(wrapVector(), 0i32), wrapVector().at(0i32)), wrapVector()[0i32]),
                        plus(at_unsafe(wrapVector(), 0i32), wrapVector().at_unsafe(0i32))))))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_user_wrapper_temp_access_shadow_precedence.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_cpp_user_wrapper_temp_access_shadow_precedence_exe")
          .string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == expectedProcessExitCode(814));
}

TEST_CASE("rejects user wrapper temporary access shadow value mismatch in C++ emitter") {
  const std::string source = R"(
[return<map<i32, i32>>]
wrapMap() {
  return(map<i32, i32>(1i32, 2i32))
}

[effects(heap_alloc), return<vector<i32>>]
wrapVector() {
  return(vector<i32>(3i32, 4i32))
}

[return<bool>]
/map/at([map<i32, i32>] values, [i32] key) {
  return(true)
}

[return<bool>]
/map/at_unsafe([map<i32, i32>] values, [i32] key) {
  return(false)
}

[effects(heap_alloc), return<bool>]
/vector/at([vector<i32>] values, [i32] index) {
  return(true)
}

[effects(heap_alloc), return<bool>]
/vector/at_unsafe([vector<i32>] values, [i32] index) {
  return(false)
}

[effects(heap_alloc), return<int>]
main() {
  [i32] mapCall{at(wrapMap(), 1i32)}
  [i32] mapMethod{wrapMap().at(1i32)}
  [i32] mapIndex{wrapMap()[1i32]}
  [i32] mapUnsafeCall{at_unsafe(wrapMap(), 1i32)}
  [i32] mapUnsafeMethod{wrapMap().at_unsafe(1i32)}
  [i32] vectorCall{at(wrapVector(), 0i32)}
  [i32] vectorMethod{wrapVector().at(0i32)}
  [i32] vectorIndex{wrapVector()[0i32]}
  [i32] vectorUnsafeCall{at_unsafe(wrapVector(), 0i32)}
  [i32] vectorUnsafeMethod{wrapVector().at_unsafe(0i32)}
  return(plus(mapCall, plus(mapMethod, plus(mapIndex, plus(mapUnsafeCall, plus(mapUnsafeMethod,
              plus(vectorCall, plus(vectorMethod, plus(vectorIndex, plus(vectorUnsafeCall, vectorUnsafeMethod)))))))))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_user_wrapper_temp_access_shadow_value_mismatch.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_cpp_user_wrapper_temp_access_shadow_value_mismatch_exe")
          .string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 2);
}

TEST_CASE("rejects non-vector capacity call target in C++ emitter") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 2i32)}
  return(capacity(values))
}
)";
  const std::string srcPath = writeTemp("compile_cpp_capacity_call_non_vector_reject.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_cpp_capacity_call_non_vector_reject.err").string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("capacity requires vector target") != std::string::npos);
}

TEST_CASE("rejects non-vector capacity method target in C++ emitter") {
  const std::string source = R"(
[return<int>]
main() {
  [string] text{"abc"raw_utf8}
  return(text.capacity())
}
)";
  const std::string srcPath = writeTemp("compile_cpp_capacity_method_non_vector_reject.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_cpp_capacity_method_non_vector_reject.err").string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("capacity requires vector target") != std::string::npos);
}

TEST_CASE("compiles and runs user array capacity helper shadow in C++ emitter") {
  const std::string source = R"(
[return<int>]
/array/capacity([array<i32>] values) {
  return(plus(count(values), 5i32))
}

[return<int>]
main() {
  [array<i32>] values{array<i32>(1i32, 2i32)}
  return(plus(capacity(values), values.capacity()))
}
)";
  const std::string srcPath = writeTemp("compile_cpp_user_array_capacity_shadow.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_cpp_user_array_capacity_shadow_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 14);
}

TEST_CASE("compiles and runs std math vector and color types") {
  const std::string source = R"(
import /std/math/*

[return<int>]
main() {
  [Vec2] v2{Vec2(1.0f32, 2.0f32)}
  [Vec3] v3{Vec3(3.0f32, 4.0f32, 0.0f32)}
  [Vec4] v4{Vec4(1.0f32, 2.0f32, 3.0f32, 4.0f32)}
  [f32] len2{v3.lengthSquared()}

  [ColorRGB] base{ColorRGB(0.5f32, 0.5f32, 0.5f32)}
  [ColorRGB] mixed{base.add(ColorRGB(0.5f32, 0.5f32, 0.5f32))}
  [ColorRGBA] rgba{ColorRGBA(0.5f32, 0.5f32, 0.5f32, 1.0f32)}
  [ColorRGBA] mixedA{rgba.mulScalar(2.0f32)}
  [ColorSRGB] srgb{ColorSRGB(0.0f32, 0.0f32, 0.0f32)}
  [ColorSRGBA] srgba{ColorSRGBA(0.0f32, 0.0f32, 0.0f32, 1.0f32)}

  [f32] total{len2 + mixed.r + mixed.g + mixed.b + mixedA.a + v2.x + v4.w + srgb.r + srgba.a}
  return(convert<int>(total))
}
)";
  const std::string srcPath = writeTemp("compile_std_math_vec_color.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_std_math_vec_color_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 36);
}

TEST_CASE("compiles and runs std math matrix types") {
  const std::string source = R"(
import /std/math/*

[return<int>]
main() {
  [Mat2] m2{Mat2(1.0f32, 2.0f32, 3.0f32, 4.0f32)}
  [Mat3] m3{Mat3(
    5.0f32, 6.0f32, 7.0f32,
    8.0f32, 9.0f32, 10.0f32,
    11.0f32, 12.0f32, 13.0f32
  )}
  [Mat4] m4{Mat4(
    14.0f32, 15.0f32, 16.0f32, 17.0f32,
    18.0f32, 19.0f32, 20.0f32, 21.0f32,
    22.0f32, 23.0f32, 24.0f32, 25.0f32,
    26.0f32, 27.0f32, 28.0f32, 29.0f32
  )}

  [f32] total{m2.m00 + m2.m11 + m3.m12 + m4.m33}
  return(convert<int>(total))
}
)";
  const std::string srcPath = writeTemp("compile_std_math_matrix.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_std_math_matrix_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 44);
}

TEST_CASE("compiles and runs std math quaternion type") {
  const std::string source = R"(
import /std/math/*

[return<int>]
main() {
  [Quat] raw{Quat(0.0f32, 0.0f32, 0.0f32, 2.0f32)}
  [Quat] normalized{raw.toNormalized()}
  [Quat] zero{Quat(0.0f32, 0.0f32, 0.0f32, 0.0f32).normalize()}
  [f32] total{normalized.w + zero.x + zero.y + zero.z + zero.w}
  return(convert<int>(total))
}
)";
  const std::string srcPath = writeTemp("compile_std_math_quat.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_std_math_quat_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 1);
}

TEST_CASE("compiles and runs std math quat_to_mat3 helper") {
  const std::string source = R"(
import /std/math/*

[return<int>]
main() {
  [Quat] raw{Quat(2.0f32, 0.0f32, 0.0f32, 0.0f32)}
  [Mat3] basis{quat_to_mat3(raw)}
  return(convert<int>(basis.m00 - basis.m11 - basis.m22))
}
)";
  const std::string srcPath = writeTemp("compile_std_math_quat_to_mat3.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_std_math_quat_to_mat3_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 3);
}

TEST_CASE("compiles and runs std math quat_to_mat4 helper") {
  const std::string source = R"(
import /std/math/*

[return<int>]
main() {
  [Quat] raw{Quat(2.0f32, 0.0f32, 0.0f32, 0.0f32)}
  [Mat4] basis{quat_to_mat4(raw)}
  return(convert<int>(basis.m00 - basis.m11 - basis.m22 + basis.m33))
}
)";
  const std::string srcPath = writeTemp("compile_std_math_quat_to_mat4.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_std_math_quat_to_mat4_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 4);
}

TEST_CASE("compiles and runs std math mat3_to_quat helper") {
  const std::string source = R"(
import /std/math/*

[return<int>]
main() {
  [Mat3] basis{Mat3(
    1.0f32, 0.0f32, 0.0f32,
    0.0f32, -1.0f32, 0.0f32,
    0.0f32, 0.0f32, -1.0f32
  )}
  [Quat] value{mat3_to_quat(basis)}
  return(convert<int>(value.x - value.y - value.z + value.w))
}
)";
  const std::string srcPath = writeTemp("compile_std_math_mat3_to_quat.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_std_math_mat3_to_quat_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 1);
}

TEST_CASE("compiles and runs C++ support-matrix math nominal helpers") {
  const std::string source = R"(
import /std/math/*

[return<int>]
main() {
  [Mat2] m2{Mat2(1.0f32, 2.0f32, 3.0f32, 4.0f32)}
  [Quat] raw{Quat(0.0f32, 0.0f32, 0.0f32, 2.0f32)}
  [Quat] normalized{raw.toNormalized()}
  [Mat3] basis3{quat_to_mat3(raw)}
  [Mat4] basis4{quat_to_mat4(raw)}
  [Quat] restored{mat3_to_quat(Mat3(
    1.0f32, 0.0f32, 0.0f32,
    0.0f32, -1.0f32, 0.0f32,
    0.0f32, 0.0f32, -1.0f32
  ))}
  [f32] total{m2.m00 + m2.m11 + normalized.w + basis3.m00 + basis4.m33 + restored.x + restored.w}
  return(convert<int>(total))
}
)";
  const std::string srcPath = writeTemp("compile_cpp_support_matrix_math_nominal_helpers.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_cpp_support_matrix_math_nominal_helpers_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 9);
}

TEST_CASE("compiles and runs C++ quaternion reference multiply and rotation") {
  const std::string source = R"(
import /std/math/*

[return<int>]
main() {
  [Quat] turnX{Quat(1.0f32, 0.0f32, 0.0f32, 0.0f32)}
  [Quat] turnY{Quat(0.0f32, 1.0f32, 0.0f32, 0.0f32)}
  [Quat] product{multiply(turnX, turnY)}
  [Vec3] input{Vec3(1.0f32, 2.0f32, 3.0f32)}
  [Vec3] rotated{multiply(product, input)}
  [f32] total{product.z - product.x - product.y - product.w + rotated.z - rotated.x - rotated.y}
  return(convert<int>(total))
}
)";
  const std::string srcPath = writeTemp("compile_cpp_quaternion_reference_multiply_rotation.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_cpp_quaternion_reference_multiply_rotation_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 7);
}

TEST_CASE("compiles and runs C++ matrix composition order references with tolerance") {
  const std::string source = R"(
import /std/math/*

[return<int>]
main() {
  [Mat3] rotate{Mat3(
    0.0f32, -1.0f32, 0.0f32,
    1.0f32, 0.0f32, 0.0f32,
    0.0f32, 0.0f32, 1.0f32
  )}
  [Mat3] scale{Mat3(
    2.0f32, 0.0f32, 0.0f32,
    0.0f32, 3.0f32, 0.0f32,
    0.0f32, 0.0f32, 1.0f32
  )}
  [Vec3] input{Vec3(1.0f32, 2.0f32, 4.0f32)}
  [Vec3] rotatedInput{multiply(rotate, input)}
  [Vec3] nested{multiply(scale, rotatedInput)}
  [Mat3] combined{multiply(scale, rotate)}
  [Vec3] viaCombined{multiply(combined, input)}
  [Mat3] wrongCombined{multiply(rotate, scale)}
  [Vec3] wrongOrder{multiply(wrongCombined, input)}
  [f32] tolerance{0.0001f32}
  [f32] nestedError{abs(nested.x + 4.0f32) + abs(nested.y - 3.0f32) + abs(nested.z - 4.0f32)}
  [f32] combinedError{abs(viaCombined.x + 4.0f32) + abs(viaCombined.y - 3.0f32) + abs(viaCombined.z - 4.0f32)}
  [f32] wrongOrderError{abs(wrongOrder.x + 6.0f32) + abs(wrongOrder.y - 2.0f32) + abs(wrongOrder.z - 4.0f32)}
  return(convert<int>(nestedError <= tolerance && combinedError <= tolerance && wrongOrderError <= tolerance))
}
)";
  const std::string srcPath = writeTemp("compile_cpp_matrix_composition_reference.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_cpp_matrix_composition_reference_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 1);
}

TEST_SUITE_END();
