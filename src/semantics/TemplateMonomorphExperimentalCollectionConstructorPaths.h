#pragma once

#include "MapConstructorHelpers.h"

std::string experimentalMapConstructorRewritePath(const std::string &resolvedPath, size_t argumentCount) {
  return metadataBackedExperimentalMapConstructorRewritePath(resolvedPath, argumentCount);
}

std::string experimentalVectorConstructorRewritePath(const std::string &resolvedPath, size_t argumentCount) {
  (void)argumentCount;
  return metadataBackedExperimentalVectorConstructorCompatibilityPath(resolvedPath);
}
