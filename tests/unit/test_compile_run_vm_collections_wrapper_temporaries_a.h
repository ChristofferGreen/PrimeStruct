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

TEST_CASE("vm canonical soa_vector get helper keeps canonical reject on non-soa receiver") {
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
  CHECK(readFile(errPath).find("unknown call target: /std/collections/soa_vector/get") !=
        std::string::npos);
}

TEST_CASE("vm canonical soa_vector get slash-method keeps canonical reject") {
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
  CHECK(readFile(errPath).find("unknown call target: /std/collections/soa_vector/get") !=
        std::string::npos);
}

TEST_CASE("vm canonical soa_vector to_aos slash-method keeps canonical reject") {
  const std::string source = R"(
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
  CHECK(readFile(errPath).find("unknown method: ") != std::string::npos);
  CHECK(readFile(errPath).find("/std/collections/soa_vector/to_aos") != std::string::npos);
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
  [Particle] second{ref(values, 1i32)}
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

TEST_CASE("vm rejects experimental soa_vector stdlib wide structs on pending width boundary") {
  const std::string source = R"(
import /std/collections/experimental_soa_vector/*

[struct reflect]
Particle16() {
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
}

[effects(heap_alloc), return<int>]
main() {
  [SoaVector<Particle16>] values{soaVectorNew<Particle16>()}
  return(soaVectorCount<Particle16>(values))
}
)";
  const std::string srcPath = writeTemp("vm_experimental_soa_vector_wide_pending.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_vm_experimental_soa_vector_wide_pending_err.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main 2> " + errPath;
  CHECK(runCommand(runCmd) == 3);
  CHECK(readFile(errPath) == "experimental soa storage arbitrary-width schemas pending\n");
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

TEST_CASE("vm root soa_vector to_aos helper forms still reject") {
  const std::string source = R"(
import /std/collections/*

[struct reflect]
Particle() {
  [i32] x{1i32}
}

[effects(heap_alloc), return<int>]
main() {
  [soa_vector<Particle>] values{soa_vector<Particle>()}
  [vector<Particle>] unpackedA{to_aos(values)}
  [vector<Particle>] unpackedB{/to_aos(values)}
  [vector<Particle>] unpackedC{values.to_aos()}
  [vector<Particle>] unpackedD{values./to_aos()}
  return(plus(count(unpackedA),
              plus(count(unpackedB),
                   plus(count(unpackedC), count(unpackedD)))))
}
)";
  const std::string srcPath = writeTemp("vm_root_soa_vector_to_aos_forms.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_vm_root_soa_vector_to_aos_forms_err.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main 2> " + errPath;
  CHECK(runCommand(runCmd) == 2);
  CHECK(readFile(errPath).find("vm backend only supports arithmetic/comparison") !=
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
  [Particle] value{soaVectorRef<Particle>(values, 0i32)}
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
  [Particle] value{values.ref(0i32)}
  return(value.x)
}
)";
  const std::string srcPath = writeTemp("vm_experimental_soa_vector_ref_method.prime", source);
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

TEST_CASE("vm rejects experimental soa_vector stdlib field-view index syntax with pending diagnostic") {
  const std::string source = R"(
import /std/collections/experimental_soa_vector/*

[struct reflect]
Particle() {
  [i32] x{1i32}
}

[effects(heap_alloc), return<int>]
main() {
  [SoaVector<Particle>] values{soaVectorSingle<Particle>(Particle(7i32))}
  return(values.x()[0i32])
}
)";
  const std::string srcPath = writeTemp("vm_experimental_soa_vector_field_view.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_vm_experimental_soa_vector_field_view_err.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main 2> " + errPath;
  CHECK(runCommand(runCmd) == 2);
  CHECK(readFile(errPath).find("soa_vector field views are not implemented yet: x") != std::string::npos);
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
  [i32] borrowed{soaColumnRef<i32>(values, 1i32)}
  return(plus(borrowed, soaColumnCount<i32>(values)))
}
)";
  const std::string srcPath = writeTemp("vm_experimental_soa_storage_ref.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 7);
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
