#include "test_compile_run_text_filters_helpers.h"

TEST_SUITE_BEGIN("primestruct.compile.run.text_filters");

#include "primec/testing/CompilePipelineDumpHelpers.h"

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
  CHECK(ast.find("/std/collections/internal_soa_storage/soaColumnCount", countPos) !=
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
  CHECK(ast.find("/std/collections/soa_vector/get_ref__", mainPos) !=
        std::string::npos);
  CHECK(ast.find("return /std/collections/soa_vector/get_ref__", mainPos) !=
        std::string::npos);
  CHECK(ast.find("return get(", mainPos) == std::string::npos);
}

TEST_CASE("dump ast-semantic rewrites global helper-return soa_vector method shadows to same-path helpers") {
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

[return<int>]
/soa_vector/count([SoaVector<Particle>] values) {
  return(11i32)
}

[return<Particle>]
/soa_vector/get([SoaVector<Particle>] values, [int] index) {
  return(Particle(23i32))
}

[return<Particle>]
/soa_vector/ref([SoaVector<Particle>] values, [int] index) {
  return(Particle(29i32))
}

[return<int>]
/soa_vector/push([SoaVector<Particle>] values, [Particle] value) {
  return(value.x)
}

[return<int>]
/soa_vector/reserve([SoaVector<Particle>] values, [int] count) {
  return(count)
}

[effects(heap_alloc), return<int>]
main() {
  [Particle] value{Particle(31i32)}
  return(plus(cloneValues().count(),
              plus(cloneValues().get(0i32).x,
                   plus(cloneValues().ref(0i32).x,
                        plus(cloneValues().push(value),
                             cloneValues().reserve(37i32))))))
}
)";
  const std::string srcPath =
      writeTemp("compile_dump_ast_semantic_experimental_soa_vector_method_shadow_global_helper_return.prime",
                source);
  const std::string outPath =
      (testScratchPath("") /
       "primec_dump_ast_semantic_experimental_soa_vector_method_shadow_global_helper_return.txt")
          .string();

  const std::string dumpCmd =
      "./primec " + quoteShellArg(srcPath) + " --dump-stage ast-semantic > " + quoteShellArg(outPath);
  CHECK(runCommand(dumpCmd) == 0);
  const std::string ast = readFile(outPath);
  const size_t mainPos = ast.find("/main()");
  CHECK(mainPos != std::string::npos);
  CHECK(ast.find("/soa_vector/count(", mainPos) != std::string::npos);
  CHECK(ast.find("/soa_vector/get(", mainPos) != std::string::npos);
  CHECK(ast.find("/soa_vector/ref(", mainPos) != std::string::npos);
  CHECK(ast.find("/soa_vector/push(", mainPos) != std::string::npos);
  CHECK(ast.find("/soa_vector/reserve(", mainPos) != std::string::npos);
  CHECK(ast.find(".count(", mainPos) == std::string::npos);
  CHECK(ast.find(".get(", mainPos) == std::string::npos);
  CHECK(ast.find(".ref(", mainPos) == std::string::npos);
  CHECK(ast.find(".push(", mainPos) == std::string::npos);
  CHECK(ast.find(".reserve(", mainPos) == std::string::npos);
}

TEST_CASE("dump ast-semantic rewrites method-like helper-return soa_vector method shadows to same-path helpers") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_soa_vector/*

[struct reflect]
Particle() {
  [i32] x{1i32}
}

[struct]
Holder() {}

[return<SoaVector<Particle>>]
/Holder/cloneValues([Holder] self) {
  return(soaVectorSingle<Particle>(Particle(7i32)))
}

[return<int>]
/soa_vector/count([SoaVector<Particle>] values) {
  return(11i32)
}

[return<Particle>]
/soa_vector/get([SoaVector<Particle>] values, [int] index) {
  return(Particle(23i32))
}

[return<Particle>]
/soa_vector/ref([SoaVector<Particle>] values, [int] index) {
  return(Particle(29i32))
}

[return<int>]
/soa_vector/push([SoaVector<Particle>] values, [Particle] value) {
  return(value.x)
}

[return<int>]
/soa_vector/reserve([SoaVector<Particle>] values, [int] count) {
  return(count)
}

[effects(heap_alloc), return<int>]
main() {
  [Holder] holder{Holder()}
  [Particle] value{Particle(31i32)}
  return(plus(holder.cloneValues().count(),
              plus(holder.cloneValues().get(0i32).x,
                   plus(holder.cloneValues().ref(0i32).x,
                        plus(holder.cloneValues().push(value),
                             holder.cloneValues().reserve(37i32))))))
}
)";
  const std::string srcPath =
      writeTemp("compile_dump_ast_semantic_experimental_soa_vector_method_shadow_method_like_helper_return.prime",
                source);
  const std::string outPath =
      (testScratchPath("") /
       "primec_dump_ast_semantic_experimental_soa_vector_method_shadow_method_like_helper_return.txt")
          .string();

  const std::string dumpCmd =
      "./primec " + quoteShellArg(srcPath) + " --dump-stage ast-semantic > " + quoteShellArg(outPath);
  CHECK(runCommand(dumpCmd) == 0);
  const std::string ast = readFile(outPath);
  const size_t mainPos = ast.find("/main()");
  CHECK(mainPos != std::string::npos);
  CHECK(ast.find("/soa_vector/count(/Holder/cloneValues(holder))", mainPos) != std::string::npos);
  CHECK(ast.find("/soa_vector/get(/Holder/cloneValues(holder), 0)", mainPos) != std::string::npos);
  CHECK(ast.find("/soa_vector/ref(/Holder/cloneValues(holder), 0)", mainPos) != std::string::npos);
  CHECK(ast.find("/soa_vector/push(/Holder/cloneValues(holder), value)", mainPos) != std::string::npos);
  CHECK(ast.find("/soa_vector/reserve(/Holder/cloneValues(holder), 37)", mainPos) != std::string::npos);
  CHECK(ast.find(".count(", mainPos) == std::string::npos);
  CHECK(ast.find(".get(", mainPos) == std::string::npos);
  CHECK(ast.find(".ref(", mainPos) == std::string::npos);
  CHECK(ast.find(".push(", mainPos) == std::string::npos);
  CHECK(ast.find(".reserve(", mainPos) == std::string::npos);
}

TEST_CASE("dump ast-semantic accepts nested struct-body soa_vector constructor-bearing helper returns") {
  const std::string source = R"(
import /std/collections/experimental_soa_vector/*

[struct reflect]
Particle() {
  [i32] x{1i32}
}

[struct]
Holder() {
  [return<SoaVector<Particle>>]
  cloneValues() {
    return(soaVectorSingle<Particle>(Particle(7i32)))
  }
}

[effects(heap_alloc), return<int>]
main() {
  return(0i32)
}
)";
  const std::string srcPath =
      writeTemp("compile_dump_ast_semantic_nested_struct_body_soa_vector_constructor_helper.prime",
                source);
  const std::string outPath =
      (testScratchPath("") /
       "primec_dump_ast_semantic_nested_struct_body_soa_vector_constructor_helper.txt")
          .string();

  const std::string dumpCmd =
      "./primec " + quoteShellArg(srcPath) + " --dump-stage ast-semantic > " + quoteShellArg(outPath);
  CHECK(runCommand(dumpCmd) == 0);
  const std::string ast = readFile(outPath);
  CHECK(ast.find("[return</std/collections/experimental_soa_vector/SoaVector__") != std::string::npos);
  CHECK(ast.find("/Holder/cloneValues()") != std::string::npos);
  CHECK(ast.find("return /std/collections/experimental_soa_vector/soaVectorSingle__") != std::string::npos);
  CHECK(ast.find("Particle(7)") != std::string::npos);
}

TEST_CASE("dump ast-semantic rewrites nested struct-body soa_vector method shadows to same-path helpers") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_soa_vector/*

[struct reflect]
Particle() {
  [i32] x{1i32}
}

[struct]
Holder() {
  [return<SoaVector<Particle>>]
  cloneValues() {
    return(soaVectorSingle<Particle>(Particle(7i32)))
  }
}

[return<i32>]
/soa_vector/count([SoaVector<Particle>] values) {
  return(13i32)
}

[return<Particle>]
/soa_vector/get([SoaVector<Particle>] values, [i32] index) {
  return(Particle(23i32))
}

[return<Particle>]
/soa_vector/ref([SoaVector<Particle>] values, [i32] index) {
  return(Particle(29i32))
}

[return<i32>]
/soa_vector/push([SoaVector<Particle>] values, [Particle] value) {
  return(31i32)
}

[return<i32>]
/soa_vector/reserve([SoaVector<Particle>] values, [i32] capacity) {
  return(37i32)
}

[effects(heap_alloc), return<vector<Particle>>]
/to_aos([SoaVector<Particle>] values) {
  [vector<Particle>, mut] out{vector<Particle>()}
  out.push(Particle(19i32))
  return(out)
}

[effects(heap_alloc), return<int>]
main() {
  [Holder] holder{Holder()}
  [vector<Particle>] items{holder.cloneValues().to_aos()}
  return(plus(plus(plus(plus(plus(holder.cloneValues().count(),
                                  holder.cloneValues().get(0i32).x),
                             holder.cloneValues().ref(0i32).x),
                        holder.cloneValues().push(Particle(1i32))),
                   holder.cloneValues().reserve(4i32)),
              1i32))
}
)";
  const std::string srcPath =
      writeTemp("compile_dump_ast_semantic_nested_struct_body_soa_vector_method_shadows.prime",
                source);
  const std::string outPath =
      (testScratchPath("") /
       "primec_dump_ast_semantic_nested_struct_body_soa_vector_method_shadows.txt")
          .string();

  const std::string dumpCmd =
      "./primec " + quoteShellArg(srcPath) + " --dump-stage ast-semantic > " + quoteShellArg(outPath);
  CHECK(runCommand(dumpCmd) == 0);
  const std::string ast = readFile(outPath);
  const size_t mainPos = ast.find("/main()");
  CHECK(mainPos != std::string::npos);
  CHECK(ast.find("/soa_vector/count(/Holder/cloneValues(holder))", mainPos) != std::string::npos);
  CHECK(ast.find("/soa_vector/get(/Holder/cloneValues(holder), 0)", mainPos) != std::string::npos);
  CHECK(ast.find("/soa_vector/ref(/Holder/cloneValues(holder), 0)", mainPos) != std::string::npos);
  CHECK(ast.find("/soa_vector/push(/Holder/cloneValues(holder), Particle(1))", mainPos) != std::string::npos);
  CHECK(ast.find("/soa_vector/reserve(/Holder/cloneValues(holder), 4)", mainPos) != std::string::npos);
  CHECK(ast.find("/to_aos(/Holder/cloneValues(holder))", mainPos) != std::string::npos);
  CHECK(ast.find(".count(", mainPos) == std::string::npos);
  CHECK(ast.find(".get(", mainPos) == std::string::npos);
  CHECK(ast.find(".ref(", mainPos) == std::string::npos);
  CHECK(ast.find(".push(", mainPos) == std::string::npos);
  CHECK(ast.find(".reserve(", mainPos) == std::string::npos);
  CHECK(ast.find(".to_aos(", mainPos) == std::string::npos);
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
  CHECK(ast.find("/std/collections/soa_vector/get_ref__", mainPos) !=
        std::string::npos);
  CHECK(ast.find(".y", mainPos) != std::string::npos);
  CHECK(ast.find("values.y()[", mainPos) == std::string::npos);
}

TEST_CASE("dump ast-semantic rewrites experimental soa_vector mutating field index targets to soaVectorRef") {
  const std::string source = R"(
import /std/collections/*
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
  assign(values.y()[1i32], 17i32)
  assign(y(values)[0i32], 19i32)
  return(plus(values.y()[0i32], values.y()[1i32]))
}
)";
  const std::string srcPath =
      writeTemp("compile_dump_ast_semantic_experimental_soa_vector_mutating_field_view.prime",
                source);
  const std::string outPath =
      (testScratchPath("") /
       "primec_dump_ast_semantic_experimental_soa_vector_mutating_field_view.txt")
          .string();

  const std::string dumpCmd =
      "./primec " + quoteShellArg(srcPath) + " --dump-stage ast-semantic > " + quoteShellArg(outPath);
  CHECK(runCommand(dumpCmd) == 0);
  const std::string ast = readFile(outPath);
  const size_t mainPos = ast.find("/main()");
  CHECK(mainPos != std::string::npos);
  CHECK(ast.find("/std/collections/experimental_soa_vector/soaVectorRef__", mainPos) !=
        std::string::npos);
  CHECK(ast.find("assign(values.y()[1]", mainPos) == std::string::npos);
  CHECK(ast.find("assign(y(values)[0]", mainPos) == std::string::npos);
  CHECK(ast.find(".y", mainPos) != std::string::npos);
}

TEST_CASE("dump ast-semantic rewrites richer borrowed experimental soa_vector mutating field index targets to soaVectorRef") {
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
  values.push(Particle(7i32, 8i32))
  values.push(Particle(9i32, 12i32))
  assign(dereference(pickBorrowed(location(values))).y()[1i32], 17i32)
  assign(y(location(pickBorrowed(location(values))))[0i32], 19i32)
  return(plus(dereference(pickBorrowed(location(values))).y()[1i32],
              y(location(pickBorrowed(location(values))))[0i32]))
}
)";
  const std::string srcPath = writeTemp(
      "compile_dump_ast_semantic_experimental_soa_vector_richer_borrowed_mutating_field_view.prime",
      source);
  const std::string outPath =
      (testScratchPath("") /
       "primec_dump_ast_semantic_experimental_soa_vector_richer_borrowed_mutating_field_view.txt")
          .string();

  const std::string dumpCmd =
      "./primec " + quoteShellArg(srcPath) + " --dump-stage ast-semantic > " + quoteShellArg(outPath);
  CHECK(runCommand(dumpCmd) == 0);
  const std::string ast = readFile(outPath);
  const size_t mainPos = ast.find("/main()");
  CHECK(mainPos != std::string::npos);
  CHECK(ast.find("/std/collections/experimental_soa_vector/soaVectorRef__", mainPos) !=
        std::string::npos);
  CHECK(ast.find("assign(dereference(pickBorrowed(location(values))).y()[1]", mainPos) ==
        std::string::npos);
  CHECK(ast.find("assign(y(location(pickBorrowed(location(values))))[0]", mainPos) ==
        std::string::npos);
  CHECK(ast.find(".y", mainPos) != std::string::npos);
}

TEST_CASE("dump ast-semantic rewrites method-like borrowed experimental soa_vector mutating field index targets to soaVectorRef") {
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
  values.push(Particle(7i32, 8i32))
  values.push(Particle(9i32, 12i32))
  [Holder] holder{Holder()}
  assign(holder.pickBorrowed(location(values)).y()[1i32], 17i32)
  assign(y(holder.pickBorrowed(location(values)))[0i32], 19i32)
  assign(location(holder.pickBorrowed(location(values))).y()[0i32], 23i32)
  assign(y(dereference(location(holder.pickBorrowed(location(values)))))[1i32], 29i32)
  return(
    plus(holder.pickBorrowed(location(values)).y()[0i32],
         plus(y(holder.pickBorrowed(location(values)))[1i32],
              plus(location(holder.pickBorrowed(location(values))).y()[0i32],
                   y(dereference(location(holder.pickBorrowed(location(values)))))[1i32])))
  )
}
)";
  const std::string srcPath = writeTemp(
      "compile_dump_ast_semantic_experimental_soa_vector_method_like_borrowed_mutating_field_view.prime",
      source);
  const std::string outPath =
      (testScratchPath("") /
       "primec_dump_ast_semantic_experimental_soa_vector_method_like_borrowed_mutating_field_view.txt")
          .string();

  const std::string dumpCmd =
      "./primec " + quoteShellArg(srcPath) + " --dump-stage ast-semantic > " + quoteShellArg(outPath);
  CHECK(runCommand(dumpCmd) == 0);
  const std::string ast = readFile(outPath);
  const size_t mainPos = ast.find("/main()");
  CHECK(mainPos != std::string::npos);
  CHECK(ast.find("/std/collections/experimental_soa_vector/soaVectorRef__", mainPos) !=
        std::string::npos);
  CHECK(ast.find("assign(holder.pickBorrowed(location(values)).y()[1]", mainPos) == std::string::npos);
  CHECK(ast.find("assign(y(holder.pickBorrowed(location(values)))[0]", mainPos) == std::string::npos);
  CHECK(ast.find("assign(location(holder.pickBorrowed(location(values))).y()[0]", mainPos) ==
        std::string::npos);
  CHECK(ast.find("assign(y(dereference(location(holder.pickBorrowed(location(values)))))[1]", mainPos) ==
        std::string::npos);
  CHECK(ast.find(".y", mainPos) != std::string::npos);
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
  CHECK(ast.find("/std/collections/soa_vector/get_ref__", mainPos) !=
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
  CHECK(ast.find("/std/collections/soa_vector/get_ref__", mainPos) !=
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
  CHECK(ast.find("/std/collections/soa_vector/get_ref__", mainPos) !=
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
  CHECK(ast.find("/std/collections/soa_vector/get_ref__", mainPos) !=
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
  CHECK(ast.find("/std/collections/soa_vector/get_ref__", mainPos) !=
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
  CHECK(ast.find("/std/collections/soa_vector/get_ref__", mainPos) !=
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
  CHECK(ast.find("/Holder/pickBorrowed(holder, location(values))", mainPos) != std::string::npos);
  CHECK(ast.find("holder.pickBorrowed(location(values)).y()[", mainPos) == std::string::npos);
  CHECK(ast.find("y(holder.pickBorrowed(location(values)))[", mainPos) == std::string::npos);
  CHECK(ast.find("/std/collections/soa_vector/get_ref__", mainPos) !=
        std::string::npos);
  CHECK(ast.find("/std/collections/soa_vector/ref_ref__", mainPos) !=
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
  [Reference<Particle>] secondA{location(holder.pickBorrowed(location(values))).ref(1i32)}
  [vector<Particle>] unpackedA{location(holder.pickBorrowed(location(values))).to_aos()}
  [i32] countA{location(holder.pickBorrowed(location(values))).count()}
  [Particle] firstB{dereference(location(holder.pickBorrowed(location(values)))).get(0i32)}
  [Reference<Particle>] secondB{dereference(location(holder.pickBorrowed(location(values)))).ref(1i32)}
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
  CHECK(ast.find("/Holder/pickBorrowed(holder, location(values))", mainPos) !=
        std::string::npos);
  CHECK(ast.find("/std/collections/soa_vector/count_ref__", mainPos) !=
        std::string::npos);
  CHECK(ast.find("/std/collections/soa_vector/to_aos_ref__", mainPos) !=
        std::string::npos);
  CHECK(ast.find("/std/collections/soa_vector/get_ref__", mainPos) !=
        std::string::npos);
  CHECK(ast.find("/std/collections/soa_vector/ref_ref__", mainPos) !=
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
  CHECK(ast.find("/Holder/pickBorrowed(holder, location(values))", mainPos) !=
        std::string::npos);
  CHECK(ast.find("/std/collections/soa_vector/count_ref__", mainPos) !=
        std::string::npos);
  CHECK(ast.find("/std/collections/soa_vector/to_aos_ref__", mainPos) !=
        std::string::npos);
  CHECK(ast.find("/std/collections/soa_vector/get_ref__", mainPos) !=
        std::string::npos);
  CHECK(ast.find("/std/collections/soa_vector/ref_ref__", mainPos) !=
        std::string::npos);
}

TEST_CASE("dump ast-semantic rewrites direct return borrowed helper-return experimental soa_vector reads") {
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
  return(
    plus(count(pickBorrowed(location(values))),
         plus(count(pickBorrowed(location(values)).to_aos()),
              plus(pickBorrowed(location(values)).get(0i32).x,
                   plus(ref(pickBorrowed(location(values)), 1i32).y,
                        plus(get(pickBorrowed(location(values)), 1i32).y,
                             plus(pickBorrowed(location(values)).y()[1i32],
                                  y(pickBorrowed(location(values)))[0i32]))))))
  )
}
)";
  const std::string srcPath = writeTemp(
      "compile_dump_ast_semantic_direct_return_borrowed_helper_reads.prime",
      source);
  const std::string outPath =
      (testScratchPath("") /
       "primec_dump_ast_semantic_direct_return_borrowed_helper_reads.txt")
          .string();

  const std::string dumpCmd =
      "./primec " + quoteShellArg(srcPath) + " --dump-stage ast-semantic > " + quoteShellArg(outPath);
  CHECK(runCommand(dumpCmd) == 0);
  const std::string ast = readFile(outPath);
  const size_t mainPos = ast.find("/main()");
  CHECK(mainPos != std::string::npos);
  CHECK(ast.find("count(pickBorrowed(location(values)))", mainPos) == std::string::npos);
  CHECK(ast.find("pickBorrowed(location(values)).to_aos()", mainPos) == std::string::npos);
  CHECK(ast.find("get(pickBorrowed(location(values)),", mainPos) == std::string::npos);
  CHECK(ast.find("ref(pickBorrowed(location(values)),", mainPos) == std::string::npos);
  CHECK(ast.find("pickBorrowed(location(values)).y()[", mainPos) == std::string::npos);
  CHECK(ast.find("y(pickBorrowed(location(values)))[", mainPos) == std::string::npos);
  CHECK(ast.find("/pickBorrowed(location(values))", mainPos) != std::string::npos);
  CHECK(ast.find("/std/collections/soa_vector/count_ref__", mainPos) !=
        std::string::npos);
  CHECK(ast.find("/std/collections/soa_vector/to_aos_ref__", mainPos) !=
        std::string::npos);
  CHECK(ast.find("/std/collections/soa_vector/get_ref__", mainPos) !=
        std::string::npos);
  CHECK(ast.find("/std/collections/soa_vector/ref_ref__", mainPos) !=
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
  CHECK(ast.find("/Holder/pickBorrowed(holder, location(values))", mainPos) !=
        std::string::npos);
  CHECK(ast.find("/std/collections/soa_vector/count_ref__", mainPos) !=
        std::string::npos);
  CHECK(ast.find("/std/collections/soa_vector/to_aos_ref__", mainPos) !=
        std::string::npos);
  CHECK(ast.find("/std/collections/soa_vector/get_ref__", mainPos) !=
        std::string::npos);
  CHECK(ast.find("/std/collections/soa_vector/ref_ref__", mainPos) !=
        std::string::npos);
}

TEST_CASE("dump ast-semantic rewrites direct return inline location borrowed helper-return experimental soa_vector reads") {
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
  return(
    plus(location(pickBorrowed(location(values))).count(),
         plus(count(location(pickBorrowed(location(values))).to_aos()),
              plus(dereference(location(pickBorrowed(location(values)))).get(1i32).x,
                   plus(ref(dereference(location(pickBorrowed(location(values)))), 0i32).x,
                        plus(get(location(pickBorrowed(location(values))), 1i32).y,
                             plus(location(pickBorrowed(location(values))).y()[0i32],
                                  y(dereference(location(pickBorrowed(location(values)))))[1i32]))))))
  )
}
)";
  const std::string srcPath = writeTemp(
      "compile_dump_ast_semantic_direct_return_inline_location_borrowed_helper_reads.prime",
      source);
  const std::string outPath =
      (testScratchPath("") /
       "primec_dump_ast_semantic_direct_return_inline_location_borrowed_helper_reads.txt")
          .string();

  const std::string dumpCmd =
      "./primec " + quoteShellArg(srcPath) + " --dump-stage ast-semantic > " + quoteShellArg(outPath);
  CHECK(runCommand(dumpCmd) == 0);
  const std::string ast = readFile(outPath);
  const size_t mainPos = ast.find("/main()");
  CHECK(mainPos != std::string::npos);
  CHECK(ast.find("location(pickBorrowed(location(values))).count()", mainPos) == std::string::npos);
  CHECK(ast.find("location(pickBorrowed(location(values))).to_aos()", mainPos) == std::string::npos);
  CHECK(ast.find("dereference(location(pickBorrowed(location(values)))).get(", mainPos) ==
        std::string::npos);
  CHECK(ast.find("get(location(pickBorrowed(location(values))),", mainPos) == std::string::npos);
  CHECK(ast.find("ref(dereference(location(pickBorrowed(location(values)))),", mainPos) ==
        std::string::npos);
  CHECK(ast.find("location(pickBorrowed(location(values))).y()[", mainPos) == std::string::npos);
  CHECK(ast.find("y(dereference(location(pickBorrowed(location(values)))))[", mainPos) ==
        std::string::npos);
  CHECK(ast.find("/pickBorrowed(location(values))", mainPos) != std::string::npos);
  CHECK(ast.find("/std/collections/soa_vector/count_ref__", mainPos) !=
        std::string::npos);
  CHECK(ast.find("/std/collections/soa_vector/to_aos_ref__", mainPos) !=
        std::string::npos);
  CHECK(ast.find("/std/collections/soa_vector/get_ref__", mainPos) !=
        std::string::npos);
  CHECK(ast.find("/std/collections/soa_vector/ref_ref__", mainPos) !=
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
  [vector<Particle>] unpackedB{values.to_aos()}
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
  CHECK(ast.find("values.to_aos()", mainPos) == std::string::npos);
}

TEST_CASE("dump ast-semantic rewrites no-import builtin soa_vector to_aos forms to canonical helper path") {
  const std::string source = R"(
[struct reflect]
Particle() {
  [i32] x{1i32}
}

[return<int>]
main() {
  [soa_vector<Particle>] values{soa_vector<Particle>()}
  [vector<Particle>] unpackedA{to_aos(values)}
  [vector<Particle>] unpackedB{values.to_aos()}
  return(0i32)
}
)";
  const std::string srcPath =
      writeTemp("compile_dump_ast_semantic_root_builtin_soa_vector_to_aos.prime", source);
  const std::string outPath =
      (testScratchPath("") / "primec_dump_ast_semantic_root_builtin_soa_vector_to_aos.txt").string();

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
  CHECK(ast.find("/to_aos(values)", mainPos) == std::string::npos);
  CHECK(ast.find("values.to_aos()", mainPos) == std::string::npos);
}

TEST_CASE("dump ast-semantic rewrites vector-target helper-shadowed to_aos method forms to direct helper path") {
  const std::string source = R"(
Particle() {
  [i32] x{1i32}
}

[return<int>]
/to_aos([vector<Particle>] values) {
  return(9i32)
}

[return<int>]
main() {
  [vector<Particle>] values{vector<Particle>()}
  [int] bare{to_aos(values)}
  [int] direct{/to_aos(values)}
  [int] method{values.to_aos()}
  [int] slash{values./to_aos()}
  return(0i32)
}
)";
  const std::string srcPath =
      writeTemp("compile_dump_ast_semantic_vector_target_to_aos_shadow.prime", source);
  const std::string outPath =
      (testScratchPath("") / "primec_dump_ast_semantic_vector_target_to_aos_shadow.txt").string();

  const std::string dumpCmd =
      "./primec " + quoteShellArg(srcPath) + " --dump-stage ast-semantic > " + quoteShellArg(outPath);
  CHECK(runCommand(dumpCmd) == 0);
  const std::string ast = readFile(outPath);
  const size_t mainPos = ast.find("/main()");
  CHECK(mainPos != std::string::npos);
  CHECK(ast.find("[int] bare{to_aos(values)}", mainPos) != std::string::npos);
  CHECK(ast.find("[int] direct{/to_aos(values)}", mainPos) != std::string::npos);
  CHECK(ast.find("[int] method{/to_aos(values)}", mainPos) != std::string::npos);
  CHECK(ast.find("[int] slash{/to_aos(values)}", mainPos) != std::string::npos);
  CHECK(ast.find("values.to_aos()", mainPos) == std::string::npos);
  CHECK(ast.find("values./to_aos()", mainPos) == std::string::npos);
}

TEST_CASE("dump ast-semantic rewrites vector-target old-explicit mutator shadows to direct helper path") {
  const std::string source = R"(
[return<int>]
/soa_vector/push([vector<i32>] values, [i32] value) {
  return(value)
}

[return<int>]
/soa_vector/reserve([vector<i32>] values, [i32] count) {
  return(count)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32, 3i32)}
  [int] pushed{values./soa_vector/push(4i32)}
  [int] reserved{values./soa_vector/reserve(6i32)}
  return(plus(pushed, reserved))
}
)";
  const std::string srcPath =
      writeTemp("compile_dump_ast_semantic_vector_target_soa_mutator_shadow.prime", source);
  const std::string outPath =
      (testScratchPath("") / "primec_dump_ast_semantic_vector_target_soa_mutator_shadow.txt").string();

  const std::string dumpCmd =
      "./primec " + quoteShellArg(srcPath) + " --dump-stage ast-semantic > " + quoteShellArg(outPath);
  CHECK(runCommand(dumpCmd) == 0);
  const std::string ast = readFile(outPath);
  const size_t mainPos = ast.find("/main()");
  CHECK(mainPos != std::string::npos);
  CHECK(ast.find("[int] pushed{/soa_vector/push(values, 4)}", mainPos) != std::string::npos);
  CHECK(ast.find("[int] reserved{/soa_vector/reserve(values, 6)}", mainPos) != std::string::npos);
  CHECK(ast.find("values./soa_vector/push(4)", mainPos) == std::string::npos);
  CHECK(ast.find("values./soa_vector/reserve(6)", mainPos) == std::string::npos);
}

TEST_CASE("dump ast-semantic rewrites vector-target method mutator shadows to direct helper path") {
  const std::string source = R"(
[return<int>]
/soa_vector/push([vector<i32>] values, [i32] value) {
  return(value)
}

[return<int>]
/soa_vector/reserve([vector<i32>] values, [i32] count) {
  return(count)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32, 3i32)}
  [int] pushed{values.push(4i32)}
  [int] reserved{values.reserve(6i32)}
  return(plus(pushed, reserved))
}
)";
  const std::string srcPath =
      writeTemp("compile_dump_ast_semantic_vector_target_soa_mutator_method_shadow.prime", source);
  const std::string outPath =
      (testScratchPath("") / "primec_dump_ast_semantic_vector_target_soa_mutator_method_shadow.txt").string();

  const std::string dumpCmd =
      "./primec " + quoteShellArg(srcPath) + " --dump-stage ast-semantic > " + quoteShellArg(outPath);
  CHECK(runCommand(dumpCmd) == 0);
  const std::string ast = readFile(outPath);
  const size_t mainPos = ast.find("/main()");
  CHECK(mainPos != std::string::npos);
  CHECK(ast.find("[int] pushed{/soa_vector/push(values, 4)}", mainPos) != std::string::npos);
  CHECK(ast.find("[int] reserved{/soa_vector/reserve(values, 6)}", mainPos) != std::string::npos);
  CHECK(ast.find("values.push(4)", mainPos) == std::string::npos);
  CHECK(ast.find("values.reserve(6)", mainPos) == std::string::npos);
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

TEST_CASE("dump ast-semantic canonical soa_vector to_aos helper body uses canonical count/get loop") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/soa_vector/*
import /std/collections/soa_vector_conversions/*

[struct reflect]
Particle() {
  [i32] x{1i32}
}

[effects(heap_alloc), return<int>]
main() {
  [auto] values{soaVectorSingle<Particle>(Particle(7i32))}
  [vector<Particle>] unpacked{/std/collections/soa_vector/to_aos<Particle>(values)}
  return(count(unpacked))
}
)";
  const std::string srcPath =
      writeTemp("compile_dump_ast_semantic_canonical_soa_vector_to_aos_body.prime", source);
  const std::string outPath =
      (testScratchPath("") / "primec_dump_ast_semantic_canonical_soa_vector_to_aos_body.txt").string();

  const std::string dumpCmd =
      "./primec " + quoteShellArg(srcPath) + " --dump-stage ast-semantic > " + quoteShellArg(outPath);
  CHECK(runCommand(dumpCmd) == 0);
  const std::string ast = readFile(outPath);
  const size_t helperPos = ast.find("/std/collections/soa_vector/to_aos__");
  const size_t mainPos = ast.find("/main()");
  REQUIRE(helperPos != std::string::npos);
  REQUIRE(mainPos != std::string::npos);
  REQUIRE(helperPos < mainPos);
  const std::string helperBlock = ast.substr(helperPos, mainPos - helperPos);
  CHECK(helperBlock.find("/std/collections/soa_vector/count__") != std::string::npos);
  CHECK(helperBlock.find("/std/collections/soa_vector/get__") != std::string::npos);
  CHECK(helperBlock.find("/std/collections/experimental_soa_vector_conversions/soaVectorToAos__") ==
        std::string::npos);
}

TEST_CASE("dump ast-semantic canonical soa_vector to_aos_ref helper body uses canonical count_ref/get_ref loop") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/soa_vector/*
import /std/collections/soa_vector_conversions/*

[struct reflect]
Particle() {
  [i32] x{1i32}
}

[effects(heap_alloc), return<int>]
main() {
  [auto mut] values{soaVectorNew<Particle>()}
  values.push(Particle(7i32))
  [vector<Particle>] unpacked{/std/collections/soa_vector/to_aos_ref<Particle>(location(values))}
  return(count(unpacked))
}
)";
  const std::string srcPath =
      writeTemp("compile_dump_ast_semantic_canonical_soa_vector_to_aos_ref_body.prime", source);
  const std::string outPath =
      (testScratchPath("") / "primec_dump_ast_semantic_canonical_soa_vector_to_aos_ref_body.txt").string();

  const std::string dumpCmd =
      "./primec " + quoteShellArg(srcPath) + " --dump-stage ast-semantic > " + quoteShellArg(outPath);
  CHECK(runCommand(dumpCmd) == 0);
  const std::string ast = readFile(outPath);
  const size_t helperPos = ast.find("/std/collections/soa_vector/to_aos_ref__");
  const size_t mainPos = ast.find("/main()");
  REQUIRE(helperPos != std::string::npos);
  REQUIRE(mainPos != std::string::npos);
  REQUIRE(helperPos < mainPos);
  const std::string helperBlock = ast.substr(helperPos, mainPos - helperPos);
  CHECK(helperBlock.find("/std/collections/soa_vector/count_ref__") != std::string::npos);
  CHECK(helperBlock.find("/std/collections/soa_vector/get_ref__") != std::string::npos);
  CHECK(helperBlock.find("/std/collections/soa_vector/to_aos__") == std::string::npos);
  CHECK(helperBlock.find("/std/collections/experimental_soa_vector_conversions/soaVectorToAosRef__") ==
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
  CHECK(ast.find("/std/collections/soa_vector/to_aos_ref__", mainPos) !=
        std::string::npos);
  CHECK(ast.find("pickBorrowed(location(values)).to_aos()", mainPos) == std::string::npos);
}

TEST_CASE("dump ast-semantic rewrites borrowed helper-return experimental soa_vector to_aos_ref via canonical helper") {
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
  [vector<Particle>] unpacked{pickBorrowed(location(values)).to_aos_ref<Particle>()}
  return(count(unpacked))
}
)";
  const std::string srcPath =
      writeTemp("compile_dump_ast_semantic_borrowed_return_experimental_soa_vector_to_aos_ref.prime", source);
  const std::string outPath =
      (testScratchPath("") / "primec_dump_ast_semantic_borrowed_return_experimental_soa_vector_to_aos_ref.txt")
          .string();

  const std::string dumpCmd =
      "./primec " + quoteShellArg(srcPath) + " --dump-stage ast-semantic > " + quoteShellArg(outPath);
  CHECK(runCommand(dumpCmd) == 0);
  const std::string ast = readFile(outPath);
  const size_t mainPos = ast.find("/main()");
  CHECK(mainPos != std::string::npos);
  CHECK(ast.find("/std/collections/soa_vector/to_aos_ref__", mainPos) != std::string::npos);
  CHECK(ast.find("/std/collections/experimental_soa_vector_conversions/soaVectorToAosRef__", mainPos) ==
        std::string::npos);
  CHECK(ast.find("pickBorrowed(location(values)).to_aos_ref<Particle>()", mainPos) == std::string::npos);
}

TEST_CASE("dump ast-semantic keeps helper-return experimental soa_vector to_aos with same-path helper") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_soa_vector/*

[struct reflect]
Particle() {
  [i32] x{1i32}
}

Holder() {}

[return<SoaVector<Particle>>]
/Holder/cloneValues([Holder] self) {
  return(soaVectorNew<Particle>())
}

[return<int>]
/to_aos([SoaVector<Particle>] values) {
  return(7i32)
}

[return<int>]
main() {
  [Holder] holder{Holder()}
  [auto] item{holder.cloneValues().to_aos()}
  return(item)
}
)";
  const std::string srcPath =
      writeTemp("compile_dump_ast_semantic_helper_return_experimental_soa_vector_to_aos_shadow.prime", source);
  const std::string outPath =
      (testScratchPath("") /
       "primec_dump_ast_semantic_helper_return_experimental_soa_vector_to_aos_shadow.txt")
          .string();

  const std::string dumpCmd =
      "./primec " + quoteShellArg(srcPath) + " --dump-stage ast-semantic > " + quoteShellArg(outPath);
  CHECK(runCommand(dumpCmd) == 0);
  const std::string ast = readFile(outPath);
  const size_t mainPos = ast.find("/main()");
  CHECK(mainPos != std::string::npos);
  CHECK(ast.find("/std/collections/experimental_soa_vector_conversions/soaVectorToAos__", mainPos) ==
        std::string::npos);
  CHECK(ast.find("/to_aos(/Holder/cloneValues(holder))", mainPos) != std::string::npos);
  CHECK(ast.find("holder.cloneValues().to_aos()", mainPos) == std::string::npos);
}

TEST_CASE("dump ast-semantic keeps helper-return builtin soa_vector to_aos with same-path helper") {
  const std::string source = R"(
Particle() {
  [i32] x{1i32}
}

Holder() {}

[return<soa_vector<Particle>>]
/Holder/cloneValues([Holder] self) {
  return(soa_vector<Particle>())
}

[return<int>]
/to_aos([soa_vector<Particle>] values) {
  return(9i32)
}

[return<int>]
main() {
  [Holder] holder{Holder()}
  [auto] itemA{to_aos(holder.cloneValues())}
  [auto] itemB{/to_aos(holder.cloneValues())}
  [auto] itemC{holder.cloneValues().to_aos()}
  [auto] itemD{holder.cloneValues()./to_aos()}
  return(plus(plus(itemA, itemB), plus(itemC, itemD)))
}
)";
  const std::string srcPath =
      writeTemp("compile_dump_ast_semantic_helper_return_builtin_soa_vector_to_aos_shadow.prime", source);
  const std::string outPath =
      (testScratchPath("") /
       "primec_dump_ast_semantic_helper_return_builtin_soa_vector_to_aos_shadow.txt")
          .string();

  const std::string dumpCmd =
      "./primec " + quoteShellArg(srcPath) + " --dump-stage ast-semantic > " + quoteShellArg(outPath);
  CHECK(runCommand(dumpCmd) == 0);
  const std::string ast = readFile(outPath);
  const size_t mainPos = ast.find("/main()");
  CHECK(mainPos != std::string::npos);
  CHECK(ast.find("/std/collections/soa_vector/to_aos", mainPos) == std::string::npos);
  CHECK(ast.find("/to_aos(holder.cloneValues())", mainPos) != std::string::npos);
  CHECK(ast.find("[auto] itemA{/to_aos(holder.cloneValues())}", mainPos) != std::string::npos);
  CHECK(ast.find("[auto] itemB{/to_aos(holder.cloneValues())}", mainPos) != std::string::npos);
  CHECK(ast.find("[auto] itemC{/to_aos(holder.cloneValues())}", mainPos) != std::string::npos);
  CHECK(ast.find("[auto] itemD{/to_aos(holder.cloneValues())}", mainPos) != std::string::npos);
}

TEST_CASE("dump ast-semantic rewrites global helper-return builtin soa_vector reads to canonical helpers") {
  const std::string source = R"(
Particle() {
  [i32] x{1i32}
}

[effects(heap_alloc), return<soa_vector<Particle>>]
cloneValues() {
  [soa_vector<Particle>, mut] values{soa_vector<Particle>()}
  values.push(Particle(7i32))
  return(values)
}

[effects(heap_alloc), return<int>]
main() {
  return(plus(count(cloneValues()),
              plus(cloneValues().count(),
                   plus(get(cloneValues(), 0i32).x,
                        plus(cloneValues().get(0i32).x,
                             plus(ref(cloneValues(), 0i32).x,
                                  cloneValues().ref(0i32).x))))))
}
)";
  const std::string srcPath =
      writeTemp("compile_dump_ast_semantic_builtin_soa_vector_global_helper_return_reads.prime", source);
  const std::string outPath =
      (testScratchPath("") /
       "primec_dump_ast_semantic_builtin_soa_vector_global_helper_return_reads.txt")
          .string();

  const std::string dumpCmd =
      "./primec " + quoteShellArg(srcPath) + " --dump-stage ast-semantic > " + quoteShellArg(outPath);
  CHECK(runCommand(dumpCmd) == 0);
  const std::string ast = readFile(outPath);
  const size_t mainPos = ast.find("/main()");
  CHECK(mainPos != std::string::npos);
  CHECK(ast.find("/std/collections/soa_vector/count", mainPos) != std::string::npos);
  CHECK(ast.find("/std/collections/soa_vector/get", mainPos) != std::string::npos);
  CHECK(ast.find("/std/collections/soa_vector/ref", mainPos) != std::string::npos);
  CHECK(ast.find("count(cloneValues())", mainPos) == std::string::npos);
  CHECK(ast.find("cloneValues().count()", mainPos) == std::string::npos);
  CHECK(ast.find("get(cloneValues(), 0)", mainPos) == std::string::npos);
  CHECK(ast.find("cloneValues().get(0)", mainPos) == std::string::npos);
  CHECK(ast.find("ref(cloneValues(), 0)", mainPos) == std::string::npos);
  CHECK(ast.find("cloneValues().ref(0)", mainPos) == std::string::npos);
}

TEST_CASE("dump ast-semantic rewrites method-like helper-return builtin soa_vector reads to canonical helpers") {
  const std::string source = R"(
Particle() {
  [i32] x{1i32}
}

Holder() {}

[effects(heap_alloc), return<soa_vector<Particle>>]
/Holder/cloneValues([Holder] self) {
  [soa_vector<Particle>, mut] values{soa_vector<Particle>()}
  values.push(Particle(7i32))
  return(values)
}

[effects(heap_alloc), return<int>]
main() {
  [Holder] holder{Holder()}
  return(plus(count(holder.cloneValues()),
              plus(holder.cloneValues().count(),
                   plus(get(holder.cloneValues(), 0i32).x,
                        plus(holder.cloneValues().get(0i32).x,
                             plus(ref(holder.cloneValues(), 0i32).x,
                                  holder.cloneValues().ref(0i32).x))))))
}
)";
  const std::string srcPath =
      writeTemp("compile_dump_ast_semantic_builtin_soa_vector_method_like_helper_return_reads.prime", source);
  const std::string outPath =
      (testScratchPath("") /
       "primec_dump_ast_semantic_builtin_soa_vector_method_like_helper_return_reads.txt")
          .string();

  const std::string dumpCmd =
      "./primec " + quoteShellArg(srcPath) + " --dump-stage ast-semantic > " + quoteShellArg(outPath);
  CHECK(runCommand(dumpCmd) == 0);
  const std::string ast = readFile(outPath);
  const size_t mainPos = ast.find("/main()");
  CHECK(mainPos != std::string::npos);
  CHECK(ast.find("/std/collections/soa_vector/count<Particle>(holder.cloneValues())", mainPos) !=
        std::string::npos);
  CHECK(ast.find("/std/collections/soa_vector/get<Particle>(holder.cloneValues(), 0).x", mainPos) !=
        std::string::npos);
  CHECK(ast.find("/std/collections/soa_vector/ref<Particle>(holder.cloneValues(), 0).x", mainPos) !=
        std::string::npos);
  CHECK(ast.find("count(holder.cloneValues())", mainPos) == std::string::npos);
  CHECK(ast.find("holder.cloneValues().count()", mainPos) == std::string::npos);
  CHECK(ast.find("get(holder.cloneValues(), 0)", mainPos) == std::string::npos);
  CHECK(ast.find("holder.cloneValues().get(0)", mainPos) == std::string::npos);
  CHECK(ast.find("ref(holder.cloneValues(), 0)", mainPos) == std::string::npos);
  CHECK(ast.find("holder.cloneValues().ref(0)", mainPos) == std::string::npos);
}

TEST_CASE("dump ast-semantic keeps borrowed soa_vector ref_ref same-path helper shadows") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_soa_vector/*

[struct reflect]
Particle() {
  [i32] x{1i32}
}

[return<Reference<SoaVector<Particle>>>]
pickBorrowed([Reference<SoaVector<Particle>>] values) {
  return(values)
}

[return<int>]
/soa_vector/ref_ref([Reference<SoaVector<Particle>>] values, [int] index) {
  return(41i32)
}

[effects(heap_alloc), return<int>]
main() {
  [SoaVector<Particle> mut] values{soaVectorNew<Particle>()}
  return(plus(pickBorrowed(location(values)).ref(0i32),
              ref_ref(pickBorrowed(location(values)), 0i32)))
}
)";
  const std::string srcPath =
      writeTemp("compile_dump_ast_semantic_borrowed_soa_vector_ref_ref_same_path.prime",
                source);
  const std::string outPath =
      (testScratchPath("") /
       "primec_dump_ast_semantic_borrowed_soa_vector_ref_ref_same_path.txt")
          .string();

  const std::string dumpCmd =
      "./primec " + quoteShellArg(srcPath) + " --dump-stage ast-semantic > " + quoteShellArg(outPath);
  CHECK(runCommand(dumpCmd) == 0);
  const std::string ast = readFile(outPath);
  const size_t mainPos = ast.find("/main()");
  CHECK(mainPos != std::string::npos);
  CHECK(ast.find("/soa_vector/ref_ref(pickBorrowed(location(values)), 0)", mainPos) !=
        std::string::npos);
  CHECK(ast.find("pickBorrowed(location(values)).ref(0)", mainPos) == std::string::npos);
  CHECK(ast.find("return plus(ref_ref(", mainPos) == std::string::npos);
  CHECK(ast.find("/std/collections/soa_vector/ref_ref", mainPos) == std::string::npos);
}

TEST_CASE("dump ast-semantic keeps builtin soa_vector ref_ref same-path helper shadows") {
  const std::string source = R"(
import /std/collections/*

[struct reflect]
Particle() {
  [i32] x{1i32}
}

[effects(heap_alloc), return<soa_vector<Particle>>]
cloneValues() {
  return(soa_vector<Particle>())
}

[effects(heap_alloc), return<int>]
/soa_vector/ref_ref([soa_vector<Particle>] values, [vector<i32>] index) {
  return(17i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] idx{vector<i32>(0i32)}
  [soa_vector<Particle>] values{cloneValues()}
  [auto] direct{ref_ref(values, idx)}
  [auto] method{values.ref_ref(idx)}
  [auto] helperReturn{ref_ref(cloneValues(), idx)}
  return(plus(direct, plus(method, helperReturn)))
}
)";
  const std::string srcPath =
      writeTemp("compile_dump_ast_semantic_builtin_soa_vector_ref_ref_same_path.prime",
                source);
  const std::string outPath =
      (testScratchPath("") /
       "primec_dump_ast_semantic_builtin_soa_vector_ref_ref_same_path.txt")
          .string();

  const std::string dumpCmd =
      "./primec " + quoteShellArg(srcPath) + " --dump-stage ast-semantic > " + quoteShellArg(outPath);
  CHECK(runCommand(dumpCmd) == 0);
  const std::string ast = readFile(outPath);
  const size_t mainPos = ast.find("/main()");
  CHECK(mainPos != std::string::npos);
  CHECK(ast.find("[auto] direct{/soa_vector/ref_ref(values, idx)}", mainPos) !=
        std::string::npos);
  CHECK(ast.find("[auto] method{/soa_vector/ref_ref(values, idx)}", mainPos) !=
        std::string::npos);
  CHECK(ast.find("[auto] helperReturn{/soa_vector/ref_ref(cloneValues(), idx)}",
                 mainPos) != std::string::npos);
  CHECK(ast.find("values.ref_ref(idx)", mainPos) == std::string::npos);
  CHECK(ast.find("[auto] direct{ref_ref(values, idx)}", mainPos) == std::string::npos);
  CHECK(ast.find("[auto] helperReturn{ref_ref(cloneValues(), idx)}", mainPos) ==
        std::string::npos);
  CHECK(ast.find("/std/collections/soa_vector/ref_ref", mainPos) == std::string::npos);
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
  [Reference<Particle>] secondA{location(values).ref(1i32)}
  [vector<Particle>] unpackedA{location(values).to_aos()}
  [i32] countA{location(values).count()}
  [Particle] firstB{dereference(location(values)).get(0i32)}
  [Reference<Particle>] secondB{dereference(location(values)).ref(1i32)}
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
  [Reference<Particle>] secondA{location(pickBorrowed(location(values))).ref(1i32)}
  [vector<Particle>] unpackedA{location(pickBorrowed(location(values))).to_aos()}
  [i32] countA{location(pickBorrowed(location(values))).count()}
  [Particle] firstB{dereference(location(pickBorrowed(location(values)))).get(0i32)}
  [Reference<Particle>] secondB{dereference(location(pickBorrowed(location(values)))).ref(1i32)}
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
  CHECK(ast.find("/pickBorrowed(location(values))", mainPos) != std::string::npos);
  CHECK(ast.find("/std/collections/soa_vector/count_ref__", mainPos) !=
        std::string::npos);
  CHECK(ast.find("/std/collections/soa_vector/to_aos_ref__", mainPos) !=
        std::string::npos);
  CHECK(ast.find("/std/collections/soa_vector/get_ref__", mainPos) !=
        std::string::npos);
  CHECK(ast.find("/std/collections/soa_vector/ref_ref__", mainPos) !=
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

TEST_CASE("dump semantic_product alias works and prints semantic output") {
  const std::string source = R"(
import /std/collections/*

[return<T>]
id<T>([T] value) {
  return(value)
}

[return<int>]
main() {
  [vector<i32>] values{vector<i32>()}
  return(id(values.count()))
}
)";
  const std::string srcPath = writeTemp("compile_dump_semantic_product_alias.prime", source);
  const std::string hyphenOut =
      (testScratchPath("") / "primec_dump_semantic_product_hyphen.txt").string();
  const std::string underscoreOut =
      (testScratchPath("") / "primec_dump_semantic_product_underscore.txt").string();

  const std::string hyphenCmd =
      "./primec " + quoteShellArg(srcPath) + " --dump-stage semantic-product > " + quoteShellArg(hyphenOut);
  const std::string underscoreCmd =
      "./primec " + quoteShellArg(srcPath) + " --dump-stage semantic_product > " + quoteShellArg(underscoreOut);
  CHECK(runCommand(hyphenCmd) == 0);
  CHECK(runCommand(underscoreCmd) == 0);

  const std::string dump = readFile(hyphenOut);
  CHECK(dump == readFile(underscoreOut));
  CHECK(dump.find("semantic_product {") != std::string::npos);
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

TEST_CASE("primec and primevm dump semantic-product match") {
  const std::string source = R"(
import /std/collections/*

[return<T>]
id<T>([T] value) {
  return(value)
}

[return<int>]
main() {
  [vector<i32>] values{vector<i32>()}
  return(id(values.count()))
}
)";
  const std::string srcPath = writeTemp("compile_dump_shared_semantic_product.prime", source);
  const std::string primecOut =
      (testScratchPath("") / "primec_dump_shared_semantic_product.txt").string();
  const std::string primevmOut =
      (testScratchPath("") / "primevm_dump_shared_semantic_product.txt").string();

  const std::string primecCmd =
      "./primec " + quoteShellArg(srcPath) + " --dump-stage semantic-product > " + quoteShellArg(primecOut);
  const std::string primevmCmd =
      "./primevm " + quoteShellArg(srcPath) + " --dump-stage semantic-product > " + quoteShellArg(primevmOut);
  CHECK(runCommand(primecCmd) == 0);
  CHECK(runCommand(primevmCmd) == 0);
  CHECK(readFile(primecOut) == readFile(primevmOut));
}

TEST_CASE("semantic-product dump keeps provenance handles while ast-semantic keeps syntax") {
  const std::string source =
      "Packet {\n"
      "  [i32] left{1i32}\n"
      "  [i32] right{2i32}\n"
      "}\n"
      "\n"
      "[return<i32>]\n"
      "pick([i32] value) {\n"
      "  return(value)\n"
      "}\n"
      "\n"
      "[return<i32>]\n"
      "main() {\n"
      "  [Packet] packet{Packet(3i32, 4i32)}\n"
      "  [i32] selected{pick(packet.left)}\n"
      "  return(selected)\n"
      "}\n";
  primec::testing::CompilePipelineBoundaryDumps dumps;
  std::string error;
  REQUIRE(primec::testing::captureSemanticBoundaryDumpsForTesting(source, "/main", dumps, error));
  CHECK(error.empty());

  CHECK(dumps.astSemantic.find("left{1}") != std::string::npos);
  CHECK(dumps.astSemantic.find("return(selected)") != std::string::npos);

  CHECK(dumps.semanticProduct.find("semantic_product {") != std::string::npos);
  CHECK(dumps.semanticProduct.find("struct_field_metadata[0]: struct_path=\"/Packet\" field_name=\"left\"") !=
        std::string::npos);
  CHECK(dumps.semanticProduct.find("binding_facts[0]: scope_path=\"/main\" site_kind=\"local\" name=\"packet\"") !=
        std::string::npos);
  CHECK(dumps.semanticProduct.find("provenance_handle=") != std::string::npos);
  CHECK(dumps.semanticProduct.find("source=\"2:") != std::string::npos);
  CHECK(dumps.semanticProduct.find("left{1}") == std::string::npos);
  CHECK(dumps.semanticProduct.find("return(selected)") == std::string::npos);
}

TEST_CASE("pipeline dump surfaces keep inspection order and lowering-facing boundaries") {
  const std::string source =
      "Packet {\n"
      "  [i32] left{1i32}\n"
      "  [i32] right{2i32}\n"
      "}\n"
      "\n"
      "import /std/collections/*\n"
      "\n"
      "[return<T>]\n"
      "id<T>([T] value) {\n"
      "  return(value)\n"
      "}\n"
      "\n"
      "[return<i32>]\n"
      "main() {\n"
      "  [Packet] packet{Packet(3i32, 4i32)}\n"
      "  [vector<i32>] values{vector<i32>()}\n"
      "  [i32] selected{id(packet.left + values.count())}\n"
      "  return(selected)\n"
      "}\n";

  primec::testing::CompilePipelineBoundaryDumps dumps;
  std::string error;
  REQUIRE(primec::testing::captureSemanticBoundaryDumpsForTesting(source, "/main", dumps, error));
  CHECK(error.empty());

  CHECK(dumps.astSemantic.find("left{1}") != std::string::npos);
  CHECK(dumps.astSemantic.find("return(selected)") != std::string::npos);

  CHECK(dumps.semanticProduct.find("semantic_product {") != std::string::npos);
  CHECK(dumps.semanticProduct.find("direct_call_targets[") != std::string::npos);
  CHECK(dumps.semanticProduct.find("method_call_targets[") != std::string::npos);
  CHECK(dumps.semanticProduct.find("bridge_path_choices[") != std::string::npos);
  CHECK(dumps.semanticProduct.find("binding_facts[") != std::string::npos);
  CHECK(dumps.semanticProduct.find("return_facts[") != std::string::npos);
  CHECK(dumps.semanticProduct.find("left{1}") == std::string::npos);
  CHECK(dumps.semanticProduct.find("return(selected)") == std::string::npos);

  CHECK(dumps.ir.find("module {") != std::string::npos);
  CHECK(dumps.ir.find("def /main(): i32") != std::string::npos);
  CHECK(dumps.ir.find("semantic_product {") == std::string::npos);
  CHECK(dumps.ir.find("binding_facts[") == std::string::npos);
  CHECK(dumps.ir.find("left{1}") == std::string::npos);
  CHECK(dumps.ir.find("return(selected)") == std::string::npos);
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

TEST_SUITE_END();
