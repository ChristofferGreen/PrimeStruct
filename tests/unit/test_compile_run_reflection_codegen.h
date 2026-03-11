TEST_SUITE_BEGIN("primestruct.compile.run.reflection_codegen");

static std::string reflectionCodegenDumpSource() {
  return R"(
[struct reflect generate(Clone, DebugPrint, IsDefault, Default, NotEqual, Equal)]
Pair() {
  [i32] x{1i32}
  [i32] y{2i32}
}

[return<int>]
main() {
  return(0i32)
}
)";
}

static std::string reflectionCodegenRuntimeSource() {
  return R"(
[struct reflect generate(Equal, NotEqual, Default, IsDefault, Clone, DebugPrint)]
Pair() {
  [i32] x{1i32}
  [i32] y{2i32}
}

[return<int> effects(io_out)]
main() {
  [Pair] baseline{/Pair/Default()}
  [Pair] copy{/Pair/Clone(baseline)}
  /Pair/DebugPrint(copy)
  [bool] equalCopy{/Pair/Equal(copy, baseline)}
  [bool] notEqual{/Pair/NotEqual(copy, Pair([x] 9i32, [y] 2i32))}
  [bool] isDefault{/Pair/IsDefault(baseline)}
  return(if(and(and(equalCopy, notEqual), isDefault), then() { 7i32 }, else() { 3i32 }))
}
)";
}

TEST_CASE("reflection codegen ast-semantic dump uses canonical helper order") {
  const std::string srcPath = writeTemp("compile_reflection_codegen_ast_semantic.prime", reflectionCodegenDumpSource());
  const std::string outPath =
      (std::filesystem::temp_directory_path() / "primec_reflection_codegen_ast_semantic.txt").string();
  const std::string dumpCmd =
      "./primec " + quoteShellArg(srcPath) + " --dump-stage ast-semantic > " + quoteShellArg(outPath);

  CHECK(runCommand(dumpCmd) == 0);
  const std::string ast = readFile(outPath);

  const size_t equalPos = ast.find("/Pair/Equal(");
  const size_t notEqualPos = ast.find("/Pair/NotEqual(");
  const size_t defaultPos = ast.find("/Pair/Default(");
  const size_t isDefaultPos = ast.find("/Pair/IsDefault(");
  const size_t clonePos = ast.find("/Pair/Clone(");
  const size_t debugPrintPos = ast.find("/Pair/DebugPrint(");
  const size_t pairStructPos = ast.find("/Pair()");

  REQUIRE(equalPos != std::string::npos);
  REQUIRE(notEqualPos != std::string::npos);
  REQUIRE(defaultPos != std::string::npos);
  REQUIRE(isDefaultPos != std::string::npos);
  REQUIRE(clonePos != std::string::npos);
  REQUIRE(debugPrintPos != std::string::npos);
  REQUIRE(pairStructPos != std::string::npos);

  CHECK(equalPos < notEqualPos);
  CHECK(notEqualPos < defaultPos);
  CHECK(defaultPos < isDefaultPos);
  CHECK(isDefaultPos < clonePos);
  CHECK(clonePos < debugPrintPos);
  CHECK(debugPrintPos < pairStructPos);
}

TEST_CASE("reflection codegen ir dump includes generated helper definitions") {
  const std::string srcPath = writeTemp("compile_reflection_codegen_ir.prime", reflectionCodegenDumpSource());
  const std::string outPath = (std::filesystem::temp_directory_path() / "primec_reflection_codegen_ir.txt").string();
  const std::string dumpCmd = "./primec " + quoteShellArg(srcPath) + " --dump-stage ir > " + quoteShellArg(outPath);

  CHECK(runCommand(dumpCmd) == 0);
  const std::string ir = readFile(outPath);

  const size_t equalPos = ir.find("def /Pair/Equal");
  const size_t notEqualPos = ir.find("def /Pair/NotEqual");
  const size_t defaultPos = ir.find("def /Pair/Default");
  const size_t isDefaultPos = ir.find("def /Pair/IsDefault");
  const size_t clonePos = ir.find("def /Pair/Clone");
  const size_t debugPrintPos = ir.find("def /Pair/DebugPrint");

  REQUIRE(equalPos != std::string::npos);
  REQUIRE(notEqualPos != std::string::npos);
  REQUIRE(defaultPos != std::string::npos);
  REQUIRE(isDefaultPos != std::string::npos);
  REQUIRE(clonePos != std::string::npos);
  REQUIRE(debugPrintPos != std::string::npos);

  CHECK(equalPos < notEqualPos);
  CHECK(notEqualPos < defaultPos);
  CHECK(defaultPos < isDefaultPos);
  CHECK(isDefaultPos < clonePos);
  CHECK(clonePos < debugPrintPos);
}

TEST_CASE("reflection codegen helper runtime stays aligned across backends") {
  const std::string source = reflectionCodegenRuntimeSource();
  const std::string srcPath = writeTemp("compile_reflection_codegen_runtime.prime", source);
  const std::string expectedOut = "/Pair {\n  x\n  y\n}\n";

  const std::string vmOutPath = (std::filesystem::temp_directory_path() / "primec_reflection_codegen_vm.out.txt").string();
  const std::string vmCmd =
      "./primec --emit=vm " + quoteShellArg(srcPath) + " --entry /main > " + quoteShellArg(vmOutPath);
  CHECK(runCommand(vmCmd) == 7);
  CHECK(readFile(vmOutPath) == expectedOut);

  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_reflection_codegen_exe").string();
  const std::string exeCompileCmd =
      "./primec --emit=exe " + quoteShellArg(srcPath) + " -o " + quoteShellArg(exePath) + " --entry /main";
  CHECK(runCommand(exeCompileCmd) == 0);
  const std::string exeOutPath =
      (std::filesystem::temp_directory_path() / "primec_reflection_codegen_exe.out.txt").string();
  const std::string exeRunCmd = quoteShellArg(exePath) + " > " + quoteShellArg(exeOutPath);
  CHECK(runCommand(exeRunCmd) == 7);
  CHECK(readFile(exeOutPath) == expectedOut);

  const std::string nativePath = (std::filesystem::temp_directory_path() / "primec_reflection_codegen_native").string();
  const std::string nativeCompileCmd =
      "./primec --emit=native " + quoteShellArg(srcPath) + " -o " + quoteShellArg(nativePath) + " --entry /main";
  CHECK(runCommand(nativeCompileCmd) == 0);
  const std::string nativeOutPath =
      (std::filesystem::temp_directory_path() / "primec_reflection_codegen_native.out.txt").string();
  const std::string nativeRunCmd = quoteShellArg(nativePath) + " > " + quoteShellArg(nativeOutPath);
  CHECK(runCommand(nativeRunCmd) == 7);
  CHECK(readFile(nativeOutPath) == expectedOut);
}

TEST_SUITE_END();
