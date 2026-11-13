#include "SemanticsAstBuilders.h"

#include "primec/support/TypeText.h"

#include <optional>
#include <utility>

namespace primec::semantics {
namespace {

// Splits `Base<A, B>` into the base name and its top-level template arguments; any other spelling is the name.
void splitTypeSpelling(const std::string &typeName, std::string &nameOut, std::vector<std::string> &templateArgsOut) {
  std::string typeBase;
  std::string typeArgText;
  if (splitTemplateTypeName(typeName, typeBase, typeArgText) && !typeBase.empty() && !typeArgText.empty()) {
    nameOut = typeBase;
    std::vector<std::string> typeArgs;
    if (splitTopLevelTemplateArgs(typeArgText, typeArgs)) {
      templateArgsOut = std::move(typeArgs);
    } else {
      templateArgsOut.push_back(typeArgText);
    }
  } else {
    nameOut = typeName;
  }
}

std::string escapeStringLiteral(const std::string &text) {
  std::string escaped;
  escaped.reserve(text.size() + 8);
  escaped.push_back('"');
  for (char c : text) {
    if (c == '"' || c == '\\') {
      escaped.push_back('\\');
    }
    escaped.push_back(c);
  }
  escaped += "\"utf8";
  return escaped;
}

void appendPublicTransform(std::vector<Transform> &transforms) {
  Transform visibilityTransform;
  visibilityTransform.name = "public";
  transforms.push_back(std::move(visibilityTransform));
}

} // namespace

void appendPublicVisibility(Definition &helper) {
  appendPublicTransform(helper.transforms);
}

void appendPublicVisibility(Expr &binding) {
  appendPublicTransform(binding.transforms);
}

Expr makeTypeBinding(const std::string &bindingName,
                     const std::string &typeName,
                     const std::string &namespacePrefix,
                     bool isMutable) {
  Expr binding;
  binding.isBinding = true;
  binding.name = bindingName;
  binding.namespacePrefix = namespacePrefix;
  Transform typeTransform;
  splitTypeSpelling(typeName, typeTransform.name, typeTransform.templateArgs);
  binding.transforms.push_back(std::move(typeTransform));
  if (isMutable) {
    Transform mutTransform;
    mutTransform.name = "mut";
    binding.transforms.push_back(std::move(mutTransform));
  }
  return binding;
}

Expr makeNameExpr(const std::string &name) {
  Expr expr;
  expr.kind = Expr::Kind::Name;
  expr.name = name;
  return expr;
}

Expr makeBoolLiteralExpr(bool value) {
  Expr expr;
  expr.kind = Expr::Kind::BoolLiteral;
  expr.boolValue = value;
  return expr;
}

Expr makeI32LiteralExpr(uint64_t value) {
  Expr expr;
  expr.kind = Expr::Kind::Literal;
  expr.literalValue = value;
  expr.intWidth = 32;
  expr.isUnsigned = false;
  return expr;
}

Expr makeU64LiteralExpr(uint64_t value) {
  Expr expr;
  expr.kind = Expr::Kind::Literal;
  expr.literalValue = value;
  expr.intWidth = 64;
  expr.isUnsigned = true;
  return expr;
}

Expr makeStringLiteralExpr(const std::string &value) {
  Expr expr;
  expr.kind = Expr::Kind::StringLiteral;
  expr.stringValue = escapeStringLiteral(value);
  return expr;
}

Expr makeReturnStatementExpr(Expr valueExpr) {
  Expr returnCall;
  returnCall.kind = Expr::Kind::Call;
  returnCall.name = "return";
  returnCall.args.push_back(std::move(valueExpr));
  returnCall.argNames.push_back(std::nullopt);
  return returnCall;
}

Expr makeBinaryCallExpr(const std::string &name, Expr left, Expr right) {
  Expr call;
  call.kind = Expr::Kind::Call;
  call.name = name;
  call.args.push_back(std::move(left));
  call.argNames.push_back(std::nullopt);
  call.args.push_back(std::move(right));
  call.argNames.push_back(std::nullopt);
  return call;
}

Expr makeBraceConstructorCall(const std::string &typeName) {
  Expr call;
  call.kind = Expr::Kind::Call;
  call.isBraceConstructor = true;
  splitTypeSpelling(typeName, call.name, call.templateArgs);
  return call;
}

Expr makeFieldAccessExpr(const std::string &receiverName, const std::string &fieldName) {
  Expr fieldAccess;
  fieldAccess.kind = Expr::Kind::Call;
  fieldAccess.isFieldAccess = true;
  fieldAccess.name = fieldName;
  fieldAccess.args.push_back(makeNameExpr(receiverName));
  fieldAccess.argNames.push_back(std::nullopt);
  return fieldAccess;
}

Expr makeFieldComparisonExpr(const std::string &comparisonName,
                             const std::string &leftReceiverName,
                             const std::string &rightReceiverName,
                             const std::string &fieldName) {
  return makeBinaryCallExpr(comparisonName,
                            makeFieldAccessExpr(leftReceiverName, fieldName),
                            makeFieldAccessExpr(rightReceiverName, fieldName));
}

Expr makeEnvelopeExpr(const std::string &name, std::vector<Expr> bodyArguments) {
  Expr envelope;
  envelope.kind = Expr::Kind::Call;
  envelope.name = name;
  envelope.hasBodyArguments = true;
  envelope.bodyArguments = std::move(bodyArguments);
  return envelope;
}

Expr makeIfStatementExpr(Expr conditionExpr, Expr thenEnvelopeExpr, Expr elseEnvelopeExpr) {
  Expr ifCall;
  ifCall.kind = Expr::Kind::Call;
  ifCall.name = "if";
  ifCall.args.push_back(std::move(conditionExpr));
  ifCall.argNames.push_back(std::nullopt);
  ifCall.args.push_back(std::move(thenEnvelopeExpr));
  ifCall.argNames.push_back(std::nullopt);
  ifCall.args.push_back(std::move(elseEnvelopeExpr));
  ifCall.argNames.push_back(std::nullopt);
  return ifCall;
}

Expr makeOkResultExpr() {
  Expr okResultCall;
  okResultCall.kind = Expr::Kind::Call;
  okResultCall.isMethodCall = true;
  okResultCall.name = "ok";
  okResultCall.args.push_back(makeNameExpr("Result"));
  okResultCall.argNames.push_back(std::nullopt);
  return okResultCall;
}

} // namespace primec::semantics
