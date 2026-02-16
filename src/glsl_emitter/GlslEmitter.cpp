#include "primec/GlslEmitter.h"

#include <string>
#include <unordered_map>
#include <vector>

namespace primec {
namespace {

enum class GlslType {
  Bool,
  Int,
  UInt,
  Int64,
  UInt64,
  Float,
  Double,
  Unknown,
};

struct LocalInfo {
  GlslType type = GlslType::Unknown;
  bool isMutable = false;
};

struct EmitState {
  std::unordered_map<std::string, LocalInfo> locals;
  bool needsInt64Ext = false;
  bool needsFp64Ext = false;
  bool needsIntPow = false;
  bool needsUIntPow = false;
  bool needsInt64Pow = false;
  bool needsUInt64Pow = false;
  uint32_t tempIndex = 0;
};

struct ExprResult {
  std::string code;
  GlslType type = GlslType::Unknown;
  std::string prelude;
};

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
      if (transform.name == "effects") {
        if (isAllowedEffect(effectName)) {
          continue;
        }
        error = "glsl backend does not support effect: " + effectName + " on " + context;
        return false;
      }
      error = "glsl backend does not support capability: " + effectName + " on " + context;
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
  const std::string name = normalizeName(expr);
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
      error = "glsl backend requires numeric or boolean block values";
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

bool emitStatement(const Expr &stmt, EmitState &state, std::string &out, std::string &error, const std::string &indent);

bool emitBlock(const std::vector<Expr> &stmts,
               EmitState &state,
               std::string &out,
               std::string &error,
               const std::string &indent) {
  auto savedLocals = state.locals;
  for (const auto &stmt : stmts) {
    if (!emitStatement(stmt, state, out, error, indent)) {
      state.locals = savedLocals;
      return false;
    }
  }
  state.locals = savedLocals;
  return true;
}

bool emitValueBlock(const Expr &blockExpr,
                    EmitState &state,
                    std::string &out,
                    std::string &error,
                    const std::string &indent,
                    ExprResult &result) {
  if (blockExpr.bodyArguments.empty()) {
    error = "glsl backend requires block to yield a value";
    return false;
  }
  for (size_t i = 0; i < blockExpr.bodyArguments.size(); ++i) {
    const Expr &stmt = blockExpr.bodyArguments[i];
    const bool isLast = (i + 1 == blockExpr.bodyArguments.size());
    if (stmt.isBinding) {
      if (isLast) {
        error = "glsl backend requires block to end with a value expression";
        return false;
      }
      if (!emitStatement(stmt, state, out, error, indent)) {
        return false;
      }
      continue;
    }
    if (isSimpleCallName(stmt, "return")) {
      if (stmt.args.size() != 1) {
        error = "glsl backend requires return(value) in value blocks";
        return false;
      }
      result = emitExpr(stmt.args.front(), state, error);
      if (!error.empty()) {
        return false;
      }
      appendIndented(out, result.prelude, indent);
      result.prelude.clear();
      return true;
    }
    if (isLast) {
      result = emitExpr(stmt, state, error);
      if (!error.empty()) {
        return false;
      }
      appendIndented(out, result.prelude, indent);
      result.prelude.clear();
      return true;
    }
    if (!emitStatement(stmt, state, out, error, indent)) {
      return false;
    }
  }
  error = "glsl backend requires block to yield a value";
  return false;
}

bool emitStatement(const Expr &stmt, EmitState &state, std::string &out, std::string &error, const std::string &indent) {
  if (stmt.isBinding) {
    bool isMutable = false;
    bool isStatic = false;
    for (const auto &transform : stmt.transforms) {
      if (transform.name == "mut" && transform.templateArgs.empty() && transform.arguments.empty()) {
        isMutable = true;
      }
      if (transform.name == "static" && transform.templateArgs.empty() && transform.arguments.empty()) {
        isStatic = true;
      }
    }
    if (isStatic) {
      error = "glsl backend does not support static bindings";
      return false;
    }
    if (stmt.args.empty()) {
      error = "glsl backend requires binding initializers";
      return false;
    }
    std::string explicitType;
    bool hasExplicitType = getExplicitBindingTypeName(stmt, explicitType);
    const Expr &initExpr = stmt.args.front();
    if (isSimpleCallName(initExpr, "block") && initExpr.hasBodyArguments) {
      if (!initExpr.args.empty() || !initExpr.templateArgs.empty() || hasNamedArguments(initExpr.argNames)) {
        error = "glsl backend requires block() { ... }";
        return false;
      }
      EmitState blockState = state;
      std::string blockBody;
      ExprResult blockResult;
      if (!emitValueBlock(initExpr, blockState, blockBody, error, indent + "  ", blockResult)) {
        return false;
      }
      GlslType bindingType = blockResult.type;
      if (hasExplicitType) {
        bindingType = glslTypeFromName(explicitType, state, error);
        if (!error.empty()) {
          return false;
        }
        blockResult = castExpr(blockResult, bindingType);
      }
      std::string typeName = glslTypeName(bindingType);
      if (typeName.empty()) {
        error = "glsl backend requires numeric or boolean binding types";
        return false;
      }
      state.needsInt64Ext = state.needsInt64Ext || blockState.needsInt64Ext;
      state.needsFp64Ext = state.needsFp64Ext || blockState.needsFp64Ext;
      state.needsIntPow = state.needsIntPow || blockState.needsIntPow;
      state.needsUIntPow = state.needsUIntPow || blockState.needsUIntPow;
      state.needsInt64Pow = state.needsInt64Pow || blockState.needsInt64Pow;
      state.needsUInt64Pow = state.needsUInt64Pow || blockState.needsUInt64Pow;
      state.tempIndex = blockState.tempIndex;
      state.locals[stmt.name] = {bindingType, isMutable};
      // GLSL const requires an initializer, so block initializers emit a scoped assignment.
      out += indent + typeName + " " + stmt.name + ";\n";
      out += indent + "{\n";
      out += blockBody;
      out += indent + "  " + stmt.name + " = " + blockResult.code + ";\n";
      out += indent + "}\n";
      return true;
    }
    ExprResult init = emitExpr(initExpr, state, error);
    if (!error.empty()) {
      return false;
    }
    appendIndented(out, init.prelude, indent);
    GlslType bindingType = init.type;
    if (hasExplicitType) {
      bindingType = glslTypeFromName(explicitType, state, error);
      if (!error.empty()) {
        return false;
      }
      init = castExpr(init, bindingType);
    }
    std::string typeName = glslTypeName(bindingType);
    if (typeName.empty()) {
      error = "glsl backend requires numeric or boolean binding types";
      return false;
    }
    state.locals[stmt.name] = {bindingType, isMutable};
    out += indent;
    if (!isMutable) {
      out += "const ";
    }
    out += typeName + " " + stmt.name + " = " + init.code + ";\n";
    return true;
  }
  if (stmt.kind == Expr::Kind::Call && !stmt.isBinding) {
    if (isSimpleCallName(stmt, "return")) {
      if (!stmt.args.empty()) {
        error = "glsl backend requires entry definition to return void";
        return false;
      }
      out += indent + "return;\n";
      return true;
    }
    if (isSimpleCallName(stmt, "assign")) {
      if (stmt.args.size() != 2) {
        error = "glsl backend requires assign(target, value)";
        return false;
      }
      const Expr &target = stmt.args.front();
      if (target.kind != Expr::Kind::Name) {
        error = "glsl backend requires assign target to be a local name";
        return false;
      }
      auto it = state.locals.find(target.name);
      if (it == state.locals.end()) {
        error = "glsl backend requires local binding for assign target";
        return false;
      }
      if (!it->second.isMutable) {
        error = "glsl backend requires assign target to be mutable";
        return false;
      }
      ExprResult value = emitExpr(stmt.args[1], state, error);
      if (!error.empty()) {
        return false;
      }
      value = castExpr(value, it->second.type);
      appendIndented(out, value.prelude, indent);
      out += indent + target.name + " = " + value.code + ";\n";
      return true;
    }
    if (isSimpleCallName(stmt, "increment") || isSimpleCallName(stmt, "decrement")) {
      if (stmt.args.size() != 1 || stmt.args.front().kind != Expr::Kind::Name) {
        error = "glsl backend requires increment/decrement target to be a local name";
        return false;
      }
      const std::string &targetName = stmt.args.front().name;
      auto it = state.locals.find(targetName);
      if (it == state.locals.end()) {
        error = "glsl backend requires local binding for increment/decrement target";
        return false;
      }
      if (!it->second.isMutable) {
        error = "glsl backend requires increment/decrement target to be mutable";
        return false;
      }
      out += indent + targetName + (isSimpleCallName(stmt, "increment") ? "++" : "--") + ";\n";
      return true;
    }
    if (isSimpleCallName(stmt, "block")) {
      if (!stmt.args.empty() || !stmt.templateArgs.empty() || hasNamedArguments(stmt.argNames)) {
        error = "glsl backend requires block() { ... }";
        return false;
      }
      out += indent + "{\n";
      if (!emitBlock(stmt.bodyArguments, state, out, error, indent + "  ")) {
        return false;
      }
      out += indent + "}\n";
      return true;
    }
    if (isSimpleCallName(stmt, "notify") || isSimpleCallName(stmt, "insert") || isSimpleCallName(stmt, "take")) {
      const char *name = stmt.name.c_str();
      const size_t expectedArgs = isSimpleCallName(stmt, "take") ? 1 : 2;
      if (hasNamedArguments(stmt.argNames)) {
        error = "glsl backend requires " + std::string(name) + " to use positional arguments";
        return false;
      }
      if (!stmt.templateArgs.empty()) {
        error = "glsl backend does not support template arguments on " + std::string(name);
        return false;
      }
      if (stmt.hasBodyArguments || !stmt.bodyArguments.empty()) {
        error = "glsl backend does not support block arguments on " + std::string(name);
        return false;
      }
      if (stmt.args.size() != expectedArgs) {
        error = "glsl backend requires " + std::string(name) + " with " + std::to_string(expectedArgs) +
                " argument" + (expectedArgs == 1 ? "" : "s");
        return false;
      }
      if (!stmt.args.empty()) {
        const Expr &pathExpr = stmt.args.front();
        if (pathExpr.kind != Expr::Kind::StringLiteral) {
          error = "glsl backend requires string literal path for " + std::string(name);
          return false;
        }
      }
      for (size_t i = 0; i < stmt.args.size(); ++i) {
        if (i == 0 && stmt.args[i].kind == Expr::Kind::StringLiteral) {
          continue;
        }
        ExprResult arg = emitExpr(stmt.args[i], state, error);
        if (!error.empty()) {
          return false;
        }
        appendIndented(out, arg.prelude, indent);
        out += indent + arg.code + ";\n";
      }
      return true;
    }
    if (isSimpleCallName(stmt, "print") || isSimpleCallName(stmt, "print_line") || isSimpleCallName(stmt, "print_error") ||
        isSimpleCallName(stmt, "print_line_error")) {
      const std::string name = normalizeName(stmt);
      if (hasNamedArguments(stmt.argNames)) {
        error = "glsl backend requires " + name + " to use positional arguments";
        return false;
      }
      if (!stmt.templateArgs.empty()) {
        error = "glsl backend does not support template arguments on " + name;
        return false;
      }
      if (stmt.hasBodyArguments || !stmt.bodyArguments.empty()) {
        error = "glsl backend does not support block arguments on " + name;
        return false;
      }
      if (stmt.args.size() != 1) {
        error = "glsl backend requires " + name + " with exactly one argument";
        return false;
      }
      const Expr &argExpr = stmt.args.front();
      if (argExpr.kind == Expr::Kind::StringLiteral) {
        return true;
      }
      ExprResult arg = emitExpr(argExpr, state, error);
      if (!error.empty()) {
        return false;
      }
      appendIndented(out, arg.prelude, indent);
      out += indent + arg.code + ";\n";
      return true;
    }
    if (isSimpleCallName(stmt, "if")) {
      if (stmt.args.size() < 2 || stmt.args.size() > 3) {
        error = "glsl backend requires if(cond, then() { ... }, else() { ... })";
        return false;
      }
      ExprResult cond = emitExpr(stmt.args[0], state, error);
      if (!error.empty()) {
        return false;
      }
      if (cond.type != GlslType::Bool) {
        error = "glsl backend requires if condition to be bool";
        return false;
      }
      appendIndented(out, cond.prelude, indent);
      const Expr &thenExpr = stmt.args[1];
      auto isIfBlockEnvelope = [&](const Expr &candidate) -> bool {
        if (candidate.kind != Expr::Kind::Call || candidate.isBinding || candidate.isMethodCall) {
          return false;
        }
        if (!candidate.args.empty() || !candidate.templateArgs.empty()) {
          return false;
        }
        if (!candidate.hasBodyArguments && candidate.bodyArguments.empty()) {
          return false;
        }
        return true;
      };
      if (!isIfBlockEnvelope(thenExpr)) {
        error = "glsl backend requires then() { ... } block";
        return false;
      }
      const Expr *elseExpr = nullptr;
      if (stmt.args.size() == 3) {
        elseExpr = &stmt.args[2];
        if (!isIfBlockEnvelope(*elseExpr)) {
          error = "glsl backend requires else() { ... } block";
          return false;
        }
      }
      out += indent + "if (" + cond.code + ") {\n";
      if (!emitBlock(thenExpr.bodyArguments, state, out, error, indent + "  ")) {
        return false;
      }
      out += indent + "}";
      if (elseExpr) {
        out += " else {\n";
        if (!emitBlock(elseExpr->bodyArguments, state, out, error, indent + "  ")) {
          return false;
        }
        out += indent + "}";
      }
      out += "\n";
      return true;
    }
    auto isLoopBlockEnvelope = [&](const Expr &candidate) -> bool {
      if (candidate.kind != Expr::Kind::Call || candidate.isBinding || candidate.isMethodCall) {
        return false;
      }
      if (!candidate.args.empty() || !candidate.templateArgs.empty()) {
        return false;
      }
      if (!candidate.hasBodyArguments && candidate.bodyArguments.empty()) {
        return false;
      }
      return true;
    };
    if (isSimpleCallName(stmt, "loop")) {
      if (stmt.args.size() != 2) {
        error = "glsl backend requires loop(count, body() { ... })";
        return false;
      }
      const Expr &countExpr = stmt.args[0];
      const Expr &bodyExpr = stmt.args[1];
      if (!isLoopBlockEnvelope(bodyExpr)) {
        error = "glsl backend requires loop body block";
        return false;
      }
      ExprResult count = emitExpr(countExpr, state, error);
      if (!error.empty()) {
        return false;
      }
      if (!isIntegerType(count.type)) {
        error = "glsl backend requires loop count to be integer";
        return false;
      }
      noteTypeExtensions(count.type, state);
      appendIndented(out, count.prelude, indent);
      const std::string counterName = allocTempName(state, "_ps_loop_");
      const std::string counterType = glslTypeName(count.type);
      const std::string zeroLiteral = literalForType(count.type, 0);
      const std::string oneLiteral = literalForType(count.type, 1);
      const std::string cond =
          isUnsignedInteger(count.type) ? (counterName + " != " + zeroLiteral)
                                         : (counterName + " > " + zeroLiteral);

      auto savedLocals = state.locals;
      state.locals[counterName] = {count.type, true};
      out += indent + "{\n";
      out += indent + "  " + counterType + " " + counterName + " = " + count.code + ";\n";
      out += indent + "  while (" + cond + ") {\n";
      if (!emitBlock(bodyExpr.bodyArguments, state, out, error, indent + "    ")) {
        state.locals = savedLocals;
        return false;
      }
      out += indent + "    " + counterName + " = " + counterName + " - " + oneLiteral + ";\n";
      out += indent + "  }\n";
      out += indent + "}\n";
      state.locals = savedLocals;
      return true;
    }
    if (isSimpleCallName(stmt, "repeat")) {
      if (stmt.args.size() != 1 || !stmt.hasBodyArguments) {
        error = "glsl backend requires repeat(count) { ... }";
        return false;
      }
      ExprResult count = emitExpr(stmt.args[0], state, error);
      if (!error.empty()) {
        return false;
      }
      GlslType counterType = count.type;
      if (counterType == GlslType::Bool) {
        counterType = GlslType::Int;
      }
      if (!isIntegerType(counterType)) {
        error = "glsl backend requires repeat count to be integer or bool";
        return false;
      }
      noteTypeExtensions(counterType, state);
      appendIndented(out, count.prelude, indent);
      ExprResult countCast = castExpr(count, counterType);
      const std::string counterName = allocTempName(state, "_ps_repeat_");
      const std::string counterTypeName = glslTypeName(counterType);
      const std::string zeroLiteral = literalForType(counterType, 0);
      const std::string oneLiteral = literalForType(counterType, 1);
      const std::string cond =
          isUnsignedInteger(counterType) ? (counterName + " != " + zeroLiteral)
                                          : (counterName + " > " + zeroLiteral);

      auto savedLocals = state.locals;
      state.locals[counterName] = {counterType, true};
      out += indent + "{\n";
      out += indent + "  " + counterTypeName + " " + counterName + " = " + countCast.code + ";\n";
      out += indent + "  while (" + cond + ") {\n";
      if (!emitBlock(stmt.bodyArguments, state, out, error, indent + "    ")) {
        state.locals = savedLocals;
        return false;
      }
      out += indent + "    " + counterName + " = " + counterName + " - " + oneLiteral + ";\n";
      out += indent + "  }\n";
      out += indent + "}\n";
      state.locals = savedLocals;
      return true;
    }
    if (isSimpleCallName(stmt, "while")) {
      if (stmt.args.size() != 2) {
        error = "glsl backend requires while(cond, body() { ... })";
        return false;
      }
      ExprResult cond = emitExpr(stmt.args[0], state, error);
      if (!error.empty()) {
        return false;
      }
      if (cond.type != GlslType::Bool) {
        error = "glsl backend requires while condition to be bool";
        return false;
      }
      const Expr &bodyExpr = stmt.args[1];
      if (!isLoopBlockEnvelope(bodyExpr)) {
        error = "glsl backend requires while body block";
        return false;
      }
      if (cond.prelude.empty()) {
        out += indent + "while (" + cond.code + ") {\n";
        if (!emitBlock(bodyExpr.bodyArguments, state, out, error, indent + "  ")) {
          return false;
        }
        out += indent + "}\n";
        return true;
      }
      out += indent + "while (true) {\n";
      appendIndented(out, cond.prelude, indent + "  ");
      out += indent + "  if (!(" + cond.code + ")) { break; }\n";
      if (!emitBlock(bodyExpr.bodyArguments, state, out, error, indent + "  ")) {
        return false;
      }
      out += indent + "}\n";
      return true;
    }
    if (isSimpleCallName(stmt, "for")) {
      if (stmt.args.size() != 4) {
        error = "glsl backend requires for(init, cond, step, body() { ... })";
        return false;
      }
      auto savedLocals = state.locals;
      out += indent + "{\n";
      if (!emitStatement(stmt.args[0], state, out, error, indent + "  ")) {
        state.locals = savedLocals;
        return false;
      }
      const Expr &condExpr = stmt.args[1];
      const Expr &stepExpr = stmt.args[2];
      const Expr &bodyExpr = stmt.args[3];
      if (!isLoopBlockEnvelope(bodyExpr)) {
        error = "glsl backend requires for body block";
        state.locals = savedLocals;
        return false;
      }
      if (condExpr.isBinding) {
        out += indent + "  while (true) {\n";
        if (!emitStatement(condExpr, state, out, error, indent + "    ")) {
          state.locals = savedLocals;
          return false;
        }
        auto condIt = state.locals.find(condExpr.name);
        if (condIt == state.locals.end() || condIt->second.type != GlslType::Bool) {
          error = "glsl backend requires for condition to be bool";
          state.locals = savedLocals;
          return false;
        }
        out += indent + "    if (!" + condExpr.name + ") { break; }\n";
        if (!emitBlock(bodyExpr.bodyArguments, state, out, error, indent + "    ")) {
          state.locals = savedLocals;
          return false;
        }
        if (!emitStatement(stepExpr, state, out, error, indent + "    ")) {
          state.locals = savedLocals;
          return false;
        }
        out += indent + "  }\n";
      } else {
        ExprResult cond = emitExpr(condExpr, state, error);
        if (!error.empty()) {
          state.locals = savedLocals;
          return false;
        }
        if (cond.type != GlslType::Bool) {
          error = "glsl backend requires for condition to be bool";
          state.locals = savedLocals;
          return false;
        }
        if (cond.prelude.empty()) {
          out += indent + "  while (" + cond.code + ") {\n";
          if (!emitBlock(bodyExpr.bodyArguments, state, out, error, indent + "    ")) {
            state.locals = savedLocals;
            return false;
          }
          if (!emitStatement(stepExpr, state, out, error, indent + "    ")) {
            state.locals = savedLocals;
            return false;
          }
          out += indent + "  }\n";
        } else {
          out += indent + "  while (true) {\n";
          appendIndented(out, cond.prelude, indent + "    ");
          out += indent + "    if (!(" + cond.code + ")) { break; }\n";
          if (!emitBlock(bodyExpr.bodyArguments, state, out, error, indent + "    ")) {
            state.locals = savedLocals;
            return false;
          }
          if (!emitStatement(stepExpr, state, out, error, indent + "    ")) {
            state.locals = savedLocals;
            return false;
          }
          out += indent + "  }\n";
        }
      }
      out += indent + "}\n";
      state.locals = savedLocals;
      return true;
    }
    ExprResult exprResult = emitExpr(stmt, state, error);
    if (!error.empty()) {
      return false;
    }
    appendIndented(out, exprResult.prelude, indent);
    out += indent + exprResult.code + ";\n";
    return true;
  }
  error = "glsl backend encountered unsupported statement";
  return false;
}

} // namespace

bool GlslEmitter::emitSource(const Program &program,
                             const std::string &entryPath,
                             std::string &out,
                             std::string &error) const {
  const Definition *entryDef = nullptr;
  for (const auto &def : program.definitions) {
    if (def.fullPath == entryPath) {
      entryDef = &def;
      break;
    }
  }
  if (!entryDef) {
    error = "glsl backend requires entry definition " + entryPath;
    return false;
  }
  for (const auto &def : program.definitions) {
    if (!rejectEffectTransforms(def.transforms, def.fullPath, error)) {
      return false;
    }
    for (const auto &param : def.parameters) {
      if (!rejectEffectTransforms(param, def.fullPath, error)) {
        return false;
      }
    }
    for (const auto &stmt : def.statements) {
      if (!rejectEffectTransforms(stmt, def.fullPath, error)) {
        return false;
      }
    }
    if (def.returnExpr.has_value()) {
      if (!rejectEffectTransforms(*def.returnExpr, def.fullPath, error)) {
        return false;
      }
    }
  }
  for (const auto &exec : program.executions) {
    if (!rejectEffectTransforms(exec.transforms, exec.fullPath, error)) {
      return false;
    }
    for (const auto &arg : exec.arguments) {
      if (!rejectEffectTransforms(arg, exec.fullPath, error)) {
        return false;
      }
    }
    for (const auto &bodyArg : exec.bodyArguments) {
      if (!rejectEffectTransforms(bodyArg, exec.fullPath, error)) {
        return false;
      }
    }
  }
  if (!entryDef->parameters.empty()) {
    if (entryDef->parameters.size() != 1 || !isEntryArgsParam(entryDef->parameters.front())) {
      error = "glsl backend requires entry definition to have no parameters or a single array<string> parameter";
      return false;
    }
  }
  bool hasReturnTransform = false;
  bool returnsVoid = false;
  for (const auto &transform : entryDef->transforms) {
    if (transform.name != "return") {
      continue;
    }
    hasReturnTransform = true;
    returnsVoid = transform.templateArgs.size() == 1 && transform.templateArgs.front() == "void";
  }
  if (hasReturnTransform && !returnsVoid) {
    error = "glsl backend requires entry definition to return void";
    return false;
  }
  if (!hasReturnTransform && entryDef->returnExpr.has_value()) {
    error = "glsl backend requires entry definition to return void";
    return false;
  }
  EmitState state;
  std::string body;
  bool sawReturn = false;
  for (const auto &stmt : entryDef->statements) {
    if (stmt.kind == Expr::Kind::Call && !stmt.isBinding && stmt.name == "return") {
      if (sawReturn) {
        error = "glsl backend only supports a single return statement";
        return false;
      }
      sawReturn = true;
    }
    if (!emitStatement(stmt, state, body, error, "  ")) {
      return false;
    }
  }
  out.clear();
  out += "#version 450\n";
  if (state.needsInt64Ext) {
    out += "#extension GL_ARB_gpu_shader_int64 : require\n";
  }
  if (state.needsFp64Ext) {
    out += "#extension GL_ARB_gpu_shader_fp64 : require\n";
  }
  if (state.needsIntPow) {
    out += emitPowHelper(GlslType::Int, true);
  }
  if (state.needsUIntPow) {
    out += emitPowHelper(GlslType::UInt, false);
  }
  if (state.needsInt64Pow) {
    out += emitPowHelper(GlslType::Int64, true);
  }
  if (state.needsUInt64Pow) {
    out += emitPowHelper(GlslType::UInt64, false);
  }
  out += "void main() {\n";
  out += body;
  out += "}\n";
  return true;
}

} // namespace primec
