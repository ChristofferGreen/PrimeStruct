#include "third_party/doctest.h"

#include "test_semantics_helpers.h"

TEST_SUITE_BEGIN("primestruct.semantics.read_only_roots");

// A write to a field of, or a `[mut]` method call on, a read-only root (a local that is not `mut`, a `[T]` or
// `[T copy]` parameter, the `this` of a method that is not `[mut]`) is a diagnostic that names the fix
// (docs/spec/value-lifecycle.md, Parameter modes). A `Reference<T>` carries its own write access.

namespace {

const char *CounterStruct = R"(
[struct]
Counter() {
  [i32 mut] total{0i32}
  [mut] bump() {
    this.total = this.total + 1i32
  }
  [return<int>] peek() {
    return(this.total)
  }
}
)";

void checkRejectedAt(const std::string &body, const std::string &expected, int line, int column) {
  const std::string source = std::string(CounterStruct) + body;
  primec::SemanticDiagnosticInfo diagnosticInfo;
  std::string error;
  CHECK_FALSE(validateProgramReportingDiagnostic(source, "/main", error, diagnosticInfo));
  CHECK_MESSAGE(error.find(expected) != std::string::npos, error);
  REQUIRE(diagnosticInfo.hasPrimarySpan);
  CHECK(diagnosticInfo.primarySpan.line == line);
  CHECK(diagnosticInfo.primarySpan.column == column);
}

void checkAccepted(const std::string &body) {
  primec::SemanticDiagnosticInfo diagnosticInfo;
  std::string error;
  CHECK_MESSAGE(validateProgramReportingDiagnostic(std::string(CounterStruct) + body, "/main", error, diagnosticInfo),
                error);
}

} // namespace

TEST_CASE("a field is not written through a read-only parameter") {
  checkRejectedAt(R"(
[return<void>]
write([Counter] counter) {
  counter.total = 5i32
}

[return<int>]
main() {
  return(0i32)
}
)",
                  "assign target must be a mutable binding: counter.total (counter is a read-only parameter; "
                  "take it as a [T mut] parameter)",
                  15, 3);
}

TEST_CASE("a mut method is not called on a read-only parameter") {
  checkRejectedAt(R"(
[return<void>]
bump([Counter] counter) {
  counter.bump()
}

[return<int>]
main() {
  return(0i32)
}
)",
                  "mutating method needs a mutable receiver: counter.bump() (counter is a read-only parameter; "
                  "take it as a [T mut] parameter)",
                  15, 3);
}

TEST_CASE("a copy parameter is read-only too") {
  checkRejectedAt(R"(
[return<void>]
write([Counter copy] counter) {
  counter.total = 5i32
}

[return<int>]
main() {
  return(0i32)
}
)",
                  "counter is a read-only parameter", 15, 3);
}

TEST_CASE("a field is not written and a mut method is not called on a local that is not mut") {
  checkRejectedAt(R"(
[return<int>]
main() {
  [Counter] counter{Counter{}}
  counter.total = 3i32
  return(0i32)
}
)",
                  "assign target must be a mutable binding: counter.total (counter is read-only; declare it with "
                  "mut: [T mut] counter{...})",
                  16, 3);
  checkRejectedAt(R"(
[return<int>]
main() {
  [Counter] counter{Counter{}}
  counter.bump()
  return(0i32)
}
)",
                  "mutating method needs a mutable receiver: counter.bump() (counter is read-only; declare it "
                  "with mut: [T mut] counter{...})",
                  16, 3);
}

TEST_CASE("a nested field is not written through a read-only root") {
  checkRejectedAt(R"(
[struct]
Pair() {
  [Counter mut] first{Counter{}}
}

[return<int>]
main() {
  [Pair] pair{Pair{}}
  pair.first.total = 3i32
  return(0i32)
}
)",
                  "assign target must be a mutable binding: pair.first.total (pair is read-only", 21, 3);
}

TEST_CASE("a lambda parameter without mut is read-only") {
  checkRejectedAt(R"(
[effects(heap_alloc) return<int>]
main() {
  [Function<void, Counter>] f{[]([Counter] counter) { counter.bump() }}
  return(0i32)
}
)",
                  "mutating method needs a mutable receiver: counter.bump() (counter is a read-only parameter",
                  15, 55);
}

TEST_CASE("a method that is not mut cannot write this") {
  const std::string source = R"(
[struct]
Counter() {
  [i32 mut] total{0i32}
  [return<void>] reset() {
    this.total = 0i32
  }
}

[return<int>]
main() {
  return(0i32)
}
)";
  primec::SemanticDiagnosticInfo diagnosticInfo;
  std::string error;
  CHECK_FALSE(validateProgramReportingDiagnostic(source, "/main", error, diagnosticInfo));
  CHECK_MESSAGE(error.find("this is read-only in a method that is not [mut]; mark the method [mut]") !=
                    std::string::npos,
                error);
  REQUIRE(diagnosticInfo.hasPrimarySpan);
  CHECK(diagnosticInfo.primarySpan.line == 6);
}

TEST_CASE("mut roots, mut parameters and references still write and call mut methods") {
  checkAccepted(R"(
[return<void>]
write_param([Counter mut] counter) {
  counter.total = 5i32
  counter.bump()
}

[return<void>]
write_reference([Reference<Counter>] counter) {
  counter.total = 5i32
  counter.bump()
}

[struct]
Holder() {
  [Reference<Counter> mut] kept
  [mut] bumpKept() {
    this.kept.bump()
    this.kept.total = 2i32
  }
}

[return<int>]
main() {
  [Counter mut] counter{Counter{}}
  counter.total = 3i32
  counter.bump()
  write_param(counter)
  write_reference(location(counter))
  [Reference<Counter>] alias{location(counter)}
  alias.total = 4i32
  alias.bump()
  return(0i32)
}
)");
}

TEST_CASE("lifecycle helpers and mut methods write this") {
  checkAccepted(R"(
[struct]
Cell() {
  [i32 mut] value{0i32}
  Create() {
    this.value = 1i32
  }
  [mut] set([i32] next) {
    this.value = next
    this.set2(next)
  }
  [mut] set2([i32] next) {
    this.value = next
  }
}

[return<int>]
main() {
  [Cell mut] cell{Cell{}}
  cell.set(2i32)
  return(0i32)
}
)");
}

TEST_CASE("reading through a read-only root and calling a plain method are accepted") {
  checkAccepted(R"(
[return<int>]
read([Counter] counter) {
  [i32] seen{counter.total}
  return(plus(seen, counter.peek()))
}

[return<int>]
main() {
  [Counter] counter{Counter{}}
  return(minus(read(counter), 0i32))
}
)");
}

TEST_SUITE_END();
