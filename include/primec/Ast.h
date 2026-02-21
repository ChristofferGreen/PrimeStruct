#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace primec {

enum class TransformPhase { Auto, Text, Semantic };

struct Transform {
  std::string name;
  std::vector<std::string> templateArgs;
  std::vector<std::string> arguments;
  TransformPhase phase = TransformPhase::Auto;
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
  bool isMethodCall = false;
  bool isFieldAccess = false;
  bool isLambda = false;
  std::vector<std::string> lambdaCaptures;
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
};

struct Program {
  std::vector<std::string> imports;
  std::vector<Definition> definitions;
  std::vector<Execution> executions;
};

} // namespace primec
