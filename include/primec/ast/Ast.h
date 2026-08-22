#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace primec {

enum class TransformPhase { Auto, Text, Semantic };

enum class TemplateArgumentKind { Type, Integer, Symbol, Unsupported };
enum class RequirementPredicateKind { Relation, PredicateCall, Unsupported };
enum class RequirementPredicateOperandKind {
  TypeFact,
  Value,
  CompileTimeSymbol,
  LiteralCompileTimeArgument,
  UnsupportedRuntimeOnlyExpression,
};
enum class RequirementPredicateEvaluationOutcome {
  Satisfied,
  Unsatisfied,
  InvalidEvaluation,
};

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

  static TemplateArgument symbol(std::string value) {
    TemplateArgument arg;
    arg.kind = TemplateArgumentKind::Symbol;
    arg.text = std::move(value);
    return arg;
  }

  static TemplateArgument unsupported(std::string value) {
    TemplateArgument arg;
    arg.kind = TemplateArgumentKind::Unsupported;
    arg.text = std::move(value);
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
  struct RequirementPredicateOperand {
    RequirementPredicateOperandKind kind =
        RequirementPredicateOperandKind::UnsupportedRuntimeOnlyExpression;
    std::string text = {};
    std::string stableHandle = {};
    int sourceLine = 0;
    int sourceColumn = 0;
  };
  struct RequirementPredicate {
    RequirementPredicateKind kind = RequirementPredicateKind::Unsupported;
    std::string predicateName = {};
    std::string relationOperator = {};
    std::string sourceText = {};
    std::vector<RequirementPredicateOperand> operands = {};
    RequirementPredicateEvaluationOutcome evaluationOutcome =
        RequirementPredicateEvaluationOutcome::InvalidEvaluation;
    std::string evaluationDiagnostic = {};
    int sourceLine = 0;
    int sourceColumn = 0;
  };
  std::vector<RequirementPredicate> requirementPredicates = {};
  TransformPhase phase = TransformPhase::Auto;
  std::string resolvedPath = {};
  bool isAstTransformHook = false;
  bool isPackExpansion = false;
  int sourceLine = 0;
  int sourceColumn = 0;
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
  // True when this call was written (or rewritten) as dot-call syntax,
  // `receiver.method(args)`, false for bare-call syntax, `method(receiver,
  // args)` - a spelling/call-form distinction, not "does this call resolve
  // to a method" (a bare-call-syntax call can still resolve to a method,
  // and vice versa). Easy to misread at a call site; see
  // SemanticsValidatorExprMethodTargetResolution.cpp's TODO-4724 history.
  bool isMethodCall = false;
  bool isFieldAccess = false;
  bool isLambda = false;
  std::vector<std::string> lambdaCaptures;
  bool isSpread = false;
  int sourceLine = 0;
  int sourceColumn = 0;
  uint64_t semanticNodeId = 0;
  // Fully-resolved internal definition path for this call, as decided once
  // by template monomorphization's resolveCalleePath/selectHelperOverloadPath.
  // Populated unconditionally for both direct and method calls, independent
  // of whether the call landed in a registered overload family. Downstream
  // consumers (validator, semantic-product publication, IR lowering) should
  // read this instead of re-deriving resolution from `name`/`__ov` string
  // conventions. Empty when this expression is not a resolvable call (e.g.
  // literals, bindings, compile-time predicate forms).
  std::string resolvedCallPath;
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
