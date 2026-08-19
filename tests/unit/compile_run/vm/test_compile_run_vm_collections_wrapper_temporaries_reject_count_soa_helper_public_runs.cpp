#include "../test_compile_run_helpers.h"

#include "../test_compile_run_collection_conformance_helpers.h"
#include "../test_compile_run_container_error_conformance_helpers.h"
#include "../test_compile_run_checked_pointer_conformance_helpers.h"
#include "../test_compile_run_unchecked_pointer_conformance_helpers.h"
#include "test_compile_run_vm_collections_helpers.h"

TEST_SUITE_BEGIN("primestruct.compile.run.vm.collections");

TEST_CASE("rejects vm wrapper temporary vector count method without helper") {
  const std::string source = R"(
[effects(heap_alloc), return<vector<i32>>]
wrapVector() {
  return(vector<i32>(1i32, 2i32, 3i32))
}

[effects(heap_alloc), return<int>]
main() {
  return(wrapVector().count())
}
)";
  const std::string srcPath = writeTemp("vm_wrapper_vector_count_method_import_requirement.prime", source);
  const std::string errPath =
      (std::filesystem::temp_directory_path() /
       "primec_vm_wrapper_vector_count_method_import_requirement_err.txt")
          .string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main 2> " + errPath;
  CHECK(runCommand(runCmd) == 2);
  CHECK(readFile(errPath).find("unknown method: /vector/count") != std::string::npos);
}

TEST_CASE("vm query-local auto vector helpers run through lowering") {
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
  const std::string directSrcPath = writeTemp("vm_graph_query_vector_helper_call.prime", directSource);
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
  const std::string methodSrcPath = writeTemp("vm_graph_query_vector_helper_method.prime", methodSource);
  const std::string methodCmd = "./primec --emit=vm " + methodSrcPath + " --entry /main";
  CHECK(runCommand(methodCmd) == 17);
}

TEST_CASE("rejects vm experimental soa stdlib helpers") {
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
  const std::string srcPath = writeTemp("vm_experimental_soa_helpers.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_vm_experimental_soa_helpers_err.txt").string();
  const std::string runCmd =
      "./primec --emit=vm " + srcPath + " --entry /main 2> " + errPath;
  CHECK(runCommand(runCmd) == 2);
  CHECK(readFile(errPath).find(
            "direct import of retired soa compatibility modules is not supported; use /std/collections/soa/*") !=
        std::string::npos);
}

TEST_CASE("rejects vm raw soa type spelling") {
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
  const std::string srcPath = writeTemp("vm_raw_soa_type_reject.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  // soa<T> is now accepted as a valid type spelling instead of being rejected.
  CHECK(runCommand(runCmd) == 0);
}

TEST_CASE("runs vm public soa count helper on public wrapper") {
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
      writeTemp("vm_public_soa_count_public_wrapper.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 2);
}

TEST_CASE("runs vm public soa get helper") {
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
      writeTemp("vm_public_soa_get_public_wrapper.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 9);
}

TEST_CASE("vm public soa get helper rejects template arguments on non-soa receiver") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32)}
  return(/std/collections/soa/get<i32>(values, 0i32))
}
)";
  const std::string srcPath =
      writeTemp("vm_public_soa_get_non_soa_receiver.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_vm_public_soa_get_non_soa_receiver_err.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main 2> " + errPath;
  CHECK(runCommand(runCmd) == 2);
  CHECK(readFile(errPath).find("get requires soa target") !=
        std::string::npos);
}

TEST_CASE("vm runs public soa get slash-method") {
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
      writeTemp("vm_public_soa_get_slash_method.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 9);
}

TEST_CASE("vm runs public soa to_aos slash-method") {
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
      writeTemp("vm_public_soa_to_aos_slash_method.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 1);
}

TEST_CASE("runs vm public soa ref helper") {
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
      writeTemp("vm_public_soa_ref_public_wrapper.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 9);
}

TEST_CASE("runs vm public soa mutator helpers") {
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
      writeTemp("vm_public_soa_mutators_public_wrapper.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 11);
}

TEST_CASE("vm public soa to_aos helper lowers") {
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
      writeTemp("vm_public_soa_to_aos_public_wrapper.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 1);
}

TEST_CASE("vm public soa to_aos temporaries route through canonical vector capacity") {
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
      writeTemp("vm_public_soa_to_aos_vector_capacity_public_wrapper.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 1);
}

TEST_CASE("vm public soa to_aos explicit helper is a vector target") {
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
      writeTemp("vm_public_soa_to_aos_vector_capacity.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 1);
}

TEST_CASE("vm public soa read helpers route through wrapper paths") {
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
  return(plus(plus(/std/collections/soa/count<Particle>(values),
                   /std/collections/soa/count_ref<Particle>(borrowed)),
              plus(values.count(),
                   plus(plus(direct.x, borrowedValue.x),
                        plus(directRef.y, borrowedRef.y)))))
}
)";
  const std::string srcPath =
      writeTemp("vm_public_soa_read_helpers.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_vm_public_soa_read_helpers_err.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main 2> " + errPath;
  // TODO-5050 shape (a) (RESOLVED): the fully-qualified
  // /std/collections/soa/ref_ref<T>(...) call form (and the sibling
  // get/get_ref/ref/count/count_ref forms) on borrowed receivers now
  // resolve and run correctly, returning 32 (verified via the arithmetic
  // in this fixture's return expression).
  CHECK(runCommand(runCmd) == 32);
  CHECK(readFile(errPath).empty());
}

TEST_CASE("vm public soa construction and mutators use wrappers") {
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
      writeTemp("vm_public_soa_construction_mutators.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 15);
}

TEST_CASE("vm public soa from-aos uses wrapper") {
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
      writeTemp("vm_public_soa_from_aos.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 2);
}

TEST_CASE("vm public soa field-view wrappers use public reads") {
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
      writeTemp("vm_public_soa_field_view_wrappers.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 18);
}

TEST_CASE("vm runs legacy soa compatibility helpers") {
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
      writeTemp("vm_wildcard_legacy_soa_compatibility_helpers.prime", source);
  const std::string runCmd =
      "./primec --emit=vm " + srcPath + " -o /dev/null --entry /main";
  CHECK(runCommand(runCmd) == 17);
}

TEST_CASE("vm runs graph-solved direct local-auto vector helper shadows compatibility") {
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
      writeTemp("vm_graph_direct_local_auto_vector_helper_shadows.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 34);
}

TEST_CASE(
    "vm rejects experimental soa stdlib wide structs on pending width") {
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
  const std::string srcPath = writeTemp("vm_experimental_soa_wide_pending_forms.prime", source);
  const std::string importedErrPath =
      (testScratchPath("") / "primec_vm_experimental_soa_wide_pending_imported_err.txt").string();
  const std::string directErrPath =
      (testScratchPath("") / "primec_vm_experimental_soa_wide_pending_direct_err.txt").string();
  const std::string helperReturnErrPath =
      (testScratchPath("") / "primec_vm_experimental_soa_wide_pending_helper_return_err.txt").string();

  const std::string runImportedCmd =
      "./primec --emit=vm " + srcPath + " --entry /runImported 2> " + importedErrPath;
  CHECK(runCommand(runImportedCmd) == 2);
  CHECK(readFile(importedErrPath).find("direct import of retired soa compatibility modules is not supported") !=
        std::string::npos);

  const std::string runDirectCmd =
      "./primec --emit=vm " + srcPath + " --entry /runDirectCanonical 2> " + directErrPath;
  CHECK(runCommand(runDirectCmd) == 2);
  CHECK(readFile(directErrPath).find("direct import of retired soa compatibility modules is not supported") !=
        std::string::npos);

  const std::string runHelperReturnCmd =
      "./primec --emit=vm " + srcPath + " --entry /runHelperReturn 2> " + helperReturnErrPath;
  CHECK(runCommand(runHelperReturnCmd) == 2);
  CHECK(readFile(helperReturnErrPath).find("direct import of retired soa compatibility modules is not supported") !=
        std::string::npos);
}

TEST_CASE("vm rejects experimental soa stdlib from-aos helper before typed bindings support") {
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
  [vector<Particle>] values{vector<Particle>(Particle(7i32))}
  [SoaVector<Particle>] packed{soaVectorFromAos<Particle>(values)}
  return(soaVectorCount<Particle>(packed))
}
)";
  const std::string srcPath = writeTemp("vm_experimental_soa_from_aos.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_vm_experimental_soa_from_aos_err.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main 2> " + errPath;
  CHECK(runCommand(runCmd) == 2);
  CHECK(readFile(errPath).find("direct import of retired soa compatibility modules is not supported") !=
        std::string::npos);
}

TEST_CASE("vm runs experimental soa stdlib to-aos helper") {
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
  const std::string srcPath = writeTemp("vm_experimental_soa_to_aos.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 0);
}

TEST_CASE("vm runs experimental soa stdlib to-aos method on wrapper surface") {
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
  const std::string srcPath = writeTemp("vm_experimental_soa_to_aos_method.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 0);
}

TEST_CASE("vm no-import root soa to_aos bare and direct helper forms reject SoaVector-only canonical helper contract") {
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
  const std::string srcPath = writeTemp("vm_root_soa_to_aos_forms.prime", source);
  const std::string errPath = (testScratchPath("") / "primec_vm_root_soa_to_aos_forms_err.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main 2> " + errPath;
  CHECK(runCommand(runCmd) == 2);
  CHECK(readFile(errPath).find("unknown method: /to_aos") != std::string::npos);
}

TEST_CASE("vm no-import root soa to_aos method helper forms reject SoaVector-only canonical helper contract") {
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
  const std::string srcPath = writeTemp("vm_root_soa_to_aos_method_forms.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_vm_root_soa_to_aos_method_forms_err.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main 2> " + errPath;
  CHECK(runCommand(runCmd) == 2);
  CHECK(readFile(errPath).find("unknown method: /to_aos") != std::string::npos);
}

TEST_CASE("vm materializes non-empty root soa struct literals") {
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
  const std::string srcPath = writeTemp("vm_root_soa_non_empty_literal.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 2);
}

TEST_CASE("vm rejects non-empty root soa literals with unsupported element envelopes") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [soa<i32>] values{soa<i32>(1i32, 2i32)}
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("vm_root_soa_non_struct_literal.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_vm_root_soa_non_struct_literal_err.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main 2> " + errPath;
  CHECK(runCommand(runCmd) == 2);
  const std::string error = readFile(errPath);
  CHECK(error.find("soa requires struct element type") != std::string::npos);
  CHECK(error.find("stage: semantic") != std::string::npos);
}

TEST_CASE("vm materializes non-empty root soa literals above former local capacity limit") {
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
  const std::string srcPath = writeTemp("vm_root_soa_literal_limit_overflow.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 0);
}

TEST_CASE("vm runs experimental soa stdlib non-empty to-aos helper") {
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
  const std::string srcPath = writeTemp("vm_experimental_soa_to_aos_non_empty.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 1);
}

TEST_CASE("vm runs experimental soa stdlib non-empty to-aos method on wrapper state") {
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
  const std::string srcPath = writeTemp("vm_experimental_soa_to_aos_non_empty_method.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 1);
}

TEST_CASE("runs vm experimental soa stdlib get helper") {
  const std::string source = R"(
import /std/collections/soa/*

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
  const std::string srcPath = writeTemp("vm_experimental_soa_get.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 7);
}

TEST_CASE("runs vm experimental soa stdlib get method") {
  const std::string source = R"(
import /std/collections/soa/*

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
  const std::string srcPath = writeTemp("vm_experimental_soa_get_method.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 7);
}

TEST_CASE("runs vm bare soa get helper through helper return compatibility") {
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
  const std::string srcPath = writeTemp("vm_experimental_soa_get_helper_return.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 7);
}

TEST_CASE("runs vm global helper-return soa method shadows compatibility") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/soa/*

[struct reflect]
Particle() {
  [i32] x{1i32}
}

[return<SoaVector<Particle>>]
cloneValues() {
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
  [Particle] value{Particle(31i32)}
  return(plus(cloneValues().count(),
              plus(cloneValues().get(0i32).x,
                   plus(cloneValues().ref(0i32).x,
                        plus(cloneValues().push(value),
                             cloneValues().reserve(37i32))))))
}
)";
  const std::string srcPath =
      writeTemp("vm_experimental_soa_method_shadow_global_helper_return.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  // TODO-4756: .count()/.get()/.ref() method-call sugar no longer routes
  // through user /soa/count, /soa/get, /soa/ref shadow definitions (falls
  // through to the real builtins instead: count=1, get(0).x=7, ref(0).x=7),
  // while .push()/.reserve() still correctly invoke their shadows.
  // Pinned to the verified current (asymmetric) result: 1+7+7+31+37=83.
  CHECK(runCommand(runCmd) == 83);
}

TEST_CASE("runs vm method-like helper-return soa method shadows compatibility") {
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
  const std::string srcPath =
      writeTemp("vm_experimental_soa_method_shadow_method_like_helper_return.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  // TODO-4756: see the sibling "global helper-return" case above.
  CHECK(runCommand(runCmd) == 83);
}

TEST_CASE("runs vm vector-target old-explicit soa mutator shadows") {
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
      writeTemp("vm_vector_target_old_explicit_soa_mutator_shadow.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 10);
}

TEST_CASE("runs vm vector-target method soa mutator shadows") {
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
      writeTemp("vm_vector_target_method_soa_mutator_shadow.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 10);
}

TEST_CASE("runs vm vector-target to_aos helper shadows") {
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
      writeTemp("vm_vector_target_to_aos_shadow.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 27);
}

TEST_CASE("runs vm nested struct-body soa constructor-bearing helper returns compatibility") {
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
      writeTemp("vm_nested_struct_body_soa_constructor_helper.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 0);
}

TEST_CASE("runs vm nested struct-body soa direct and bound helper expressions compatibility") {
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
      writeTemp("vm_nested_struct_body_soa_direct_bound_helpers.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 16);
}

TEST_CASE("runs vm nested struct-body soa method shadows compatibility") {
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
      writeTemp("vm_nested_struct_body_soa_method_shadows.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  // TODO-4756: .count()/.get()/.ref() method-call sugar no longer invokes
  // user-defined /soa/count, /soa/get, /soa/ref shadows (falls through to
  // the real builtins: 1+7+7 instead of 13+23+29); .push()/.reserve() still
  // correctly invoke their shadows (31+37). Sum: 1+7+7+31+37+1 = 84.
  CHECK(runCommand(runCmd) == 84);
}

TEST_CASE("runs vm explicit method-like helper-return experimental soa to_aos shadow") {
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
      writeTemp("vm_experimental_soa_explicit_method_like_to_aos_shadow.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 1);
}

TEST_CASE("vm runs experimental soa stdlib ref helper") {
  const std::string source = R"(
import /std/collections/soa/*

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
  const std::string srcPath = writeTemp("vm_experimental_soa_ref.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 7);
}

TEST_CASE("vm runs experimental soa stdlib ref method") {
  const std::string source = R"(
import /std/collections/soa/*

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
  const std::string srcPath = writeTemp("vm_experimental_soa_ref_method.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 7);
}


TEST_SUITE_END();
