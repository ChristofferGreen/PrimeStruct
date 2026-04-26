#include "test_semantics_helpers.h"

TEST_SUITE_BEGIN("primestruct.semantics.calls_flow.collections");

TEST_CASE("capacity method keeps user-defined array helper precedence") {
  const std::string source = R"(
[return<int>]
/array/capacity([array<i32>] values) {
  return(count(values))
}

[return<int>]
main() {
  [array<i32>] values{array<i32>(1i32, 2i32)}
  return(values.capacity())
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("capacity call keeps user-defined array helper precedence") {
  const std::string source = R"(
[return<int>]
/array/capacity([array<i32>] values) {
  return(plus(count(values), 33i32))
}

[return<int>]
main() {
  [array<i32>] values{array<i32>(1i32, 2i32)}
  return(capacity(values))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("count method keeps user-defined array helper precedence") {
  const std::string source = R"(
[return<int>]
/array/count([array<i32>] values) {
  return(plus(count(values), 10i32))
}

[return<int>]
main() {
  [array<i32>] values{array<i32>(1i32, 2i32)}
  return(values.count())
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("count call keeps user-defined array helper precedence") {
  const std::string source = R"(
[return<int>]
/array/count([array<i32>] values) {
  return(plus(count(values), 11i32))
}

[return<int>]
main() {
  [array<i32>] values{array<i32>(1i32, 2i32)}
  return(count(values))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("count call rejects user-defined map alias helper precedence") {
  const std::string source = R"(
[return<int>]
/map/count([map<i32, i32>] values) {
  return(plus(count(values), 12i32))
}

[return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 2i32)}
  return(count(values))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /std/collections/map/count") != std::string::npos);
}

TEST_CASE("count method requires canonical map helper even when alias helper exists") {
  const std::string source = R"(
[return<int>]
/map/count([map<i32, i32>] values) {
  return(93i32)
}

[return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 2i32)}
  return(values.count())
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /std/collections/map/count") != std::string::npos);
}

TEST_CASE("count call keeps user-defined string helper precedence") {
  const std::string source = R"(
[return<int>]
/string/count([string] values) {
  return(94i32)
}

[return<int>]
main() {
  [string] text{"abc"utf8}
  return(count(text))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("count method keeps user-defined string helper precedence") {
  const std::string source = R"(
[return<int>]
/string/count([string] values) {
  return(95i32)
}

[return<int>]
main() {
  [string] text{"abc"utf8}
  return(text.count())
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("count method keeps user-defined vector helper precedence") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/vector/count([vector<i32>] values) {
  return(97i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32)}
  return(values.count())
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("capacity method keeps user-defined vector helper precedence") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/vector/capacity([vector<i32>] values) {
  return(20i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32)}
  return(values.capacity())
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("count call keeps user-defined vector helper precedence") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/vector/count([vector<i32>] values) {
  return(21i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32)}
  return(count(values))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("capacity call keeps user-defined vector helper precedence") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/vector/capacity([vector<i32>] values) {
  return(20i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32)}
  return(capacity(values))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("bare vector count auto inference resolves through imported stdlib helper") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32)}
  [auto] inferred{count(values)}
  return(inferred)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("bare vector count auto inference keeps canonical unknown target") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32)}
  [auto] inferred{count(values)}
  return(inferred)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /std/collections/vector/count") != std::string::npos);
}

TEST_CASE("bare vector capacity auto inference resolves through imported stdlib helper") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32)}
  [auto] inferred{capacity(values)}
  return(inferred)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("bare vector capacity auto inference keeps canonical unknown target") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32)}
  [auto] inferred{capacity(values)}
  return(inferred)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /std/collections/vector/capacity") !=
        std::string::npos);
}

TEST_CASE("at call keeps user-defined array helper precedence") {
  const std::string source = R"(
[return<int>]
/array/at([array<i32>] values, [i32] index) {
  return(73i32)
}

[return<int>]
main() {
  [array<i32>] values{array<i32>(1i32, 2i32)}
  return(at(values, 1i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("at_unsafe call requires canonical map helper even when alias helper exists") {
  const std::string source = R"(
[return<int>]
/map/at_unsafe([map<i32, i32>] values, [i32] key) {
  return(74i32)
}

[return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 2i32)}
  return(at_unsafe(values, 1i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /std/collections/map/at_unsafe") != std::string::npos);
}

TEST_CASE("at method keeps user-defined array helper precedence") {
  const std::string source = R"(
[return<int>]
/array/at([array<i32>] values, [i32] index) {
  return(75i32)
}

[return<int>]
main() {
  [array<i32>] values{array<i32>(1i32, 2i32)}
  return(values.at(1i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("at_unsafe call keeps user-defined array helper precedence") {
  const std::string source = R"(
[return<int>]
/array/at_unsafe([array<i32>] values, [i32] index) {
  return(85i32)
}

[return<int>]
main() {
  [array<i32>] values{array<i32>(1i32, 2i32)}
  return(at_unsafe(values, 1i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("at_unsafe method keeps user-defined array helper precedence") {
  const std::string source = R"(
[return<int>]
/array/at_unsafe([array<i32>] values, [i32] index) {
  return(86i32)
}

[return<int>]
main() {
  [array<i32>] values{array<i32>(1i32, 2i32)}
  return(values.at_unsafe(1i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("at_unsafe method requires canonical map helper even when alias helper exists") {
  const std::string source = R"(
[return<int>]
/map/at_unsafe([map<i32, i32>] values, [i32] key) {
  return(76i32)
}

[return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 2i32)}
  return(values.at_unsafe(1i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /std/collections/map/at_unsafe") != std::string::npos);
}

TEST_CASE("at call requires canonical map helper even when alias helper exists") {
  const std::string source = R"(
[return<int>]
/map/at([map<i32, i32>] values, [i32] key) {
  return(67i32)
}

[return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 2i32)}
  return(at(values, 1i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /std/collections/map/at") != std::string::npos);
}

TEST_CASE("at method requires canonical map helper even when alias helper exists") {
  const std::string source = R"(
[return<int>]
/map/at([map<i32, i32>] values, [i32] key) {
  return(65i32)
}

[return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 2i32)}
  return(values.at(1i32))
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /std/collections/map/at") != std::string::npos);
}

TEST_CASE("at call keeps user-defined vector helper precedence") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/vector/at([vector<i32>] values, [i32] index) {
  return(77i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32)}
  return(at(values, 1i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("at method keeps user-defined vector helper precedence") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/vector/at([vector<i32>] values, [i32] index) {
  return(78i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32)}
  return(values.at(1i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("at_unsafe call keeps user-defined string helper precedence") {
  const std::string source = R"(
[return<int>]
/string/at_unsafe([string] values, [i32] index) {
  return(79i32)
}

[return<int>]
main() {
  [string] text{"abc"utf8}
  return(at_unsafe(text, 1i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("at_unsafe method keeps user-defined string helper precedence") {
  const std::string source = R"(
[return<int>]
/string/at_unsafe([string] values, [i32] index) {
  return(80i32)
}

[return<int>]
main() {
  [string] text{"abc"utf8}
  return(text.at_unsafe(1i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("at_unsafe call keeps user-defined vector helper precedence") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/vector/at_unsafe([vector<i32>] values, [i32] index) {
  return(81i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32)}
  return(at_unsafe(values, 1i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("at_unsafe method keeps user-defined vector helper precedence") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/vector/at_unsafe([vector<i32>] values, [i32] index) {
  return(82i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32)}
  return(values.at_unsafe(1i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("at call keeps user-defined string helper precedence") {
  const std::string source = R"(
[return<int>]
/string/at([string] values, [i32] index) {
  return(83i32)
}

[return<int>]
main() {
  [string] text{"abc"utf8}
  return(at(text, 1i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("at method keeps user-defined string helper precedence") {
  const std::string source = R"(
[return<int>]
/string/at([string] values, [i32] index) {
  return(84i32)
}

[return<int>]
main() {
  [string] text{"abc"utf8}
  return(text.at(1i32))
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("vector push alias requires same-path helper before mutability") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32)}
  /vector/push(values, 2i32)
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /vector/push") != std::string::npos);
}

TEST_CASE("vector push alias requires same-path helper before effects") {
  const std::string source = R"(
[return<int>]
main() {
  [vector<i32> mut] values{vector<i32>()}
  /vector/push(values, 2i32)
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK(error.find("unknown call target: /vector/push") != std::string::npos);
}

TEST_CASE("push on array reports vector binding before effect requirement") {
  const auto checkInvalidPush = [](const std::string &stmtText) {
    const std::string source =
        "[return<int>]\n"
        "main() {\n"
        "  [array<i32> mut] values{array<i32>(1i32)}\n"
        "  " +
        stmtText +
        "\n"
        "  return(0i32)\n"
        "}\n";
    std::string error;
    CHECK_FALSE(validateProgram(source, "/main", error));
    CHECK(error.find("push requires vector binding") != std::string::npos);
  };

  checkInvalidPush("push(values, 2i32)");
  checkInvalidPush("values.push(2i32)");
}

TEST_CASE("bare vector push validates through imported stdlib helper") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32)}
  push(values, 2i32)
  return(0i32)
}
)";
  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_SUITE_END();
