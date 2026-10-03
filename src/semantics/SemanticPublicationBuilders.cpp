#include "SemanticPublicationBuilders.h"

#include "RequirementPredicateFacts.h"
#include "StdlibCollectionSurfaceHelpers.h"
#include "primec/ir/StdlibCollectionPaths.h"
#include "primec/support/StdlibSurfaceRegistry.h"
#include "primec/support/CollectionHelperNames.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <optional>
#include <sstream>
#include <string_view>
#include <unordered_map>
#include <utility>
#include "SemanticPublicationBuildersInternal.h"

namespace primec {
namespace semantics {
using namespace publicationBuilders;


SemanticProgram buildSemanticProgramFromPublicationSurface(
    const Program &program,
    const std::string &entryPath,
    SemanticPublicationSurface publicationSurface,
    const SemanticProductBuildConfig *buildConfig) {
  SemanticPublicationBuilderState state(program, entryPath, buildConfig);
  initializeSemanticProgramPublicationShell(state);
  preseedSemanticProgramCallTargetStrings(state, publicationSurface);
  publishRoutingLookupIndexes(state);
  publishLowererPreflightFacts(state);
  publishRequirementPredicateFacts(state, publicationSurface);
  publishSemanticRoutingFamilies(state, publicationSurface);
  publishSemanticMetadataFamilies(state, publicationSurface);
  publishSemanticScopedFactFamilies(state, publicationSurface);
  finalizeSemanticModuleArtifacts(state);
  freezeSemanticProgramPublishedStorage(state.semanticProgram);
  return std::move(state.semanticProgram);
}

} // namespace semantics
} // namespace primec
