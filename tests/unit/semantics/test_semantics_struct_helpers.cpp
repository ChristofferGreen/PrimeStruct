#include "third_party/doctest.h"

#include "test_semantics_helpers.h"

TEST_SUITE_BEGIN("primestruct.semantics.struct_helpers");

TEST_CASE("struct helper method call uses implicit this") {
  const std::string source = R"(
[struct]
Thing() {
  [i32] value{3i32}

  [return<i32>]
  get() {
    return(this.value)
  }
}

[return<i32>]
main() {
  [Thing] t{Thing{}}
  return(t.get())
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("struct helper accepts explicit this argument") {
  const std::string source = R"(
[struct]
Thing() {
  [i32] value{5i32}

  [return<i32>]
  get() {
    return(this.value)
  }
}

[return<i32>]
main() {
  [Thing] t{Thing{}}
  return(/Thing/get(t))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("static helper rejects method-call sugar") {
  const std::string source = R"(
[struct]
Thing() {
  [i32] value{1i32}

  [static return<i32>]
  id() {
    return(1i32)
  }
}

[return<i32>]
main() {
  [Thing] t{Thing{}}
  return(t.id())
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("static helper does not accept method-call syntax") != std::string::npos);
}

TEST_CASE("implicit auto helper inference works through method-call sugar") {
  const std::string source = R"(
[struct]
Box() {
  [i32] seed{1i32}

  [return<auto>]
  echo([auto] value) {
    return(value)
  }
}

[return<i32>]
main() {
  [Box] box{Box{}}
  return(box.echo(7i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("static helper allows direct calls") {
  const std::string source = R"(
[struct]
Thing() {
  [i32] value{1i32}

  [static return<i32>]
  id() {
    return(1i32)
  }
}

[return<i32>]
main() {
  return(/Thing/id())
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("static helper allows type-qualified dot calls") {
  const std::string source = R"(
[struct]
Counter() {
  [i32] value{5i32}

  [static return<i32>]
  defaultStep() {
    return(2i32)
  }

  [return<i32>]
  doubled() {
    return(multiply(this.value, 2i32))
  }
}

[return<i32>]
main() {
  [Counter] counter{Counter{}}
  return(plus(counter.doubled(), Counter.defaultStep()))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("generic struct helper resolves local templated root helper with implicit this reference") {
  const std::string source = R"(
[struct]
VecLike<T>() {
  [return<void>]
  check() {
    /vectorCheckShape<T>(this)
  }
}

[return<void>]
/vectorCheckShape<T>([Reference<VecLike<T>>] values) {
}

[return<int>]
main() {
  [VecLike<i32>] values{VecLike<i32>{}}
  values.check()
  return(0i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("mut helper allows assignment to this") {
  const std::string source = R"(
[struct]
Thing() {
  [i32] value{1i32}

  [mut return<void>]
  set([i32] v) {
    assign(this, Thing{v})
  }
}

[return<i32>]
main() {
  [Thing] t{Thing{}}
  t.set(7i32)
  return(0i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("mut is rejected on static helpers") {
  const std::string source = R"(
[struct]
Thing() {
  [i32] value{1i32}

  [static mut return<void>]
  set([i32] v) {
    return()
  }
}

[return<i32>]
main() {
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("mut transform is not allowed on static helpers") != std::string::npos);
}

TEST_CASE("an inferred binding in a generic Copy helper does not take the error span") {
  // Vector<T>.Copy infers `allocCount` from `other.fieldCapacity`; that speculative inference
  // must not leave its span behind for the unrelated error below.
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc) return<int>]
main() {
  [Vector<i32> mut] a{vector<i32>()}
  vectorPush<i32>(a, 4i32)
  [Vector<i32>] b{a}
  no_such_function(3i32)
  return(0i32)
}
)";
  primec::SemanticDiagnosticInfo diagnosticInfo;
  std::string error;
  CHECK_FALSE(validateProgramReportingDiagnostic(source, "/main", error, diagnosticInfo));
  CHECK(error.find("unknown call target: no_such_function") != std::string::npos);
  REQUIRE(diagnosticInfo.hasPrimarySpan);
  CHECK(diagnosticInfo.primarySpan.line == 9);
  CHECK(diagnosticInfo.primarySpan.column == 3);
  for (const auto &related : diagnosticInfo.relatedSpans) {
    CHECK(related.label.find("/Copy") == std::string::npos);
  }
}

// A method declared in the struct body overloads by arity with its implicit `this` counted, as a method call counts
// its receiver; a literal still converts to the StringView an overload expects.
TEST_CASE("struct body methods overload by arity and take literals as views") {
  const std::string source = R"(
import /std/text/view/*

[struct]
Sink() {
  [i32 mut] seen{0i32}

  [mut]
  put([StringView] text) {
    this.put(1i32, text)
  }

  [mut]
  put([i32] times, [StringView] text) {
    this.seen = this.seen + times * text.count()
  }
}

[return<int>]
main() {
  [Sink mut] sink{Sink{}}
  sink.put("ab")
  sink.put(3i32, "abc")
  return(sink.seen)
}
)";
  std::string error;
  CHECK_MESSAGE(validateProgram(source, "/main", error), error);
}

TEST_CASE("struct body methods with the same parameter types are still duplicates") {
  const std::string source = R"(
[struct]
Sink() {
  [i32 mut] seen{0i32}

  [mut]
  put([i32] amount) {
    assign(this.seen, amount)
  }

  [mut]
  put([i32] other) {
    assign(this.seen, other)
  }
}

[return<int>]
main() {
  [Sink mut] sink{Sink{}}
  sink.put(1i32)
  return(sink.seen)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK_MESSAGE(error.find("duplicate definition: /Sink/put") != std::string::npos, error);
}

TEST_SUITE_END();
