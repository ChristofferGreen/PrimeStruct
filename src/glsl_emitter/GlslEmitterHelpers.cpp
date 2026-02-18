#include "GlslEmitterInternal.h"

namespace primec::glsl_emitter {

std::string glslTypeName(GlslType type);
std::string literalForType(GlslType type, int value);

std::string normalizeName(const Expr &expr) {
  std::string name = expr.name;
  if (!name.empty() && name[0] == '/') {
    name.erase(0, 1);
  }
  return name;
}

std::string powHelperName(GlslType type) {
  switch (type) {
  case GlslType::Int:
    return "ps_pow_i32";
  case GlslType::UInt:
    return "ps_pow_u32";
  case GlslType::Int64:
    return "ps_pow_i64";
  case GlslType::UInt64:
    return "ps_pow_u64";
  default:
    return "ps_pow_unknown";
  }
}

std::string emitPowHelper(GlslType type, bool isSigned) {
  std::string typeName = glslTypeName(type);
  std::string funcName = powHelperName(type);
  std::string zeroLiteral = literalForType(type, 0);
  std::string oneLiteral = literalForType(type, 1);
  std::string out;
  out += typeName + " " + funcName + "(" + typeName + " base, " + typeName + " exp) {\n";
  if (isSigned) {
    out += "  if (exp < " + zeroLiteral + ") {\n";
    out += "    return " + zeroLiteral + ";\n";
    out += "  }\n";
  }
  out += "  " + typeName + " result = " + oneLiteral + ";\n";
  out += "  " + typeName + " factor = base;\n";
  out += "  " + typeName + " e = exp;\n";
  out += "  while (";
  if (isSigned) {
    out += "e > " + zeroLiteral;
  } else {
    out += "e != " + zeroLiteral;
  }
  out += ") {\n";
  out += "    if ((e & " + oneLiteral + ") != " + zeroLiteral + ") {\n";
  out += "      result = result * factor;\n";
  out += "    }\n";
  out += "    factor = factor * factor;\n";
  out += "    e = e >> " + oneLiteral + ";\n";
  out += "  }\n";
  out += "  return result;\n";
  out += "}\n";
  return out;
}

void appendIndented(std::string &out, const std::string &text, const std::string &indent) {
  if (text.empty()) {
    return;
  }
  size_t start = 0;
  while (start < text.size()) {
    size_t end = text.find('\n', start);
    if (end == std::string::npos) {
      end = text.size();
    }
    out += indent;
    out.append(text, start, end - start);
    out += "\n";
    start = end + 1;
  }
}

bool isSimpleCallName(const Expr &expr, const char *name) {
  if (expr.kind != Expr::Kind::Call || expr.name.empty()) {
    return false;
  }
  std::string normalized = normalizeName(expr);
  if (normalized.find('/') != std::string::npos) {
    return false;
  }
  return normalized == name;
}

bool getMathBuiltinName(const Expr &expr, std::string &out) {
  if (expr.kind != Expr::Kind::Call || expr.name.empty()) {
    return false;
  }
  std::string normalized = normalizeName(expr);
  if (normalized.rfind("math/", 0) == 0) {
    out = normalized.substr(5);
  } else {
    if (normalized.find('/') != std::string::npos) {
      return false;
    }
    out = normalized;
  }
  return out == "abs" || out == "sign" || out == "min" || out == "max" || out == "clamp" || out == "saturate" ||
         out == "lerp" || out == "pow" || out == "floor" || out == "ceil" || out == "round" || out == "trunc" ||
         out == "fract" || out == "sqrt" || out == "cbrt" || out == "exp" || out == "exp2" || out == "log" ||
         out == "log2" || out == "log10" || out == "sin" || out == "cos" || out == "tan" || out == "asin" ||
         out == "acos" || out == "atan" || out == "atan2" || out == "radians" || out == "degrees" ||
         out == "sinh" || out == "cosh" || out == "tanh" || out == "asinh" || out == "acosh" || out == "atanh" ||
         out == "fma" || out == "hypot" || out == "copysign" || out == "is_nan" || out == "is_inf" ||
         out == "is_finite";
}

bool getMathConstantName(const Expr &expr, std::string &out) {
  if (expr.name.empty()) {
    return false;
  }
  std::string normalized = normalizeName(expr);
  if (normalized.rfind("math/", 0) == 0) {
    normalized = normalized.substr(5);
  }
  if (normalized.find('/') != std::string::npos) {
    return false;
  }
  if (normalized == "pi" || normalized == "tau" || normalized == "e") {
    out = normalized;
    return true;
  }
  return false;
}

bool hasNamedArguments(const std::vector<std::optional<std::string>> &argNames) {
  for (const auto &name : argNames) {
    if (name.has_value()) {
      return true;
    }
  }
  return false;
}

bool isBindingAuxTransformName(const std::string &name) {
  return name == "mut" || name == "copy" || name == "restrict" || name == "align_bytes" || name == "align_kbytes" ||
         name == "pod" || name == "handle" || name == "gpu_lane" || name == "public" || name == "private" ||
         name == "package" || name == "static";
}

bool rejectEffectTransforms(const std::vector<Transform> &transforms,
                            const std::string &context,
                            std::string &error) {
  auto isAllowedEffect = [](const std::string &name) -> bool {
    return name == "io_out" || name == "io_err" || name.rfind("pathspace_", 0) == 0;
  };
  for (const auto &transform : transforms) {
    if (transform.name != "effects" && transform.name != "capabilities") {
      continue;
    }
    if (transform.arguments.empty()) {
      continue;
    }
    for (const auto &effectName : transform.arguments) {
      if (isAllowedEffect(effectName)) {
        continue;
      }
      if (transform.name == "effects") {
        error = "glsl backend does not support effect: " + effectName + " on " + context;
      } else {
        error = "glsl backend does not support capability: " + effectName + " on " + context;
      }
      return false;
    }
  }
  return true;
}

bool rejectEffectTransforms(const Expr &expr, const std::string &context, std::string &error) {
  if (!rejectEffectTransforms(expr.transforms, context, error)) {
    return false;
  }
  for (const auto &arg : expr.args) {
    if (!rejectEffectTransforms(arg, context, error)) {
      return false;
    }
  }
  for (const auto &bodyArg : expr.bodyArguments) {
    if (!rejectEffectTransforms(bodyArg, context, error)) {
      return false;
    }
  }
  return true;
}

bool getExplicitBindingTypeName(const Expr &expr, std::string &outTypeName) {
  for (const auto &transform : expr.transforms) {
    if (transform.name == "effects" || transform.name == "capabilities") {
      continue;
    }
    if (isBindingAuxTransformName(transform.name)) {
      continue;
    }
    if (!transform.arguments.empty()) {
      continue;
    }
    if (transform.templateArgs.empty()) {
      outTypeName = transform.name;
      return true;
    }
  }
  return false;
}

bool isEntryArgsParam(const Expr &expr) {
  bool sawType = false;
  for (const auto &transform : expr.transforms) {
    if (transform.name == "effects" || transform.name == "capabilities") {
      continue;
    }
    if (isBindingAuxTransformName(transform.name)) {
      continue;
    }
    if (!transform.arguments.empty()) {
      return false;
    }
    sawType = true;
    if (transform.name != "array" || transform.templateArgs.size() != 1 || transform.templateArgs.front() != "string") {
      return false;
    }
  }
  return sawType;
}

GlslType glslTypeFromName(const std::string &name, EmitState &state, std::string &error) {
  if (name == "bool") {
    return GlslType::Bool;
  }
  if (name == "i32" || name == "int") {
    return GlslType::Int;
  }
  if (name == "u32") {
    return GlslType::UInt;
  }
  if (name == "i64") {
    state.needsInt64Ext = true;
    return GlslType::Int64;
  }
  if (name == "u64") {
    state.needsInt64Ext = true;
    return GlslType::UInt64;
  }
  if (name == "f32" || name == "float") {
    return GlslType::Float;
  }
  if (name == "f64") {
    state.needsFp64Ext = true;
    return GlslType::Double;
  }
  error = "glsl backend does not support type: " + name;
  return GlslType::Unknown;
}

std::string glslTypeName(GlslType type) {
  switch (type) {
  case GlslType::Bool:
    return "bool";
  case GlslType::Int:
    return "int";
  case GlslType::UInt:
    return "uint";
  case GlslType::Int64:
    return "int64_t";
  case GlslType::UInt64:
    return "uint64_t";
  case GlslType::Float:
    return "float";
  case GlslType::Double:
    return "double";
  case GlslType::Unknown:
    return "";
  }
  return "";
}

bool isNumericType(GlslType type) {
  return type == GlslType::Int || type == GlslType::UInt || type == GlslType::Int64 || type == GlslType::UInt64 ||
         type == GlslType::Float || type == GlslType::Double;
}

bool isIntegerType(GlslType type) {
  return type == GlslType::Int || type == GlslType::UInt || type == GlslType::Int64 || type == GlslType::UInt64;
}

bool isSignedInteger(GlslType type) {
  return type == GlslType::Int || type == GlslType::Int64;
}

bool isUnsignedInteger(GlslType type) {
  return type == GlslType::UInt || type == GlslType::UInt64;
}

void noteTypeExtensions(GlslType type, EmitState &state) {
  if (type == GlslType::Int64 || type == GlslType::UInt64) {
    state.needsInt64Ext = true;
  }
  if (type == GlslType::Double) {
    state.needsFp64Ext = true;
  }
}

std::string ensureFloatLiteral(const std::string &literal);

std::string literalForType(GlslType type, int value) {
  switch (type) {
  case GlslType::Int:
    return std::to_string(value);
  case GlslType::UInt:
    return std::to_string(value) + "u";
  case GlslType::Int64:
    return "int64_t(" + std::to_string(value) + ")";
  case GlslType::UInt64:
    return "uint64_t(" + std::to_string(value) + ")";
  case GlslType::Float: {
    std::string literal = std::to_string(value);
    return ensureFloatLiteral(literal);
  }
  case GlslType::Double: {
    std::string literal = std::to_string(value);
    return "double(" + ensureFloatLiteral(literal) + ")";
  }
  case GlslType::Bool:
  case GlslType::Unknown:
    return std::to_string(value);
  }
  return std::to_string(value);
}

std::string allocTempName(EmitState &state, const std::string &prefix) {
  for (;;) {
    std::string candidate = prefix + std::to_string(state.tempIndex++);
    if (state.locals.find(candidate) == state.locals.end()) {
      return candidate;
    }
  }
}

GlslType combineNumericTypes(GlslType left, GlslType right, std::string &error) {
  if (left == right) {
    return left;
  }
  if (!isNumericType(left) || !isNumericType(right)) {
    error = "glsl backend requires numeric operands";
    return GlslType::Unknown;
  }
  if (left == GlslType::Double || right == GlslType::Double) {
    return GlslType::Double;
  }
  if (left == GlslType::Float || right == GlslType::Float) {
    return GlslType::Float;
  }
  if (left == GlslType::Int64 || right == GlslType::Int64) {
    if (isSignedInteger(left) && isSignedInteger(right)) {
      return GlslType::Int64;
    }
    error = "glsl backend does not allow mixed signed/unsigned integer operands";
    return GlslType::Unknown;
  }
  if (left == GlslType::UInt64 || right == GlslType::UInt64) {
    if (isUnsignedInteger(left) && isUnsignedInteger(right)) {
      return GlslType::UInt64;
    }
    error = "glsl backend does not allow mixed signed/unsigned integer operands";
    return GlslType::Unknown;
  }
  if (left == GlslType::Int && right == GlslType::Int) {
    return GlslType::Int;
  }
  if (left == GlslType::UInt && right == GlslType::UInt) {
    return GlslType::UInt;
  }
  error = "glsl backend does not allow mixed signed/unsigned integer operands";
  return GlslType::Unknown;
}

std::string ensureFloatLiteral(const std::string &literal) {
  if (literal.find('.') == std::string::npos && literal.find('e') == std::string::npos &&
      literal.find('E') == std::string::npos) {
    return literal + ".0";
  }
  return literal;
}

ExprResult emitExpr(const Expr &expr, EmitState &state, std::string &error);
bool emitValueBlock(const Expr &blockExpr,
                    EmitState &state,
                    std::string &out,
                    std::string &error,
                    const std::string &indent,
                    ExprResult &result);

ExprResult castExpr(const ExprResult &expr, GlslType target) {
  if (expr.type == target) {
    return expr;
  }
  ExprResult out;
  out.type = target;
  out.code = glslTypeName(target) + "(" + expr.code + ")";
  out.prelude = expr.prelude;
  return out;
}

} // namespace primec::glsl_emitter
