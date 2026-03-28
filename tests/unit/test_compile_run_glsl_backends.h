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
