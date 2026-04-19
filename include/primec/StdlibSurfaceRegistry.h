#pragma once

#include <span>
#include <string_view>

namespace primec {

enum class StdlibSurfaceDomain {
  File,
  Collections,
  Gfx,
};

enum class StdlibSurfaceShape {
  HelperFamily,
  ErrorFamily,
  ConstructorFamily,
};

enum class StdlibSurfaceId {
  FileHelpers,
  FileErrorHelpers,
  CollectionsVectorHelpers,
  CollectionsMapHelpers,
  CollectionsMapConstructors,
  CollectionsContainerErrorHelpers,
  GfxBufferHelpers,
  GfxErrorHelpers,
};

struct StdlibSurfaceMetadata {
  StdlibSurfaceId id{};
  StdlibSurfaceDomain domain{};
  StdlibSurfaceShape shape{};
  std::string_view bridgeKey;
  std::string_view canonicalImportRoot;
  std::string_view canonicalPath;
  std::span<const std::string_view> memberNames;
  std::span<const std::string_view> importAliasSpellings;
  std::span<const std::string_view> compatibilitySpellings;
  std::span<const std::string_view> loweringSpellings;
};

std::span<const StdlibSurfaceMetadata> stdlibSurfaceRegistry();
const StdlibSurfaceMetadata *findStdlibSurfaceMetadata(StdlibSurfaceId id);
const StdlibSurfaceMetadata *findStdlibSurfaceMetadataByCanonicalPath(std::string_view canonicalPath);
const StdlibSurfaceMetadata *findStdlibSurfaceMetadataByBridgeKey(std::string_view bridgeKey);
const StdlibSurfaceMetadata *findStdlibSurfaceMetadataBySpelling(std::string_view spelling);
bool stdlibSurfaceMatchesSpelling(const StdlibSurfaceMetadata &metadata, std::string_view spelling);

} // namespace primec
