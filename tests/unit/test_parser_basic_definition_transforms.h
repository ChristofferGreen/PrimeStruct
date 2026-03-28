#pragma once

TEST_CASE("parses trait constraint transforms") {
  const std::string source = R"(
[return<int> Additive<i32> Indexable<array<i32>, i32>]
main() {
  return(1i32)
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  const auto &transforms = program.definitions[0].transforms;
  REQUIRE(transforms.size() == 3);
  CHECK(transforms[0].name == "return");
  CHECK(transforms[1].name == "Additive");
  REQUIRE(transforms[1].templateArgs.size() == 1);
  CHECK(transforms[1].templateArgs[0] == "i32");
  CHECK(transforms[2].name == "Indexable");
  REQUIRE(transforms[2].templateArgs.size() == 2);
  CHECK(transforms[2].templateArgs[0] == "array<i32>");
  CHECK(transforms[2].templateArgs[1] == "i32");
}

TEST_CASE("parses reflection transforms on struct definitions") {
  const std::string source = R"(
[struct reflect generate(Equal, DebugPrint)]
Widget() {
  [i32] value{1i32}
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  const auto &transforms = program.definitions[0].transforms;
  REQUIRE(transforms.size() == 3);
  CHECK(transforms[0].name == "struct");
  CHECK(transforms[1].name == "reflect");
  CHECK(transforms[2].name == "generate");
  REQUIRE(transforms[2].arguments.size() == 2);
  CHECK(transforms[2].arguments[0] == "Equal");
  CHECK(transforms[2].arguments[1] == "DebugPrint");
}

TEST_CASE("parses reflection generate list for all v1 helpers") {
  const std::string source = R"(
[struct reflect generate(Equal, NotEqual, Default, IsDefault, Clone, DebugPrint)]
Widget() {
  [i32] value{1i32}
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  const auto &transforms = program.definitions[0].transforms;
  REQUIRE(transforms.size() == 3);
  CHECK(transforms[0].name == "struct");
  CHECK(transforms[1].name == "reflect");
  CHECK(transforms[2].name == "generate");
  REQUIRE(transforms[2].arguments.size() == 6);
  CHECK(transforms[2].arguments[0] == "Equal");
  CHECK(transforms[2].arguments[1] == "NotEqual");
  CHECK(transforms[2].arguments[2] == "Default");
  CHECK(transforms[2].arguments[3] == "IsDefault");
  CHECK(transforms[2].arguments[4] == "Clone");
  CHECK(transforms[2].arguments[5] == "DebugPrint");
}

TEST_CASE("parses reflection generate Compare helper") {
  const std::string source = R"(
[struct reflect generate(Compare)]
Widget() {
  [i32] x{1i32}
  [i32] y{2i32}
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  const auto &transforms = program.definitions[0].transforms;
  REQUIRE(transforms.size() == 3);
  CHECK(transforms[0].name == "struct");
  CHECK(transforms[1].name == "reflect");
  CHECK(transforms[2].name == "generate");
  REQUIRE(transforms[2].arguments.size() == 1);
  CHECK(transforms[2].arguments[0] == "Compare");
}

TEST_CASE("parses transform groups") {
  const std::string source = R"(
[text(operators, collections) semantic(return<int>, effects(io_out))]
main() {
  return(1i32)
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  REQUIRE(program.definitions[0].transforms.size() == 4);
  CHECK(program.definitions[0].transforms[0].name == "operators");
  CHECK(program.definitions[0].transforms[0].phase == primec::TransformPhase::Text);
  CHECK(program.definitions[0].transforms[1].name == "collections");
  CHECK(program.definitions[0].transforms[1].phase == primec::TransformPhase::Text);
  CHECK(program.definitions[0].transforms[2].name == "return");
  CHECK(program.definitions[0].transforms[2].phase == primec::TransformPhase::Semantic);
  CHECK(program.definitions[0].transforms[3].name == "effects");
  CHECK(program.definitions[0].transforms[3].phase == primec::TransformPhase::Semantic);
}

TEST_CASE("parses transform groups with semicolons") {
  const std::string source = R"(
[text(operators; collections;); semantic(return<int>; effects(io_out; io_err;););]
main() {
  return(1i32)
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  const auto &transforms = program.definitions[0].transforms;
  REQUIRE(transforms.size() == 4);
  CHECK(transforms[0].name == "operators");
  CHECK(transforms[0].phase == primec::TransformPhase::Text);
  CHECK(transforms[1].name == "collections");
  CHECK(transforms[1].phase == primec::TransformPhase::Text);
  CHECK(transforms[2].name == "return");
  CHECK(transforms[2].phase == primec::TransformPhase::Semantic);
  CHECK(transforms[3].name == "effects");
  CHECK(transforms[3].phase == primec::TransformPhase::Semantic);
  REQUIRE(transforms[3].arguments.size() == 2);
  CHECK(transforms[3].arguments[0] == "io_out");
  CHECK(transforms[3].arguments[1] == "io_err");
}

TEST_CASE("parses transform groups with comments and semicolons") {
  const std::string source = R"(
[text(/*first*/ operators; /*second*/ collections;); semantic(return<int>; effects(io_out; /*mid*/ io_err;);)]
main() {
  return(1i32)
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  const auto &transforms = program.definitions[0].transforms;
  REQUIRE(transforms.size() == 4);
  CHECK(transforms[0].name == "operators");
  CHECK(transforms[0].phase == primec::TransformPhase::Text);
  CHECK(transforms[1].name == "collections");
  CHECK(transforms[1].phase == primec::TransformPhase::Text);
  CHECK(transforms[2].name == "return");
  CHECK(transforms[2].phase == primec::TransformPhase::Semantic);
  CHECK(transforms[3].name == "effects");
  CHECK(transforms[3].phase == primec::TransformPhase::Semantic);
  REQUIRE(transforms[3].arguments.size() == 2);
  CHECK(transforms[3].arguments[0] == "io_out");
  CHECK(transforms[3].arguments[1] == "io_err");
}

TEST_CASE("parses semantic transform full form arguments") {
  const std::string source = R"(
[semantic(tag(foo(1i32)))]
main() {
  return(1i32)
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  const auto &transforms = program.definitions[0].transforms;
  REQUIRE(transforms.size() == 1);
  CHECK(transforms[0].name == "tag");
  CHECK(transforms[0].phase == primec::TransformPhase::Semantic);
  REQUIRE(transforms[0].arguments.size() == 1);
  CHECK(transforms[0].arguments[0] == "foo(1i32)");
}

TEST_CASE("parses semantic transform arguments without separators") {
  const std::string source = R"(
[semantic(tag(foo(1i32) bar(2i32)))]
main() {
  return(1i32)
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  const auto &transforms = program.definitions[0].transforms;
  REQUIRE(transforms.size() == 1);
  REQUIRE(transforms[0].arguments.size() == 2);
  CHECK(transforms[0].arguments[0] == "foo(1i32)");
  CHECK(transforms[0].arguments[1] == "bar(2i32)");
}

TEST_CASE("parses semantic transform full forms across bracket continuation") {
  const std::string source = R"(
[semantic(tag(foo(1i32) [i32] value{1i32}))]
main() {
  return(1i32)
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  const auto &transforms = program.definitions[0].transforms;
  REQUIRE(transforms.size() == 1);
  REQUIRE(transforms[0].arguments.size() == 2);
  CHECK(transforms[0].arguments[0] == "at(foo(1i32), i32)");
  CHECK(transforms[0].arguments[1] == "value(block() { 1i32 })");
}

TEST_CASE("parses semantic transform scalar and body full forms") {
  const std::string source = R"(
[semantic(tag(true 1.5f32 "ok"utf8 [i32] value{1i32} block() { 1i32 }))]
main() {
  return(1i32)
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  const auto &transforms = program.definitions[0].transforms;
  REQUIRE(transforms.size() == 1);
  REQUIRE(transforms[0].arguments.size() == 5);
  CHECK(transforms[0].arguments[0] == "true");
  CHECK(transforms[0].arguments[1] == "1.5f32");
  CHECK(transforms[0].arguments[2] == "at(\"ok\"utf8, i32)");
  CHECK(transforms[0].arguments[3] == "value(block() { 1i32 })");
  CHECK(transforms[0].arguments[4] == "block() { 1i32 }");
}

TEST_CASE("parses semantic transform method-call full form argument") {
  const std::string source = R"(
[semantic(tag(values.count(2i32)))]
main() {
  return(1i32)
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  const auto &transforms = program.definitions[0].transforms;
  REQUIRE(transforms.size() == 1);
  REQUIRE(transforms[0].arguments.size() == 1);
  CHECK(transforms[0].arguments[0] == "values.count(2i32)");
}

TEST_CASE("parses semantic transform method-call without explicit arguments") {
  const std::string source = R"(
[semantic(tag(values.count()))]
main() {
  return(1i32)
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  const auto &transforms = program.definitions[0].transforms;
  REQUIRE(transforms.size() == 1);
  REQUIRE(transforms[0].arguments.size() == 1);
  CHECK(transforms[0].arguments[0] == "values.count()");
}

TEST_CASE("parses semantic transform field-access full form argument") {
  const std::string source = R"(
[semantic(tag(values.count))]
main() {
  return(1i32)
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  const auto &transforms = program.definitions[0].transforms;
  REQUIRE(transforms.size() == 1);
  REQUIRE(transforms[0].arguments.size() == 1);
  CHECK(transforms[0].arguments[0] == "values.count()");
}

TEST_CASE("parses semantic transform field-access full form on call receiver") {
  const std::string source = R"(
[semantic(tag(fetch_values(1i32).count))]
main() {
  return(1i32)
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  const auto &transforms = program.definitions[0].transforms;
  REQUIRE(transforms.size() == 1);
  REQUIRE(transforms[0].arguments.size() == 1);
  CHECK(transforms[0].arguments[0] == "fetch_values(1i32).count()");
}

TEST_CASE("parses semantic transform indexing full form argument") {
  const std::string source = R"(
[semantic(tag(values[1i32]))]
main() {
  return(1i32)
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  const auto &transforms = program.definitions[0].transforms;
  REQUIRE(transforms.size() == 1);
  REQUIRE(transforms[0].arguments.size() == 1);
  CHECK(transforms[0].arguments[0] == "at(values, 1i32)");
}

TEST_CASE("parses semantic transform index then field-access full form") {
  const std::string source = R"(
[semantic(tag(values[1i32].count))]
main() {
  return(1i32)
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  const auto &transforms = program.definitions[0].transforms;
  REQUIRE(transforms.size() == 1);
  REQUIRE(transforms[0].arguments.size() == 1);
  CHECK(transforms[0].arguments[0] == "at(values, 1i32).count()");
}

TEST_CASE("parses semantic transform index then method-call full form") {
  const std::string source = R"(
[semantic(tag(values[1i32].count(2i32)))]
main() {
  return(1i32)
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  const auto &transforms = program.definitions[0].transforms;
  REQUIRE(transforms.size() == 1);
  REQUIRE(transforms[0].arguments.size() == 1);
  CHECK(transforms[0].arguments[0] == "at(values, 1i32).count(2i32)");
}

TEST_CASE("parses semantic transform index then template method-call full form") {
  const std::string source = R"(
[semantic(tag(values[1i32].pick<i32>(2i32)))]
main() {
  return(1i32)
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  const auto &transforms = program.definitions[0].transforms;
  REQUIRE(transforms.size() == 1);
  REQUIRE(transforms[0].arguments.size() == 1);
  CHECK(transforms[0].arguments[0] == "at(values, 1i32).pick<i32>(2i32)");
}

TEST_CASE("parses semantic transform index then template method-call named argument") {
  const std::string source = R"(
[semantic(tag(values[1i32].pick<i32>([index] 2i32)))]
main() {
  return(1i32)
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  const auto &transforms = program.definitions[0].transforms;
  REQUIRE(transforms.size() == 1);
  REQUIRE(transforms[0].arguments.size() == 1);
  CHECK(transforms[0].arguments[0] == "at(values, 1i32).pick<i32>([index] 2i32)");
}

TEST_CASE("parses semantic transform indexing full form on field access") {
  const std::string source = R"(
[semantic(tag(values.count[1i32]))]
main() {
  return(1i32)
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  const auto &transforms = program.definitions[0].transforms;
  REQUIRE(transforms.size() == 1);
  REQUIRE(transforms[0].arguments.size() == 1);
  CHECK(transforms[0].arguments[0] == "at(values.count(), 1i32)");
}

TEST_CASE("parses semantic transform indexing field access on call receiver") {
  const std::string source = R"(
[semantic(tag(fetch_values(1i32).count[2i32]))]
main() {
  return(1i32)
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  const auto &transforms = program.definitions[0].transforms;
  REQUIRE(transforms.size() == 1);
  REQUIRE(transforms[0].arguments.size() == 1);
  CHECK(transforms[0].arguments[0] == "at(fetch_values(1i32).count(), 2i32)");
}

TEST_CASE("parses semantic transform indexing full form on method call") {
  const std::string source = R"(
[semantic(tag(values.count()[1i32]))]
main() {
  return(1i32)
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  const auto &transforms = program.definitions[0].transforms;
  REQUIRE(transforms.size() == 1);
  REQUIRE(transforms[0].arguments.size() == 1);
  CHECK(transforms[0].arguments[0] == "at(values.count(), 1i32)");
}

TEST_CASE("parses semantic transform indexing method call on call receiver") {
  const std::string source = R"(
[semantic(tag(fetch_values(1i32).count()[2i32]))]
main() {
  return(1i32)
}
)";
  const auto program = parseProgram(source);
  REQUIRE(program.definitions.size() == 1);
  const auto &transforms = program.definitions[0].transforms;
  REQUIRE(transforms.size() == 1);
  REQUIRE(transforms[0].arguments.size() == 1);
  CHECK(transforms[0].arguments[0] == "at(fetch_values(1i32).count(), 2i32)");
}

