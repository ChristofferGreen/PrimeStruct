#pragma once

#include "primec/StdlibSurfaceRegistry.h"

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
    return "/std/collections/" "vec" "tor";
  case EmitterCollectionSurface::VectorConstructors:
    return "/std/collections/" "vec" "tor/" "vec" "tor";
  case EmitterCollectionSurface::KeyValueHelpers:
    return "/std/collections/" "ma" "p";
  case EmitterCollectionSurface::KeyValueConstructors:
    return "/std/collections/" "ma" "p/" "ma" "p";
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
