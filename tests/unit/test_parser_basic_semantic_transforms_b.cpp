#include "test_parser_basic_helpers.h"

TEST_SUITE_BEGIN("primestruct.parser.basic");

TEST_CASE(
    "parses semantic transform indexing after nested indexed template body chain indexed method-call field-access tail indexed method-call field-access index method-call field-access index method-call field-access index method-call field-access") {
  const std::string source = R"(
[semantic(tag(fetch_values(1i32).count()[2i32].pick<i32>([index] 3i32) { foo(4i32) }[5i32].count(6i32)[7i32].count(8i32)[9i32].count[10i32].count(11i32)[12i32].count[13i32].count(14i32).count[15i32].count(16i32)[17i32].count[18i32].count(19i32)[20i32].count[21i32].count(22i32).count[23i32].count(24i32)[25i32]))]
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
      "at(at(at(at(at(at(at(at(at(at(at(at(at(at(fetch_values(1i32).count(), 2i32).pick<i32>([index] 3i32) { foo(4i32) }, 5i32).count(6i32), 7i32).count(8i32), 9i32).count(), 10i32).count(11i32), 12i32).count(), 13i32).count(14i32).count(), 15i32).count(16i32), 17i32).count(), 18i32).count(19i32), 20i32).count(), 21i32).count(22i32).count(), 23i32).count(24i32), 25i32)");
}

TEST_CASE(
    "parses semantic transform field-access after nested indexed template body chain indexed method-call field-access tail indexed method-call field-access index method-call field-access index method-call field-access index method-call") {
  const std::string source = R"(
[semantic(tag(fetch_values(1i32).count()[2i32].pick<i32>([index] 3i32) { foo(4i32) }[5i32].count(6i32)[7i32].count(8i32)[9i32].count[10i32].count(11i32)[12i32].count[13i32].count(14i32).count[15i32].count(16i32)[17i32].count[18i32].count(19i32)[20i32].count[21i32].count(22i32).count[23i32].count(24i32)[25i32].count))]
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
      "at(at(at(at(at(at(at(at(at(at(at(at(at(at(fetch_values(1i32).count(), 2i32).pick<i32>([index] 3i32) { foo(4i32) }, 5i32).count(6i32), 7i32).count(8i32), 9i32).count(), 10i32).count(11i32), 12i32).count(), 13i32).count(14i32).count(), 15i32).count(16i32), 17i32).count(), 18i32).count(19i32), 20i32).count(), 21i32).count(22i32).count(), 23i32).count(24i32), 25i32).count()");
}

TEST_CASE(
    "parses semantic transform indexing after nested indexed template body chain indexed method-call field-access tail indexed method-call field-access index method-call field-access index method-call field-access index method-call") {
  const std::string source = R"(
[semantic(tag(fetch_values(1i32).count()[2i32].pick<i32>([index] 3i32) { foo(4i32) }[5i32].count(6i32)[7i32].count(8i32)[9i32].count[10i32].count(11i32)[12i32].count[13i32].count(14i32).count[15i32].count(16i32)[17i32].count[18i32].count(19i32)[20i32].count[21i32].count(22i32).count[23i32].count(24i32)[25i32].count[26i32]))]
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
      "at(at(at(at(at(at(at(at(at(at(at(at(at(at(at(fetch_values(1i32).count(), 2i32).pick<i32>([index] 3i32) { foo(4i32) }, 5i32).count(6i32), 7i32).count(8i32), 9i32).count(), 10i32).count(11i32), 12i32).count(), 13i32).count(14i32).count(), 15i32).count(16i32), 17i32).count(), 18i32).count(19i32), 20i32).count(), 21i32).count(22i32).count(), 23i32).count(24i32), 25i32).count(), 26i32)");
}

TEST_CASE(
    "parses semantic transform method-call after nested indexed template body chain indexed method-call field-access tail indexed method-call field-access index method-call field-access index method-call field-access index method-call field-access") {
  const std::string source = R"(
[semantic(tag(fetch_values(1i32).count()[2i32].pick<i32>([index] 3i32) { foo(4i32) }[5i32].count(6i32)[7i32].count(8i32)[9i32].count[10i32].count(11i32)[12i32].count[13i32].count(14i32).count[15i32].count(16i32)[17i32].count[18i32].count(19i32)[20i32].count[21i32].count(22i32).count[23i32].count(24i32)[25i32].count[26i32].count(27i32)))]
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
      "at(at(at(at(at(at(at(at(at(at(at(at(at(at(at(fetch_values(1i32).count(), 2i32).pick<i32>([index] 3i32) { foo(4i32) }, 5i32).count(6i32), 7i32).count(8i32), 9i32).count(), 10i32).count(11i32), 12i32).count(), 13i32).count(14i32).count(), 15i32).count(16i32), 17i32).count(), 18i32).count(19i32), 20i32).count(), 21i32).count(22i32).count(), 23i32).count(24i32), 25i32).count(), 26i32).count(27i32)");
}

TEST_CASE(
    "parses semantic transform indexing after nested indexed template body chain indexed method-call field-access tail indexed method-call field-access index method-call field-access index method-call field-access index method-call field-access method-call field-access") {
  const std::string source = R"(
[semantic(tag(fetch_values(1i32).count()[2i32].pick<i32>([index] 3i32) { foo(4i32) }[5i32].count(6i32)[7i32].count(8i32)[9i32].count[10i32].count(11i32)[12i32].count[13i32].count(14i32).count[15i32].count(16i32)[17i32].count[18i32].count(19i32)[20i32].count[21i32].count(22i32).count[23i32].count(24i32)[25i32].count[26i32].count(27i32)[28i32]))]
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
      "at(at(at(at(at(at(at(at(at(at(at(at(at(at(at(at(fetch_values(1i32).count(), 2i32).pick<i32>([index] 3i32) { foo(4i32) }, 5i32).count(6i32), 7i32).count(8i32), 9i32).count(), 10i32).count(11i32), 12i32).count(), 13i32).count(14i32).count(), 15i32).count(16i32), 17i32).count(), 18i32).count(19i32), 20i32).count(), 21i32).count(22i32).count(), 23i32).count(24i32), 25i32).count(), 26i32).count(27i32), 28i32)");
}

TEST_CASE(
    "parses semantic transform field-access after nested indexed template body chain indexed method-call field-access tail indexed method-call field-access index method-call field-access index method-call field-access index method-call field-access method-call") {
  const std::string source = R"(
[semantic(tag(fetch_values(1i32).count()[2i32].pick<i32>([index] 3i32) { foo(4i32) }[5i32].count(6i32)[7i32].count(8i32)[9i32].count[10i32].count(11i32)[12i32].count[13i32].count(14i32).count[15i32].count(16i32)[17i32].count[18i32].count(19i32)[20i32].count[21i32].count(22i32).count[23i32].count(24i32)[25i32].count[26i32].count(27i32)[28i32].count))]
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
      "at(at(at(at(at(at(at(at(at(at(at(at(at(at(at(at(fetch_values(1i32).count(), 2i32).pick<i32>([index] 3i32) { foo(4i32) }, 5i32).count(6i32), 7i32).count(8i32), 9i32).count(), 10i32).count(11i32), 12i32).count(), 13i32).count(14i32).count(), 15i32).count(16i32), 17i32).count(), 18i32).count(19i32), 20i32).count(), 21i32).count(22i32).count(), 23i32).count(24i32), 25i32).count(), 26i32).count(27i32), 28i32).count()");
}

TEST_CASE(
    "parses semantic transform indexing after nested indexed template body chain indexed method-call field-access tail indexed method-call field-access index method-call field-access index method-call field-access index method-call field-access method-call field-access index") {
  const std::string source = R"(
[semantic(tag(fetch_values(1i32).count()[2i32].pick<i32>([index] 3i32) { foo(4i32) }[5i32].count(6i32)[7i32].count(8i32)[9i32].count[10i32].count(11i32)[12i32].count[13i32].count(14i32).count[15i32].count(16i32)[17i32].count[18i32].count(19i32)[20i32].count[21i32].count(22i32).count[23i32].count(24i32)[25i32].count[26i32].count(27i32)[28i32].count[29i32]))]
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
      "at(at(at(at(at(at(at(at(at(at(at(at(at(at(at(at(at(fetch_values(1i32).count(), 2i32).pick<i32>([index] 3i32) { foo(4i32) }, 5i32).count(6i32), 7i32).count(8i32), 9i32).count(), 10i32).count(11i32), 12i32).count(), 13i32).count(14i32).count(), 15i32).count(16i32), 17i32).count(), 18i32).count(19i32), 20i32).count(), 21i32).count(22i32).count(), 23i32).count(24i32), 25i32).count(), 26i32).count(27i32), 28i32).count(), 29i32)");
}

TEST_CASE(
    "parses semantic transform method-call after nested indexed template body chain indexed method-call field-access tail indexed method-call field-access index method-call field-access index method-call field-access index method-call field-access index") {
  const std::string source = R"(
[semantic(tag(fetch_values(1i32).count()[2i32].pick<i32>([index] 3i32) { foo(4i32) }[5i32].count(6i32)[7i32].count(8i32)[9i32].count[10i32].count(11i32)[12i32].count[13i32].count(14i32).count[15i32].count(16i32)[17i32].count[18i32].count(19i32)[20i32].count[21i32].count(22i32).count[23i32].count(24i32)[25i32].count[26i32].count(27i32)[28i32].count[29i32].count(30i32)))]
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
      "at(at(at(at(at(at(at(at(at(at(at(at(at(at(at(at(at(fetch_values(1i32).count(), 2i32).pick<i32>([index] 3i32) { foo(4i32) }, 5i32).count(6i32), 7i32).count(8i32), 9i32).count(), 10i32).count(11i32), 12i32).count(), 13i32).count(14i32).count(), 15i32).count(16i32), 17i32).count(), 18i32).count(19i32), 20i32).count(), 21i32).count(22i32).count(), 23i32).count(24i32), 25i32).count(), 26i32).count(27i32), 28i32).count(), 29i32).count(30i32)");
}

TEST_CASE(
    "parses semantic transform indexing after nested indexed template body chain indexed method-call field-access tail indexed method-call field-access index method-call field-access index method-call field-access index method-call field-access index method-call") {
  const std::string source = R"(
[semantic(tag(fetch_values(1i32).count()[2i32].pick<i32>([index] 3i32) { foo(4i32) }[5i32].count(6i32)[7i32].count(8i32)[9i32].count[10i32].count(11i32)[12i32].count[13i32].count(14i32).count[15i32].count(16i32)[17i32].count[18i32].count(19i32)[20i32].count[21i32].count(22i32).count[23i32].count(24i32)[25i32].count[26i32].count(27i32)[28i32].count[29i32].count(30i32)[31i32]))]
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
      "at(at(at(at(at(at(at(at(at(at(at(at(at(at(at(at(at(at(fetch_values(1i32).count(), 2i32).pick<i32>([index] 3i32) { foo(4i32) }, 5i32).count(6i32), 7i32).count(8i32), 9i32).count(), 10i32).count(11i32), 12i32).count(), 13i32).count(14i32).count(), 15i32).count(16i32), 17i32).count(), 18i32).count(19i32), 20i32).count(), 21i32).count(22i32).count(), 23i32).count(24i32), 25i32).count(), 26i32).count(27i32), 28i32).count(), 29i32).count(30i32), 31i32)");
}

TEST_CASE(
    "parses semantic transform field-access after nested indexed template body chain method-call") {
  const std::string source = R"(
[semantic(tag(fetch_values(1i32).count()[2i32].pick<i32>([index] 3i32) { foo(4i32) }[5i32].count(6i32)[7i32].count))]
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
      "at(at(at(fetch_values(1i32).count(), 2i32).pick<i32>([index] 3i32) { foo(4i32) }, 5i32).count(6i32), 7i32).count()");
}

TEST_CASE(
    "parses semantic transform field-access after nested indexed template body chain") {
  const std::string source = R"(
[semantic(tag(fetch_values(1i32).count()[2i32].pick<i32>([index] 3i32) { foo(4i32) }[5i32].count))]
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
      "at(at(fetch_values(1i32).count(), 2i32).pick<i32>([index] 3i32) { foo(4i32) }, 5i32).count()");
}

TEST_CASE(
    "parses semantic transform indexing after nested indexed template body chain field-access") {
  const std::string source = R"(
[semantic(tag(fetch_values(1i32).count()[2i32].pick<i32>([index] 3i32) { foo(4i32) }[5i32].count[6i32]))]
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
      "at(at(at(fetch_values(1i32).count(), 2i32).pick<i32>([index] 3i32) { foo(4i32) }, 5i32).count(), 6i32)");
}

TEST_CASE("parses semantic transform indexing full form on call result") {
  const std::string source = R"(
[semantic(tag(fetch_values(1i32)[2i32]))]
main() {
  return(1i32)
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  const auto &transforms = program.definitions[0].transforms;
  REQUIRE(transforms.size() == 1);
  REQUIRE(transforms[0].arguments.size() == 1);
  CHECK(transforms[0].arguments[0] == "at(fetch_values(1i32), 2i32)");
}

TEST_CASE("parses semantic transform index then method-call on call result") {
  const std::string source = R"(
[semantic(tag(fetch_values(1i32)[2i32].count(3i32)))]
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
        "at(fetch_values(1i32), 2i32).count(3i32)");
}

TEST_CASE(
    "parses semantic transform index then template method-call on call result") {
  const std::string source = R"(
[semantic(tag(fetch_values(1i32)[2i32].pick<i32>([index] 3i32)))]
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
        "at(fetch_values(1i32), 2i32).pick<i32>([index] 3i32)");
}

TEST_CASE(
    "parses semantic transform index then template method-call without arguments") {
  const std::string source = R"(
[semantic(tag(fetch_values(1i32)[2i32].pick<i32>()))]
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
        "at(fetch_values(1i32), 2i32).pick<i32>()");
}

TEST_CASE(
    "parses semantic transform index after indexed template method-call result") {
  const std::string source = R"(
[semantic(tag(fetch_values(1i32)[2i32].pick<i32>()[3i32]))]
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
        "at(at(fetch_values(1i32), 2i32).pick<i32>(), 3i32)");
}

TEST_CASE("parses semantic transform index full form on template method chain") {
  const std::string source = R"(
[semantic(tag(fetch_values(1i32).pick<i32>([index] 2i32)[3i32]))]
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
        "at(fetch_values(1i32).pick<i32>([index] 2i32), 3i32)");
}

TEST_CASE("parses semantic transform method-call with body arguments") {
  const std::string source = R"(
[semantic(tag(values.count() { 1i32, foo(2i32) }))]
main() {
  return(1i32)
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  const auto &transforms = program.definitions[0].transforms;
  REQUIRE(transforms.size() == 1);
  REQUIRE(transforms[0].arguments.size() == 1);
  CHECK(transforms[0].arguments[0] == "values.count() { 1i32, foo(2i32) }");
}

TEST_CASE("parses semantic transform method-call with named argument") {
  const std::string source = R"(
[semantic(tag(values.mix([ratio] 2i32)))]
main() {
  return(1i32)
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  const auto &transforms = program.definitions[0].transforms;
  REQUIRE(transforms.size() == 1);
  REQUIRE(transforms[0].arguments.size() == 1);
  CHECK(transforms[0].arguments[0] == "values.mix([ratio] 2i32)");
}

TEST_CASE("parses semantic transform method-call with multiple arguments") {
  const std::string source = R"(
[semantic(tag(values.mix([ratio] 2i32, 3i32)))]
main() {
  return(1i32)
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  const auto &transforms = program.definitions[0].transforms;
  REQUIRE(transforms.size() == 1);
  REQUIRE(transforms[0].arguments.size() == 1);
  CHECK(transforms[0].arguments[0] == "values.mix([ratio] 2i32, 3i32)");
}

TEST_CASE("parses semantic transform method-call with multiple named arguments") {
  const std::string source = R"(
[semantic(tag(values.mix([ratio] 2i32, [bias] 3i32)))]
main() {
  return(1i32)
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  const auto &transforms = program.definitions[0].transforms;
  REQUIRE(transforms.size() == 1);
  REQUIRE(transforms[0].arguments.size() == 1);
  CHECK(transforms[0].arguments[0] == "values.mix([ratio] 2i32, [bias] 3i32)");
}

TEST_CASE("parses semantic transform method-call with template arguments") {
  const std::string source = R"(
[semantic(tag(values.pick<i32>([index] 1i32)))]
main() {
  return(1i32)
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  const auto &transforms = program.definitions[0].transforms;
  REQUIRE(transforms.size() == 1);
  REQUIRE(transforms[0].arguments.size() == 1);
  CHECK(transforms[0].arguments[0] == "values.pick<i32>([index] 1i32)");
}

TEST_CASE(
    "parses semantic transform method-call with template arguments and no call arguments") {
  const std::string source = R"(
[semantic(tag(values.pick<i32>()))]
main() {
  return(1i32)
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  const auto &transforms = program.definitions[0].transforms;
  REQUIRE(transforms.size() == 1);
  REQUIRE(transforms[0].arguments.size() == 1);
  CHECK(transforms[0].arguments[0] == "values.pick<i32>()");
}

TEST_CASE(
    "parses semantic transform method-call with template arguments and body") {
  const std::string source = R"(
[semantic(tag(values.pick<i32>([index] 1i32) { foo(2i32) }))]
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
        "values.pick<i32>([index] 1i32) { foo(2i32) }");
}

TEST_CASE(
    "parses semantic transform method-call with call receiver and template args") {
  const std::string source = R"(
[semantic(tag(fetch_values(1i32).pick<i32>([index] 2i32)))]
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
        "fetch_values(1i32).pick<i32>([index] 2i32)");
}

TEST_CASE(
    "parses semantic transform method-call with multiple template arguments") {
  const std::string source = R"(
[semantic(tag(values.pick<i32, i64>([index] 1i32, 2i32)))]
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
        "values.pick<i32, i64>([index] 1i32, 2i32)");
}

TEST_CASE(
    "parses semantic transform method-call with template named arguments") {
  const std::string source = R"(
[semantic(tag(values.pick<i32, i64>([index] 1i32, [fallback] 2i32)))]
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
        "values.pick<i32, i64>([index] 1i32, [fallback] 2i32)");
}

TEST_CASE("parses semantic transform full form template call") {
  const std::string source = R"(
[semantic(tag(convert<i32>(2u64)))]
main() {
  return(1i32)
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  const auto &transforms = program.definitions[0].transforms;
  REQUIRE(transforms.size() == 1);
  REQUIRE(transforms[0].arguments.size() == 1);
  CHECK(transforms[0].arguments[0] == "convert<i32>(2u64)");
}

TEST_CASE("parses semantic transform full form multi-template call") {
  const std::string source = R"(
[semantic(tag(make_pair<i32, i64>(1i32, 2i32)))]
main() {
  return(1i32)
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  const auto &transforms = program.definitions[0].transforms;
  REQUIRE(transforms.size() == 1);
  REQUIRE(transforms[0].arguments.size() == 1);
  CHECK(transforms[0].arguments[0] == "make_pair<i32, i64>(1i32, 2i32)");
}

TEST_CASE("parses semantic transform full form string literal") {
  const std::string source = R"(
[semantic(tag("hello"utf8))]
main() {
  return(1i32)
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  const auto &transforms = program.definitions[0].transforms;
  REQUIRE(transforms.size() == 1);
  REQUIRE(transforms[0].arguments.size() == 1);
  CHECK(transforms[0].arguments[0] == "\"hello\"utf8");
}

TEST_CASE("parses semantic transform full form false literal") {
  const std::string source = R"(
[semantic(tag(false))]
main() {
  return(1i32)
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  const auto &transforms = program.definitions[0].transforms;
  REQUIRE(transforms.size() == 1);
  REQUIRE(transforms[0].arguments.size() == 1);
  CHECK(transforms[0].arguments[0] == "false");
}

TEST_CASE("parses semantic transform full form f64 literal") {
  const std::string source = R"(
[semantic(tag(2.5f64))]
main() {
  return(1i32)
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  const auto &transforms = program.definitions[0].transforms;
  REQUIRE(transforms.size() == 1);
  REQUIRE(transforms[0].arguments.size() == 1);
  CHECK(transforms[0].arguments[0] == "2.5f64");
}

TEST_CASE("parses semantic transform full form u64 literal") {
  const std::string source = R"(
[semantic(tag(42u64))]
main() {
  return(1i32)
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  const auto &transforms = program.definitions[0].transforms;
  REQUIRE(transforms.size() == 1);
  REQUIRE(transforms[0].arguments.size() == 1);
  CHECK(transforms[0].arguments[0] == "42u64");
}

TEST_CASE("parses semantic transform full form i64 literal") {
  const std::string source = R"(
[semantic(tag(7i64))]
main() {
  return(1i32)
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  const auto &transforms = program.definitions[0].transforms;
  REQUIRE(transforms.size() == 1);
  REQUIRE(transforms[0].arguments.size() == 1);
  CHECK(transforms[0].arguments[0] == "7i64");
}

TEST_CASE("parses semantic transform full form brace call with multiple values") {
  const std::string source = R"(
[semantic(tag(value{1i32, foo(2i32)}))]
main() {
  return(1i32)
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  const auto &transforms = program.definitions[0].transforms;
  REQUIRE(transforms.size() == 1);
  REQUIRE(transforms[0].arguments.size() == 1);
  CHECK(transforms[0].arguments[0] == "value(1i32, foo(2i32))");
}

TEST_SUITE_END();
