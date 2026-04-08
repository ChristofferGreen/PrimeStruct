#include "TypeResolutionGraphPreparation.h"

#include "SemanticsValidateConvertConstructors.h"
#include "SemanticsValidateExperimentalGfxConstructors.h"
#include "SemanticsValidateReflectionGeneratedHelpers.h"
#include "SemanticsValidateReflectionMetadata.h"
#include "SemanticsValidateTransforms.h"

namespace primec::semantics {

bool monomorphizeTemplates(Program &program,
                           const std::string &entryPath,
                           std::string &error,
                           uint64_t *explicitTemplateArgFactHitCountOut,
                           uint64_t *implicitTemplateArgFactHitCountOut);

bool prepareProgramForTypeResolutionAnalysis(Program &program,
                                             const std::string &entryPath,
                                             const std::vector<std::string> &semanticTransforms,
                                             std::string &error,
                                             uint64_t *explicitTemplateArgFactHitCountOut,
                                             uint64_t *implicitTemplateArgFactHitCountOut) {
  if (explicitTemplateArgFactHitCountOut != nullptr) {
    *explicitTemplateArgFactHitCountOut = 0;
  }
  if (implicitTemplateArgFactHitCountOut != nullptr) {
    *implicitTemplateArgFactHitCountOut = 0;
  }
  if (!applySemanticTransforms(program, semanticTransforms, error)) {
    return false;
  }
  if (!rewriteExperimentalGfxConstructors(program, error)) {
    return false;
  }
  if (!rewriteReflectionGeneratedHelpers(program, error)) {
    return false;
  }
  if (!monomorphizeTemplates(program,
                             entryPath,
                             error,
                             explicitTemplateArgFactHitCountOut,
                             implicitTemplateArgFactHitCountOut)) {
    return false;
  }
  if (!rewriteReflectionMetadataQueries(program, error)) {
    return false;
  }
  if (!rewriteConvertConstructors(program, error)) {
    return false;
  }
  return true;
}

} // namespace primec::semantics
