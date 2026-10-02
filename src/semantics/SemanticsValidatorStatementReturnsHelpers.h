#pragma once

// Helpers shared by the SemanticsValidatorStatementReturns*.cpp units (split out of
// SemanticsValidatorStatementReturns.cpp without changes, TODO-5384).
#include "SemanticsValidator.h"
#include "StdlibCollectionSurfaceHelpers.h"
#include "SemanticsValidatorInferCollectionCompatibilityInternal.h"
#include "primec/support/CollectionHelperNames.h"
#include <array>
#include <cctype>
#include <functional>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace primec::semantics {
namespace statementReturnsHelpers {

inline bool isMatrixQuaternionTypePathForReturnDiagnostic(const std::string &typePath) {
  return typePath == "/std/math/Mat2" || typePath == "/std/math/Mat3" || typePath == "/std/math/Mat4" ||
         typePath == "/std/math/Quat";
}

inline bool isVectorTypePathForReturnDiagnostic(const std::string &typePath) {
  return typePath == "/std/math/Vec2" || typePath == "/std/math/Vec3" || typePath == "/std/math/Vec4";
}

inline std::string collectionTypePathLocal(std::string_view collectionName,
                                    std::string_view typeName = {},
                                    bool leadingSlash = true) {
  std::string path = leadingSlash ? "/" : "";
  path += "std/collections/";
  path += std::string(collectionName);
  if (!typeName.empty()) {
    path += "/";
    path += std::string(typeName);
  }
  return path;
}

inline bool isSpecializedExperimentalKeyValueBackingPath(std::string typeName) {
  typeName = normalizeBindingTypeName(typeName);
  if (!typeName.empty() && typeName.front() == '/') {
    typeName.erase(typeName.begin());
  }
  return isExperimentalCollectionBackingTypeName("map", "Map", typeName) &&
         typeName.find("__") != std::string::npos;
}

inline std::string keyValueCollectionMarkerPathLocal() {
  const StdlibSurfaceMetadata *metadata = keyValueConstructorSurfaceMetadataLocal();
  if (metadata != nullptr) {
    for (std::string_view alias : metadata->importAliasSpellings) {
      if (alias.empty()) {
        continue;
      }
      std::string rootedAlias(alias);
      if (rootedAlias.front() != '/') {
        rootedAlias.insert(rootedAlias.begin(), '/');
      }
      if (rootedAlias.find('/', 1) == std::string::npos) {
        return rootedAlias;
      }
    }
  }
  return std::string(1, '/') + std::string("map");
}

inline bool isMatrixQuaternionConversionTypePathForReturn(const std::string &typePath) {
  return isMatrixQuaternionTypePathForReturnDiagnostic(typePath) || isVectorTypePathForReturnDiagnostic(typePath);
}

inline bool isImplicitMatrixQuaternionReturnConversion(const std::string &expectedTypePath,
                                                const std::string &actualTypePath) {
  return !expectedTypePath.empty() && !actualTypePath.empty() && expectedTypePath != actualTypePath &&
         isMatrixQuaternionConversionTypePathForReturn(expectedTypePath) &&
         isMatrixQuaternionConversionTypePathForReturn(actualTypePath) &&
         (isMatrixQuaternionTypePathForReturnDiagnostic(expectedTypePath) ||
          isMatrixQuaternionTypePathForReturnDiagnostic(actualTypePath));
}

inline std::string implicitMatrixQuaternionReturnDiagnostic(const std::string &expectedTypePath,
                                                     const std::string &actualTypePath) {
  return "implicit matrix/quaternion family conversion requires explicit helper: expected " + expectedTypePath +
         " got " + actualTypePath;
}

inline std::string returnTypeMismatchDiagnostic(const std::string &expectedTypePath,
                                         const std::string &actualTypePath,
                                         const std::string &fallbackExpectedType) {
  if (isImplicitMatrixQuaternionReturnConversion(expectedTypePath, actualTypePath)) {
    return "return type mismatch: " +
           implicitMatrixQuaternionReturnDiagnostic(expectedTypePath, actualTypePath);
  }
  return "return type mismatch: expected " + fallbackExpectedType;
}

inline bool isUnknownBorrowedKeyValueAccessMethodDiagnostic(const std::string &message) {
  const StdlibSurfaceMetadata *metadata = keyValueHelperSurfaceMetadataLocal();
  if (metadata == nullptr) {
    return false;
  }
  static constexpr std::array<std::string_view, 4> KeyValueAccessHelpers = {
      "at", collection_helpers::kAtRef, "at_unsafe", collection_helpers::kAtUnsafeRef};
  for (std::string_view alias : metadata->importAliasSpellings) {
    if (!alias.empty() && alias.front() == '/') {
      alias.remove_prefix(1);
    }
    if (alias.find('/') != std::string_view::npos) {
      continue;
    }
    for (std::string_view helperName : KeyValueAccessHelpers) {
      if (message == "unknown method: /" + std::string(alias) + "/" +
                         std::string(helperName)) {
        return true;
      }
    }
  }
  return false;
}

} // namespace statementReturnsHelpers
} // namespace primec::semantics
