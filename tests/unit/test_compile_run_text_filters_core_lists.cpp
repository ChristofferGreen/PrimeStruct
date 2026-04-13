#include "test_compile_run_text_filters_helpers.h"

TEST_SUITE_BEGIN("primestruct.compile.run.text_filters");


TEST_CASE("compiles and runs implicit i32 suffix") {
  const std::string source = R"(
[return<int>]
main() {
  return(8)
}
)";
  const std::string srcPath = writeTemp("compile_suffix.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_suffix_exe").string();
  const std::string nativePath = (testScratchPath("") / "primec_suffix_native").string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main --text-transforms=default,implicit-i32";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 8);

  const std::string runVmCmd =
      "./primec --emit=vm " + srcPath + " --entry /main --text-transforms=default,implicit-i32";
  CHECK(runCommand(runVmCmd) == 8);

  const std::string compileNativeCmd =
      "./primec --emit=native " + srcPath + " -o " + nativePath +
      " --entry /main --text-transforms=default,implicit-i32";
  CHECK(runCommand(compileNativeCmd) == 0);
  CHECK(runCommand(nativePath) == 8);
}

TEST_CASE("compiles and runs increment/decrement sugar") {
  const std::string source = R"(
[return<int>]
main() {
  [i32 mut] value{1i32}
  ++value
  value--
  return(value++)
}
)";
  const std::string srcPath = writeTemp("compile_inc_dec_sugar.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_inc_dec_sugar_exe").string();
  const std::string nativePath = (testScratchPath("") / "primec_inc_dec_sugar_native").string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main --text-transforms=default";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 2);

  const std::string runVmCmd = "./primec --emit=vm " + srcPath + " --entry /main --text-transforms=default";
  CHECK(runCommand(runVmCmd) == 2);

  const std::string compileNativeCmd =
      "./primec --emit=native " + srcPath + " -o " + nativePath + " --entry /main --text-transforms=default";
  CHECK(runCommand(compileNativeCmd) == 0);
  CHECK(runCommand(nativePath) == 2);
}

TEST_CASE("no transforms overrides text transforms") {
  const std::string source = R"(
[return<int>]
main() {
  return(11)
}
)";
  const std::string srcPath = writeTemp("compile_no_transforms_override.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_no_transforms_override_exe").string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o " + exePath +
      " --entry /main --no-transforms --text-transforms=default,implicit-i32";
  CHECK(runCommand(compileCmd) != 0);
}

TEST_CASE("no transforms accepts canonical syntax") {
  const std::string source = R"(
[return<int>]
main() {
  return(plus(1i32, 2i32))
}
)";
  const std::string srcPath = writeTemp("compile_no_transforms_canonical.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_no_transforms_canonical_exe").string();
  const std::string nativePath =
      (testScratchPath("") / "primec_no_transforms_canonical_native").string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main --no-transforms";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 3);

  const std::string runVmCmd = "./primec --emit=vm " + srcPath + " --entry /main --no-transforms";
  CHECK(runCommand(runVmCmd) == 3);

  const std::string compileNativeCmd =
      "./primec --emit=native " + srcPath + " -o " + nativePath + " --entry /main --no-transforms";
  CHECK(runCommand(compileNativeCmd) == 0);
  CHECK(runCommand(nativePath) == 3);
}

TEST_CASE("no transforms accepts brace constructors") {
  const std::string source = R"(
[return<int>]
main() {
  return(i32{ 1i32 })
}
)";
  const std::string srcPath = writeTemp("compile_no_transforms_brace_ctor.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_no_transforms_brace_ctor_exe").string();
  const std::string nativePath =
      (testScratchPath("") / "primec_no_transforms_brace_ctor_native").string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main --no-transforms";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 1);

  const std::string runVmCmd = "./primec --emit=vm " + srcPath + " --entry /main --no-transforms";
  CHECK(runCommand(runVmCmd) == 1);

  const std::string compileNativeCmd =
      "./primec --emit=native " + srcPath + " -o " + nativePath + " --entry /main --no-transforms";
  CHECK(runCommand(compileNativeCmd) == 0);
  CHECK(runCommand(nativePath) == 1);
}

TEST_CASE("no transforms rejects infix operators") {
  const std::string source = R"(
[return<int>]
main() {
  return(1i32 + 2i32)
}
)";
  const std::string srcPath = writeTemp("compile_no_transforms_infix.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_no_transforms_infix_err.txt").string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main --no-transforms 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("Parse error") != std::string::npos);
}

TEST_CASE("no transforms rejects float without suffix") {
  const std::string source = R"(
[return<int>]
main() {
  return(plus(1.0, 2.0f32))
}
)";
  const std::string srcPath = writeTemp("compile_no_transforms_float_suffix.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_no_transforms_float_suffix_err.txt").string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main --no-transforms 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("float literal requires f32/f64 suffix") != std::string::npos);
}

TEST_CASE("no transforms rejects single-letter float suffix") {
  const std::string source = R"(
[return<int>]
main() {
  return(plus(1f, 2.0f32))
}
)";
  const std::string srcPath = writeTemp("compile_no_transforms_float_f_suffix.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_no_transforms_float_f_suffix_err.txt").string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main --no-transforms 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("float literal requires f32/f64 suffix") != std::string::npos);
}

TEST_CASE("no transforms rejects increment sugar") {
  const std::string source = R"(
[return<int>]
main() {
  [i32 mut] value{1i32}
  return(value++)
}
)";
  const std::string srcPath = writeTemp("compile_no_transforms_inc_sugar.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_no_transforms_inc_sugar_err.txt").string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main --no-transforms 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("Parse error") != std::string::npos);
}

TEST_CASE("no transforms rejects if sugar") {
  const std::string source = R"(
[return<int>]
main() {
  if(true) { return(1i32) } else { return(2i32) }
}
)";
  const std::string srcPath = writeTemp("compile_no_transforms_if_sugar.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_no_transforms_if_sugar_err.txt").string();

  const std::string compileCmd =
      "./primec --emit=exe " + quoteShellArg(srcPath) + " -o /dev/null --entry /main --no-transforms 2> " +
      quoteShellArg(errPath);
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("control-flow body sugar") != std::string::npos);
}

TEST_CASE("no transforms rejects loop sugar") {
  const std::string source = R"(
[return<int>]
main() {
  loop(2i32) { }
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_no_transforms_loop_sugar.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_no_transforms_loop_sugar_err.txt").string();

  const std::string compileCmd =
      "./primec --emit=exe " + quoteShellArg(srcPath) + " -o /dev/null --entry /main --no-transforms 2> " +
      quoteShellArg(errPath);
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("control-flow body sugar") != std::string::npos);
}

TEST_CASE("no transforms rejects while sugar") {
  const std::string source = R"(
[return<int>]
main() {
  while(true) { }
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_no_transforms_while_sugar.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_no_transforms_while_sugar_err.txt").string();

  const std::string compileCmd =
      "./primec --emit=exe " + quoteShellArg(srcPath) + " -o /dev/null --entry /main --no-transforms 2> " +
      quoteShellArg(errPath);
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("control-flow body sugar") != std::string::npos);
}

TEST_CASE("no transforms rejects for sugar") {
  const std::string source = R"(
[return<int>]
main() {
  for([i32 mut] i{0i32} less_than(i 2i32) increment(i)) { }
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_no_transforms_for_sugar.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_no_transforms_for_sugar_err.txt").string();

  const std::string compileCmd =
      "./primec --emit=exe " + quoteShellArg(srcPath) + " -o /dev/null --entry /main --no-transforms 2> " +
      quoteShellArg(errPath);
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("control-flow body sugar") != std::string::npos);
}

TEST_CASE("no transforms rejects indexing sugar") {
  const std::string source = R"(
[return<int>]
main([array<i32>] values) {
  return(values[0i32])
}
)";
  const std::string srcPath = writeTemp("compile_no_transforms_index_sugar.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_no_transforms_index_sugar_err.txt").string();

  const std::string compileCmd =
      "./primec --emit=exe " + quoteShellArg(srcPath) + " -o /dev/null --entry /main --no-transforms 2> " +
      quoteShellArg(errPath);
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("indexing sugar") != std::string::npos);
}

TEST_CASE("text transforms none disables implicit utf8") {
  const std::string source = R"(
[return<int> effects(io_out)]
main() {
  print_line("no suffix")
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_text_filters_none_utf8.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_text_filters_none_utf8_err.txt").string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main --text-transforms=none 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("string literal requires utf8/ascii/raw_utf8/raw_ascii suffix") !=
        std::string::npos);
}

TEST_CASE("transform list none disables implicit utf8") {
  const std::string source = R"(
[return<int> effects(io_out)]
main() {
  print_line("no suffix")
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_transform_list_none_utf8.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_transform_list_none_utf8_err.txt").string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main --transform-list=none 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("string literal requires utf8/ascii/raw_utf8/raw_ascii suffix") !=
        std::string::npos);
}

TEST_CASE("transform list default enables implicit i32") {
  const std::string source = R"(
[return<int>]
main() {
  return(12)
}
)";
  const std::string srcPath = writeTemp("compile_transform_list_default_no_i32.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_transform_list_default_i32_exe").string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main --transform-list=default";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 12);
}

TEST_CASE("transform list none clears prior defaults") {
  const std::string source = R"(
[return<int> effects(io_out)]
main() {
  print_line("no suffix")
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_transform_list_none_clears_defaults.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_transform_list_none_clears_defaults_exe").string();
  const std::string errPath =
      (testScratchPath("") / "primec_transform_list_none_clears_defaults_err.txt").string();

  const std::string clearedCmd =
      "./primec --emit=exe " + quoteShellArg(srcPath) +
      " -o /dev/null --entry /main --transform-list=default,none 2> " + quoteShellArg(errPath);
  CHECK(runCommand(clearedCmd) == 2);
  CHECK(readFile(errPath).find("string literal requires utf8/ascii/raw_utf8/raw_ascii suffix") !=
        std::string::npos);

  const std::string restoredCmd =
      "./primec --emit=exe " + quoteShellArg(srcPath) + " -o " + quoteShellArg(exePath) +
      " --entry /main --transform-list=none,default";
  CHECK(runCommand(restoredCmd) == 0);
  CHECK(runCommand(exePath) == 0);
}

TEST_CASE("transform list semicolons split tokens") {
  const std::string source = R"(
[return<int> effects(io_out)]
main() {
  print_line("no suffix")
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_transform_list_semicolon_split.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_transform_list_semicolon_split_exe").string();
  const std::string errPath =
      (testScratchPath("") / "primec_transform_list_semicolon_split_err.txt").string();

  const std::string clearedCmd =
      "./primec --emit=exe " + quoteShellArg(srcPath) +
      " -o /dev/null --entry /main --transform-list=default\\;none 2> " + quoteShellArg(errPath);
  CHECK(runCommand(clearedCmd) == 2);
  CHECK(readFile(errPath).find("string literal requires utf8/ascii/raw_utf8/raw_ascii suffix") !=
        std::string::npos);

  const std::string restoredCmd =
      "./primec --emit=exe " + quoteShellArg(srcPath) + " -o " + quoteShellArg(exePath) +
      " --entry /main --transform-list=default\\;none\\;default";
  CHECK(runCommand(restoredCmd) == 0);
  CHECK(runCommand(exePath) == 0);
}

TEST_CASE("transform list whitespace splits tokens") {
  const std::string source = R"(
[return<int> effects(io_out)]
main() {
  print_line("no suffix")
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_transform_list_whitespace_split.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_transform_list_whitespace_split_exe").string();
  const std::string errPath =
      (testScratchPath("") / "primec_transform_list_whitespace_split_err.txt").string();

  const std::string clearedCmd =
      "./primec --emit=exe " + quoteShellArg(srcPath) +
      " -o /dev/null --entry /main --transform-list=" + quoteShellArg("default none") +
      " 2> " + quoteShellArg(errPath);
  CHECK(runCommand(clearedCmd) == 2);
  CHECK(readFile(errPath).find("string literal requires utf8/ascii/raw_utf8/raw_ascii suffix") !=
        std::string::npos);

  const std::string restoredCmd =
      "./primec --emit=exe " + quoteShellArg(srcPath) + " -o " + quoteShellArg(exePath) +
      " --entry /main --transform-list=" + quoteShellArg("default none default");
  CHECK(runCommand(restoredCmd) == 0);
  CHECK(runCommand(exePath) == 0);
}

TEST_CASE("transform list rejects unknown transform name") {
  const std::string source = R"(
[i32]
main() {
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_transform_list_unknown_name.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_transform_list_unknown_name_err.txt").string();

  const std::string compileCmd =
      "./primec --emit=exe " + quoteShellArg(srcPath) +
      " -o /dev/null --entry /main --transform-list=not_a_transform 2> " + quoteShellArg(errPath);
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("unknown transform: not_a_transform") != std::string::npos);
}

TEST_CASE("transform list none rejects infix operators") {
  const std::string source = R"(
[return<int>]
main() {
  return(1i32 + 2i32)
}
)";
  const std::string srcPath = writeTemp("compile_transform_list_none_infix.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_transform_list_none_infix_err.txt").string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main --transform-list=none 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("Parse error") != std::string::npos);
}

TEST_CASE("text transforms none rejects infix operators") {
  const std::string source = R"(
[return<int>]
main() {
  return(1i32 + 2i32)
}
)";
  const std::string srcPath = writeTemp("compile_text_filters_none_infix.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_text_filters_none_infix_err.txt").string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main --text-transforms=none 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("Parse error") != std::string::npos);
}

TEST_CASE("text transforms none still accepts canonical syntax") {
  const std::string source = R"(
[return<int>]
main() {
  return(plus(1i32, 2i32))
}
)";
  const std::string srcPath = writeTemp("compile_text_filters_none_canonical.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_text_filters_none_canonical_exe").string();
  const std::string nativePath =
      (testScratchPath("") / "primec_text_filters_none_canonical_native").string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main --text-transforms=none";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 3);

  const std::string runVmCmd = "./primec --emit=vm " + srcPath + " --entry /main --text-transforms=none";
  CHECK(runCommand(runVmCmd) == 3);

  const std::string compileNativeCmd =
      "./primec --emit=native " + srcPath + " -o " + nativePath + " --entry /main --text-transforms=none";
  CHECK(runCommand(compileNativeCmd) == 0);
  CHECK(runCommand(nativePath) == 3);
}

TEST_CASE("legacy text-filters alias forms are rejected in primec and primevm") {
  const std::string source = R"(
[return<int>]
main() {
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_text_filters_legacy_alias.prime", source);
  const std::string primecErrPath =
      (testScratchPath("") / "primec_text_filters_legacy_alias_err.txt").string();
  const std::string primevmErrPath =
      (testScratchPath("") / "primevm_text_filters_legacy_alias_err.txt").string();

  const std::string primecEqCmd =
      "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main --text-filters=none 2> " + primecErrPath;
  CHECK(runCommand(primecEqCmd) == 2);
  CHECK(readFile(primecErrPath).find("Argument error: unknown option: --text-filters=none\n") != std::string::npos);

  const std::string primevmEqCmd =
      "./primevm " + srcPath + " --entry /main --text-filters=none 2> " + primevmErrPath;
  CHECK(runCommand(primevmEqCmd) == 2);
  CHECK(readFile(primevmErrPath).find("Argument error: unknown option: --text-filters=none\n") != std::string::npos);

  const std::string primecBareCmd =
      "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main --text-filters 2> " + primecErrPath;
  CHECK(runCommand(primecBareCmd) == 2);
  CHECK(readFile(primecErrPath).find("Argument error: unknown option: --text-filters\n") != std::string::npos);

  const std::string primevmBareCmd = "./primevm " + srcPath + " --entry /main --text-filters 2> " + primevmErrPath;
  CHECK(runCommand(primevmBareCmd) == 2);
  CHECK(readFile(primevmErrPath).find("Argument error: unknown option: --text-filters\n") != std::string::npos);
}

TEST_CASE("compiles and runs implicit i32 via transform list") {
  const std::string source = R"(
[return<int>]
main() {
  return(9)
}
)";
  const std::string srcPath = writeTemp("compile_transform_list_i32.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_transform_list_i32_exe").string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main --transform-list=default,implicit-i32";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 9);
}

TEST_CASE("compiles and runs implicit i32 via transform list in primevm") {
  const std::string source = R"(
[return<int>]
main() {
  return(10)
}
)";
  const std::string srcPath = writeTemp("compile_transform_list_i32_vm.prime", source);

  const std::string runVmCmd = "./primevm " + srcPath + " --entry /main --transform-list=default,implicit-i32";
  CHECK(runCommand(runVmCmd) == 10);
}


TEST_SUITE_END();
