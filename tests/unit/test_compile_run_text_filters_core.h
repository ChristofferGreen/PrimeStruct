TEST_SUITE_BEGIN("primestruct.compile.run.text_filters");

TEST_CASE("compiles and runs implicit i32 suffix") {
  const std::string source = R"(
[return<int>]
main() {
  return(8)
}
)";
  const std::string srcPath = writeTemp("compile_suffix.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_suffix_exe").string();
  const std::string nativePath = (std::filesystem::temp_directory_path() / "primec_suffix_native").string();

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
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_inc_dec_sugar_exe").string();
  const std::string nativePath = (std::filesystem::temp_directory_path() / "primec_inc_dec_sugar_native").string();

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

TEST_CASE("no transforms overrides text filters") {
  const std::string source = R"(
[return<int>]
main() {
  return(11)
}
)";
  const std::string srcPath = writeTemp("compile_no_transforms_override.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() / "primec_no_transforms_override_exe").string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o " + exePath +
      " --entry /main --no-transforms --text-filters=default,implicit-i32";
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
      (std::filesystem::temp_directory_path() / "primec_no_transforms_canonical_exe").string();
  const std::string nativePath =
      (std::filesystem::temp_directory_path() / "primec_no_transforms_canonical_native").string();

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
      (std::filesystem::temp_directory_path() / "primec_no_transforms_brace_ctor_exe").string();
  const std::string nativePath =
      (std::filesystem::temp_directory_path() / "primec_no_transforms_brace_ctor_native").string();

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
      (std::filesystem::temp_directory_path() / "primec_no_transforms_infix_err.txt").string();

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
      (std::filesystem::temp_directory_path() / "primec_no_transforms_float_suffix_err.txt").string();

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
      (std::filesystem::temp_directory_path() / "primec_no_transforms_float_f_suffix_err.txt").string();

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
      (std::filesystem::temp_directory_path() / "primec_no_transforms_inc_sugar_err.txt").string();

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
      (std::filesystem::temp_directory_path() / "primec_no_transforms_if_sugar_err.txt").string();

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
      (std::filesystem::temp_directory_path() / "primec_no_transforms_loop_sugar_err.txt").string();

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
      (std::filesystem::temp_directory_path() / "primec_no_transforms_while_sugar_err.txt").string();

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
      (std::filesystem::temp_directory_path() / "primec_no_transforms_for_sugar_err.txt").string();

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
      (std::filesystem::temp_directory_path() / "primec_no_transforms_index_sugar_err.txt").string();

  const std::string compileCmd =
      "./primec --emit=exe " + quoteShellArg(srcPath) + " -o /dev/null --entry /main --no-transforms 2> " +
      quoteShellArg(errPath);
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("indexing sugar") != std::string::npos);
}

TEST_CASE("text filters none disables implicit utf8") {
  const std::string source = R"(
[return<int> effects(io_out)]
main() {
  print_line("no suffix")
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_text_filters_none_utf8.prime", source);
  const std::string errPath =
      (std::filesystem::temp_directory_path() / "primec_text_filters_none_utf8_err.txt").string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main --text-filters=none 2> " + errPath;
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
      (std::filesystem::temp_directory_path() / "primec_transform_list_none_utf8_err.txt").string();

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
      (std::filesystem::temp_directory_path() / "primec_transform_list_default_i32_exe").string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main --transform-list=default";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 12);
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
      (std::filesystem::temp_directory_path() / "primec_transform_list_none_infix_err.txt").string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main --transform-list=none 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("Parse error") != std::string::npos);
}

TEST_CASE("text filters none rejects infix operators") {
  const std::string source = R"(
[return<int>]
main() {
  return(1i32 + 2i32)
}
)";
  const std::string srcPath = writeTemp("compile_text_filters_none_infix.prime", source);
  const std::string errPath =
      (std::filesystem::temp_directory_path() / "primec_text_filters_none_infix_err.txt").string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main --text-filters=none 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("Parse error") != std::string::npos);
}

TEST_CASE("text filters none still accepts canonical syntax") {
  const std::string source = R"(
[return<int>]
main() {
  return(plus(1i32, 2i32))
}
)";
  const std::string srcPath = writeTemp("compile_text_filters_none_canonical.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() / "primec_text_filters_none_canonical_exe").string();
  const std::string nativePath =
      (std::filesystem::temp_directory_path() / "primec_text_filters_none_canonical_native").string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main --text-filters=none";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 3);

  const std::string runVmCmd = "./primec --emit=vm " + srcPath + " --entry /main --text-filters=none";
  CHECK(runCommand(runVmCmd) == 3);

  const std::string compileNativeCmd =
      "./primec --emit=native " + srcPath + " -o " + nativePath + " --entry /main --text-filters=none";
  CHECK(runCommand(compileNativeCmd) == 0);
  CHECK(runCommand(nativePath) == 3);
}

TEST_CASE("compiles and runs implicit i32 via transform list") {
  const std::string source = R"(
[return<int>]
main() {
  return(9)
}
)";
  const std::string srcPath = writeTemp("compile_transform_list_i32.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_transform_list_i32_exe").string();

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
      (std::filesystem::temp_directory_path() / "primec_text_transforms_whitespace_exe").string();

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
      (std::filesystem::temp_directory_path() / "primec_single_type_to_return_err.txt").string();

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
      (std::filesystem::temp_directory_path() / "primec_single_type_to_return_marker_err.txt").string();

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
      (std::filesystem::temp_directory_path() / "primec_semantic_single_type_to_return_err.txt").string();

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
      (std::filesystem::temp_directory_path() / "primec_semantic_transform_rules_exe").string();
  const std::string errPath =
      (std::filesystem::temp_directory_path() / "primec_semantic_transform_rules_err.txt").string();

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

TEST_CASE("semantic transforms ignore text transforms") {
  const std::string source = R"(
[operators i32]
main() {
}
)";
  const std::string srcPath = writeTemp("compile_semantic_single_type_to_return_text.prime", source);
  const std::string errPath =
      (std::filesystem::temp_directory_path() / "primec_semantic_single_type_to_return_text_err.txt").string();

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
      (std::filesystem::temp_directory_path() / "primec_single_type_to_return_disabled_err.txt").string();

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
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_per_env_collections_exe").string();

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
      (std::filesystem::temp_directory_path() / "primec_per_env_override_err.txt").string();

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
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_per_env_binding_ops_exe").string();

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
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_per_env_exec_args_exe").string();

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
      (std::filesystem::temp_directory_path() / "primec_append_operators_exe").string();

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
      (std::filesystem::temp_directory_path() / "primec_append_operators_auto_exe").string();

  const std::string compileCmd =
      "./primec --emit=exe " + quoteShellArg(srcPath) + " -o " + quoteShellArg(exePath) +
      " --entry /main --transform-list=append_operators";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 3);
}

TEST_CASE("text transform rules apply to namespace paths") {
  const std::string source = R"(
namespace math {
  [return<int>]
  add() {
    return(1i32+2i32)
  }
}

[return<int>]
main() {
  return(/math/add())
}
)";
  const std::string srcPath = writeTemp("compile_text_rule_namespace.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_text_rule_namespace_exe").string();

  const std::string compileCmd =
      "./primec --emit=exe " + quoteShellArg(srcPath) + " -o " + quoteShellArg(exePath) +
      " --entry /main --text-transforms=none --text-transform-rules=/math/*=operators";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 3);
}

TEST_CASE("text transform rules apply without transform lists") {
  const std::string source = R"(
main() {
  return(1i32+2i32)
}
)";
  const std::string srcPath = writeTemp("compile_text_rule_no_list.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() / "primec_text_rule_no_list_exe").string();

  const std::string compileCmd =
      "./primec --emit=exe " + quoteShellArg(srcPath) + " -o " + quoteShellArg(exePath) +
      " --entry /main --text-transforms=none --text-transform-rules=/main=operators";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 3);
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
      (std::filesystem::temp_directory_path() / "primec_text_rule_nested_def_exe").string();

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
      (std::filesystem::temp_directory_path() / "primec_text_rule_nested_list_err.txt").string();

  const std::string compileCmd =
      "./primec --emit=exe " + quoteShellArg(srcPath) + " -o /dev/null --entry /main --text-transforms=none " +
      "--text-transform-rules=/main=operators 2> " + quoteShellArg(errPath);
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("Parse error") != std::string::npos);
}

TEST_CASE("text transform rules recurse when requested") {
  const std::string source = R"(
namespace math {
  namespace ops {
    [return<int>]
    add() {
      return(1i32+2i32)
    }
  }
}

[return<int>]
main() {
  return(/math/ops/add())
}
)";
  const std::string srcPath = writeTemp("compile_text_rule_recurse.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_text_rule_recurse_exe").string();
  const std::string errPath =
      (std::filesystem::temp_directory_path() / "primec_text_rule_recurse_err.txt").string();

  const std::string noRecurseCmd =
      "./primec --emit=exe " + quoteShellArg(srcPath) + " -o /dev/null --entry /main --text-transforms=none "
      "--text-transform-rules=/math/*=operators 2> " +
      quoteShellArg(errPath);
  CHECK(runCommand(noRecurseCmd) == 2);
  CHECK(readFile(errPath).find("Parse error") != std::string::npos);

  const std::string recurseCmd =
      "./primec --emit=exe " + quoteShellArg(srcPath) + " -o " + quoteShellArg(exePath) +
      " --entry /main --text-transforms=none --text-transform-rules=/math/*:recurse=operators";
  CHECK(runCommand(recurseCmd) == 0);
  CHECK(runCommand(exePath) == 3);
}
