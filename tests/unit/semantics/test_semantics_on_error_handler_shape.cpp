#include "third_party/doctest.h"

#include "test_semantics_helpers.h"

#include <string>
#include <vector>

// The arity and the first parameter of an `on_error` handler are checked in semantics. The error is at the
// `on_error` transform that names the handler, with the handler's definition as a related span.

TEST_SUITE_BEGIN("primestruct.semantics.on_error_handler_shape");

namespace {

// A program whose `main` is `[return<int> on_error<Oops, /onErr>...]` (line 12, the transform at column 14) and whose
// handler `onErr` is defined on line 9. `handlerParams` and `transformTail` vary the shape. The import makes the
// program go through the compile pipeline, so the spans carry the source file.
std::string programWith(const std::string &handlerParams, const std::string &transformTail) {
  return "import /std/result/*\n"
         "\n"
         "[struct]\n"
         "Oops() {\n"
         "  [i32] code{1i32}\n"
         "}\n"
         "\n"
         "[return<void>]\n"
         "onErr(" +
         handlerParams +
         ") {\n"
         "}\n"
         "\n"
         "[return<int> on_error<Oops, /onErr>" +
         transformTail +
         "]\n"
         "main() {\n"
         "  return(0i32)\n"
         "}\n";
}

struct Outcome {
  bool ok = false;
  std::string error;
  primec::CompilePipelineDiagnosticInfo info;
};

Outcome check(const std::string &source) {
  Outcome outcome;
  const std::vector<std::string> defaults = {"io_out", "io_err"};
  outcome.ok = validateProgramThroughCompilePipeline(
      source, "/main", defaults, defaults, outcome.error, &outcome.info);
  return outcome;
}

} // namespace

TEST_CASE("an on_error handler with an extra parameter is rejected at the transform") {
  const Outcome outcome = check(programWith("[Oops] err, [i32] extra", ""));
  CHECK_FALSE(outcome.ok);
  CHECK(outcome.error.find(
            "on_error handler argument count mismatch: /onErr takes 2 parameters, expected 1") !=
        std::string::npos);
  REQUIRE(outcome.info.hasPrimarySpan);
  CHECK_FALSE(outcome.info.primarySpan.file.empty());
  CHECK(outcome.info.primarySpan.line == 12);
  CHECK(outcome.info.primarySpan.column == 14);
  REQUIRE(outcome.info.relatedSpans.size() >= 1);
  CHECK(outcome.info.relatedSpans.front().label == "definition: /onErr");
  CHECK(outcome.info.relatedSpans.front().span.line == 9);
}

TEST_CASE("an on_error handler whose first parameter is not the error type is rejected at the "
          "transform") {
  const Outcome outcome = check(programWith("[i32] err", ""));
  CHECK_FALSE(outcome.ok);
  CHECK(outcome.error.find(
            "on_error handler error type mismatch: /onErr takes i32 as its first parameter") !=
        std::string::npos);
  REQUIRE(outcome.info.hasPrimarySpan);
  CHECK(outcome.info.primarySpan.line == 12);
  CHECK(outcome.info.primarySpan.column == 14);
  REQUIRE(outcome.info.relatedSpans.size() >= 1);
  CHECK(outcome.info.relatedSpans.front().label == "definition: /onErr");
  CHECK(outcome.info.relatedSpans.front().span.line == 9);
}

TEST_CASE("an on_error handler takes the error and one parameter per bound argument") {
  const Outcome bound = check(programWith("[Oops] err, [i32] extra", "(1i32)"));
  CHECK(bound.ok);
  CHECK(bound.error.empty());
  const Outcome plain = check(programWith("[Oops] err", ""));
  CHECK(plain.ok);
  CHECK(plain.error.empty());
  const Outcome tooMany = check(programWith("[Oops] err", "(1i32)"));
  CHECK_FALSE(tooMany.ok);
  CHECK(tooMany.error.find("takes 1 parameters, expected 2") != std::string::npos);
}

TEST_SUITE_END();
