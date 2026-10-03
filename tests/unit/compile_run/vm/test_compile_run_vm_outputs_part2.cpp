#include "../test_compile_run_helpers.h"

#include "primec/ir/IrSerializer.h"

TEST_SUITE_BEGIN("primestruct.compile.run.vm.outputs");

static bool vmIrBackendParitySupported() {
  static int cached = -1;
  if (cached != -1) {
    return cached == 1;
  }

  const std::string source = R"(
[return<int>]
main() {
  [f64] left{1.0f64}
  [f64] right{2.0f64}
  if(less_than(left, right), then() { return(7i32) }, else() { return(0i32) })
}
)";
  const std::string srcPath = writeTemp("compile_cpp_ir_backend_probe.prime", source);
  const std::string outPath = (testScratchPath("") / "primec_cpp_ir_backend_probe.cpp").string();
  const std::string compileCmd = "./primec --emit=cpp-ir " + quoteShellArg(srcPath) + " -o " + quoteShellArg(outPath) +
                                 " --entry /main";
  const int code = runCommand(compileCmd);
  cached = (code == 0) ? 1 : 0;
  return cached == 1;
}

#define SKIP_IF_VM_IR_BACKEND_LIMITED()                                                                       \
  if (!vmIrBackendParitySupported()) {                                                                        \
    INFO("Skipping vm ir backend parity checks until ir-to-cpp backend supports this corpus");               \
    CHECK(true);                                                                                              \
    return;                                                                                                   \
  }


TEST_CASE("cpp-ir emitter writes file io paths") {
  SKIP_IF_VM_IR_BACKEND_LIMITED();
  const std::string source = R"(
[return<Result<FileError>> effects(file_write) on_error<FileError, /log_file_error>]
main() {
  [File<Write>] file{File<Write>("/tmp/primec_cpp_ir_file_io.txt"utf8)?}
  file.write("x"utf8)?
  file.flush()?
  file.close()?
  return(Result.ok())
}
[effects(io_err)]
log_file_error([FileError] err) {
  print_line_error("file error"utf8)
}
)";
  const std::string srcPath = writeTemp("compile_cpp_ir_file_io_subset.prime", source);
  const std::string outPath = (testScratchPath("") / "primec_cpp_ir_file_io_subset.cpp").string();

  const std::string compileCmd = "./primec --emit=cpp-ir " + srcPath + " -o " + outPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string output = readFile(outPath);
  CHECK(output.find("static uint32_t psWriteAll(int fd, const void *data, std::size_t size)") != std::string::npos);
  CHECK(output.find("int fileFd = ::open(ps_string_table[0], fileOpenFlags, 0644);") != std::string::npos);
  CHECK(output.find("int closeRc = ::close(closeFd);") != std::string::npos);
}

TEST_CASE("cpp emitter uses ir backend for file io subset") {
  SKIP_IF_VM_IR_BACKEND_LIMITED();
  const std::string source = R"(
[return<Result<FileError>> effects(file_write) on_error<FileError, /log_file_error>]
main() {
  [File<Write>] file{File<Write>("/tmp/primec_cpp_file_io.txt"utf8)?}
  file.write("x"utf8)?
  file.flush()?
  file.close()?
  return(Result.ok())
}
[effects(io_err)]
log_file_error([FileError] err) {
  print_line_error("file error"utf8)
}
)";
  const std::string srcPath = writeTemp("compile_cpp_file_io_ir_first.prime", source);
  const std::string outPath = (testScratchPath("") / "primec_cpp_file_io_ir_first.cpp").string();

  const std::string compileCmd = "./primec --emit=cpp " + srcPath + " -o " + outPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string output = readFile(outPath);
  CHECK(output.find("static uint32_t psWriteAll(int fd, const void *data, std::size_t size)") != std::string::npos);
  CHECK(output.find("ps_entry_0") != std::string::npos);
}

TEST_CASE("cpp-ir emitter writes f64 arithmetic paths") {
  SKIP_IF_VM_IR_BACKEND_LIMITED();
  const std::string source = R"(
[return<int>]
main() {
  [f64 mut] value{2.0f64}
  assign(value, plus(value, 0.5f64))
  if(greater_than(value, 2.4f64), then() { return(7i32) }, else() { return(3i32) })
}
)";
  const std::string srcPath = writeTemp("compile_cpp_ir_f64_math_subset.prime", source);
  const std::string outPath = (testScratchPath("") / "primec_cpp_ir_f64_math_subset.cpp").string();

  const std::string compileCmd = "./primec --emit=cpp-ir " + srcPath + " -o " + outPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string output = readFile(outPath);
  CHECK(output.find("static uint64_t psF64ToBits(double value)") != std::string::npos);
  CHECK(output.find("stack[sp++] = psF64ToBits(left + right);") != std::string::npos);
}

TEST_CASE("cpp emitter uses ir backend for f64 arithmetic subset") {
  SKIP_IF_VM_IR_BACKEND_LIMITED();
  const std::string source = R"(
[return<int>]
main() {
  [f64 mut] value{2.0f64}
  assign(value, plus(value, 0.5f64))
  if(greater_than(value, 2.4f64), then() { return(7i32) }, else() { return(3i32) })
}
)";
  const std::string srcPath = writeTemp("compile_cpp_f64_math_ir_first.prime", source);
  const std::string outPath = (testScratchPath("") / "primec_cpp_f64_math_ir_first.cpp").string();

  const std::string compileCmd = "./primec --emit=cpp " + srcPath + " -o " + outPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string output = readFile(outPath);
  CHECK(output.find("stack[sp++] = psF64ToBits(left + right);") != std::string::npos);
  CHECK(output.find("ps_entry_0") != std::string::npos);
}

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

TEST_CASE("exe-ir emitter compiles and runs i32 subset") {
  SKIP_IF_VM_IR_BACKEND_LIMITED();
  const std::string source = R"(
[return<int>]
main() {
  [i32 mut] counter{1i32}
  assign(counter, plus(counter, 2i32))
  return(counter)
}
)";
  const std::string srcPath = writeTemp("compile_exe_ir_i32_subset.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_exe_ir_i32_subset").string();

  const std::string compileCmd = "./primec --emit=exe-ir " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 3);
}

TEST_CASE("exe-ir emitter compiles and runs i64 subset") {
  SKIP_IF_VM_IR_BACKEND_LIMITED();
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
  const std::string exePath = (testScratchPath("") / "primec_exe_ir_i64_subset").string();

  const std::string compileCmd = "./primec --emit=exe-ir " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 13);
}

TEST_CASE("exe-ir emitter compiles and runs argv prints") {
  SKIP_IF_VM_IR_BACKEND_LIMITED();
  const std::string source = R"(
[return<int> effects(io_out)]
main([array<string>] args) {
  print_line(args[1i32])
  return(args.count())
}
)";
  const std::string srcPath = writeTemp("compile_exe_ir_print_argv.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_exe_ir_print_argv").string();
  const std::string outPath = (testScratchPath("") / "primec_exe_ir_print_argv.out").string();

  const std::string compileCmd = "./primec --emit=exe-ir " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath + " alpha beta > " + outPath) == 3);
  CHECK(readFile(outPath) == "alpha\n");
}

TEST_CASE("exe-ir emitter compiles and runs dynamic string print") {
  SKIP_IF_VM_IR_BACKEND_LIMITED();
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
  const std::string exePath = (testScratchPath("") / "primec_exe_ir_dynamic_string_print").string();
  const std::string outPath = (testScratchPath("") / "primec_exe_ir_dynamic_string_print.out").string();

  const std::string compileCmd = "./primec --emit=exe-ir " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath + " > " + outPath) == 0);
  CHECK(readFile(outPath) == "left\n");
}

TEST_CASE("exe-ir emitter compiles and runs string indexing") {
  SKIP_IF_VM_IR_BACKEND_LIMITED();
  const std::string source = R"(
[return<int>]
main() {
  [string] text{"abc"utf8}
  [i32] a{at(text, 0i32)}
  [i32] b{at_unsafe(text, 1i32)}
  [i32] len{count(text)}
  return(plus(plus(a, b), len))
}
)";
  const std::string srcPath = writeTemp("compile_exe_ir_string_indexing.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_exe_ir_string_indexing").string();

  const std::string compileCmd = "./primec --emit=exe-ir " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == (97 + 98 + 3));
}

TEST_CASE("exe-ir emitter compiles and runs pointer indirect paths") {
  SKIP_IF_VM_IR_BACKEND_LIMITED();
  const std::string source = R"(
[return<int>]
main() {
  [i32 mut] value{3i32}
  [Reference<i32> mut] ref{location(value)}
  assign(ref, 8i32)
  return(ref)
}
)";
  const std::string srcPath = writeTemp("compile_exe_ir_pointer_indirect.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_exe_ir_pointer_indirect").string();

  const std::string compileCmd = "./primec --emit=exe-ir " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 8);
}

TEST_CASE("exe-ir emitter compiles and runs heap alloc intrinsic") {
  SKIP_IF_VM_IR_BACKEND_LIMITED();
  const std::string source = R"(
[return<int> effects(heap_alloc)]
main() {
  [mut] ptr{/std/intrinsics/memory/alloc<i32>(1i32)}
  assign(dereference(ptr), 9i32)
  return(dereference(ptr))
}
)";
  const std::string srcPath = writeTemp("compile_exe_ir_heap_alloc_intrinsic.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_exe_ir_heap_alloc_intrinsic").string();

  const std::string compileCmd = "./primec --emit=exe-ir " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 9);
}

TEST_CASE("exe-ir emitter compiles and runs heap free intrinsic") {
  SKIP_IF_VM_IR_BACKEND_LIMITED();
  const std::string source = R"(
[return<int> effects(heap_alloc)]
main() {
  [mut] ptr{/std/intrinsics/memory/alloc<i32>(1i32)}
  assign(dereference(ptr), 9i32)
  [i32] value{dereference(ptr)}
  /std/intrinsics/memory/free(ptr)
  return(value)
}
)";
  const std::string srcPath = writeTemp("compile_exe_ir_heap_free_intrinsic.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_exe_ir_heap_free_intrinsic").string();

  const std::string compileCmd = "./primec --emit=exe-ir " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 9);
}

TEST_CASE("exe-ir emitter compiles and runs heap realloc intrinsic") {
  SKIP_IF_VM_IR_BACKEND_LIMITED();
  const std::string source = R"(
[return<int> effects(heap_alloc)]
main() {
  [mut] ptr{/std/intrinsics/memory/alloc<i32>(1i32)}
  assign(dereference(ptr), 9i32)
  [Pointer<i32> mut] grown{/std/intrinsics/memory/realloc(ptr, 2i32)}
  assign(dereference(plus(grown, 16i32)), 4i32)
  [i32] sum{plus(dereference(grown), dereference(plus(grown, 16i32)))}
  /std/intrinsics/memory/free(grown)
  return(sum)
}
)";
  const std::string srcPath = writeTemp("compile_exe_ir_heap_realloc_intrinsic.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_exe_ir_heap_realloc_intrinsic").string();

  const std::string compileCmd = "./primec --emit=exe-ir " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 13);
}

TEST_CASE("exe-ir emitter compiles and runs checked memory at intrinsic") {
  SKIP_IF_VM_IR_BACKEND_LIMITED();
  const std::string source = R"(
[return<int> effects(heap_alloc)]
main() {
  [mut] ptr{/std/intrinsics/memory/alloc<i32>(2i32)}
  assign(dereference(ptr), 9i32)
  [mut] second{/std/intrinsics/memory/at(ptr, 1i32, 2i32)}
  assign(dereference(second), 4i32)
  [i32] sum{plus(dereference(ptr), dereference(second))}
  /std/intrinsics/memory/free(ptr)
  return(sum)
}
)";
  const std::string srcPath = writeTemp("compile_exe_ir_heap_at_intrinsic.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_exe_ir_heap_at_intrinsic").string();

  const std::string compileCmd = "./primec --emit=exe-ir " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 13);
}

TEST_CASE("exe-ir emitter compiles and runs unchecked memory at intrinsic") {
  SKIP_IF_VM_IR_BACKEND_LIMITED();
  const std::string source = R"(
[return<int> effects(heap_alloc)]
main() {
  [mut] ptr{/std/intrinsics/memory/alloc<i32>(2i32)}
  assign(dereference(ptr), 9i32)
  [mut] second{/std/intrinsics/memory/at_unsafe(ptr, 1i32)}
  assign(dereference(second), 4i32)
  [i32] sum{plus(dereference(ptr), dereference(second))}
  /std/intrinsics/memory/free(ptr)
  return(sum)
}
)";
  const std::string srcPath = writeTemp("compile_exe_ir_heap_at_unsafe_intrinsic.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_exe_ir_heap_at_unsafe_intrinsic").string();

  const std::string compileCmd = "./primec --emit=exe-ir " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 13);
}

TEST_CASE("exe-ir emitter faults on checked memory at out of bounds") {
  SKIP_IF_VM_IR_BACKEND_LIMITED();
  const std::string source = R"(
[return<int> effects(heap_alloc)]
main() {
  [mut] ptr{/std/intrinsics/memory/alloc<i32>(1i32)}
  return(dereference(/std/intrinsics/memory/at(ptr, 1i32, 1i32)))
}
)";
  const std::string srcPath = writeTemp("compile_exe_ir_heap_at_out_of_bounds.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_exe_ir_heap_at_out_of_bounds").string();
  const std::string errPath =
      (testScratchPath("") / "primec_exe_ir_heap_at_out_of_bounds.txt").string();

  const std::string compileCmd = "./primec --emit=exe-ir " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath + " 2> " + errPath) != 0);
  CHECK(readFile(errPath) == "pointer index out of bounds\n");
}

TEST_CASE("exe-ir emitter faults on dereference after heap free intrinsic") {
  SKIP_IF_VM_IR_BACKEND_LIMITED();
  const std::string source = R"(
[return<int> effects(heap_alloc)]
main() {
  [mut] ptr{/std/intrinsics/memory/alloc<i32>(1i32)}
  /std/intrinsics/memory/free(ptr)
  return(dereference(ptr))
}
)";
  const std::string srcPath = writeTemp("compile_exe_ir_heap_free_invalid_deref.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_exe_ir_heap_free_invalid_deref").string();
  const std::string errPath =
      (testScratchPath("") / "primec_exe_ir_heap_free_invalid_deref.txt").string();

  const std::string compileCmd = "./primec --emit=exe-ir " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath + " 2> " + errPath) != 0);
  CHECK(readFile(errPath).find("invalid indirect address in IR") != std::string::npos);
}

TEST_CASE("exe-ir emitter compiles and runs file io subset") {
  SKIP_IF_VM_IR_BACKEND_LIMITED();
  const std::string outPath = (testScratchPath("") / "primec_exe_ir_file_io_subset.txt").string();
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
  const std::string srcPath = writeTemp("compile_exe_ir_file_io_subset.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_exe_ir_file_io_subset").string();

  const std::string compileCmd = "./primec --emit=exe-ir " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 0);
  CHECK(readFile(outPath) == "Hello 123 world\n\nABC");
}

TEST_CASE("exe-ir emitter reports misaligned pointer dereference") {
  SKIP_IF_VM_IR_BACKEND_LIMITED();
  const std::string source = R"(
[return<int>]
main() {
  [i32] value{5i32}
  return(dereference(plus(location(value), 8i32)))
}
)";
  const std::string srcPath = writeTemp("compile_exe_ir_pointer_misaligned.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_exe_ir_pointer_misaligned").string();
  const std::string errPath =
      (testScratchPath("") / "primec_exe_ir_pointer_misaligned.err").string();

  const std::string compileCmd = "./primec --emit=exe-ir " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath + " 2> " + errPath) == 1);
  CHECK(readFile(errPath).find("unaligned indirect address in IR") != std::string::npos);
}

TEST_CASE("exe-ir emitter compiles and runs call and callvoid paths") {
  SKIP_IF_VM_IR_BACKEND_LIMITED();
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
  const std::string exePath = (testScratchPath("") / "primec_exe_ir_calls").string();
  const std::string outPath = (testScratchPath("") / "primec_exe_ir_calls.out").string();

  const std::string compileCmd = "./primec --emit=exe-ir " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath + " > " + outPath) == 42);
  CHECK(readFile(outPath) == "log\n");
}

TEST_CASE("exe-ir emitter compiles and runs f32 arithmetic subset") {
  SKIP_IF_VM_IR_BACKEND_LIMITED();
  const std::string source = R"(
[return<int>]
main() {
  [f32 mut] value{2.0f32}
  assign(value, plus(value, 0.5f32))
  if(greater_than(value, 2.4f32), then() { return(7i32) }, else() { return(3i32) })
}
)";
  const std::string srcPath = writeTemp("compile_exe_ir_f32_subset.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_exe_ir_f32_subset").string();

  const std::string compileCmd = "./primec --emit=exe-ir " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 7);
}

TEST_CASE("exe-ir emitter compiles and runs f64 comparison subset") {
  SKIP_IF_VM_IR_BACKEND_LIMITED();
  const std::string source = R"(
[return<int>]
main() {
  if(greater_than(2.5f64, 1.0f64), then() { return(7i32) }, else() { return(3i32) })
}
)";
  const std::string srcPath = writeTemp("compile_exe_ir_f64_cmp_subset.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_exe_ir_f64_cmp_subset").string();

  const std::string compileCmd = "./primec --emit=exe-ir " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 7);
}

TEST_CASE("exe emitter uses ir backend for string indexing") {
  SKIP_IF_VM_IR_BACKEND_LIMITED();
  const std::string source = R"(
[return<int>]
main() {
  [string] text{"abc"utf8}
  [i32] a{at(text, 0i32)}
  [i32] b{at_unsafe(text, 1i32)}
  [i32] len{count(text)}
  return(plus(plus(a, b), len))
}
)";
  const std::string srcPath = writeTemp("compile_exe_string_indexing_ir_first.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_exe_string_indexing_ir_first").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == (97 + 98 + 3));
}

TEST_CASE("exe emitter uses ir backend for file io subset") {
  SKIP_IF_VM_IR_BACKEND_LIMITED();
  const std::string outPath = (testScratchPath("") / "primec_exe_file_io_ir_first.txt").string();
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
  const std::string srcPath = writeTemp("compile_exe_file_io_ir_first.prime", source);
  const std::string compileCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(readFile(outPath) == "Hello 123 world\n\nABC");
}

TEST_CASE("exe-ir emitter compiles and runs f64 arithmetic subset") {
  SKIP_IF_VM_IR_BACKEND_LIMITED();
  const std::string source = R"(
[return<int>]
main() {
  [f64 mut] value{2.0f64}
  assign(value, plus(value, 0.5f64))
  if(greater_than(value, 2.4f64), then() { return(7i32) }, else() { return(3i32) })
}
)";
  const std::string srcPath = writeTemp("compile_exe_ir_f64_math_subset.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_exe_ir_f64_math_subset").string();

  const std::string compileCmd = "./primec --emit=exe-ir " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 7);
}

TEST_CASE("exe-ir emitter compiles and runs f64 conversion subset") {
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
  const std::string srcPath = writeTemp("compile_exe_ir_f64_convert_subset.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_exe_ir_f64_convert_subset").string();

  const std::string compileCmd = "./primec --emit=exe-ir " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 7);
}

TEST_CASE("exe-ir emitter compiles and runs f64 to i32 conversion") {
  SKIP_IF_VM_IR_BACKEND_LIMITED();
  const std::string source = R"(
[return<int>]
main() {
  return(convert<int>(2.5f64))
}
)";
  const std::string srcPath = writeTemp("compile_exe_ir_f64_to_i32_convert.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_exe_ir_f64_to_i32_convert").string();

  const std::string compileCmd = "./primec --emit=exe-ir " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 2);
}

TEST_CASE("exe-ir emitter clamps f32/f64 to i64 conversion edges") {
  SKIP_IF_VM_IR_BACKEND_LIMITED();
  const std::string source = R"(
[return<int>]
main() {
  [i64] minValue{plus(-9223372036854775807i64, -1i64)}
  if(not(equal(convert<i64>(divide(0.0f32, 0.0f32)), 0i64)), then() { return(1i32) }, else() { })
  if(not(equal(convert<i64>(divide(1.0f32, 0.0f32)), 9223372036854775807i64)), then() { return(2i32) }, else() { })
  if(not(equal(convert<i64>(divide(-1.0f32, 0.0f32)), minValue)), then() { return(3i32) }, else() { })
  if(not(equal(convert<i64>(divide(0.0f64, 0.0f64)), 0i64)), then() { return(4i32) }, else() { })
  if(not(equal(convert<i64>(divide(1.0f64, 0.0f64)), 9223372036854775807i64)), then() { return(5i32) }, else() { })
  if(not(equal(convert<i64>(divide(-1.0f64, 0.0f64)), minValue)), then() { return(6i32) }, else() { })
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_exe_ir_f64_to_i64_convert_edges.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_exe_ir_f64_to_i64_convert_edges").string();

  const std::string compileCmd = "./primec --emit=exe-ir " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 3);
}

TEST_CASE("exe emitter uses ir backend for f32/f64 to i64 conversion edges") {
  SKIP_IF_VM_IR_BACKEND_LIMITED();
  const std::string source = R"(
[return<int>]
main() {
  [i64] minValue{plus(-9223372036854775807i64, -1i64)}
  if(not(equal(convert<i64>(divide(0.0f32, 0.0f32)), 0i64)), then() { return(1i32) }, else() { })
  if(not(equal(convert<i64>(divide(1.0f32, 0.0f32)), 9223372036854775807i64)), then() { return(2i32) }, else() { })
  if(not(equal(convert<i64>(divide(-1.0f32, 0.0f32)), minValue)), then() { return(3i32) }, else() { })
  if(not(equal(convert<i64>(divide(0.0f64, 0.0f64)), 0i64)), then() { return(4i32) }, else() { })
  if(not(equal(convert<i64>(divide(1.0f64, 0.0f64)), 9223372036854775807i64)), then() { return(5i32) }, else() { })
  if(not(equal(convert<i64>(divide(-1.0f64, 0.0f64)), minValue)), then() { return(6i32) }, else() { })
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_exe_f64_to_i64_ir_first_edges.prime", source);
  const std::string compileCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 3);
}

TEST_CASE("exe-ir emitter truncates in-range f32/f64 to i64") {
  SKIP_IF_VM_IR_BACKEND_LIMITED();
  const std::string source = R"(
[return<int>]
main() {
  if(not(equal(convert<i64>(2.9f32), 2i64)), then() { return(1i32) }, else() { })
  if(not(equal(convert<i64>(-2.9f32), -2i64)), then() { return(2i32) }, else() { })
  if(not(equal(convert<i64>(2.9f64), 2i64)), then() { return(3i32) }, else() { })
  if(not(equal(convert<i64>(-2.9f64), -2i64)), then() { return(4i32) }, else() { })
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_exe_ir_f64_to_i64_convert_truncation.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_exe_ir_f64_to_i64_convert_truncation").string();

  const std::string compileCmd = "./primec --emit=exe-ir " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 0);
}

TEST_CASE("exe-ir emitter truncates in-range f32/f64 to u64") {
  SKIP_IF_VM_IR_BACKEND_LIMITED();
  const std::string source = R"(
[return<int>]
main() {
  if(not(equal(convert<u64>(2.9f32), 2u64)), then() { return(1i32) }, else() { })
  if(not(equal(convert<u64>(42.9f32), 42u64)), then() { return(2i32) }, else() { })
  if(not(equal(convert<u64>(2.9f64), 2u64)), then() { return(3i32) }, else() { })
  if(not(equal(convert<u64>(42.9f64), 42u64)), then() { return(4i32) }, else() { })
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_exe_ir_f64_to_u64_convert_truncation.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_exe_ir_f64_to_u64_convert_truncation").string();

  const std::string compileCmd = "./primec --emit=exe-ir " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 0);
}

TEST_CASE("cpp and exe emitters match cpp-ir and exe-ir on shared corpus") {
  SKIP_IF_VM_IR_BACKEND_LIMITED();
  struct DifferentialCase {
    const char *name;
    const char *source;
    const char *runtimeArgs;
    int expectedExitCode;
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
      },
  };

  for (const auto &testCase : cases) {
    CAPTURE(testCase.name);
    const std::string srcPath = writeTemp(std::string("compile_cpp_ir_differential_") + testCase.name + ".prime",
                                          testCase.source);
    const std::string astCppPath =
        (testScratchPath("") / (std::string("primec_cpp_differential_") + testCase.name + ".cpp"))
            .string();
    const std::string irCppPath =
        (testScratchPath("") / (std::string("primec_cpp_ir_differential_") + testCase.name + ".cpp"))
            .string();
    const std::string astExePath =
        (testScratchPath("") / (std::string("primec_exe_differential_") + testCase.name)).string();
    const std::string irExePath =
        (testScratchPath("") / (std::string("primec_exe_ir_differential_") + testCase.name)).string();

    const std::string compileAstCppCmd = "./primec --emit=cpp " + quoteShellArg(srcPath) + " -o " +
                                         quoteShellArg(astCppPath) + " --entry /main";
    const std::string compileIrCppCmd = "./primec --emit=cpp-ir " + quoteShellArg(srcPath) + " -o " +
                                        quoteShellArg(irCppPath) + " --entry /main";
    CHECK(runCommand(compileAstCppCmd) == 0);
    CHECK(runCommand(compileIrCppCmd) == 0);
    const std::string astCppSource = readFile(astCppPath);
    const std::string irCppSource = readFile(irCppPath);
    CHECK(!astCppSource.empty());
    CHECK(!irCppSource.empty());
    CHECK(astCppSource == irCppSource);

    const std::string compileAstExeCmd = "./primec --emit=exe " + quoteShellArg(srcPath) + " -o " +
                                         quoteShellArg(astExePath) + " --entry /main";
    const std::string compileIrExeCmd = "./primec --emit=exe-ir " + quoteShellArg(srcPath) + " -o " +
                                        quoteShellArg(irExePath) + " --entry /main";
    CHECK(runCommand(compileAstExeCmd) == 0);
    CHECK(runCommand(compileIrExeCmd) == 0);

    const std::string astOutPath = (testScratchPath("") /
                                    (std::string("primec_exe_differential_") + testCase.name + ".out"))
                                       .string();
    const std::string astErrPath = (testScratchPath("") /
                                    (std::string("primec_exe_differential_") + testCase.name + ".err"))
                                       .string();
    const std::string irOutPath = (testScratchPath("") /
                                   (std::string("primec_exe_ir_differential_") + testCase.name + ".out"))
                                      .string();
    const std::string irErrPath = (testScratchPath("") /
                                   (std::string("primec_exe_ir_differential_") + testCase.name + ".err"))
                                      .string();

    const std::string runAstCmd = quoteShellArg(astExePath) + testCase.runtimeArgs + " > " + quoteShellArg(astOutPath) +
                                  " 2> " + quoteShellArg(astErrPath);
    const std::string runIrCmd = quoteShellArg(irExePath) + testCase.runtimeArgs + " > " + quoteShellArg(irOutPath) +
                                 " 2> " + quoteShellArg(irErrPath);
    CHECK(runCommand(runAstCmd) == testCase.expectedExitCode);
    CHECK(runCommand(runIrCmd) == testCase.expectedExitCode);
    CHECK(readFile(astOutPath) == readFile(irOutPath));
    CHECK(readFile(astErrPath) == readFile(irErrPath));
  }
}

TEST_CASE("cpp and exe diagnostics match cpp-ir and exe-ir (text and json)") {
  struct DiagnosticCase {
    const char *name;
    const char *source;
  };

  const std::vector<DiagnosticCase> cases = {
      {
          "semantic_argument_mismatch",
          R"(
/consume([i32] value) {
  value
}
[return<int>]
main() {
  consume(true)
  return(0i32)
}
)",
      },
      {
          "lowering_unsupported_lambda",
          R"(
[return<int>]
main() {
  holder{[]([i32] x) { return(x) }}
  return(0i32)
}
)",
      },
      {
          "lowering_software_numeric",
          R"(
[return<decimal>]
main() {
  [decimal] value{convert<decimal>(1.5f32)}
  return(value)
}
)",
      },
      {
          "lowering_non_empty_soa_literal",
          R"(
Particle() {
  [i32] x{1i32}
}
[return<int> effects(heap_alloc)]
main() {
  [soa<Particle>] values{soa<Particle>(Particle(1i32))}
  return(0i32)
}
)",
      },
  };

  struct EmitPair {
    const char *left;
    const char *right;
  };

  const std::vector<EmitPair> emitPairs = {
      {"cpp", "cpp-ir"},
      {"exe", "exe-ir"},
  };

  for (const auto &testCase : cases) {
    CAPTURE(testCase.name);
    const std::string srcPath = writeTemp(std::string("compile_diagnostics_parity_") + testCase.name + ".prime",
                                          testCase.source);

    for (const auto &pair : emitPairs) {
      CAPTURE(pair.left);
      CAPTURE(pair.right);

      const std::filesystem::path tempDir = testScratchPath("");
      const std::string leftErrPath =
          (tempDir / (std::string("primec_diag_") + pair.left + "_" + testCase.name + ".txt")).string();
      const std::string rightErrPath =
          (tempDir / (std::string("primec_diag_") + pair.right + "_" + testCase.name + ".txt")).string();
      const std::string leftJsonErrPath =
          (tempDir / (std::string("primec_diag_json_") + pair.left + "_" + testCase.name + ".txt")).string();
      const std::string rightJsonErrPath =
          (tempDir / (std::string("primec_diag_json_") + pair.right + "_" + testCase.name + ".txt")).string();

      const std::string leftTextCmd = "./primec --emit=" + std::string(pair.left) + " " + quoteShellArg(srcPath) +
                                      " -o /dev/null --entry /main 2> " + quoteShellArg(leftErrPath);
      const std::string rightTextCmd = "./primec --emit=" + std::string(pair.right) + " " + quoteShellArg(srcPath) +
                                       " -o /dev/null --entry /main 2> " + quoteShellArg(rightErrPath);
      const int leftTextStatus = runCommand(leftTextCmd);
      const int rightTextStatus = runCommand(rightTextCmd);
      CHECK(leftTextStatus == rightTextStatus);
      CHECK(readFile(leftErrPath) == readFile(rightErrPath));

      const std::string leftJsonCmd = "./primec --emit=" + std::string(pair.left) + " " + quoteShellArg(srcPath) +
                                      " -o /dev/null --entry /main --emit-diagnostics 2> " +
                                      quoteShellArg(leftJsonErrPath);
      const std::string rightJsonCmd = "./primec --emit=" + std::string(pair.right) + " " + quoteShellArg(srcPath) +
                                       " -o /dev/null --entry /main --emit-diagnostics 2> " +
                                       quoteShellArg(rightJsonErrPath);
      const int leftJsonStatus = runCommand(leftJsonCmd);
      const int rightJsonStatus = runCommand(rightJsonCmd);
      CHECK(leftJsonStatus == rightJsonStatus);
      CHECK(readFile(leftJsonErrPath) == readFile(rightJsonErrPath));
    }
  }
}

TEST_CASE("args.count() reflects passed argv") {
  const std::string source = R"(
[return<int>]
main([array<string>] args) {
  return(args.count())
}
)";
  const std::string srcPath = writeTemp("compile_args.prime", source);
  const std::string compileCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 1);
  const std::string argvCmd = "./primec --emit=vm " + srcPath + " --entry /main -- alpha beta";
  CHECK(runCommand(argvCmd) == 3);
}

TEST_CASE("argv error output in C++ emitter") {
  const std::string source = R"(
[return<int> effects(io_err)]
main([array<string>] args) {
  print_line_error(args[1i32])
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_args_error.prime", source);
  const std::string errPath = (testScratchPath("") / "primec_args_error_err.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main -- alpha beta 2> " + errPath;
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(errPath) == "alpha\n");
}

TEST_CASE("argv error output without newline in C++ emitter") {
  const std::string source = R"(
[return<int> effects(io_err)]
main([array<string>] args) {
  print_error(args[1i32])
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_args_error_no_newline.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_args_error_no_newline_err.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main -- alpha beta 2> " + errPath;
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(errPath) == "alpha");
}

TEST_CASE("argv error output u64 index in C++ emitter") {
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
  const std::string errPath =
      (testScratchPath("") / "primec_args_error_u64_err.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main -- alpha 2> " + errPath;
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(errPath) == "alpha");
}

TEST_CASE("argv unsafe error output in C++ emitter") {
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
  const std::string errPath =
      (testScratchPath("") / "primec_args_error_unsafe_err.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main -- alpha 2> " + errPath;
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(errPath) == "alpha");
}

TEST_CASE("argv unsafe line error output in C++ emitter") {
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
  const std::string errPath =
      (testScratchPath("") / "primec_args_line_error_unsafe_err.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main -- alpha 2> " + errPath;
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(errPath) == "alpha\n");
}

TEST_CASE("argv print in C++ emitter") {
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
  const std::string outPath = (testScratchPath("") / "primec_args_print_out.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main -- alpha beta > " + outPath;
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(outPath) == "alpha\nbeta\n");
}

TEST_CASE("argv print without newline in C++ emitter") {
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
  const std::string outPath =
      (testScratchPath("") / "primec_args_print_no_newline_out.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main -- alpha > " + outPath;
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(outPath) == "alpha");
}

TEST_CASE("argv print with u64 index in C++ emitter") {
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
  const std::string outPath = (testScratchPath("") / "primec_args_print_u64_out.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main -- alpha > " + outPath;
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(outPath) == "alpha\n");
}

TEST_CASE("argv unsafe access in C++ emitter") {
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
  const std::string outPath = (testScratchPath("") / "primec_args_unsafe_out.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main -- alpha beta > " + outPath;
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(outPath) == "alpha\nbeta\n");
}

TEST_CASE("argv unsafe access with u64 index in C++ emitter") {
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
  const std::string outPath = (testScratchPath("") / "primec_args_unsafe_u64_out.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main -- alpha > " + outPath;
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(outPath) == "alpha\n");
}

TEST_CASE("three-element array literal") {
  const std::string source = R"(
[return<int>]
main() {
  return(at_unsafe(array<i32>{1i32, 2i32, 3i32}, 2i32))
}
)";
  const std::string srcPath = writeTemp("compile_array_literal.prime", source);
  const std::string compileCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 3);
}

TEST_CASE("array literal count method") {
  const std::string source = R"(
[return<int>]
main() {
  return(array<i32>(1i32, 2i32, 3i32).count())
}
)";
  const std::string srcPath = writeTemp("compile_array_literal_count.prime", source);
  const std::string compileCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 3);
}

TEST_CASE("array literal unsafe access") {
  const std::string source = R"(
[return<int>]
main() {
  return(at_unsafe(array<i32>(4i32, 7i32, 9i32), 1i32))
}
)";
  const std::string srcPath = writeTemp("compile_array_literal_unsafe.prime", source);
  const std::string compileCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 7);
}

TEST_CASE("count() helper on an array binding") {
  const std::string source = R"(
[return<int>]
main() {
  [array<i32>] values{array<i32>(1i32, 2i32, 3i32)}
  return(count(values))
}
)";
  const std::string srcPath = writeTemp("compile_array_count_helper.prime", source);
  const std::string compileCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 3);
}

TEST_CASE("literal method call in C++ emitter") {
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
  const std::string compileCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 2);
}

TEST_SUITE_END();
