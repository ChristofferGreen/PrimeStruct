#include "third_party/doctest.h"

#include "test_semantics_helpers.h"

TEST_SUITE_BEGIN("primestruct.semantics.text_views");

// A `string` converts to a StringView where one is expected (docs/spec/strings-and-views.md, Current surface of the
// text view).

TEST_CASE("a string converts to a StringView parameter, field, binding, assignment and return") {
  const std::string source = R"(
import /std/text/view/*

[struct]
Label {
  [StringView] text{StringView{}}
}

[struct]
Counter {
  [i32] base{0i32}

  [return<i32>]
  add([StringView] text) {
    return(plus(this.base, text.count()))
  }
}

[return<i32>]
width([StringView] text) {
  return(text.count())
}

[return<i32>]
total([StringView] first, [args<StringView>] rest) {
  return(plus(first.count(), rest.count()))
}

[return<StringView>]
fallback() {
  return("none")
}

[return<int>]
main() {
  [string] word{"four"}
  [StringView] bound{"seven"}
  [StringView mut] later{StringView{}}
  later = word
  [Label] label{Label{[text] "label"}}
  [Counter] counter{Counter{}}
  [i32] sum{plus(width("hello"), width(word))}
  return(plus(plus(sum, counter.add("ab")), total("a", "b", word)))
}
)";
  std::string error;
  CHECK_MESSAGE(validateProgram(source, "/main", error), error);
}

TEST_CASE("a non-text argument for a StringView parameter is rejected") {
  const std::string source = R"(
import /std/text/view/*

[return<i32>]
width([StringView] text) {
  return(text.count())
}

[return<int>]
main() {
  return(width(42i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK_MESSAGE(error.find("argument type mismatch for /width parameter text: expected /std/text/view/StringView "
                           "got i32") != std::string::npos,
                error);
}

TEST_CASE("an owned String still needs view() for a StringView parameter") {
  const std::string source = R"(
import /std/text/*
import /std/text/view/*

[return<i32>]
width([StringView] text) {
  return(text.count())
}

[return<int> effects(heap_alloc)]
main() {
  [String] owned{stringFrom("abc")}
  return(width(owned))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK_MESSAGE(error.find("expected /std/text/view/StringView got /std/text/String") != std::string::npos, error);
}

TEST_CASE("a user struct named StringView does not take a string") {
  const std::string source = R"(
import /std/text/view/*

namespace app {
  [public struct]
  StringView() {
    [public i32] size{0i32}
  }

  [public return<i32>]
  width([StringView] text) {
    return(text.size)
  }
}

[return<int>]
main() {
  return(/app/width("abc"))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK_MESSAGE(error.find("argument type mismatch for /app/width parameter text: expected /app/StringView") !=
                    std::string::npos,
                error);
}

TEST_CASE("a string overload is the exact match next to a StringView overload") {
  const std::string source = R"(
import /std/text/view/*

[return<i32>]
describe([string] text) {
  return(1i32)
}

[return<i32>]
describe([StringView] text) {
  return(10i32)
}

[return<int>]
main() {
  [StringView] view{"view"}
  return(plus(describe("text"), describe(view)))
}
)";
  std::string error;
  CHECK_MESSAGE(validateProgram(source, "/main", error), error);
}

TEST_CASE("a string converts at a StringView field of a generic struct only") {
  const std::string head = R"(
import /std/text/view/*

[struct]
Route<T>() {
  [StringView] method{StringView{}}
  [T] state{}
}

[return<int>]
main() {
)";
  std::string error;
  const std::string accepted = head + R"(
  [string] name{"GET"}
  [Route<i32>] byValue{Route<i32>{[method] name, [state] 3i32}}
  [Route<i32>] byLiteral{Route<i32>{[method] "POST", [state] 4i32}}
  return(plus(byValue.state, byLiteral.state))
}
)";
  CHECK_MESSAGE(validateProgram(accepted, "/main", error), error);
  const std::string rejected = head + R"(
  [Route<i32>] bad{Route<i32>{[method] 5i32, [state] 3i32}}
  return(bad.state)
}
)";
  CHECK_FALSE(validateProgram(rejected, "/main", error));
  CHECK_MESSAGE(error.find("parameter method: expected /std/text/view/StringView got i32") !=
                    std::string::npos,
                error);
}

TEST_SUITE_END();
