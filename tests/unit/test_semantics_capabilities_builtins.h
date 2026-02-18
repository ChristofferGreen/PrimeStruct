TEST_CASE("struct transforms are rejected on executions") {
  const std::string source = R"(
[return<int>]
task([i32] x) {
  return(x)
}

[return<int>]
main() {
  return(1i32)
}

[struct]
task(1i32)
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("struct transforms are not allowed on executions") != std::string::npos);
}

TEST_CASE("mut transforms are rejected on executions") {
  const std::string source = R"(
[return<int>]
task([i32] x) {
  return(x)
}

[return<int>]
main() {
  return(1i32)
}

[mut]
task(1i32)
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("mut transform is not allowed on executions") != std::string::npos);
}

TEST_CASE("copy transforms are rejected on executions") {
  const std::string source = R"(
[return<int>]
task([i32] x) {
  return(x)
}

[return<int>]
main() {
  return(1i32)
}

[copy]
task(1i32)
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("copy transform is not allowed on executions") != std::string::npos);
}

TEST_CASE("restrict transforms are rejected on executions") {
  const std::string source = R"(
[return<int>]
task([i32] x) {
  return(x)
}

[return<int>]
main() {
  return(1i32)
}

[restrict<i32>]
task(1i32)
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("restrict transform is not allowed on executions") != std::string::npos);
}

TEST_CASE("visibility transforms are rejected on executions") {
  const std::string source = R"(
[return<int>]
task([i32] x) {
  return(x)
}

[return<int>]
main() {
  return(1i32)
}

[public]
task(1i32)
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("binding visibility/static transforms are only valid on bindings") != std::string::npos);
}

TEST_CASE("static transforms are rejected on executions") {
  const std::string source = R"(
[return<int>]
task([i32] x) {
  return(x)
}

[return<int>]
main() {
  return(1i32)
}

[static]
task(1i32)
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("binding visibility/static transforms are only valid on bindings") != std::string::npos);
}

TEST_SUITE_END();

TEST_SUITE_BEGIN("primestruct.semantics.builtin_calls");

TEST_CASE("builtin arithmetic calls validate") {
  const std::string source = R"(
namespace demo {
  [return<int>]
  main() {
    return(plus(1i32, 2i32))
  }
}
)";
  std::string error;
  CHECK(validateProgram(source, "/demo/main", error));
  CHECK(error.empty());
}

TEST_CASE("builtin negate calls validate") {
  const std::string source = R"(
[return<int>]
main() {
  return(negate(2i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("builtin comparison calls validate") {
  const std::string source = R"(
[return<bool>]
main() {
  return(greater_than(2i32, 1i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("builtin comparison accepts bool operands") {
  const std::string source = R"(
[return<bool>]
main() {
  return(greater_than(true, false))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("builtin comparison accepts bool and signed int") {
  const std::string source = R"(
[return<bool>]
main() {
  return(equal(true, 1i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("builtin comparison rejects bool with u64") {
  const std::string source = R"(
[return<bool>]
main() {
  return(greater_than(true, 1u64))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("mixed signed/unsigned") != std::string::npos);
}

TEST_CASE("builtin less_than calls validate") {
  const std::string source = R"(
[return<bool>]
main() {
  return(less_than(1i32, 2i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("builtin equal calls validate") {
  const std::string source = R"(
[return<bool>]
main() {
  return(equal(2i32, 2i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("builtin not_equal calls validate") {
  const std::string source = R"(
[return<bool>]
main() {
  return(not_equal(2i32, 3i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("builtin greater_equal calls validate") {
  const std::string source = R"(
[return<bool>]
main() {
  return(greater_equal(2i32, 2i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("builtin less_equal calls validate") {
  const std::string source = R"(
[return<bool>]
main() {
  return(less_equal(2i32, 3i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("builtin and calls validate") {
  const std::string source = R"(
[return<bool>]
main() {
  return(and(true, false))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("builtin and rejects float operands") {
  const std::string source = R"(
[return<bool>]
main() {
  return(and(1.5f, true))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("boolean operators require bool operands") != std::string::npos);
}

TEST_CASE("builtin and rejects struct operands") {
  const std::string source = R"(
thing() {
  [i32] value{1i32}
}

[return<bool>]
main() {
  [thing] item{1i32}
  return(and(item, true))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("boolean operators require bool operands") != std::string::npos);
}

TEST_CASE("builtin or calls validate") {
  const std::string source = R"(
[return<bool>]
main() {
  return(or(false, true))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("builtin not calls validate") {
  const std::string source = R"(
[return<bool>]
main() {
  return(not(false))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("builtin comparison accepts float operands") {
  const std::string source = R"(
[return<bool>]
main() {
  return(less_than(1.5f, 2.5f))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("builtin comparison accepts string operands") {
  const std::string source = R"(
[return<bool>]
main() {
  return(equal("a"utf8, "b"utf8))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("builtin comparison rejects pointer operands") {
  const std::string source = R"(
[return<bool>]
main() {
  [i32] value{1i32}
  return(greater_than(location(value), 1i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("comparisons require numeric, bool, or string operands") != std::string::npos);
}

TEST_CASE("builtin comparison rejects struct operands") {
  const std::string source = R"(
thing() {
  [i32] value{1i32}
}

[return<bool>]
main() {
  [thing] item{1i32}
  return(equal(item, item))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("comparisons require numeric, bool, or string operands") != std::string::npos);
}

TEST_CASE("builtin comparison rejects mixed signed/unsigned operands") {
  const std::string source = R"(
[return<bool>]
main() {
  return(greater_than(1i64, 2u64))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("mixed signed/unsigned") != std::string::npos);
}

TEST_CASE("builtin comparison rejects mixed int/float operands") {
  const std::string source = R"(
[return<bool>]
main() {
  return(greater_than(1i32, 2.5f))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("mixed int/float") != std::string::npos);
}

TEST_CASE("builtin clamp calls validate") {
  const std::string source = R"(
import /math/*
[return<int>]
main() {
  return(clamp(2i32, 1i32, 5i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("builtin clamp rejects mixed signed/unsigned operands") {
  const std::string source = R"(
import /math/*
[return<int>]
main() {
  return(clamp(2i64, 1u64, 5u64))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("mixed signed/unsigned") != std::string::npos);
}

TEST_CASE("builtin arithmetic arity mismatch fails") {
  const std::string source = R"(
[return<int>]
main() {
  return(plus(1i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("argument count mismatch for builtin plus") != std::string::npos);
}

TEST_CASE("builtin negate arity mismatch fails") {
  const std::string source = R"(
[return<int>]
main() {
  return(negate(1i32, 2i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("argument count mismatch for builtin negate") != std::string::npos);
}

TEST_CASE("builtin comparison arity mismatch fails") {
  const std::string source = R"(
[return<bool>]
main() {
  return(greater_than(1i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("argument count mismatch for builtin greater_than") != std::string::npos);
}

TEST_CASE("builtin less_than arity mismatch fails") {
  const std::string source = R"(
[return<bool>]
main() {
  return(less_than(1i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("argument count mismatch for builtin less_than") != std::string::npos);
}

TEST_CASE("builtin equal arity mismatch fails") {
  const std::string source = R"(
[return<bool>]
main() {
  return(equal(1i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("argument count mismatch for builtin equal") != std::string::npos);
}

TEST_CASE("builtin greater_equal arity mismatch fails") {
  const std::string source = R"(
[return<bool>]
main() {
  return(greater_equal(1i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("argument count mismatch for builtin greater_equal") != std::string::npos);
}

TEST_CASE("builtin and arity mismatch fails") {
  const std::string source = R"(
[return<bool>]
main() {
  return(and(1i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("argument count mismatch for builtin and") != std::string::npos);
}

TEST_CASE("builtin not arity mismatch fails") {
  const std::string source = R"(
[return<bool>]
main() {
  return(not(1i32, 2i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("argument count mismatch for builtin not") != std::string::npos);
}

TEST_CASE("builtin clamp arity mismatch fails") {
  const std::string source = R"(
import /math/*
[return<int>]
main() {
  return(clamp(1i32, 2i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("argument count mismatch for builtin clamp") != std::string::npos);
}

TEST_CASE("void return can omit return statement") {
  const std::string source = R"(
[return<void>]
main() {
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("local binding names are visible") {
  const std::string source = R"(
[return<int>]
main() {
  [i32] value{6i32}
  return(value)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}
