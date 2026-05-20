#pragma once

#include "StdlibCollectionSurfaceHelpers.h"

std::string experimentalVectorConstructorRewritePath(const std::string &resolvedPath, size_t argumentCount) {
  (void)argumentCount;
  return metadataBackedExperimentalVectorConstructorCompatibilityPath(resolvedPath);
}
