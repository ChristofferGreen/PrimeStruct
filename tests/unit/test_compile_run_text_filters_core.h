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

TEST_CASE("text transforms accept whitespace separators") {
  const std::string source = R"(
[return<int>]
main() {
  return(1+2)
}
)";
  const std::string srcPath = writeTemp("compile_text_transforms_whitespace.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_text_transforms_whitespace_exe").string();

  const std::string compileCmd =
      "./primec --emit=exe " + quoteShellArg(srcPath) + " -o " + quoteShellArg(exePath) +
      " --entry /main --text-transforms=" + quoteShellArg("operators implicit-i32");
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 3);
}

TEST_CASE("transform list enables single_type_to_return") {
  const std::string source = R"(
[i32]
main() {
}
)";
  const std::string srcPath = writeTemp("compile_single_type_to_return_missing_return.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_single_type_to_return_err.txt").string();

  const std::string compileCmd = "./primec --emit=exe " + quoteShellArg(srcPath) +
                                 " -o /dev/null --entry /main --transform-list=default,single_type_to_return 2> " +
                                 quoteShellArg(errPath);
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("missing return statement") != std::string::npos);
}

TEST_CASE("per-definition single_type_to_return marker") {
  const std::string source = R"(
[single_type_to_return i32]
main() {
}
)";
  const std::string srcPath = writeTemp("compile_single_type_to_return_marker.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_single_type_to_return_marker_err.txt").string();

  const std::string compileCmd =
      "./primec --emit=exe " + quoteShellArg(srcPath) + " -o /dev/null --entry /main 2> " + quoteShellArg(errPath);
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("missing return statement") != std::string::npos);
}

TEST_CASE("semantic transforms flag enables single_type_to_return") {
  const std::string source = R"(
[i32]
main() {
}
)";
  const std::string srcPath = writeTemp("compile_semantic_single_type_to_return.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_semantic_single_type_to_return_err.txt").string();

  const std::string compileCmd = "./primec --emit=exe " + quoteShellArg(srcPath) +
                                 " -o /dev/null --entry /main --semantic-transforms=single_type_to_return 2> " +
                                 quoteShellArg(errPath);
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("missing return statement") != std::string::npos);
}

TEST_CASE("semantic transform rules apply per path") {
  const std::string source = R"(
[i32]
main() {
  return(1u64)
}
)";
  const std::string srcPath = writeTemp("compile_semantic_transform_rules.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_semantic_transform_rules_exe").string();
  const std::string errPath =
      (testScratchPath("") / "primec_semantic_transform_rules_err.txt").string();

  const std::string okCmd =
      "./primec --emit=exe " + quoteShellArg(srcPath) + " -o " + quoteShellArg(exePath) +
      " --entry /main --semantic-transforms=none";
  CHECK(runCommand(okCmd) == 0);

  const std::string ruleCmd =
      "./primec --emit=exe " + quoteShellArg(srcPath) +
      " -o /dev/null --entry /main --semantic-transforms=none --semantic-transform-rules=/main=single_type_to_return 2> " +
      quoteShellArg(errPath);
  CHECK(runCommand(ruleCmd) == 2);
  CHECK(readFile(errPath).find("return type mismatch") != std::string::npos);
}

TEST_CASE("semantic transform rules prefer later matching entry") {
  const std::string source = R"(
[i32]
main() {
  return(1u64)
}
)";
  const std::string srcPath = writeTemp("compile_semantic_transform_rule_order.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_semantic_transform_rule_order_exe").string();
  const std::string errPath =
      (testScratchPath("") / "primec_semantic_transform_rule_order_err.txt").string();

  const std::string disableLastCmd =
      "./primec --emit=exe " + quoteShellArg(srcPath) + " -o " + quoteShellArg(exePath) +
      " --entry /main --semantic-transforms=none "
      "--semantic-transform-rules=/main=single_type_to_return\\;/main=none";
  CHECK(runCommand(disableLastCmd) == 0);

  const std::string enableLastCmd =
      "./primec --emit=exe " + quoteShellArg(srcPath) +
      " -o /dev/null --entry /main --semantic-transforms=none "
      "--semantic-transform-rules=/main=none\\;/main=single_type_to_return 2> " +
      quoteShellArg(errPath);
  CHECK(runCommand(enableLastCmd) == 2);
  CHECK(readFile(errPath).find("return type mismatch") != std::string::npos);
}

TEST_CASE("semantic transform rules clear on none token") {
  const std::string source = R"(
[i32]
main() {
  return(1u64)
}
)";
  const std::string srcPath = writeTemp("compile_semantic_transform_rules_none_token.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_semantic_transform_rules_none_token_exe").string();
  const std::string errPath =
      (testScratchPath("") / "primec_semantic_transform_rules_none_token_err.txt").string();

  const std::string clearedCmd =
      "./primec --emit=exe " + quoteShellArg(srcPath) + " -o " + quoteShellArg(exePath) +
      " --entry /main --semantic-transforms=none "
      "--semantic-transform-rules=/main=single_type_to_return\\;none";
  CHECK(runCommand(clearedCmd) == 0);
  CHECK(runCommand(exePath) == 1);

  const std::string reappliedCmd =
      "./primec --emit=exe " + quoteShellArg(srcPath) +
      " -o /dev/null --entry /main --semantic-transforms=none "
      "--semantic-transform-rules=none\\;/main=single_type_to_return 2> " +
      quoteShellArg(errPath);
  CHECK(runCommand(reappliedCmd) == 2);
  CHECK(readFile(errPath).find("return type mismatch") != std::string::npos);
}

TEST_CASE("semantic transform rules ignore empty rule tokens") {
  const std::string source = R"(
[i32]
main() {
  return(1u64)
}
)";
  const std::string srcPath = writeTemp("compile_semantic_transform_empty_tokens.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_semantic_transform_empty_tokens_err.txt").string();

  const std::string ruleCmd =
      "./primec --emit=exe " + quoteShellArg(srcPath) +
      " -o /dev/null --entry /main --semantic-transforms=none --semantic-transform-rules " +
      quoteShellArg(";;/main=single_type_to_return;;") + " 2> " + quoteShellArg(errPath);
  CHECK(runCommand(ruleCmd) == 2);
  CHECK(readFile(errPath).find("return type mismatch") != std::string::npos);
}

TEST_CASE("semantic transform rules prefer later wildcard match") {
  const std::string source = R"(
[i32]
main() {
  return(1u64)
}
)";
  const std::string srcPath = writeTemp("compile_semantic_transform_rule_wildcard_order.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_semantic_transform_rule_wildcard_order_exe").string();
  const std::string errPath =
      (testScratchPath("") / "primec_semantic_transform_rule_wildcard_order_err.txt").string();

  const std::string wildcardLastCmd =
      "./primec --emit=exe " + quoteShellArg(srcPath) + " -o " + quoteShellArg(exePath) +
      " --entry /main --semantic-transforms=none "
      "--semantic-transform-rules=/main=single_type_to_return\\;/*=none";
  CHECK(runCommand(wildcardLastCmd) == 0);
  CHECK(runCommand(exePath) == 1);

  const std::string exactLastCmd =
      "./primec --emit=exe " + quoteShellArg(srcPath) +
      " -o /dev/null --entry /main --semantic-transforms=none "
      "--semantic-transform-rules=/*=none\\;/main=single_type_to_return 2> " +
      quoteShellArg(errPath);
  CHECK(runCommand(exactLastCmd) == 2);
  CHECK(readFile(errPath).find("return type mismatch") != std::string::npos);
}

TEST_CASE("semantic transform root wildcard applies to top-level definitions") {
  const std::string source = R"(
main() {
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_semantic_transform_root_wildcard_top_level.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_semantic_transform_root_wildcard_top_level_err.txt")
          .string();

  const std::string ruleCmd =
      "./primec --emit=exe " + quoteShellArg(srcPath) +
      " -o /dev/null --entry /main --semantic-transforms=none "
      "--semantic-transform-rules=/*=single_type_to_return 2> " +
      quoteShellArg(errPath);
  CHECK(runCommand(ruleCmd) == 2);
  CHECK(readFile(errPath).find("single_type_to_return requires a type transform on /main") !=
        std::string::npos);
}

TEST_CASE("semantic transform rules reject recurse without wildcard") {
  const std::string source = R"(
[i32]
main() {
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_semantic_transform_bad_recurse_rule.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_semantic_transform_bad_recurse_rule_err.txt").string();

  const std::string badRuleCmd =
      "./primec --emit=exe " + quoteShellArg(srcPath) +
      " -o /dev/null --entry /main --semantic-transforms=none "
      "--semantic-transform-rules=/main:recurse=single_type_to_return 2> " +
      quoteShellArg(errPath);
  CHECK(runCommand(badRuleCmd) == 2);
  CHECK(readFile(errPath).find("transform rule recursion requires a '/*' suffix") != std::string::npos);
}

TEST_CASE("semantic transform rules reject empty transform list") {
  const std::string source = R"(
[i32]
main() {
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_semantic_transform_empty_list_rule.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_semantic_transform_empty_list_rule_err.txt").string();

  const std::string badRuleCmd =
      "./primec --emit=exe " + quoteShellArg(srcPath) +
      " -o /dev/null --entry /main --semantic-transforms=none "
      "--semantic-transform-rules=/main= 2> " +
      quoteShellArg(errPath);
  CHECK(runCommand(badRuleCmd) == 2);
  CHECK(readFile(errPath).find("transform rule list cannot be empty") != std::string::npos);
}

TEST_CASE("semantic transform rules reject non-trailing wildcard") {
  const std::string source = R"(
[i32]
main() {
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_semantic_transform_bad_wildcard_rule.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_semantic_transform_bad_wildcard_rule_err.txt").string();

  const std::string badRuleCmd =
      "./primec --emit=exe " + quoteShellArg(srcPath) +
      " -o /dev/null --entry /main --semantic-transforms=none "
      "--semantic-transform-rules=/ma*in=single_type_to_return 2> " +
      quoteShellArg(errPath);
  CHECK(runCommand(badRuleCmd) == 2);
  CHECK(readFile(errPath).find("transform rule '*' is only allowed as trailing '/*'") !=
        std::string::npos);
}

TEST_CASE("semantic transform rules reject path without slash prefix") {
  const std::string source = R"(
[i32]
main() {
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_semantic_transform_missing_slash_rule.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_semantic_transform_missing_slash_rule_err.txt").string();

  const std::string badRuleCmd =
      "./primec --emit=exe " + quoteShellArg(srcPath) +
      " -o /dev/null --entry /main --semantic-transforms=none "
      "--semantic-transform-rules=main=single_type_to_return 2> " +
      quoteShellArg(errPath);
  CHECK(runCommand(badRuleCmd) == 2);
  CHECK(readFile(errPath).find("transform rule path must start with '/': main") != std::string::npos);
}

TEST_CASE("semantic transform rules reject missing equals") {
  const std::string source = R"(
[i32]
main() {
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_semantic_transform_missing_equals_rule.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_semantic_transform_missing_equals_rule_err.txt").string();

  const std::string badRuleCmd =
      "./primec --emit=exe " + quoteShellArg(srcPath) +
      " -o /dev/null --entry /main --semantic-transforms=none "
      "--semantic-transform-rules=/main 2> " +
      quoteShellArg(errPath);
  CHECK(runCommand(badRuleCmd) == 2);
  CHECK(readFile(errPath).find("transform rule must include '=': /main") != std::string::npos);
}

TEST_CASE("semantic transform rules reject empty path") {
  const std::string source = R"(
[i32]
main() {
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_semantic_transform_empty_path_rule.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_semantic_transform_empty_path_rule_err.txt").string();

  const std::string badRuleCmd =
      "./primec --emit=exe " + quoteShellArg(srcPath) +
      " -o /dev/null --entry /main --semantic-transforms=none "
      "--semantic-transform-rules==single_type_to_return 2> " +
      quoteShellArg(errPath);
  CHECK(runCommand(badRuleCmd) == 2);
  CHECK(readFile(errPath).find("transform rule path cannot be empty") != std::string::npos);
}

TEST_CASE("semantic transform rules reject text-only transform name") {
  const std::string source = R"(
[i32]
main() {
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_semantic_transform_text_only_name.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_semantic_transform_text_only_name_err.txt").string();

  const std::string badRuleCmd =
      "./primec --emit=exe " + quoteShellArg(srcPath) +
      " -o /dev/null --entry /main --semantic-transforms=none "
      "--semantic-transform-rules=/main=operators 2> " +
      quoteShellArg(errPath);
  CHECK(runCommand(badRuleCmd) == 2);
  CHECK(readFile(errPath).find("unsupported semantic transform: operators") != std::string::npos);
}

TEST_CASE("semantic transform rules reject unknown transform name") {
  const std::string source = R"(
[i32]
main() {
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_semantic_transform_unknown_name.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_semantic_transform_unknown_name_err.txt").string();

  const std::string badRuleCmd =
      "./primec --emit=exe " + quoteShellArg(srcPath) +
      " -o /dev/null --entry /main --semantic-transforms=none "
      "--semantic-transform-rules=/main=not_a_transform 2> " +
      quoteShellArg(errPath);
  CHECK(runCommand(badRuleCmd) == 2);
  CHECK(readFile(errPath).find("unknown transform: not_a_transform") != std::string::npos);
}

TEST_CASE("semantic transform rules accept recursive suffix alias") {
  const std::string source = R"(
[i32]
main() {
  [i32]
  helper() {
    inner() {
      return(1u64)
    }
    return(0i32)
  }
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_semantic_transform_recursive_alias.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_semantic_transform_recursive_alias_exe").string();
  const std::string errPath =
      (testScratchPath("") / "primec_semantic_transform_recursive_alias_err.txt").string();

  const std::string nonRecursiveCmd =
      "./primec --emit=exe " + quoteShellArg(srcPath) + " -o " + quoteShellArg(exePath) +
      " --entry /main --semantic-transforms=none --semantic-transform-rules=/main/*=single_type_to_return";
  CHECK(runCommand(nonRecursiveCmd) == 0);

  const std::string recursiveAliasCmd =
      "./primec --emit=exe " + quoteShellArg(srcPath) +
      " -o /dev/null --entry /main --semantic-transforms=none "
      "--semantic-transform-rules=/main/*:recursive=single_type_to_return 2> " +
      quoteShellArg(errPath);
  CHECK(runCommand(recursiveAliasCmd) == 2);
  CHECK(readFile(errPath).find("single_type_to_return requires a type transform on /main/helper/inner") !=
        std::string::npos);
}

TEST_CASE("semantic transform wildcard does not match sibling prefix") {
  const std::string source = R"(
[i32]
mainly() {
  return(1u64)
}
)";
  const std::string srcPath = writeTemp("compile_semantic_transform_wildcard_prefix.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_semantic_transform_wildcard_prefix_exe").string();

  const std::string ruleCmd =
      "./primec --emit=exe " + quoteShellArg(srcPath) + " -o " + quoteShellArg(exePath) +
      " --entry /mainly --semantic-transforms=none "
      "--semantic-transform-rules=/main/*=single_type_to_return";
  CHECK(runCommand(ruleCmd) == 0);
  CHECK(runCommand(exePath) == 1);
}

TEST_CASE("semantic transform wildcard does not match base path") {
  const std::string source = R"(
[i32]
main() {
  return(1u64)
}
)";
  const std::string srcPath = writeTemp("compile_semantic_transform_wildcard_base.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_semantic_transform_wildcard_base_exe").string();

  const std::string ruleCmd =
      "./primec --emit=exe " + quoteShellArg(srcPath) + " -o " + quoteShellArg(exePath) +
      " --entry /main --semantic-transforms=none "
      "--semantic-transform-rules=/main/*=single_type_to_return";
  CHECK(runCommand(ruleCmd) == 0);
  CHECK(runCommand(exePath) == 1);
}

TEST_CASE("semantic transform rules ignore unrelated exact path") {
  const std::string source = R"(
[i32]
main() {
  return(1u64)
}
)";
  const std::string srcPath = writeTemp("compile_semantic_transform_unrelated_exact_path.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_semantic_transform_unrelated_exact_path_exe").string();

  const std::string ruleCmd =
      "./primec --emit=exe " + quoteShellArg(srcPath) + " -o " + quoteShellArg(exePath) +
      " --entry /main --semantic-transforms=none "
      "--semantic-transform-rules=/other=single_type_to_return";
  CHECK(runCommand(ruleCmd) == 0);
  CHECK(runCommand(exePath) == 1);
}

TEST_CASE("semantic transform rules ignore unrelated wildcard path") {
  const std::string source = R"(
[i32]
mainly() {
  return(1u64)
}
)";
  const std::string srcPath = writeTemp("compile_semantic_transform_unrelated_wildcard_path.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_semantic_transform_unrelated_wildcard_path_exe").string();

  const std::string ruleCmd =
      "./primec --emit=exe " + quoteShellArg(srcPath) + " -o " + quoteShellArg(exePath) +
      " --entry /mainly --semantic-transforms=none "
      "--semantic-transform-rules=/other/*=single_type_to_return";
  CHECK(runCommand(ruleCmd) == 0);
  CHECK(runCommand(exePath) == 1);
}

TEST_CASE("semantic root wildcard only recurses when requested") {
  const std::string source = R"(
[i32]
main() {
  [i32]
  helper() {
    inner() {
      return(1u64)
    }
    return(0i32)
  }
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_semantic_root_wildcard_recurse.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_semantic_root_wildcard_recurse_exe").string();
  const std::string errPath =
      (testScratchPath("") / "primec_semantic_root_wildcard_recurse_err.txt").string();

  const std::string noRecurseCmd =
      "./primec --emit=exe " + quoteShellArg(srcPath) + " -o " + quoteShellArg(exePath) +
      " --entry /main --semantic-transforms=none --semantic-transform-rules=/*=single_type_to_return";
  CHECK(runCommand(noRecurseCmd) == 0);
  CHECK(runCommand(exePath) == 0);

  const std::string recurseCmd =
      "./primec --emit=exe " + quoteShellArg(srcPath) +
      " -o /dev/null --entry /main --semantic-transforms=none "
      "--semantic-transform-rules=/*:recurse=single_type_to_return 2> " +
      quoteShellArg(errPath);
  CHECK(runCommand(recurseCmd) == 2);
  CHECK(readFile(errPath).find("single_type_to_return requires a type transform on /main/helper/inner") !=
        std::string::npos);
}

TEST_CASE("semantic transforms ignore text transforms") {
  const std::string source = R"(
[operators i32]
main() {
}
)";
  const std::string srcPath = writeTemp("compile_semantic_single_type_to_return_text.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_semantic_single_type_to_return_text_err.txt").string();

  const std::string compileCmd = "./primec --emit=exe " + quoteShellArg(srcPath) +
                                 " -o /dev/null --entry /main --semantic-transforms=single_type_to_return 2> " +
                                 quoteShellArg(errPath);
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("missing return statement") != std::string::npos);
}

TEST_CASE("no transforms disables single_type_to_return") {
  const std::string source = R"(
[i32]
main() {
}
)";
  const std::string srcPath = writeTemp("compile_single_type_to_return_disabled.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_single_type_to_return_disabled_err.txt").string();

  const std::string compileCmd = "./primec --emit=exe " + quoteShellArg(srcPath) + " -o /dev/null" +
                                 " --entry /main --no-transforms --transform-list=default,single_type_to_return 2> " +
                                 quoteShellArg(errPath);
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("explicit return transform") != std::string::npos);
}

TEST_CASE("per-envelope text transforms enable collections") {
  const std::string source = R"(
[collections return<int>]
main() {
  [array<i32>] values{array<i32>{1i32, 2i32}}
  return(1i32)
}
)";
  const std::string srcPath = writeTemp("compile_per_env_collections.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_per_env_collections_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + quoteShellArg(srcPath) + " -o " +
                                 quoteShellArg(exePath) + " --entry /main --text-transforms=operators";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 1);
}

TEST_CASE("per-envelope text transforms override operators") {
  const std::string source = R"(
[collections return<int>]
main() {
  return(1i32 + 2i32)
}
)";
  const std::string srcPath = writeTemp("compile_per_env_override.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_per_env_override_err.txt").string();

  const std::string compileCmd = "./primec --emit=exe " + quoteShellArg(srcPath) +
                                 " -o /dev/null --entry /main 2> " + quoteShellArg(errPath);
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("Parse error") != std::string::npos);
}

TEST_CASE("per-envelope text transforms apply to bindings") {
  const std::string source = R"(
[return<int>]
main() {
  [operators i32] value{1i32+2i32}
  return(value)
}
)";
  const std::string srcPath = writeTemp("compile_per_env_binding_ops.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_per_env_binding_ops_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + quoteShellArg(srcPath) + " -o " +
                                 quoteShellArg(exePath) + " --entry /main --text-transforms=none";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 3);
}

TEST_CASE("per-envelope text transforms apply to executions in arguments") {
  const std::string source = R"(
[return<int>]
add([i32] left, [i32] right) {
  return(plus(left, right))
}

[return<int>]
main() {
  return([operators] add(1i32+2i32, 3i32))
}
)";
  const std::string srcPath = writeTemp("compile_per_env_exec_args.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_per_env_exec_args_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + quoteShellArg(srcPath) + " -o " +
                                 quoteShellArg(exePath) + " --entry /main --text-transforms=none";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 6);
}

TEST_CASE("text transforms can append additional transforms") {
  const std::string source = R"(
[append_operators return<int>]
main() {
  return(1i32+2i32)
}
)";
  const std::string srcPath = writeTemp("compile_append_operators.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_append_operators_exe").string();

  const std::string compileCmd =
      "./primec --emit=exe " + quoteShellArg(srcPath) + " -o " + quoteShellArg(exePath) +
      " --entry /main --text-transforms=append_operators";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 3);
}

TEST_CASE("transform list auto-deduces append_operators") {
  const std::string source = R"(
[append_operators return<int>]
main() {
  return(1i32+2i32)
}
)";
  const std::string srcPath = writeTemp("compile_append_operators_auto.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_append_operators_auto_exe").string();

  const std::string compileCmd =
      "./primec --emit=exe " + quoteShellArg(srcPath) + " -o " + quoteShellArg(exePath) +
      " --entry /main --transform-list=append_operators";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 3);
}

TEST_CASE("text transform rules apply to namespace paths") {
  const std::string source = R"(
namespace std {
  namespace math {
    [return<int>]
    add() {
      return(1i32+2i32)
    }
  }
}

[return<int>]
main() {
  return(/std/math/add())
}
)";
  const std::string srcPath = writeTemp("compile_text_rule_namespace.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_text_rule_namespace_exe").string();

  const std::string compileCmd =
      "./primec --emit=exe " + quoteShellArg(srcPath) + " -o " + quoteShellArg(exePath) +
      " --entry /main --text-transforms=none --text-transform-rules=/std/math/*=operators";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 3);
}

TEST_CASE("text transform rules ignore unrelated wildcard path") {
  const std::string source = R"(
[return<int>]
mainly() {
  return(1i32+2i32)
}
)";
  const std::string srcPath = writeTemp("compile_text_rule_unrelated_wildcard_path.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_text_rule_unrelated_wildcard_path_err.txt").string();

  const std::string compileCmd =
      "./primec --emit=exe " + quoteShellArg(srcPath) +
      " -o /dev/null --entry /mainly --text-transforms=none --text-transform-rules=/other/*=operators 2> " +
      quoteShellArg(errPath);
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("Parse error") != std::string::npos);
}

TEST_CASE("text transform rules ignore unrelated exact path") {
  const std::string source = R"(
[return<int>]
main() {
  return(1i32+2i32)
}
)";
  const std::string srcPath = writeTemp("compile_text_rule_unrelated_exact_path.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_text_rule_unrelated_exact_path_err.txt").string();

  const std::string compileCmd =
      "./primec --emit=exe " + quoteShellArg(srcPath) +
      " -o /dev/null --entry /main --text-transforms=none --text-transform-rules=/other=operators 2> " +
      quoteShellArg(errPath);
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("Parse error") != std::string::npos);
}

TEST_CASE("text transform wildcard does not match base path") {
  const std::string source = R"(
[return<int>]
main() {
  return(1i32+2i32)
}
)";
  const std::string srcPath = writeTemp("compile_text_rule_wildcard_base_path.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_text_rule_wildcard_base_path_err.txt").string();

  const std::string compileCmd =
      "./primec --emit=exe " + quoteShellArg(srcPath) +
      " -o /dev/null --entry /main --text-transforms=none --text-transform-rules=/main/*=operators 2> " +
      quoteShellArg(errPath);
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("Parse error") != std::string::npos);
}

TEST_CASE("text transform wildcard does not match sibling prefix") {
  const std::string source = R"(
[return<int>]
mainly() {
  return(1i32+2i32)
}
)";
  const std::string srcPath = writeTemp("compile_text_rule_wildcard_sibling_prefix.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_text_rule_wildcard_sibling_prefix_err.txt").string();

  const std::string compileCmd =
      "./primec --emit=exe " + quoteShellArg(srcPath) +
      " -o /dev/null --entry /mainly --text-transforms=none --text-transform-rules=/main/*=operators 2> " +
      quoteShellArg(errPath);
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("Parse error") != std::string::npos);
}

TEST_CASE("text transform rules apply without transform lists") {
  const std::string source = R"(
main() {
  return(1i32+2i32)
}
)";
  const std::string srcPath = writeTemp("compile_text_rule_no_list.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_text_rule_no_list_exe").string();

  const std::string compileCmd =
      "./primec --emit=exe " + quoteShellArg(srcPath) + " -o " + quoteShellArg(exePath) +
      " --entry /main --text-transforms=none --text-transform-rules=/main=operators";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 3);
}

TEST_CASE("text transform rules ignore empty rule tokens") {
  const std::string source = R"(
[return<int>]
main() {
  return(1i32+2i32)
}
)";
  const std::string srcPath = writeTemp("compile_text_rule_empty_tokens.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_text_rule_empty_tokens_exe").string();

  const std::string compileCmd =
      "./primec --emit=exe " + quoteShellArg(srcPath) + " -o " + quoteShellArg(exePath) +
      " --entry /main --text-transforms=none --text-transform-rules " +
      quoteShellArg(";;/main=operators;;");
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 3);
}

TEST_CASE("text transform rules none token without follow-up keeps rules empty") {
  const std::string source = R"(
[return<int>]
main() {
  return(1i32+2i32)
}
)";
  const std::string srcPath = writeTemp("compile_text_rule_none_only.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_text_rule_none_only_err.txt").string();

  const std::string compileCmd =
      "./primec --emit=exe " + quoteShellArg(srcPath) +
      " -o /dev/null --entry /main --text-transforms=none --text-transform-rules=none 2> " +
      quoteShellArg(errPath);
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("Parse error") != std::string::npos);
}

TEST_CASE("text transform rules prefer later matching entry") {
  const std::string source = R"(
[return<int>]
main() {
  return(1i32+2i32)
}
)";
  const std::string srcPath = writeTemp("compile_text_rule_order.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_text_rule_order_exe").string();
  const std::string errPath = (testScratchPath("") / "primec_text_rule_order_err.txt").string();

  const std::string disableLastCmd =
      "./primec --emit=exe " + quoteShellArg(srcPath) +
      " -o /dev/null --entry /main --text-transforms=none "
      "--text-transform-rules=/main=operators\\;/main=none 2> " +
      quoteShellArg(errPath);
  CHECK(runCommand(disableLastCmd) == 2);
  CHECK(readFile(errPath).find("Parse error") != std::string::npos);

  const std::string enableLastCmd =
      "./primec --emit=exe " + quoteShellArg(srcPath) + " -o " + quoteShellArg(exePath) +
      " --entry /main --text-transforms=none "
      "--text-transform-rules=/main=none\\;/main=operators";
  CHECK(runCommand(enableLastCmd) == 0);
  CHECK(runCommand(exePath) == 3);
}

TEST_CASE("text transform rules prefer later wildcard match") {
  const std::string source = R"(
[return<int>]
main() {
  return(1i32+2i32)
}
)";
  const std::string srcPath = writeTemp("compile_text_rule_wildcard_order.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_text_rule_wildcard_order_exe").string();
  const std::string errPath =
      (testScratchPath("") / "primec_text_rule_wildcard_order_err.txt").string();

  const std::string wildcardLastCmd =
      "./primec --emit=exe " + quoteShellArg(srcPath) +
      " -o /dev/null --entry /main --text-transforms=none "
      "--text-transform-rules=/main=operators\\;/*=none 2> " +
      quoteShellArg(errPath);
  CHECK(runCommand(wildcardLastCmd) == 2);
  CHECK(readFile(errPath).find("Parse error") != std::string::npos);

  const std::string exactLastCmd =
      "./primec --emit=exe " + quoteShellArg(srcPath) + " -o " + quoteShellArg(exePath) +
      " --entry /main --text-transforms=none "
      "--text-transform-rules=/*=none\\;/main=operators";
  CHECK(runCommand(exactLastCmd) == 0);
  CHECK(runCommand(exePath) == 3);
}

TEST_CASE("text transform root wildcard applies to top-level definitions") {
  const std::string source = R"(
[return<int>]
main() {
  return(1i32+2i32)
}
)";
  const std::string srcPath = writeTemp("compile_text_rule_root_wildcard_top_level.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_text_rule_root_wildcard_top_level_exe").string();

  const std::string compileCmd =
      "./primec --emit=exe " + quoteShellArg(srcPath) + " -o " + quoteShellArg(exePath) +
      " --entry /main --text-transforms=none --text-transform-rules=/*=operators";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 3);
}

TEST_CASE("text transform rules clear on none token") {
  const std::string source = R"(
[return<int>]
main() {
  return(1i32+2i32)
}
)";
  const std::string srcPath = writeTemp("compile_text_rule_none_token.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_text_rule_none_token_exe").string();
  const std::string errPath =
      (testScratchPath("") / "primec_text_rule_none_token_err.txt").string();

  const std::string clearedCmd =
      "./primec --emit=exe " + quoteShellArg(srcPath) +
      " -o /dev/null --entry /main --text-transforms=none "
      "--text-transform-rules=/main=operators\\;none 2> " +
      quoteShellArg(errPath);
  CHECK(runCommand(clearedCmd) == 2);
  CHECK(readFile(errPath).find("Parse error") != std::string::npos);

  const std::string reappliedCmd =
      "./primec --emit=exe " + quoteShellArg(srcPath) + " -o " + quoteShellArg(exePath) +
      " --entry /main --text-transforms=none --text-transform-rules=none\\;/main=operators";
  CHECK(runCommand(reappliedCmd) == 0);
  CHECK(runCommand(exePath) == 3);
}

TEST_CASE("text transform rules reject missing equals") {
  const std::string source = R"(
[i32]
main() {
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_text_rule_missing_equals.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_text_rule_missing_equals_err.txt").string();

  const std::string badRuleCmd =
      "./primec --emit=exe " + quoteShellArg(srcPath) +
      " -o /dev/null --entry /main --text-transforms=none "
      "--text-transform-rules=/main 2> " +
      quoteShellArg(errPath);
  CHECK(runCommand(badRuleCmd) == 2);
  CHECK(readFile(errPath).find("transform rule must include '=': /main") != std::string::npos);
}

TEST_CASE("text transform rules reject empty transform list") {
  const std::string source = R"(
[i32]
main() {
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_text_rule_empty_list_rule.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_text_rule_empty_list_rule_err.txt").string();

  const std::string badRuleCmd =
      "./primec --emit=exe " + quoteShellArg(srcPath) +
      " -o /dev/null --entry /main --text-transforms=none "
      "--text-transform-rules=/main= 2> " +
      quoteShellArg(errPath);
  CHECK(runCommand(badRuleCmd) == 2);
  CHECK(readFile(errPath).find("transform rule list cannot be empty") != std::string::npos);
}

TEST_CASE("text transform rules reject empty path") {
  const std::string source = R"(
[i32]
main() {
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_text_rule_empty_path_rule.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_text_rule_empty_path_rule_err.txt").string();

  const std::string badRuleCmd =
      "./primec --emit=exe " + quoteShellArg(srcPath) +
      " -o /dev/null --entry /main --text-transforms=none "
      "--text-transform-rules==operators 2> " +
      quoteShellArg(errPath);
  CHECK(runCommand(badRuleCmd) == 2);
  CHECK(readFile(errPath).find("transform rule path cannot be empty") != std::string::npos);
}

TEST_CASE("text transform rules reject path without slash prefix") {
  const std::string source = R"(
[i32]
main() {
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_text_rule_missing_slash_rule.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_text_rule_missing_slash_rule_err.txt").string();

  const std::string badRuleCmd =
      "./primec --emit=exe " + quoteShellArg(srcPath) +
      " -o /dev/null --entry /main --text-transforms=none "
      "--text-transform-rules=main=operators 2> " +
      quoteShellArg(errPath);
  CHECK(runCommand(badRuleCmd) == 2);
  CHECK(readFile(errPath).find("transform rule path must start with '/': main") != std::string::npos);
}

TEST_CASE("text transform rules reject non-trailing wildcard") {
  const std::string source = R"(
[i32]
main() {
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_text_rule_bad_wildcard_rule.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_text_rule_bad_wildcard_rule_err.txt").string();

  const std::string badRuleCmd =
      "./primec --emit=exe " + quoteShellArg(srcPath) +
      " -o /dev/null --entry /main --text-transforms=none "
      "--text-transform-rules=/ma*in=operators 2> " +
      quoteShellArg(errPath);
  CHECK(runCommand(badRuleCmd) == 2);
  CHECK(readFile(errPath).find("transform rule '*' is only allowed as trailing '/*'") !=
        std::string::npos);
}

TEST_CASE("text transform rules reject recurse without wildcard") {
  const std::string source = R"(
[i32]
main() {
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_text_rule_bad_recurse_rule.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_text_rule_bad_recurse_rule_err.txt").string();

  const std::string badRuleCmd =
      "./primec --emit=exe " + quoteShellArg(srcPath) +
      " -o /dev/null --entry /main --text-transforms=none "
      "--text-transform-rules=/main:recurse=operators 2> " +
      quoteShellArg(errPath);
  CHECK(runCommand(badRuleCmd) == 2);
  CHECK(readFile(errPath).find("transform rule recursion requires a '/*' suffix") != std::string::npos);
}

TEST_CASE("text transform rules reject semantic-only transform name") {
  const std::string source = R"(
[i32]
main() {
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_text_rule_semantic_only_name.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_text_rule_semantic_only_name_err.txt").string();

  const std::string badRuleCmd =
      "./primec --emit=exe " + quoteShellArg(srcPath) +
      " -o /dev/null --entry /main --text-transforms=none "
      "--text-transform-rules=/main=single_type_to_return 2> " +
      quoteShellArg(errPath);
  CHECK(runCommand(badRuleCmd) == 2);
  CHECK(readFile(errPath).find("unsupported text transform: single_type_to_return") != std::string::npos);
}

TEST_CASE("text transform rules reject unknown transform name") {
  const std::string source = R"(
[i32]
main() {
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_text_rule_unknown_name.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_text_rule_unknown_name_err.txt").string();

  const std::string badRuleCmd =
      "./primec --emit=exe " + quoteShellArg(srcPath) +
      " -o /dev/null --entry /main --text-transforms=none "
      "--text-transform-rules=/main=not_a_transform 2> " +
      quoteShellArg(errPath);
  CHECK(runCommand(badRuleCmd) == 2);
  CHECK(readFile(errPath).find("unknown transform: not_a_transform") != std::string::npos);
}

TEST_CASE("text transform rules apply to nested definitions") {
  const std::string source = R"(
[return<int>]
main() {
  helper() {
    return(1i32+2i32)
  }
  return(/main/helper())
}
)";
  const std::string srcPath = writeTemp("compile_text_rule_nested_def.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_text_rule_nested_def_exe").string();

  const std::string compileCmd =
      "./primec --emit=exe " + quoteShellArg(srcPath) + " -o " + quoteShellArg(exePath) +
      " --entry /main --text-transforms=none --text-transform-rules=/main/*=operators";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 3);
}

TEST_CASE("text transform rules ignore nested transform lists") {
  const std::string source = R"(
main() {
  [implicit-utf8 string] name{"ok"}
  print_line("nope")
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_text_rule_nested_list.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_text_rule_nested_list_err.txt").string();

  const std::string compileCmd =
      "./primec --emit=exe " + quoteShellArg(srcPath) + " -o /dev/null --entry /main --text-transforms=none " +
      "--text-transform-rules=/main=operators 2> " + quoteShellArg(errPath);
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("Parse error") != std::string::npos);
}

TEST_CASE("text transform rules recurse when requested") {
  const std::string source = R"(
namespace std {
  namespace math {
    namespace ops {
      [return<int>]
      add() {
        return(1i32+2i32)
      }
    }
  }
}

[return<int>]
main() {
  return(/std/math/ops/add())
}
)";
  const std::string srcPath = writeTemp("compile_text_rule_recurse.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_text_rule_recurse_exe").string();
  const std::string errPath =
      (testScratchPath("") / "primec_text_rule_recurse_err.txt").string();

  const std::string noRecurseCmd =
      "./primec --emit=exe " + quoteShellArg(srcPath) + " -o /dev/null --entry /main --text-transforms=none "
      "--text-transform-rules=/std/math/*=operators 2> " +
      quoteShellArg(errPath);
  CHECK(runCommand(noRecurseCmd) == 2);
  CHECK(readFile(errPath).find("Parse error") != std::string::npos);

  const std::string recurseCmd =
      "./primec --emit=exe " + quoteShellArg(srcPath) + " -o " + quoteShellArg(exePath) +
      " --entry /main --text-transforms=none --text-transform-rules=/std/math/*:recurse=operators";
  CHECK(runCommand(recurseCmd) == 0);
  CHECK(runCommand(exePath) == 3);
}

TEST_CASE("text transform rules accept recursive suffix alias") {
  const std::string source = R"(
namespace std {
  namespace math {
    namespace ops {
      [return<int>]
      add() {
        return(1i32+2i32)
      }
    }
  }
}

[return<int>]
main() {
  return(/std/math/ops/add())
}
)";
  const std::string srcPath = writeTemp("compile_text_rule_recursive_alias.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_text_rule_recursive_alias_exe").string();

  const std::string recursiveAliasCmd =
      "./primec --emit=exe " + quoteShellArg(srcPath) + " -o " + quoteShellArg(exePath) +
      " --entry /main --text-transforms=none --text-transform-rules=/std/math/*:recursive=operators";
  CHECK(runCommand(recursiveAliasCmd) == 0);
  CHECK(runCommand(exePath) == 3);
}

TEST_CASE("root wildcard transform rules only recurse when requested") {
  const std::string source = R"(
[return<int>]
main() {
  helper() {
    return(1i32+2i32)
  }
  return(/main/helper())
}
)";
  const std::string srcPath = writeTemp("compile_text_rule_root_wildcard_recurse.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_text_rule_root_wildcard_recurse_exe").string();
  const std::string errPath =
      (testScratchPath("") / "primec_text_rule_root_wildcard_recurse_err.txt").string();

  const std::string noRecurseCmd =
      "./primec --emit=exe " + quoteShellArg(srcPath) + " -o /dev/null --entry /main --text-transforms=none "
      "--text-transform-rules=/*=operators 2> " +
      quoteShellArg(errPath);
  CHECK(runCommand(noRecurseCmd) == 2);
  CHECK(readFile(errPath).find("Parse error") != std::string::npos);

  const std::string recurseCmd =
      "./primec --emit=exe " + quoteShellArg(srcPath) + " -o " + quoteShellArg(exePath) +
      " --entry /main --text-transforms=none --text-transform-rules=/*:recurse=operators";
  CHECK(runCommand(recurseCmd) == 0);
  CHECK(runCommand(exePath) == 3);
}
