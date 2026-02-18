#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "primec/Ast.h"

namespace primec::glsl_emitter {

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

std::string normalizeName(const Expr &expr);
std::string powHelperName(GlslType type);
std::string emitPowHelper(GlslType type, bool isSigned);
void appendIndented(std::string &out, const std::string &text, const std::string &indent);
bool isSimpleCallName(const Expr &expr, const char *name);
bool getMathBuiltinName(const Expr &expr, std::string &out);
bool getMathConstantName(const Expr &expr, std::string &out);
bool hasNamedArguments(const std::vector<std::optional<std::string>> &argNames);
bool isBindingAuxTransformName(const std::string &name);
bool rejectEffectTransforms(const std::vector<Transform> &transforms, const std::string &context, std::string &error);
bool rejectEffectTransforms(const Expr &expr, const std::string &context, std::string &error);
bool getExplicitBindingTypeName(const Expr &expr, std::string &outTypeName);
bool isEntryArgsParam(const Expr &expr);
GlslType glslTypeFromName(const std::string &name, EmitState &state, std::string &error);
std::string glslTypeName(GlslType type);
bool isNumericType(GlslType type);
bool isIntegerType(GlslType type);
bool isSignedInteger(GlslType type);
bool isUnsignedInteger(GlslType type);
void noteTypeExtensions(GlslType type, EmitState &state);
std::string literalForType(GlslType type, int value);
std::string allocTempName(EmitState &state, const std::string &prefix);
GlslType combineNumericTypes(GlslType left, GlslType right, std::string &error);
std::string ensureFloatLiteral(const std::string &literal);
ExprResult castExpr(const ExprResult &expr, GlslType target);

ExprResult emitExpr(const Expr &expr, EmitState &state, std::string &error);
ExprResult emitBinaryNumeric(const Expr &leftExpr,
                             const Expr &rightExpr,
                             const char *op,
                             EmitState &state,
                             std::string &error);
bool emitValueBlock(const Expr &blockExpr,
                    EmitState &state,
                    std::string &out,
                    std::string &error,
                    const std::string &indent,
                    ExprResult &result);
bool emitStatement(const Expr &stmt, EmitState &state, std::string &out, std::string &error, const std::string &indent);
bool emitBlock(const std::vector<Expr> &stmts,
               EmitState &state,
               std::string &out,
               std::string &error,
               const std::string &indent);

} // namespace primec::glsl_emitter
