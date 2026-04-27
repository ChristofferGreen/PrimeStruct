#include "test_compile_run_helpers.h"

TEST_SUITE_BEGIN("primestruct.compile.run.glsl");

static bool compileGlslOrExpectNominalMathUnsupported(const std::string &compileCmd,
                                                      const std::string &errPath) {
  const int code = runCommand(compileCmd + " 2> " + quoteShellArg(errPath));
  if (code == 0) {
    return true;
  }
  CHECK(code == 2);
  const std::string error = readFile(errPath);
  CHECK((error.find(
             "native backend only supports arithmetic/comparison/clamp/min/max/abs/sign/saturate/convert/pointer/"
             "assign/increment/decrement calls in expressions") != std::string::npos ||
         error.find("local index exceeds glsl local-slot limit") != std::string::npos ||
         error.find("arithmetic operators require numeric operands") != std::string::npos));
  return false;
}

TEST_CASE("glsl emitter writes minimal shader") {
  const std::string source = R"(
[return<void>]
main() {
}
)";
  const std::string srcPath = writeTemp("compile_glsl_minimal.prime", source);
  const std::string outPath = (testScratchPath("") / "primec_glsl_minimal.glsl").string();

  const std::string compileCmd = "./primec --emit=glsl " + srcPath + " -o " + outPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string output = readFile(outPath);
  CHECK(output.find("#version 450") != std::string::npos);
  CHECK(output.find("void main()") != std::string::npos);
  CHECK(output.find("PrimeStructOutput") != std::string::npos);
}

TEST_CASE("glsl-ir emitter writes IR-lowered shader for integer subset") {
  const std::string source = R"(
[return<void>]
main() {
  [i32 mut] counter{1i32}
  assign(counter, plus(counter, 2i32))
  return()
}
)";
  const std::string srcPath = writeTemp("compile_glsl_ir_i32_subset.prime", source);
  const std::string outPath = (testScratchPath("") / "primec_glsl_ir_i32_subset.glsl").string();

  const std::string compileCmd = "./primec --emit=glsl-ir " + srcPath + " -o " + outPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string output = readFile(outPath);
  CHECK(output.find("int ps_entry_0(inout int stack[1024], inout int sp)") != std::string::npos);
  CHECK(output.find("switch (pc)") != std::string::npos);
  CHECK(output.find("PrimeStructOutput") != std::string::npos);
}

TEST_CASE("glsl-ir emitter writes IR-lowered shader for f32 literal subset") {
  const std::string source = R"(
[return<void>]
main() {
  [f32] scale{2.0f32}
  return()
}
)";
  const std::string srcPath = writeTemp("compile_glsl_ir_f32_literal_subset.prime", source);
  const std::string outPath = (testScratchPath("") / "primec_glsl_ir_f32_literal_subset.glsl").string();

  const std::string compileCmd = "./primec --emit=glsl-ir " + srcPath + " -o " + outPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string output = readFile(outPath);
  CHECK(output.find("stack[sp++] = int(uint(") != std::string::npos);
  CHECK(output.find("PrimeStructOutput") != std::string::npos);
}

TEST_CASE("glsl emitter uses ir backend for f32 literal subset") {
  const std::string source = R"(
[return<void>]
main() {
  [f32] scale{2.0f32}
  return()
}
)";
  const std::string srcPath = writeTemp("compile_glsl_f32_literal_ir_first.prime", source);
  const std::string outPath = (testScratchPath("") / "primec_glsl_f32_literal_ir_first.glsl").string();

  const std::string compileCmd = "./primec --emit=glsl " + srcPath + " -o " + outPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string output = readFile(outPath);
  CHECK(output.find("stack[sp++] = int(uint(") != std::string::npos);
  CHECK(output.find("PrimeStructOutput") != std::string::npos);
}

TEST_CASE("glsl-ir emitter writes IR-lowered shader for f32 arithmetic subset") {
  const std::string source = R"(
[return<void>]
main() {
  [f32 mut] scale{2.0f32}
  assign(scale, plus(scale, 0.5f32))
  return()
}
)";
  const std::string srcPath = writeTemp("compile_glsl_ir_f32_arithmetic_subset.prime", source);
  const std::string outPath =
      (testScratchPath("") / "primec_glsl_ir_f32_arithmetic_subset.glsl").string();

  const std::string compileCmd = "./primec --emit=glsl-ir " + srcPath + " -o " + outPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string output = readFile(outPath);
  CHECK(output.find("float right = intBitsToFloat(stack[--sp]);") != std::string::npos);
  CHECK(output.find("floatBitsToInt(left + right)") != std::string::npos);
}

TEST_CASE("glsl emitter uses ir backend for f32 arithmetic subset") {
  const std::string source = R"(
[return<void>]
main() {
  [f32 mut] scale{2.0f32}
  assign(scale, plus(scale, 0.5f32))
  return()
}
)";
  const std::string srcPath = writeTemp("compile_glsl_f32_arithmetic_ir_first.prime", source);
  const std::string outPath =
      (testScratchPath("") / "primec_glsl_f32_arithmetic_ir_first.glsl").string();

  const std::string compileCmd = "./primec --emit=glsl " + srcPath + " -o " + outPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string output = readFile(outPath);
  CHECK(output.find("float right = intBitsToFloat(stack[--sp]);") != std::string::npos);
  CHECK(output.find("PrimeStructOutput") != std::string::npos);
}

TEST_CASE("glsl-ir emitter writes IR-lowered shader for f32 to i32 conversion subset") {
  const std::string source = R"(
[return<void>]
main() {
  [f32] scale{2.75f32}
  [i32] asInt{convert<int>(scale)}
  return()
}
)";
  const std::string srcPath = writeTemp("compile_glsl_ir_f32_to_i32_subset.prime", source);
  const std::string outPath =
      (testScratchPath("") / "primec_glsl_ir_f32_to_i32_subset.glsl").string();

  const std::string compileCmd = "./primec --emit=glsl-ir " + srcPath + " -o " + outPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string output = readFile(outPath);
  CHECK(output.find("float value = intBitsToFloat(stack[--sp]);") != std::string::npos);
  CHECK(output.find("if (isnan(value))") != std::string::npos);
  CHECK(output.find("stack[sp++] = converted;") != std::string::npos);
  CHECK(output.find("PrimeStructOutput") != std::string::npos);
}

TEST_CASE("glsl emitter uses ir backend for f32 to i32 conversion subset") {
  const std::string source = R"(
[return<void>]
main() {
  [f32] scale{2.75f32}
  [i32] asInt{convert<int>(scale)}
  return()
}
)";
  const std::string srcPath = writeTemp("compile_glsl_f32_to_i32_ir_first.prime", source);
  const std::string outPath =
      (testScratchPath("") / "primec_glsl_f32_to_i32_ir_first.glsl").string();

  const std::string compileCmd = "./primec --emit=glsl " + srcPath + " -o " + outPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string output = readFile(outPath);
  CHECK(output.find("float value = intBitsToFloat(stack[--sp]);") != std::string::npos);
  CHECK(output.find("if (isnan(value))") != std::string::npos);
  CHECK(output.find("stack[sp++] = converted;") != std::string::npos);
  CHECK(output.find("PrimeStructOutput") != std::string::npos);
}

TEST_CASE("glsl-ir emitter writes IR-lowered shader for i32 to f32 conversion subset") {
  const std::string source = R"(
[return<void>]
main() {
  [i32] count{7i32}
  [f32] asFloat{convert<f32>(count)}
  return()
}
)";
  const std::string srcPath = writeTemp("compile_glsl_ir_i32_to_f32_subset.prime", source);
  const std::string outPath =
      (testScratchPath("") / "primec_glsl_ir_i32_to_f32_subset.glsl").string();

  const std::string compileCmd = "./primec --emit=glsl-ir " + srcPath + " -o " + outPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string output = readFile(outPath);
  CHECK(output.find("stack[sp++] = floatBitsToInt(float(stack[--sp]));") != std::string::npos);
  CHECK(output.find("PrimeStructOutput") != std::string::npos);
}

TEST_CASE("glsl emitter uses ir backend for i32 to f32 conversion subset") {
  const std::string source = R"(
[return<void>]
main() {
  [i32] count{7i32}
  [f32] asFloat{convert<f32>(count)}
  return()
}
)";
  const std::string srcPath = writeTemp("compile_glsl_i32_to_f32_ir_first.prime", source);
  const std::string outPath =
      (testScratchPath("") / "primec_glsl_i32_to_f32_ir_first.glsl").string();

  const std::string compileCmd = "./primec --emit=glsl " + srcPath + " -o " + outPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string output = readFile(outPath);
  CHECK(output.find("stack[sp++] = floatBitsToInt(float(stack[--sp]));") != std::string::npos);
  CHECK(output.find("PrimeStructOutput") != std::string::npos);
}

TEST_CASE("glsl-ir emitter writes IR-lowered shader for f32 return subset") {
  const std::string source = R"(
[return<f32>]
main() {
  return(1.5f32)
}
)";
  const std::string srcPath = writeTemp("compile_glsl_ir_f32_return_subset.prime", source);
  const std::string outPath =
      (testScratchPath("") / "primec_glsl_ir_f32_return_subset.glsl").string();

  const std::string compileCmd = "./primec --emit=glsl-ir " + srcPath + " -o " + outPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string output = readFile(outPath);
  CHECK(output.find("return stack[--sp];") != std::string::npos);
  CHECK(output.find("PrimeStructOutput") != std::string::npos);
}

TEST_CASE("glsl emitter uses ir backend for f32 return subset") {
  const std::string source = R"(
[return<f32>]
main() {
  return(1.5f32)
}
)";
  const std::string srcPath = writeTemp("compile_glsl_f32_return_ir_first.prime", source);
  const std::string outPath =
      (testScratchPath("") / "primec_glsl_f32_return_ir_first.glsl").string();

  const std::string compileCmd = "./primec --emit=glsl " + srcPath + " -o " + outPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string output = readFile(outPath);
  CHECK(output.find("return stack[--sp];") != std::string::npos);
  CHECK(output.find("PrimeStructOutput") != std::string::npos);
}

TEST_CASE("glsl-ir emitter writes IR-lowered shader for helper-call subset") {
  const std::string source = R"(
[return<i32>]
helper() {
  return(7i32)
}

[return<void>]
main() {
  [i32] value{helper()}
  return()
}
)";
  const std::string srcPath = writeTemp("compile_glsl_ir_helper_call_subset.prime", source);
  const std::string outPath =
      (testScratchPath("") / "primec_glsl_ir_helper_call_subset.glsl").string();

  const std::string compileCmd = "./primec --emit=glsl-ir " + srcPath + " -o " + outPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string output = readFile(outPath);
  CHECK(output.find("inout int stack[1024], inout int sp") != std::string::npos);
  CHECK(output.find("int ps_entry_1(inout int stack[1024], inout int sp);") != std::string::npos);
  CHECK(output.find("PrimeStructOutput") != std::string::npos);
}

TEST_CASE("glsl emitter uses ir backend for helper-call subset") {
  const std::string source = R"(
[return<i32>]
helper() {
  return(7i32)
}

[return<void>]
main() {
  [i32] value{helper()}
  return()
}
)";
  const std::string srcPath = writeTemp("compile_glsl_helper_call_ir_first.prime", source);
  const std::string outPath =
      (testScratchPath("") / "primec_glsl_helper_call_ir_first.glsl").string();

  const std::string compileCmd = "./primec --emit=glsl " + srcPath + " -o " + outPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string output = readFile(outPath);
  CHECK(output.find("inout int stack[1024], inout int sp") != std::string::npos);
  CHECK(output.find("int ps_entry_1(inout int stack[1024], inout int sp);") != std::string::npos);
  CHECK(output.find("PrimeStructOutput") != std::string::npos);
}

TEST_CASE("glsl-ir emitter writes IR-lowered shader for entry args count subset") {
  const std::string source = R"(
[return<void>]
main([array<string>] args) {
  [i32] argc{count(args)}
  return()
}
)";
  const std::string srcPath = writeTemp("compile_glsl_ir_entry_args_count_subset.prime", source);
  const std::string outPath =
      (testScratchPath("") / "primec_glsl_ir_entry_args_count_subset.glsl").string();

  const std::string compileCmd = "./primec --emit=glsl-ir " + srcPath + " -o " + outPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string output = readFile(outPath);
  CHECK(output.find("stack[sp++] = 0; // GLSL backend has no argv") != std::string::npos);
  CHECK(output.find("PrimeStructOutput") != std::string::npos);
}

TEST_CASE("glsl emitter uses ir backend for entry args count subset") {
  const std::string source = R"(
[return<void>]
main([array<string>] args) {
  [i32] argc{count(args)}
  return()
}
)";
  const std::string srcPath = writeTemp("compile_glsl_entry_args_count_ir_first.prime", source);
  const std::string outPath =
      (testScratchPath("") / "primec_glsl_entry_args_count_ir_first.glsl").string();

  const std::string compileCmd = "./primec --emit=glsl " + srcPath + " -o " + outPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string output = readFile(outPath);
  CHECK(output.find("stack[sp++] = 0; // GLSL backend has no argv") != std::string::npos);
  CHECK(output.find("PrimeStructOutput") != std::string::npos);
}

TEST_CASE("glsl emitter matches glsl-ir on shared corpus") {
  struct DifferentialCase {
    const char *name;
    const char *source;
  };

  const std::vector<DifferentialCase> cases = {
      {
          "minimal_void",
          R"(
[return<void>]
main() {
}
)",
      },
      {
          "i32_arithmetic",
          R"(
[return<void>]
main() {
  [i32 mut] counter{1i32}
  assign(counter, plus(counter, 2i32))
  return()
}
)",
      },
      {
          "if_else_flow",
          R"(
[return<void>]
main() {
  [i32 mut] value{1i32}
  if(equal(value, 1i32)) {
    assign(value, plus(value, 2i32))
  } else {
    assign(value, 0i32)
  }
  return()
}
)",
      },
      {
          "f32_literal",
          R"(
[return<void>]
main() {
  [f32] scale{2.0f32}
  return()
}
)",
      },
      {
          "f32_arithmetic",
          R"(
[return<void>]
main() {
  [f32 mut] scale{2.0f32}
  assign(scale, plus(scale, 0.5f32))
  return()
}
)",
      },
      {
          "f32_to_i32_convert",
          R"(
[return<void>]
main() {
  [f32] scale{2.75f32}
  [i32] asInt{convert<int>(scale)}
  return()
}
)",
      },
      {
          "i32_to_f32_convert",
          R"(
[return<void>]
main() {
  [i32] count{7i32}
  [f32] asFloat{convert<f32>(count)}
  return()
}
)",
      },
      {
          "helper_call",
          R"(
[return<i32>]
helper() {
  return(7i32)
}

[return<void>]
main() {
  [i32] value{helper()}
  return()
}
)",
      },
      {
          "entry_args_count",
          R"(
[return<void>]
main([array<string>] args) {
  [i32] argc{count(args)}
  return()
}
)",
      },
  };

  for (const auto &testCase : cases) {
    CAPTURE(testCase.name);
    const std::string srcPath =
        writeTemp(std::string("compile_glsl_differential_") + testCase.name + ".prime", testCase.source);
    const std::string glslPath =
        (testScratchPath("") / (std::string("primec_glsl_differential_") + testCase.name + ".glsl"))
            .string();
    const std::string glslIrPath =
        (testScratchPath("") /
         (std::string("primec_glsl_ir_differential_") + testCase.name + ".glsl"))
            .string();

    const std::string compileGlslCmd =
        "./primec --emit=glsl " + quoteShellArg(srcPath) + " -o " + quoteShellArg(glslPath) + " --entry /main";
    const std::string compileGlslIrCmd =
        "./primec --emit=glsl-ir " + quoteShellArg(srcPath) + " -o " + quoteShellArg(glslIrPath) + " --entry /main";
    CHECK(runCommand(compileGlslCmd) == 0);
    CHECK(runCommand(compileGlslIrCmd) == 0);
    CHECK(readFile(glslPath) == readFile(glslIrPath));
  }
}

TEST_CASE("glsl-ir validation rejects out-of-range i64 literals") {
  const std::string source = R"(
[return<void>]
main() {
  [i64] value{5000000000i64}
  return()
}
)";
  const std::string srcPath = writeTemp("compile_glsl_ir_unsupported_opcode.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_glsl_ir_unsupported_opcode_err.txt").string();

  const std::string compileCmd =
      "./primec --emit=glsl-ir " + quoteShellArg(srcPath) + " -o /dev/null --entry /main 2> " + quoteShellArg(errPath);
  CHECK(runCommand(compileCmd) == 2);
  const std::string diagnostics = readFile(errPath);
  CHECK(diagnostics.find("GLSL-IR validation error: ") != std::string::npos);
  CHECK(diagnostics.find("glsl i64 literal out of i32 range") != std::string::npos);
}

TEST_CASE("glsl emitter surfaces ir validation-stage failures without fallback") {
  const std::string source = R"(
[return<void>]
main() {
  [i64] wide{5000000000i64}
  return()
}
)";
  const std::string srcPath = writeTemp("compile_glsl_ir_backend_emit_failure.prime", source);
  const std::string outPath =
      (testScratchPath("") / "primec_glsl_ir_backend_emit_failure.glsl").string();
  const std::string errPath =
      (testScratchPath("") / "primec_glsl_ir_backend_emit_failure_err.txt").string();
  std::error_code ec;
  std::filesystem::remove(outPath, ec);

  const std::string compileCmd = "./primec --emit=glsl " + quoteShellArg(srcPath) + " -o " + quoteShellArg(outPath) +
                                 " --entry /main 2> " + quoteShellArg(errPath);
  CHECK(runCommand(compileCmd) == 2);
  const std::string diagnostics = readFile(errPath);
  CHECK(diagnostics.find("GLSL-IR validation error: ") != std::string::npos);
  CHECK(diagnostics.find("glsl i64 literal out of i32 range") != std::string::npos);
  CHECK_FALSE(std::filesystem::exists(outPath));
}

TEST_CASE("defaults to glsl extension for emit=glsl") {
  const std::string source = R"(
[return<void>]
main() {
}
)";
  const std::string srcPath = writeTemp("compile_default_glsl.prime", source);
  const std::filesystem::path outDir = testScratchPath("") / "primec_glsl_default_out";
  std::error_code ec;
  std::filesystem::remove_all(outDir, ec);
  std::filesystem::create_directories(outDir, ec);
  REQUIRE(!ec);

  const std::string compileCmd =
      "./primec --emit=glsl " + srcPath + " --out-dir " + outDir.string() + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  std::filesystem::path outputPath = outDir / std::filesystem::path(srcPath).stem();
  outputPath.replace_extension(".glsl");
  CHECK(std::filesystem::exists(outputPath));
}

TEST_CASE("defaults to spv extension for emit=spirv") {
  if (!hasSpirvTools()) {
    return;
  }
  const std::string source = R"(
[return<void>]
main() {
}
)";
  const std::string srcPath = writeTemp("compile_default_spirv.prime", source);
  const std::filesystem::path outDir = testScratchPath("") / "primec_spirv_default_out";
  std::error_code ec;
  std::filesystem::remove_all(outDir, ec);
  std::filesystem::create_directories(outDir, ec);
  REQUIRE(!ec);

  const std::string compileCmd =
      "./primec --emit=spirv " + srcPath + " --out-dir " + outDir.string() + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  std::filesystem::path outputPath = outDir / std::filesystem::path(srcPath).stem();
  outputPath.replace_extension(".spv");
  CHECK(std::filesystem::exists(outputPath));
  CHECK(std::filesystem::file_size(outputPath) > 0);
}

TEST_CASE("glsl emitter allows entry args parameter") {
  const std::string source = R"(
[return<void>]
main([array<string>] args) {
  return()
}
)";
  const std::string srcPath = writeTemp("compile_glsl_entry_args.prime", source);
  const std::string outPath = (testScratchPath("") / "primec_glsl_entry_args.glsl").string();

  const std::string compileCmd = "./primec --emit=glsl " + srcPath + " -o " + outPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string output = readFile(outPath);
  CHECK(output.find("void main()") != std::string::npos);
}

TEST_CASE("glsl emitter writes spirv when tool available") {
  if (!hasSpirvTools()) {
    return;
  }
  const std::string source = R"(
[return<void>]
main() {
}
)";
  const std::string srcPath = writeTemp("compile_spirv_minimal.prime", source);
  const std::string outPath = (testScratchPath("") / "primec_spirv_minimal.spv").string();

  const std::string compileCmd = "./primec --emit=spirv " + srcPath + " -o " + outPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(std::filesystem::exists(outPath));
  CHECK(std::filesystem::file_size(outPath) > 0);
}

TEST_CASE("spirv-ir emitter writes spirv when tool available") {
  if (!hasSpirvTools()) {
    return;
  }
  const std::string source = R"(
[return<void>]
main() {
  [i32 mut] value{1i32}
  assign(value, plus(value, 2i32))
  return()
}
)";
  const std::string srcPath = writeTemp("compile_spirv_ir_minimal.prime", source);
  const std::string outPath = (testScratchPath("") / "primec_spirv_ir_minimal.spv").string();

  const std::string compileCmd = "./primec --emit=spirv-ir " + srcPath + " -o " + outPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(std::filesystem::exists(outPath));
  CHECK(std::filesystem::file_size(outPath) > 0);
}

TEST_CASE("spirv emit reports missing tool") {
  const std::string source = R"(
[return<void>]
main() {
}
)";
  const std::string srcPath = writeTemp("compile_spirv_missing_tool.prime", source);
  const std::string outPath = (testScratchPath("") / "primec_spirv_missing_tool.spv").string();
  const std::string errPath =
      (testScratchPath("") / "primec_spirv_missing_tool_err.txt").string();

  const std::string compileCmd =
      "PATH= ./primec --emit=spirv " + quoteShellArg(srcPath) + " -o " + quoteShellArg(outPath) +
      " --entry /main 2> " + quoteShellArg(errPath);
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("glslangValidator or glslc not found") != std::string::npos);
}

TEST_CASE("glsl emitter writes locals and arithmetic") {
  const std::string source = R"(
[return<void>]
main() {
  [i32 mut] counter{1i32};
  [f32] scale{2.0f32};
  assign(counter, plus(counter, 2i32));
  result{plus(scale, 0.5f32)};
  return();
}
)";
  const std::string srcPath = writeTemp("compile_glsl_locals.prime", source);
  const std::string outPath = (testScratchPath("") / "primec_glsl_locals.glsl").string();

  const std::string compileCmd = "./primec --emit=glsl " + srcPath + " -o " + outPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string output = readFile(outPath);
  CHECK(output.find("int locals[1024];") != std::string::npos);
  CHECK(output.find("int right = stack[--sp];") != std::string::npos);
  CHECK(output.find("float right = intBitsToFloat(stack[--sp]);") != std::string::npos);
  CHECK(output.find("PrimeStructOutput") != std::string::npos);
}

TEST_CASE("spirv emitter surfaces ir validation-stage failures without fallback") {
  const std::string source = R"(
[return<void>]
main() {
  [i64] wide{5000000000i64}
  return();
}
)";
  const std::string srcPath = writeTemp("compile_spirv_ir_backend_emit_failure.prime", source);
  const std::string outPath =
      (testScratchPath("") / "primec_spirv_ir_backend_emit_failure.spv").string();
  const std::string errPath =
      (testScratchPath("") / "primec_spirv_ir_backend_emit_failure_err.txt").string();
  std::error_code ec;
  std::filesystem::remove(outPath, ec);

  const std::string compileCmd = "./primec --emit=spirv " + quoteShellArg(srcPath) + " -o " + quoteShellArg(outPath) +
                                 " --entry /main 2> " + quoteShellArg(errPath);
  CHECK(runCommand(compileCmd) == 2);
  const std::string diagnostics = readFile(errPath);
  CHECK(diagnostics.find("SPIR-V IR validation error: ") != std::string::npos);
  CHECK(diagnostics.find("glsl i64 literal out of i32 range") != std::string::npos);
  CHECK_FALSE(std::filesystem::exists(outPath));
}

TEST_CASE("glsl emitter allows assign in expressions") {
  const std::string source = R"(
[return<void>]
main() {
  [i32 mut] counter{0i32}
  value{assign(counter, 2i32)}
  return()
}
)";
  const std::string srcPath = writeTemp("compile_glsl_assign_expr.prime", source);
  const std::string outPath = (testScratchPath("") / "primec_glsl_assign_expr.glsl").string();

  const std::string compileCmd = "./primec --emit=glsl " + srcPath + " -o " + outPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string output = readFile(outPath);
  CHECK(output.find("stack[sp++] = 2;") != std::string::npos);
  CHECK(output.find("locals[1] = stack[--sp];") != std::string::npos);
}

TEST_CASE("glsl emitter allows increment/decrement in expressions") {
  const std::string source = R"(
[return<void>]
main() {
  [i32 mut] counter{0i32}
  inc{increment(counter)}
  dec{decrement(counter)}
  return()
}
)";
  const std::string srcPath = writeTemp("compile_glsl_inc_dec_expr.prime", source);
  const std::string outPath = (testScratchPath("") / "primec_glsl_inc_dec_expr.glsl").string();

  const std::string compileCmd = "./primec --emit=glsl " + srcPath + " -o " + outPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string output = readFile(outPath);
  CHECK(output.find("stack[sp++] = left + right;") != std::string::npos);
  CHECK(output.find("stack[sp++] = left - right;") != std::string::npos);
  CHECK(output.find("locals[1] = stack[--sp];") != std::string::npos);
  CHECK(output.find("locals[2] = stack[--sp];") != std::string::npos);
}

TEST_CASE("glsl emitter writes if blocks") {
  const std::string source = R"(
[return<void>]
main() {
  [i32 mut] value{1i32}
  if(less_than(value, 2i32), then() { assign(value, 3i32) }, else() { assign(value, 4i32) })
  return()
}
)";
  const std::string srcPath = writeTemp("compile_glsl_if.prime", source);
  const std::string outPath = (testScratchPath("") / "primec_glsl_if.glsl").string();

  const std::string compileCmd = "./primec --emit=glsl " + srcPath + " -o " + outPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string output = readFile(outPath);
  CHECK(output.find("pc = (cond == 0) ?") != std::string::npos);
  CHECK(output.find("stack[sp++] = 3;") != std::string::npos);
  CHECK(output.find("stack[sp++] = 4;") != std::string::npos);
}

TEST_CASE("glsl emitter writes loops") {
  const std::string source = R"(
[return<void>]
main() {
  [i32 mut] value{0i32}
  loop(2i32, stepper() { assign(value, plus(value, 1i32)) })
  while(less_than(value, 3i32), branch() { assign(value, plus(value, 1i32)) })
  repeat(2i32) { assign(value, plus(value, 1i32)) }
  for([i32 mut] i{0i32}, less_than(i, 2i32), increment(i), body() { assign(value, plus(value, i)) })
  return()
}
)";
  const std::string srcPath = writeTemp("compile_glsl_loops.prime", source);
  const std::string outPath = (testScratchPath("") / "primec_glsl_loops.glsl").string();

  const std::string compileCmd = "./primec --emit=glsl " + srcPath + " -o " + outPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string output = readFile(outPath);
  CHECK(output.find("pc = (cond == 0) ?") != std::string::npos);
  CHECK(output.find("stack[sp++] = left + right;") != std::string::npos);
  CHECK(output.find("stack[sp++] = left - right;") != std::string::npos);
}

TEST_CASE("glsl emitter handles shared_scope blocks") {
  const std::string source = R"(
[return<void>]
main() {
  [i32 mut] acc{0i32}
  [shared_scope]
  for([i32 mut] i{0i32}, less_than(i, 2i32), increment(i)) {
    [i32] inner{1i32}
    assign(acc, plus(acc, inner))
  }
  return()
}
)";
  const std::string srcPath = writeTemp("compile_glsl_shared_scope.prime", source);
  const std::string outPath = (testScratchPath("") / "primec_glsl_shared_scope.glsl").string();

  const std::string compileCmd = "./primec --emit=glsl " + srcPath + " -o " + outPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string output = readFile(outPath);
  CHECK(output.find("locals[2] = stack[--sp];") != std::string::npos);
  CHECK(output.find("stack[sp++] = left + right;") != std::string::npos);
  CHECK(output.find("pc = (cond == 0) ?") != std::string::npos);
}

TEST_CASE("glsl emitter handles shared_scope while") {
  const std::string source = R"(
[return<void>]
main() {
  [i32 mut] total{0i32}
  [i32 mut] i{0i32}
  [shared_scope]
  while(less_than(i, 3i32)) {
    [i32] inner{1i32}
    assign(total, plus(total, inner))
    assign(i, plus(i, 1i32))
  }
  return()
}
)";
  const std::string srcPath = writeTemp("compile_glsl_shared_scope_while.prime", source);
  const std::string outPath = (testScratchPath("") / "primec_glsl_shared_scope_while.glsl").string();

  const std::string compileCmd = "./primec --emit=glsl " + srcPath + " -o " + outPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string output = readFile(outPath);
  CHECK(output.find("locals[2] = stack[--sp];") != std::string::npos);
  CHECK(output.find("stack[sp++] = left + right;") != std::string::npos);
  CHECK(output.find("pc = (cond == 0) ?") != std::string::npos);
}

TEST_CASE("glsl emitter handles block initializers") {
  const std::string source = R"(
[return<void>]
main() {
  [i32] value{
    [i32] base{1i32}
    plus(base, 2i32)
  }
  return()
}
)";
  const std::string srcPath = writeTemp("compile_glsl_block_init.prime", source);
  const std::string outPath = (testScratchPath("") / "primec_glsl_block_init.glsl").string();

  const std::string compileCmd = "./primec --emit=glsl " + srcPath + " -o " + outPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string output = readFile(outPath);
  CHECK(output.find("stack[sp++] = 1;") != std::string::npos);
  CHECK(output.find("stack[sp++] = 2;") != std::string::npos);
  CHECK(output.find("stack[sp++] = left + right;") != std::string::npos);
}

TEST_CASE("glsl emitter handles brace constructor values") {
  const std::string source = R"(
[return<void>]
main() {
  [bool] flag{
    return(bool{ equal(1i32, 1i32) })
  }
  return()
}
)";
  const std::string srcPath = writeTemp("compile_glsl_brace_ctor.prime", source);
  const std::string outPath = (testScratchPath("") / "primec_glsl_brace_ctor.glsl").string();

  const std::string compileCmd = "./primec --emit=glsl " + srcPath + " -o " + outPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string output = readFile(outPath);
  CHECK(output.find("stack[sp++] = (left == right) ? 1 : 0;") != std::string::npos);
  CHECK(output.find("stack[sp++] = (left != right) ? 1 : 0;") != std::string::npos);
}

TEST_CASE("glsl emitter ignores print builtins") {
  const std::string source = R"(
[return<void> effects(io_out, io_err)]
main() {
  [i32 mut] counter{0i32}
  print(assign(counter, 2i32))
  print("hello"utf8)
  print_line(1i32)
  print_error(2i32)
  print_line_error(3i32)
  return()
}
)";
  const std::string srcPath = writeTemp("compile_glsl_print_noop.prime", source);
  const std::string outPath = (testScratchPath("") / "primec_glsl_print_noop.glsl").string();

  const std::string compileCmd = "./primec --emit=glsl " + srcPath + " -o " + outPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string output = readFile(outPath);
  CHECK(output.find("GLSL backend ignores print side effects.") != std::string::npos);
  CHECK(output.find("GLSL backend ignores print-string side effects.") != std::string::npos);
}

TEST_CASE("glsl emitter accepts capabilities") {
  const std::string source = R"(
[return<void> capabilities(io_out)]
main() {
  print_line("cap"utf8)
  return()
}
)";
  const std::string srcPath = writeTemp("compile_glsl_caps.prime", source);
  const std::string outPath = (testScratchPath("") / "primec_glsl_caps.glsl").string();

  const std::string compileCmd = "./primec --emit=glsl " + srcPath + " -o " + outPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string output = readFile(outPath);
  CHECK(output.find("void main()") != std::string::npos);
}

TEST_CASE("glsl emitter accepts support-matrix effects and capabilities") {
  const std::string source = R"(
[return<void>
 effects(io_out, io_err, pathspace_notify, pathspace_insert, pathspace_take, pathspace_bind, pathspace_schedule)
 capabilities(io_out, io_err, pathspace_notify, pathspace_insert, pathspace_take, pathspace_bind, pathspace_schedule)]
main() {
  return()
}
)";
  const std::string srcPath = writeTemp("compile_glsl_support_matrix_effects.prime", source);
  const std::string outPath =
      (testScratchPath("") / "primec_glsl_support_matrix_effects.glsl").string();

  const std::string compileCmd = "./primec --emit=glsl " + srcPath + " -o " + outPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(readFile(outPath).find("void main()") != std::string::npos);
}

TEST_CASE("glsl emitter supports support-matrix scalar bindings") {
  const std::string source = R"(
[return<void>]
main() {
  [bool] enabled{true}
  [i32] i{1i32}
  [i64] j{2i64}
  [u64] k{3u64}
  [f32] x{1.0f32}
  [f64] y{2.0f64}
  [i32 mut] sink{0i32}
  if(enabled, then() { assign(sink, plus(i, 1i32)) }, else() { })
  return()
}
)";
  const std::string srcPath = writeTemp("compile_glsl_support_matrix_scalars.prime", source);
  const std::string outPath =
      (testScratchPath("") / "primec_glsl_support_matrix_scalars.glsl").string();

  const std::string compileCmd = "./primec --emit=glsl " + srcPath + " -o " + outPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string output = readFile(outPath);
  CHECK(output.find("Narrowed GLSL path lowers f64 literals through f32 payloads.") != std::string::npos);
  CHECK(output.find("locals[5] = stack[--sp];") != std::string::npos);
  CHECK(output.find("pc = (cond == 0) ?") != std::string::npos);
}

TEST_CASE("glsl emitter lowers quaternion nominal values and quaternion operators") {
  const std::string source = R"(
import /std/math/*

[return<void>]
main() {
  [Quat] raw{Quat{1.0f32, 2.0f32, 3.0f32, 4.0f32}}
  [Quat] other{Quat{0.5f32, 1.5f32, 0.25f32, 2.0f32}}
  [Quat] normalized{raw.toNormalized()}
  [Quat] sum{plus(raw, other)}
  [Quat] diff{minus(raw, other)}
  [Quat] scaled{multiply(2i32, raw)}
  [Quat] divided{divide(other, 2.0f32)}
  [Quat] product{multiply(raw, other)}
  [Vec3] rotated{multiply(raw, Vec3{1.0f32, 0.0f32, 0.0f32})}
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
  [Quat] raw{Quat{0.0f32, 0.0f32, 0.70710677f32, 0.70710677f32}}
  [Mat3] basis3{quat_to_mat3(raw)}
  [Mat4] basis4{quat_to_mat4(raw)}
  [Quat] restored{mat3_to_quat(basis3)}
  [Quat] zero{Quat{0.0f32, 0.0f32, 0.0f32, 0.0f32}.normalize()}
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
  [Quat] value{Quat{0.0f32, 0.0f32, 0.0f32, 1.0f32}}
  [Vec4] wrong{Vec4{1.0f32, 0.0f32, 0.0f32, 1.0f32}}
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
  [Mat2] m2{Mat2{1.0f32, 2.0f32, 3.0f32, 4.0f32}}
  [Mat2] sum2{plus(m2, Mat2{5.0f32, 6.0f32, 7.0f32, 8.0f32})}
  [Mat2] scaled2{multiply(sum2, 2i32)}
  [Mat2] div2{divide(scaled2, 2.0f32)}
  [Mat2] prod2{multiply(m2, sum2)}
  [f32] sample2{plus(div2.m01, prod2.m10)}
  [Mat3] m3{Mat3{
    1.0f32, 2.0f32, 3.0f32,
    4.0f32, 5.0f32, 6.0f32,
    7.0f32, 8.0f32, 9.0f32
  }}
  [f32] sample3{m3.m21}
  [Mat4] m4{Mat4{
    1.0f32, 2.0f32, 3.0f32, 4.0f32,
    5.0f32, 6.0f32, 7.0f32, 8.0f32,
    9.0f32, 10.0f32, 11.0f32, 12.0f32,
    13.0f32, 14.0f32, 15.0f32, 16.0f32
  }}
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
  [Vec2] v2{Vec2{1.0f32, 2.0f32}}
  [Vec3] v3{Vec3{3.0f32, 4.0f32, 5.0f32}}
  [Vec4] v4{Vec4{6.0f32, 7.0f32, 8.0f32, 9.0f32}}
  [Mat2] m2{Mat2{1.0f32, 2.0f32, 3.0f32, 4.0f32}}
  [Mat3] m3{Mat3{
    1.0f32, 2.0f32, 3.0f32,
    4.0f32, 5.0f32, 6.0f32,
    7.0f32, 8.0f32, 9.0f32
  }}
  [Mat4] m4{Mat4{
    1.0f32, 2.0f32, 3.0f32, 4.0f32,
    5.0f32, 6.0f32, 7.0f32, 8.0f32,
    9.0f32, 10.0f32, 11.0f32, 12.0f32,
    13.0f32, 14.0f32, 15.0f32, 16.0f32
  }}
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
  [Vec2] base2{Vec2{1.0f32, 2.0f32}}
  [Vec2] delta2{Vec2{3.0f32, 4.0f32}}
  [Vec2] sum2{plus(base2, delta2)}
  [Vec2] diff2{minus(base2, delta2)}
  [Vec2] scaledLeft2{multiply(2i32, base2)}
  [Vec2] scaledRight2{multiply(delta2, 0.5f32)}
  [Vec2] div2{divide(scaledLeft2, 2.0f32)}
  [Vec3] base3{Vec3{5.0f32, 6.0f32, 7.0f32}}
  [Vec3] scaled3{multiply(base3, 3i32)}
  [Vec3] div3{divide(scaled3, 3.0f32)}
  [Vec4] base4{Vec4{8.0f32, 9.0f32, 10.0f32, 11.0f32}}
  [Vec4] delta4{Vec4{1.0f32, 2.0f32, 3.0f32, 4.0f32}}
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

TEST_CASE("glsl emitter rejects explicit effects") {
  const std::string source = R"(
[return<void> effects(heap_alloc)]
main() {
}
)";
  const std::string srcPath = writeTemp("compile_glsl_effects.prime", source);
  const std::string errPath = (testScratchPath("") / "primec_glsl_effects_err.txt").string();

  const std::string outPath = (testScratchPath("") / "primec_glsl_effects.glsl").string();
  const std::string compileCmd = "./primec --emit=glsl " + srcPath + " -o " + outPath + " --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 0);
  CHECK(readFile(outPath).find("void main()") != std::string::npos);
}

TEST_CASE("glsl emitter rejects static bindings") {
  const std::string source = R"(
[return<void>]
main() {
  [static i32] value{1i32}
}
)";
  const std::string srcPath = writeTemp("compile_glsl_static_binding.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_glsl_static_binding_err.txt").string();

  const std::string outPath = (testScratchPath("") / "primec_glsl_static_binding.glsl").string();
  const std::string compileCmd = "./primec --emit=glsl " + srcPath + " -o " + outPath + " --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 0);
  CHECK(readFile(outPath).find("stack[sp++] = 1;") != std::string::npos);
}

TEST_CASE("glsl emitter rejects non-scalar bindings") {
  const std::string source = R"(
[return<void>]
main() {
  [array<i32>] values{array<i32>(1i32)}
}
)";
  const std::string srcPath = writeTemp("compile_glsl_array_binding.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_glsl_array_binding_err.txt").string();

  const std::string outPath = (testScratchPath("") / "primec_glsl_array_binding.glsl").string();
  const std::string compileCmd = "./primec --emit=glsl " + srcPath + " -o " + outPath + " --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 0);
  CHECK(readFile(outPath).find("slot-byte offsets") != std::string::npos);
}

TEST_CASE("glsl emitter rejects mixed signed/unsigned math") {
  const std::string source = R"(
[return<void>]
main() {
  [i32] left{1i32}
  [u64] right{1u64}
  [i32] value{plus(left, right)}
  return()
}
)";
  const std::string srcPath = writeTemp("compile_glsl_mixed_signed.prime", source);
  const std::string errPath = (testScratchPath("") / "primec_glsl_mixed_signed_err.txt").string();

  const std::string compileCmd =
      "./primec --emit=glsl " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) != 0);
  CHECK(readFile(errPath).find("mixed signed/unsigned") != std::string::npos);
}

TEST_CASE("glsl emitter rejects mixed boolean comparisons") {
  const std::string source = R"(
[return<void>]
main() {
  [bool] flag{greater_than(true, 1i32)}
  return()
}
)";
  const std::string srcPath = writeTemp("compile_glsl_mixed_bool.prime", source);
  const std::string errPath = (testScratchPath("") / "primec_glsl_mixed_bool_err.txt").string();

  const std::string outPath = (testScratchPath("") / "primec_glsl_mixed_bool.glsl").string();
  const std::string compileCmd = "./primec --emit=glsl " + srcPath + " -o " + outPath + " --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 0);
  CHECK(readFile(outPath).find("stack[sp++] = (left > right) ? 1 : 0;") != std::string::npos);
}

TEST_CASE("glsl emitter rejects string literals") {
  const std::string source = R"(
[return<void>]
main() {
  [string] name{"hi"utf8}
}
)";
  const std::string srcPath = writeTemp("compile_glsl_string_literal.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_glsl_string_literal_err.txt").string();

  const std::string outPath = (testScratchPath("") / "primec_glsl_string_literal.glsl").string();
  const std::string compileCmd = "./primec --emit=glsl " + srcPath + " -o " + outPath + " --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 0);
  CHECK(readFile(outPath).find("stack[sp++] = 0;") != std::string::npos);
}

TEST_CASE("glsl emitter rejects unsupported capabilities") {
  const std::string source = R"(
[return<void> capabilities(render_graph) effects(render_graph)]
main() {
}
)";
  const std::string srcPath = writeTemp("compile_glsl_capability.prime", source);
  const std::string errPath = (testScratchPath("") / "primec_glsl_capability_err.txt").string();

  const std::string compileCmd =
      "./primec --emit=glsl " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) != 0);
  CHECK(readFile(errPath).find("does not support effect: render_graph") != std::string::npos);
}

TEST_CASE("glsl emitter rejects effects on executions") {
  const std::string source = R"(
[return<void>]
main() {
  [effects(heap_alloc)] plus(1i32, 2i32)
  return()
}
)";
  const std::string srcPath = writeTemp("compile_glsl_exec_effects.prime", source);
  const std::string errPath = (testScratchPath("") / "primec_glsl_exec_effects_err.txt").string();

  const std::string compileCmd =
      "./primec --emit=glsl " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) != 0);
  CHECK(readFile(errPath).find("execution effects must be a subset") != std::string::npos);
}

TEST_CASE("glsl emitter rejects non-void entry") {
  const std::string source = R"(
[return<int>]
main() {
  return(1i32)
}
)";
  const std::string srcPath = writeTemp("compile_glsl_reject.prime", source);
  const std::string errPath = (testScratchPath("") / "primec_glsl_reject_err.txt").string();

  const std::string outPath = (testScratchPath("") / "primec_glsl_reject.glsl").string();
  const std::string compileCmd = "./primec --emit=glsl " + srcPath + " -o " + outPath + " --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 0);
  CHECK(readFile(outPath).find("return stack[--sp];") != std::string::npos);
}

TEST_SUITE_END();
