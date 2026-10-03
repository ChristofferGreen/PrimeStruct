#include "../test_compile_run_helpers.h"

#include "../test_compile_run_collection_conformance_helpers.h"
#include "../test_compile_run_container_error_conformance_helpers.h"
#include "../test_compile_run_checked_pointer_conformance_helpers.h"
#include "../test_compile_run_unchecked_pointer_conformance_helpers.h"

TEST_SUITE_BEGIN("primestruct.compile.run.imports");


TEST_CASE("rejects experimental soa reflected call-form index syntax in C++ emitter") {
  const std::string source = R"(
import /std/collections/soa/*
import /std/collections/internal_soa/*

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
      writeTemp("compile_experimental_soa_call_form_field_view_exe.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_experimental_soa_call_form_field_view_exe.err")
          .string();
  const std::string compileCmd =
      "./primec --emit=vm " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find(
            "direct import of retired soa compatibility modules is not supported") !=
        std::string::npos);
}

TEST_CASE("rejects experimental soa inline location borrow index syntax in C++ emitter") {
  const std::string source = R"(
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
      writeTemp("compile_experimental_soa_inline_location_field_view_exe.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_experimental_soa_inline_location_field_view_exe.err")
          .string();
  const std::string compileCmd =
      "./primec --emit=vm " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find(
            "direct import of retired soa compatibility modules is not supported") !=
        std::string::npos);
}

TEST_CASE("rejects dereferenced borrowed helper-return experimental soa reflected index syntax in C++ emitter") {
  const std::string source = R"(
import /std/collections/soa/*
import /std/collections/internal_soa/*

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
      writeTemp("compile_experimental_soa_dereferenced_borrowed_return_field_view_exe.prime", source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_experimental_soa_dereferenced_borrowed_return_field_view_exe.err")
          .string();
  const std::string compileCmd =
      "./primec --emit=vm " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find(
            "direct import of retired soa compatibility modules is not supported") !=
        std::string::npos);
}

TEST_CASE("rejects borrowed helper-return experimental soa get/ref methods in C++ emitter") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/soa/*
import /std/collections/internal_soa/*

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
      writeTemp("compile_experimental_soa_borrowed_return_get_ref_exe.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_experimental_soa_borrowed_return_get_ref_exe.err")
          .string();
  const std::string compileCmd =
      "./primec --emit=vm " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("direct import of retired soa compatibility modules is not supported") !=
        std::string::npos);
}

TEST_CASE("borrowed helper-return soa ref_ref same-path helper in C++ emitter compatibility") {
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
/soa/ref_ref<T>([Reference<SoaVector<T>>] values, [int] index) {
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
      writeTemp("compile_experimental_soa_borrowed_return_ref_ref_same_path_exe.prime",
                source);
  const std::string errPath =
      (testScratchPath("") / "primec_experimental_soa_borrowed_return_ref_ref_same_path_exe.err")
          .string();
  const std::string compileCmd =
      "./primec --emit=vm " + srcPath + " --entry /main 2> " + errPath;
  // TODO-5050 shape (a) (RESOLVED), shape (b) still open for TEMPLATED
  // same-path shadows specifically: .ref(...)/ref_ref(...) on this
  // borrowed helper-return receiver now correctly resolve to the real
  // canonical /std/collections/soa/ref_ref<T> stdlib helper rather than
  // the templated user shadow at the same short path - unlike the
  // non-templated same-path-shadow case (see
  // test_semantics_type_resolution_graph_snapshots.cpp's "keeps borrowed
  // soa ref_ref targets on same-path helpers" test), a templated
  // same-path shadow does not take priority. Since `values` is empty
  // here, the real helper's array indexing now correctly fails at
  // runtime instead of silently returning the shadow's fixed 19i32.
  CHECK(runCommand(compileCmd) == 3);
  CHECK(readFile(errPath).find("array index out of bounds") != std::string::npos);
}

TEST_CASE("runs builtin helper-return soa ref_ref same-path helper in C++ emitter") {
  const std::string source = R"(
import /std/collections/*

[struct reflect]
Particle() {
  [i32] x{1i32}
}

[effects(heap_alloc), return<soa<Particle>>]
cloneValues() {
  return(soa<Particle>())
}

[effects(heap_alloc), return<int>]
/soa/ref_ref([soa<Particle>] values, [vector<i32>] index) {
  return(17i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] idx{vector<i32>(0i32)}
  [soa<Particle>] values{cloneValues()}
  return(plus(ref_ref(values, idx),
              plus(values.ref_ref(idx), ref_ref(cloneValues(), idx))))
}
)";
  const std::string srcPath =
      writeTemp("compile_builtin_soa_ref_ref_same_path_exe.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_builtin_soa_ref_ref_same_path_exe").string();
  const std::string errPath =
      (testScratchPath("") / "primec_builtin_soa_ref_ref_same_path.err")
          .string();
  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath +
                                 " --entry /main 2> " + errPath;
  // TODO-5295 (RESOLVED): the same-path /soa/ref_ref shadow over a public
  // soa<T> receiver now wins for bare, method-sugar, and helper-return
  // call forms, so the C++ emitter compiles it and each call returns 17.
  CHECK(runCommand(compileCmd) == 0);
  CHECK(readFile(errPath).empty());
  CHECK(runCommand(exePath) == 51);
}

TEST_CASE("rejects helper-return experimental soa method shadows in C++ emitter") {
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
  return(soaVectorSingle<Particle>(Particle(7i32)))
}

[return<Particle>]
/soa/get([SoaVector<Particle>] values, [i32] index) {
  return(/std/collections/experimental_soa/soaVectorGet<Particle>(values, 0i32))
}

[return<Particle>]
/soa/ref([SoaVector<Particle>] values, [i32] index) {
  return(/std/collections/experimental_soa/soaVectorGet<Particle>(values, 0i32))
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
      writeTemp("compile_experimental_soa_helper_return_shadow_methods_exe.prime",
                source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_experimental_soa_helper_return_shadow_methods_exe.err")
          .string();
  const std::string compileCmd =
      "./primec --emit=vm " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("direct import of retired soa compatibility modules is not supported") !=
        std::string::npos);
}

TEST_CASE("rejects helper-return soa shadows with explicit canonical fallbacks in C++ emitter compatibility") {
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
  /std/collections/experimental_soa/soaVectorReserve<Particle>(values, 2i32)
  /std/collections/experimental_soa/soaVectorPush<Particle>(values, Particle(7i32))
  [Particle] first{/std/collections/experimental_soa/soaVectorGet<Particle>(values, 0i32)}
  [i32] firstRef{/std/collections/experimental_soa/soaVectorRef<Particle>(values, 0i32).x}
  [vector<Particle>] unpacked{/std/collections/experimental_soa_conversions/soaVectorToAos<Particle>(values)}
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
      writeTemp("compile_experimental_soa_helper_return_shadow_canonical_fallbacks_exe.prime",
                source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_experimental_soa_helper_return_shadow_canonical_fallbacks_exe.err")
          .string();
  const std::string compileCmd =
      "./primec --emit=vm " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find(
            "direct import of retired soa compatibility modules is not supported") !=
        std::string::npos);
}

TEST_CASE("rejects borrowed local experimental soa read-only methods in C++ emitter") {
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
      writeTemp("compile_experimental_soa_borrowed_local_methods_exe.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_experimental_soa_borrowed_local_methods_exe.err")
          .string();
  const std::string compileCmd =
      "./primec --emit=vm " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("direct import of retired soa compatibility modules is not supported") !=
        std::string::npos);
}

TEST_CASE("rejects inline location experimental soa read-only methods in C++ emitter") {
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
      writeTemp("compile_experimental_soa_inline_location_methods_exe.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_experimental_soa_inline_location_methods_exe.err")
          .string();
  const std::string compileCmd =
      "./primec --emit=vm " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("direct import of retired soa compatibility modules is not supported") !=
        std::string::npos);
}

TEST_CASE("rejects borrowed helper-return experimental soa helper surfaces in C++ emitter") {
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
      writeTemp("compile_experimental_soa_borrowed_return_methods_exe.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_experimental_soa_borrowed_return_methods_exe.err")
          .string();
  const std::string compileCmd =
      "./primec --emit=vm " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("direct import of retired soa compatibility modules is not supported") !=
        std::string::npos);
}

TEST_CASE("rejects method-like borrowed helper-return experimental soa helper surfaces in C++ emitter") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/soa/*
import /std/collections/internal_soa/*

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
      writeTemp("compile_experimental_soa_method_like_borrowed_return_helpers_exe.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_experimental_soa_method_like_borrowed_return_helpers_exe.err")
          .string();
  const std::string compileCmd =
      "./primec --emit=vm " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("direct import of retired soa compatibility modules is not supported") !=
        std::string::npos);
}

TEST_CASE("rejects direct return borrowed helper-return experimental soa reads in C++ emitter") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/soa/*
import /std/collections/internal_soa/*
import /std/collections/soa/*
import /std/collections/internal_soa_conversions/*

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
      "compile_experimental_soa_direct_return_borrowed_return_reads_exe.prime",
      source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_experimental_soa_direct_return_borrowed_return_reads_exe.err")
          .string();
  const std::string compileCmd =
      "./primec --emit=vm " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("direct import of retired soa compatibility modules is not supported") !=
        std::string::npos);
}

TEST_CASE("rejects direct return method-like borrowed helper-return experimental soa reads in C++ emitter") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/soa/*
import /std/collections/internal_soa/*
import /std/collections/soa/*
import /std/collections/internal_soa_conversions/*

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
      "compile_experimental_soa_direct_return_method_like_borrowed_return_reads_exe.prime",
      source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_experimental_soa_direct_return_method_like_borrowed_return_reads_exe.err")
          .string();
  const std::string compileCmd =
      "./primec --emit=vm " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("direct import of retired soa compatibility modules is not supported") !=
        std::string::npos);
}

TEST_CASE("rejects direct return inline location borrowed helper-return experimental soa reads in C++ emitter") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/soa/*
import /std/collections/internal_soa/*
import /std/collections/soa/*
import /std/collections/internal_soa_conversions/*

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
      "compile_experimental_soa_direct_return_inline_location_borrowed_return_reads_exe.prime",
      source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_experimental_soa_direct_return_inline_location_borrowed_return_reads_exe.err")
          .string();
  const std::string compileCmd =
      "./primec --emit=vm " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("direct import of retired soa compatibility modules is not supported") !=
        std::string::npos);
}

TEST_CASE("rejects inline location method-like borrowed helper-return experimental soa helpers in C++ emitter") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/soa/*
import /std/collections/internal_soa/*
import /std/collections/soa/*
import /std/collections/internal_soa_conversions/*

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
  [Particle] firstA{location(holder.pickBorrowed(location(values))).get(0i32)}
  [Reference<Particle>] secondA{location(holder.pickBorrowed(location(values))).ref(1i32)}
  [Particle] firstC{get(location(holder.pickBorrowed(location(values))), 1i32)}
  [Reference<Particle>] secondC{ref(location(holder.pickBorrowed(location(values))), 0i32)}
  [vector<Particle>] unpackedA{location(holder.pickBorrowed(location(values))).to_aos()}
  [i32] countA{location(holder.pickBorrowed(location(values))).count()}
  [i32] fieldBareGet{get(location(holder.pickBorrowed(location(values))), 1i32).y}
  [i32] fieldMethodRef{location(holder.pickBorrowed(location(values))).ref(1i32).y}
  [int] helpersA{plus(plus(firstA.x, secondA.x), plus(firstC.x, secondC.y))}
  [int] unpackedCountsA{plus(count(unpackedA), countA)}
  [int] total{plus(helpersA, plus(unpackedCountsA, plus(fieldBareGet, fieldMethodRef)))}
  return(total)
}
)";
  const std::string srcPath = writeTemp(
      "compile_experimental_soa_inline_location_method_like_borrowed_return_helpers_exe.prime",
      source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_experimental_soa_inline_location_method_like_borrowed_return_helpers_exe.err")
          .string();
  const std::string compileCmd =
      "./primec --emit=vm " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("direct import of retired soa compatibility modules is not supported") !=
        std::string::npos);
}

TEST_CASE("rejects direct return inline location method-like borrowed helper-return experimental soa reads in C++ emitter") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/soa/*
import /std/collections/internal_soa/*
import /std/collections/soa/*
import /std/collections/internal_soa_conversions/*

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
      "compile_experimental_soa_direct_return_inline_location_method_like_borrowed_return_reads_exe.prime",
      source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_experimental_soa_direct_return_inline_location_method_like_borrowed_return_reads_exe.err")
          .string();
  const std::string compileCmd =
      "./primec --emit=vm " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("direct import of retired soa compatibility modules is not supported") !=
        std::string::npos);
}

TEST_CASE("rejects inline location borrowed helper-return experimental soa helpers in C++ emitter") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/soa/*
import /std/collections/internal_soa/*
import /std/collections/soa/*
import /std/collections/internal_soa_conversions/*

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
      writeTemp("compile_experimental_soa_inline_location_borrowed_return_helpers_exe.prime",
                source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_experimental_soa_inline_location_borrowed_return_helpers_exe.err")
          .string();
  const std::string compileCmd =
      "./primec --emit=vm " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("direct import of retired soa compatibility modules is not supported") !=
        std::string::npos);
}

TEST_CASE("experimental soa storage helpers in C++ emitter") {
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
  const std::string srcPath = writeTemp("compile_soa_storage_exe.prime", source);
  const std::string compileCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 14);
}

TEST_CASE("experimental soa storage borrowed ref helper in C++ emitter") {
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
  const std::string srcPath = writeTemp("compile_soa_storage_ref_exe.prime", source);
  const std::string compileCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 7);
}

TEST_CASE("experimental soa storage borrowed view helper in C++ emitter") {
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
  const std::string srcPath = writeTemp("compile_soa_storage_view_exe.prime", source);
  const std::string compileCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 9);
}

TEST_CASE("rejects experimental soa storage reserve overflow in C++ emitter") {
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
      writeTemp("compile_soa_storage_reserve_overflow_exe.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_soa_storage_reserve_overflow_err.txt").string();

  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main 2> " + errPath;
  CHECK(runCommand(runCmd) == 3);
  CHECK(readFile(errPath) == "array index out of bounds\n");
}

TEST_CASE("experimental two-column soa storage helpers in C++ emitter") {
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
  const std::string srcPath = writeTemp("compile_soa_storage_two_columns_exe.prime", source);
  const std::string compileCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 24);
}

TEST_CASE("experimental ten-column soa storage helpers in C++ emitter") {
  const std::string source = R"(
import /std/collections/soa_storage/*

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
  const std::string srcPath = writeTemp("compile_soa_storage_ten_columns_exe.prime", source);
  const std::string compileCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 74);
}

TEST_CASE("emits experimental eleven-column soa storage helpers in C++ emitter") {
  const std::string source = R"(
import /std/collections/soa_storage/*

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
  const std::string srcPath = writeTemp("emit_soa_storage_eleven_columns_cpp.prime", source);
  const std::string cppPath =
      (testScratchPath("") / "primec_soa_storage_eleven_columns.cpp").string();

  const std::string emitCmd = "./primec --emit=cpp " + srcPath + " -o " + cppPath + " --entry /main";
  CHECK(runCommand(emitCmd) == 0);
}

TEST_CASE("emits experimental sixteen-column soa storage helpers in C++ emitter") {
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
  const std::string srcPath = writeTemp("emit_soa_storage_sixteen_columns_cpp.prime", source);
  const std::string cppPath =
      (testScratchPath("") / "primec_soa_storage_sixteen_columns.cpp").string();

  const std::string emitCmd = "./primec --emit=cpp " + srcPath + " -o " + cppPath + " --entry /main";
  CHECK(runCommand(emitCmd) == 0);
}

TEST_CASE("compiles and runs string-keyed map constructors in C++ emitter") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [map<string, i32>] values{/std/collections/map/map<string, i32>("a"utf8, 1i32, "b"utf8, 2i32)}
  return(count(values))
}
)";
  const std::string srcPath = writeTemp("compile_collections_string_map.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_collections_string_map_exe").string();

  // TODO-5311: string-keyed .prime maps lower on native/exe.
  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 2);
}

TEST_CASE("compiles and runs string-keyed map constructor indexing sugar in C++ emitter") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [map<string, i32>] values{/std/collections/map/map<string, i32>("a"utf8, 1i32, "b"utf8, 2i32)}
  return(values["b"utf8])
}
)";
  const std::string srcPath = writeTemp("compile_collections_string_map_brackets.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_collections_string_map_brackets_exe").string();

  // TODO-5311: string-keyed .prime maps lower on native/exe.
  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 2);
}

TEST_CASE("canonical namespaced map helpers on experimental map values in C++ emitter") {
  expectCanonicalMapNamespaceExperimentalValueConformance("vm");
}

TEST_CASE("wrapper map helpers on experimental map values in C++ emitter") {
  expectWrapperMapHelperExperimentalValueConformance("exe");
}

TEST_CASE("ownership-sensitive experimental map value methods in C++ emitter") {
  expectExperimentalMapOwnershipMethodConformance("exe");
}

TEST_CASE("helper-wrapped inferred experimental map returns in C++ emitter") {
  expectWrappedInferredExperimentalMapReturnConformance("vm");
}

TEST_CASE("helper-wrapped experimental map parameters in C++ emitter") {
  expectWrappedExperimentalMapParameterConformance("vm");
}

TEST_CASE("helper-wrapped experimental map bindings in C++ emitter") {
  expectWrappedExperimentalMapBindingConformance("vm");
}

TEST_CASE("helper-wrapped experimental map assignment RHS values in C++ emitter") {
  expectWrappedExperimentalMapAssignConformance("vm");
}

TEST_CASE("canonical namespaced map constructors on explicit experimental map bindings in C++ emitter") {
  expectCanonicalMapNamespaceExperimentalConstructorConformance("vm");
}

TEST_CASE("canonical namespaced map constructors through explicit experimental map returns in C++ emitter") {
  expectCanonicalMapNamespaceExperimentalReturnConformance("vm");
}

TEST_CASE("canonical namespaced map constructors through explicit experimental map parameters in C++ emitter") {
  expectCanonicalMapNamespaceExperimentalParameterConformance("vm");
}

TEST_CASE("wrapper map constructors on explicit experimental map bindings in C++ emitter") {
  expectWrapperMapConstructorExperimentalBindingConformance("vm");
}

TEST_CASE("wrapper map constructors through explicit experimental map returns in C++ emitter") {
  expectWrapperMapConstructorExperimentalReturnConformance("vm");
}

TEST_CASE("wrapper map constructors through explicit experimental map parameters in C++ emitter") {
  expectWrapperMapConstructorExperimentalParameterConformance("vm");
}

TEST_CASE("experimental map constructor assignments in C++ emitter") {
  expectExperimentalMapAssignConformance("vm");
}

TEST_CASE("implicit map auto constructor inference in C++ emitter") {
  expectImplicitMapAutoInferenceConformance("vm");
}

TEST_CASE("inferred experimental map returns in C++ emitter") {
  expectInferredExperimentalMapReturnConformance("exe");
}

TEST_CASE("block inferred experimental map returns in C++ emitter") {
  expectBlockInferredExperimentalMapReturnConformance("vm");
}

TEST_CASE("auto block inferred experimental map returns in C++ emitter") {
  expectAutoBlockInferredExperimentalMapReturnConformance("vm");
}

TEST_CASE("inferred experimental map call receivers in C++ emitter") {
  expectInferredExperimentalMapCallReceiverConformance("vm");
}

TEST_CASE("rejects explicit experimental map struct field constructors in C++ emitter") {
  expectExperimentalMapStructFieldConformance("vm");
}

TEST_CASE("inferred experimental map struct fields in C++ emitter") {
  expectInferredExperimentalMapStructFieldConformance("vm");
}

TEST_CASE("helper-wrapped inferred experimental map struct fields in C++ emitter") {
  expectWrappedInferredExperimentalMapStructFieldConformance("vm");
}

TEST_CASE("experimental map method parameters in C++ emitter") {
  expectExperimentalMapMethodParameterConformance("vm");
}

TEST_CASE("inferred experimental map parameters in C++ emitter") {
  expectInferredExperimentalMapParameterConformance("vm");
}

TEST_CASE("inferred experimental map default parameters in C++ emitter") {
  expectInferredExperimentalMapDefaultParameterConformance("vm");
}

TEST_CASE("helper-wrapped inferred experimental map default parameters in C++ emitter") {
  expectWrappedInferredExperimentalMapDefaultParameterConformance("vm");
}

TEST_CASE("experimental map helper receivers in C++ emitter") {
  expectExperimentalMapHelperReceiverConformance("vm");
}

TEST_CASE("helper-wrapped experimental map helper receivers in C++ emitter") {
  expectWrappedExperimentalMapHelperReceiverConformance("vm");
}

TEST_CASE("runs direct-constructor experimental map method receivers in C++ emitter") {
  expectExperimentalMapMethodReceiverConformance("vm");
}

TEST_CASE("runs helper-wrapped experimental map method receivers in C++ emitter") {
  expectWrappedExperimentalMapMethodReceiverConformance("vm");
}

TEST_CASE("experimental map field assignments through canonical helper access in C++ emitter") {
  expectExperimentalMapFieldAssignConformance("vm");
}

TEST_CASE("dereferenced experimental map storage references in C++ emitter") {
  expectExperimentalMapStorageReferenceConformance("vm");
}

TEST_CASE("helper-wrapped Result.ok experimental map result struct fields in C++ emitter") {
  expectWrappedExperimentalMapResultFieldAssignConformance("vm");
}

TEST_CASE("helper-wrapped dereferenced Result.ok experimental map result struct fields in C++ emitter") {
  expectWrappedExperimentalMapResultDerefFieldAssignConformance("vm");
}

TEST_CASE("helper-wrapped experimental map struct storage fields in C++ emitter") {
  expectWrappedExperimentalMapStorageFieldConformance("exe");
}

TEST_CASE("helper-wrapped dereferenced experimental map struct storage fields in C++ emitter") {
  expectWrappedExperimentalMapStorageDerefFieldConformance("vm");
}

TEST_CASE("rejects canonical namespaced map helpers on borrowed experimental map values in C++ emitter") {
  expectCanonicalMapNamespaceExperimentalReferenceConformance("vm");
}

TEST_CASE("canonical namespaced map _ref helpers on borrowed experimental map values in C++ emitter") {
  expectCanonicalMapNamespaceExperimentalBorrowedRefConformance("vm");
}

TEST_CASE("experimental map methods on bound map values in C++ emitter") {
  expectExperimentalMapMethodConformance("vm");
}

TEST_CASE("borrowed experimental map helpers in C++ emitter") {
  expectExperimentalMapReferenceHelperConformance("exe");
}

TEST_CASE("public borrowed map wrappers in C++ emitter") {
  expectPublicMapReferenceWrapperConformance("exe");
}

TEST_CASE("borrowed experimental map methods in C++ emitter") {
  expectExperimentalMapReferenceMethodConformance("exe");
}

TEST_CASE("experimental map inserts in C++ emitter") {
  expectExperimentalMapInsertConformance("vm");
}

TEST_CASE("experimental map ownership-sensitive values in C++ emitter") {
  expectExperimentalMapOwnershipConformance("exe");
}

TEST_CASE("canonical namespaced map inserts on explicit experimental map bindings in C++ emitter") {
  expectCanonicalMapNamespaceExperimentalInsertConformance("exe");
}

TEST_CASE("builtin canonical map first-growth inserts in C++ emitter") {
  expectBuiltinCanonicalMapInsertFirstGrowthConformance("vm");
}

TEST_CASE("builtin canonical map repeated-growth inserts in C++ emitter") {
  expectBuiltinCanonicalMapInsertRepeatedGrowthConformance("vm");
}

TEST_CASE("builtin canonical map insert overwrites in C++ emitter") {
  expectBuiltinCanonicalMapInsertOverwriteConformance("vm");
}

TEST_CASE("builtin canonical map non-local growth in C++ emitter") {
  expectBuiltinCanonicalMapInsertNonLocalGrowthConformance("vm");
}

TEST_CASE("builtin canonical map nested non-local growth in C++ emitter") {
  expectBuiltinCanonicalMapInsertNestedNonLocalGrowthConformance("vm");
}

TEST_CASE("builtin canonical map helper-return borrowed method inserts in C++ emitter") {
  expectBuiltinCanonicalMapInsertHelperReturnBorrowedMethodConformance("vm");
}

TEST_CASE("builtin canonical map struct-field initializer in C++ emitter") {
  expectBuiltinCanonicalMapStructFieldInitializerConformance("vm");
}

TEST_CASE("builtin canonical map direct insert on helper-return value receivers in C++ emitter") {
  expectBuiltinCanonicalMapInsertHelperReturnValueDirectConformance("vm");
}

TEST_CASE("builtin canonical map method insert on helper-return value receivers in C++ emitter") {
  expectBuiltinCanonicalMapInsertHelperReturnValueMethodConformance("vm");
}

TEST_CASE("builtin canonical map direct insert on borrowed holder field receivers in C++ emitter") {
  expectBuiltinCanonicalMapInsertBorrowedHolderFieldDirectConformance("vm");
}

TEST_CASE("rejects canonical map constructor ownership growth in C++ emitter") {
  expectCanonicalMapNamespaceOwnershipReject("vm");
}

TEST_CASE("rejects experimental map bracket access in C++ emitter") {
  expectExperimentalMapIndexConformance("vm");
}

TEST_CASE("canonical namespaced vector helpers in C++ emitter") {
  expectCanonicalVectorNamespaceConformance("vm");
}

TEST_CASE("canonical namespaced vector helpers on explicit Vector bindings in C++ emitter") {
  expectCanonicalVectorNamespaceExplicitVectorBindingConformance("vm");
}

TEST_CASE("stdlib wrapper vector helpers on explicit Vector bindings in C++ emitter") {
  expectStdlibWrapperVectorHelperExplicitVectorBindingConformance("vm");
}

TEST_CASE("rejects stdlib wrapper vector helper explicit Vector mismatch in C++ emitter") {
  expectStdlibWrapperVectorHelperExplicitVectorBindingMismatchReject("vm");
}

TEST_CASE("stdlib wrapper vector constructors on explicit Vector bindings in C++ emitter") {
  expectStdlibWrapperVectorConstructorExplicitVectorBindingConformance("vm");
}

TEST_CASE("keeps stdlib wrapper vector constructor explicit Vector mismatch contract in C++ emitter") {
  expectStdlibWrapperVectorConstructorExplicitVectorBindingMismatchContract("vm");
}

TEST_CASE("stdlib wrapper vector constructors on inferred auto bindings in C++ emitter") {
  expectStdlibWrapperVectorConstructorAutoInferenceConformance("vm");
}

TEST_CASE("rejects stdlib wrapper vector constructor auto inference mismatch in C++ emitter") {
  expectStdlibWrapperVectorConstructorAutoInferenceMismatchReject("vm");
}

TEST_CASE("rejects stdlib wrapper vector constructor receivers in C++ emitter") {
  expectStdlibWrapperVectorConstructorReceiverConformance("vm");
}

TEST_CASE("rejects stdlib wrapper vector helper receiver mismatch in C++ emitter") {
  expectStdlibWrapperVectorConstructorHelperReceiverMismatchReject("vm");
}

TEST_CASE("rejects stdlib wrapper vector method receiver mismatch in C++ emitter") {
  expectStdlibWrapperVectorConstructorMethodReceiverMismatchReject("vm");
}

TEST_CASE("rejects canonical namespaced vector constructor temporaries in C++ emitter") {
  expectCanonicalVectorNamespaceTemporaryReceiverConformance("vm");
}

TEST_CASE("rejects canonical namespaced vector explicit builtin bindings in C++ emitter") {
  expectCanonicalVectorNamespaceExplicitBindingReject("vm");
}

TEST_CASE("rejects canonical namespaced vector named-argument temporaries in C++ emitter") {
  expectCanonicalVectorNamespaceNamedArgsTemporaryReceiverConformance("vm");
}

TEST_CASE("rejects canonical namespaced vector named-argument explicit builtin bindings in C++ emitter") {
  expectCanonicalVectorNamespaceNamedArgsExplicitBindingReject("vm");
}

TEST_CASE("rejects canonical namespaced vector mutators without imported helpers in C++ emitter") {
  expectCanonicalVectorClearImportRequirement("vm");
  expectCanonicalVectorRemoveAtImportRequirement("vm");
  expectCanonicalVectorRemoveSwapImportRequirement("vm");
}

TEST_CASE("bare vector count and capacity through imported stdlib helpers in C++ emitter") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32, 3i32)}
  return(plus(/std/collections/vector/count<i32>(values), /std/collections/vector/capacity<i32>(values)))
}
)";
  const std::string srcPath = writeTemp("compile_exe_bare_vector_count_capacity_imported.prime", source);
  const std::string compileCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 6);
}

TEST_CASE("bare vector access through imported stdlib helpers in C++ emitter") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32, 3i32, 4i32)}
  return(plus(
      plus(/std/collections/vector/at<i32>(values, 0i32), /std/collections/vector/at_unsafe<i32>(values, 1i32)),
      plus(/std/collections/vector/at<i32>(values, 2i32), /std/collections/vector/at_unsafe<i32>(values, 3i32))))
}
)";
  const std::string srcPath = writeTemp("compile_exe_bare_vector_access_imported.prime", source);
  const std::string compileCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 10);
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
      "./primec --emit=vm " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
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
      "./primec --emit=vm " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
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

TEST_CASE("experimental vector helper runtime contracts in C++ emitter") {
  expectExperimentalVectorRuntimeContracts("vm");
}

TEST_CASE("experimental vector ownership-sensitive helpers in C++ emitter") {
  expectExperimentalVectorOwnershipContracts("vm");
}

TEST_CASE("canonical vector helpers on experimental vector receivers in C++ emitter") {
  expectExperimentalVectorCanonicalHelperRoutingConformance("vm");
}
TEST_CASE("vector pop empty runtime contract in C++ emitter") {
  SUBCASE("call") {
    expectVectorPopEmptyRuntimeContract("exe", false);
  }

  SUBCASE("method") {
    expectVectorPopEmptyRuntimeContract("exe", true);
  }
}

TEST_CASE("vector index runtime contract in C++ emitter") {
  expectVectorIndexRuntimeContract("exe", "access_call");
  expectVectorIndexRuntimeContract("exe", "access_method");
  expectVectorIndexRuntimeContract("exe", "access_bracket");
  expectVectorIndexRuntimeContract("exe", "remove_at_call");
  expectVectorIndexRuntimeContract("exe", "remove_at_method");
  expectVectorIndexRuntimeContract("exe", "remove_swap_call");
  expectVectorIndexRuntimeContract("exe", "remove_swap_method");
}

TEST_CASE("container error contract conformance in C++ emitter") {
  expectContainerErrorConformance("vm");
}

TEST_CASE("checked pointer conformance harness in C++ emitter") {
  expectCheckedPointerHelperSurfaceConformance("exe");
  expectCheckedPointerGrowthConformance("exe");
  expectCheckedPointerOutOfBoundsConformance("exe");
}

TEST_CASE("unchecked pointer conformance harness in C++ emitter") {
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
  const std::string compileCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 1);
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

  const std::string compileCmd = "./primec --emit=vm " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 2);
}

TEST_CASE("pointer plus u64 offset") {
  const std::string source = R"(
[return<int>]
main() {
  [i32] value{5i32}
  return(dereference(plus(location(value), 0u64)))
}
)";
  const std::string srcPath = writeTemp("compile_pointer_plus_u64.prime", source);
  const std::string compileCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 5);
}

TEST_CASE("i64 literal returns its value") {
  const std::string source = R"(
[return<i64>]
main() {
  return(9i64)
}
)";
  const std::string srcPath = writeTemp("compile_i64_literal.prime", source);
  const std::string compileCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 9);
}

TEST_CASE("u64 literal returns its value") {
  const std::string source = R"(
[return<u64>]
main() {
  return(10u64)
}
)";
  const std::string srcPath = writeTemp("compile_u64_literal.prime", source);
  const std::string compileCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 10);
}

TEST_CASE("assignment operator rewrite") {
  const std::string source = R"(
[return<int>]
main() {
  [i32 mut] value{1i32}
  value=2i32
  return(value)
}
)";
  const std::string srcPath = writeTemp("compile_assign_op.prime", source);
  const std::string compileCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 2);
}

TEST_CASE("comparison operator rewrite") {
  const std::string source = R"(
[return<bool>]
main() {
  return(2i32>1i32)
}
)";
  const std::string srcPath = writeTemp("compile_gt_op.prime", source);
  const std::string compileCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 1);
}

TEST_CASE("less_than operator rewrite") {
  const std::string source = R"(
[return<bool>]
main() {
  return(1i32<2i32)
}
)";
  const std::string srcPath = writeTemp("compile_lt_op.prime", source);
  const std::string compileCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 1);
}

TEST_CASE("greater_equal operator rewrite") {
  const std::string source = R"(
[return<bool>]
main() {
  return(2i32>=2i32)
}
)";
  const std::string srcPath = writeTemp("compile_ge_op.prime", source);
  const std::string compileCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 1);
}

TEST_CASE("less_equal operator rewrite") {
  const std::string source = R"(
[return<bool>]
main() {
  return(2i32<=2i32)
}
)";
  const std::string srcPath = writeTemp("compile_le_op.prime", source);
  const std::string compileCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 1);
}

TEST_CASE("and operator rewrite") {
  const std::string source = R"(
[return<bool>]
main() {
  return(true&&true)
}
)";
  const std::string srcPath = writeTemp("compile_and_op.prime", source);
  const std::string compileCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 1);
}

TEST_CASE("|| operator rewrites to or()") {
  const std::string source = R"(
[return<bool>]
main() {
  return(false||true)
}
)";
  const std::string srcPath = writeTemp("compile_or_op.prime", source);
  const std::string compileCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 1);
}

TEST_CASE("not operator rewrite") {
  const std::string source = R"(
[return<bool>]
main() {
  return(!false)
}
)";
  const std::string srcPath = writeTemp("compile_not_op.prime", source);
  const std::string compileCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 1);
}

TEST_CASE("not operator with parentheses") {
  const std::string source = R"(
[return<bool>]
main() {
  return(!(false))
}
)";
  const std::string srcPath = writeTemp("compile_not_paren.prime", source);
  const std::string compileCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 1);
}

TEST_CASE("unary minus operator rewrite") {
  const std::string source = R"(
[return<int>]
main() {
  [i32] value{3i32}
  return(plus(-value, 5i32))
}
)";
  const std::string srcPath = writeTemp("compile_unary_minus.prime", source);
  const std::string compileCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 2);
}

TEST_CASE("equality operator rewrite") {
  const std::string source = R"(
[return<bool>]
main() {
  return(2i32==2i32)
}
)";
  const std::string srcPath = writeTemp("compile_eq_op.prime", source);
  const std::string compileCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 1);
}

TEST_CASE("not_equal operator rewrite") {
  const std::string source = R"(
[return<bool>]
main() {
  return(2i32!=3i32)
}
)";
  const std::string srcPath = writeTemp("compile_neq_op.prime", source);
  const std::string compileCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 1);
}

TEST_SUITE_END();
