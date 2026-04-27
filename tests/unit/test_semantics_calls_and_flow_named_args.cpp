#include "third_party/doctest.h"

#include "test_semantics_helpers.h"

TEST_SUITE_BEGIN("primestruct.semantics.calls_flow.named_args");

TEST_CASE("unknown named argument fails") {
  const std::string source = R"(
[return<int>]
foo([i32] a, [i32] b) {
  return(a)
}

[return<int>]
main() {
  return(foo([c] 1i32, [b] 2i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown named argument") != std::string::npos);
}

TEST_CASE("named argument duplicates positional parameter fails") {
  const std::string source = R"(
[return<int>]
foo([i32] a, [i32] b) {
  return(a)
}

[return<int>]
main() {
  return(foo(1i32, [a] 2i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("named argument duplicates parameter") != std::string::npos);
}

TEST_CASE("execution named arguments match parameters") {
  const std::string source = R"(
[return<int>]
main() {
  return(1i32)
}

[return<int>]
job([i32] a, [i32] b) {
  return(a)
}

job([a] 1i32, [b] 2i32)
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("execution named arguments allow positional fill after labels") {
  const std::string source = R"(
[return<int>]
main() {
  return(1i32)
}

[return<int>]
job([i32] a, [i32] b) {
  return(plus(a, b))
}

job([b] 2i32, 1i32)
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("execution named argument duplicates positional parameter fails") {
  const std::string source = R"(
[return<int>]
main() {
  return(1i32)
}

[return<int>]
job([i32] a, [i32] b) {
  return(plus(a, b))
}

job(1i32, [a] 2i32)
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("named argument duplicates parameter: a") != std::string::npos);
}

TEST_CASE("execution unknown named argument fails in source syntax") {
  const std::string source = R"(
[return<int>]
main() {
  return(1i32)
}

[return<int>]
job([i32] a, [i32] b) {
  return(a)
}

job([c] 1i32, [b] 2i32)
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown named argument: c") != std::string::npos);
}

TEST_CASE("execution duplicate named argument fails") {
  const std::string source = R"(
[return<int>]
main() {
  return(1i32)
}

[return<int>]
job([i32] a, [i32] b) {
  return(plus(a, b))
}

job([a] 1i32, [a] 2i32)
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("duplicate named argument: a") != std::string::npos);
}

TEST_CASE("execution arguments reject named builtins") {
  const std::string source = R"(
[return<int>]
main() {
  return(1i32)
}

[return<int>]
job([array<i32>] values) {
  return(values.count())
}

job(array<i32>([first] 1i32))
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("named arguments not supported for builtin calls") != std::string::npos);
}

TEST_CASE("named arguments not allowed on builtin calls in source syntax") {
  const std::string source = R"(
[return<int>]
main() {
  return(plus([left] 1i32, [right] 2i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("named arguments not supported for builtin calls") != std::string::npos);
}

TEST_CASE("named argument cannot bind variadic parameter") {
  const std::string source = R"(
[return<i32>]
collect([i32] head, [i32] values...) {
  return(head)
}

[return<i32>]
main() {
  return(collect([head] 1i32, [values] 2i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("named arguments cannot bind variadic parameter: values") != std::string::npos);
}

TEST_CASE("named fixed argument still allows trailing variadic positionals") {
  const std::string source = R"(
[return<i32>]
collect([i32] head, [i32] values...) {
  return(head)
}

[return<i32>]
main() {
  return(collect([head] 1i32, 2i32, 3i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("method call fixed argument still allows trailing variadic positionals") {
  const std::string source = R"(
[struct]
Collector() {
  [i32] seed{0i32}
}

[return<i32>]
/Collector/collect([Collector] self, [i32] head, [i32] values...) {
  return(head)
}

[return<i32>]
main() {
  [Collector] collector{Collector{7i32}}
  return(collector.collect(1i32, 2i32, 3i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("execution named argument cannot bind variadic parameter") {
  const std::string source = R"(
[return<int>]
main() {
  return(1i32)
}

[return<int>]
job([i32] head, [i32] values...) {
  return(head)
}

job([head] 1i32, [values] 2i32)
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("named arguments cannot bind variadic parameter: values") != std::string::npos);
}

TEST_CASE("method call variadic parameter rejects mismatched trailing argument") {
  const std::string source = R"(
[struct]
Collector() {
  [i32] seed{0i32}
}

[return<i32>]
/Collector/collect([Collector] self, [i32] head, [i32] values...) {
  return(head)
}

[return<i32>]
main() {
  [Collector] collector{Collector{7i32}}
  return(collector.collect(1i32, "wrong"raw_utf8))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("argument type mismatch for /Collector/collect parameter values: expected i32") !=
        std::string::npos);
}

TEST_CASE("execution variadic parameter rejects mismatched trailing argument") {
  const std::string source = R"(
[return<int>]
main() {
  return(1i32)
}

[return<int>]
job([i32] head, [i32] values...) {
  return(head)
}

job(1i32, "wrong"raw_utf8)
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("argument type mismatch for /job parameter values: expected i32") !=
        std::string::npos);
}

TEST_SUITE_END();
