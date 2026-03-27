#pragma once

#include <cstddef>
#include <string>

namespace primec::semantics::detail {

inline bool isMatrixQuaternionTypePath(const std::string &typePath) {
  return typePath == "/std/math/Mat2" || typePath == "/std/math/Mat3" || typePath == "/std/math/Mat4" ||
         typePath == "/std/math/Quat";
}

inline size_t matrixDimensionForTypePath(const std::string &typePath) {
  if (typePath == "/std/math/Mat2") {
    return 2;
  }
  if (typePath == "/std/math/Mat3") {
    return 3;
  }
  if (typePath == "/std/math/Mat4") {
    return 4;
  }
  return 0;
}

inline bool isVectorTypePath(const std::string &typePath) {
  return typePath == "/std/math/Vec2" || typePath == "/std/math/Vec3" || typePath == "/std/math/Vec4";
}

inline bool isMatrixQuaternionConversionTypePath(const std::string &typePath) {
  return isMatrixQuaternionTypePath(typePath) || isVectorTypePath(typePath);
}

inline size_t vectorDimensionForTypePath(const std::string &typePath) {
  if (typePath == "/std/math/Vec2") {
    return 2;
  }
  if (typePath == "/std/math/Vec3") {
    return 3;
  }
  if (typePath == "/std/math/Vec4") {
    return 4;
  }
  return 0;
}

enum class NumericDomain { Unknown, Fixed, Software };
enum class NumericCategory { Unknown, Integer, Float, Complex };

struct NumericInfo {
  NumericDomain domain = NumericDomain::Unknown;
  NumericCategory category = NumericCategory::Unknown;
};

struct MatrixQuaternionMultiplyInfo {
  bool handled = false;
  bool valid = false;
};

struct MatrixQuaternionDivideInfo {
  bool handled = false;
  bool valid = false;
};

} // namespace primec::semantics::detail
