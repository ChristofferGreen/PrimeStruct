#if defined(__APPLE__) && (defined(__arm64__) || defined(__aarch64__))
TEST_SUITE_BEGIN("primestruct.compile.run.native_backend.argv");

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

TEST_CASE("compiles and runs native string literal binding index") {
  const std::string source = R"(
[return<int>]
main() {
  [string] value{"hello"utf8}
  return(at(value, 4i32))
}
)";
  const std::string srcPath = writeTemp("compile_native_string_binding_index.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() / "primec_native_string_binding_index_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 111);
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

TEST_CASE("native argv string binding count fails") {
  const std::string source = R"(
[return<int>]
main([array<string>] args) {
  [string] value{args[0i32]}
  return(count(value))
}
)";
  const std::string srcPath = writeTemp("compile_native_argv_binding_count.prime", source);
  const std::string errPath =
      (std::filesystem::temp_directory_path() / "primec_native_argv_binding_count_err.txt").string();
  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath) ==
        "Semantic error: entry argument strings are only supported in print calls or string bindings\n");
}

TEST_CASE("native argv string binding index fails") {
  const std::string source = R"(
[return<int>]
main([array<string>] args) {
  [string] value{args[0i32]}
  return(value[0i32])
}
)";
  const std::string srcPath = writeTemp("compile_native_argv_binding_index.prime", source);
  const std::string errPath =
      (std::filesystem::temp_directory_path() / "primec_native_argv_binding_index_err.txt").string();
  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath) ==
        "Semantic error: entry argument strings are only supported in print calls or string bindings\n");
}

TEST_CASE("rejects native argv call argument unsafe") {
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
  const std::string errPath =
      (std::filesystem::temp_directory_path() / "primec_native_argv_call_unsafe_err.txt").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath) ==
        "Semantic error: entry argument strings are only supported in print calls or string bindings\n");
}

TEST_SUITE_END();
#endif
