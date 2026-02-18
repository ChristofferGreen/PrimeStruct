TEST_CASE("dump pre_ast shows includes and text filters") {
  const std::string libPath =
      writeTemp("compile_dump_pre_ast_lib.prime", "// PRE_AST_LIB\n[return<int>]\nhelper(){ return(1i32) }\n");
  const std::string source =
      "include<\"" + libPath + "\">\n"
      "[return<int> effects(io_out)]\n"
      "main(){\n"
      "  print_line(\"hello\")\n"
      "  return(1i32+2i32)\n"
      "}\n";
  const std::string srcPath = writeTemp("compile_dump_pre_ast.prime", source);
  const std::string outPath =
      (std::filesystem::temp_directory_path() / "primec_dump_pre_ast.txt").string();

  const std::string dumpCmd =
      "./primec " + quoteShellArg(srcPath) + " --dump-stage pre_ast > " + quoteShellArg(outPath);
  CHECK(runCommand(dumpCmd) == 0);
  const std::string preAst = readFile(outPath);
  CHECK(preAst.find("PRE_AST_LIB") != std::string::npos);
  CHECK(preAst.find("\"hello\"utf8") != std::string::npos);
  const size_t plusPos = preAst.find("plus(");
  CHECK(plusPos != std::string::npos);
  CHECK(preAst.find("1i32", plusPos) != std::string::npos);
  CHECK(preAst.find("2i32", plusPos) != std::string::npos);
}

TEST_CASE("dump ir prints canonical output") {
  const std::string source = R"(
[return<int>]
main() {
  return(1i32+2i32)
}
)";
  const std::string srcPath = writeTemp("compile_dump_ir.prime", source);
  const std::string outPath =
      (std::filesystem::temp_directory_path() / "primec_dump_ir.txt").string();

  const std::string dumpCmd =
      "./primec " + quoteShellArg(srcPath) + " --dump-stage ir > " + quoteShellArg(outPath);
  CHECK(runCommand(dumpCmd) == 0);
  const std::string ir = readFile(outPath);
  CHECK(ir.find("module {") != std::string::npos);
  CHECK(ir.find("def /main(): i32") != std::string::npos);
  CHECK(ir.find("return plus(1, 2)") != std::string::npos);
}

TEST_CASE("dump ast ignores semantic errors") {
  const std::string source = R"(
[return<int>]
main() {
  return(nope(1i32))
}
)";
  const std::string srcPath = writeTemp("compile_dump_ast_nope.prime", source);
  const std::string outPath =
      (std::filesystem::temp_directory_path() / "primec_dump_ast_nope.txt").string();

  const std::string dumpCmd =
      "./primec " + quoteShellArg(srcPath) + " --dump-stage ast > " + quoteShellArg(outPath);
  CHECK(runCommand(dumpCmd) == 0);
  const std::string ast = readFile(outPath);
  CHECK(ast.find("/main()") != std::string::npos);
  CHECK(ast.find("nope(1)") != std::string::npos);
}

TEST_CASE("dump stage rejects unknown value") {
  const std::string source = R"(
[return<int>]
main() {
  return(1i32)
}
)";
  const std::string srcPath = writeTemp("compile_dump_stage_unknown.prime", source);
  const std::string errPath =
      (std::filesystem::temp_directory_path() / "primec_dump_stage_unknown_err.txt").string();

  const std::string dumpCmd =
      "./primec " + quoteShellArg(srcPath) + " --dump-stage bananas 2> " + quoteShellArg(errPath);
  CHECK(runCommand(dumpCmd) == 2);
  CHECK(readFile(errPath).find("Unsupported dump stage: bananas") != std::string::npos);
}

TEST_CASE("compiles and runs implicit utf8 suffix by default") {
  const std::string source = R"(
[return<int> effects(io_out)]
main() {
  print_line("hello")
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_implicit_utf8.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_implicit_utf8_exe").string();
  const std::string nativePath = (std::filesystem::temp_directory_path() / "primec_implicit_utf8_native").string();

  const std::string compileCppCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCppCmd) == 0);
  CHECK(runCommand(exePath) == 0);

  const std::string runVmCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runVmCmd) == 0);

  const std::string compileNativeCmd = "./primec --emit=native " + srcPath + " -o " + nativePath + " --entry /main";
  CHECK(runCommand(compileNativeCmd) == 0);
  CHECK(runCommand(nativePath) == 0);
}

TEST_CASE("compiles and runs implicit hex literal") {
  const std::string source = R"(
[return<int>]
main() {
  return(0x2A)
}
)";
  const std::string srcPath = writeTemp("compile_hex.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_hex_exe").string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main --text-filters=default,implicit-i32";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 42);
}

TEST_CASE("compiles and runs float binding") {
  const std::string source = R"(
[return<int>]
main() {
  [float] value{1.5f}
  return(1i32)
}
)";
  const std::string srcPath = writeTemp("compile_float.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_float_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 1);
}

TEST_CASE("compiles and runs single-letter float suffix") {
  const std::string source = R"(
[return<int>]
main() {
  return(convert<int>(1f))
}
)";
  const std::string srcPath = writeTemp("compile_float_suffix_f.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_float_suffix_f_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 1);
}

TEST_CASE("compiles and runs float comparison") {
  const std::string source = R"(
[return<bool>]
main() {
  return(greater_than(1.5f, 0.5f))
}
)";
  const std::string srcPath = writeTemp("compile_float_compare.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_float_compare_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 1);
}

TEST_CASE("compiles and runs string comparison") {
  const std::string source = R"(
[return<bool>]
main() {
  return(equal("alpha"utf8, "alpha"utf8))
}
)";
  const std::string srcPath = writeTemp("compile_string_compare.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_string_compare_exe").string();
  const std::string nativePath = (std::filesystem::temp_directory_path() / "primec_string_compare_native").string();
  const std::string vmErrPath = (std::filesystem::temp_directory_path() / "primec_string_compare_vm_err.txt").string();
  const std::string nativeErrPath =
      (std::filesystem::temp_directory_path() / "primec_string_compare_native_err.txt").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 1);

  const std::string runVmCmd =
      "./primec --emit=vm " + quoteShellArg(srcPath) + " --entry /main 2> " + quoteShellArg(vmErrPath);
  CHECK(runCommand(runVmCmd) == 2);
  CHECK(readFile(vmErrPath).find("vm backend does not support string comparisons") != std::string::npos);

  const std::string compileNativeCmd =
      "./primec --emit=native " + quoteShellArg(srcPath) + " -o " + quoteShellArg(nativePath) + " --entry /main 2> " +
      quoteShellArg(nativeErrPath);
  CHECK(runCommand(compileNativeCmd) == 2);
  CHECK(readFile(nativeErrPath).find("native backend does not support string comparisons") != std::string::npos);
}

TEST_CASE("rejects mixed int/float arithmetic") {
  const std::string source = R"(
[return<int>]
main() {
  return(plus(1i32, 1.5f))
}
)";
  const std::string srcPath = writeTemp("compile_mixed_int_float.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_mixed_int_float_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 2);
}

TEST_CASE("rejects method call on array") {
  const std::string source = R"(
[return<int>]
main() {
  [array<i32>] values{array<i32>(1i32, 2i32)}
  return(values.inc())
}
)";
  const std::string srcPath = writeTemp("compile_method_array.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_method_array_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 2);
}

TEST_CASE("rejects method call on pointer") {
  const std::string source = R"(
[return<int>]
main() {
  [i32 mut] value{2i32}
  [Pointer<i32>] ptr{location(value)}
  return(ptr.inc())
}
)";
  const std::string srcPath = writeTemp("compile_method_pointer.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_method_pointer_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 2);
}

TEST_CASE("rejects method call on reference") {
  const std::string source = R"(
[return<int>]
main() {
  [i32 mut] value{2i32}
  [Reference<i32>] ref{location(value)}
  return(ref.inc())
}
)";
  const std::string srcPath = writeTemp("compile_method_reference.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_method_reference_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 2);
}

TEST_CASE("compiles and runs pointer operator sugar") {
  const std::string source = R"(
[return<int>]
main() {
  [i32 mut] value{7i32}
  [Pointer<i32>] ptr{&value}
  return(*ptr)
}
)";
  const std::string srcPath = writeTemp("compile_pointer_sugar.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_pointer_sugar_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 7);
}

TEST_CASE("rejects method call on map") {
  const std::string source = R"(
[return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>{1i32=2i32}}
  return(values.inc())
}
)";
  const std::string srcPath = writeTemp("compile_method_map.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_method_map_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 2);
}

TEST_CASE("implicit suffix disabled by default") {
  const std::string source = R"(
[return<int>]
main() {
  return(8)
}
)";
  const std::string srcPath = writeTemp("compile_suffix_off.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_suffix_off_exe").string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 2);
}

TEST_CASE("compiles and runs if") {
  const std::string source = R"(
[return<int>]
main() {
  [i32 mut] value{1i32}
  if(false, then(){
    [i32] temp{4i32}
    assign(value, temp)
  }, else(){ assign(value, 9i32) })
  return(value)
}
)";
  const std::string srcPath = writeTemp("compile_if.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_if_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 9);
}

TEST_CASE("compiles and runs if expression") {
  const std::string source = R"(
[return<int>]
main() {
  return(if(false, then(){ 4i32 }, else(){ 9i32 }))
}
)";
  const std::string srcPath = writeTemp("compile_if_expr.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_if_expr_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 9);
}

TEST_CASE("runs if expression in vm") {
  const std::string source = R"(
[return<int>]
main() {
  return(if(false, then(){ 4i32 }, else(){ 9i32 }))
}
)";
  const std::string srcPath = writeTemp("vm_if_expr.prime", source);
  const std::string runVmCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runVmCmd) == 9);
}

TEST_CASE("compiles and runs if block sugar in return expression") {
  const std::string source = R"(
[return<int>]
main() {
  return(if(true) { 4i32 } else { 9i32 })
}
)";
  const std::string srcPath = writeTemp("compile_if_expr_sugar.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_if_expr_sugar_exe").string();
  const std::string nativePath = (std::filesystem::temp_directory_path() / "primec_if_expr_sugar_native").string();

  const std::string compileCppCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCppCmd) == 0);
  CHECK(runCommand(exePath) == 4);

  const std::string runVmCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runVmCmd) == 4);

  const std::string compileNativeCmd = "./primec --emit=native " + srcPath + " -o " + nativePath + " --entry /main";
  CHECK(runCommand(compileNativeCmd) == 0);
  CHECK(runCommand(nativePath) == 4);
}

TEST_CASE("compiles and runs if expr block statements") {
  const std::string source = R"(
[return<int>]
main() {
  return(if(true) { [i32] x{4i32} plus(x, 1i32) } else { 0i32 })
}
)";
  const std::string srcPath = writeTemp("compile_if_expr_block_stmts.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_if_expr_block_stmts_exe").string();
  const std::string nativePath =
      (std::filesystem::temp_directory_path() / "primec_if_expr_block_stmts_native").string();

  const std::string compileCppCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCppCmd) == 0);
  CHECK(runCommand(exePath) == 5);

  const std::string runVmCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runVmCmd) == 5);

  const std::string compileNativeCmd = "./primec --emit=native " + srcPath + " -o " + nativePath + " --entry /main";
  CHECK(runCommand(compileNativeCmd) == 0);
  CHECK(runCommand(nativePath) == 5);
}

TEST_CASE("compiles and runs lazy if expression taking then branch") {
  const std::string source = R"(
[return<int>]
main() {
  [array<i32>] values{array<i32>(4i32)}
  [i32] n{values.count()}
  return(if(equal(n, 1i32), then(){ values[0i32] }, else(){ values[9i32] }))
}
)";
  const std::string srcPath = writeTemp("compile_if_lazy_then.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_if_lazy_then_exe").string();
  const std::string nativePath = (std::filesystem::temp_directory_path() / "primec_if_lazy_then_native").string();
  const std::string errPath = (std::filesystem::temp_directory_path() / "primec_if_lazy_then_err.txt").string();

  const std::string compileCppCmd =
      "./primec --emit=exe " + quoteShellArg(srcPath) + " -o " + quoteShellArg(exePath) + " --entry /main";
  CHECK(runCommand(compileCppCmd) == 0);
  const std::string runCmd = quoteShellArg(exePath) + " 2> " + quoteShellArg(errPath);
  CHECK(runCommand(runCmd) == 4);
  CHECK(readFile(errPath).empty());

  const std::string runVmCmd =
      "./primec --emit=vm " + quoteShellArg(srcPath) + " --entry /main 2> " + quoteShellArg(errPath);
  CHECK(runCommand(runVmCmd) == 4);
  CHECK(readFile(errPath).empty());

  const std::string compileNativeCmd =
      "./primec --emit=native " + quoteShellArg(srcPath) + " -o " + quoteShellArg(nativePath) + " --entry /main";
  CHECK(runCommand(compileNativeCmd) == 0);
  const std::string runNativeCmd = quoteShellArg(nativePath) + " 2> " + quoteShellArg(errPath);
  CHECK(runCommand(runNativeCmd) == 4);
  CHECK(readFile(errPath).empty());
}

TEST_CASE("compiles and runs lazy if expression taking else branch") {
  const std::string source = R"(
[return<int>]
main() {
  [array<i32>] values{array<i32>(4i32)}
  [i32] n{values.count()}
  return(if(equal(n, 0i32), then(){ values[9i32] }, else(){ values[0i32] }))
}
)";
  const std::string srcPath = writeTemp("compile_if_lazy_else.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_if_lazy_else_exe").string();
  const std::string nativePath = (std::filesystem::temp_directory_path() / "primec_if_lazy_else_native").string();
  const std::string errPath = (std::filesystem::temp_directory_path() / "primec_if_lazy_else_err.txt").string();

  const std::string compileCppCmd =
      "./primec --emit=exe " + quoteShellArg(srcPath) + " -o " + quoteShellArg(exePath) + " --entry /main";
  CHECK(runCommand(compileCppCmd) == 0);
  const std::string runCmd = quoteShellArg(exePath) + " 2> " + quoteShellArg(errPath);
  CHECK(runCommand(runCmd) == 4);
  CHECK(readFile(errPath).empty());

  const std::string runVmCmd =
      "./primec --emit=vm " + quoteShellArg(srcPath) + " --entry /main 2> " + quoteShellArg(errPath);
  CHECK(runCommand(runVmCmd) == 4);
  CHECK(readFile(errPath).empty());

  const std::string compileNativeCmd =
      "./primec --emit=native " + quoteShellArg(srcPath) + " -o " + quoteShellArg(nativePath) + " --entry /main";
  CHECK(runCommand(compileNativeCmd) == 0);
  const std::string runNativeCmd = quoteShellArg(nativePath) + " 2> " + quoteShellArg(errPath);
  CHECK(runCommand(runNativeCmd) == 4);
  CHECK(readFile(errPath).empty());
}

