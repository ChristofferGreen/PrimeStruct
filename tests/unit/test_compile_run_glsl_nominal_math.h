TEST_CASE("glsl emitter lowers quaternion nominal values and quaternion operators") {
  const std::string source = R"(
import /std/math/*

[return<void>]
main() {
  [Quat] raw{Quat(1.0f32, 2.0f32, 3.0f32, 4.0f32)}
  [Quat] other{Quat(0.5f32, 1.5f32, 0.25f32, 2.0f32)}
  [Quat] normalized{raw.toNormalized()}
  [Quat] sum{plus(raw, other)}
  [Quat] diff{minus(raw, other)}
  [Quat] scaled{multiply(2i32, raw)}
  [Quat] divided{divide(other, 2.0f32)}
  [Quat] product{multiply(raw, other)}
  [Vec3] rotated{multiply(raw, Vec3(1.0f32, 0.0f32, 0.0f32))}
  [f32] sample{plus(normalized.x, plus(sum.w, plus(diff.y, plus(scaled.z, plus(divided.w, plus(product.x, rotated.y))))))}
  [i32 mut] sink{0i32}
  if(true, then() { assign(sink, convert<int>(sample)) }, else() { })
  return()
}
)";
  const std::string srcPath = writeTemp("compile_glsl_support_quaternion_nominal_values.prime", source);
  const std::string outPath =
      (testScratchPath("") / "primec_glsl_support_quaternion_nominal_values.glsl").string();
  const std::string errPath =
      (testScratchPath("") / "primec_glsl_support_quaternion_nominal_values.err").string();

  const std::string compileCmd = "./primec --emit=glsl " + quoteShellArg(srcPath) + " -o " + quoteShellArg(outPath) +
                                 " --entry /main";
  if (!compileGlslOrExpectNominalMathUnsupported(compileCmd, errPath)) {
    return;
  }
  const std::string output = readFile(outPath);
  CHECK(output.find("const vec4 raw = vec4(1.0, 2.0, 3.0, 4.0);") != std::string::npos);
  CHECK(output.find("const vec4 other = vec4(0.5, 1.5, 0.25, 2.0);") != std::string::npos);
  CHECK(output.find("_ps_quat_norm_") != std::string::npos);
  CHECK(output.find("dot(_ps_quat_") != std::string::npos);
  CHECK(output.find("const vec4 sum = (raw + other);") != std::string::npos);
  CHECK(output.find("const vec4 diff = (raw - other);") != std::string::npos);
  CHECK(output.find("const vec4 scaled = (float(2) * raw);") != std::string::npos);
  CHECK(output.find("const vec4 divided = (other / 2.0);") != std::string::npos);
  CHECK(output.find("const vec4 product = vec4(") != std::string::npos);
  CHECK(output.find("(raw).w * (other).x") != std::string::npos);
  CHECK(output.find("(raw).w * (other).w") != std::string::npos);
  CHECK(output.find("const vec3 rotated = (") != std::string::npos);
  CHECK(output.find("* vec3(1.0, 0.0, 0.0)") != std::string::npos);
  CHECK(output.find("(normalized).x") != std::string::npos);
  CHECK(output.find("(sum).w") != std::string::npos);
  CHECK(output.find("(product).x") != std::string::npos);
  CHECK(output.find("(rotated).y") != std::string::npos);
}

TEST_CASE("glsl emitter lowers quaternion conversion helpers") {
  const std::string source = R"(
import /std/math/*

[return<void>]
main() {
  [Quat] raw{Quat(0.0f32, 0.0f32, 0.70710677f32, 0.70710677f32)}
  [Mat3] basis3{quat_to_mat3(raw)}
  [Mat4] basis4{quat_to_mat4(raw)}
  [Quat] restored{mat3_to_quat(basis3)}
  [Quat] zero{Quat(0.0f32, 0.0f32, 0.0f32, 0.0f32).normalize()}
  [f32] sample{plus(basis3.m01, plus(basis4.m33, plus(restored.w, zero.x)))}
  [i32 mut] sink{0i32}
  if(true, then() { assign(sink, convert<int>(sample)) }, else() { })
  return()
}
)";
  const std::string srcPath = writeTemp("compile_glsl_support_quaternion_helpers.prime", source);
  const std::string outPath =
      (testScratchPath("") / "primec_glsl_support_quaternion_helpers.glsl").string();
  const std::string errPath =
      (testScratchPath("") / "primec_glsl_support_quaternion_helpers.err").string();

  const std::string compileCmd = "./primec --emit=glsl " + quoteShellArg(srcPath) + " -o " + quoteShellArg(outPath) +
                                 " --entry /main";
  if (!compileGlslOrExpectNominalMathUnsupported(compileCmd, errPath)) {
    return;
  }
  const std::string output = readFile(outPath);
  CHECK(output.find("const vec4 raw = vec4(0.0, 0.0, 0.70710677, 0.70710677);") != std::string::npos);
  CHECK(output.find("const mat3 basis3 = mat3(") != std::string::npos);
  CHECK(output.find("const mat4 basis4 = mat4(") != std::string::npos);
  CHECK(output.find("_ps_mat3_trace_") != std::string::npos);
  CHECK(output.find("_ps_mat3_to_quat_raw_") != std::string::npos);
  CHECK(output.find("const vec4 restored = _ps_mat3_to_quat_") != std::string::npos);
  CHECK(output.find("const vec4 zero = _ps_quat_norm_") != std::string::npos);
  CHECK(output.find("const float sample = ") != std::string::npos);
  CHECK(output.find("(basis3)[1][0]") != std::string::npos);
  CHECK(output.find("(basis4)[3][3]") != std::string::npos);
  CHECK(output.find("(restored).w") != std::string::npos);
  CHECK(output.find("(zero).x") != std::string::npos);
}

TEST_CASE("glsl emitter surfaces quaternion shape diagnostics") {
  const std::string source = R"(
import /std/math/*

[return<void>]
main() {
  [Quat] value{Quat(0.0f32, 0.0f32, 0.0f32, 1.0f32)}
  [Vec4] wrong{Vec4(1.0f32, 0.0f32, 0.0f32, 1.0f32)}
  [auto] bad{multiply(value, wrong)}
  return()
}
)";
  const std::string srcPath = writeTemp("compile_glsl_support_quaternion_shape_reject.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_glsl_support_quaternion_shape_reject.err").string();

  const std::string compileCmd =
      "./primec --emit=glsl " + quoteShellArg(srcPath) + " -o /dev/null --entry /main 2> " + quoteShellArg(errPath);
  CHECK(runCommand(compileCmd) != 0);
  CHECK(readFile(errPath).find(
            "multiply requires scalar scaling, matrix-vector, matrix-matrix, quaternion-quaternion, or quaternion-Vec3 operands") !=
        std::string::npos);
}

TEST_CASE("glsl emitter lowers matrix nominal values field access and matrix operators") {
  const std::string source = R"(
import /std/math/*

[return<void>]
main() {
  [Mat2] m2{Mat2(1.0f32, 2.0f32, 3.0f32, 4.0f32)}
  [Mat2] sum2{plus(m2, Mat2(5.0f32, 6.0f32, 7.0f32, 8.0f32))}
  [Mat2] scaled2{multiply(sum2, 2i32)}
  [Mat2] div2{divide(scaled2, 2.0f32)}
  [Mat2] prod2{multiply(m2, sum2)}
  [f32] sample2{plus(div2.m01, prod2.m10)}
  [Mat3] m3{Mat3(
    1.0f32, 2.0f32, 3.0f32,
    4.0f32, 5.0f32, 6.0f32,
    7.0f32, 8.0f32, 9.0f32
  )}
  [f32] sample3{m3.m21}
  [Mat4] m4{Mat4(
    1.0f32, 2.0f32, 3.0f32, 4.0f32,
    5.0f32, 6.0f32, 7.0f32, 8.0f32,
    9.0f32, 10.0f32, 11.0f32, 12.0f32,
    13.0f32, 14.0f32, 15.0f32, 16.0f32
  )}
  [f32] sample4{m4.m32}
  [i32 mut] sink{0i32}
  if(true, then() { assign(sink, convert<int>(plus(sample2, plus(sample3, sample4)))) }, else() { })
  return()
}
)";
  const std::string srcPath = writeTemp("compile_glsl_support_matrix_nominal_values.prime", source);
  const std::string outPath =
      (testScratchPath("") / "primec_glsl_support_matrix_nominal_values.glsl").string();
  const std::string errPath =
      (testScratchPath("") / "primec_glsl_support_matrix_nominal_values.err").string();

  const std::string compileCmd = "./primec --emit=glsl " + srcPath + " -o " + outPath + " --entry /main";
  if (!compileGlslOrExpectNominalMathUnsupported(compileCmd, errPath)) {
    return;
  }
  const std::string output = readFile(outPath);
  CHECK(output.find("const mat2 m2 = mat2(1.0, 3.0, 2.0, 4.0);") != std::string::npos);
  CHECK(output.find("const mat2 sum2 = (m2 + mat2(5.0, 7.0, 6.0, 8.0));") != std::string::npos);
  CHECK(output.find("const mat2 scaled2 = (sum2 * float(2));") != std::string::npos);
  CHECK(output.find("const mat2 div2 = (scaled2 / 2.0);") != std::string::npos);
  CHECK(output.find("const mat2 prod2 = (m2 * sum2);") != std::string::npos);
  CHECK(output.find("const float sample2 = ") != std::string::npos);
  CHECK(output.find("(div2)[1][0]") != std::string::npos);
  CHECK(output.find("(prod2)[0][1]") != std::string::npos);
  CHECK(output.find("const mat3 m3 = mat3(1.0, 4.0, 7.0, 2.0, 5.0, 8.0, 3.0, 6.0, 9.0);") != std::string::npos);
  CHECK(output.find("const float sample3 = (m3)[1][2];") != std::string::npos);
  CHECK(output.find("const mat4 m4 = mat4(1.0, 5.0, 9.0, 13.0, 2.0, 6.0, 10.0, 14.0, 3.0, 7.0, 11.0, 15.0, 4.0, 8.0, 12.0, 16.0);") != std::string::npos);
  CHECK(output.find("const float sample4 = (m4)[2][3];") != std::string::npos);
}

TEST_CASE("glsl emitter accepts vector nominal values and matrix vector multiply") {
  const std::string source = R"(
import /std/math/*

[return<void>]
main() {
  [Vec2] v2{Vec2(1.0f32, 2.0f32)}
  [Vec3] v3{Vec3(3.0f32, 4.0f32, 5.0f32)}
  [Vec4] v4{Vec4(6.0f32, 7.0f32, 8.0f32, 9.0f32)}
  [Mat2] m2{Mat2(1.0f32, 2.0f32, 3.0f32, 4.0f32)}
  [Mat3] m3{Mat3(
    1.0f32, 2.0f32, 3.0f32,
    4.0f32, 5.0f32, 6.0f32,
    7.0f32, 8.0f32, 9.0f32
  )}
  [Mat4] m4{Mat4(
    1.0f32, 2.0f32, 3.0f32, 4.0f32,
    5.0f32, 6.0f32, 7.0f32, 8.0f32,
    9.0f32, 10.0f32, 11.0f32, 12.0f32,
    13.0f32, 14.0f32, 15.0f32, 16.0f32
  )}
  [Vec2] out2{multiply(m2, v2)}
  [Vec3] out3{multiply(m3, v3)}
  [Vec4] out4{multiply(m4, v4)}
  [f32] sample{plus(out2.y, plus(out3.z, out4.w))}
  [i32 mut] sink{0i32}
  if(true, then() { assign(sink, convert<int>(sample)) }, else() { })
  return()
}
)";
  const std::string srcPath = writeTemp("compile_glsl_support_matrix_vector_values.prime", source);
  const std::string outPath =
      (testScratchPath("") / "primec_glsl_support_matrix_vector_values.glsl").string();
  const std::string errPath =
      (testScratchPath("") / "primec_glsl_support_matrix_vector_values.err").string();

  const std::string compileCmd = "./primec --emit=glsl " + srcPath + " -o " + outPath + " --entry /main";
  if (!compileGlslOrExpectNominalMathUnsupported(compileCmd, errPath)) {
    return;
  }
  const std::string output = readFile(outPath);
  if (output.find("const vec2 v2 = vec2(1.0, 2.0);") != std::string::npos) {
    CHECK(output.find("const vec3 v3 = vec3(3.0, 4.0, 5.0);") != std::string::npos);
    CHECK(output.find("const vec4 v4 = vec4(6.0, 7.0, 8.0, 9.0);") != std::string::npos);
    CHECK(output.find("const vec2 out2 = (m2 * v2);") != std::string::npos);
    CHECK(output.find("const vec3 out3 = (m3 * v3);") != std::string::npos);
    CHECK(output.find("const vec4 out4 = (m4 * v4);") != std::string::npos);
    CHECK(output.find("const float sample = ") != std::string::npos);
    CHECK(output.find("(out2).y") != std::string::npos);
    CHECK(output.find("(out3).z") != std::string::npos);
    CHECK(output.find("(out4).w") != std::string::npos);
    return;
  }
  CHECK(output.find("PrimeStructOutput") != std::string::npos);
  CHECK(output.find("int ps_entry_0(inout int stack[1024], inout int sp)") != std::string::npos);
  CHECK(output.find("GLSL backend lowers local addresses to deterministic slot-byte offsets.") !=
        std::string::npos);
}

TEST_CASE("glsl emitter lowers documented vector arithmetic operators") {
  const std::string source = R"(
import /std/math/*

[return<void>]
main() {
  [Vec2] base2{Vec2(1.0f32, 2.0f32)}
  [Vec2] delta2{Vec2(3.0f32, 4.0f32)}
  [Vec2] sum2{plus(base2, delta2)}
  [Vec2] diff2{minus(base2, delta2)}
  [Vec2] scaledLeft2{multiply(2i32, base2)}
  [Vec2] scaledRight2{multiply(delta2, 0.5f32)}
  [Vec2] div2{divide(scaledLeft2, 2.0f32)}
  [Vec3] base3{Vec3(5.0f32, 6.0f32, 7.0f32)}
  [Vec3] scaled3{multiply(base3, 3i32)}
  [Vec3] div3{divide(scaled3, 3.0f32)}
  [Vec4] base4{Vec4(8.0f32, 9.0f32, 10.0f32, 11.0f32)}
  [Vec4] delta4{Vec4(1.0f32, 2.0f32, 3.0f32, 4.0f32)}
  [Vec4] sum4{plus(base4, delta4)}
  [Vec4] diff4{minus(base4, delta4)}
  [f32] sample{plus(sum2.x, plus(diff2.y, plus(scaledRight2.x, plus(div2.y, plus(div3.z, plus(sum4.w, diff4.z))))))}
  [i32 mut] sink{0i32}
  if(true, then() { assign(sink, convert<int>(sample)) }, else() { })
  return()
}
)";
  const std::string srcPath = writeTemp("compile_glsl_support_vector_arithmetic.prime", source);
  const std::string outPath =
      (testScratchPath("") / "primec_glsl_support_vector_arithmetic.glsl").string();
  const std::string errPath =
      (testScratchPath("") / "primec_glsl_support_vector_arithmetic.err").string();

  const std::string compileCmd = "./primec --emit=glsl " + srcPath + " -o " + outPath + " --entry /main";
  if (!compileGlslOrExpectNominalMathUnsupported(compileCmd, errPath)) {
    return;
  }
  const std::string output = readFile(outPath);
  CHECK(output.find("const vec2 sum2 = (base2 + delta2);") != std::string::npos);
  CHECK(output.find("const vec2 diff2 = (base2 - delta2);") != std::string::npos);
  CHECK(output.find("const vec2 scaledLeft2 = (float(2) * base2);") != std::string::npos);
  CHECK(output.find("const vec2 scaledRight2 = (delta2 * 0.5);") != std::string::npos);
  CHECK(output.find("const vec2 div2 = (scaledLeft2 / 2.0);") != std::string::npos);
  CHECK(output.find("const vec3 scaled3 = (base3 * float(3));") != std::string::npos);
  CHECK(output.find("const vec3 div3 = (scaled3 / 3.0);") != std::string::npos);
  CHECK(output.find("const vec4 sum4 = (base4 + delta4);") != std::string::npos);
  CHECK(output.find("const vec4 diff4 = (base4 - delta4);") != std::string::npos);
  CHECK(output.find("const float sample = ") != std::string::npos);
  CHECK(output.find("(sum2).x") != std::string::npos);
  CHECK(output.find("(diff2).y") != std::string::npos);
  CHECK(output.find("(scaledRight2).x") != std::string::npos);
  CHECK(output.find("(div2).y") != std::string::npos);
  CHECK(output.find("(div3).z") != std::string::npos);
  CHECK(output.find("(sum4).w") != std::string::npos);
  CHECK(output.find("(diff4).z") != std::string::npos);
}

TEST_CASE("glsl emitter handles math constants") {
  const std::string source = R"(
[return<void>]
main() {
  [f64] pi_val{/std/math/pi}
  [f64] tau_val{/std/math/tau}
  [f64] e_val{/std/math/e}
  return()
}
)";
  const std::string srcPath = writeTemp("compile_glsl_math_constants.prime", source);
  const std::string outPath = (testScratchPath("") / "primec_glsl_math_constants.glsl").string();

  const std::string compileCmd = "./primec --emit=glsl " + srcPath + " -o " + outPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string output = readFile(outPath);
  CHECK(output.find("Narrowed GLSL path lowers f64 literals through f32 payloads.") != std::string::npos);
  CHECK(output.find("1078530011u") != std::string::npos);
  CHECK(output.find("1086918619u") != std::string::npos);
  CHECK(output.find("1076754516u") != std::string::npos);
}

TEST_CASE("glsl emitter writes integer pow helper") {
  const std::string source = R"(
import /std/math/*
[return<void>]
main() {
  [i32] value{pow(2i32, 3i32)}
  return()
}
)";
  const std::string srcPath = writeTemp("compile_glsl_int_pow.prime", source);
  const std::string outPath = (testScratchPath("") / "primec_glsl_int_pow.glsl").string();

  const std::string compileCmd = "./primec --emit=glsl " + srcPath + " -o " + outPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string output = readFile(outPath);
  CHECK(output.find("stack[sp++] = left * right;") != std::string::npos);
  CHECK(output.find("stack[sp++] = left - right;") != std::string::npos);
  CHECK(output.find("pc = 13;") != std::string::npos);
}

TEST_CASE("glsl emitter handles block expressions in arguments") {
  const std::string source = R"(
[return<void>]
main() {
  [i32] value{plus(block() { [i32] base{1i32} base }, 2i32)}
  return()
}
)";
  const std::string srcPath = writeTemp("compile_glsl_block_expr.prime", source);
  const std::string outPath = (testScratchPath("") / "primec_glsl_block_expr.glsl").string();

  const std::string compileCmd = "./primec --emit=glsl " + srcPath + " -o " + outPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string output = readFile(outPath);
  CHECK(output.find("stack[sp++] = left + right;") != std::string::npos);
  CHECK(output.find("locals[1] = stack[--sp];") != std::string::npos);
}

TEST_CASE("glsl emitter handles block expression return value") {
  const std::string source = R"(
[return<void>]
main() {
  [i32] value{block(){
    [i32] base{1i32}
    return(plus(base, 2i32))
  }}
  return()
}
)";
  const std::string srcPath = writeTemp("compile_glsl_block_return.prime", source);
  const std::string outPath = (testScratchPath("") / "primec_glsl_block_return.glsl").string();

  const std::string compileCmd = "./primec --emit=glsl " + srcPath + " -o " + outPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string output = readFile(outPath);
  CHECK(output.find("int value") != std::string::npos);
}

TEST_CASE("glsl emitter handles block expression early return") {
  const std::string source = R"(
[return<void>]
main() {
  [i32] value{block(){
    return(3i32)
    4i32
  }}
  return()
}
)";
  const std::string srcPath = writeTemp("compile_glsl_block_early_return.prime", source);
  const std::string outPath =
      (testScratchPath("") / "primec_glsl_block_early_return.glsl").string();

  const std::string compileCmd = "./primec --emit=glsl " + srcPath + " -o " + outPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string output = readFile(outPath);
  CHECK(output.find("int value") != std::string::npos);
}

TEST_CASE("glsl emitter ignores pathspace builtins") {
  const std::string source = R"(
[return<void> effects(pathspace_notify, pathspace_insert, pathspace_take, pathspace_bind, pathspace_schedule)]
main() {
  notify("/events/test"utf8, 1i32)
  insert("/events/test"utf8, 2i32)
  take("/events/test"utf8)
  bind("/events/test"utf8, 3i32)
  unbind("/events/test"utf8)
  schedule("/events/test"utf8, 4i32)
  return()
}
)";
  const std::string srcPath = writeTemp("compile_glsl_pathspace.prime", source);
  const std::string outPath = (testScratchPath("") / "primec_glsl_pathspace.glsl").string();

  const std::string compileCmd = "./primec --emit=glsl " + srcPath + " -o " + outPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string output = readFile(outPath);
  CHECK(output.find("void main") != std::string::npos);
  CHECK(output.find("stack[sp++] = 1;") != std::string::npos);
  CHECK(output.find("--sp;") != std::string::npos);
}

TEST_CASE("glsl emitter writes math builtins") {
  const std::string source = R"(
import /std/math/*
[return<void>]
main() {
  [f32] value{floor(1.9f32)}
  [f32] clamped{clamp(2.5f32, 0.0f32, 1.0f32)}
  [f32] powed{pow(2.0f32, 3.0f32)}
  [bool] flag{is_nan(divide(0.0f32, 0.0f32))}
  return()
}
)";
  const std::string srcPath = writeTemp("compile_glsl_math.prime", source);
  const std::string outPath = (testScratchPath("") / "primec_glsl_math.glsl").string();

  const std::string compileCmd = "./primec --emit=glsl " + srcPath + " -o " + outPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string output = readFile(outPath);
  CHECK(output.find("isnan(value)") != std::string::npos);
  CHECK(output.find("floatBitsToInt(left * right)") != std::string::npos);
  CHECK(output.find("stack[sp++] = (left < right) ? 1 : 0;") != std::string::npos);
  CHECK(output.find("isnan(") != std::string::npos);
}
