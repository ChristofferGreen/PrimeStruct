#include "primec/TransformRegistry.h"

#include <array>
#include <span>

namespace {
constexpr std::array<primec::TransformInfo, 8> kTransforms = {{
    {"append_operators", primec::TransformPhase::Text, "-", true, true},
    {"collections", primec::TransformPhase::Text, "-", true, true},
    {"implicit-i32", primec::TransformPhase::Text, "-", true, true},
    {"implicit-utf8", primec::TransformPhase::Text, "-", true, true},
    {"on_error", primec::TransformPhase::Semantic, "-", true, true},
    {"operators", primec::TransformPhase::Text, "-", true, true},
    {"single_type_to_return", primec::TransformPhase::Semantic, "-", true, true},
    {"unsafe", primec::TransformPhase::Semantic, "-", true, true},
}};

constexpr std::array<std::string_view, 5> kTextTransforms = {
    "append_operators", "collections", "operators", "implicit-utf8", "implicit-i32",
};

constexpr std::array<std::string_view, 3> kSemanticTransforms = {
    "single_type_to_return", "on_error", "unsafe",
};

bool containsTransform(std::string_view name, const auto &list) {
  for (const auto &entry : list) {
    if (entry == name) {
      return true;
    }
  }
  return false;
}
} // namespace

namespace primec {

std::string_view transformPhaseName(TransformPhase phase) {
  if (phase == TransformPhase::Text) {
    return "text";
  }
  return "semantic";
}

bool isTextTransformName(std::string_view name) {
  return containsTransform(name, kTextTransforms);
}

bool isSemanticTransformName(std::string_view name) {
  return containsTransform(name, kSemanticTransforms);
}

std::span<const TransformInfo> listTransforms() {
  return kTransforms;
}

} // namespace primec
