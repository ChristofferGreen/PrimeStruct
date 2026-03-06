TEST_SUITE_BEGIN("primestruct.compile.run.glsl");

TEST_CASE("glsl emitter writes minimal shader") {
  const std::string source = R"(
[return<void>]
main() {
}
)";
  const std::string srcPath = writeTemp("compile_glsl_minimal.prime", source);
  const std::string outPath = (std::filesystem::temp_directory_path() / "primec_glsl_minimal.glsl").string();

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
  const std::string outPath = (std::filesystem::temp_directory_path() / "primec_glsl_ir_i32_subset.glsl").string();

  const std::string compileCmd = "./primec --emit=glsl-ir " + srcPath + " -o " + outPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string output = readFile(outPath);
  CHECK(output.find("int ps_entry_0()") != std::string::npos);
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
  const std::string outPath = (std::filesystem::temp_directory_path() / "primec_glsl_ir_f32_literal_subset.glsl").string();

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
  const std::string outPath = (std::filesystem::temp_directory_path() / "primec_glsl_f32_literal_ir_first.glsl").string();

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
      (std::filesystem::temp_directory_path() / "primec_glsl_ir_f32_arithmetic_subset.glsl").string();

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
      (std::filesystem::temp_directory_path() / "primec_glsl_f32_arithmetic_ir_first.glsl").string();

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
      (std::filesystem::temp_directory_path() / "primec_glsl_ir_f32_to_i32_subset.glsl").string();

  const std::string compileCmd = "./primec --emit=glsl-ir " + srcPath + " -o " + outPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string output = readFile(outPath);
  CHECK(output.find("stack[sp++] = int(intBitsToFloat(stack[--sp]));") != std::string::npos);
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
      (std::filesystem::temp_directory_path() / "primec_glsl_f32_to_i32_ir_first.glsl").string();

  const std::string compileCmd = "./primec --emit=glsl " + srcPath + " -o " + outPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string output = readFile(outPath);
  CHECK(output.find("stack[sp++] = int(intBitsToFloat(stack[--sp]));") != std::string::npos);
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
  if(equals(value, 1i32)) {
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
  };

  for (const auto &testCase : cases) {
    CAPTURE(testCase.name);
    const std::string srcPath =
        writeTemp(std::string("compile_glsl_differential_") + testCase.name + ".prime", testCase.source);
    const std::string glslPath =
        (std::filesystem::temp_directory_path() / (std::string("primec_glsl_differential_") + testCase.name + ".glsl"))
            .string();
    const std::string glslIrPath =
        (std::filesystem::temp_directory_path() /
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

TEST_CASE("glsl-ir emitter reports unsupported opcodes") {
  const std::string source = R"(
[return<void>]
main() {
  [f64] value{1.5f64}
  return()
}
)";
  const std::string srcPath = writeTemp("compile_glsl_ir_unsupported_opcode.prime", source);
  const std::string errPath =
      (std::filesystem::temp_directory_path() / "primec_glsl_ir_unsupported_opcode_err.txt").string();

  const std::string compileCmd =
      "./primec --emit=glsl-ir " + quoteShellArg(srcPath) + " -o /dev/null --entry /main 2> " + quoteShellArg(errPath);
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("ir-to-glsl failed: IrToGlslEmitter unsupported opcode") != std::string::npos);
}

TEST_CASE("glsl emitter falls back to legacy output on ir emit-stage failures") {
  const std::string source = R"(
[return<void>]
main() {
  [f32] asFloat{convert<f32>(1i32)}
  return()
}
)";
  const std::string srcPath = writeTemp("compile_glsl_ir_backend_fallback_legacy.prime", source);
  const std::string outPath =
      (std::filesystem::temp_directory_path() / "primec_glsl_ir_backend_fallback_legacy.glsl").string();

  const std::string compileCmd = "./primec --emit=glsl " + quoteShellArg(srcPath) + " -o " + quoteShellArg(outPath) +
                                 " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string output = readFile(outPath);
  CHECK(output.find("float asFloat") != std::string::npos);
  CHECK(output.find("PrimeStructOutput") == std::string::npos);
}

TEST_CASE("defaults to glsl extension for emit=glsl") {
  const std::string source = R"(
[return<void>]
main() {
}
)";
  const std::string srcPath = writeTemp("compile_default_glsl.prime", source);
  const std::filesystem::path outDir = std::filesystem::temp_directory_path() / "primec_glsl_default_out";
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
  const std::filesystem::path outDir = std::filesystem::temp_directory_path() / "primec_spirv_default_out";
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
  const std::string outPath = (std::filesystem::temp_directory_path() / "primec_glsl_entry_args.glsl").string();

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
  const std::string outPath = (std::filesystem::temp_directory_path() / "primec_spirv_minimal.spv").string();

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
  const std::string outPath = (std::filesystem::temp_directory_path() / "primec_spirv_ir_minimal.spv").string();

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
  const std::string outPath = (std::filesystem::temp_directory_path() / "primec_spirv_missing_tool.spv").string();
  const std::string errPath =
      (std::filesystem::temp_directory_path() / "primec_spirv_missing_tool_err.txt").string();

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
  const std::string outPath = (std::filesystem::temp_directory_path() / "primec_glsl_locals.glsl").string();

  const std::string compileCmd = "./primec --emit=glsl " + srcPath + " -o " + outPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string output = readFile(outPath);
  CHECK(output.find("int counter") != std::string::npos);
  CHECK(output.find("float scale") != std::string::npos);
  CHECK(output.find("counter = (counter + 2)") != std::string::npos);
  CHECK(output.find("float result") != std::string::npos);
  CHECK(output.find("PrimeStructOutput") == std::string::npos);
}

TEST_CASE("spirv emitter falls back to legacy glsl on ir emit-stage failures") {
  if (!hasSpirvTools()) {
    return;
  }
  const std::string source = R"(
[return<void>]
main() {
  [f32] asFloat{convert<f32>(1i32)};
  return();
}
)";
  const std::string srcPath = writeTemp("compile_spirv_fallback_legacy_glsl.prime", source);
  const std::string outPath = (std::filesystem::temp_directory_path() / "primec_spirv_fallback_legacy_glsl.spv").string();

  const std::string compileCmd = "./primec --emit=spirv " + srcPath + " -o " + outPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(std::filesystem::exists(outPath));
  CHECK(std::filesystem::file_size(outPath) > 0);
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
  const std::string outPath = (std::filesystem::temp_directory_path() / "primec_glsl_assign_expr.glsl").string();

  const std::string compileCmd = "./primec --emit=glsl " + srcPath + " -o " + outPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string output = readFile(outPath);
  CHECK(output.find("int value") != std::string::npos);
  CHECK(output.find("counter = 2") != std::string::npos);
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
  const std::string outPath = (std::filesystem::temp_directory_path() / "primec_glsl_inc_dec_expr.glsl").string();

  const std::string compileCmd = "./primec --emit=glsl " + srcPath + " -o " + outPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string output = readFile(outPath);
  CHECK(output.find("int inc") != std::string::npos);
  CHECK(output.find("int dec") != std::string::npos);
  CHECK(output.find("++counter") != std::string::npos);
  CHECK(output.find("--counter") != std::string::npos);
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
  const std::string outPath = (std::filesystem::temp_directory_path() / "primec_glsl_if.glsl").string();

  const std::string compileCmd = "./primec --emit=glsl " + srcPath + " -o " + outPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string output = readFile(outPath);
  CHECK(output.find("if (") != std::string::npos);
  CHECK(output.find("value = 3") != std::string::npos);
  CHECK(output.find("value = 4") != std::string::npos);
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
  const std::string outPath = (std::filesystem::temp_directory_path() / "primec_glsl_loops.glsl").string();

  const std::string compileCmd = "./primec --emit=glsl " + srcPath + " -o " + outPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string output = readFile(outPath);
  CHECK(output.find("while (") != std::string::npos);
  CHECK(output.find("value = (value + 1)") != std::string::npos);
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
  const std::string outPath = (std::filesystem::temp_directory_path() / "primec_glsl_shared_scope.glsl").string();

  const std::string compileCmd = "./primec --emit=glsl " + srcPath + " -o " + outPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string output = readFile(outPath);
  CHECK(output.find("int acc") != std::string::npos);
  CHECK(output.find("int inner") != std::string::npos);
  CHECK(output.find("while (") != std::string::npos);
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
  const std::string outPath = (std::filesystem::temp_directory_path() / "primec_glsl_shared_scope_while.glsl").string();

  const std::string compileCmd = "./primec --emit=glsl " + srcPath + " -o " + outPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string output = readFile(outPath);
  CHECK(output.find("int total") != std::string::npos);
  CHECK(output.find("int inner") != std::string::npos);
  CHECK(output.find("while (") != std::string::npos);
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
  const std::string outPath = (std::filesystem::temp_directory_path() / "primec_glsl_block_init.glsl").string();

  const std::string compileCmd = "./primec --emit=glsl " + srcPath + " -o " + outPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string output = readFile(outPath);
  CHECK(output.find("int value") != std::string::npos);
  CHECK(output.find("int base") != std::string::npos);
  CHECK(output.find("value = (base + 2)") != std::string::npos);
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
  const std::string outPath = (std::filesystem::temp_directory_path() / "primec_glsl_brace_ctor.glsl").string();

  const std::string compileCmd = "./primec --emit=glsl " + srcPath + " -o " + outPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string output = readFile(outPath);
  CHECK(output.find("bool flag") != std::string::npos);
  CHECK(output.find("flag = _ps_block_") != std::string::npos);
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
  const std::string outPath = (std::filesystem::temp_directory_path() / "primec_glsl_print_noop.glsl").string();

  const std::string compileCmd = "./primec --emit=glsl " + srcPath + " -o " + outPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string output = readFile(outPath);
  CHECK(output.find("int counter") != std::string::npos);
  CHECK(output.find("counter = 2") != std::string::npos);
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
  const std::string outPath = (std::filesystem::temp_directory_path() / "primec_glsl_caps.glsl").string();

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
      (std::filesystem::temp_directory_path() / "primec_glsl_support_matrix_effects.glsl").string();

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
      (std::filesystem::temp_directory_path() / "primec_glsl_support_matrix_scalars.glsl").string();

  const std::string compileCmd = "./primec --emit=glsl " + srcPath + " -o " + outPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string output = readFile(outPath);
  CHECK(output.find("bool enabled") != std::string::npos);
  CHECK(output.find("int i") != std::string::npos);
  CHECK(output.find("int64_t j") != std::string::npos);
  CHECK(output.find("uint64_t k") != std::string::npos);
  CHECK(output.find("float x") != std::string::npos);
  CHECK(output.find("double y") != std::string::npos);
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
  const std::string outPath = (std::filesystem::temp_directory_path() / "primec_glsl_math_constants.glsl").string();

  const std::string compileCmd = "./primec --emit=glsl " + srcPath + " -o " + outPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string output = readFile(outPath);
  CHECK(output.find("#extension GL_ARB_gpu_shader_fp64 : require") != std::string::npos);
  CHECK(output.find("double pi_val") != std::string::npos);
  CHECK(output.find("double tau_val") != std::string::npos);
  CHECK(output.find("double e_val") != std::string::npos);
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
  const std::string outPath = (std::filesystem::temp_directory_path() / "primec_glsl_int_pow.glsl").string();

  const std::string compileCmd = "./primec --emit=glsl " + srcPath + " -o " + outPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string output = readFile(outPath);
  CHECK(output.find("ps_pow_i32") != std::string::npos);
  CHECK(output.find("int value") != std::string::npos);
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
  const std::string outPath = (std::filesystem::temp_directory_path() / "primec_glsl_block_expr.glsl").string();

  const std::string compileCmd = "./primec --emit=glsl " + srcPath + " -o " + outPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string output = readFile(outPath);
  CHECK(output.find("_ps_block_") != std::string::npos);
  CHECK(output.find("int value") != std::string::npos);
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
  const std::string outPath = (std::filesystem::temp_directory_path() / "primec_glsl_block_return.glsl").string();

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
      (std::filesystem::temp_directory_path() / "primec_glsl_block_early_return.glsl").string();

  const std::string compileCmd = "./primec --emit=glsl " + srcPath + " -o " + outPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string output = readFile(outPath);
  CHECK(output.find("int value") != std::string::npos);
}

TEST_CASE("glsl emitter ignores pathspace builtins") {
  const std::string source = R"(
[return<void> effects(pathspace_notify, pathspace_insert, pathspace_take, pathspace_bind, pathspace_schedule)]
main() {
  notify(array<string>("/events/test"utf8)[0i32], 1i32)
  insert(array<string>("/events/test"utf8)[0i32], 2i32)
  take(array<string>("/events/test"utf8)[0i32])
  bind(array<string>("/events/test"utf8)[0i32], 3i32)
  unbind(array<string>("/events/test"utf8)[0i32])
  schedule(array<string>("/events/test"utf8)[0i32], 4i32)
  return()
}
)";
  const std::string srcPath = writeTemp("compile_glsl_pathspace.prime", source);
  const std::string outPath = (std::filesystem::temp_directory_path() / "primec_glsl_pathspace.glsl").string();

  const std::string compileCmd = "./primec --emit=glsl " + srcPath + " -o " + outPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string output = readFile(outPath);
  CHECK(output.find("void main") != std::string::npos);
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
  const std::string outPath = (std::filesystem::temp_directory_path() / "primec_glsl_math.glsl").string();

  const std::string compileCmd = "./primec --emit=glsl " + srcPath + " -o " + outPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string output = readFile(outPath);
  CHECK(output.find("floor(") != std::string::npos);
  CHECK(output.find("clamp(") != std::string::npos);
  CHECK(output.find("pow(") != std::string::npos);
  CHECK(output.find("isnan(") != std::string::npos);
}

TEST_CASE("glsl emitter rejects explicit effects") {
  const std::string source = R"(
[return<void> effects(heap_alloc)]
main() {
}
)";
  const std::string srcPath = writeTemp("compile_glsl_effects.prime", source);
  const std::string errPath = (std::filesystem::temp_directory_path() / "primec_glsl_effects_err.txt").string();

  const std::string compileCmd =
      "./primec --emit=glsl " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) != 0);
  CHECK(readFile(errPath).find("glsl backend does not support effect") != std::string::npos);
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
      (std::filesystem::temp_directory_path() / "primec_glsl_static_binding_err.txt").string();

  const std::string compileCmd =
      "./primec --emit=glsl " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) != 0);
  CHECK(readFile(errPath).find("glsl backend does not support static bindings") != std::string::npos);
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
      (std::filesystem::temp_directory_path() / "primec_glsl_array_binding_err.txt").string();

  const std::string compileCmd =
      "./primec --emit=glsl " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) != 0);
  CHECK(readFile(errPath).find("glsl backend does not support builtin: array") != std::string::npos);
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
  const std::string errPath = (std::filesystem::temp_directory_path() / "primec_glsl_mixed_signed_err.txt").string();

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
  const std::string errPath = (std::filesystem::temp_directory_path() / "primec_glsl_mixed_bool_err.txt").string();

  const std::string compileCmd =
      "./primec --emit=glsl " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) != 0);
  CHECK(readFile(errPath).find("glsl backend does not allow mixed boolean/numeric comparisons") != std::string::npos);
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
      (std::filesystem::temp_directory_path() / "primec_glsl_string_literal_err.txt").string();

  const std::string compileCmd =
      "./primec --emit=glsl " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) != 0);
  CHECK(readFile(errPath).find("glsl backend does not support string literals") != std::string::npos);
}

TEST_CASE("glsl emitter rejects unsupported capabilities") {
  const std::string source = R"(
[return<void> capabilities(render_graph) effects(render_graph)]
main() {
}
)";
  const std::string srcPath = writeTemp("compile_glsl_capability.prime", source);
  const std::string errPath = (std::filesystem::temp_directory_path() / "primec_glsl_capability_err.txt").string();

  const std::string compileCmd =
      "./primec --emit=glsl " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) != 0);
  CHECK(readFile(errPath).find("glsl backend does not support capability") != std::string::npos);
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
  const std::string errPath = (std::filesystem::temp_directory_path() / "primec_glsl_exec_effects_err.txt").string();

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
  const std::string errPath = (std::filesystem::temp_directory_path() / "primec_glsl_reject_err.txt").string();

  const std::string compileCmd =
      "./primec --emit=glsl " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) != 0);
  CHECK(readFile(errPath).find("glsl backend") != std::string::npos);
}

TEST_SUITE_END();
