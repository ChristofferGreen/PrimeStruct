TEST_CASE("compiles and runs binding inference from if expression feeding method call") {
  const std::string source = R"(
namespace i64 {
  [return<i64>]
  inc([i64] self) {
    return(plus(self, 1i64))
  }
}

[return<int>]
main() {
  [mut] value{if(true) { 3i64 } else { 7i64 }}
  return(convert<i32>(value.inc()))
}
)";
  const std::string srcPath = writeTemp("compile_infer_binding_if_method.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() / "primec_infer_binding_if_method_exe").string();
  const std::string nativePath =
      (std::filesystem::temp_directory_path() / "primec_infer_binding_if_method_native").string();

  const std::string compileCppCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCppCmd) == 0);
  CHECK(runCommand(exePath) == 4);

  const std::string runVmCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runVmCmd) == 4);

  const std::string compileNativeCmd = "./primec --emit=native " + srcPath + " -o " + nativePath + " --entry /main";
  CHECK(runCommand(compileNativeCmd) == 0);
  CHECK(runCommand(nativePath) == 4);
}

TEST_CASE("compiles and runs binding inference from mixed if branches") {
  const std::string source = R"(
namespace i64 {
  [return<i64>]
  inc([i64] self) {
    return(plus(self, 1i64))
  }
}

[return<int>]
main() {
  [mut] value{if(false, then(){ 2i32 }, else(){ 5i64 })}
  return(convert<i32>(value.inc()))
}
)";
  const std::string srcPath = writeTemp("compile_infer_if_mixed_numeric.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_infer_if_mixed_exe").string();
  const std::string nativePath =
      (std::filesystem::temp_directory_path() / "primec_infer_if_mixed_native").string();

  const std::string compileCppCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCppCmd) == 0);
  CHECK(runCommand(exePath) == 6);

  const std::string runVmCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runVmCmd) == 6);

  const std::string compileNativeCmd = "./primec --emit=native " + srcPath + " -o " + nativePath + " --entry /main";
  CHECK(runCommand(compileNativeCmd) == 0);
  CHECK(runCommand(nativePath) == 6);
}

TEST_CASE("compiles and runs parameter inferring i64 from default initializer") {
  const std::string source = R"(
[return<i64>]
id([mut] value{3i64}) {
  return(value)
}

[return<int>]
main() {
  return(convert<i32>(id()))
}
)";
  const std::string srcPath = writeTemp("compile_infer_i64_param_default.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_infer_i64_param_default_exe").string();
  const std::string nativePath =
      (std::filesystem::temp_directory_path() / "primec_infer_i64_param_default_native").string();

  const std::string compileCppCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCppCmd) == 0);
  CHECK(runCommand(exePath) == 3);

  const std::string runVmCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runVmCmd) == 3);

  const std::string compileNativeCmd = "./primec --emit=native " + srcPath + " -o " + nativePath + " --entry /main";
  CHECK(runCommand(compileNativeCmd) == 0);
  CHECK(runCommand(nativePath) == 3);
}

TEST_CASE("compiles and runs map literal preserving assignment value") {
  const std::string source = R"(
[return<int>]
main() {
  [i32 mut] value{0i32}
  map<i32, i32>{1i32=value=2i32}
  return(value)
}
)";
  const std::string srcPath = writeTemp("compile_map_value_assign.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_map_value_assign_exe").string();
  const std::string nativePath = (std::filesystem::temp_directory_path() / "primec_map_value_assign_native").string();

  const std::string compileCppCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCppCmd) == 0);
  CHECK(runCommand(exePath) == 2);

  const std::string runVmCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runVmCmd) == 2);

  const std::string compileNativeCmd = "./primec --emit=native " + srcPath + " -o " + nativePath + " --entry /main";
  CHECK(runCommand(compileNativeCmd) == 0);
  CHECK(runCommand(nativePath) == 2);
}

TEST_CASE("C++ emitter array access checks bounds") {
  const std::string source = R"(
[return<int>]
main() {
  [array<i32>] values{array<i32>(4i32)}
  return(values[9i32])
}
)";
  const std::string srcPath = writeTemp("compile_cpp_array_bounds.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_cpp_array_bounds_exe").string();
  const std::string errPath = (std::filesystem::temp_directory_path() / "primec_cpp_array_bounds_err.txt").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string runCmd = exePath + " 2> " + errPath;
  CHECK(runCommand(runCmd) == 3);
  CHECK(readFile(errPath) == "array index out of bounds\n");
}

TEST_CASE("C++ emitter string access checks bounds") {
  const std::string source = R"(
[return<int>]
main() {
  [string] text{"abc"utf8}
  return(at(text, 9i32))
}
)";
  const std::string srcPath = writeTemp("compile_cpp_string_bounds.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_cpp_string_bounds_exe").string();
  const std::string errPath = (std::filesystem::temp_directory_path() / "primec_cpp_string_bounds_err.txt").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string runCmd = exePath + " 2> " + errPath;
  CHECK(runCommand(runCmd) == 3);
  CHECK(readFile(errPath) == "string index out of bounds\n");
}

TEST_CASE("runs program in vm") {
  const std::string source = R"(
[return<int>]
main() {
  return(7i32)
}
)";
  const std::string srcPath = writeTemp("vm_simple.prime", source);
  const std::string runVmCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runVmCmd) == 7);
}

TEST_CASE("runs vm with string count and indexing") {
  const std::string source = R"(
[return<int>]
main() {
  [string] text{"abc"utf8}
  [i32] a{text[0i32]}
  [i32] b{at_unsafe(text, 1i32)}
  [i32] len{text.count()}
  return(plus(plus(a, b), len))
}
)";
  const std::string srcPath = writeTemp("vm_string_index.prime", source);
  const std::string runVmCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runVmCmd) == (97 + 98 + 3));
}

TEST_CASE("runs vm with string literal count") {
  const std::string source = R"(
[return<int>]
main() {
  [i32] a{count("abc"utf8)}
  [i32] b{count("hi"utf8)}
  return(plus(a, b))
}
)";
  const std::string srcPath = writeTemp("vm_string_literal_count.prime", source);
  const std::string runVmCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runVmCmd) == (3 + 2));
}

TEST_CASE("runs vm with string literal method count") {
  const std::string source = R"(
[return<int>]
main() {
  return("hi"utf8.count())
}
)";
  const std::string srcPath = writeTemp("vm_string_literal_method_count.prime", source);
  const std::string runVmCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runVmCmd) == 2);
}

TEST_CASE("runs primevm with argv count") {
  const std::string source = R"(
[return<int>]
main([array<string>] args) {
  return(args.count())
}
)";
  const std::string srcPath = writeTemp("primevm_args_count.prime", source);
  const std::string runVmCmd = "./primevm " + srcPath + " --entry /main -- alpha beta";
  CHECK(runCommand(runVmCmd) == 3);
}

TEST_CASE("vm string access checks bounds") {
  const std::string source = R"(
[return<int>]
main() {
  [string] text{"abc"utf8}
  return(plus(100i32, text[9i32]))
}
)";
  const std::string srcPath = writeTemp("vm_string_bounds.prime", source);
  const std::string errPath = (std::filesystem::temp_directory_path() / "primec_vm_string_bounds_err.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main 2> " + errPath;
  CHECK(runCommand(runCmd) == 3);
  CHECK(readFile(errPath) == "string index out of bounds\n");
}

TEST_CASE("vm string access rejects negative index") {
  const std::string source = R"(
[return<int>]
main() {
  [string] text{"abc"utf8}
  return(plus(100i32, text[-1i32]))
}
)";
  const std::string srcPath = writeTemp("vm_string_negative.prime", source);
  const std::string errPath = (std::filesystem::temp_directory_path() / "primec_vm_string_negative_err.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main 2> " + errPath;
  CHECK(runCommand(runCmd) == 3);
  CHECK(readFile(errPath) == "string index out of bounds\n");
}

TEST_CASE("runs vm with argv printing") {
  const std::string source = R"(
[return<int> effects(io_out)]
main([array<string>] args) {
  print_line(args[0i32])
  return(args.count())
}
)";
  const std::string srcPath = writeTemp("vm_argv_print.prime", source);
  const std::string outPath = (std::filesystem::temp_directory_path() / "primec_vm_argv_print_out.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + outPath;
  CHECK(runCommand(runCmd) == 1);
  CHECK(readFile(outPath) == srcPath + "\n");
}

TEST_CASE("runs vm with argv printing without newline") {
  const std::string source = R"(
[return<int> effects(io_out)]
main([array<string>] args) {
  print(args[0i32])
  return(args.count())
}
)";
  const std::string srcPath = writeTemp("vm_argv_print_no_newline.prime", source);
  const std::string outPath =
      (std::filesystem::temp_directory_path() / "primec_vm_argv_print_no_newline_out.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + outPath;
  CHECK(runCommand(runCmd) == 1);
  CHECK(readFile(outPath) == srcPath);
}

TEST_CASE("runs vm with forwarded argv") {
  const std::string source = R"(
[return<int> effects(io_out)]
main([array<string>] args) {
  if(greater_than(args.count(), 1i32)) {
    print_line(args[1i32])
  } else {
    print_line("missing"utf8)
  }
  return(args.count())
}
)";
  const std::string srcPath = writeTemp("vm_forward_argv.prime", source);
  const std::string outPath = (std::filesystem::temp_directory_path() / "primec_vm_forward_argv_out.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main -- alpha beta > " + outPath;
  CHECK(runCommand(runCmd) == 3);
  CHECK(readFile(outPath) == "alpha\n");
}

TEST_CASE("runs vm with argv count helper") {
  const std::string source = R"(
[return<int>]
main([array<string>] args) {
  return(count(args))
}
)";
  const std::string srcPath = writeTemp("vm_argv_count_helper.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main -- alpha beta";
  CHECK(runCommand(runCmd) == 3);
}

TEST_CASE("runs vm with argv i64 index") {
  const std::string source = R"(
[return<int> effects(io_out)]
main([array<string>] args) {
  print_line(args[1i64])
  return(args.count())
}
)";
  const std::string srcPath = writeTemp("vm_argv_i64.prime", source);
  const std::string outPath = (std::filesystem::temp_directory_path() / "primec_vm_argv_i64_out.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main -- alpha beta > " + outPath;
  CHECK(runCommand(runCmd) == 3);
  CHECK(readFile(outPath) == "alpha\n");
}

TEST_CASE("runs vm with argv u64 index") {
  const std::string source = R"(
[return<int> effects(io_out)]
main([array<string>] args) {
  print_line(args[1u64])
  return(args.count())
}
)";
  const std::string srcPath = writeTemp("vm_argv_u64.prime", source);
  const std::string outPath = (std::filesystem::temp_directory_path() / "primec_vm_argv_u64_out.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main -- alpha beta > " + outPath;
  CHECK(runCommand(runCmd) == 3);
  CHECK(readFile(outPath) == "alpha\n");
}

TEST_CASE("runs vm with argv error output") {
  const std::string source = R"(
[return<int> effects(io_err)]
main([array<string>] args) {
  print_line_error(args[1i32])
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("vm_argv_error.prime", source);
  const std::string errPath = (std::filesystem::temp_directory_path() / "primec_vm_argv_error_err.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main -- alpha beta 2> " + errPath;
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(errPath) == "alpha\n");
}


TEST_CASE("runs vm with argv error output without newline") {
  const std::string source = R"(
[return<int> effects(io_err)]
main([array<string>] args) {
  print_error(args[1i32])
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("vm_argv_error_no_newline.prime", source);
  const std::string errPath =
      (std::filesystem::temp_directory_path() / "primec_vm_argv_error_no_newline_err.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main -- alpha beta 2> " + errPath;
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(errPath) == "alpha");
}

TEST_CASE("runs vm with argv error output u64 index") {
  const std::string source = R"(
[return<int> effects(io_err)]
main([array<string>] args) {
  print_error(args[1u64])
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("vm_argv_error_u64.prime", source);
  const std::string errPath =
      (std::filesystem::temp_directory_path() / "primec_vm_argv_error_u64_err.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main -- alpha beta 2> " + errPath;
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(errPath) == "alpha");
}

TEST_CASE("runs vm with argv line error output u64 index") {
  const std::string source = R"(
[return<int> effects(io_err)]
main([array<string>] args) {
  print_line_error(args[1u64])
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("vm_argv_line_error_u64.prime", source);
  const std::string errPath =
      (std::filesystem::temp_directory_path() / "primec_vm_argv_line_error_u64_err.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main -- alpha beta 2> " + errPath;
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(errPath) == "alpha\n");
}

TEST_CASE("runs vm with argv unsafe error output") {
  const std::string source = R"(
[return<int> effects(io_err)]
main([array<string>] args) {
  print_error(at_unsafe(args, 1i32))
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("vm_argv_error_unsafe.prime", source);
  const std::string errPath =
      (std::filesystem::temp_directory_path() / "primec_vm_argv_error_unsafe_err.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main -- alpha beta 2> " + errPath;
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(errPath) == "alpha");
}

TEST_CASE("runs vm with argv unsafe line error output") {
  const std::string source = R"(
[return<int> effects(io_err)]
main([array<string>] args) {
  print_line_error(at_unsafe(args, 1i32))
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("vm_argv_line_error_unsafe.prime", source);
  const std::string errPath =
      (std::filesystem::temp_directory_path() / "primec_vm_argv_line_error_unsafe_err.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main -- alpha beta 2> " + errPath;
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(errPath) == "alpha\n");
}

TEST_SUITE_END();
