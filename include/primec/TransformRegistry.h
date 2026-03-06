#pragma once

#include "primec/Ast.h"

#include <span>
#include <string_view>
#include <vector>

namespace primec {

struct TransformInfo {
  std::string_view name;
  TransformPhase phase;
  std::string_view aliases;
  bool availableInPrimec;
  bool availableInPrimevm;
};

class TransformRegistry {
public:
  void registerTransform(const TransformInfo &info);
  bool contains(std::string_view name, TransformPhase phase) const;
  std::span<const TransformInfo> list() const;

private:
  std::vector<TransformInfo> transforms;
};

const TransformRegistry &defaultTransformRegistry();

std::string_view transformPhaseName(TransformPhase phase);
bool isTextTransformName(std::string_view name);
bool isSemanticTransformName(std::string_view name);
std::span<const TransformInfo> listTransforms();

} // namespace primec
