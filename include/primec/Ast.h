#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace primec {

enum class TransformPhase { Auto, Text, Semantic };

struct Transform {
  std::string name;
  std::vector<std::string> templateArgs = {};
  std::vector<std::string> arguments = {};
  TransformPhase phase = TransformPhase::Auto;
  std::string resolvedPath = {};
  bool isAstTransformHook = false;
};

struct Expr {
  enum class Kind { Literal, BoolLiteral, FloatLiteral, StringLiteral, Call, Name } kind = Kind::Literal;
  uint64_t literalValue = 0;
  int intWidth = 32;
  bool isUnsigned = false;
  bool boolValue = false;
  std::string floatValue;
  int floatWidth = 32;
  std::string stringValue;
  std::string name;
  std::vector<Expr> args;
  std::vector<std::optional<std::string>> argNames;
  std::vector<Expr> bodyArguments;
  // True when `{ ... }` was present in the source, even if the list is empty.
  bool hasBodyArguments = false;
  std::vector<std::string> templateArgs;
  std::string namespacePrefix;
  std::vector<Transform> transforms;
  bool isBinding = false;
  bool isBraceConstructor = false;
  bool isMethodCall = false;
  bool isFieldAccess = false;
  bool isLambda = false;
  std::vector<std::string> lambdaCaptures;
  bool isSpread = false;
  int sourceLine = 0;
  int sourceColumn = 0;
  uint64_t semanticNodeId = 0;
};

struct Definition {
  std::string name;
  std::string fullPath;
  std::string namespacePrefix;
  std::vector<Transform> transforms;
  std::vector<std::string> templateArgs;
  std::vector<Expr> parameters;
  std::vector<Expr> statements;
  std::optional<Expr> returnExpr;
  bool hasReturnStatement = false;
  bool isNested = false;
  int sourceLine = 0;
  int sourceColumn = 0;
  uint64_t semanticNodeId = 0;
};

struct Execution {
  std::string name;
  std::string fullPath;
  std::string namespacePrefix;
  std::vector<Transform> transforms;
  std::vector<std::string> templateArgs;
  std::vector<Expr> arguments;
  std::vector<std::optional<std::string>> argumentNames;
  std::vector<Expr> bodyArguments;
  // True when `{ ... }` was present in the source, even if the list is empty.
  bool hasBodyArguments = false;
  int sourceLine = 0;
  int sourceColumn = 0;
  uint64_t semanticNodeId = 0;
};

struct Program {
  std::vector<std::string> sourceImports;
  std::vector<std::string> imports;
  std::vector<Definition> definitions;
  std::vector<Execution> executions;
};

} // namespace primec
