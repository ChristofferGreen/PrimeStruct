#if defined(__APPLE__) && (defined(__arm64__) || defined(__aarch64__))
TEST_SUITE_BEGIN("primestruct.compile.run.native_backend.core");
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

TEST_CASE("compiles and runs native float arithmetic") {
  const std::string source = R"(
[return<int>]
main() {
  [f32] a{1.5f32}
  [f32] b{2.0f32}
  [f32] c{multiply(plus(a, b), 2.0f32)}
  return(convert<int>(c))
}
)";
  const std::string srcPath = writeTemp("compile_native_float.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_native_float_exe").string();

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

TEST_SUITE_END();

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
        "Native lowering error: native backend only supports count() on string literals or string bindings\n");
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
        "Native lowering error: native backend only supports indexing into string literals or string bindings\n");
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

TEST_SUITE_END();

TEST_SUITE_BEGIN("primestruct.compile.run.native_backend.control");

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
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_native_print_default_effects.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_native_print_default_exe").string();
  const std::string outPath = (std::filesystem::temp_directory_path() / "primec_native_print_default_out.txt").string();

  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main --default-effects=default";
  CHECK(runCommand(compileCmd) == 0);
  const std::string runCmd = exePath + " > " + outPath;
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(outPath) == "default effects\n");
}

TEST_CASE("default effects token does not enable io_err output") {
  const std::string source = R"(
[return<int>]
main() {
  print_line_error("err"utf8)
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_native_default_effects_no_err.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() / "primec_native_default_effects_no_err_exe").string();
  const std::string errPath =
      (std::filesystem::temp_directory_path() / "primec_native_default_effects_no_err.txt").string();

  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main --default-effects=default 2> " +
      errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath) == "Semantic error: print_line_error requires io_err effect\n");
}

TEST_CASE("default effects token enables vm output") {
  const std::string source = R"(
[return<int>]
main() {
  print_line("vm default effects"utf8)
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_vm_print_default_effects.prime", source);
  const std::string outPath = (std::filesystem::temp_directory_path() / "primec_vm_print_default_out.txt").string();

  const std::string runCmd =
      "./primec --emit=vm " + srcPath + " --entry /main --default-effects=default > " + outPath;
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(outPath) == "vm default effects\n");
}

TEST_CASE("default effects allow capabilities in native") {
  const std::string source = R"(
[return<int> capabilities(io_out)]
main() {
  print_line("capabilities"utf8)
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_native_capabilities_default_effects.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() / "primec_native_capabilities_default_exe").string();
  const std::string outPath =
      (std::filesystem::temp_directory_path() / "primec_native_capabilities_default_out.txt").string();

  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main --default-effects=default";
  CHECK(runCommand(compileCmd) == 0);
  const std::string runCmd = exePath + " > " + outPath;
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(outPath) == "capabilities\n");
}

TEST_CASE("default effects none requires explicit effects") {
  const std::string source = R"(
[return<int>]
main() {
  print_line("no effects"utf8)
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_native_print_no_effects.prime", source);
  const std::string errPath = (std::filesystem::temp_directory_path() / "primec_print_no_effects_err.txt").string();

  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o /dev/null --entry /main --default-effects=none 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("print_line requires io_out effect") != std::string::npos);
}

TEST_CASE("compiles and runs native implicit utf8 strings") {
  const std::string source = R"(
[return<int> effects(io_out)]
main() {
  print_line("implicit")
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_native_implicit_utf8.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() / "primec_native_implicit_utf8_exe").string();
  const std::string outPath =
      (std::filesystem::temp_directory_path() / "primec_native_implicit_utf8_out.txt").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string runCmd = exePath + " > " + outPath;
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(outPath) == "implicit\n");
}

TEST_CASE("compiles and runs native implicit utf8 single-quoted strings") {
  const std::string source = R"(
[return<int> effects(io_out)]
main() {
  print_line('implicit')
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_native_implicit_utf8_single.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() / "primec_native_implicit_utf8_single_exe").string();
  const std::string outPath =
      (std::filesystem::temp_directory_path() / "primec_native_implicit_utf8_single_out.txt").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string runCmd = exePath + " > " + outPath;
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(outPath) == "implicit\n");
}

TEST_CASE("compiles and runs native escaped utf8 strings") {
  const std::string source = R"(
[return<int> effects(io_out)]
main() {
  print_line("line\nnext"utf8)
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_native_utf8_escaped.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() / "primec_native_utf8_escaped_exe").string();
  const std::string outPath =
      (std::filesystem::temp_directory_path() / "primec_native_utf8_escaped_out.txt").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string runCmd = exePath + " > " + outPath;
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(outPath) == "line\nnext\n");
}

TEST_CASE("compiles and runs native escaped utf8 single-quoted strings") {
  const std::string source = R"(
[return<int> effects(io_out)]
main() {
  print_line('line\nnext'utf8)
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_native_utf8_escaped_single.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() / "primec_native_utf8_escaped_single_exe").string();
  const std::string outPath =
      (std::filesystem::temp_directory_path() / "primec_native_utf8_escaped_single_out.txt").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string runCmd = exePath + " > " + outPath;
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(outPath) == "line\nnext\n");
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

TEST_CASE("compiles and runs native raw string literal output") {
  const std::string source = R"(
[return<int> effects(io_out)]
main() {
  print_line("line\\nnext"raw_utf8)
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_native_raw_string_literal.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() / "primec_native_raw_string_literal_exe").string();
  const std::string outPath =
      (std::filesystem::temp_directory_path() / "primec_native_raw_string_out.txt").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string runCmd = exePath + " > " + outPath;
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(outPath) == "line\\\\nnext\n");
}

TEST_CASE("compiles and runs native raw single-quoted string output") {
  const std::string source = R"(
[return<int> effects(io_out)]
main() {
  print_line('line\\nnext'raw_utf8)
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_native_raw_string_literal_single.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() / "primec_native_raw_string_literal_single_exe").string();
  const std::string outPath =
      (std::filesystem::temp_directory_path() / "primec_native_raw_string_single_out.txt").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string runCmd = exePath + " > " + outPath;
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(outPath) == "line\\\\nnext\n");
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

TEST_SUITE_END();

TEST_SUITE_BEGIN("primestruct.compile.run.native_backend.pointers");

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

TEST_SUITE_END();

TEST_SUITE_BEGIN("primestruct.compile.run.native_backend.math_numeric");

TEST_CASE("compiles and runs native clamp") {
  const std::string source = R"(
import /math/*
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
import /math/*
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

TEST_CASE("compiles and runs native math abs/sign/min/max") {
  const std::string source = R"(
import /math/*
[return<int>]
main() {
  [i32] a{abs(-5i32)}
  [i32] b{sign(-5i32)}
  [i32] c{min(7i32, 2i32)}
  [i32] d{max(7i32, 2i32)}
  return(plus(plus(a, b), plus(c, d)))
}
)";
  const std::string srcPath = writeTemp("compile_native_math_basic.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_native_math_basic_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 13);
}

TEST_CASE("compiles and runs native qualified math names") {
  const std::string source = R"(
[return<int>]
main() {
  [i32] a{/math/abs(-5i32)}
  [i32] b{/math/sign(-5i32)}
  [i32] c{/math/min(7i32, 2i32)}
  [i32] d{/math/max(7i32, 2i32)}
  [i32] e{convert<int>(/math/pi)}
  return(plus(plus(plus(a, b), plus(c, d)), e))
}
)";
  const std::string srcPath = writeTemp("compile_native_math_qualified.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() / "primec_native_math_qualified_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 16);
}

TEST_CASE("compiles and runs native math saturate/lerp") {
  const std::string source = R"(
import /math/*
[return<int>]
main() {
  [i32] a{saturate(-2i32)}
  [i32] b{saturate(2i32)}
  [i32] c{lerp(0i32, 10i32, 1i32)}
  return(plus(plus(a, b), c))
}
)";
  const std::string srcPath = writeTemp("compile_native_math_saturate_lerp.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() / "primec_native_math_saturate_lerp_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 11);
}

TEST_CASE("compiles and runs native math clamp") {
  const std::string source = R"(
import /math/*
[return<int>]
main() {
  [f32] a{clamp(2.5f32, 0.0f32, 1.0f32)}
  [u64] b{clamp(9u64, 2u64, 6u64)}
  return(plus(convert<int>(a), convert<int>(b)))
}
)";
  const std::string srcPath = writeTemp("compile_native_math_clamp.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() / "primec_native_math_clamp_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 7);
}

TEST_CASE("compiles and runs native math pow") {
  const std::string source = R"(
import /math/*
[return<int>]
main() {
  return(pow(2i32, 6i32))
}
)";
  const std::string srcPath = writeTemp("compile_native_math_pow.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() / "primec_native_math_pow_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 64);
}

TEST_CASE("compiles and runs native math pow rejects negative exponent") {
  const std::string source = R"(
import /math/*
[return<int>]
main() {
  return(pow(2i32, -1i32))
}
)";
  const std::string srcPath = writeTemp("compile_native_math_pow_negative.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() / "primec_native_math_pow_negative_exe").string();
  const std::string errPath =
      (std::filesystem::temp_directory_path() / "primec_native_math_pow_negative_err.txt").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string runCmd = exePath + " 2> " + errPath;
  CHECK(runCommand(runCmd) == 3);
  CHECK(readFile(errPath) == "pow exponent must be non-negative\n");
}

TEST_CASE("compiles and runs native math constant conversions") {
  const std::string source = R"(
import /math/*
[return<int>]
main() {
  return(plus(convert<int>(pi), plus(convert<int>(tau), convert<int>(e))))
}
)";
  const std::string srcPath = writeTemp("compile_native_math_constants_convert.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() / "primec_native_math_constants_convert_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 11);
}

TEST_CASE("compiles and runs native math constants") {
  const std::string source = R"(
import /math/*
[return<int>]
main() {
  [f64] sum{plus(pi, plus(tau, e))}
  return(convert<int>(sum))
}
)";
  const std::string srcPath = writeTemp("compile_native_math_constants_direct.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() / "primec_native_math_constants_direct_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 12);
}

TEST_CASE("compiles and runs native math predicates") {
  const std::string source = R"(
import /math/*
[return<int>]
main() {
  [f32] nan{divide(0.0f32, 0.0f32)}
  [f32] inf{divide(1.0f32, 0.0f32)}
  return(plus(
    plus(convert<int>(is_nan(nan)), convert<int>(is_inf(inf))),
    convert<int>(is_finite(1.0f32))
  ))
}
)";
  const std::string srcPath = writeTemp("compile_native_math_predicates.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() / "primec_native_math_predicates_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 3);
}

TEST_CASE("compiles and runs native math rounding") {
  const std::string source = R"(
import /math/*
[return<int>]
main() {
  [f32] a{floor(1.9f32)}
  [f32] b{ceil(1.1f32)}
  [f32] c{round(2.5f32)}
  [f32] d{trunc(-1.7f32)}
  [f32] e{fract(2.25f32)}
  return(plus(
    plus(convert<int>(a), convert<int>(b)),
    plus(convert<int>(c), plus(convert<int>(d), convert<int>(multiply(e, 4.0f32))))
  ))
}
)";
  const std::string srcPath = writeTemp("compile_native_math_rounding.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() / "primec_native_math_rounding_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 6);
}

TEST_CASE("compiles and runs native math roots") {
  const std::string source = R"(
import /math/*
[return<int>]
main() {
  [f32] a{sqrt(9.0f32)}
  [f32] b{cbrt(27.0f32)}
  return(plus(convert<int>(a), convert<int>(b)))
}
)";
  const std::string srcPath = writeTemp("compile_native_math_roots.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() / "primec_native_math_roots_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 6);
}

TEST_CASE("compiles and runs native math fma/hypot") {
  const std::string source = R"(
import /math/*
[return<int>]
main() {
  [f32] a{fma(2.0f32, 3.0f32, 4.0f32)}
  [f32] b{hypot(1.0f32, 1.0f32)}
  return(plus(convert<int>(a), convert<int>(b)))
}
)";
  const std::string srcPath = writeTemp("compile_native_math_fma_hypot.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() / "primec_native_math_fma_hypot_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 11);
}

TEST_CASE("compiles and runs native math copysign") {
  const std::string source = R"(
import /math/*
[return<int>]
main() {
  [f32] a{copysign(3.0f32, -1.0f32)}
  [f32] b{copysign(4.0f32, 1.0f32)}
  return(convert<int>(plus(a, b)))
}
)";
  const std::string srcPath = writeTemp("compile_native_math_copysign.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() / "primec_native_math_copysign_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 1);
}

TEST_CASE("compiles and runs native math angle helpers") {
  const std::string source = R"(
import /math/*
[return<int>]
main() {
  [f32] a{radians(90.0f32)}
  [f32] b{degrees(1.0f32)}
  return(plus(convert<int>(a), convert<int>(b)))
}
)";
  const std::string srcPath = writeTemp("compile_native_math_angles.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() / "primec_native_math_angles_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 58);
}

TEST_CASE("compiles and runs native math trig helpers") {
  const std::string source = R"(
import /math/*
[return<int>]
main() {
  [f32] a{sin(0.0f32)}
  [f32] b{cos(0.0f32)}
  [f32] c{tan(0.0f32)}
  [f32] d{atan2(1.0f32, 0.0f32)}
  [f32] sum{plus(plus(a, b), c)}
  return(plus(convert<int>(sum), convert<int>(d)))
}
)";
  const std::string srcPath = writeTemp("compile_native_math_trig.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() / "primec_native_math_trig_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 2);
}

TEST_CASE("compiles and runs native math arc trig helpers") {
  const std::string source = R"(
import /math/*
[return<int>]
main() {
  [f32] a{asin(0.0f32)}
  [f32] b{acos(0.0f32)}
  [f32] c{atan(1.0f32)}
  return(plus(plus(convert<int>(a), convert<int>(b)), convert<int>(c)))
}
)";
  const std::string srcPath = writeTemp("compile_native_math_arc_trig.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() / "primec_native_math_arc_trig_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 1);
}

TEST_CASE("compiles and runs native math exp/log") {
  const std::string source = R"(
import /math/*
[return<int>]
main() {
  [f32] a{exp(0.0f32)}
  [f32] b{exp2(1.0f32)}
  [f32] c{log(1.0f32)}
  [f32] d{log2(2.0f32)}
  [f32] e{log10(1.0f32)}
  [f32] sum{plus(plus(a, b), plus(c, plus(d, e)))}
  return(convert<int>(plus(sum, 0.5f32)))
}
)";
  const std::string srcPath = writeTemp("compile_native_math_exp_log.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() / "primec_native_math_exp_log_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 4);
}

TEST_CASE("compiles and runs native math hyperbolic") {
  const std::string source = R"(
import /math/*
[return<int>]
main() {
  [f32] a{sinh(0.0f32)}
  [f32] b{cosh(0.0f32)}
  [f32] c{tanh(0.0f32)}
  [f32] d{asinh(0.0f32)}
  [f32] e{acosh(1.0f32)}
  [f32] f{atanh(0.0f32)}
  [f32] sum{plus(plus(a, b), plus(c, plus(d, plus(e, f))))}
  return(convert<int>(sum))
}
)";
  const std::string srcPath = writeTemp("compile_native_math_hyperbolic.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() / "primec_native_math_hyperbolic_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 1);
}

TEST_CASE("compiles and runs native explicit math imports") {
  const std::string source = R"(
import /math/min /math/pi
[return<int>]
main() {
  return(plus(convert<int>(pi), min(7i32, 3i32)))
}
)";
  const std::string srcPath = writeTemp("compile_native_math_explicit_imports.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() / "primec_native_math_explicit_imports_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 6);
}

TEST_CASE("compiles and runs native float pow") {
  const std::string source = R"(
import /math/*
[return<int>]
main() {
  return(convert<int>(plus(pow(2.0f32, 3.0f32), 0.5f32)))
}
)";
  const std::string srcPath = writeTemp("compile_native_math_pow_float.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() / "primec_native_math_pow_float_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 8);
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

TEST_CASE("compiles and runs native convert bool from integers") {
  const std::string source = R"(
[return<int>]
main() {
  return(plus(
    convert<int>(convert<bool>(0i32)),
    plus(convert<int>(convert<bool>(-1i32)), convert<int>(convert<bool>(5u64)))
  ))
}
)";
  const std::string srcPath = writeTemp("compile_native_convert_bool_ints.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_native_convert_bool_ints_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 2);
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

TEST_CASE("compiles and runs native float literals") {
  const std::string source = R"(
[return<int>]
main() {
  return(convert<int>(1.5f))
}
)";
  const std::string srcPath = writeTemp("compile_native_float_literal.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_native_float_literal_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 1);
}

TEST_CASE("compiles and runs native float bindings") {
  const std::string source = R"(
[return<int>]
main() {
  [float] value{1.5f}
  [float] other{2.0f32}
  return(convert<int>(plus(value, other)))
}
)";
  const std::string srcPath = writeTemp("compile_native_float_binding.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_native_float_binding_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 3);
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

TEST_SUITE_END();

TEST_SUITE_BEGIN("primestruct.compile.run.native_backend.collections");

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

TEST_CASE("compiles and runs native vector capacity helpers") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32, 3i32)}
  [i32] a{capacity(values)}
  return(plus(a, values.capacity()))
}
)";
  const std::string srcPath = writeTemp("compile_native_vector_capacity_helper.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() / "primec_native_vector_capacity_helper_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 6);
}

TEST_CASE("compiles and runs native vector capacity after pop") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32, 2i32, 3i32)}
  pop(values)
  return(capacity(values))
}
)";
  const std::string srcPath = writeTemp("compile_native_vector_capacity_after_pop.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() / "primec_native_vector_capacity_after_pop_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 3);
}

TEST_CASE("compiles and runs native vector push helper") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32, 2i32, 3i32)}
  pop(values)
  reserve(values, 3i32)
  push(values, 9i32)
  return(plus(values[2i32], capacity(values)))
}
)";
  const std::string srcPath = writeTemp("compile_native_vector_push.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_native_vector_push_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 12);
}

TEST_CASE("rejects native vector reserve beyond capacity") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32, 2i32)}
  reserve(values, 3i32)
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_native_vector_reserve_unsupported.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() / "primec_native_vector_reserve_unsupported_exe").string();
  const std::string errPath =
      (std::filesystem::temp_directory_path() / "primec_native_vector_reserve_err.txt").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string runCmd = exePath + " 2> " + errPath;
  CHECK(runCommand(runCmd) == 3);
  CHECK(readFile(errPath) == "vector reserve exceeds capacity\n");
}

TEST_CASE("rejects native vector push beyond capacity") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32)}
  push(values, 2i32)
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_native_vector_push_full.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() / "primec_native_vector_push_full_exe").string();
  const std::string errPath =
      (std::filesystem::temp_directory_path() / "primec_native_vector_push_full_err.txt").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string runCmd = exePath + " 2> " + errPath;
  CHECK(runCommand(runCmd) == 3);
  CHECK(readFile(errPath) == "vector capacity exceeded\n");
}

TEST_CASE("compiles and runs native vector shrink helpers") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32, 2i32, 3i32, 4i32)}
  pop(values)
  remove_at(values, 1i32)
  remove_swap(values, 0i32)
  last{at(values, 0i32)}
  size{count(values)}
  clear(values)
  return(plus(plus(last, size), count(values)))
}
)";
  const std::string srcPath = writeTemp("compile_native_vector_shrink_helpers.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() / "primec_native_vector_shrink_helpers_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 4);
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

TEST_CASE("compiles and runs native collection bracket literals") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [array<i32>] values{array<i32>[1i32, 2i32]}
  [vector<i32>] list{vector<i32>[3i32, 4i32]}
  [map<i32, i32>] table{map<i32, i32>[5i32=6i32]}
  return(plus(plus(values.count(), list.count()), count(table)))
}
)";
  const std::string srcPath = writeTemp("compile_native_collection_brackets.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() / "primec_native_collection_brackets_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 5);
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

TEST_CASE("compiles and runs native map at helper") {
  const std::string source = R"(
[return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>{1i32=2i32, 3i32=4i32}}
  return(at(values, 3i32))
}
)";
  const std::string srcPath = writeTemp("compile_native_map_at.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_native_map_at_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 4);
}

TEST_CASE("compiles and runs native map at_unsafe helper") {
  const std::string source = R"(
[return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>{1i32=2i32, 3i32=4i32}}
  return(at_unsafe(values, 1i32))
}
)";
  const std::string srcPath = writeTemp("compile_native_map_at_unsafe.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_native_map_at_unsafe_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 2);
}

TEST_CASE("compiles and runs native bool map access helpers") {
  const std::string source = R"(
[return<int>]
main() {
  [map<bool, i32>] values{map<bool, i32>{true=1i32, false=2i32}}
  return(plus(at(values, true), at_unsafe(values, false)))
}
)";
  const std::string srcPath = writeTemp("compile_native_map_bool_access.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_native_map_bool_access_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 3);
}

TEST_CASE("compiles and runs native u64 map access helpers") {
  const std::string source = R"(
[return<int>]
main() {
  [map<u64, i32>] values{map<u64, i32>{2u64=7i32, 11u64=5i32}}
  return(plus(at(values, 2u64), at_unsafe(values, 11u64)))
}
)";
  const std::string srcPath = writeTemp("compile_native_map_u64_access.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_native_map_u64_access_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 12);
}

TEST_CASE("compiles and runs native map at missing key") {
  const std::string source = R"(
[return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>{1i32=2i32, 3i32=4i32}}
  return(at(values, 9i32))
}
)";
  const std::string srcPath = writeTemp("compile_native_map_at_missing.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_native_map_at_missing_exe").string();
  const std::string errPath =
      (std::filesystem::temp_directory_path() / "primec_native_map_at_missing_err.txt").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string runCmd = exePath + " 2> " + errPath;
  CHECK(runCommand(runCmd) == 3);
  CHECK(readFile(errPath) == "map key not found\n");
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

TEST_CASE("compiles and runs native string-keyed map literals") {
  const std::string source = R"(
[return<int>]
main() {
  [map<string, i32>] values{map<string, i32>("a"raw_utf8, 1i32, "b"raw_utf8, 2i32)}
  [i32] a{at(values, "b"raw_utf8)}
  [i32] b{at_unsafe(values, "a"raw_utf8)}
  return(plus(plus(a, b), count(values)))
}
)";
  const std::string srcPath = writeTemp("compile_native_map_literal_string_key.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() / "primec_native_map_literal_string_key_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 5);
}

TEST_CASE("compiles and runs native string-keyed map binding lookup") {
  const std::string source = R"(
[return<int>]
main() {
  [map<string, i32>] values{map<string, i32>("a"raw_utf8, 1i32, "b"raw_utf8, 2i32)}
  [string] key{"b"raw_utf8}
  return(plus(at(values, key), count(values)))
}
)";
  const std::string srcPath = writeTemp("compile_native_map_literal_string_binding.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() / "primec_native_map_literal_string_binding_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 4);
}

TEST_CASE("rejects native map lookup with argv string key") {
  const std::string source = R"(
[return<int>]
main([array<string>] args) {
  [map<string, i32>] values{map<string, i32>("a"raw_utf8, 1i32)}
  [string] key{args[0i32]}
  return(at_unsafe(values, key))
}
)";
  const std::string srcPath = writeTemp("compile_native_map_lookup_argv_key.prime", source);
  const std::string errPath =
      (std::filesystem::temp_directory_path() / "primec_native_map_lookup_argv_key_err.txt").string();
  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("map lookup key") != std::string::npos);
}

TEST_CASE("rejects native map literal string key from argv binding") {
  const std::string source = R"(
[return<int>]
main([array<string>] args) {
  [string] key{args[0i32]}
  map<string, i32>(key, 1i32)
  return(1i32)
}
)";
  const std::string srcPath = writeTemp("compile_native_map_literal_string_argv_key.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() / "primec_native_map_literal_string_argv_key_exe").string();
  const std::string errPath =
      (std::filesystem::temp_directory_path() / "primec_native_map_literal_string_argv_key_err.txt").string();

  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("map literal string keys") != std::string::npos);
}

TEST_SUITE_END();

TEST_SUITE_BEGIN("primestruct.compile.run.native_backend.imports");

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

TEST_CASE("rejects unsupported default effects in native backend") {
  const std::string source = R"(
[return<int>]
main() {
  return(1i32)
}
)";
  const std::string srcPath = writeTemp("compile_native_unsupported_default_effect.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() / "primec_native_unsupported_default_effect_exe").string();
  const std::string errPath =
      (std::filesystem::temp_directory_path() / "primec_native_unsupported_default_effect_err.txt").string();

  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main --default-effects=render_graph 2> " +
      errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath) ==
        "Native lowering error: native backend does not support effect: render_graph on /main\n");
}

TEST_CASE("rejects unsupported default effects in vm backend") {
  const std::string source = R"(
[return<int>]
main() {
  return(1i32)
}
)";
  const std::string srcPath = writeTemp("compile_vm_unsupported_default_effect.prime", source);
  const std::string errPath =
      (std::filesystem::temp_directory_path() / "primec_vm_unsupported_default_effect_err.txt").string();

  const std::string compileCmd =
      "./primec --emit=vm " + srcPath + " --entry /main --default-effects=render_graph 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath) ==
        "VM lowering error: vm backend does not support effect: render_graph on /main\n");
}

TEST_CASE("rejects unsupported execution effects in native backend") {
  const std::string source = R"(
[return<int>]
main() {
  [effects(render_graph)] plus(1i32, 2i32)
  return(1i32)
}
)";
  const std::string srcPath = writeTemp("compile_native_unsupported_exec_effect.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() / "primec_native_unsupported_exec_effect_exe").string();
  const std::string errPath =
      (std::filesystem::temp_directory_path() / "primec_native_unsupported_exec_effect_err.txt").string();

  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main --default-effects=render_graph 2> " +
      errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath) ==
        "Native lowering error: native backend does not support effect: render_graph on /main\n");
}

TEST_CASE("rejects unsupported execution effects in vm backend") {
  const std::string source = R"(
[return<int>]
main() {
  [effects(render_graph)] plus(1i32, 2i32)
  return(1i32)
}
)";
  const std::string srcPath = writeTemp("compile_vm_unsupported_exec_effect.prime", source);
  const std::string errPath =
      (std::filesystem::temp_directory_path() / "primec_vm_unsupported_exec_effect_err.txt").string();

  const std::string compileCmd =
      "./primec --emit=vm " + srcPath + " --entry /main --default-effects=render_graph 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath) ==
        "VM lowering error: vm backend does not support effect: render_graph on /main\n");
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

TEST_CASE("compiles and runs native import alias in namespace") {
  const std::string source = R"(
import /util
namespace util {
  [return<int>]
  helper() {
    return(7i32)
  }
}
namespace demo {
  [return<int>]
  main() {
    return(helper())
  }
}
)";
  const std::string srcPath = writeTemp("compile_import_alias_namespace.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_import_alias_namespace").string();

  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /demo/main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 7);
}

TEST_CASE("compiles and runs native import alias") {
  const std::string source = R"(
import /util
namespace util {
  [return<int>]
  inc([i32] value) {
    return(plus(value, 1i32))
  }
}
[return<int>]
main() {
  return(inc(4i32))
}
)";
  const std::string srcPath = writeTemp("compile_native_import_alias.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_import_alias_exe").string();

  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 5);
}

TEST_CASE("compiles and runs native with multiple imports") {
  const std::string source = R"(
import /util, /math/*
namespace util {
  [return<int>]
  add([i32] a, [i32] b) {
    return(plus(a, b))
  }
}
[return<int>]
main() {
  return(plus(add(2i32, 3i32), min(7i32, 3i32)))
}
)";
  const std::string srcPath = writeTemp("compile_native_import_multiple.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_import_multiple_exe").string();

  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 8);
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

TEST_CASE("compiles and runs versioned include with version first") {
  const std::filesystem::path includeRoot =
      std::filesystem::temp_directory_path() / "primec_tests" / "include_root_versioned_first";
  std::filesystem::remove_all(includeRoot);
  std::filesystem::create_directories(includeRoot);
  {
    std::filesystem::create_directories((includeRoot / "1.2.0" / "lib"));
    std::ofstream oldLib(includeRoot / "1.2.0" / "lib" / "lib.prime");
    CHECK(oldLib.good());
    oldLib << "[return<int>]\nhelper(){ return(4i32) }\n";
    CHECK(oldLib.good());
  }
  {
    std::filesystem::create_directories((includeRoot / "1.2.5" / "lib"));
    std::ofstream newLib(includeRoot / "1.2.5" / "lib" / "lib.prime");
    CHECK(newLib.good());
    newLib << "[return<int>]\nhelper(){ return(9i32) }\n";
    CHECK(newLib.good());
  }

  const std::string source =
      "include<version=\"1.2\", \"/lib\">\n"
      "[return<int>]\n"
      "main(){ return(helper()) }\n";
  const std::string srcPath = writeTemp("compile_versioned_include_first.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_versioned_inc_first_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath +
                                 " --entry /main --include-path " + includeRoot.string();
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 9);
}

TEST_SUITE_END();
#endif
