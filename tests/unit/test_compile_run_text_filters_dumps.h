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
      (testScratchPath("") / "primec_dump_pre_ast.txt").string();

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
      (testScratchPath("") / "primec_dump_ir.txt").string();

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
      (testScratchPath("") / "primec_dump_ast_nope.txt").string();

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
      (testScratchPath("") / "primec_dump_ast_semantic.txt").string();

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
      (testScratchPath("") / "primec_dump_ast_semantic_hyphen.txt").string();
  const std::string underscoreOut =
      (testScratchPath("") / "primec_dump_ast_semantic_underscore.txt").string();

  const std::string hyphenCmd =
      "./primec " + quoteShellArg(srcPath) + " --dump-stage ast-semantic > " + quoteShellArg(hyphenOut);
  const std::string underscoreCmd =
      "./primec " + quoteShellArg(srcPath) + " --dump-stage ast_semantic > " + quoteShellArg(underscoreOut);
  CHECK(runCommand(hyphenCmd) == 0);
  CHECK(runCommand(underscoreCmd) == 0);
  CHECK(readFile(hyphenOut) == readFile(underscoreOut));
}

TEST_CASE("dump type_graph alias works and prints graph output") {
  const std::string source = R"(
[return<auto>]
leaf() {
  return(1i32)
}

[return<auto>]
main() {
  return(leaf())
}
)";
  const std::string srcPath = writeTemp("compile_dump_type_graph_alias.prime", source);
  const std::string hyphenOut =
      (testScratchPath("") / "primec_dump_type_graph_hyphen.txt").string();
  const std::string underscoreOut =
      (testScratchPath("") / "primec_dump_type_graph_underscore.txt").string();

  const std::string hyphenCmd =
      "./primec " + quoteShellArg(srcPath) + " --dump-stage type-graph > " + quoteShellArg(hyphenOut);
  const std::string underscoreCmd =
      "./primec " + quoteShellArg(srcPath) + " --dump-stage type_graph > " + quoteShellArg(underscoreOut);
  CHECK(runCommand(hyphenCmd) == 0);
  CHECK(runCommand(underscoreCmd) == 0);

  const std::string dump = readFile(hyphenOut);
  CHECK(dump == readFile(underscoreOut));
  CHECK(dump.find("type_graph {") != std::string::npos);
  CHECK(dump.find("kind=definition_return label=\"/leaf\"") != std::string::npos);
  CHECK(dump.find("kind=call_constraint label=\"/main::call#0\"") != std::string::npos);
  CHECK(dump.find("path=\"/leaf\"") != std::string::npos);
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
      (testScratchPath("") / "primec_dump_ast_semantic_nope_err.txt").string();

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
      (testScratchPath("") / "primec_dump_stage_unknown_err.txt").string();

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
      (testScratchPath("") / "primec_dump_shared_pre_ast.txt").string();
  const std::string primevmOut =
      (testScratchPath("") / "primevm_dump_shared_pre_ast.txt").string();

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
      (testScratchPath("") / "primec_dump_shared_ast_semantic.txt").string();
  const std::string primevmOut =
      (testScratchPath("") / "primevm_dump_shared_ast_semantic.txt").string();

  const std::string primecCmd =
      "./primec " + quoteShellArg(srcPath) + " --dump-stage ast-semantic > " + quoteShellArg(primecOut);
  const std::string primevmCmd =
      "./primevm " + quoteShellArg(srcPath) + " --dump-stage ast-semantic > " + quoteShellArg(primevmOut);
  CHECK(runCommand(primecCmd) == 0);
  CHECK(runCommand(primevmCmd) == 0);
  CHECK(readFile(primecOut) == readFile(primevmOut));
}

TEST_CASE("primec and primevm dump type-graph match") {
  const std::string source = R"(
[return<auto>]
leaf() {
  return(1i32)
}

[return<auto>]
main() {
  return(leaf())
}
)";
  const std::string srcPath = writeTemp("compile_dump_shared_type_graph.prime", source);
  const std::string primecOut =
      (testScratchPath("") / "primec_dump_shared_type_graph.txt").string();
  const std::string primevmOut =
      (testScratchPath("") / "primevm_dump_shared_type_graph.txt").string();

  const std::string primecCmd =
      "./primec " + quoteShellArg(srcPath) + " --dump-stage type-graph > " + quoteShellArg(primecOut);
  const std::string primevmCmd =
      "./primevm " + quoteShellArg(srcPath) + " --dump-stage type-graph > " + quoteShellArg(primevmOut);
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
      (testScratchPath("") / "primevm_dump_stage_unknown_err.txt").string();

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
      (testScratchPath("") / "primec_plain_parse_diagnostic_err.txt").string();

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
      (testScratchPath("") / "primevm_plain_semantic_diagnostic_err.txt").string();

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
      (testScratchPath("") / "primec_emit_diagnostics_parse_err.json").string();

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

