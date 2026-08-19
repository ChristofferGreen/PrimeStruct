#include "../test_semantics_helpers.h"

TEST_SUITE_BEGIN("primestruct.semantics.calls_flow.collections");

TEST_CASE("experimental soa inline location read-only methods reject rooted helper path") {
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
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.empty());
}

TEST_CASE("experimental soa inline location borrowed helper-return helper surfaces reject current ref_ref path") {
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
  [SoaVector<Particle>] values{
      soaVectorSingle<Particle>(Particle(7i32, 8i32))}
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
  return(plus(plus(firstA.x, secondA.x),
              plus(plus(firstC.x, secondC.y),
                   plus(count(unpackedA),
                        plus(countA,
                             plus(plus(firstB.x, secondB.x),
                                  plus(plus(firstD.x, secondD.y),
                                       plus(count(unpackedB), countB))))))))
}
  )";
  std::string error;
  // TODO-5050 shape (a) (RESOLVED): get/ref/to_aos/count all now route
  // correctly on this doubly-borrowed helper-return receiver, so the
  // program validates. (A standalone `--emit=vm` probe shows it then
  // crashes at runtime with "array index out of bounds" - the fixture's
  // single-element vector doesn't have the index-1 elements various
  // `.ref(1i32)`/`get(..., 1i32)` calls read, unrelated to this TODO's
  // routing gaps.)
  CHECK(validateProgram(source, "/main", error));
  INFO(error);
}

TEST_CASE("experimental soa borrowed helper-return helper surfaces reject current ref_ref path") {
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

[effects(heap_alloc), return<int>]
main() {
  [SoaVector<Particle>] values{soaVectorSingle<Particle>(Particle(7i32))}
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
  std::string error;
  // TODO-5050 shape (a) (RESOLVED): get/ref/to_aos/count all now route
  // correctly on this borrowed helper-return receiver, so the program
  // validates. (A standalone `--emit=vm` probe shows it then crashes at
  // runtime with "array index out of bounds" - the fixture's single-
  // element vector doesn't have the index-1 elements `.ref(1i32)`/
  // `get(..., 1i32)` read, unrelated to this TODO's routing gaps.)
  CHECK(validateProgram(source, "/main", error));
  INFO(error);
}

TEST_CASE("experimental soa alias-only borrowed helper-return helpers resolve current ref_ref path") {
  const std::string source = R"(
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

[effects(heap_alloc), return<int>]
main() {
  [SoaVector<Particle>] values{soaVectorSingle<Particle>(Particle(7i32))}
  [Particle] first{get(pickBorrowed(location(values)), 1i32)}
  [Reference<Particle>] second{ref(pickBorrowed(location(values)), 0i32)}
  [i32] countBare{count(pickBorrowed(location(values)))}
  return(plus(plus(first.x, second.x), countBare))
}
  )";
  std::string error;
  // TODO-5050 shape (a) (RESOLVED): canonical public soa read-helper
  // routing now works on this helper-return borrowed receiver; get/ref/
  // count all resolve correctly.
  CHECK(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.empty());
}

TEST_CASE("experimental soa method-like borrowed helper-return helper surfaces resolve current ref_ref path") {
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
  [SoaVector<Particle>] values{
      soaVectorSingle<Particle>(Particle(7i32, 8i32))}
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
  std::string error;
  // TODO-5050 shape (a) (RESOLVED): same fix class as the free-function
  // variant above, for the struct-method receiver form - get/ref/count/
  // field-access-sugar variants all resolve correctly now.
  CHECK(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.empty());
}

TEST_CASE("experimental soa direct return method-like borrowed helper-return reads reject retired helper path") {
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
  [SoaVector<Particle>] values{
      soaVectorSingle<Particle>(Particle(7i32, 8i32))}
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
  std::string error;
  // TODO-5050 shape (a) + to_aos_ref gap (RESOLVED): count/to_aos/get/ref/
  // field-view accessors all now resolve on this borrowed helper-return
  // receiver, so the program validates. (A standalone `--emit=vm` probe
  // shows it then crashes at runtime with "array index out of bounds" -
  // the fixture's single-element vector doesn't have the index-1 elements
  // `.ref(...)`/`get(..., 1i32)` read, unrelated to this TODO's routing
  // gaps.)
  CHECK(validateProgram(source, "/main", error));
  INFO(error);
}

TEST_CASE("experimental soa direct helper shadows validate without duplicate reserve diagnostics") {
  const std::string source = R"(
import /std/collections/soa/*

[struct reflect]
Particle() {
  [i32] x{1i32}
}

[return<SoaVector<Particle>>]
cloneValues() {
  return(SoaVector<Particle>{})
}

[return<Particle>]
/soa/get([SoaVector<Particle>] values, [i32] index) {
  return(Particle(index))
}

[return<Particle>]
/soa/ref([SoaVector<Particle>] values, [i32] index) {
  return(Particle(index))
}

[return<vector<Particle>>]
/to_aos([SoaVector<Particle>] values) {
  return(vector<Particle>())
}

[return<i32>]
/soa/push([SoaVector<Particle>] values, [Particle] value) {
  return(value.x)
}

[return<i32>]
/soa/reserve([SoaVector<Particle>] values, [i32] count) {
  return(count)
}

[return<Particle>]
/shadow/soa/get([SoaVector<Particle>] values, [i32] index) {
  return(Particle(plus(index, 100i32)))
}

[return<Particle>]
/shadow/soa/ref([SoaVector<Particle>] values, [i32] index) {
  return(Particle(plus(index, 100i32)))
}

[return<vector<Particle>>]
/shadow/soa/to_aos([SoaVector<Particle>] values) {
  return(vector<Particle>())
}

[return<i32>]
/shadow/soa/push([SoaVector<Particle>] values, [Particle] value) {
  return(plus(value.x, 100i32))
}

[return<i32>]
/shadow/soa/reserve([SoaVector<Particle>] values, [i32] count) {
  return(plus(count, 100i32))
}

[return<Particle>]
/shadow/soa/SoaVector__Particle/get([SoaVector<Particle>] values,
                                                                  [i32] index) {
  return(Particle(plus(index, 200i32)))
}

[return<Particle>]
/shadow/soa/SoaVector__Particle/ref([SoaVector<Particle>] values,
                                                                  [i32] index) {
  return(Particle(plus(index, 200i32)))
}

[return<vector<Particle>>]
/shadow/soa/SoaVector__Particle/to_aos([SoaVector<Particle>] values) {
  return(vector<Particle>())
}

[return<void>]
/shadow/soa/SoaVector__Particle/push([SoaVector<Particle>] values,
                                                                   [Particle] value) {
}

[return<void>]
/shadow/soa/SoaVector__Particle/reserve([SoaVector<Particle>] values,
                                                                      [i32] count) {
}

[effects(heap_alloc), return<int>]
main() {
  [Particle] picked{/soa/get(cloneValues(), 1i32)}
  [Particle] pickedRef{/soa/ref(cloneValues(), 1i32)}
  [vector<Particle>] unpacked{/to_aos(cloneValues())}
  [i32] pushed{/soa/push(cloneValues(), Particle(7i32))}
  [i32] reserved{/soa/reserve(cloneValues(), 4i32)}
  return(plus(plus(picked.x, pickedRef.x), plus(pushed, reserved)))
}
  )";
  std::string error;
  INFO(error);
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("experimental soa inline location method-like borrowed helper-return helper surfaces reject current ref_ref path") {
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
  [SoaVector<Particle>] values{
      soaVectorSingle<Particle>(Particle(7i32, 8i32))}
  [Holder] holder{Holder{}}
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
  std::string error;
  // TODO-5050 shape (a) + to_aos_ref gap (RESOLVED): get/ref/count/to_aos/
  // field-view accessors all now resolve on this borrowed helper-return
  // receiver, so the program validates. (A standalone `--emit=vm` probe
  // shows it then crashes at runtime with "array index out of bounds" -
  // the fixture's single-element vector doesn't have the index-1 elements
  // various `.ref(1i32)`/`get(..., 1i32)` calls read, unrelated to this
  // TODO's routing gaps.)
  CHECK(validateProgram(source, "/main", error));
  INFO(error);
}

TEST_CASE("experimental soa direct return inline location method-like borrowed") {
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
  [SoaVector<Particle>] values{
      soaVectorSingle<Particle>(Particle(7i32, 8i32))}
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
  std::string error;
  // TODO-5050 shape (a) + to_aos_ref gap (RESOLVED): count/to_aos/get/ref/
  // field-view accessors all now resolve on this borrowed helper-return
  // receiver, so the program validates. (A standalone `--emit=vm` probe
  // shows it then crashes at runtime with "array index out of bounds" -
  // the fixture's single-element vector doesn't have the index-1 elements
  // `.get(1i32)`/`get(..., 1i32)` read, unrelated to this TODO's routing
  // gaps.)
  CHECK(validateProgram(source, "/main", error));
  INFO(error);
}

TEST_CASE("experimental soa stdlib get helper accepts direct soa wildcard import") {
  const std::string source = R"(
import /std/collections/soa/*

[struct reflect]
Particle() {
  [i32] x{1i32}
}

[effects(heap_alloc), return<Particle>]
main() {
  [SoaVector<Particle>] values{soaVectorNew<Particle>()}
  return(soaVectorGet<Particle>(values, 0i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("experimental soa stdlib get method reports current helper path") {
  const std::string source = R"(
import /std/collections/soa/*

[struct reflect]
Particle() {
  [i32] x{1i32}
}

[effects(heap_alloc), return<Particle>]
main() {
  [SoaVector<Particle>] values{soaVectorNew<Particle>()}
  return(values.get(0i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.empty());
}

TEST_CASE("experimental soa stdlib ref helper accepts direct soa wildcard import") {
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
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("experimental soa stdlib ref method reports current dereference diagnostic") {
  const std::string source = R"(
import /std/collections/soa/*

[struct reflect]
Particle() {
  [i32] x{1i32}
}

[effects(heap_alloc), return<Particle>]
main() {
  [SoaVector<Particle>] values{soaVectorSingle<Particle>(Particle(7i32))}
  return(dereference(values.ref(0i32)))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.find("dereference requires a pointer or reference") != std::string::npos);
}

TEST_CASE("experimental soa stdlib ref method pass-through reports current helper path") {
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
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.empty());
}

TEST_CASE("experimental soa standalone ref method reports current binding diagnostic") {
  const std::string source = R"(
import /std/collections/soa/*

[struct reflect]
Particle() {
  [i32] x{1i32}
}

[effects(heap_alloc), return<int>]
main() {
  [SoaVector<Particle> mut] values{soaVectorSingle<Particle>(Particle(7i32))}
  [Reference<Particle>] value{values.ref(0i32)}
  values.push(Particle(9i32))
  return(values.count())
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.empty());
}

TEST_CASE("experimental soa standalone ref method push conflict reports current binding diagnostic") {
  const std::string source = R"(
import /std/collections/soa/*

[struct reflect]
Particle() {
  [i32] x{1i32}
}

[effects(heap_alloc), return<int>]
main() {
  [SoaVector<Particle> mut] values{soaVectorSingle<Particle>(Particle(7i32))}
  [Reference<Particle>] value{values.ref(0i32)}
  values.push(Particle(9i32))
  return(value.x)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  INFO(error);
  // Fixed dispatch reaches the borrow checker, which correctly rejects push while a ref borrow is live.
  CHECK(error.find("borrowed binding: values") != std::string::npos);
}

TEST_CASE("experimental soa standalone ref helper reserve conflict accepts direct soa wildcard import") {
  const std::string source = R"(
import /std/collections/soa/*

[struct reflect]
Particle() {
  [i32] x{1i32}
}

[effects(heap_alloc), return<int>]
main() {
  [SoaVector<Particle> mut] values{soaVectorSingle<Particle>(Particle(7i32))}
  [Reference<Particle>] value{soaVectorRef<Particle>(values, 0i32)}
  soaVectorReserve<Particle>(values, 3i32)
  return(value.x)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("experimental soa helper-return ref method validates direct soa wildcard import") {
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
  [SoaVector<Particle> mut] values{soaVectorSingle<Particle>(Particle(7i32))}
  [Reference<Particle>] value{pick(values)}
  values.push(Particle(9i32))
  return(values.count())
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.empty());
}

TEST_CASE("experimental soa helper-return ref method push conflict validates direct soa wildcard import") {
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
  [SoaVector<Particle> mut] values{soaVectorSingle<Particle>(Particle(7i32))}
  [Reference<Particle>] value{pick(values)}
  values.push(Particle(9i32))
  return(value.x)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  INFO(error);
  // Fixed dispatch reaches the borrow checker, which correctly rejects push while a ref borrow is live.
  CHECK(error.find("borrowed binding: values") != std::string::npos);
}

TEST_CASE("experimental soa borrowed helper-return ref method validates direct soa wildcard import") {
  const std::string source = R"(
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
  [SoaVector<Particle>] values{soaVectorSingle<Particle>(Particle(7i32))}
  [Reference<Particle>] value{pickBorrowed(location(values)).ref(0i32)}
  soaVectorReserve<Particle>(values, 3i32)
  return(value.x)
}
)";
  std::string error;
  // TODO-5050 shape (a) (RESOLVED): .ref(...) on this borrowed helper-
  // return receiver now resolves correctly. The bare soaVectorReserve<T>(...)
  // call form isn't currently tracked by the borrow checker (unlike
  // .push()), so full validation now succeeds.
  CHECK(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.empty());
}

TEST_CASE("experimental soa inline location borrowed helper-return ref validates direct soa wildcard import") {
  const std::string source = R"(
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
  [SoaVector<Particle> mut] values{
      soaVectorSingle<Particle>(Particle(7i32))}
  [Reference<Particle>] value{location(pickBorrowed(location(values))).ref(0i32)}
  values.push(Particle(11i32))
  return(value.x)
}
)";
  std::string error;
  // TODO-5050 shape (a) (RESOLVED): .ref(...) on this borrowed helper-
  // return receiver now resolves correctly, so validation reaches the
  // borrow checker, which correctly rejects the .push() call while the
  // `value` reference is still live.
  CHECK_FALSE(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.find("borrowed binding: values") != std::string::npos);
}

TEST_CASE("experimental soa stdlib push and reserve helpers validate through soa import") {
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
  return(soaVectorGet<Particle>(values, 1i32).x)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("experimental soa stdlib push and reserve methods reject retired method paths") {
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
  return(values.get(1i32).x)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.empty());
}

TEST_CASE("experimental soa stdlib single-field index syntax reports field_count diagnostic") {
  const std::string source = R"(
import /std/collections/soa/*

[struct reflect]
ScalarBox() {
  [i32] x{1i32}
}

[effects(heap_alloc), return<int>]
main() {
  [SoaVector<ScalarBox> mut] values{soaVectorNew<ScalarBox>()}
  push(values, ScalarBox(4i32))
  push(values, ScalarBox(9i32))
  return(values.x()[1i32])
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.empty());
}

TEST_CASE("experimental soa stdlib field-view method reports escape diagnostic") {
  const std::string source = R"(
import /std/collections/soa/*

[struct reflect]
Particle() {
  [i32] x{1i32}
}

[effects(heap_alloc), return<int>]
main() {
  [SoaVector<Particle>] values{soaVectorSingle<Particle>(Particle(7i32))}
  return(values.x())
}
  )";
  std::string error;
  CHECK(!validateProgram(source, "/main", error));
  CHECK(error.find("field-view escapes via return") != std::string::npos);
}

TEST_CASE("experimental soa borrowed local field-view method reports escape diagnostic") {
  const std::string source = R"(
import /std/collections/soa/*

[struct reflect]
Particle() {
  [i32] x{1i32}
}

[effects(heap_alloc), return<int>]
main() {
  [SoaVector<Particle> mut] values{soaVectorNew<Particle>()}
  [Reference<SoaVector<Particle>>] borrowed{location(values)}
  return(borrowed.x())
}
  )";
  std::string error;
  CHECK(!validateProgram(source, "/main", error));
  CHECK(error.find("field-view escapes via return") != std::string::npos);
}

TEST_CASE("experimental soa borrowed local field-view call-form validates") {
  const std::string source = R"(
import /std/collections/soa/*

[struct reflect]
Particle() {
  [i32] x{1i32}
}

[effects(heap_alloc), return<int>]
main() {
  [SoaVector<Particle> mut] values{soaVectorNew<Particle>()}
  [Reference<SoaVector<Particle>>] borrowed{location(values)}
  x(borrowed)
  x(dereference(borrowed))
  return(0i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("experimental soa inline location field-view methods report field_count diagnostic") {
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
  push(values, Particle(7i32, 8i32))
  location(values).x()
  x(location(values))
  x(dereference(location(values)))
  return(0i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.empty());
}

TEST_CASE("experimental soa inline location borrowed helper-return field-view methods report field_count diagnostic") {
  const std::string source = R"(
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
  push(values, Particle(7i32))
  location(pickBorrowed(location(values))).x()
  x(location(pickBorrowed(location(values))))
  x(dereference(location(pickBorrowed(location(values)))))
  return(0i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.empty());
}

TEST_CASE("experimental soa borrowed helper-return field-view call-form validates") {
  const std::string source = R"(
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
  x(pickBorrowed(location(values)))
  x(dereference(pickBorrowed(location(values))))
  return(0i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("experimental soa method-like borrowed helper-return field-view methods validate") {
  const std::string source = R"(
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

[effects(heap_alloc), return<int>]
main() {
  [SoaVector<Particle> mut] values{soaVectorNew<Particle>()}
  [Holder] holder{Holder{}}
  holder.pickBorrowed(location(values)).x()
  x(holder.pickBorrowed(location(values)))
  location(holder.pickBorrowed(location(values))).x()
  x(location(holder.pickBorrowed(location(values))))
  return(0i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("experimental soa mutating field-view index reports field_count diagnostic") {
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
  push(values, Particle(7i32, 8i32))
  push(values, Particle(9i32, 12i32))
  assign(values.y()[1i32], 17i32)
  return(values.y()[1i32])
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.empty());
}

TEST_CASE("experimental soa mutating field-view call-form index reports field_count diagnostic") {
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
  push(values, Particle(7i32, 8i32))
  push(values, Particle(9i32, 12i32))
  assign(y(values)[1i32], 17i32)
  return(y(values)[1i32])
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.empty());
}

TEST_CASE("experimental soa mutating field-view method reports pending diagnostic") {
  const std::string source = R"(
import /std/collections/soa/*

[struct reflect]
Particle() {
  [i32] x{1i32}
}

[effects(heap_alloc), return<int>]
main() {
  [SoaVector<Particle> mut] values{soaVectorNew<Particle>()}
  assign(values.x(), 17i32)
  return(0i32)
}
)";
  std::string error;
  CHECK(!validateProgram(source, "/main", error));
}

TEST_CASE("experimental soa mutating field-view call-form reports pending diagnostic") {
  const std::string source = R"(
import /std/collections/soa/*

[struct reflect]
Particle() {
  [i32] x{1i32}
}

[effects(heap_alloc), return<int>]
main() {
  [SoaVector<Particle> mut] values{soaVectorNew<Particle>()}
  assign(x(values), 17i32)
  return(0i32)
}
)";
  std::string error;
  CHECK(!validateProgram(source, "/main", error));
}

TEST_CASE("experimental soa mutating dereferenced borrowed helper-return field-view index reports field_count diagnostic") {
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
  push(values, Particle(7i32, 8i32))
  push(values, Particle(9i32, 12i32))
  assign(dereference(pickBorrowed(location(values))).y()[1i32], 17i32)
  return(dereference(pickBorrowed(location(values))).y()[1i32])
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.empty());
}

TEST_CASE("experimental soa mutating dereferenced borrowed helper-return field-view method reports pending diagnostic") {
  const std::string source = R"(
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
  assign(dereference(pickBorrowed(location(values))).x(), 17i32)
  return(0i32)
}
)";
  std::string error;
  CHECK(!validateProgram(source, "/main", error));
}

TEST_CASE("experimental soa mutating inline location borrowed helper-return field-view indexes validate") {
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
  push(values, Particle(7i32, 8i32))
  assign(location(pickBorrowed(location(values))).y()[0i32], 17i32)
  assign(y(location(pickBorrowed(location(values))))[0i32], 17i32)
  return(plus(location(pickBorrowed(location(values))).y()[0i32],
              y(location(pickBorrowed(location(values))))[0i32]))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.empty());
}

TEST_CASE("experimental soa mutating method-like borrowed helper-return field-view indexes validate") {
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
  push(values, Particle(7i32, 8i32))
  push(values, Particle(9i32, 12i32))
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
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.empty());
}

TEST_CASE("experimental soa mutating inline location borrowed helper-return field-view methods report pending diagnostic") {
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
  push(values, Particle(7i32, 8i32))
  assign(location(pickBorrowed(location(values))).x(), 17i32)
  assign(dereference(location(pickBorrowed(location(values)))).x(), 17i32)
  return(0i32)
}
)";
  std::string error;
  CHECK(!validateProgram(source, "/main", error));
}

TEST_CASE("experimental soa mutating method-like borrowed helper-return field-view methods report pending diagnostic") {
  const std::string source = R"(
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

[effects(heap_alloc), return<int>]
main() {
  [SoaVector<Particle> mut] values{soaVectorNew<Particle>()}
  [Holder] holder{Holder{}}
  assign(holder.pickBorrowed(location(values)).x(), 17i32)
  assign(x(holder.pickBorrowed(location(values))), 17i32)
  assign(location(holder.pickBorrowed(location(values))).x(), 17i32)
  assign(x(location(holder.pickBorrowed(location(values)))), 17i32)
  return(0i32)
}
)";
  std::string error;
  CHECK(!validateProgram(source, "/main", error));
  CHECK(error.find("unknown method") !=
        std::string::npos);
}

TEST_CASE("experimental soa mutating ref field access validates internal metadata validation") {
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
  assign(ref(values, 0i32).y, 19i32)
  assign(values.ref(1i32).y, 17i32)
  return(plus(ref(values, 0i32).y, values.ref(1i32).y))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.empty());
}

TEST_CASE("experimental soa bare get and ref field access validates internal metadata validation") {
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
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.empty());
}

TEST_CASE("experimental soa stdlib reflected multi-field index syntax reports field_count diagnostic") {
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
  push(values, Particle(7i32, 8i32))
  push(values, Particle(9i32, 12i32))
  return(values.y()[1i32])
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.empty());
}

TEST_CASE("experimental soa stdlib reflected call-form index syntax reports field_count diagnostic") {
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
  push(values, Particle(7i32, 8i32))
  push(values, Particle(9i32, 12i32))
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
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  INFO(error);
  CHECK(error.empty());
}


TEST_SUITE_END();
