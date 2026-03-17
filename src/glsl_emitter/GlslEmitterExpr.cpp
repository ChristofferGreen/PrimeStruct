#include "GlslEmitterInternal.h"

namespace primec::glsl_emitter {

ExprResult emitExpr(const Expr &expr, EmitState &state, std::string &error);

namespace {

std::string callLeafName(const std::string &name) {
  const size_t slash = name.find_last_of('/');
  if (slash == std::string::npos) {
    return name;
  }
  return name.substr(slash + 1);
}

std::string componentExpr(const std::string &valueCode, char component) {
  std::string out = "(" + valueCode + ").";
  out.push_back(component);
  return out;
}

std::string matrixElementExpr(const std::string &valueCode, int row, int column) {
  return "(" + valueCode + ")[" + std::to_string(column) + "][" + std::to_string(row) + "]";
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

bool emitVectorConstructor(const Expr &expr, const std::string &name, GlslType type, EmitState &state, std::string &error, ExprResult &out) {
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

} // namespace

ExprResult emitBinaryNumeric(const Expr &leftExpr, const Expr &rightExpr, const char *op, EmitState &state, std::string &error) {
  ExprResult left = emitExpr(leftExpr, state, error);
  if (error.empty() == false) {
    return {};
  }
  ExprResult right = emitExpr(rightExpr, state, error);
  if (error.empty() == false) {
    return {};
  }
  GlslType resultType = combineNumericTypes(left.type, right.type, error);
  if (resultType == GlslType::Unknown) {
    return {};
  }
  left = castExpr(left, resultType);
  right = castExpr(right, resultType);
  ExprResult out;
  out.type = resultType;
  out.code = "(" + left.code + " " + op + " " + right.code + ")";
  out.prelude = left.prelude;
  out.prelude += right.prelude;
  return out;
}

ExprResult emitExpr(const Expr &expr, EmitState &state, std::string &error) {
  if (expr.kind == Expr::Kind::Literal) {
    ExprResult out;
    if (expr.isUnsigned) {
      if (expr.intWidth == 64) {
        out.type = GlslType::UInt64;
        out.code = "uint64_t(" + std::to_string(static_cast<uint64_t>(expr.literalValue)) + ")";
      } else {
        out.type = GlslType::UInt;
        out.code = std::to_string(static_cast<uint64_t>(expr.literalValue)) + "u";
      }
    } else if (expr.intWidth == 64) {
      out.type = GlslType::Int64;
      out.code = "int64_t(" + std::to_string(static_cast<int64_t>(expr.literalValue)) + ")";
    } else {
      out.type = GlslType::Int;
      out.code = std::to_string(static_cast<int64_t>(expr.literalValue));
    }
    if (out.type == GlslType::Int64 || out.type == GlslType::UInt64) {
      state.needsInt64Ext = true;
    }
    return out;
  }
  if (expr.kind == Expr::Kind::BoolLiteral) {
    ExprResult out;
    out.type = GlslType::Bool;
    out.code = expr.boolValue ? "true" : "false";
    return out;
  }
  if (expr.kind == Expr::Kind::FloatLiteral) {
    ExprResult out;
    if (expr.floatWidth == 64) {
      state.needsFp64Ext = true;
      out.type = GlslType::Double;
      out.code = "double(" + ensureFloatLiteral(expr.floatValue) + ")";
    } else {
      out.type = GlslType::Float;
      out.code = ensureFloatLiteral(expr.floatValue);
    }
    return out;
  }
  if (expr.kind == Expr::Kind::StringLiteral) {
    error = "glsl backend does not support string literals";
    return {};
  }
  if (expr.kind == Expr::Kind::Name) {
    auto it = state.locals.find(expr.name);
    if (it != state.locals.end()) {
      ExprResult out;
      out.type = it->second.type;
      out.code = expr.name;
      return out;
    }
    std::string constantName;
    if (getMathConstantName(expr, constantName)) {
      state.needsFp64Ext = true;
      if (constantName == "pi") {
        ExprResult out;
        out.type = GlslType::Double;
        out.code = "double(3.14159265358979323846)";
        return out;
      }
      if (constantName == "tau") {
        ExprResult out;
        out.type = GlslType::Double;
        out.code = "double(6.28318530717958647692)";
        return out;
      }
      ExprResult out;
      out.type = GlslType::Double;
      out.code = "double(2.71828182845904523536)";
      return out;
    }
    error = "glsl backend requires local binding for name: " + expr.name;
    return {};
  }
  if (expr.kind != Expr::Kind::Call) {
    error = "glsl backend encountered unsupported expression";
    return {};
  }
  if (expr.isFieldAccess) {
    if (expr.args.size() != 1) {
      error = "glsl backend requires field access receiver";
      return {};
    }
    ExprResult receiver = emitExpr(expr.args.front(), state, error);
    if (!error.empty()) {
      return {};
    }
    if (isQuaternionType(receiver.type)) {
      int component = 0;
      if (!vectorFieldComponentIndex(expr.name, 4, component)) {
        error = "glsl backend does not support quaternion field: " + expr.name;
        return {};
      }
      ExprResult out;
      out.type = GlslType::Float;
      out.code = "(" + receiver.code + ").";
      out.code.push_back("xyzw"[component]);
      out.prelude = receiver.prelude;
      return out;
    }
    const int vectorSize = vectorDimension(receiver.type);
    if (vectorSize != 0) {
      int component = 0;
      if (!vectorFieldComponentIndex(expr.name, vectorSize, component)) {
        error = "glsl backend does not support vector field: " + expr.name;
        return {};
      }
      ExprResult out;
      out.type = GlslType::Float;
      out.code = "(" + receiver.code + ").";
      out.code.push_back("xyzw"[component]);
      out.prelude = receiver.prelude;
      return out;
    }
    const int dimension = matrixDimension(receiver.type);
    if (dimension == 0) {
      error = "glsl backend only supports vector, quaternion, or matrix field access";
      return {};
    }
    int row = 0;
    int column = 0;
    if (!matrixFieldRowColumn(expr.name, dimension, row, column)) {
      error = "glsl backend does not support matrix field: " + expr.name;
      return {};
    }
    ExprResult out;
    out.type = GlslType::Float;
    out.code = "(" + receiver.code + ")[" + std::to_string(column) + "][" + std::to_string(row) + "]";
    out.prelude = receiver.prelude;
    return out;
  }
  const std::string name = normalizeName(expr);
  const std::string leafName = callLeafName(name);
  ExprResult quaternionHelperOut;
  if (emitQuaternionHelperCall(expr, leafName, state, error, quaternionHelperOut)) {
    return quaternionHelperOut;
  }
  if (!error.empty()) {
    return {};
  }
  if (name == "Vec2") {
    ExprResult out;
    if (!emitVectorConstructor(expr, name, GlslType::Vec2, state, error, out)) {
      return {};
    }
    return out;
  }
  if (name == "Vec3") {
    ExprResult out;
    if (!emitVectorConstructor(expr, name, GlslType::Vec3, state, error, out)) {
      return {};
    }
    return out;
  }
  if (name == "Vec4") {
    ExprResult out;
    if (!emitVectorConstructor(expr, name, GlslType::Vec4, state, error, out)) {
      return {};
    }
    return out;
  }
  if (name == "Mat2") {
    ExprResult out;
    if (!emitMatrixConstructor(expr, name, GlslType::Mat2, state, error, out)) {
      return {};
    }
    return out;
  }
  if (name == "Mat3") {
    ExprResult out;
    if (!emitMatrixConstructor(expr, name, GlslType::Mat3, state, error, out)) {
      return {};
    }
    return out;
  }
  if (name == "Mat4") {
    ExprResult out;
    if (!emitMatrixConstructor(expr, name, GlslType::Mat4, state, error, out)) {
      return {};
    }
    return out;
  }
  if (name == "block") {
    if (!expr.hasBodyArguments) {
      error = "glsl backend requires block() { ... }";
      return {};
    }
    if (!expr.args.empty() || !expr.templateArgs.empty() || hasNamedArguments(expr.argNames)) {
      error = "glsl backend requires block() { ... }";
      return {};
    }
    EmitState blockState = state;
    std::string blockBody;
    ExprResult blockResult;
    if (!emitValueBlock(expr, blockState, blockBody, error, "  ", blockResult)) {
      return {};
    }
    std::string typeName = glslTypeName(blockResult.type);
    if (typeName.empty()) {
      error = "glsl backend requires GLSL-supported block values";
      return {};
    }
    std::string tempName = allocTempName(state, "_ps_block_");
    ExprResult out;
    out.type = blockResult.type;
    out.code = tempName;
    out.prelude = typeName + " " + tempName + ";\n";
    out.prelude += "{\n";
    out.prelude += blockBody;
    out.prelude += "  " + tempName + " = " + blockResult.code + ";\n";
    out.prelude += "}\n";
    state.needsInt64Ext = state.needsInt64Ext || blockState.needsInt64Ext;
    state.needsFp64Ext = state.needsFp64Ext || blockState.needsFp64Ext;
    state.needsIntPow = state.needsIntPow || blockState.needsIntPow;
    state.needsUIntPow = state.needsUIntPow || blockState.needsUIntPow;
    state.needsInt64Pow = state.needsInt64Pow || blockState.needsInt64Pow;
    state.needsUInt64Pow = state.needsUInt64Pow || blockState.needsUInt64Pow;
    state.tempIndex = blockState.tempIndex;
    return out;
  }
  if (name == "plus" || name == "minus" || name == "multiply" || name == "divide") {
    ExprResult quaternionOut;
    if (emitQuaternionArithmetic(expr, name, state, error, quaternionOut)) {
      return quaternionOut;
    }
    if (!error.empty()) {
      return {};
    }
    ExprResult vectorOut;
    if (emitVectorArithmetic(expr, name, state, error, vectorOut)) {
      return vectorOut;
    }
    if (!error.empty()) {
      return {};
    }
    ExprResult matrixOut;
    if (emitMatrixArithmetic(expr, name, state, error, matrixOut)) {
      return matrixOut;
    }
    if (!error.empty()) {
      return {};
    }
    if (expr.args.size() != 2) {
      error = "glsl backend requires binary numeric operator arguments";
      return {};
    }
    const char *op = name == "plus" ? "+" : (name == "minus" ? "-" : (name == "multiply" ? "*" : "/"));
    return emitBinaryNumeric(expr.args[0], expr.args[1], op, state, error);
  }
  if (name == "negate") {
    if (expr.args.size() != 1) {
      error = "glsl backend requires unary negate argument";
      return {};
    }
    ExprResult arg = emitExpr(expr.args[0], state, error);
    if (!error.empty()) {
      return {};
    }
    if (!isNumericType(arg.type)) {
      error = "glsl backend requires numeric operand for negate";
      return {};
    }
    ExprResult out;
    out.type = arg.type;
    out.code = "(-" + arg.code + ")";
    out.prelude = arg.prelude;
    return out;
  }
  if (name == "assign") {
    if (expr.args.size() != 2) {
      error = "glsl backend requires assign(target, value)";
      return {};
    }
    const Expr &target = expr.args.front();
    if (target.kind != Expr::Kind::Name) {
      error = "glsl backend requires assign target to be a local name";
      return {};
    }
    auto it = state.locals.find(target.name);
    if (it == state.locals.end()) {
      error = "glsl backend requires local binding for assign target";
      return {};
    }
    if (!it->second.isMutable) {
      error = "glsl backend requires assign target to be mutable";
      return {};
    }
    ExprResult value = emitExpr(expr.args[1], state, error);
    if (!error.empty()) {
      return {};
    }
    value = castExpr(value, it->second.type);
    ExprResult out;
    out.type = it->second.type;
    out.code = "(" + target.name + " = " + value.code + ")";
    out.prelude = value.prelude;
    return out;
  }
  if (name == "increment" || name == "decrement") {
    if (expr.args.size() != 1) {
      error = "glsl backend requires increment/decrement target to be a local name";
      return {};
    }
    const Expr &target = expr.args.front();
    if (target.kind != Expr::Kind::Name) {
      error = "glsl backend requires increment/decrement target to be a local name";
      return {};
    }
    auto it = state.locals.find(target.name);
    if (it == state.locals.end()) {
      error = "glsl backend requires local binding for increment/decrement target";
      return {};
    }
    if (!it->second.isMutable) {
      error = "glsl backend requires increment/decrement target to be mutable";
      return {};
    }
    if (!isNumericType(it->second.type)) {
      error = "glsl backend requires increment/decrement target to be numeric";
      return {};
    }
    const char *op = (name == "increment") ? "++" : "--";
    ExprResult out;
    out.type = it->second.type;
    out.code = "(" + std::string(op) + target.name + ")";
    return out;
  }
  if (name == "equal" || name == "not_equal" || name == "less_than" || name == "less_equal" ||
      name == "greater_than" || name == "greater_equal") {
    if (expr.args.size() != 2) {
      error = "glsl backend requires comparison arguments";
      return {};
    }
    ExprResult left = emitExpr(expr.args[0], state, error);
    if (!error.empty()) {
      return {};
    }
    ExprResult right = emitExpr(expr.args[1], state, error);
    if (!error.empty()) {
      return {};
    }
    if (left.type == GlslType::Bool || right.type == GlslType::Bool) {
      if (left.type != right.type) {
        error = "glsl backend does not allow mixed boolean/numeric comparisons";
        return {};
      }
    } else {
      GlslType common = combineNumericTypes(left.type, right.type, error);
      if (common == GlslType::Unknown) {
        return {};
      }
      left = castExpr(left, common);
      right = castExpr(right, common);
    }
    const char *op = "==";
    if (name == "not_equal") {
      op = "!=";
    } else if (name == "less_than") {
      op = "<";
    } else if (name == "less_equal") {
      op = "<=";
    } else if (name == "greater_than") {
      op = ">";
    } else if (name == "greater_equal") {
      op = ">=";
    }
    ExprResult out;
    out.type = GlslType::Bool;
    out.code = "(" + left.code + " " + op + " " + right.code + ")";
    out.prelude = left.prelude;
    out.prelude += right.prelude;
    return out;
  }
  if (name == "and" || name == "or") {
    if (expr.args.size() != 2) {
      error = "glsl backend requires boolean arguments";
      return {};
    }
    ExprResult left = emitExpr(expr.args[0], state, error);
    if (!error.empty()) {
      return {};
    }
    ExprResult right = emitExpr(expr.args[1], state, error);
    if (!error.empty()) {
      return {};
    }
    if (left.type != GlslType::Bool || right.type != GlslType::Bool) {
      error = "glsl backend requires boolean operands";
      return {};
    }
    ExprResult out;
    out.type = GlslType::Bool;
    out.code = "(" + left.code + (name == "and" ? " && " : " || ") + right.code + ")";
    out.prelude = left.prelude;
    out.prelude += right.prelude;
    return out;
  }
  if (name == "not") {
    if (expr.args.size() != 1) {
      error = "glsl backend requires boolean operand";
      return {};
    }
    ExprResult arg = emitExpr(expr.args[0], state, error);
    if (!error.empty()) {
      return {};
    }
    if (arg.type != GlslType::Bool) {
      error = "glsl backend requires boolean operand";
      return {};
    }
    ExprResult out;
    out.type = GlslType::Bool;
    out.code = "(!" + arg.code + ")";
    out.prelude = arg.prelude;
    return out;
  }
  std::string mathName;
  if (getMathBuiltinName(expr, mathName)) {
    auto emitUnaryMath = [&](const char *func, bool requireFloat) -> ExprResult {
      if (expr.args.size() != 1) {
        error = mathName + " requires exactly one argument";
        return {};
      }
      ExprResult arg = emitExpr(expr.args.front(), state, error);
      if (!error.empty()) {
        return {};
      }
      if (requireFloat && arg.type != GlslType::Float && arg.type != GlslType::Double) {
        error = mathName + " requires float argument";
        return {};
      }
      if (mathName == "abs" && (arg.type == GlslType::UInt || arg.type == GlslType::UInt64)) {
        return arg;
      }
      if (mathName == "sign" && (arg.type == GlslType::UInt || arg.type == GlslType::UInt64)) {
        std::string zeroLiteral = literalForType(arg.type, 0);
        std::string oneLiteral = literalForType(arg.type, 1);
        ExprResult out;
        out.type = arg.type;
        out.code = "(" + arg.code + " == " + zeroLiteral + " ? " + zeroLiteral + " : " + oneLiteral + ")";
        out.prelude = arg.prelude;
        return out;
      }
      ExprResult out;
      out.type = arg.type;
      out.code = std::string(func) + "(" + arg.code + ")";
      out.prelude = arg.prelude;
      return out;
    };
    auto emitBinaryMath = [&](const char *func, bool requireFloat) -> ExprResult {
      if (expr.args.size() != 2) {
        error = mathName + " requires exactly two arguments";
        return {};
      }
      ExprResult left = emitExpr(expr.args[0], state, error);
      if (!error.empty()) {
        return {};
      }
      ExprResult right = emitExpr(expr.args[1], state, error);
      if (!error.empty()) {
        return {};
      }
      GlslType common = combineNumericTypes(left.type, right.type, error);
      if (common == GlslType::Unknown) {
        return {};
      }
      if (requireFloat && common != GlslType::Float && common != GlslType::Double) {
        error = mathName + " requires float arguments";
        return {};
      }
      left = castExpr(left, common);
      right = castExpr(right, common);
      ExprResult out;
      out.type = common;
      out.code = std::string(func) + "(" + left.code + ", " + right.code + ")";
      out.prelude = left.prelude;
      out.prelude += right.prelude;
      return out;
    };

    if (mathName == "abs") {
      return emitUnaryMath("abs", false);
    }
    if (mathName == "sign") {
      return emitUnaryMath("sign", false);
    }
    if (mathName == "min") {
      return emitBinaryMath("min", false);
    }
    if (mathName == "max") {
      return emitBinaryMath("max", false);
    }
    if (mathName == "clamp") {
      if (expr.args.size() != 3) {
        error = "clamp requires exactly three arguments";
        return {};
      }
      ExprResult value = emitExpr(expr.args[0], state, error);
      if (!error.empty()) {
        return {};
      }
      ExprResult minValue = emitExpr(expr.args[1], state, error);
      if (!error.empty()) {
        return {};
      }
      ExprResult maxValue = emitExpr(expr.args[2], state, error);
      if (!error.empty()) {
        return {};
      }
      GlslType common = combineNumericTypes(value.type, minValue.type, error);
      if (common == GlslType::Unknown) {
        return {};
      }
      common = combineNumericTypes(common, maxValue.type, error);
      if (common == GlslType::Unknown) {
        return {};
      }
      value = castExpr(value, common);
      minValue = castExpr(minValue, common);
      maxValue = castExpr(maxValue, common);
      ExprResult out;
      out.type = common;
      out.code = "clamp(" + value.code + ", " + minValue.code + ", " + maxValue.code + ")";
      out.prelude = value.prelude;
      out.prelude += minValue.prelude;
      out.prelude += maxValue.prelude;
      return out;
    }
    if (mathName == "saturate") {
      if (expr.args.size() != 1) {
        error = "saturate requires exactly one argument";
        return {};
      }
      ExprResult value = emitExpr(expr.args[0], state, error);
      if (!error.empty()) {
        return {};
      }
      if (!isNumericType(value.type)) {
        error = "saturate requires numeric argument";
        return {};
      }
      std::string zeroLiteral = literalForType(value.type, 0);
      std::string oneLiteral = literalForType(value.type, 1);
      ExprResult out;
      out.type = value.type;
      out.code = "clamp(" + value.code + ", " + zeroLiteral + ", " + oneLiteral + ")";
      out.prelude = value.prelude;
      return out;
    }
    if (mathName == "lerp") {
      if (expr.args.size() != 3) {
        error = "lerp requires exactly three arguments";
        return {};
      }
      ExprResult start = emitExpr(expr.args[0], state, error);
      if (!error.empty()) {
        return {};
      }
      ExprResult end = emitExpr(expr.args[1], state, error);
      if (!error.empty()) {
        return {};
      }
      ExprResult t = emitExpr(expr.args[2], state, error);
      if (!error.empty()) {
        return {};
      }
      GlslType common = combineNumericTypes(start.type, end.type, error);
      if (common == GlslType::Unknown) {
        return {};
      }
      common = combineNumericTypes(common, t.type, error);
      if (common == GlslType::Unknown) {
        return {};
      }
      start = castExpr(start, common);
      end = castExpr(end, common);
      t = castExpr(t, common);
      ExprResult out;
      out.type = common;
      out.code = "(" + start.code + " + (" + end.code + " - " + start.code + ") * " + t.code + ")";
      out.prelude = start.prelude;
      out.prelude += end.prelude;
      out.prelude += t.prelude;
      return out;
    }
    if (mathName == "pow") {
      if (expr.args.size() != 2) {
        error = "pow requires exactly two arguments";
        return {};
      }
      ExprResult base = emitExpr(expr.args[0], state, error);
      if (!error.empty()) {
        return {};
      }
      ExprResult exponent = emitExpr(expr.args[1], state, error);
      if (!error.empty()) {
        return {};
      }
      GlslType common = combineNumericTypes(base.type, exponent.type, error);
      if (common == GlslType::Unknown) {
        return {};
      }
      base = castExpr(base, common);
      exponent = castExpr(exponent, common);
      if (common == GlslType::Float || common == GlslType::Double) {
        ExprResult out;
        out.type = common;
        out.code = "pow(" + base.code + ", " + exponent.code + ")";
        out.prelude = base.prelude;
        out.prelude += exponent.prelude;
        return out;
      }
      if (!isIntegerType(common)) {
        error = "pow requires numeric arguments";
        return {};
      }
      noteTypeExtensions(common, state);
      std::string funcName = powHelperName(common);
      if (common == GlslType::Int) {
        state.needsIntPow = true;
      } else if (common == GlslType::UInt) {
        state.needsUIntPow = true;
      } else if (common == GlslType::Int64) {
        state.needsInt64Pow = true;
      } else if (common == GlslType::UInt64) {
        state.needsUInt64Pow = true;
      }
      ExprResult out;
      out.type = common;
      out.code = funcName + "(" + base.code + ", " + exponent.code + ")";
      out.prelude = base.prelude;
      out.prelude += exponent.prelude;
      return out;
    }
    if (mathName == "floor") {
      return emitUnaryMath("floor", true);
    }
    if (mathName == "ceil") {
      return emitUnaryMath("ceil", true);
    }
    if (mathName == "round") {
      return emitUnaryMath("round", true);
    }
    if (mathName == "trunc") {
      return emitUnaryMath("trunc", true);
    }
    if (mathName == "fract") {
      return emitUnaryMath("fract", true);
    }
    if (mathName == "sqrt") {
      return emitUnaryMath("sqrt", true);
    }
    if (mathName == "cbrt") {
      if (expr.args.size() != 1) {
        error = "cbrt requires exactly one argument";
        return {};
      }
      ExprResult arg = emitExpr(expr.args.front(), state, error);
      if (!error.empty()) {
        return {};
      }
      if (arg.type != GlslType::Float && arg.type != GlslType::Double) {
        error = "cbrt requires float argument";
        return {};
      }
      std::string oneThird = glslTypeName(arg.type) + "(1.0/3.0)";
      ExprResult out;
      out.type = arg.type;
      out.code = "pow(" + arg.code + ", " + oneThird + ")";
      out.prelude = arg.prelude;
      return out;
    }
    if (mathName == "exp") {
      return emitUnaryMath("exp", true);
    }
    if (mathName == "exp2") {
      return emitUnaryMath("exp2", true);
    }
    if (mathName == "log") {
      return emitUnaryMath("log", true);
    }
    if (mathName == "log2") {
      return emitUnaryMath("log2", true);
    }
    if (mathName == "log10") {
      if (expr.args.size() != 1) {
        error = "log10 requires exactly one argument";
        return {};
      }
      ExprResult arg = emitExpr(expr.args.front(), state, error);
      if (!error.empty()) {
        return {};
      }
      if (arg.type != GlslType::Float && arg.type != GlslType::Double) {
        error = "log10 requires float argument";
        return {};
      }
      std::string tenLiteral = literalForType(arg.type, 10);
      ExprResult out;
      out.type = arg.type;
      out.code = "(log(" + arg.code + ") / log(" + tenLiteral + "))";
      out.prelude = arg.prelude;
      return out;
    }
    if (mathName == "sin" || mathName == "cos" || mathName == "tan" || mathName == "asin" || mathName == "acos" ||
        mathName == "atan" || mathName == "radians" || mathName == "degrees" || mathName == "sinh" ||
        mathName == "cosh" || mathName == "tanh" || mathName == "asinh" || mathName == "acosh" ||
        mathName == "atanh") {
      return emitUnaryMath(mathName.c_str(), true);
    }
    if (mathName == "atan2") {
      if (expr.args.size() != 2) {
        error = "atan2 requires exactly two arguments";
        return {};
      }
      ExprResult y = emitExpr(expr.args[0], state, error);
      if (!error.empty()) {
        return {};
      }
      ExprResult x = emitExpr(expr.args[1], state, error);
      if (!error.empty()) {
        return {};
      }
      GlslType common = combineNumericTypes(y.type, x.type, error);
      if (common == GlslType::Unknown) {
        return {};
      }
      if (common != GlslType::Float && common != GlslType::Double) {
        error = "atan2 requires float arguments";
        return {};
      }
      y = castExpr(y, common);
      x = castExpr(x, common);
      ExprResult out;
      out.type = common;
      out.code = "atan(" + y.code + ", " + x.code + ")";
      out.prelude = y.prelude;
      out.prelude += x.prelude;
      return out;
    }
    if (mathName == "fma") {
      if (expr.args.size() != 3) {
        error = "fma requires exactly three arguments";
        return {};
      }
      ExprResult a = emitExpr(expr.args[0], state, error);
      if (!error.empty()) {
        return {};
      }
      ExprResult b = emitExpr(expr.args[1], state, error);
      if (!error.empty()) {
        return {};
      }
      ExprResult c = emitExpr(expr.args[2], state, error);
      if (!error.empty()) {
        return {};
      }
      GlslType common = combineNumericTypes(a.type, b.type, error);
      if (common == GlslType::Unknown) {
        return {};
      }
      common = combineNumericTypes(common, c.type, error);
      if (common == GlslType::Unknown) {
        return {};
      }
      if (common != GlslType::Float && common != GlslType::Double) {
        error = "fma requires float arguments";
        return {};
      }
      a = castExpr(a, common);
      b = castExpr(b, common);
      c = castExpr(c, common);
      ExprResult out;
      out.type = common;
      out.code = "fma(" + a.code + ", " + b.code + ", " + c.code + ")";
      out.prelude = a.prelude;
      out.prelude += b.prelude;
      out.prelude += c.prelude;
      return out;
    }
    if (mathName == "hypot") {
      if (expr.args.size() != 2) {
        error = "hypot requires exactly two arguments";
        return {};
      }
      ExprResult a = emitExpr(expr.args[0], state, error);
      if (!error.empty()) {
        return {};
      }
      ExprResult b = emitExpr(expr.args[1], state, error);
      if (!error.empty()) {
        return {};
      }
      GlslType common = combineNumericTypes(a.type, b.type, error);
      if (common == GlslType::Unknown) {
        return {};
      }
      if (common != GlslType::Float && common != GlslType::Double) {
        error = "hypot requires float arguments";
        return {};
      }
      a = castExpr(a, common);
      b = castExpr(b, common);
      ExprResult out;
      out.type = common;
      out.code = "sqrt(" + a.code + " * " + a.code + " + " + b.code + " * " + b.code + ")";
      out.prelude = a.prelude;
      out.prelude += b.prelude;
      return out;
    }
    if (mathName == "copysign") {
      if (expr.args.size() != 2) {
        error = "copysign requires exactly two arguments";
        return {};
      }
      ExprResult mag = emitExpr(expr.args[0], state, error);
      if (!error.empty()) {
        return {};
      }
      ExprResult sign = emitExpr(expr.args[1], state, error);
      if (!error.empty()) {
        return {};
      }
      GlslType common = combineNumericTypes(mag.type, sign.type, error);
      if (common == GlslType::Unknown) {
        return {};
      }
      if (common != GlslType::Float && common != GlslType::Double) {
        error = "copysign requires float arguments";
        return {};
      }
      mag = castExpr(mag, common);
      sign = castExpr(sign, common);
      ExprResult out;
      out.type = common;
      out.code = "(abs(" + mag.code + ") * sign(" + sign.code + "))";
      out.prelude = mag.prelude;
      out.prelude += sign.prelude;
      return out;
    }
    if (mathName == "is_nan" || mathName == "is_inf" || mathName == "is_finite") {
      if (expr.args.size() != 1) {
        error = mathName + " requires exactly one argument";
        return {};
      }
      ExprResult arg = emitExpr(expr.args.front(), state, error);
      if (!error.empty()) {
        return {};
      }
      if (arg.type != GlslType::Float && arg.type != GlslType::Double) {
        error = mathName + " requires float argument";
        return {};
      }
      std::string func = mathName == "is_nan" ? "isnan" : (mathName == "is_inf" ? "isinf" : "isfinite");
      ExprResult out;
      out.type = GlslType::Bool;
      out.code = func + "(" + arg.code + ")";
      out.prelude = arg.prelude;
      return out;
    }
    error = "glsl backend does not support math builtin: " + mathName;
    return {};
  }
  if (name == "convert") {
    if (expr.templateArgs.size() != 1 || expr.args.size() != 1) {
      error = "glsl backend requires convert<T>(value)";
      return {};
    }
    ExprResult arg = emitExpr(expr.args[0], state, error);
    if (!error.empty()) {
      return {};
    }
    GlslType target = glslTypeFromName(expr.templateArgs.front(), state, error);
    if (!error.empty()) {
      return {};
    }
    return castExpr(arg, target);
  }
  error = "glsl backend does not support builtin: " + name;
  return {};
}

} // namespace primec::glsl_emitter
