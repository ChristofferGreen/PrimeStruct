#include "examples/shared/scene_bgra8_renderer.h"

#include "third_party/doctest.h"

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

TEST_SUITE_BEGIN("primestruct.scene.renderer");

namespace {

using BgraPixel = std::array<std::uint8_t, 4>;

struct TestNode {
  int painterOrder = 0;
  int localZ = 0;
  int primitiveId = 0;
  int materialId = 0;
  int translationX = 0;
  int translationY = 0;
  int translationZ = 0;
  int scaleZ = 1000;
};

struct TestPrimitive {
  int width = 1000;
  int height = 1000;
  int radius = 0;
  int materialId = 0;
  int kind = 1;
};

struct TestMaterial {
  int red = 0;
  int green = 0;
  int blue = 0;
  int opacity = 1000;
  int shadeStrength = 1000;
};

std::vector<std::int32_t> makeSceneWords(int width,
                                         int height,
                                         const std::vector<TestNode> &nodes,
                                         const std::vector<TestPrimitive> &primitives,
                                         const std::vector<TestMaterial> &materials,
                                         bool includeUiLightRig = false) {
  std::vector<std::int32_t> words;
  auto push = [&](int value) { words.push_back(static_cast<std::int32_t>(value)); };

  push(1); // Scene.serialize() version.
  push(0); // activeCameraId.

  push(static_cast<int>(nodes.size()));
  for (std::size_t index = 0; index < nodes.size(); ++index) {
    const TestNode &node = nodes[index];
    push(static_cast<int>(index));
    push(-1);
    push(node.painterOrder);
    push(node.localZ);
    push(node.primitiveId);
    push(node.materialId);
    push(node.translationX);
    push(node.translationY);
    push(node.translationZ);
    push(1000);
    push(1000);
    push(node.scaleZ);
    push(0);
  }

  push(static_cast<int>(primitives.size()));
  for (std::size_t index = 0; index < primitives.size(); ++index) {
    const TestPrimitive &primitive = primitives[index];
    push(static_cast<int>(index));
    push(primitive.kind);
    push(primitive.width);
    push(primitive.height);
    push(primitive.radius);
    push(primitive.materialId);
  }

  push(static_cast<int>(materials.size()));
  for (std::size_t index = 0; index < materials.size(); ++index) {
    const TestMaterial &material = materials[index];
    push(static_cast<int>(index));
    push(material.red);
    push(material.green);
    push(material.blue);
    push(material.opacity);
    push(material.shadeStrength);
  }

  if (includeUiLightRig) {
    push(2);
    push(0);
    push(1);
    push(550);
    push(0);
    push(0);
    push(1000);
    push(1);
    push(2);
    push(450);
    push(-1000);
    push(-1000);
    push(1000);
  } else {
    push(0);
  }

  push(1); // cameraCount.
  push(0);
  push(1);
  push(width * 1000);
  push(height * 1000);
  push(1000);
  push(0);
  push(0);
  push(-1000000);
  push(1000000);

  return words;
}

int milli(double value) {
  return static_cast<int>(std::lround(value * 1000.0));
}

BgraPixel pixelAt(const primestruct::software_surface::SoftwareSurfaceFrame &frame, int x, int y) {
  const std::size_t byteIndex =
      (static_cast<std::size_t>(y) * static_cast<std::size_t>(frame.width) + static_cast<std::size_t>(x)) * 4u;
  return BgraPixel{frame.pixelsBgra8[byteIndex + 0u],
                   frame.pixelsBgra8[byteIndex + 1u],
                   frame.pixelsBgra8[byteIndex + 2u],
                   frame.pixelsBgra8[byteIndex + 3u]};
}

std::uint64_t hashPixels(const primestruct::software_surface::SoftwareSurfaceFrame &frame) {
  std::uint64_t hash = 14695981039346656037ull;
  for (std::uint8_t byte : frame.pixelsBgra8) {
    hash ^= static_cast<std::uint64_t>(byte);
    hash *= 1099511628211ull;
  }
  return hash;
}

std::uint64_t hashCoverage(const std::vector<std::uint8_t> &coverage) {
  std::uint64_t hash = 14695981039346656037ull;
  for (std::uint8_t byte : coverage) {
    hash ^= static_cast<std::uint64_t>(byte);
    hash *= 1099511628211ull;
  }
  return hash;
}

std::vector<primestruct::scene_bgra8_renderer::SceneTextFixtureFont> makeTextFixtureFonts() {
  using Font = primestruct::scene_bgra8_renderer::SceneTextFixtureFont;
  using Range = primestruct::scene_bgra8_renderer::SceneTextCodepointRange;
  return std::vector<Font>{
      Font{"LatinPrimary", std::vector<Range>{{0x0041u, 0x005au},
                                              {0x0061u, 0x007au},
                                              {0x0300u, 0x036fu}},
           100u, 0.5},
      Font{"CyrillicFixture", std::vector<Range>{{0x0400u, 0x052fu}}, 1000u, 0.75},
      Font{"HebrewFixture", std::vector<Range>{{0x0590u, 0x05ffu}}, 2000u, 0.8},
      Font{"DevanagariFixture", std::vector<Range>{{0x0900u, 0x097fu}}, 3000u, 0.7}};
}

} // namespace

TEST_CASE("scene bgra8 renderer fills clipped flat rect from serialized scene") {
  const BgraPixel transparent{0, 0, 0, 0};
  const BgraPixel red{0, 0, 255, 255};
  const std::vector<std::int32_t> words = makeSceneWords(
      4,
      3,
      std::vector<TestNode>{{0, 0, 0, 0, -1000, 1000, 0}},
      std::vector<TestPrimitive>{{3000, 2000, 0, 0}},
      std::vector<TestMaterial>{{1000, 0, 0, 1000}});

  const auto result = primestruct::scene_bgra8_renderer::renderSerializedSceneToBgra8(words);

  REQUIRE(result.ok);
  CHECK(result.error.empty());
  CHECK(result.frame.width == 4);
  CHECK(result.frame.height == 3);
  std::string error;
  CHECK(primestruct::software_surface::validateFrame(result.frame, error));
  CHECK(pixelAt(result.frame, 0, 1) == red);
  CHECK(pixelAt(result.frame, 1, 1) == red);
  CHECK(pixelAt(result.frame, 2, 1) == transparent);
  CHECK(pixelAt(result.frame, 0, 2) == red);
  CHECK(hashPixels(result.frame) == 0xd990ac7d44b08b7dull);
}

TEST_CASE("scene bgra8 renderer applies rounded rect sdf edge coverage") {
  const BgraPixel transparent{0, 0, 0, 0};
  const BgraPixel edgeGreen{0, 255, 0, 202};
  const BgraPixel solidGreen{0, 255, 0, 255};
  const std::vector<std::int32_t> words = makeSceneWords(
      6,
      6,
      std::vector<TestNode>{{0, 0, 0, 0, 1000, 1000, 0}},
      std::vector<TestPrimitive>{{4000, 4000, 1000, 0}},
      std::vector<TestMaterial>{{0, 1000, 0, 1000}});

  const auto result = primestruct::scene_bgra8_renderer::renderSerializedSceneToBgra8(words);

  REQUIRE(result.ok);
  CHECK(pixelAt(result.frame, 0, 0) == transparent);
  CHECK(pixelAt(result.frame, 1, 1) == edgeGreen);
  CHECK(pixelAt(result.frame, 2, 2) == solidGreen);
  CHECK(hashPixels(result.frame) == 0x92eb0f8a74ae11c5ull);
}

TEST_CASE("scene bgra8 renderer orders painter before local z") {
  const BgraPixel blue{255, 0, 0, 255};
  const std::vector<TestPrimitive> primitives{{1000, 1000, 0, 0}};
  const std::vector<TestMaterial> materials{{1000, 0, 0, 1000}, {0, 0, 1000, 1000}};
  const std::vector<std::int32_t> words = makeSceneWords(
      1,
      1,
      std::vector<TestNode>{{1, 100000, 0, 0, 0, 0, 0}, {2, -100000, 0, 1, 0, 0, 0}},
      primitives,
      materials);

  const auto result = primestruct::scene_bgra8_renderer::renderSerializedSceneToBgra8(words);

  REQUIRE(result.ok);
  CHECK(pixelAt(result.frame, 0, 0) == blue);
}

TEST_CASE("scene bgra8 renderer uses local z for painter ties") {
  const BgraPixel green{0, 255, 0, 255};
  const std::vector<TestPrimitive> primitives{{1000, 1000, 0, 0}};
  const std::vector<TestMaterial> materials{{1000, 0, 0, 1000}, {0, 1000, 0, 1000}};
  const std::vector<std::int32_t> words = makeSceneWords(
      1,
      1,
      std::vector<TestNode>{{0, -1000, 0, 0, 0, 0, 0}, {0, 1000, 0, 1, 0, 0, 0}},
      primitives,
      materials);

  const auto result = primestruct::scene_bgra8_renderer::renderSerializedSceneToBgra8(words);

  REQUIRE(result.ok);
  CHECK(pixelAt(result.frame, 0, 0) == green);
}

TEST_CASE("scene bgra8 renderer source-over blends overlapping primitives") {
  const BgraPixel blendedBlueOverRed{170, 0, 85, 191};
  const std::vector<TestPrimitive> primitives{{1000, 1000, 0, 0}};
  const std::vector<TestMaterial> materials{{1000, 0, 0, 500}, {0, 0, 1000, 500}};
  const std::vector<std::int32_t> words = makeSceneWords(
      1,
      1,
      std::vector<TestNode>{{0, 0, 0, 0, 0, 0, 0}, {1, 0, 0, 1, 0, 0, 0}},
      primitives,
      materials);

  const auto result = primestruct::scene_bgra8_renderer::renderSerializedSceneToBgra8(words);

  REQUIRE(result.ok);
  CHECK(pixelAt(result.frame, 0, 0) == blendedBlueOverRed);
  CHECK(hashPixels(result.frame) == 0x8a3ce6f34f75ad8bull);
}

TEST_CASE("scene bgra8 renderer shades deterministic sdf button states") {
  const BgraPixel normalCenter{195, 132, 38, 255};
  const BgraPixel normalBevel{227, 153, 45, 246};
  const BgraPixel pressedBevel{207, 140, 41, 246};
  const int bevelRadius = milli(primestruct::scene_bgra8_renderer::detail::DefaultSdfButtonBevelRadius);
  const std::vector<TestPrimitive> primitives{{12000, 8000, bevelRadius, 0, 2}};
  const std::vector<TestMaterial> materials{{180, 620, 920, 1000, 1000}};
  const std::vector<std::int32_t> normalWords = makeSceneWords(
      16,
      12,
      std::vector<TestNode>{{0,
                             0,
                             0,
                             0,
                             2000,
                             2000,
                             0,
                             milli(primestruct::scene_bgra8_renderer::detail::DefaultSdfButtonNormalDepth)}},
      primitives,
      materials,
      true);
  const std::vector<std::int32_t> pressedWords = makeSceneWords(
      16,
      12,
      std::vector<TestNode>{{0,
                             0,
                             0,
                             0,
                             2000,
                             2000,
                             0,
                             milli(primestruct::scene_bgra8_renderer::detail::DefaultSdfButtonPressedDepth)}},
      primitives,
      materials,
      true);

  const auto normal = primestruct::scene_bgra8_renderer::renderSerializedSceneToBgra8(normalWords);
  const auto pressed = primestruct::scene_bgra8_renderer::renderSerializedSceneToBgra8(pressedWords);

  REQUIRE(normal.ok);
  REQUIRE(pressed.ok);
  CHECK(pixelAt(normal.frame, 7, 5) == normalCenter);
  CHECK(pixelAt(normal.frame, 3, 3) == normalBevel);
  CHECK(pixelAt(pressed.frame, 3, 3) == pressedBevel);
  CHECK(hashPixels(normal.frame) == 0xc59b6e72c765de6full);
  CHECK(hashPixels(pressed.frame) == 0xee616cc629ff12e7ull);
  CHECK(hashPixels(normal.frame) != hashPixels(pressed.frame));
}

TEST_CASE("scene bgra8 renderer composes sdf button with 2d primitives") {
  const BgraPixel backgroundPixel{20, 18, 15, 255};
  const BgraPixel raisedButtonPixel{223, 150, 44, 255};
  const BgraPixel foregroundPixel{26, 26, 230, 255};
  const int bevelRadius = milli(primestruct::scene_bgra8_renderer::detail::DefaultSdfButtonBevelRadius);
  const std::vector<TestPrimitive> primitives{
      {16000, 10000, 0, 0, 1}, {12000, 8000, bevelRadius, 1, 2}, {4000, 3000, 0, 2, 1}};
  const std::vector<TestMaterial> materials{
      {60, 70, 80, 1000, 1000}, {180, 620, 920, 1000, 1000}, {900, 100, 100, 1000, 1000}};
  const std::vector<std::int32_t> words = makeSceneWords(
      16,
      10,
      std::vector<TestNode>{{0, 0, 0, 0, 0, 0, 0},
                            {1,
                             0,
                             1,
                             1,
                             2000,
                             1000,
                             0,
                             milli(primestruct::scene_bgra8_renderer::detail::DefaultSdfButtonNormalDepth)},
                            {2, 0, 2, 2, 5000, 3000, 0}},
      primitives,
      materials,
      true);

  const auto result = primestruct::scene_bgra8_renderer::renderSerializedSceneToBgra8(words);

  REQUIRE(result.ok);
  CHECK(pixelAt(result.frame, 1, 1) == backgroundPixel);
  CHECK(pixelAt(result.frame, 4, 2) == raisedButtonPixel);
  CHECK(pixelAt(result.frame, 5, 3) == foregroundPixel);
  CHECK(hashPixels(result.frame) == 0xab0df377cebef9bbull);
}

TEST_CASE("scene text shaper emits deterministic international glyph runs") {
  namespace renderer = primestruct::scene_bgra8_renderer;
  const std::vector<renderer::SceneTextFixtureFont> fonts = makeTextFixtureFonts();
  const std::string text = std::string("A") + "\xCC\x81" + "\xD0\x96" + "\xD7\x90\xD7\x91" +
                           "\xE0\xA4\x95\xE0\xA5\x8D\xE0\xA4\xB7";

  const auto first = renderer::shapeSceneTextUtf8(text, fonts, 16.0);
  const auto second = renderer::shapeSceneTextUtf8(text, fonts, 16.0);

  REQUIRE(first.ok);
  REQUIRE(second.ok);
  CHECK(first.error.empty());
  CHECK(first.glyphs.size() == 8u);
  CHECK(first.runs.size() == 4u);
  CHECK(first.width == doctest::Approx(68.0));
  CHECK(first.height == doctest::Approx(16.0));
  CHECK(second.width == first.width);
  CHECK(second.height == first.height);
  REQUIRE(second.glyphs.size() == first.glyphs.size());
  REQUIRE(second.runs.size() == first.runs.size());

  CHECK(first.runs[0].script == renderer::SceneTextScript::Latin);
  CHECK(first.runs[0].direction == renderer::SceneTextDirection::LeftToRight);
  CHECK(first.runs[0].glyphBegin == 0u);
  CHECK(first.runs[0].glyphCount == 2u);
  CHECK(first.glyphs[0].codepoint == 0x0041u);
  CHECK(first.glyphs[0].glyphId == 100u);
  CHECK(first.glyphs[0].fontName == "LatinPrimary");
  CHECK(first.glyphs[0].advance == doctest::Approx(8.0));
  CHECK_FALSE(first.glyphs[0].combiningMark);
  CHECK(first.glyphs[1].codepoint == 0x0301u);
  CHECK(first.glyphs[1].combiningMark);
  CHECK(first.glyphs[1].script == renderer::SceneTextScript::Latin);
  CHECK(first.glyphs[1].advance == doctest::Approx(0.0));
  CHECK(first.glyphs[1].x == doctest::Approx(first.glyphs[0].x));
  CHECK(first.glyphs[1].xOffset == doctest::Approx(3.6));
  CHECK(first.glyphs[1].yOffset == doctest::Approx(-4.0));

  CHECK(first.runs[1].script == renderer::SceneTextScript::Cyrillic);
  CHECK(first.runs[1].direction == renderer::SceneTextDirection::LeftToRight);
  CHECK(first.glyphs[2].codepoint == 0x0416u);
  CHECK(first.glyphs[2].glyphId == 1022u);
  CHECK(first.glyphs[2].fontName == "CyrillicFixture");

  CHECK(first.runs[2].script == renderer::SceneTextScript::Hebrew);
  CHECK(first.runs[2].direction == renderer::SceneTextDirection::RightToLeft);
  CHECK(first.runs[2].glyphBegin == 3u);
  CHECK(first.runs[2].glyphCount == 2u);
  CHECK(first.glyphs[3].codepoint == 0x05d1u);
  CHECK(first.glyphs[3].clusterByteOffset == 7u);
  CHECK(first.glyphs[4].codepoint == 0x05d0u);
  CHECK(first.glyphs[4].clusterByteOffset == 5u);

  CHECK(first.runs[3].script == renderer::SceneTextScript::Devanagari);
  CHECK(first.runs[3].glyphBegin == 5u);
  CHECK(first.runs[3].glyphCount == 3u);
  CHECK(first.glyphs[5].codepoint == 0x0915u);
  CHECK(first.glyphs[6].codepoint == 0x094du);
  CHECK(first.glyphs[6].combiningMark);
  CHECK(first.glyphs[6].advance == doctest::Approx(0.0));
  CHECK(first.glyphs[7].codepoint == 0x0937u);

  for (std::size_t index = 0; index < first.glyphs.size(); ++index) {
    CHECK(second.glyphs[index].codepoint == first.glyphs[index].codepoint);
    CHECK(second.glyphs[index].glyphId == first.glyphs[index].glyphId);
    CHECK(second.glyphs[index].fontName == first.glyphs[index].fontName);
    CHECK(second.glyphs[index].clusterByteOffset == first.glyphs[index].clusterByteOffset);
    CHECK(second.glyphs[index].x == first.glyphs[index].x);
    CHECK(second.glyphs[index].advance == first.glyphs[index].advance);
  }
}

TEST_CASE("scene text shaper chooses first fallback and stable missing glyph") {
  namespace renderer = primestruct::scene_bgra8_renderer;
  using Font = renderer::SceneTextFixtureFont;
  using Range = renderer::SceneTextCodepointRange;
  const std::vector<Font> fonts{
      Font{"FirstAscii", std::vector<Range>{{0x0041u, 0x0041u}}, 500u, 0.5},
      Font{"SecondAscii", std::vector<Range>{{0x0041u, 0x0041u}}, 900u, 0.9}};

  const auto result = renderer::shapeSceneTextUtf8(std::string("A") + "\xF0\x9F\x99\x82", fonts, 10.0);

  REQUIRE(result.ok);
  REQUIRE(result.glyphs.size() == 2u);
  CHECK(result.runs.size() == 2u);
  CHECK(result.glyphs[0].fontName == "FirstAscii");
  CHECK(result.glyphs[0].glyphId == 500u);
  CHECK(result.glyphs[0].advance == doctest::Approx(5.0));
  CHECK(result.glyphs[1].codepoint == 0x1f642u);
  CHECK(result.glyphs[1].script == renderer::SceneTextScript::Unknown);
  CHECK(result.glyphs[1].fontName == "missing-glyph");
  CHECK(result.glyphs[1].glyphId == 0u);
  CHECK(result.glyphs[1].missingGlyph);
  CHECK(result.glyphs[1].advance == doctest::Approx(6.0));
  CHECK(result.width == doctest::Approx(11.0));
}

TEST_CASE("scene text shaper rejects invalid utf8 deterministically") {
  namespace renderer = primestruct::scene_bgra8_renderer;
  const std::string invalid("\xC3\x28", 2u);

  const auto result = renderer::shapeSceneTextUtf8(invalid, makeTextFixtureFonts(), 12.0);

  CHECK_FALSE(result.ok);
  CHECK(result.error == "scene text shaper expected valid UTF-8");
  CHECK(result.glyphs.empty());
  CHECK(result.runs.empty());
}

TEST_CASE("scene text rasterizer packs shaped glyph coverage deterministically") {
  namespace renderer = primestruct::scene_bgra8_renderer;
  const std::vector<renderer::SceneTextFixtureFont> fonts = makeTextFixtureFonts();
  const std::string text = std::string("\xD7\x90\xD7\x91") +
                           "\xE0\xA4\x95\xE0\xA5\x8D\xE0\xA4\xB7";
  const auto shape = renderer::shapeSceneTextUtf8(text, fonts, 16.0);
  REQUIRE(shape.ok);
  renderer::SceneTextRasterOptions options;
  options.atlasWidth = 24;
  options.padding = 1;

  const auto first = renderer::rasterizeSceneTextGlyphs(shape, options);
  const auto second = renderer::rasterizeSceneTextGlyphs(shape, options);

  REQUIRE(first.ok);
  REQUIRE(second.ok);
  CHECK(first.glyphs.size() == shape.glyphs.size());
  CHECK(first.atlas.width == 24);
  CHECK(first.atlas.height > 0);
  CHECK(first.atlas.placements.size() == first.glyphs.size());
  CHECK(first.atlas.coverage == second.atlas.coverage);
  CHECK(hashCoverage(first.atlas.coverage) == hashCoverage(second.atlas.coverage));
  REQUIRE(first.atlas.placements.size() == second.atlas.placements.size());
  for (std::size_t index = 0; index < first.atlas.placements.size(); ++index) {
    CHECK(first.atlas.placements[index].glyphIndex == second.atlas.placements[index].glyphIndex);
    CHECK(first.atlas.placements[index].x == second.atlas.placements[index].x);
    CHECK(first.atlas.placements[index].y == second.atlas.placements[index].y);
    CHECK(first.atlas.placements[index].width == second.atlas.placements[index].width);
    CHECK(first.atlas.placements[index].height == second.atlas.placements[index].height);
  }
  REQUIRE(first.glyphs.size() == 5u);
  CHECK(first.glyphs[0].codepoint == 0x05d1u);
  CHECK(first.glyphs[1].codepoint == 0x05d0u);
  CHECK(first.glyphs[3].codepoint == 0x094du);
  CHECK(first.glyphs[3].combiningMark);
  CHECK(first.glyphs[3].coverage.front() > 0);
}

TEST_CASE("scene text rasterizer emits stable missing glyph coverage") {
  namespace renderer = primestruct::scene_bgra8_renderer;
  using Font = renderer::SceneTextFixtureFont;
  using Range = renderer::SceneTextCodepointRange;
  const std::vector<Font> fonts{Font{"AsciiOnly", std::vector<Range>{{0x0041u, 0x0041u}}, 800u, 0.5}};
  const auto shape = renderer::shapeSceneTextUtf8(std::string("A") + "\xF0\x9F\x99\x82", fonts, 12.0);
  REQUIRE(shape.ok);

  const auto raster = renderer::rasterizeSceneTextGlyphs(shape);

  REQUIRE(raster.ok);
  REQUIRE(raster.glyphs.size() == 2u);
  const auto &missing = raster.glyphs[1];
  CHECK(missing.missingGlyph);
  CHECK(missing.fontName == "missing-glyph");
  CHECK(missing.coverage.front() == 230);
  CHECK(missing.coverage.back() == 230);
  CHECK(hashCoverage(missing.coverage) != 0u);
}

TEST_CASE("scene bgra8 renderer composes text overlay over scene primitives") {
  namespace renderer = primestruct::scene_bgra8_renderer;
  const int bevelRadius = milli(renderer::detail::DefaultSdfButtonBevelRadius);
  const std::vector<TestPrimitive> primitives{
      {16000, 10000, 0, 0, 1}, {12000, 8000, bevelRadius, 1, 2}};
  const std::vector<TestMaterial> materials{
      {60, 70, 80, 1000, 1000}, {180, 620, 920, 1000, 1000}};
  const std::vector<std::int32_t> words = makeSceneWords(
      16,
      10,
      std::vector<TestNode>{{0, 0, 0, 0, 0, 0, 0},
                            {1,
                             0,
                             1,
                             1,
                             2000,
                             1000,
                             0,
                             milli(renderer::detail::DefaultSdfButtonNormalDepth)}},
      primitives,
      materials,
      true);
  renderer::SceneTextOverlay overlay;
  overlay.utf8 = std::string("A") + "\xF0\x9F\x99\x82";
  overlay.shapeOptions.fallbackFonts = makeTextFixtureFonts();
  overlay.shapeOptions.textSize = 8.0;
  overlay.rasterOptions.atlasWidth = 24;
  overlay.x = 3.0;
  overlay.y = 3.0;
  overlay.color = renderer::Bgra8Color{0, 0, 255, 255};

  const auto base = renderer::renderSerializedSceneToBgra8(words);
  const auto first = renderer::renderSerializedSceneWithTextOverlayToBgra8(words, std::vector<renderer::SceneTextOverlay>{overlay});
  const auto second = renderer::renderSerializedSceneWithTextOverlayToBgra8(words, std::vector<renderer::SceneTextOverlay>{overlay});

  REQUIRE(base.ok);
  REQUIRE(first.ok);
  REQUIRE(second.ok);
  CHECK(hashPixels(first.frame) == hashPixels(second.frame));
  CHECK(hashPixels(first.frame) != hashPixels(base.frame));
  CHECK(pixelAt(first.frame, 3, 3) != pixelAt(base.frame, 3, 3));
  CHECK(pixelAt(first.frame, 1, 1) == pixelAt(base.frame, 1, 1));
}

TEST_SUITE_END();
