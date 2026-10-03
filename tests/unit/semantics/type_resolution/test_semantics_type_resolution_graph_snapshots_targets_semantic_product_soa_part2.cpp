#include "third_party/doctest.h"

#include "test_semantics_type_resolution_graph_snapshots_shared.h"

TEST_SUITE_BEGIN("primestruct.semantics.type_resolution_graph");

TEST_CASE("semantic product keeps method-like borrowed soa read targets on canonical wrappers compatibility") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/soa/*
import /std/collections/soa/*

[struct reflect]
Particle() {
  [i32] x{1i32}
}

[struct]
Holder() {}

[return<Reference<SoaVector<Particle>>>]
/Holder/pickBorrowed([Holder] self, [Reference<SoaVector<Particle>>] values) {
  return(values)
}

[effects(heap_alloc), return<i32>]
main() {
  [SoaVector<Particle> mut] values{soaVectorNew<Particle>()}
  values.push(Particle(7i32))
  values.push(Particle(9i32))
  [Holder] holder{Holder{}}
  [auto] picked{holder.pickBorrowed(location(values)).get(1i32)}
  [auto] pickedRef{holder.pickBorrowed(location(values)).ref(0i32)}
  [auto] unpacked{holder.pickBorrowed(location(values)).to_aos()}
  [i32] borrowedCount{holder.pickBorrowed(location(values)).count()}
  return(plus(plus(picked.x, pickedRef.x),
              plus(count(unpacked), borrowedCount)))
}
)";

  primec::CompilePipelineOutput output;
  std::string error;
  const bool ok = validateSoaCompatSourceForTesting(source, output, error);
  INFO(error);
  // TODO-5050 shape (a) + to_aos_ref gap (RESOLVED): same fix class as the
  // free-function variant above, for the struct-method receiver form -
  // get/ref/count/to_aos now all resolve. Verified via a standalone
  // `--emit=vm` probe that the fixture also runs to completion, returning
  // 20 (9 + 7 + 2 + 2).
  CHECK(ok);
}

TEST_CASE("semantic product keeps borrowed soa ref_ref targets on same-path helpers compatibility") {
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
  return(23i32)
}

[effects(heap_alloc), return<i32>]
main() {
  [SoaVector<Particle> mut] values{soaVectorNew<Particle>()}
  [auto] picked{pickBorrowed(location(values)).ref(0i32)}
  [auto] pickedDirect{ref_ref(pickBorrowed(location(values)), 0i32)}
  return(plus(picked, pickedDirect))
}
)";

  primec::CompilePipelineOutput output;
  std::string error;
  const bool ok = validateSoaCompatSourceForTesting(source, output, error);
  INFO(error);
  // TODO-5050 shape (a) (RESOLVED), shape (b) side effect: fixing the
  // canonical public soa read-helper routing for helper-return borrowed
  // receivers also fixed method-call-form dispatch (.ref(...)) honoring
  // the user's same-path /soa/ref_ref shadow, matching the direct-call
  // form (ref_ref(receiver, ...)) which already honored it. Confirmed via
  // a standalone --emit=vm run of this exact fixture: it now runs to
  // completion returning 46 (23 + 23), proving BOTH call forms dispatch
  // to the user's shadow (which literally returns 23i32) rather than the
  // real canonical stdlib helper (which would index an empty vector).
  CHECK(ok);
  CHECK(error.empty());
}

TEST_CASE("semantic product keeps builtin soa ref_ref targets on same-path helpers") {
  const std::string source = R"(
import /std/collections/*

[struct reflect]
Particle() {
  [i32] x{1i32}
}

[effects(heap_alloc), return<soa<Particle>>]
cloneValues() {
  [soa<Particle>] values{soa<Particle>()}
  return(values)
}

[effects(heap_alloc), return<int>]
/soa/ref_ref([soa<Particle>] values, [vector<i32>] index) {
  return(17i32)
}

[effects(heap_alloc), return<i32>]
main() {
  [vector<i32>] idx{vector<i32>(0i32)}
  [soa<Particle>] values{cloneValues()}
  [auto] direct{ref_ref(values, idx)}
  [auto] method{values.ref_ref(idx)}
  [auto] helperReturn{ref_ref(cloneValues(), idx)}
  return(plus(direct, plus(method, helperReturn)))
}
)";

  primec::CompilePipelineOutput output;
  std::string error;
  const bool ok = validateSoaCompatSourceForTesting(source, output, error);
  INFO(error);
  // TODO-5295 (RESOLVED): all three forms (direct, method sugar,
  // helper-return receiver) now target the user's /soa/ref_ref shadow
  // instead of failing with "template arguments required for
  // /std/collections/soa/ref_ref". TODO-5307 (RESOLVED): the [auto]
  // locals now take the shadow's declared int result instead of the
  // canonical helper's element type, so plus(...) over them validates
  // (and runs to 51 on --emit=vm).
  CHECK(ok);
  CHECK(error.empty());
}

TEST_CASE("semantic product validates direct return method-like borrowed helper-return experimental soa reads") {
  const std::string source = R"(
[struct reflect]
Particle() {
  [i32] x{1i32}
  [i32] y{2i32}
}

[return<void>]
/std/collections/soa/push([soa<Particle> mut] values, [Particle] value) {
}

[return<i32>]
/std/collections/soa/count([soa<Particle>] values) {
  return(2i32)
}

[return<i32>]
/std/collections/soa/count_ref([Reference<soa<Particle>>] values) {
  return(2i32)
}

[return<Particle>]
/std/collections/soa/get_ref([Reference<soa<Particle>>] values, [i32] index) {
  return(Particle(0i32, 0i32))
}

[struct]
Holder() {}

[return<Reference<soa<Particle>>>]
/Holder/pickBorrowed([Holder] self, [Reference<soa<Particle>>] values) {
  return(values)
}

[effects(heap_alloc), return<int>]
main() {
  [soa<Particle> mut] values{soa<Particle>()}
  /std/collections/soa/push(values, Particle(7i32, 8i32))
  /std/collections/soa/push(values, Particle(9i32, 12i32))
  [Holder] holder{Holder{}}
  return(
    plus(/std/collections/soa/count_ref(holder.pickBorrowed(location(values))),
         plus(/std/collections/soa/count(
                  dereference(holder.pickBorrowed(location(values)))),
              plus(holder.pickBorrowed(location(values)).get_ref(0i32).x,
                   plus(/std/collections/soa/get_ref(
                            holder.pickBorrowed(location(values)), 1i32).y,
                        get_ref(holder.pickBorrowed(location(values)), 1i32).y))))
  )
}
)";

  auto program = parseProgram(source);
  primec::Semantics semantics;
  primec::SemanticProgram semanticProgram;
  std::string error;
  const std::vector<std::string> defaults = {"io_out", "io_err"};
  const bool valid =
      semantics.validate(program, "/main", error, defaults, defaults, {}, nullptr, false,
                         &semanticProgram);
  INFO(error);
  // TODO-5050 shape (c) (the "explicit rooted-path direct call to a
  // user-declared function breaks specifically on a borrowed
  // helper-return receiver" bug this test originally pinned) was root-
  // caused and fixed by TODO-5285 (see docs/todo_finished.md): the
  // explicit rooted-path direct call now resolves correctly on a
  // borrowed helper-return receiver, matching the bare unrooted direct-
  // call and method-call forms' already-working behavior on the same
  // receiver.
  CHECK(valid);
}

TEST_CASE("semantic product keeps helper-return SoaVector mutator initializer facts on wrappers compatibility") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/soa/*

[struct reflect]
Particle() {
  [i32] x{1i32}
}

[return<SoaVector<Particle>>]
cloneValues() {
  return(SoaVector<Particle>{})
}

[return<i32>]
/soa/push([SoaVector<Particle>] values, [Particle] value) {
  return(value.x)
}

[return<i32>]
/soa/reserve([SoaVector<Particle>] values, [i32] count) {
  return(count)
}

[return<i32>]
/std/collections/soa/push([SoaVector<Particle>] values, [Particle] value) {
  return(plus(value.x, 100i32))
}

[return<i32>]
/std/collections/soa/reserve([SoaVector<Particle>] values, [i32] count) {
  return(plus(count, 100i32))
}

[return<void>]
/std/collections/soa/SoaVector__Particle/push([SoaVector<Particle>] values,
                                                                   [Particle] value) {
}

[return<void>]
/std/collections/soa/SoaVector__Particle/reserve([SoaVector<Particle>] values,
                                                                      [i32] count) {
}

[effects(heap_alloc), return<i32>]
main() {
  [auto] pushed{cloneValues().push(Particle(7i32))}
  [auto] reserved{cloneValues().reserve(4i32)}
  return(plus(pushed, reserved))
}
)";

  primec::CompilePipelineOutput output;
  std::string error;
  const bool ok = validateSoaCompatSourceForTesting(source, output, error);
  INFO(error);
  REQUIRE(ok);
  REQUIRE(output.hasSemanticProgram);
  primec::SemanticProgram &semanticProgram = output.semanticProgram;
  CHECK(error.empty());

  // TODO-5050 (categorization gap, not a validation failure): unlike the
  // equivalent same-path-shadow .push()/.reserve() method-call sugar on the
  // builtin soa<T> receiver (see "semantic product keeps helper-return soa
  // mutator initializer facts on wrappers compatibility" earlier in this
  // file, which does route through methodCallTargets), the public
  // SoaVector<T> wrapper receiver desugars .push(...)/.reserve(...) into a
  // direct-call fact (callName is the full rooted "/soa/push"/"/soa/reserve"
  // path, not the bare method name) rather than a methodCallTarget entry -
  // pinning verified current behavior.
  const auto *pushDirectTarget = findSemanticEntry(
      primec::semanticProgramDirectCallTargetView(semanticProgram),
      [&semanticProgram](const primec::SemanticProgramDirectCallTarget &entry) {
        return entry.scopePath == "/main" && entry.callName == "/soa/push" &&
               primec::semanticProgramDirectCallTargetResolvedPath(semanticProgram, entry) ==
                   "/soa/push";
      });
  REQUIRE(pushDirectTarget != nullptr);

  const auto *reserveDirectTarget = findSemanticEntry(
      primec::semanticProgramDirectCallTargetView(semanticProgram),
      [&semanticProgram](const primec::SemanticProgramDirectCallTarget &entry) {
        return entry.scopePath == "/main" && entry.callName == "/soa/reserve" &&
               primec::semanticProgramDirectCallTargetResolvedPath(semanticProgram, entry) ==
                   "/soa/reserve";
      });
  REQUIRE(reserveDirectTarget != nullptr);

  const auto *pushedEntry = findSemanticEntry(
      primec::semanticProgramLocalAutoFactView(semanticProgram),
      [](const primec::SemanticProgramLocalAutoFact &entry) {
        return entry.scopePath == "/main" && entry.bindingName == "pushed";
      });
  REQUIRE(pushedEntry != nullptr);
  CHECK(primec::semanticProgramLocalAutoFactInitializerResolvedPath(semanticProgram, *pushedEntry) ==
        "/soa/push");
  CHECK(pushedEntry->initializerDirectCallResolvedPath == "/soa/push");
  CHECK(pushedEntry->initializerMethodCallResolvedPath.empty());

  const auto *reservedEntry = findSemanticEntry(
      primec::semanticProgramLocalAutoFactView(semanticProgram),
      [](const primec::SemanticProgramLocalAutoFact &entry) {
        return entry.scopePath == "/main" && entry.bindingName == "reserved";
      });
  REQUIRE(reservedEntry != nullptr);
  CHECK(primec::semanticProgramLocalAutoFactInitializerResolvedPath(semanticProgram, *reservedEntry) ==
        "/soa/reserve");
  CHECK(reservedEntry->initializerDirectCallResolvedPath == "/soa/reserve");
  CHECK(reservedEntry->initializerMethodCallResolvedPath.empty());

  const auto choseConcreteExperimentalPushTargets = primec::semanticProgramDirectCallTargetView(semanticProgram);
  const bool choseConcreteExperimentalPush = std::any_of(
      choseConcreteExperimentalPushTargets.begin(),
      choseConcreteExperimentalPushTargets.end(),
      [&semanticProgram](const primec::SemanticProgramDirectCallTarget *entry) {
        return entry->scopePath == "/main" &&
               primec::semanticProgramDirectCallTargetResolvedPath(semanticProgram, *entry) ==
                   "/std/collections/soa/SoaVector__Particle/push";
      });
  CHECK_FALSE(choseConcreteExperimentalPush);
}

TEST_CASE("semantic product keeps helper-return borrowed soa direct-call targets on canonical wrappers compatibility") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/soa/*
import /std/collections/soa/*

[struct reflect]
Particle() {
  [i32] x{1i32}
}

[return<Reference<SoaVector<Particle>>>]
pickBorrowed([Reference<SoaVector<Particle>>] values) {
  return(values)
}

[effects(heap_alloc), return<i32>]
main() {
  [SoaVector<Particle> mut] values{soaVectorNew<Particle>()}
  values.push(Particle(7i32))
  values.push(Particle(9i32))
  [auto] picked{get(pickBorrowed(location(values)), 1i32)}
  [auto] pickedBorrowed{get_ref(pickBorrowed(location(values)), 1i32)}
  [auto] pickedRef{ref(pickBorrowed(location(values)), 0i32)}
  [auto] pickedBorrowedRef{ref_ref(pickBorrowed(location(values)), 0i32)}
  [auto] unpacked{to_aos(pickBorrowed(location(values)))}
  [auto] borrowedUnpacked{to_aos_ref(pickBorrowed(location(values)))}
  [i32] borrowedCount{count(pickBorrowed(location(values)))}
  [i32] borrowedCountRef{count_ref(pickBorrowed(location(values)))}
  return(plus(plus(plus(picked.x, pickedBorrowed.x),
                   plus(pickedRef.x, pickedBorrowedRef.x)),
              plus(plus(count(unpacked), count(borrowedUnpacked)),
                   plus(borrowedCount, borrowedCountRef))))
}
)";

  primec::CompilePipelineOutput output;
  std::string error;
  const bool ok = validateSoaCompatSourceForTesting(source, output, error);
  INFO(error);
  // TODO-5050 shape (a) + to_aos_ref gap (RESOLVED): same fix class as the
  // method-call-form cases above, for direct-call syntax -
  // get/get_ref/ref/ref_ref/count/count_ref/to_aos/to_aos_ref now all
  // resolve. Verified via a standalone `--emit=vm` probe that the fixture
  // also runs to completion, returning 40 (9+9+7+7+2+2+2+2).
  CHECK(ok);
}

TEST_CASE("semantic product keeps helper-return borrowed soa field views on canonical reads compatibility") {
  const std::string source = R"(
import /std/collections/*
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

[effects(heap_alloc), return<i32>]
main() {
  [SoaVector<Particle> mut] values{soaVectorNew<Particle>()}
  values.push(Particle(7i32, 8i32))
  values.push(Particle(9i32, 12i32))
  [auto] fieldMethod{pickBorrowed(location(values)).y()[1i32]}
  [auto] fieldCall{y(pickBorrowed(location(values)))[0i32]}
  return(plus(fieldMethod, fieldCall))
}
)";

  primec::CompilePipelineOutput output;
  std::string error;
  const bool ok = validateSoaCompatSourceForTesting(source, output, error);
  INFO(error);
  REQUIRE(ok);
  REQUIRE(output.hasSemanticProgram);
  primec::SemanticProgram &semanticProgram = output.semanticProgram;
  CHECK(error.empty());

  const auto *fieldMethodEntry = findSemanticEntry(
      primec::semanticProgramLocalAutoFactView(semanticProgram),
      [](const primec::SemanticProgramLocalAutoFact &entry) {
        return entry.scopePath == "/main" && entry.bindingName == "fieldMethod";
      });
  REQUIRE(fieldMethodEntry != nullptr);
  CHECK(fieldMethodEntry->bindingTypeText == "i32");
  // TODO-5050 (stale expected routing symbol, re-pinned to verified
  // current behavior): the SoA field-view read no longer routes through a
  // soaFieldViewRead direct-call bridge - it resolves as a method-call
  // fact to the element struct's own reflect-generated field accessor.
  CHECK(fieldMethodEntry->initializerMethodCallResolvedPath == "/Particle/y");
  CHECK(fieldMethodEntry->initializerMethodCallReturnKind == "i32");

  const auto *fieldCallEntry = findSemanticEntry(
      primec::semanticProgramLocalAutoFactView(semanticProgram),
      [](const primec::SemanticProgramLocalAutoFact &entry) {
        return entry.scopePath == "/main" && entry.bindingName == "fieldCall";
      });
  REQUIRE(fieldCallEntry != nullptr);
  CHECK(fieldCallEntry->bindingTypeText == "i32");
  CHECK(fieldCallEntry->initializerMethodCallResolvedPath == "/Particle/y");
  CHECK(fieldCallEntry->initializerMethodCallReturnKind == "i32");

  const auto choseExplicitFieldViewBridgeTargets = primec::semanticProgramDirectCallTargetView(semanticProgram);
  const bool choseExplicitFieldViewBridge = std::any_of(
      choseExplicitFieldViewBridgeTargets.begin(),
      choseExplicitFieldViewBridgeTargets.end(),
      [&semanticProgram](const primec::SemanticProgramDirectCallTarget *entry) {
        return entry->scopePath == "/main" &&
               primec::semanticProgramDirectCallTargetResolvedPath(semanticProgram, *entry) ==
                   "/std/collections/experimental_soa/soaVectorFieldView";
      });
  CHECK_FALSE(choseExplicitFieldViewBridge);
}

TEST_CASE("semantic product keeps borrowed local soa field views on canonical reads compatibility") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/soa/*

[struct reflect]
Particle() {
  [i32] x{1i32}
  [i32] y{2i32}
}

[effects(heap_alloc), return<i32>]
main() {
  [SoaVector<Particle> mut] values{soaVectorNew<Particle>()}
  values.push(Particle(7i32, 8i32))
  values.push(Particle(9i32, 12i32))
  [Reference<SoaVector<Particle>>] borrowed{location(values)}
  [auto] fieldMethod{borrowed.y()[1i32]}
  [auto] fieldCall{y(borrowed)[0i32]}
  return(plus(fieldMethod, fieldCall))
}
)";

  primec::CompilePipelineOutput output;
  std::string error;
  const bool ok = validateSoaCompatSourceForTesting(source, output, error);
  INFO(error);
  REQUIRE(ok);
  REQUIRE(output.hasSemanticProgram);
  primec::SemanticProgram &semanticProgram = output.semanticProgram;
  CHECK(error.empty());

  const auto *fieldMethodEntry = findSemanticEntry(
      primec::semanticProgramLocalAutoFactView(semanticProgram),
      [](const primec::SemanticProgramLocalAutoFact &entry) {
        return entry.scopePath == "/main" && entry.bindingName == "fieldMethod";
      });
  REQUIRE(fieldMethodEntry != nullptr);
  CHECK(fieldMethodEntry->bindingTypeText == "i32");
  // TODO-5050 (stale expected routing symbol, re-pinned to verified
  // current behavior): the SoA field-view read no longer routes through a
  // soaFieldViewRead direct-call bridge - it resolves as a method-call
  // fact to the element struct's own reflect-generated field accessor.
  CHECK(fieldMethodEntry->initializerMethodCallResolvedPath == "/Particle/y");
  CHECK(fieldMethodEntry->initializerMethodCallReturnKind == "i32");

  const auto *fieldCallEntry = findSemanticEntry(
      primec::semanticProgramLocalAutoFactView(semanticProgram),
      [](const primec::SemanticProgramLocalAutoFact &entry) {
        return entry.scopePath == "/main" && entry.bindingName == "fieldCall";
      });
  REQUIRE(fieldCallEntry != nullptr);
  CHECK(fieldCallEntry->bindingTypeText == "i32");
  CHECK(fieldCallEntry->initializerMethodCallResolvedPath == "/Particle/y");
  CHECK(fieldCallEntry->initializerMethodCallReturnKind == "i32");

  const auto choseExplicitFieldViewBridgeTargets = primec::semanticProgramDirectCallTargetView(semanticProgram);
  const bool choseExplicitFieldViewBridge = std::any_of(
      choseExplicitFieldViewBridgeTargets.begin(),
      choseExplicitFieldViewBridgeTargets.end(),
      [&semanticProgram](const primec::SemanticProgramDirectCallTarget *entry) {
        return entry->scopePath == "/main" &&
               primec::semanticProgramDirectCallTargetResolvedPath(semanticProgram, *entry) ==
                   "/std/collections/experimental_soa/soaVectorFieldView";
      });
  CHECK_FALSE(choseExplicitFieldViewBridge);
}

TEST_CASE("semantic product keeps method-like borrowed soa field views on canonical reads compatibility") {
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

[effects(heap_alloc), return<i32>]
main() {
  [SoaVector<Particle> mut] values{soaVectorNew<Particle>()}
  values.push(Particle(7i32, 8i32))
  values.push(Particle(9i32, 12i32))
  [Holder] holder{Holder{}}
  [auto] fieldMethod{holder.pickBorrowed(location(values)).y()[1i32]}
  [auto] fieldCall{y(holder.pickBorrowed(location(values)))[0i32]}
  return(plus(fieldMethod, fieldCall))
}
)";

  primec::CompilePipelineOutput output;
  std::string error;
  const bool ok = validateSoaCompatSourceForTesting(source, output, error);
  INFO(error);
  REQUIRE(ok);
  REQUIRE(output.hasSemanticProgram);
  primec::SemanticProgram &semanticProgram = output.semanticProgram;
  CHECK(error.empty());

  const auto *fieldMethodEntry = findSemanticEntry(
      primec::semanticProgramLocalAutoFactView(semanticProgram),
      [](const primec::SemanticProgramLocalAutoFact &entry) {
        return entry.scopePath == "/main" && entry.bindingName == "fieldMethod";
      });
  REQUIRE(fieldMethodEntry != nullptr);
  CHECK(fieldMethodEntry->bindingTypeText == "i32");
  // TODO-5050 (stale expected routing symbol, re-pinned to verified
  // current behavior): the SoA field-view read no longer routes through a
  // soaFieldViewRead direct-call bridge - it resolves as a method-call
  // fact to the element struct's own reflect-generated field accessor.
  CHECK(fieldMethodEntry->initializerMethodCallResolvedPath == "/Particle/y");
  CHECK(fieldMethodEntry->initializerMethodCallReturnKind == "i32");

  const auto *fieldCallEntry = findSemanticEntry(
      primec::semanticProgramLocalAutoFactView(semanticProgram),
      [](const primec::SemanticProgramLocalAutoFact &entry) {
        return entry.scopePath == "/main" && entry.bindingName == "fieldCall";
      });
  REQUIRE(fieldCallEntry != nullptr);
  CHECK(fieldCallEntry->bindingTypeText == "i32");
  CHECK(fieldCallEntry->initializerMethodCallResolvedPath == "/Particle/y");
  CHECK(fieldCallEntry->initializerMethodCallReturnKind == "i32");

  const auto choseExplicitFieldViewBridgeTargets = primec::semanticProgramDirectCallTargetView(semanticProgram);
  const bool choseExplicitFieldViewBridge = std::any_of(
      choseExplicitFieldViewBridgeTargets.begin(),
      choseExplicitFieldViewBridgeTargets.end(),
      [&semanticProgram](const primec::SemanticProgramDirectCallTarget *entry) {
        return entry->scopePath == "/main" &&
               primec::semanticProgramDirectCallTargetResolvedPath(semanticProgram, *entry) ==
                   "/std/collections/experimental_soa/soaVectorFieldView";
      });
  CHECK_FALSE(choseExplicitFieldViewBridge);
}

TEST_CASE("semantic product keeps helper-return soa direct-call targets on alias wrappers") {
  const std::string source = R"(
[struct reflect]
Particle() {
  [i32] x{1i32}
}

[return<soa<Particle>>]
cloneValues() {
  [soa<Particle>] values{soa<Particle>()}
  return(values)
}

[return<Particle>]
/soa/get([soa<Particle>] values, [i32] index) {
  return(Particle(index))
}

[return<Particle>]
/soa/ref([soa<Particle>] values, [i32] index) {
  return(Particle(index))
}

[return<vector<Particle>>]
/to_aos([soa<Particle>] values) {
  return(vector<Particle>())
}

[return<i32>]
/soa/push([soa<Particle>] values, [Particle] value) {
  return(value.x)
}

[return<i32>]
/soa/reserve([soa<Particle>] values, [i32] count) {
  return(count)
}

[return<Particle>]
/std/collections/soa/get([soa<Particle>] values, [i32] index) {
  return(Particle(plus(index, 100i32)))
}

[return<Particle>]
/std/collections/soa/ref([soa<Particle>] values, [i32] index) {
  return(Particle(plus(index, 100i32)))
}

[return<vector<Particle>>]
/std/collections/soa/to_aos([soa<Particle>] values) {
  return(vector<Particle>())
}

[return<i32>]
/std/collections/soa/push([soa<Particle>] values, [Particle] value) {
  return(plus(value.x, 100i32))
}

[return<i32>]
/std/collections/soa/reserve([soa<Particle>] values, [i32] count) {
  return(plus(count, 100i32))
}

[return<Particle>]
/std/collections/soa/SoaVector__Particle/get([soa<Particle>] values,
                                                                  [i32] index) {
  return(Particle(plus(index, 200i32)))
}

[return<Particle>]
/std/collections/soa/SoaVector__Particle/ref([soa<Particle>] values,
                                                                  [i32] index) {
  return(Particle(plus(index, 200i32)))
}

[return<vector<Particle>>]
/std/collections/soa/SoaVector__Particle/to_aos([soa<Particle>] values) {
  return(vector<Particle>())
}

[return<void>]
/std/collections/soa/SoaVector__Particle/push([soa<Particle>] values,
                                                                   [Particle] value) {
}

[return<void>]
/std/collections/soa/SoaVector__Particle/reserve([soa<Particle>] values,
                                                                      [i32] count) {
}

[effects(heap_alloc), return<i32>]
main() {
  [auto] picked{/soa/get(cloneValues(), 1i32)}
  [auto] pickedRef{/soa/ref(cloneValues(), 1i32)}
  [auto] unpacked{/to_aos(cloneValues())}
  [auto] pushed{/soa/push(cloneValues(), Particle(7i32))}
  [auto] reserved{/soa/reserve(cloneValues(), 4i32)}
  return(plus(plus(picked.x, pickedRef.x), plus(pushed, reserved)))
}
)";

  auto program = parseProgram(source);
  primec::Semantics semantics;
  primec::SemanticProgram semanticProgram;
  std::string error;
  const std::vector<std::string> defaults = {"io_out", "io_err"};
  REQUIRE(semantics.validate(program, "/main", error, defaults, defaults, {}, nullptr, false,
                             &semanticProgram));
  CHECK(error.empty());

  const auto *pickedEntry = findSemanticEntry(
      primec::semanticProgramLocalAutoFactView(semanticProgram),
      [](const primec::SemanticProgramLocalAutoFact &entry) {
        return entry.scopePath == "/main" && entry.bindingName == "picked";
      });
  REQUIRE(pickedEntry != nullptr);
  CHECK(pickedEntry->initializerDirectCallResolvedPath == "/soa/get");

  const auto *pickedRefEntry = findSemanticEntry(
      primec::semanticProgramLocalAutoFactView(semanticProgram),
      [](const primec::SemanticProgramLocalAutoFact &entry) {
        return entry.scopePath == "/main" && entry.bindingName == "pickedRef";
      });
  REQUIRE(pickedRefEntry != nullptr);
  CHECK(pickedRefEntry->initializerDirectCallResolvedPath == "/soa/ref");

  const auto *unpackedEntry = findSemanticEntry(
      primec::semanticProgramLocalAutoFactView(semanticProgram),
      [](const primec::SemanticProgramLocalAutoFact &entry) {
        return entry.scopePath == "/main" && entry.bindingName == "unpacked";
      });
  REQUIRE(unpackedEntry != nullptr);
  CHECK(unpackedEntry->initializerDirectCallResolvedPath == "/to_aos");

  const auto *pushedEntry = findSemanticEntry(
      primec::semanticProgramLocalAutoFactView(semanticProgram),
      [](const primec::SemanticProgramLocalAutoFact &entry) {
        return entry.scopePath == "/main" && entry.bindingName == "pushed";
      });
  REQUIRE(pushedEntry != nullptr);
  CHECK(pushedEntry->initializerDirectCallResolvedPath == "/soa/push");
  CHECK(pushedEntry->initializerDirectCallReturnKind == "i32");

  const auto *reservedEntry = findSemanticEntry(
      primec::semanticProgramLocalAutoFactView(semanticProgram),
      [](const primec::SemanticProgramLocalAutoFact &entry) {
        return entry.scopePath == "/main" && entry.bindingName == "reserved";
      });
  REQUIRE(reservedEntry != nullptr);
  CHECK(reservedEntry->initializerDirectCallResolvedPath == "/soa/reserve");
  CHECK(reservedEntry->initializerDirectCallReturnKind == "i32");

  const auto choseConcreteExperimentalPushTargets = primec::semanticProgramDirectCallTargetView(semanticProgram);
  const bool choseConcreteExperimentalPush = std::any_of(
      choseConcreteExperimentalPushTargets.begin(),
      choseConcreteExperimentalPushTargets.end(),
      [&semanticProgram](const primec::SemanticProgramDirectCallTarget *entry) {
        return entry->scopePath == "/main" &&
               resolveDirectCallPath(semanticProgram, *entry) ==
                   "/std/collections/soa/SoaVector__Particle/push";
      });
  CHECK_FALSE(choseConcreteExperimentalPush);
}

TEST_CASE("semantic product direct-call targets carry interned path ids") {
  const std::string source =
      "[return<i32>]\n"
      "id_i32() {\n"
      "  return(1i32)\n"
      "}\n"
      "\n"
      "[return<i32>]\n"
      "main() {\n"
      "  [i32] a{id_i32()}\n"
      "  [i32] b{id_i32()}\n"
      "  return(plus(a, b))\n"
      "}\n";

  auto program = parseProgram(source);
  primec::Semantics semantics;
  primec::SemanticProgram semanticProgram;
  std::string error;
  const std::vector<std::string> defaults = {"io_out", "io_err"};
  REQUIRE(semantics.validate(program, "/main", error, defaults, defaults, {}, nullptr, false, &semanticProgram));
  CHECK(error.empty());

  std::vector<const primec::SemanticProgramDirectCallTarget *> mainTargets;
  for (const auto *entry : primec::semanticProgramDirectCallTargetView(semanticProgram)) {
    if (entry->scopePath == "/main" &&
        resolveDirectCallPath(semanticProgram, *entry) == "/id_i32") {
      mainTargets.push_back(entry);
    }
  }
  REQUIRE(mainTargets.size() >= 2);
  REQUIRE(mainTargets[0]->resolvedPathId != primec::InvalidSymbolId);
  CHECK(mainTargets[0]->resolvedPathId == mainTargets[1]->resolvedPathId);
  CHECK(primec::semanticProgramResolveCallTargetString(semanticProgram, mainTargets[0]->resolvedPathId) ==
        "/id_i32");
}

TEST_CASE("semantic product publishes specialized SoaColumn field access targets") {
  const std::string source = R"(
import /std/collections/soa_storage/*

[effects(heap_alloc), return<int>]
main() {
  [SoaColumns5<i32, i32, i32, i32, i32> mut] values{soaColumns5New<i32, i32, i32, i32, i32>()}
  return(soaColumns5Capacity<i32, i32, i32, i32, i32>(values))
}
)";

  primec::testing::CompilePipelineBoundaryDumps dumps;
  std::string error;
  REQUIRE(primec::testing::captureSemanticBoundaryDumpsForTesting(
      source, "/main", dumps, error));
  CHECK(error.empty());
  CHECK(dumps.semanticProduct.find(
            "scope_path=\"/std/collections/soa_storage/SoaColumn__") !=
        std::string::npos);
  CHECK(dumps.semanticProduct.find(
            "/field_capacity\" method_name=\"capacity\" receiver_type_text=\"Reference</std/collections/soa_storage/SoaColumn__") !=
        std::string::npos);
  CHECK(dumps.semanticProduct.find(
            "resolved_path=\"/std/collections/soa_storage/SoaColumn__") !=
        std::string::npos);
  CHECK(dumps.semanticProduct.find("/capacity\" provenance_handle=") !=
        std::string::npos);
}

TEST_CASE("semantic product publishes explicit SoaColumn helper parameter binding facts") {
  const std::string source = R"(
import /std/collections/soa_storage/*

[effects(heap_alloc), return<int>]
main() {
  [SoaColumns5<i32, i32, i32, i32, i32> mut] values{soaColumns5New<i32, i32, i32, i32, i32>()}
  soaColumns5Reserve<i32, i32, i32, i32, i32>(values, 4i32)
  return(soaColumns5Capacity<i32, i32, i32, i32, i32>(values))
}
)";

  primec::testing::CompilePipelineBoundaryDumps dumps;
  std::string error;
  REQUIRE(primec::testing::captureSemanticBoundaryDumpsForTesting(
      source, "/main", dumps, error));
  CHECK(error.empty());
  CHECK(dumps.semanticProduct.find(
            "/set_field_capacity\" site_kind=\"parameter\" name=\"value\"") !=
        std::string::npos);
  CHECK(dumps.semanticProduct.find(
            "site_kind=\"parameter\" name=\"value\" resolved_path=\"/") !=
        std::string::npos);
}

TEST_CASE("semantic product query facts prefer local bindings over math constants") {
  const std::string source =
      "import /std/math/*\n"
      "\n"
      "[return<int>]\n"
      "main() {\n"
      "  [f32] e{acosh(1.0f32)}\n"
      "  [f32] f{atanh(0.0f32)}\n"
      "  return(convert<int>(plus(e, f)))\n"
      "}\n";

  auto program = parseProgram(source);
  primec::Semantics semantics;
  primec::SemanticProgram semanticProgram;
  std::string error;
  const std::vector<std::string> defaults = {"io_out", "io_err"};
  REQUIRE(semantics.validate(program, "/main", error, defaults, defaults, {}, nullptr, false, &semanticProgram));
  CHECK(error.empty());

  const auto *queryEntry = findSemanticEntry(
      primec::semanticProgramQueryFactView(semanticProgram),
      [&semanticProgram](const primec::SemanticProgramQueryFact &entry) {
        return primec::semanticProgramResolveCallTargetString(semanticProgram, entry.scopePathId) == "/main" &&
               primec::semanticProgramResolveCallTargetString(semanticProgram, entry.callNameId) == "plus";
      });
  REQUIRE(queryEntry != nullptr);
  CHECK(queryEntry->queryTypeText == "f32");
  CHECK(queryEntry->bindingTypeText == "f32");
}

TEST_CASE("semantic product query facts include sum pick call targets") {
  const std::string source = R"(
[sum]
Choice {
  [i32] left
  [i32] right
}

[struct]
Picker {
  [i32] seed
}

[return<Choice>]
makeChoice() {
  [Choice] choice{[right] 41i32}
  return(choice)
}

[return<Choice>]
/Picker/makeChoice([Picker] self) {
  [Choice] choice{[right] self.seed}
  return(choice)
}

[return<int>]
main() {
  [Picker] picker{Picker{7i32}}
  [i32] first{pick(makeChoice()) {
    left(value) {
      plus(value, 1i32)
    }
    right(value) {
      plus(value, 2i32)
    }
  }}
  return(pick(picker.makeChoice()) {
    left(value) {
      plus(first, value)
    }
    right(value) {
      plus(first, value)
    }
  })
}
)";

  auto program = parseProgram(source);
  primec::Semantics semantics;
  primec::SemanticProgram semanticProgram;
  std::string error;
  const std::vector<std::string> defaults = {"io_out", "io_err"};
  REQUIRE(semantics.validate(program, "/main", error, defaults, defaults, {}, nullptr, false, &semanticProgram));
  CHECK(error.empty());

  const auto queryFacts = primec::semanticProgramQueryFactView(semanticProgram);
  const auto *directPickTarget = findSemanticEntry(
      queryFacts,
      [&semanticProgram](const primec::SemanticProgramQueryFact &entry) {
        return primec::semanticProgramResolveCallTargetString(semanticProgram, entry.scopePathId) == "/main" &&
               primec::semanticProgramResolveCallTargetString(semanticProgram, entry.callNameId) == "makeChoice" &&
               primec::semanticProgramQueryFactResolvedPath(
                   semanticProgram, entry) == "/makeChoice";
      });
  REQUIRE(directPickTarget != nullptr);
  CHECK(directPickTarget->bindingTypeText == "/Choice");

  const auto *methodPickTarget = findSemanticEntry(
      queryFacts,
      [&semanticProgram](const primec::SemanticProgramQueryFact &entry) {
        return primec::semanticProgramResolveCallTargetString(semanticProgram, entry.scopePathId) == "/main" &&
               primec::semanticProgramResolveCallTargetString(semanticProgram, entry.callNameId) == "makeChoice" &&
               primec::semanticProgramQueryFactResolvedPath(
                   semanticProgram, entry) == "/Picker/makeChoice";
      });
  REQUIRE(methodPickTarget != nullptr);
  CHECK(methodPickTarget->bindingTypeText == "/Choice");
}

TEST_CASE("semantic product query facts carry interned text ids") {
  const std::string source =
      "[return<i32>]\n"
      "main() {\n"
      "  [i32] a{plus(1i32, 2i32)}\n"
      "  [i32] b{plus(3i32, 4i32)}\n"
      "  return(plus(a, b))\n"
      "}\n";

  auto program = parseProgram(source);
  primec::Semantics semantics;
  primec::SemanticProgram semanticProgram;
  std::string error;
  const std::vector<std::string> defaults = {"io_out", "io_err"};
  REQUIRE(semantics.validate(program, "/main", error, defaults, defaults, {}, nullptr, false, &semanticProgram));
  CHECK(error.empty());

  std::vector<const primec::SemanticProgramQueryFact *> plusQueryFacts;
  for (const auto *entry : primec::semanticProgramQueryFactView(semanticProgram)) {
    if (primec::semanticProgramResolveCallTargetString(semanticProgram, entry->scopePathId) == "/main" &&
        primec::semanticProgramResolveCallTargetString(semanticProgram, entry->callNameId) == "plus" &&
        entry->queryTypeText == "i32") {
      plusQueryFacts.push_back(entry);
    }
  }
  REQUIRE(plusQueryFacts.size() >= 2);
  REQUIRE(plusQueryFacts[0]->scopePathId != primec::InvalidSymbolId);
  REQUIRE(plusQueryFacts[0]->callNameId != primec::InvalidSymbolId);
  REQUIRE(plusQueryFacts[0]->queryTypeTextId != primec::InvalidSymbolId);
  CHECK(plusQueryFacts[0]->scopePathId == plusQueryFacts[1]->scopePathId);
  CHECK(plusQueryFacts[0]->callNameId == plusQueryFacts[1]->callNameId);
  CHECK(plusQueryFacts[0]->queryTypeTextId == plusQueryFacts[1]->queryTypeTextId);
  CHECK(primec::semanticProgramResolveCallTargetString(semanticProgram, plusQueryFacts[0]->scopePathId) ==
        "/main");
  CHECK(primec::semanticProgramResolveCallTargetString(semanticProgram, plusQueryFacts[0]->callNameId) ==
        "plus");
  CHECK(primec::semanticProgramResolveCallTargetString(semanticProgram, plusQueryFacts[0]->queryTypeTextId) ==
        "i32");
}

TEST_CASE("semantic product binding facts carry interned text ids") {
  const std::string source =
      "[return<i32>]\n"
      "main() {\n"
      "  [i32] a{1i32}\n"
      "  [i32] b{2i32}\n"
      "  return(plus(a, b))\n"
      "}\n";

  auto program = parseProgram(source);
  primec::Semantics semantics;
  primec::SemanticProgram semanticProgram;
  std::string error;
  const std::vector<std::string> defaults = {"io_out", "io_err"};
  REQUIRE(semantics.validate(program, "/main", error, defaults, defaults, {}, nullptr, false, &semanticProgram));
  CHECK(error.empty());

  const auto *aEntry = findSemanticEntry(
      primec::semanticProgramBindingFactView(semanticProgram),
      [&](const primec::SemanticProgramBindingFact &entry) {
        return semanticTextOrFallback(semanticProgram, entry.scopePathId, entry.scopePath) == "/main" &&
               semanticTextOrFallback(semanticProgram, entry.siteKindId, entry.siteKind) == "local" &&
               semanticTextOrFallback(semanticProgram, entry.nameId, entry.name) == "a";
      });
  const auto *bEntry = findSemanticEntry(
      primec::semanticProgramBindingFactView(semanticProgram),
      [&](const primec::SemanticProgramBindingFact &entry) {
        return semanticTextOrFallback(semanticProgram, entry.scopePathId, entry.scopePath) == "/main" &&
               semanticTextOrFallback(semanticProgram, entry.siteKindId, entry.siteKind) == "local" &&
               semanticTextOrFallback(semanticProgram, entry.nameId, entry.name) == "b";
      });
  REQUIRE(aEntry != nullptr);
  REQUIRE(bEntry != nullptr);
  REQUIRE(aEntry->scopePathId != primec::InvalidSymbolId);
  REQUIRE(aEntry->siteKindId != primec::InvalidSymbolId);
  REQUIRE(aEntry->nameId != primec::InvalidSymbolId);
  REQUIRE(aEntry->bindingTypeTextId != primec::InvalidSymbolId);
  CHECK(aEntry->referenceRootId == primec::InvalidSymbolId);
  CHECK(aEntry->scopePathId == bEntry->scopePathId);
  CHECK(aEntry->siteKindId == bEntry->siteKindId);
  CHECK(aEntry->bindingTypeTextId == bEntry->bindingTypeTextId);
  CHECK(aEntry->nameId != bEntry->nameId);
  const std::string_view aResolvedPath =
      primec::semanticProgramBindingFactResolvedPath(semanticProgram, *aEntry);
  if (aResolvedPath.empty()) {
    CHECK(aEntry->resolvedPathId == primec::InvalidSymbolId);
  } else {
    REQUIRE(aEntry->resolvedPathId != primec::InvalidSymbolId);
    CHECK(primec::semanticProgramResolveCallTargetString(semanticProgram, aEntry->resolvedPathId) ==
          aResolvedPath);
  }
  const std::string_view bResolvedPath =
      primec::semanticProgramBindingFactResolvedPath(semanticProgram, *bEntry);
  if (bResolvedPath.empty()) {
    CHECK(bEntry->resolvedPathId == primec::InvalidSymbolId);
  } else {
    REQUIRE(bEntry->resolvedPathId != primec::InvalidSymbolId);
    CHECK(primec::semanticProgramResolveCallTargetString(semanticProgram, bEntry->resolvedPathId) ==
          bResolvedPath);
  }
  CHECK(primec::semanticProgramResolveCallTargetString(semanticProgram, aEntry->scopePathId) == "/main");
  CHECK(primec::semanticProgramResolveCallTargetString(semanticProgram, aEntry->siteKindId) == "local");
  CHECK(primec::semanticProgramResolveCallTargetString(semanticProgram, aEntry->bindingTypeTextId) == "i32");
}

TEST_CASE("semantic product binding facts include sum typed locals") {
  const std::string source = R"(
[sum]
Choice {
  [i32] left
  [i32] right
}

[return<i32>]
main() {
  [Choice] choice{[right] 41i32}
  return(pick(choice) {
    left(value) {
      plus(value, 1i32)
    }
    right(value) {
      plus(value, 2i32)
    }
  })
}
)";

  auto program = parseProgram(source);
  primec::Semantics semantics;
  primec::SemanticProgram semanticProgram;
  std::string error;
  const std::vector<std::string> defaults = {"io_out", "io_err"};
  REQUIRE(semantics.validate(program, "/main", error, defaults, defaults, {}, nullptr, false, &semanticProgram));
  CHECK(error.empty());

  const auto *choiceEntry = findSemanticEntry(
      primec::semanticProgramBindingFactView(semanticProgram),
      [&](const primec::SemanticProgramBindingFact &entry) {
        return semanticTextOrFallback(semanticProgram, entry.scopePathId, entry.scopePath) == "/main" &&
               semanticTextOrFallback(semanticProgram, entry.siteKindId, entry.siteKind) == "local" &&
               semanticTextOrFallback(semanticProgram, entry.nameId, entry.name) == "choice";
      });
  REQUIRE(choiceEntry != nullptr);
  CHECK(choiceEntry->bindingTypeText == "Choice");
  CHECK(choiceEntry->semanticNodeId != 0);
  REQUIRE(choiceEntry->bindingTypeTextId != primec::InvalidSymbolId);
  REQUIRE(choiceEntry->resolvedPathId != primec::InvalidSymbolId);
  CHECK(primec::semanticProgramResolveCallTargetString(semanticProgram, choiceEntry->bindingTypeTextId) ==
        "Choice");
  CHECK(primec::semanticProgramBindingFactResolvedPath(semanticProgram, *choiceEntry) ==
        "/main/choice");
}

TEST_SUITE_END();
