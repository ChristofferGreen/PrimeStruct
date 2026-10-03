#include "../test_compile_run_helpers.h"

#include "../test_compile_run_collection_conformance_helpers.h"
#include "../test_compile_run_container_error_conformance_helpers.h"
#include "../test_compile_run_checked_pointer_conformance_helpers.h"
#include "../test_compile_run_unchecked_pointer_conformance_helpers.h"
#include "test_compile_run_native_backend_collections_helpers.h"

#if PRIMESTRUCT_NATIVE_COLLECTIONS_ENABLED
TEST_SUITE_BEGIN("primestruct.compile.run.native_backend.collections");


TEST_CASE("native rejects experimental soa stdlib push and reserve helpers") {
  const std::string source = R"(
import /std/collections/soa/*

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
  const std::string srcPath =
      writeTemp("compile_native_experimental_soa_push_helpers.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_native_experimental_soa_push_helpers.err").string();
  const std::string exePath =
      (testScratchPath("") / "native_rejects_experimental_soa_stdlib_exe").string();

  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 11);
}

TEST_CASE("native rejects experimental soa stdlib push and reserve methods") {
  const std::string source = R"(
import /std/collections/soa/*

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
  const std::string srcPath =
      writeTemp("compile_native_experimental_soa_push_method.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_native_experimental_soa_push_method.err").string();
  const std::string exePath =
      (testScratchPath("") / "native_rejects_experimental_soa_stdlib_exe").string();

  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 11);
}

TEST_CASE("native rejects experimental soa single-field index syntax") {
  const std::string source = R"(
import /std/collections/soa/*

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
  const std::string srcPath =
      writeTemp("compile_native_experimental_soa_single_field_view.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_native_experimental_soa_single_field_view.err").string();
  const std::string exePath =
      (testScratchPath("") / "native_rejects_experimental_soa_single_exe").string();

  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 9);
}

TEST_CASE("native rejects experimental soa reflected multi-field index syntax") {
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
  values.push(Particle(7i32, 8i32))
  values.push(Particle(9i32, 12i32))
  return(values.y()[1i32])
}
)";
  const std::string srcPath =
      writeTemp("compile_native_experimental_soa_field_view.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_native_experimental_soa_field_view.err").string();
  const std::string exePath =
      (testScratchPath("") / "native_rejects_experimental_soa_reflec_exe").string();
  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 12);
}

TEST_CASE("native rejects experimental soa mutating indexed field writes") {
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
  const std::string srcPath = writeTemp(
      "compile_native_experimental_soa_mutating_indexed_field_writes.prime",
      source);
  const std::string exePath =
      (testScratchPath("") /
       "primec_native_experimental_soa_mutating_indexed_field_writes.err")
          .string();
  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 52);
}

TEST_CASE("native rejects experimental soa bare get and ref field access") {
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
  [SoaVector<Particle> mut] values{soaVectorNew<Particle>()}
  values.push(Particle(7i32, 8i32))
  values.push(Particle(9i32, 12i32))
  return(plus(ref(values, 0i32).y, get(values, 1i32).y))
}
)";
  const std::string srcPath =
      writeTemp("compile_native_experimental_soa_bare_ref_field_access.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_native_experimental_soa_bare_ref_field_access.err")
          .string();
  const std::string exePath =
      (testScratchPath("") / "native_rejects_experimental_soa_bare_g_exe").string();
  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 20);
}

TEST_CASE("native rejects borrowed helper-return experimental soa reflected index syntax") {
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
  values.push(Particle(4i32, 6i32))
  values.push(Particle(9i32, 12i32))
  return(pickBorrowed(location(values)).y()[1i32])
}
)";
  const std::string srcPath =
      writeTemp("compile_native_experimental_soa_borrowed_return_field_view.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_native_experimental_soa_borrowed_return_field_view.err").string();
  const std::string exePath =
      (testScratchPath("") / "native_rejects_borrowed_helper_return__exe").string();
  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 12);
}

TEST_CASE("native rejects borrowed helper-return experimental soa get/ref methods") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/soa/*

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
      writeTemp("compile_native_experimental_soa_borrowed_return_get_ref.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_native_experimental_soa_borrowed_return_get_ref.err").string();
  const std::string exePath =
      (testScratchPath("") / "native_rejects_borrowed_helper_return__exe").string();
  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 16);
}

TEST_CASE("native compiles and runs borrowed helper-return soa ref_ref same-path helper compatibility") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/soa/*

[struct reflect]
Particle() {
  [i32] x{1i32}
}

[return<Reference<SoaVector<Particle>>>]
pickBorrowed([Reference<SoaVector<Particle>>] values) {
  return(values)
}

[return<int>]
/soa/ref_ref([Reference<SoaVector<Particle>>] values, [int] index) {
  return(19i32)
}

[effects(heap_alloc), return<int>]
main() {
  [SoaVector<Particle> mut] values{soaVectorNew<Particle>()}
  return(plus(pickBorrowed(location(values)).ref(0i32),
              ref_ref(pickBorrowed(location(values)), 0i32)))
}
)";
  const std::string srcPath =
      writeTemp("compile_native_experimental_soa_borrowed_return_ref_ref_same_path.prime",
                source);
  const std::string exePath =
      (testScratchPath("") /
       "primec_native_experimental_soa_borrowed_return_ref_ref_same_path_exe")
          .string();
  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 38);
}

TEST_CASE("native rejects helper-return experimental soa method shadows") {
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

[return<Particle>]
/soa/get([SoaVector<Particle>] values, [i32] index) {
  return(/std/collections/soa/soaVectorGet<Particle>(values, 0i32))
}

[return<Particle>]
/soa/ref([SoaVector<Particle>] values, [i32] index) {
  return(/std/collections/soa/soaVectorGet<Particle>(values, 0i32))
}

[effects(heap_alloc), return<vector<Particle>>]
/to_aos([SoaVector<Particle>] values) {
  [vector<Particle>, mut] out{vector<Particle>()}
  out.push(Particle(5i32))
  return(out)
}

[effects(heap_alloc), return<int>]
main() {
  [Particle] picked{cloneValues().get(1i32)}
  [Particle] pickedRef{cloneValues().ref(1i32)}
  [vector<Particle>] unpacked{cloneValues().to_aos()}
  return(plus(picked.x, plus(pickedRef.x, count(unpacked))))
}
)";
  const std::string srcPath =
      writeTemp("compile_native_experimental_soa_helper_return_shadow_methods.prime",
                source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_native_experimental_soa_helper_return_shadow_methods.err")
          .string();
  const std::string exePath =
      (testScratchPath("") / "native_rejects_helper_return_experimen_exe").string();
  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 3);
}

TEST_CASE("native rejects helper-return soa shadows with explicit canonical fallbacks compatibility") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/soa/*
import /std/collections/soa/*

[struct reflect]
Particle() {
  [i32] x{1i32}
}

[return<SoaVector<Particle>>]
cloneValues() {
  return(soaVectorNew<Particle>())
}

[return<Particle>]
/soa/get([SoaVector<Particle>] values, [i32] index) {
  return(Particle(101i32))
}

[return<Particle>]
/soa/ref([SoaVector<Particle>] values, [i32] index) {
  return(Particle(102i32))
}

[effects(heap_alloc), return<vector<Particle>>]
/to_aos([SoaVector<Particle>] values) {
  [vector<Particle>, mut] out{vector<Particle>()}
  out.push(Particle(5i32))
  return(out)
}

[return<i32>]
/soa/push([SoaVector<Particle>] values, [Particle] value) {
  return(31i32)
}

[return<i32>]
/soa/reserve([SoaVector<Particle>] values, [i32] capacity) {
  return(37i32)
}

[effects(heap_alloc), return<i32>]
canonicalProbe([SoaVector<Particle> mut] values) {
  /std/collections/soa/soaVectorReserve<Particle>(values, 2i32)
  /std/collections/soa/soaVectorPush<Particle>(values, Particle(7i32))
  [Particle] first{/std/collections/soa/soaVectorGet<Particle>(values, 0i32)}
  [i32] firstRef{/std/collections/soa/soaVectorRef<Particle>(values, 0i32).x}
  [vector<Particle>] unpacked{/std/collections/soa/soaVectorToAos<Particle>(values)}
  return(plus(first.x, plus(firstRef, count(unpacked))))
}

[effects(heap_alloc), return<int>]
main() {
  [Particle] shadowGet{/soa/get(cloneValues(), 0i32)}
  [Particle] shadowRef{/soa/ref(cloneValues(), 0i32)}
  [vector<Particle>] shadowAos{/to_aos(cloneValues())}
  [i32] shadowPush{/soa/push(cloneValues(), Particle(11i32))}
  [i32] shadowReserve{/soa/reserve(cloneValues(), 4i32)}
  return(plus(shadowGet.x,
              plus(shadowRef.x,
                   plus(count(shadowAos),
                        plus(shadowPush, shadowReserve)))))
}
)";
  const std::string srcPath =
      writeTemp("compile_native_experimental_soa_helper_return_shadow_canonical_fallbacks.prime",
                source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_native_experimental_soa_helper_return_shadow_canonical_fallbacks.err")
          .string();
  const std::string exePath =
      (testScratchPath("") / "native_rejects_helper_return_soa_shado_exe").string();
  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 3);
}

TEST_CASE("native rejects borrowed local experimental soa read-only methods") {
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
      writeTemp("compile_native_experimental_soa_borrowed_local_methods.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_native_experimental_soa_borrowed_local_methods.err")
          .string();
  const std::string exePath =
      (testScratchPath("") / "native_rejects_borrowed_local_experime_exe").string();
  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 38);
}

TEST_CASE("native rejects inline location experimental soa read-only methods") {
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
      writeTemp("compile_native_experimental_soa_inline_location_methods.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_native_experimental_soa_inline_location_methods.err")
          .string();
  const std::string exePath =
      (testScratchPath("") / "native_rejects_inline_location_experim_exe").string();
  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 40);
}

TEST_CASE("native rejects method-like borrowed helper-return experimental soa helper surfaces") {
  const std::string source = R"(
import /std/collections/*
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
  values.push(Particle(7i32, 8i32))
  values.push(Particle(9i32, 12i32))
  [Holder] holder{Holder{}}
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
      writeTemp("compile_native_experimental_soa_method_like_borrowed_return_helpers.prime", source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_native_experimental_soa_method_like_borrowed_return_helpers.err")
          .string();
  const std::string exePath =
      (testScratchPath("") / "native_rejects_method_like_borrowed_he_exe").string();
  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 85);
}

TEST_CASE("native rejects direct return borrowed helper-return experimental soa reads") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/soa/*
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
      "compile_native_experimental_soa_direct_return_borrowed_return_reads.prime",
      source);
  const std::string exePath =
      (testScratchPath("") /
       "primec_native_experimental_soa_direct_return_borrowed_return_reads.err")
          .string();
  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 55);
}

TEST_CASE("native rejects direct return method-like borrowed helper-return experimental soa reads") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/soa/*
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
  values.push(Particle(7i32, 8i32))
  values.push(Particle(9i32, 12i32))
  [Holder] holder{Holder{}}
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
      "compile_native_experimental_soa_direct_return_method_like_borrowed_return_reads.prime",
      source);
  const std::string exePath =
      (testScratchPath("") /
       "primec_native_experimental_soa_direct_return_method_like_borrowed_return_reads.err")
          .string();
  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 55);
}

TEST_CASE("native experimental soa storage helpers") {
  const std::string source = R"(
import /std/collections/soa_storage/*

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
  const std::string srcPath =
      writeTemp("compile_native_soa_storage.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_soa_storage_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 14);
}

TEST_CASE("native experimental soa storage borrowed ref helper") {
  const std::string source = R"(
import /std/collections/soa_storage/*

[effects(heap_alloc), return<int>]
main() {
  [SoaColumn<i32> mut] values{soaColumnNew<i32>()}
  soaColumnPush<i32>(values, 2i32)
  soaColumnPush<i32>(values, 5i32)
  [Reference<i32>] borrowed{soaColumnRef<i32>(values, 1i32)}
  return(plus(dereference(borrowed), soaColumnCount<i32>(values)))
}
)";
  const std::string srcPath =
      writeTemp("compile_native_soa_storage_ref.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_soa_storage_ref_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 7);
}

TEST_CASE("native experimental soa storage borrowed view helper") {
  const std::string source = R"(
import /std/collections/soa_storage/*

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
  const std::string srcPath =
      writeTemp("compile_native_soa_storage_view.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_soa_storage_view_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 9);
}

TEST_CASE("rejects native experimental soa storage reserve overflow") {
  const std::string source = R"(
import /std/collections/soa_storage/*

[effects(heap_alloc), return<int>]
main() {
  [SoaColumn<i32> mut] values{soaColumnNew<i32>()}
  soaColumnReserve<i32>(values, 1073741824i32)
  return(0i32)
}
)";
  const std::string srcPath =
      writeTemp("compile_native_soa_storage_reserve_overflow.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_soa_storage_reserve_overflow_exe").string();
  const std::string errPath =
      (testScratchPath("") / "primec_native_soa_storage_reserve_overflow_err.txt").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string runCmd = exePath + " 2> " + errPath;
  CHECK(runCommand(runCmd) == 3);
  CHECK(readFile(errPath) == "array index out of bounds\n");
}

TEST_CASE("native experimental two-column soa storage helpers") {
  const std::string source = R"(
import /std/collections/soa_storage/*

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
  const std::string srcPath =
      writeTemp("compile_native_soa_storage_two_columns.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_soa_storage_two_columns_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 24);
}

TEST_CASE("native experimental sixteen-column soa storage helpers") {
  const std::string source = R"(
import /std/collections/soa_storage/*

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
  const std::string srcPath =
      writeTemp("compile_native_soa_storage_sixteen_columns.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_soa_storage_sixteen_columns_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 143);
}

TEST_CASE("rejects native templated stdlib wrapper temporary call forms") {
  const std::string source = R"(
import /std/collections/*

[return<auto>]
wrapMap<K, V>([K] key, [V] value) {
  [/std/collections/map<K, V>] values{map<K, V>(key, value)}
  return(values)
}

[return<int>]
main() {
  [i32] a{/std/collections/map/at<string, i32>(wrapMap<string, i32>("only"raw_utf8, 4i32), "only"raw_utf8)}
  [i32] b{/std/collections/map/at_unsafe<string, i32>(wrapMap<string, i32>("only"raw_utf8, 4i32), "only"raw_utf8)}
  [i32] c{/std/collections/map/count<string, i32>(wrapMap<string, i32>("only"raw_utf8, 4i32))}
  return(plus(plus(a, b), c))
}
)";
  const std::string srcPath =
      writeTemp("compile_native_stdlib_collection_shim_templated_return_temp_call_forms.prime", source);
  const std::string errPath = (testScratchPath("") /
                               "primec_native_stdlib_collection_shim_templated_return_temp_call_forms.err")
                                  .string();

  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
}

TEST_CASE("rejects native canonical namespaced map helpers on experimental map values") {
  expectCanonicalMapNamespaceExperimentalValueConformance("native");
}

TEST_CASE("rejects native wrapper map helpers on experimental map values") {
  expectWrapperMapHelperExperimentalValueConformance("native");
}

TEST_CASE("rejects native ownership-sensitive experimental map value methods") {
  expectExperimentalMapOwnershipMethodConformance("native");
}

TEST_CASE("native helper-wrapped inferred experimental map returns") {
  expectWrappedInferredExperimentalMapReturnConformance("native");
}

TEST_CASE("native helper-wrapped experimental map parameters") {
  expectWrappedExperimentalMapParameterConformance("native");
}

TEST_CASE("native helper-wrapped experimental map bindings") {
  expectWrappedExperimentalMapBindingConformance("native");
}

TEST_CASE("native helper-wrapped experimental map assignment RHS values") {
  expectWrappedExperimentalMapAssignConformance("native");
}

TEST_CASE("rejects native canonical namespaced map constructors on explicit experimental map bindings") {
  expectCanonicalMapNamespaceExperimentalConstructorConformance("native");
}

TEST_CASE("rejects native canonical namespaced map constructors through explicit experimental map returns") {
  expectCanonicalMapNamespaceExperimentalReturnConformance("native");
}

TEST_CASE("rejects native canonical namespaced map constructors through explicit experimental map parameters") {
  expectCanonicalMapNamespaceExperimentalParameterConformance("native");
}

TEST_CASE("native wrapper map constructors on explicit experimental map bindings") {
  expectWrapperMapConstructorExperimentalBindingConformance("native");
}

TEST_CASE("native wrapper map constructors through explicit experimental map returns") {
  expectWrapperMapConstructorExperimentalReturnConformance("native");
}

TEST_CASE("native wrapper map constructors through explicit experimental map parameters") {
  expectWrapperMapConstructorExperimentalParameterConformance("native");
}

TEST_CASE("rejects native experimental map variadic constructors") {
  expectExperimentalMapVariadicConstructorConformance("native");
}

TEST_CASE("rejects native experimental map variadic constructor type mismatch") {
  expectExperimentalMapVariadicConstructorMismatchReject("native");
}

TEST_CASE("native experimental map constructor assignments") {
  expectExperimentalMapAssignConformance("native");
}

TEST_CASE("native implicit map auto constructor inference") {
  expectImplicitMapAutoInferenceConformance("native");
}

TEST_CASE("rejects native inferred experimental map returns") {
  expectInferredExperimentalMapReturnConformance("native");
}

TEST_CASE("native block inferred experimental map returns") {
  expectBlockInferredExperimentalMapReturnConformance("native");
}

TEST_CASE("native auto block inferred experimental map returns") {
  expectAutoBlockInferredExperimentalMapReturnConformance("native");
}

TEST_CASE("rejects native inferred experimental map call receivers") {
  expectInferredExperimentalMapCallReceiverConformance("native");
}

TEST_CASE("rejects native experimental map struct fields") {
  expectExperimentalMapStructFieldConformance("native");
}

TEST_CASE("rejects native inferred experimental map struct field constructor expressions") {
  expectInferredExperimentalMapStructFieldConformance("native");
}

TEST_CASE("rejects native helper-wrapped inferred experimental map struct field constructor expressions") {
  expectWrappedInferredExperimentalMapStructFieldConformance("native");
}

TEST_CASE("rejects native experimental map method parameter constructor expressions") {
  expectExperimentalMapMethodParameterConformance("native");
}

TEST_CASE("rejects native inferred experimental map parameter call expressions") {
  expectInferredExperimentalMapParameterConformance("native");
}

TEST_CASE("rejects native inferred experimental map default parameter call expressions") {
  expectInferredExperimentalMapDefaultParameterConformance("native");
}

TEST_CASE("rejects native helper-wrapped inferred experimental map default parameter call expressions") {
  expectWrappedInferredExperimentalMapDefaultParameterConformance("native");
}

TEST_CASE("native experimental map helper receivers") {
  expectExperimentalMapHelperReceiverConformance("native");
}

TEST_CASE("native helper-wrapped experimental map helper receivers") {
  expectWrappedExperimentalMapHelperReceiverConformance("native");
}

TEST_CASE("rejects native experimental map method receivers") {
  expectExperimentalMapMethodReceiverConformance("native");
}

TEST_CASE("rejects native helper-wrapped experimental map method receivers") {
  expectWrappedExperimentalMapMethodReceiverConformance("native");
}

TEST_CASE("native experimental map field assignments") {
  expectExperimentalMapFieldAssignConformance("native");
}

TEST_CASE("native dereferenced experimental map storage references") {
  expectExperimentalMapStorageReferenceConformance("native");
}

TEST_CASE("native helper-wrapped Result.ok experimental map result struct fields") {
  expectWrappedExperimentalMapResultFieldAssignConformance("native");
}

TEST_CASE("native helper-wrapped dereferenced Result.ok experimental map result struct fields") {
  expectWrappedExperimentalMapResultDerefFieldAssignConformance("native");
}

TEST_CASE("native helper-wrapped experimental map struct storage fields") {
  expectWrappedExperimentalMapStorageFieldConformance("native");
}

TEST_CASE("native helper-wrapped dereferenced experimental map struct storage fields") {
  expectWrappedExperimentalMapStorageDerefFieldConformance("native");
}

TEST_CASE("rejects native canonical namespaced map helpers on borrowed experimental map values") {
  expectCanonicalMapNamespaceExperimentalReferenceConformance("native");
}

TEST_CASE("rejects native canonical namespaced map _ref helpers on borrowed experimental map values") {
  expectCanonicalMapNamespaceExperimentalBorrowedRefConformance("native");
}

TEST_CASE("rejects native experimental map methods") {
  expectExperimentalMapMethodConformance("native");
}

TEST_CASE("native borrowed experimental map helpers") {
  expectExperimentalMapReferenceHelperConformance("native");
}

TEST_CASE("native public borrowed map wrappers") {
  expectPublicMapReferenceWrapperConformance("native");
}

TEST_CASE("rejects native borrowed experimental map methods") {
  expectExperimentalMapReferenceMethodConformance("native");
}

TEST_CASE("native experimental map inserts") {
  expectExperimentalMapInsertConformance("native");
}

TEST_CASE("rejects native experimental map ownership-sensitive values") {
  expectExperimentalMapOwnershipConformance("native");
}

TEST_CASE("rejects native canonical namespaced map inserts on explicit experimental map bindings") {
  expectCanonicalMapNamespaceExperimentalInsertConformance("native");
}

TEST_CASE("rejects native builtin canonical map first-growth inserts") {
  expectBuiltinCanonicalMapInsertFirstGrowthConformance("native");
}

TEST_CASE("rejects native builtin canonical map repeated-growth inserts") {
  expectBuiltinCanonicalMapInsertRepeatedGrowthConformance("native");
}

TEST_CASE("native builtin canonical map insert overwrites") {
  expectBuiltinCanonicalMapInsertOverwriteConformance("native");
}

TEST_CASE("native builtin canonical map non-local growth") {
  expectBuiltinCanonicalMapInsertNonLocalGrowthConformance("native");
}

TEST_CASE("native builtin canonical map nested non-local growth") {
  expectBuiltinCanonicalMapInsertNestedNonLocalGrowthConformance("native");
}

TEST_CASE("native builtin canonical map helper-return borrowed method inserts") {
  expectBuiltinCanonicalMapInsertHelperReturnBorrowedMethodConformance("native");
}

TEST_CASE("native builtin canonical map struct-field initializer") {
  expectBuiltinCanonicalMapStructFieldInitializerConformance("native");
}

TEST_CASE("native builtin canonical map direct insert on helper-return value receivers") {
  expectBuiltinCanonicalMapInsertHelperReturnValueDirectConformance("native");
}

TEST_CASE("native builtin canonical map method insert on helper-return value receivers") {
  expectBuiltinCanonicalMapInsertHelperReturnValueMethodConformance("native");
}

TEST_CASE("native builtin canonical map direct insert on borrowed holder field receivers") {
  expectBuiltinCanonicalMapInsertBorrowedHolderFieldDirectConformance("native");
}

TEST_CASE("rejects native canonical map constructor ownership growth") {
  expectCanonicalMapNamespaceOwnershipReject("native");
}

TEST_CASE("native experimental map bracket access") {
  expectExperimentalMapIndexConformance("native");
}

TEST_CASE("rejects native canonical map custom comparable struct keys") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/map/*

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
  [map<Key, i32>] values{/std/collections/map/map<Key, i32>(Key{2i32}, 7i32, Key{5i32}, 11i32)}
  [i32 mut] total{/std/collections/map/count<Key, i32>(values)}
  assign(total, plus(total, /std/collections/map/at<Key, i32>(values, Key{2i32})))
  assign(total, plus(total, /std/collections/map/at_unsafe<Key, i32>(values, Key{5i32})))
  if(/std/collections/map/contains<Key, i32>(values, Key{2i32}),
     then() { assign(total, plus(total, 1i32)) },
     else() { })
  return(total)
}
)";
  expectMapConformanceCompileReject(source,
                                    "compile_native_experimental_map_custom_comparable_key",
                                    "native",
                                    "");
}

TEST_CASE("covers native shared vector harness contracts") {
  expectSharedVectorConformanceHarness("native");
}

TEST_CASE("rejects native canonical namespaced vector helpers") {
  expectCanonicalVectorNamespaceConformance("native");
}

TEST_CASE("native canonical namespaced vector helpers on explicit Vector bindings") {
  expectCanonicalVectorNamespaceExplicitVectorBindingConformance("native");
}

TEST_CASE("native stdlib wrapper vector helpers on explicit Vector bindings") {
  expectStdlibWrapperVectorHelperExplicitVectorBindingConformance("native");
}

TEST_CASE("rejects native stdlib wrapper vector helper explicit Vector mismatch") {
  expectStdlibWrapperVectorHelperExplicitVectorBindingMismatchReject("native");
}

TEST_CASE("native stdlib wrapper vector constructors on explicit Vector bindings") {
  expectStdlibWrapperVectorConstructorExplicitVectorBindingConformance("native");
}

TEST_CASE("keeps native stdlib wrapper vector constructor explicit Vector mismatch contract") {
  expectStdlibWrapperVectorConstructorExplicitVectorBindingMismatchContract("native");
}

TEST_CASE("native stdlib wrapper vector constructors on inferred auto bindings") {
  expectStdlibWrapperVectorConstructorAutoInferenceConformance("native");
}

TEST_CASE("rejects native stdlib wrapper vector constructor auto inference mismatch") {
  expectStdlibWrapperVectorConstructorAutoInferenceMismatchReject("native");
}

TEST_CASE("rejects native stdlib wrapper vector constructor receivers") {
  expectStdlibWrapperVectorConstructorReceiverConformance("native");
}

TEST_CASE("rejects native stdlib wrapper vector helper receiver mismatch") {
  expectStdlibWrapperVectorConstructorHelperReceiverMismatchReject("native");
}

TEST_CASE("rejects native stdlib wrapper vector method receiver mismatch") {
  expectStdlibWrapperVectorConstructorMethodReceiverMismatchReject("native");
}

TEST_CASE("rejects native canonical namespaced vector constructor temporaries") {
  expectCanonicalVectorNamespaceTemporaryReceiverConformance("native");
}

TEST_CASE("native canonical namespaced vector explicit builtin bindings") {
  expectCanonicalVectorNamespaceExplicitBindingConformance("native");
}

TEST_CASE("rejects native canonical namespaced vector named-argument temporaries") {
  expectCanonicalVectorNamespaceNamedArgsTemporaryReceiverConformance("native");
}

TEST_CASE("native canonical namespaced vector named-argument explicit builtin bindings") {
  expectCanonicalVectorNamespaceNamedArgsExplicitBindingConformance("native");
}

TEST_CASE("rejects native canonical namespaced vector mutators without imported helpers") {
  expectCanonicalVectorClearImportRequirement("native");
  expectCanonicalVectorRemoveAtImportRequirement("native");
  expectCanonicalVectorRemoveSwapImportRequirement("native");
}

TEST_CASE("native experimental vector helper runtime contracts") {
  expectExperimentalVectorRuntimeContracts("native");
}

TEST_CASE("native experimental vector ownership-sensitive helpers") {
  expectExperimentalVectorOwnershipContracts("native");
}

TEST_CASE("native canonical vector helpers on experimental vector receivers") {
  expectExperimentalVectorCanonicalHelperRoutingConformance("native");
}
TEST_CASE("native vector pop empty runtime contract") {
  SUBCASE("call") {
    expectVectorPopEmptyRuntimeContract("native", false);
  }

  SUBCASE("method") {
    expectVectorPopEmptyRuntimeContract("native", true);
  }
}

TEST_CASE("native vector index runtime contract") {
  expectVectorIndexRuntimeContract("native", "access_call");
  expectVectorIndexRuntimeContract("native", "access_method");
  expectVectorIndexRuntimeContract("native", "access_bracket");
  expectVectorIndexRuntimeContract("native", "remove_at_call");
  expectVectorIndexRuntimeContract("native", "remove_at_method");
  expectVectorIndexRuntimeContract("native", "remove_swap_call");
  expectVectorIndexRuntimeContract("native", "remove_swap_method");
}

TEST_CASE("native imported container error contract conformance") {
  expectContainerErrorConformance("native");
}

TEST_CASE("native checked pointer conformance harness for imported .prime helpers") {
  expectCheckedPointerHelperSurfaceConformance("native");
  expectCheckedPointerGrowthConformance("native");
  expectCheckedPointerUninitializedPrefixMoveConformance("native");
  expectCheckedPointerOutOfBoundsConformance("native");
  expectCheckedPointerUninitializedOutOfBoundsConformance("native");
}

TEST_CASE("native templated stdlib vector wrapper temporary call forms") {
  const std::string source = R"(
import /std/collections/*

[return<vector<T>>]
wrapVector<T>([T] value) {
  return(/std/collections/vector/vector<T>(value))
}

[effects(heap_alloc), return<int>]
main() {
  [i32] a{/std/collections/vector/at<i32>(wrapVector<i32>(4i32), 0i32)}
  [i32] b{/std/collections/vector/at_unsafe<i32>(wrapVector<i32>(5i32), 0i32)}
  [i32] c{/std/collections/vector/count<i32>(wrapVector<i32>(6i32))}
  [i32] d{/std/collections/vector/capacity<i32>(wrapVector<i32>(7i32))}
  return(plus(plus(plus(a, b), c), d))
}
)";
  const std::string srcPath =
      writeTemp("compile_native_stdlib_collection_shim_templated_return_vector_temp_call_forms.prime", source);
  const std::string exePath = (testScratchPath("") /
                               "primec_native_stdlib_collection_shim_templated_return_vector_temp_call_forms_exe")
                                  .string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 11);
}

TEST_CASE("native templated stdlib vector wrapper temporary methods in expressions") {
  const std::string source = R"(
import /std/collections/*

[return<vector<T>>]
wrapVector<T>([T] value) {
  return(/std/collections/vector/vector<T>(value))
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
      writeTemp("compile_native_stdlib_collection_shim_templated_return_vector_temp_methods.prime", source);
  const std::string exePath = (testScratchPath("") /
                               "primec_native_stdlib_collection_shim_templated_return_vector_temp_methods_exe")
                                  .string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 11);
}

TEST_CASE("rejects native templated stdlib wrapper temporary index forms") {
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
  [i32] a{wrapVector<i32>(4i32)[0i32]}
  [i32] b{wrapMap<string, i32>("only"raw_utf8, 5i32)["only"raw_utf8]}
  return(plus(a, b))
}
)";
  const std::string srcPath =
      writeTemp("compile_native_stdlib_collection_shim_templated_return_temp_index_forms.prime", source);
  const std::string errPath = (testScratchPath("") /
                               "primec_native_stdlib_collection_shim_templated_return_temp_index_forms.err")
                                  .string();

  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
}

TEST_CASE("rejects native templated stdlib wrapper temporary syntax parity") {
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
  [i32] vectorCall{/std/collections/vector/at<i32>(wrapVector<i32>(4i32), 0i32)}
  [i32] vectorMethod{wrapVector<i32>(4i32).at(0i32)}
  [i32] vectorIndex{wrapVector<i32>(4i32)[0i32]}
  [i32] mapCall{/std/collections/map/at<string, i32>(wrapMap<string, i32>("only"raw_utf8, 5i32), "only"raw_utf8)}
  [i32] mapMethod{wrapMap<string, i32>("only"raw_utf8, 5i32).at("only"raw_utf8)}
  [i32] mapIndex{wrapMap<string, i32>("only"raw_utf8, 5i32)["only"raw_utf8]}
  return(plus(plus(plus(vectorCall, vectorMethod), vectorIndex), plus(plus(mapCall, mapMethod), mapIndex)))
}
)";
  const std::string srcPath =
      writeTemp("compile_native_stdlib_collection_shim_templated_return_temp_syntax_parity.prime", source);
  const std::string errPath = (testScratchPath("") /
                               "primec_native_stdlib_collection_shim_templated_return_temp_syntax_parity.err")
                                  .string();

  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
}

TEST_CASE("rejects native templated stdlib wrapper temporary unsafe parity") {
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
  [i32] vectorCall{/std/collections/vector/at_unsafe<i32>(wrapVector<i32>(4i32), 0i32)}
  [i32] vectorMethod{wrapVector<i32>(4i32).at_unsafe(0i32)}
  [i32] mapCall{/std/collections/map/at_unsafe<string, i32>(wrapMap<string, i32>("only"raw_utf8, 5i32), "only"raw_utf8)}
  [i32] mapMethod{wrapMap<string, i32>("only"raw_utf8, 5i32).at_unsafe("only"raw_utf8)}
  return(plus(plus(vectorCall, vectorMethod), plus(mapCall, mapMethod)))
}
)";
  const std::string srcPath =
      writeTemp("compile_native_stdlib_collection_shim_templated_return_temp_unsafe_parity.prime", source);
  const std::string errPath = (testScratchPath("") /
                               "primec_native_stdlib_collection_shim_templated_return_temp_unsafe_parity.err")
                                  .string();

  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
}

TEST_CASE("rejects native templated stdlib wrapper temporary count capacity parity") {
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
  [i32] mapCall{/std/collections/map/count<string, i32>(wrapMap<string, i32>("only"raw_utf8, 5i32))}
  [i32] mapMethod{wrapMap<string, i32>("only"raw_utf8, 5i32).count()}
  [i32] vectorCountCall{/std/collections/vector/count<i32>(wrapVector<i32>(4i32))}
  [i32] vectorCountMethod{wrapVector<i32>(4i32).count()}
  [i32] vectorCapacityCall{/std/collections/vector/capacity<i32>(wrapVector<i32>(4i32))}
  [i32] vectorCapacityMethod{wrapVector<i32>(4i32).capacity()}
  return(plus(plus(plus(mapCall, mapMethod), plus(vectorCountCall, vectorCountMethod)),
              plus(vectorCapacityCall, vectorCapacityMethod)))
}
)";
  const std::string srcPath =
      writeTemp("compile_native_stdlib_collection_shim_templated_return_temp_count_capacity_parity.prime", source);
  const std::string errPath = (testScratchPath("") /
                               "primec_native_stdlib_collection_shim_templated_return_temp_count_capacity_parity.err")
                                  .string();
  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
}

TEST_SUITE_END();
#endif
