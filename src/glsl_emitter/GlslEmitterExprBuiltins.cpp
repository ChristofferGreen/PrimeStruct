#include "GlslEmitterExprInternal.h"

namespace primec::glsl_emitter {

namespace {

std::string componentExpr(const std::string &valueCode, char component) {
  std::string out = "(" + valueCode + ").";
  out.push_back(component);
  return out;
}

std::string matrixElementExpr(const std::string &valueCode, int row, int column) {
  return "(" + valueCode + ")[" + std::to_string(column) + "][" + std::to_string(row) + "]";
}

std::string constructMatrixFromRowMajor(GlslType type, const std::vector<std::string> &rowMajor) {
  const int dimension = matrixDimension(type);
  std::string out = glslTypeName(type) + "(";
  bool first = true;
  for (int column = 0; column < dimension; ++column) {
    for (int row = 0; row < dimension; ++row) {
      if (!first) {
        out += ", ";
      }
      first = false;
      out += rowMajor[static_cast<size_t>(row * dimension + column)];
    }
  }
  out += ")";
  return out;
}

ExprResult castScalarToMatrixElementFloat(const ExprResult &expr) {
  return castExpr(expr, GlslType::Float);
}

ExprResult materializeExprResult(const ExprResult &expr, EmitState &state, const std::string &prefix) {
  ExprResult out;
  out.type = expr.type;
  out.code = allocTempName(state, prefix);
  out.prelude = expr.prelude;
  out.prelude += glslTypeName(expr.type) + " " + out.code + " = " + expr.code + ";\n";
  return out;
}

ExprResult emitNormalizedQuatValue(const ExprResult &quat, EmitState &state) {
  ExprResult quatTemp = materializeExprResult(quat, state, "_ps_quat_");
  const std::string lenSquaredName = allocTempName(state, "_ps_quat_len2_");
  ExprResult out;
  out.type = GlslType::Quat;
  out.code = allocTempName(state, "_ps_quat_norm_");
  out.prelude = quatTemp.prelude;
  out.prelude += "float " + lenSquaredName + " = dot(" + quatTemp.code + ", " + quatTemp.code + ");\n";
  out.prelude += "vec4 " + out.code + " = ((" + lenSquaredName + " <= 0.0) ? " + quatTemp.code + " : (" +
                 quatTemp.code + " / sqrt(" + lenSquaredName + ")));\n";
  return out;
}

std::vector<std::string> quatToMat3RowMajorElements(const std::string &quatCode) {
  const std::string x = componentExpr(quatCode, 'x');
  const std::string y = componentExpr(quatCode, 'y');
  const std::string z = componentExpr(quatCode, 'z');
  const std::string w = componentExpr(quatCode, 'w');
  const std::string xx = x + " * " + x;
  const std::string yy = y + " * " + y;
  const std::string zz = z + " * " + z;
  const std::string xy = x + " * " + y;
  const std::string xz = x + " * " + z;
  const std::string yz = y + " * " + z;
  const std::string wx = w + " * " + x;
  const std::string wy = w + " * " + y;
  const std::string wz = w + " * " + z;
  return {
      "(1.0 - 2.0 * ((" + yy + ") + (" + zz + ")))",
      "(2.0 * ((" + xy + ") - (" + wz + ")))",
      "(2.0 * ((" + xz + ") + (" + wy + ")))",
      "(2.0 * ((" + xy + ") + (" + wz + ")))",
      "(1.0 - 2.0 * ((" + xx + ") + (" + zz + ")))",
      "(2.0 * ((" + yz + ") - (" + wx + ")))",
      "(2.0 * ((" + xz + ") - (" + wy + ")))",
      "(2.0 * ((" + yz + ") + (" + wx + ")))",
      "(1.0 - 2.0 * ((" + xx + ") + (" + yy + ")))",
  };
}

ExprResult emitQuatToMat3Value(const ExprResult &value, EmitState &state) {
  ExprResult normalized = emitNormalizedQuatValue(value, state);
  ExprResult out;
  out.type = GlslType::Mat3;
  out.code = constructMatrixFromRowMajor(GlslType::Mat3, quatToMat3RowMajorElements(normalized.code));
  out.prelude = normalized.prelude;
  return out;
}

ExprResult emitQuatToMat4Value(const ExprResult &value, EmitState &state) {
  ExprResult normalized = emitNormalizedQuatValue(value, state);
  const std::vector<std::string> basis = quatToMat3RowMajorElements(normalized.code);
  std::vector<std::string> rowMajor = {
      basis[0], basis[1], basis[2], "0.0",
      basis[3], basis[4], basis[5], "0.0",
      basis[6], basis[7], basis[8], "0.0",
      "0.0",   "0.0",   "0.0",   "1.0",
  };
  ExprResult out;
  out.type = GlslType::Mat4;
  out.code = constructMatrixFromRowMajor(GlslType::Mat4, rowMajor);
  out.prelude = normalized.prelude;
  return out;
}

ExprResult emitMat3ToQuatValue(const ExprResult &value, EmitState &state) {
  ExprResult matrixTemp = materializeExprResult(value, state, "_ps_mat3_");
  const std::string rawQuatName = allocTempName(state, "_ps_mat3_to_quat_raw_");
  const std::string traceName = allocTempName(state, "_ps_mat3_trace_");
  const std::string normalizedQuatName = allocTempName(state, "_ps_mat3_to_quat_");
  const std::string lenSquaredName = allocTempName(state, "_ps_mat3_to_quat_len2_");

  const std::string m00 = matrixElementExpr(matrixTemp.code, 0, 0);
  const std::string m01 = matrixElementExpr(matrixTemp.code, 0, 1);
  const std::string m02 = matrixElementExpr(matrixTemp.code, 0, 2);
  const std::string m10 = matrixElementExpr(matrixTemp.code, 1, 0);
  const std::string m11 = matrixElementExpr(matrixTemp.code, 1, 1);
  const std::string m12 = matrixElementExpr(matrixTemp.code, 1, 2);
  const std::string m20 = matrixElementExpr(matrixTemp.code, 2, 0);
  const std::string m21 = matrixElementExpr(matrixTemp.code, 2, 1);
  const std::string m22 = matrixElementExpr(matrixTemp.code, 2, 2);

  ExprResult out;
  out.type = GlslType::Quat;
  out.code = normalizedQuatName;
  out.prelude = matrixTemp.prelude;
  out.prelude += "vec4 " + rawQuatName + ";\n";
  out.prelude += "float " + traceName + " = " + m00 + " + " + m11 + " + " + m22 + ";\n";
  out.prelude += "if (" + traceName + " > 0.0) {\n";
  out.prelude += "  float s = sqrt(" + traceName + " + 1.0) * 2.0;\n";
  out.prelude += "  " + rawQuatName + " = vec4((" + m21 + " - " + m12 + ") / s, (" + m02 + " - " + m20 +
                 ") / s, (" + m10 + " - " + m01 + ") / s, 0.25 * s);\n";
  out.prelude += "} else {\n";
  out.prelude += "  if (" + m00 + " > " + m11 + ") {\n";
  out.prelude += "    if (" + m00 + " > " + m22 + ") {\n";
  out.prelude += "      float s = sqrt(1.0 + " + m00 + " - " + m11 + " - " + m22 + ") * 2.0;\n";
  out.prelude += "      " + rawQuatName + " = vec4(0.25 * s, (" + m01 + " + " + m10 + ") / s, (" + m02 + " + " +
                 m20 + ") / s, (" + m21 + " - " + m12 + ") / s);\n";
  out.prelude += "    } else {\n";
  out.prelude += "      float s = sqrt(1.0 + " + m22 + " - " + m00 + " - " + m11 + ") * 2.0;\n";
  out.prelude += "      " + rawQuatName + " = vec4((" + m02 + " + " + m20 + ") / s, (" + m12 + " + " + m21 +
                 ") / s, 0.25 * s, (" + m10 + " - " + m01 + ") / s);\n";
  out.prelude += "    }\n";
  out.prelude += "  } else {\n";
  out.prelude += "    if (" + m11 + " > " + m22 + ") {\n";
  out.prelude += "      float s = sqrt(1.0 + " + m11 + " - " + m00 + " - " + m22 + ") * 2.0;\n";
  out.prelude += "      " + rawQuatName + " = vec4((" + m01 + " + " + m10 + ") / s, 0.25 * s, (" + m12 + " + " +
                 m21 + ") / s, (" + m02 + " - " + m20 + ") / s);\n";
  out.prelude += "    } else {\n";
  out.prelude += "      float s = sqrt(1.0 + " + m22 + " - " + m00 + " - " + m11 + ") * 2.0;\n";
  out.prelude += "      " + rawQuatName + " = vec4((" + m02 + " + " + m20 + ") / s, (" + m12 + " + " + m21 +
                 ") / s, 0.25 * s, (" + m10 + " - " + m01 + ") / s);\n";
  out.prelude += "    }\n";
  out.prelude += "  }\n";
  out.prelude += "}\n";
  out.prelude += "float " + lenSquaredName + " = dot(" + rawQuatName + ", " + rawQuatName + ");\n";
  out.prelude += "vec4 " + normalizedQuatName + " = ((" + lenSquaredName + " <= 0.0) ? " + rawQuatName +
                 " : (" + rawQuatName + " / sqrt(" + lenSquaredName + ")));\n";
  return out;
}

} // namespace

std::string callLeafName(const std::string &name) {
  const size_t slash = name.find_last_of('/');
  if (slash == std::string::npos) {
    return name;
  }
  return name.substr(slash + 1);
}

int vectorDimension(GlslType type) {
  switch (type) {
  case GlslType::Vec2:
    return 2;
  case GlslType::Vec3:
    return 3;
  case GlslType::Vec4:
    return 4;
  default:
    return 0;
  }
}

int matrixDimension(GlslType type) {
  switch (type) {
  case GlslType::Mat2:
    return 2;
  case GlslType::Mat3:
    return 3;
  case GlslType::Mat4:
    return 4;
  default:
    return 0;
  }
}

bool vectorFieldComponentIndex(const std::string &name, int dimension, int &componentOut) {
  if (name.size() != 1) {
    return false;
  }
  const char componentName = name[0];
  const char *components = "xyzw";
  for (int i = 0; i < dimension; ++i) {
    if (components[i] != componentName) {
      continue;
    }
    componentOut = i;
    return true;
  }
  return false;
}

bool matrixFieldRowColumn(const std::string &name, int dimension, int &rowOut, int &columnOut) {
  if (name.size() != 3 || name[0] != 'm' || name[1] < '0' || name[1] > '9' || name[2] < '0' || name[2] > '9') {
    return false;
  }
  const int row = name[1] - '0';
  const int column = name[2] - '0';
  if (row < 0 || row >= dimension || column < 0 || column >= dimension) {
    return false;
  }
  rowOut = row;
  columnOut = column;
  return true;
}

bool emitQuaternionHelperCall(const Expr &expr,
                              const std::string &leafName,
                              EmitState &state,
                              std::string &error,
                              ExprResult &out) {
  if (leafName == "Quat") {
    if (expr.args.size() != 4) {
      error = "glsl backend requires Quat constructor with 4 arguments";
      return false;
    }
    out = {};
    out.type = GlslType::Quat;
    out.code = "vec4(";
    for (size_t i = 0; i < expr.args.size(); ++i) {
      ExprResult arg = emitExpr(expr.args[i], state, error);
      if (!error.empty()) {
        return false;
      }
      if (!isNumericType(arg.type)) {
        error = "glsl backend requires numeric quaternion constructor arguments";
        return false;
      }
      arg = castScalarToMatrixElementFloat(arg);
      out.prelude += arg.prelude;
      if (i != 0) {
        out.code += ", ";
      }
      out.code += arg.code;
    }
    out.code += ")";
    return true;
  }

  if (leafName == "toNormalized" || leafName == "normalize") {
    if (expr.args.size() != 1) {
      error = "glsl backend requires quaternion normalize helper with one receiver";
      return false;
    }
    ExprResult value = emitExpr(expr.args.front(), state, error);
    if (!error.empty()) {
      return false;
    }
    if (!isQuaternionType(value.type)) {
      error = "glsl backend requires quaternion operand for " + leafName;
      return false;
    }
    out = emitNormalizedQuatValue(value, state);
    return true;
  }

  if (leafName == "quat_to_mat3") {
    if (expr.args.size() != 1) {
      error = "glsl backend requires quat_to_mat3(value)";
      return false;
    }
    ExprResult value = emitExpr(expr.args.front(), state, error);
    if (!error.empty()) {
      return false;
    }
    if (!isQuaternionType(value.type)) {
      error = "glsl backend requires quaternion operand for quat_to_mat3";
      return false;
    }
    out = emitQuatToMat3Value(value, state);
    return true;
  }

  if (leafName == "quat_to_mat4") {
    if (expr.args.size() != 1) {
      error = "glsl backend requires quat_to_mat4(value)";
      return false;
    }
    ExprResult value = emitExpr(expr.args.front(), state, error);
    if (!error.empty()) {
      return false;
    }
    if (!isQuaternionType(value.type)) {
      error = "glsl backend requires quaternion operand for quat_to_mat4";
      return false;
    }
    out = emitQuatToMat4Value(value, state);
    return true;
  }

  if (leafName == "mat3_to_quat") {
    if (expr.args.size() != 1) {
      error = "glsl backend requires mat3_to_quat(value)";
      return false;
    }
    ExprResult value = emitExpr(expr.args.front(), state, error);
    if (!error.empty()) {
      return false;
    }
    if (value.type != GlslType::Mat3) {
      error = "glsl backend requires Mat3 operand for mat3_to_quat";
      return false;
    }
    out = emitMat3ToQuatValue(value, state);
    return true;
  }

  return false;
}

bool emitVectorArithmetic(const Expr &expr,
                          const std::string &name,
                          EmitState &state,
                          std::string &error,
                          ExprResult &out) {
  if (expr.args.size() != 2) {
    return false;
  }
  ExprResult left = emitExpr(expr.args[0], state, error);
  if (!error.empty()) {
    return false;
  }
  ExprResult right = emitExpr(expr.args[1], state, error);
  if (!error.empty()) {
    return false;
  }
  const bool leftVector = isVectorType(left.type);
  const bool rightVector = isVectorType(right.type);
  if (!leftVector && !rightVector) {
    return false;
  }
  if (name == "plus" || name == "minus") {
    if (!leftVector || !rightVector || left.type != right.type) {
      error = "glsl backend requires matching vector operands for " + name;
      return false;
    }
    out.type = left.type;
    out.code = "(" + left.code + (name == "plus" ? " + " : " - ") + right.code + ")";
    out.prelude = left.prelude + right.prelude;
    return true;
  }
  if (name == "multiply") {
    if (leftVector && isNumericType(right.type)) {
      right = castScalarToMatrixElementFloat(right);
      out.type = left.type;
      out.code = "(" + left.code + " * " + right.code + ")";
      out.prelude = left.prelude + right.prelude;
      return true;
    }
    if (isNumericType(left.type) && rightVector) {
      left = castScalarToMatrixElementFloat(left);
      out.type = right.type;
      out.code = "(" + left.code + " * " + right.code + ")";
      out.prelude = left.prelude + right.prelude;
      return true;
    }
    error = "glsl backend requires vector-scalar operands for multiply";
    return false;
  }
  if (name == "divide") {
    if (!leftVector || !isNumericType(right.type)) {
      error = "glsl backend requires vector-scalar operands for divide";
      return false;
    }
    right = castScalarToMatrixElementFloat(right);
    out.type = left.type;
    out.code = "(" + left.code + " / " + right.code + ")";
    out.prelude = left.prelude + right.prelude;
    return true;
  }
  return false;
}

bool emitQuaternionArithmetic(const Expr &expr,
                              const std::string &name,
                              EmitState &state,
                              std::string &error,
                              ExprResult &out) {
  if (expr.args.size() != 2) {
    return false;
  }
  ExprResult left = emitExpr(expr.args[0], state, error);
  if (!error.empty()) {
    return false;
  }
  ExprResult right = emitExpr(expr.args[1], state, error);
  if (!error.empty()) {
    return false;
  }
  const bool leftQuat = isQuaternionType(left.type);
  const bool rightQuat = isQuaternionType(right.type);
  if (!leftQuat && !rightQuat) {
    return false;
  }
  if (name == "plus" || name == "minus") {
    if (!leftQuat || !rightQuat) {
      error = "glsl backend requires matching quaternion operands for " + name;
      return false;
    }
    out.type = GlslType::Quat;
    out.code = "(" + left.code + (name == "plus" ? " + " : " - ") + right.code + ")";
    out.prelude = left.prelude + right.prelude;
    return true;
  }
  if (name == "multiply") {
    if (leftQuat && rightQuat) {
      const std::string lx = componentExpr(left.code, 'x');
      const std::string ly = componentExpr(left.code, 'y');
      const std::string lz = componentExpr(left.code, 'z');
      const std::string lw = componentExpr(left.code, 'w');
      const std::string rx = componentExpr(right.code, 'x');
      const std::string ry = componentExpr(right.code, 'y');
      const std::string rz = componentExpr(right.code, 'z');
      const std::string rw = componentExpr(right.code, 'w');
      out.type = GlslType::Quat;
      out.code = "vec4(" + lw + " * " + rx + " + " + lx + " * " + rw + " + " + ly + " * " + rz + " - " + lz +
                 " * " + ry + ", " + lw + " * " + ry + " - " + lx + " * " + rz + " + " + ly + " * " + rw + " + " +
                 lz + " * " + rx + ", " + lw + " * " + rz + " + " + lx + " * " + ry + " - " + ly + " * " + rx +
                 " + " + lz + " * " + rw + ", " + lw + " * " + rw + " - " + lx + " * " + rx + " - " + ly + " * " +
                 ry + " - " + lz + " * " + rz + ")";
      out.prelude = left.prelude + right.prelude;
      return true;
    }
    if (leftQuat && right.type == GlslType::Vec3) {
      ExprResult basis = emitQuatToMat3Value(left, state);
      out.type = GlslType::Vec3;
      out.code = "(" + basis.code + " * " + right.code + ")";
      out.prelude = basis.prelude + right.prelude;
      return true;
    }
    if (leftQuat && isNumericType(right.type)) {
      right = castScalarToMatrixElementFloat(right);
      out.type = GlslType::Quat;
      out.code = "(" + left.code + " * " + right.code + ")";
      out.prelude = left.prelude + right.prelude;
      return true;
    }
    if (isNumericType(left.type) && rightQuat) {
      left = castScalarToMatrixElementFloat(left);
      out.type = GlslType::Quat;
      out.code = "(" + left.code + " * " + right.code + ")";
      out.prelude = left.prelude + right.prelude;
      return true;
    }
    error = "glsl backend requires quaternion, quaternion-Vec3, or scalar-quaternion operands for multiply";
    return false;
  }
  if (name == "divide") {
    if (!leftQuat || !isNumericType(right.type)) {
      error = "glsl backend requires quaternion-scalar operands for divide";
      return false;
    }
    right = castScalarToMatrixElementFloat(right);
    out.type = GlslType::Quat;
    out.code = "(" + left.code + " / " + right.code + ")";
    out.prelude = left.prelude + right.prelude;
    return true;
  }
  return false;
}

bool emitVectorConstructor(const Expr &expr,
                           const std::string &name,
                           GlslType type,
                           EmitState &state,
                           std::string &error,
                           ExprResult &out) {
  const int dimension = vectorDimension(type);
  const size_t expectedArgs = static_cast<size_t>(dimension);
  if (expr.args.size() != expectedArgs) {
    error = "glsl backend requires " + name + " constructor with " + std::to_string(expectedArgs) + " arguments";
    return false;
  }
  out = {};
  out.type = type;
  out.code = glslTypeName(type) + "(";
  for (size_t i = 0; i < expectedArgs; ++i) {
    ExprResult arg = emitExpr(expr.args[i], state, error);
    if (!error.empty()) {
      return false;
    }
    if (!isNumericType(arg.type)) {
      error = "glsl backend requires numeric vector constructor arguments";
      return false;
    }
    arg = castScalarToMatrixElementFloat(arg);
    out.prelude += arg.prelude;
    if (i != 0) {
      out.code += ", ";
    }
    out.code += arg.code;
  }
  out.code += ")";
  return true;
}

bool emitMatrixConstructor(const Expr &expr,
                           const std::string &name,
                           GlslType type,
                           EmitState &state,
                           std::string &error,
                           ExprResult &out) {
  const int dimension = matrixDimension(type);
  const size_t expectedArgs = static_cast<size_t>(dimension * dimension);
  if (expr.args.size() != expectedArgs) {
    error = "glsl backend requires " + name + " constructor with " + std::to_string(expectedArgs) + " arguments";
    return false;
  }
  out = {};
  out.type = type;
  std::vector<std::string> rows(expectedArgs);
  for (size_t i = 0; i < expectedArgs; ++i) {
    ExprResult arg = emitExpr(expr.args[i], state, error);
    if (!error.empty()) {
      return false;
    }
    if (!isNumericType(arg.type)) {
      error = "glsl backend requires numeric matrix constructor arguments";
      return false;
    }
    arg = castScalarToMatrixElementFloat(arg);
    out.prelude += arg.prelude;
    rows[i] = arg.code;
  }
  out.code = glslTypeName(type) + "(";
  bool first = true;
  for (int column = 0; column < dimension; ++column) {
    for (int row = 0; row < dimension; ++row) {
      if (!first) {
        out.code += ", ";
      }
      first = false;
      out.code += rows[static_cast<size_t>(row * dimension + column)];
    }
  }
  out.code += ")";
  return true;
}

bool emitMatrixArithmetic(const Expr &expr,
                          const std::string &name,
                          EmitState &state,
                          std::string &error,
                          ExprResult &out) {
  if (expr.args.size() != 2) {
    return false;
  }
  ExprResult left = emitExpr(expr.args[0], state, error);
  if (!error.empty()) {
    return false;
  }
  ExprResult right = emitExpr(expr.args[1], state, error);
  if (!error.empty()) {
    return false;
  }
  const bool leftMatrix = isMatrixType(left.type);
  const bool rightMatrix = isMatrixType(right.type);
  if (name == "plus" || name == "minus") {
    if (!leftMatrix || !rightMatrix || left.type != right.type) {
      error = "glsl backend requires matching matrix operands for " + name;
      return false;
    }
    out.type = left.type;
    out.code = "(" + left.code + (name == "plus" ? " + " : " - ") + right.code + ")";
    out.prelude = left.prelude + right.prelude;
    return true;
  }
  if (name == "multiply") {
    if (leftMatrix && rightMatrix) {
      if (left.type != right.type) {
        error = "glsl backend requires matching matrix operands for multiply";
        return false;
      }
      out.type = left.type;
      out.code = "(" + left.code + " * " + right.code + ")";
      out.prelude = left.prelude + right.prelude;
      return true;
    }
    if (leftMatrix && isVectorType(right.type)) {
      if (matrixDimension(left.type) != vectorDimension(right.type)) {
        error = "glsl backend requires matching matrix/vector operands for multiply";
        return false;
      }
      out.type = right.type;
      out.code = "(" + left.code + " * " + right.code + ")";
      out.prelude = left.prelude + right.prelude;
      return true;
    }
    if (leftMatrix && isNumericType(right.type)) {
      right = castScalarToMatrixElementFloat(right);
      out.type = left.type;
      out.code = "(" + left.code + " * " + right.code + ")";
      out.prelude = left.prelude + right.prelude;
      return true;
    }
    if (isNumericType(left.type) && rightMatrix) {
      left = castScalarToMatrixElementFloat(left);
      out.type = right.type;
      out.code = "(" + left.code + " * " + right.code + ")";
      out.prelude = left.prelude + right.prelude;
      return true;
    }
    return false;
  }
  if (name == "divide") {
    if (!leftMatrix || !isNumericType(right.type)) {
      return false;
    }
    right = castScalarToMatrixElementFloat(right);
    out.type = left.type;
    out.code = "(" + left.code + " / " + right.code + ")";
    out.prelude = left.prelude + right.prelude;
    return true;
  }
  return false;
}

} // namespace primec::glsl_emitter
