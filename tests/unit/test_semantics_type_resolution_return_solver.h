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

TEST_CASE("graph type resolver preserves recursive annotation diagnostic") {
  const std::string source = R"(
[return<auto>]
main() {
  return(main())
}
)";
  std::string error;
  CHECK_FALSE(validateProgramWithTypeResolver(source, "/main", "graph", error));
  CHECK(error.find("return type inference requires explicit annotation on /main") != std::string::npos);
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
