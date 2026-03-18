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
  CHECK(validateProgramWithTypeResolver(source, "/main", "graph", error));
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
  CHECK_FALSE(validateProgramWithTypeResolver(source, "/main", "graph", error));
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
  CHECK_FALSE(validateProgramWithTypeResolver(source, "/main", "graph", error));
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
  CHECK_FALSE(validateProgramWithTypeResolver(source, "/main", "graph", error));
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
  CHECK_FALSE(validateProgramWithTypeResolver(source, "/main", "graph", error));
  CHECK(error.find("conflicting return types on /main") != std::string::npos);
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
  std::string graphError;
  CHECK(validateProgramWithTypeResolver(source, "/main", "graph", graphError));
  CHECK(graphError.empty());

  std::string legacyError;
  CHECK(validateProgramWithTypeResolver(source, "/main", "legacy", legacyError));
  CHECK(legacyError.empty());
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
  std::string graphError;
  CHECK(validateProgramWithTypeResolver(source, "/main", "graph", graphError));
  CHECK(graphError.empty());

  std::string legacyError;
  CHECK(validateProgramWithTypeResolver(source, "/main", "legacy", legacyError));
  CHECK(legacyError.empty());
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
  std::string graphError;
  CHECK(validateProgramWithTypeResolver(source, "/main", "graph", graphError));
  CHECK(graphError.empty());

  std::string legacyError;
  CHECK(validateProgramWithTypeResolver(source, "/main", "legacy", legacyError));
  CHECK(legacyError.empty());
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
  std::string graphError;
  CHECK(validateProgramWithTypeResolver(source, "/main", "graph", graphError));
  CHECK(graphError.empty());

  std::string legacyError;
  CHECK(validateProgramWithTypeResolver(source, "/main", "legacy", legacyError));
  CHECK(legacyError.empty());
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
  std::string graphError;
  CHECK(validateProgramWithTypeResolver(source, "/main", "graph", graphError));
  CHECK(graphError.empty());

  std::string legacyError;
  CHECK(validateProgramWithTypeResolver(source, "/main", "legacy", legacyError));
  CHECK(legacyError.empty());
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
  std::string graphError;
  CHECK(validateProgramWithTypeResolver(source, "/main", "graph", graphError));
  CHECK(graphError.empty());

  std::string legacyError;
  CHECK(validateProgramWithTypeResolver(source, "/main", "legacy", legacyError));
  CHECK(legacyError.empty());
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
  std::string graphError;
  CHECK_FALSE(validateProgramWithTypeResolver(source, "/main", "graph", graphError));
  CHECK(graphError.find("unresolved or ambiguous omitted struct field envelope: /Shape/center") !=
        std::string::npos);

  std::string legacyError;
  CHECK_FALSE(validateProgramWithTypeResolver(source, "/main", "legacy", legacyError));
  CHECK(legacyError.find("unresolved or ambiguous omitted struct field envelope: /Shape/center") !=
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
  std::string graphError;
  CHECK(validateProgramWithTypeResolver(source, "/main", "graph", graphError));
  CHECK(graphError.empty());

  std::string legacyError;
  CHECK(validateProgramWithTypeResolver(source, "/main", "legacy", legacyError));
  CHECK(legacyError.empty());
}

TEST_CASE("graph type resolver answers result queries through shared return-binding inference") {
  const std::string source = R"(
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

[return<i32>]
main() {
  [auto] values{try(wrapStatus())}
  return(count(values))
}
)";
  std::string graphError;
  CHECK(validateProgramWithTypeResolver(source, "/main", "graph", graphError));
  CHECK(graphError.empty());

  std::string legacyError;
  CHECK(validateProgramWithTypeResolver(source, "/main", "legacy", legacyError));
  CHECK(legacyError.empty());
}

TEST_CASE("graph type resolver answers map receiver queries through shared type-text helper") {
  const std::string source = R"(
[return<auto> effects(heap_alloc)]
selectValues() {
  if(true,
    then(){ return(/std/collections/map/map("left"raw_utf8, 4i32, "right"raw_utf8, 7i32)) },
    else(){ return(/std/collections/mapPair("left"raw_utf8, 4i32, "right"raw_utf8, 7i32)) })
}

[return<Result<int, ContainerError>> effects(heap_alloc)]
main() {
  [i32] total{plus(selectValues().count(), try(selectValues().tryAt("left"raw_utf8)))}
  return(Result.ok(total))
}
)";
  std::string graphError;
  CHECK(validateProgramWithTypeResolver(source, "/main", "graph", graphError));
  CHECK(graphError.empty());

  std::string legacyError;
  CHECK(validateProgramWithTypeResolver(source, "/main", "legacy", legacyError));
  CHECK(legacyError.empty());
}

TEST_CASE("default semantics path uses graph resolver while legacy remains available for rollback") {
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

  std::string legacyError;
  CHECK_FALSE(validateProgramWithTypeResolver(source, "/main", "legacy", legacyError));
  CHECK(legacyError.find("return type inference requires explicit annotation") != std::string::npos);
}

TEST_SUITE_END();
