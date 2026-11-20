#include "third_party/doctest.h"

#include "test_semantics_helpers.h"

TEST_SUITE_BEGIN("primestruct.semantics.construction_only_fields");

// A `ctor_only` field takes its value from the brace construction, is read by the defaults after it, and is not
// stored (docs/spec/type-system.md, Construction-only fields).

namespace {

void checkRejected(const std::string &source, const std::string &expected) {
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK_MESSAGE(error.find(expected) != std::string::npos, error);
}

} // namespace

TEST_CASE("a ctor_only field is built by braces and read by the defaults after it") {
  const std::string source = R"(
[struct]
Counter() {
  [ctor_only i64] start{0i64}
  [i64 mut] total{plus(start, 1i64)}
}

[return<int>]
main() {
  [Counter] counted{Counter{5i64}}
  [i64] total{counted.total}
  return(0i32)
}
)";
  std::string error;
  CHECK_MESSAGE(validateProgram(source, "/main", error), error);
}

TEST_CASE("a ctor_only field without a default must be given or the construction is a diagnostic") {
  checkRejected(R"(
[struct]
Counter() {
  [ctor_only i64] start
  [i64 mut] total{plus(start, 1i64)}
}

[return<int>]
main() {
  [Counter] counted{Counter{}}
  return(0i32)
}
)",
                "argument count mismatch");
}

TEST_CASE("reading a ctor_only field after construction names the field") {
  checkRejected(R"(
[struct]
Counter() {
  [ctor_only i64] start{0i64}
  [i64 mut] total{plus(start, 1i64)}
}

[return<int>]
main() {
  [Counter] counted{Counter{5i64}}
  [i64] first{counted.start}
  return(0i32)
}
)",
                "construction-only field is not stored: /Counter/start");
}

TEST_CASE("a method that names a ctor_only field through this is the same diagnostic") {
  checkRejected(R"(
[struct]
Counter() {
  [ctor_only i64] start{0i64}
  [i64 mut] total{plus(start, 1i64)}

  [return<i64>]
  first() {
    return(this.start)
  }
}

[return<int>]
main() {
  return(0i32)
}
)",
                "construction-only field is not stored: /Counter/start");
}

TEST_CASE("ctor_only on a local binding is rejected") {
  checkRejected(R"(
[return<int>]
main() {
  [ctor_only i64] start{1i64}
  return(0i32)
}
)",
                "ctor_only is only valid on struct fields: /main/start");
}

TEST_CASE("ctor_only on a parameter is rejected") {
  checkRejected(R"(
[return<int>]
bump([ctor_only i64] amount) {
  return(0i32)
}

[return<int>]
main() {
  return(bump(1i64))
}
)",
                "ctor_only is only valid on struct fields: /bump/amount");
}

TEST_CASE("ctor_only on a field of a struct that is not marked [struct] is rejected") {
  checkRejected(R"(
Pair() {
  [ctor_only i64] first{0i64}
  [i64] second{first}
}

[return<int>]
main() {
  return(0i32)
}
)",
                "ctor_only is only valid on struct fields: /Pair/first");
}

TEST_CASE("ctor_only cannot be combined with static") {
  checkRejected(R"(
[struct]
Counter() {
  [static ctor_only i64] start{0i64}
  [i64 mut] total{plus(start, 1i64)}
}

[return<int>]
main() {
  return(0i32)
}
)",
                "ctor_only cannot be combined with static: /Counter/start");
}

TEST_CASE("a ctor_only field that no later default reads is rejected") {
  checkRejected(R"(
[struct]
Counter() {
  [ctor_only i64] start{0i64}
  [i64 mut] total{1i64}
}

[return<int>]
main() {
  return(0i32)
}
)",
                "ctor_only field is never read by a later field default: /Counter/start");
}

TEST_SUITE_END();
