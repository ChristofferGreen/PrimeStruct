#include "../test_semantics_helpers.h"

// TODO-5278: direct source-level regression coverage for
// SemanticsValidator::explicitRemovedCollectionMethodPathForCallNamespace
// (promoted in TODO-5277 from the local lambda
// explicitRemovedCollectionMethodPathLocal, itself the site of TODO-4724
// seam (4d)'s only real regression this session - see docs/todo_finished.md).
// This function decides whether an explicitly-spelled removed
// compatibility-helper path (bare /vector/*, /array/*, or /map/* spellings
// of the old count/push/pop/etc. family) is rejected outright, across all
// three collection families it classifies. These cases pin that rejection
// directly at the source level (matching this codebase's existing testing
// convention of validating full .prime programs through the public
// SemanticsValidator entry point, rather than calling private resolver
// members directly, since none of the promoted resolvers are exposed to
// test code and adding such exposure was judged not worth the intrusion
// into the class's real access boundaries).

TEST_SUITE_BEGIN("primestruct.semantics.calls_flow.collections");

TEST_CASE("explicit rooted vector removed-helper path stays rejected without import") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32)}
  /vector/push(values, 2i32)
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /vector/push") != std::string::npos);
}

TEST_CASE("explicit rooted vector removed-helper path stays rejected even with stdlib import") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32)}
  /vector/push(values, 2i32)
  return(count(values))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /vector/push") != std::string::npos);
}

TEST_CASE("explicit rooted array removed-helper path stays rejected without import") {
  const std::string source = R"(
[return<int>]
main() {
  [array<i32>] values{array<i32>(1i32)}
  return(/array/count(values))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /array/count") != std::string::npos);
}

TEST_CASE("explicit rooted map removed-helper path stays rejected without import") {
  const std::string source = R"(
[return<int>]
main() {
  [map<i32, i32>] m{map<i32, i32>(1i32, 2i32)}
  return(/map/count(m))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /map/count") != std::string::npos);
}

TEST_SUITE_END();
