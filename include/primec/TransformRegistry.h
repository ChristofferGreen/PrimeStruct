#pragma once

#include "primec/Ast.h"

#include <span>
#include <string_view>

namespace primec {

struct TransformInfo {
  std::string_view name;
  TransformPhase phase;
  std::string_view aliases;
  bool availableInPrimec;
  bool availableInPrimevm;
};

std::string_view transformPhaseName(TransformPhase phase);
bool isTextTransformName(std::string_view name);
bool isSemanticTransformName(std::string_view name);
std::span<const TransformInfo> listTransforms();

} // namespace primec
