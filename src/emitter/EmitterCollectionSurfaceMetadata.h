#pragma once

#include "primec/support/StdlibSurfaceRegistry.h"
#include "primec/support/CollectionHelperNames.h"

#include <string_view>

namespace primec::emitter {

enum class EmitterCollectionSurface {
  VectorHelpers,
  VectorConstructors,
  KeyValueHelpers,
  KeyValueConstructors,
};

inline std::string_view emitterCollectionSurfaceCanonicalPath(
    EmitterCollectionSurface surface) {
  switch (surface) {
  case EmitterCollectionSurface::VectorHelpers:
    return collection_helpers::kCanonicalVector;
  case EmitterCollectionSurface::VectorConstructors:
    return collection_helpers::kCanonicalVectorVector;
  case EmitterCollectionSurface::KeyValueHelpers:
    return collection_helpers::kCanonicalMap;
  case EmitterCollectionSurface::KeyValueConstructors:
    return collection_helpers::kCanonicalMapMap;
  }
  return {};
}

inline StdlibSurfaceShape emitterCollectionSurfaceShape(
    EmitterCollectionSurface surface) {
  switch (surface) {
  case EmitterCollectionSurface::VectorHelpers:
  case EmitterCollectionSurface::KeyValueHelpers:
    return StdlibSurfaceShape::HelperFamily;
  case EmitterCollectionSurface::VectorConstructors:
  case EmitterCollectionSurface::KeyValueConstructors:
    return StdlibSurfaceShape::ConstructorFamily;
  }
  return StdlibSurfaceShape::HelperFamily;
}

inline const StdlibSurfaceMetadata *emitterCollectionSurfaceMetadata(
    EmitterCollectionSurface surface) {
  const StdlibSurfaceMetadata *metadata = findStdlibSurfaceMetadataByCanonicalPath(
      emitterCollectionSurfaceCanonicalPath(surface));
  if (metadata == nullptr ||
      metadata->domain != StdlibSurfaceDomain::Collections ||
      metadata->shape != emitterCollectionSurfaceShape(surface)) {
    return nullptr;
  }
  return metadata;
}

inline bool isEmitterCollectionSurfaceMetadata(
    const StdlibSurfaceMetadata &metadata,
    EmitterCollectionSurface surface) {
  const StdlibSurfaceMetadata *expected = emitterCollectionSurfaceMetadata(surface);
  return expected != nullptr && metadata.id == expected->id;
}

} // namespace primec::emitter
