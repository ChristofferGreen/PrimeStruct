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
} // namespace buildInitializerHelpers
}  // namespace primec::semantics
