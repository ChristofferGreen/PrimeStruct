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

TEST_CASE("dump ast-semantic shows experimental map destroy cleanup") {
  const std::string source = R"(
import /std/collections/experimental_map/*

[return<int>]
main() {
  [Map<string, i32> mut] values{mapSingle<string, i32>("left"raw_utf8, 4i32)}
  return(mapCount<string, i32>(values))
}
)";
  const std::string srcPath = writeTemp("compile_dump_ast_semantic_experimental_map_destroy.prime", source);
  const std::string outPath =
      (testScratchPath("") / "primec_dump_ast_semantic_experimental_map_destroy.txt").string();

  const std::string dumpCmd =
      "./primec " + quoteShellArg(srcPath) + " --dump-stage ast-semantic > " + quoteShellArg(outPath);
  CHECK(runCommand(dumpCmd) == 0);
  const std::string ast = readFile(outPath);
  const size_t mapDestroyPos =
      ast.find("[public, effects(heap_alloc)] /std/collections/experimental_map/Map__");
  CHECK(mapDestroyPos != std::string::npos);
  const size_t keysDestroyPos = ast.find("mapDestroyVector__", mapDestroyPos);
  CHECK(keysDestroyPos != std::string::npos);
  CHECK(ast.find("this.keys", keysDestroyPos) != std::string::npos);
  const size_t payloadsDestroyPos = ast.find("mapDestroyVector__", keysDestroyPos + 1);
  CHECK(payloadsDestroyPos != std::string::npos);
  CHECK(ast.find("this.payloads", payloadsDestroyPos) != std::string::npos);
}

TEST_CASE("dump ast-semantic shows experimental soa_vector wrapper count runtime") {
  const std::string source = R"(
import /std/collections/experimental_soa_vector/*

[struct reflect]
Particle() {
  [i32] x{1i32}
}

[effects(heap_alloc), return<int>]
main() {
  [SoaVector<Particle>] values{soaVectorNew<Particle>()}
  return(values.count())
}
)";
  const std::string srcPath = writeTemp("compile_dump_ast_semantic_experimental_soa_vector_count.prime", source);
  const std::string outPath =
      (testScratchPath("") / "primec_dump_ast_semantic_experimental_soa_vector_count.txt").string();

  const std::string dumpCmd =
      "./primec " + quoteShellArg(srcPath) + " --dump-stage ast-semantic > " + quoteShellArg(outPath);
  CHECK(runCommand(dumpCmd) == 0);
  const std::string ast = readFile(outPath);
  const size_t countPos =
      ast.find("[public, return<i32>] /std/collections/experimental_soa_vector/SoaVector__");
  CHECK(countPos != std::string::npos);
  CHECK(ast.find("/count()", countPos) != std::string::npos);
  CHECK(ast.find("/std/collections/experimental_soa_storage/soaColumnCount", countPos) !=
        std::string::npos);
  CHECK(ast.find("this.storage", countPos) != std::string::npos);
  CHECK(ast.find("countValue", countPos) == std::string::npos);
  CHECK(ast.find("/soa_vector/count(this.storage)", countPos) == std::string::npos);
}

TEST_CASE("dump ast-semantic keeps canonical soa_vector get helper path") {
  const std::string source = R"(
import /std/collections/experimental_soa_vector/*

[struct reflect]
Particle() {
  [i32] x{1i32}
}

[effects(heap_alloc), return<int>]
main() {
  [SoaVector<Particle>] values{soaVectorSingle<Particle>(Particle(7i32))}
  return(/std/collections/soa_vector/get<Particle>(values, 0i32).x)
}
)";
  const std::string srcPath = writeTemp("compile_dump_ast_semantic_canonical_soa_vector_get.prime", source);
  const std::string outPath =
      (testScratchPath("") / "primec_dump_ast_semantic_canonical_soa_vector_get.txt").string();

  const std::string dumpCmd =
      "./primec " + quoteShellArg(srcPath) + " --dump-stage ast-semantic > " + quoteShellArg(outPath);
  CHECK(runCommand(dumpCmd) == 0);
  const std::string ast = readFile(outPath);
  const size_t mainPos = ast.find("/main()");
  CHECK(mainPos != std::string::npos);
  CHECK(ast.find("/std/collections/soa_vector/get__", mainPos) != std::string::npos);
  CHECK(ast.find("return /std/collections/soa_vector/get__", mainPos) != std::string::npos);
}

TEST_CASE("dump ast-semantic rewrites bare soa_vector get helper on helper return") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_soa_vector/*

[struct reflect]
Particle() {
  [i32] x{1i32}
}

[return<SoaVector<Particle>>]
cloneValues() {
  return(soaVectorSingle<Particle>(Particle(7i32)))
}

[effects(heap_alloc), return<int>]
main() {
  return(get(cloneValues(), 0i32).x)
}
)";
  const std::string srcPath =
      writeTemp("compile_dump_ast_semantic_experimental_soa_vector_get_helper_return.prime", source);
  const std::string outPath =
      (testScratchPath("") / "primec_dump_ast_semantic_experimental_soa_vector_get_helper_return.txt")
          .string();

  const std::string dumpCmd =
      "./primec " + quoteShellArg(srcPath) + " --dump-stage ast-semantic > " + quoteShellArg(outPath);
  CHECK(runCommand(dumpCmd) == 0);
  const std::string ast = readFile(outPath);
  const size_t mainPos = ast.find("/main()");
  CHECK(mainPos != std::string::npos);
  CHECK(ast.find("/std/collections/experimental_soa_vector/soaVectorGet__", mainPos) !=
        std::string::npos);
  CHECK(ast.find("return /std/collections/experimental_soa_vector/soaVectorGet__", mainPos) !=
        std::string::npos);
  CHECK(ast.find("return get(", mainPos) == std::string::npos);
}

TEST_CASE("dump ast-semantic rewrites experimental soa_vector reflected field index syntax") {
  const std::string source = R"(
import /std/collections/experimental_soa_vector/*

[struct reflect]
Particle() {
  [i32] x{1i32}
  [i32] y{2i32}
}

[effects(heap_alloc), return<int>]
main() {
  [SoaVector<Particle> mut] values{soaVectorNew<Particle>()}
  values.push(Particle(4i32, 6i32))
  values.push(Particle(9i32, 12i32))
  return(values.y()[1i32])
}
)";
  const std::string srcPath =
      writeTemp("compile_dump_ast_semantic_experimental_soa_vector_field_view.prime", source);
  const std::string outPath =
      (testScratchPath("") / "primec_dump_ast_semantic_experimental_soa_vector_field_view.txt")
          .string();

  const std::string dumpCmd =
      "./primec " + quoteShellArg(srcPath) + " --dump-stage ast-semantic > " + quoteShellArg(outPath);
  CHECK(runCommand(dumpCmd) == 0);
  const std::string ast = readFile(outPath);
  const size_t mainPos = ast.find("/main()");
  CHECK(mainPos != std::string::npos);
  CHECK(ast.find("/std/collections/experimental_soa_vector/soaVectorGet__", mainPos) !=
        std::string::npos);
  CHECK(ast.find(".y", mainPos) != std::string::npos);
  CHECK(ast.find("values.y()[", mainPos) == std::string::npos);
}

TEST_CASE("dump ast-semantic rewrites borrowed experimental soa_vector reflected field index syntax") {
  const std::string source = R"(
import /std/collections/experimental_soa_vector/*

[struct reflect]
Particle() {
  [i32] x{1i32}
  [i32] y{2i32}
}

[effects(heap_alloc), return<int>]
main() {
  [SoaVector<Particle> mut] values{soaVectorNew<Particle>()}
  values.push(Particle(4i32, 6i32))
  values.push(Particle(9i32, 12i32))
  [Reference<SoaVector<Particle>>] borrowed{location(values)}
  return(dereference(borrowed).y()[1i32])
}
)";
  const std::string srcPath =
      writeTemp("compile_dump_ast_semantic_borrowed_experimental_soa_vector_field_view.prime", source);
  const std::string outPath =
      (testScratchPath("") / "primec_dump_ast_semantic_borrowed_experimental_soa_vector_field_view.txt")
          .string();

  const std::string dumpCmd =
      "./primec " + quoteShellArg(srcPath) + " --dump-stage ast-semantic > " + quoteShellArg(outPath);
  CHECK(runCommand(dumpCmd) == 0);
  const std::string ast = readFile(outPath);
  const size_t mainPos = ast.find("/main()");
  CHECK(mainPos != std::string::npos);
  CHECK(ast.find("/std/collections/experimental_soa_vector/soaVectorGet__", mainPos) !=
        std::string::npos);
  CHECK(ast.find(".y", mainPos) != std::string::npos);
  CHECK(ast.find("dereference(borrowed).y()[", mainPos) == std::string::npos);
}

TEST_CASE("dump ast-semantic rewrites borrowed local experimental soa_vector reflected field index syntax") {
  const std::string source = R"(
import /std/collections/experimental_soa_vector/*

[struct reflect]
Particle() {
  [i32] x{1i32}
  [i32] y{2i32}
}

[effects(heap_alloc), return<int>]
main() {
  [SoaVector<Particle> mut] values{soaVectorNew<Particle>()}
  values.push(Particle(4i32, 6i32))
  values.push(Particle(9i32, 12i32))
  [Reference<SoaVector<Particle>>] borrowed{location(values)}
  return(borrowed.y()[1i32])
}
)";
  const std::string srcPath =
      writeTemp("compile_dump_ast_semantic_borrowed_local_experimental_soa_vector_field_view.prime",
                source);
  const std::string outPath =
      (testScratchPath("") /
       "primec_dump_ast_semantic_borrowed_local_experimental_soa_vector_field_view.txt")
          .string();

  const std::string dumpCmd =
      "./primec " + quoteShellArg(srcPath) + " --dump-stage ast-semantic > " + quoteShellArg(outPath);
  CHECK(runCommand(dumpCmd) == 0);
  const std::string ast = readFile(outPath);
  const size_t mainPos = ast.find("/main()");
  CHECK(mainPos != std::string::npos);
  CHECK(ast.find("/std/collections/experimental_soa_vector/soaVectorGet__", mainPos) !=
        std::string::npos);
  CHECK(ast.find(".y", mainPos) != std::string::npos);
  CHECK(ast.find("borrowed.y()[", mainPos) == std::string::npos);
}

TEST_CASE("dump ast-semantic rewrites borrowed helper-return experimental soa_vector reflected field index syntax") {
  const std::string source = R"(
import /std/collections/experimental_soa_vector/*

[struct reflect]
Particle() {
  [i32] x{1i32}
  [i32] y{2i32}
}

[return<Reference<SoaVector<Particle>>>]
pickBorrowed([Reference<SoaVector<Particle>>] values) {
  return(values)
}

[effects(heap_alloc), return<int>]
main() {
  [SoaVector<Particle> mut] values{soaVectorNew<Particle>()}
  values.push(Particle(4i32, 6i32))
  values.push(Particle(9i32, 12i32))
  return(pickBorrowed(location(values)).y()[1i32])
}
)";
  const std::string srcPath =
      writeTemp("compile_dump_ast_semantic_borrowed_return_experimental_soa_vector_field_view.prime",
                source);
  const std::string outPath =
      (testScratchPath("") /
       "primec_dump_ast_semantic_borrowed_return_experimental_soa_vector_field_view.txt")
          .string();

  const std::string dumpCmd =
      "./primec " + quoteShellArg(srcPath) + " --dump-stage ast-semantic > " + quoteShellArg(outPath);
  CHECK(runCommand(dumpCmd) == 0);
  const std::string ast = readFile(outPath);
  const size_t mainPos = ast.find("/main()");
  CHECK(mainPos != std::string::npos);
  CHECK(ast.find("/std/collections/experimental_soa_vector/soaVectorGet__", mainPos) !=
        std::string::npos);
  CHECK(ast.find(".y", mainPos) != std::string::npos);
  CHECK(ast.find("pickBorrowed(location(values)).y()[", mainPos) == std::string::npos);
}

TEST_CASE("dump ast-semantic rewrites experimental soa_vector reflected call-form field index syntax") {
  const std::string source = R"(
import /std/collections/experimental_soa_vector/*

[struct reflect]
Particle() {
  [i32] x{1i32}
  [i32] y{2i32}
}

[return<Reference<SoaVector<Particle>>>]
pickBorrowed([Reference<SoaVector<Particle>>] values) {
  return(values)
}

[effects(heap_alloc), return<int>]
main() {
  [SoaVector<Particle> mut] values{soaVectorNew<Particle>()}
  values.push(Particle(4i32, 6i32))
  values.push(Particle(9i32, 12i32))
  [Reference<SoaVector<Particle>>] borrowed{location(values)}
  [int] total{
    plus(
      y(values)[0i32],
      plus(
        y(dereference(borrowed))[1i32],
        plus(
          y(pickBorrowed(location(values)))[1i32],
          y(dereference(pickBorrowed(location(values))))[0i32]
        )
      )
    )
  }
  return(total)
}
)";
  const std::string srcPath =
      writeTemp("compile_dump_ast_semantic_call_form_experimental_soa_vector_field_view.prime", source);
  const std::string outPath =
      (testScratchPath("") /
       "primec_dump_ast_semantic_call_form_experimental_soa_vector_field_view.txt")
          .string();

  const std::string dumpCmd =
      "./primec " + quoteShellArg(srcPath) + " --dump-stage ast-semantic > " + quoteShellArg(outPath);
  CHECK(runCommand(dumpCmd) == 0);
  const std::string ast = readFile(outPath);
  const size_t mainPos = ast.find("/main()");
  CHECK(mainPos != std::string::npos);
  CHECK(ast.find("/std/collections/experimental_soa_vector/soaVectorGet__", mainPos) !=
        std::string::npos);
  CHECK(ast.find(".y", mainPos) != std::string::npos);
  CHECK(ast.find("y(values)[", mainPos) == std::string::npos);
  CHECK(ast.find("y(dereference(borrowed))[", mainPos) == std::string::npos);
  CHECK(ast.find("y(pickBorrowed(location(values)))[", mainPos) == std::string::npos);
  CHECK(ast.find("y(dereference(pickBorrowed(location(values))))[", mainPos) == std::string::npos);
}

TEST_CASE("dump ast-semantic rewrites experimental soa_vector inline location borrow field index syntax") {
  const std::string source = R"(
import /std/collections/experimental_soa_vector/*

[struct reflect]
Particle() {
  [i32] x{1i32}
  [i32] y{2i32}
}

[effects(heap_alloc), return<int>]
main() {
  [SoaVector<Particle> mut] values{soaVectorNew<Particle>()}
  values.push(Particle(7i32, 8i32))
  values.push(Particle(9i32, 12i32))
  [int] total{
    plus(
      location(values).y()[0i32],
      plus(
        dereference(location(values)).y()[1i32],
        plus(
          y(location(values))[0i32],
          y(dereference(location(values)))[1i32]
        )
      )
    )
  }
  return(total)
}
)";
  const std::string srcPath =
      writeTemp("compile_dump_ast_semantic_inline_location_experimental_soa_vector_field_view.prime",
                source);
  const std::string outPath =
      (testScratchPath("") /
       "primec_dump_ast_semantic_inline_location_experimental_soa_vector_field_view.txt")
          .string();

  const std::string dumpCmd =
      "./primec " + quoteShellArg(srcPath) + " --dump-stage ast-semantic > " + quoteShellArg(outPath);
  CHECK(runCommand(dumpCmd) == 0);
  const std::string ast = readFile(outPath);
  const size_t mainPos = ast.find("/main()");
  CHECK(mainPos != std::string::npos);
  CHECK(ast.find("/std/collections/experimental_soa_vector/soaVectorGet__", mainPos) !=
        std::string::npos);
  CHECK(ast.find(".y", mainPos) != std::string::npos);
  CHECK(ast.find("location(values).y()[", mainPos) == std::string::npos);
  CHECK(ast.find("dereference(location(values)).y()[", mainPos) == std::string::npos);
  CHECK(ast.find("y(location(values))[", mainPos) == std::string::npos);
  CHECK(ast.find("y(dereference(location(values)))[", mainPos) == std::string::npos);
}

TEST_CASE("dump ast-semantic rewrites dereferenced borrowed helper-return experimental soa_vector reflected field index syntax") {
  const std::string source = R"(
import /std/collections/experimental_soa_vector/*

[struct reflect]
Particle() {
  [i32] x{1i32}
  [i32] y{2i32}
}

[return<Reference<SoaVector<Particle>>>]
pickBorrowed([Reference<SoaVector<Particle>>] values) {
  return(values)
}

[effects(heap_alloc), return<int>]
main() {
  [SoaVector<Particle> mut] values{soaVectorNew<Particle>()}
  values.push(Particle(4i32, 6i32))
  values.push(Particle(9i32, 12i32))
  return(dereference(pickBorrowed(location(values))).y()[1i32])
}
)";
  const std::string srcPath = writeTemp(
      "compile_dump_ast_semantic_dereferenced_borrowed_return_experimental_soa_vector_field_view.prime",
      source);
  const std::string outPath =
      (testScratchPath("") /
       "primec_dump_ast_semantic_dereferenced_borrowed_return_experimental_soa_vector_field_view.txt")
          .string();

  const std::string dumpCmd =
      "./primec " + quoteShellArg(srcPath) + " --dump-stage ast-semantic > " + quoteShellArg(outPath);
  CHECK(runCommand(dumpCmd) == 0);
  const std::string ast = readFile(outPath);
  const size_t mainPos = ast.find("/main()");
  CHECK(mainPos != std::string::npos);
  CHECK(ast.find("/std/collections/experimental_soa_vector/soaVectorGet__", mainPos) !=
        std::string::npos);
CHECK(ast.find(".y", mainPos) != std::string::npos);
CHECK(ast.find("dereference(pickBorrowed(location(values))).y()[", mainPos) == std::string::npos);
}

TEST_CASE("dump ast-semantic rewrites method-like borrowed helper-return experimental soa_vector helpers") {
  const std::string source = R"(
import /std/collections/experimental_soa_vector/*

[struct reflect]
Particle() {
  [i32] x{1i32}
  [i32] y{2i32}
}

[struct]
Holder() {}

[return<Reference<SoaVector<Particle>>>]
/Holder/pickBorrowed([Holder] self, [Reference<SoaVector<Particle>>] values) {
  return(values)
}

[effects(heap_alloc), return<int>]
main() {
  [SoaVector<Particle> mut] values{soaVectorNew<Particle>()}
  values.push(Particle(4i32, 6i32))
  values.push(Particle(9i32, 12i32))
  [Holder] holder{Holder()}
  [Particle] picked{get(holder.pickBorrowed(location(values)), 1i32)}
  [i32] fieldBareGet{get(holder.pickBorrowed(location(values)), 1i32).y}
  [i32] fieldBareRef{ref(holder.pickBorrowed(location(values)), 0i32).x}
  [i32] fieldMethodRef{holder.pickBorrowed(location(values)).ref(1i32).y}
  return(plus(picked.x,
              plus(plus(plus(fieldBareGet, fieldBareRef), fieldMethodRef),
                   plus(holder.pickBorrowed(location(values)).y()[0i32],
                        y(holder.pickBorrowed(location(values)))[1i32]))))
}
)";
  const std::string srcPath = writeTemp(
      "compile_dump_ast_semantic_method_like_borrowed_return_experimental_soa_vector_helpers.prime",
      source);
  const std::string outPath =
      (testScratchPath("") /
       "primec_dump_ast_semantic_method_like_borrowed_return_experimental_soa_vector_helpers.txt")
          .string();

  const std::string dumpCmd =
      "./primec " + quoteShellArg(srcPath) + " --dump-stage ast-semantic > " + quoteShellArg(outPath);
  CHECK(runCommand(dumpCmd) == 0);
  const std::string ast = readFile(outPath);
  const size_t mainPos = ast.find("/main()");
  CHECK(mainPos != std::string::npos);
  CHECK(ast.find("get(holder.pickBorrowed(location(values)),", mainPos) == std::string::npos);
  CHECK(ast.find("ref(holder.pickBorrowed(location(values)),", mainPos) == std::string::npos);
  CHECK(ast.find("holder.pickBorrowed(location(values)).get(1)", mainPos) == std::string::npos);
  CHECK(ast.find("holder.pickBorrowed(location(values)).ref(1).y", mainPos) == std::string::npos);
  CHECK(ast.find("/Holder/pickBorrowed(holder, location(values)).get(1)", mainPos) != std::string::npos);
  CHECK(ast.find("/Holder/pickBorrowed(holder, location(values)).ref(0)", mainPos) != std::string::npos);
  CHECK(ast.find("/Holder/pickBorrowed(holder, location(values)).ref(1).y", mainPos) !=
        std::string::npos);
  CHECK(ast.find("holder.pickBorrowed(location(values)).y()[", mainPos) == std::string::npos);
  CHECK(ast.find("y(holder.pickBorrowed(location(values)))[", mainPos) == std::string::npos);
  CHECK(ast.find(".get(1).y", mainPos) != std::string::npos);
  CHECK(ast.find(".ref(0).x", mainPos) != std::string::npos);
  CHECK(ast.find("/std/collections/experimental_soa_vector/soaVectorGet__", mainPos) !=
        std::string::npos);
  CHECK(ast.find(".y", mainPos) != std::string::npos);
}

TEST_CASE("dump ast-semantic rewrites inline location method-like borrowed helper-return experimental soa_vector helpers") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_soa_vector/*
import /std/collections/experimental_soa_vector_conversions/*

[struct reflect]
Particle() {
  [i32] x{1i32}
  [i32] y{2i32}
}

[struct]
Holder() {}

[return<Reference<SoaVector<Particle>>>]
/Holder/pickBorrowed([Holder] self, [Reference<SoaVector<Particle>>] values) {
  return(values)
}

[effects(heap_alloc), return<int>]
main() {
  [SoaVector<Particle> mut] values{soaVectorNew<Particle>()}
  values.push(Particle(7i32, 8i32))
  values.push(Particle(9i32, 12i32))
  [Holder] holder{Holder()}
  [Particle] firstA{location(holder.pickBorrowed(location(values))).get(0i32)}
  [Particle] secondA{location(holder.pickBorrowed(location(values))).ref(1i32)}
  [vector<Particle>] unpackedA{location(holder.pickBorrowed(location(values))).to_aos()}
  [i32] countA{location(holder.pickBorrowed(location(values))).count()}
  [Particle] firstB{dereference(location(holder.pickBorrowed(location(values)))).get(0i32)}
  [Particle] secondB{dereference(location(holder.pickBorrowed(location(values)))).ref(1i32)}
  [i32] fieldBareGet{get(location(holder.pickBorrowed(location(values))), 1i32).y}
  [i32] fieldBareRef{ref(dereference(location(holder.pickBorrowed(location(values)))), 0i32).x}
  [i32] fieldMethodRef{location(holder.pickBorrowed(location(values))).ref(1i32).y}
  [vector<Particle>] unpackedB{dereference(location(holder.pickBorrowed(location(values)))).to_aos()}
  [i32] countB{dereference(location(holder.pickBorrowed(location(values)))).count()}
  [int] fieldTotals{
    plus(location(holder.pickBorrowed(location(values))).y()[0i32],
         plus(dereference(location(holder.pickBorrowed(location(values)))).y()[1i32],
              plus(y(location(holder.pickBorrowed(location(values))))[0i32],
                   y(dereference(location(holder.pickBorrowed(location(values)))))[1i32])))
  }
  [int] total{
    plus(plus(firstA.x, secondA.x),
         plus(count(unpackedA),
              plus(countA,
                   plus(plus(firstB.x, secondB.x),
                        plus(count(unpackedB),
                             plus(countB,
                                  plus(plus(plus(fieldBareGet, fieldBareRef), fieldMethodRef),
                                       fieldTotals)))))))
  }
  return(total)
}
)";
  const std::string srcPath = writeTemp(
      "compile_dump_ast_semantic_inline_location_method_like_borrowed_return_experimental_soa_vector_helpers.prime",
      source);
  const std::string outPath =
      (testScratchPath("") /
       "primec_dump_ast_semantic_inline_location_method_like_borrowed_return_experimental_soa_vector_helpers.txt")
          .string();

  const std::string dumpCmd =
      "./primec " + quoteShellArg(srcPath) + " --dump-stage ast-semantic > " + quoteShellArg(outPath);
  CHECK(runCommand(dumpCmd) == 0);
  const std::string ast = readFile(outPath);
  const size_t mainPos = ast.find("/main()");
  CHECK(mainPos != std::string::npos);
  CHECK(ast.find("location(holder.pickBorrowed(location(values))).get(", mainPos) == std::string::npos);
  CHECK(ast.find("location(holder.pickBorrowed(location(values))).ref(", mainPos) == std::string::npos);
  CHECK(ast.find("get(location(holder.pickBorrowed(location(values))),", mainPos) == std::string::npos);
  CHECK(ast.find("ref(dereference(location(holder.pickBorrowed(location(values)))),", mainPos) ==
        std::string::npos);
  CHECK(ast.find("location(holder.pickBorrowed(location(values))).ref(1).y", mainPos) ==
        std::string::npos);
  CHECK(ast.find("location(holder.pickBorrowed(location(values))).to_aos()", mainPos) ==
        std::string::npos);
  CHECK(ast.find("location(holder.pickBorrowed(location(values))).count()", mainPos) ==
        std::string::npos);
  CHECK(ast.find("dereference(location(holder.pickBorrowed(location(values)))).get(", mainPos) ==
        std::string::npos);
  CHECK(ast.find("dereference(location(holder.pickBorrowed(location(values)))).ref(", mainPos) ==
        std::string::npos);
  CHECK(ast.find("dereference(location(holder.pickBorrowed(location(values)))).to_aos()", mainPos) ==
        std::string::npos);
  CHECK(ast.find("dereference(location(holder.pickBorrowed(location(values)))).count()", mainPos) ==
        std::string::npos);
  CHECK(ast.find("location(holder.pickBorrowed(location(values))).y()[", mainPos) == std::string::npos);
  CHECK(ast.find("dereference(location(holder.pickBorrowed(location(values)))).y()[", mainPos) ==
        std::string::npos);
  CHECK(ast.find("y(location(holder.pickBorrowed(location(values))))[", mainPos) == std::string::npos);
  CHECK(ast.find("y(dereference(location(holder.pickBorrowed(location(values)))))[", mainPos) ==
        std::string::npos);
  CHECK(ast.find("/Holder/pickBorrowed(holder, location(values)).get(0)", mainPos) !=
        std::string::npos);
  CHECK(ast.find("/Holder/pickBorrowed(holder, location(values)).ref(1)", mainPos) !=
        std::string::npos);
  CHECK(ast.find("/Holder/pickBorrowed(holder, location(values)).ref(1).y", mainPos) !=
        std::string::npos);
  CHECK(ast.find("/Holder/pickBorrowed(holder, location(values)).get(1).y", mainPos) !=
        std::string::npos);
  CHECK(ast.find("/Holder/pickBorrowed(holder, location(values)).ref(0).x", mainPos) !=
        std::string::npos);
  CHECK(ast.find("/Holder/pickBorrowed(holder, location(values)).count()", mainPos) !=
        std::string::npos);
  CHECK(ast.find("/std/collections/experimental_soa_vector_conversions/soaVectorToAos__", mainPos) !=
        std::string::npos);
  CHECK(ast.find("/std/collections/experimental_soa_vector/soaVectorGet__", mainPos) !=
        std::string::npos);
  CHECK(ast.find(".y", mainPos) != std::string::npos);
}

TEST_CASE("dump ast-semantic rewrites direct return method-like borrowed helper-return experimental soa_vector reads") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_soa_vector/*
import /std/collections/experimental_soa_vector_conversions/*

[struct reflect]
Particle() {
  [i32] x{1i32}
  [i32] y{2i32}
}

[struct]
Holder() {}

[return<Reference<SoaVector<Particle>>>]
/Holder/pickBorrowed([Holder] self, [Reference<SoaVector<Particle>>] values) {
  return(values)
}

[effects(heap_alloc), return<int>]
main() {
  [SoaVector<Particle> mut] values{soaVectorNew<Particle>()}
  values.push(Particle(7i32, 8i32))
  values.push(Particle(9i32, 12i32))
  [Holder] holder{Holder()}
  return(
    plus(count(holder.pickBorrowed(location(values))),
         plus(count(holder.pickBorrowed(location(values)).to_aos()),
              plus(holder.pickBorrowed(location(values)).get(0i32).x,
                   plus(ref(holder.pickBorrowed(location(values)), 1i32).y,
                        plus(get(holder.pickBorrowed(location(values)), 1i32).y,
                             plus(holder.pickBorrowed(location(values)).y()[1i32],
                                  y(holder.pickBorrowed(location(values)))[0i32]))))))
  )
}
)";
  const std::string srcPath = writeTemp(
      "compile_dump_ast_semantic_direct_return_method_like_borrowed_helper_reads.prime",
      source);
  const std::string outPath =
      (testScratchPath("") /
       "primec_dump_ast_semantic_direct_return_method_like_borrowed_helper_reads.txt")
          .string();

  const std::string dumpCmd =
      "./primec " + quoteShellArg(srcPath) + " --dump-stage ast-semantic > " + quoteShellArg(outPath);
  CHECK(runCommand(dumpCmd) == 0);
  const std::string ast = readFile(outPath);
  const size_t mainPos = ast.find("/main()");
  CHECK(mainPos != std::string::npos);
  CHECK(ast.find("holder.pickBorrowed(location(values)).get(", mainPos) == std::string::npos);
  CHECK(ast.find("holder.pickBorrowed(location(values)).to_aos()", mainPos) == std::string::npos);
  CHECK(ast.find("count(holder.pickBorrowed(location(values)))", mainPos) == std::string::npos);
  CHECK(ast.find("get(holder.pickBorrowed(location(values)),", mainPos) == std::string::npos);
  CHECK(ast.find("ref(holder.pickBorrowed(location(values)),", mainPos) == std::string::npos);
  CHECK(ast.find("holder.pickBorrowed(location(values)).y()[", mainPos) == std::string::npos);
  CHECK(ast.find("y(holder.pickBorrowed(location(values)))[", mainPos) == std::string::npos);
  CHECK(ast.find("/Holder/pickBorrowed(holder, location(values)).count()", mainPos) !=
        std::string::npos);
  CHECK(ast.find("/Holder/pickBorrowed(holder, location(values)).get(0).x", mainPos) !=
        std::string::npos);
  CHECK(ast.find("/Holder/pickBorrowed(holder, location(values)).ref(1).y", mainPos) !=
        std::string::npos);
  CHECK(ast.find(".get(1).y", mainPos) != std::string::npos);
  CHECK(ast.find("/std/collections/experimental_soa_vector_conversions/soaVectorToAos__", mainPos) !=
        std::string::npos);
  CHECK(ast.find("/std/collections/experimental_soa_vector/soaVectorGet__", mainPos) !=
        std::string::npos);
}

TEST_CASE("dump ast-semantic rewrites direct return inline location method-like borrowed helper-return experimental soa_vector reads") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_soa_vector/*
import /std/collections/experimental_soa_vector_conversions/*

[struct reflect]
Particle() {
  [i32] x{1i32}
  [i32] y{2i32}
}

[struct]
Holder() {}

[return<Reference<SoaVector<Particle>>>]
/Holder/pickBorrowed([Holder] self, [Reference<SoaVector<Particle>>] values) {
  return(values)
}

[effects(heap_alloc), return<int>]
main() {
  [SoaVector<Particle> mut] values{soaVectorNew<Particle>()}
  values.push(Particle(7i32, 8i32))
  values.push(Particle(9i32, 12i32))
  [Holder] holder{Holder()}
  return(
    plus(location(holder.pickBorrowed(location(values))).count(),
         plus(count(location(holder.pickBorrowed(location(values))).to_aos()),
              plus(dereference(location(holder.pickBorrowed(location(values)))).get(1i32).x,
                   plus(ref(dereference(location(holder.pickBorrowed(location(values)))), 0i32).x,
                        plus(get(location(holder.pickBorrowed(location(values))), 1i32).y,
                             plus(location(holder.pickBorrowed(location(values))).y()[0i32],
                                  y(dereference(location(holder.pickBorrowed(location(values)))))[1i32]))))))
  )
}
)";
  const std::string srcPath = writeTemp(
      "compile_dump_ast_semantic_direct_return_inline_location_method_like_borrowed_helper_reads.prime",
      source);
  const std::string outPath =
      (testScratchPath("") /
       "primec_dump_ast_semantic_direct_return_inline_location_method_like_borrowed_helper_reads.txt")
          .string();

  const std::string dumpCmd =
      "./primec " + quoteShellArg(srcPath) + " --dump-stage ast-semantic > " + quoteShellArg(outPath);
  CHECK(runCommand(dumpCmd) == 0);
  const std::string ast = readFile(outPath);
  const size_t mainPos = ast.find("/main()");
  CHECK(mainPos != std::string::npos);
  CHECK(ast.find("location(holder.pickBorrowed(location(values))).count()", mainPos) ==
        std::string::npos);
  CHECK(ast.find("location(holder.pickBorrowed(location(values))).to_aos()", mainPos) ==
        std::string::npos);
  CHECK(ast.find("dereference(location(holder.pickBorrowed(location(values)))).get(", mainPos) ==
        std::string::npos);
  CHECK(ast.find("get(location(holder.pickBorrowed(location(values))),", mainPos) ==
        std::string::npos);
  CHECK(ast.find("ref(dereference(location(holder.pickBorrowed(location(values)))),", mainPos) ==
        std::string::npos);
  CHECK(ast.find("location(holder.pickBorrowed(location(values))).y()[", mainPos) ==
        std::string::npos);
  CHECK(ast.find("y(dereference(location(holder.pickBorrowed(location(values)))))[", mainPos) ==
        std::string::npos);
  CHECK(ast.find("/Holder/pickBorrowed(holder, location(values)).count()", mainPos) !=
        std::string::npos);
  CHECK(ast.find("/Holder/pickBorrowed(holder, location(values)).get(1).x", mainPos) !=
        std::string::npos);
  CHECK(ast.find("/Holder/pickBorrowed(holder, location(values)).ref(0).x", mainPos) !=
        std::string::npos);
  CHECK(ast.find("/Holder/pickBorrowed(holder, location(values)).get(1).y", mainPos) !=
        std::string::npos);
  CHECK(ast.find("/std/collections/experimental_soa_vector_conversions/soaVectorToAos__", mainPos) !=
        std::string::npos);
  CHECK(ast.find("/std/collections/experimental_soa_vector/soaVectorGet__", mainPos) !=
        std::string::npos);
}

TEST_CASE("dump ast-semantic rewrites builtin soa_vector count forms to canonical helper path") {
  const std::string source = R"(
[struct reflect]
Particle() {
  [i32] x{1i32}
}

[return<int>]
main() {
  [soa_vector<Particle>] values{soa_vector<Particle>()}
  [int] total{plus(count(values), plus(/soa_vector/count(values), values./soa_vector/count()))}
  return(total)
}
)";
  const std::string srcPath = writeTemp("compile_dump_ast_semantic_builtin_soa_vector_count.prime", source);
  const std::string outPath =
      (testScratchPath("") / "primec_dump_ast_semantic_builtin_soa_vector_count.txt").string();

  const std::string dumpCmd =
      "./primec " + quoteShellArg(srcPath) + " --dump-stage ast-semantic > " + quoteShellArg(outPath);
  CHECK(runCommand(dumpCmd) == 0);
  const std::string ast = readFile(outPath);
  const size_t mainPos = ast.find("/main()");
  CHECK(mainPos != std::string::npos);
  CHECK(ast.find("/std/collections/soa_vector/count__", mainPos) != std::string::npos);
  CHECK(ast.find("return /std/collections/soa_vector/count__", mainPos) == std::string::npos);
  CHECK(ast.find("/soa_vector/count(values)", mainPos) == std::string::npos);
  CHECK(ast.find("values./soa_vector/count()", mainPos) == std::string::npos);
}

TEST_CASE("dump ast-semantic rewrites imported builtin soa_vector to_aos forms to canonical helper path") {
  const std::string source = R"(
import /std/collections/*

[struct reflect]
Particle() {
  [i32] x{1i32}
}

[return<int>]
main() {
  [soa_vector<Particle>] values{soa_vector<Particle>()}
  [vector<Particle>] unpackedA{to_aos(values)}
  [vector<Particle>] unpackedB{/to_aos(values)}
  [vector<Particle>] unpackedC{values.to_aos()}
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_dump_ast_semantic_builtin_soa_vector_to_aos.prime", source);
  const std::string outPath =
      (testScratchPath("") / "primec_dump_ast_semantic_builtin_soa_vector_to_aos.txt").string();

  const std::string dumpCmd =
      "./primec " + quoteShellArg(srcPath) + " --dump-stage ast-semantic > " + quoteShellArg(outPath);
  CHECK(runCommand(dumpCmd) == 0);
  const std::string ast = readFile(outPath);
  const size_t mainPos = ast.find("/main()");
  CHECK(mainPos != std::string::npos);
  CHECK(ast.find("/std/collections/soa_vector/to_aos__", mainPos) != std::string::npos);
  CHECK(ast.find("/std/collections/experimental_soa_vector_conversions/soaVectorToAos__", mainPos) ==
        std::string::npos);
  CHECK(ast.find("/to_aos(values)", mainPos) == std::string::npos);
}

TEST_CASE("dump ast-semantic keeps direct canonical experimental soa_vector to_aos helper path") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_soa_vector/*

[struct reflect]
Particle() {
  [i32] x{1i32}
}

[effects(heap_alloc), return<int>]
main() {
  [SoaVector<Particle>] values{soaVectorSingle<Particle>(Particle(7i32))}
  [vector<Particle>] unpacked{/std/collections/soa_vector/to_aos<Particle>(values)}
  return(count(unpacked))
}
)";
  const std::string srcPath =
      writeTemp("compile_dump_ast_semantic_direct_canonical_experimental_soa_vector_to_aos.prime", source);
  const std::string outPath =
      (testScratchPath("") / "primec_dump_ast_semantic_direct_canonical_experimental_soa_vector_to_aos.txt")
          .string();

  const std::string dumpCmd =
      "./primec " + quoteShellArg(srcPath) + " --dump-stage ast-semantic > " + quoteShellArg(outPath);
  CHECK(runCommand(dumpCmd) == 0);
  const std::string ast = readFile(outPath);
  const size_t mainPos = ast.find("/main()");
  CHECK(mainPos != std::string::npos);
  CHECK(ast.find("/std/collections/soa_vector/to_aos__", mainPos) != std::string::npos);
  CHECK(ast.find("/std/collections/experimental_soa_vector_conversions/soaVectorToAos__", mainPos) ==
        std::string::npos);
}

TEST_CASE("dump ast-semantic keeps imported experimental soa_vector to_aos helper path") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_soa_vector/*

[struct reflect]
Particle() {
  [i32] x{1i32}
}

[effects(heap_alloc), return<int>]
main() {
  [SoaVector<Particle>] values{soaVectorSingle<Particle>(Particle(7i32))}
  [vector<Particle>] unpacked{to_aos(values)}
  return(count(unpacked))
}
)";
  const std::string srcPath =
      writeTemp("compile_dump_ast_semantic_imported_experimental_soa_vector_to_aos.prime", source);
  const std::string outPath =
      (testScratchPath("") / "primec_dump_ast_semantic_imported_experimental_soa_vector_to_aos.txt")
          .string();

  const std::string dumpCmd =
      "./primec " + quoteShellArg(srcPath) + " --dump-stage ast-semantic > " + quoteShellArg(outPath);
  CHECK(runCommand(dumpCmd) == 0);
  const std::string ast = readFile(outPath);
  const size_t mainPos = ast.find("/main()");
  CHECK(mainPos != std::string::npos);
  CHECK(ast.find("/std/collections/soa_vector/to_aos__", mainPos) != std::string::npos);
  CHECK(ast.find("/std/collections/experimental_soa_vector_conversions/soaVectorToAos__", mainPos) ==
        std::string::npos);
  CHECK(ast.find("to_aos(values)", mainPos) == std::string::npos);
}

TEST_CASE("dump ast-semantic rewrites borrowed helper-return experimental soa_vector to_aos") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_soa_vector/*
import /std/collections/experimental_soa_vector_conversions/*

[struct reflect]
Particle() {
  [i32] x{1i32}
}

[return<Reference<SoaVector<Particle>>>]
pickBorrowed([Reference<SoaVector<Particle>>] values) {
  return(values)
}

[effects(heap_alloc), return<int>]
main() {
  [SoaVector<Particle> mut] values{soaVectorNew<Particle>()}
  values.push(Particle(7i32))
  values.push(Particle(9i32))
  [vector<Particle>] unpacked{pickBorrowed(location(values)).to_aos()}
  return(count(unpacked))
}
)";
  const std::string srcPath =
      writeTemp("compile_dump_ast_semantic_borrowed_return_experimental_soa_vector_to_aos.prime", source);
  const std::string outPath =
      (testScratchPath("") / "primec_dump_ast_semantic_borrowed_return_experimental_soa_vector_to_aos.txt")
          .string();

  const std::string dumpCmd =
      "./primec " + quoteShellArg(srcPath) + " --dump-stage ast-semantic > " + quoteShellArg(outPath);
  CHECK(runCommand(dumpCmd) == 0);
  const std::string ast = readFile(outPath);
  const size_t mainPos = ast.find("/main()");
  CHECK(mainPos != std::string::npos);
  CHECK(ast.find("/std/collections/experimental_soa_vector_conversions/soaVectorToAos__", mainPos) !=
        std::string::npos);
  CHECK(ast.find("pickBorrowed(location(values)).to_aos()", mainPos) == std::string::npos);
}

TEST_CASE("dump ast-semantic rewrites inline location experimental soa_vector read-only methods") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_soa_vector/*
import /std/collections/experimental_soa_vector_conversions/*

[struct reflect]
Particle() {
  [i32] x{1i32}
}

[effects(heap_alloc), return<int>]
main() {
  [SoaVector<Particle> mut] values{soaVectorNew<Particle>()}
  values.push(Particle(7i32))
  values.push(Particle(9i32))
  [Particle] firstA{location(values).get(0i32)}
  [Particle] secondA{location(values).ref(1i32)}
  [vector<Particle>] unpackedA{location(values).to_aos()}
  [i32] countA{location(values).count()}
  [Particle] firstB{dereference(location(values)).get(0i32)}
  [Particle] secondB{dereference(location(values)).ref(1i32)}
  [vector<Particle>] unpackedB{dereference(location(values)).to_aos()}
  [i32] countB{dereference(location(values)).count()}
  return(plus(plus(firstA.x, secondA.x),
              plus(count(unpackedA),
                   plus(countA,
                        plus(plus(firstB.x, secondB.x),
                             plus(count(unpackedB), countB))))))
}
)";
  const std::string srcPath =
      writeTemp("compile_dump_ast_semantic_inline_location_experimental_soa_vector_methods.prime", source);
  const std::string outPath =
      (testScratchPath("") /
       "primec_dump_ast_semantic_inline_location_experimental_soa_vector_methods.txt")
          .string();

  const std::string dumpCmd =
      "./primec " + quoteShellArg(srcPath) + " --dump-stage ast-semantic > " + quoteShellArg(outPath);
  CHECK(runCommand(dumpCmd) == 0);
  const std::string ast = readFile(outPath);
  const size_t mainPos = ast.find("/main()");
  CHECK(mainPos != std::string::npos);
  CHECK(ast.find("location(values).get(", mainPos) == std::string::npos);
  CHECK(ast.find("location(values).ref(", mainPos) == std::string::npos);
  CHECK(ast.find("location(values).to_aos()", mainPos) == std::string::npos);
  CHECK(ast.find("location(values).count()", mainPos) == std::string::npos);
  CHECK(ast.find("dereference(location(values)).get(", mainPos) == std::string::npos);
  CHECK(ast.find("dereference(location(values)).ref(", mainPos) == std::string::npos);
  CHECK(ast.find("dereference(location(values)).to_aos()", mainPos) == std::string::npos);
  CHECK(ast.find("dereference(location(values)).count()", mainPos) == std::string::npos);
  CHECK(ast.find("values.get(0)", mainPos) != std::string::npos);
  CHECK(ast.find("values.ref(1)", mainPos) != std::string::npos);
  CHECK(ast.find("values.count()", mainPos) != std::string::npos);
  CHECK(ast.find("/std/collections/experimental_soa_vector_conversions/soaVectorToAos__", mainPos) !=
        std::string::npos);
}

TEST_CASE("dump ast-semantic rewrites inline location borrowed helper-return experimental soa_vector helpers") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_soa_vector/*
import /std/collections/experimental_soa_vector_conversions/*

[struct reflect]
Particle() {
  [i32] x{1i32}
  [i32] y{2i32}
}

[return<Reference<SoaVector<Particle>>>]
pickBorrowed([Reference<SoaVector<Particle>>] values) {
  return(values)
}

[effects(heap_alloc), return<int>]
main() {
  [SoaVector<Particle> mut] values{soaVectorNew<Particle>()}
  values.push(Particle(7i32, 8i32))
  values.push(Particle(9i32, 12i32))
  [Particle] firstA{location(pickBorrowed(location(values))).get(0i32)}
  [Particle] secondA{location(pickBorrowed(location(values))).ref(1i32)}
  [vector<Particle>] unpackedA{location(pickBorrowed(location(values))).to_aos()}
  [i32] countA{location(pickBorrowed(location(values))).count()}
  [Particle] firstB{dereference(location(pickBorrowed(location(values)))).get(0i32)}
  [Particle] secondB{dereference(location(pickBorrowed(location(values)))).ref(1i32)}
  [vector<Particle>] unpackedB{dereference(location(pickBorrowed(location(values)))).to_aos()}
  [i32] countB{dereference(location(pickBorrowed(location(values)))).count()}
  [int] total{
    plus(plus(firstA.x, secondA.x),
         plus(count(unpackedA),
              plus(countA,
                   plus(plus(firstB.x, secondB.x),
                        plus(count(unpackedB),
                             plus(countB,
                                  plus(location(pickBorrowed(location(values))).y()[0i32],
                                       plus(dereference(location(pickBorrowed(location(values)))).y()[1i32],
                                            plus(y(location(pickBorrowed(location(values))))[0i32],
                                                 y(dereference(location(pickBorrowed(location(values)))))[1i32])))))))))
  }
  return(total)
}
)";
  const std::string srcPath =
      writeTemp("compile_dump_ast_semantic_inline_location_borrowed_return_experimental_soa_vector_helpers.prime",
                source);
  const std::string outPath =
      (testScratchPath("") /
       "primec_dump_ast_semantic_inline_location_borrowed_return_experimental_soa_vector_helpers.txt")
          .string();

  const std::string dumpCmd =
      "./primec " + quoteShellArg(srcPath) + " --dump-stage ast-semantic > " + quoteShellArg(outPath);
  CHECK(runCommand(dumpCmd) == 0);
  const std::string ast = readFile(outPath);
  const size_t mainPos = ast.find("/main()");
  CHECK(mainPos != std::string::npos);
  CHECK(ast.find("location(pickBorrowed(location(values))).get(", mainPos) == std::string::npos);
  CHECK(ast.find("location(pickBorrowed(location(values))).ref(", mainPos) == std::string::npos);
  CHECK(ast.find("location(pickBorrowed(location(values))).to_aos()", mainPos) == std::string::npos);
  CHECK(ast.find("location(pickBorrowed(location(values))).count()", mainPos) == std::string::npos);
  CHECK(ast.find("dereference(location(pickBorrowed(location(values)))).get(", mainPos) ==
        std::string::npos);
  CHECK(ast.find("dereference(location(pickBorrowed(location(values)))).ref(", mainPos) ==
        std::string::npos);
  CHECK(ast.find("dereference(location(pickBorrowed(location(values)))).to_aos()", mainPos) ==
        std::string::npos);
  CHECK(ast.find("dereference(location(pickBorrowed(location(values)))).count()", mainPos) ==
        std::string::npos);
  CHECK(ast.find("location(pickBorrowed(location(values))).y()[", mainPos) == std::string::npos);
  CHECK(ast.find("dereference(location(pickBorrowed(location(values)))).y()[", mainPos) ==
        std::string::npos);
  CHECK(ast.find("y(location(pickBorrowed(location(values))))[", mainPos) == std::string::npos);
  CHECK(ast.find("y(dereference(location(pickBorrowed(location(values)))))[", mainPos) ==
        std::string::npos);
  CHECK(ast.find("pickBorrowed(location(values)).get(0)", mainPos) != std::string::npos);
  CHECK(ast.find("pickBorrowed(location(values)).ref(1)", mainPos) != std::string::npos);
  CHECK(ast.find("pickBorrowed(location(values)).count()", mainPos) != std::string::npos);
  CHECK(ast.find("/std/collections/experimental_soa_vector_conversions/soaVectorToAos__", mainPos) !=
        std::string::npos);
  CHECK(ast.find("/std/collections/experimental_soa_vector/soaVectorGet__", mainPos) !=
        std::string::npos);
  CHECK(ast.find(".y", mainPos) != std::string::npos);
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
