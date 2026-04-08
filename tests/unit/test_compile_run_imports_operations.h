#include "test_compile_run_collection_conformance_helpers.h"
#include "test_compile_run_container_error_conformance_helpers.h"
#include "test_compile_run_checked_pointer_conformance_helpers.h"
#include "test_compile_run_unchecked_pointer_conformance_helpers.h"

TEST_SUITE_BEGIN("primestruct.compile.run.imports");

TEST_CASE("compiles and runs collection literals in C++ emitter") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
main() {
  return(plus(at_unsafe(array<i32>{1i32, 2i32, 3i32}, 1i32),
              at(map<i32, i32>{1i32=10i32, 2i32=20i32}, 2i32)))
}
)";
  const std::string srcPath = writeTemp("compile_collections_exe.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_collections_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 22);
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
  const std::string directExePath =
      (testScratchPath("") / "compile_graph_query_vector_helper_call_exe").string();
  const std::string directCmd =
      "./primec --emit=exe " + directSrcPath + " -o " + directExePath + " --entry /main";
  CHECK(runCommand(directCmd) == 0);
  CHECK(runCommand(directExePath) == 17);

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
  const std::string methodExePath =
      (testScratchPath("") / "compile_graph_query_vector_helper_method_exe").string();
  const std::string methodCmd =
      "./primec --emit=exe " + methodSrcPath + " -o " + methodExePath + " --entry /main";
  CHECK(runCommand(methodCmd) == 0);
  CHECK(runCommand(methodExePath) == 17);
}

TEST_CASE("compiles and runs experimental soa_vector stdlib helpers in C++ emitter") {
  const std::string source = R"(
import /std/collections/experimental_soa_vector/*

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
  const std::string srcPath = writeTemp("compile_experimental_soa_vector_helpers_exe.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_experimental_soa_vector_helpers_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 0);
}

TEST_CASE("compiles and runs experimental soa_vector stdlib non-empty helper in C++ emitter") {
  const std::string source = R"(
import /std/collections/experimental_soa_vector/*

[struct reflect]
Particle() {
  [i32] x{1i32}
}

[effects(heap_alloc), return<int>]
main() {
  [SoaVector<Particle>] values{soaVectorSingle<Particle>(Particle(7i32))}
  [Particle] value{soaVectorGet<Particle>(values, 0i32)}
  return(plus(soaVectorCount<Particle>(values), value.x))
}
)";
  const std::string srcPath = writeTemp("compile_experimental_soa_vector_single_exe.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_experimental_soa_vector_single_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 8);
}

TEST_CASE("compiles and runs canonical soa_vector count helper on experimental wrapper in C++ emitter") {
  const std::string source = R"(
import /std/collections/experimental_soa_vector/*

[struct reflect]
Particle() {
  [i32] x{1i32}
}

[effects(heap_alloc), return<int>]
main() {
  [SoaVector<Particle>] values{soaVectorSingle<Particle>(Particle(7i32))}
  return(plus(values.count(), /std/collections/soa_vector/count<Particle>(values)))
}
)";
  const std::string srcPath =
      writeTemp("compile_canonical_soa_vector_count_experimental_wrapper_exe.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_canonical_soa_vector_count_experimental_wrapper_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 2);
}

TEST_CASE("compiles and runs canonical soa_vector get helper in C++ emitter") {
  const std::string source = R"(
import /std/collections/experimental_soa_vector/*

[struct reflect]
Particle() {
  [i32] x{1i32}
}

[effects(heap_alloc), return<int>]
main() {
  [SoaVector<Particle>] values{soaVectorSingle<Particle>(Particle(9i32))}
  return(/std/collections/soa_vector/get<Particle>(values, 0i32).x)
}
)";
  const std::string srcPath =
      writeTemp("compile_canonical_soa_vector_get_experimental_wrapper_exe.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_canonical_soa_vector_get_experimental_wrapper_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 9);
}

TEST_CASE("canonical soa_vector get helper rejects template arguments on non-soa receiver in C++ emitter") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32)}
  return(/std/collections/soa_vector/get<i32>(values, 0i32))
}
)";
  const std::string srcPath =
      writeTemp("compile_canonical_soa_vector_get_non_soa_receiver_exe.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_canonical_soa_vector_get_non_soa_receiver_exe_err.txt").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("get requires soa_vector target") !=
        std::string::npos);
}

TEST_CASE("canonical soa_vector get slash-method reaches field access reject in C++ emitter") {
  const std::string source = R"(
import /std/collections/experimental_soa_vector/*

[struct reflect]
Particle() {
  [i32] x{1i32}
}

[effects(heap_alloc), return<int>]
main() {
  [SoaVector<Particle>] values{soaVectorSingle<Particle>(Particle(9i32))}
  return(values./std/collections/soa_vector/get(0i32).x)
}
)";
  const std::string srcPath =
      writeTemp("compile_canonical_soa_vector_get_slash_method_exe.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_canonical_soa_vector_get_slash_method_exe_err.txt").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("field access requires struct receiver") !=
        std::string::npos);
}

TEST_CASE("canonical soa_vector to_aos slash-method runs in C++ emitter") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_soa_vector/*

[struct reflect]
Particle() {
  [i32] x{1i32}
}

[effects(heap_alloc), return<int>]
main() {
  [SoaVector<Particle>] values{soaVectorSingle<Particle>(Particle(9i32))}
  [vector<Particle>] unpacked{values./std/collections/soa_vector/to_aos()}
  return(count(unpacked))
}
)";
  const std::string srcPath =
      writeTemp("compile_canonical_soa_vector_to_aos_slash_method_exe.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_canonical_soa_vector_to_aos_slash_method_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 1);
}

TEST_CASE("compiles and runs canonical soa_vector ref helper in C++ emitter") {
  const std::string source = R"(
import /std/collections/experimental_soa_vector/*

[struct reflect]
Particle() {
  [i32] x{1i32}
}

[effects(heap_alloc), return<int>]
main() {
  [SoaVector<Particle>] values{soaVectorSingle<Particle>(Particle(9i32))}
  return(/std/collections/soa_vector/ref<Particle>(values, 0i32).x)
}
)";
  const std::string srcPath =
      writeTemp("compile_canonical_soa_vector_ref_experimental_wrapper_exe.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_canonical_soa_vector_ref_experimental_wrapper_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 9);
}

TEST_CASE("compiles and runs canonical soa_vector mutator helpers in C++ emitter") {
  const std::string source = R"(
import /std/collections/experimental_soa_vector/*

[struct reflect]
Particle() {
  [i32] x{1i32}
}

[effects(heap_alloc), return<int>]
main() {
  [SoaVector<Particle> mut] values{soaVectorNew<Particle>()}
  /std/collections/soa_vector/reserve<Particle>(values, 2i32)
  /std/collections/soa_vector/push<Particle>(values, Particle(4i32))
  /std/collections/soa_vector/push<Particle>(values, Particle(9i32))
  return(plus(/std/collections/soa_vector/count<Particle>(values),
              /std/collections/soa_vector/get<Particle>(values, 1i32).x))
}
)";
  const std::string srcPath =
      writeTemp("compile_canonical_soa_vector_mutators_experimental_wrapper_exe.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_canonical_soa_vector_mutators_experimental_wrapper_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 11);
}

TEST_CASE("canonical soa_vector to_aos helper lowers in C++ emitter") {
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
      writeTemp("compile_canonical_soa_vector_to_aos_experimental_wrapper_exe.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_canonical_soa_vector_to_aos_experimental_wrapper_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 1);
}

TEST_CASE("canonical soa_vector to_aos temporaries route through canonical vector capacity in C++ emitter") {
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
  return(/std/collections/vector/capacity(/std/collections/soa_vector/to_aos<Particle>(values)))
}
)";
  const std::string srcPath =
      writeTemp("compile_canonical_soa_vector_to_aos_vector_capacity_experimental_wrapper_exe.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_canonical_soa_vector_to_aos_vector_capacity_experimental_wrapper_exe")
          .string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 1);
}

TEST_CASE("wildcard-imported canonical soa_vector helpers lower in C++ emitter") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/soa_vector/*
import /std/collections/experimental_soa_vector/*

[struct reflect]
Particle() {
  [i32] x{1i32}
}

[effects(heap_alloc), return<int>]
main() {
  [SoaVector<Particle> mut] values{soaVectorNew<Particle>()}
  reserve(values, 2i32)
  push(values, Particle(4i32))
  push(values, Particle(9i32))
  [Particle] first{get(values, 0i32)}
  [Reference<Particle>] second{ref(values, 1i32)}
  [vector<Particle>] unpacked{to_aos(values)}
  return(plus(plus(count(values), plus(first.x, second.x)), count(unpacked)))
}
)";
  const std::string srcPath =
      writeTemp("compile_wildcard_canonical_soa_vector_helpers_experimental_wrapper_exe.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_wildcard_canonical_soa_vector_helpers_experimental_wrapper_exe")
          .string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 17);
}

TEST_CASE("compiles and runs graph-solved direct local-auto vector helper shadows in C++ emitter") {
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
  const std::string exePath =
      (testScratchPath("") / "primec_graph_direct_local_auto_vector_helper_shadows_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 34);
}

TEST_CASE("rejects experimental soa_vector stdlib wide structs on pending width boundary in C++ emitter") {
  const std::string source = R"(
import /std/collections/experimental_soa_vector/*

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
main() {
  [SoaVector<Particle17>] values{soaVectorNew<Particle17>()}
  return(soaVectorCount<Particle17>(values))
}
)";
  const std::string srcPath =
      writeTemp("compile_experimental_soa_vector_wide_pending_exe.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_experimental_soa_vector_wide_pending_exe").string();
  const std::string errPath =
      (testScratchPath("") / "primec_experimental_soa_vector_wide_pending_err.txt").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath + " 2> " + errPath) == 3);
  CHECK(readFile(errPath) == "array index out of bounds\n");
}

TEST_CASE("rejects experimental soa_vector stdlib from-aos helper in C++ emitter before typed bindings support") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_soa_vector/*

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
  const std::string srcPath = writeTemp("compile_experimental_soa_vector_from_aos_exe.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_experimental_soa_vector_from_aos_err.txt").string();
  const std::string compileCmd = "./primec --emit=exe " + srcPath + " --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("native backend only supports numeric/bool/string vector literals") !=
        std::string::npos);
}

TEST_CASE("runs experimental soa_vector stdlib to-aos helper in C++ emitter") {
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
  [SoaVector<Particle>] values{soaVectorNew<Particle>()}
  [vector<Particle>] unpacked{soaVectorToAos<Particle>(values)}
  return(count(unpacked))
}
)";
  const std::string srcPath = writeTemp("compile_experimental_soa_vector_to_aos_exe.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_experimental_soa_vector_to_aos_exe").string();
  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 0);
}

TEST_CASE("runs experimental soa_vector stdlib to-aos method on wrapper surface in C++ emitter") {
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
  [SoaVector<Particle>] values{soaVectorNew<Particle>()}
  [vector<Particle>] unpacked{values.to_aos()}
  return(count(unpacked))
}
)";
  const std::string srcPath = writeTemp("compile_experimental_soa_vector_to_aos_method_exe.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_experimental_soa_vector_to_aos_method_exe").string();
  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 0);
}

TEST_CASE("no-import root soa_vector to_aos bare and direct helper forms run in C++ emitter") {
  const std::string source = R"(
[struct reflect]
Particle() {
  [i32] x{1i32}
}

[effects(heap_alloc), return<int>]
main() {
  [soa_vector<Particle>] values{soa_vector<Particle>()}
  [vector<Particle>] unpackedA{to_aos(values)}
  [vector<Particle>] unpackedB{/to_aos(values)}
  return(plus(count(unpackedA), count(unpackedB)))
}
)";
  const std::string srcPath = writeTemp("compile_root_soa_vector_to_aos_forms_exe.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_root_soa_vector_to_aos_forms_exe").string();
  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 0);
}

TEST_CASE("no-import root soa_vector to_aos method helper forms run in C++ emitter") {
  const std::string source = R"(
[struct reflect]
Particle() {
  [i32] x{1i32}
}

[effects(heap_alloc), return<int>]
main() {
  [soa_vector<Particle>] values{soa_vector<Particle>()}
  [vector<Particle>] unpackedA{values.to_aos()}
  [vector<Particle>] unpackedB{values./to_aos()}
  return(plus(count(unpackedA), count(unpackedB)))
}
)";
  const std::string srcPath = writeTemp("compile_root_soa_vector_to_aos_method_forms_exe.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_root_soa_vector_to_aos_method_forms_exe").string();
  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 0);
}
TEST_CASE("runs experimental soa_vector stdlib non-empty to-aos helper in C++ emitter") {
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
  [SoaVector<Particle>] values{soaVectorSingle<Particle>(Particle(7i32))}
  [vector<Particle>] unpacked{soaVectorToAos<Particle>(values)}
  return(count(unpacked))
}
)";
  const std::string srcPath = writeTemp("compile_experimental_soa_vector_to_aos_non_empty_exe.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_experimental_soa_vector_to_aos_non_empty_exe").string();
  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 1);
}

TEST_CASE("runs experimental soa_vector stdlib non-empty to-aos method on wrapper state in C++ emitter") {
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
  [SoaVector<Particle>] values{soaVectorSingle<Particle>(Particle(7i32))}
  [vector<Particle>] unpacked{values.to_aos()}
  return(count(unpacked))
}
)";
  const std::string srcPath = writeTemp("compile_experimental_soa_vector_to_aos_non_empty_method_exe.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_experimental_soa_vector_to_aos_non_empty_method_exe").string();
  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 1);
}

TEST_CASE("compiles and runs experimental soa_vector stdlib get helper in C++ emitter") {
  const std::string source = R"(
import /std/collections/experimental_soa_vector/*

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
  const std::string srcPath = writeTemp("compile_experimental_soa_vector_get_exe.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_experimental_soa_vector_get_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 7);
}

TEST_CASE("compiles and runs experimental soa_vector stdlib get method in C++ emitter") {
  const std::string source = R"(
import /std/collections/experimental_soa_vector/*

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
  const std::string srcPath = writeTemp("compile_experimental_soa_vector_get_method_exe.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_experimental_soa_vector_get_method_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 7);
}

TEST_CASE("compiles and runs bare soa_vector get helper through helper return in C++ emitter") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_soa_vector/*

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
      writeTemp("compile_experimental_soa_vector_get_helper_return_exe.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_experimental_soa_vector_get_helper_return_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 7);
}

TEST_CASE("compiles and runs global helper-return soa_vector method shadows in C++ emitter") {
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
      writeTemp("compile_experimental_soa_vector_method_shadow_global_helper_return_exe.prime",
                source);
  const std::string exePath =
      (testScratchPath("") /
       "primec_experimental_soa_vector_method_shadow_global_helper_return_exe")
          .string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 131);
}

TEST_CASE("compiles and runs method-like helper-return soa_vector method shadows in C++ emitter") {
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
      writeTemp("compile_experimental_soa_vector_method_shadow_method_like_helper_return_exe.prime",
                source);
  const std::string exePath =
      (testScratchPath("") /
       "primec_experimental_soa_vector_method_shadow_method_like_helper_return_exe")
          .string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 131);
}

TEST_CASE("compiles and runs vector-target old-explicit soa mutator shadows in C++ emitter") {
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
  return(plus(values./soa_vector/push(4i32), values./soa_vector/reserve(6i32)))
}
)";
  const std::string srcPath =
      writeTemp("compile_vector_target_old_explicit_soa_mutator_shadow_exe.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_vector_target_old_explicit_soa_mutator_shadow_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 10);
}

TEST_CASE("compiles and runs vector-target method soa mutator shadows in C++ emitter") {
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
  return(plus(values.push(4i32), values.reserve(6i32)))
}
)";
  const std::string srcPath =
      writeTemp("compile_vector_target_method_soa_mutator_shadow_exe.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_vector_target_method_soa_mutator_shadow_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 10);
}

TEST_CASE("compiles and runs vector-target to_aos helper shadows in C++ emitter") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
/to_aos([vector<i32>] values) {
  return(9i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vectorSingle<i32>(1i32)}
  [int] direct{/to_aos(values)}
  [int] method{values.to_aos()}
  [int] slash{values./to_aos()}
  return(plus(direct, plus(method, slash)))
}
)";
  const std::string srcPath =
      writeTemp("compile_vector_target_to_aos_shadow_exe.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_vector_target_to_aos_shadow_exe").string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 27);
}

TEST_CASE("compiles nested struct-body soa_vector constructor-bearing helper returns in C++ emitter") {
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
      writeTemp("compile_nested_struct_body_soa_vector_constructor_helper_exe.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_nested_struct_body_soa_vector_constructor_helper_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 0);
}

TEST_CASE("compiles nested struct-body soa_vector direct and bound helper expressions in C++ emitter") {
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

[effects(heap_alloc), return<int>]
main() {
  [Holder] holder{Holder()}
  [SoaVector<Particle>] values{holder.cloneValues()}
  return(plus(plus(plus(holder.cloneValues().count(), holder.cloneValues().get(0i32).x),
                    values.ref(0i32).x),
              count(values.to_aos())))
}
)";
  const std::string srcPath =
      writeTemp("compile_nested_struct_body_soa_vector_direct_bound_helpers_exe.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_nested_struct_body_soa_vector_direct_bound_helpers_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 16);
}

TEST_CASE("compiles nested struct-body soa_vector method shadows in C++ emitter") {
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
      writeTemp("compile_nested_struct_body_soa_vector_method_shadows_exe.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_nested_struct_body_soa_vector_method_shadows_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 134);
}

TEST_CASE("compiles and runs explicit method-like helper-return experimental soa_vector to_aos shadow in C++ emitter") {
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

[effects(heap_alloc), return<vector<Particle>>]
/to_aos([SoaVector<Particle>] values) {
  [vector<Particle>, mut] out{vector<Particle>()}
  out.push(Particle(19i32))
  return(out)
}

[effects(heap_alloc), return<int>]
main() {
  [Holder] holder{Holder()}
  [vector<Particle>] values{holder.cloneValues().to_aos()}
  return(count(values))
}
)";
  const std::string srcPath =
      writeTemp("compile_experimental_soa_vector_explicit_method_like_to_aos_shadow_exe.prime",
                source);
  const std::string exePath =
      (testScratchPath("") /
       "primec_experimental_soa_vector_explicit_method_like_to_aos_shadow_exe")
          .string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 1);
}

TEST_CASE("compiles and runs experimental soa_vector stdlib ref helper in C++ emitter") {
  const std::string source = R"(
import /std/collections/experimental_soa_vector/*

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
  const std::string srcPath = writeTemp("compile_experimental_soa_vector_ref_exe.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_experimental_soa_vector_ref_exe").string();
  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 7);
}

TEST_CASE("compiles and runs experimental soa_vector stdlib ref method in C++ emitter") {
  const std::string source = R"(
import /std/collections/experimental_soa_vector/*

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
  const std::string srcPath = writeTemp("compile_experimental_soa_vector_ref_method_exe.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_experimental_soa_vector_ref_method_exe").string();
  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 7);
}

TEST_CASE("compiles and runs experimental soa_vector ref pass-through and return in C++ emitter") {
  const std::string source = R"(
import /std/collections/experimental_soa_vector/*

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
      writeTemp("compile_experimental_soa_vector_ref_passthrough_exe.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_experimental_soa_vector_ref_passthrough_exe").string();
  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 7);
}

TEST_CASE("compiles and runs experimental soa_vector stdlib push and reserve helpers in C++ emitter") {
  const std::string source = R"(
import /std/collections/experimental_soa_vector/*

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
  const std::string srcPath = writeTemp("compile_experimental_soa_vector_push_helpers_exe.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_experimental_soa_vector_push_helpers_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 11);
}

TEST_CASE("compiles and runs experimental soa_vector stdlib push and reserve methods in C++ emitter") {
  const std::string source = R"(
import /std/collections/experimental_soa_vector/*

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
  const std::string srcPath = writeTemp("compile_experimental_soa_vector_push_method_exe.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_experimental_soa_vector_push_method_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 11);
}

TEST_CASE("compiles and runs experimental soa_vector single-field index syntax in C++ emitter") {
  const std::string source = R"(
import /std/collections/experimental_soa_vector/*

[struct reflect]
ScalarBox() {
  [i32] x{1i32}
}

[effects(heap_alloc), return<int>]
main() {
  [SoaVector<ScalarBox> mut] values{soaVectorNew<ScalarBox>()}
  values.push(ScalarBox(4i32))
  values.push(ScalarBox(9i32))
  return(values.x()[1i32])
}
)";
  const std::string srcPath = writeTemp("compile_experimental_soa_vector_single_field_view_exe.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_experimental_soa_vector_single_field_view_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 9);
}

TEST_CASE("compiles and runs experimental soa_vector reflected multi-field index syntax in C++ emitter") {
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
  return(values.y()[1i32])
}
)";
  const std::string srcPath = writeTemp("compile_experimental_soa_vector_field_view_exe.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_experimental_soa_vector_field_view_exe").string();
  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 12);
}

TEST_CASE("compiles and runs experimental soa_vector mutating indexed field writes in C++ emitter") {
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
      writeTemp("compile_experimental_soa_vector_mutating_indexed_field_writes_exe.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_experimental_soa_vector_mutating_indexed_field_writes_exe")
          .string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 52);
}

TEST_CASE("compiles and runs richer borrowed experimental soa_vector mutating indexed field writes in C++ emitter") {
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
      "compile_experimental_soa_vector_richer_borrowed_mutating_indexed_field_writes_exe.prime",
      source);
  const std::string exePath =
      (testScratchPath("") /
       "primec_experimental_soa_vector_richer_borrowed_mutating_indexed_field_writes_exe")
          .string();
  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 36);
}

TEST_CASE("compiles and runs method-like borrowed experimental soa_vector mutating indexed field writes in C++ emitter") {
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
      "compile_experimental_soa_vector_method_like_borrowed_mutating_indexed_field_writes_exe.prime",
      source);
  const std::string exePath =
      (testScratchPath("") /
       "primec_experimental_soa_vector_method_like_borrowed_mutating_indexed_field_writes_exe")
          .string();
  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 104);
}

TEST_CASE("compiles and runs borrowed experimental soa_vector reflected index syntax in C++ emitter") {
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
  [Reference<SoaVector<Particle>>] borrowed{location(values)}
  return(dereference(borrowed).y()[1i32])
}
)";
  const std::string srcPath =
      writeTemp("compile_experimental_soa_vector_borrowed_field_view_exe.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_experimental_soa_vector_borrowed_field_view_exe").string();
  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 12);
}

TEST_CASE("compiles and runs borrowed local experimental soa_vector reflected index syntax in C++ emitter") {
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
  [Reference<SoaVector<Particle>>] borrowed{location(values)}
  return(borrowed.y()[1i32])
}
)";
  const std::string srcPath =
      writeTemp("compile_experimental_soa_vector_borrowed_local_field_view_exe.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_experimental_soa_vector_borrowed_local_field_view_exe").string();
  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 12);
}

TEST_CASE("compiles and runs borrowed helper-return experimental soa_vector reflected index syntax in C++ emitter") {
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
      writeTemp("compile_experimental_soa_vector_borrowed_return_field_view_exe.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_experimental_soa_vector_borrowed_return_field_view_exe").string();
  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 12);
}

TEST_CASE("compiles and runs experimental soa_vector bare get and ref field access in C++ emitter") {
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
  values.push(Particle(7i32, 8i32))
  values.push(Particle(9i32, 12i32))
  return(plus(ref(values, 0i32).y, get(values, 1i32).y))
}
)";
  const std::string srcPath =
      writeTemp("compile_experimental_soa_vector_bare_ref_field_access_exe.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_experimental_soa_vector_bare_ref_field_access_exe").string();
  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 20);
}

TEST_CASE("compiles and runs experimental soa_vector reflected call-form index syntax in C++ emitter") {
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
      writeTemp("compile_experimental_soa_vector_call_form_field_view_exe.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_experimental_soa_vector_call_form_field_view_exe").string();
  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 36);
}

TEST_CASE("compiles and runs experimental soa_vector inline location borrow index syntax in C++ emitter") {
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
      writeTemp("compile_experimental_soa_vector_inline_location_field_view_exe.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_experimental_soa_vector_inline_location_field_view_exe").string();
  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 40);
}

TEST_CASE("compiles and runs dereferenced borrowed helper-return experimental soa_vector reflected index syntax in C++ emitter") {
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
  const std::string srcPath =
      writeTemp("compile_experimental_soa_vector_dereferenced_borrowed_return_field_view_exe.prime", source);
  const std::string exePath =
      (testScratchPath("") /
       "primec_experimental_soa_vector_dereferenced_borrowed_return_field_view_exe")
          .string();
  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 12);
}

TEST_CASE("compiles and runs borrowed helper-return experimental soa_vector get/ref methods in C++ emitter") {
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

[effects(heap_alloc), return<int>]
main() {
  [SoaVector<Particle> mut] values{soaVectorNew<Particle>()}
  values.push(Particle(7i32))
  values.push(Particle(9i32))
  [Particle] first{pickBorrowed(location(values)).get(0i32)}
  [Reference<Particle>] second{pickBorrowed(location(values)).ref(1i32)}
  return(plus(first.x, second.x))
}
)";
  const std::string srcPath =
      writeTemp("compile_experimental_soa_vector_borrowed_return_get_ref_exe.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_experimental_soa_vector_borrowed_return_get_ref_exe").string();
  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 16);
}

TEST_CASE("compiles and runs borrowed local experimental soa_vector read-only methods in C++ emitter") {
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
  [Reference<SoaVector<Particle>>] borrowed{location(values)}
  [Particle] first{borrowed.get(0i32)}
  [Reference<Particle>] second{borrowed.ref(1i32)}
  [Particle] firstBare{get(borrowed, 1i32)}
  [Reference<Particle>] secondBare{ref(dereference(borrowed), 0i32)}
  [vector<Particle>] unpacked{borrowed.to_aos()}
  [vector<Particle>] unpackedBare{to_aos(borrowed)}
  [i32] countBare{count(borrowed)}
  return(plus(plus(first.x, second.x),
              plus(plus(firstBare.x, secondBare.x),
                   plus(count(unpacked),
                        plus(count(unpackedBare), countBare)))))
}
)";
  const std::string srcPath =
      writeTemp("compile_experimental_soa_vector_borrowed_local_methods_exe.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_experimental_soa_vector_borrowed_local_methods_exe").string();
  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 38);
}

TEST_CASE("compiles and runs inline location experimental soa_vector read-only methods in C++ emitter") {
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
      writeTemp("compile_experimental_soa_vector_inline_location_methods_exe.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_experimental_soa_vector_inline_location_methods_exe").string();
  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 40);
}

TEST_CASE("compiles and runs borrowed helper-return experimental soa_vector helper surfaces in C++ emitter") {
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
  [Particle] first{pickBorrowed(location(values)).get(0i32)}
  [Reference<Particle>] second{pickBorrowed(location(values)).ref(1i32)}
  [Particle] firstBare{get(pickBorrowed(location(values)), 1i32)}
  [Reference<Particle>] secondBare{ref(dereference(pickBorrowed(location(values))), 0i32)}
  [vector<Particle>] unpacked{pickBorrowed(location(values)).to_aos()}
  [vector<Particle>] unpackedBare{to_aos(pickBorrowed(location(values)))}
  [i32] countBare{count(pickBorrowed(location(values)))}
  return(plus(plus(first.x, second.x),
              plus(plus(firstBare.x, secondBare.x),
                   plus(count(unpacked),
                        plus(count(unpackedBare), countBare)))))
}
)";
  const std::string srcPath =
      writeTemp("compile_experimental_soa_vector_borrowed_return_methods_exe.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_experimental_soa_vector_borrowed_return_methods_exe").string();
  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 38);
}

TEST_CASE("compiles and runs method-like borrowed helper-return experimental soa_vector helper surfaces in C++ emitter") {
  const std::string source = R"(
import /std/collections/*
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
  [Particle] first{holder.pickBorrowed(location(values)).get(0i32)}
  [Reference<Particle>] second{holder.pickBorrowed(location(values)).ref(1i32)}
  [Particle] firstBare{get(holder.pickBorrowed(location(values)), 1i32)}
  [Reference<Particle>] secondBare{ref(dereference(holder.pickBorrowed(location(values))), 0i32)}
  [i32] countBare{count(holder.pickBorrowed(location(values)))}
  [i32] fieldBareGet{get(holder.pickBorrowed(location(values)), 1i32).y}
  [i32] fieldBareRef{ref(holder.pickBorrowed(location(values)), 0i32).x}
  [i32] fieldMethodRef{holder.pickBorrowed(location(values)).ref(1i32).y}
  [i32] fieldMethod{holder.pickBorrowed(location(values)).y()[1i32]}
  [i32] fieldCall{y(holder.pickBorrowed(location(values)))[0i32]}
  return(plus(plus(first.x, second.x),
              plus(plus(firstBare.x, secondBare.x),
                   plus(countBare,
                        plus(plus(plus(fieldBareGet, fieldBareRef), fieldMethodRef),
                             plus(fieldMethod, fieldCall))))))
}
)";
  const std::string srcPath =
      writeTemp("compile_experimental_soa_vector_method_like_borrowed_return_helpers_exe.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_experimental_soa_vector_method_like_borrowed_return_helpers_exe")
          .string();
  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 85);
}

TEST_CASE("compiles and runs direct return borrowed helper-return experimental soa_vector reads in C++ emitter") {
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
      "compile_experimental_soa_vector_direct_return_borrowed_return_reads_exe.prime",
      source);
  const std::string exePath =
      (testScratchPath("") /
       "primec_experimental_soa_vector_direct_return_borrowed_return_reads_exe")
          .string();
  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 55);
}

TEST_CASE("compiles and runs direct return method-like borrowed helper-return experimental soa_vector reads in C++ emitter") {
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
      "compile_experimental_soa_vector_direct_return_method_like_borrowed_return_reads_exe.prime",
      source);
  const std::string exePath =
      (testScratchPath("") /
       "primec_experimental_soa_vector_direct_return_method_like_borrowed_return_reads_exe")
          .string();
  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 55);
}

TEST_CASE("compiles and runs direct return inline location borrowed helper-return experimental soa_vector reads in C++ emitter") {
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
      "compile_experimental_soa_vector_direct_return_inline_location_borrowed_return_reads_exe.prime",
      source);
  const std::string exePath =
      (testScratchPath("") /
       "primec_experimental_soa_vector_direct_return_inline_location_borrowed_return_reads_exe")
          .string();
  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 52);
}

TEST_CASE("compiles and runs inline location method-like borrowed helper-return experimental soa_vector helpers in C++ emitter") {
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
  [Particle] firstC{get(location(holder.pickBorrowed(location(values))), 1i32)}
  [Reference<Particle>] secondC{ref(location(holder.pickBorrowed(location(values))), 0i32)}
  [vector<Particle>] unpackedA{location(holder.pickBorrowed(location(values))).to_aos()}
  [i32] countA{location(holder.pickBorrowed(location(values))).count()}
  [Particle] firstB{dereference(location(holder.pickBorrowed(location(values)))).get(0i32)}
  [Reference<Particle>] secondB{dereference(location(holder.pickBorrowed(location(values)))).ref(1i32)}
  [Particle] firstD{get(dereference(location(holder.pickBorrowed(location(values)))), 0i32)}
  [Reference<Particle>] secondD{ref(dereference(location(holder.pickBorrowed(location(values)))), 1i32)}
  [vector<Particle>] unpackedB{dereference(location(holder.pickBorrowed(location(values)))).to_aos()}
  [i32] countB{dereference(location(holder.pickBorrowed(location(values)))).count()}
  [i32] fieldBareGet{get(location(holder.pickBorrowed(location(values))), 1i32).y}
  [i32] fieldBareRef{ref(dereference(location(holder.pickBorrowed(location(values)))), 0i32).x}
  [i32] fieldMethodRef{location(holder.pickBorrowed(location(values))).ref(1i32).y}
  [int] helpersA{plus(plus(firstA.x, secondA.x), plus(firstC.x, secondC.y))}
  [int] unpackedCountsA{plus(count(unpackedA), countA)}
  [int] helpersB{plus(plus(firstB.x, secondB.x), plus(firstD.x, secondD.y))}
  [int] unpackedCountsB{plus(count(unpackedB), countB)}
  [int] fieldTotals{
    plus(location(holder.pickBorrowed(location(values))).y()[0i32],
         plus(dereference(location(holder.pickBorrowed(location(values)))).y()[1i32],
              plus(y(location(holder.pickBorrowed(location(values))))[0i32],
                   y(dereference(location(holder.pickBorrowed(location(values)))))[1i32])))
  }
  [int] total{
    plus(helpersA,
         plus(unpackedCountsA,
              plus(helpersB,
                   plus(unpackedCountsB,
                        plus(plus(plus(fieldBareGet, fieldBareRef), fieldMethodRef), fieldTotals)))))
  }
  return(total)
}
)";
  const std::string srcPath = writeTemp(
      "compile_experimental_soa_vector_inline_location_method_like_borrowed_return_helpers_exe.prime",
      source);
  const std::string exePath =
      (testScratchPath("") /
       "primec_experimental_soa_vector_inline_location_method_like_borrowed_return_helpers_exe")
          .string();
  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 147);
}

TEST_CASE("compiles and runs direct return inline location method-like borrowed helper-return experimental soa_vector reads in C++ emitter") {
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
      "compile_experimental_soa_vector_direct_return_inline_location_method_like_borrowed_return_reads_exe.prime",
      source);
  const std::string exePath =
      (testScratchPath("") /
       "primec_experimental_soa_vector_direct_return_inline_location_method_like_borrowed_return_reads_exe")
          .string();
  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 52);
}

TEST_CASE("compiles and runs inline location borrowed helper-return experimental soa_vector helpers in C++ emitter") {
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
  [Particle] firstC{get(location(pickBorrowed(location(values))), 1i32)}
  [Reference<Particle>] secondC{ref(location(pickBorrowed(location(values))), 0i32)}
  [vector<Particle>] unpackedA{location(pickBorrowed(location(values))).to_aos()}
  [i32] countA{location(pickBorrowed(location(values))).count()}
  [Particle] firstB{dereference(location(pickBorrowed(location(values)))).get(0i32)}
  [Reference<Particle>] secondB{dereference(location(pickBorrowed(location(values)))).ref(1i32)}
  [Particle] firstD{get(dereference(location(pickBorrowed(location(values)))), 0i32)}
  [Reference<Particle>] secondD{ref(dereference(location(pickBorrowed(location(values)))), 1i32)}
  [vector<Particle>] unpackedB{dereference(location(pickBorrowed(location(values)))).to_aos()}
  [i32] countB{dereference(location(pickBorrowed(location(values)))).count()}
  [int] total{
    plus(plus(firstA.x, secondA.x),
         plus(plus(firstC.x, secondC.y),
              plus(count(unpackedA),
                   plus(countA,
                        plus(plus(firstB.x, secondB.x),
                             plus(plus(firstD.x, secondD.y),
                                  plus(count(unpackedB),
                                       plus(countB,
                                            plus(location(pickBorrowed(location(values))).y()[0i32],
                                                 plus(dereference(location(pickBorrowed(location(values)))).y()[1i32],
                                                      plus(y(location(pickBorrowed(location(values))))[0i32],
                                                           y(dereference(location(pickBorrowed(location(values)))))[1i32])))))))))))
  }
  return(total)
}
)";
  const std::string srcPath =
      writeTemp("compile_experimental_soa_vector_inline_location_borrowed_return_helpers_exe.prime",
                source);
  const std::string exePath =
      (testScratchPath("") /
       "primec_experimental_soa_vector_inline_location_borrowed_return_helpers_exe")
          .string();
  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 116);
}

TEST_CASE("compiles and runs experimental soa storage helpers in C++ emitter") {
  const std::string source = R"(
import /std/collections/experimental_soa_storage/*

[effects(heap_alloc), return<int>]
main() {
  [SoaColumn<i32> mut] values{soaColumnNew<i32>()}
  soaColumnReserve<i32>(values, 3i32)
  soaColumnPush<i32>(values, 2i32)
  soaColumnPush<i32>(values, 5i32)
  soaColumnWrite<i32>(values, 1i32, 7i32)
  [i32] total{plus(soaColumnRead<i32>(values, 0i32),
                   plus(soaColumnRead<i32>(values, 1i32),
                        plus(soaColumnCount<i32>(values), soaColumnCapacity<i32>(values))))}
  soaColumnClear<i32>(values)
  return(plus(total, soaColumnCount<i32>(values)))
}
)";
  const std::string srcPath = writeTemp("compile_experimental_soa_storage_exe.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_experimental_soa_storage_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 14);
}

TEST_CASE("compiles and runs experimental soa storage borrowed ref helper in C++ emitter") {
  const std::string source = R"(
import /std/collections/experimental_soa_storage/*

[effects(heap_alloc), return<int>]
main() {
  [SoaColumn<i32> mut] values{soaColumnNew<i32>()}
  soaColumnPush<i32>(values, 2i32)
  soaColumnPush<i32>(values, 5i32)
  [Reference<i32>] borrowed{soaColumnRef<i32>(values, 1i32)}
  return(plus(dereference(borrowed), soaColumnCount<i32>(values)))
}
)";
  const std::string srcPath = writeTemp("compile_experimental_soa_storage_ref_exe.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_experimental_soa_storage_ref_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 7);
}

TEST_CASE("compiles and runs experimental soa storage borrowed view helper in C++ emitter") {
  const std::string source = R"(
import /std/collections/experimental_soa_storage/*

[effects(heap_alloc), return<int>]
main() {
  [SoaColumn<i32> mut] values{soaColumnNew<i32>()}
  soaColumnPush<i32>(values, 2i32)
  soaColumnPush<i32>(values, 5i32)
  [SoaColumn<i32> mut] view{soaColumnBorrowedView<i32>(values)}
  soaColumnWrite<i32>(view, 1i32, 7i32)
  return(plus(soaColumnRead<i32>(view, 1i32), soaColumnCount<i32>(values)))
}
)";
  const std::string srcPath = writeTemp("compile_experimental_soa_storage_view_exe.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_experimental_soa_storage_view_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 9);
}

TEST_CASE("rejects experimental soa storage reserve overflow in C++ emitter") {
  const std::string source = R"(
import /std/collections/experimental_soa_storage/*

[effects(heap_alloc), return<int>]
main() {
  [SoaColumn<i32> mut] values{soaColumnNew<i32>()}
  soaColumnReserve<i32>(values, 1073741824i32)
  return(0i32)
}
)";
  const std::string srcPath =
      writeTemp("compile_experimental_soa_storage_reserve_overflow_exe.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_experimental_soa_storage_reserve_overflow_exe").string();
  const std::string errPath =
      (testScratchPath("") / "primec_experimental_soa_storage_reserve_overflow_err.txt").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string runCmd = exePath + " 2> " + errPath;
  CHECK(runCommand(runCmd) == 3);
  CHECK(readFile(errPath) == "vector reserve allocation failed (out of memory)\n");
}

TEST_CASE("compiles and runs experimental two-column soa storage helpers in C++ emitter") {
  const std::string source = R"(
import /std/collections/experimental_soa_storage/*

[effects(heap_alloc), return<int>]
main() {
  [SoaColumns2<i32, i32> mut] values{soaColumns2New<i32, i32>()}
  soaColumns2Reserve<i32, i32>(values, 3i32)
  soaColumns2Push<i32, i32>(values, 2i32, 5i32)
  soaColumns2Push<i32, i32>(values, 7i32, 11i32)
  soaColumns2Write<i32, i32>(values, 1i32, 13i32, 17i32)
  [i32] total{plus(soaColumns2ReadFirst<i32, i32>(values, 0i32),
                   plus(soaColumns2ReadSecond<i32, i32>(values, 1i32),
                        plus(soaColumns2Count<i32, i32>(values),
                             soaColumns2Capacity<i32, i32>(values))))}
  soaColumns2Clear<i32, i32>(values)
  return(plus(total, soaColumns2Count<i32, i32>(values)))
}
)";
  const std::string srcPath = writeTemp("compile_experimental_soa_storage_two_columns_exe.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_experimental_soa_storage_two_columns_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 24);
}

TEST_CASE("compiles and runs experimental three-column soa storage helpers in C++ emitter") {
  const std::string source = R"(
import /std/collections/experimental_soa_storage/*

[effects(heap_alloc), return<int>]
main() {
  [SoaColumns3<i32, i32, i32> mut] values{soaColumns3New<i32, i32, i32>()}
  soaColumns3Reserve<i32, i32, i32>(values, 4i32)
  soaColumns3Push<i32, i32, i32>(values, 2i32, 5i32, 7i32)
  soaColumns3Push<i32, i32, i32>(values, 11i32, 13i32, 17i32)
  soaColumns3Write<i32, i32, i32>(values, 1i32, 19i32, 23i32, 29i32)
  [i32] total{plus(soaColumns3ReadFirst<i32, i32, i32>(values, 0i32),
                   plus(soaColumns3ReadSecond<i32, i32, i32>(values, 1i32),
                        plus(soaColumns3ReadThird<i32, i32, i32>(values, 1i32),
                             plus(soaColumns3Count<i32, i32, i32>(values),
                                  soaColumns3Capacity<i32, i32, i32>(values)))))}
  soaColumns3Clear<i32, i32, i32>(values)
  return(plus(total, soaColumns3Count<i32, i32, i32>(values)))
}
)";
  const std::string srcPath = writeTemp("compile_experimental_soa_storage_three_columns_exe.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_experimental_soa_storage_three_columns_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 60);
}

TEST_CASE("compiles and runs experimental four-column soa storage helpers in C++ emitter") {
  const std::string source = R"(
import /std/collections/experimental_soa_storage/*

[effects(heap_alloc), return<int>]
main() {
  [SoaColumns4<i32, i32, i32, i32> mut] values{soaColumns4New<i32, i32, i32, i32>()}
  soaColumns4Reserve<i32, i32, i32, i32>(values, 4i32)
  soaColumns4Push<i32, i32, i32, i32>(values, 2i32, 3i32, 5i32, 7i32)
  soaColumns4Push<i32, i32, i32, i32>(values, 11i32, 13i32, 17i32, 19i32)
  soaColumns4Write<i32, i32, i32, i32>(values, 1i32, 23i32, 29i32, 31i32, 37i32)
  [i32] total{plus(soaColumns4ReadFirst<i32, i32, i32, i32>(values, 0i32),
                   plus(soaColumns4ReadSecond<i32, i32, i32, i32>(values, 1i32),
                        plus(soaColumns4ReadThird<i32, i32, i32, i32>(values, 1i32),
                             plus(soaColumns4ReadFourth<i32, i32, i32, i32>(values, 1i32),
                                  plus(soaColumns4Count<i32, i32, i32, i32>(values),
                                       soaColumns4Capacity<i32, i32, i32, i32>(values))))))}
  soaColumns4Clear<i32, i32, i32, i32>(values)
  return(plus(total, soaColumns4Count<i32, i32, i32, i32>(values)))
}
)";
  const std::string srcPath = writeTemp("compile_experimental_soa_storage_four_columns_exe.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_experimental_soa_storage_four_columns_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 105);
}

TEST_CASE("compiles and runs experimental five-column soa storage helpers in C++ emitter") {
  const std::string source = R"(
import /std/collections/experimental_soa_storage/*

[effects(heap_alloc), return<int>]
main() {
  [SoaColumns5<i32, i32, i32, i32, i32> mut] values{soaColumns5New<i32, i32, i32, i32, i32>()}
  soaColumns5Reserve<i32, i32, i32, i32, i32>(values, 4i32)
  soaColumns5Push<i32, i32, i32, i32, i32>(values, 2i32, 3i32, 5i32, 7i32, 11i32)
  soaColumns5Push<i32, i32, i32, i32, i32>(values, 13i32, 17i32, 19i32, 23i32, 29i32)
  soaColumns5Write<i32, i32, i32, i32, i32>(values, 1i32, 31i32, 37i32, 41i32, 43i32, 47i32)
  [i32] total{plus(soaColumns5ReadFirst<i32, i32, i32, i32, i32>(values, 0i32),
                   plus(soaColumns5ReadSecond<i32, i32, i32, i32, i32>(values, 1i32),
                        plus(soaColumns5ReadThird<i32, i32, i32, i32, i32>(values, 1i32),
                             plus(soaColumns5ReadFourth<i32, i32, i32, i32, i32>(values, 1i32),
                                  plus(soaColumns5ReadFifth<i32, i32, i32, i32, i32>(values, 1i32),
                                       plus(soaColumns5Count<i32, i32, i32, i32, i32>(values),
                                            soaColumns5Capacity<i32, i32, i32, i32, i32>(values)))))))}
  soaColumns5Clear<i32, i32, i32, i32, i32>(values)
  return(plus(total, soaColumns5Count<i32, i32, i32, i32, i32>(values)))
}
)";
  const std::string srcPath = writeTemp("compile_experimental_soa_storage_five_columns_exe.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_experimental_soa_storage_five_columns_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 176);
}

TEST_CASE("compiles and runs experimental six-column soa storage helpers in C++ emitter") {
  const std::string source = R"(
import /std/collections/experimental_soa_storage/*

[effects(heap_alloc), return<int>]
main() {
  [SoaColumns6<i32, i32, i32, i32, i32, i32> mut] values{soaColumns6New<i32, i32, i32, i32, i32, i32>()}
  soaColumns6Reserve<i32, i32, i32, i32, i32, i32>(values, 4i32)
  soaColumns6Push<i32, i32, i32, i32, i32, i32>(values, 2i32, 3i32, 5i32, 7i32, 11i32, 13i32)
  soaColumns6Push<i32, i32, i32, i32, i32, i32>(values, 13i32, 17i32, 19i32, 23i32, 29i32, 31i32)
  soaColumns6Write<i32, i32, i32, i32, i32, i32>(values, 1i32, 23i32, 29i32, 31i32, 37i32, 41i32, 43i32)
  [i32] total{plus(soaColumns6ReadFirst<i32, i32, i32, i32, i32, i32>(values, 0i32),
                   plus(soaColumns6ReadSecond<i32, i32, i32, i32, i32, i32>(values, 1i32),
                        plus(soaColumns6ReadThird<i32, i32, i32, i32, i32, i32>(values, 1i32),
                             plus(soaColumns6ReadFourth<i32, i32, i32, i32, i32, i32>(values, 1i32),
                                  plus(soaColumns6ReadFifth<i32, i32, i32, i32, i32, i32>(values, 1i32),
                                       plus(soaColumns6ReadSixth<i32, i32, i32, i32, i32, i32>(values, 1i32),
                                            plus(soaColumns6Count<i32, i32, i32, i32, i32, i32>(values),
                                                 soaColumns6Capacity<i32, i32, i32, i32, i32, i32>(values))))))))}
  soaColumns6Clear<i32, i32, i32, i32, i32, i32>(values)
  return(plus(total, soaColumns6Count<i32, i32, i32, i32, i32, i32>(values)))
}
)";
  const std::string srcPath = writeTemp("compile_experimental_soa_storage_six_columns_exe.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_experimental_soa_storage_six_columns_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 189);
}

TEST_CASE("compiles and runs experimental seven-column soa storage helpers in C++ emitter") {
  const std::string source = R"(
import /std/collections/experimental_soa_storage/*

[effects(heap_alloc), return<int>]
main() {
  [SoaColumns7<i32, i32, i32, i32, i32, i32, i32> mut] values{soaColumns7New<i32, i32, i32, i32, i32, i32, i32>()}
  soaColumns7Reserve<i32, i32, i32, i32, i32, i32, i32>(values, 4i32)
  soaColumns7Push<i32, i32, i32, i32, i32, i32, i32>(values, 2i32, 3i32, 5i32, 7i32, 11i32, 13i32, 17i32)
  soaColumns7Push<i32, i32, i32, i32, i32, i32, i32>(values, 19i32, 23i32, 29i32, 31i32, 37i32, 41i32, 43i32)
  soaColumns7Write<i32, i32, i32, i32, i32, i32, i32>(values, 1i32, 19i32, 23i32, 29i32, 31i32, 37i32, 41i32, 43i32)
  [i32] total{plus(soaColumns7ReadFirst<i32, i32, i32, i32, i32, i32, i32>(values, 0i32),
                   plus(soaColumns7ReadSecond<i32, i32, i32, i32, i32, i32, i32>(values, 1i32),
                        plus(soaColumns7ReadThird<i32, i32, i32, i32, i32, i32, i32>(values, 1i32),
                             plus(soaColumns7ReadFourth<i32, i32, i32, i32, i32, i32, i32>(values, 1i32),
                                  plus(soaColumns7ReadFifth<i32, i32, i32, i32, i32, i32, i32>(values, 1i32),
                                       plus(soaColumns7ReadSixth<i32, i32, i32, i32, i32, i32, i32>(values, 1i32),
                                            plus(soaColumns7ReadSeventh<i32, i32, i32, i32, i32, i32, i32>(values, 1i32),
                                                 plus(soaColumns7Count<i32, i32, i32, i32, i32, i32, i32>(values),
                                                      soaColumns7Capacity<i32, i32, i32, i32, i32, i32, i32>(values)))))))))}
  soaColumns7Clear<i32, i32, i32, i32, i32, i32, i32>(values)
  return(plus(total, soaColumns7Count<i32, i32, i32, i32, i32, i32, i32>(values)))
}
)";
  const std::string srcPath = writeTemp("compile_experimental_soa_storage_seven_columns_exe.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_experimental_soa_storage_seven_columns_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 212);
}

TEST_CASE("compiles and runs experimental eight-column soa storage helpers in C++ emitter") {
  const std::string source = R"(
import /std/collections/experimental_soa_storage/*

[effects(heap_alloc), return<int>]
main() {
  [SoaColumns8<i32, i32, i32, i32, i32, i32, i32, i32> mut] values{soaColumns8New<i32, i32, i32, i32, i32, i32, i32, i32>()}
  soaColumns8Reserve<i32, i32, i32, i32, i32, i32, i32, i32>(values, 4i32)
  soaColumns8Push<i32, i32, i32, i32, i32, i32, i32, i32>(values, 2i32, 3i32, 5i32, 7i32, 11i32, 13i32, 17i32, 19i32)
  soaColumns8Push<i32, i32, i32, i32, i32, i32, i32, i32>(values, 17i32, 19i32, 23i32, 29i32, 31i32, 37i32, 41i32, 43i32)
  soaColumns8Write<i32, i32, i32, i32, i32, i32, i32, i32>(values, 1i32, 17i32, 19i32, 23i32, 29i32, 31i32, 37i32, 41i32, 43i32)
  [i32] total{plus(soaColumns8ReadFirst<i32, i32, i32, i32, i32, i32, i32, i32>(values, 0i32),
                   plus(soaColumns8ReadSecond<i32, i32, i32, i32, i32, i32, i32, i32>(values, 1i32),
                        plus(soaColumns8ReadThird<i32, i32, i32, i32, i32, i32, i32, i32>(values, 1i32),
                             plus(soaColumns8ReadFourth<i32, i32, i32, i32, i32, i32, i32, i32>(values, 1i32),
                                  plus(soaColumns8ReadFifth<i32, i32, i32, i32, i32, i32, i32, i32>(values, 1i32),
                                       plus(soaColumns8ReadSixth<i32, i32, i32, i32, i32, i32, i32, i32>(values, 1i32),
                                            plus(soaColumns8ReadSeventh<i32, i32, i32, i32, i32, i32, i32, i32>(values, 1i32),
                                                 plus(soaColumns8ReadEighth<i32, i32, i32, i32, i32, i32, i32, i32>(values, 1i32),
                                                      plus(soaColumns8Count<i32, i32, i32, i32, i32, i32, i32, i32>(values),
                                                           soaColumns8Capacity<i32, i32, i32, i32, i32, i32, i32, i32>(values))))))))))}
  soaColumns8Clear<i32, i32, i32, i32, i32, i32, i32, i32>(values)
  return(plus(total, soaColumns8Count<i32, i32, i32, i32, i32, i32, i32, i32>(values)))
}
)";
  const std::string srcPath = writeTemp("compile_experimental_soa_storage_eight_columns_exe.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_experimental_soa_storage_eight_columns_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 231);
}

TEST_CASE("compiles and runs experimental nine-column soa storage helpers in C++ emitter") {
  const std::string source = R"(
import /std/collections/experimental_soa_storage/*

[effects(heap_alloc), return<int>]
main() {
  [SoaColumns9<i32, i32, i32, i32, i32, i32, i32, i32, i32> mut] values{soaColumns9New<i32, i32, i32, i32, i32, i32, i32, i32, i32>()}
  soaColumns9Reserve<i32, i32, i32, i32, i32, i32, i32, i32, i32>(values, 4i32)
  soaColumns9Push<i32, i32, i32, i32, i32, i32, i32, i32, i32>(values, 2i32, 3i32, 5i32, 7i32, 11i32, 13i32, 17i32, 19i32, 23i32)
  soaColumns9Push<i32, i32, i32, i32, i32, i32, i32, i32, i32>(values, 29i32, 31i32, 37i32, 41i32, 43i32, 47i32, 53i32, 59i32, 61i32)
  soaColumns9Write<i32, i32, i32, i32, i32, i32, i32, i32, i32>(values, 1i32, 6i32, 3i32, 5i32, 7i32, 11i32, 13i32, 17i32, 19i32, 23i32)
  [i32 mut] total{soaColumns9ReadFirst<i32, i32, i32, i32, i32, i32, i32, i32, i32>(values, 0i32)}
  assign(total, plus(total, soaColumns9ReadSecond<i32, i32, i32, i32, i32, i32, i32, i32, i32>(values, 1i32)))
  assign(total, plus(total, soaColumns9ReadFifth<i32, i32, i32, i32, i32, i32, i32, i32, i32>(values, 1i32)))
  assign(total, plus(total, soaColumns9ReadNinth<i32, i32, i32, i32, i32, i32, i32, i32, i32>(values, 1i32)))
  assign(total, plus(total, soaColumns9Count<i32, i32, i32, i32, i32, i32, i32, i32, i32>(values)))
  assign(total, plus(total, soaColumns9Capacity<i32, i32, i32, i32, i32, i32, i32, i32, i32>(values)))
  soaColumns9Clear<i32, i32, i32, i32, i32, i32, i32, i32, i32>(values)
  return(plus(total, soaColumns9Count<i32, i32, i32, i32, i32, i32, i32, i32, i32>(values)))
}
)";
  const std::string srcPath = writeTemp("compile_experimental_soa_storage_nine_columns_exe.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_experimental_soa_storage_nine_columns_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 45);
}

TEST_CASE("compiles and runs experimental ten-column soa storage helpers in C++ emitter") {
  const std::string source = R"(
import /std/collections/experimental_soa_storage/*

[effects(heap_alloc), return<int>]
main() {
  [SoaColumns10<i32, i32, i32, i32, i32, i32, i32, i32, i32, i32> mut] values{soaColumns10New<i32, i32, i32, i32, i32, i32, i32, i32, i32, i32>()}
  soaColumns10Reserve<i32, i32, i32, i32, i32, i32, i32, i32, i32, i32>(values, 4i32)
  soaColumns10Push<i32, i32, i32, i32, i32, i32, i32, i32, i32, i32>(values, 2i32, 3i32, 5i32, 7i32, 11i32, 13i32, 17i32, 19i32, 23i32, 29i32)
  soaColumns10Push<i32, i32, i32, i32, i32, i32, i32, i32, i32, i32>(values, 31i32, 37i32, 41i32, 43i32, 47i32, 53i32, 59i32, 61i32, 67i32, 71i32)
  soaColumns10Write<i32, i32, i32, i32, i32, i32, i32, i32, i32, i32>(values, 1i32, 6i32, 3i32, 5i32, 7i32, 11i32, 13i32, 17i32, 19i32, 23i32, 29i32)
  [i32 mut] total{soaColumns10ReadFirst<i32, i32, i32, i32, i32, i32, i32, i32, i32, i32>(values, 0i32)}
  assign(total, plus(total, soaColumns10ReadSecond<i32, i32, i32, i32, i32, i32, i32, i32, i32, i32>(values, 1i32)))
  assign(total, plus(total, soaColumns10ReadFifth<i32, i32, i32, i32, i32, i32, i32, i32, i32, i32>(values, 1i32)))
  assign(total, plus(total, soaColumns10ReadNinth<i32, i32, i32, i32, i32, i32, i32, i32, i32, i32>(values, 1i32)))
  assign(total, plus(total, soaColumns10ReadTenth<i32, i32, i32, i32, i32, i32, i32, i32, i32, i32>(values, 1i32)))
  assign(total, plus(total, soaColumns10Count<i32, i32, i32, i32, i32, i32, i32, i32, i32, i32>(values)))
  assign(total, plus(total, soaColumns10Capacity<i32, i32, i32, i32, i32, i32, i32, i32, i32, i32>(values)))
  soaColumns10Clear<i32, i32, i32, i32, i32, i32, i32, i32, i32, i32>(values)
  return(plus(total, soaColumns10Count<i32, i32, i32, i32, i32, i32, i32, i32, i32, i32>(values)))
}
)";
  const std::string srcPath = writeTemp("compile_experimental_soa_storage_ten_columns_exe.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_experimental_soa_storage_ten_columns_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 74);
}

TEST_CASE("emits experimental eleven-column soa storage helpers in C++ emitter") {
  const std::string source = R"(
import /std/collections/experimental_soa_storage/*

[effects(heap_alloc), return<int>]
main() {
  [SoaColumns11<i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32> mut] values{soaColumns11New<i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32>()}
  soaColumns11Reserve<i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32>(values, 4i32)
  soaColumns11Push<i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32>(values, 2i32, 3i32, 5i32, 7i32, 11i32, 13i32, 17i32, 19i32, 23i32, 29i32, 31i32)
  soaColumns11Push<i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32>(values, 37i32, 41i32, 43i32, 47i32, 53i32, 59i32, 61i32, 67i32, 71i32, 73i32, 79i32)
  soaColumns11Write<i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32>(values, 1i32, 6i32, 3i32, 5i32, 7i32, 11i32, 13i32, 17i32, 19i32, 23i32, 29i32, 31i32)
  [i32 mut] total{soaColumns11ReadSecond<i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32>(values, 1i32)}
  assign(total, plus(total, soaColumns11ReadEleventh<i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32>(values, 1i32)))
  return(total)
}
)";
  const std::string srcPath = writeTemp("emit_experimental_soa_storage_eleven_columns_cpp.prime", source);
  const std::string cppPath =
      (testScratchPath("") / "primec_experimental_soa_storage_eleven_columns.cpp").string();

  const std::string emitCmd = "./primec --emit=cpp " + srcPath + " -o " + cppPath + " --entry /main";
  CHECK(runCommand(emitCmd) == 0);
}

TEST_CASE("emits experimental twelve-column soa storage helpers in C++ emitter") {
  const std::string source = R"(
import /std/collections/experimental_soa_storage/*

[effects(heap_alloc), return<int>]
main() {
  [SoaColumns12<i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32> mut] values{soaColumns12New<i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32>()}
  soaColumns12Reserve<i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32>(values, 4i32)
  soaColumns12Push<i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32>(values, 2i32, 3i32, 5i32, 7i32, 11i32, 13i32, 17i32, 19i32, 23i32, 29i32, 31i32, 37i32)
  soaColumns12Push<i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32>(values, 41i32, 43i32, 47i32, 53i32, 59i32, 61i32, 67i32, 71i32, 73i32, 79i32, 83i32, 89i32)
  soaColumns12Write<i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32>(values, 1i32, 3i32, 6i32, 5i32, 7i32, 11i32, 13i32, 17i32, 19i32, 23i32, 29i32, 31i32, 41i32)
  [i32 mut] total{soaColumns12ReadSecond<i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32>(values, 1i32)}
  assign(total, plus(total, soaColumns12ReadTwelfth<i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32>(values, 1i32)))
  return(total)
}
)";
  const std::string srcPath = writeTemp("emit_experimental_soa_storage_twelve_columns_cpp.prime", source);
  const std::string cppPath =
      (testScratchPath("") / "primec_experimental_soa_storage_twelve_columns.cpp").string();

  const std::string emitCmd = "./primec --emit=cpp " + srcPath + " -o " + cppPath + " --entry /main";
  CHECK(runCommand(emitCmd) == 0);
}

TEST_CASE("emits experimental thirteen-column soa storage helpers in C++ emitter") {
  const std::string source = R"(
import /std/collections/experimental_soa_storage/*

[effects(heap_alloc), return<int>]
main() {
  [SoaColumns13<i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32> mut] values{soaColumns13New<i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32>()}
  soaColumns13Reserve<i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32>(values, 4i32)
  soaColumns13Push<i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32>(values, 2i32, 3i32, 5i32, 7i32, 11i32, 13i32, 17i32, 19i32, 23i32, 29i32, 31i32, 37i32, 41i32)
  soaColumns13Push<i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32>(values, 43i32, 47i32, 53i32, 59i32, 61i32, 67i32, 71i32, 73i32, 79i32, 83i32, 89i32, 97i32, 101i32)
  soaColumns13Write<i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32>(values, 1i32, 3i32, 6i32, 5i32, 7i32, 11i32, 13i32, 17i32, 19i32, 23i32, 29i32, 31i32, 41i32, 43i32)
  [i32 mut] total{soaColumns13ReadSecond<i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32>(values, 1i32)}
  assign(total, plus(total, soaColumns13ReadThirteenth<i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32>(values, 1i32)))
  return(total)
}
)";
  const std::string srcPath = writeTemp("emit_experimental_soa_storage_thirteen_columns_cpp.prime", source);
  const std::string cppPath =
      (testScratchPath("") / "primec_experimental_soa_storage_thirteen_columns.cpp").string();

  const std::string emitCmd = "./primec --emit=cpp " + srcPath + " -o " + cppPath + " --entry /main";
  CHECK(runCommand(emitCmd) == 0);
}

TEST_CASE("emits experimental fourteen-column soa storage helpers in C++ emitter") {
  const std::string source = R"(
import /std/collections/experimental_soa_storage/*

[effects(heap_alloc), return<int>]
main() {
  [SoaColumns14<i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32> mut] values{soaColumns14New<i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32>()}
  soaColumns14Reserve<i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32>(values, 4i32)
  soaColumns14Push<i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32>(values, 2i32, 3i32, 5i32, 7i32, 11i32, 13i32, 17i32, 19i32, 23i32, 29i32, 31i32, 37i32, 41i32, 43i32)
  soaColumns14Push<i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32>(values, 47i32, 53i32, 59i32, 61i32, 67i32, 71i32, 73i32, 79i32, 83i32, 89i32, 97i32, 101i32, 103i32, 107i32)
  soaColumns14Write<i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32>(values, 1i32, 3i32, 6i32, 5i32, 7i32, 11i32, 13i32, 17i32, 19i32, 23i32, 29i32, 31i32, 41i32, 43i32, 47i32)
  [i32 mut] total{soaColumns14ReadSecond<i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32>(values, 1i32)}
  assign(total, plus(total, soaColumns14ReadFourteenth<i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32>(values, 1i32)))
  return(total)
}
)";
  const std::string srcPath = writeTemp("emit_experimental_soa_storage_fourteen_columns_cpp.prime", source);
  const std::string cppPath =
      (testScratchPath("") / "primec_experimental_soa_storage_fourteen_columns.cpp").string();

  const std::string emitCmd = "./primec --emit=cpp " + srcPath + " -o " + cppPath + " --entry /main";
  CHECK(runCommand(emitCmd) == 0);
}

TEST_CASE("emits experimental fifteen-column soa storage helpers in C++ emitter") {
  const std::string source = R"(
import /std/collections/experimental_soa_storage/*

[effects(heap_alloc), return<int>]
main() {
  [SoaColumns15<i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32> mut] values{soaColumns15New<i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32>()}
  soaColumns15Reserve<i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32>(values, 4i32)
  soaColumns15Push<i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32>(values, 2i32, 3i32, 5i32, 7i32, 11i32, 13i32, 17i32, 19i32, 23i32, 29i32, 31i32, 37i32, 41i32, 43i32, 47i32)
  soaColumns15Push<i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32>(values, 53i32, 59i32, 61i32, 67i32, 71i32, 73i32, 79i32, 83i32, 89i32, 97i32, 101i32, 103i32, 107i32, 109i32, 113i32)
  soaColumns15Write<i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32>(values, 1i32, 3i32, 6i32, 5i32, 7i32, 11i32, 13i32, 17i32, 19i32, 23i32, 29i32, 31i32, 41i32, 43i32, 47i32, 53i32)
  [i32 mut] total{soaColumns15ReadSecond<i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32>(values, 1i32)}
  assign(total, plus(total, soaColumns15ReadFifteenth<i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32>(values, 1i32)))
  return(total)
}
)";
  const std::string srcPath = writeTemp("emit_experimental_soa_storage_fifteen_columns_cpp.prime", source);
  const std::string cppPath =
      (testScratchPath("") / "primec_experimental_soa_storage_fifteen_columns.cpp").string();

  const std::string emitCmd = "./primec --emit=cpp " + srcPath + " -o " + cppPath + " --entry /main";
  CHECK(runCommand(emitCmd) == 0);
}


TEST_CASE("emits experimental sixteen-column soa storage helpers in C++ emitter") {
  const std::string source = R"(
import /std/collections/experimental_soa_storage/*

[effects(heap_alloc), return<int>]
main() {
  [SoaColumns16<i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32> mut] values{soaColumns16New<i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32>()}
  soaColumns16Reserve<i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32>(values, 4i32)
  soaColumns16Push<i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32>(values, 2i32, 3i32, 5i32, 7i32, 11i32, 13i32, 17i32, 19i32, 23i32, 29i32, 31i32, 37i32, 41i32, 43i32, 47i32, 53i32)
  soaColumns16Push<i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32>(values, 59i32, 61i32, 67i32, 71i32, 73i32, 79i32, 83i32, 89i32, 97i32, 101i32, 103i32, 107i32, 109i32, 113i32, 127i32, 131i32)
  soaColumns16Write<i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32>(values, 1i32, 3i32, 6i32, 5i32, 7i32, 11i32, 13i32, 17i32, 19i32, 23i32, 29i32, 31i32, 41i32, 43i32, 47i32, 53i32, 137i32)
  [i32 mut] total{soaColumns16ReadSecond<i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32>(values, 1i32)}
  assign(total, plus(total, soaColumns16ReadSixteenth<i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32>(values, 1i32)))
  return(total)
}
)";
  const std::string srcPath = writeTemp("emit_experimental_soa_storage_sixteen_columns_cpp.prime", source);
  const std::string cppPath =
      (testScratchPath("") / "primec_experimental_soa_storage_sixteen_columns.cpp").string();

  const std::string emitCmd = "./primec --emit=cpp " + srcPath + " -o " + cppPath + " --entry /main";
  CHECK(runCommand(emitCmd) == 0);
}

TEST_CASE("compiles and runs string-keyed map literals in C++ emitter") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
main() {
  return(map<string, i32>{"a"utf8=1i32, "b"utf8=2i32}["b"utf8])
}
)";
  const std::string srcPath = writeTemp("compile_collections_string_map.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_collections_string_map_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 2);
}

TEST_CASE("compiles and runs string-keyed map literals with bracket sugar in C++ emitter") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
main() {
  return(map<string, i32>["a"=1i32, "b"=2i32]["b"utf8])
}
)";
  const std::string srcPath = writeTemp("compile_collections_string_map_brackets.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_collections_string_map_brackets_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 2);
}

TEST_CASE("compiles and runs canonical namespaced map helpers on experimental map values in C++ emitter") {
  expectCanonicalMapNamespaceExperimentalValueConformance("exe");
}

TEST_CASE("compiles and runs wrapper map helpers on experimental map values in C++ emitter") {
  expectWrapperMapHelperExperimentalValueConformance("exe");
}

TEST_CASE("compiles and runs ownership-sensitive experimental map value methods in C++ emitter") {
  expectExperimentalMapOwnershipMethodConformance("exe");
}

TEST_CASE("compiles and runs helper-wrapped inferred experimental map returns in C++ emitter") {
  expectWrappedInferredExperimentalMapReturnConformance("exe");
}

TEST_CASE("compiles and runs helper-wrapped experimental map parameters in C++ emitter") {
  expectWrappedExperimentalMapParameterConformance("exe");
}

TEST_CASE("compiles and runs helper-wrapped experimental map bindings in C++ emitter") {
  expectWrappedExperimentalMapBindingConformance("exe");
}

TEST_CASE("compiles and runs helper-wrapped experimental map assignment RHS values in C++ emitter") {
  expectWrappedExperimentalMapAssignConformance("exe");
}

TEST_CASE("compiles and runs canonical namespaced map constructors on explicit experimental map bindings in C++ emitter") {
  expectCanonicalMapNamespaceExperimentalConstructorConformance("exe");
}

TEST_CASE("compiles and runs canonical namespaced map constructors through explicit experimental map returns in C++ emitter") {
  expectCanonicalMapNamespaceExperimentalReturnConformance("exe");
}

TEST_CASE("compiles and runs canonical namespaced map constructors through explicit experimental map parameters in C++ emitter") {
  expectCanonicalMapNamespaceExperimentalParameterConformance("exe");
}

TEST_CASE("compiles and runs wrapper map constructors on explicit experimental map bindings in C++ emitter") {
  expectWrapperMapConstructorExperimentalBindingConformance("exe");
}

TEST_CASE("compiles and runs wrapper map constructors through explicit experimental map returns in C++ emitter") {
  expectWrapperMapConstructorExperimentalReturnConformance("exe");
}

TEST_CASE("compiles and runs wrapper map constructors through explicit experimental map parameters in C++ emitter") {
  expectWrapperMapConstructorExperimentalParameterConformance("exe");
}

TEST_CASE("compiles and runs experimental map constructor assignments in C++ emitter") {
  expectExperimentalMapAssignConformance("exe");
}

TEST_CASE("compiles and runs implicit map auto constructor inference in C++ emitter") {
  expectImplicitMapAutoInferenceConformance("exe");
}

TEST_CASE("compiles and runs inferred experimental map returns in C++ emitter") {
  expectInferredExperimentalMapReturnConformance("exe");
}

TEST_CASE("compiles and runs block inferred experimental map returns in C++ emitter") {
  expectBlockInferredExperimentalMapReturnConformance("exe");
}

TEST_CASE("compiles and runs auto block inferred experimental map returns in C++ emitter") {
  expectAutoBlockInferredExperimentalMapReturnConformance("exe");
}

TEST_CASE("compiles and runs inferred experimental map call receivers in C++ emitter") {
  expectInferredExperimentalMapCallReceiverConformance("exe");
}

TEST_CASE("rejects explicit experimental map struct field constructors in C++ emitter") {
  expectExperimentalMapStructFieldConformance("exe");
}

TEST_CASE("compiles and runs inferred experimental map struct fields in C++ emitter") {
  expectInferredExperimentalMapStructFieldConformance("exe");
}

TEST_CASE("compiles and runs helper-wrapped inferred experimental map struct fields in C++ emitter") {
  expectWrappedInferredExperimentalMapStructFieldConformance("exe");
}

TEST_CASE("compiles and runs experimental map method parameters in C++ emitter") {
  expectExperimentalMapMethodParameterConformance("exe");
}

TEST_CASE("compiles and runs inferred experimental map parameters in C++ emitter") {
  expectInferredExperimentalMapParameterConformance("exe");
}

TEST_CASE("compiles and runs inferred experimental map default parameters in C++ emitter") {
  expectInferredExperimentalMapDefaultParameterConformance("exe");
}

TEST_CASE("compiles and runs helper-wrapped inferred experimental map default parameters in C++ emitter") {
  expectWrappedInferredExperimentalMapDefaultParameterConformance("exe");
}

TEST_CASE("compiles and runs experimental map helper receivers in C++ emitter") {
  expectExperimentalMapHelperReceiverConformance("exe");
}

TEST_CASE("compiles and runs helper-wrapped experimental map helper receivers in C++ emitter") {
  expectWrappedExperimentalMapHelperReceiverConformance("exe");
}

TEST_CASE("runs direct-constructor experimental map method receivers in C++ emitter") {
  expectExperimentalMapMethodReceiverConformance("exe");
}

TEST_CASE("runs helper-wrapped experimental map method receivers in C++ emitter") {
  expectWrappedExperimentalMapMethodReceiverConformance("exe");
}

TEST_CASE("compiles and runs experimental map field assignments through canonical helper access in C++ emitter") {
  expectExperimentalMapFieldAssignConformance("exe");
}

TEST_CASE("compiles and runs dereferenced experimental map storage references in C++ emitter") {
  expectExperimentalMapStorageReferenceConformance("exe");
}

TEST_CASE("compiles and runs helper-wrapped Result.ok experimental map result struct fields in C++ emitter") {
  expectWrappedExperimentalMapResultFieldAssignConformance("exe");
}

TEST_CASE("compiles and runs helper-wrapped dereferenced Result.ok experimental map result struct fields in C++ emitter") {
  expectWrappedExperimentalMapResultDerefFieldAssignConformance("exe");
}

TEST_CASE("compiles and runs helper-wrapped experimental map struct storage fields in C++ emitter") {
  expectWrappedExperimentalMapStorageFieldConformance("exe");
}

TEST_CASE("compiles and runs helper-wrapped dereferenced experimental map struct storage fields in C++ emitter") {
  expectWrappedExperimentalMapStorageDerefFieldConformance("exe");
}

TEST_CASE("rejects canonical namespaced map helpers on borrowed experimental map values in C++ emitter") {
  expectCanonicalMapNamespaceExperimentalReferenceConformance("exe");
}

TEST_CASE("compiles and runs canonical namespaced map _ref helpers on borrowed experimental map values in C++ emitter") {
  expectCanonicalMapNamespaceExperimentalBorrowedRefConformance("exe");
}

TEST_CASE("compiles and runs experimental map methods on bound map values in C++ emitter") {
  expectExperimentalMapMethodConformance("exe");
}

TEST_CASE("compiles and runs borrowed experimental map helpers in C++ emitter") {
  expectExperimentalMapReferenceHelperConformance("exe");
}

TEST_CASE("compiles and runs borrowed experimental map methods in C++ emitter") {
  expectExperimentalMapReferenceMethodConformance("exe");
}

TEST_CASE("compiles and runs experimental map inserts in C++ emitter") {
  expectExperimentalMapInsertConformance("exe");
}

TEST_CASE("compiles and runs experimental map ownership-sensitive values in C++ emitter") {
  expectExperimentalMapOwnershipConformance("exe");
}

TEST_CASE("compiles and runs canonical namespaced map inserts on explicit experimental map bindings in C++ emitter") {
  expectCanonicalMapNamespaceExperimentalInsertConformance("exe");
}

TEST_CASE("compiles and runs builtin canonical map first-growth inserts in C++ emitter") {
  expectBuiltinCanonicalMapInsertFirstGrowthConformance("exe");
}

TEST_CASE("compiles and runs builtin canonical map repeated-growth inserts in C++ emitter") {
  expectBuiltinCanonicalMapInsertRepeatedGrowthConformance("exe");
}

TEST_CASE("compiles and runs builtin canonical map insert overwrites in C++ emitter") {
  expectBuiltinCanonicalMapInsertOverwriteConformance("exe");
}

TEST_CASE("compiles and runs builtin canonical map non-local growth in C++ emitter") {
  expectBuiltinCanonicalMapInsertNonLocalGrowthConformance("exe");
}

TEST_CASE("compiles and runs builtin canonical map nested non-local growth in C++ emitter") {
  expectBuiltinCanonicalMapInsertNestedNonLocalGrowthConformance("exe");
}

TEST_CASE("compiles and runs builtin canonical map helper-return borrowed method inserts in C++ emitter") {
  expectBuiltinCanonicalMapInsertHelperReturnBorrowedMethodConformance("exe");
}

TEST_CASE("rejects builtin canonical map direct insert on helper-return value receivers in C++ emitter") {
  expectBuiltinCanonicalMapInsertHelperReturnValueDirectReject("exe");
}

TEST_CASE("rejects builtin canonical map method insert on helper-return value receivers in C++ emitter") {
  expectBuiltinCanonicalMapInsertHelperReturnValueMethodReject("exe");
}

TEST_CASE("rejects canonical map constructor ownership growth in C++ emitter") {
  expectCanonicalMapNamespaceOwnershipReject("exe");
}

TEST_CASE("rejects experimental map bracket access in C++ emitter") {
  expectExperimentalMapIndexConformance("exe");
}

TEST_CASE("compiles and runs shared vector conformance harness in C++ emitter") {
  expectSharedVectorConformanceHarness("exe");
}

TEST_CASE("compiles and runs canonical namespaced vector helpers in C++ emitter") {
  expectCanonicalVectorNamespaceConformance("exe");
}

TEST_CASE("compiles and runs canonical namespaced vector helpers on explicit Vector bindings in C++ emitter") {
  expectCanonicalVectorNamespaceExplicitVectorBindingConformance("exe");
}

TEST_CASE("compiles and runs stdlib wrapper vector helpers on explicit Vector bindings in C++ emitter") {
  expectStdlibWrapperVectorHelperExplicitVectorBindingConformance("exe");
}

TEST_CASE("rejects stdlib wrapper vector helper explicit Vector mismatch in C++ emitter") {
  expectStdlibWrapperVectorHelperExplicitVectorBindingMismatchReject("exe");
}

TEST_CASE("compiles and runs stdlib wrapper vector constructors on explicit Vector bindings in C++ emitter") {
  expectStdlibWrapperVectorConstructorExplicitVectorBindingConformance("exe");
}

TEST_CASE("rejects stdlib wrapper vector constructor explicit Vector mismatch in C++ emitter") {
  expectStdlibWrapperVectorConstructorExplicitVectorBindingMismatchReject("exe");
}

TEST_CASE("compiles and runs stdlib wrapper vector constructors on inferred auto bindings in C++ emitter") {
  expectStdlibWrapperVectorConstructorAutoInferenceConformance("exe");
}

TEST_CASE("rejects stdlib wrapper vector constructor auto inference mismatch in C++ emitter") {
  expectStdlibWrapperVectorConstructorAutoInferenceMismatchReject("exe");
}

TEST_CASE("rejects stdlib wrapper vector constructor receivers in C++ emitter") {
  expectStdlibWrapperVectorConstructorReceiverConformance("exe");
}

TEST_CASE("rejects stdlib wrapper vector helper receiver mismatch in C++ emitter") {
  expectStdlibWrapperVectorConstructorHelperReceiverMismatchReject("exe");
}

TEST_CASE("rejects stdlib wrapper vector method receiver mismatch in C++ emitter") {
  expectStdlibWrapperVectorConstructorMethodReceiverMismatchReject("exe");
}

TEST_CASE("rejects canonical namespaced vector constructor temporaries in C++ emitter") {
  expectCanonicalVectorNamespaceTemporaryReceiverConformance("exe");
}

TEST_CASE("rejects canonical namespaced vector explicit builtin bindings in C++ emitter") {
  expectCanonicalVectorNamespaceExplicitBindingReject("exe");
}

TEST_CASE("rejects canonical namespaced vector named-argument temporaries in C++ emitter") {
  expectCanonicalVectorNamespaceNamedArgsTemporaryReceiverConformance("exe");
}

TEST_CASE("rejects canonical namespaced vector named-argument explicit builtin bindings in C++ emitter") {
  expectCanonicalVectorNamespaceNamedArgsExplicitBindingReject("exe");
}

TEST_CASE("rejects canonical namespaced vector mutators without imported helpers in C++ emitter") {
  expectCanonicalVectorClearImportRequirement("exe");
  expectCanonicalVectorRemoveAtImportRequirement("exe");
  expectCanonicalVectorRemoveSwapImportRequirement("exe");
}

TEST_CASE("compiles and runs bare vector count and capacity through imported stdlib helpers in C++ emitter") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32, 3i32)}
  return(plus(vectorCount<i32>(values), vectorCapacity<i32>(values)))
}
)";
  const std::string srcPath = writeTemp("compile_exe_bare_vector_count_capacity_imported.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_exe_bare_vector_count_capacity_imported_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 6);
}

TEST_CASE("compiles and runs bare vector access through imported stdlib helpers in C++ emitter") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32, 3i32, 4i32)}
  return(plus(
      plus(vectorAt<i32>(values, 0i32), vectorAtUnsafe<i32>(values, 1i32)),
      plus(/std/collections/vector/at<i32>(values, 2i32), /std/collections/vector/at_unsafe<i32>(values, 3i32))))
}
)";
  const std::string srcPath = writeTemp("compile_exe_bare_vector_access_imported.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_exe_bare_vector_access_imported_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 10);
}

TEST_CASE("rejects bare vector count without imported helper in C++ emitter") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32, 3i32)}
  return(count(values))
}
)";
  const std::string srcPath = writeTemp("compile_exe_bare_vector_count_import_requirement.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_exe_bare_vector_count_import_requirement_err.txt").string();
  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("unknown call target: /std/collections/vector/count") != std::string::npos);
}

TEST_CASE("rejects bare vector capacity without imported helper in C++ emitter") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32, 3i32)}
  return(capacity(values))
}
)";
  const std::string srcPath = writeTemp("compile_exe_bare_vector_capacity_import_requirement.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_exe_bare_vector_capacity_import_requirement_err.txt")
          .string();
  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("unknown call target: /std/collections/vector/capacity") != std::string::npos);
}

TEST_CASE("bare vector mutators reject without imported helpers in C++ emitter") {
  expectBareVectorMutatorImportRequirement("exe", "push", "values, 7i32");
  expectBareVectorMutatorImportRequirement("exe", "pop", "values");
  expectBareVectorMutatorImportRequirement("exe", "reserve", "values, 8i32");
  expectBareVectorMutatorImportRequirement("exe", "clear", "values");
  expectBareVectorMutatorImportRequirement("exe", "remove_at", "values, 1i32");
  expectBareVectorMutatorImportRequirement("exe", "remove_swap", "values, 1i32");
}

TEST_CASE("compiles and runs experimental vector helper runtime contracts in C++ emitter") {
  expectExperimentalVectorRuntimeContracts("exe");
}

TEST_CASE("compiles and runs experimental vector ownership-sensitive helpers in C++ emitter") {
  expectExperimentalVectorOwnershipContracts("exe");
}

TEST_CASE("compiles and runs canonical vector helpers on experimental vector receivers in C++ emitter") {
  expectExperimentalVectorCanonicalHelperRoutingConformance("exe");
}
TEST_CASE("compiles and runs vector pop empty runtime contract in C++ emitter") {
  SUBCASE("call") {
    expectVectorPopEmptyRuntimeContract("exe", false);
  }

  SUBCASE("method") {
    expectVectorPopEmptyRuntimeContract("exe", true);
  }
}

TEST_CASE("compiles and runs vector index runtime contract in C++ emitter") {
  expectVectorIndexRuntimeContract("exe", "access_call");
  expectVectorIndexRuntimeContract("exe", "access_method");
  expectVectorIndexRuntimeContract("exe", "access_bracket");
  expectVectorIndexRuntimeContract("exe", "remove_at_call");
  expectVectorIndexRuntimeContract("exe", "remove_at_method");
  expectVectorIndexRuntimeContract("exe", "remove_swap_call");
  expectVectorIndexRuntimeContract("exe", "remove_swap_method");
}

TEST_CASE("compiles and runs container error contract conformance in C++ emitter") {
  expectContainerErrorConformance("exe");
}

TEST_CASE("compiles and runs checked pointer conformance harness in C++ emitter") {
  expectCheckedPointerHelperSurfaceConformance("exe");
  expectCheckedPointerGrowthConformance("exe");
  expectCheckedPointerOutOfBoundsConformance("exe");
}

TEST_CASE("compiles and runs unchecked pointer conformance harness in C++ emitter") {
  expectUncheckedPointerHelperSurfaceConformance("exe");
  expectUncheckedPointerGrowthConformance("exe");
}

TEST_CASE("compiles with executions using collection arguments") {
  const std::string source = R"(
[return<int>]
main() {
  return(1i32)
}

execute_task([i32] items, [i32] pairs) {
  return(1i32)
}

execute_task([items] array<i32>(1i32, 2i32), [pairs] map<i32, i32>(1i32, 2i32))
)";
  const std::string srcPath = writeTemp("compile_exec_collections.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_exec_collections_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 1);
}

TEST_CASE("compile run rejects execution body arguments") {
  const std::string source = R"(
[return<int>]
main() {
  return(1i32)
}

execute_repeat([i32] count) {
  return(1i32)
}

execute_repeat(2i32) {
  main(),
  main()
}
)";
  const std::string srcPath = writeTemp("compile_exec_body.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_exec_body_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 2);
}

TEST_CASE("compiles and runs pointer plus u64 offset") {
  const std::string source = R"(
[return<int>]
main() {
  [i32] value{5i32}
  return(dereference(plus(location(value), 0u64)))
}
)";
  const std::string srcPath = writeTemp("compile_pointer_plus_u64.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_pointer_plus_u64_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 5);
}

TEST_CASE("compiles and runs i64 literals") {
  const std::string source = R"(
[return<i64>]
main() {
  return(9i64)
}
)";
  const std::string srcPath = writeTemp("compile_i64_literal.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_i64_literal_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 9);
}

TEST_CASE("compiles and runs u64 literals") {
  const std::string source = R"(
[return<u64>]
main() {
  return(10u64)
}
)";
  const std::string srcPath = writeTemp("compile_u64_literal.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_u64_literal_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 10);
}

TEST_CASE("compiles and runs assignment operator rewrite") {
  const std::string source = R"(
[return<int>]
main() {
  [i32 mut] value{1i32}
  value=2i32
  return(value)
}
)";
  const std::string srcPath = writeTemp("compile_assign_op.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_assign_op_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 2);
}

TEST_CASE("compiles and runs comparison operator rewrite") {
  const std::string source = R"(
[return<bool>]
main() {
  return(2i32>1i32)
}
)";
  const std::string srcPath = writeTemp("compile_gt_op.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_gt_op_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 1);
}

TEST_CASE("compiles and runs less_than operator rewrite") {
  const std::string source = R"(
[return<bool>]
main() {
  return(1i32<2i32)
}
)";
  const std::string srcPath = writeTemp("compile_lt_op.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_lt_op_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 1);
}

TEST_CASE("compiles and runs greater_equal operator rewrite") {
  const std::string source = R"(
[return<bool>]
main() {
  return(2i32>=2i32)
}
)";
  const std::string srcPath = writeTemp("compile_ge_op.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_ge_op_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 1);
}

TEST_CASE("compiles and runs less_equal operator rewrite") {
  const std::string source = R"(
[return<bool>]
main() {
  return(2i32<=2i32)
}
)";
  const std::string srcPath = writeTemp("compile_le_op.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_le_op_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 1);
}

TEST_CASE("compiles and runs and operator rewrite") {
  const std::string source = R"(
[return<bool>]
main() {
  return(true&&true)
}
)";
  const std::string srcPath = writeTemp("compile_and_op.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_and_op_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 1);
}

TEST_CASE("compiles and runs or operator rewrite") {
  const std::string source = R"(
[return<bool>]
main() {
  return(false||true)
}
)";
  const std::string srcPath = writeTemp("compile_or_op.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_or_op_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 1);
}

TEST_CASE("compiles and runs not operator rewrite") {
  const std::string source = R"(
[return<bool>]
main() {
  return(!false)
}
)";
  const std::string srcPath = writeTemp("compile_not_op.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_not_op_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 1);
}

TEST_CASE("compiles and runs not operator with parentheses") {
  const std::string source = R"(
[return<bool>]
main() {
  return(!(false))
}
)";
  const std::string srcPath = writeTemp("compile_not_paren.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_not_paren_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 1);
}

TEST_CASE("compiles and runs unary minus operator rewrite") {
  const std::string source = R"(
[return<int>]
main() {
  [i32] value{3i32}
  return(plus(-value, 5i32))
}
)";
  const std::string srcPath = writeTemp("compile_unary_minus.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_unary_minus_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 2);
}

TEST_CASE("compiles and runs equality operator rewrite") {
  const std::string source = R"(
[return<bool>]
main() {
  return(2i32==2i32)
}
)";
  const std::string srcPath = writeTemp("compile_eq_op.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_eq_op_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 1);
}

TEST_CASE("compiles and runs not_equal operator rewrite") {
  const std::string source = R"(
[return<bool>]
main() {
  return(2i32!=3i32)
}
)";
  const std::string srcPath = writeTemp("compile_neq_op.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_neq_op_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 1);
}

TEST_SUITE_END();
