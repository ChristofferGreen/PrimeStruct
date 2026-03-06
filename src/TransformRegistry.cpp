#include "primec/TransformRegistry.h"

#include <span>

namespace {
primec::TransformRegistry buildDefaultTransformRegistry() {
  primec::TransformRegistry registry;
  registry.registerTransform({"append_operators", primec::TransformPhase::Text, "-", true, true});
  registry.registerTransform({"collections", primec::TransformPhase::Text, "-", true, true});
  registry.registerTransform({"implicit-i32", primec::TransformPhase::Text, "-", true, true});
  registry.registerTransform({"implicit-utf8", primec::TransformPhase::Text, "-", true, true});
  registry.registerTransform({"on_error", primec::TransformPhase::Semantic, "-", true, true});
  registry.registerTransform({"operators", primec::TransformPhase::Text, "-", true, true});
  registry.registerTransform({"single_type_to_return", primec::TransformPhase::Semantic, "-", true, true});
  registry.registerTransform({"unsafe", primec::TransformPhase::Semantic, "-", true, true});
  return registry;
}

const primec::TransformInfo *findTransform(std::span<const primec::TransformInfo> transforms,
                                           std::string_view name,
                                           primec::TransformPhase phase) {
  for (const auto &transform : transforms) {
    if (transform.name != name) {
      continue;
    }
    if (phase == primec::TransformPhase::Auto || transform.phase == phase) {
      return &transform;
    }
  }
  return nullptr;
}
} // namespace

namespace primec {

void TransformRegistry::registerTransform(const TransformInfo &info) {
  for (auto &transform : transforms) {
    if (transform.name == info.name && transform.phase == info.phase) {
      transform = info;
      return;
    }
  }
  transforms.push_back(info);
}

bool TransformRegistry::contains(std::string_view name, TransformPhase phase) const {
  return findTransform(transforms, name, phase) != nullptr;
}

std::span<const TransformInfo> TransformRegistry::list() const {
  return transforms;
}

const TransformRegistry &defaultTransformRegistry() {
  static const TransformRegistry registry = buildDefaultTransformRegistry();
  return registry;
}

std::string_view transformPhaseName(TransformPhase phase) {
  if (phase == TransformPhase::Text) {
    return "text";
  }
  return "semantic";
}

bool isTextTransformName(std::string_view name) {
  return defaultTransformRegistry().contains(name, TransformPhase::Text);
}

bool isSemanticTransformName(std::string_view name) {
  return defaultTransformRegistry().contains(name, TransformPhase::Semantic);
}

std::span<const TransformInfo> listTransforms() {
  return defaultTransformRegistry().list();
}

} // namespace primec
