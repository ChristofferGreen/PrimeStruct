#include "../test_compile_run_helpers.h"

#include "../test_compile_run_collection_conformance_helpers.h"
#include "../test_compile_run_container_error_conformance_helpers.h"
#include "../test_compile_run_checked_pointer_conformance_helpers.h"
#include "../test_compile_run_unchecked_pointer_conformance_helpers.h"

TEST_SUITE_BEGIN("primestruct.compile.run.imports");

static void expect_soa_helper_return_shadow_reject(const std::string &source,
                                                          const std::string &nameStem,
                                                          const std::string &emitMode,
                                                          const std::string &expectedDiagnostic) {
  const std::string srcPath = writeTemp(nameStem + ".prime", source);
  const std::string outPath =
      (testScratchPath("") / (nameStem + "_" + emitMode + "_out.txt")).string();
  const std::string artifactPath =
      (testScratchPath("") / (nameStem + "_" + emitMode + "_artifact")).string();

  const std::string compileCmd = "./primec --emit=" + emitMode + " " + quoteShellArg(srcPath) +
                                 " -o " + quoteShellArg(artifactPath) + " --entry /main > " +
                                 quoteShellArg(outPath) + " 2>&1";
  const int compileResult = runCommand(compileCmd);
  INFO(readFile(outPath));
  REQUIRE(compileResult == 2);
  CHECK(readFile(outPath).find(expectedDiagnostic) != std::string::npos);
}

TEST_CASE("runs collection literals with map at in C++ emitter") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  return(plus(at_unsafe(array<i32>{1i32, 2i32, 3i32}, 1i32),
              at(map<i32, i32>(1i32, 10i32, 2i32, 20i32), 2i32)))
}
)";
  const std::string srcPath = writeTemp("compile_collections_exe.prime", source);
  const std::string compileCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 22);
}

TEST_CASE("query-local auto vector helpers run in C++ emitter") {
  const std::string directSource = R"(
/vector/count([vector<i32>] values) {
  return(17i32)
}

[return<vector<i32>> effects(heap_alloc)]
valuesA() {
  [vector<i32>] values{vector<i32>(1i32, 2i32)}
  return(values)
}

[return<vector<i32>> effects(heap_alloc)]
valuesB() {
  [vector<i32>] values{vector<i32>(3i32, 4i32)}
  return(values)
}

[return<i32> effects(heap_alloc)]
main() {
  [auto] values{
    if(true,
      then(){ return(valuesA()) },
      else(){ return(valuesB()) })
  }
  return(/vector/count(values))
}
)";
  const std::string directSrcPath = writeTemp("compile_graph_query_vector_helper_call_exe.prime", directSource);
  const std::string directCmd = "./primec --emit=vm " + directSrcPath + " --entry /main";
  CHECK(runCommand(directCmd) == 17);

  const std::string methodSource = R"(
/vector/count([vector<i32>] values) {
  return(17i32)
}

[return<vector<i32>> effects(heap_alloc)]
valuesA() {
  [vector<i32>] values{vector<i32>(1i32, 2i32)}
  return(values)
}

[return<vector<i32>> effects(heap_alloc)]
valuesB() {
  [vector<i32>] values{vector<i32>(3i32, 4i32)}
  return(values)
}

[return<i32> effects(heap_alloc)]
main() {
  [auto] values{
    if(true,
      then(){ return(valuesA()) },
      else(){ return(valuesB()) })
  }
  return(values./vector/count())
}
)";
  const std::string methodSrcPath = writeTemp("compile_graph_query_vector_helper_method_exe.prime", methodSource);
  const std::string methodCmd = "./primec --emit=vm " + methodSrcPath + " --entry /main";
  CHECK(runCommand(methodCmd) == 17);
}

TEST_CASE("exact vector import runs explicit stdlib surface in C++ emitter") {
  const std::string source = R"(
import /std/collections/vector

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(4i32, 8i32, 15i32)}
  return(plus(/std/collections/vector/count(values),
      plus(/std/collections/vector/at(values, 0i32),
          /std/collections/vector/at_unsafe(values, 2i32))))
}
)";
  const std::string srcPath = writeTemp("compile_exact_vector_import_exe.prime", source);
  const std::string compileCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 22);
}

TEST_CASE("map wildcard import runs explicit stdlib-owned surface in C++ emitter") {
  // NOTE (TODO-5301, resolved): this test previously asserted that the
  // native (--emit=exe) backend *rejected* this call shape (fully-qualified
  // mapNew/mapInsert/mapCount/mapAtUnsafe calls on a named MapValue<K,V>
  // local) the same way it rejects other unsupported map builtin call
  // shapes. Investigation confirmed the called helpers are all legitimately
  // [public] in stdlib/std/collections/map.prime, so the wildcard import is
  // correct to make them callable, and - unlike the genuinely-unsupported
  // shapes the sibling "rejects ..." tests in this file cover - the native
  // backend actually lowers and runs this shape correctly today, producing
  // the same result (2 + 8 = 10) as the VM backend. The old "reject"
  // expectation was stale, not a live architectural decision, so this test
  // now asserts the real (successful, matching) behavior instead of a
  // fabricated rejection.
  const std::string source = R"(
import /std/collections/map/*

[effects(heap_alloc), return<int>]
main() {
  [MapValue<string, i32> mut] values{/std/collections/map/mapNew<string, i32>()}
  /std/collections/map/mapInsert<string, i32>(values, "one"raw_utf8, 4i32)
  /std/collections/map/mapInsert<string, i32>(values, "two"raw_utf8, 8i32)
  return(plus(/std/collections/map/mapCount<string, i32>(values),
      /std/collections/map/mapAtUnsafe<string, i32>(values, "two"raw_utf8)))
}
)";
  const std::string srcPath = writeTemp("compile_exact_map_import_exe.prime", source);
  const std::string exePath =
      (testScratchPath("") / "compile_exact_map_import_exe_exe").string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 10);

  const std::string vmCompileCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(vmCompileCmd) == 10);
}

TEST_CASE("concise vector binding example runs in C++ emitter") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
sumValues() {
  [vector<int> mut] values{4, 8, 15}
  [int mut] total{0}
  [int] count{values.count()}

  for([int mut] index{0}; index < count; ++index) {
    total = total + values[index]
  }

  return(total)
}

[effects(heap_alloc), return<int>]
main() {
  return(sumValues())
}
)";
  const std::string srcPath = writeTemp("compile_concise_vector_example_exe.prime", source);
  const std::string compileCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 27);
}

TEST_CASE("concise vector binding example runs in VM") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
sumValues() {
  [vector<int> mut] values{4, 8, 15}
  [int mut] total{0}
  [int] count{values.count()}

  for([int mut] index{0}; index < count; ++index) {
    total = total + values[index]
  }

  return(total)
}

[effects(heap_alloc), return<int>]
main() {
  return(sumValues())
}
)";
  const std::string srcPath = writeTemp("run_concise_vector_example_vm.prime", source);
  const std::string runCmd = "./primevm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 27);
}

TEST_CASE("rejects experimental soa stdlib helpers in C++ emitter") {
  const std::string source = R"(
import /std/collections/experimental_soa/*

[struct reflect]
Particle() {
  [i32] x{1i32}
}

[effects(heap_alloc), return<int>]
main() {
  [SoaVector<Particle>] values{soaVectorNew<Particle>()}
  return(plus(values.count(), soaVectorCount<Particle>(values)))
}
)";
  const std::string srcPath = writeTemp("compile_experimental_soa_helpers_exe.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_experimental_soa_helpers_exe_err.txt").string();

  const std::string compileCmd =
      "./primec --emit=vm " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find(
            "direct import of retired soa compatibility modules is not supported; use /std/collections/soa/*") !=
        std::string::npos);
}

TEST_CASE("validates soa type spelling in C++ emitter") {
  const std::string source = R"(
import /std/collections/soa/*

[struct reflect]
Particle() {
  [i32] x{1i32}
}

[effects(heap_alloc), return<int>]
main() {
  [soa<Particle>] values{soa<Particle>()}
  return(count(values))
}
)";
  const std::string srcPath = writeTemp("compile_raw_soa_type_exe.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_raw_soa_type_exe_err.txt").string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 0);
}

TEST_CASE("public soa count helper on public wrapper in C++ emitter") {
  const std::string source = R"(
import /std/collections/soa/*

[struct reflect]
Particle() {
  [i32] x{1i32}
}

[effects(heap_alloc), return<int>]
main() {
  [soa<Particle>] values{/std/collections/soa/single<Particle>(Particle(7i32))}
  return(plus(values.count(), /std/collections/soa/count<Particle>(values)))
}
)";
  const std::string srcPath =
      writeTemp("compile_public_soa_count_public_wrapper_exe.prime", source);
  const std::string compileCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 2);
}

TEST_CASE("public soa get helper in C++ emitter") {
  const std::string source = R"(
import /std/collections/soa/*

[struct reflect]
Particle() {
  [i32] x{1i32}
}

[effects(heap_alloc), return<int>]
main() {
  [soa<Particle>] values{/std/collections/soa/single<Particle>(Particle(9i32))}
  return(/std/collections/soa/get<Particle>(values, 0i32).x)
}
)";
  const std::string srcPath =
      writeTemp("compile_public_soa_get_public_wrapper_exe.prime", source);
  const std::string compileCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 9);
}

TEST_CASE("public soa get helper rejects template arguments on non-soa receiver in C++ emitter") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32)}
  return(/std/collections/soa/get<i32>(values, 0i32))
}
)";
  const std::string srcPath =
      writeTemp("compile_public_soa_get_non_soa_receiver_exe.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_public_soa_get_non_soa_receiver_exe_err.txt").string();

  const std::string compileCmd = "./primec --emit=vm " + srcPath + " --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("get requires soa target") !=
        std::string::npos);
}

TEST_CASE("runs public soa get slash-method in C++ emitter") {
  const std::string source = R"(
import /std/collections/soa/*

[struct reflect]
Particle() {
  [i32] x{1i32}
}

[effects(heap_alloc), return<int>]
main() {
  [soa<Particle>] values{/std/collections/soa/single<Particle>(Particle(9i32))}
  return(values./std/collections/soa/get(0i32).x)
}
)";
  const std::string srcPath =
      writeTemp("compile_public_soa_get_slash_method_exe.prime", source);

  const std::string compileCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 9);
}

TEST_CASE("runs public soa to_aos slash-method in C++ emitter") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/soa/*

[struct reflect]
Particle() {
  [i32] x{1i32}
}

[effects(heap_alloc), return<int>]
main() {
  [soa<Particle>] values{/std/collections/soa/single<Particle>(Particle(9i32))}
  [vector<Particle>] unpacked{values./std/collections/soa/to_aos()}
  return(count(unpacked))
}
)";
  const std::string srcPath =
      writeTemp("compile_public_soa_to_aos_slash_method_exe.prime", source);

  const std::string compileCmd =
      "./primec --emit=vm " + srcPath + " -o /dev/null --entry /main";
  CHECK(runCommand(compileCmd) == 1);
}

TEST_CASE("public soa ref helper in C++ emitter") {
  const std::string source = R"(
import /std/collections/soa/*

[struct reflect]
Particle() {
  [i32] x{1i32}
}

[effects(heap_alloc), return<int>]
main() {
  [soa<Particle>] values{/std/collections/soa/single<Particle>(Particle(9i32))}
  return(/std/collections/soa/ref<Particle>(values, 0i32).x)
}
)";
  const std::string srcPath =
      writeTemp("compile_public_soa_ref_public_wrapper_exe.prime", source);
  const std::string compileCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 9);
}

TEST_CASE("public soa mutator helpers in C++ emitter") {
  const std::string source = R"(
import /std/collections/soa/*

[struct reflect]
Particle() {
  [i32] x{1i32}
}

[effects(heap_alloc), return<int>]
main() {
  [soa<Particle> mut] values{soa<Particle>()}
  /std/collections/soa/reserve<Particle>(values, 2i32)
  /std/collections/soa/push<Particle>(values, Particle(4i32))
  /std/collections/soa/push<Particle>(values, Particle(9i32))
  return(plus(/std/collections/soa/count<Particle>(values),
              /std/collections/soa/get<Particle>(values, 1i32).x))
}
)";
  const std::string srcPath =
      writeTemp("compile_public_soa_mutators_public_wrapper_exe.prime", source);
  const std::string compileCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 11);
}

TEST_CASE("public soa to_aos helper lowers in C++ emitter") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/soa/*

[struct reflect]
Particle() {
  [i32] x{1i32}
}

[effects(heap_alloc), return<int>]
main() {
  [soa<Particle>] values{/std/collections/soa/single<Particle>(Particle(7i32))}
  [vector<Particle>] unpacked{/std/collections/soa/to_aos<Particle>(values)}
  return(count(unpacked))
}
)";
  const std::string srcPath =
      writeTemp("compile_public_soa_to_aos_public_wrapper_exe.prime", source);
  const std::string compileCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 1);
}

TEST_CASE("public soa to_aos temporaries route through canonical vector capacity in C++ emitter") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/soa/*

[struct reflect]
Particle() {
  [i32] x{1i32}
}

[effects(heap_alloc), return<int>]
main() {
  [soa<Particle>] values{/std/collections/soa/single<Particle>(Particle(7i32))}
  return(/std/collections/vector/capacity(/std/collections/soa/to_aos<Particle>(values)))
}
)";
  const std::string srcPath =
      writeTemp("compile_public_soa_to_aos_vector_capacity_public_wrapper_exe.prime", source);
  const std::string compileCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 1);
}

TEST_CASE("public soa to_aos explicit helper is a vector target in C++ emitter") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/soa/*

[struct reflect]
Particle() {
  [i32] x{1i32}
}

[effects(heap_alloc), return<int>]
main() {
  [soa<Particle>] values{/std/collections/soa/single<Particle>(Particle(7i32))}
  return(/std/collections/vector/capacity(/std/collections/soa/to_aos<Particle>(values)))
}
)";
  const std::string srcPath =
      writeTemp("compile_public_soa_to_aos_vector_capacity_exe.prime", source);
  const std::string compileCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 1);
}

TEST_CASE("runs legacy soa compatibility helpers in C++ emitter") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/soa/*
import /std/collections/soa_conversions/*

[struct reflect]
Particle() {
  [i32] x{1i32}
  [i32] y{2i32}
}

[effects(heap_alloc), return<int>]
main() {
  [auto mut] values{soaVectorNew<Particle>()}
  reserve(values, 2i32)
  push(values, Particle(4i32, 6i32))
  push(values, Particle(9i32, 11i32))
  [Particle] first{get(values, 0i32)}
  [Reference<Particle>] second{ref(values, 1i32)}
  [vector<Particle>] unpacked{to_aos(values)}
  return(plus(plus(count(values), plus(first.x, second.x)), count(unpacked)))
}
)";
  const std::string srcPath =
      writeTemp("compile_wildcard_legacy_soa_compatibility_helpers_exe.prime", source);

  const std::string compileCmd =
      "./primec --emit=vm " + srcPath + " -o /dev/null --entry /main";
  CHECK(runCommand(compileCmd) == 17);
}

TEST_CASE("rejects graph-solved direct local-auto vector helper shadows in C++ emitter compatibility") {
  const std::string source = R"(
/vector/count([vector<i32>] values) {
  return(17i32)
}

[return<vector<i32>> effects(heap_alloc)]
makeValues() {
  [vector<i32>] values{vector<i32>(1i32, 2i32, 3i32)}
  return(values)
}

[return<int> effects(heap_alloc)]
main() {
  [auto] values{makeValues()}
  return(plus(/vector/count(values), values./vector/count()))
}
)";
  const std::string srcPath =
      writeTemp("compile_graph_direct_local_auto_vector_helper_shadows_exe.prime", source);
  const std::string compileCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 34);
}

TEST_CASE(
    "rejects experimental soa stdlib wide structs on pending width boundary") {
  const std::string source = R"(
import /std/collections/soa/*
import /std/collections/internal_soa/*

[struct reflect]
Particle17() {
  [i32] a0{0i32}
  [i32] a1{0i32}
  [i32] a2{0i32}
  [i32] a3{0i32}
  [i32] a4{0i32}
  [i32] a5{0i32}
  [i32] a6{0i32}
  [i32] a7{0i32}
  [i32] a8{0i32}
  [i32] a9{0i32}
  [i32] a10{0i32}
  [i32] a11{0i32}
  [i32] a12{0i32}
  [i32] a13{0i32}
  [i32] a14{0i32}
  [i32] a15{0i32}
  [i32] a16{0i32}
}

[effects(heap_alloc), return<int>]
runImported() {
  [SoaVector<Particle17>] values{soaVectorNew<Particle17>()}
  return(soaVectorCount<Particle17>(values))
}

[effects(heap_alloc), return<int>]
runDirectCanonical() {
  [SoaVector<Particle17>] values{/std/collections/experimental_soa/soaVectorNew<Particle17>()}
  return(/std/collections/experimental_soa/soaVectorCount<Particle17>(values))
}

[return<SoaVector<Particle17>> effects(heap_alloc)]
makeWideValues() {
  return(soaVectorNew<Particle17>())
}

[effects(heap_alloc), return<int>]
runHelperReturn() {
  return(soaVectorCount<Particle17>(makeWideValues()))
}
)";
  const std::string srcPath =
      writeTemp("compile_experimental_soa_wide_pending_forms_exe.prime", source);
  const std::string importedErrPath =
      (testScratchPath("") / "primec_experimental_soa_wide_pending_imported_err.txt").string();
  const std::string directErrPath =
      (testScratchPath("") / "primec_experimental_soa_wide_pending_direct_err.txt").string();
  const std::string helperReturnErrPath =
      (testScratchPath("") / "primec_experimental_soa_wide_pending_helper_return_err.txt").string();

  const std::string compileImportedCmd =
      "./primec --emit=vm " + srcPath + " -o /dev/null --entry /runImported 2> " +
      importedErrPath;
  CHECK(runCommand(compileImportedCmd) == 2);
  CHECK(readFile(importedErrPath).find("direct import of retired soa compatibility modules is not supported") !=
        std::string::npos);

  const std::string compileDirectCmd =
      "./primec --emit=vm " + srcPath + " -o /dev/null --entry /runDirectCanonical 2> " +
      directErrPath;
  CHECK(runCommand(compileDirectCmd) == 2);
  CHECK(readFile(directErrPath).find("direct import of retired soa compatibility modules is not supported") !=
        std::string::npos);

  const std::string compileHelperReturnCmd =
      "./primec --emit=vm " + srcPath + " -o /dev/null --entry /runHelperReturn 2> " +
      helperReturnErrPath;
  CHECK(runCommand(compileHelperReturnCmd) == 2);
  CHECK(readFile(helperReturnErrPath).find("direct import of retired soa compatibility modules is not supported") !=
        std::string::npos);
}

TEST_CASE("rejects experimental soa stdlib from-aos helper in C++ emitter before typed bindings support") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/soa/*
import /std/collections/internal_soa/*

[struct reflect]
Particle() {
  [i32] x{1i32}
}

[effects(heap_alloc), return<int>]
main() {
  [vector<Particle>] values{vector<Particle>(Particle(7i32), Particle(9i32))}
  [SoaVector<Particle>] packed{soaVectorFromAos<Particle>(values)}
  [Particle] second{soaVectorGet<Particle>(packed, 1i32)}
  return(plus(soaVectorCount<Particle>(packed), second.x))
}
)";
  const std::string srcPath = writeTemp("compile_experimental_soa_from_aos_exe.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_experimental_soa_from_aos_err.txt").string();
  const std::string compileCmd =
      "./primec --emit=vm " + srcPath + " --entry /main > " + errPath + " 2>&1";
  CHECK(runCommand(compileCmd) == 2);
}

TEST_CASE("rejects root non-struct non-empty soa literal with semantic/emit parity in C++ emitter") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [soa<i32>] values{soa<i32>(1i32)}
  return(count(values))
}
)";
  const std::string srcPath = writeTemp("compile_root_soa_non_struct_literal_reject.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_root_soa_non_struct_literal_reject_exe").string();
  const std::string semanticErrPath =
      (testScratchPath("") / "primec_root_soa_non_struct_literal_reject_semantic_err.txt").string();
  const std::string emitErrPath =
      (testScratchPath("") / "primec_root_soa_non_struct_literal_reject_emit_err.txt").string();

  const std::string semanticCmd =
      "./primec --dump-stage ast-semantic " + srcPath + " --entry /main > /dev/null 2> " + semanticErrPath;
  CHECK(runCommand(semanticCmd) == 2);
  const std::string semanticErr = readFile(semanticErrPath);
  CHECK(semanticErr.find("soa requires struct element type") !=
        std::string::npos);
  CHECK(semanticErr.find("stage: semantic") != std::string::npos);

  const std::string emitCmd =
      "./primec --emit=vm " + srcPath + " -o " + exePath + " --entry /main 2> " + emitErrPath;
  CHECK(runCommand(emitCmd) == 2);
  const std::string emitErr = readFile(emitErrPath);
  CHECK(emitErr.find("soa requires struct element type") !=
        std::string::npos);
  CHECK(emitErr.find("stage: semantic") != std::string::npos);
}

TEST_CASE("runs experimental soa stdlib to-aos helper in C++ emitter") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/soa/*

[struct reflect]
Particle() {
  [i32] x{1i32}
}

[effects(heap_alloc), return<int>]
main() {
  [SoaVector<Particle>] values{soaVectorNew<Particle>()}
  [vector<Particle>] unpacked{soaVectorToAos<Particle>(values)}
  return(count(unpacked))
}
)";
  const std::string srcPath = writeTemp("compile_experimental_soa_to_aos_exe.prime", source);
  const std::string compileCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
}

TEST_CASE("rejects experimental soa stdlib to-aos method on wrapper surface in C++ emitter") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/soa/*
import /std/collections/internal_soa/*
import /std/collections/soa/*
import /std/collections/internal_soa_conversions/*

[struct reflect]
Particle() {
  [i32] x{1i32}
}

[effects(heap_alloc), return<int>]
main() {
  [SoaVector<Particle>] values{soaVectorNew<Particle>()}
  [vector<Particle>] unpacked{values.to_aos()}
  return(count(unpacked))
}
)";
  const std::string srcPath = writeTemp("compile_experimental_soa_to_aos_method_exe.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_experimental_soa_to_aos_method_exe.err").string();
  const std::string compileCmd =
      "./primec --emit=vm " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("direct import of retired soa compatibility modules is not supported") !=
        std::string::npos);
}

TEST_CASE("no-import root soa to_aos bare and direct helper forms reject") {
  const std::string source = R"(
[struct reflect]
Particle() {
  [i32] x{1i32}
}

[effects(heap_alloc), return<int>]
main() {
  [soa<Particle>] values{soa<Particle>()}
  [vector<Particle>] unpackedA{to_aos(values)}
  [vector<Particle>] unpackedB{/to_aos(values)}
  return(plus(count(unpackedA), count(unpackedB)))
}
)";
  const std::string srcPath = writeTemp("compile_root_soa_to_aos_forms_exe.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_root_soa_to_aos_forms_exe_err.txt").string();
  const std::string compileCmd =
      "./primec --emit=vm " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  // TODO-5318: no-import soa helpers reject in semantics with one import
  // diagnostic (docs/PrimeStruct.md, No-import helper rule).
  CHECK(readFile(errPath).find("soa helper requires import /std/collections/soa/*: to_aos") != std::string::npos);
}

TEST_CASE("no-import root soa to_aos method helper forms reject during semantics in C++ emitter") {
  const std::string source = R"(
[struct reflect]
Particle() {
  [i32] x{1i32}
}

[effects(heap_alloc), return<int>]
main() {
  [soa<Particle>] values{soa<Particle>()}
  [vector<Particle>] unpackedA{values.to_aos()}
  [vector<Particle>] unpackedB{values./to_aos()}
  return(plus(count(unpackedA), count(unpackedB)))
}
)";
  const std::string srcPath = writeTemp("compile_root_soa_to_aos_method_forms_exe.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_root_soa_to_aos_method_forms_exe_err.txt").string();
  const std::string compileCmd =
      "./primec --emit=vm " + srcPath + " --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  // TODO-5318: no-import soa helpers reject in semantics with one import
  // diagnostic (docs/PrimeStruct.md, No-import helper rule).
  CHECK(readFile(errPath).find("soa helper requires import /std/collections/soa/*: to_aos") != std::string::npos);
}

TEST_CASE("no-import root soa canonical to_aos_ref helper form rejects in C++ emitter") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/soa/*

[struct reflect]
Particle() {
  [i32] x{1i32}
}

[effects(heap_alloc), return<int>]
main() {
  [soa<Particle> mut] values{soa<Particle>()}
  [vector<Particle>] unpacked{/std/collections/soa/to_aos_ref<Particle>(location(values))}
  return(count(unpacked))
}
)";
  const std::string srcPath = writeTemp("compile_root_soa_to_aos_ref_form_exe.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_root_soa_to_aos_ref_form_exe.err").string();
  const std::string compileCmd =
      "./primec --emit=vm " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  // TODO-5050 to_aos_ref gap (RESOLVED): a real `to_aos_ref<T>` stdlib
  // function now exists, so this no longer fails semantic validation with
  // "unknown method". It still fails to compile, but later, at IR
  // lowering, with a distinct (narrower, not investigated further here)
  // struct-parameter-type-mismatch error - `soa<Particle>` and
  // `SoaVector<Particle>` apparently monomorphize to two different
  // specialized struct paths that don't line up through this call shape.
  CHECK(readFile(errPath).find("struct parameter type mismatch") !=
        std::string::npos);
}

TEST_CASE("experimental SoaVector canonical to_aos_ref helper form rejects in C++ emitter") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/soa/*
import /std/collections/internal_soa/*

[struct reflect]
Particle() {
  [i32] x{1i32}
}

[effects(heap_alloc), return<int>]
main() {
  [SoaVector<Particle> mut] values{soaVectorNew<Particle>()}
  soaVectorPush<Particle>(values, Particle(7i32))
  [vector<Particle>] unpacked{/std/collections/soa/to_aos_ref<Particle>(location(values))}
  return(count(unpacked))
}
)";
  const std::string srcPath = writeTemp("compile_experimental_soa_to_aos_ref_form_exe.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_experimental_soa_to_aos_ref_form_exe.err").string();
  const std::string compileCmd =
      "./primec --emit=vm " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("direct import of retired soa compatibility modules is not supported") !=
        std::string::npos);
}

TEST_CASE(
    "direct experimental soaVectorToAos helpers on builtin soa reject in C++ emitter") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/soa/*
import /std/collections/internal_soa_conversions/*

[struct reflect]
Particle() {
  [i32] x{1i32}
}

[effects(heap_alloc), return<int>]
runDirect() {
  [soa<Particle>] values{soa<Particle>(Particle(7i32))}
  [vector<Particle>] unpacked{
      /std/collections/experimental_soa_conversions/soaVectorToAos<Particle>(values)}
  return(count(unpacked))
}

[effects(heap_alloc), return<int>]
runRef() {
  [soa<Particle> mut] values{soa<Particle>(Particle(7i32))}
  [vector<Particle>] unpacked{
      /std/collections/experimental_soa_conversions/soaVectorToAosRef<Particle>(location(values))}
  return(count(unpacked))
}
)";
  const std::string srcPath =
      writeTemp("compile_root_builtin_soa_direct_experimental_to_aos_reject.prime", source);
  const std::string directErrPath =
      (testScratchPath("") /
       "primec_root_builtin_soa_direct_experimental_to_aos_reject_direct.err")
          .string();
  const std::string refErrPath =
      (testScratchPath("") /
       "primec_root_builtin_soa_direct_experimental_to_aos_reject_ref.err")
          .string();

  const std::string compileDirectCmd =
      "./primec --emit=vm " + srcPath + " -o /dev/null --entry /runDirect 2> " +
      directErrPath;
  CHECK(runCommand(compileDirectCmd) != 0);
  CHECK(readFile(directErrPath).find("direct import of retired soa compatibility modules is not supported") !=
        std::string::npos);

  const std::string compileRefCmd =
      "./primec --emit=vm " + srcPath + " -o /dev/null --entry /runRef 2> " +
      refErrPath;
  CHECK(runCommand(compileRefCmd) != 0);
  CHECK(readFile(refErrPath).find("direct import of retired soa compatibility modules is not supported") !=
        std::string::npos);
}

TEST_CASE("runs experimental soa stdlib non-empty to-aos helper in C++ emitter") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/soa/*

[struct reflect]
Particle() {
  [i32] x{1i32}
}

[effects(heap_alloc), return<int>]
main() {
  [SoaVector<Particle>] values{soaVectorSingle<Particle>(Particle(7i32))}
  [vector<Particle>] unpacked{soaVectorToAos<Particle>(values)}
  return(count(unpacked))
}
)";
  const std::string srcPath = writeTemp("compile_experimental_soa_to_aos_non_empty_exe.prime", source);
  const std::string compileCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 1);
}

TEST_CASE("rejects experimental soa stdlib non-empty to-aos method on wrapper state in C++ emitter") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/soa/*
import /std/collections/internal_soa/*
import /std/collections/soa/*
import /std/collections/internal_soa_conversions/*

[struct reflect]
Particle() {
  [i32] x{1i32}
}

[effects(heap_alloc), return<int>]
main() {
  [SoaVector<Particle>] values{soaVectorSingle<Particle>(Particle(7i32))}
  [vector<Particle>] unpacked{values.to_aos()}
  return(count(unpacked))
}
)";
  const std::string srcPath = writeTemp("compile_experimental_soa_to_aos_non_empty_method_exe.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_experimental_soa_to_aos_non_empty_method_exe.err")
          .string();
  const std::string compileCmd =
      "./primec --emit=vm " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("direct import of retired soa compatibility modules is not supported") !=
        std::string::npos);
}

TEST_CASE("rejects experimental soa stdlib get helper in C++ emitter") {
  const std::string source = R"(
import /std/collections/soa/*
import /std/collections/internal_soa/*

[struct reflect]
Particle() {
  [i32] x{1i32}
}

[effects(heap_alloc), return<int>]
main() {
  [SoaVector<Particle>] values{soaVectorSingle<Particle>(Particle(7i32))}
  [Particle] value{soaVectorGet<Particle>(values, 0i32)}
  return(value.x)
}
)";
  const std::string srcPath = writeTemp("compile_experimental_soa_get_exe.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_experimental_soa_get_exe.err").string();

  const std::string compileCmd =
      "./primec --emit=vm " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("direct import of retired soa compatibility modules is not supported") !=
        std::string::npos);
}

TEST_CASE("rejects experimental soa stdlib get method in C++ emitter") {
  const std::string source = R"(
import /std/collections/soa/*
import /std/collections/internal_soa/*

[struct reflect]
Particle() {
  [i32] x{1i32}
}

[effects(heap_alloc), return<int>]
main() {
  [SoaVector<Particle>] values{soaVectorSingle<Particle>(Particle(7i32))}
  [Particle] value{values.get(0i32)}
  return(value.x)
}
)";
  const std::string srcPath = writeTemp("compile_experimental_soa_get_method_exe.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_experimental_soa_get_method_exe.err").string();

  const std::string compileCmd =
      "./primec --emit=vm " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("direct import of retired soa compatibility modules is not supported") !=
        std::string::npos);
}

TEST_CASE("rejects bare soa get helper through helper return in C++ emitter compatibility") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/soa/*
import /std/collections/internal_soa/*

[struct reflect]
Particle() {
  [i32] x{1i32}
}

[return<SoaVector<Particle>>]
cloneValues() {
  [SoaVector<Particle>, mut] values{soaVectorNew<Particle>()}
  values.push(Particle(7i32))
  return(values)
}

[effects(heap_alloc), return<int>]
main() {
  return(get(cloneValues(), 0i32).x)
}
)";
  const std::string srcPath =
      writeTemp("compile_experimental_soa_get_helper_return_exe.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_experimental_soa_get_helper_return_exe.err").string();

  const std::string compileCmd =
      "./primec --emit=vm " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find(
            "direct import of retired soa compatibility modules is not supported") !=
        std::string::npos);
}

TEST_CASE("rejects global helper-return soa method shadows in C++ emitter compatibility") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/soa/*
import /std/collections/internal_soa/*

[struct reflect]
Particle() {
  [i32] x{1i32}
}

[return<SoaVector<Particle>>]
cloneValues() {
  [SoaVector<Particle> mut] values{soaVectorNew<Particle>()}
  /std/collections/experimental_soa/soaVectorPush<Particle>(values, Particle(7i32))
  return(values)
}

[return<int>]
/soa/count([SoaVector<Particle>] values) {
  return(11i32)
}

[return<Particle>]
/soa/get([SoaVector<Particle>] values, [int] index) {
  return(Particle(23i32))
}

[return<Particle>]
/soa/ref([SoaVector<Particle>] values, [int] index) {
  return(Particle(29i32))
}

[return<int>]
/soa/push([SoaVector<Particle>] values, [Particle] value) {
  return(value.x)
}

[return<int>]
/soa/reserve([SoaVector<Particle>] values, [int] count) {
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
  expect_soa_helper_return_shadow_reject(
      source,
      "compile_experimental_soa_method_shadow_global_helper_return_exe",
      "exe",
      "direct import of retired soa compatibility modules is not supported");
}

TEST_CASE("rejects method-like helper-return soa method shadows in C++ emitter compatibility") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/soa/*
import /std/collections/internal_soa/*

[struct reflect]
Particle() {
  [i32] x{1i32}
}

[struct]
Holder() {}

[return<SoaVector<Particle>>]
/Holder/cloneValues([Holder] self) {
  [SoaVector<Particle> mut] values{soaVectorNew<Particle>()}
  /std/collections/experimental_soa/soaVectorPush<Particle>(values, Particle(7i32))
  return(values)
}

[return<int>]
/soa/count([SoaVector<Particle>] values) {
  return(11i32)
}

[return<Particle>]
/soa/get([SoaVector<Particle>] values, [int] index) {
  return(Particle(23i32))
}

[return<Particle>]
/soa/ref([SoaVector<Particle>] values, [int] index) {
  return(Particle(29i32))
}

[return<int>]
/soa/push([SoaVector<Particle>] values, [Particle] value) {
  return(value.x)
}

[return<int>]
/soa/reserve([SoaVector<Particle>] values, [int] count) {
  return(count)
}

[effects(heap_alloc), return<int>]
main() {
  [Holder] holder{Holder{}}
  [Particle] value{Particle(31i32)}
  return(plus(holder.cloneValues().count(),
              plus(holder.cloneValues().get(0i32).x,
                   plus(holder.cloneValues().ref(0i32).x,
                        plus(holder.cloneValues().push(value),
                             holder.cloneValues().reserve(37i32))))))
}
)";
  expect_soa_helper_return_shadow_reject(
      source,
      "compile_experimental_soa_method_shadow_method_like_helper_return_exe",
      "exe",
      "direct import of retired soa compatibility modules is not supported");
}

TEST_CASE("vector-target old-explicit soa mutator shadows in C++ emitter") {
  const std::string source = R"(
[return<int>]
/soa/push([vector<i32>] values, [i32] value) {
  return(value)
}

[return<int>]
/soa/reserve([vector<i32>] values, [i32] count) {
  return(count)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32, 3i32)}
  return(plus(values./soa/push(4i32), values./soa/reserve(6i32)))
}
)";
  const std::string srcPath =
      writeTemp("compile_vector_target_old_explicit_soa_mutator_shadow_exe.prime", source);
  const std::string compileCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 10);
}

TEST_CASE("vector-target method soa mutator shadows in C++ emitter") {
  const std::string source = R"(
[return<int>]
/soa/push([vector<i32>] values, [i32] value) {
  return(value)
}

[return<int>]
/soa/reserve([vector<i32>] values, [i32] count) {
  return(count)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32, 3i32)}
  return(plus(values.push(4i32), values.reserve(6i32)))
}
)";
  const std::string srcPath =
      writeTemp("compile_vector_target_method_soa_mutator_shadow_exe.prime", source);
  const std::string compileCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 10);
}

TEST_CASE("runs vector-target to_aos helper shadows in C++ emitter") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
/to_aos([vector<i32>] values) {
  return(9i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{/std/collections/vector/vector<i32>(1i32)}
  [int] direct{/to_aos(values)}
  [int] method{values.to_aos()}
  [int] slash{values./to_aos()}
  return(plus(direct, plus(method, slash)))
}
)";
  const std::string srcPath =
      writeTemp("compile_vector_target_to_aos_shadow_exe.prime", source);

  const std::string compileCmd =
      "./primec --emit=vm " + srcPath + " -o /dev/null --entry /main";
  CHECK(runCommand(compileCmd) == 27);
}

TEST_CASE("rejects nested struct-body soa constructor-bearing helper returns in C++ emitter compatibility") {
  const std::string source = R"(
import /std/collections/soa/*
import /std/collections/internal_soa/*

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
      writeTemp("compile_nested_struct_body_soa_constructor_helper_exe.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_nested_struct_body_soa_constructor_helper_exe.err")
          .string();

  const std::string compileCmd =
      "./primec --emit=vm " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("direct import of retired soa compatibility modules is not supported") !=
        std::string::npos);
}

TEST_CASE("rejects nested struct-body soa direct and bound helper expressions in C++ emitter compatibility") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/soa/*
import /std/collections/internal_soa/*

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
  [Holder] holder{Holder{}}
  [SoaVector<Particle>] values{holder.cloneValues()}
  return(plus(plus(plus(holder.cloneValues().count(), holder.cloneValues().get(0i32).x),
                    values.ref(0i32).x),
              count(values.to_aos())))
}
)";
  const std::string srcPath =
      writeTemp("compile_nested_struct_body_soa_direct_bound_helpers_exe.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_nested_struct_body_soa_direct_bound_helpers_exe.err")
          .string();

  const std::string compileCmd =
      "./primec --emit=vm " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("direct import of retired soa compatibility modules is not supported") !=
        std::string::npos);
}

TEST_CASE("rejects nested struct-body soa method shadows in C++ emitter compatibility") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/soa/*
import /std/collections/internal_soa/*

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
/soa/count([SoaVector<Particle>] values) {
  return(13i32)
}

[return<Particle>]
/soa/get([SoaVector<Particle>] values, [i32] index) {
  return(Particle(23i32))
}

[return<Particle>]
/soa/ref([SoaVector<Particle>] values, [i32] index) {
  return(Particle(29i32))
}

[return<i32>]
/soa/push([SoaVector<Particle>] values, [Particle] value) {
  return(31i32)
}

[return<i32>]
/soa/reserve([SoaVector<Particle>] values, [i32] capacity) {
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
  [Holder] holder{Holder{}}
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
      writeTemp("compile_nested_struct_body_soa_method_shadows_exe.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_nested_struct_body_soa_method_shadows_exe.err")
          .string();

  const std::string compileCmd =
      "./primec --emit=vm " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("direct import of retired soa compatibility modules is not supported") !=
        std::string::npos);
}

TEST_CASE("rejects explicit method-like helper-return experimental soa to_aos shadow in C++ emitter") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/soa/*
import /std/collections/internal_soa/*

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

[effects(heap_alloc), return<vector<Particle>>]
/to_aos([SoaVector<Particle>] values) {
  [vector<Particle>, mut] out{vector<Particle>()}
  out.push(Particle(19i32))
  return(out)
}

[effects(heap_alloc), return<int>]
main() {
  [Holder] holder{Holder{}}
  [vector<Particle>] values{holder.cloneValues().to_aos()}
  return(count(values))
}
)";
  const std::string srcPath =
      writeTemp("compile_experimental_soa_explicit_method_like_to_aos_shadow_exe.prime",
                source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_experimental_soa_explicit_method_like_to_aos_shadow_exe.err")
          .string();

  const std::string compileCmd =
      "./primec --emit=vm " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("direct import of retired soa compatibility modules is not supported") !=
        std::string::npos);
}

TEST_CASE("rejects experimental soa stdlib ref helper in C++ emitter") {
  const std::string source = R"(
import /std/collections/soa/*
import /std/collections/internal_soa/*

[struct reflect]
Particle() {
  [i32] x{1i32}
}

[effects(heap_alloc), return<int>]
main() {
  [SoaVector<Particle>] values{soaVectorSingle<Particle>(Particle(7i32))}
  [Reference<Particle>] value{soaVectorRef<Particle>(values, 0i32)}
  return(value.x)
}
)";
  const std::string srcPath = writeTemp("compile_experimental_soa_ref_exe.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_experimental_soa_ref_exe.err").string();
  const std::string compileCmd =
      "./primec --emit=vm " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("direct import of retired soa compatibility modules is not supported") !=
        std::string::npos);
}

TEST_CASE("rejects experimental soa stdlib ref method in C++ emitter") {
  const std::string source = R"(
import /std/collections/soa/*
import /std/collections/internal_soa/*

[struct reflect]
Particle() {
  [i32] x{1i32}
}

[effects(heap_alloc), return<int>]
main() {
  [SoaVector<Particle>] values{soaVectorSingle<Particle>(Particle(7i32))}
  [Reference<Particle>] value{values.ref(0i32)}
  return(value.x)
}
)";
  const std::string srcPath = writeTemp("compile_experimental_soa_ref_method_exe.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_experimental_soa_ref_method_exe.err").string();
  const std::string compileCmd =
      "./primec --emit=vm " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("direct import of retired soa compatibility modules is not supported") !=
        std::string::npos);
}

TEST_CASE("rejects experimental soa ref pass-through and return in C++ emitter") {
  const std::string source = R"(
import /std/collections/soa/*
import /std/collections/internal_soa/*

[struct reflect]
Particle() {
  [i32] x{1i32}
}

[return<Reference<Particle>>]
pass([Reference<Particle>] value) {
  return(value)
}

[return<Reference<Particle>>]
pick([SoaVector<Particle>] values) {
  return(pass(values.ref(0i32)))
}

[effects(heap_alloc), return<int>]
main() {
  [SoaVector<Particle>] values{soaVectorSingle<Particle>(Particle(7i32))}
  [Reference<Particle>] value{pick(values)}
  return(value.x)
}
)";
  const std::string srcPath =
      writeTemp("compile_experimental_soa_ref_passthrough_exe.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_experimental_soa_ref_passthrough_exe.err").string();
  const std::string compileCmd =
      "./primec --emit=vm " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("direct import of retired soa compatibility modules is not supported") !=
        std::string::npos);
}

TEST_CASE("rejects experimental soa stdlib push and reserve helpers in C++ emitter") {
  const std::string source = R"(
import /std/collections/soa/*
import /std/collections/internal_soa/*

[struct reflect]
Particle() {
  [i32] x{1i32}
}

[effects(heap_alloc), return<int>]
main() {
  [SoaVector<Particle> mut] values{soaVectorNew<Particle>()}
  soaVectorReserve<Particle>(values, 2i32)
  soaVectorPush<Particle>(values, Particle(4i32))
  soaVectorPush<Particle>(values, Particle(9i32))
  [Particle] second{soaVectorGet<Particle>(values, 1i32)}
  return(plus(soaVectorCount<Particle>(values), second.x))
}
)";
  const std::string srcPath = writeTemp("compile_experimental_soa_push_helpers_exe.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_experimental_soa_push_helpers_exe.err").string();

  const std::string compileCmd =
      "./primec --emit=vm " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("direct import of retired soa compatibility modules is not supported") !=
        std::string::npos);
}

TEST_CASE("rejects experimental soa stdlib push and reserve methods in C++ emitter") {
  const std::string source = R"(
import /std/collections/soa/*
import /std/collections/internal_soa/*

[struct reflect]
Particle() {
  [i32] x{1i32}
}

[effects(heap_alloc), return<int>]
main() {
  [SoaVector<Particle> mut] values{soaVectorNew<Particle>()}
  values.reserve(2i32)
  values.push(Particle(4i32))
  values.push(Particle(9i32))
  [Particle] second{values.get(1i32)}
  return(plus(values.count(), second.x))
}
)";
  const std::string srcPath = writeTemp("compile_experimental_soa_push_method_exe.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_experimental_soa_push_method_exe.err").string();

  const std::string compileCmd =
      "./primec --emit=vm " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("direct import of retired soa compatibility modules is not supported") !=
        std::string::npos);
}

TEST_CASE("runs experimental soa single-field index syntax in C++ emitter") {
  const std::string source = R"(
import /std/collections/soa/*

[struct reflect]
ScalarBox() {
  [i32] x{1i32}
}

[effects(heap_alloc), return<int>]
main() {
  [SoaVector<ScalarBox> mut] values{soaVectorNew<ScalarBox>()}
  soaVectorPush<ScalarBox>(values, ScalarBox(4i32))
  soaVectorPush<ScalarBox>(values, ScalarBox(9i32))
  return(values.x()[1i32])
}
)";
  const std::string srcPath = writeTemp("compile_experimental_soa_single_field_view_exe.prime", source);
  const std::string compileCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 9);
}

TEST_CASE("runs experimental soa reflected multi-field index syntax in C++ emitter") {
  const std::string source = R"(
import /std/collections/soa/*

[struct reflect]
Particle() {
  [i32] x{1i32}
  [i32] y{2i32}
}

[effects(heap_alloc), return<int>]
main() {
  [SoaVector<Particle> mut] values{soaVectorNew<Particle>()}
  soaVectorPush<Particle>(values, Particle(7i32, 8i32))
  soaVectorPush<Particle>(values, Particle(9i32, 12i32))
  return(values.y()[1i32])
}
)";
  const std::string srcPath = writeTemp("compile_experimental_soa_field_view_exe.prime", source);
  const std::string compileCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 12);
}

TEST_CASE("rejects experimental soa mutating indexed field writes in C++ emitter") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/soa/*
import /std/collections/internal_soa/*

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
  assign(values.y()[1i32], 17i32)
  assign(y(values)[0i32], 19i32)
  assign(ref(values, 0i32).x, 5i32)
  assign(values.ref(1i32).x, 11i32)
  return(plus(values.x()[0i32],
              plus(values.y()[0i32],
                   plus(values.x()[1i32], values.y()[1i32]))))
}
)";
  const std::string srcPath =
      writeTemp("compile_experimental_soa_mutating_indexed_field_writes_exe.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_experimental_soa_mutating_indexed_field_writes_exe.err")
          .string();

  const std::string compileCmd =
      "./primec --emit=vm " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find(
            "direct import of retired soa compatibility modules is not supported") !=
        std::string::npos);
}

TEST_CASE("runs richer borrowed experimental soa mutating indexed field writes in C++ emitter") {
  const std::string source = R"(
import /std/collections/soa/*

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
  soaVectorPush<Particle>(values, Particle(7i32, 8i32))
  soaVectorPush<Particle>(values, Particle(9i32, 12i32))
  assign(dereference(pickBorrowed(location(values))).y()[1i32], 17i32)
  assign(y(location(pickBorrowed(location(values))))[0i32], 19i32)
  return(plus(dereference(pickBorrowed(location(values))).y()[1i32],
              y(location(pickBorrowed(location(values))))[0i32]))
}
)";
  const std::string srcPath = writeTemp(
      "compile_experimental_soa_richer_borrowed_mutating_indexed_field_writes_exe.prime",
      source);
  const std::string compileCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 36);
}

TEST_CASE("runs method-like borrowed experimental soa mutating indexed field writes in C++ emitter") {
  const std::string source = R"(
import /std/collections/soa/*

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
  soaVectorPush<Particle>(values, Particle(7i32, 8i32))
  soaVectorPush<Particle>(values, Particle(9i32, 12i32))
  [Holder] holder{Holder{}}
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
      "compile_experimental_soa_method_like_borrowed_mutating_indexed_field_writes_exe.prime",
      source);
  const std::string compileCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 104);
}

TEST_CASE("runs borrowed experimental soa reflected index syntax in C++ emitter") {
  const std::string source = R"(
import /std/collections/soa/*

[struct reflect]
Particle() {
  [i32] x{1i32}
  [i32] y{2i32}
}

[effects(heap_alloc), return<int>]
main() {
  [SoaVector<Particle> mut] values{soaVectorNew<Particle>()}
  soaVectorPush<Particle>(values, Particle(7i32, 8i32))
  soaVectorPush<Particle>(values, Particle(9i32, 12i32))
  [Reference<SoaVector<Particle>>] borrowed{location(values)}
  return(dereference(borrowed).y()[1i32])
}
)";
  const std::string srcPath =
      writeTemp("compile_experimental_soa_borrowed_field_view_exe.prime", source);
  const std::string compileCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 12);
}

TEST_CASE("runs borrowed local experimental soa reflected index syntax in C++ emitter") {
  const std::string source = R"(
import /std/collections/soa/*

[struct reflect]
Particle() {
  [i32] x{1i32}
  [i32] y{2i32}
}

[effects(heap_alloc), return<int>]
main() {
  [SoaVector<Particle> mut] values{soaVectorNew<Particle>()}
  soaVectorPush<Particle>(values, Particle(7i32, 8i32))
  soaVectorPush<Particle>(values, Particle(9i32, 12i32))
  [Reference<SoaVector<Particle>>] borrowed{location(values)}
  return(borrowed.y()[1i32])
}
)";
  const std::string srcPath =
      writeTemp("compile_experimental_soa_borrowed_local_field_view_exe.prime", source);
  const std::string compileCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 12);
}

TEST_CASE("runs borrowed helper-return experimental soa reflected index syntax in C++ emitter") {
  const std::string source = R"(
import /std/collections/soa/*

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
  soaVectorPush<Particle>(values, Particle(4i32, 6i32))
  soaVectorPush<Particle>(values, Particle(9i32, 12i32))
  return(pickBorrowed(location(values)).y()[1i32])
}
)";
  const std::string srcPath =
      writeTemp("compile_experimental_soa_borrowed_return_field_view_exe.prime", source);
  const std::string compileCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 12);
}

TEST_CASE("rejects experimental soa bare get and ref field access in C++ emitter") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/soa/*
import /std/collections/internal_soa/*

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
  return(plus(ref(values, 0i32).y, get(values, 1i32).y))
}
)";
  const std::string srcPath =
      writeTemp("compile_experimental_soa_bare_ref_field_access_exe.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_experimental_soa_bare_ref_field_access_exe.err")
          .string();
  const std::string compileCmd =
      "./primec --emit=vm " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find(
            "direct import of retired soa compatibility modules is not supported") !=
        std::string::npos);
}

TEST_SUITE_END();
