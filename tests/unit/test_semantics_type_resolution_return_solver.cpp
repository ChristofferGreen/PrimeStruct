#include "third_party/doctest.h"

#include "test_semantics_helpers.h"

TEST_SUITE_BEGIN("primestruct.semantics.type_resolution_return_solver");

TEST_CASE("graph type resolver infers acyclic helper return kinds") {
  const std::string source = R"(
[return<auto>]
leaf() {
  return(1i32)
}

[return<auto>]
wrap() {
  return(leaf())
}

[return<i32>]
main() {
  return(wrap())
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("graph type resolver reports self-recursive return inference cycles explicitly") {
  const std::string source = R"(
[return<auto>]
main() {
  return(main())
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error == "return type inference cycle requires explicit annotation on /main");
}

TEST_CASE("graph type resolver reports mutual recursion cycle members in deterministic order") {
  const std::string source = R"(
[return<auto>]
alpha() {
  return(beta())
}

[return<auto>]
beta() {
  return(alpha())
}

[return<i32>]
main() {
  return(alpha())
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error == "return type inference cycle requires explicit annotations on /alpha, /beta");
}

TEST_CASE("graph type resolver preserves unresolved acyclic diagnostic") {
  const std::string source = R"(
[return<auto>]
main() {
  [i32] value{1i32}
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unable to infer return type on /main") != std::string::npos);
}

TEST_CASE("graph type resolver preserves conflicting return diagnostic") {
  const std::string source = R"(
[return<auto>]
main() {
  if(true,
    then(){ return(1i32) },
    else(){ return(1.5f) })
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("conflicting return types on /main") != std::string::npos);
}

TEST_CASE("graph type resolver preserves conflicting collection template return diagnostic") {
  const std::string source = R"(
[return<auto>]
pick([bool] cond) {
  if(cond,
    then(){ return(map<i32, i32>(1i32, 2i32)) },
    else(){ return(map<i32, bool>(1i32, true)) })
}

[return<i32>]
main() {
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("conflicting return types on /pick") != std::string::npos);
}

TEST_CASE("graph type resolver infers direct-call auto binding from struct-return helper") {
  const std::string source = R"(
[struct]
Pair() {
  [i32] value{7i32}
}

[return<Pair>]
makePair() {
  return(Pair())
}

[return<i32>]
main() {
  [auto] pair{makePair()}
  return(pair.value)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("graph type resolver infers direct-call auto binding from auto map-return helper") {
  const std::string source = R"(
[return<auto>]
makeValues() {
  return(map<i32, i32>(1i32, 7i32, 2i32, 11i32))
}

[return<i32>]
main() {
  [auto] values{makeValues()}
  return(at_unsafe(values, 2i32))
}
  )";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK_FALSE(error.empty());
}

TEST_CASE("graph type resolver infers direct-call auto binding from collection-return helper") {
  const std::string source = R"(
[return<array<i32>>]
makeValues() {
  return(array<i32>(1i32, 2i32, 3i32))
}

[return<i32>]
main() {
  [auto] values{makeValues()}
  return(count(values))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("graph type resolver infers block-valued auto binding from helper-returned struct") {
  const std::string source = R"(
[struct]
Pair() {
  [i32] value{7i32}
}

[return<Pair>]
makePair() {
  return(Pair())
}

[return<i32>]
main() {
  [auto] pair{
    block {
      return(makePair())
    }
  }
  return(pair.value)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("graph type resolver infers control-flow auto binding from helper-returned collection") {
  const std::string source = R"(
[return<array<i32>>]
valuesA() {
  return(array<i32>(1i32, 2i32))
}

[return<array<i32>>]
valuesB() {
  return(array<i32>(3i32, 4i32, 5i32))
}

[return<i32>]
main() {
  [auto] values{
    if(true,
      then(){ return(valuesA()) },
      else(){ return(valuesB()) })
  }
  return(count(values))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("graph type resolver infers if-join return from branch-local auto collection values") {
  const std::string source = R"(
[return<array<i32>>]
valuesA() {
  return(array<i32>(1i32, 2i32))
}

[return<array<i32>>]
valuesB() {
  return(array<i32>(3i32, 4i32))
}

[return<auto>]
wrapValues() {
  return(if(true,
    then(){
      [auto] left{valuesA()}
      return(left)
    },
    else(){
      [auto] right{valuesB()}
      return(right)
    }))
}

[return<i32>]
main() {
  return(count(wrapValues()))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("graph type resolver infers omitted struct field envelope from block-valued helper") {
  const std::string source = R"(
[struct]
Vec3() {
  [i32] x{7i32}

  [return<i32>]
  getX() {
    return(this.x)
  }
}

[return<Vec3>]
makeCenter() {
  return(Vec3())
}

[struct]
Sphere() {
  center{
    block {
      return(makeCenter())
    }
  }
}

[return<i32>]
main() {
  [Sphere] shape{Sphere()}
  return(shape.center.getX())
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("graph type resolver infers omitted struct field envelope from control-flow helper") {
  const std::string source = R"(
[struct]
Vec3() {
  [i32] x{7i32}

  [return<i32>]
  getX() {
    return(this.x)
  }
}

[return<Vec3>]
leftCenter() {
  return(Vec3())
}

[return<Vec3>]
rightCenter() {
  return(Vec3())
}

[struct]
Sphere() {
  center{
    if(true,
      then(){ return(leftCenter()) },
      else(){ return(rightCenter()) })
  }
}

[return<i32>]
main() {
  [Sphere] shape{Sphere()}
  return(shape.center.getX())
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("graph type resolver preserves ambiguous omitted struct field envelope diagnostic") {
  const std::string source = R"(
[struct]
Vec2() {
  [i32] x{1i32}
}

[struct]
Vec3() {
  [i32] x{2i32}
}

[struct]
Shape() {
  center{
    if(true,
      then(){ return(Vec2()) },
      else(){ return(Vec3()) })
  }
}

[return<i32>]
main() {
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unresolved or ambiguous omitted struct field envelope: /Shape/center") !=
        std::string::npos);
}

TEST_CASE("graph type resolver answers collection receiver queries through shared return-binding inference") {
  const std::string source = R"(
[return<array<i32>>]
valuesA() {
  return(array<i32>(1i32, 2i32))
}

[return<array<i32>>]
valuesB() {
  return(array<i32>(3i32, 4i32))
}

[return<auto>]
wrapValues() {
  [auto] values{
    if(true,
      then(){ return(valuesA()) },
      else(){ return(valuesB()) })
  }
  return(values)
}

[return<i32>]
main() {
  return(count(wrapValues()))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("graph type resolver answers result queries through shared return-binding inference") {
  const std::string source = R"(
import /std/collections/*

[return<Result<array<i32>, ContainerError>>]
valuesOkA() {
  return(Result.ok(array<i32>(1i32, 2i32)))
}

[return<Result<array<i32>, ContainerError>>]
valuesOkB() {
  return(Result.ok(array<i32>(3i32, 4i32)))
}

[return<auto>]
wrapStatus() {
  [auto] status{
    if(true,
      then(){ return(valuesOkA()) },
      else(){ return(valuesOkB()) })
  }
  return(status)
}

unexpectedWrapStatusError([ContainerError] err) {
  [Result<ContainerError>] status{err.code}
}

[return<Result<int, ContainerError>> on_error<ContainerError, /unexpectedWrapStatusError>]
main() {
  [auto] values{try(wrapStatus())}
  return(Result.ok(count(values)))
}
  )";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK_FALSE(error.empty());
}

TEST_CASE("graph type resolver answers map receiver queries through shared type-text helper") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_map/*

[return<auto> effects(heap_alloc)]
selectValues() {
  if(true,
    then(){ return(/std/collections/map/map("left"raw_utf8, 4i32, "right"raw_utf8, 7i32)) },
    else(){ return(/std/collections/mapPair("left"raw_utf8, 4i32, "right"raw_utf8, 7i32)) })
}

[return<i32> effects(heap_alloc)]
main() {
  return(plus(selectValues().count(), selectValues().at("left"raw_utf8)))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("graph type resolver infers map value return kinds through shared infer helper") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_map/*

[return<auto> effects(heap_alloc)]
selectValues() {
  if(true,
    then(){ return(/std/collections/map/map("left"raw_utf8, 4i32, "right"raw_utf8, 7i32)) },
    else(){ return(/std/collections/mapPair("left"raw_utf8, 4i32, "right"raw_utf8, 7i32)) })
}

[return<auto> effects(heap_alloc)]
pickLeft() {
  return(selectValues().at("left"raw_utf8))
}

[return<i32> effects(heap_alloc)]
main() {
  return(pickLeft())
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("graph type resolver preserves nested string helper inference through shared collection receiver classifiers") {
  const std::string source = R"(
[return<int>]
packScore([args<string>] values) {
  return(plus(values.at(0i32).count(), values.at_unsafe(1i32).count()))
}

[return<int> effects(heap_alloc)]
vectorScore() {
  [vector<string>] values{vector<string>("alpha"raw_utf8, "beta"raw_utf8)}
  return(plus(values.at(0i32).count(), values.at_unsafe(1i32).count()))
}

[return<int> effects(heap_alloc)]
mapScore() {
  [map<i32, string>] values{map<i32, string>(1i32, "left"raw_utf8, 2i32, "right"raw_utf8)}
  return(plus(values.at(1i32).count(), values.at_unsafe(2i32).count()))
}

[return<int> effects(heap_alloc)]
main() {
  return(plus(packScore("ab"raw_utf8, "cde"raw_utf8),
              plus(vectorScore(), mapScore())))
}
  )";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK_FALSE(error.empty());
}

TEST_CASE("graph type resolver infers auto return kinds through shared collection receiver classifiers") {
  const std::string source = R"(
[return<auto>]
packScore([args<string>] values) {
  return(plus(values.at(0i32).count(), values.at_unsafe(1i32).count()))
}

[return<auto> effects(heap_alloc)]
vectorScore() {
  [vector<string>] values{vector<string>("alpha"raw_utf8, "beta"raw_utf8)}
  return(plus(values.at(0i32).count(), values.at_unsafe(1i32).count()))
}

[return<auto> effects(heap_alloc)]
mapScore() {
  [map<i32, string>] values{map<i32, string>(1i32, "left"raw_utf8, 2i32, "right"raw_utf8)}
  return(plus(values.at(1i32).count(), values.at_unsafe(2i32).count()))
}

[return<i32> effects(heap_alloc)]
main() {
  return(plus(packScore("ab"raw_utf8, "cde"raw_utf8),
              plus(vectorScore(), mapScore())))
}
  )";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK_FALSE(error.empty());
}

TEST_CASE("graph type resolver shares borrowed indexed collection plumbing for soa_vector auto returns") {
  const std::string source = R"(
import /std/collections/*

Particle() {
  [i32] x{1i32}
}

[return<auto>]
scoreRefs([args<Reference<soa_vector<Particle>>>] values) {
  return(count(to_aos(dereference(values[0i32]))))
}

[return<i32>]
main() {
  return(0i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("default semantics path carries grounded mutual recursion") {
  const std::string source = R"(
[return<auto>]
alpha([bool] done{false}) {
  if(done,
    then(){ return(1i32) },
    else(){ return(beta(true)) })
}

[return<auto>]
beta([bool] done{false}) {
  if(done,
    then(){ return(1i32) },
    else(){ return(alpha(true)) })
}

[return<i32>]
main() {
  return(alpha())
}
)";

  std::string defaultError;
  CHECK(validateProgram(source, "/main", defaultError));
  CHECK(defaultError.empty());
}

TEST_SUITE_END();
