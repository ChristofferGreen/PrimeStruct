#include "primec/testing/DumpNormalization.h"

#include "third_party/doctest.h"

TEST_SUITE_BEGIN("primestruct.dumps.normalization");

using primec::testing::stripDumpTimings;

TEST_CASE("stripDumpTimings blanks every type-graph metrics timing field") {
  // The full field set the type-graph metrics line emits (see
  // `--dump-stage type-graph`); any new `*_ms`/`*_over` field must be covered here.
  const std::string fast =
      "metrics prepare_ms=1 build_ms=0 prepare_ms_max=2 build_ms_max=3 prepare_over=false build_over=false nodes=163";
  const std::string slow =
      "metrics prepare_ms=912 build_ms=77 prepare_ms_max=4096 build_ms_max=15 prepare_over=true build_over=true nodes=163";
  CHECK(stripDumpTimings(fast) == stripDumpTimings(slow));
  CHECK(stripDumpTimings(fast) ==
        "metrics prepare_ms=N build_ms=N prepare_ms_max=N build_ms_max=N prepare_over=B build_over=B nodes=163");
}

TEST_CASE("stripDumpTimings keeps deterministic content") {
  const std::string text = "kind=definition_return label=\"/leaf\" nodes=163 edges=115\nname_ms_suffix and ms=5\n";
  CHECK(stripDumpTimings(text) == text);
  CHECK(stripDumpTimings("") == "");
  CHECK(stripDumpTimings("x_ms=") == "x_ms=N");
  CHECK(stripDumpTimings("a_over=true b_over=false") == "a_over=B b_over=B");
}

TEST_CASE("stripDumpTimings leaves differing non-timing content different") {
  CHECK(stripDumpTimings("nodes=163 build_ms=1") != stripDumpTimings("nodes=164 build_ms=1"));
}

TEST_SUITE_END();
