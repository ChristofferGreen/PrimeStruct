#pragma once

// Helpers shared by the SemanticsValidatorBuildInitializerInference*.cpp units (split out of
// SemanticsValidatorBuildInitializerInference.cpp without changes, TODO-5384).
#include "SemanticsValidator.h"
#include "StdlibCollectionSurfaceHelpers.h"
#include "SemanticsValidatorInferCollectionCompatibilityInternal.h"
#include "primec/support/CollectionSpellingClassifier.h"
#include "primec/support/CollectionHelperNames.h"
#include <algorithm>
#include <limits>

namespace primec::semantics {

namespace buildInitializerHelpers {
inline bool isSpecializedExperimentalKeyValueBackingPath(std::string typeName) {
  typeName = normalizeBindingTypeName(typeName);
  if (!typeName.empty() && typeName.front() == '/') {
    typeName.erase(typeName.begin());
  }
  return isQualifiedExperimentalKeyValueBackingTypeName(typeName) &&
         typeName.find("__") != std::string::npos;
}
} // namespace buildInitializerHelpers
}  // namespace primec::semantics
