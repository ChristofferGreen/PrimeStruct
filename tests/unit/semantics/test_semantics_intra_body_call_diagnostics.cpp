#include "third_party/doctest.h"

#include "test_semantics_helpers.h"

#include <string>
#include <vector>

// TODO-5960: an execution and a definition get the same intra-body call
// diagnostics from one implementation. Each case places one call in an
// execution argument and in a definition body and pins what both report.

TEST_SUITE_BEGIN("primestruct.semantics.intra_body_call_diagnostics");

namespace {

struct CollectedRecord {
  std::string message;
  std::string label;
  int line = 0;
  int column = 0;
};

const char *const SinkPreamble = R"(
[return<i32>]
main() {
  return(0i32)
}

[return<void>]
sink([i32] a) {
  return()
}
)";

std::vector<CollectedRecord> collectRecords(const std::string &source) {
  std::string error;
  primec::SemanticDiagnosticInfo info;
  validateProgramCollectingDiagnostics(source, "/main", error, info);
  std::vector<CollectedRecord> records;
  for (const auto &record : info.records) {
    CollectedRecord collected;
    collected.message = record.message;
    if (!record.relatedSpans.empty()) {
      collected.label = record.relatedSpans.front().label;
      collected.line = record.relatedSpans.front().span.line;
      collected.column = record.relatedSpans.front().span.column;
    }
    records.push_back(std::move(collected));
  }
  return records;
}

// The records of `call` written as the argument of an execution of `sink`.
std::vector<CollectedRecord> recordsForExecution(const std::string &preamble,
                                                 const std::string &call) {
  return collectRecords(preamble + "\nsink(" + call + ")\n");
}

// The records of `call` written as an argument of a call inside a definition.
std::vector<CollectedRecord> recordsForDefinition(const std::string &preamble,
                                                  const std::string &call) {
  return collectRecords(preamble + "\n[return<void>]\ncaller() {\n  sink(" + call + ")\n}\n");
}

bool hasMessageContaining(const std::vector<CollectedRecord> &records, const std::string &needle) {
  for (const auto &record : records) {
    if (record.message.find(needle) != std::string::npos) {
      return true;
    }
  }
  return false;
}

} // namespace

TEST_CASE("unknown call targets are reported with the owner label and position") {
  const std::string preamble = SinkPreamble;
  const auto execution = recordsForExecution(preamble, "nope(1i32)");
  const auto definition = recordsForDefinition(preamble, "nope(1i32)");
  REQUIRE(execution.size() == 1);
  REQUIRE(definition.size() == 1);
  CHECK(execution.front().message == "unknown call target: nope");
  CHECK(definition.front().message == "unknown call target: nope");
  CHECK(execution.front().label == "execution: /sink");
  CHECK(definition.front().label == "definition: /caller");
  CHECK(execution.front().line > 0);
  CHECK(definition.front().line > 0);
}

TEST_CASE("bare get is a builtin call for executions and definitions alike") {
  const std::string preamble = SinkPreamble;
  const std::string call = "get(map<i32, i32>(1i32, 2i32), 1i32)";
  CHECK_FALSE(
      hasMessageContaining(recordsForExecution(preamble, call), "unknown call target: get"));
  CHECK_FALSE(
      hasMessageContaining(recordsForDefinition(preamble, call), "unknown call target: get"));
}

TEST_CASE("bare ref is a builtin call for executions and definitions alike") {
  const std::string preamble = SinkPreamble;
  const std::string call = "ref(vector<i32>(1i32), 0i32)";
  CHECK_FALSE(
      hasMessageContaining(recordsForExecution(preamble, call), "unknown call target: ref"));
  CHECK_FALSE(
      hasMessageContaining(recordsForDefinition(preamble, call), "unknown call target: ref"));
}

TEST_CASE("pick is control flow and its arms are not calls for executions and definitions alike") {
  const std::string preamble = std::string(R"(
[sum]
Maybe {
  none
  [i32] some
}

[return<Maybe>]
make_maybe() {
  [Maybe] result{}
  return(result)
}
)") + SinkPreamble;
  const std::string call = R"(pick(make_maybe()) {
  none {
    return(0i32)
  }
  some(v) {
    return(v)
  }
})";
  // A definition returns the pick; an execution takes it as an argument.
  const auto definition =
      collectRecords(preamble + "\n[return<i32>]\ncaller() {\n  return(" + call + ")\n}\n");
  for (const auto &records : {recordsForExecution(preamble, call), definition}) {
    CHECK_FALSE(hasMessageContaining(records, "unknown call target: pick"));
    CHECK_FALSE(hasMessageContaining(records, "unknown call target: some"));
    CHECK_FALSE(hasMessageContaining(records, "unknown call target: none"));
  }
}

TEST_CASE("a user definition of a canonical key-value access path is not reported as unknown") {
  const std::string preamble = std::string(R"(
[return<i32>]
/std/collections/map/at([i32] values, [i32] key) {
  return(key)
}
)") + SinkPreamble;
  const std::string call = "/std/collections/map/at(1i32, 2i32)";
  CHECK_FALSE(hasMessageContaining(recordsForExecution(preamble, call), "unknown call target"));
  CHECK_FALSE(hasMessageContaining(recordsForDefinition(preamble, call), "unknown call target"));
}

TEST_SUITE_END();
