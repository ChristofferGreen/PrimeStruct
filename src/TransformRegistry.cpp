#include "primec/TransformRegistry.h"

#include <array>

namespace {
constexpr std::array<std::string_view, 5> kTextTransforms = {
    "append_operators",
    "collections",
    "operators",
    "implicit-utf8",
    "implicit-i32",
};

constexpr std::array<std::string_view, 1> kSemanticTransforms = {
    "single_type_to_return",
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

bool isTextTransformName(std::string_view name) {
  return containsTransform(name, kTextTransforms);
}

bool isSemanticTransformName(std::string_view name) {
  return containsTransform(name, kSemanticTransforms);
}

} // namespace primec
