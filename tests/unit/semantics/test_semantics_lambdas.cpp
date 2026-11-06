#include "third_party/doctest.h"

#include "test_semantics_helpers.h"

TEST_SUITE_BEGIN("primestruct.semantics.lambdas");

TEST_CASE("lambda expressions validate without captures") {
  const std::string source = R"(
[return<void>]
main() {
  []([i32] value) { return(plus(value, 1i32)) }
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
}

TEST_CASE("lambda captures allow outer bindings") {
  const std::string source = R"(
[return<void>]
main() {
  [i32] base{1i32}
  [value base]([i32] offset) { return(plus(base, offset)) }
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
}

TEST_CASE("lambda captures accept ref qualifier") {
  const std::string source = R"(
[return<void>]
main() {
  [i32 mut] base{1i32}
  [ref base]([i32] offset) { return(plus(base, offset)) }
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
}

TEST_CASE("lambda without capture rejects outer binding") {
  const std::string source = R"(
[return<void>]
main() {
  [i32] base{1i32}
  []([i32] offset) { return(plus(base, offset)) }
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown identifier: base") != std::string::npos);
}

TEST_CASE("lambda rejects unknown capture name") {
  const std::string source = R"(
[return<void>]
main() {
  [value missing]([i32] offset) { return(offset) }
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown capture: missing") != std::string::npos);
}

TEST_CASE("lambda rejects invalid capture qualifier") {
  const std::string source = R"(
[return<void>]
main() {
  [i32] base{1i32}
  [foo base]([i32] offset) { return(offset) }
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("invalid lambda capture") != std::string::npos);
}

TEST_CASE("lambda parameter default rejects user-defined array call") {
  const std::string source = R"(
[return<i32>]
array([i32] value) {
  return(value)
}

[return<void>]
main() {
  []([i32] value{array(1i32)}) { return(value) }
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("parameter default must be a literal or pure expression") != std::string::npos);
}

TEST_CASE("lambda parameter default allows builtin array literal") {
  const std::string source = R"(
[return<void>]
main() {
  []([array<i32>] value{array<i32>(1i32, 2i32)}) { return(at(value, 0i32)) }
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
}

TEST_CASE("lambda rejects conflicting capture-all tokens") {
  const std::string source = R"(
[return<void>]
main() {
  [i32] base{1i32}
  [=, &]([i32] offset) { return(offset) }
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("invalid lambda capture") != std::string::npos);
}

TEST_CASE("lambda capture-all allows explicit known names") {
  const std::string source = R"(
[return<void>]
main() {
  [i32] base{1i32}
  [=, value base]([i32] offset) { return(plus(base, offset)) }
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
}

TEST_CASE("lambda capture-all rejects explicit unknown names") {
  const std::string source = R"(
[return<void>]
main() {
  [i32] base{1i32}
  [=, value missing]([i32] offset) { return(offset) }
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown capture: missing") != std::string::npos);
}

TEST_CASE("lambda calls accept named arguments") {
  // A closure call is a call of its `call` method, so arguments may be labeled like any method call.
  const std::string source = R"(
[return<int>]
main() {
  holder{[]([i32] value) { return(value) }}
  return(holder([value] 1i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
}

TEST_CASE("lambda rejects writing a capture that is not mut") {
  const std::string source = R"(
[return<int>]
main() {
  [i32 mut] count{0i32}
  bump{[count]() {
    assign(count, plus(count, 1i32))
    return(count)
  }}
  return(bump())
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("assign target must be a mutable binding") != std::string::npos);
}

TEST_CASE("ref capture is a loan on the captured binding until the closure's last use") {
  const std::string source = R"(
import /std/collections/*

[return<int> effects(heap_alloc)]
main() {
  [vector<i32> mut] values{vector<i32>(1i32, 2i32)}
  reader{[ref values]() { return(values.count()) }}
  values.push(3i32)
  return(reader())
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("borrowed binding: values (root: values, sink: reader)") != std::string::npos);
}

TEST_CASE("ref capture allows changes after the closure's last use") {
  const std::string source = R"(
import /std/collections/*

[return<int> effects(heap_alloc)]
main() {
  [vector<i32> mut] values{vector<i32>(1i32, 2i32)}
  reader{[ref values]() { return(values.count()) }}
  [i32] before{reader()}
  values.push(3i32)
  return(plus(before, values.count()))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
}

TEST_CASE("closure holding a ref capture cannot become a Function") {
  const std::string source = R"(
[return<int> effects(heap_alloc)]
main() {
  [i32 mut] hits{0i32}
  reader{[ref hits]() { return(hits) }}
  [Function<i32> mut] erased{Function<i32>{reader}}
  return(erased())
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("closure escapes via Function (root: hits)") != std::string::npos);
}

TEST_CASE("inline ref lambda cannot become a Function") {
  const std::string source = R"(
[return<int> effects(heap_alloc)]
main() {
  [i32 mut] hits{0i32}
  [Function<i32> mut] erased{Function<i32>{[ref hits]() { return(hits) }}}
  return(erased())
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("closure escapes via Function (root: hits)") != std::string::npos);
}

TEST_CASE("closure capturing a view by value borrows the view's owner") {
  const std::string source = R"(
import /std/text/*
import /std/text/view/*

[return<int> effects(heap_alloc)]
main() {
  [String mut] text{/std/text/stringFrom("hello")}
  [StringView] view{text.view()}
  reader{[view]() { return(view.count()) }}
  text.append("!")
  return(reader())
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("borrowed binding: text (root: text, sink: reader)") != std::string::npos);
}

TEST_SUITE_END();
