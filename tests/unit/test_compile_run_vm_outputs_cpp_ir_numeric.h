#pragma once

TEST_CASE("cpp-ir emitter writes f64 conversion paths") {
  SKIP_IF_VM_IR_BACKEND_LIMITED();
  const std::string source = R"(
[return<i64>]
main() {
  [i32] base{7i32}
  [f64] widened{convert<f64>(base)}
  [f32] narrowed{convert<f32>(widened)}
  [f64] roundTrip{convert<f64>(narrowed)}
  return(convert<i64>(roundTrip))
}
)";
  const std::string srcPath = writeTemp("compile_cpp_ir_f64_convert_subset.prime", source);
  const std::string outPath =
      (testScratchPath("") / "primec_cpp_ir_f64_convert_subset.cpp").string();

  const std::string compileCmd = "./primec --emit=cpp-ir " + srcPath + " -o " + outPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string output = readFile(outPath);
  CHECK(output.find("stack[sp++] = psF64ToBits(static_cast<double>(value));") != std::string::npos);
  CHECK(output.find("stack[sp++] = psF32ToBits(static_cast<float>(value));") != std::string::npos);
  CHECK(output.find("static int64_t psConvertF64ToI64(double value)") != std::string::npos);
  CHECK(output.find("int64_t converted = psConvertF64ToI64(value);") != std::string::npos);
}

TEST_CASE("cpp emitter uses ir backend for f64 conversion subset") {
  SKIP_IF_VM_IR_BACKEND_LIMITED();
  const std::string source = R"(
[return<i64>]
main() {
  [i32] base{7i32}
  [f64] widened{convert<f64>(base)}
  [f32] narrowed{convert<f32>(widened)}
  [f64] roundTrip{convert<f64>(narrowed)}
  return(convert<i64>(roundTrip))
}
)";
  const std::string srcPath = writeTemp("compile_cpp_f64_convert_ir_first.prime", source);
  const std::string outPath = (testScratchPath("") / "primec_cpp_f64_convert_ir_first.cpp").string();

  const std::string compileCmd = "./primec --emit=cpp " + srcPath + " -o " + outPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string output = readFile(outPath);
  CHECK(output.find("stack[sp++] = psF64ToBits(static_cast<double>(value));") != std::string::npos);
  CHECK(output.find("static int64_t psConvertF64ToI64(double value)") != std::string::npos);
  CHECK(output.find("int64_t converted = psConvertF64ToI64(value);") != std::string::npos);
  CHECK(output.find("ps_entry_0") != std::string::npos);
}

TEST_CASE("cpp-ir emitter writes f64 to i32 conversion paths") {
  SKIP_IF_VM_IR_BACKEND_LIMITED();
  const std::string source = R"(
[return<int>]
main() {
  return(convert<int>(2.5f64))
}
)";
  const std::string srcPath = writeTemp("compile_cpp_ir_f64_to_i32_convert.prime", source);
  const std::string outPath =
      (testScratchPath("") / "primec_cpp_ir_f64_to_i32_convert.cpp").string();

  const std::string compileCmd = "./primec --emit=cpp-ir " + srcPath + " -o " + outPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string output = readFile(outPath);
  CHECK(output.find("static int32_t psConvertF64ToI32(double value)") != std::string::npos);
  CHECK(output.find("int32_t converted = psConvertF64ToI32(value);") != std::string::npos);
}

TEST_CASE("cpp emitter uses ir backend for f64 to i32 conversion") {
  SKIP_IF_VM_IR_BACKEND_LIMITED();
  const std::string source = R"(
[return<int>]
main() {
  return(convert<int>(2.5f64))
}
)";
  const std::string srcPath = writeTemp("compile_cpp_f64_to_i32_ir_first.prime", source);
  const std::string outPath =
      (testScratchPath("") / "primec_cpp_f64_to_i32_ir_first.cpp").string();

  const std::string compileCmd = "./primec --emit=cpp " + srcPath + " -o " + outPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string output = readFile(outPath);
  CHECK(output.find("static int32_t psConvertF64ToI32(double value)") != std::string::npos);
  CHECK(output.find("int32_t converted = psConvertF64ToI32(value);") != std::string::npos);
  CHECK(output.find("ps_entry_0") != std::string::npos);
}

TEST_CASE("cpp-ir emitter writes f32 to i64 conversion paths") {
  SKIP_IF_VM_IR_BACKEND_LIMITED();
  const std::string source = R"(
[return<i64>]
main() {
  return(convert<i64>(2.5f32))
}
)";
  const std::string srcPath = writeTemp("compile_cpp_ir_f32_to_i64_convert.prime", source);
  const std::string outPath =
      (testScratchPath("") / "primec_cpp_ir_f32_to_i64_convert.cpp").string();

  const std::string compileCmd = "./primec --emit=cpp-ir " + srcPath + " -o " + outPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string output = readFile(outPath);
  CHECK(output.find("static int64_t psConvertF32ToI64(float value)") != std::string::npos);
  CHECK(output.find("int64_t converted = psConvertF32ToI64(value);") != std::string::npos);
}

TEST_CASE("cpp emitter uses ir backend for f32 to i64 conversion") {
  SKIP_IF_VM_IR_BACKEND_LIMITED();
  const std::string source = R"(
[return<i64>]
main() {
  return(convert<i64>(2.5f32))
}
)";
  const std::string srcPath = writeTemp("compile_cpp_f32_to_i64_ir_first.prime", source);
  const std::string outPath =
      (testScratchPath("") / "primec_cpp_f32_to_i64_ir_first.cpp").string();

  const std::string compileCmd = "./primec --emit=cpp " + srcPath + " -o " + outPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string output = readFile(outPath);
  CHECK(output.find("static int64_t psConvertF32ToI64(float value)") != std::string::npos);
  CHECK(output.find("int64_t converted = psConvertF32ToI64(value);") != std::string::npos);
  CHECK(output.find("ps_entry_0") != std::string::npos);
}

TEST_CASE("cpp-ir emitter writes f64 to i64 conversion paths") {
  SKIP_IF_VM_IR_BACKEND_LIMITED();
  const std::string source = R"(
[return<i64>]
main() {
  return(convert<i64>(2.5f64))
}
)";
  const std::string srcPath = writeTemp("compile_cpp_ir_f64_to_i64_convert.prime", source);
  const std::string outPath =
      (testScratchPath("") / "primec_cpp_ir_f64_to_i64_convert.cpp").string();

  const std::string compileCmd = "./primec --emit=cpp-ir " + srcPath + " -o " + outPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string output = readFile(outPath);
  CHECK(output.find("static int64_t psConvertF64ToI64(double value)") != std::string::npos);
  CHECK(output.find("int64_t converted = psConvertF64ToI64(value);") != std::string::npos);
}

TEST_CASE("cpp emitter uses ir backend for f64 to i64 conversion") {
  SKIP_IF_VM_IR_BACKEND_LIMITED();
  const std::string source = R"(
[return<i64>]
main() {
  return(convert<i64>(2.5f64))
}
)";
  const std::string srcPath = writeTemp("compile_cpp_f64_to_i64_ir_first.prime", source);
  const std::string outPath =
      (testScratchPath("") / "primec_cpp_f64_to_i64_ir_first.cpp").string();

  const std::string compileCmd = "./primec --emit=cpp " + srcPath + " -o " + outPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string output = readFile(outPath);
  CHECK(output.find("static int64_t psConvertF64ToI64(double value)") != std::string::npos);
  CHECK(output.find("int64_t converted = psConvertF64ToI64(value);") != std::string::npos);
  CHECK(output.find("ps_entry_0") != std::string::npos);
}

TEST_CASE("cpp-ir emitter writes f64 to u64 conversion paths") {
  SKIP_IF_VM_IR_BACKEND_LIMITED();
  const std::string source = R"(
[return<u64>]
main() {
  return(convert<u64>(2.5f64))
}
)";
  const std::string srcPath = writeTemp("compile_cpp_ir_f64_to_u64_convert.prime", source);
  const std::string outPath =
      (testScratchPath("") / "primec_cpp_ir_f64_to_u64_convert.cpp").string();

  const std::string compileCmd = "./primec --emit=cpp-ir " + srcPath + " -o " + outPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string output = readFile(outPath);
  CHECK(output.find("static uint64_t psConvertF64ToU64(double value)") != std::string::npos);
  CHECK(output.find("uint64_t converted = psConvertF64ToU64(value);") != std::string::npos);
}

TEST_CASE("cpp emitter uses ir backend for f64 to u64 conversion") {
  SKIP_IF_VM_IR_BACKEND_LIMITED();
  const std::string source = R"(
[return<u64>]
main() {
  return(convert<u64>(2.5f64))
}
)";
  const std::string srcPath = writeTemp("compile_cpp_f64_to_u64_ir_first.prime", source);
  const std::string outPath =
      (testScratchPath("") / "primec_cpp_f64_to_u64_ir_first.cpp").string();

  const std::string compileCmd = "./primec --emit=cpp " + srcPath + " -o " + outPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string output = readFile(outPath);
  CHECK(output.find("static uint64_t psConvertF64ToU64(double value)") != std::string::npos);
  CHECK(output.find("uint64_t converted = psConvertF64ToU64(value);") != std::string::npos);
  CHECK(output.find("ps_entry_0") != std::string::npos);
}

TEST_CASE("cpp-ir emitter writes f64 comparison paths") {
  SKIP_IF_VM_IR_BACKEND_LIMITED();
  const std::string source = R"(
[return<int>]
main() {
  if(greater_than(2.5f64, 1.0f64), then() { return(7i32) }, else() { return(3i32) })
}
)";
  const std::string srcPath = writeTemp("compile_cpp_ir_f64_cmp_subset.prime", source);
  const std::string outPath = (testScratchPath("") / "primec_cpp_ir_f64_cmp_subset.cpp").string();

  const std::string compileCmd = "./primec --emit=cpp-ir " + srcPath + " -o " + outPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string output = readFile(outPath);
  CHECK(output.find("static double psBitsToF64(uint64_t raw)") != std::string::npos);
  CHECK(output.find("double right = psBitsToF64(stack[--sp]);") != std::string::npos);
}

TEST_CASE("cpp emitter uses ir backend for f64 comparison subset") {
  SKIP_IF_VM_IR_BACKEND_LIMITED();
  const std::string source = R"(
[return<int>]
main() {
  if(greater_than(2.5f64, 1.0f64), then() { return(7i32) }, else() { return(3i32) })
}
)";
  const std::string srcPath = writeTemp("compile_cpp_f64_cmp_ir_first.prime", source);
  const std::string outPath = (testScratchPath("") / "primec_cpp_f64_cmp_ir_first.cpp").string();

  const std::string compileCmd = "./primec --emit=cpp " + srcPath + " -o " + outPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string output = readFile(outPath);
  CHECK(output.find("static double psBitsToF64(uint64_t raw)") != std::string::npos);
  CHECK(output.find("ps_entry_0") != std::string::npos);
}

TEST_CASE("cpp-ir emitter writes f32 arithmetic and comparison paths") {
  SKIP_IF_VM_IR_BACKEND_LIMITED();
  const std::string source = R"(
[return<int>]
main() {
  [f32 mut] value{2.0f32}
  assign(value, plus(value, 0.5f32))
  if(greater_than(value, 2.4f32), then() { return(7i32) }, else() { return(3i32) })
}
)";
  const std::string srcPath = writeTemp("compile_cpp_ir_f32_subset.prime", source);
  const std::string outPath = (testScratchPath("") / "primec_cpp_ir_f32_subset.cpp").string();

  const std::string compileCmd = "./primec --emit=cpp-ir " + srcPath + " -o " + outPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string output = readFile(outPath);
  CHECK(output.find("static float psBitsToF32(uint64_t raw)") != std::string::npos);
  CHECK(output.find("static uint64_t psF32ToBits(float value)") != std::string::npos);
  CHECK(output.find("float right = psBitsToF32(stack[--sp]);") != std::string::npos);
}

TEST_CASE("cpp emitter uses ir backend for f32 arithmetic subset") {
  SKIP_IF_VM_IR_BACKEND_LIMITED();
  const std::string source = R"(
[return<int>]
main() {
  [f32 mut] value{2.0f32}
  assign(value, plus(value, 0.5f32))
  if(greater_than(value, 2.4f32), then() { return(7i32) }, else() { return(3i32) })
}
)";
  const std::string srcPath = writeTemp("compile_cpp_f32_ir_first.prime", source);
  const std::string outPath = (testScratchPath("") / "primec_cpp_f32_ir_first.cpp").string();

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
  const std::filesystem::path outDir = testScratchPath("") / "primec_ir_default_out";
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

TEST_CASE("cpp emitter uses ir backend for file read subset") {
  SKIP_IF_VM_IR_BACKEND_LIMITED();
  const std::string source = R"(
[return<Result<FileError>> effects(file_write) on_error<FileError, /log_file_error>]
main() {
  [File<Read>] file{File<Read>("/dev/null"utf8)?}
  file.close()?
  return(Result.ok())
}
[effects(io_err)]
log_file_error([FileError] err) {
  print_line_error("file error"utf8)
}
)";
  const std::string srcPath = writeTemp("compile_cpp_file_read_ir_first.prime", source);
  const std::string outPath = (testScratchPath("") / "primec_cpp_file_read_ir_first.cpp").string();

  const std::string compileCmd = "./primec --emit=cpp " + srcPath + " -o " + outPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string output = readFile(outPath);
  CHECK(output.find("ps_entry_0") != std::string::npos);
  CHECK(output.find("int fileOpenFlags = O_RDONLY;") != std::string::npos);
}

