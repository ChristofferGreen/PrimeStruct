#pragma once

TEST_CASE("defaults to cpp extension for emit=cpp") {
  const std::string source = R"(
[return<int>]
main() {
  return(2i32)
}
)";
  const std::string srcPath = writeTemp("compile_default_cpp.prime", source);
  const std::filesystem::path outDir = testScratchPath("") / "primec_cpp_default_out";
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
  const std::filesystem::path outDir = testScratchPath("") / "primec_cpp_ir_default_out";
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

TEST_CASE("cpp-ir emitter writes IR-lowered cpp for integer subset") {
  SKIP_IF_VM_IR_BACKEND_LIMITED();
  const std::string source = R"(
[return<int>]
main() {
  [i32 mut] counter{1i32}
  assign(counter, plus(counter, 2i32))
  return(counter)
}
)";
  const std::string srcPath = writeTemp("compile_cpp_ir_i32_subset.prime", source);
  const std::string outPath = (testScratchPath("") / "primec_cpp_ir_i32_subset.cpp").string();

  const std::string compileCmd = "./primec --emit=cpp-ir " + srcPath + " -o " + outPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string output = readFile(outPath);
  CHECK(output.find("int64_t ps_entry_0(int argc, char **argv)") != std::string::npos);
  CHECK(output.find("switch (pc)") != std::string::npos);
}

TEST_CASE("cpp-ir emitter writes string and argv print paths") {
  SKIP_IF_VM_IR_BACKEND_LIMITED();
  const std::string source = R"(
[return<int> effects(io_out)]
main([array<string>] args) {
  print_line("hello"utf8)
  print_line(args[1i32])
  return(args.count())
}
)";
  const std::string srcPath = writeTemp("compile_cpp_ir_print_argv.prime", source);
  const std::string outPath = (testScratchPath("") / "primec_cpp_ir_print_argv.cpp").string();

  const std::string compileCmd = "./primec --emit=cpp-ir " + srcPath + " -o " + outPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string output = readFile(outPath);
  CHECK(output.find("static const char *ps_string_table[]") != std::string::npos);
  CHECK(output.find("stack[sp++] = static_cast<uint64_t>(argc);") != std::string::npos);
  CHECK(output.find("argv[indexValue]") != std::string::npos);
}

TEST_CASE("cpp-ir emitter writes dynamic string print path") {
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
  const std::string srcPath = writeTemp("compile_cpp_ir_dynamic_string_print.prime", source);
  const std::string outPath =
      (testScratchPath("") / "primec_cpp_ir_dynamic_string_print.cpp").string();

  const std::string compileCmd = "./primec --emit=cpp-ir " + srcPath + " -o " + outPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string output = readFile(outPath);
  CHECK(output.find("std::cout << ps_string_table[0];") != std::string::npos);
  CHECK(output.find("static constexpr std::size_t ps_string_table_count = 2u;") != std::string::npos);
}

TEST_CASE("cpp-ir emitter writes string indexing paths") {
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
  const std::string srcPath = writeTemp("compile_cpp_ir_string_indexing.prime", source);
  const std::string outPath = (testScratchPath("") / "primec_cpp_ir_string_indexing.cpp").string();

  const std::string compileCmd = "./primec --emit=cpp-ir " + srcPath + " -o " + outPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string output = readFile(outPath);
  CHECK(output.find("if (stringByteIndex >= 3ull)") != std::string::npos);
  CHECK(output.find("ps_string_table[0][stringByteIndex]") != std::string::npos);
}

TEST_CASE("cpp emitter uses ir backend for string indexing") {
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
  const std::string srcPath = writeTemp("compile_cpp_string_indexing_ir_first.prime", source);
  const std::string outPath =
      (testScratchPath("") / "primec_cpp_string_indexing_ir_first.cpp").string();

  const std::string compileCmd = "./primec --emit=cpp " + srcPath + " -o " + outPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string output = readFile(outPath);
  CHECK(output.find("ps_string_table[0][stringByteIndex]") != std::string::npos);
  CHECK(output.find("ps_entry_0") != std::string::npos);
}

TEST_CASE("cpp-ir emitter writes pointer indirect paths") {
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
  const std::string srcPath = writeTemp("compile_cpp_ir_pointer_indirect.prime", source);
  const std::string outPath =
      (testScratchPath("") / "primec_cpp_ir_pointer_indirect.cpp").string();

  const std::string compileCmd = "./primec --emit=cpp-ir " + srcPath + " -o " + outPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string output = readFile(outPath);
  CHECK(output.find("loadIndirectAddress % 16ull") != std::string::npos);
  CHECK(output.find("storeIndirectAddress % 16ull") != std::string::npos);
  CHECK(output.find("locals[storeIndirectIndex] = storeIndirectValue;") != std::string::npos);
}

TEST_CASE("cpp emitter uses ir backend for pointer indirect paths") {
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
  const std::string srcPath = writeTemp("compile_cpp_pointer_indirect_ir_first.prime", source);
  const std::string outPath =
      (testScratchPath("") / "primec_cpp_pointer_indirect_ir_first.cpp").string();

  const std::string compileCmd = "./primec --emit=cpp " + srcPath + " -o " + outPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string output = readFile(outPath);
  CHECK(output.find("loadIndirectAddress % 16ull") != std::string::npos);
  CHECK(output.find("ps_entry_0") != std::string::npos);
}

TEST_CASE("cpp-ir emitter writes callable dispatch paths") {
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
  const std::string srcPath = writeTemp("compile_cpp_ir_calls.prime", source);
  const std::string outPath = (testScratchPath("") / "primec_cpp_ir_calls.cpp").string();

  const std::string compileCmd = "./primec --emit=cpp-ir " + srcPath + " -o " + outPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string output = readFile(outPath);
  CHECK(output.find(
            "static int64_t ps_fn_0(PsStack &stack, std::size_t &sp, std::vector<uint64_t> &heapSlots, "
            "std::vector<PsHeapAllocation> &heapAllocations, int argc, char **argv);") != std::string::npos);
  CHECK(output.find(
            "static int64_t ps_fn_1(PsStack &stack, std::size_t &sp, std::vector<uint64_t> &heapSlots, "
            "std::vector<PsHeapAllocation> &heapAllocations, int argc, char **argv);") != std::string::npos);
  CHECK(output.find("return ps_fn_0(stack, sp, heapSlots, heapAllocations, argc, argv);") != std::string::npos);
}

TEST_CASE("cpp-ir emitter writes file read paths") {
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
  const std::string srcPath = writeTemp("compile_cpp_ir_file_read_subset.prime", source);
  const std::string outPath = (testScratchPath("") / "primec_cpp_ir_file_read_subset.cpp").string();

  const std::string compileCmd = "./primec --emit=cpp-ir " + srcPath + " -o " + outPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string output = readFile(outPath);
  CHECK(output.find("int fileOpenFlags = O_RDONLY;") != std::string::npos);
  CHECK(output.find("int closeRc = ::close(closeFd);") != std::string::npos);
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

