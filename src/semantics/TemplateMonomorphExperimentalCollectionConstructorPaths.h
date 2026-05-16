#pragma once

#include "MapConstructorHelpers.h"

std::string experimentalVectorConstructorRewritePath(const std::string &resolvedPath, size_t argumentCount) {
  (void)argumentCount;
  return metadataBackedExperimentalVectorConstructorCompatibilityPath(resolvedPath);
}
