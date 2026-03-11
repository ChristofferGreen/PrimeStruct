TEST_SUITE_BEGIN("primestruct.compile.run.reflection_codegen");

static std::string reflectionCodegenDumpSource() {
  return R"(
[struct reflect generate(Clone, DebugPrint, IsDefault, Default, NotEqual, Equal)]
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
  return(if(and(and(equalCopy, notEqual), isDefault), then() { 0i32 }, else() { 1i32 }))
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

TEST_CASE("reflection codegen ir dump keeps generated helper call sites") {
  const std::string srcPath = writeTemp("compile_reflection_codegen_ir.prime", reflectionCodegenDumpSource());
  const std::string outPath = (std::filesystem::temp_directory_path() / "primec_reflection_codegen_ir.txt").string();
  const std::string dumpCmd = "./primec " + quoteShellArg(srcPath) + " --dump-stage ir > " + quoteShellArg(outPath);

  CHECK(runCommand(dumpCmd) == 0);
  const std::string ir = readFile(outPath);

  const size_t equalPos = ir.find("/Pair/Equal(copy, baseline)");
  const size_t notEqualPos = ir.find("/Pair/NotEqual(copy, Pair([x] 9, [y] 2))");
  const size_t defaultPos = ir.find("/Pair/Default()");
  const size_t isDefaultPos = ir.find("/Pair/IsDefault(baseline)");
  const size_t clonePos = ir.find("/Pair/Clone(baseline)");
  const size_t debugPrintPos = ir.find("call /Pair/DebugPrint(copy)");

  REQUIRE(equalPos != std::string::npos);
  REQUIRE(notEqualPos != std::string::npos);
  REQUIRE(defaultPos != std::string::npos);
  REQUIRE(isDefaultPos != std::string::npos);
  REQUIRE(clonePos != std::string::npos);
  REQUIRE(debugPrintPos != std::string::npos);

  CHECK(defaultPos < clonePos);
  CHECK(clonePos < debugPrintPos);
  CHECK(debugPrintPos < equalPos);
  CHECK(equalPos < notEqualPos);
  CHECK(notEqualPos < isDefaultPos);
}

TEST_CASE("reflection compare helper appears in ast-semantic and ir dumps") {
  const std::string source = R"(
[struct reflect generate(Compare)]
Pair() {
  [i32] x{1i32}
  [i32] y{2i32}
}

[return<int>]
main() {
  [Pair] left{Pair([x] 1i32, [y] 2i32)}
  [Pair] right{Pair([x] 2i32, [y] 3i32)}
  return(/Pair/Compare(left, right))
}
)";
  const std::string srcPath = writeTemp("compile_reflection_compare_dump.prime", source);
  const std::string astOutPath =
      (std::filesystem::temp_directory_path() / "primec_reflection_compare_ast_semantic.txt").string();
  const std::string irOutPath =
      (std::filesystem::temp_directory_path() / "primec_reflection_compare_ir.txt").string();

  const std::string astCmd =
      "./primec " + quoteShellArg(srcPath) + " --dump-stage ast-semantic > " + quoteShellArg(astOutPath);
  const std::string irCmd = "./primec " + quoteShellArg(srcPath) + " --dump-stage ir > " + quoteShellArg(irOutPath);
  CHECK(runCommand(astCmd) == 0);
  CHECK(runCommand(irCmd) == 0);

  const std::string ast = readFile(astOutPath);
  const std::string ir = readFile(irOutPath);
  CHECK(ast.find("/Pair/Compare(") != std::string::npos);
  CHECK(ast.find("less_than(x(left), x(right))") != std::string::npos);
  CHECK(ast.find("greater_than(x(left), x(right))") != std::string::npos);
  CHECK(ir.find("return /Pair/Compare(left, right)") != std::string::npos);
}

TEST_CASE("reflection hash64 helper appears in ast-semantic and ir dumps") {
  const std::string source = R"(
[struct reflect generate(Hash64)]
Pair() {
  [i32] x{1i32}
  [i32] y{2i32}
}

[return<u64>]
main() {
  [Pair] value{Pair([x] 1i32, [y] 2i32)}
  return(/Pair/Hash64(value))
}
)";
  const std::string srcPath = writeTemp("compile_reflection_hash64_dump.prime", source);
  const std::string astOutPath =
      (std::filesystem::temp_directory_path() / "primec_reflection_hash64_ast_semantic.txt").string();
  const std::string irOutPath =
      (std::filesystem::temp_directory_path() / "primec_reflection_hash64_ir.txt").string();

  const std::string astCmd =
      "./primec " + quoteShellArg(srcPath) + " --dump-stage ast-semantic > " + quoteShellArg(astOutPath);
  const std::string irCmd = "./primec " + quoteShellArg(srcPath) + " --dump-stage ir > " + quoteShellArg(irOutPath);
  CHECK(runCommand(astCmd) == 0);
  CHECK(runCommand(irCmd) == 0);

  const std::string ast = readFile(astOutPath);
  const std::string ir = readFile(irOutPath);
  CHECK(ast.find("/Pair/Hash64(") != std::string::npos);
  CHECK(ast.find("convert<u64>(x(value))") != std::string::npos);
  CHECK(ast.find("convert<u64>(y(value))") != std::string::npos);
  CHECK(ir.find("return /Pair/Hash64(value)") != std::string::npos);
}

TEST_CASE("reflection clear helper appears in ast-semantic and ir dumps") {
  const std::string source = R"(
[struct reflect generate(Clear)]
Pair() {
  [i32] x{1i32}
  [i32] y{2i32}
}

[return<int>]
main() {
  [Pair mut] value{Pair([x] 9i32, [y] 8i32)}
  /Pair/Clear(value)
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_reflection_clear_dump.prime", source);
  const std::string astOutPath =
      (std::filesystem::temp_directory_path() / "primec_reflection_clear_ast_semantic.txt").string();
  const std::string irOutPath =
      (std::filesystem::temp_directory_path() / "primec_reflection_clear_ir.txt").string();

  const std::string astCmd =
      "./primec " + quoteShellArg(srcPath) + " --dump-stage ast-semantic > " + quoteShellArg(astOutPath);
  const std::string irCmd = "./primec " + quoteShellArg(srcPath) + " --dump-stage ir > " + quoteShellArg(irOutPath);
  CHECK(runCommand(astCmd) == 0);
  CHECK(runCommand(irCmd) == 0);

  const std::string ast = readFile(astOutPath);
  const std::string ir = readFile(irOutPath);
  CHECK(ast.find("/Pair/Clear(") != std::string::npos);
  CHECK(ast.find("assign(x(value), x(defaultValue))") != std::string::npos);
  CHECK(ast.find("assign(y(value), y(defaultValue))") != std::string::npos);
  CHECK(ir.find("call /Pair/Clear(value)") != std::string::npos);
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

TEST_CASE("reflection compare helper runtime stays aligned across backends") {
  const std::string source = R"(
[struct reflect generate(Compare)]
Pair() {
  [i32] x{0i32}
  [i32] y{0i32}
}

[return<int>]
main() {
  [Pair] low{Pair([x] 1i32, [y] 2i32)}
  [Pair] mid{Pair([x] 1i32, [y] 5i32)}
  [Pair] high{Pair([x] 2i32, [y] 0i32)}
  [i32 mut] score{0i32}
  if(less_than(/Pair/Compare(low, mid), 0i32), then() { assign(score, plus(score, 1i32)) }, else() { })
  if(greater_than(/Pair/Compare(high, mid), 0i32), then() { assign(score, plus(score, 2i32)) }, else() { })
  if(equal(/Pair/Compare(low, low), 0i32), then() { assign(score, plus(score, 4i32)) }, else() { })
  return(score)
}
)";
  const std::string srcPath = writeTemp("compile_reflection_compare_runtime.prime", source);

  const std::string vmCmd = "./primec --emit=vm " + quoteShellArg(srcPath) + " --entry /main";
  CHECK(runCommand(vmCmd) == 7);

  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_reflection_compare_exe").string();
  const std::string exeCompileCmd =
      "./primec --emit=exe " + quoteShellArg(srcPath) + " -o " + quoteShellArg(exePath) + " --entry /main";
  CHECK(runCommand(exeCompileCmd) == 0);
  CHECK(runCommand(quoteShellArg(exePath)) == 7);

  const std::string nativePath = (std::filesystem::temp_directory_path() / "primec_reflection_compare_native").string();
  const std::string nativeCompileCmd =
      "./primec --emit=native " + quoteShellArg(srcPath) + " -o " + quoteShellArg(nativePath) + " --entry /main";
  CHECK(runCommand(nativeCompileCmd) == 0);
  CHECK(runCommand(quoteShellArg(nativePath)) == 7);
}

TEST_CASE("reflection hash64 helper runtime stays aligned across backends") {
  const std::string source = R"(
[struct reflect generate(Hash64)]
Pair() {
  [i32] x{0i32}
  [i32] y{0i32}
}

[return<int>]
main() {
  [Pair] first{Pair([x] 3i32, [y] 5i32)}
  [Pair] same{Pair([x] 3i32, [y] 5i32)}
  [Pair] swapped{Pair([x] 5i32, [y] 3i32)}
  [u64] firstHash{/Pair/Hash64(first)}
  [u64] sameHash{/Pair/Hash64(same)}
  [u64] swappedHash{/Pair/Hash64(swapped)}
  [bool] sameOk{equal(firstHash, sameHash)}
  [bool] orderOk{not_equal(firstHash, swappedHash)}
  return(if(and(sameOk, orderOk), then() { 7i32 }, else() { 3i32 }))
}
)";
  const std::string srcPath = writeTemp("compile_reflection_hash64_runtime.prime", source);

  const std::string vmCmd = "./primec --emit=vm " + quoteShellArg(srcPath) + " --entry /main";
  CHECK(runCommand(vmCmd) == 7);

  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_reflection_hash64_exe").string();
  const std::string exeCompileCmd =
      "./primec --emit=exe " + quoteShellArg(srcPath) + " -o " + quoteShellArg(exePath) + " --entry /main";
  CHECK(runCommand(exeCompileCmd) == 0);
  CHECK(runCommand(quoteShellArg(exePath)) == 7);

  const std::string nativePath = (std::filesystem::temp_directory_path() / "primec_reflection_hash64_native").string();
  const std::string nativeCompileCmd =
      "./primec --emit=native " + quoteShellArg(srcPath) + " -o " + quoteShellArg(nativePath) + " --entry /main";
  CHECK(runCommand(nativeCompileCmd) == 0);
  CHECK(runCommand(quoteShellArg(nativePath)) == 7);
}

TEST_CASE("reflection clear helper runtime stays aligned across backends") {
  const std::string source = R"(
[struct reflect generate(Clear, Default, Compare)]
Pair() {
  [i32] x{1i32}
  [i32] y{2i32}
}

[return<int>]
main() {
  [Pair mut] value{Pair([x] 9i32, [y] 8i32)}
  /Pair/Clear(value)
  [i32] cmp{/Pair/Compare(value, /Pair/Default())}
  return(if(equal(cmp, 0i32), then() { 7i32 }, else() { 3i32 }))
}
)";
  const std::string srcPath = writeTemp("compile_reflection_clear_runtime.prime", source);

  const std::string vmCmd = "./primec --emit=vm " + quoteShellArg(srcPath) + " --entry /main";
  CHECK(runCommand(vmCmd) == 7);

  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_reflection_clear_exe").string();
  const std::string exeCompileCmd =
      "./primec --emit=exe " + quoteShellArg(srcPath) + " -o " + quoteShellArg(exePath) + " --entry /main";
  CHECK(runCommand(exeCompileCmd) == 0);
  CHECK(runCommand(quoteShellArg(exePath)) == 7);

  const std::string nativePath = (std::filesystem::temp_directory_path() / "primec_reflection_clear_native").string();
  const std::string nativeCompileCmd =
      "./primec --emit=native " + quoteShellArg(srcPath) + " -o " + quoteShellArg(nativePath) + " --entry /main";
  CHECK(runCommand(nativeCompileCmd) == 0);
  CHECK(runCommand(quoteShellArg(nativePath)) == 7);
}

TEST_SUITE_END();
