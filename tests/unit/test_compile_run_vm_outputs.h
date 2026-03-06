TEST_CASE("writes serialized ir output") {
  const std::string source = R"(
[return<int>]
main() {
  return(3i32)
}
)";
  const std::string srcPath = writeTemp("compile_ir_simple.prime", source);
  const std::string irPath = (std::filesystem::temp_directory_path() / "primec_ir_simple.psir").string();
  const std::string compileCmd = "./primec --emit=ir " + srcPath + " -o " + irPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  std::ifstream file(irPath, std::ios::binary);
  REQUIRE(file.good());
  std::vector<uint8_t> data((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
  primec::IrModule module;
  std::string error;
  REQUIRE(primec::deserializeIr(data, module, error));
  CHECK(error.empty());
  CHECK(module.entryIndex == 0);
  REQUIRE(module.functions.size() == 1);
  CHECK(module.functions[0].name == "/main");
}

TEST_CASE("no transforms rejects sugar") {
  const std::string source = R"(
[return<int>]
main() {
  return(1i32+2i32)
}
)";
  const std::string srcPath = writeTemp("compile_no_transforms.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_no_transforms_exe").string();

  const std::string compileCmd = "./primec --emit=cpp --no-transforms " + srcPath + " -o " + exePath;
  CHECK(runCommand(compileCmd) != 0);
}

TEST_CASE("no transforms rejects implicit utf8") {
  const std::string source = R"(
[return<int> effects(io_out)]
main() {
  print_line("implicit")
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_no_transforms_implicit_utf8.prime", source);
  const std::string errPath =
      (std::filesystem::temp_directory_path() / "primec_no_transforms_implicit_utf8_err.txt").string();
  const std::string compileCmd =
      "./primec --emit=cpp --no-transforms " + srcPath + " -o /dev/null 2> " + errPath;
  CHECK(runCommand(compileCmd) != 0);
  CHECK(readFile(errPath).find("canonical string literal requires utf8/ascii suffix") != std::string::npos);
}

TEST_CASE("writes outputs under out dir") {
  const std::string source = R"(
[return<int>]
main() {
  return(1i32)
}
)";
  const std::string srcPath = writeTemp("compile_out_dir.prime", source);
  const std::filesystem::path outDir = std::filesystem::temp_directory_path() / "primec_out_dir_test";
  std::error_code ec;
  std::filesystem::remove_all(outDir, ec);
  const std::string outFile = "primec_out_dir.cpp";
  const std::string expectedPath = (outDir / outFile).string();
  const std::string compileCmd =
      "./primec --emit=cpp " + srcPath + " -o " + outFile + " --out-dir " + outDir.string();
  CHECK(runCommand(compileCmd) == 0);
  CHECK(std::filesystem::exists(expectedPath));
}

TEST_CASE("runs vm file io") {
  const std::string outPath = (std::filesystem::temp_directory_path() / "primec_vm_file_io.txt").string();
  auto escape = [](const std::string &text) {
    std::string out;
    out.reserve(text.size());
    for (char c : text) {
      if (c == '\\' || c == '"') {
        out.push_back('\\');
      }
      out.push_back(c);
    }
    return out;
  };
  const std::string escapedPath = escape(outPath);
  const std::string source =
      "[return<Result<FileError>> effects(file_write) on_error<FileError, /log_file_error>]\n"
      "main() {\n"
      "  [File<Write>] file{ File<Write>(\"" + escapedPath + "\"utf8)? }\n"
      "  [array<i32>] bytes{ array<i32>(65i32, 66i32, 67i32) }\n"
      "  file.write(\"Hello \"utf8, 123i32, \" world\"utf8)?\n"
      "  file.write_line(\"\"utf8)?\n"
      "  file.write_byte(10i32)?\n"
      "  file.write_bytes(bytes)?\n"
      "  file.flush()?\n"
      "  file.close()?\n"
      "  return(Result.ok())\n"
      "}\n"
      "[effects(io_err)]\n"
      "log_file_error([FileError] err) {\n"
      "  print_line_error(\"file error\"utf8)\n"
      "}\n";
  const std::string srcPath = writeTemp("vm_file_io.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(outPath) == "Hello 123 world\n\nABC");
}

TEST_CASE("defaults to cpp extension for emit=cpp") {
  const std::string source = R"(
[return<int>]
main() {
  return(2i32)
}
)";
  const std::string srcPath = writeTemp("compile_default_cpp.prime", source);
  const std::filesystem::path outDir = std::filesystem::temp_directory_path() / "primec_cpp_default_out";
  std::error_code ec;
  std::filesystem::remove_all(outDir, ec);
  std::filesystem::create_directories(outDir, ec);
  REQUIRE(!ec);

  const std::string compileCmd =
      "./primec --emit=cpp " + srcPath + " --out-dir " + outDir.string() + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  std::filesystem::path outputPath = outDir / std::filesystem::path(srcPath).stem();
  outputPath.replace_extension(".cpp");
  CHECK(std::filesystem::exists(outputPath));
}

TEST_CASE("defaults to cpp extension for emit=cpp-ir") {
  const std::string source = R"(
[return<int>]
main() {
  return(4i32)
}
)";
  const std::string srcPath = writeTemp("compile_default_cpp_ir.prime", source);
  const std::filesystem::path outDir = std::filesystem::temp_directory_path() / "primec_cpp_ir_default_out";
  std::error_code ec;
  std::filesystem::remove_all(outDir, ec);
  std::filesystem::create_directories(outDir, ec);
  REQUIRE(!ec);

  const std::string compileCmd =
      "./primec --emit=cpp-ir " + srcPath + " --out-dir " + outDir.string() + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  std::filesystem::path outputPath = outDir / std::filesystem::path(srcPath).stem();
  outputPath.replace_extension(".cpp");
  CHECK(std::filesystem::exists(outputPath));
}

TEST_CASE("cpp-ir emitter writes IR-lowered cpp for integer subset") {
  const std::string source = R"(
[return<int>]
main() {
  [i32 mut] counter{1i32}
  assign(counter, plus(counter, 2i32))
  return(counter)
}
)";
  const std::string srcPath = writeTemp("compile_cpp_ir_i32_subset.prime", source);
  const std::string outPath = (std::filesystem::temp_directory_path() / "primec_cpp_ir_i32_subset.cpp").string();

  const std::string compileCmd = "./primec --emit=cpp-ir " + srcPath + " -o " + outPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string output = readFile(outPath);
  CHECK(output.find("int64_t ps_entry_0(int argc, char **argv)") != std::string::npos);
  CHECK(output.find("switch (pc)") != std::string::npos);
}

TEST_CASE("cpp-ir emitter writes string and argv print paths") {
  const std::string source = R"(
[return<int> effects(io_out)]
main([array<string>] args) {
  print_line("hello"utf8)
  print_line(args[1i32])
  return(args.count())
}
)";
  const std::string srcPath = writeTemp("compile_cpp_ir_print_argv.prime", source);
  const std::string outPath = (std::filesystem::temp_directory_path() / "primec_cpp_ir_print_argv.cpp").string();

  const std::string compileCmd = "./primec --emit=cpp-ir " + srcPath + " -o " + outPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string output = readFile(outPath);
  CHECK(output.find("static const char *ps_string_table[]") != std::string::npos);
  CHECK(output.find("stack[sp++] = static_cast<uint64_t>(argc);") != std::string::npos);
  CHECK(output.find("argv[indexValue]") != std::string::npos);
}

TEST_CASE("cpp-ir emitter writes dynamic string print path") {
  const std::string source = R"(
[return<int> effects(io_out)]
main() {
  [string mut] msg{"left"utf8}
  assign(msg, "right"utf8)
  print_line(msg)
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_cpp_ir_dynamic_string_print.prime", source);
  const std::string outPath =
      (std::filesystem::temp_directory_path() / "primec_cpp_ir_dynamic_string_print.cpp").string();

  const std::string compileCmd = "./primec --emit=cpp-ir " + srcPath + " -o " + outPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string output = readFile(outPath);
  CHECK(output.find("if (stringIndex >= ps_string_table_count)") != std::string::npos);
  CHECK(output.find("ps_string_table[stringIndex]") != std::string::npos);
}

TEST_CASE("cpp-ir emitter writes callable dispatch paths") {
  const std::string source = R"(
[return<void> effects(io_out)]
logCall() {
  print_line("log"utf8)
}

[return<int>]
value() {
  return(41i32)
}

[return<int> effects(io_out)]
main() {
  logCall()
  return(plus(value(), 1i32))
}
)";
  const std::string srcPath = writeTemp("compile_cpp_ir_calls.prime", source);
  const std::string outPath = (std::filesystem::temp_directory_path() / "primec_cpp_ir_calls.cpp").string();

  const std::string compileCmd = "./primec --emit=cpp-ir " + srcPath + " -o " + outPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string output = readFile(outPath);
  CHECK(output.find("static int64_t ps_fn_0(uint64_t *stack, std::size_t &sp, int argc, char **argv);") !=
        std::string::npos);
  CHECK(output.find("static int64_t ps_fn_1(uint64_t *stack, std::size_t &sp, int argc, char **argv);") !=
        std::string::npos);
  CHECK(output.find("return ps_fn_0(stack, sp, argc, argv);") != std::string::npos);
}

TEST_CASE("cpp-ir emitter reports unsupported opcodes") {
  const std::string source = R"(
[return<int>]
main() {
  return(convert<int>(2.5f64))
}
)";
  const std::string srcPath = writeTemp("compile_cpp_ir_unsupported_opcode.prime", source);
  const std::string errPath =
      (std::filesystem::temp_directory_path() / "primec_cpp_ir_unsupported_opcode_err.txt").string();

  const std::string compileCmd =
      "./primec --emit=cpp-ir " + quoteShellArg(srcPath) + " -o /dev/null --entry /main 2> " + quoteShellArg(errPath);
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("ir-to-cpp failed: IrToCppEmitter unsupported opcode") != std::string::npos);
}

TEST_CASE("cpp-ir emitter writes f64 arithmetic paths") {
  const std::string source = R"(
[return<int>]
main() {
  [f64 mut] value{2.0f64}
  assign(value, plus(value, 0.5f64))
  if(greater_than(value, 2.4f64), then() { return(7i32) }, else() { return(3i32) })
}
)";
  const std::string srcPath = writeTemp("compile_cpp_ir_f64_math_subset.prime", source);
  const std::string outPath = (std::filesystem::temp_directory_path() / "primec_cpp_ir_f64_math_subset.cpp").string();

  const std::string compileCmd = "./primec --emit=cpp-ir " + srcPath + " -o " + outPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string output = readFile(outPath);
  CHECK(output.find("static uint64_t psF64ToBits(double value)") != std::string::npos);
  CHECK(output.find("stack[sp++] = psF64ToBits(left + right);") != std::string::npos);
}

TEST_CASE("cpp emitter uses ir backend for f64 arithmetic subset") {
  const std::string source = R"(
[return<int>]
main() {
  [f64 mut] value{2.0f64}
  assign(value, plus(value, 0.5f64))
  if(greater_than(value, 2.4f64), then() { return(7i32) }, else() { return(3i32) })
}
)";
  const std::string srcPath = writeTemp("compile_cpp_f64_math_ir_first.prime", source);
  const std::string outPath = (std::filesystem::temp_directory_path() / "primec_cpp_f64_math_ir_first.cpp").string();

  const std::string compileCmd = "./primec --emit=cpp " + srcPath + " -o " + outPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string output = readFile(outPath);
  CHECK(output.find("stack[sp++] = psF64ToBits(left + right);") != std::string::npos);
  CHECK(output.find("ps_entry_0") != std::string::npos);
}

TEST_CASE("cpp-ir emitter writes f64 comparison paths") {
  const std::string source = R"(
[return<int>]
main() {
  if(greater_than(2.5f64, 1.0f64), then() { return(7i32) }, else() { return(3i32) })
}
)";
  const std::string srcPath = writeTemp("compile_cpp_ir_f64_cmp_subset.prime", source);
  const std::string outPath = (std::filesystem::temp_directory_path() / "primec_cpp_ir_f64_cmp_subset.cpp").string();

  const std::string compileCmd = "./primec --emit=cpp-ir " + srcPath + " -o " + outPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string output = readFile(outPath);
  CHECK(output.find("static double psBitsToF64(uint64_t raw)") != std::string::npos);
  CHECK(output.find("double right = psBitsToF64(stack[--sp]);") != std::string::npos);
}

TEST_CASE("cpp emitter uses ir backend for f64 comparison subset") {
  const std::string source = R"(
[return<int>]
main() {
  if(greater_than(2.5f64, 1.0f64), then() { return(7i32) }, else() { return(3i32) })
}
)";
  const std::string srcPath = writeTemp("compile_cpp_f64_cmp_ir_first.prime", source);
  const std::string outPath = (std::filesystem::temp_directory_path() / "primec_cpp_f64_cmp_ir_first.cpp").string();

  const std::string compileCmd = "./primec --emit=cpp " + srcPath + " -o " + outPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string output = readFile(outPath);
  CHECK(output.find("static double psBitsToF64(uint64_t raw)") != std::string::npos);
  CHECK(output.find("ps_entry_0") != std::string::npos);
}

TEST_CASE("cpp-ir emitter writes f32 arithmetic and comparison paths") {
  const std::string source = R"(
[return<int>]
main() {
  [f32 mut] value{2.0f32}
  assign(value, plus(value, 0.5f32))
  if(greater_than(value, 2.4f32), then() { return(7i32) }, else() { return(3i32) })
}
)";
  const std::string srcPath = writeTemp("compile_cpp_ir_f32_subset.prime", source);
  const std::string outPath = (std::filesystem::temp_directory_path() / "primec_cpp_ir_f32_subset.cpp").string();

  const std::string compileCmd = "./primec --emit=cpp-ir " + srcPath + " -o " + outPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string output = readFile(outPath);
  CHECK(output.find("static float psBitsToF32(uint64_t raw)") != std::string::npos);
  CHECK(output.find("static uint64_t psF32ToBits(float value)") != std::string::npos);
  CHECK(output.find("float right = psBitsToF32(stack[--sp]);") != std::string::npos);
}

TEST_CASE("cpp emitter uses ir backend for f32 arithmetic subset") {
  const std::string source = R"(
[return<int>]
main() {
  [f32 mut] value{2.0f32}
  assign(value, plus(value, 0.5f32))
  if(greater_than(value, 2.4f32), then() { return(7i32) }, else() { return(3i32) })
}
)";
  const std::string srcPath = writeTemp("compile_cpp_f32_ir_first.prime", source);
  const std::string outPath = (std::filesystem::temp_directory_path() / "primec_cpp_f32_ir_first.cpp").string();

  const std::string compileCmd = "./primec --emit=cpp " + srcPath + " -o " + outPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string output = readFile(outPath);
  CHECK(output.find("static float psBitsToF32(uint64_t raw)") != std::string::npos);
  CHECK(output.find("ps_entry_0") != std::string::npos);
}

TEST_CASE("defaults to psir extension for emit=ir") {
  const std::string source = R"(
[return<int>]
main() {
  return(3i32)
}
)";
  const std::string srcPath = writeTemp("compile_default_ir.prime", source);
  const std::filesystem::path outDir = std::filesystem::temp_directory_path() / "primec_ir_default_out";
  std::error_code ec;
  std::filesystem::remove_all(outDir, ec);
  std::filesystem::create_directories(outDir, ec);
  REQUIRE(!ec);

  const std::string compileCmd =
      "./primec --emit=ir " + srcPath + " --out-dir " + outDir.string() + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  std::filesystem::path outputPath = outDir / std::filesystem::path(srcPath).stem();
  outputPath.replace_extension(".psir");
  CHECK(std::filesystem::exists(outputPath));
}

TEST_CASE("cpp emitter falls back to legacy output when ir subset is unsupported") {
  const std::string source = R"(
[return<int>]
main() {
  return(convert<int>(2.5f64))
}
)";
  const std::string srcPath = writeTemp("compile_cpp_ir_fallback_legacy_cpp.prime", source);
  const std::string outPath = (std::filesystem::temp_directory_path() / "primec_cpp_ir_fallback_legacy_cpp.cpp").string();

  const std::string compileCmd = "./primec --emit=cpp " + srcPath + " -o " + outPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string output = readFile(outPath);
  CHECK(output.find("ps_entry_0") == std::string::npos);
}

TEST_CASE("compiles and runs void main") {
  const std::string source = R"(
[return<void>]
main() {
  [i32] value{1i32}
}
)";
  const std::string srcPath = writeTemp("compile_void_main.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_void_main_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 0);
}

TEST_CASE("exe-ir emitter compiles and runs i32 subset") {
  const std::string source = R"(
[return<int>]
main() {
  [i32 mut] counter{1i32}
  assign(counter, plus(counter, 2i32))
  return(counter)
}
)";
  const std::string srcPath = writeTemp("compile_exe_ir_i32_subset.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_exe_ir_i32_subset").string();

  const std::string compileCmd = "./primec --emit=exe-ir " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 3);
}

TEST_CASE("exe-ir emitter compiles and runs i64 subset") {
  const std::string source = R"(
[return<i64>]
main() {
  [i64 mut] counter{10i64}
  assign(counter, plus(counter, 5i64))
  assign(counter, minus(counter, 2i64))
  return(counter)
}
)";
  const std::string srcPath = writeTemp("compile_exe_ir_i64_subset.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_exe_ir_i64_subset").string();

  const std::string compileCmd = "./primec --emit=exe-ir " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 13);
}

TEST_CASE("exe-ir emitter compiles and runs argv prints") {
  const std::string source = R"(
[return<int> effects(io_out)]
main([array<string>] args) {
  print_line(args[1i32])
  return(args.count())
}
)";
  const std::string srcPath = writeTemp("compile_exe_ir_print_argv.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_exe_ir_print_argv").string();
  const std::string outPath = (std::filesystem::temp_directory_path() / "primec_exe_ir_print_argv.out").string();

  const std::string compileCmd = "./primec --emit=exe-ir " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath + " alpha beta > " + outPath) == 3);
  CHECK(readFile(outPath) == "alpha\n");
}

TEST_CASE("exe-ir emitter compiles and runs dynamic string print") {
  const std::string source = R"(
[return<int> effects(io_out)]
main() {
  [string mut] msg{"left"utf8}
  assign(msg, "right"utf8)
  print_line(msg)
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_exe_ir_dynamic_string_print.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_exe_ir_dynamic_string_print").string();
  const std::string outPath = (std::filesystem::temp_directory_path() / "primec_exe_ir_dynamic_string_print.out").string();

  const std::string compileCmd = "./primec --emit=exe-ir " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath + " > " + outPath) == 0);
  CHECK(readFile(outPath) == "right\n");
}

TEST_CASE("exe-ir emitter compiles and runs call and callvoid paths") {
  const std::string source = R"(
[return<void> effects(io_out)]
logCall() {
  print_line("log"utf8)
}

[return<int>]
value() {
  return(41i32)
}

[return<int> effects(io_out)]
main() {
  logCall()
  return(plus(value(), 1i32))
}
)";
  const std::string srcPath = writeTemp("compile_exe_ir_calls.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_exe_ir_calls").string();
  const std::string outPath = (std::filesystem::temp_directory_path() / "primec_exe_ir_calls.out").string();

  const std::string compileCmd = "./primec --emit=exe-ir " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath + " > " + outPath) == 42);
  CHECK(readFile(outPath) == "log\n");
}

TEST_CASE("exe-ir emitter compiles and runs f32 arithmetic subset") {
  const std::string source = R"(
[return<int>]
main() {
  [f32 mut] value{2.0f32}
  assign(value, plus(value, 0.5f32))
  if(greater_than(value, 2.4f32), then() { return(7i32) }, else() { return(3i32) })
}
)";
  const std::string srcPath = writeTemp("compile_exe_ir_f32_subset.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_exe_ir_f32_subset").string();

  const std::string compileCmd = "./primec --emit=exe-ir " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 7);
}

TEST_CASE("exe-ir emitter compiles and runs f64 comparison subset") {
  const std::string source = R"(
[return<int>]
main() {
  if(greater_than(2.5f64, 1.0f64), then() { return(7i32) }, else() { return(3i32) })
}
)";
  const std::string srcPath = writeTemp("compile_exe_ir_f64_cmp_subset.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_exe_ir_f64_cmp_subset").string();

  const std::string compileCmd = "./primec --emit=exe-ir " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 7);
}

TEST_CASE("exe emitter uses ir backend for f32 arithmetic subset") {
  const std::string source = R"(
[return<int>]
main() {
  [f32 mut] value{2.0f32}
  assign(value, plus(value, 0.5f32))
  if(greater_than(value, 2.4f32), then() { return(7i32) }, else() { return(3i32) })
}
)";
  const std::string srcPath = writeTemp("compile_exe_f32_ir_first.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_exe_f32_ir_first").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 7);
}

TEST_CASE("exe-ir emitter compiles and runs f64 arithmetic subset") {
  const std::string source = R"(
[return<int>]
main() {
  [f64 mut] value{2.0f64}
  assign(value, plus(value, 0.5f64))
  if(greater_than(value, 2.4f64), then() { return(7i32) }, else() { return(3i32) })
}
)";
  const std::string srcPath = writeTemp("compile_exe_ir_f64_math_subset.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_exe_ir_f64_math_subset").string();

  const std::string compileCmd = "./primec --emit=exe-ir " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 7);
}

TEST_CASE("exe emitter uses ir backend for f64 comparison subset") {
  const std::string source = R"(
[return<int>]
main() {
  if(greater_than(2.5f64, 1.0f64), then() { return(7i32) }, else() { return(3i32) })
}
)";
  const std::string srcPath = writeTemp("compile_exe_f64_cmp_ir_first.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_exe_f64_cmp_ir_first").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 7);
}

TEST_CASE("exe emitter uses ir backend for f64 arithmetic subset") {
  const std::string source = R"(
[return<int>]
main() {
  [f64 mut] value{2.0f64}
  assign(value, plus(value, 0.5f64))
  if(greater_than(value, 2.4f64), then() { return(7i32) }, else() { return(3i32) })
}
)";
  const std::string srcPath = writeTemp("compile_exe_f64_math_ir_first.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_exe_f64_math_ir_first").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 7);
}

TEST_CASE("cpp and exe emitters match cpp-ir and exe-ir on shared corpus") {
  struct DifferentialCase {
    const char *name;
    const char *source;
    const char *runtimeArgs;
    int expectedExitCode;
    const char *expectedStdout;
    const char *expectedStderr;
  };

  const std::vector<DifferentialCase> cases = {
      {
          "i32_arithmetic",
          R"(
[return<int>]
main() {
  [i32 mut] counter{1i32}
  assign(counter, plus(counter, 2i32))
  return(counter)
}
)",
          "",
          3,
          "",
          "",
      },
      {
          "argv_and_io",
          R"(
[return<int> effects(io_out, io_err)]
main([array<string>] args) {
  print_line(args[1i32])
  print_error("!"utf8)
  return(args.count())
}
)",
          " alpha beta",
          3,
          "alpha\n",
          "!",
      },
      {
          "dynamic_string",
          R"(
[return<int> effects(io_out)]
main() {
  [string mut] msg{"left"utf8}
  assign(msg, "right"utf8)
  print_line(msg)
  return(0i32)
}
)",
          "",
          0,
          "right\n",
          "",
      },
  };

  for (const auto &testCase : cases) {
    CAPTURE(testCase.name);
    const std::string srcPath = writeTemp(std::string("compile_cpp_ir_differential_") + testCase.name + ".prime",
                                          testCase.source);
    const std::string astCppPath =
        (std::filesystem::temp_directory_path() / (std::string("primec_cpp_differential_") + testCase.name + ".cpp"))
            .string();
    const std::string irCppPath =
        (std::filesystem::temp_directory_path() / (std::string("primec_cpp_ir_differential_") + testCase.name + ".cpp"))
            .string();
    const std::string astExePath =
        (std::filesystem::temp_directory_path() / (std::string("primec_exe_differential_") + testCase.name)).string();
    const std::string irExePath =
        (std::filesystem::temp_directory_path() / (std::string("primec_exe_ir_differential_") + testCase.name)).string();

    const std::string compileAstCppCmd = "./primec --emit=cpp " + quoteShellArg(srcPath) + " -o " +
                                         quoteShellArg(astCppPath) + " --entry /main";
    const std::string compileIrCppCmd = "./primec --emit=cpp-ir " + quoteShellArg(srcPath) + " -o " +
                                        quoteShellArg(irCppPath) + " --entry /main";
    CHECK(runCommand(compileAstCppCmd) == 0);
    CHECK(runCommand(compileIrCppCmd) == 0);
    CHECK(readFile(astCppPath) == readFile(irCppPath));

    const std::string compileAstExeCmd = "./primec --emit=exe " + quoteShellArg(srcPath) + " -o " +
                                         quoteShellArg(astExePath) + " --entry /main";
    const std::string compileIrExeCmd = "./primec --emit=exe-ir " + quoteShellArg(srcPath) + " -o " +
                                        quoteShellArg(irExePath) + " --entry /main";
    CHECK(runCommand(compileAstExeCmd) == 0);
    CHECK(runCommand(compileIrExeCmd) == 0);

    const std::string astOutPath = (std::filesystem::temp_directory_path() /
                                    (std::string("primec_exe_differential_") + testCase.name + ".out"))
                                       .string();
    const std::string astErrPath = (std::filesystem::temp_directory_path() /
                                    (std::string("primec_exe_differential_") + testCase.name + ".err"))
                                       .string();
    const std::string irOutPath = (std::filesystem::temp_directory_path() /
                                   (std::string("primec_exe_ir_differential_") + testCase.name + ".out"))
                                      .string();
    const std::string irErrPath = (std::filesystem::temp_directory_path() /
                                   (std::string("primec_exe_ir_differential_") + testCase.name + ".err"))
                                      .string();

    const std::string runAstCmd = quoteShellArg(astExePath) + testCase.runtimeArgs + " > " + quoteShellArg(astOutPath) +
                                  " 2> " + quoteShellArg(astErrPath);
    const std::string runIrCmd = quoteShellArg(irExePath) + testCase.runtimeArgs + " > " + quoteShellArg(irOutPath) +
                                 " 2> " + quoteShellArg(irErrPath);
    CHECK(runCommand(runAstCmd) == testCase.expectedExitCode);
    CHECK(runCommand(runIrCmd) == testCase.expectedExitCode);
    CHECK(readFile(astOutPath) == testCase.expectedStdout);
    CHECK(readFile(astErrPath) == testCase.expectedStderr);
    CHECK(readFile(irOutPath) == testCase.expectedStdout);
    CHECK(readFile(irErrPath) == testCase.expectedStderr);
  }
}

TEST_CASE("compiles and runs explicit void return") {
  const std::string source = R"(
[return<void>]
main() {
  return()
}
)";
  const std::string srcPath = writeTemp("compile_void_return.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_void_return_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 0);
}

TEST_CASE("exe emitter falls back to legacy output when ir subset is unsupported") {
  const std::string source = R"(
[return<int>]
main() {
  return(convert<int>(2.5f64))
}
)";
  const std::string srcPath = writeTemp("compile_exe_ir_fallback_legacy_cpp.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_exe_ir_fallback_legacy_cpp").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 2);
}

TEST_CASE("compiles and runs implicit void main") {
  const std::string source = R"(
main() {
  [i32] value{1i32}
  value
}
)";
  const std::string srcPath = writeTemp("compile_void_implicit.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_void_implicit_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 0);
}

TEST_CASE("compiles and runs argv count") {
  const std::string source = R"(
[return<int>]
main([array<string>] args) {
  return(args.count())
}
)";
  const std::string srcPath = writeTemp("compile_args.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_args_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 1);
  CHECK(runCommand(exePath + " alpha beta") == 3);
}

TEST_CASE("compiles and runs argv count helper") {
  const std::string source = R"(
[return<int>]
main([array<string>] args) {
  return(count(args))
}
)";
  const std::string srcPath = writeTemp("compile_args_count_helper.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_args_count_helper_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 1);
  CHECK(runCommand(exePath + " alpha beta") == 3);
}

TEST_CASE("compiles and runs argv error output in C++ emitter") {
  const std::string source = R"(
[return<int> effects(io_err)]
main([array<string>] args) {
  print_line_error(args[1i32])
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_args_error.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_args_error_exe").string();
  const std::string errPath = (std::filesystem::temp_directory_path() / "primec_args_error_err.txt").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string runCmd = exePath + " alpha beta 2> " + errPath;
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(errPath) == "alpha\n");
}

TEST_CASE("compiles and runs argv error output without newline in C++ emitter") {
  const std::string source = R"(
[return<int> effects(io_err)]
main([array<string>] args) {
  print_error(args[1i32])
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_args_error_no_newline.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_args_error_no_newline_exe").string();
  const std::string errPath =
      (std::filesystem::temp_directory_path() / "primec_args_error_no_newline_err.txt").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string runCmd = exePath + " alpha beta 2> " + errPath;
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(errPath) == "alpha");
}

TEST_CASE("compiles and runs argv error output u64 index in C++ emitter") {
  const std::string source = R"(
[return<int> effects(io_err)]
main([array<string>] args) {
  if(greater_than(args.count(), 1i32)) {
    print_error(args[1u64])
  } else {
  }
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_args_error_u64.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_args_error_u64_exe").string();
  const std::string errPath =
      (std::filesystem::temp_directory_path() / "primec_args_error_u64_err.txt").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string runCmd = exePath + " alpha 2> " + errPath;
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(errPath) == "alpha");
}

TEST_CASE("compiles and runs argv unsafe error output in C++ emitter") {
  const std::string source = R"(
[return<int> effects(io_err)]
main([array<string>] args) {
  if(greater_than(args.count(), 1i32)) {
    print_error(at_unsafe(args, 1i32))
  } else {
  }
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_args_error_unsafe.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_args_error_unsafe_exe").string();
  const std::string errPath =
      (std::filesystem::temp_directory_path() / "primec_args_error_unsafe_err.txt").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string runCmd = exePath + " alpha 2> " + errPath;
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(errPath) == "alpha");
}

TEST_CASE("compiles and runs argv unsafe line error output in C++ emitter") {
  const std::string source = R"(
[return<int> effects(io_err)]
main([array<string>] args) {
  if(greater_than(args.count(), 1i32)) {
    print_line_error(at_unsafe(args, 1i32))
  } else {
  }
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_args_line_error_unsafe.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() / "primec_args_line_error_unsafe_exe").string();
  const std::string errPath =
      (std::filesystem::temp_directory_path() / "primec_args_line_error_unsafe_err.txt").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string runCmd = exePath + " alpha 2> " + errPath;
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(errPath) == "alpha\n");
}

TEST_CASE("compiles and runs argv print in C++ emitter") {
  const std::string source = R"(
[return<int> effects(io_out)]
main([array<string>] args) {
  if(greater_than(args.count(), 2i32)) {
    print_line(args[1i32])
    print_line(args[2i32])
  } else {
  }
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_args_print.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_args_print_exe").string();
  const std::string outPath = (std::filesystem::temp_directory_path() / "primec_args_print_out.txt").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string runCmd = exePath + " alpha beta > " + outPath;
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(outPath) == "alpha\nbeta\n");
}

TEST_CASE("compiles and runs argv print without newline in C++ emitter") {
  const std::string source = R"(
[return<int> effects(io_out)]
main([array<string>] args) {
  if(greater_than(args.count(), 1i32)) {
    print(args[1i32])
  } else {
  }
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_args_print_no_newline.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_args_print_no_newline_exe").string();
  const std::string outPath =
      (std::filesystem::temp_directory_path() / "primec_args_print_no_newline_out.txt").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string runCmd = exePath + " alpha > " + outPath;
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(outPath) == "alpha");
}

TEST_CASE("compiles and runs argv print with u64 index in C++ emitter") {
  const std::string source = R"(
[return<int> effects(io_out)]
main([array<string>] args) {
  if(greater_than(args.count(), 1i32)) {
    print_line(args[1u64])
  } else {
  }
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_args_print_u64.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_args_print_u64_exe").string();
  const std::string outPath = (std::filesystem::temp_directory_path() / "primec_args_print_u64_out.txt").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string runCmd = exePath + " alpha > " + outPath;
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(outPath) == "alpha\n");
}

TEST_CASE("compiles and runs argv unsafe access in C++ emitter") {
  const std::string source = R"(
[return<int> effects(io_out)]
main([array<string>] args) {
  if(greater_than(args.count(), 2i32)) {
    print_line(at_unsafe(args, 1i32))
    print_line(at_unsafe(args, 2i32))
  } else {
  }
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_args_unsafe.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_args_unsafe_exe").string();
  const std::string outPath = (std::filesystem::temp_directory_path() / "primec_args_unsafe_out.txt").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string runCmd = exePath + " alpha beta > " + outPath;
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(outPath) == "alpha\nbeta\n");
}

TEST_CASE("compiles and runs argv unsafe access with u64 index in C++ emitter") {
  const std::string source = R"(
[return<int> effects(io_out)]
main([array<string>] args) {
  if(greater_than(args.count(), 1i32)) {
    print_line(at_unsafe(args, 1u64))
  } else {
  }
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_args_unsafe_u64.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_args_unsafe_u64_exe").string();
  const std::string outPath = (std::filesystem::temp_directory_path() / "primec_args_unsafe_u64_out.txt").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string runCmd = exePath + " alpha > " + outPath;
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(outPath) == "alpha\n");
}

TEST_CASE("compiles and runs array literal") {
  const std::string source = R"(
[return<int>]
main() {
  array<i32>{1i32, 2i32, 3i32}
  return(7i32)
}
)";
  const std::string srcPath = writeTemp("compile_array_literal.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_array_literal_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 7);
}

TEST_CASE("compiles and runs array literal count method") {
  const std::string source = R"(
[return<int>]
main() {
  return(array<i32>(1i32, 2i32, 3i32).count())
}
)";
  const std::string srcPath = writeTemp("compile_array_literal_count.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_array_literal_count_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 3);
}

TEST_CASE("compiles and runs array literal unsafe access") {
  const std::string source = R"(
[return<int>]
main() {
  return(at_unsafe(array<i32>(4i32, 7i32, 9i32), 1i32))
}
)";
  const std::string srcPath = writeTemp("compile_array_literal_unsafe.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_array_literal_unsafe_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 7);
}

TEST_CASE("compiles and runs array count helper") {
  const std::string source = R"(
[return<int>]
main() {
  [array<i32>] values{array<i32>(1i32, 2i32, 3i32)}
  return(count(values))
}
)";
  const std::string srcPath = writeTemp("compile_array_count_helper.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_array_count_helper_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 3);
}

TEST_CASE("compiles and runs array literal count helper") {
  const std::string source = R"(
[return<int>]
main() {
  return(count(array<i32>(1i32, 2i32, 3i32)))
}
)";
  const std::string srcPath = writeTemp("compile_array_literal_count_helper.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() / "primec_array_literal_count_helper_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 3);
}

TEST_CASE("compiles and runs literal method call in C++ emitter") {
  const std::string source = R"(
namespace i32 {
  [return<int>]
  inc([i32] value) {
    return(plus(value, 1i32))
  }
}

[return<int>]
main() {
  return(1i32.inc())
}
)";
  const std::string srcPath = writeTemp("compile_method_literal.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_method_literal_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 2);
}

TEST_SUITE_END();
