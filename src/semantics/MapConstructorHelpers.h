#pragma once

#include <array>
#include <string>
#include <utility>

inline std::string stripMapConstructorSuffixes(std::string resolvedPath) {
  const size_t specializationSuffix = resolvedPath.find("__t");
  if (specializationSuffix != std::string::npos) {
    resolvedPath.erase(specializationSuffix);
  }
  const size_t overloadSuffix = resolvedPath.find("__ov");
  if (overloadSuffix != std::string::npos) {
    resolvedPath.erase(overloadSuffix);
  }
  return resolvedPath;
}

inline bool isResolvedMapConstructorPath(const std::string &rawPath) {
  const std::string resolvedPath = stripMapConstructorSuffixes(rawPath);
  static constexpr std::array<const char *, 32> validPaths = {{
      "/map",
      "/std/collections/map/map",
      "/mapNew",
      "/std/collections/mapNew",
      "/mapSingle",
      "/std/collections/mapSingle",
      "/mapDouble",
      "/std/collections/mapDouble",
      "/mapPair",
      "/std/collections/mapPair",
      "/mapTriple",
      "/std/collections/mapTriple",
      "/mapQuad",
      "/std/collections/mapQuad",
      "/mapQuint",
      "/std/collections/mapQuint",
      "/mapSext",
      "/std/collections/mapSext",
      "/mapSept",
      "/std/collections/mapSept",
      "/mapOct",
      "/std/collections/mapOct",
      "/std/collections/experimental_map/mapNew",
      "/std/collections/experimental_map/mapSingle",
      "/std/collections/experimental_map/mapDouble",
      "/std/collections/experimental_map/mapPair",
      "/std/collections/experimental_map/mapTriple",
      "/std/collections/experimental_map/mapQuad",
      "/std/collections/experimental_map/mapQuint",
      "/std/collections/experimental_map/mapSext",
      "/std/collections/experimental_map/mapSept",
      "/std/collections/experimental_map/mapOct",
  }};
  for (const auto *path : validPaths) {
    if (resolvedPath == path) {
      return true;
    }
  }
  return false;
}

inline std::string canonicalMapConstructorHelperPath(size_t argumentCount) {
  switch (argumentCount) {
  case 0:
    return "/std/collections/mapNew";
  case 2:
    return "/std/collections/mapSingle";
  case 4:
    return "/std/collections/mapPair";
  case 6:
    return "/std/collections/mapTriple";
  case 8:
    return "/std/collections/mapQuad";
  case 10:
    return "/std/collections/mapQuint";
  case 12:
    return "/std/collections/mapSext";
  case 14:
    return "/std/collections/mapSept";
  case 16:
    return "/std/collections/mapOct";
  default:
    return {};
  }
}

inline std::string experimentalMapConstructorHelperPath(size_t argumentCount) {
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

inline std::string canonicalMapConstructorToExperimental(const std::string &canonicalPath) {
  static const std::array<std::pair<const char *, const char *>, 9> mapping = {{
      {"/std/collections/mapNew", "/std/collections/experimental_map/mapNew"},
      {"/std/collections/mapSingle", "/std/collections/experimental_map/mapSingle"},
      {"/std/collections/mapPair", "/std/collections/experimental_map/mapPair"},
      {"/std/collections/mapTriple", "/std/collections/experimental_map/mapTriple"},
      {"/std/collections/mapQuad", "/std/collections/experimental_map/mapQuad"},
      {"/std/collections/mapQuint", "/std/collections/experimental_map/mapQuint"},
      {"/std/collections/mapSext", "/std/collections/experimental_map/mapSext"},
      {"/std/collections/mapSept", "/std/collections/experimental_map/mapSept"},
      {"/std/collections/mapOct", "/std/collections/experimental_map/mapOct"},
  }};
  for (const auto &pair : mapping) {
    if (canonicalPath == pair.first) {
      return pair.second;
    }
  }
  return {};
}
