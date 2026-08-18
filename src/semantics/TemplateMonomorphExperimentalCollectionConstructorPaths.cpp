#include "TemplateMonomorphExperimentalCollectionConstructorPaths.h"

#include "StdlibCollectionSurfaceHelpers.h"

namespace primec {

std::string experimentalVectorConstructorRewritePath(const std::string &resolvedPath, size_t argumentCount) {
  (void)argumentCount;
  return metadataBackedExperimentalVectorConstructorCompatibilityPath(resolvedPath);
}

} // namespace primec
