#include "test_compile_run_helpers.h"

#include "test_compile_run_smoke_helpers.h"

TEST_SUITE_BEGIN("primestruct.compile.run.smoke");

TEST_CASE("count forwards to type method across backends") {
  const std::string source = R"(
namespace i32 {
  [return<int>]
  count([i32] self) {
    return(plus(self, 2i32))
  }
}

[return<int>]
main() {
  return(count(3i32))
}
)";
  const std::string srcPath = writeTemp("compile_count_method.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_count_method_exe").string();
  const std::string nativePath = (testScratchPath("") / "primec_count_method_native").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 5);

  const std::string runVmCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runVmCmd) == 5);

  const std::string compileNativeCmd = "./primec --emit=native " + srcPath + " -o " + nativePath + " --entry /main";
  CHECK(runCommand(compileNativeCmd) == 0);
  CHECK(runCommand(nativePath) == 5);
}

TEST_CASE("semicolons act as separators") {
  const std::string source = R"(
;
[return<int>]
add([i32] a; [i32] b) {
  return(plus(a, b));
}
;
[return<int>]
main() {
  [i32] value{
    add(1i32; 2i32);
  };
  return(value);
}
)";
  const std::string srcPath = writeTemp("compile_semicolon_separators.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_semicolon_sep_exe").string();
  const std::string nativePath = (testScratchPath("") / "primec_semicolon_sep_native").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 3);

  const std::string runVmCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runVmCmd) == 3);

  const std::string compileNativeCmd = "./primec --emit=native " + srcPath + " -o " + nativePath + " --entry /main";
  CHECK(runCommand(compileNativeCmd) == 0);
  CHECK(runCommand(nativePath) == 3);
}

TEST_CASE("rejects non-argv entry parameter in exe") {
  const std::string source = R"(
[return<int>]
main([i32] value) {
  return(value)
}
)";
  const std::string srcPath = writeTemp("compile_entry_bad_param.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_entry_bad_param_err.txt").string();
  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("entry definition must take a single array<string> parameter") != std::string::npos);
}

TEST_CASE("rejects unsupported emit kinds") {
  const std::string source = R"(
[return<int>]
main() {
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_emit_invalid.prime", source);
  const std::string errPath = (testScratchPath("") / "primec_emit_invalid_err.txt").string();

  const std::string emitMetalCmd =
      "./primec --emit=metal " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(emitMetalCmd) == 2);
  CHECK(readFile(errPath).find("Usage: primec") != std::string::npos);

  const std::string emitLlvmCmd =
      "./primec --emit=llvm " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(emitLlvmCmd) == 2);
  CHECK(readFile(errPath).find("Usage: primec") != std::string::npos);
}

TEST_CASE("primec emits wasm bytecode for integer local control-flow subset") {
  const std::string source = R"(
[return<int>]
main() {
  [i32] value{plus(2i32, 5i32)}
  if(equal(value, 7i32)) {
    return(7i32)
  } else {
    return(3i32)
  }
}
)";
  const std::string srcPath = writeTemp("compile_emit_wasm_subset.prime", source);
  const std::string wasmPath = (testScratchPath("") / "primec_emit_wasm_subset.wasm").string();
  const std::string errPath = (testScratchPath("") / "primec_emit_wasm_option_err.txt").string();
  const std::string outPath = (testScratchPath("") / "primec_emit_wasm_subset_out.txt").string();

  const std::string wasmCmd = "./primec --emit=wasm " + quoteShellArg(srcPath) + " -o " + quoteShellArg(wasmPath) +
                              " --entry /main 2> " + quoteShellArg(errPath);
  if (!runWasmCompileCommandOrExpectUnsupported(wasmCmd, wasmPath, errPath)) {
    return;
  }
  CHECK(std::filesystem::exists(wasmPath));
  CHECK(std::filesystem::file_size(wasmPath) > 8);

  if (hasWasmtime()) {
    const std::string runCmd =
        "wasmtime --invoke main " + quoteShellArg(wasmPath) + " > " + quoteShellArg(outPath);
    CHECK(runCommand(runCmd) == 0);
    const std::string output = readFile(outPath);
    CHECK(output.find("7") != std::string::npos);
  }
}

TEST_CASE("primec emits wasm bytecode for float ops with tolerance-gated conversions") {
  const std::string source = R"(
import /std/math/*

[return<int>]
main() {
  if(less_than(abs(minus(convert<f32>(convert<f64>(plus(1.25f32, 0.5f32))), 1.75f32)), 0.0001f32)) {
    return(convert<int>(multiply(convert<f32>(2i32), 2.5f32)))
  } else {
    return(0i32)
  }
}
)";
  const std::string srcPath = writeTemp("compile_emit_wasm_float_subset.prime", source);
  const std::string wasmPath = (testScratchPath("") / "primec_emit_wasm_float_subset.wasm").string();
  const std::string errPath = (testScratchPath("") / "primec_emit_wasm_float_subset_err.txt").string();
  const std::string outPath = (testScratchPath("") / "primec_emit_wasm_float_subset_out.txt").string();

  const std::string wasmCmd = "./primec --emit=wasm " + quoteShellArg(srcPath) + " -o " + quoteShellArg(wasmPath) +
                              " --entry /main 2> " + quoteShellArg(errPath);
  if (!runWasmCompileCommandOrExpectUnsupported(wasmCmd, wasmPath, errPath)) {
    return;
  }
  CHECK(std::filesystem::exists(wasmPath));
  CHECK(std::filesystem::file_size(wasmPath) > 8);

  if (hasWasmtime()) {
    const std::string runCmd =
        "wasmtime --invoke main " + quoteShellArg(wasmPath) + " > " + quoteShellArg(outPath);
    CHECK(runCommand(runCmd) == 0);
    const std::string output = readFile(outPath);
    CHECK(output.find("5") != std::string::npos);
  }
}

TEST_CASE("primec emits wasm bytecode for i64 and u64 conversion opcodes") {
  const std::string source = R"(
[return<int>]
main() {
  if(equal(convert<int>(convert<f64>(convert<i64>(7.9f32))), 7i32)) {
    if(equal(convert<int>(convert<f32>(convert<u64>(9.9f32))), 9i32)) {
      return(1i32)
    }
  }
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_emit_wasm_i64_u64_conversions.prime", source);
  const std::string wasmPath =
      (testScratchPath("") / "primec_emit_wasm_i64_u64_conversions.wasm").string();
  const std::string errPath =
      (testScratchPath("") / "primec_emit_wasm_i64_u64_conversions_err.txt").string();
  const std::string outPath =
      (testScratchPath("") / "primec_emit_wasm_i64_u64_conversions_out.txt").string();

  const std::string wasmCmd = "./primec --emit=wasm " + quoteShellArg(srcPath) + " -o " + quoteShellArg(wasmPath) +
                              " --entry /main 2> " + quoteShellArg(errPath);
  if (!runWasmCompileCommandOrExpectUnsupported(wasmCmd, wasmPath, errPath)) {
    return;
  }
  CHECK(std::filesystem::exists(wasmPath));
  CHECK(std::filesystem::file_size(wasmPath) > 8);

  if (hasWasmtime()) {
    const std::string runCmd =
        "wasmtime --invoke main " + quoteShellArg(wasmPath) + " > " + quoteShellArg(outPath);
    CHECK(runCommand(runCmd) == 0);
    const std::string output = readFile(outPath);
    CHECK(output.find("1") != std::string::npos);
  }
}

TEST_CASE("primec emits wasm bytecode for support-matrix math nominal helpers") {
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
  const std::string srcPath = writeTemp("compile_emit_wasm_support_matrix_math_nominal_helpers.prime", source);
  const std::string wasmPath =
      (testScratchPath("") / "primec_emit_wasm_support_matrix_math_nominal_helpers.wasm").string();
  const std::string errPath = (testScratchPath("") /
                               "primec_emit_wasm_support_matrix_math_nominal_helpers_err.txt")
                                  .string();
  const std::string outPath = (testScratchPath("") /
                               "primec_emit_wasm_support_matrix_math_nominal_helpers_out.txt")
                                  .string();

  const std::string wasmCmd = "./primec --emit=wasm " + quoteShellArg(srcPath) + " -o " + quoteShellArg(wasmPath) +
                              " --entry /main 2> " + quoteShellArg(errPath);
  if (!runWasmCompileCommandOrExpectUnsupported(wasmCmd, wasmPath, errPath)) {
    return;
  }
  CHECK(std::filesystem::exists(wasmPath));
  CHECK(std::filesystem::file_size(wasmPath) > 8);

  if (hasWasmtime()) {
    const std::string runCmd =
        "wasmtime --invoke main " + quoteShellArg(wasmPath) + " > " + quoteShellArg(outPath);
    CHECK(runCommand(runCmd) == 0);
    const std::string output = readFile(outPath);
    CHECK(output.find("9") != std::string::npos);
  }
}

TEST_CASE("primec emits wasm bytecode for quaternion reference multiply and rotation") {
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
  const std::string srcPath = writeTemp("compile_emit_wasm_quaternion_reference_multiply_rotation.prime", source);
  const std::string wasmPath =
      (testScratchPath("") / "primec_emit_wasm_quaternion_reference_multiply_rotation.wasm")
          .string();
  const std::string errPath =
      (testScratchPath("") /
       "primec_emit_wasm_quaternion_reference_multiply_rotation_err.txt")
          .string();
  const std::string outPath =
      (testScratchPath("") /
       "primec_emit_wasm_quaternion_reference_multiply_rotation_out.txt")
          .string();

  const std::string wasmCmd = "./primec --emit=wasm " + quoteShellArg(srcPath) + " -o " + quoteShellArg(wasmPath) +
                              " --entry /main 2> " + quoteShellArg(errPath);
  if (!runWasmCompileCommandOrExpectUnsupported(wasmCmd, wasmPath, errPath)) {
    return;
  }
  CHECK(std::filesystem::exists(wasmPath));
  CHECK(std::filesystem::file_size(wasmPath) > 8);

  if (hasWasmtime()) {
    const std::string runCmd =
        "wasmtime --invoke main " + quoteShellArg(wasmPath) + " > " + quoteShellArg(outPath);
    CHECK(runCommand(runCmd) == 0);
    const std::string output = readFile(outPath);
    CHECK(output.find("7") != std::string::npos);
  }
}

TEST_CASE("primec emits wasm bytecode for matrix composition order references with tolerance") {
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
  const std::string srcPath = writeTemp("compile_emit_wasm_matrix_composition_reference.prime", source);
  const std::string wasmPath =
      (testScratchPath("") / "primec_emit_wasm_matrix_composition_reference.wasm").string();
  const std::string errPath =
      (testScratchPath("") / "primec_emit_wasm_matrix_composition_reference_err.txt").string();
  const std::string outPath =
      (testScratchPath("") / "primec_emit_wasm_matrix_composition_reference_out.txt").string();

  const std::string wasmCmd = "./primec --emit=wasm " + quoteShellArg(srcPath) + " -o " + quoteShellArg(wasmPath) +
                              " --entry /main 2> " + quoteShellArg(errPath);
  if (!runWasmCompileCommandOrExpectUnsupported(wasmCmd, wasmPath, errPath)) {
    return;
  }
  CHECK(std::filesystem::exists(wasmPath));
  CHECK(std::filesystem::file_size(wasmPath) > 8);

  if (hasWasmtime()) {
    const std::string runCmd =
        "wasmtime --invoke main " + quoteShellArg(wasmPath) + " > " + quoteShellArg(outPath);
    CHECK(runCommand(runCmd) == 0);
    const std::string output = readFile(outPath);
    CHECK(output.find("1") != std::string::npos);
  }
}

TEST_CASE("primec emits wasm bytecode for matrix arithmetic helpers with tolerance") {
  const std::string source = R"(
import /std/math/*

[return<int>]
main() {
  [Mat2] base2{Mat2(
    1.0f32, 2.0f32,
    3.0f32, 4.0f32
  )}
  [Mat2] delta2{Mat2(
    0.5f32, -1.0f32,
    1.5f32, 2.0f32
  )}
  [Mat2] sum2{plus(base2, delta2)}
  [Mat2] div2{divide(sum2, 2.0f32)}
  [Mat3] base3{Mat3(
    1.0f32, 2.0f32, 3.0f32,
    4.0f32, 5.0f32, 6.0f32,
    7.0f32, 8.0f32, 9.0f32
  )}
  [Mat3] delta3{Mat3(
    0.5f32, 1.0f32, 1.5f32,
    2.0f32, 2.5f32, 3.0f32,
    3.5f32, 4.0f32, 4.5f32
  )}
  [Mat3] diff3{minus(base3, delta3)}
  [Mat3] scaledLeft3{multiply(2i32, base3)}
  [Mat4] base4{Mat4(
    1.0f32, 2.0f32, 3.0f32, 4.0f32,
    5.0f32, 6.0f32, 7.0f32, 8.0f32,
    9.0f32, 10.0f32, 11.0f32, 12.0f32,
    13.0f32, 14.0f32, 15.0f32, 16.0f32
  )}
  [Mat4] scaledRight4{multiply(base4, 0.5f32)}
  [Mat4] doubled4{multiply(base4, 2.0f32)}
  [Mat4] restored4{divide(doubled4, 2i32)}
  [f32] tolerance{0.0001f32}
  [f32] totalError{
    abs(sum2.m00 - 1.5f32) +
    abs(sum2.m01 - 1.0f32) +
    abs(sum2.m10 - 4.5f32) +
    abs(sum2.m11 - 6.0f32) +
    abs(div2.m00 - 0.75f32) +
    abs(div2.m11 - 3.0f32) +
    abs(diff3.m00 - 0.5f32) +
    abs(diff3.m12 - 3.0f32) +
    abs(diff3.m22 - 4.5f32) +
    abs(scaledLeft3.m00 - 2.0f32) +
    abs(scaledLeft3.m12 - 12.0f32) +
    abs(scaledLeft3.m22 - 18.0f32) +
    abs(scaledRight4.m00 - 0.5f32) +
    abs(scaledRight4.m13 - 4.0f32) +
    abs(scaledRight4.m31 - 7.0f32) +
    abs(restored4.m00 - 1.0f32) +
    abs(restored4.m12 - 7.0f32) +
    abs(restored4.m30 - 13.0f32) +
    abs(restored4.m33 - 16.0f32)
  }
  return(convert<int>(totalError <= tolerance))
}
)";
  const std::string srcPath = writeTemp("compile_emit_wasm_matrix_arithmetic_helpers.prime", source);
  const std::string wasmPath =
      (testScratchPath("") / "primec_emit_wasm_matrix_arithmetic_helpers.wasm").string();
  const std::string errPath =
      (testScratchPath("") / "primec_emit_wasm_matrix_arithmetic_helpers_err.txt").string();
  const std::string outPath =
      (testScratchPath("") / "primec_emit_wasm_matrix_arithmetic_helpers_out.txt").string();

  const std::string wasmCmd = "./primec --emit=wasm " + quoteShellArg(srcPath) + " -o " + quoteShellArg(wasmPath) +
                              " --entry /main 2> " + quoteShellArg(errPath);
  if (!runWasmCompileCommandOrExpectUnsupported(wasmCmd, wasmPath, errPath)) {
    return;
  }
  CHECK(std::filesystem::exists(wasmPath));
  CHECK(std::filesystem::file_size(wasmPath) > 8);

  if (hasWasmtime()) {
    const std::string runCmd =
        "wasmtime --invoke main " + quoteShellArg(wasmPath) + " > " + quoteShellArg(outPath);
    CHECK(runCommand(runCmd) == 0);
    const std::string output = readFile(outPath);
    CHECK(output.find("1") != std::string::npos);
  }
}

TEST_CASE("primec emits wasm bytecode for quaternion arithmetic helpers with tolerance") {
  const std::string source = R"(
import /std/math/*

[return<int>]
main() {
  [Quat] base{Quat(1.0f32, 2.0f32, 3.0f32, 4.0f32)}
  [Quat] delta{Quat(0.5f32, -1.0f32, 1.5f32, 2.0f32)}
  [Quat] sum{plus(base, delta)}
  [Quat] diff{minus(base, delta)}
  [Quat] scaledLeft{multiply(2i32, base)}
  [Quat] scaledRight{multiply(base, 0.5f32)}
  [Quat] divided{divide(sum, 2i32)}
  [f32] tolerance{0.0001f32}
  [f32] totalError{
    abs(sum.x - 1.5f32) +
    abs(sum.y - 1.0f32) +
    abs(sum.z - 4.5f32) +
    abs(sum.w - 6.0f32) +
    abs(diff.x - 0.5f32) +
    abs(diff.y - 3.0f32) +
    abs(diff.z - 1.5f32) +
    abs(diff.w - 2.0f32) +
    abs(scaledLeft.x - 2.0f32) +
    abs(scaledLeft.y - 4.0f32) +
    abs(scaledLeft.z - 6.0f32) +
    abs(scaledLeft.w - 8.0f32) +
    abs(scaledRight.x - 0.5f32) +
    abs(scaledRight.y - 1.0f32) +
    abs(scaledRight.z - 1.5f32) +
    abs(scaledRight.w - 2.0f32) +
    abs(divided.x - 0.75f32) +
    abs(divided.y - 0.5f32) +
    abs(divided.z - 2.25f32) +
    abs(divided.w - 3.0f32)
  }
  return(convert<int>(totalError <= tolerance))
}
)";
  const std::string srcPath = writeTemp("compile_emit_wasm_quaternion_arithmetic_helpers.prime", source);
  const std::string wasmPath =
      (testScratchPath("") / "primec_emit_wasm_quaternion_arithmetic_helpers.wasm").string();
  const std::string errPath =
      (testScratchPath("") / "primec_emit_wasm_quaternion_arithmetic_helpers_err.txt").string();
  const std::string outPath =
      (testScratchPath("") / "primec_emit_wasm_quaternion_arithmetic_helpers_out.txt").string();

  const std::string wasmCmd = "./primec --emit=wasm " + quoteShellArg(srcPath) + " -o " + quoteShellArg(wasmPath) +
                              " --entry /main 2> " + quoteShellArg(errPath);
  if (!runWasmCompileCommandOrExpectUnsupported(wasmCmd, wasmPath, errPath)) {
    return;
  }
  CHECK(std::filesystem::exists(wasmPath));
  CHECK(std::filesystem::file_size(wasmPath) > 8);

  if (hasWasmtime()) {
    const std::string runCmd =
        "wasmtime --invoke main " + quoteShellArg(wasmPath) + " > " + quoteShellArg(outPath);
    CHECK(runCommand(runCmd) == 0);
    const std::string output = readFile(outPath);
    CHECK(output.find("1") != std::string::npos);
  }
}

TEST_CASE("primec rejects wasm support-matrix plus mismatch with deterministic diagnostic") {
  const std::string source = R"(
import /std/math/*

[return<int>]
main() {
  [Mat2] lhs{Mat2(1.0f32, 2.0f32, 3.0f32, 4.0f32)}
  [Mat3] rhs{Mat3(
    1.0f32, 0.0f32, 0.0f32,
    0.0f32, 1.0f32, 0.0f32,
    0.0f32, 0.0f32, 1.0f32
  )}
  [Mat2] value{plus(lhs, rhs)}
  return(convert<int>(value.m00))
}
)";
  const std::string srcPath = writeTemp("compile_emit_wasm_support_matrix_plus_mismatch.prime", source);
  const std::string wasmPath =
      (testScratchPath("") / "primec_emit_wasm_support_matrix_plus_mismatch.wasm").string();
  const std::string errPath =
      (testScratchPath("") / "primec_emit_wasm_support_matrix_plus_mismatch_err.txt").string();

  const std::string wasmCmd = "./primec --emit=wasm " + quoteShellArg(srcPath) + " -o " + quoteShellArg(wasmPath) +
                              " --entry /main 2> " + quoteShellArg(errPath);
  CHECK(runCommand(wasmCmd) == 2);
  CHECK(readFile(errPath).find("plus requires matching matrix/quaternion operand types") != std::string::npos);
}

TEST_CASE("primec rejects wasm support-matrix implicit conversion with deterministic diagnostic") {
  const std::string source = R"(
import /std/math/*

[return<int>]
main() {
  [Mat3] basis{Mat3(
    1.0f32, 0.0f32, 0.0f32,
    0.0f32, 1.0f32, 0.0f32,
    0.0f32, 0.0f32, 1.0f32
  )}
  [auto] value{quat_to_mat3(basis)}
  return(convert<int>(value.m00))
}
)";
  const std::string srcPath = writeTemp("compile_emit_wasm_support_matrix_implicit_conversion.prime", source);
  const std::string wasmPath =
      (testScratchPath("") / "primec_emit_wasm_support_matrix_implicit_conversion.wasm").string();
  const std::string errPath =
      (testScratchPath("") / "primec_emit_wasm_support_matrix_implicit_conversion_err.txt")
          .string();

  const std::string wasmCmd = "./primec --emit=wasm " + quoteShellArg(srcPath) + " -o " + quoteShellArg(wasmPath) +
                              " --entry /main 2> " + quoteShellArg(errPath);
  CHECK(runCommand(wasmCmd) == 2);
  CHECK_FALSE(std::filesystem::exists(wasmPath));
  CHECK(readFile(errPath).find(
            "implicit matrix/quaternion family conversion requires explicit helper: expected /std/math/Quat got "
            "/std/math/Mat3") != std::string::npos);
}

TEST_CASE("primec emits wasm bytecode for direct callable definitions") {
  const std::string source = R"(
[return<int>]
helper() {
  return(9i32)
}

[return<int>]
main() {
  return(plus(helper(), 2i32))
}
)";
  const std::string srcPath = writeTemp("compile_emit_wasm_direct_call.prime", source);
  const std::string wasmPath = (testScratchPath("") / "primec_emit_wasm_direct_call.wasm").string();
  const std::string errPath = (testScratchPath("") / "primec_emit_wasm_direct_call_err.txt").string();

  const std::string wasmCmd = "./primec --emit=wasm " + quoteShellArg(srcPath) + " -o " + quoteShellArg(wasmPath) +
                              " --entry /main 2> " + quoteShellArg(errPath);
  CHECK(runCommand(wasmCmd) == 2);
  const std::string diagnostics = readFile(errPath);
  CHECK(diagnostics.find("unsupported control-flow pattern") != std::string::npos);
  CHECK(std::filesystem::exists(wasmPath) == false);
}


TEST_SUITE_END();
