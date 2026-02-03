#pragma once

#include <optional>
#include <string>
#include <vector>

namespace primec {

struct Transform {
  std::string name;
  std::optional<std::string> templateArg;
};

struct Expr {
  enum class Kind { Literal, FloatLiteral, StringLiteral, Call, Name } kind = Kind::Literal;
  int literalValue = 0;
  std::string floatValue;
  int floatWidth = 32;
  std::string stringValue;
  std::string name;
  std::vector<Expr> args;
  std::vector<Expr> bodyArguments;
  std::vector<std::string> templateArgs;
  std::string namespacePrefix;
  std::vector<Transform> transforms;
  bool isBinding = false;
};

struct Definition {
  std::string name;
  std::string fullPath;
  std::string namespacePrefix;
  std::vector<Transform> transforms;
  std::vector<std::string> templateArgs;
  std::vector<std::string> parameters;
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
  std::vector<Expr> bodyArguments;
};

struct Program {
  std::vector<Definition> definitions;
  std::vector<Execution> executions;
};

} // namespace primec
