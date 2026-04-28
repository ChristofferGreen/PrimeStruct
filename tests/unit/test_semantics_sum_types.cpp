#include "test_semantics_helpers.h"

TEST_SUITE_BEGIN("primestruct.semantics");

namespace {

bool validateProgramWithSemanticProduct(const std::string &source,
                                        primec::SemanticProgram &semanticProgram,
                                        std::string &error) {
  auto program = parseProgram(source);
  primec::Semantics semantics;
  const std::vector<std::string> defaults = {"io_out", "io_err"};
  return semantics.validate(program, "/main", error, defaults, defaults, {}, nullptr, false, &semanticProgram);
}

bool validateProgramExpectingError(const std::string &source, std::string &error) {
  return validateProgram(source, "/main", error);
}

} // namespace

TEST_CASE("sum declarations publish deterministic type and variant metadata") {
  const std::string source = R"(
[struct]
Circle {
  [i32] radius
}

[struct]
Rectangle {
  [i32] width
  [i32] height
}

[public sum]
Shape {
  [Circle] circle
  [Rectangle] rectangle
}

[return<i32>]
main() {
  return(0i32)
}
)";

  primec::SemanticProgram semanticProgram;
  std::string error;
  REQUIRE(validateProgramWithSemanticProduct(source, semanticProgram, error));
  CHECK(error.empty());

  const auto *shapeType = primec::semanticProgramLookupTypeMetadata(semanticProgram, "/Shape");
  REQUIRE(shapeType != nullptr);
  CHECK(shapeType->category == "sum");
  CHECK(shapeType->isPublic);
  CHECK(shapeType->fieldCount == 2);

  REQUIRE(semanticProgram.sumTypeMetadata.size() == 1);
  CHECK(semanticProgram.sumTypeMetadata[0].fullPath == "/Shape");
  CHECK(semanticProgram.sumTypeMetadata[0].isPublic);
  CHECK(semanticProgram.sumTypeMetadata[0].activeTagTypeText == "u32");
  CHECK(semanticProgram.sumTypeMetadata[0].payloadStorageText == "inline_max_payload");
  CHECK(semanticProgram.sumTypeMetadata[0].variantCount == 2);

  REQUIRE(semanticProgram.sumVariantMetadata.size() == 2);
  CHECK(semanticProgram.sumVariantMetadata[0].sumPath == "/Shape");
  CHECK(semanticProgram.sumVariantMetadata[0].variantName == "circle");
  CHECK(semanticProgram.sumVariantMetadata[0].variantIndex == 0);
  CHECK(semanticProgram.sumVariantMetadata[0].tagValue == 0);
  CHECK(semanticProgram.sumVariantMetadata[0].payloadTypeText == "Circle");
  CHECK(semanticProgram.sumVariantMetadata[1].sumPath == "/Shape");
  CHECK(semanticProgram.sumVariantMetadata[1].variantName == "rectangle");
  CHECK(semanticProgram.sumVariantMetadata[1].variantIndex == 1);
  CHECK(semanticProgram.sumVariantMetadata[1].tagValue == 1);
  CHECK(semanticProgram.sumVariantMetadata[1].payloadTypeText == "Rectangle");

  const std::string dump = primec::formatSemanticProgram(semanticProgram);
  CHECK(dump.find("full_path=\"/Shape\" category=\"sum\"") != std::string::npos);
  CHECK(dump.find("sum_type_metadata[0]: full_path=\"/Shape\" is_public=true "
                  "active_tag_type_text=\"u32\" payload_storage_text=\"inline_max_payload\" "
                  "variant_count=2") != std::string::npos);
  CHECK(dump.find("sum_variant_metadata[0]: sum_path=\"/Shape\" variant_name=\"circle\" "
                  "variant_index=0 tag_value=0 payload_type_text=\"Circle\"") != std::string::npos);
}

TEST_CASE("sum declarations reject duplicate variant names") {
  const std::string source = R"(
[struct]
Circle {
  [i32] radius
}

[sum]
Shape {
  [Circle] circle
  [Circle] circle
}

[return<i32>]
main() {
  return(0i32)
}
)";

  std::string error;
  CHECK_FALSE(validateProgramExpectingError(source, error));
  CHECK(error.find("duplicate sum variant: circle on /Shape") != std::string::npos);
}

TEST_CASE("sum declarations reject missing payload envelopes") {
  const std::string source = R"(
[sum]
Maybe {
  none
}

[return<i32>]
main() {
  return(0i32)
}
)";

  std::string error;
  CHECK_FALSE(validateProgramExpectingError(source, error));
  CHECK(error.find("sum variants require one payload envelope on /Maybe") != std::string::npos);
}

TEST_CASE("sum declarations reject field-like variant modifiers") {
  const std::string source = R"(
[struct]
Circle {
  [i32] radius
}

[sum]
Shape {
  [public Circle] circle
}

[return<i32>]
main() {
  return(0i32)
}
)";

  std::string error;
  CHECK_FALSE(validateProgramExpectingError(source, error));
  CHECK(error.find("sum variants require exactly one payload envelope on /Shape/circle") !=
        std::string::npos);
}

TEST_CASE("sum declarations reject unsupported payload envelopes") {
  const std::string source = R"(
[sum]
Bad {
  [auto] item
}

[return<i32>]
main() {
  return(0i32)
}
)";

  std::string error;
  CHECK_FALSE(validateProgramExpectingError(source, error));
  CHECK(error.find("unsupported sum payload envelope on /Bad/item: auto") != std::string::npos);
}

TEST_CASE("explicit sum construction accepts labeled and positional payloads") {
  const std::string source = R"(
[struct]
Circle {
  [f32] radius
}

[struct]
Rectangle {
  [f32] width
  [f32] height
}

[sum]
Shape {
  [Circle] circle
  [Rectangle] rectangle
}

[return<i32>]
main() {
  [Shape] labeled{[circle] Circle{[radius] 3.4}}
  [Shape] positional{[rectangle] Rectangle{4.0, 5.0}}
  return(0i32)
}
)";

  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("explicit sum construction works for fields and auto inference") {
  const std::string source = R"(
[struct]
Circle {
  [f32] radius
}

[sum]
Shape {
  [Circle] circle
}

[struct]
Holder {
  [Shape] primary
}

[return<Shape>]
makeShape() {
  [Shape] made{[circle] Circle{1.0}}
  return(made)
}

[return<i32>]
main() {
  [Holder] holder{Holder{[primary] Shape{[circle] Circle{2.0}}}}
  [auto] direct{Shape{[circle] Circle{3.0}}}
  return(0i32)
}
)";

  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("explicit sum construction rejects invalid variant shapes") {
  SUBCASE("unknown variant") {
    const std::string source = R"(
[struct]
Circle {
  [f32] radius
}

[sum]
Shape {
  [Circle] circle
}

[return<i32>]
main() {
  [Shape] bad{[triangle] Circle{1.0}}
  return(0i32)
}
)";

    std::string error;
    CHECK_FALSE(validateProgramExpectingError(source, error));
    CHECK(error.find("unknown sum variant on /Shape: triangle") != std::string::npos);
  }

  SUBCASE("duplicate variant entries") {
    const std::string source = R"(
[struct]
Circle {
  [f32] radius
}

[sum]
Shape {
  [Circle] circle
}

[return<i32>]
main() {
  [Shape] bad{Shape{[circle] Circle{1.0}, [circle] Circle{2.0}}}
  return(0i32)
}
)";

    std::string error;
    CHECK_FALSE(validateProgramExpectingError(source, error));
    CHECK(error.find("duplicate sum variant in construction: circle on /Shape") !=
          std::string::npos);
  }

  SUBCASE("empty initializer") {
    const std::string source = R"(
[struct]
Circle {
  [f32] radius
}

[sum]
Shape {
  [Circle] circle
}

[return<i32>]
main() {
  [Shape] bad{Shape{}}
  return(0i32)
}
)";

    std::string error;
    CHECK_FALSE(validateProgramExpectingError(source, error));
    CHECK(error.find("sum construction requires exactly one explicit variant for /Shape") !=
          std::string::npos);
  }
}

TEST_CASE("explicit sum construction rejects payload and target mismatches") {
  SUBCASE("payload type mismatch") {
    const std::string source = R"(
[struct]
Circle {
  [f32] radius
}

[struct]
Rectangle {
  [f32] width
  [f32] height
}

[sum]
Shape {
  [Circle] circle
  [Rectangle] rectangle
}

[return<i32>]
main() {
  [Shape] bad{[circle] Rectangle{1.0, 2.0}}
  return(0i32)
}
)";

    std::string error;
    CHECK_FALSE(validateProgramExpectingError(source, error));
    CHECK(error.find("argument type mismatch for /Shape parameter circle") !=
          std::string::npos);
  }

  SUBCASE("auto context without named sum target") {
    const std::string source = R"(
[struct]
Circle {
  [f32] radius
}

[sum]
Shape {
  [Circle] circle
}

[return<i32>]
main() {
  [auto] bad{[circle] Circle{1.0}}
  return(0i32)
}
)";

    std::string error;
    CHECK_FALSE(validateProgramExpectingError(source, error));
    CHECK(error.find("sum construction requires target sum type") != std::string::npos);
  }
}

TEST_SUITE_END();
