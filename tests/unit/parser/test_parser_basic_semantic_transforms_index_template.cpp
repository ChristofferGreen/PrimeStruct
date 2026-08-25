#include "test_parser_basic_helpers.h"

TEST_SUITE_BEGIN("primestruct.parser.basic");

TEST_CASE(
    "parses semantic transform index then template method-call on method-call receiver") {
  const std::string source = R"(
[semantic(tag(fetch_values(1i32).count()[2i32].pick<i32>(3i32)))]
main() {
  return(1i32)
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  const auto &transforms = program.definitions[0].transforms;
  REQUIRE(transforms.size() == 1);
  REQUIRE(transforms[0].arguments.size() == 1);
  CHECK(transforms[0].arguments[0] ==
        "at(fetch_values(1i32).count(), 2i32).pick<i32>(3i32)");
}

TEST_CASE(
    "parses semantic transform index then template named method-call on method-call receiver") {
  const std::string source = R"(
[semantic(tag(fetch_values(1i32).count()[2i32].pick<i32>([index] 3i32)))]
main() {
  return(1i32)
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  const auto &transforms = program.definitions[0].transforms;
  REQUIRE(transforms.size() == 1);
  REQUIRE(transforms[0].arguments.size() == 1);
  CHECK(transforms[0].arguments[0] ==
        "at(fetch_values(1i32).count(), 2i32).pick<i32>([index] 3i32)");
}

TEST_CASE(
    "parses semantic transform index then template method-call body on method-call receiver") {
  const std::string source = R"(
[semantic(tag(fetch_values(1i32).count()[2i32].pick<i32>(3i32) { foo(4i32) }))]
main() {
  return(1i32)
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  const auto &transforms = program.definitions[0].transforms;
  REQUIRE(transforms.size() == 1);
  REQUIRE(transforms[0].arguments.size() == 1);
  CHECK(transforms[0].arguments[0] ==
        "at(fetch_values(1i32).count(), 2i32).pick<i32>(3i32) { foo(4i32) }");
}

TEST_CASE(
    "parses semantic transform index then template named method-call body on method-call receiver") {
  const std::string source = R"(
[semantic(tag(fetch_values(1i32).count()[2i32].pick<i32>([index] 3i32) { foo(4i32) }))]
main() {
  return(1i32)
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  const auto &transforms = program.definitions[0].transforms;
  REQUIRE(transforms.size() == 1);
  REQUIRE(transforms[0].arguments.size() == 1);
  CHECK(transforms[0].arguments[0] ==
        "at(fetch_values(1i32).count(), 2i32).pick<i32>([index] 3i32) { foo(4i32) }");
}

TEST_CASE(
    "parses semantic transform index then template method-call no-arg body on method-call receiver") {
  const std::string source = R"(
[semantic(tag(fetch_values(1i32).count()[2i32].pick<i32>() { foo(4i32) }))]
main() {
  return(1i32)
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  const auto &transforms = program.definitions[0].transforms;
  REQUIRE(transforms.size() == 1);
  REQUIRE(transforms[0].arguments.size() == 1);
  CHECK(transforms[0].arguments[0] ==
        "at(fetch_values(1i32).count(), 2i32).pick<i32>() { foo(4i32) }");
}

TEST_CASE(
    "parses semantic transform index after template method-call no-arg body on method-call receiver") {
  const std::string source = R"(
[semantic(tag(fetch_values(1i32).count()[2i32].pick<i32>() { foo(4i32) }[5i32]))]
main() {
  return(1i32)
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  const auto &transforms = program.definitions[0].transforms;
  REQUIRE(transforms.size() == 1);
  REQUIRE(transforms[0].arguments.size() == 1);
  CHECK(transforms[0].arguments[0] ==
        "at(at(fetch_values(1i32).count(), 2i32).pick<i32>() { foo(4i32) }, 5i32)");
}

TEST_CASE(
    "parses semantic transform index after template named method-call body on method-call receiver") {
  const std::string source = R"(
[semantic(tag(fetch_values(1i32).count()[2i32].pick<i32>([index] 3i32) { foo(4i32) }[5i32]))]
main() {
  return(1i32)
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  const auto &transforms = program.definitions[0].transforms;
  REQUIRE(transforms.size() == 1);
  REQUIRE(transforms[0].arguments.size() == 1);
  CHECK(transforms[0].arguments[0] ==
        "at(at(fetch_values(1i32).count(), 2i32).pick<i32>([index] 3i32) { foo(4i32) }, 5i32)");
}

TEST_CASE(
    "parses semantic transform method-call after nested indexed template body chain through 6i32") {
  const std::string source = R"(
[semantic(tag(fetch_values(1i32).count()[2i32].pick<i32>([index] 3i32) { foo(4i32) }[5i32].count(6i32)))]
main() {
  return(1i32)
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  const auto &transforms = program.definitions[0].transforms;
  REQUIRE(transforms.size() == 1);
  REQUIRE(transforms[0].arguments.size() == 1);
  CHECK(
      transforms[0].arguments[0] ==
      "at(at(fetch_values(1i32).count(), 2i32).pick<i32>([index] 3i32) { foo(4i32) }, 5i32).count(6i32)");
}

TEST_CASE(
    "parses semantic transform method-call after nested indexed template body chain through 24i32") {
  const std::string source = R"(
[semantic(tag(fetch_values(1i32).count()[2i32].pick<i32>([index] 3i32) { foo(4i32) }[5i32].count(6i32)[7i32].count(8i32)[9i32].count[10i32].count(11i32)[12i32].count[13i32].count(14i32).count[15i32].count(16i32)[17i32].count[18i32].count(19i32)[20i32].count[21i32].count(22i32).count[23i32].count(24i32)))]
main() {
  return(1i32)
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  const auto &transforms = program.definitions[0].transforms;
  REQUIRE(transforms.size() == 1);
  REQUIRE(transforms[0].arguments.size() == 1);
  CHECK(
      transforms[0].arguments[0] ==
      "at(at(at(at(at(at(at(at(at(at(at(at(at(fetch_values(1i32).count(), 2i32).pick<i32>([index] 3i32) { foo(4i32) }, 5i32).count(6i32), 7i32).count(8i32), 9i32).count(), 10i32).count(11i32), 12i32).count(), 13i32).count(14i32).count(), 15i32).count(16i32), 17i32).count(), 18i32).count(19i32), 20i32).count(), 21i32).count(22i32).count(), 23i32).count(24i32)");
}

TEST_SUITE_END();
