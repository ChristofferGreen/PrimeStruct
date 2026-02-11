#if defined(__APPLE__) && (defined(__arm64__) || defined(__aarch64__))
TEST_CASE("compiles and runs native executable") {
  const std::string source = R"(
[return<int>]
main() {
  return(7i32)
}
)";
  const std::string srcPath = writeTemp("compile_native.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_native_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 7);
}

TEST_CASE("compiles and runs if expression in native backend") {
  const std::string source = R"(
[return<int>]
main() {
  return(if(false, 4i32, 9i32))
}
)";
  const std::string srcPath = writeTemp("compile_native_if_expr.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_native_if_expr_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 9);
}

TEST_CASE("compiles and runs native definition call") {
  const std::string source = R"(
[return<int>]
inc([i32] x) {
  return(plus(x, 1i32))
}

[return<int>]
main() {
  return(inc(6i32))
}
)";
  const std::string srcPath = writeTemp("compile_native_def_call.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_native_def_call_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 7);
}

TEST_CASE("compiles and runs native method call") {
  const std::string source = R"(
namespace i32 {
  [return<int>]
  inc([i32] value) {
    return(plus(value, 1i32))
  }
}

[return<int>]
main() {
  [i32] value{5i32}
  return(value.inc())
}
)";
  const std::string srcPath = writeTemp("compile_native_method_call.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_native_method_call_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 6);
}

TEST_CASE("compiles and runs native method count call") {
  const std::string source = R"(
namespace i32 {
  [return<int>]
  count([i32] value) {
    return(plus(value, 2i32))
  }
}

[return<int>]
main() {
  [i32] value{3i32}
  return(value.count())
}
)";
  const std::string srcPath = writeTemp("compile_native_method_count.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_native_method_count_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 5);
}

TEST_CASE("compiles and runs native literal method call") {
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
  const std::string srcPath = writeTemp("compile_native_method_literal.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_native_method_literal_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 2);
}

TEST_CASE("compiles and runs native chained method calls") {
  const std::string source = R"(
namespace i32 {
  [return<int>]
  inc([i32] value) {
    return(plus(value, 1i32))
  }

  [return<int>]
  dec([i32] value) {
    return(minus(value, 1i32))
  }
}

[return<int>]
make() {
  return(4i32)
}

[return<int>]
main() {
  return(make().inc().dec())
}
)";
  const std::string srcPath = writeTemp("compile_native_method_chain.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_native_method_chain_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 4);
}

TEST_CASE("compiles and runs native void call with string param") {
  const std::string source = R"(
[return<void> effects(io_out)]
echo([string] msg) {
  print_line(msg)
}

[return<int> effects(io_out)]
main([array<string>] args) {
  if(greater_than(args.count(), 1i32)) {
    echo(args[1i32])
  } else {
    echo("missing"utf8)
  }
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_native_string_call.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_native_string_call_exe").string();
  const std::string outPath = (std::filesystem::temp_directory_path() / "primec_native_string_call_out.txt").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string runCmd = exePath + " alpha > " + outPath;
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(outPath) == "alpha\n");
}

TEST_CASE("compiles and runs native argv count") {
  const std::string source = R"(
[return<int>]
main([array<string>] args) {
  return(args.count())
}
)";
  const std::string srcPath = writeTemp("compile_native_args.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_native_args_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 1);
  CHECK(runCommand(exePath + " alpha beta") == 3);
}

TEST_CASE("compiles and runs native argv count helper") {
  const std::string source = R"(
[return<int>]
main([array<string>] args) {
  return(count(args))
}
)";
  const std::string srcPath = writeTemp("compile_native_args_count_helper.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_native_args_count_helper_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 1);
  CHECK(runCommand(exePath + " alpha beta") == 3);
}

TEST_CASE("compiles and runs native argv error output") {
  const std::string source = R"(
[return<int> effects(io_err)]
main([array<string>] args) {
  print_line_error(args[1i32])
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_native_args_error.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_native_args_error_exe").string();
  const std::string errPath = (std::filesystem::temp_directory_path() / "primec_native_args_error_err.txt").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string runCmd = exePath + " alpha beta 2> " + errPath;
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(errPath) == "alpha\n");
}

TEST_CASE("compiles and runs native argv error output without newline") {
  const std::string source = R"(
[return<int> effects(io_err)]
main([array<string>] args) {
  print_error(args[1i32])
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_native_args_error_no_newline.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() / "primec_native_args_error_no_newline_exe").string();
  const std::string errPath =
      (std::filesystem::temp_directory_path() / "primec_native_args_error_no_newline_err.txt").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string runCmd = exePath + " alpha beta 2> " + errPath;
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(errPath) == "alpha");
}

TEST_CASE("compiles and runs native argv error output u64 index") {
  const std::string source = R"(
[return<int> effects(io_err)]
main([array<string>] args) {
  print_error(args[1u64])
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_native_args_error_u64.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() / "primec_native_args_error_u64_exe").string();
  const std::string errPath =
      (std::filesystem::temp_directory_path() / "primec_native_args_error_u64_err.txt").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string runCmd = exePath + " alpha 2> " + errPath;
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(errPath) == "alpha");
}

TEST_CASE("compiles and runs native argv unsafe error output") {
  const std::string source = R"(
[return<int> effects(io_err)]
main([array<string>] args) {
  print_error(at_unsafe(args, 1i32))
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_native_args_error_unsafe.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() / "primec_native_args_error_unsafe_exe").string();
  const std::string errPath =
      (std::filesystem::temp_directory_path() / "primec_native_args_error_unsafe_err.txt").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string runCmd = exePath + " alpha 2> " + errPath;
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(errPath) == "alpha");
}

TEST_CASE("compiles and runs native argv unsafe line error output") {
  const std::string source = R"(
[return<int> effects(io_err)]
main([array<string>] args) {
  print_line_error(at_unsafe(args, 1i32))
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_native_args_line_error_unsafe.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() / "primec_native_args_line_error_unsafe_exe").string();
  const std::string errPath =
      (std::filesystem::temp_directory_path() / "primec_native_args_line_error_unsafe_err.txt").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string runCmd = exePath + " alpha 2> " + errPath;
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(errPath) == "alpha\n");
}

TEST_CASE("compiles and runs native argv print") {
  const std::string source = R"(
[return<int> effects(io_out)]
main([array<string>] args) {
  if(greater_than(args.count(), 1i32)) {
    print_line(args[1i32])
  } else {
    print_line("missing"utf8)
  }
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_native_argv_print.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_native_argv_print_exe").string();
  const std::string outPath = (std::filesystem::temp_directory_path() / "primec_native_argv_print_out.txt").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string runCmd = exePath + " alpha > " + outPath;
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(outPath) == "alpha\n");
}

TEST_CASE("compiles and runs native argv print without newline") {
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
  const std::string srcPath = writeTemp("compile_native_argv_print_no_newline.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() / "primec_native_argv_print_no_newline_exe").string();
  const std::string outPath =
      (std::filesystem::temp_directory_path() / "primec_native_argv_print_no_newline_out.txt").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string runCmd = exePath + " alpha > " + outPath;
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(outPath) == "alpha");
}

TEST_CASE("compiles and runs native argv print with u64 index") {
  const std::string source = R"(
[return<int> effects(io_out)]
main([array<string>] args) {
  if(greater_than(args.count(), 1i32)) {
    print_line(args[1u64])
  } else {
    print_line("missing"utf8)
  }
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_native_argv_print_u64.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_native_argv_print_u64_exe").string();
  const std::string outPath =
      (std::filesystem::temp_directory_path() / "primec_native_argv_print_u64_out.txt").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string runCmd = exePath + " alpha > " + outPath;
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(outPath) == "alpha\n");
}

TEST_CASE("compiles and runs native argv access checks bounds") {
  const std::string source = R"(
[return<int> effects(io_out)]
main([array<string>] args) {
  print_line(args[9i32])
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_native_argv_bounds.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_native_argv_bounds_exe").string();
  const std::string errPath = (std::filesystem::temp_directory_path() / "primec_native_argv_bounds_err.txt").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string runCmd = exePath + " 2> " + errPath;
  CHECK(runCommand(runCmd) == 3);
  CHECK(readFile(errPath) == "array index out of bounds\n");
}

TEST_CASE("compiles and runs native argv access rejects negative index") {
  const std::string source = R"(
[return<int> effects(io_out)]
main([array<string>] args) {
  print_line(args[-1i32])
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_native_argv_negative.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_native_argv_negative_exe").string();
  const std::string errPath = (std::filesystem::temp_directory_path() / "primec_native_argv_negative_err.txt").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string runCmd = exePath + " 2> " + errPath;
  CHECK(runCommand(runCmd) == 3);
  CHECK(readFile(errPath) == "array index out of bounds\n");
}

TEST_CASE("compiles and runs native argv unsafe access skips bounds") {
  const std::string source = R"(
[return<int> effects(io_out)]
main([array<string>] args) {
  print_line(at_unsafe(args, 9i32))
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_native_argv_unsafe.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_native_argv_unsafe_exe").string();
  const std::string outPath = (std::filesystem::temp_directory_path() / "primec_native_argv_unsafe_out.txt").string();
  const std::string errPath = (std::filesystem::temp_directory_path() / "primec_native_argv_unsafe_err.txt").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string runCmd = exePath + " > " + outPath + " 2> " + errPath;
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(errPath).empty());
  CHECK(readFile(outPath).empty());
}

TEST_CASE("compiles and runs native argv unsafe access with u64 index") {
  const std::string source = R"(
[return<int> effects(io_out)]
main([array<string>] args) {
  print_line(at_unsafe(args, 1u64))
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_native_argv_unsafe_u64.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() / "primec_native_argv_unsafe_u64_exe").string();
  const std::string outPath =
      (std::filesystem::temp_directory_path() / "primec_native_argv_unsafe_u64_out.txt").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string runCmd = exePath + " alpha beta > " + outPath;
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(outPath) == "alpha\n");
}

TEST_CASE("compiles and runs native argv unsafe access skips negative index") {
  const std::string source = R"(
[return<int> effects(io_out)]
main([array<string>] args) {
  print_line(at_unsafe(args, -1i32))
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_native_argv_unsafe_negative.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() / "primec_native_argv_unsafe_negative_exe").string();
  const std::string outPath =
      (std::filesystem::temp_directory_path() / "primec_native_argv_unsafe_negative_out.txt").string();
  const std::string errPath =
      (std::filesystem::temp_directory_path() / "primec_native_argv_unsafe_negative_err.txt").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string runCmd = exePath + " > " + outPath + " 2> " + errPath;
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(errPath).empty());
  CHECK(readFile(outPath).empty());
}

TEST_CASE("compiles and runs native argv binding") {
  const std::string source = R"(
[return<int> effects(io_out)]
main([array<string>] args) {
  if(greater_than(args.count(), 1i32)) {
    [string] first{args[1i32]}
    [string] copy{first}
    print_line(copy)
  } else {
    print_line("missing"utf8)
  }
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_native_argv_binding.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_native_argv_binding_exe").string();
  const std::string outPath =
      (std::filesystem::temp_directory_path() / "primec_native_argv_binding_out.txt").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string runCmd = exePath + " alpha > " + outPath;
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(outPath) == "alpha\n");
}

TEST_CASE("compiles and runs native argv binding unsafe skips bounds") {
  const std::string source = R"(
[return<int> effects(io_out)]
main([array<string>] args) {
  [string] value{at_unsafe(args, 9i32)}
  print_line(value)
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_native_argv_binding_unsafe.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() / "primec_native_argv_binding_unsafe_exe").string();
  const std::string outPath =
      (std::filesystem::temp_directory_path() / "primec_native_argv_binding_unsafe_out.txt").string();
  const std::string errPath =
      (std::filesystem::temp_directory_path() / "primec_native_argv_binding_unsafe_err.txt").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string runCmd = exePath + " > " + outPath + " 2> " + errPath;
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(errPath).empty());
  CHECK(readFile(outPath).empty());
}

TEST_CASE("compiles and runs native argv unsafe binding copy skips bounds") {
  const std::string source = R"(
[return<int> effects(io_out)]
main([array<string>] args) {
  [string] first{at_unsafe(args, 9i32)}
  [string] second{first}
  print_line(second)
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_native_argv_binding_unsafe_copy.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() / "primec_native_argv_binding_unsafe_copy_exe").string();
  const std::string outPath =
      (std::filesystem::temp_directory_path() / "primec_native_argv_binding_unsafe_copy_out.txt").string();
  const std::string errPath =
      (std::filesystem::temp_directory_path() / "primec_native_argv_binding_unsafe_copy_err.txt").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string runCmd = exePath + " > " + outPath + " 2> " + errPath;
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(errPath).empty());
  CHECK(readFile(outPath).empty());
}

TEST_CASE("compiles and runs native argv call argument unsafe skips bounds") {
  const std::string source = R"(
[return<void> effects(io_out)]
echo([string] msg) {
  print_line(msg)
}

[return<int> effects(io_out)]
main([array<string>] args) {
  echo(at_unsafe(args, 9i32))
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_native_argv_call_unsafe.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_native_argv_call_unsafe_exe").string();
  const std::string outPath =
      (std::filesystem::temp_directory_path() / "primec_native_argv_call_unsafe_out.txt").string();
  const std::string errPath =
      (std::filesystem::temp_directory_path() / "primec_native_argv_call_unsafe_err.txt").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string runCmd = exePath + " > " + outPath + " 2> " + errPath;
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(errPath).empty());
  CHECK(readFile(outPath).empty());
}

TEST_CASE("compiles and runs native void executable") {
  const std::string source = R"(
[return<void>]
main() {
  [i32] value{1i32}
}
)";
  const std::string srcPath = writeTemp("compile_native_void.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_native_void_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 0);
}

TEST_CASE("compiles and runs native explicit void return") {
  const std::string source = R"(
[return<void>]
main() {
  return()
}
)";
  const std::string srcPath = writeTemp("compile_native_void_return.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_native_void_return_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 0);
}

TEST_CASE("compiles and runs native locals") {
  const std::string source = R"(
[return<int>]
main() {
  [i32 mut] value{2i32}
  assign(value, plus(value, 3i32))
  return(value)
}
)";
  const std::string srcPath = writeTemp("compile_native_locals.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_native_locals_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 5);
}

TEST_CASE("compiles and runs native if/else") {
  const std::string source = R"(
[return<int>]
main() {
  [i32] value{4i32}
  if(greater_equal(value, 4i32)) {
    return(9i32)
  } else {
    return(2i32)
  }
}
)";
  const std::string srcPath = writeTemp("compile_native_if.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_native_if_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 9);
}

TEST_CASE("compiles and runs native repeat loop") {
  const std::string source = R"(
[return<int>]
main() {
  [i32 mut] value{0i32}
  repeat(3i32) {
    assign(value, plus(value, 2i32))
  }
  return(value)
}
)";
  const std::string srcPath = writeTemp("compile_native_repeat_loop.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_native_repeat_loop_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 6);
}

TEST_CASE("compiles and runs native pointer helpers") {
  const std::string source = R"(
[return<int>]
main() {
  [i32 mut] value{1i32}
  [Pointer<i32> mut] ptr{location(value)}
  assign(dereference(ptr), 6i32)
  return(dereference(ptr))
}
)";
  const std::string srcPath = writeTemp("compile_native_ptr.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_native_ptr_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 6);
}

TEST_CASE("compiles and runs native pointer plus") {
  const std::string source = R"(
[return<int>]
main() {
  [i32] value{5i32}
  return(dereference(plus(location(value), 0i32)))
}
)";
  const std::string srcPath = writeTemp("compile_native_ptr_plus.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_native_ptr_plus_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 5);
}

TEST_CASE("compiles and runs native print output") {
  const std::string source = R"(
[return<int>]
main() {
  print(42i32)
  print_line("hello"utf8)
  print_error("oops"utf8)
  print_line_error(7i32)
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_native_print.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_native_print_exe").string();
  const std::string outPath = (std::filesystem::temp_directory_path() / "primec_native_print_out.txt").string();
  const std::string errPath = (std::filesystem::temp_directory_path() / "primec_native_print_err.txt").string();

  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main --default-effects=io_out,io_err";
  CHECK(runCommand(compileCmd) == 0);
  const std::string runCmd = exePath + " > " + outPath + " 2> " + errPath;
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(outPath) == "42hello\n");
  CHECK(readFile(errPath) == "oops7\n");
}

TEST_CASE("default effects token enables io output") {
  const std::string source = R"(
[return<int>]
main() {
  print_line("default effects"utf8)
  print_line_error("err"utf8)
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_native_print_default_effects.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_native_print_default_exe").string();
  const std::string outPath = (std::filesystem::temp_directory_path() / "primec_native_print_default_out.txt").string();
  const std::string errPath = (std::filesystem::temp_directory_path() / "primec_native_print_default_err.txt").string();

  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main --default-effects=default";
  CHECK(runCommand(compileCmd) == 0);
  const std::string runCmd = exePath + " > " + outPath + " 2> " + errPath;
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(outPath) == "default effects\n");
  CHECK(readFile(errPath) == "err\n");
}

TEST_CASE("default effects token enables vm output") {
  const std::string source = R"(
[return<int>]
main() {
  print_line("vm default effects"utf8)
  print_line_error("vm err"utf8)
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_vm_print_default_effects.prime", source);
  const std::string outPath = (std::filesystem::temp_directory_path() / "primec_vm_print_default_out.txt").string();
  const std::string errPath = (std::filesystem::temp_directory_path() / "primec_vm_print_default_err.txt").string();

  const std::string runCmd =
      "./primec --emit=vm " + srcPath + " --entry /main --default-effects=default > " + outPath + " 2> " + errPath;
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(outPath) == "vm default effects\n");
  CHECK(readFile(errPath) == "vm err\n");
}

TEST_CASE("compiles and runs native string binding print") {
  const std::string source = R"(
[return<int> effects(io_out)]
main() {
  [string] greeting{"hi"ascii}
  print_line(greeting)
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_native_string_print.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_native_string_print_exe").string();
  const std::string outPath = (std::filesystem::temp_directory_path() / "primec_native_string_print_out.txt").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string runCmd = exePath + " > " + outPath;
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(outPath) == "hi\n");
}


TEST_CASE("compiles and runs native string binding copy") {
  const std::string source = R"(
[return<int> effects(io_out)]
main() {
  [string] greeting{"hey"utf8}
  [string] copy{greeting}
  print_line(copy)
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_native_string_copy.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_native_string_copy_exe").string();
  const std::string outPath = (std::filesystem::temp_directory_path() / "primec_native_string_copy_out.txt").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string runCmd = exePath + " > " + outPath;
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(outPath) == "hey\n");
}

TEST_CASE("compiles and runs native string count and indexing") {
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
  const std::string srcPath = writeTemp("compile_native_string_index.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_native_string_index_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == (97 + 98 + 3));
}

TEST_CASE("compiles and runs native string access checks bounds") {
  const std::string source = R"(
[return<int>]
main() {
  [string] text{"abc"utf8}
  return(plus(100i32, text[9i32]))
}
)";
  const std::string srcPath = writeTemp("compile_native_string_bounds.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_native_string_bounds_exe").string();
  const std::string errPath = (std::filesystem::temp_directory_path() / "primec_native_string_bounds_err.txt").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string runCmd = exePath + " 2> " + errPath;
  CHECK(runCommand(runCmd) == 3);
  CHECK(readFile(errPath) == "string index out of bounds\n");
}

TEST_CASE("compiles and runs native string access rejects negative index") {
  const std::string source = R"(
[return<int>]
main() {
  [string] text{"abc"utf8}
  return(plus(100i32, text[-1i32]))
}
)";
  const std::string srcPath = writeTemp("compile_native_string_negative.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_native_string_negative_exe").string();
  const std::string errPath = (std::filesystem::temp_directory_path() / "primec_native_string_negative_err.txt").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string runCmd = exePath + " 2> " + errPath;
  CHECK(runCommand(runCmd) == 3);
  CHECK(readFile(errPath) == "string index out of bounds\n");
}

TEST_CASE("compiles and runs native hello world example") {
  const std::filesystem::path repoRoot = std::filesystem::current_path().parent_path();
  const std::string srcPath = (repoRoot / "examples" / "hello_world.prime").string();
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_native_hello_world_exe").string();
  const std::string outPath = (std::filesystem::temp_directory_path() / "primec_native_hello_world_out.txt").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string runCmd = exePath + " > " + outPath;
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(outPath) == "Hello, world!\n");
}

TEST_CASE("compiles and runs native pointer plus offsets") {
  const std::string source = R"(
[return<int>]
main() {
  [i32] first{4i32}
  [i32] second{9i32}
  return(dereference(plus(location(first), 16i32)))
}
)";
  const std::string srcPath = writeTemp("compile_native_ptr_plus_offset.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_native_ptr_plus_offset_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 9);
}

TEST_CASE("compiles and runs native pointer plus on reference") {
  const std::string source = R"(
[return<int>]
main() {
  [i32] first{4i32}
  [i32] second{9i32}
  [Reference<i32>] ref{location(first)}
  return(dereference(plus(location(ref), 16i32)))
}
)";
  const std::string srcPath = writeTemp("compile_native_ptr_plus_ref.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_native_ptr_plus_ref_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 9);
}

TEST_CASE("compiles and runs native pointer minus offsets") {
  const std::string source = R"(
[return<int>]
main() {
  [i32] first{4i32}
  [i32] second{9i32}
  return(dereference(minus(location(second), 16i32)))
}
)";
  const std::string srcPath = writeTemp("compile_native_ptr_minus_offset.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_native_ptr_minus_offset_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 4);
}

TEST_CASE("compiles and runs native pointer minus u64 offsets") {
  const std::string source = R"(
[return<int>]
main() {
  [i32] first{4i32}
  [i32] second{9i32}
  return(dereference(minus(location(second), 16u64)))
}
)";
  const std::string srcPath = writeTemp("compile_native_ptr_minus_u64.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_native_ptr_minus_u64_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 4);
}

TEST_CASE("compiles and runs native pointer minus negative i64 offsets") {
  const std::string source = R"(
[return<int>]
main() {
  [i32] first{4i32}
  [i32] second{9i32}
  return(dereference(minus(location(first), -16i64)))
}
)";
  const std::string srcPath = writeTemp("compile_native_ptr_minus_neg_i64.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_native_ptr_minus_neg_i64_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 9);
}

TEST_CASE("compiles and runs native pointer plus u64 offsets") {
  const std::string source = R"(
[return<int>]
main() {
  [i32] first{4i32}
  [i32] second{9i32}
  return(dereference(plus(location(first), 16u64)))
}
)";
  const std::string srcPath = writeTemp("compile_native_ptr_plus_u64.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_native_ptr_plus_u64_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 9);
}

TEST_CASE("compiles and runs native pointer plus negative i64 offsets") {
  const std::string source = R"(
[return<int>]
main() {
  [i32] first{4i32}
  [i32] second{9i32}
  return(dereference(plus(location(second), -16i64)))
}
)";
  const std::string srcPath = writeTemp("compile_native_ptr_plus_neg_i64.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_native_ptr_plus_neg_i64_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 4);
}

TEST_CASE("compiles and runs native references") {
  const std::string source = R"(
[return<int>]
main() {
  [i32 mut] value{4i32}
  [Reference<i32> mut] ref{location(value)}
  assign(ref, 7i32)
  return(ref)
}
)";
  const std::string srcPath = writeTemp("compile_native_ref.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_native_ref_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 7);
}

TEST_CASE("compiles and runs native location on reference") {
  const std::string source = R"(
[return<int>]
main() {
  [i32 mut] value{8i32}
  [Reference<i32> mut] ref{location(value)}
  [Pointer<i32>] ptr{location(ref)}
  return(dereference(ptr))
}
)";
  const std::string srcPath = writeTemp("compile_native_ref_location.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_native_ref_location_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 8);
}

TEST_CASE("compiles and runs native reference arithmetic") {
  const std::string source = R"(
[return<int>]
main() {
  [i32 mut] value{4i32}
  [Reference<i32> mut] ref{location(value)}
  assign(ref, plus(ref, 3i32))
  return(ref)
}
)";
  const std::string srcPath = writeTemp("compile_native_ref_arith.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_native_ref_arith_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 7);
}

TEST_CASE("compiles and runs native clamp") {
  const std::string source = R"(
import /math
[return<int>]
main() {
  return(clamp(9i32, 2i32, 6i32))
}
)";
  const std::string srcPath = writeTemp("compile_native_clamp.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_native_clamp_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 6);
}

TEST_CASE("compiles and runs native clamp i64") {
  const std::string source = R"(
import /math
[return<bool>]
main() {
  return(equal(clamp(9i64, 2i64, 6i64), 6i64))
}
)";
  const std::string srcPath = writeTemp("compile_native_clamp_i64.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_native_clamp_i64_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 1);
}

TEST_CASE("compiles and runs native i64 arithmetic") {
  const std::string source = R"(
[return<bool>]
main() {
  return(equal(plus(4000000000i64, 2i64), 4000000002i64))
}
)";
  const std::string srcPath = writeTemp("compile_native_i64.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_native_i64_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 1);
}

TEST_CASE("compiles and runs native u64 division") {
  const std::string source = R"(
[return<bool>]
main() {
  return(equal(divide(10u64, 2u64), 5u64))
}
)";
  const std::string srcPath = writeTemp("compile_native_u64.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_native_u64_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 1);
}

TEST_CASE("compiles and runs native u64 comparison") {
  const std::string source = R"(
[return<bool>]
main() {
  return(greater_than(0xFFFFFFFFFFFFFFFFu64, 1u64))
}
)";
  const std::string srcPath = writeTemp("compile_native_u64_compare.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_native_u64_compare_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 1);
}

TEST_CASE("compiles and runs native bool return") {
  const std::string source = R"(
[return<bool>]
main() {
  return(true)
}
)";
  const std::string srcPath = writeTemp("compile_native_bool.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_native_bool_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 1);
}

TEST_CASE("compiles and runs native bool comparison with i32") {
  const std::string source = R"(
[return<bool>]
main() {
  return(equal(true, 1i32))
}
)";
  const std::string srcPath = writeTemp("compile_native_bool_compare.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() / "primec_native_bool_compare_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 1);
}

TEST_CASE("compiles and runs native implicit void main") {
  const std::string source = R"(
main() {
  [i32] value{1i32}
}
)";
  const std::string srcPath = writeTemp("compile_native_void_implicit.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_native_void_implicit_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 0);
}

TEST_CASE("compiles and runs native boolean ops") {
  const std::string source = R"(
[return<bool>]
main() {
  return(or(and(true, false), not(false)))
}
)";
  const std::string srcPath = writeTemp("compile_native_bool_ops.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_native_bool_ops_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 1);
}

TEST_CASE("compiles and runs native numeric boolean ops") {
  const std::string source = R"(
[return<bool>]
main() {
  return(or(and(0i32, 5i32), not(0i32)))
}
)";
  const std::string srcPath = writeTemp("compile_native_bool_numeric.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_native_bool_numeric_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 1);
}

TEST_CASE("compiles and runs native short-circuit and") {
  const std::string source = R"(
[return<int>]
main() {
  [i32 mut] value{1i32}
  [i32 mut] witness{0i32}
  assign(value, and(equal(value, 0i32), assign(witness, 9i32)))
  return(witness)
}
)";
  const std::string srcPath = writeTemp("compile_native_and_short.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_native_and_short_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 0);
}

TEST_CASE("compiles and runs native short-circuit or") {
  const std::string source = R"(
[return<int>]
main() {
  [i32 mut] value{1i32}
  [i32 mut] witness{0i32}
  assign(value, or(equal(value, 1i32), assign(witness, 9i32)))
  return(witness)
}
)";
  const std::string srcPath = writeTemp("compile_native_or_short.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_native_or_short_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 0);
}

TEST_CASE("compiles and runs native convert") {
  const std::string source = R"(
[return<int>]
main() {
  return(convert<int>(1i32))
}
)";
  const std::string srcPath = writeTemp("compile_native_convert.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_native_convert_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 1);
}

TEST_CASE("compiles and runs native convert bool") {
  const std::string source = R"(
[return<bool>]
main() {
  return(convert<bool>(0i32))
}
)";
  const std::string srcPath = writeTemp("compile_native_convert_bool.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_native_convert_bool_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 0);
}

TEST_CASE("compiles and runs native convert i64") {
  const std::string source = R"(
[return<bool>]
main() {
  return(equal(convert<i64>(9i64), 9i64))
}
)";
  const std::string srcPath = writeTemp("compile_native_convert_i64.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_native_convert_i64_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 1);
}

TEST_CASE("compiles and runs native convert u64") {
  const std::string source = R"(
[return<bool>]
main() {
  return(equal(convert<u64>(10u64), 10u64))
}
)";
  const std::string srcPath = writeTemp("compile_native_convert_u64.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_native_convert_u64_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 1);
}

TEST_CASE("rejects native float literals") {
  const std::string source = R"(
[return<int>]
main() {
  return(convert<int>(1.5f))
}
)";
  const std::string srcPath = writeTemp("compile_native_float_literal.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_native_float_literal_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 2);
}

TEST_CASE("rejects native float bindings") {
  const std::string source = R"(
[return<int>]
main() {
  [float] value{1.5f}
  return(1i32)
}
)";
  const std::string srcPath = writeTemp("compile_native_float_binding.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_native_float_binding_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 2);
}

TEST_CASE("rejects native non-literal string bindings") {
  const std::string source = R"(
[return<int>]
main() {
  [string] message{1i32}
  return(1i32)
}
)";
  const std::string srcPath = writeTemp("compile_native_string_binding.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_native_string_binding_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 2);
}

TEST_CASE("compiles and runs native array literals") {
  const std::string source = R"(
[return<int>]
main() {
  [array<i32>] values{array<i32>(4i32, 7i32, 9i32)}
  return(values[1i32])
}
)";
  const std::string srcPath = writeTemp("compile_native_array_literal.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_native_array_literal_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 7);
}

TEST_CASE("compiles and runs native array access with u64 index") {
  const std::string source = R"(
[return<int>]
main() {
  [array<i32>] values{array<i32>(4i32, 7i32, 9i32)}
  return(values[1u64])
}
)";
  const std::string srcPath = writeTemp("compile_native_array_u64.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_native_array_u64_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 7);
}

TEST_CASE("compiles and runs native array access rejects negative index") {
  const std::string source = R"(
[return<int>]
main() {
  [array<i32>] values{array<i32>(4i32)}
  return(plus(100i32, values[-1i32]))
}
)";
  const std::string srcPath = writeTemp("compile_native_array_negative.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_native_array_negative_exe").string();
  const std::string errPath = (std::filesystem::temp_directory_path() / "primec_native_array_negative_err.txt").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string runCmd = exePath + " 2> " + errPath;
  CHECK(runCommand(runCmd) == 3);
  CHECK(readFile(errPath) == "array index out of bounds\n");
}

TEST_CASE("compiles and runs native array unsafe access") {
  const std::string source = R"(
[return<int>]
main() {
  [array<i32>] values{array<i32>(4i32, 7i32, 9i32)}
  return(at_unsafe(values, 1i32))
}
)";
  const std::string srcPath = writeTemp("compile_native_array_unsafe.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_native_array_unsafe_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 7);
}

TEST_CASE("compiles and runs native array unsafe access with u64 index") {
  const std::string source = R"(
[return<int>]
main() {
  [array<i32>] values{array<i32>(4i32, 7i32, 9i32)}
  return(at_unsafe(values, 1u64))
}
)";
  const std::string srcPath = writeTemp("compile_native_array_unsafe_u64.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_native_array_unsafe_u64_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 7);
}

TEST_CASE("compiles and runs native array literal count method") {
  const std::string source = R"(
[return<int>]
main() {
  return(array<i32>(1i32, 2i32, 3i32).count())
}
)";
  const std::string srcPath = writeTemp("compile_native_array_literal_count.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() / "primec_native_array_literal_count_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 3);
}

TEST_CASE("compiles and runs native array literal unsafe access") {
  const std::string source = R"(
[return<int>]
main() {
  return(at_unsafe(array<i32>(4i32, 7i32, 9i32), 1i32))
}
)";
  const std::string srcPath = writeTemp("compile_native_array_literal_unsafe.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() / "primec_native_array_literal_unsafe_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 7);
}

TEST_CASE("compiles and runs native array count helper") {
  const std::string source = R"(
[return<int>]
main() {
  [array<i32>] values{array<i32>(1i32, 2i32, 3i32)}
  return(count(values))
}
)";
  const std::string srcPath = writeTemp("compile_native_array_count_helper.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() / "primec_native_array_count_helper_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 3);
}

TEST_CASE("compiles and runs native array literal count helper") {
  const std::string source = R"(
[return<int>]
main() {
  return(count(array<i32>(1i32, 2i32, 3i32)))
}
)";
  const std::string srcPath = writeTemp("compile_native_array_literal_count_helper.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() / "primec_native_array_literal_count_helper_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 3);
}

TEST_CASE("compiles and runs native vector literals") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(4i32, 7i32, 9i32)}
  return(values[1i32])
}
)";
  const std::string srcPath = writeTemp("compile_native_vector_literal.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_native_vector_literal_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 7);
}

TEST_CASE("compiles and runs native vector access checks bounds") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(4i32)}
  return(plus(100i32, values[9i32]))
}
)";
  const std::string srcPath = writeTemp("compile_native_vector_bounds.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_native_vector_bounds_exe").string();
  const std::string errPath = (std::filesystem::temp_directory_path() / "primec_native_vector_bounds_err.txt").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string runCmd = exePath + " 2> " + errPath;
  CHECK(runCommand(runCmd) == 3);
  CHECK(readFile(errPath) == "array index out of bounds\n");
}

TEST_CASE("compiles and runs native vector access rejects negative index") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(4i32)}
  return(plus(100i32, values[-1i32]))
}
)";
  const std::string srcPath = writeTemp("compile_native_vector_negative.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_native_vector_negative_exe").string();
  const std::string errPath =
      (std::filesystem::temp_directory_path() / "primec_native_vector_negative_err.txt").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string runCmd = exePath + " 2> " + errPath;
  CHECK(runCommand(runCmd) == 3);
  CHECK(readFile(errPath) == "array index out of bounds\n");
}

TEST_CASE("compiles and runs native vector literal count method") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  return(vector<i32>(1i32, 2i32, 3i32).count())
}
)";
  const std::string srcPath = writeTemp("compile_native_vector_literal_count.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() / "primec_native_vector_literal_count_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 3);
}

TEST_CASE("compiles and runs native vector method call") {
  const std::string source = R"(
[return<int>]
/vector/first([vector<i32>] items) {
  return(items[0i32])
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(4i32, 7i32, 9i32)}
  return(values.first())
}
)";
  const std::string srcPath = writeTemp("compile_native_vector_method_call.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() / "primec_native_vector_method_call_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 4);
}

TEST_CASE("compiles and runs native vector literal unsafe access") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  return(at_unsafe(vector<i32>(4i32, 7i32, 9i32), 1i32))
}
)";
  const std::string srcPath = writeTemp("compile_native_vector_literal_unsafe.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() / "primec_native_vector_literal_unsafe_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 7);
}

TEST_CASE("compiles and runs native vector count helper") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32, 3i32)}
  return(count(values))
}
)";
  const std::string srcPath = writeTemp("compile_native_vector_count_helper.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() / "primec_native_vector_count_helper_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 3);
}

TEST_CASE("compiles and runs native vector literal count helper") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  return(count(vector<i32>(1i32, 2i32, 3i32)))
}
)";
  const std::string srcPath = writeTemp("compile_native_vector_literal_count_helper.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() / "primec_native_vector_literal_count_helper_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 3);
}

TEST_CASE("compiles and runs native map literals") {
  const std::string source = R"(
[return<int>]
main() {
  map<i32, i32>{1i32=2i32}
  return(1i32)
}
)";
  const std::string srcPath = writeTemp("compile_native_map_literal.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_native_map_literal_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 1);
}

TEST_CASE("compiles and runs native map count helper") {
  const std::string source = R"(
[return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>{1i32=2i32, 3i32=4i32}}
  return(count(values))
}
)";
  const std::string srcPath = writeTemp("compile_native_map_count_helper.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_native_map_count_helper_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 2);
}

TEST_CASE("compiles and runs native map method call") {
  const std::string source = R"(
[return<int>]
/map/size([map<i32, i32>] items) {
  return(count(items))
}

[return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>{1i32=2i32, 3i32=4i32}}
  return(values.size())
}
)";
  const std::string srcPath = writeTemp("compile_native_map_method_call.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() / "primec_native_map_method_call_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 2);
}

TEST_CASE("compiles and runs native typed map binding") {
  const std::string source = R"(
[return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>{1i32=2i32, 3i32=4i32}}
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_native_map_binding.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_native_map_binding_exe").string();
  const std::string nativePath = (std::filesystem::temp_directory_path() / "primec_native_map_binding_native").string();

  const std::string compileCppCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCppCmd) == 0);
  CHECK(runCommand(exePath) == 0);

  const std::string runVmCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runVmCmd) == 0);

  const std::string compileNativeCmd = "./primec --emit=native " + srcPath + " -o " + nativePath + " --entry /main";
  CHECK(runCommand(compileNativeCmd) == 0);
  CHECK(runCommand(nativePath) == 0);
}

TEST_CASE("rejects native map literal odd args") {
  const std::string source = R"(
[return<int>]
main() {
  map<i32, i32>(1i32)
  return(1i32)
}
)";
  const std::string srcPath = writeTemp("compile_native_map_literal_odd.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_native_map_literal_odd_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 2);
}

TEST_CASE("rejects native map literal type mismatch") {
  const std::string source = R"(
[return<int>]
main() {
  map<i32, i32>(1i32, true)
  return(1i32)
}
)";
  const std::string srcPath = writeTemp("compile_native_map_literal_mismatch.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() / "primec_native_map_literal_mismatch_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 2);
}

TEST_CASE("rejects native string-keyed map literals") {
  const std::string source = R"(
[return<int>]
main() {
  map<string, i32>{"a"raw_utf8=1i32}
  return(1i32)
}
)";
  const std::string srcPath = writeTemp("compile_native_map_literal_string_key.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() / "primec_native_map_literal_string_key_exe").string();
  const std::string errPath =
      (std::filesystem::temp_directory_path() / "primec_native_map_literal_string_key_err.txt").string();

  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath) ==
        "Native lowering error: native backend only supports numeric/bool map literals\n");
}

TEST_CASE("rejects unsupported effects in native backend") {
  const std::string source = R"(
[return<int> effects(render_graph)]
main() {
  return(1i32)
}
)";
  const std::string srcPath = writeTemp("compile_native_unsupported_effect.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() / "primec_native_unsupported_effect_exe").string();
  const std::string errPath =
      (std::filesystem::temp_directory_path() / "primec_native_unsupported_effect_err.txt").string();

  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath) ==
        "Native lowering error: native backend does not support effect: render_graph on /main\n");
}

TEST_CASE("compiles and runs namespace entry") {
  const std::string source = R"(
namespace demo {
  [return<int>]
  main() {
    return(9i32)
  }
}
)";
  const std::string srcPath = writeTemp("compile_ns.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_ns_exe").string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /demo/main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 9);
}

TEST_CASE("compiles and runs include expansion") {
  const std::string libPath = writeTemp("compile_lib.prime", "[return<int>]\nhelper(){ return(5i32) }\n");
  const std::string source = "include<\"" + libPath + "\">\n[return<int>]\nmain(){ return(helper()) }\n";
  const std::string srcPath = writeTemp("compile_include.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_inc_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 5);
}

TEST_CASE("compiles and runs single-quoted include expansion") {
  const std::string libPath = writeTemp("compile_lib_single.prime", "[return<int>]\nhelper(){ return(6i32) }\n");
  const std::string source = "include<'" + libPath + "'>\n[return<int>]\nmain(){ return(helper()) }\n";
  const std::string srcPath = writeTemp("compile_include_single.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_inc_single_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 6);
}

TEST_CASE("compiles and runs with duplicate includes ignored") {
  const std::string libPath = writeTemp("compile_lib_dupe.prime", "[return<int>]\nhelper(){ return(5i32) }\n");
  const std::string source = "include<\"" + libPath + "\">\ninclude<\"" + libPath +
                             "\">\n[return<int>]\nmain(){ return(helper()) }\n";
  const std::string srcPath = writeTemp("compile_include_dupe.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_inc_dupe_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 5);
}

TEST_CASE("rejects include path with suffix") {
  const std::string srcPath =
      writeTemp("compile_include_suffix.prime", "include<\"/std/io\"utf8>\n[return<int>]\nmain(){ return(0i32) }\n");
  const std::string errPath = (std::filesystem::temp_directory_path() / "primec_include_suffix_err.txt").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath) == "Include error: include path cannot have suffix\n");
}

TEST_CASE("compiles and runs unquoted include expansion") {
  const std::filesystem::path includeRoot =
      std::filesystem::temp_directory_path() / "primec_tests" / "include_root_unquoted";
  std::filesystem::create_directories(includeRoot);
  {
    std::filesystem::create_directories(includeRoot / "lib");
    std::ofstream libFile(includeRoot / "lib" / "lib.prime");
    CHECK(libFile.good());
    libFile << "[return<int>]\nhelper(){ return(5i32) }\n";
    CHECK(libFile.good());
  }

  const std::string source = "include</lib>\n[return<int>]\nmain(){ return(helper()) }\n";
  const std::string srcPath = writeTemp("compile_unquoted_include.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_unquoted_inc_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath +
                                 " --entry /main --include-path " + includeRoot.string();
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 5);
}

TEST_CASE("compiles and runs versioned include expansion") {
  const std::filesystem::path includeRoot =
      std::filesystem::temp_directory_path() / "primec_tests" / "include_root_versioned";
  std::filesystem::remove_all(includeRoot);
  std::filesystem::create_directories(includeRoot);
  {
    std::filesystem::create_directories((includeRoot / "1.2.0" / "lib"));
    std::ofstream oldLib(includeRoot / "1.2.0" / "lib" / "lib.prime");
    CHECK(oldLib.good());
    oldLib << "[return<int>]\nhelper(){ return(5i32) }\n";
    CHECK(oldLib.good());
  }
  {
    std::filesystem::create_directories((includeRoot / "1.2.3" / "lib"));
    std::ofstream newLib(includeRoot / "1.2.3" / "lib" / "lib.prime");
    CHECK(newLib.good());
    newLib << "[return<int>]\nhelper(){ return(7i32) }\n";
    CHECK(newLib.good());
  }

  const std::string source =
      "include</lib, version=\"1.2\">\n"
      "[return<int>]\n"
      "main(){ return(helper()) }\n";
  const std::string srcPath = writeTemp("compile_versioned_include.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_versioned_inc_exe").string();
  const std::string nativePath = (std::filesystem::temp_directory_path() / "primec_versioned_inc_native").string();

  const std::string compileCppCmd = "./primec --emit=exe " + srcPath + " -o " + exePath +
                                    " --entry /main --include-path " + includeRoot.string();
  CHECK(runCommand(compileCppCmd) == 0);
  CHECK(runCommand(exePath) == 7);

  const std::string runVmCmd =
      "./primec --emit=vm " + srcPath + " --entry /main --include-path " + includeRoot.string();
  CHECK(runCommand(runVmCmd) == 7);

  const std::string compileNativeCmd = "./primec --emit=native " + srcPath + " -o " + nativePath +
                                       " --entry /main --include-path " + includeRoot.string();
  CHECK(runCommand(compileNativeCmd) == 0);
  CHECK(runCommand(nativePath) == 7);
}

#endif
