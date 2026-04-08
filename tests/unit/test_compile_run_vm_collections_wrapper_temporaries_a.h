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

TEST_CASE("runs vm experimental soa_vector stdlib helpers") {
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
  const std::string srcPath = writeTemp("vm_experimental_soa_vector_helpers.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 0);
}

TEST_CASE("runs vm experimental soa_vector stdlib non-empty helper") {
  const std::string source = R"(
import /std/collections/experimental_soa_vector/*

[struct reflect]
Particle() {
  [i32] x{1i32}
}

[effects(heap_alloc), return<int>]
main() {
  [SoaVector<Particle>] values{soaVectorSingle<Particle>(Particle(7i32))}
  return(soaVectorCount<Particle>(values))
}
)";
  const std::string srcPath = writeTemp("vm_experimental_soa_vector_single.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 1);
}

TEST_CASE("runs vm canonical soa_vector count helper on experimental wrapper") {
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
      writeTemp("vm_canonical_soa_vector_count_experimental_wrapper.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 2);
}

TEST_CASE("runs vm canonical soa_vector get helper") {
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
      writeTemp("vm_canonical_soa_vector_get_experimental_wrapper.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 9);
}

TEST_CASE("vm canonical soa_vector get helper rejects template arguments on non-soa receiver") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32)}
  return(/std/collections/soa_vector/get<i32>(values, 0i32))
}
)";
  const std::string srcPath =
      writeTemp("vm_canonical_soa_vector_get_non_soa_receiver.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_vm_canonical_soa_vector_get_non_soa_receiver_err.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main 2> " + errPath;
  CHECK(runCommand(runCmd) == 2);
  CHECK(readFile(errPath).find("get requires soa_vector target") !=
        std::string::npos);
}

TEST_CASE("vm canonical soa_vector get slash-method reaches field access reject") {
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
      writeTemp("vm_canonical_soa_vector_get_slash_method.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_vm_canonical_soa_vector_get_slash_method_err.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main 2> " + errPath;
  CHECK(runCommand(runCmd) == 2);
  CHECK(readFile(errPath).find("field access requires struct receiver") !=
        std::string::npos);
}

TEST_CASE("vm canonical soa_vector to_aos slash-method keeps canonical reject") {
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
      writeTemp("vm_canonical_soa_vector_to_aos_slash_method.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_vm_canonical_soa_vector_to_aos_slash_method_err.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main 2> " + errPath;
  CHECK(runCommand(runCmd) == 2);
  CHECK(readFile(errPath).find("struct parameter type mismatch") != std::string::npos);
  CHECK(readFile(errPath).find("/std/collections/experimental_soa_vector/SoaVector__") !=
        std::string::npos);
}

TEST_CASE("runs vm canonical soa_vector ref helper") {
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
      writeTemp("vm_canonical_soa_vector_ref_experimental_wrapper.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 9);
}

TEST_CASE("runs vm canonical soa_vector mutator helpers") {
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
      writeTemp("vm_canonical_soa_vector_mutators_experimental_wrapper.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 11);
}

TEST_CASE("vm canonical soa_vector to_aos helper lowers") {
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
      writeTemp("vm_canonical_soa_vector_to_aos_experimental_wrapper.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 1);
}

TEST_CASE("vm canonical soa_vector to_aos temporaries route through canonical vector capacity") {
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
      writeTemp("vm_canonical_soa_vector_to_aos_vector_capacity_experimental_wrapper.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 1);
}

TEST_CASE("vm wildcard-imported canonical soa_vector helpers lower") {
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
      writeTemp("vm_wildcard_canonical_soa_vector_helpers.prime", source);
  const std::string outPath =
      (testScratchPath("") / "primec_vm_wildcard_canonical_soa_vector_helpers.psvm").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " -o " + outPath + " --entry /main";
  CHECK(runCommand(runCmd) == 17);
}

TEST_CASE("vm runs graph-solved direct local-auto vector helper shadows") {
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
    "vm rejects experimental soa_vector stdlib wide structs on pending width boundary across imported/direct/helper-return forms") {
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
runImported() {
  [SoaVector<Particle17>] values{soaVectorNew<Particle17>()}
  return(soaVectorCount<Particle17>(values))
}

[effects(heap_alloc), return<int>]
runDirectCanonical() {
  [SoaVector<Particle17>] values{/std/collections/experimental_soa_vector/soaVectorNew<Particle17>()}
  return(/std/collections/experimental_soa_vector/soaVectorCount<Particle17>(values))
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
  const std::string srcPath = writeTemp("vm_experimental_soa_vector_wide_pending_forms.prime", source);
  const std::string importedErrPath =
      (testScratchPath("") / "primec_vm_experimental_soa_vector_wide_pending_imported_err.txt").string();
  const std::string directErrPath =
      (testScratchPath("") / "primec_vm_experimental_soa_vector_wide_pending_direct_err.txt").string();
  const std::string helperReturnErrPath =
      (testScratchPath("") / "primec_vm_experimental_soa_vector_wide_pending_helper_return_err.txt").string();

  const std::string runImportedCmd =
      "./primec --emit=vm " + srcPath + " --entry /runImported 2> " + importedErrPath;
  CHECK(runCommand(runImportedCmd) == 3);
  CHECK(readFile(importedErrPath) == "array index out of bounds\n");

  const std::string runDirectCmd =
      "./primec --emit=vm " + srcPath + " --entry /runDirectCanonical 2> " + directErrPath;
  CHECK(runCommand(runDirectCmd) == 3);
  CHECK(readFile(directErrPath) == "array index out of bounds\n");

  const std::string runHelperReturnCmd =
      "./primec --emit=vm " + srcPath + " --entry /runHelperReturn 2> " + helperReturnErrPath;
  CHECK(runCommand(runHelperReturnCmd) == 3);
  CHECK(readFile(helperReturnErrPath) == "array index out of bounds\n");
}

TEST_CASE("vm rejects experimental soa_vector stdlib from-aos helper before typed bindings support") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_soa_vector/*

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
  const std::string srcPath = writeTemp("vm_experimental_soa_vector_from_aos.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_vm_experimental_soa_vector_from_aos_err.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main 2> " + errPath;
  CHECK(runCommand(runCmd) == 2);
  CHECK(readFile(errPath).find("vm backend only supports numeric/bool/string vector literals") !=
        std::string::npos);
}

TEST_CASE("vm runs experimental soa_vector stdlib to-aos helper") {
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
  const std::string srcPath = writeTemp("vm_experimental_soa_vector_to_aos.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 0);
}

TEST_CASE("vm runs experimental soa_vector stdlib to-aos method on wrapper surface") {
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
  const std::string srcPath = writeTemp("vm_experimental_soa_vector_to_aos_method.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 0);
}

TEST_CASE("vm no-import root soa_vector to_aos bare and direct helper forms run") {
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
  const std::string srcPath = writeTemp("vm_root_soa_vector_to_aos_forms.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 0);
}

TEST_CASE("vm no-import root soa_vector to_aos method helper forms run") {
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
  const std::string srcPath = writeTemp("vm_root_soa_vector_to_aos_method_forms.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 0);
}

TEST_CASE("vm materializes non-empty root soa_vector struct literals") {
  const std::string source = R"(
[struct reflect]
Particle() {
  [i32] x{1i32}
}

[effects(heap_alloc), return<int>]
main() {
  [soa_vector<Particle>] values{soa_vector<Particle>(Particle(7i32), Particle(9i32))}
  return(count(values))
}
)";
  const std::string srcPath = writeTemp("vm_root_soa_vector_non_empty_literal.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 2);
}

TEST_CASE("vm rejects non-empty root soa_vector literals with unsupported element envelopes") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [soa_vector<i32>] values{soa_vector<i32>(1i32, 2i32)}
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("vm_root_soa_vector_non_struct_literal.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_vm_root_soa_vector_non_struct_literal_err.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main 2> " + errPath;
  CHECK(runCommand(runCmd) == 2);
  CHECK(readFile(errPath).find("soa_vector requires struct element type") != std::string::npos);
}

TEST_CASE("vm rejects non-empty root soa_vector literals above local capacity limit") {
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
      "  [soa_vector<Particle>] values{soa_vector<Particle>(") +
                             buildParticleLiteralArgs(257) +
                             ")}\n"
                             "  return(0i32)\n"
                             "}\n";
  const std::string srcPath = writeTemp("vm_root_soa_vector_literal_limit_overflow.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_vm_root_soa_vector_literal_limit_overflow_err.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main 2> " + errPath;
  CHECK(runCommand(runCmd) == 2);
  CHECK(readFile(errPath).find("soa_vector literal exceeds local capacity limit (256)") !=
        std::string::npos);
}

TEST_CASE("vm runs experimental soa_vector stdlib non-empty to-aos helper") {
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
  const std::string srcPath = writeTemp("vm_experimental_soa_vector_to_aos_non_empty.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 1);
}

TEST_CASE("vm runs experimental soa_vector stdlib non-empty to-aos method on wrapper state") {
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
  const std::string srcPath = writeTemp("vm_experimental_soa_vector_to_aos_non_empty_method.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 1);
}

TEST_CASE("runs vm experimental soa_vector stdlib get helper") {
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
  const std::string srcPath = writeTemp("vm_experimental_soa_vector_get.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 7);
}

TEST_CASE("runs vm experimental soa_vector stdlib get method") {
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
  const std::string srcPath = writeTemp("vm_experimental_soa_vector_get_method.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 7);
}

TEST_CASE("runs vm bare soa_vector get helper through helper return") {
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
  const std::string srcPath = writeTemp("vm_experimental_soa_vector_get_helper_return.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 7);
}

TEST_CASE("runs vm global helper-return soa_vector method shadows") {
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
      writeTemp("vm_experimental_soa_vector_method_shadow_global_helper_return.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 131);
}

TEST_CASE("runs vm method-like helper-return soa_vector method shadows") {
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
      writeTemp("vm_experimental_soa_vector_method_shadow_method_like_helper_return.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 131);
}

TEST_CASE("runs vm vector-target old-explicit soa mutator shadows") {
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
      writeTemp("vm_vector_target_old_explicit_soa_mutator_shadow.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 10);
}

TEST_CASE("runs vm vector-target method soa mutator shadows") {
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
  [vector<i32>] values{vectorSingle<i32>(1i32)}
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

TEST_CASE("runs vm nested struct-body soa_vector constructor-bearing helper returns") {
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
      writeTemp("vm_nested_struct_body_soa_vector_constructor_helper.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 0);
}

TEST_CASE("runs vm nested struct-body soa_vector direct and bound helper expressions") {
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
      writeTemp("vm_nested_struct_body_soa_vector_direct_bound_helpers.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 16);
}

TEST_CASE("runs vm nested struct-body soa_vector method shadows") {
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
      writeTemp("vm_nested_struct_body_soa_vector_method_shadows.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 134);
}

TEST_CASE("runs vm explicit method-like helper-return experimental soa_vector to_aos shadow") {
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
      writeTemp("vm_experimental_soa_vector_explicit_method_like_to_aos_shadow.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 1);
}

TEST_CASE("vm runs experimental soa_vector stdlib ref helper") {
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
  const std::string srcPath = writeTemp("vm_experimental_soa_vector_ref.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 7);
}

TEST_CASE("vm runs experimental soa_vector stdlib ref method") {
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
  const std::string srcPath = writeTemp("vm_experimental_soa_vector_ref_method.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 7);
}

TEST_CASE("vm runs experimental soa_vector ref pass-through and return") {
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
  const std::string srcPath = writeTemp("vm_experimental_soa_vector_ref_passthrough.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 7);
}

TEST_CASE("runs vm experimental soa_vector stdlib push and reserve helpers") {
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
  const std::string srcPath = writeTemp("vm_experimental_soa_vector_push_helpers.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 11);
}

TEST_CASE("runs vm experimental soa_vector stdlib push and reserve methods") {
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
  const std::string srcPath = writeTemp("vm_experimental_soa_vector_push_method.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 11);
}

TEST_CASE("vm runs experimental soa_vector single-field index syntax") {
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
  const std::string srcPath = writeTemp("vm_experimental_soa_vector_single_field_view.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 9);
}

TEST_CASE("vm runs experimental soa_vector reflected multi-field index syntax") {
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
  const std::string srcPath = writeTemp("vm_experimental_soa_vector_field_view.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 12);
}

TEST_CASE("vm runs experimental soa_vector mutating indexed field writes") {
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
      writeTemp("vm_experimental_soa_vector_mutating_indexed_field_writes.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 52);
}

TEST_CASE("vm runs richer borrowed experimental soa_vector mutating indexed field writes") {
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
      "vm_experimental_soa_vector_richer_borrowed_mutating_indexed_field_writes.prime",
      source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 36);
}

TEST_CASE("vm runs method-like borrowed experimental soa_vector mutating indexed field writes") {
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
      "vm_experimental_soa_vector_method_like_borrowed_mutating_indexed_field_writes.prime",
      source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 104);
}

TEST_CASE("vm runs borrowed experimental soa_vector reflected index syntax") {
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
  const std::string srcPath = writeTemp("vm_experimental_soa_vector_borrowed_field_view.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 12);
}

TEST_CASE("vm runs experimental soa_vector bare get and ref field access") {
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
  const std::string srcPath = writeTemp("vm_experimental_soa_vector_bare_ref_field_access.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 20);
}

TEST_CASE("vm runs borrowed local experimental soa_vector reflected index syntax") {
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
      writeTemp("vm_experimental_soa_vector_borrowed_local_field_view.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 12);
}

TEST_CASE("vm runs borrowed helper-return experimental soa_vector reflected index syntax") {
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
      writeTemp("vm_experimental_soa_vector_borrowed_return_field_view.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 12);
}

TEST_CASE("vm runs experimental soa_vector reflected call-form index syntax") {
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
      writeTemp("vm_experimental_soa_vector_call_form_field_view.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 36);
}

TEST_CASE("vm runs experimental soa_vector inline location borrow index syntax") {
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
      writeTemp("vm_experimental_soa_vector_inline_location_field_view.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 40);
}

TEST_CASE("vm runs dereferenced borrowed helper-return experimental soa_vector reflected index syntax") {
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
      writeTemp("vm_experimental_soa_vector_dereferenced_borrowed_return_field_view.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 12);
}

TEST_CASE("vm runs borrowed helper-return experimental soa_vector get/ref methods") {
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
      writeTemp("vm_experimental_soa_vector_borrowed_return_get_ref.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 16);
}

TEST_CASE("vm runs borrowed local experimental soa_vector read-only methods") {
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
      writeTemp("vm_experimental_soa_vector_borrowed_local_methods.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 38);
}

TEST_CASE("vm runs inline location experimental soa_vector read-only methods") {
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
      writeTemp("vm_experimental_soa_vector_inline_location_methods.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 40);
}

TEST_CASE("vm runs borrowed helper-return experimental soa_vector helper surfaces") {
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
      writeTemp("vm_experimental_soa_vector_borrowed_return_methods.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 38);
}

TEST_CASE("vm runs method-like borrowed helper-return experimental soa_vector helper surfaces") {
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
      writeTemp("vm_experimental_soa_vector_method_like_borrowed_return_helpers.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 85);
}

TEST_CASE("vm runs direct return borrowed helper-return experimental soa_vector reads") {
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
      "vm_experimental_soa_vector_direct_return_borrowed_return_reads.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 55);
}

TEST_CASE("vm runs direct return method-like borrowed helper-return experimental soa_vector reads") {
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
      "vm_experimental_soa_vector_direct_return_method_like_borrowed_return_reads.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 55);
}

TEST_CASE("vm runs direct return inline location borrowed helper-return experimental soa_vector reads") {
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
      "vm_experimental_soa_vector_direct_return_inline_location_borrowed_return_reads.prime",
      source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 52);
}

TEST_CASE("vm runs inline location method-like borrowed helper-return experimental soa_vector helpers") {
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
      "vm_experimental_soa_vector_inline_location_method_like_borrowed_return_helpers.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 147);
}

TEST_CASE("vm runs direct return inline location method-like borrowed helper-return experimental soa_vector reads") {
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
      "vm_experimental_soa_vector_direct_return_inline_location_method_like_borrowed_return_reads.prime",
      source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 52);
}

TEST_CASE("vm runs inline location borrowed helper-return experimental soa_vector helpers") {
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
      writeTemp("vm_experimental_soa_vector_inline_location_borrowed_return_helpers.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 116);
}

TEST_CASE("runs vm experimental soa storage helpers") {
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
  const std::string srcPath = writeTemp("vm_experimental_soa_storage.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 14);
}

TEST_CASE("runs vm experimental soa storage borrowed ref helper") {
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
  const std::string srcPath = writeTemp("vm_experimental_soa_storage_ref.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 7);
}

TEST_CASE("runs vm experimental soa storage borrowed view helper") {
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
  const std::string srcPath = writeTemp("vm_experimental_soa_storage_view.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 9);
}

TEST_CASE("rejects vm experimental soa storage reserve overflow") {
  const std::string source = R"(
import /std/collections/experimental_soa_storage/*

[effects(heap_alloc), return<int>]
main() {
  [SoaColumn<i32> mut] values{soaColumnNew<i32>()}
  soaColumnReserve<i32>(values, 1073741824i32)
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("vm_experimental_soa_storage_reserve_overflow.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_vm_experimental_soa_storage_reserve_overflow_err.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main 2> " + errPath;
  CHECK(runCommand(runCmd) == 3);
  CHECK(readFile(errPath) == "vector reserve allocation failed (out of memory)\n");
}

TEST_CASE("runs vm experimental two-column soa storage helpers") {
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
  const std::string srcPath = writeTemp("vm_experimental_soa_storage_two_columns.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 24);
}

TEST_CASE("runs vm experimental three-column soa storage helpers") {
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
  const std::string srcPath = writeTemp("vm_experimental_soa_storage_three_columns.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 60);
}

TEST_CASE("runs vm experimental four-column soa storage helpers") {
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
  const std::string srcPath = writeTemp("vm_experimental_soa_storage_four_columns.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 105);
}

TEST_CASE("runs vm experimental five-column soa storage helpers") {
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
  const std::string srcPath = writeTemp("vm_experimental_soa_storage_five_columns.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 176);
}

TEST_CASE("runs vm experimental six-column soa storage helpers") {
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
  const std::string srcPath = writeTemp("vm_experimental_soa_storage_six_columns.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 189);
}

TEST_CASE("runs vm experimental seven-column soa storage helpers") {
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
  const std::string srcPath = writeTemp("vm_experimental_soa_storage_seven_columns.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 212);
}

TEST_CASE("runs vm experimental eight-column soa storage helpers") {
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
  const std::string srcPath = writeTemp("vm_experimental_soa_storage_eight_columns.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 231);
}

TEST_CASE("runs vm experimental nine-column soa storage helpers") {
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
  const std::string srcPath = writeTemp("vm_experimental_soa_storage_nine_columns.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 45);
}

TEST_CASE("runs vm experimental ten-column soa storage helpers") {
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
  const std::string srcPath = writeTemp("vm_experimental_soa_storage_ten_columns.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 74);
}

TEST_CASE("runs vm experimental eleven-column soa storage helpers") {
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
  const std::string srcPath = writeTemp("vm_experimental_soa_storage_eleven_columns.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 34);
}

TEST_CASE("runs vm experimental twelve-column soa storage helpers") {
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
  const std::string srcPath = writeTemp("vm_experimental_soa_storage_twelve_columns.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 47);
}

TEST_CASE("runs vm experimental thirteen-column soa storage helpers") {
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
  const std::string srcPath = writeTemp("vm_experimental_soa_storage_thirteen_columns.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 49);
}

TEST_CASE("runs vm experimental fourteen-column soa storage helpers") {
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
  const std::string srcPath = writeTemp("vm_experimental_soa_storage_fourteen_columns.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 53);
}

TEST_CASE("runs vm experimental fifteen-column soa storage helpers") {
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
  const std::string srcPath = writeTemp("vm_experimental_soa_storage_fifteen_columns.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 59);
}


TEST_CASE("runs vm experimental sixteen-column soa storage helpers") {
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
  const std::string srcPath = writeTemp("vm_experimental_soa_storage_sixteen_columns.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 143);
}

TEST_CASE("runs vm with stdlib collection shim helpers") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vectorSingle<i32>(7i32)}
  [map<i32, i32>] pairs{mapSingle<i32, i32>(3i32, 9i32)}
  [i32 mut] total{plus(vectorCount<i32>(values), mapCount<i32, i32>(pairs))}
  [vector<i32>] emptyValues{vectorNew<i32>()}
  [map<i32, i32>] emptyPairs{mapNew<i32, i32>()}
  assign(total, plus(total, plus(vectorCount<i32>(emptyValues), mapCount<i32, i32>(emptyPairs))))
  return(total)
}
)";
  const std::string srcPath = writeTemp("vm_stdlib_collection_shims.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 2);
}

TEST_CASE("runs vm with stdlib collection shim multi constructors") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vectorTriple<i32>(5i32, 7i32, 9i32)}
  [map<i32, i32>] pairs{mapDouble<i32, i32>(1i32, 11i32, 2i32, 22i32)}
  [i32] vectorTotal{plus(vectorAt<i32>(values, 0i32), vectorAt<i32>(values, 2i32))}
  [i32] mapTotal{plus(mapAt<i32, i32>(pairs, 1i32), mapAtUnsafe<i32, i32>(pairs, 2i32))}
  return(plus(plus(vectorTotal, mapTotal), plus(vectorCount<i32>(values), mapCount<i32, i32>(pairs))))
}
)";
  const std::string srcPath = writeTemp("vm_stdlib_collection_shim_multi_ctor.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 52);
}

TEST_CASE("runs vm with templated stdlib collection return envelopes") {
  const std::string source = R"(
import /std/collections/*

[return<vector<T>>]
wrapVector<T>([T] value) {
  return(vectorSingle<T>(value))
}

[return<map<K, V>>]
wrapMap<K, V>([K] key, [V] value) {
  [map<K, V>] values{mapSingle<K, V>(key, value)}
  return(values)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{wrapVector<i32>(9i32)}
  [map<string, i32>] pairs{wrapMap<string, i32>("only"raw_utf8, 4i32)}
  return(plus(plus(vectorCount<i32>(values), mapCount<string, i32>(pairs)), mapAt<string, i32>(pairs, "only"raw_utf8)))
}
)";
  const std::string srcPath = writeTemp("vm_stdlib_collection_shim_templated_returns.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 6);
}

TEST_CASE("runs vm templated stdlib return wrapper temporaries in expressions") {
  const std::string source = R"(
import /std/collections/*

[return<vector<T>>]
wrapVector<T>([T] value) {
  return(vectorSingle<T>(value))
}

[return<map<K, V>>]
wrapMap<K, V>([K] key, [V] value) {
  [map<K, V>] values{mapSingle<K, V>(key, value)}
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
  const std::string srcPath = writeTemp("vm_stdlib_collection_shim_templated_return_temporaries.prime", source);
  const std::string outPath =
      (std::filesystem::temp_directory_path() /
       "primec_vm_stdlib_collection_shim_templated_return_temporaries.out")
          .string();
  const std::string runCmd =
      "./primec --emit=vm " + srcPath + " --entry /main > " + outPath + " 2>&1";
  CHECK(runCommand(runCmd) == 10);
  CHECK(readFile(outPath).empty());
}

TEST_CASE("runs vm with templated stdlib wrapper temporary call forms") {
  const std::string source = R"(
import /std/collections/*

[return<map<K, V>>]
wrapMap<K, V>([K] key, [V] value) {
  [map<K, V>] values{mapSingle<K, V>(key, value)}
  return(values)
}

[return<int>]
main() {
  [i32] a{mapAt<string, i32>(wrapMap<string, i32>("only"raw_utf8, 4i32), "only"raw_utf8)}
  [i32] b{mapAtUnsafe<string, i32>(wrapMap<string, i32>("only"raw_utf8, 4i32), "only"raw_utf8)}
  [i32] c{mapCount<string, i32>(wrapMap<string, i32>("only"raw_utf8, 4i32))}
  return(plus(plus(a, b), c))
}
)";
  const std::string srcPath = writeTemp("vm_stdlib_collection_shim_templated_return_temp_call_forms.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 9);
}

TEST_CASE("runs vm shared stdlib map conformance harness") {
  expectVmSharedStdlibMapConformanceHarness();
}

TEST_CASE("runs vm canonical namespaced map helpers on experimental map values") {
  expectCanonicalMapNamespaceExperimentalValueConformance("vm");
}

TEST_CASE("runs vm wrapper map helpers on experimental map values") {
  expectWrapperMapHelperExperimentalValueConformance("vm");
}

TEST_CASE("runs vm ownership-sensitive experimental map value methods") {
  expectExperimentalMapOwnershipMethodConformance("vm");
}

TEST_CASE("runs vm helper-wrapped inferred experimental map returns") {
  expectWrappedInferredExperimentalMapReturnConformance("vm");
}

TEST_CASE("runs vm helper-wrapped experimental map parameters") {
  expectWrappedExperimentalMapParameterConformance("vm");
}

TEST_CASE("runs vm helper-wrapped experimental map bindings") {
  expectWrappedExperimentalMapBindingConformance("vm");
}

TEST_CASE("runs vm helper-wrapped experimental map assignment RHS values") {
  expectWrappedExperimentalMapAssignConformance("vm");
}

TEST_CASE("runs vm canonical namespaced map constructors on explicit experimental map bindings") {
  expectCanonicalMapNamespaceExperimentalConstructorConformance("vm");
}

TEST_CASE("runs vm canonical namespaced map constructors through explicit experimental map returns") {
  expectCanonicalMapNamespaceExperimentalReturnConformance("vm");
}

TEST_CASE("runs vm canonical namespaced map constructors through explicit experimental map parameters") {
  expectCanonicalMapNamespaceExperimentalParameterConformance("vm");
}

TEST_CASE("runs vm wrapper map constructors on explicit experimental map bindings") {
  expectWrapperMapConstructorExperimentalBindingConformance("vm");
}

TEST_CASE("runs vm wrapper map constructors through explicit experimental map returns") {
  expectWrapperMapConstructorExperimentalReturnConformance("vm");
}

TEST_CASE("runs vm wrapper map constructors through explicit experimental map parameters") {
  expectWrapperMapConstructorExperimentalParameterConformance("vm");
}

TEST_CASE("runs vm experimental map variadic constructors") {
  expectExperimentalMapVariadicConstructorConformance("vm");
}

TEST_CASE("rejects vm experimental map variadic constructor type mismatch") {
  expectExperimentalMapVariadicConstructorMismatchReject("vm");
}

TEST_CASE("runs vm experimental map constructor assignments") {
  expectExperimentalMapAssignConformance("vm");
}

TEST_CASE("runs vm implicit map auto constructor inference") {
  expectImplicitMapAutoInferenceConformance("vm");
}

TEST_CASE("runs vm inferred experimental map returns") {
  expectInferredExperimentalMapReturnConformance("vm");
}

TEST_CASE("runs vm block inferred experimental map returns") {
  expectBlockInferredExperimentalMapReturnConformance("vm");
}

TEST_CASE("runs vm auto block inferred experimental map returns") {
  expectAutoBlockInferredExperimentalMapReturnConformance("vm");
}

TEST_CASE("runs vm inferred experimental map call receivers") {
  expectInferredExperimentalMapCallReceiverConformance("vm");
}

TEST_CASE("runs vm experimental map struct fields") {
  expectExperimentalMapStructFieldConformance("vm");
}

TEST_CASE("runs vm inferred experimental map struct fields") {
  expectInferredExperimentalMapStructFieldConformance("vm");
}

TEST_CASE("runs vm helper-wrapped inferred experimental map struct fields") {
  expectWrappedInferredExperimentalMapStructFieldConformance("vm");
}

TEST_CASE("runs vm experimental map method parameters") {
  expectExperimentalMapMethodParameterConformance("vm");
}

TEST_CASE("runs vm inferred experimental map parameters") {
  expectInferredExperimentalMapParameterConformance("vm");
}

TEST_CASE("runs vm inferred experimental map default parameters") {
  expectInferredExperimentalMapDefaultParameterConformance("vm");
}

TEST_CASE("runs vm helper-wrapped inferred experimental map default parameters") {
  expectWrappedInferredExperimentalMapDefaultParameterConformance("vm");
}

TEST_CASE("runs vm experimental map helper receivers") {
  expectExperimentalMapHelperReceiverConformance("vm");
}

TEST_CASE("runs vm helper-wrapped experimental map helper receivers") {
  expectWrappedExperimentalMapHelperReceiverConformance("vm");
}

TEST_CASE("runs vm experimental map method receivers") {
  expectExperimentalMapMethodReceiverConformance("vm");
}

TEST_CASE("runs vm helper-wrapped experimental map method receivers") {
  expectWrappedExperimentalMapMethodReceiverConformance("vm");
}

TEST_CASE("runs vm experimental map field assignments") {
  expectExperimentalMapFieldAssignConformance("vm");
}

TEST_CASE("runs vm helper-wrapped Result.ok experimental map result struct fields") {
  expectWrappedExperimentalMapResultFieldAssignConformance("vm");
}

TEST_CASE("runs vm dereferenced experimental map storage references") {
  expectExperimentalMapStorageReferenceConformance("vm");
}

TEST_CASE("runs vm helper-wrapped dereferenced Result.ok experimental map result struct fields") {
  expectWrappedExperimentalMapResultDerefFieldAssignConformance("vm");
}

TEST_CASE("runs vm helper-wrapped experimental map struct storage fields") {
  expectWrappedExperimentalMapStorageFieldConformance("vm");
}

TEST_CASE("runs vm helper-wrapped dereferenced experimental map struct storage fields") {
  expectWrappedExperimentalMapStorageDerefFieldConformance("vm");
}

TEST_CASE("rejects vm canonical namespaced map helpers on borrowed experimental map values") {
  expectCanonicalMapNamespaceExperimentalReferenceConformance("vm");
}

TEST_CASE("runs vm canonical namespaced map _ref helpers on borrowed experimental map values") {
  expectCanonicalMapNamespaceExperimentalBorrowedRefConformance("vm");
}

TEST_CASE("runs vm experimental map methods") {
  expectExperimentalMapMethodConformance("vm");
}

TEST_CASE("runs vm borrowed experimental map helpers") {
  expectExperimentalMapReferenceHelperConformance("vm");
}

TEST_CASE("runs vm borrowed experimental map methods") {
  expectExperimentalMapReferenceMethodConformance("vm");
}

TEST_CASE("runs vm experimental map inserts") {
  expectExperimentalMapInsertConformance("vm");
}

TEST_CASE("runs vm experimental map ownership-sensitive values") {
  expectExperimentalMapOwnershipConformance("vm");
}

TEST_CASE("runs vm canonical namespaced map inserts on explicit experimental map bindings") {
  expectCanonicalMapNamespaceExperimentalInsertConformance("vm");
}

TEST_CASE("runs vm builtin canonical map first-growth inserts") {
  expectBuiltinCanonicalMapInsertFirstGrowthConformance("vm");
}

TEST_CASE("runs vm builtin canonical map repeated-growth inserts") {
  expectBuiltinCanonicalMapInsertRepeatedGrowthConformance("vm");
}

TEST_CASE("runs vm builtin canonical map insert overwrites") {
  expectBuiltinCanonicalMapInsertOverwriteConformance("vm");
}

TEST_CASE("runs vm builtin canonical map non-local growth") {
  expectBuiltinCanonicalMapInsertNonLocalGrowthConformance("vm");
}

TEST_CASE("runs vm builtin canonical map nested non-local growth") {
  expectBuiltinCanonicalMapInsertNestedNonLocalGrowthConformance("vm");
}

TEST_CASE("runs vm builtin canonical map helper-return borrowed method inserts") {
  expectBuiltinCanonicalMapInsertHelperReturnBorrowedMethodConformance("vm");
}

TEST_CASE("rejects vm builtin canonical map direct insert on helper-return value receivers") {
  expectBuiltinCanonicalMapInsertHelperReturnValueDirectReject("vm");
}

TEST_CASE("rejects vm builtin canonical map method insert on helper-return value receivers") {
  expectBuiltinCanonicalMapInsertHelperReturnValueMethodReject("vm");
}

TEST_CASE("rejects vm canonical map constructor ownership growth") {
  expectCanonicalMapNamespaceOwnershipReject("vm");
}

TEST_CASE("runs vm experimental map bracket access") {
  expectExperimentalMapIndexConformance("vm");
}

TEST_CASE("runs vm experimental map custom comparable struct keys") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_map/*

[struct]
Key() {
  [i32] value{0i32}
}

[return<bool>]
/Key/equal([Key] left, [Key] right) {
  return(equal(left.value, right.value))
}

[return<bool>]
/Key/less_than([Key] left, [Key] right) {
  return(less_than(left.value, right.value))
}

[effects(heap_alloc), return<int>]
main() {
  [Map<Key, i32>] values{mapPair<Key, i32>(Key(2i32), 7i32, Key(5i32), 11i32)}
  [i32 mut] total{mapCount<Key, i32>(values)}
  assign(total, plus(total, mapAt<Key, i32>(values, Key(2i32))))
  assign(total, plus(total, mapAtUnsafe<Key, i32>(values, Key(5i32))))
  if(mapContains<Key, i32>(values, Key(2i32)),
     then() { assign(total, plus(total, 1i32)) },
     else() { })
  return(total)
}
)";
  const std::string srcPath = writeTemp("vm_experimental_map_custom_comparable_key.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 21);
}

TEST_CASE("runs vm shared stdlib vector conformance harness") {
  expectVmSharedStdlibVectorConformanceHarness();
}

TEST_CASE("runs vm shared vector conformance harness for stdlib and experimental helpers") {
  expectSharedVectorConformanceHarness("vm");
}

TEST_CASE("runs vm canonical namespaced vector helpers") {
  expectCanonicalVectorNamespaceConformance("vm");
}

TEST_CASE("runs vm canonical namespaced vector helpers on explicit Vector bindings") {
  expectCanonicalVectorNamespaceExplicitVectorBindingConformance("vm");
}

TEST_CASE("runs vm stdlib wrapper vector helpers on explicit Vector bindings") {
  expectStdlibWrapperVectorHelperExplicitVectorBindingConformance("vm");
}

TEST_CASE("rejects vm stdlib wrapper vector helper explicit Vector mismatch") {
  expectStdlibWrapperVectorHelperExplicitVectorBindingMismatchReject("vm");
}

TEST_CASE("runs vm stdlib wrapper vector constructors on explicit Vector bindings") {
  expectStdlibWrapperVectorConstructorExplicitVectorBindingConformance("vm");
}

TEST_CASE("rejects vm stdlib wrapper vector constructor explicit Vector mismatch") {
  expectStdlibWrapperVectorConstructorExplicitVectorBindingMismatchReject("vm");
}

TEST_CASE("runs vm stdlib wrapper vector constructors on inferred auto bindings") {
  expectStdlibWrapperVectorConstructorAutoInferenceConformance("vm");
}

TEST_CASE("rejects vm stdlib wrapper vector constructor auto inference mismatch") {
  expectStdlibWrapperVectorConstructorAutoInferenceMismatchReject("vm");
}

TEST_CASE("runs vm stdlib wrapper vector constructor receivers") {
  expectStdlibWrapperVectorConstructorReceiverConformance("vm");
}

TEST_CASE("rejects vm stdlib wrapper vector helper receiver mismatch") {
  expectStdlibWrapperVectorConstructorHelperReceiverMismatchReject("vm");
}

TEST_CASE("rejects vm stdlib wrapper vector method receiver mismatch") {
  expectStdlibWrapperVectorConstructorMethodReceiverMismatchReject("vm");
}

TEST_CASE("rejects vm canonical namespaced vector constructor temporaries") {
  expectCanonicalVectorNamespaceTemporaryReceiverConformance("vm");
}

TEST_CASE("rejects vm canonical namespaced vector explicit builtin bindings") {
  expectCanonicalVectorNamespaceExplicitBindingReject("vm");
}

TEST_CASE("rejects vm canonical namespaced vector named-argument temporaries") {
  expectCanonicalVectorNamespaceNamedArgsTemporaryReceiverConformance("vm");
}

TEST_CASE("rejects vm canonical namespaced vector named-argument explicit builtin bindings") {
  expectCanonicalVectorNamespaceNamedArgsExplicitBindingReject("vm");
}

TEST_CASE("rejects vm canonical namespaced vector mutators without imported helpers") {
  expectCanonicalVectorClearImportRequirement("vm");
  expectCanonicalVectorRemoveAtImportRequirement("vm");
  expectCanonicalVectorRemoveSwapImportRequirement("vm");
}

TEST_CASE("runs vm experimental vector helper runtime contracts") {
  expectExperimentalVectorRuntimeContracts("vm");
}

TEST_CASE("runs vm experimental vector ownership-sensitive helpers") {
  expectExperimentalVectorOwnershipContracts("vm");
}

TEST_CASE("runs vm canonical vector helpers on experimental vector receivers") {
  expectExperimentalVectorCanonicalHelperRoutingConformance("vm");
}

TEST_CASE("runs vm vector pop empty runtime contract") {
  SUBCASE("call") {
    expectVectorPopEmptyRuntimeContract("vm", false);
  }

  SUBCASE("method") {
    expectVectorPopEmptyRuntimeContract("vm", true);
  }
}

TEST_CASE("runs vm vector index runtime contract") {
  expectVectorIndexRuntimeContract("vm", "access_call");
  expectVectorIndexRuntimeContract("vm", "access_method");
  expectVectorIndexRuntimeContract("vm", "access_bracket");
  expectVectorIndexRuntimeContract("vm", "remove_at_call");
  expectVectorIndexRuntimeContract("vm", "remove_at_method");
  expectVectorIndexRuntimeContract("vm", "remove_swap_call");
  expectVectorIndexRuntimeContract("vm", "remove_swap_method");
}

TEST_CASE("runs vm imported container error contract conformance") {
  expectContainerErrorConformance("vm");
}

TEST_CASE("runs vm checked pointer conformance harness for imported .prime helpers") {
  expectCheckedPointerHelperSurfaceConformance("vm");
  expectCheckedPointerGrowthConformance("vm");
  expectCheckedPointerOutOfBoundsConformance("vm");
}

TEST_CASE("runs vm with templated stdlib vector wrapper temporary call forms") {
  const std::string source = R"(
import /std/collections/*

[return<vector<T>>]
wrapVector<T>([T] value) {
  return(vectorSingle<T>(value))
}

[effects(heap_alloc), return<int>]
main() {
  [i32] a{vectorAt<i32>(wrapVector<i32>(4i32), 0i32)}
  [i32] b{vectorAtUnsafe<i32>(wrapVector<i32>(5i32), 0i32)}
  [i32] c{vectorCount<i32>(wrapVector<i32>(6i32))}
  [i32] d{vectorCapacity<i32>(wrapVector<i32>(7i32))}
  return(plus(plus(plus(a, b), c), d))
}
)";
  const std::string srcPath =
      writeTemp("vm_stdlib_collection_shim_templated_return_vector_temp_call_forms.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 11);
}

TEST_CASE("runs vm templated stdlib vector wrapper temporary methods in expressions") {
  const std::string source = R"(
import /std/collections/*

[return<vector<T>>]
wrapVector<T>([T] value) {
  return(vectorSingle<T>(value))
}

[effects(heap_alloc), return<int>]
main() {
  [i32] a{wrapVector<i32>(4i32).at(0i32)}
  [i32] b{wrapVector<i32>(5i32).at_unsafe(0i32)}
  [i32] c{wrapVector<i32>(6i32).count()}
  [i32] d{wrapVector<i32>(7i32).capacity()}
  return(plus(plus(plus(a, b), c), d))
}
)";
  const std::string srcPath =
      writeTemp("vm_stdlib_collection_shim_templated_return_vector_temp_methods.prime", source);
  const std::string outPath =
      (std::filesystem::temp_directory_path() /
       "primec_vm_stdlib_collection_shim_templated_return_vector_temp_methods.out")
          .string();
  const std::string runCmd =
      "./primec --emit=vm " + srcPath + " --entry /main > " + outPath + " 2>&1";
  CHECK(runCommand(runCmd) == 11);
  CHECK(readFile(outPath).empty());
}

TEST_CASE("runs vm with templated stdlib wrapper temporary index forms") {
  const std::string source = R"(
import /std/collections/*

[return<vector<T>>]
wrapVector<T>([T] value) {
  return(vectorSingle<T>(value))
}

[return<map<K, V>>]
wrapMap<K, V>([K] key, [V] value) {
  [map<K, V>] values{mapSingle<K, V>(key, value)}
  return(values)
}

[effects(heap_alloc), return<int>]
main() {
  [i32] a{wrapVector<i32>(4i32)[0i32]}
  [i32] b{wrapMap<string, i32>("only"raw_utf8, 5i32)["only"raw_utf8]}
  return(plus(a, b))
}
)";
  const std::string srcPath = writeTemp("vm_stdlib_collection_shim_templated_return_temp_index_forms.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 9);
}

TEST_CASE("runs vm with templated stdlib wrapper temporary syntax parity") {
  const std::string source = R"(
import /std/collections/*

[return<vector<T>>]
wrapVector<T>([T] value) {
  return(vectorSingle<T>(value))
}

[return<map<K, V>>]
wrapMap<K, V>([K] key, [V] value) {
  [map<K, V>] values{mapSingle<K, V>(key, value)}
  return(values)
}

[effects(heap_alloc), return<int>]
main() {
  [i32] vectorCall{vectorAt<i32>(wrapVector<i32>(4i32), 0i32)}
  [i32] vectorMethod{wrapVector<i32>(4i32).at(0i32)}
  [i32] vectorIndex{wrapVector<i32>(4i32)[0i32]}
  [i32] mapCall{mapAt<string, i32>(wrapMap<string, i32>("only"raw_utf8, 5i32), "only"raw_utf8)}
  [i32] mapMethod{wrapMap<string, i32>("only"raw_utf8, 5i32).at("only"raw_utf8)}
  [i32] mapIndex{wrapMap<string, i32>("only"raw_utf8, 5i32)["only"raw_utf8]}
  return(plus(plus(plus(vectorCall, vectorMethod), vectorIndex), plus(plus(mapCall, mapMethod), mapIndex)))
}
)";
  const std::string srcPath =
      writeTemp("vm_stdlib_collection_shim_templated_return_temp_syntax_parity.prime", source);
  const std::string outPath =
      (std::filesystem::temp_directory_path() /
       "primec_vm_stdlib_collection_shim_templated_return_temp_syntax_parity.out")
          .string();
  const std::string runCmd =
      "./primec --emit=vm " + srcPath + " --entry /main > " + outPath + " 2>&1";
  CHECK(runCommand(runCmd) == 27);
  CHECK(readFile(outPath).empty());
}

TEST_CASE("runs vm with templated stdlib wrapper temporary unsafe parity") {
  const std::string source = R"(
import /std/collections/*

[return<vector<T>>]
wrapVector<T>([T] value) {
  return(vectorSingle<T>(value))
}

[return<map<K, V>>]
wrapMap<K, V>([K] key, [V] value) {
  [map<K, V>] values{mapSingle<K, V>(key, value)}
  return(values)
}

[effects(heap_alloc), return<int>]
main() {
  [i32] vectorCall{vectorAtUnsafe<i32>(wrapVector<i32>(4i32), 0i32)}
  [i32] vectorMethod{wrapVector<i32>(4i32).at_unsafe(0i32)}
  [i32] mapCall{mapAtUnsafe<string, i32>(wrapMap<string, i32>("only"raw_utf8, 5i32), "only"raw_utf8)}
  [i32] mapMethod{wrapMap<string, i32>("only"raw_utf8, 5i32).at_unsafe("only"raw_utf8)}
  return(plus(plus(vectorCall, vectorMethod), plus(mapCall, mapMethod)))
}
)";
  const std::string srcPath =
      writeTemp("vm_stdlib_collection_shim_templated_return_temp_unsafe_parity.prime", source);
  const std::string outPath =
      (std::filesystem::temp_directory_path() /
       "primec_vm_stdlib_collection_shim_templated_return_temp_unsafe_parity.out")
          .string();
  const std::string runCmd =
      "./primec --emit=vm " + srcPath + " --entry /main > " + outPath + " 2>&1";
  CHECK(runCommand(runCmd) == 18);
  CHECK(readFile(outPath).empty());
}

TEST_CASE("runs vm templated stdlib wrapper temporary count capacity parity") {
  const std::string source = R"(
import /std/collections/*

[return<vector<T>>]
wrapVector<T>([T] value) {
  return(vectorSingle<T>(value))
}

[return<map<K, V>>]
wrapMap<K, V>([K] key, [V] value) {
  [map<K, V>] values{mapSingle<K, V>(key, value)}
  return(values)
}

[effects(heap_alloc), return<int>]
main() {
  [i32] mapCall{mapCount<string, i32>(wrapMap<string, i32>("only"raw_utf8, 5i32))}
  [i32] mapMethod{wrapMap<string, i32>("only"raw_utf8, 5i32).count()}
  [i32] vectorCountCall{vectorCount<i32>(wrapVector<i32>(4i32))}
  [i32] vectorCountMethod{wrapVector<i32>(4i32).count()}
  [i32] vectorCapacityCall{vectorCapacity<i32>(wrapVector<i32>(4i32))}
  [i32] vectorCapacityMethod{wrapVector<i32>(4i32).capacity()}
  return(plus(plus(plus(mapCall, mapMethod), plus(vectorCountCall, vectorCountMethod)),
              plus(vectorCapacityCall, vectorCapacityMethod)))
}
)";
  const std::string srcPath =
      writeTemp("vm_stdlib_collection_shim_templated_return_temp_count_capacity_parity.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > /dev/null";
  CHECK(runCommand(runCmd) == 6);
}
