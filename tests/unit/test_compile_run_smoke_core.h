TEST_SUITE_BEGIN("primestruct.compile.run.smoke");

TEST_CASE("compiles and runs simple main") {
  const std::string source = R"(
[return<int>]
main() {
  return(7i32)
}
)";
  const std::string srcPath = writeTemp("compile_simple.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_simple_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  const std::string runVmCmd = "./primevm " + srcPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 7);
  CHECK(runCommand(runVmCmd) == 7);
}

TEST_CASE("compiles and runs float arithmetic in VM") {
  const std::string source = R"(
[return<int>]
main() {
  [f32] a{1.5f32}
  [f32] b{2.0f32}
  [f32] c{multiply(plus(a, b), 2.0f32)}
  return(convert<int>(c))
}
)";
  const std::string srcPath = writeTemp("compile_float_vm.prime", source);

  const std::string runVmCmd = "./primevm " + srcPath + " --entry /main";
  CHECK(runCommand(runVmCmd) == 7);
}

TEST_CASE("compiles and runs primitive brace constructors") {
  const std::string source = R"(
[return<bool>]
truthy() {
  return(bool{ 35i32 })
}

[return<int>]
main() {
  return(convert<int>(truthy()))
}
)";
  const std::string srcPath = writeTemp("compile_brace_convert.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_brace_convert_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  const std::string runVmCmd = "./primevm " + srcPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 1);
  CHECK(runCommand(runVmCmd) == 1);
}

TEST_CASE("default entry path is main") {
  const std::string source = R"(
[return<int>]
main() {
  return(4i32)
}
)";
  const std::string srcPath = writeTemp("compile_default_entry.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_default_entry_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath;
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 4);

  const std::string runVmCmd = "./primevm " + srcPath;
  CHECK(runCommand(runVmCmd) == 4);
}

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
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_count_method_exe").string();
  const std::string nativePath = (std::filesystem::temp_directory_path() / "primec_count_method_native").string();

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
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_semicolon_sep_exe").string();
  const std::string nativePath = (std::filesystem::temp_directory_path() / "primec_semicolon_sep_native").string();

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
      (std::filesystem::temp_directory_path() / "primec_entry_bad_param_err.txt").string();
  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("entry definition must take a single array<string> parameter") != std::string::npos);
}

TEST_CASE("defaults to native output with stem name") {
  const std::string source = R"(
[return<int>]
main() {
  return(5i32)
}
)";
  const std::string srcPath = writeTemp("compile_default_output.prime", source);
  const std::filesystem::path outDir = std::filesystem::temp_directory_path() / "primec_default_out";
  std::error_code ec;
  std::filesystem::remove_all(outDir, ec);
  std::filesystem::create_directories(outDir, ec);
  REQUIRE(!ec);

  const std::string compileCmd =
      "./primec " + quoteShellArg(srcPath) + " --out-dir " + quoteShellArg(outDir.string()) + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);

  const std::filesystem::path outputPath = outDir / std::filesystem::path(srcPath).stem();
  CHECK(std::filesystem::exists(outputPath));
  CHECK(runCommand(quoteShellArg(outputPath.string())) == 5);
}

TEST_CASE("emits PSIR bytecode with --emit=ir") {
  const std::string source = R"(
[return<int>]
main() {
  return(3i32)
}
)";
  const std::string srcPath = writeTemp("compile_emit_ir.prime", source);
  const std::string irPath = (std::filesystem::temp_directory_path() / "primec_emit_ir.psir").string();

  const std::string compileCmd =
      "./primec --emit=ir " + quoteShellArg(srcPath) + " -o " + quoteShellArg(irPath) + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(std::filesystem::exists(irPath));

  std::ifstream file(irPath, std::ios::binary);
  REQUIRE(file.good());
  file.seekg(0, std::ios::end);
  std::streamsize size = file.tellg();
  file.seekg(0, std::ios::beg);
  std::vector<uint8_t> data;
  if (size > 0) {
    data.resize(static_cast<size_t>(size));
    file.read(reinterpret_cast<char *>(data.data()), size);
  }
  CHECK(!data.empty());
  REQUIRE(data.size() >= 8);
  auto readU32 = [&](size_t offset) -> uint32_t {
    return static_cast<uint32_t>(data[offset]) |
           (static_cast<uint32_t>(data[offset + 1]) << 8) |
           (static_cast<uint32_t>(data[offset + 2]) << 16) |
           (static_cast<uint32_t>(data[offset + 3]) << 24);
  };
  const uint32_t magic = readU32(0);
  const uint32_t version = readU32(4);
  CHECK(magic == 0x50534952u);
  CHECK(version == 14u);

  primec::IrModule module;
  std::string error;
  CHECK(primec::deserializeIr(data, module, error));
  CHECK(error.empty());
}

TEST_CASE("primevm forwards entry args") {
  const std::string source = R"(
[return<int> effects(io_out)]
main([array<string>] args) {
  print_line(args[1i32])
  return(args.count())
}
)";
  const std::string srcPath = writeTemp("vm_args.prime", source);
  const std::string outPath = (std::filesystem::temp_directory_path() / "primevm_args_out.txt").string();

  const std::string runVmCmd = "./primevm " + srcPath + " --entry /main -- alpha beta > " + outPath;
  CHECK(runCommand(runVmCmd) == 3);
  CHECK(readFile(outPath) == "alpha\n");
}

TEST_CASE("primevm supports argv string bindings") {
  const std::string source = R"(
[return<int> effects(io_out)]
main([array<string>] args) {
  [string] first{args[1i32]}
  print_line(first)
  return(args.count())
}
)";
  const std::string srcPath = writeTemp("primevm_args_binding.prime", source);
  const std::string outPath = (std::filesystem::temp_directory_path() / "primevm_args_binding_out.txt").string();

  const std::string runVmCmd = "./primevm " + srcPath + " --entry /main -- alpha beta > " + outPath;
  CHECK(runCommand(runVmCmd) == 3);
  CHECK(readFile(outPath) == "alpha\n");
}

TEST_CASE("compiles and runs with line comments after expressions") {
  const std::string source = R"(
[return<int>]
main() {
  [i32] value{7i32}// comment with a+b and a/b should be ignored
  return(value)
}
)";
  const std::string srcPath = writeTemp("compile_line_comment.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_line_comment_exe").string();
  const std::string nativePath = (std::filesystem::temp_directory_path() / "primec_line_comment_native").string();

  const std::string compileCppCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCppCmd) == 0);
  CHECK(runCommand(exePath) == 7);

  const std::string runVmCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runVmCmd) == 7);

  const std::string compileNativeCmd = "./primec --emit=native " + srcPath + " -o " + nativePath + " --entry /main";
  CHECK(runCommand(compileNativeCmd) == 0);
  CHECK(runCommand(nativePath) == 7);
}

TEST_CASE("compiles and runs string count and indexing in C++ emitter") {
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
  const std::string srcPath = writeTemp("compile_string_index.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_string_index_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == (97 + 98 + 3));
}

TEST_CASE("compiles and runs single-quoted strings in C++ emitter") {
  const std::string source = R"(
[return<int>]
main() {
  [string] text{'{"k":"v"}'utf8}
  [i32] k{at(text, 2i32)}
  [i32] v{at_unsafe(text, 6i32)}
  [i32] len{count(text)}
  return(plus(plus(k, v), len))
}
)";
  const std::string srcPath = writeTemp("compile_single_quoted_string.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_single_quoted_string_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == (107 + 118 + 9));
}

TEST_CASE("compiles and runs method calls via type namespaces") {
  const std::string source = R"(
namespace i32 {
  [return<int>]
  inc([i32] self) {
    return(plus(self, 1i32))
  }
}

[return<int>]
main() {
  return(1i32.inc())
}
)";
  const std::string srcPath = writeTemp("compile_method_call_i32.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_method_call_i32_exe").string();
  const std::string nativePath = (std::filesystem::temp_directory_path() / "primec_method_call_i32_native").string();

  const std::string compileCppCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCppCmd) == 0);
  CHECK(runCommand(exePath) == 2);

  const std::string runVmCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runVmCmd) == 2);

  const std::string compileNativeCmd = "./primec --emit=native " + srcPath + " -o " + nativePath + " --entry /main";
  CHECK(runCommand(compileNativeCmd) == 0);
  CHECK(runCommand(nativePath) == 2);
}

TEST_CASE("compiles and runs count forwarding to method") {
  const std::string source = R"(
namespace i32 {
  [return<int>]
  count([i32] self) {
    return(plus(self, 4i32))
  }
}

[return<int>]
main() {
  return(count(3i32))
}
)";
  const std::string srcPath = writeTemp("compile_count_forward.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_count_forward_exe").string();
  const std::string nativePath = (std::filesystem::temp_directory_path() / "primec_count_forward_native").string();

  const std::string compileCppCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCppCmd) == 0);
  CHECK(runCommand(exePath) == 7);

  const std::string runVmCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runVmCmd) == 7);

  const std::string compileNativeCmd = "./primec --emit=native " + srcPath + " -o " + nativePath + " --entry /main";
  CHECK(runCommand(compileNativeCmd) == 0);
  CHECK(runCommand(nativePath) == 7);
}

TEST_CASE("compiles and runs method call on constructor") {
  const std::string source = R"(
[struct]
Foo() {
  [i32] value{1i32}
}

[return<int>]
/Foo/ping([Foo] self) {
  return(9i32)
}

[return<int>]
main() {
  return(Foo().ping())
}
)";
  const std::string srcPath = writeTemp("compile_constructor_method.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_constructor_method_exe").string();
  const std::string nativePath = (std::filesystem::temp_directory_path() / "primec_constructor_method_native").string();

  const std::string compileCppCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCppCmd) == 0);
  CHECK(runCommand(exePath) == 9);

  const std::string runVmCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runVmCmd) == 9);

  const std::string compileNativeCmd = "./primec --emit=native " + srcPath + " -o " + nativePath + " --entry /main";
  CHECK(runCommand(compileNativeCmd) == 0);
  CHECK(runCommand(nativePath) == 9);
}

TEST_CASE("compiles and runs call with body block") {
  const std::string source = R"(
[return<int>]
sum([i32] a, [i32] b) {
  return(plus(a, b))
}

[return<int>]
main() {
  [i32 mut] value{0i32}
  [i32] total{ sum(2i32, 3i32) { assign(value, 7i32) } }
  return(plus(total, value))
}
)";
  const std::string srcPath = writeTemp("compile_call_body_block.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_call_body_block_exe").string();
  const std::string nativePath = (std::filesystem::temp_directory_path() / "primec_call_body_block_native").string();

  const std::string compileCppCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCppCmd) == 0);
  CHECK(runCommand(exePath) == 12);

  const std::string runVmCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runVmCmd) == 12);

  const std::string compileNativeCmd = "./primec --emit=native " + srcPath + " -o " + nativePath + " --entry /main";
  CHECK(runCommand(compileNativeCmd) == 0);
  CHECK(runCommand(nativePath) == 12);
}

TEST_CASE("compiles and runs templated method call") {
  const std::string source = R"(
namespace i32 {
  [return<int>]
  pick<T>([i32] self, [T] other) {
    return(self)
  }
}

[return<int>]
main() {
  return(3i32.pick<int>(4i32))
}
)";
  const std::string srcPath = writeTemp("compile_template_method.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_template_method_exe").string();
  const std::string nativePath = (std::filesystem::temp_directory_path() / "primec_template_method_native").string();

  const std::string compileCppCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCppCmd) == 0);
  CHECK(runCommand(exePath) == 3);

  const std::string runVmCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runVmCmd) == 3);

  const std::string compileNativeCmd = "./primec --emit=native " + srcPath + " -o " + nativePath + " --entry /main";
  CHECK(runCommand(compileNativeCmd) == 0);
  CHECK(runCommand(nativePath) == 3);
}

TEST_CASE("compiles and runs block expression") {
  const std::string source = R"(
[return<int>]
main() {
  return(block() {
    [i32] value{1i32}
    return(plus(value, 6i32))
  })
}
)";
  const std::string srcPath = writeTemp("compile_block_expr.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_block_expr_exe").string();
  const std::string nativePath = (std::filesystem::temp_directory_path() / "primec_block_expr_native").string();

  const std::string compileCppCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCppCmd) == 0);
  CHECK(runCommand(exePath) == 7);

  const std::string runVmCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runVmCmd) == 7);

  const std::string compileNativeCmd = "./primec --emit=native " + srcPath + " -o " + nativePath + " --entry /main";
  CHECK(runCommand(compileNativeCmd) == 0);
  CHECK(runCommand(nativePath) == 7);
}

TEST_CASE("compiles and runs boolean ops with conversions") {
  const std::string source = R"(
[return<bool>]
main() {
  return(and(convert<bool>(1i32), or(convert<bool>(0i32), not(convert<bool>(0i32)))))
}
)";
  const std::string srcPath = writeTemp("compile_bool_ops_int.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_bool_ops_int_exe").string();
  const std::string nativePath = (std::filesystem::temp_directory_path() / "primec_bool_ops_int_native").string();

  const std::string compileCppCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCppCmd) == 0);
  CHECK(runCommand(exePath) == 1);

  const std::string runVmCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runVmCmd) == 1);

  const std::string compileNativeCmd = "./primec --emit=native " + srcPath + " -o " + nativePath + " --entry /main";
  CHECK(runCommand(compileNativeCmd) == 0);
  CHECK(runCommand(nativePath) == 1);
}

TEST_CASE("compiles and runs integer width converts") {
  const std::string source = R"(
[return<bool>]
main() {
  return(equal(convert<u64>(-1i32), 18446744073709551615u64))
}
)";
  const std::string srcPath = writeTemp("compile_integer_width_convert.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_integer_width_convert_exe").string();
  const std::string nativePath =
      (std::filesystem::temp_directory_path() / "primec_integer_width_convert_native").string();

  const std::string compileCppCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCppCmd) == 0);
  CHECK(runCommand(exePath) == 1);

  const std::string runVmCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runVmCmd) == 1);

  const std::string compileNativeCmd = "./primec --emit=native " + srcPath + " -o " + nativePath + " --entry /main";
  CHECK(runCommand(compileNativeCmd) == 0);
  CHECK(runCommand(nativePath) == 1);
}

TEST_CASE("compiles and runs convert bool from negative integer") {
  const std::string source = R"(
[return<bool>]
main() {
  return(convert<bool>(-1i32))
}
)";
  const std::string srcPath = writeTemp("compile_convert_bool_negative.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_convert_bool_negative_exe").string();
  const std::string nativePath =
      (std::filesystem::temp_directory_path() / "primec_convert_bool_negative_native").string();

  const std::string compileCppCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCppCmd) == 0);
  CHECK(runCommand(exePath) == 1);

  const std::string runVmCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runVmCmd) == 1);

  const std::string compileNativeCmd = "./primec --emit=native " + srcPath + " -o " + nativePath + " --entry /main";
  CHECK(runCommand(compileNativeCmd) == 0);
  CHECK(runCommand(nativePath) == 1);
}

TEST_CASE("compiles and runs boolean ops short-circuit") {
  const std::string source = R"(
[return<int>]
main() {
  [bool mut] witness{false}
  or(true, assign(witness, true))
  and(false, assign(witness, true))
  return(convert<int>(witness))
}
)";
  const std::string srcPath = writeTemp("compile_bool_ops_short_circuit.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_bool_ops_short_circuit_exe").string();
  const std::string nativePath =
      (std::filesystem::temp_directory_path() / "primec_bool_ops_short_circuit_native").string();

  const std::string compileCppCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCppCmd) == 0);
  CHECK(runCommand(exePath) == 0);

  const std::string runVmCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runVmCmd) == 0);

  const std::string compileNativeCmd = "./primec --emit=native " + srcPath + " -o " + nativePath + " --entry /main";
  CHECK(runCommand(compileNativeCmd) == 0);
  CHECK(runCommand(nativePath) == 0);
}

