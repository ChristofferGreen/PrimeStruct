#pragma once

std::string experimentalMapConstructorHelperPath(size_t argumentCount) {
  switch (argumentCount) {
  case 0:
    return "/std/collections/experimental_map/mapNew";
  case 2:
    return "/std/collections/experimental_map/mapSingle";
  case 4:
    return "/std/collections/experimental_map/mapPair";
  case 6:
    return "/std/collections/experimental_map/mapTriple";
  case 8:
    return "/std/collections/experimental_map/mapQuad";
  case 10:
    return "/std/collections/experimental_map/mapQuint";
  case 12:
    return "/std/collections/experimental_map/mapSext";
  case 14:
    return "/std/collections/experimental_map/mapSept";
  case 16:
    return "/std/collections/experimental_map/mapOct";
  default:
    return {};
  }
}

std::string experimentalMapConstructorRewritePath(const std::string &resolvedPath, size_t argumentCount) {
  const std::string normalizedPath = stripMapConstructorSuffixes(resolvedPath);
  if (normalizedPath == "/map") {
    return experimentalMapConstructorHelperPath(argumentCount);
  }
  if (normalizedPath == "/std/collections/map/map") {
    return experimentalMapConstructorHelperPath(argumentCount);
  }
  if (normalizedPath == "/std/collections/mapNew") {
    return "/std/collections/experimental_map/mapNew";
  }
  if (normalizedPath == "/std/collections/mapSingle") {
    return "/std/collections/experimental_map/mapSingle";
  }
  if (normalizedPath == "/std/collections/mapDouble") {
    return "/std/collections/experimental_map/mapDouble";
  }
  if (normalizedPath == "/std/collections/mapPair") {
    return "/std/collections/experimental_map/mapPair";
  }
  if (normalizedPath == "/std/collections/mapTriple") {
    return "/std/collections/experimental_map/mapTriple";
  }
  if (normalizedPath == "/std/collections/mapQuad") {
    return "/std/collections/experimental_map/mapQuad";
  }
  if (normalizedPath == "/std/collections/mapQuint") {
    return "/std/collections/experimental_map/mapQuint";
  }
  if (normalizedPath == "/std/collections/mapSext") {
    return "/std/collections/experimental_map/mapSext";
  }
  if (normalizedPath == "/std/collections/mapSept") {
    return "/std/collections/experimental_map/mapSept";
  }
  if (normalizedPath == "/std/collections/mapOct") {
    return "/std/collections/experimental_map/mapOct";
  }
  return {};
}

std::string experimentalVectorConstructorRewritePath(const std::string &resolvedPath, size_t argumentCount) {
  (void)argumentCount;
  if (resolvedPath == "/std/collections/vectorNew") {
    return "/std/collections/experimental_vector/vectorNew";
  }
  if (resolvedPath == "/std/collections/vectorSingle") {
    return "/std/collections/experimental_vector/vectorSingle";
  }
  if (resolvedPath == "/std/collections/vectorPair") {
    return "/std/collections/experimental_vector/vectorPair";
  }
  if (resolvedPath == "/std/collections/vectorTriple") {
    return "/std/collections/experimental_vector/vectorTriple";
  }
  if (resolvedPath == "/std/collections/vectorQuad") {
    return "/std/collections/experimental_vector/vectorQuad";
  }
  if (resolvedPath == "/std/collections/vectorQuint") {
    return "/std/collections/experimental_vector/vectorQuint";
  }
  if (resolvedPath == "/std/collections/vectorSext") {
    return "/std/collections/experimental_vector/vectorSext";
  }
  if (resolvedPath == "/std/collections/vectorSept") {
    return "/std/collections/experimental_vector/vectorSept";
  }
  if (resolvedPath == "/std/collections/vectorOct") {
    return "/std/collections/experimental_vector/vectorOct";
  }
  return {};
}
