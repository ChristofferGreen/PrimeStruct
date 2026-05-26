#include "examples/shared/scene_bgra8_renderer.h"

#include "third_party/doctest.h"

#include <array>
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
};

struct TestPrimitive {
  int width = 1000;
  int height = 1000;
  int radius = 0;
  int materialId = 0;
};

struct TestMaterial {
  int red = 0;
  int green = 0;
  int blue = 0;
  int opacity = 1000;
};

std::vector<std::int32_t> makeSceneWords(int width,
                                         int height,
                                         const std::vector<TestNode> &nodes,
                                         const std::vector<TestPrimitive> &primitives,
                                         const std::vector<TestMaterial> &materials) {
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
    push(1000);
    push(0);
  }

  push(static_cast<int>(primitives.size()));
  for (std::size_t index = 0; index < primitives.size(); ++index) {
    const TestPrimitive &primitive = primitives[index];
    push(static_cast<int>(index));
    push(1);
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
    push(1000);
  }

  push(0); // lightCount; lights are not consumed by the 2D BGRA8 slice.

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

TEST_SUITE_END();
