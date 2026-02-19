#if defined(__APPLE__) && (defined(__arm64__) || defined(__aarch64__))
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

TEST_CASE("compiles and runs native for binding condition") {
  const std::string source = R"(
[return<int>]
main() {
  [i32 mut] total{0i32}
  for([i32 mut] i{0i32} [bool] keep{less_than(i, 3i32)} assign(i, plus(i, 1i32))) {
    if(keep, then(){ assign(total, plus(total, 2i32)) }, else(){})
  }
  return(total)
}
)";
  const std::string srcPath = writeTemp("compile_native_for_condition_binding.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() / "primec_native_for_condition_binding_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 6);
}

TEST_CASE("compiles and runs native shared_scope for binding condition") {
  const std::string source = R"(
[return<int>]
main() {
  [i32 mut] total{0i32}
  [shared_scope]
  for([i32 mut] i{0i32} [bool] keep{less_than(i, 3i32)} assign(i, plus(i, 1i32))) {
    [i32 mut] acc{0i32}
    if(keep, then(){ assign(acc, plus(acc, 1i32)) }, else(){})
    assign(total, plus(total, acc))
  }
  return(total)
}
)";
  const std::string srcPath = writeTemp("compile_native_shared_scope_for_cond.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() / "primec_native_shared_scope_for_cond_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 6);
}

TEST_CASE("compiles and runs native shared_scope while loop") {
  const std::string source = R"(
[return<int>]
main() {
  [i32 mut] total{0i32}
  [i32 mut] i{0i32}
  [shared_scope]
  while(less_than(i, 3i32)) {
    [i32 mut] acc{0i32}
    assign(acc, plus(acc, 1i32))
    assign(total, plus(total, acc))
    assign(i, plus(i, 1i32))
  }
  return(total)
}
)";
  const std::string srcPath = writeTemp("compile_native_shared_scope_while.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() / "primec_native_shared_scope_while_exe").string();

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

TEST_CASE("default entry effects enable io output") {
  const std::string source = R"(
[return<int>]
main() {
  print_line("entry default effects"utf8)
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_native_entry_default_effects.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_native_entry_default_exe").string();
  const std::string outPath =
      (std::filesystem::temp_directory_path() / "primec_native_entry_default_out.txt").string();
  const std::string vmOutPath =
      (std::filesystem::temp_directory_path() / "primec_vm_entry_default_out.txt").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string runCmd = exePath + " > " + outPath;
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(outPath) == "entry default effects\n");

  const std::string runVmCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + vmOutPath;
  CHECK(runCommand(runVmCmd) == 0);
  CHECK(readFile(vmOutPath) == "entry default effects\n");
}

TEST_CASE("entry defaults apply to helpers") {
  const std::string source = R"(
[return<int>]
main() {
  log()
  return(0i32)
}

[return<void>]
log() {
  print_line("helper"utf8)
  return()
}
)";
  const std::string srcPath = writeTemp("compile_native_entry_defaults_helpers.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() / "primec_native_entry_defaults_helpers_exe").string();
  const std::string outPath =
      (std::filesystem::temp_directory_path() / "primec_native_entry_defaults_helpers_out.txt").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string runCmd = exePath + " > " + outPath;
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(outPath) == "helper\n");
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

TEST_CASE("compiles and runs native raw utf8 single-quoted strings") {
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
  CHECK(readFile(outPath) == "line\\nnext\n");
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
#endif
