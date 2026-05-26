#pragma once

#include "software_surface_bridge.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace primestruct::scene_bgra8_renderer {

struct Bgra8Color {
  std::uint8_t blue = 0;
  std::uint8_t green = 0;
  std::uint8_t red = 0;
  std::uint8_t alpha = 0;
};

struct RenderOptions {
  Bgra8Color clearColor{};
};

struct SceneRenderResult {
  bool ok = false;
  std::string error;
  software_surface::SoftwareSurfaceFrame frame;
};

enum class SceneTextDirection {
  LeftToRight,
  RightToLeft,
};

enum class SceneTextScript {
  Common,
  Latin,
  Cyrillic,
  Greek,
  Hebrew,
  Arabic,
  Devanagari,
  Unknown,
};

struct SceneTextCodepointRange {
  std::uint32_t first = 0;
  std::uint32_t last = 0;
};

struct SceneTextFixtureFont {
  std::string name;
  std::vector<SceneTextCodepointRange> coverage;
  std::uint32_t glyphBase = 1;
  double advanceScale = 1.0;
};

struct SceneTextShapeOptions {
  std::vector<SceneTextFixtureFont> fallbackFonts;
  double textSize = 16.0;
  double missingGlyphAdvanceScale = 0.6;
};

struct SceneTextGlyph {
  std::uint32_t codepoint = 0;
  std::uint32_t glyphId = 0;
  std::string fontName;
  SceneTextScript script = SceneTextScript::Common;
  SceneTextDirection direction = SceneTextDirection::LeftToRight;
  std::size_t clusterByteOffset = 0;
  double x = 0.0;
  double y = 0.0;
  double xOffset = 0.0;
  double yOffset = 0.0;
  double advance = 0.0;
  bool combiningMark = false;
  bool missingGlyph = false;
};

struct SceneTextGlyphRun {
  SceneTextDirection direction = SceneTextDirection::LeftToRight;
  SceneTextScript script = SceneTextScript::Common;
  std::size_t glyphBegin = 0;
  std::size_t glyphCount = 0;
  double advance = 0.0;
};

struct SceneTextShapeResult {
  bool ok = false;
  std::string error;
  std::vector<SceneTextGlyph> glyphs;
  std::vector<SceneTextGlyphRun> runs;
  double width = 0.0;
  double height = 0.0;
};

struct SceneTextRasterOptions {
  int atlasWidth = 64;
  int padding = 1;
};

struct SceneTextGlyphBitmap {
  std::size_t glyphIndex = 0;
  std::uint32_t codepoint = 0;
  std::uint32_t glyphId = 0;
  std::string fontName;
  int width = 0;
  int height = 0;
  double drawX = 0.0;
  double drawY = 0.0;
  bool missingGlyph = false;
  bool combiningMark = false;
  std::vector<std::uint8_t> coverage;
};

struct SceneTextAtlasPlacement {
  std::size_t glyphIndex = 0;
  int x = 0;
  int y = 0;
  int width = 0;
  int height = 0;
};

struct SceneTextGlyphAtlas {
  int width = 0;
  int height = 0;
  std::vector<SceneTextAtlasPlacement> placements;
  std::vector<std::uint8_t> coverage;
};

struct SceneTextRasterResult {
  bool ok = false;
  std::string error;
  std::vector<SceneTextGlyphBitmap> glyphs;
  SceneTextGlyphAtlas atlas;
  double width = 0.0;
  double height = 0.0;
};

struct SceneTextOverlay {
  std::string utf8;
  SceneTextShapeOptions shapeOptions;
  SceneTextRasterOptions rasterOptions;
  double x = 0.0;
  double y = 0.0;
  Bgra8Color color{255, 255, 255, 255};
};

namespace detail {

constexpr int SceneRecordVersion = 1;
constexpr int OrthographicProjectionMode = 1;
constexpr int RectPrimitiveKind = 1;
constexpr int SdfButtonPrimitiveKind = 2;
constexpr int MaxSoftwareSurfaceExtent = 16384;
constexpr double DefaultSdfButtonBevelRadius = 4.0;
constexpr double DefaultSdfButtonNormalDepth = 3.0;
constexpr double DefaultSdfButtonPressedDepth = 1.0;
constexpr double DefaultUiLightAmbientWeight = 0.55;
constexpr double DefaultUiLightKeyWeight = 0.45;

struct NormalizedPixel {
  double blue = 0.0;
  double green = 0.0;
  double red = 0.0;
  double alpha = 0.0;
};

struct SceneNode {
  int id = -1;
  int parentId = -1;
  int painterOrder = 0;
  double localZ = 0.0;
  int primitiveId = -1;
  int materialId = -1;
  double translationX = 0.0;
  double translationY = 0.0;
  double translationZ = 0.0;
  double scaleX = 1.0;
  double scaleY = 1.0;
  double scaleZ = 1.0;
  double rotationZDegrees = 0.0;
};

struct ScenePrimitive {
  int id = -1;
  int kind = 0;
  double width = 0.0;
  double height = 0.0;
  double radius = 0.0;
  int materialId = -1;
};

struct SceneMaterial {
  int id = -1;
  double red = 1.0;
  double green = 1.0;
  double blue = 1.0;
  double opacity = 1.0;
  double shadeStrength = 1.0;
};

struct SceneCamera {
  int id = -1;
  int projectionMode = OrthographicProjectionMode;
  double viewportWidth = 1.0;
  double viewportHeight = 1.0;
  double unitsPerPixel = 1.0;
  double originX = 0.0;
  double originY = 0.0;
  double nearZ = -1000.0;
  double farZ = 1000.0;
};

struct SceneLight {
  int id = -1;
  int kind = 0;
  double weight = 0.0;
  double directionX = 0.0;
  double directionY = 0.0;
  double directionZ = 1.0;
};

struct SceneRecord {
  int activeCameraId = -1;
  std::vector<SceneNode> nodes;
  std::vector<ScenePrimitive> primitives;
  std::vector<SceneMaterial> materials;
  std::vector<SceneLight> lights;
  std::vector<SceneCamera> cameras;
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

  bool readMilli(double &out) {
    int raw = 0;
    if (!readInt(raw)) {
      return false;
    }
    out = static_cast<double>(raw) / 1000.0;
    return true;
  }

  bool finished() const { return offset == words.size(); }

private:
  const std::vector<std::int32_t> &words;
  std::size_t offset = 0;
};

inline double clampUnit(double value) { return std::clamp(value, 0.0, 1.0); }

struct DecodedScalar {
  std::uint32_t codepoint = 0;
  std::size_t byteOffset = 0;
};

inline bool isContinuationByte(unsigned char byte) {
  return (byte & 0xc0u) == 0x80u;
}

inline bool readUtf8Scalar(std::string_view text,
                           std::size_t &offset,
                           DecodedScalar &scalarOut) {
  if (offset >= text.size()) {
    return false;
  }
  const std::size_t start = offset;
  const unsigned char first = static_cast<unsigned char>(text[offset]);
  if (first < 0x80u) {
    scalarOut = DecodedScalar{first, start};
    ++offset;
    return true;
  }

  std::uint32_t codepoint = 0;
  std::size_t width = 0;
  std::uint32_t minimum = 0;
  if ((first & 0xe0u) == 0xc0u) {
    codepoint = first & 0x1fu;
    width = 2;
    minimum = 0x80u;
  } else if ((first & 0xf0u) == 0xe0u) {
    codepoint = first & 0x0fu;
    width = 3;
    minimum = 0x800u;
  } else if ((first & 0xf8u) == 0xf0u) {
    codepoint = first & 0x07u;
    width = 4;
    minimum = 0x10000u;
  } else {
    return false;
  }
  if (offset + width > text.size()) {
    return false;
  }
  for (std::size_t index = 1; index < width; ++index) {
    const unsigned char byte = static_cast<unsigned char>(text[offset + index]);
    if (!isContinuationByte(byte)) {
      return false;
    }
    codepoint = (codepoint << 6u) | static_cast<std::uint32_t>(byte & 0x3fu);
  }
  if (codepoint < minimum || codepoint > 0x10ffffu ||
      (codepoint >= 0xd800u && codepoint <= 0xdfffu)) {
    return false;
  }
  scalarOut = DecodedScalar{codepoint, start};
  offset += width;
  return true;
}

inline bool decodeUtf8(std::string_view text,
                       std::vector<DecodedScalar> &scalarsOut,
                       std::string &errorOut) {
  std::size_t offset = 0;
  while (offset < text.size()) {
    DecodedScalar scalar;
    if (!readUtf8Scalar(text, offset, scalar)) {
      errorOut = "scene text shaper expected valid UTF-8";
      return false;
    }
    scalarsOut.push_back(scalar);
  }
  return true;
}

inline bool codepointInRange(std::uint32_t codepoint,
                             std::uint32_t first,
                             std::uint32_t last) {
  return codepoint >= first && codepoint <= last;
}

inline SceneTextScript classifyTextScript(std::uint32_t codepoint) {
  if ((codepointInRange(codepoint, 0x0041u, 0x005au) ||
       codepointInRange(codepoint, 0x0061u, 0x007au) ||
       codepointInRange(codepoint, 0x00c0u, 0x024fu))) {
    return SceneTextScript::Latin;
  }
  if (codepoint <= 0x007fu || codepointInRange(codepoint, 0x0300u, 0x036fu) ||
      codepointInRange(codepoint, 0x2000u, 0x206fu)) {
    return SceneTextScript::Common;
  }
  if (codepointInRange(codepoint, 0x0370u, 0x03ffu)) {
    return SceneTextScript::Greek;
  }
  if (codepointInRange(codepoint, 0x0400u, 0x052fu)) {
    return SceneTextScript::Cyrillic;
  }
  if (codepointInRange(codepoint, 0x0590u, 0x05ffu)) {
    return SceneTextScript::Hebrew;
  }
  if (codepointInRange(codepoint, 0x0600u, 0x06ffu) ||
      codepointInRange(codepoint, 0x0750u, 0x077fu)) {
    return SceneTextScript::Arabic;
  }
  if (codepointInRange(codepoint, 0x0900u, 0x097fu)) {
    return SceneTextScript::Devanagari;
  }
  return SceneTextScript::Unknown;
}

inline SceneTextDirection classifyTextDirection(SceneTextScript script) {
  return (script == SceneTextScript::Hebrew || script == SceneTextScript::Arabic)
             ? SceneTextDirection::RightToLeft
             : SceneTextDirection::LeftToRight;
}

inline bool isCombiningTextMark(std::uint32_t codepoint) {
  return codepointInRange(codepoint, 0x0300u, 0x036fu) ||
         codepointInRange(codepoint, 0x0591u, 0x05bdu) ||
         codepointInRange(codepoint, 0x05bfu, 0x05bfu) ||
         codepointInRange(codepoint, 0x05c1u, 0x05c2u) ||
         codepointInRange(codepoint, 0x05c4u, 0x05c5u) ||
         codepointInRange(codepoint, 0x05c7u, 0x05c7u) ||
         codepointInRange(codepoint, 0x064bu, 0x065fu) ||
         codepointInRange(codepoint, 0x093cu, 0x094du);
}

inline bool fontCoversCodepoint(const SceneTextFixtureFont &font,
                                std::uint32_t codepoint,
                                std::uint32_t &glyphIdOut) {
  for (const SceneTextCodepointRange &range : font.coverage) {
    if (range.first <= range.last && codepointInRange(codepoint, range.first, range.last)) {
      glyphIdOut = font.glyphBase + (codepoint - range.first);
      return true;
    }
  }
  return false;
}

inline double glyphAdvance(const SceneTextFixtureFont *font,
                           const SceneTextShapeOptions &options,
                           bool missingGlyph,
                           bool combiningMark) {
  if (combiningMark) {
    return 0.0;
  }
  const double scale = missingGlyph ? options.missingGlyphAdvanceScale : font->advanceScale;
  return options.textSize * std::max(scale, 0.0);
}

struct PendingTextGlyph {
  SceneTextGlyph glyph;
  const SceneTextFixtureFont *font = nullptr;
};

struct ShapedTextScalar {
  DecodedScalar scalar;
  SceneTextScript script = SceneTextScript::Common;
  SceneTextDirection direction = SceneTextDirection::LeftToRight;
  bool combiningMark = false;
};

struct ShapedTextScalarRun {
  SceneTextScript script = SceneTextScript::Common;
  SceneTextDirection direction = SceneTextDirection::LeftToRight;
  std::size_t scalarBegin = 0;
  std::size_t scalarEnd = 0;
};

inline SceneTextScript inheritedTextScript(SceneTextScript script,
                                           bool combiningMark,
                                           SceneTextScript previousStrongScript) {
  if (combiningMark && previousStrongScript != SceneTextScript::Common &&
      previousStrongScript != SceneTextScript::Unknown) {
    return previousStrongScript;
  }
  return script;
}

inline bool isStrongTextScript(SceneTextScript script) {
  return script != SceneTextScript::Common && script != SceneTextScript::Unknown;
}

inline PendingTextGlyph makePendingTextGlyph(const ShapedTextScalar &scalar,
                                             const SceneTextShapeOptions &options) {
  PendingTextGlyph pending;
  pending.glyph.codepoint = scalar.scalar.codepoint;
  pending.glyph.script = scalar.script;
  pending.glyph.direction = scalar.direction;
  pending.glyph.clusterByteOffset = scalar.scalar.byteOffset;
  pending.glyph.combiningMark = scalar.combiningMark;

  std::uint32_t glyphId = 0;
  for (const SceneTextFixtureFont &font : options.fallbackFonts) {
    if (fontCoversCodepoint(font, scalar.scalar.codepoint, glyphId)) {
      pending.font = &font;
      pending.glyph.glyphId = glyphId;
      pending.glyph.fontName = font.name;
      return pending;
    }
  }

  pending.glyph.fontName = "missing-glyph";
  pending.glyph.missingGlyph = true;
  return pending;
}

inline void appendSceneTextGlyph(PendingTextGlyph pending,
                                 const SceneTextShapeOptions &options,
                                 double &cursorX,
                                 double &lastBaseX,
                                 double &lastBaseAdvance,
                                 bool &haveLastBase,
                                 SceneTextShapeResult &result) {
  pending.glyph.advance =
      glyphAdvance(pending.font, options, pending.glyph.missingGlyph, pending.glyph.combiningMark);
  if (pending.glyph.combiningMark) {
    pending.glyph.x = haveLastBase ? lastBaseX : cursorX;
    pending.glyph.xOffset = haveLastBase ? lastBaseAdvance * 0.45 : 0.0;
    pending.glyph.yOffset = -options.textSize * 0.25;
  } else {
    pending.glyph.x = cursorX;
    lastBaseX = cursorX;
    lastBaseAdvance = pending.glyph.advance;
    haveLastBase = true;
    cursorX += pending.glyph.advance;
  }
  result.glyphs.push_back(std::move(pending.glyph));
}

inline void appendSceneTextRun(const std::vector<ShapedTextScalar> &scalars,
                               const ShapedTextScalarRun &run,
                               const SceneTextShapeOptions &options,
                               double &cursorX,
                               double &lastBaseX,
                               double &lastBaseAdvance,
                               bool &haveLastBase,
                               SceneTextShapeResult &result) {
  SceneTextGlyphRun glyphRun;
  glyphRun.direction = run.direction;
  glyphRun.script = run.script;
  glyphRun.glyphBegin = result.glyphs.size();
  const double startX = cursorX;

  if (run.direction == SceneTextDirection::RightToLeft) {
    for (std::size_t scalarIndex = run.scalarEnd; scalarIndex > run.scalarBegin; --scalarIndex) {
      appendSceneTextGlyph(makePendingTextGlyph(scalars[scalarIndex - 1u], options),
                           options,
                           cursorX,
                           lastBaseX,
                           lastBaseAdvance,
                           haveLastBase,
                           result);
    }
  } else {
    for (std::size_t scalarIndex = run.scalarBegin; scalarIndex < run.scalarEnd; ++scalarIndex) {
      appendSceneTextGlyph(makePendingTextGlyph(scalars[scalarIndex], options),
                           options,
                           cursorX,
                           lastBaseX,
                           lastBaseAdvance,
                           haveLastBase,
                           result);
    }
  }

  glyphRun.glyphCount = result.glyphs.size() - glyphRun.glyphBegin;
  glyphRun.advance = cursorX - startX;
  result.runs.push_back(glyphRun);
}

inline std::uint8_t unitToUnorm(double value) {
  return static_cast<std::uint8_t>(std::lround(clampUnit(value) * 255.0));
}

inline NormalizedPixel normalize(Bgra8Color color) {
  return NormalizedPixel{static_cast<double>(color.blue) / 255.0,
                         static_cast<double>(color.green) / 255.0,
                         static_cast<double>(color.red) / 255.0,
                         static_cast<double>(color.alpha) / 255.0};
}

inline NormalizedPixel readPixel(const software_surface::SoftwareSurfaceFrame &frame,
                                 std::size_t pixelIndex) {
  const std::size_t byteIndex = pixelIndex * 4u;
  return NormalizedPixel{
      static_cast<double>(frame.pixelsBgra8[byteIndex + 0u]) / 255.0,
      static_cast<double>(frame.pixelsBgra8[byteIndex + 1u]) / 255.0,
      static_cast<double>(frame.pixelsBgra8[byteIndex + 2u]) / 255.0,
      static_cast<double>(frame.pixelsBgra8[byteIndex + 3u]) / 255.0};
}

inline void writePixel(const NormalizedPixel &pixel,
                       software_surface::SoftwareSurfaceFrame &frame,
                       std::size_t pixelIndex) {
  const std::size_t byteIndex = pixelIndex * 4u;
  frame.pixelsBgra8[byteIndex + 0u] = unitToUnorm(pixel.blue);
  frame.pixelsBgra8[byteIndex + 1u] = unitToUnorm(pixel.green);
  frame.pixelsBgra8[byteIndex + 2u] = unitToUnorm(pixel.red);
  frame.pixelsBgra8[byteIndex + 3u] = unitToUnorm(pixel.alpha);
}

inline void blendSourceOver(NormalizedPixel &destination,
                            double sourceRed,
                            double sourceGreen,
                            double sourceBlue,
                            double sourceAlpha) {
  sourceAlpha = clampUnit(sourceAlpha);
  if (sourceAlpha <= 0.0) {
    return;
  }

  const double keptDestination = destination.alpha * (1.0 - sourceAlpha);
  const double outAlpha = sourceAlpha + keptDestination;
  if (outAlpha <= 0.0) {
    destination = NormalizedPixel{};
    return;
  }

  destination.red =
      (clampUnit(sourceRed) * sourceAlpha + destination.red * keptDestination) / outAlpha;
  destination.green =
      (clampUnit(sourceGreen) * sourceAlpha + destination.green * keptDestination) / outAlpha;
  destination.blue =
      (clampUnit(sourceBlue) * sourceAlpha + destination.blue * keptDestination) / outAlpha;
  destination.alpha = outAlpha;
}

template <typename Record>
inline const Record *findById(const std::vector<Record> &records, int id) {
  if (id < 0) {
    return nullptr;
  }
  for (const Record &record : records) {
    if (record.id == id) {
      return &record;
    }
  }
  return nullptr;
}

inline bool readSceneRecord(const std::vector<std::int32_t> &words,
                            SceneRecord &scene,
                            std::string &errorOut) {
  WordReader reader(words);

  int version = 0;
  if (!reader.readInt(version) || version != SceneRecordVersion) {
    errorOut = "scene renderer expected Scene.serialize() version 1";
    return false;
  }
  if (!reader.readInt(scene.activeCameraId)) {
    errorOut = "scene renderer record ended before active camera id";
    return false;
  }

  int nodeCount = 0;
  if (!reader.readInt(nodeCount) || nodeCount < 0) {
    errorOut = "scene renderer record has invalid node count";
    return false;
  }
  scene.nodes.reserve(static_cast<std::size_t>(nodeCount));
  for (int index = 0; index < nodeCount; ++index) {
    SceneNode node;
    if (!reader.readInt(node.id) || !reader.readInt(node.parentId) ||
        !reader.readInt(node.painterOrder) || !reader.readMilli(node.localZ) ||
        !reader.readInt(node.primitiveId) || !reader.readInt(node.materialId) ||
        !reader.readMilli(node.translationX) || !reader.readMilli(node.translationY) ||
        !reader.readMilli(node.translationZ) || !reader.readMilli(node.scaleX) ||
        !reader.readMilli(node.scaleY) || !reader.readMilli(node.scaleZ) ||
        !reader.readMilli(node.rotationZDegrees)) {
      errorOut = "scene renderer record ended inside node table";
      return false;
    }
    scene.nodes.push_back(node);
  }

  int primitiveCount = 0;
  if (!reader.readInt(primitiveCount) || primitiveCount < 0) {
    errorOut = "scene renderer record has invalid primitive count";
    return false;
  }
  scene.primitives.reserve(static_cast<std::size_t>(primitiveCount));
  for (int index = 0; index < primitiveCount; ++index) {
    ScenePrimitive primitive;
    if (!reader.readInt(primitive.id) || !reader.readInt(primitive.kind) ||
        !reader.readMilli(primitive.width) || !reader.readMilli(primitive.height) ||
        !reader.readMilli(primitive.radius) || !reader.readInt(primitive.materialId)) {
      errorOut = "scene renderer record ended inside primitive table";
      return false;
    }
    scene.primitives.push_back(primitive);
  }

  int materialCount = 0;
  if (!reader.readInt(materialCount) || materialCount < 0) {
    errorOut = "scene renderer record has invalid material count";
    return false;
  }
  scene.materials.reserve(static_cast<std::size_t>(materialCount));
  for (int index = 0; index < materialCount; ++index) {
    SceneMaterial material;
    if (!reader.readInt(material.id) || !reader.readMilli(material.red) ||
        !reader.readMilli(material.green) || !reader.readMilli(material.blue) ||
        !reader.readMilli(material.opacity) || !reader.readMilli(material.shadeStrength)) {
      errorOut = "scene renderer record ended inside material table";
      return false;
    }
    scene.materials.push_back(material);
  }

  int lightCount = 0;
  if (!reader.readInt(lightCount) || lightCount < 0) {
    errorOut = "scene renderer record has invalid light count";
    return false;
  }
  scene.lights.reserve(static_cast<std::size_t>(lightCount));
  for (int index = 0; index < lightCount; ++index) {
    SceneLight light;
    if (!reader.readInt(light.id) || !reader.readInt(light.kind) ||
        !reader.readMilli(light.weight) || !reader.readMilli(light.directionX) ||
        !reader.readMilli(light.directionY) || !reader.readMilli(light.directionZ)) {
      errorOut = "scene renderer record ended inside light table";
      return false;
    }
    scene.lights.push_back(light);
  }

  int cameraCount = 0;
  if (!reader.readInt(cameraCount) || cameraCount < 0) {
    errorOut = "scene renderer record has invalid camera count";
    return false;
  }
  scene.cameras.reserve(static_cast<std::size_t>(cameraCount));
  for (int index = 0; index < cameraCount; ++index) {
    SceneCamera camera;
    if (!reader.readInt(camera.id) || !reader.readInt(camera.projectionMode) ||
        !reader.readMilli(camera.viewportWidth) || !reader.readMilli(camera.viewportHeight) ||
        !reader.readMilli(camera.unitsPerPixel) || !reader.readMilli(camera.originX) ||
        !reader.readMilli(camera.originY) || !reader.readMilli(camera.nearZ) ||
        !reader.readMilli(camera.farZ)) {
      errorOut = "scene renderer record ended inside camera table";
      return false;
    }
    scene.cameras.push_back(camera);
  }

  if (!reader.finished()) {
    errorOut = "scene renderer record has trailing words";
    return false;
  }
  return true;
}

inline bool isRenderableExtent(int width, int height) {
  return width > 0 && height > 0 && width <= MaxSoftwareSurfaceExtent &&
         height <= MaxSoftwareSurfaceExtent;
}

inline double roundedRectCoverage(double localX,
                                  double localY,
                                  double width,
                                  double height,
                                  double radius,
                                  double edgeWidth) {
  if (width <= 0.0 || height <= 0.0) {
    return 0.0;
  }
  radius = std::clamp(radius, 0.0, std::min(width, height) * 0.5);
  if (radius <= 0.0) {
    return (localX >= 0.0 && localY >= 0.0 && localX < width && localY < height) ? 1.0 : 0.0;
  }

  const double halfWidth = width * 0.5;
  const double halfHeight = height * 0.5;
  const double centeredX = localX - halfWidth;
  const double centeredY = localY - halfHeight;
  const double qx = std::abs(centeredX) - (halfWidth - radius);
  const double qy = std::abs(centeredY) - (halfHeight - radius);
  const double outsideX = std::max(qx, 0.0);
  const double outsideY = std::max(qy, 0.0);
  const double outsideDistance = std::sqrt(outsideX * outsideX + outsideY * outsideY);
  const double insideDistance = std::min(std::max(qx, qy), 0.0);
  const double signedDistance = outsideDistance + insideDistance - radius;
  const double safeEdgeWidth = std::max(edgeWidth, 0.000001);
  return clampUnit(0.5 - signedDistance / safeEdgeWidth);
}

inline double roundedRectSignedDistance(double localX,
                                        double localY,
                                        double width,
                                        double height,
                                        double radius) {
  if (width <= 0.0 || height <= 0.0) {
    return 1.0;
  }
  radius = std::clamp(radius, 0.0, std::min(width, height) * 0.5);
  if (radius <= 0.0) {
    const double outsideX = std::max(std::max(-localX, localX - width), 0.0);
    const double outsideY = std::max(std::max(-localY, localY - height), 0.0);
    const double outsideDistance = std::sqrt(outsideX * outsideX + outsideY * outsideY);
    const double insideDistance =
        std::min(std::max(std::max(-localX, localX - width), std::max(-localY, localY - height)), 0.0);
    return outsideDistance + insideDistance;
  }

  const double halfWidth = width * 0.5;
  const double halfHeight = height * 0.5;
  const double centeredX = localX - halfWidth;
  const double centeredY = localY - halfHeight;
  const double qx = std::abs(centeredX) - (halfWidth - radius);
  const double qy = std::abs(centeredY) - (halfHeight - radius);
  const double outsideX = std::max(qx, 0.0);
  const double outsideY = std::max(qy, 0.0);
  const double outsideDistance = std::sqrt(outsideX * outsideX + outsideY * outsideY);
  const double insideDistance = std::min(std::max(qx, qy), 0.0);
  return outsideDistance + insideDistance - radius;
}

inline double roundedRectGradientComponent(double positiveDistance, double negativeDistance) {
  const double gradient = positiveDistance - negativeDistance;
  if (std::abs(gradient) <= 0.000001) {
    return 0.0;
  }
  return gradient;
}

inline double sdfButtonShade(double localX,
                             double localY,
                             double width,
                             double height,
                             double bevelRadius,
                             double depth,
                             double shadeStrength) {
  bevelRadius = bevelRadius > 0.0 ? bevelRadius : DefaultSdfButtonBevelRadius;
  depth = depth > 0.0 ? depth : DefaultSdfButtonNormalDepth;
  const double signedDistance =
      roundedRectSignedDistance(localX, localY, width, height, bevelRadius);
  const double interiorDistance = std::max(-signedDistance, 0.0);
  const double bevelT = clampUnit((bevelRadius - interiorDistance) / bevelRadius);
  const double slope = bevelT * depth / bevelRadius;
  const double sample = 0.25;
  double gradientX = roundedRectGradientComponent(
      roundedRectSignedDistance(localX + sample, localY, width, height, bevelRadius),
      roundedRectSignedDistance(localX - sample, localY, width, height, bevelRadius));
  double gradientY = roundedRectGradientComponent(
      roundedRectSignedDistance(localX, localY + sample, width, height, bevelRadius),
      roundedRectSignedDistance(localX, localY - sample, width, height, bevelRadius));
  const double gradientLength = std::sqrt(gradientX * gradientX + gradientY * gradientY);
  if (gradientLength > 0.000001) {
    gradientX /= gradientLength;
    gradientY /= gradientLength;
  }

  const double normalX = gradientX * slope;
  const double normalY = gradientY * slope;
  constexpr double normalZ = 1.0;
  const double normalLength = std::sqrt(normalX * normalX + normalY * normalY + normalZ * normalZ);
  const double unitNormalX = normalX / normalLength;
  const double unitNormalY = normalY / normalLength;
  const double unitNormalZ = normalZ / normalLength;

  constexpr double invSqrt3 = 0.5773502691896258;
  const double keyDot = std::max(0.0, unitNormalX * -invSqrt3 + unitNormalY * -invSqrt3 +
                                           unitNormalZ * invSqrt3);
  const double lit = DefaultUiLightAmbientWeight + DefaultUiLightKeyWeight * keyDot;
  return 1.0 - clampUnit(shadeStrength) + clampUnit(shadeStrength) * lit;
}

inline std::uint8_t fixtureGlyphCoverage(const SceneTextGlyphBitmap &bitmap,
                                         int x,
                                         int y) {
  if (bitmap.width <= 0 || bitmap.height <= 0) {
    return 0;
  }
  if (bitmap.missingGlyph) {
    const bool border = x == 0 || y == 0 || x == bitmap.width - 1 || y == bitmap.height - 1;
    const bool diagonal = x == y || x == bitmap.width - 1 - y;
    return border || diagonal ? 230 : 0;
  }
  if (bitmap.combiningMark) {
    return y <= std::max(0, bitmap.height / 3) ? 220 : 0;
  }

  const std::uint32_t seed = bitmap.glyphId != 0 ? bitmap.glyphId : bitmap.codepoint;
  const bool stem = x == 0 || x == bitmap.width - 1 || y == bitmap.height - 1;
  const bool fill = ((static_cast<std::uint32_t>(x) * 31u +
                      static_cast<std::uint32_t>(y) * 17u + seed) %
                     11u) < 6u;
  return stem || fill ? 210 : 0;
}

inline SceneTextGlyphBitmap rasterizeSceneTextGlyph(std::size_t glyphIndex,
                                                    const SceneTextGlyph &glyph,
                                                    double textHeight) {
  SceneTextGlyphBitmap bitmap;
  bitmap.glyphIndex = glyphIndex;
  bitmap.codepoint = glyph.codepoint;
  bitmap.glyphId = glyph.glyphId;
  bitmap.fontName = glyph.fontName;
  bitmap.missingGlyph = glyph.missingGlyph;
  bitmap.combiningMark = glyph.combiningMark;
  bitmap.drawX = glyph.x + glyph.xOffset;
  bitmap.drawY = glyph.y + glyph.yOffset;
  const double baseHeight = std::max(textHeight, 1.0);
  const double baseAdvance = std::max(glyph.advance, baseHeight * 0.35);
  bitmap.width = std::max(1, static_cast<int>(std::lround(
                                glyph.combiningMark ? baseHeight * 0.30 : baseAdvance * 0.75)));
  bitmap.height = std::max(1, static_cast<int>(std::lround(
                                 glyph.combiningMark ? baseHeight * 0.22 : baseHeight * 0.80)));
  bitmap.coverage.resize(static_cast<std::size_t>(bitmap.width) *
                         static_cast<std::size_t>(bitmap.height));
  for (int y = 0; y < bitmap.height; ++y) {
    for (int x = 0; x < bitmap.width; ++x) {
      bitmap.coverage[static_cast<std::size_t>(y) * static_cast<std::size_t>(bitmap.width) +
                      static_cast<std::size_t>(x)] =
          fixtureGlyphCoverage(bitmap, x, y);
    }
  }
  return bitmap;
}

} // namespace detail

inline SceneTextShapeResult shapeSceneTextUtf8(
    std::string_view utf8,
    const SceneTextShapeOptions &options = {}) {
  SceneTextShapeResult result;
  if (options.textSize <= 0.0 || !std::isfinite(options.textSize)) {
    result.error = "scene text shaper text size must be positive";
    return result;
  }
  if (options.missingGlyphAdvanceScale < 0.0 ||
      !std::isfinite(options.missingGlyphAdvanceScale)) {
    result.error = "scene text shaper missing glyph advance scale must be non-negative";
    return result;
  }
  for (const SceneTextFixtureFont &font : options.fallbackFonts) {
    if (!std::isfinite(font.advanceScale)) {
      result.error = "scene text shaper font advance scale must be finite";
      return result;
    }
  }

  std::vector<detail::DecodedScalar> decoded;
  if (!detail::decodeUtf8(utf8, decoded, result.error)) {
    return result;
  }

  std::vector<detail::ShapedTextScalar> scalars;
  scalars.reserve(decoded.size());
  SceneTextScript previousStrongScript = SceneTextScript::Common;
  for (const detail::DecodedScalar &scalar : decoded) {
    detail::ShapedTextScalar shaped;
    shaped.scalar = scalar;
    shaped.combiningMark = detail::isCombiningTextMark(scalar.codepoint);
    shaped.script = detail::inheritedTextScript(
        detail::classifyTextScript(scalar.codepoint), shaped.combiningMark, previousStrongScript);
    shaped.direction = detail::classifyTextDirection(shaped.script);
    if (!shaped.combiningMark && detail::isStrongTextScript(shaped.script)) {
      previousStrongScript = shaped.script;
    }
    scalars.push_back(shaped);
  }

  std::vector<detail::ShapedTextScalarRun> scalarRuns;
  for (std::size_t index = 0; index < scalars.size(); ++index) {
    const detail::ShapedTextScalar &scalar = scalars[index];
    if (scalarRuns.empty() || scalarRuns.back().script != scalar.script ||
        scalarRuns.back().direction != scalar.direction) {
      scalarRuns.push_back(detail::ShapedTextScalarRun{
          scalar.script, scalar.direction, index, index + 1u});
      continue;
    }
    scalarRuns.back().scalarEnd = index + 1u;
  }

  double cursorX = 0.0;
  double lastBaseX = 0.0;
  double lastBaseAdvance = 0.0;
  bool haveLastBase = false;
  for (const detail::ShapedTextScalarRun &run : scalarRuns) {
    detail::appendSceneTextRun(
        scalars, run, options, cursorX, lastBaseX, lastBaseAdvance, haveLastBase, result);
  }

  result.width = cursorX;
  result.height = options.textSize;
  result.ok = true;
  return result;
}

inline SceneTextShapeResult shapeSceneTextUtf8(
    std::string_view utf8,
    const std::vector<SceneTextFixtureFont> &fallbackFonts,
    double textSize) {
  SceneTextShapeOptions options;
  options.fallbackFonts = fallbackFonts;
  options.textSize = textSize;
  return shapeSceneTextUtf8(utf8, options);
}

inline SceneTextRasterResult rasterizeSceneTextGlyphs(
    const SceneTextShapeResult &shape,
    const SceneTextRasterOptions &options = {}) {
  SceneTextRasterResult result;
  if (!shape.ok) {
    result.error = shape.error.empty() ? "scene text rasterizer requires a valid shape" : shape.error;
    return result;
  }
  if (options.atlasWidth <= 0 || options.padding < 0) {
    result.error = "scene text rasterizer atlas dimensions were invalid";
    return result;
  }

  result.width = shape.width;
  result.height = shape.height;
  result.glyphs.reserve(shape.glyphs.size());
  int maxGlyphWidth = 1;
  for (std::size_t index = 0; index < shape.glyphs.size(); ++index) {
    result.glyphs.push_back(detail::rasterizeSceneTextGlyph(index, shape.glyphs[index], shape.height));
    maxGlyphWidth = std::max(maxGlyphWidth, result.glyphs.back().width);
  }

  result.atlas.width = std::max(options.atlasWidth, maxGlyphWidth + options.padding * 2);
  int cursorX = options.padding;
  int cursorY = options.padding;
  int rowHeight = 0;
  for (const SceneTextGlyphBitmap &glyph : result.glyphs) {
    if (cursorX > options.padding &&
        cursorX + glyph.width + options.padding > result.atlas.width) {
      cursorX = options.padding;
      cursorY += rowHeight + options.padding;
      rowHeight = 0;
    }
    result.atlas.placements.push_back(
        SceneTextAtlasPlacement{glyph.glyphIndex, cursorX, cursorY, glyph.width, glyph.height});
    cursorX += glyph.width + options.padding;
    rowHeight = std::max(rowHeight, glyph.height);
  }
  result.atlas.height = cursorY + rowHeight + options.padding;
  result.atlas.coverage.assign(static_cast<std::size_t>(result.atlas.width) *
                                   static_cast<std::size_t>(std::max(result.atlas.height, 0)),
                               0);
  for (const SceneTextAtlasPlacement &placement : result.atlas.placements) {
    const SceneTextGlyphBitmap &glyph = result.glyphs[placement.glyphIndex];
    for (int y = 0; y < placement.height; ++y) {
      for (int x = 0; x < placement.width; ++x) {
        const std::size_t atlasIndex =
            static_cast<std::size_t>(placement.y + y) *
                static_cast<std::size_t>(result.atlas.width) +
            static_cast<std::size_t>(placement.x + x);
        const std::size_t glyphIndex =
            static_cast<std::size_t>(y) * static_cast<std::size_t>(glyph.width) +
            static_cast<std::size_t>(x);
        result.atlas.coverage[atlasIndex] = glyph.coverage[glyphIndex];
      }
    }
  }
  result.ok = true;
  return result;
}

inline bool composeSceneTextBgra8(software_surface::SoftwareSurfaceFrame &frame,
                                  const SceneTextRasterResult &raster,
                                  double originX,
                                  double originY,
                                  Bgra8Color color,
                                  std::string &errorOut) {
  if (!raster.ok) {
    errorOut = raster.error.empty() ? "scene text composer requires a valid raster" : raster.error;
    return false;
  }
  if (frame.width <= 0 || frame.height <= 0 ||
      frame.pixelsBgra8.size() !=
          static_cast<std::size_t>(frame.width) * static_cast<std::size_t>(frame.height) * 4u) {
    errorOut = "scene text composer expected a valid BGRA8 frame";
    return false;
  }

  for (const SceneTextGlyphBitmap &glyph : raster.glyphs) {
    const int drawX = static_cast<int>(std::lround(originX + glyph.drawX));
    const int drawY = static_cast<int>(std::lround(originY + glyph.drawY));
    for (int glyphY = 0; glyphY < glyph.height; ++glyphY) {
      const int frameY = drawY + glyphY;
      if (frameY < 0 || frameY >= frame.height) {
        continue;
      }
      for (int glyphX = 0; glyphX < glyph.width; ++glyphX) {
        const int frameX = drawX + glyphX;
        if (frameX < 0 || frameX >= frame.width) {
          continue;
        }
        const std::uint8_t coverage =
            glyph.coverage[static_cast<std::size_t>(glyphY) *
                               static_cast<std::size_t>(glyph.width) +
                           static_cast<std::size_t>(glyphX)];
        if (coverage == 0) {
          continue;
        }
        const std::size_t pixelIndex =
            static_cast<std::size_t>(frameY) * static_cast<std::size_t>(frame.width) +
            static_cast<std::size_t>(frameX);
        detail::NormalizedPixel pixel = detail::readPixel(frame, pixelIndex);
        detail::blendSourceOver(pixel,
                                static_cast<double>(color.red) / 255.0,
                                static_cast<double>(color.green) / 255.0,
                                static_cast<double>(color.blue) / 255.0,
                                (static_cast<double>(color.alpha) / 255.0) *
                                    (static_cast<double>(coverage) / 255.0));
        detail::writePixel(pixel, frame, pixelIndex);
      }
    }
  }
  return true;
}

inline SceneRenderResult renderSerializedSceneToBgra8(
    const std::vector<std::int32_t> &serializedScene,
    const RenderOptions &options = {}) {
  detail::SceneRecord scene;
  SceneRenderResult result;
  if (!detail::readSceneRecord(serializedScene, scene, result.error)) {
    return result;
  }

  const detail::SceneCamera *camera = detail::findById(scene.cameras, scene.activeCameraId);
  if (camera == nullptr) {
    result.error = "scene renderer active camera id was not present";
    return result;
  }
  if (camera->projectionMode != detail::OrthographicProjectionMode) {
    result.error = "scene renderer only supports orthographic cameras";
    return result;
  }
  if (camera->unitsPerPixel <= 0.0 || !std::isfinite(camera->unitsPerPixel)) {
    result.error = "scene renderer camera unitsPerPixel must be positive";
    return result;
  }

  const int width = static_cast<int>(std::lround(camera->viewportWidth));
  const int height = static_cast<int>(std::lround(camera->viewportHeight));
  if (!detail::isRenderableExtent(width, height)) {
    result.error = "scene renderer camera viewport dimensions were invalid";
    return result;
  }

  std::vector<detail::NormalizedPixel> pixels(static_cast<std::size_t>(width) *
                                                  static_cast<std::size_t>(height),
                                              detail::normalize(options.clearColor));

  struct DrawCommand {
    const detail::SceneNode *node = nullptr;
    const detail::ScenePrimitive *primitive = nullptr;
    const detail::SceneMaterial *material = nullptr;
  };
  std::vector<DrawCommand> commands;
  commands.reserve(scene.nodes.size());
  for (const detail::SceneNode &node : scene.nodes) {
    const detail::ScenePrimitive *primitive = detail::findById(scene.primitives, node.primitiveId);
    if (primitive == nullptr ||
        (primitive->kind != detail::RectPrimitiveKind &&
         primitive->kind != detail::SdfButtonPrimitiveKind) ||
        primitive->width <= 0.0 || primitive->height <= 0.0) {
      continue;
    }
    const int materialId = node.materialId >= 0 ? node.materialId : primitive->materialId;
    const detail::SceneMaterial *material = detail::findById(scene.materials, materialId);
    if (material == nullptr || material->opacity <= 0.0) {
      continue;
    }
    if (node.translationZ < camera->nearZ || node.translationZ > camera->farZ) {
      continue;
    }
    commands.push_back(DrawCommand{&node, primitive, material});
  }

  std::stable_sort(commands.begin(), commands.end(), [](const DrawCommand &left, const DrawCommand &right) {
    if (left.node->painterOrder != right.node->painterOrder) {
      return left.node->painterOrder < right.node->painterOrder;
    }
    if (left.node->localZ != right.node->localZ) {
      return left.node->localZ < right.node->localZ;
    }
    return left.node->id < right.node->id;
  });

  for (const DrawCommand &command : commands) {
    for (int y = 0; y < height; ++y) {
      const double sceneY = camera->originY + (static_cast<double>(y) + 0.5) * camera->unitsPerPixel;
      const double localY = sceneY - command.node->translationY;
      for (int x = 0; x < width; ++x) {
        const double sceneX = camera->originX + (static_cast<double>(x) + 0.5) * camera->unitsPerPixel;
        const double localX = sceneX - command.node->translationX;
        const double coverage = detail::roundedRectCoverage(localX,
                                                            localY,
                                                            command.primitive->width,
                                                            command.primitive->height,
                                                            command.primitive->radius,
                                                            camera->unitsPerPixel);
        if (coverage <= 0.0) {
          continue;
        }
        double red = command.material->red;
        double green = command.material->green;
        double blue = command.material->blue;
        if (command.primitive->kind == detail::SdfButtonPrimitiveKind) {
          const double shade = detail::sdfButtonShade(localX,
                                                      localY,
                                                      command.primitive->width,
                                                      command.primitive->height,
                                                      command.primitive->radius,
                                                      command.node->scaleZ,
                                                      command.material->shadeStrength);
          red *= shade;
          green *= shade;
          blue *= shade;
        }
        const std::size_t pixelIndex =
            static_cast<std::size_t>(y) * static_cast<std::size_t>(width) + static_cast<std::size_t>(x);
        detail::blendSourceOver(pixels[pixelIndex],
                                red,
                                green,
                                blue,
                                command.material->opacity * coverage);
      }
    }
  }

  result.frame.width = width;
  result.frame.height = height;
  result.frame.pixelsBgra8.resize(static_cast<std::size_t>(width) *
                                  static_cast<std::size_t>(height) * 4u);
  for (std::size_t index = 0; index < pixels.size(); ++index) {
    detail::writePixel(pixels[index], result.frame, index);
  }

  std::string validationError;
  if (!software_surface::validateFrame(result.frame, validationError)) {
    result.error = validationError;
    return result;
  }
  result.ok = true;
  return result;
}

inline SceneRenderResult renderSerializedSceneWithTextOverlayToBgra8(
    const std::vector<std::int32_t> &serializedScene,
    const std::vector<SceneTextOverlay> &overlays,
    const RenderOptions &options = {}) {
  SceneRenderResult result = renderSerializedSceneToBgra8(serializedScene, options);
  if (!result.ok) {
    return result;
  }
  for (const SceneTextOverlay &overlay : overlays) {
    SceneTextShapeResult shape = shapeSceneTextUtf8(overlay.utf8, overlay.shapeOptions);
    if (!shape.ok) {
      result.ok = false;
      result.error = shape.error;
      return result;
    }
    SceneTextRasterResult raster = rasterizeSceneTextGlyphs(shape, overlay.rasterOptions);
    if (!raster.ok) {
      result.ok = false;
      result.error = raster.error;
      return result;
    }
    if (!composeSceneTextBgra8(result.frame, raster, overlay.x, overlay.y, overlay.color, result.error)) {
      result.ok = false;
      return result;
    }
  }
  std::string validationError;
  if (!software_surface::validateFrame(result.frame, validationError)) {
    result.ok = false;
    result.error = validationError;
    return result;
  }
  return result;
}

} // namespace primestruct::scene_bgra8_renderer
