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
  CHECK(semanticProgram.sumVariantMetadata[0].hasPayload);
  CHECK(semanticProgram.sumVariantMetadata[0].payloadTypeText == "Circle");
  CHECK(semanticProgram.sumVariantMetadata[1].sumPath == "/Shape");
  CHECK(semanticProgram.sumVariantMetadata[1].variantName == "rectangle");
  CHECK(semanticProgram.sumVariantMetadata[1].variantIndex == 1);
  CHECK(semanticProgram.sumVariantMetadata[1].tagValue == 1);
  CHECK(semanticProgram.sumVariantMetadata[1].hasPayload);
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

TEST_CASE("sum declarations publish unit variant metadata") {
  const std::string source = R"(
[sum]
Maybe {
  none
  [i32] some
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

  REQUIRE(semanticProgram.sumVariantMetadata.size() == 2);
  CHECK(semanticProgram.sumVariantMetadata[0].sumPath == "/Maybe");
  CHECK(semanticProgram.sumVariantMetadata[0].variantName == "none");
  CHECK(semanticProgram.sumVariantMetadata[0].variantIndex == 0);
  CHECK(semanticProgram.sumVariantMetadata[0].tagValue == 0);
  CHECK_FALSE(semanticProgram.sumVariantMetadata[0].hasPayload);
  CHECK(semanticProgram.sumVariantMetadata[0].payloadTypeText.empty());
  CHECK(semanticProgram.sumVariantMetadata[1].variantName == "some");
  CHECK(semanticProgram.sumVariantMetadata[1].hasPayload);
  CHECK(semanticProgram.sumVariantMetadata[1].payloadTypeText == "i32");

  const std::string dump = primec::formatSemanticProgram(semanticProgram);
  CHECK(dump.find("sum_variant_metadata[0]: sum_path=\"/Maybe\" variant_name=\"none\" "
                  "variant_index=0 tag_value=0 payload_type_text=\"\" has_payload=false") !=
        std::string::npos);
  CHECK(dump.find("sum_variant_metadata[1]: sum_path=\"/Maybe\" variant_name=\"some\" "
                  "variant_index=1 tag_value=1 payload_type_text=\"i32\" has_payload=true") !=
        std::string::npos);
}

TEST_CASE("sum declarations reject call-shaped unit variants") {
  const std::string source = R"(
[sum]
Maybe {
  none()
}

[return<i32>]
main() {
  return(0i32)
}
)";

  std::string error;
  CHECK_FALSE(validateProgramExpectingError(source, error));
  CHECK(error.find("sum variants require one payload envelope or bare unit variant on /Maybe") !=
        std::string::npos);
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

TEST_CASE("inferred sum construction accepts unique target-typed payloads") {
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

[struct]
Holder {
  [Shape] primary
}

[return<Shape>]
makeShape() {
  return(Circle{1.0})
}

[return<i32>]
acceptShape([Shape] shape) {
  return(1i32)
}

[return<i32>]
main() {
  [Shape] named{Circle{[radius] 3.4}}
  [Shape] positional{Rectangle{4.0, 5.0}}
  [Circle] localCircle{Circle{6.0}}
  [Shape] localPayload{localCircle}
  [Holder] holder{Holder{[primary] Circle{2.0}}}
  [i32] accepted{acceptShape(Circle{7.0})}
  [Shape] returned{makeShape()}
  return(accepted)
}
)";

  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("inferred sum construction resolves imported sums deterministically") {
  const std::string source = R"(
import /geo

namespace geo {
  [public struct]
  Circle {
    [f32] radius
  }

  [public struct]
  Rectangle {
    [f32] width
    [f32] height
  }

  [public sum]
  Shape {
    [Circle] circle
    [Rectangle] rectangle
  }
}

[return<i32>]
main() {
  [Shape] imported{Circle{1.0}}
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
  SUBCASE("unit variant payload") {
    const std::string source = R"(
[sum]
Maybe {
  none
  [i32] some
}

[return<i32>]
main() {
  [Maybe] bad{[none] 0i32}
  return(0i32)
}
)";

    std::string error;
    CHECK_FALSE(validateProgramExpectingError(source, error));
    CHECK(error.find("unit sum variant does not accept payload: /Maybe/none") !=
          std::string::npos);
  }

  SUBCASE("payload variant while unit execution is deferred") {
    const std::string source = R"(
[sum]
Maybe {
  none
  [i32] some
}

[return<i32>]
main() {
  [Maybe] bad{[some] 1i32}
  return(0i32)
}
)";

    std::string error;
    CHECK_FALSE(validateProgramExpectingError(source, error));
    CHECK(error.find("sum construction with unit variants is not supported yet: /Maybe") !=
          std::string::npos);
  }

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

TEST_CASE("inferred sum construction rejects missing and ambiguous payloads") {
  SUBCASE("no matching variant") {
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

[struct]
Triangle {
  [f32] side
}

[sum]
Shape {
  [Circle] circle
  [Rectangle] rectangle
}

[return<i32>]
main() {
  [Shape] bad{Triangle{1.0}}
  return(0i32)
}
)";

    std::string error;
    CHECK_FALSE(validateProgramExpectingError(source, error));
    CHECK(error.find("no inferred sum variant for /Shape accepts payload; "
                     "candidates: circle, rectangle") != std::string::npos);
  }

  SUBCASE("duplicate payload envelope is ambiguous") {
    const std::string source = R"(
[struct]
Circle {
  [f32] radius
}

[sum]
Shape {
  [Circle] circle
  [Circle] round
}

[return<i32>]
main() {
  [Shape] bad{Circle{1.0}}
  return(0i32)
}
)";

    std::string error;
    CHECK_FALSE(validateProgramExpectingError(source, error));
    CHECK(error.find("ambiguous inferred sum construction for /Shape: circle, round") !=
          std::string::npos);
  }

  SUBCASE("primitive payloads do not widen implicitly") {
    const std::string source = R"(
[sum]
Shape {
  [i32] count
}

[return<i32>]
main() {
  [Shape] bad{1.0}
  return(0i32)
}
)";

    std::string error;
    CHECK_FALSE(validateProgramExpectingError(source, error));
    CHECK(error.find("no inferred sum variant for /Shape accepts payload; candidates: count") !=
          std::string::npos);
  }
}

TEST_CASE("pick rejects payload binders on unit variants") {
  const std::string source = R"(
[sum]
Maybe {
  none
  [i32] some
}

[return<i32>]
measure([Maybe] value) {
  return(pick(value) {
    none(n) {
      return(0i32)
    }
    some(v) {
      return(v)
    }
  })
}

[return<i32>]
main() {
  return(0i32)
}
)";

  std::string error;
  CHECK_FALSE(validateProgramExpectingError(source, error));
  CHECK(error.find("pick unit variant arm does not accept payload binder: /Maybe/none") !=
        std::string::npos);
}

TEST_CASE("pick validates exhaustive sum arms and payload binders") {
  const std::string source = R"(
[struct]
Circle {
  [i32] radius
}

[struct]
Rectangle {
  [i32] width
}

[sum]
Shape {
  [Circle] circle
  [Rectangle] rectangle
}

[return<i32>]
measure([Shape] shape) {
  return(pick(shape) {
    circle(circlePayload) {
      circlePayload.radius
    }
    rectangle(rectanglePayload) {
      rectanglePayload.width
    }
  })
}

[return<i32>]
main() {
  [Shape] shape{Circle{4i32}}
  return(measure(shape))
}
)";

  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("pick statement arms can mutate through payload-specific branches") {
  const std::string source = R"(
[struct]
Circle {
  [i32] radius
}

[struct]
Rectangle {
  [i32] width
}

[sum]
Shape {
  [Circle] circle
  [Rectangle] rectangle
}

[return<i32>]
main() {
  [Shape] shape{Rectangle{7i32}}
  [i32 mut] out{0i32}
  pick(shape) {
    circle(circlePayload) {
      assign(out, circlePayload.radius)
    }
    rectangle(rectanglePayload) {
      assign(out, rectanglePayload.width)
    }
  }
  return(out)
}
)";

  std::string error;
  CHECK(validateProgram(source, "/main", error));
  CHECK(error.empty());
}

TEST_CASE("pick rejects invalid sum arm shapes") {
  SUBCASE("missing variant") {
    const std::string source = R"(
[struct]
Circle {
  [i32] radius
}

[struct]
Rectangle {
  [i32] width
}

[sum]
Shape {
  [Circle] circle
  [Rectangle] rectangle
}

[return<i32>]
main() {
  [Shape] shape{Circle{1i32}}
  return(pick(shape) {
    circle(circlePayload) {
      circlePayload.radius
    }
  })
}
)";

    std::string error;
    CHECK_FALSE(validateProgramExpectingError(source, error));
    CHECK(error.find("missing pick variants for /Shape: rectangle") !=
          std::string::npos);
  }

  SUBCASE("duplicate variant") {
    const std::string source = R"(
[struct]
Circle {
  [i32] radius
}

[sum]
Shape {
  [Circle] circle
}

[return<i32>]
main() {
  [Shape] shape{Circle{1i32}}
  return(pick(shape) {
    circle(firstPayload) {
      firstPayload.radius
    }
    circle(secondPayload) {
      secondPayload.radius
    }
  })
}
)";

    std::string error;
    CHECK_FALSE(validateProgramExpectingError(source, error));
    CHECK(error.find("duplicate pick variant on /Shape: circle") !=
          std::string::npos);
  }

  SUBCASE("unknown variant") {
    const std::string source = R"(
[struct]
Circle {
  [i32] radius
}

[sum]
Shape {
  [Circle] circle
}

[return<i32>]
main() {
  [Shape] shape{Circle{1i32}}
  return(pick(shape) {
    rectangle(payload) {
      0i32
    }
  })
}
)";

    std::string error;
    CHECK_FALSE(validateProgramExpectingError(source, error));
    CHECK(error.find("unknown pick variant on /Shape: rectangle") !=
          std::string::npos);
  }

  SUBCASE("missing payload binder") {
    const std::string source = R"(
[struct]
Circle {
  [i32] radius
}

[sum]
Shape {
  [Circle] circle
}

[return<i32>]
main() {
  [Shape] shape{Circle{1i32}}
  return(pick(shape) {
    circle() {
      0i32
    }
  })
}
)";

    std::string error;
    CHECK_FALSE(validateProgramExpectingError(source, error));
    CHECK(error.find("pick arms require variant(payload) block for /Shape") !=
          std::string::npos);
  }
}

TEST_CASE("pick validates branch payload types and value compatibility") {
  SUBCASE("payload binder has the selected variant type") {
    const std::string source = R"(
[struct]
Circle {
  [i32] radius
}

[struct]
Rectangle {
  [i32] width
}

[sum]
Shape {
  [Circle] circle
  [Rectangle] rectangle
}

[return<i32>]
acceptRectangle([Rectangle] rectangle) {
  return(rectangle.width)
}

[return<i32>]
main() {
  [Shape] shape{Circle{1i32}}
  return(pick(shape) {
    circle(circlePayload) {
      acceptRectangle(circlePayload)
    }
    rectangle(rectanglePayload) {
      acceptRectangle(rectanglePayload)
    }
  })
}
)";

    std::string error;
    CHECK_FALSE(validateProgramExpectingError(source, error));
    CHECK(error.find("argument type mismatch for /acceptRectangle parameter rectangle") !=
          std::string::npos);
  }

  SUBCASE("branch values must be compatible") {
    const std::string source = R"(
[struct]
Circle {
  [i32] radius
}

[struct]
Rectangle {
  [i32] width
}

[sum]
Shape {
  [Circle] circle
  [Rectangle] rectangle
}

[return<i32>]
main() {
  [Shape] shape{Circle{1i32}}
  return(pick(shape) {
    circle(circlePayload) {
      circlePayload.radius
    }
    rectangle(rectanglePayload) {
      "wide"utf8
    }
  })
}
)";

    std::string error;
    CHECK_FALSE(validateProgramExpectingError(source, error));
    CHECK(error.find("pick branches must return compatible types") !=
          std::string::npos);
  }
}

TEST_SUITE_END();
