#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace primec {

enum class TransformPhase { Auto, Text, Semantic };

enum class TemplateArgumentKind { Type, Integer };

struct TemplateArgument {
  TemplateArgumentKind kind = TemplateArgumentKind::Type;
  std::string text;
  uint64_t integerValue = 0;

  static TemplateArgument type(std::string value) {
    TemplateArgument arg;
    arg.kind = TemplateArgumentKind::Type;
    arg.text = std::move(value);
    return arg;
  }

  static TemplateArgument integer(std::string value, uint64_t parsedValue) {
    TemplateArgument arg;
    arg.kind = TemplateArgumentKind::Integer;
    arg.text = std::move(value);
    arg.integerValue = parsedValue;
    return arg;
  }
};

inline TemplateArgument templateArgumentAt(const std::vector<std::string> &texts,
                                           const std::vector<TemplateArgument> &details,
                                           size_t index) {
  if (index < details.size()) {
    return details[index];
  }
  if (index < texts.size()) {
    return TemplateArgument::type(texts[index]);
  }
  return {};
}

struct Transform {
  std::string name;
  std::vector<std::string> templateArgs = {};
  std::vector<TemplateArgument> templateArgDetails = {};
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
  // Original parsed callee/member spelling, retained for diagnostics after
  // semantic rewrites specialize or canonicalize name.
  std::string sourceName;
  bool sourceIsMethodCall = false;
  std::vector<Expr> args;
  std::vector<std::optional<std::string>> argNames;
  std::vector<Expr> bodyArguments;
  // True when `{ ... }` was present in the source, even if the list is empty.
  bool hasBodyArguments = false;
  std::vector<std::string> templateArgs;
  std::vector<TemplateArgument> templateArgDetails;
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

struct SumVariant {
  std::string name;
  bool hasPayload = true;
  std::string payloadType;
  std::vector<std::string> payloadTemplateArgs;
  std::string payloadTypeText;
  std::size_t variantIndex = 0;
  int sourceLine = 0;
  int sourceColumn = 0;
  uint64_t semanticNodeId = 0;
};

struct TemplatePackBinding {
  std::string parameterName;
  std::vector<std::string> arguments;
};

struct Definition {
  std::string name;
  std::string fullPath;
  std::string namespacePrefix;
  std::vector<Transform> transforms;
  std::vector<std::string> templateArgs;
  std::vector<bool> templateArgIsPack;
  std::vector<TemplatePackBinding> templatePackBindings;
  std::vector<Expr> parameters;
  std::vector<Expr> statements;
  std::vector<SumVariant> sumVariants;
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
  std::vector<TemplateArgument> templateArgDetails;
  std::vector<bool> templateArgIsPack;
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
