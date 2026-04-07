#pragma once

#include <array>
#include <string>
#include <string_view>

namespace primec::semantics {
namespace {

struct ExperimentalMapHelperDescriptor {
  std::string_view helperName;
  std::string_view canonicalPath;
  std::string_view aliasPath;
  std::string_view wrapperPath;
  std::string_view experimentalPath;
  bool supportsMethodCompatibility;
  bool requiresValidatedMapReceiverForDirectCompatibility;
  std::array<std::string_view, 6> wrapperDefinitionNeedles;
  size_t wrapperDefinitionNeedlesCount;
};

constexpr ExperimentalMapHelperDescriptor kExperimentalMapHelperDescriptors[] = {
    {"count", "/std/collections/map/count", "/map/count", "/std/collections/mapCount",
     "/std/collections/experimental_map/mapCount", true, true,
     {"/mapCount", "/Reference/count", "/count_ref", "", "", ""}, 3},
    {"contains", "/std/collections/map/contains", "/map/contains", "/std/collections/mapContains",
     "/std/collections/experimental_map/mapContains", true, true,
     {"/mapContains", "/Reference/contains", "/contains_ref", "/mapTryAt", "", ""}, 4},
    {"tryAt", "/std/collections/map/tryAt", "/map/tryAt", "/std/collections/mapTryAt",
     "/std/collections/experimental_map/mapTryAt", true, true,
     {"/mapTryAt", "/Reference/tryAt", "/tryAt_ref", "", "", ""}, 3},
    {"at", "/std/collections/map/at", "/map/at", "/std/collections/mapAt",
     "/std/collections/experimental_map/mapAt", true, false,
     {"/mapAt", "/mapAtUnsafe", "/mapTryAt", "/mapAtRef", "/Reference/at", "/at_ref"}, 6},
    {"at_unsafe", "/std/collections/map/at_unsafe", "/map/at_unsafe", "/std/collections/mapAtUnsafe",
     "/std/collections/experimental_map/mapAtUnsafe", true, false,
    {"/mapAt", "/mapAtUnsafe", "/mapTryAt", "/mapAtRef", "/Reference/at_unsafe", "/at_unsafe_ref"}, 6},
    {"insert", "/std/collections/map/insert", "/map/insert", "/std/collections/mapInsert",
     "/std/collections/experimental_map/mapInsert", true, true,
     {"/mapInsert", "/Reference/insert", "/insert_ref", "", "", ""}, 3},
};

struct BorrowedExperimentalMapHelperDescriptor {
  std::string_view helperName;
  std::string_view baseHelperName;
  std::string_view canonicalPath;
  std::string_view aliasPath;
  std::string_view wrapperPath;
  std::string_view experimentalPath;
};

constexpr BorrowedExperimentalMapHelperDescriptor kBorrowedExperimentalMapHelperDescriptors[] = {
    {"count_ref", "count", "/std/collections/map/count_ref", "/map/count_ref",
     "/std/collections/mapCountRef", "/std/collections/experimental_map/mapCountRef"},
    {"contains_ref", "contains", "/std/collections/map/contains_ref", "/map/contains_ref",
     "/std/collections/mapContainsRef", "/std/collections/experimental_map/mapContainsRef"},
    {"tryAt_ref", "tryAt", "/std/collections/map/tryAt_ref", "/map/tryAt_ref",
     "/std/collections/mapTryAtRef", "/std/collections/experimental_map/mapTryAtRef"},
    {"at_ref", "at", "/std/collections/map/at_ref", "/map/at_ref",
     "/std/collections/mapAtRef", "/std/collections/experimental_map/mapAtRef"},
    {"at_unsafe_ref", "at_unsafe", "/std/collections/map/at_unsafe_ref", "/map/at_unsafe_ref",
     "/std/collections/mapAtUnsafeRef", "/std/collections/experimental_map/mapAtUnsafeRef"},
    {"insert_ref", "insert", "/std/collections/map/insert_ref", "/map/insert_ref",
     "/std/collections/mapInsertRef", "/std/collections/experimental_map/mapInsertRef"},
};

struct ExperimentalVectorHelperDescriptor {
  std::string_view helperName;
  std::string_view canonicalPath;
  std::string_view aliasPath;
  std::string_view wrapperPath;
  std::string_view experimentalPath;
};

constexpr ExperimentalVectorHelperDescriptor kExperimentalVectorHelperDescriptors[] = {
    {"count", "/std/collections/vector/count", "/vector/count", "/std/collections/vectorCount",
     "/std/collections/experimental_vector/vectorCount"},
    {"capacity", "/std/collections/vector/capacity", "/vector/capacity", "/std/collections/vectorCapacity",
     "/std/collections/experimental_vector/vectorCapacity"},
    {"push", "/std/collections/vector/push", "/vector/push", "/std/collections/vectorPush",
     "/std/collections/experimental_vector/vectorPush"},
    {"pop", "/std/collections/vector/pop", "/vector/pop", "/std/collections/vectorPop",
     "/std/collections/experimental_vector/vectorPop"},
    {"reserve", "/std/collections/vector/reserve", "/vector/reserve", "/std/collections/vectorReserve",
     "/std/collections/experimental_vector/vectorReserve"},
    {"clear", "/std/collections/vector/clear", "/vector/clear", "/std/collections/vectorClear",
     "/std/collections/experimental_vector/vectorClear"},
    {"remove_at", "/std/collections/vector/remove_at", "/vector/remove_at", "/std/collections/vectorRemoveAt",
     "/std/collections/experimental_vector/vectorRemoveAt"},
    {"remove_swap", "/std/collections/vector/remove_swap", "/vector/remove_swap",
     "/std/collections/vectorRemoveSwap", "/std/collections/experimental_vector/vectorRemoveSwap"},
    {"at", "/std/collections/vector/at", "/vector/at", "/std/collections/vectorAt",
     "/std/collections/experimental_vector/vectorAt"},
    {"at_unsafe", "/std/collections/vector/at_unsafe", "/vector/at_unsafe", "/std/collections/vectorAtUnsafe",
     "/std/collections/experimental_vector/vectorAtUnsafe"},
};

enum class RemovedCollectionHelperFamily {
  VectorLike,
  Map,
};

struct RemovedCollectionHelperDescriptor {
  RemovedCollectionHelperFamily family;
  std::string_view helperName;
  std::string_view removedPath;
  bool statementOnly;
  bool supportsUnnamespacedFallback;
};

constexpr RemovedCollectionHelperDescriptor kRemovedCollectionHelperDescriptors[] = {
    {RemovedCollectionHelperFamily::VectorLike, "count", "/vector/count", false, false},
    {RemovedCollectionHelperFamily::VectorLike, "capacity", "/vector/capacity", false, false},
    {RemovedCollectionHelperFamily::VectorLike, "at", "/vector/at", false, false},
    {RemovedCollectionHelperFamily::VectorLike, "at_unsafe", "/vector/at_unsafe", false, false},
    {RemovedCollectionHelperFamily::VectorLike, "push", "/vector/push", true, false},
    {RemovedCollectionHelperFamily::VectorLike, "pop", "/vector/pop", true, false},
    {RemovedCollectionHelperFamily::VectorLike, "reserve", "/vector/reserve", true, false},
    {RemovedCollectionHelperFamily::VectorLike, "clear", "/vector/clear", true, false},
    {RemovedCollectionHelperFamily::VectorLike, "remove_at", "/vector/remove_at", true, false},
    {RemovedCollectionHelperFamily::VectorLike, "remove_swap", "/vector/remove_swap", true, false},
    {RemovedCollectionHelperFamily::Map, "count", "/map/count", false, true},
    {RemovedCollectionHelperFamily::Map, "contains", "/map/contains", false, false},
    {RemovedCollectionHelperFamily::Map, "tryAt", "/map/tryAt", false, false},
    {RemovedCollectionHelperFamily::Map, "at", "/map/at", false, false},
    {RemovedCollectionHelperFamily::Map, "at_unsafe", "/map/at_unsafe", false, false},
    {RemovedCollectionHelperFamily::Map, "insert", "/map/insert", false, false},
};

bool resolveRemovedCollectionHelperReference(std::string_view rawMethodName,
                                             std::string_view namespacePrefix,
                                             RemovedCollectionHelperFamily &familyOut,
                                             std::string_view &helperNameOut,
                                             bool &preserveArrayPathOut);

[[maybe_unused]] std::string bindingTypeText(const BindingInfo &binding) {
  if (binding.typeTemplateArg.empty()) {
    return binding.typeName;
  }
  return binding.typeName + "<" + binding.typeTemplateArg + ">";
}

[[maybe_unused]] bool isDirectMapConstructorPath(std::string_view resolvedCandidate) {
  auto matchesDirectMapConstructorPath = [&](std::string_view basePath) {
    return resolvedCandidate == basePath || resolvedCandidate.rfind(std::string(basePath) + "__t", 0) == 0;
  };
  return matchesDirectMapConstructorPath("/map") ||
         matchesDirectMapConstructorPath("/std/collections/map/map") ||
         matchesDirectMapConstructorPath("/mapNew") ||
         matchesDirectMapConstructorPath("/std/collections/mapNew") ||
         matchesDirectMapConstructorPath("/mapSingle") ||
         matchesDirectMapConstructorPath("/std/collections/mapSingle") ||
         matchesDirectMapConstructorPath("/mapDouble") ||
         matchesDirectMapConstructorPath("/std/collections/mapDouble") ||
         matchesDirectMapConstructorPath("/mapPair") ||
         matchesDirectMapConstructorPath("/std/collections/mapPair") ||
         matchesDirectMapConstructorPath("/mapTriple") ||
         matchesDirectMapConstructorPath("/std/collections/mapTriple") ||
         matchesDirectMapConstructorPath("/mapQuad") ||
         matchesDirectMapConstructorPath("/std/collections/mapQuad") ||
         matchesDirectMapConstructorPath("/mapQuint") ||
         matchesDirectMapConstructorPath("/std/collections/mapQuint") ||
         matchesDirectMapConstructorPath("/mapSext") ||
         matchesDirectMapConstructorPath("/std/collections/mapSext") ||
         matchesDirectMapConstructorPath("/mapSept") ||
         matchesDirectMapConstructorPath("/std/collections/mapSept") ||
         matchesDirectMapConstructorPath("/mapOct") ||
         matchesDirectMapConstructorPath("/std/collections/mapOct") ||
         matchesDirectMapConstructorPath("/std/collections/experimental_map/mapNew") ||
         matchesDirectMapConstructorPath("/std/collections/experimental_map/mapSingle") ||
         matchesDirectMapConstructorPath("/std/collections/experimental_map/mapDouble") ||
         matchesDirectMapConstructorPath("/std/collections/experimental_map/mapPair") ||
         matchesDirectMapConstructorPath("/std/collections/experimental_map/mapTriple") ||
         matchesDirectMapConstructorPath("/std/collections/experimental_map/mapQuad") ||
         matchesDirectMapConstructorPath("/std/collections/experimental_map/mapQuint") ||
         matchesDirectMapConstructorPath("/std/collections/experimental_map/mapSext") ||
         matchesDirectMapConstructorPath("/std/collections/experimental_map/mapSept") ||
         matchesDirectMapConstructorPath("/std/collections/experimental_map/mapOct");
}

[[maybe_unused]] bool matchesResolvedPath(std::string_view resolvedPath, std::string_view basePath) {
  return resolvedPath == basePath || resolvedPath.rfind(std::string(basePath) + "__t", 0) == 0;
}

[[maybe_unused]] std::string_view trimLeadingSlash(std::string_view text) {
  if (!text.empty() && text.front() == '/') {
    text.remove_prefix(1);
  }
  return text;
}

[[maybe_unused]] const ExperimentalMapHelperDescriptor *findExperimentalMapHelperByName(std::string_view helperName) {
  for (const auto &descriptor : kExperimentalMapHelperDescriptors) {
    if (descriptor.helperName == helperName) {
      return &descriptor;
    }
  }
  for (const auto &descriptor : kBorrowedExperimentalMapHelperDescriptors) {
    if (descriptor.helperName == helperName) {
      return findExperimentalMapHelperByName(descriptor.baseHelperName);
    }
  }
  return nullptr;
}

[[maybe_unused]] const BorrowedExperimentalMapHelperDescriptor *findBorrowedExperimentalMapHelperByName(
    std::string_view helperName) {
  for (const auto &descriptor : kBorrowedExperimentalMapHelperDescriptors) {
    if (descriptor.helperName == helperName) {
      return &descriptor;
    }
  }
  return nullptr;
}

[[maybe_unused]] const ExperimentalVectorHelperDescriptor *findExperimentalVectorHelperByName(std::string_view helperName) {
  for (const auto &descriptor : kExperimentalVectorHelperDescriptors) {
    if (descriptor.helperName == helperName) {
      return &descriptor;
    }
  }
  return nullptr;
}

[[maybe_unused]] const ExperimentalMapHelperDescriptor *findExperimentalMapCompatibilityHelper(
    std::string_view normalizedName,
    std::string_view normalizedPrefix,
    std::string_view resolvedPath,
    bool allowResolvedAliasMatch,
    bool requireMethodCompatibility) {
  normalizedName = trimLeadingSlash(normalizedName);
  normalizedPrefix = trimLeadingSlash(normalizedPrefix);
  for (const auto &descriptor : kExperimentalMapHelperDescriptors) {
    if (requireMethodCompatibility && !descriptor.supportsMethodCompatibility) {
      continue;
    }
    const std::string_view aliasPath = trimLeadingSlash(descriptor.aliasPath);
    const std::string_view canonicalPath = trimLeadingSlash(descriptor.canonicalPath);
    if (normalizedName == aliasPath || normalizedName == canonicalPath ||
        ((normalizedPrefix == "map" || normalizedPrefix == "std/collections/map") &&
         normalizedName == descriptor.helperName) ||
        (allowResolvedAliasMatch && matchesResolvedPath(resolvedPath, descriptor.aliasPath))) {
      return &descriptor;
    }
  }
  return nullptr;
}

[[maybe_unused]] const RemovedCollectionHelperDescriptor *findRemovedCollectionHelper(
    RemovedCollectionHelperFamily family,
    std::string_view helperName) {
  for (const auto &descriptor : kRemovedCollectionHelperDescriptors) {
    if (descriptor.family == family && descriptor.helperName == helperName) {
      return &descriptor;
    }
  }
  return nullptr;
}

[[maybe_unused]] const RemovedCollectionHelperDescriptor *findRemovedCollectionHelperReference(
    RemovedCollectionHelperFamily family,
    std::string_view rawMethodName,
    std::string_view namespacePrefix = "",
    bool allowWrappedReceiverPath = false) {
  rawMethodName = trimLeadingSlash(rawMethodName);
  namespacePrefix = trimLeadingSlash(namespacePrefix);
  if (allowWrappedReceiverPath) {
    if (rawMethodName.rfind("Reference/", 0) == 0) {
      rawMethodName.remove_prefix(std::string_view("Reference/").size());
    } else if (rawMethodName.rfind("Pointer/", 0) == 0) {
      rawMethodName.remove_prefix(std::string_view("Pointer/").size());
    }
  }
  RemovedCollectionHelperFamily resolvedFamily = RemovedCollectionHelperFamily::VectorLike;
  std::string_view helperName;
  bool preserveArrayPath = false;
  if (resolveRemovedCollectionHelperReference(
          rawMethodName, namespacePrefix, resolvedFamily, helperName, preserveArrayPath) &&
      resolvedFamily == family) {
    return findRemovedCollectionHelper(family, helperName);
  }
  return findRemovedCollectionHelper(family, rawMethodName);
}

[[maybe_unused]] std::string removedCollectionMethodPath(RemovedCollectionHelperFamily family,
                                                         std::string_view helperName,
                                                         bool preserveArrayPath) {
  const RemovedCollectionHelperDescriptor *descriptor = findRemovedCollectionHelper(family, helperName);
  if (descriptor == nullptr) {
    return "";
  }
  if (family == RemovedCollectionHelperFamily::VectorLike && preserveArrayPath) {
    return "/array/" + std::string(helperName);
  }
  return std::string(descriptor->removedPath);
}

[[maybe_unused]] bool resolveRemovedCollectionHelperReference(std::string_view rawMethodName,
                                                              std::string_view namespacePrefix,
                                                              RemovedCollectionHelperFamily &familyOut,
                                                              std::string_view &helperNameOut,
                                                              bool &preserveArrayPathOut) {
  rawMethodName = trimLeadingSlash(rawMethodName);
  namespacePrefix = trimLeadingSlash(namespacePrefix);
  preserveArrayPathOut = false;

  auto setVectorLike = [&](std::string_view helperName, bool preserveArrayPath) {
    helperNameOut = helperName;
    familyOut = RemovedCollectionHelperFamily::VectorLike;
    preserveArrayPathOut = preserveArrayPath;
    return true;
  };
  auto setMap = [&](std::string_view helperName) {
    helperNameOut = helperName;
    familyOut = RemovedCollectionHelperFamily::Map;
    preserveArrayPathOut = false;
    return true;
  };

  if (namespacePrefix == "array") {
    return setVectorLike(rawMethodName, true);
  }
  if (namespacePrefix == "vector" || namespacePrefix == "std/collections/vector") {
    return setVectorLike(rawMethodName, false);
  }
  if (namespacePrefix == "map" || namespacePrefix == "std/collections/map") {
    return setMap(rawMethodName);
  }
  if (rawMethodName.rfind("array/", 0) == 0) {
    return setVectorLike(rawMethodName.substr(std::string_view("array/").size()), true);
  }
  if (rawMethodName.rfind("vector/", 0) == 0) {
    return setVectorLike(rawMethodName.substr(std::string_view("vector/").size()), false);
  }
  if (rawMethodName.rfind("std/collections/vector/", 0) == 0) {
    return setVectorLike(rawMethodName.substr(std::string_view("std/collections/vector/").size()), false);
  }
  if (rawMethodName.rfind("map/", 0) == 0) {
    return setMap(rawMethodName.substr(std::string_view("map/").size()));
  }
  if (rawMethodName.rfind("std/collections/map/", 0) == 0) {
    return setMap(rawMethodName.substr(std::string_view("std/collections/map/").size()));
  }
  return false;
}

} // namespace
} // namespace primec::semantics
