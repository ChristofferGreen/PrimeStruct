TEST_CASE("dump pre_ast shows imports and text filters") {
  const std::string libPath =
      writeTemp("compile_dump_pre_ast_lib.prime", "// PRE_AST_LIB\n[return<int>]\nhelper(){ return(1i32) }\n");
  const std::string source =
      "import<\"" + libPath + "\">\n"
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

TEST_CASE("dump ast-semantic shows canonicalized ast") {
  const std::string source = R"(
[enum]
Colors() {
  Red
  Green
}

[return<int>]
main() {
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_dump_ast_semantic.prime", source);
  const std::string outPath =
      (std::filesystem::temp_directory_path() / "primec_dump_ast_semantic.txt").string();

  const std::string dumpCmd =
      "./primec " + quoteShellArg(srcPath) + " --dump-stage ast-semantic > " + quoteShellArg(outPath);
  CHECK(runCommand(dumpCmd) == 0);
  const std::string ast = readFile(outPath);
  CHECK(ast.find("[struct] /Colors()") != std::string::npos);
  CHECK(ast.find("[i32] value{0}") != std::string::npos);
  CHECK(ast.find("Red{/Colors(0)}") != std::string::npos);
  CHECK(ast.find("Green{/Colors(1)}") != std::string::npos);
}

TEST_CASE("dump ast_semantic alias works") {
  const std::string source = R"(
[return<int>]
main() {
  return(1i32)
}
)";
  const std::string srcPath = writeTemp("compile_dump_ast_semantic_alias.prime", source);
  const std::string hyphenOut =
      (std::filesystem::temp_directory_path() / "primec_dump_ast_semantic_hyphen.txt").string();
  const std::string underscoreOut =
      (std::filesystem::temp_directory_path() / "primec_dump_ast_semantic_underscore.txt").string();

  const std::string hyphenCmd =
      "./primec " + quoteShellArg(srcPath) + " --dump-stage ast-semantic > " + quoteShellArg(hyphenOut);
  const std::string underscoreCmd =
      "./primec " + quoteShellArg(srcPath) + " --dump-stage ast_semantic > " + quoteShellArg(underscoreOut);
  CHECK(runCommand(hyphenCmd) == 0);
  CHECK(runCommand(underscoreCmd) == 0);
  CHECK(readFile(hyphenOut) == readFile(underscoreOut));
}

TEST_CASE("dump ast-semantic reports semantic errors") {
  const std::string source = R"(
[return<int>]
main() {
  return(nope(1i32))
}
)";
  const std::string srcPath = writeTemp("compile_dump_ast_semantic_nope.prime", source);
  const std::string errPath =
      (std::filesystem::temp_directory_path() / "primec_dump_ast_semantic_nope_err.txt").string();

  const std::string dumpCmd =
      "./primec " + quoteShellArg(srcPath) + " --dump-stage ast-semantic 2> " + quoteShellArg(errPath);
  CHECK(runCommand(dumpCmd) == 2);
  CHECK(readFile(errPath).find("Semantic error: unknown call target: nope") != std::string::npos);
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

TEST_CASE("primec and primevm dump pre_ast match") {
  const std::string libPath =
      writeTemp("compile_dump_shared_lib.prime", "[return<int>]\nhelper(){ return(2i32) }\n");
  const std::string source =
      "import<\"" + libPath + "\">\n"
      "[return<int>]\n"
      "main(){\n"
      "  return(helper()+1i32)\n"
      "}\n";
  const std::string srcPath = writeTemp("compile_dump_shared.prime", source);
  const std::string primecOut =
      (std::filesystem::temp_directory_path() / "primec_dump_shared_pre_ast.txt").string();
  const std::string primevmOut =
      (std::filesystem::temp_directory_path() / "primevm_dump_shared_pre_ast.txt").string();

  const std::string primecCmd =
      "./primec " + quoteShellArg(srcPath) + " --dump-stage pre_ast > " + quoteShellArg(primecOut);
  const std::string primevmCmd =
      "./primevm " + quoteShellArg(srcPath) + " --dump-stage pre_ast > " + quoteShellArg(primevmOut);
  CHECK(runCommand(primecCmd) == 0);
  CHECK(runCommand(primevmCmd) == 0);
  CHECK(readFile(primecOut) == readFile(primevmOut));
}

TEST_CASE("primec and primevm dump ast-semantic match") {
  const std::string source = R"(
[enum]
Colors() {
  Red
  Green
}

[return<int>]
main() {
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_dump_shared_ast_semantic.prime", source);
  const std::string primecOut =
      (std::filesystem::temp_directory_path() / "primec_dump_shared_ast_semantic.txt").string();
  const std::string primevmOut =
      (std::filesystem::temp_directory_path() / "primevm_dump_shared_ast_semantic.txt").string();

  const std::string primecCmd =
      "./primec " + quoteShellArg(srcPath) + " --dump-stage ast-semantic > " + quoteShellArg(primecOut);
  const std::string primevmCmd =
      "./primevm " + quoteShellArg(srcPath) + " --dump-stage ast-semantic > " + quoteShellArg(primevmOut);
  CHECK(runCommand(primecCmd) == 0);
  CHECK(runCommand(primevmCmd) == 0);
  CHECK(readFile(primecOut) == readFile(primevmOut));
}

TEST_CASE("primevm dump stage rejects unknown value") {
  const std::string source = R"(
[return<int>]
main() {
  return(1i32)
}
)";
  const std::string srcPath = writeTemp("primevm_dump_stage_unknown.prime", source);
  const std::string errPath =
      (std::filesystem::temp_directory_path() / "primevm_dump_stage_unknown_err.txt").string();

  const std::string dumpCmd =
      "./primevm " + quoteShellArg(srcPath) + " --dump-stage bananas 2> " + quoteShellArg(errPath);
  CHECK(runCommand(dumpCmd) == 2);
  CHECK(readFile(errPath).find("Unsupported dump stage: bananas") != std::string::npos);
}

TEST_CASE("primec plain parse diagnostics include file line and caret") {
  const std::string source = R"(
[return<int>]
main( {
  return(1i32)
}
)";
  const std::string srcPath = writeTemp("primec_plain_parse_diagnostic.prime", source);
  const std::string errPath =
      (std::filesystem::temp_directory_path() / "primec_plain_parse_diagnostic_err.txt").string();

  const std::string cmd = "./primec " + quoteShellArg(srcPath) + " 2> " + quoteShellArg(errPath);
  CHECK(runCommand(cmd) == 2);

  const std::string diagnostics = readFile(errPath);
  CHECK(diagnostics.find(srcPath + ":3:7: error: Parse error:") != std::string::npos);
  CHECK(diagnostics.find("3 | main( {") != std::string::npos);
  CHECK(diagnostics.find("^") != std::string::npos);
}

TEST_CASE("primevm plain semantic diagnostics include file line and note") {
  const std::string source =
      "[return<int>]\n"
      "main() {\n"
      "  return(nope(1i32))\n"
      "}\n";
  const std::string srcPath = writeTemp("primevm_plain_semantic_diagnostic.prime", source);
  const std::string errPath =
      (std::filesystem::temp_directory_path() / "primevm_plain_semantic_diagnostic_err.txt").string();

  const std::string cmd = "./primevm " + quoteShellArg(srcPath) + " --entry /main 2> " + quoteShellArg(errPath);
  CHECK(runCommand(cmd) == 2);

  const std::string diagnostics = readFile(errPath);
  CHECK(diagnostics.find(srcPath + ":3:") != std::string::npos);
  CHECK(diagnostics.find(": error: Semantic error: unknown call target: nope") != std::string::npos);
  CHECK(diagnostics.find("3 |   return(nope(1i32))") != std::string::npos);
  CHECK(diagnostics.find("note: definition: /main") != std::string::npos);
}

TEST_CASE("primec emit-diagnostics reports structured parse payload") {
  const std::string source = R"(
[return<int>]
main() {
  return(1i32
}
)";
  const std::string srcPath = writeTemp("primec_emit_diagnostics_parse.prime", source);
  const std::string errPath =
      (std::filesystem::temp_directory_path() / "primec_emit_diagnostics_parse_err.json").string();

  const std::string cmd =
      "./primec " + quoteShellArg(srcPath) + " --emit-diagnostics 2> " + quoteShellArg(errPath);
  CHECK(runCommand(cmd) == 2);

  const std::string diagnostics = readFile(errPath);
  CHECK(diagnostics.find("\"version\":1") != std::string::npos);
  CHECK(diagnostics.find("\"code\":\"PSC1003\"") != std::string::npos);
  CHECK(diagnostics.find("\"severity\":\"error\"") != std::string::npos);
  CHECK(diagnostics.find("\"message\":\"") != std::string::npos);
  CHECK(diagnostics.find("\"line\":0") == std::string::npos);
  CHECK(diagnostics.find("\"column\":0") == std::string::npos);
  CHECK(diagnostics.find("\"related_spans\":[]") != std::string::npos);
}

TEST_CASE("primec collect-diagnostics emits stable multi-parse payload") {
  const std::string source = R"(
import bad
import /std/math
[return<int>]
main() {
  return(1i32)
}
)";
  const std::string srcPath = writeTemp("primec_collect_diagnostics_parse.prime", source);
  const std::string errPath =
      (std::filesystem::temp_directory_path() / "primec_collect_diagnostics_parse_err.json").string();

  const std::string cmd = "./primec " + quoteShellArg(srcPath) +
                          " --emit-diagnostics --collect-diagnostics 2> " + quoteShellArg(errPath);
  CHECK(runCommand(cmd) == 2);

  const std::string diagnostics = readFile(errPath);
  CHECK(diagnostics.find("\"version\":1") != std::string::npos);
  CHECK(diagnostics.find("\"code\":\"PSC1003\"") != std::string::npos);
  CHECK(diagnostics.find("\"message\":\"import path must be a slash path\"") != std::string::npos);
  CHECK(diagnostics.find("\"message\":\"import /std/math is not supported; use import /std/math/* or /std/math/<name>\"") !=
        std::string::npos);

  size_t parseCount = 0;
  size_t scan = 0;
  while ((scan = diagnostics.find("\"code\":\"PSC1003\"", scan)) != std::string::npos) {
    ++parseCount;
    scan += 16;
  }
  CHECK(parseCount == 2);

  const size_t firstMessage = diagnostics.find("\"message\":\"import path must be a slash path\"");
  const size_t secondMessage = diagnostics.find(
      "\"message\":\"import /std/math is not supported; use import /std/math/* or /std/math/<name>\"");
  REQUIRE(firstMessage != std::string::npos);
  REQUIRE(secondMessage != std::string::npos);
  CHECK(firstMessage < secondMessage);
}

TEST_CASE("primevm collect-diagnostics emits stable multi-parse payload") {
  const std::string source = R"(
import bad
import /std/math
[return<int>]
main() {
  return(1i32)
}
)";
  const std::string srcPath = writeTemp("primevm_collect_diagnostics_parse.prime", source);
  const std::string errPath =
      (std::filesystem::temp_directory_path() / "primevm_collect_diagnostics_parse_err.json").string();

  const std::string cmd = "./primevm " + quoteShellArg(srcPath) +
                          " --emit-diagnostics --collect-diagnostics 2> " + quoteShellArg(errPath);
  CHECK(runCommand(cmd) == 2);

  const std::string diagnostics = readFile(errPath);
  CHECK(diagnostics.find("\"version\":1") != std::string::npos);
  CHECK(diagnostics.find("\"code\":\"PSC1003\"") != std::string::npos);
  CHECK(diagnostics.find("\"message\":\"import path must be a slash path\"") != std::string::npos);
  CHECK(diagnostics.find("\"message\":\"import /std/math is not supported; use import /std/math/* or /std/math/<name>\"") !=
        std::string::npos);

  size_t parseCount = 0;
  size_t scan = 0;
  while ((scan = diagnostics.find("\"code\":\"PSC1003\"", scan)) != std::string::npos) {
    ++parseCount;
    scan += 16;
  }
  CHECK(parseCount == 2);

  const size_t firstMessage = diagnostics.find("\"message\":\"import path must be a slash path\"");
  const size_t secondMessage = diagnostics.find(
      "\"message\":\"import /std/math is not supported; use import /std/math/* or /std/math/<name>\"");
  REQUIRE(firstMessage != std::string::npos);
  REQUIRE(secondMessage != std::string::npos);
  CHECK(firstMessage < secondMessage);
}

TEST_CASE("primec collect-diagnostics emits stable multi-semantic payload for duplicates") {
  const std::string source = R"(
[return<int>]
dup() {
  return(1i32)
}
[return<int>]
dup() {
  return(2i32)
}
[return<int>]
other() {
  return(3i32)
}
[return<int>]
other() {
  return(4i32)
}
[return<int>]
main() {
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("primec_collect_diagnostics_semantic_dupes.prime", source);
  const std::string errPath =
      (std::filesystem::temp_directory_path() / "primec_collect_diagnostics_semantic_dupes_err.json").string();

  const std::string cmd = "./primec " + quoteShellArg(srcPath) +
                          " --emit-diagnostics --collect-diagnostics 2> " + quoteShellArg(errPath);
  CHECK(runCommand(cmd) == 2);

  const std::string diagnostics = readFile(errPath);
  CHECK(diagnostics.find("\"version\":1") != std::string::npos);
  CHECK(diagnostics.find("\"code\":\"PSC1005\"") != std::string::npos);
  CHECK(diagnostics.find("\"message\":\"duplicate definition: /dup\"") != std::string::npos);
  CHECK(diagnostics.find("\"message\":\"duplicate definition: /other\"") != std::string::npos);
  CHECK(diagnostics.find("\"label\":\"definition: /dup\"") != std::string::npos);
  CHECK(diagnostics.find("\"label\":\"definition: /other\"") != std::string::npos);

  size_t semanticCount = 0;
  size_t scan = 0;
  while ((scan = diagnostics.find("\"code\":\"PSC1005\"", scan)) != std::string::npos) {
    ++semanticCount;
    scan += 16;
  }
  CHECK(semanticCount == 2);

  const size_t firstMessage = diagnostics.find("\"message\":\"duplicate definition: /dup\"");
  const size_t secondMessage = diagnostics.find("\"message\":\"duplicate definition: /other\"");
  REQUIRE(firstMessage != std::string::npos);
  REQUIRE(secondMessage != std::string::npos);
  CHECK(firstMessage < secondMessage);
}

TEST_CASE("primevm collect-diagnostics emits stable multi-semantic payload for duplicates") {
  const std::string source = R"(
[return<int>]
dup() {
  return(1i32)
}
[return<int>]
dup() {
  return(2i32)
}
[return<int>]
other() {
  return(3i32)
}
[return<int>]
other() {
  return(4i32)
}
[return<int>]
main() {
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("primevm_collect_diagnostics_semantic_dupes.prime", source);
  const std::string errPath =
      (std::filesystem::temp_directory_path() / "primevm_collect_diagnostics_semantic_dupes_err.json").string();

  const std::string cmd = "./primevm " + quoteShellArg(srcPath) +
                          " --emit-diagnostics --collect-diagnostics 2> " + quoteShellArg(errPath);
  CHECK(runCommand(cmd) == 2);

  const std::string diagnostics = readFile(errPath);
  CHECK(diagnostics.find("\"version\":1") != std::string::npos);
  CHECK(diagnostics.find("\"code\":\"PSC1005\"") != std::string::npos);
  CHECK(diagnostics.find("\"message\":\"duplicate definition: /dup\"") != std::string::npos);
  CHECK(diagnostics.find("\"message\":\"duplicate definition: /other\"") != std::string::npos);
  CHECK(diagnostics.find("\"label\":\"definition: /dup\"") != std::string::npos);
  CHECK(diagnostics.find("\"label\":\"definition: /other\"") != std::string::npos);

  size_t semanticCount = 0;
  size_t scan = 0;
  while ((scan = diagnostics.find("\"code\":\"PSC1005\"", scan)) != std::string::npos) {
    ++semanticCount;
    scan += 16;
  }
  CHECK(semanticCount == 2);

  const size_t firstMessage = diagnostics.find("\"message\":\"duplicate definition: /dup\"");
  const size_t secondMessage = diagnostics.find("\"message\":\"duplicate definition: /other\"");
  REQUIRE(firstMessage != std::string::npos);
  REQUIRE(secondMessage != std::string::npos);
  CHECK(firstMessage < secondMessage);
}

TEST_CASE("primec collect-diagnostics emits stable multi-semantic payload for import errors") {
  const std::string source = R"(
import /std/math
import /hidden
import /missing
[return<int>]
hidden() {
  return(7i32)
}
[return<int>]
main() {
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("primec_collect_diagnostics_semantic_imports.prime", source);
  const std::string errPath =
      (std::filesystem::temp_directory_path() / "primec_collect_diagnostics_semantic_imports_err.json").string();

  const std::string cmd = "./primec " + quoteShellArg(srcPath) +
                          " --emit-diagnostics --collect-diagnostics 2> " + quoteShellArg(errPath);
  CHECK(runCommand(cmd) == 2);

  const std::string diagnostics = readFile(errPath);
  CHECK(diagnostics.find("\"version\":1") != std::string::npos);
  CHECK(diagnostics.find("\"code\":\"PSC1005\"") != std::string::npos);
  CHECK(diagnostics.find("\"message\":\"import /std/math is not supported; use import /std/math/* or /std/math/<name>\"") !=
        std::string::npos);
  CHECK(diagnostics.find("\"message\":\"import path refers to private definition: /hidden\"") != std::string::npos);
  CHECK(diagnostics.find("\"message\":\"unknown import path: /missing\"") != std::string::npos);
  CHECK(diagnostics.find("\"label\":\"definition: /hidden\"") != std::string::npos);

  size_t semanticCount = 0;
  size_t scan = 0;
  while ((scan = diagnostics.find("\"code\":\"PSC1005\"", scan)) != std::string::npos) {
    ++semanticCount;
    scan += 16;
  }
  CHECK(semanticCount == 3);

  const size_t firstMessage = diagnostics.find(
      "\"message\":\"import /std/math is not supported; use import /std/math/* or /std/math/<name>\"");
  const size_t secondMessage = diagnostics.find("\"message\":\"import path refers to private definition: /hidden\"");
  const size_t thirdMessage = diagnostics.find("\"message\":\"unknown import path: /missing\"");
  REQUIRE(firstMessage != std::string::npos);
  REQUIRE(secondMessage != std::string::npos);
  REQUIRE(thirdMessage != std::string::npos);
  CHECK(firstMessage < secondMessage);
  CHECK(secondMessage < thirdMessage);
}

TEST_CASE("primevm collect-diagnostics emits stable multi-semantic payload for import errors") {
  const std::string source = R"(
import /std/math
import /hidden
import /missing
[return<int>]
hidden() {
  return(7i32)
}
[return<int>]
main() {
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("primevm_collect_diagnostics_semantic_imports.prime", source);
  const std::string errPath =
      (std::filesystem::temp_directory_path() / "primevm_collect_diagnostics_semantic_imports_err.json").string();

  const std::string cmd = "./primevm " + quoteShellArg(srcPath) +
                          " --emit-diagnostics --collect-diagnostics 2> " + quoteShellArg(errPath);
  CHECK(runCommand(cmd) == 2);

  const std::string diagnostics = readFile(errPath);
  CHECK(diagnostics.find("\"version\":1") != std::string::npos);
  CHECK(diagnostics.find("\"code\":\"PSC1005\"") != std::string::npos);
  CHECK(diagnostics.find("\"message\":\"import /std/math is not supported; use import /std/math/* or /std/math/<name>\"") !=
        std::string::npos);
  CHECK(diagnostics.find("\"message\":\"import path refers to private definition: /hidden\"") != std::string::npos);
  CHECK(diagnostics.find("\"message\":\"unknown import path: /missing\"") != std::string::npos);
  CHECK(diagnostics.find("\"label\":\"definition: /hidden\"") != std::string::npos);

  size_t semanticCount = 0;
  size_t scan = 0;
  while ((scan = diagnostics.find("\"code\":\"PSC1005\"", scan)) != std::string::npos) {
    ++semanticCount;
    scan += 16;
  }
  CHECK(semanticCount == 3);

  const size_t firstMessage = diagnostics.find(
      "\"message\":\"import /std/math is not supported; use import /std/math/* or /std/math/<name>\"");
  const size_t secondMessage = diagnostics.find("\"message\":\"import path refers to private definition: /hidden\"");
  const size_t thirdMessage = diagnostics.find("\"message\":\"unknown import path: /missing\"");
  REQUIRE(firstMessage != std::string::npos);
  REQUIRE(secondMessage != std::string::npos);
  REQUIRE(thirdMessage != std::string::npos);
  CHECK(firstMessage < secondMessage);
  CHECK(secondMessage < thirdMessage);
}

TEST_CASE("primec collect-diagnostics emits stable multi-semantic payload for invalid transforms") {
  const std::string source = R"(
[static return<int>]
one() {
  return(1i32)
}
[effects(io_out) effects(io_out) return<int>]
two() {
  return(2i32)
}
[return<int>]
main() {
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("primec_collect_diagnostics_semantic_invalid_transforms.prime", source);
  const std::string errPath =
      (std::filesystem::temp_directory_path() / "primec_collect_diagnostics_semantic_invalid_transforms_err.json")
          .string();

  const std::string cmd = "./primec " + quoteShellArg(srcPath) +
                          " --emit-diagnostics --collect-diagnostics 2> " + quoteShellArg(errPath);
  CHECK(runCommand(cmd) == 2);

  const std::string diagnostics = readFile(errPath);
  CHECK(diagnostics.find("\"version\":1") != std::string::npos);
  CHECK(diagnostics.find("\"code\":\"PSC1005\"") != std::string::npos);
  CHECK(diagnostics.find("\"message\":\"binding visibility/static transforms are only valid on bindings: /one\"") !=
        std::string::npos);
  CHECK(diagnostics.find("\"message\":\"duplicate effects transform on /two\"") != std::string::npos);
  CHECK(diagnostics.find("\"label\":\"definition: /one\"") != std::string::npos);
  CHECK(diagnostics.find("\"label\":\"definition: /two\"") != std::string::npos);

  size_t semanticCount = 0;
  size_t scan = 0;
  while ((scan = diagnostics.find("\"code\":\"PSC1005\"", scan)) != std::string::npos) {
    ++semanticCount;
    scan += 16;
  }
  CHECK(semanticCount == 2);

  const size_t firstMessage =
      diagnostics.find("\"message\":\"binding visibility/static transforms are only valid on bindings: /one\"");
  const size_t secondMessage = diagnostics.find("\"message\":\"duplicate effects transform on /two\"");
  REQUIRE(firstMessage != std::string::npos);
  REQUIRE(secondMessage != std::string::npos);
  CHECK(firstMessage < secondMessage);
}

TEST_CASE("primevm collect-diagnostics emits stable multi-semantic payload for invalid transforms") {
  const std::string source = R"(
[static return<int>]
one() {
  return(1i32)
}
[effects(io_out) effects(io_out) return<int>]
two() {
  return(2i32)
}
[return<int>]
main() {
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("primevm_collect_diagnostics_semantic_invalid_transforms.prime", source);
  const std::string errPath =
      (std::filesystem::temp_directory_path() / "primevm_collect_diagnostics_semantic_invalid_transforms_err.json")
          .string();

  const std::string cmd = "./primevm " + quoteShellArg(srcPath) +
                          " --emit-diagnostics --collect-diagnostics 2> " + quoteShellArg(errPath);
  CHECK(runCommand(cmd) == 2);

  const std::string diagnostics = readFile(errPath);
  CHECK(diagnostics.find("\"version\":1") != std::string::npos);
  CHECK(diagnostics.find("\"code\":\"PSC1005\"") != std::string::npos);
  CHECK(diagnostics.find("\"message\":\"binding visibility/static transforms are only valid on bindings: /one\"") !=
        std::string::npos);
  CHECK(diagnostics.find("\"message\":\"duplicate effects transform on /two\"") != std::string::npos);
  CHECK(diagnostics.find("\"label\":\"definition: /one\"") != std::string::npos);
  CHECK(diagnostics.find("\"label\":\"definition: /two\"") != std::string::npos);

  size_t semanticCount = 0;
  size_t scan = 0;
  while ((scan = diagnostics.find("\"code\":\"PSC1005\"", scan)) != std::string::npos) {
    ++semanticCount;
    scan += 16;
  }
  CHECK(semanticCount == 2);

  const size_t firstMessage =
      diagnostics.find("\"message\":\"binding visibility/static transforms are only valid on bindings: /one\"");
  const size_t secondMessage = diagnostics.find("\"message\":\"duplicate effects transform on /two\"");
  REQUIRE(firstMessage != std::string::npos);
  REQUIRE(secondMessage != std::string::npos);
  CHECK(firstMessage < secondMessage);
}

TEST_CASE("primec collect-diagnostics emits stable multi-semantic payload for return-kind errors") {
  const std::string source = R"(
[return]
badNoType() {
  return(1i32)
}
[return<array>]
badArray() {
  return(array<i32>(1i32))
}
[return<MissingType>]
badUnknown() {
  return(3i32)
}
[return<int>]
main() {
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("primec_collect_diagnostics_semantic_return_kind.prime", source);
  const std::string errPath =
      (std::filesystem::temp_directory_path() / "primec_collect_diagnostics_semantic_return_kind_err.json").string();

  const std::string cmd = "./primec " + quoteShellArg(srcPath) +
                          " --emit-diagnostics --collect-diagnostics 2> " + quoteShellArg(errPath);
  CHECK(runCommand(cmd) == 2);

  const std::string diagnostics = readFile(errPath);
  CHECK(diagnostics.find("\"version\":1") != std::string::npos);
  CHECK(diagnostics.find("\"code\":\"PSC1005\"") != std::string::npos);
  CHECK(diagnostics.find("\"message\":\"return transform requires a type on /badNoType\"") != std::string::npos);
  CHECK(diagnostics.find("\"message\":\"array return type requires exactly one template argument on /badArray\"") !=
        std::string::npos);
  CHECK(diagnostics.find("\"message\":\"unsupported return type on /badUnknown\"") != std::string::npos);
  CHECK(diagnostics.find("\"label\":\"definition: /badNoType\"") != std::string::npos);
  CHECK(diagnostics.find("\"label\":\"definition: /badArray\"") != std::string::npos);
  CHECK(diagnostics.find("\"label\":\"definition: /badUnknown\"") != std::string::npos);

  size_t semanticCount = 0;
  size_t scan = 0;
  while ((scan = diagnostics.find("\"code\":\"PSC1005\"", scan)) != std::string::npos) {
    ++semanticCount;
    scan += 16;
  }
  CHECK(semanticCount == 3);

  const size_t firstMessage = diagnostics.find("\"message\":\"return transform requires a type on /badNoType\"");
  const size_t secondMessage =
      diagnostics.find("\"message\":\"array return type requires exactly one template argument on /badArray\"");
  const size_t thirdMessage = diagnostics.find("\"message\":\"unsupported return type on /badUnknown\"");
  REQUIRE(firstMessage != std::string::npos);
  REQUIRE(secondMessage != std::string::npos);
  REQUIRE(thirdMessage != std::string::npos);
  CHECK(firstMessage < secondMessage);
  CHECK(secondMessage < thirdMessage);
}

TEST_CASE("primevm collect-diagnostics emits stable multi-semantic payload for return-kind errors") {
  const std::string source = R"(
[return]
badNoType() {
  return(1i32)
}
[return<array>]
badArray() {
  return(array<i32>(1i32))
}
[return<MissingType>]
badUnknown() {
  return(3i32)
}
[return<int>]
main() {
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("primevm_collect_diagnostics_semantic_return_kind.prime", source);
  const std::string errPath =
      (std::filesystem::temp_directory_path() / "primevm_collect_diagnostics_semantic_return_kind_err.json").string();

  const std::string cmd = "./primevm " + quoteShellArg(srcPath) +
                          " --emit-diagnostics --collect-diagnostics 2> " + quoteShellArg(errPath);
  CHECK(runCommand(cmd) == 2);

  const std::string diagnostics = readFile(errPath);
  CHECK(diagnostics.find("\"version\":1") != std::string::npos);
  CHECK(diagnostics.find("\"code\":\"PSC1005\"") != std::string::npos);
  CHECK(diagnostics.find("\"message\":\"return transform requires a type on /badNoType\"") != std::string::npos);
  CHECK(diagnostics.find("\"message\":\"array return type requires exactly one template argument on /badArray\"") !=
        std::string::npos);
  CHECK(diagnostics.find("\"message\":\"unsupported return type on /badUnknown\"") != std::string::npos);
  CHECK(diagnostics.find("\"label\":\"definition: /badNoType\"") != std::string::npos);
  CHECK(diagnostics.find("\"label\":\"definition: /badArray\"") != std::string::npos);
  CHECK(diagnostics.find("\"label\":\"definition: /badUnknown\"") != std::string::npos);

  size_t semanticCount = 0;
  size_t scan = 0;
  while ((scan = diagnostics.find("\"code\":\"PSC1005\"", scan)) != std::string::npos) {
    ++semanticCount;
    scan += 16;
  }
  CHECK(semanticCount == 3);

  const size_t firstMessage = diagnostics.find("\"message\":\"return transform requires a type on /badNoType\"");
  const size_t secondMessage =
      diagnostics.find("\"message\":\"array return type requires exactly one template argument on /badArray\"");
  const size_t thirdMessage = diagnostics.find("\"message\":\"unsupported return type on /badUnknown\"");
  REQUIRE(firstMessage != std::string::npos);
  REQUIRE(secondMessage != std::string::npos);
  REQUIRE(thirdMessage != std::string::npos);
  CHECK(firstMessage < secondMessage);
  CHECK(secondMessage < thirdMessage);
}

TEST_CASE("primec collect-diagnostics emits stable multi-semantic payload for definition pass errors") {
  const std::string source = R"(
[return<int>]
first() {
  return(nope(1i32))
}
[return<int>]
second() {
  return(missing(2i32))
}
[return<int>]
main() {
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("primec_collect_diagnostics_semantic_definitions.prime", source);
  const std::string errPath =
      (std::filesystem::temp_directory_path() / "primec_collect_diagnostics_semantic_definitions_err.json").string();

  const std::string cmd = "./primec " + quoteShellArg(srcPath) +
                          " --emit-diagnostics --collect-diagnostics 2> " + quoteShellArg(errPath);
  CHECK(runCommand(cmd) == 2);

  const std::string diagnostics = readFile(errPath);
  CHECK(diagnostics.find("\"version\":1") != std::string::npos);
  CHECK(diagnostics.find("\"code\":\"PSC1005\"") != std::string::npos);
  CHECK(diagnostics.find("\"message\":\"unknown call target: nope\"") != std::string::npos);
  CHECK(diagnostics.find("\"message\":\"unknown call target: missing\"") != std::string::npos);
  CHECK(diagnostics.find("\"label\":\"definition: /first\"") != std::string::npos);
  CHECK(diagnostics.find("\"label\":\"definition: /second\"") != std::string::npos);

  size_t semanticCount = 0;
  size_t scan = 0;
  while ((scan = diagnostics.find("\"code\":\"PSC1005\"", scan)) != std::string::npos) {
    ++semanticCount;
    scan += 16;
  }
  CHECK(semanticCount == 2);

  const size_t firstMessage = diagnostics.find("\"message\":\"unknown call target: nope\"");
  const size_t secondMessage = diagnostics.find("\"message\":\"unknown call target: missing\"");
  REQUIRE(firstMessage != std::string::npos);
  REQUIRE(secondMessage != std::string::npos);
  CHECK(firstMessage < secondMessage);
}

TEST_CASE("primevm collect-diagnostics emits stable multi-semantic payload for definition pass errors") {
  const std::string source = R"(
[return<int>]
first() {
  return(nope(1i32))
}
[return<int>]
second() {
  return(missing(2i32))
}
[return<int>]
main() {
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("primevm_collect_diagnostics_semantic_definitions.prime", source);
  const std::string errPath =
      (std::filesystem::temp_directory_path() / "primevm_collect_diagnostics_semantic_definitions_err.json").string();

  const std::string cmd = "./primevm " + quoteShellArg(srcPath) +
                          " --emit-diagnostics --collect-diagnostics 2> " + quoteShellArg(errPath);
  CHECK(runCommand(cmd) == 2);

  const std::string diagnostics = readFile(errPath);
  CHECK(diagnostics.find("\"version\":1") != std::string::npos);
  CHECK(diagnostics.find("\"code\":\"PSC1005\"") != std::string::npos);
  CHECK(diagnostics.find("\"message\":\"unknown call target: nope\"") != std::string::npos);
  CHECK(diagnostics.find("\"message\":\"unknown call target: missing\"") != std::string::npos);
  CHECK(diagnostics.find("\"label\":\"definition: /first\"") != std::string::npos);
  CHECK(diagnostics.find("\"label\":\"definition: /second\"") != std::string::npos);

  size_t semanticCount = 0;
  size_t scan = 0;
  while ((scan = diagnostics.find("\"code\":\"PSC1005\"", scan)) != std::string::npos) {
    ++semanticCount;
    scan += 16;
  }
  CHECK(semanticCount == 2);

  const size_t firstMessage = diagnostics.find("\"message\":\"unknown call target: nope\"");
  const size_t secondMessage = diagnostics.find("\"message\":\"unknown call target: missing\"");
  REQUIRE(firstMessage != std::string::npos);
  REQUIRE(secondMessage != std::string::npos);
  CHECK(firstMessage < secondMessage);
}

TEST_CASE("primec collect-diagnostics emits intra-definition multi-semantic payload") {
  const std::string source = R"(
[return<int>]
bad() {
  nope(1i32)
  missing(2i32)
  return(0i32)
}
[return<int>]
main() {
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("primec_collect_diagnostics_semantic_intra_definition.prime", source);
  const std::string errPath =
      (std::filesystem::temp_directory_path() / "primec_collect_diagnostics_semantic_intra_definition_err.json")
          .string();

  const std::string cmd = "./primec " + quoteShellArg(srcPath) +
                          " --emit-diagnostics --collect-diagnostics 2> " + quoteShellArg(errPath);
  CHECK(runCommand(cmd) == 2);

  const std::string diagnostics = readFile(errPath);
  CHECK(diagnostics.find("\"code\":\"PSC1005\"") != std::string::npos);
  CHECK(diagnostics.find("\"message\":\"unknown call target: nope\"") != std::string::npos);
  CHECK(diagnostics.find("\"message\":\"unknown call target: missing\"") != std::string::npos);
  CHECK(diagnostics.find("\"label\":\"definition: /bad\"") != std::string::npos);

  size_t semanticCount = 0;
  size_t scan = 0;
  while ((scan = diagnostics.find("\"code\":\"PSC1005\"", scan)) != std::string::npos) {
    ++semanticCount;
    scan += 16;
  }
  CHECK(semanticCount == 2);

  const size_t firstMessage = diagnostics.find("\"message\":\"unknown call target: nope\"");
  const size_t secondMessage = diagnostics.find("\"message\":\"unknown call target: missing\"");
  REQUIRE(firstMessage != std::string::npos);
  REQUIRE(secondMessage != std::string::npos);
  CHECK(firstMessage < secondMessage);
}

TEST_CASE("primevm collect-diagnostics emits intra-definition multi-semantic payload") {
  const std::string source = R"(
[return<int>]
bad() {
  nope(1i32)
  missing(2i32)
  return(0i32)
}
[return<int>]
main() {
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("primevm_collect_diagnostics_semantic_intra_definition.prime", source);
  const std::string errPath =
      (std::filesystem::temp_directory_path() / "primevm_collect_diagnostics_semantic_intra_definition_err.json")
          .string();

  const std::string cmd = "./primevm " + quoteShellArg(srcPath) +
                          " --emit-diagnostics --collect-diagnostics 2> " + quoteShellArg(errPath);
  CHECK(runCommand(cmd) == 2);

  const std::string diagnostics = readFile(errPath);
  CHECK(diagnostics.find("\"code\":\"PSC1005\"") != std::string::npos);
  CHECK(diagnostics.find("\"message\":\"unknown call target: nope\"") != std::string::npos);
  CHECK(diagnostics.find("\"message\":\"unknown call target: missing\"") != std::string::npos);
  CHECK(diagnostics.find("\"label\":\"definition: /bad\"") != std::string::npos);

  size_t semanticCount = 0;
  size_t scan = 0;
  while ((scan = diagnostics.find("\"code\":\"PSC1005\"", scan)) != std::string::npos) {
    ++semanticCount;
    scan += 16;
  }
  CHECK(semanticCount == 2);

  const size_t firstMessage = diagnostics.find("\"message\":\"unknown call target: nope\"");
  const size_t secondMessage = diagnostics.find("\"message\":\"unknown call target: missing\"");
  REQUIRE(firstMessage != std::string::npos);
  REQUIRE(secondMessage != std::string::npos);
  CHECK(firstMessage < secondMessage);
}

TEST_CASE("primec collect-diagnostics emits intra-definition argument-shape payload") {
  const std::string source = R"(
[return<i32>]
take_two([i32] a, [i32] b) {
  return(a)
}
[return<i32>]
bad() {
  take_two(a=1i32, a=2i32)
  take_two(1i32)
  return(0i32)
}
[return<i32>]
main() {
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("primec_collect_diagnostics_semantic_intra_definition_argshape.prime", source);
  const std::string errPath =
      (std::filesystem::temp_directory_path() /
       "primec_collect_diagnostics_semantic_intra_definition_argshape_err.json")
          .string();

  const std::string cmd = "./primec " + quoteShellArg(srcPath) +
                          " --emit-diagnostics --collect-diagnostics 2> " + quoteShellArg(errPath);
  CHECK(runCommand(cmd) == 2);

  const std::string diagnostics = readFile(errPath);
  CHECK(diagnostics.find("\"code\":\"PSC1005\"") != std::string::npos);
  CHECK(diagnostics.find("\"message\":\"duplicate named argument: a\"") != std::string::npos);
  CHECK(diagnostics.find("\"message\":\"argument count mismatch for /take_two\"") != std::string::npos);
  CHECK(diagnostics.find("\"label\":\"definition: /bad\"") != std::string::npos);

  size_t semanticCount = 0;
  size_t scan = 0;
  while ((scan = diagnostics.find("\"code\":\"PSC1005\"", scan)) != std::string::npos) {
    ++semanticCount;
    scan += 16;
  }
  CHECK(semanticCount == 2);

  const size_t firstMessage = diagnostics.find("\"message\":\"duplicate named argument: a\"");
  const size_t secondMessage = diagnostics.find("\"message\":\"argument count mismatch for /take_two\"");
  REQUIRE(firstMessage != std::string::npos);
  REQUIRE(secondMessage != std::string::npos);
  CHECK(firstMessage < secondMessage);
}

TEST_CASE("primevm collect-diagnostics emits intra-definition argument-shape payload") {
  const std::string source = R"(
[return<i32>]
take_two([i32] a, [i32] b) {
  return(a)
}
[return<i32>]
bad() {
  take_two(a=1i32, a=2i32)
  take_two(1i32)
  return(0i32)
}
[return<i32>]
main() {
  return(0i32)
}
)";
  const std::string srcPath =
      writeTemp("primevm_collect_diagnostics_semantic_intra_definition_argshape.prime", source);
  const std::string errPath =
      (std::filesystem::temp_directory_path() /
       "primevm_collect_diagnostics_semantic_intra_definition_argshape_err.json")
          .string();

  const std::string cmd = "./primevm " + quoteShellArg(srcPath) +
                          " --emit-diagnostics --collect-diagnostics 2> " + quoteShellArg(errPath);
  CHECK(runCommand(cmd) == 2);

  const std::string diagnostics = readFile(errPath);
  CHECK(diagnostics.find("\"code\":\"PSC1005\"") != std::string::npos);
  CHECK(diagnostics.find("\"message\":\"duplicate named argument: a\"") != std::string::npos);
  CHECK(diagnostics.find("\"message\":\"argument count mismatch for /take_two\"") != std::string::npos);
  CHECK(diagnostics.find("\"label\":\"definition: /bad\"") != std::string::npos);

  size_t semanticCount = 0;
  size_t scan = 0;
  while ((scan = diagnostics.find("\"code\":\"PSC1005\"", scan)) != std::string::npos) {
    ++semanticCount;
    scan += 16;
  }
  CHECK(semanticCount == 2);

  const size_t firstMessage = diagnostics.find("\"message\":\"duplicate named argument: a\"");
  const size_t secondMessage = diagnostics.find("\"message\":\"argument count mismatch for /take_two\"");
  REQUIRE(firstMessage != std::string::npos);
  REQUIRE(secondMessage != std::string::npos);
  CHECK(firstMessage < secondMessage);
}

TEST_CASE("primec collect-diagnostics emits intra-definition argument-type payload") {
  const std::string source = R"(
[return<i32>]
expects_bool([bool] cond, [i32] value) {
  return(value)
}
[return<i32>]
bad() {
  expects_bool(1i32, 7i32)
  expects_bool(true, false)
  return(0i32)
}
[return<i32>]
main() {
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("primec_collect_diagnostics_semantic_intra_definition_argtype.prime", source);
  const std::string errPath =
      (std::filesystem::temp_directory_path() /
       "primec_collect_diagnostics_semantic_intra_definition_argtype_err.json")
          .string();

  const std::string cmd = "./primec " + quoteShellArg(srcPath) +
                          " --emit-diagnostics --collect-diagnostics 2> " + quoteShellArg(errPath);
  CHECK(runCommand(cmd) == 2);

  const std::string diagnostics = readFile(errPath);
  CHECK(diagnostics.find("\"code\":\"PSC1005\"") != std::string::npos);
  CHECK(diagnostics.find("\"message\":\"argument type mismatch for /expects_bool parameter cond: expected bool got i32\"") !=
        std::string::npos);
  CHECK(diagnostics.find("\"message\":\"argument type mismatch for /expects_bool parameter value: expected i32 got bool\"") !=
        std::string::npos);
  CHECK(diagnostics.find("\"label\":\"definition: /bad\"") != std::string::npos);

  size_t semanticCount = 0;
  size_t scan = 0;
  while ((scan = diagnostics.find("\"code\":\"PSC1005\"", scan)) != std::string::npos) {
    ++semanticCount;
    scan += 16;
  }
  CHECK(semanticCount == 2);

  const size_t firstMessage =
      diagnostics.find("\"message\":\"argument type mismatch for /expects_bool parameter cond: expected bool got i32\"");
  const size_t secondMessage =
      diagnostics.find("\"message\":\"argument type mismatch for /expects_bool parameter value: expected i32 got bool\"");
  REQUIRE(firstMessage != std::string::npos);
  REQUIRE(secondMessage != std::string::npos);
  CHECK(firstMessage < secondMessage);
}

TEST_CASE("primevm collect-diagnostics emits intra-definition argument-type payload") {
  const std::string source = R"(
[return<i32>]
expects_bool([bool] cond, [i32] value) {
  return(value)
}
[return<i32>]
bad() {
  expects_bool(1i32, 7i32)
  expects_bool(true, false)
  return(0i32)
}
[return<i32>]
main() {
  return(0i32)
}
)";
  const std::string srcPath =
      writeTemp("primevm_collect_diagnostics_semantic_intra_definition_argtype.prime", source);
  const std::string errPath =
      (std::filesystem::temp_directory_path() /
       "primevm_collect_diagnostics_semantic_intra_definition_argtype_err.json")
          .string();

  const std::string cmd = "./primevm " + quoteShellArg(srcPath) +
                          " --emit-diagnostics --collect-diagnostics 2> " + quoteShellArg(errPath);
  CHECK(runCommand(cmd) == 2);

  const std::string diagnostics = readFile(errPath);
  CHECK(diagnostics.find("\"code\":\"PSC1005\"") != std::string::npos);
  CHECK(diagnostics.find("\"message\":\"argument type mismatch for /expects_bool parameter cond: expected bool got i32\"") !=
        std::string::npos);
  CHECK(diagnostics.find("\"message\":\"argument type mismatch for /expects_bool parameter value: expected i32 got bool\"") !=
        std::string::npos);
  CHECK(diagnostics.find("\"label\":\"definition: /bad\"") != std::string::npos);

  size_t semanticCount = 0;
  size_t scan = 0;
  while ((scan = diagnostics.find("\"code\":\"PSC1005\"", scan)) != std::string::npos) {
    ++semanticCount;
    scan += 16;
  }
  CHECK(semanticCount == 2);

  const size_t firstMessage =
      diagnostics.find("\"message\":\"argument type mismatch for /expects_bool parameter cond: expected bool got i32\"");
  const size_t secondMessage =
      diagnostics.find("\"message\":\"argument type mismatch for /expects_bool parameter value: expected i32 got bool\"");
  REQUIRE(firstMessage != std::string::npos);
  REQUIRE(secondMessage != std::string::npos);
  CHECK(firstMessage < secondMessage);
}

TEST_CASE("primec collect-diagnostics emits stable multi-semantic payload for execution pass errors") {
  const std::string source = R"(
[return<int>]
main() {
  return(0i32)
}

run_missing_one()
run_missing_two()
)";
  const std::string srcPath = writeTemp("primec_collect_diagnostics_semantic_executions.prime", source);
  const std::string errPath =
      (std::filesystem::temp_directory_path() / "primec_collect_diagnostics_semantic_executions_err.json").string();

  const std::string cmd = "./primec " + quoteShellArg(srcPath) +
                          " --emit-diagnostics --collect-diagnostics 2> " + quoteShellArg(errPath);
  CHECK(runCommand(cmd) == 2);

  const std::string diagnostics = readFile(errPath);
  CHECK(diagnostics.find("\"version\":1") != std::string::npos);
  CHECK(diagnostics.find("\"code\":\"PSC1005\"") != std::string::npos);
  CHECK(diagnostics.find("\"message\":\"unknown execution target: /run_missing_one\"") != std::string::npos);
  CHECK(diagnostics.find("\"message\":\"unknown execution target: /run_missing_two\"") != std::string::npos);
  CHECK(diagnostics.find("\"label\":\"execution: /run_missing_one\"") != std::string::npos);
  CHECK(diagnostics.find("\"label\":\"execution: /run_missing_two\"") != std::string::npos);

  size_t semanticCount = 0;
  size_t scan = 0;
  while ((scan = diagnostics.find("\"code\":\"PSC1005\"", scan)) != std::string::npos) {
    ++semanticCount;
    scan += 16;
  }
  CHECK(semanticCount == 2);

  const size_t firstMessage = diagnostics.find("\"message\":\"unknown execution target: /run_missing_one\"");
  const size_t secondMessage = diagnostics.find("\"message\":\"unknown execution target: /run_missing_two\"");
  REQUIRE(firstMessage != std::string::npos);
  REQUIRE(secondMessage != std::string::npos);
  CHECK(firstMessage < secondMessage);
}

TEST_CASE("primec collect-diagnostics emits intra-execution multi-semantic payload") {
  const std::string source = R"(
[return<int>]
main() {
  return(0i32)
}

[return<void>]
execute_repeat([i32] a, [i32] b) {
  return()
}

execute_repeat(nope(1i32), missing(2i32))
)";
  const std::string srcPath = writeTemp("primec_collect_diagnostics_semantic_intra_execution.prime", source);
  const std::string errPath =
      (std::filesystem::temp_directory_path() / "primec_collect_diagnostics_semantic_intra_execution_err.json")
          .string();

  const std::string cmd = "./primec " + quoteShellArg(srcPath) +
                          " --emit-diagnostics --collect-diagnostics 2> " + quoteShellArg(errPath);
  CHECK(runCommand(cmd) == 2);

  const std::string diagnostics = readFile(errPath);
  CHECK(diagnostics.find("\"code\":\"PSC1005\"") != std::string::npos);
  CHECK(diagnostics.find("\"message\":\"unknown call target: nope\"") != std::string::npos);
  CHECK(diagnostics.find("\"message\":\"unknown call target: missing\"") != std::string::npos);
  CHECK(diagnostics.find("\"label\":\"execution: /execute_repeat\"") != std::string::npos);

  size_t semanticCount = 0;
  size_t scan = 0;
  while ((scan = diagnostics.find("\"code\":\"PSC1005\"", scan)) != std::string::npos) {
    ++semanticCount;
    scan += 16;
  }
  CHECK(semanticCount == 2);

  const size_t firstMessage = diagnostics.find("\"message\":\"unknown call target: nope\"");
  const size_t secondMessage = diagnostics.find("\"message\":\"unknown call target: missing\"");
  REQUIRE(firstMessage != std::string::npos);
  REQUIRE(secondMessage != std::string::npos);
  CHECK(firstMessage < secondMessage);
}

TEST_CASE("primevm collect-diagnostics emits stable multi-semantic payload for execution pass errors") {
  const std::string source = R"(
[return<int>]
main() {
  return(0i32)
}

run_missing_one()
run_missing_two()
)";
  const std::string srcPath = writeTemp("primevm_collect_diagnostics_semantic_executions.prime", source);
  const std::string errPath =
      (std::filesystem::temp_directory_path() / "primevm_collect_diagnostics_semantic_executions_err.json").string();

  const std::string cmd = "./primevm " + quoteShellArg(srcPath) +
                          " --emit-diagnostics --collect-diagnostics 2> " + quoteShellArg(errPath);
  CHECK(runCommand(cmd) == 2);

  const std::string diagnostics = readFile(errPath);
  CHECK(diagnostics.find("\"version\":1") != std::string::npos);
  CHECK(diagnostics.find("\"code\":\"PSC1005\"") != std::string::npos);
  CHECK(diagnostics.find("\"message\":\"unknown execution target: /run_missing_one\"") != std::string::npos);
  CHECK(diagnostics.find("\"message\":\"unknown execution target: /run_missing_two\"") != std::string::npos);
  CHECK(diagnostics.find("\"label\":\"execution: /run_missing_one\"") != std::string::npos);
  CHECK(diagnostics.find("\"label\":\"execution: /run_missing_two\"") != std::string::npos);

  size_t semanticCount = 0;
  size_t scan = 0;
  while ((scan = diagnostics.find("\"code\":\"PSC1005\"", scan)) != std::string::npos) {
    ++semanticCount;
    scan += 16;
  }
  CHECK(semanticCount == 2);

  const size_t firstMessage = diagnostics.find("\"message\":\"unknown execution target: /run_missing_one\"");
  const size_t secondMessage = diagnostics.find("\"message\":\"unknown execution target: /run_missing_two\"");
  REQUIRE(firstMessage != std::string::npos);
  REQUIRE(secondMessage != std::string::npos);
  CHECK(firstMessage < secondMessage);
}

TEST_CASE("primevm collect-diagnostics emits intra-execution multi-semantic payload") {
  const std::string source = R"(
[return<int>]
main() {
  return(0i32)
}

[return<void>]
execute_repeat([i32] a, [i32] b) {
  return()
}

execute_repeat(nope(1i32), missing(2i32))
)";
  const std::string srcPath = writeTemp("primevm_collect_diagnostics_semantic_intra_execution.prime", source);
  const std::string errPath =
      (std::filesystem::temp_directory_path() / "primevm_collect_diagnostics_semantic_intra_execution_err.json")
          .string();

  const std::string cmd = "./primevm " + quoteShellArg(srcPath) +
                          " --emit-diagnostics --collect-diagnostics 2> " + quoteShellArg(errPath);
  CHECK(runCommand(cmd) == 2);

  const std::string diagnostics = readFile(errPath);
  CHECK(diagnostics.find("\"code\":\"PSC1005\"") != std::string::npos);
  CHECK(diagnostics.find("\"message\":\"unknown call target: nope\"") != std::string::npos);
  CHECK(diagnostics.find("\"message\":\"unknown call target: missing\"") != std::string::npos);
  CHECK(diagnostics.find("\"label\":\"execution: /execute_repeat\"") != std::string::npos);

  size_t semanticCount = 0;
  size_t scan = 0;
  while ((scan = diagnostics.find("\"code\":\"PSC1005\"", scan)) != std::string::npos) {
    ++semanticCount;
    scan += 16;
  }
  CHECK(semanticCount == 2);

  const size_t firstMessage = diagnostics.find("\"message\":\"unknown call target: nope\"");
  const size_t secondMessage = diagnostics.find("\"message\":\"unknown call target: missing\"");
  REQUIRE(firstMessage != std::string::npos);
  REQUIRE(secondMessage != std::string::npos);
  CHECK(firstMessage < secondMessage);
}

TEST_CASE("primec collect-diagnostics emits intra-execution argument-shape payload") {
  const std::string source = R"(
[return<i32>]
main() {
  return(0i32)
}

[return<i32>]
take_two([i32] a, [i32] b) {
  return(a)
}

[return<void>]
execute_repeat([i32] a, [i32] b) {
  return()
}

execute_repeat(take_two(a=1i32, a=2i32), take_two(1i32))
)";
  const std::string srcPath = writeTemp("primec_collect_diagnostics_semantic_intra_execution_argshape.prime", source);
  const std::string errPath =
      (std::filesystem::temp_directory_path() /
       "primec_collect_diagnostics_semantic_intra_execution_argshape_err.json")
          .string();

  const std::string cmd = "./primec " + quoteShellArg(srcPath) +
                          " --emit-diagnostics --collect-diagnostics 2> " + quoteShellArg(errPath);
  CHECK(runCommand(cmd) == 2);

  const std::string diagnostics = readFile(errPath);
  CHECK(diagnostics.find("\"code\":\"PSC1005\"") != std::string::npos);
  CHECK(diagnostics.find("\"message\":\"duplicate named argument: a\"") != std::string::npos);
  CHECK(diagnostics.find("\"message\":\"argument count mismatch for /take_two\"") != std::string::npos);
  CHECK(diagnostics.find("\"label\":\"execution: /execute_repeat\"") != std::string::npos);

  size_t semanticCount = 0;
  size_t scan = 0;
  while ((scan = diagnostics.find("\"code\":\"PSC1005\"", scan)) != std::string::npos) {
    ++semanticCount;
    scan += 16;
  }
  CHECK(semanticCount == 2);

  const size_t firstMessage = diagnostics.find("\"message\":\"duplicate named argument: a\"");
  const size_t secondMessage = diagnostics.find("\"message\":\"argument count mismatch for /take_two\"");
  REQUIRE(firstMessage != std::string::npos);
  REQUIRE(secondMessage != std::string::npos);
  CHECK(firstMessage < secondMessage);
}

TEST_CASE("primevm collect-diagnostics emits intra-execution argument-shape payload") {
  const std::string source = R"(
[return<i32>]
main() {
  return(0i32)
}

[return<i32>]
take_two([i32] a, [i32] b) {
  return(a)
}

[return<void>]
execute_repeat([i32] a, [i32] b) {
  return()
}

execute_repeat(take_two(a=1i32, a=2i32), take_two(1i32))
)";
  const std::string srcPath =
      writeTemp("primevm_collect_diagnostics_semantic_intra_execution_argshape.prime", source);
  const std::string errPath =
      (std::filesystem::temp_directory_path() /
       "primevm_collect_diagnostics_semantic_intra_execution_argshape_err.json")
          .string();

  const std::string cmd = "./primevm " + quoteShellArg(srcPath) +
                          " --emit-diagnostics --collect-diagnostics 2> " + quoteShellArg(errPath);
  CHECK(runCommand(cmd) == 2);

  const std::string diagnostics = readFile(errPath);
  CHECK(diagnostics.find("\"code\":\"PSC1005\"") != std::string::npos);
  CHECK(diagnostics.find("\"message\":\"duplicate named argument: a\"") != std::string::npos);
  CHECK(diagnostics.find("\"message\":\"argument count mismatch for /take_two\"") != std::string::npos);
  CHECK(diagnostics.find("\"label\":\"execution: /execute_repeat\"") != std::string::npos);

  size_t semanticCount = 0;
  size_t scan = 0;
  while ((scan = diagnostics.find("\"code\":\"PSC1005\"", scan)) != std::string::npos) {
    ++semanticCount;
    scan += 16;
  }
  CHECK(semanticCount == 2);

  const size_t firstMessage = diagnostics.find("\"message\":\"duplicate named argument: a\"");
  const size_t secondMessage = diagnostics.find("\"message\":\"argument count mismatch for /take_two\"");
  REQUIRE(firstMessage != std::string::npos);
  REQUIRE(secondMessage != std::string::npos);
  CHECK(firstMessage < secondMessage);
}

TEST_CASE("primec collect-diagnostics emits intra-execution argument-type payload") {
  const std::string source = R"(
[return<i32>]
main() {
  return(0i32)
}

[return<i32>]
expects_bool([bool] cond, [i32] value) {
  return(value)
}

[return<void>]
execute_repeat([i32] a, [i32] b) {
  return()
}

execute_repeat(expects_bool(1i32, 7i32), expects_bool(true, false))
)";
  const std::string srcPath = writeTemp("primec_collect_diagnostics_semantic_intra_execution_argtype.prime", source);
  const std::string errPath =
      (std::filesystem::temp_directory_path() /
       "primec_collect_diagnostics_semantic_intra_execution_argtype_err.json")
          .string();

  const std::string cmd = "./primec " + quoteShellArg(srcPath) +
                          " --emit-diagnostics --collect-diagnostics 2> " + quoteShellArg(errPath);
  CHECK(runCommand(cmd) == 2);

  const std::string diagnostics = readFile(errPath);
  CHECK(diagnostics.find("\"code\":\"PSC1005\"") != std::string::npos);
  CHECK(diagnostics.find("\"message\":\"argument type mismatch for /expects_bool parameter cond: expected bool got i32\"") !=
        std::string::npos);
  CHECK(diagnostics.find("\"message\":\"argument type mismatch for /expects_bool parameter value: expected i32 got bool\"") !=
        std::string::npos);
  CHECK(diagnostics.find("\"label\":\"execution: /execute_repeat\"") != std::string::npos);

  size_t semanticCount = 0;
  size_t scan = 0;
  while ((scan = diagnostics.find("\"code\":\"PSC1005\"", scan)) != std::string::npos) {
    ++semanticCount;
    scan += 16;
  }
  CHECK(semanticCount == 2);

  const size_t firstMessage =
      diagnostics.find("\"message\":\"argument type mismatch for /expects_bool parameter cond: expected bool got i32\"");
  const size_t secondMessage =
      diagnostics.find("\"message\":\"argument type mismatch for /expects_bool parameter value: expected i32 got bool\"");
  REQUIRE(firstMessage != std::string::npos);
  REQUIRE(secondMessage != std::string::npos);
  CHECK(firstMessage < secondMessage);
}

TEST_CASE("primevm collect-diagnostics emits intra-execution argument-type payload") {
  const std::string source = R"(
[return<i32>]
main() {
  return(0i32)
}

[return<i32>]
expects_bool([bool] cond, [i32] value) {
  return(value)
}

[return<void>]
execute_repeat([i32] a, [i32] b) {
  return()
}

execute_repeat(expects_bool(1i32, 7i32), expects_bool(true, false))
)";
  const std::string srcPath =
      writeTemp("primevm_collect_diagnostics_semantic_intra_execution_argtype.prime", source);
  const std::string errPath =
      (std::filesystem::temp_directory_path() /
       "primevm_collect_diagnostics_semantic_intra_execution_argtype_err.json")
          .string();

  const std::string cmd = "./primevm " + quoteShellArg(srcPath) +
                          " --emit-diagnostics --collect-diagnostics 2> " + quoteShellArg(errPath);
  CHECK(runCommand(cmd) == 2);

  const std::string diagnostics = readFile(errPath);
  CHECK(diagnostics.find("\"code\":\"PSC1005\"") != std::string::npos);
  CHECK(diagnostics.find("\"message\":\"argument type mismatch for /expects_bool parameter cond: expected bool got i32\"") !=
        std::string::npos);
  CHECK(diagnostics.find("\"message\":\"argument type mismatch for /expects_bool parameter value: expected i32 got bool\"") !=
        std::string::npos);
  CHECK(diagnostics.find("\"label\":\"execution: /execute_repeat\"") != std::string::npos);

  size_t semanticCount = 0;
  size_t scan = 0;
  while ((scan = diagnostics.find("\"code\":\"PSC1005\"", scan)) != std::string::npos) {
    ++semanticCount;
    scan += 16;
  }
  CHECK(semanticCount == 2);

  const size_t firstMessage =
      diagnostics.find("\"message\":\"argument type mismatch for /expects_bool parameter cond: expected bool got i32\"");
  const size_t secondMessage =
      diagnostics.find("\"message\":\"argument type mismatch for /expects_bool parameter value: expected i32 got bool\"");
  REQUIRE(firstMessage != std::string::npos);
  REQUIRE(secondMessage != std::string::npos);
  CHECK(firstMessage < secondMessage);
}

TEST_CASE("primevm emit-diagnostics reports structured semantic payload") {
  const std::string source = R"(
[return<int>]
main() {
  return(nope(1i32))
}
)";
  const std::string srcPath = writeTemp("primevm_emit_diagnostics_semantic.prime", source);
  const std::string errPath =
      (std::filesystem::temp_directory_path() / "primevm_emit_diagnostics_semantic_err.json").string();

  const std::string cmd =
      "./primevm " + quoteShellArg(srcPath) + " --emit-diagnostics 2> " + quoteShellArg(errPath);
  CHECK(runCommand(cmd) == 2);

  const std::string diagnostics = readFile(errPath);
  CHECK(diagnostics.find("\"version\":1") != std::string::npos);
  CHECK(diagnostics.find("\"code\":\"PSC1005\"") != std::string::npos);
  CHECK(diagnostics.find("\"message\":\"unknown call target: nope\"") != std::string::npos);
  CHECK(diagnostics.find("\"line\":0") == std::string::npos);
  CHECK(diagnostics.find("\"column\":0") == std::string::npos);
  CHECK(diagnostics.find("\"label\":\"definition: /main\"") != std::string::npos);
  CHECK(diagnostics.find("\"notes\":[\"stage: semantic\"]") != std::string::npos);
}

TEST_CASE("primec emit-diagnostics reports structured lowering payload") {
  const std::string source = R"(
[return<bool>]
main() {
  return(equal("alpha"utf8, "alpha"utf8))
}
)";
  const std::string srcPath = writeTemp("primec_emit_diagnostics_lowering.prime", source);
  const std::string errPath =
      (std::filesystem::temp_directory_path() / "primec_emit_diagnostics_lowering_err.json").string();

  const std::string cmd =
      "./primec --emit=vm " + quoteShellArg(srcPath) + " --entry /main --emit-diagnostics 2> " + quoteShellArg(errPath);
  CHECK(runCommand(cmd) == 2);

  const std::string diagnostics = readFile(errPath);
  CHECK(diagnostics.find("\"version\":1") != std::string::npos);
  CHECK(diagnostics.find("\"code\":\"PSC2001\"") != std::string::npos);
  CHECK(diagnostics.find("vm backend does not support string comparisons") != std::string::npos);
  CHECK(diagnostics.find("\"notes\":[\"backend: vm\"]") != std::string::npos);
}

TEST_CASE("emit-diagnostics reports argument payload for removed text-filters option") {
  const std::string source = R"(
[return<int>]
main() {
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("emit_diagnostics_text_filters_removed.prime", source);
  const std::string primecErrPath =
      (std::filesystem::temp_directory_path() / "primec_emit_diagnostics_text_filters_removed_err.json").string();
  const std::string primecBareErrPath =
      (std::filesystem::temp_directory_path() / "primec_emit_diagnostics_text_filters_removed_bare_err.json")
          .string();
  const std::string primevmErrPath =
      (std::filesystem::temp_directory_path() / "primevm_emit_diagnostics_text_filters_removed_err.json").string();
  const std::string primevmBareErrPath =
      (std::filesystem::temp_directory_path() / "primevm_emit_diagnostics_text_filters_removed_bare_err.json")
          .string();

  const std::string primecCmd = "./primec " + quoteShellArg(srcPath) +
                                " --emit-diagnostics --text-filters=none 2> " + quoteShellArg(primecErrPath);
  CHECK(runCommand(primecCmd) == 2);
  const std::string primecDiagnostics = readFile(primecErrPath);
  CHECK(primecDiagnostics.find("\"code\":\"PSC0001\"") != std::string::npos);
  CHECK(primecDiagnostics.find("\"message\":\"unknown option: --text-filters=none\"") != std::string::npos);
  CHECK(primecDiagnostics.find("\"severity\":\"error\"") != std::string::npos);
  CHECK(primecDiagnostics.find("Usage: primec") == std::string::npos);

  const std::string primecBareCmd = "./primec " + quoteShellArg(srcPath) +
                                    " --emit-diagnostics --text-filters 2> " + quoteShellArg(primecBareErrPath);
  CHECK(runCommand(primecBareCmd) == 2);
  const std::string primecBareDiagnostics = readFile(primecBareErrPath);
  CHECK(primecBareDiagnostics.find("\"code\":\"PSC0001\"") != std::string::npos);
  CHECK(primecBareDiagnostics.find("\"message\":\"unknown option: --text-filters\"") != std::string::npos);
  CHECK(primecBareDiagnostics.find("\"severity\":\"error\"") != std::string::npos);
  CHECK(primecBareDiagnostics.find("Usage: primec") == std::string::npos);

  const std::string primevmCmd = "./primevm " + quoteShellArg(srcPath) +
                                 " --emit-diagnostics --text-filters=none 2> " + quoteShellArg(primevmErrPath);
  CHECK(runCommand(primevmCmd) == 2);
  const std::string primevmDiagnostics = readFile(primevmErrPath);
  CHECK(primevmDiagnostics.find("\"code\":\"PSC0001\"") != std::string::npos);
  CHECK(primevmDiagnostics.find("\"message\":\"unknown option: --text-filters=none\"") != std::string::npos);
  CHECK(primevmDiagnostics.find("\"severity\":\"error\"") != std::string::npos);
  CHECK(primevmDiagnostics.find("Usage: primevm") == std::string::npos);

  const std::string primevmBareCmd = "./primevm " + quoteShellArg(srcPath) +
                                     " --emit-diagnostics --text-filters 2> " + quoteShellArg(primevmBareErrPath);
  CHECK(runCommand(primevmBareCmd) == 2);
  const std::string primevmBareDiagnostics = readFile(primevmBareErrPath);
  CHECK(primevmBareDiagnostics.find("\"code\":\"PSC0001\"") != std::string::npos);
  CHECK(primevmBareDiagnostics.find("\"message\":\"unknown option: --text-filters\"") != std::string::npos);
  CHECK(primevmBareDiagnostics.find("\"severity\":\"error\"") != std::string::npos);
  CHECK(primevmBareDiagnostics.find("Usage: primevm") == std::string::npos);
}

TEST_CASE("primec list transforms prints metadata") {
  const std::string outPath =
      (std::filesystem::temp_directory_path() / "primec_list_transforms.txt").string();
  const std::string listCmd = "./primec --list-transforms > " + quoteShellArg(outPath);

  CHECK(runCommand(listCmd) == 0);
  const std::string output = readFile(outPath);
  CHECK(output.find("name\tphase\taliases\tavailability\n") != std::string::npos);
  CHECK(output.find("append_operators\ttext\t-\tprimec,primevm\n") != std::string::npos);
  CHECK(output.find("on_error\tsemantic\t-\tprimec,primevm\n") != std::string::npos);
  CHECK(output.find("single_type_to_return\tsemantic\t-\tprimec,primevm\n") != std::string::npos);
}

TEST_CASE("primec and primevm list transforms match") {
  const std::string primecOut =
      (std::filesystem::temp_directory_path() / "primec_list_transforms_parity.txt").string();
  const std::string primevmOut =
      (std::filesystem::temp_directory_path() / "primevm_list_transforms_parity.txt").string();

  const std::string primecCmd = "./primec --list-transforms > " + quoteShellArg(primecOut);
  const std::string primevmCmd = "./primevm --list-transforms > " + quoteShellArg(primevmOut);
  CHECK(runCommand(primecCmd) == 0);
  CHECK(runCommand(primevmCmd) == 0);
  CHECK(readFile(primecOut) == readFile(primevmOut));
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
      "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main --text-transforms=default,implicit-i32";
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

TEST_CASE("implicit suffix enabled by default") {
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
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 8);
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
