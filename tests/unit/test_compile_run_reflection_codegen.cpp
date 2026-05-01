#include "test_compile_run_helpers.h"
#include "test_compile_run_reflection_codegen_helpers.h"

TEST_SUITE_BEGIN("primestruct.compile.run.reflection_codegen");

TEST_CASE("reflection codegen ast-semantic dump uses canonical helper order") {
  const std::string srcPath = writeTemp("compile_reflection_codegen_ast_semantic.prime", reflectionCodegenDumpSource());
  const std::string outPath =
      (testScratchPath("") / "primec_reflection_codegen_ast_semantic.txt").string();
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
  const std::string outPath = (testScratchPath("") / "primec_reflection_codegen_ir.txt").string();
  const std::string dumpCmd = "./primec " + quoteShellArg(srcPath) + " --dump-stage ir > " + quoteShellArg(outPath);

  CHECK(runCommand(dumpCmd) == 0);
  const std::string ir = readFile(outPath);

  const size_t equalPos = ir.find("/Pair/Equal(copy, baseline)");
  const size_t notEqualPos = ir.find("/Pair/NotEqual(copy, Pair{[x] 9i32, [y] 2i32})");
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
  [Pair] left{Pair{[x] 1i32, [y] 2i32}}
  [Pair] right{Pair{[x] 2i32, [y] 3i32}}
  return(/Pair/Compare(left, right))
}
)";
  const std::string srcPath = writeTemp("compile_reflection_compare_dump.prime", source);
  const std::string astOutPath =
      (testScratchPath("") / "primec_reflection_compare_ast_semantic.txt").string();
  const std::string irOutPath =
      (testScratchPath("") / "primec_reflection_compare_ir.txt").string();

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
  [Pair] value{Pair{[x] 1i32, [y] 2i32}}
  return(/Pair/Hash64(value))
}
)";
  const std::string srcPath = writeTemp("compile_reflection_hash64_dump.prime", source);
  const std::string astOutPath =
      (testScratchPath("") / "primec_reflection_hash64_ast_semantic.txt").string();
  const std::string irOutPath =
      (testScratchPath("") / "primec_reflection_hash64_ir.txt").string();

  const std::string astCmd =
      "./primec " + quoteShellArg(srcPath) + " --dump-stage ast-semantic > " + quoteShellArg(astOutPath);
  const std::string irCmd = "./primec " + quoteShellArg(srcPath) + " --dump-stage ir > " + quoteShellArg(irOutPath);
  CHECK(runCommand(astCmd) == 0);
  CHECK(runCommand(irCmd) == 0);

  const std::string ast = readFile(astOutPath);
  const std::string ir = readFile(irOutPath);
  CHECK(ast.find("/Pair/Hash64(") != std::string::npos);
  CHECK(ast.find("[public, return<u64>] /Pair/Hash64([/Pair] value)") != std::string::npos);
  CHECK(ir.find("return /Pair/Hash64(value)") != std::string::npos);
}

TEST_CASE("reflection compare helper rejects unsupported field envelope deterministically") {
  const std::string source = R"(
[struct reflect generate(Compare)]
Pair() {
  [array<i32>] values{array<i32>(1i32)}
}

[return<int>]
main() {
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_reflection_compare_ineligible_field.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_reflection_compare_ineligible_field.err.txt").string();
  const std::string cmd =
      "./primec " + quoteShellArg(srcPath) + " --emit=ir 2> " + quoteShellArg(errPath);

  CHECK(runCommand(cmd) == 2);
  const std::string error = readFile(errPath);
  CHECK(error.find("generated reflection helper /Pair/Compare does not support field envelope: values (array<i32>)") !=
        std::string::npos);
}

TEST_CASE("reflection hash64 helper rejects unsupported field envelope deterministically") {
  const std::string source = R"(
[struct reflect generate(Hash64)]
Pair() {
  [string] label{"x"utf8}
}

[return<int>]
main() {
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_reflection_hash64_ineligible_field.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_reflection_hash64_ineligible_field.err.txt").string();
  const std::string cmd =
      "./primec " + quoteShellArg(srcPath) + " --emit=ir 2> " + quoteShellArg(errPath);

  CHECK(runCommand(cmd) == 2);
  const std::string error = readFile(errPath);
  CHECK(error.find("generated reflection helper /Pair/Hash64 does not support field envelope: label (string)") !=
        std::string::npos);
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
  [Pair mut] value{Pair{[x] 9i32, [y] 8i32}}
  /Pair/Clear(value)
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_reflection_clear_dump.prime", source);
  const std::string astOutPath =
      (testScratchPath("") / "primec_reflection_clear_ast_semantic.txt").string();
  const std::string irOutPath =
      (testScratchPath("") / "primec_reflection_clear_ir.txt").string();

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

TEST_CASE("reflection copyfrom helper appears in ast-semantic and ir dumps") {
  const std::string source = R"(
[struct reflect generate(CopyFrom)]
Pair() {
  [i32] x{1i32}
  [i32] y{2i32}
}

[return<int>]
main() {
  [Pair mut] target{Pair{[x] 9i32, [y] 8i32}}
  [Pair] source{Pair{[x] 3i32, [y] 5i32}}
  /Pair/CopyFrom(target, source)
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_reflection_copyfrom_dump.prime", source);
  const std::string astOutPath =
      (testScratchPath("") / "primec_reflection_copyfrom_ast_semantic.txt").string();
  const std::string irOutPath =
      (testScratchPath("") / "primec_reflection_copyfrom_ir.txt").string();

  const std::string astCmd =
      "./primec " + quoteShellArg(srcPath) + " --dump-stage ast-semantic > " + quoteShellArg(astOutPath);
  const std::string irCmd = "./primec " + quoteShellArg(srcPath) + " --dump-stage ir > " + quoteShellArg(irOutPath);
  CHECK(runCommand(astCmd) == 0);
  CHECK(runCommand(irCmd) == 0);

  const std::string ast = readFile(astOutPath);
  const std::string ir = readFile(irOutPath);
  CHECK(ast.find("/Pair/CopyFrom(") != std::string::npos);
  CHECK(ast.find("assign(x(value), x(other))") != std::string::npos);
  CHECK(ast.find("assign(y(value), y(other))") != std::string::npos);
  CHECK(ir.find("call /Pair/CopyFrom(target, source)") != std::string::npos);
}

TEST_CASE("reflection validate helper appears in ast-semantic and ir dumps") {
  const std::string source = R"(
[struct reflect generate(Validate)]
Pair() {
  [i32] x{1i32}
  [bool] ok{true}
}

[return<int>]
main() {
  [Pair] value{Pair{[x] 9i32, [ok] true}}
  /Pair/Validate(value)
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_reflection_validate_dump.prime", source);
  const std::string astOutPath =
      (testScratchPath("") / "primec_reflection_validate_ast_semantic.txt").string();
  const std::string irOutPath =
      (testScratchPath("") / "primec_reflection_validate_ir.txt").string();

  const std::string astCmd =
      "./primec " + quoteShellArg(srcPath) + " --dump-stage ast-semantic > " + quoteShellArg(astOutPath);
  const std::string irCmd = "./primec " + quoteShellArg(srcPath) + " --dump-stage ir > " + quoteShellArg(irOutPath);
  CHECK(runCommand(astCmd) == 0);
  CHECK(runCommand(irCmd) == 0);

  const std::string ast = readFile(astOutPath);
  const std::string ir = readFile(irOutPath);
  CHECK(ast.find("/Pair/ValidateField_x(") != std::string::npos);
  CHECK(ast.find("/Pair/ValidateField_ok(") != std::string::npos);
  CHECK(ast.find("not(/Pair/ValidateField_x(value))") != std::string::npos);
  CHECK(ir.find("call /Pair/Validate(value)") != std::string::npos);
}

TEST_CASE("reflection validate helper rejects field hook collisions deterministically") {
  const std::string source = R"(
[struct reflect generate(Validate)]
Pair() {
  [i32] x{1i32}
}

[return<bool>]
/Pair/ValidateField_x([Pair] value) {
  return(true)
}

[return<int>]
main() {
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_reflection_validate_hook_collision.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_reflection_validate_hook_collision.err.txt").string();
  const std::string cmd =
      "./primec " + quoteShellArg(srcPath) + " --emit=ir 2> " + quoteShellArg(errPath);

  CHECK(runCommand(cmd) == 2);
  const std::string error = readFile(errPath);
  CHECK(error.find("generated reflection helper already exists: /Pair/ValidateField_x") != std::string::npos);
}

TEST_CASE("reflection serialize and deserialize helpers appear in ast-semantic and ir dumps") {
  const std::string source = R"(
[struct reflect generate(Serialize, Deserialize)]
Pair() {
  [i32] x{1i32}
  [bool] ok{true}
}

[return<int>]
main() {
  [Pair] input{Pair{[x] 7i32, [ok] true}}
  [array<u64>] payload{/Pair/Serialize(input)}
  [Pair mut] output{Pair{[x] 0i32, [ok] false}}
  /Pair/Deserialize(output, payload)
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_reflection_serialize_deserialize_dump.prime", source);
  const std::string astOutPath =
      (testScratchPath("") / "primec_reflection_serialize_deserialize_ast_semantic.txt").string();
  const std::string irOutPath =
      (testScratchPath("") / "primec_reflection_serialize_deserialize_ir.txt").string();

  const std::string astCmd =
      "./primec " + quoteShellArg(srcPath) + " --dump-stage ast-semantic > " + quoteShellArg(astOutPath);
  const std::string irCmd = "./primec " + quoteShellArg(srcPath) + " --dump-stage ir > " + quoteShellArg(irOutPath);
  CHECK(runCommand(astCmd) == 0);
  CHECK(runCommand(irCmd) == 0);

  const std::string ast = readFile(astOutPath);
  const std::string ir = readFile(irOutPath);
  CHECK(ast.find("/Pair/Serialize(") != std::string::npos);
  CHECK(ast.find("/Pair/Deserialize(") != std::string::npos);
  CHECK(ast.find("if(not_equal(at(payload, 0), 1u64)") != std::string::npos);
  CHECK(ast.find("assign(x(value), convert<i32>(at(payload, 1)))") != std::string::npos);
  CHECK(ir.find("let payload = /Pair/Serialize(input)") != std::string::npos);
  CHECK(ir.find("call /Pair/Deserialize(output, payload)") != std::string::npos);
}

TEST_CASE("reflection serialize helper rejects unsupported field envelope deterministically") {
  const std::string source = R"(
[struct reflect generate(Serialize)]
Pair() {
  [string] label{"x"utf8}
}

[return<int>]
main() {
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_reflection_serialize_ineligible_field.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_reflection_serialize_ineligible_field.err.txt").string();
  const std::string cmd =
      "./primec " + quoteShellArg(srcPath) + " --emit=ir 2> " + quoteShellArg(errPath);

  CHECK(runCommand(cmd) == 2);
  const std::string error = readFile(errPath);
  CHECK(error.find("generated reflection helper /Pair/Serialize does not support field envelope: label (string)") !=
        std::string::npos);
}

TEST_CASE("reflection deserialize helper rejects unsupported field envelope deterministically") {
  const std::string source = R"(
[struct reflect generate(Deserialize)]
Pair() {
  [string] label{"x"utf8}
}

[return<int>]
main() {
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_reflection_deserialize_ineligible_field.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_reflection_deserialize_ineligible_field.err.txt").string();
  const std::string cmd =
      "./primec " + quoteShellArg(srcPath) + " --emit=ir 2> " + quoteShellArg(errPath);

  CHECK(runCommand(cmd) == 2);
  const std::string error = readFile(errPath);
  CHECK(error.find("generated reflection helper /Pair/Deserialize does not support field envelope: label (string)") !=
        std::string::npos);
}

TEST_CASE("reflection ToString generator reports deferred diagnostic deterministically") {
  const std::string source = R"(
[struct reflect generate(ToString)]
Pair() {
  [i32] x{1i32}
}

[return<int>]
main() {
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_reflection_tostring_deferred.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_reflection_tostring_deferred.err.txt").string();
  const std::string cmd =
      "./primec " + quoteShellArg(srcPath) + " --emit=ir 2> " + quoteShellArg(errPath);

  CHECK(runCommand(cmd) == 2);
  const std::string error = readFile(errPath);
  CHECK(error.find("reflection generator ToString is deferred on /Pair") != std::string::npos);
  CHECK(error.find("use DebugPrint") != std::string::npos);
}
