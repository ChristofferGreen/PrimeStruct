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
    "parses semantic transform method-call after nested indexed template body chain") {
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
    "parses semantic transform indexing after nested indexed template body chain method-call") {
  const std::string source = R"(
[semantic(tag(fetch_values(1i32).count()[2i32].pick<i32>([index] 3i32) { foo(4i32) }[5i32].count(6i32)[7i32]))]
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
      "at(at(at(fetch_values(1i32).count(), 2i32).pick<i32>([index] 3i32) { foo(4i32) }, 5i32).count(6i32), 7i32)");
}

TEST_CASE(
    "parses semantic transform method-call after nested indexed template body chain indexed method-call") {
  const std::string source = R"(
[semantic(tag(fetch_values(1i32).count()[2i32].pick<i32>([index] 3i32) { foo(4i32) }[5i32].count(6i32)[7i32].count(8i32)))]
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
      "at(at(at(fetch_values(1i32).count(), 2i32).pick<i32>([index] 3i32) { foo(4i32) }, 5i32).count(6i32), 7i32).count(8i32)");
}

TEST_CASE(
    "parses semantic transform indexing after nested indexed template body chain indexed method-call") {
  const std::string source = R"(
[semantic(tag(fetch_values(1i32).count()[2i32].pick<i32>([index] 3i32) { foo(4i32) }[5i32].count(6i32)[7i32].count(8i32)[9i32]))]
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
      "at(at(at(at(fetch_values(1i32).count(), 2i32).pick<i32>([index] 3i32) { foo(4i32) }, 5i32).count(6i32), 7i32).count(8i32), 9i32)");
}

TEST_CASE(
    "parses semantic transform field-access after nested indexed template body chain indexed method-call") {
  const std::string source = R"(
[semantic(tag(fetch_values(1i32).count()[2i32].pick<i32>([index] 3i32) { foo(4i32) }[5i32].count(6i32)[7i32].count(8i32)[9i32].count))]
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
      "at(at(at(at(fetch_values(1i32).count(), 2i32).pick<i32>([index] 3i32) { foo(4i32) }, 5i32).count(6i32), 7i32).count(8i32), 9i32).count()");
}

TEST_CASE(
    "parses semantic transform indexing after nested indexed template body chain indexed method-call field-access") {
  const std::string source = R"(
[semantic(tag(fetch_values(1i32).count()[2i32].pick<i32>([index] 3i32) { foo(4i32) }[5i32].count(6i32)[7i32].count(8i32)[9i32].count[10i32]))]
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
      "at(at(at(at(at(fetch_values(1i32).count(), 2i32).pick<i32>([index] 3i32) { foo(4i32) }, 5i32).count(6i32), 7i32).count(8i32), 9i32).count(), 10i32)");
}

TEST_CASE(
    "parses semantic transform method-call after nested indexed template body chain indexed method-call field-access") {
  const std::string source = R"(
[semantic(tag(fetch_values(1i32).count()[2i32].pick<i32>([index] 3i32) { foo(4i32) }[5i32].count(6i32)[7i32].count(8i32)[9i32].count[10i32].count(11i32)))]
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
      "at(at(at(at(at(fetch_values(1i32).count(), 2i32).pick<i32>([index] 3i32) { foo(4i32) }, 5i32).count(6i32), 7i32).count(8i32), 9i32).count(), 10i32).count(11i32)");
}

TEST_CASE(
    "parses semantic transform indexing after nested indexed template body chain indexed method-call field-access tail") {
  const std::string source = R"(
[semantic(tag(fetch_values(1i32).count()[2i32].pick<i32>([index] 3i32) { foo(4i32) }[5i32].count(6i32)[7i32].count(8i32)[9i32].count[10i32].count(11i32)[12i32]))]
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
      "at(at(at(at(at(at(fetch_values(1i32).count(), 2i32).pick<i32>([index] 3i32) { foo(4i32) }, 5i32).count(6i32), 7i32).count(8i32), 9i32).count(), 10i32).count(11i32), 12i32)");
}

TEST_CASE(
    "parses semantic transform field-access after nested indexed template body chain indexed method-call field-access tail") {
  const std::string source = R"(
[semantic(tag(fetch_values(1i32).count()[2i32].pick<i32>([index] 3i32) { foo(4i32) }[5i32].count(6i32)[7i32].count(8i32)[9i32].count[10i32].count(11i32)[12i32].count))]
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
      "at(at(at(at(at(at(fetch_values(1i32).count(), 2i32).pick<i32>([index] 3i32) { foo(4i32) }, 5i32).count(6i32), 7i32).count(8i32), 9i32).count(), 10i32).count(11i32), 12i32).count()");
}

TEST_CASE(
    "parses semantic transform indexing after nested indexed template body chain indexed method-call field-access tail field-access") {
  const std::string source = R"(
[semantic(tag(fetch_values(1i32).count()[2i32].pick<i32>([index] 3i32) { foo(4i32) }[5i32].count(6i32)[7i32].count(8i32)[9i32].count[10i32].count(11i32)[12i32].count[13i32]))]
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
      "at(at(at(at(at(at(at(fetch_values(1i32).count(), 2i32).pick<i32>([index] 3i32) { foo(4i32) }, 5i32).count(6i32), 7i32).count(8i32), 9i32).count(), 10i32).count(11i32), 12i32).count(), 13i32)");
}

TEST_CASE(
    "parses semantic transform method-call after nested indexed template body chain indexed method-call field-access tail field-access") {
  const std::string source = R"(
[semantic(tag(fetch_values(1i32).count()[2i32].pick<i32>([index] 3i32) { foo(4i32) }[5i32].count(6i32)[7i32].count(8i32)[9i32].count[10i32].count(11i32)[12i32].count[13i32].count(14i32)))]
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
      "at(at(at(at(at(at(at(fetch_values(1i32).count(), 2i32).pick<i32>([index] 3i32) { foo(4i32) }, 5i32).count(6i32), 7i32).count(8i32), 9i32).count(), 10i32).count(11i32), 12i32).count(), 13i32).count(14i32)");
}

TEST_CASE(
    "parses semantic transform field-access after nested indexed template body chain indexed method-call field-access tail indexed method-call field-access") {
  const std::string source = R"(
[semantic(tag(fetch_values(1i32).count()[2i32].pick<i32>([index] 3i32) { foo(4i32) }[5i32].count(6i32)[7i32].count(8i32)[9i32].count[10i32].count(11i32)[12i32].count[13i32].count(14i32).count))]
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
      "at(at(at(at(at(at(at(fetch_values(1i32).count(), 2i32).pick<i32>([index] 3i32) { foo(4i32) }, 5i32).count(6i32), 7i32).count(8i32), 9i32).count(), 10i32).count(11i32), 12i32).count(), 13i32).count(14i32).count()");
}

TEST_CASE(
    "parses semantic transform indexing after nested indexed template body chain indexed method-call field-access tail indexed method-call field-access") {
  const std::string source = R"(
[semantic(tag(fetch_values(1i32).count()[2i32].pick<i32>([index] 3i32) { foo(4i32) }[5i32].count(6i32)[7i32].count(8i32)[9i32].count[10i32].count(11i32)[12i32].count[13i32].count(14i32).count[15i32]))]
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
      "at(at(at(at(at(at(at(at(fetch_values(1i32).count(), 2i32).pick<i32>([index] 3i32) { foo(4i32) }, 5i32).count(6i32), 7i32).count(8i32), 9i32).count(), 10i32).count(11i32), 12i32).count(), 13i32).count(14i32).count(), 15i32)");
}

TEST_CASE(
    "parses semantic transform method-call after nested indexed template body chain indexed method-call field-access tail indexed method-call field-access index") {
  const std::string source = R"(
[semantic(tag(fetch_values(1i32).count()[2i32].pick<i32>([index] 3i32) { foo(4i32) }[5i32].count(6i32)[7i32].count(8i32)[9i32].count[10i32].count(11i32)[12i32].count[13i32].count(14i32).count[15i32].count(16i32)))]
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
      "at(at(at(at(at(at(at(at(fetch_values(1i32).count(), 2i32).pick<i32>([index] 3i32) { foo(4i32) }, 5i32).count(6i32), 7i32).count(8i32), 9i32).count(), 10i32).count(11i32), 12i32).count(), 13i32).count(14i32).count(), 15i32).count(16i32)");
}

TEST_CASE(
    "parses semantic transform indexing after nested indexed template body chain indexed method-call field-access tail indexed method-call field-access index method-call") {
  const std::string source = R"(
[semantic(tag(fetch_values(1i32).count()[2i32].pick<i32>([index] 3i32) { foo(4i32) }[5i32].count(6i32)[7i32].count(8i32)[9i32].count[10i32].count(11i32)[12i32].count[13i32].count(14i32).count[15i32].count(16i32)[17i32]))]
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
      "at(at(at(at(at(at(at(at(at(fetch_values(1i32).count(), 2i32).pick<i32>([index] 3i32) { foo(4i32) }, 5i32).count(6i32), 7i32).count(8i32), 9i32).count(), 10i32).count(11i32), 12i32).count(), 13i32).count(14i32).count(), 15i32).count(16i32), 17i32)");
}

TEST_CASE(
    "parses semantic transform field-access after nested indexed template body chain indexed method-call field-access tail indexed method-call field-access index method-call") {
  const std::string source = R"(
[semantic(tag(fetch_values(1i32).count()[2i32].pick<i32>([index] 3i32) { foo(4i32) }[5i32].count(6i32)[7i32].count(8i32)[9i32].count[10i32].count(11i32)[12i32].count[13i32].count(14i32).count[15i32].count(16i32)[17i32].count))]
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
      "at(at(at(at(at(at(at(at(at(fetch_values(1i32).count(), 2i32).pick<i32>([index] 3i32) { foo(4i32) }, 5i32).count(6i32), 7i32).count(8i32), 9i32).count(), 10i32).count(11i32), 12i32).count(), 13i32).count(14i32).count(), 15i32).count(16i32), 17i32).count()");
}

TEST_CASE(
    "parses semantic transform indexing after nested indexed template body chain indexed method-call field-access tail indexed method-call field-access index method-call field-access") {
  const std::string source = R"(
[semantic(tag(fetch_values(1i32).count()[2i32].pick<i32>([index] 3i32) { foo(4i32) }[5i32].count(6i32)[7i32].count(8i32)[9i32].count[10i32].count(11i32)[12i32].count[13i32].count(14i32).count[15i32].count(16i32)[17i32].count[18i32]))]
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
      "at(at(at(at(at(at(at(at(at(at(fetch_values(1i32).count(), 2i32).pick<i32>([index] 3i32) { foo(4i32) }, 5i32).count(6i32), 7i32).count(8i32), 9i32).count(), 10i32).count(11i32), 12i32).count(), 13i32).count(14i32).count(), 15i32).count(16i32), 17i32).count(), 18i32)");
}

TEST_CASE(
    "parses semantic transform method-call after nested indexed template body chain indexed method-call field-access tail indexed method-call field-access index method-call field-access index") {
  const std::string source = R"(
[semantic(tag(fetch_values(1i32).count()[2i32].pick<i32>([index] 3i32) { foo(4i32) }[5i32].count(6i32)[7i32].count(8i32)[9i32].count[10i32].count(11i32)[12i32].count[13i32].count(14i32).count[15i32].count(16i32)[17i32].count[18i32].count(19i32)))]
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
      "at(at(at(at(at(at(at(at(at(at(fetch_values(1i32).count(), 2i32).pick<i32>([index] 3i32) { foo(4i32) }, 5i32).count(6i32), 7i32).count(8i32), 9i32).count(), 10i32).count(11i32), 12i32).count(), 13i32).count(14i32).count(), 15i32).count(16i32), 17i32).count(), 18i32).count(19i32)");
}

TEST_CASE(
    "parses semantic transform indexing after nested indexed template body chain indexed method-call field-access tail indexed method-call field-access index method-call field-access index method-call") {
  const std::string source = R"(
[semantic(tag(fetch_values(1i32).count()[2i32].pick<i32>([index] 3i32) { foo(4i32) }[5i32].count(6i32)[7i32].count(8i32)[9i32].count[10i32].count(11i32)[12i32].count[13i32].count(14i32).count[15i32].count(16i32)[17i32].count[18i32].count(19i32)[20i32]))]
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
      "at(at(at(at(at(at(at(at(at(at(at(fetch_values(1i32).count(), 2i32).pick<i32>([index] 3i32) { foo(4i32) }, 5i32).count(6i32), 7i32).count(8i32), 9i32).count(), 10i32).count(11i32), 12i32).count(), 13i32).count(14i32).count(), 15i32).count(16i32), 17i32).count(), 18i32).count(19i32), 20i32)");
}

TEST_CASE(
    "parses semantic transform field-access after nested indexed template body chain indexed method-call field-access tail indexed method-call field-access index method-call field-access index method-call") {
  const std::string source = R"(
[semantic(tag(fetch_values(1i32).count()[2i32].pick<i32>([index] 3i32) { foo(4i32) }[5i32].count(6i32)[7i32].count(8i32)[9i32].count[10i32].count(11i32)[12i32].count[13i32].count(14i32).count[15i32].count(16i32)[17i32].count[18i32].count(19i32)[20i32].count))]
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
      "at(at(at(at(at(at(at(at(at(at(at(fetch_values(1i32).count(), 2i32).pick<i32>([index] 3i32) { foo(4i32) }, 5i32).count(6i32), 7i32).count(8i32), 9i32).count(), 10i32).count(11i32), 12i32).count(), 13i32).count(14i32).count(), 15i32).count(16i32), 17i32).count(), 18i32).count(19i32), 20i32).count()");
}

TEST_CASE(
    "parses semantic transform indexing after nested indexed template body chain indexed method-call field-access tail indexed method-call field-access index method-call field-access index method-call field-access") {
  const std::string source = R"(
[semantic(tag(fetch_values(1i32).count()[2i32].pick<i32>([index] 3i32) { foo(4i32) }[5i32].count(6i32)[7i32].count(8i32)[9i32].count[10i32].count(11i32)[12i32].count[13i32].count(14i32).count[15i32].count(16i32)[17i32].count[18i32].count(19i32)[20i32].count[21i32]))]
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
      "at(at(at(at(at(at(at(at(at(at(at(at(fetch_values(1i32).count(), 2i32).pick<i32>([index] 3i32) { foo(4i32) }, 5i32).count(6i32), 7i32).count(8i32), 9i32).count(), 10i32).count(11i32), 12i32).count(), 13i32).count(14i32).count(), 15i32).count(16i32), 17i32).count(), 18i32).count(19i32), 20i32).count(), 21i32)");
}

TEST_CASE(
    "parses semantic transform method-call after nested indexed template body chain indexed method-call field-access tail indexed method-call field-access index method-call field-access index method-call field-access index") {
  const std::string source = R"(
[semantic(tag(fetch_values(1i32).count()[2i32].pick<i32>([index] 3i32) { foo(4i32) }[5i32].count(6i32)[7i32].count(8i32)[9i32].count[10i32].count(11i32)[12i32].count[13i32].count(14i32).count[15i32].count(16i32)[17i32].count[18i32].count(19i32)[20i32].count[21i32].count(22i32)))]
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
      "at(at(at(at(at(at(at(at(at(at(at(at(fetch_values(1i32).count(), 2i32).pick<i32>([index] 3i32) { foo(4i32) }, 5i32).count(6i32), 7i32).count(8i32), 9i32).count(), 10i32).count(11i32), 12i32).count(), 13i32).count(14i32).count(), 15i32).count(16i32), 17i32).count(), 18i32).count(19i32), 20i32).count(), 21i32).count(22i32)");
}

TEST_CASE(
    "parses semantic transform field-access after nested indexed template body chain indexed method-call field-access tail indexed method-call field-access index method-call field-access index method-call field-access index") {
  const std::string source = R"(
[semantic(tag(fetch_values(1i32).count()[2i32].pick<i32>([index] 3i32) { foo(4i32) }[5i32].count(6i32)[7i32].count(8i32)[9i32].count[10i32].count(11i32)[12i32].count[13i32].count(14i32).count[15i32].count(16i32)[17i32].count[18i32].count(19i32)[20i32].count[21i32].count(22i32).count))]
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
      "at(at(at(at(at(at(at(at(at(at(at(at(fetch_values(1i32).count(), 2i32).pick<i32>([index] 3i32) { foo(4i32) }, 5i32).count(6i32), 7i32).count(8i32), 9i32).count(), 10i32).count(11i32), 12i32).count(), 13i32).count(14i32).count(), 15i32).count(16i32), 17i32).count(), 18i32).count(19i32), 20i32).count(), 21i32).count(22i32).count()");
}

TEST_CASE(
    "parses semantic transform indexing after nested indexed template body chain indexed method-call field-access tail indexed method-call field-access index method-call field-access index method-call field-access index") {
  const std::string source = R"(
[semantic(tag(fetch_values(1i32).count()[2i32].pick<i32>([index] 3i32) { foo(4i32) }[5i32].count(6i32)[7i32].count(8i32)[9i32].count[10i32].count(11i32)[12i32].count[13i32].count(14i32).count[15i32].count(16i32)[17i32].count[18i32].count(19i32)[20i32].count[21i32].count(22i32).count[23i32]))]
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
      "at(at(at(at(at(at(at(at(at(at(at(at(at(fetch_values(1i32).count(), 2i32).pick<i32>([index] 3i32) { foo(4i32) }, 5i32).count(6i32), 7i32).count(8i32), 9i32).count(), 10i32).count(11i32), 12i32).count(), 13i32).count(14i32).count(), 15i32).count(16i32), 17i32).count(), 18i32).count(19i32), 20i32).count(), 21i32).count(22i32).count(), 23i32)");
}

TEST_CASE(
    "parses semantic transform method-call after nested indexed template body chain indexed method-call field-access tail indexed method-call field-access index method-call field-access index method-call field-access index method-call") {
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
