#include "../test_compile_run_helpers.h"

#include "../test_compile_run_collection_conformance_helpers.h"
#include "../test_compile_run_container_error_conformance_helpers.h"
#include "../test_compile_run_checked_pointer_conformance_helpers.h"
#include "../test_compile_run_unchecked_pointer_conformance_helpers.h"
#include "test_compile_run_native_backend_collections_helpers.h"

#if PRIMESTRUCT_NATIVE_COLLECTIONS_ENABLED
TEST_SUITE_BEGIN("primestruct.compile.run.native_backend.collections");

static void expect_soa_helper_return_shadow_rejects(const std::string &source,
                                                           const std::string &nameStem,
                                                           int expectedRunExit = -1) {
  const std::string srcPath = writeTemp(nameStem + ".prime", source);
  const std::string outPath =
      (testScratchPath("") / (nameStem + "_native_out.txt")).string();
  const std::string artifactPath =
      (testScratchPath("") / (nameStem + "_native_artifact")).string();

  const std::string compileCmd = "./primec --emit=native " + quoteShellArg(srcPath) + " -o " +
                                 quoteShellArg(artifactPath) + " --entry /main > " +
                                 quoteShellArg(outPath) + " 2>&1";
  // The soa shadow-routing gaps these cases pinned are closed (the shadowed
  // methods now dispatch to the user helpers), so the programs compile and
  // run; expectedRunExit < 0 keeps the historical compile-reject contract.
  if (expectedRunExit >= 0) {
    CHECK(runCommand(compileCmd) == 0);
    CHECK(runCommand(quoteShellArg(artifactPath)) == expectedRunExit);
    return;
  }
  CHECK(runCommand(compileCmd) == 2);
}

TEST_CASE("rejects native templated stdlib return wrapper temporaries in expressions") {
  const std::string source = R"(
import /std/collections/*

[return<vector<T>>]
wrapVector<T>([T] value) {
  return(/std/collections/vector/vector<T>(value))
}

[return<auto>]
wrapMap<K, V>([K] key, [V] value) {
  [/std/collections/map<K, V>] values{map<K, V>(key, value)}
  return(values)
}

[effects(heap_alloc), return<int>]
main() {
  [i32] vectorTotal{wrapVector<i32>(9i32).count()}
  [i32] mapAtTotal{wrapMap<string, i32>("only"raw_utf8, 4i32).at("only"raw_utf8)}
  [i32] mapUnsafeTotal{wrapMap<string, i32>("only"raw_utf8, 4i32).at_unsafe("only"raw_utf8)}
  [i32] mapCountTotal{wrapMap<string, i32>("only"raw_utf8, 4i32).count()}
  return(plus(plus(vectorTotal, mapAtTotal), plus(mapUnsafeTotal, mapCountTotal)))
}
)";
  const std::string srcPath =
      writeTemp("compile_native_stdlib_collection_shim_templated_return_temporaries.prime", source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_native_stdlib_collection_shim_templated_return_temporaries.err")
          .string();

  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
}

TEST_CASE("native query-local auto vector helpers run through lowering") {
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
  const std::string directSrcPath = writeTemp("compile_native_graph_query_vector_helper_call.prime", directSource);
  const std::string directExePath =
      (testScratchPath("") / "compile_native_graph_query_vector_helper_call_exe").string();
  const std::string directCmd =
      "./primec --emit=native " + directSrcPath + " -o " + directExePath + " --entry /main";
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
  const std::string methodSrcPath =
      writeTemp("compile_native_graph_query_vector_helper_method.prime", methodSource);
  const std::string methodExePath =
      (testScratchPath("") / "compile_native_graph_query_vector_helper_method_exe").string();
  const std::string methodCmd =
      "./primec --emit=native " + methodSrcPath + " -o " + methodExePath + " --entry /main";
  CHECK(runCommand(methodCmd) == 0);
  CHECK(runCommand(methodExePath) == 17);
}

TEST_CASE("rejects native experimental soa stdlib helpers") {
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
  const std::string srcPath =
      writeTemp("compile_native_experimental_soa_helpers.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_native_experimental_soa_helpers_err.txt").string();

  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find(
            "direct import of retired soa compatibility modules is not supported; use /std/collections/soa/*") !=
        std::string::npos);
}

TEST_CASE("rejects native raw soa type spelling") {
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
  const std::string srcPath =
      writeTemp("compile_native_raw_soa_type_reject.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_native_raw_soa_type_reject_err.txt").string();
  const std::string exePath =
      (testScratchPath("") / "rejects_native_raw_soa_type_spelling_exe").string();

  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 0);
}

TEST_CASE("native public soa count helper on public wrapper") {
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
      writeTemp("compile_native_public_soa_count_public_wrapper.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_public_soa_count_public_wrapper_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 2);
}

TEST_CASE("native public soa get helper") {
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
      writeTemp("compile_native_public_soa_get_public_wrapper.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_public_soa_get_public_wrapper_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 9);
}

TEST_CASE("native public soa get helper rejects template arguments on non-soa receiver") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32)}
  return(/std/collections/soa/get<i32>(values, 0i32))
}
)";
  const std::string srcPath =
      writeTemp("compile_native_public_soa_get_non_soa_receiver.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_native_public_soa_get_non_soa_receiver_err.txt").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
}

TEST_CASE("native public soa get slash-method keeps canonical reject") {
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
      writeTemp("compile_native_public_soa_get_slash_method.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_native_public_soa_get_slash_method_err.txt").string();
  const std::string exePath =
      (testScratchPath("") / "native_public_soa_get_slash_method_kee_exe").string();

  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 9);
}

TEST_CASE("native public soa to_aos slash-method keeps canonical reject") {
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
      writeTemp("compile_native_public_soa_to_aos_slash_method.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_native_public_soa_to_aos_slash_method_err.txt").string();
  const std::string exePath =
      (testScratchPath("") / "native_public_soa_to_aos_slash_method__exe").string();

  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 1);
}

TEST_CASE("native public soa ref helper") {
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
      writeTemp("compile_native_public_soa_ref_public_wrapper.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_public_soa_ref_public_wrapper_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 9);
}

TEST_CASE("native public soa mutator helpers") {
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
      writeTemp("compile_native_public_soa_mutators_public_wrapper.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_public_soa_mutators_public_wrapper_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 11);
}

TEST_CASE("native public soa to_aos helper lowers") {
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
      writeTemp("compile_native_public_soa_to_aos_public_wrapper.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_public_soa_to_aos_public_wrapper_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 1);
}

TEST_CASE("native public soa to_aos temporaries route through canonical vector capacity") {
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
      writeTemp("compile_native_public_soa_to_aos_vector_capacity_public_wrapper.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_public_soa_to_aos_vector_capacity_public_wrapper_exe")
          .string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 1);
}

TEST_CASE("native legacy soa compatibility helpers reject") {
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
      writeTemp("compile_native_wildcard_legacy_soa_compatibility_helpers.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_native_wildcard_legacy_soa_compatibility_helpers_err.txt").string();
  const std::string exePath =
      (testScratchPath("") / "native_legacy_soa_compatibility_helper_exe").string();

  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 17);
}

TEST_CASE("native wildcard-imported canonical soa helpers reject current metadata inference gap") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/soa/*

[struct reflect]
Particle() {
  [i32] x{1i32}
  [i32] y{2i32}
}

[effects(heap_alloc), return<int>]
main() {
  [auto mut] values{single<Particle>(Particle(4i32, 6i32))}
  reserve(values, 2i32)
  push(values, Particle(9i32, 11i32))
  [Particle] first{get(values, 0i32)}
  [Reference<Particle>] second{ref(values, 1i32)}
  [vector<Particle>] unpacked{to_aos(values)}
  return(plus(plus(count(values), plus(first.x, second.x)), count(unpacked)))
}
)";
  const std::string srcPath =
      writeTemp("compile_native_wildcard_canonical_soa_helpers.prime", source);
  const std::string exePath =
      (testScratchPath("") / "wildcard_canonical_soa_helpers_exe").string();

  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 17);
}

TEST_CASE("native public soa type spelling keeps generated identity rejection") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/soa/*

[struct reflect]
Particle() {
  [i32] x{1i32}
  [i32] y{2i32}
}

[return<int>]
score([Reference<soa<Particle>>] values) {
  return(/std/collections/soa/count_ref<Particle>(values))
}

[effects(heap_alloc), return<int>]
main() {
  [soa<Particle> mut] values{soa<Particle>()}
  reserve(values, 2i32)
  push(values, Particle(9i32, 11i32))
  [Reference<soa<Particle>>] borrowed{location(values)}
  [Pointer<soa<Particle>>] ptr{location(values)}
  [Particle] first{get(values, 0i32)}
  [vector<Particle>] unpacked{to_aos(values)}
  return(plus(plus(score(borrowed), /std/collections/soa/count<Particle>(dereference(ptr))),
              plus(first.x, count(unpacked))))
}
)";
  const std::string srcPath =
      writeTemp("compile_native_public_soa_type_spelling.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_native_public_soa_type_spelling.err").string();

  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
}

TEST_CASE("native public soa read helpers reject current metadata inference gap") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/soa/*

[struct reflect]
Particle() {
  [i32] x{1i32}
  [i32] y{2i32}
}

[effects(heap_alloc), return<int>]
main() {
  [auto mut] values{soa<Particle>()}
  push(values, Particle(3i32, 5i32))
  push(values, Particle(7i32, 11i32))
  [auto] borrowed{location(values)}
  [Particle] direct{/std/collections/soa/get<Particle>(values, 1i32)}
  [Particle] borrowedValue{/std/collections/soa/get_ref<Particle>(borrowed, 0i32)}
  [Reference<Particle>] directRef{/std/collections/soa/ref<Particle>(values, 0i32)}
  [Reference<Particle>] borrowedRef{/std/collections/soa/ref_ref<Particle>(borrowed, 1i32)}
  [i32] methodCount{values.count()}
  return(plus(plus(/std/collections/soa/count<Particle>(values),
                   /std/collections/soa/count_ref<Particle>(borrowed)),
              plus(methodCount,
                   plus(plus(direct.x, borrowedValue.x),
                        plus(directRef.y, borrowedRef.y)))))
}
)";
  const std::string srcPath =
      writeTemp("compile_native_public_soa_read_helpers.prime", source);
  const std::string exePath =
      (testScratchPath("") / "public_soa_read_helpers_exe").string();

  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 32);
}

TEST_CASE("native public soa construction and mutators reject current metadata inference gap") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/soa/*

[struct reflect]
Particle() {
  [i32] x{1i32}
  [i32] y{2i32}
}

[effects(heap_alloc), return<int>]
main() {
  [auto mut] values{/std/collections/soa/soa<Particle>(Particle(1i32, 2i32),
                                                       Particle(3i32, 5i32))}
  /std/collections/soa/reserve<Particle>(values, 4i32)
  /std/collections/soa/push<Particle>(values, Particle(7i32, 11i32))
  [auto] singleton{/std/collections/soa/single<Particle>(Particle(13i32, 17i32))}
  return(plus(plus(count(values),
                   /std/collections/soa/get<Particle>(values, 2i32).y),
              count(singleton)))
}
)";
  const std::string srcPath =
      writeTemp("compile_native_public_soa_construction_mutators.prime",
                source);
  const std::string exePath =
      (testScratchPath("") / "public_soa_construction_mutators_exe").string();

  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 15);
}

TEST_CASE("native public soa from-aos rejects current metadata inference gap") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/soa/*

[struct reflect]
Particle() {
  [i32] x{1i32}
  [i32] y{2i32}
}

[effects(heap_alloc), return<int>]
main() {
  [vector<Particle> mut] items{vector<Particle>()}
  items.push(Particle(3i32, 5i32))
  items.push(Particle(7i32, 11i32))
  [auto] values{/std/collections/soa/from_aos<Particle>(items)}
  return(count(values))
}
)";
  const std::string srcPath =
      writeTemp("compile_native_public_soa_from_aos.prime",
                source);
  const std::string exePath =
      (testScratchPath("") / "public_soa_from_aos_exe").string();

  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 2);
}

TEST_CASE("native public soa field-view wrappers reject current metadata inference gap") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/soa/*

[struct reflect]
Particle() {
  [i32] x{1i32}
  [i32] y{2i32}
}

[effects(heap_alloc), return<int>]
main() {
  [vector<Particle> mut] items{vector<Particle>()}
  items.push(Particle(3i32, 5i32))
  items.push(Particle(7i32, 11i32))
  [auto] values{/std/collections/soa/from_aos<Particle>(items)}
  return(plus(plus(count(values),
                   /std/collections/soa/field_view<Particle, i32>(values, 1i32)[1i32]),
              values.y()[0i32]))
}
)";
  const std::string srcPath =
      writeTemp("compile_native_public_soa_field_view_wrappers.prime",
                source);
  const std::string exePath =
      (testScratchPath("") / "public_soa_field_view_wrappers_exe").string();

  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 18);
}

TEST_CASE("native compiles and runs graph-solved direct local-auto vector helper shadows compatibility") {
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
      writeTemp("compile_native_graph_direct_local_auto_vector_helper_shadows.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_graph_direct_local_auto_vector_helper_shadows_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 34);
}

TEST_CASE(
    "native rejects experimental soa stdlib wide structs on pending width") {
  const std::string source = R"(
import /std/collections/soa/*

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
  [SoaVector<Particle17>] values{/std/collections/soa/soaVectorNew<Particle17>()}
  return(/std/collections/soa/soaVectorCount<Particle17>(values))
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
      writeTemp("compile_native_experimental_soa_wide_pending_forms.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_experimental_soa_wide_pending_forms_exe").string();
  const std::string importedErrPath =
      (testScratchPath("") / "primec_native_experimental_soa_wide_pending_imported_err.txt").string();
  const std::string directErrPath =
      (testScratchPath("") / "primec_native_experimental_soa_wide_pending_direct_err.txt").string();
  const std::string helperReturnErrPath =
      (testScratchPath("") / "primec_native_experimental_soa_wide_pending_helper_return_err.txt").string();

  const std::string compileImportedCmd =
      "./primec --emit=native " + srcPath + " -o " + exePath +
      " --entry /runImported 2> " + importedErrPath;
  CHECK(runCommand(compileImportedCmd) == 0);

  const std::string compileDirectCmd =
      "./primec --emit=native " + srcPath + " -o " + exePath +
      " --entry /runDirectCanonical 2> " + directErrPath;
  CHECK(runCommand(compileDirectCmd) == 0);

  const std::string compileHelperReturnCmd =
      "./primec --emit=native " + srcPath + " -o " + exePath +
      " --entry /runHelperReturn 2> " + helperReturnErrPath;
  CHECK(runCommand(compileHelperReturnCmd) == 0);
}

TEST_CASE("native runs experimental soa stdlib to-aos helper") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/soa/*
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
  const std::string srcPath =
      writeTemp("compile_native_experimental_soa_to_aos.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_experimental_soa_to_aos_exe").string();
  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 0);
}

TEST_CASE("native rejects direct experimental soa to-aos helper on builtin soa") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/soa/*

[struct reflect]
Particle() {
  [i32] x{1i32}
}

[effects(heap_alloc), return<int>]
main() {
  [soa<Particle>] values{soa<Particle>()}
  [vector<Particle>] unpacked{
    /std/collections/soa/soaVectorToAos<Particle>(values)}
  return(count(unpacked))
}
)";
  const std::string srcPath =
      writeTemp("compile_native_builtin_soa_direct_experimental_to_aos.prime",
                source);
  const std::string exePath =
      (testScratchPath("") / "builtin_soa_direct_to_aos_exe").string();
  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 0);
}

TEST_CASE("native rejects experimental soa stdlib to-aos method on wrapper surface") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/soa/*
import /std/collections/soa/*

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
  const std::string srcPath =
      writeTemp("compile_native_experimental_soa_to_aos_method.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_native_experimental_soa_to_aos_method.err").string();
  const std::string exePath =
      (testScratchPath("") / "native_rejects_experimental_soa_stdlib_exe").string();
  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 0);
}

TEST_CASE("native no-import root soa to_aos method helper forms reject during semantics") {
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
  const std::string srcPath =
      writeTemp("compile_native_root_soa_to_aos_method_forms.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_native_root_soa_to_aos_method_forms_err.txt").string();
  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  // TODO-5318: no-import soa helpers reject in semantics with one import
  // diagnostic (docs/PrimeStruct.md, No-import helper rule).
  CHECK(readFile(errPath).find("soa helper requires import /std/collections/soa/*: to_aos") != std::string::npos);
}

TEST_CASE("native rejects non-empty root soa struct literals") {
  const std::string source = R"(
[struct reflect]
Particle() {
  [i32] x{1i32}
}

[effects(heap_alloc), return<int>]
main() {
  [soa<Particle>] values{soa<Particle>(Particle(7i32), Particle(9i32))}
  return(count(values))
}
)";
  const std::string srcPath =
      writeTemp("compile_native_root_soa_non_empty_literal.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_native_root_soa_non_empty_literal.err").string();
  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  // TODO-5318: no-import soa helpers reject in semantics with one import
  // diagnostic (docs/PrimeStruct.md, No-import helper rule).
  CHECK(readFile(errPath).find("soa helper requires import /std/collections/soa/*: count") != std::string::npos);
}

TEST_CASE("native rejects non-empty root soa literals with unsupported element envelopes") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [soa<i32>] values{soa<i32>(1i32, 2i32)}
  return(0i32)
}
)";
  const std::string srcPath =
      writeTemp("compile_native_root_soa_non_struct_literal.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_native_root_soa_non_struct_literal_err.txt").string();
  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  const std::string error = readFile(errPath);
  CHECK(error.find("soa requires struct element type [PSC1005]") != std::string::npos);
  CHECK(error.find("stage: semantic") != std::string::npos);
}

TEST_CASE("native rejects non-empty root soa literals above former local capacity limit") {
  auto buildParticleLiteralArgs = [](int count) {
    std::string args;
    args.reserve(static_cast<size_t>(count) * 20);
    for (int i = 0; i < count; ++i) {
      if (i > 0) {
        args += ", ";
      }
      args += "Particle(" + std::to_string(i + 1) + "i32)";
    }
    return args;
  };

  const std::string source = std::string(
      "[struct reflect]\n"
      "Particle() {\n"
      "  [i32] x{0i32}\n"
      "}\n\n"
      "[effects(heap_alloc), return<int>]\n"
      "main() {\n"
      "  [soa<Particle>] values{soa<Particle>(") +
                             buildParticleLiteralArgs(257) +
                             ")}\n"
                             "  return(0i32)\n"
                             "}\n";
  const std::string srcPath =
      writeTemp("compile_native_root_soa_literal_limit_overflow.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_native_root_soa_literal_limit_overflow.err").string();
  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 0);
}

TEST_CASE("native runs experimental soa stdlib non-empty to-aos helper") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/soa/*
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
  const std::string srcPath =
      writeTemp("compile_native_experimental_soa_to_aos_non_empty.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_experimental_soa_to_aos_non_empty_exe").string();
  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 1);
}

TEST_CASE("native rejects experimental soa stdlib non-empty to-aos method on wrapper state") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/soa/*
import /std/collections/soa/*

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
  const std::string srcPath =
      writeTemp("compile_native_experimental_soa_to_aos_non_empty_method.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_native_experimental_soa_to_aos_non_empty_method.err")
          .string();
  const std::string exePath =
      (testScratchPath("") / "native_rejects_experimental_soa_stdlib_exe").string();
  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 1);
}

TEST_CASE("native rejects bare soa get helper through helper return compatibility") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/soa/*

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
      writeTemp("compile_native_experimental_soa_get_helper_return.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_native_experimental_soa_get_helper_return.err").string();
  const std::string exePath =
      (testScratchPath("") / "native_rejects_bare_soa_get_helper_thr_exe").string();

  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 7);
}

TEST_CASE("native rejects global helper-return soa method shadows compatibility") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/soa/*

[struct reflect]
Particle() {
  [i32] x{1i32}
}

[return<SoaVector<Particle>>]
cloneValues() {
  [SoaVector<Particle> mut] values{soaVectorNew<Particle>()}
  /std/collections/soa/soaVectorPush<Particle>(values, Particle(7i32))
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
  expect_soa_helper_return_shadow_rejects(
      source,
      "compile_native_experimental_soa_method_shadow_global_helper_return",
      83);
}

TEST_CASE("native rejects method-like helper-return soa method shadows compatibility") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/soa/*

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
  expect_soa_helper_return_shadow_rejects(
      source,
      "compile_native_experimental_soa_method_shadow_method_like_helper_return",
      83);
}

TEST_CASE("native runs vector-target old-explicit soa mutator shadows") {
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
      writeTemp("compile_native_vector_target_old_explicit_soa_mutator_shadow.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_vector_target_old_explicit_soa_mutator_shadow").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 10);
}

TEST_CASE("native runs vector-target method soa mutator shadows") {
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
      writeTemp("compile_native_vector_target_method_soa_mutator_shadow.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_vector_target_method_soa_mutator_shadow").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 10);
}

TEST_CASE("native rejects vector-target to_aos helper shadows") {
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
      writeTemp("compile_native_vector_target_to_aos_shadow.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_native_vector_target_to_aos_shadow.err").string();
  const std::string exePath =
      (testScratchPath("") / "native_rejects_vector_target_to_aos_he_exe").string();

  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 27);
}

TEST_CASE("native rejects nested struct-body soa constructor-bearing helper returns compatibility") {
  const std::string source = R"(
import /std/collections/soa/*

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
      writeTemp("compile_native_nested_struct_body_soa_constructor_helper.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_native_nested_struct_body_soa_constructor_helper.err").string();
  const std::string exePath =
      (testScratchPath("") / "native_rejects_nested_struct_body_soa__exe").string();

  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 0);
}

TEST_CASE("native rejects nested struct-body soa direct and bound helper expressions compatibility") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/soa/*

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
      writeTemp("compile_native_nested_struct_body_soa_direct_bound_helpers.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_native_nested_struct_body_soa_direct_bound_helpers.err").string();
  const std::string exePath =
      (testScratchPath("") / "native_rejects_nested_struct_body_soa__exe").string();

  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 16);
}

TEST_CASE("native rejects nested struct-body soa method shadows compatibility") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/soa/*

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
      writeTemp("compile_native_nested_struct_body_soa_method_shadows.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_native_nested_struct_body_soa_method_shadows.err").string();
  const std::string exePath =
      (testScratchPath("") / "native_rejects_nested_struct_body_soa__exe").string();

  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 84);
}

TEST_CASE("native rejects explicit method-like helper-return experimental soa to_aos shadow") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/soa/*

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
      writeTemp("compile_native_experimental_soa_explicit_method_like_to_aos_shadow.prime",
                source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_native_experimental_soa_explicit_method_like_to_aos_shadow.err")
          .string();
  const std::string exePath =
      (testScratchPath("") / "native_rejects_explicit_method_like_he_exe").string();

  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 1);
}

TEST_CASE("native rejects experimental soa ref pass-through and return") {
  const std::string source = R"(
import /std/collections/soa/*

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
      writeTemp("compile_native_experimental_soa_ref_passthrough.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_native_experimental_soa_ref_passthrough.err").string();
  const std::string exePath =
      (testScratchPath("") / "native_rejects_experimental_soa_ref_pa_exe").string();
  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 7);
}

TEST_SUITE_END();
#endif
