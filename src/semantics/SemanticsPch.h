#pragma once

// Precompiled header for the semantics translation units that include
// SemanticsValidator.h (see PRIMESTRUCT_SEMANTICS_PCH in CMakeLists.txt,
// TODO-5352). It only bundles headers those units already include; it is not
// meant to be included directly.
#include "SemanticsValidator.h"

#include "primec/ir/StdlibCollectionPaths.h"
#include "primec/support/CollectionHelperNames.h"
#include "primec/support/StdlibSurfaceRegistry.h"

#include <algorithm>
#include <cctype>
#include <sstream>
