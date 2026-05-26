#pragma once

#include "scene_bgra8_renderer.h"

#include <algorithm>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace primestruct::ui_scene_surface {

struct UiSceneSurfaceOptions {
  int width = 40;
  int height = 32;
  scene_bgra8_renderer::RenderOptions renderOptions{};
};

struct UiSceneSurfaceResult {
  bool ok = false;
  std::string error;
  software_surface::SoftwareSurfaceFrame frame;
  std::vector<std::int32_t> serializedScene;
  std::vector<scene_bgra8_renderer::SceneTextOverlay> overlays;
};

namespace detail {

constexpr int UiSceneRecordVersion = 1;
constexpr int UiScenePrimitiveRect = 1;
constexpr int UiScenePrimitiveSdfButton = 2;
constexpr int UnsupportedId = -1;

struct UiSceneNode {
  int sceneNodeId = UnsupportedId;
  int layoutNodeId = UnsupportedId;
  int parentSceneNodeId = UnsupportedId;
  int painterOrder = 0;
  int localZMillis = 0;
  int primitiveId = UnsupportedId;
  int materialId = UnsupportedId;
  int x = 0;
  int y = 0;
  int scaleZMillis = 1000;
};

struct UiScenePrimitive {
  int primitiveId = UnsupportedId;
  int kind = 0;
  int width = 0;
  int height = 0;
  int radius = 0;
  int materialId = UnsupportedId;
};

struct UiSceneMaterial {
  int materialId = UnsupportedId;
  int red = 0;
  int green = 0;
  int blue = 0;
  int alpha = 255;
};

struct UiSceneTextOverlayRecord {
  int layoutNodeId = UnsupportedId;
  int sceneNodeId = UnsupportedId;
  int x = 0;
  int y = 0;
  int textSizePx = 16;
  int red = 255;
  int green = 255;
  int blue = 255;
  int alpha = 255;
  std::string utf8;
};

class WordReader {
public:
  explicit WordReader(const std::vector<std::int32_t> &wordsIn) : words(wordsIn) {}

  bool readInt(int &out) {
    if (offset >= words.size()) {
      return false;
    }
    out = static_cast<int>(words[offset]);
    ++offset;
    return true;
  }

  bool finished() const { return offset == words.size(); }

private:
  const std::vector<std::int32_t> &words;
  std::size_t offset = 0;
};

inline int unormByteToMilli(int value) {
  const int clamped = std::clamp(value, 0, 255);
  return (clamped * 1000 + 127) / 255;
}

inline bool readUiSceneRecord(const std::vector<std::int32_t> &words,
                              std::vector<UiSceneNode> &nodesOut,
                              std::vector<UiScenePrimitive> &primitivesOut,
                              std::vector<UiSceneMaterial> &materialsOut,
                              std::string &errorOut) {
  WordReader reader(words);
  int version = 0;
  int nodeCount = 0;
  if (!reader.readInt(version) || version != UiSceneRecordVersion || !reader.readInt(nodeCount) ||
      nodeCount < 0) {
    errorOut = "ui scene bridge expected UiScene.serialize() version 1 and a non-negative node count";
    return false;
  }
  nodesOut.clear();
  nodesOut.reserve(static_cast<std::size_t>(nodeCount));
  for (int index = 0; index < nodeCount; ++index) {
    UiSceneNode node;
    if (!reader.readInt(node.sceneNodeId) || !reader.readInt(node.layoutNodeId) ||
        !reader.readInt(node.parentSceneNodeId) || !reader.readInt(node.painterOrder) ||
        !reader.readInt(node.localZMillis) || !reader.readInt(node.primitiveId) ||
        !reader.readInt(node.materialId) || !reader.readInt(node.x) || !reader.readInt(node.y) ||
        !reader.readInt(node.scaleZMillis)) {
      errorOut = "ui scene bridge reached the end of a scene node record";
      return false;
    }
    nodesOut.push_back(node);
  }

  int primitiveCount = 0;
  if (!reader.readInt(primitiveCount) || primitiveCount < 0) {
    errorOut = "ui scene bridge expected a non-negative primitive count";
    return false;
  }
  primitivesOut.clear();
  primitivesOut.reserve(static_cast<std::size_t>(primitiveCount));
  for (int index = 0; index < primitiveCount; ++index) {
    UiScenePrimitive primitive;
    if (!reader.readInt(primitive.primitiveId) || !reader.readInt(primitive.kind) ||
        !reader.readInt(primitive.width) || !reader.readInt(primitive.height) ||
        !reader.readInt(primitive.radius) || !reader.readInt(primitive.materialId)) {
      errorOut = "ui scene bridge reached the end of a primitive record";
      return false;
    }
    primitivesOut.push_back(primitive);
  }

  int materialCount = 0;
  if (!reader.readInt(materialCount) || materialCount < 0) {
    errorOut = "ui scene bridge expected a non-negative material count";
    return false;
  }
  materialsOut.clear();
  materialsOut.reserve(static_cast<std::size_t>(materialCount));
  for (int index = 0; index < materialCount; ++index) {
    UiSceneMaterial material;
    if (!reader.readInt(material.materialId) || !reader.readInt(material.red) ||
        !reader.readInt(material.green) || !reader.readInt(material.blue) ||
        !reader.readInt(material.alpha)) {
      errorOut = "ui scene bridge reached the end of a material record";
      return false;
    }
    materialsOut.push_back(material);
  }

  if (!reader.finished()) {
    errorOut = "ui scene bridge found trailing words after material records";
    return false;
  }
  return true;
}

inline bool readUiTextOverlayRecords(const std::vector<std::int32_t> &words,
                                     std::vector<UiSceneTextOverlayRecord> &overlaysOut,
                                     std::string &errorOut) {
  WordReader reader(words);
  int version = 0;
  int overlayCount = 0;
  if (!reader.readInt(version) || version != UiSceneRecordVersion || !reader.readInt(overlayCount) ||
      overlayCount < 0) {
    errorOut = "ui scene bridge expected UiSceneTextOverlays.serialize() version 1 and a non-negative overlay count";
    return false;
  }
  overlaysOut.clear();
  overlaysOut.reserve(static_cast<std::size_t>(overlayCount));
  for (int index = 0; index < overlayCount; ++index) {
    UiSceneTextOverlayRecord overlay;
    int textLength = 0;
    if (!reader.readInt(overlay.layoutNodeId) || !reader.readInt(overlay.sceneNodeId) ||
        !reader.readInt(overlay.x) || !reader.readInt(overlay.y) || !reader.readInt(overlay.textSizePx) ||
        !reader.readInt(overlay.red) || !reader.readInt(overlay.green) || !reader.readInt(overlay.blue) ||
        !reader.readInt(overlay.alpha) || !reader.readInt(textLength) || textLength < 0) {
      errorOut = "ui scene bridge reached the end of a text overlay record";
      return false;
    }
    overlay.utf8.reserve(static_cast<std::size_t>(textLength));
    for (int byteIndex = 0; byteIndex < textLength; ++byteIndex) {
      int byte = 0;
      if (!reader.readInt(byte)) {
        errorOut = "ui scene bridge reached the end of a text overlay byte stream";
        return false;
      }
      overlay.utf8.push_back(static_cast<char>(std::clamp(byte, 0, 255)));
    }
    overlaysOut.push_back(std::move(overlay));
  }
  if (!reader.finished()) {
    errorOut = "ui scene bridge found trailing words after text overlay records";
    return false;
  }
  return true;
}

inline std::vector<scene_bgra8_renderer::SceneTextFixtureFont> makeUiTextFixtureFonts() {
  using Font = scene_bgra8_renderer::SceneTextFixtureFont;
  using Range = scene_bgra8_renderer::SceneTextCodepointRange;
  return std::vector<Font>{
      Font{"UiLatin", std::vector<Range>{{0x0041u, 0x005au}, {0x0061u, 0x007au}, {0x0300u, 0x036fu}}, 100u, 0.5},
      Font{"UiCyrillic", std::vector<Range>{{0x0400u, 0x052fu}}, 1000u, 0.75},
      Font{"UiHebrew", std::vector<Range>{{0x0590u, 0x05ffu}}, 2000u, 0.8},
      Font{"UiDevanagari", std::vector<Range>{{0x0900u, 0x097fu}}, 3000u, 0.7}};
}

inline std::vector<std::int32_t> makeRendererSceneWords(const std::vector<UiSceneNode> &nodes,
                                                        const std::vector<UiScenePrimitive> &primitives,
                                                        const std::vector<UiSceneMaterial> &materials,
                                                        const UiSceneSurfaceOptions &options) {
  std::vector<std::int32_t> words;
  auto push = [&](int value) { words.push_back(static_cast<std::int32_t>(value)); };

  push(1);
  push(0);

  push(static_cast<int>(nodes.size()));
  for (const UiSceneNode &node : nodes) {
    push(node.sceneNodeId);
    push(node.parentSceneNodeId);
    push(node.painterOrder);
    push(node.localZMillis);
    push(node.primitiveId);
    push(node.materialId);
    push(node.x * 1000);
    push(node.y * 1000);
    push(node.localZMillis);
    push(1000);
    push(1000);
    push(node.scaleZMillis);
    push(0);
  }

  push(static_cast<int>(primitives.size()));
  for (const UiScenePrimitive &primitive : primitives) {
    const int rendererKind = primitive.kind == UiScenePrimitiveSdfButton
                                 ? scene_bgra8_renderer::detail::SdfButtonPrimitiveKind
                                 : scene_bgra8_renderer::detail::RectPrimitiveKind;
    push(primitive.primitiveId);
    push(rendererKind);
    push(primitive.width * 1000);
    push(primitive.height * 1000);
    push(primitive.radius * 1000);
    push(primitive.materialId);
  }

  push(static_cast<int>(materials.size()));
  for (const UiSceneMaterial &material : materials) {
    push(material.materialId);
    push(unormByteToMilli(material.red));
    push(unormByteToMilli(material.green));
    push(unormByteToMilli(material.blue));
    push(unormByteToMilli(material.alpha));
    push(1000);
  }

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

  push(1);
  push(0);
  push(1);
  push(options.width * 1000);
  push(options.height * 1000);
  push(1000);
  push(0);
  push(0);
  push(-1000000);
  push(1000000);
  return words;
}

inline std::vector<scene_bgra8_renderer::SceneTextOverlay> makeRendererTextOverlays(
    const std::vector<UiSceneTextOverlayRecord> &records) {
  std::vector<scene_bgra8_renderer::SceneTextOverlay> overlays;
  overlays.reserve(records.size());
  for (const UiSceneTextOverlayRecord &record : records) {
    scene_bgra8_renderer::SceneTextOverlay overlay;
    overlay.utf8 = record.utf8;
    overlay.shapeOptions.fallbackFonts = makeUiTextFixtureFonts();
    overlay.shapeOptions.textSize = static_cast<double>(std::max(record.textSizePx, 1));
    overlay.rasterOptions.atlasWidth =
        std::max(16, static_cast<int>(record.utf8.size()) * std::max(record.textSizePx, 1));
    overlay.x = static_cast<double>(record.x);
    overlay.y = static_cast<double>(record.y);
    overlay.color = scene_bgra8_renderer::Bgra8Color{
        static_cast<std::uint8_t>(std::clamp(record.blue, 0, 255)),
        static_cast<std::uint8_t>(std::clamp(record.green, 0, 255)),
        static_cast<std::uint8_t>(std::clamp(record.red, 0, 255)),
        static_cast<std::uint8_t>(std::clamp(record.alpha, 0, 255))};
    overlays.push_back(std::move(overlay));
  }
  return overlays;
}

} // namespace detail

inline UiSceneSurfaceResult renderUiSceneToSurface(
    const std::vector<std::int32_t> &serializedUiScene,
    const std::vector<std::int32_t> &serializedUiTextOverlays,
    const UiSceneSurfaceOptions &options = {}) {
  UiSceneSurfaceResult result;
  if (options.width <= 0 || options.height <= 0) {
    result.error = "ui scene bridge expected positive output dimensions";
    return result;
  }

  std::vector<detail::UiSceneNode> nodes;
  std::vector<detail::UiScenePrimitive> primitives;
  std::vector<detail::UiSceneMaterial> materials;
  if (!detail::readUiSceneRecord(serializedUiScene, nodes, primitives, materials, result.error)) {
    return result;
  }

  std::vector<detail::UiSceneTextOverlayRecord> overlayRecords;
  if (!detail::readUiTextOverlayRecords(serializedUiTextOverlays, overlayRecords, result.error)) {
    return result;
  }

  result.serializedScene = detail::makeRendererSceneWords(nodes, primitives, materials, options);
  result.overlays = detail::makeRendererTextOverlays(overlayRecords);
  scene_bgra8_renderer::SceneRenderResult renderResult =
      scene_bgra8_renderer::renderSerializedSceneWithTextOverlayToBgra8(
          result.serializedScene, result.overlays, options.renderOptions);
  if (!renderResult.ok) {
    result.error = renderResult.error;
    return result;
  }
  result.frame = std::move(renderResult.frame);
  result.ok = true;
  return result;
}

inline std::vector<std::int32_t> demoSerializedUiScene() {
  return {1, 3, 0, 1, -1, 0, 0, 0, 0, 7, 8, 1000, 1, 2, 0, 1, 0, -1, -1, 9, 10, 1000,
          2, 3, 0, 2, 3000, 1, 1, 9, 21, 3000, 2, 0, 1, 38, 29, 4, 0, 1, 2, 34, 14,
          3, 1, 2, 0, 9, 8, 7, 255, 1, 50, 60, 70, 255};
}

inline std::vector<std::int32_t> demoSerializedUiTextOverlays() {
  return {1, 2, 2, 1, 9, 10, 10, 1, 2, 3, 255, 2, 72, 105, 3, 2, 11, 23, 10,
          250, 251, 252, 255, 2, 71, 111};
}

inline UiSceneSurfaceResult makeDemoUiSceneSurfaceFrame(const UiSceneSurfaceOptions &options = {}) {
  return renderUiSceneToSurface(demoSerializedUiScene(), demoSerializedUiTextOverlays(), options);
}

} // namespace primestruct::ui_scene_surface
