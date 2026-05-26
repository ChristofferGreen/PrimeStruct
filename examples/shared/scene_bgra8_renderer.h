#pragma once

#include "software_surface_bridge.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <string>
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

namespace detail {

constexpr int SceneRecordVersion = 1;
constexpr int OrthographicProjectionMode = 1;
constexpr int RectPrimitiveKind = 1;
constexpr int MaxSoftwareSurfaceExtent = 16384;

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

struct SceneRecord {
  int activeCameraId = -1;
  std::vector<SceneNode> nodes;
  std::vector<ScenePrimitive> primitives;
  std::vector<SceneMaterial> materials;
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

inline std::uint8_t unitToUnorm(double value) {
  return static_cast<std::uint8_t>(std::lround(clampUnit(value) * 255.0));
}

inline NormalizedPixel normalize(Bgra8Color color) {
  return NormalizedPixel{static_cast<double>(color.blue) / 255.0,
                         static_cast<double>(color.green) / 255.0,
                         static_cast<double>(color.red) / 255.0,
                         static_cast<double>(color.alpha) / 255.0};
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
  for (int index = 0; index < lightCount; ++index) {
    int unusedInt = 0;
    double unusedMilli = 0.0;
    if (!reader.readInt(unusedInt) || !reader.readInt(unusedInt) ||
        !reader.readMilli(unusedMilli) || !reader.readMilli(unusedMilli) ||
        !reader.readMilli(unusedMilli) || !reader.readMilli(unusedMilli)) {
      errorOut = "scene renderer record ended inside light table";
      return false;
    }
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

} // namespace detail

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
    if (primitive == nullptr || primitive->kind != detail::RectPrimitiveKind ||
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
        const std::size_t pixelIndex =
            static_cast<std::size_t>(y) * static_cast<std::size_t>(width) + static_cast<std::size_t>(x);
        detail::blendSourceOver(pixels[pixelIndex],
                                command.material->red,
                                command.material->green,
                                command.material->blue,
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

} // namespace primestruct::scene_bgra8_renderer
