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
  loop(2i32, do() { assign(value, plus(value, 1i32)) })
  while(less_than(value, 3i32), do() { assign(value, plus(value, 1i32)) })
  repeat(2i32) { assign(value, plus(value, 1i32)) }
  for([i32 mut] i{0i32}, less_than(i, 2i32), increment(i), do() { assign(value, plus(value, i)) })
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

TEST_CASE("glsl emitter writes math builtins") {
  const std::string source = R"(
import /math/*
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
[return<void> effects(io_out)]
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
