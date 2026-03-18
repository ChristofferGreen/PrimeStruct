#include "SemanticsValidateReflectionGeneratedHelpersSerialization.h"

#include "SemanticsHelpers.h"

#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace primec::semantics {
namespace {

constexpr uint64_t SerializationFormatVersionTag = 1ULL;
constexpr uint64_t DeserializePayloadSizeErrorCode = 1ULL;
constexpr uint64_t DeserializeFormatVersionErrorCode = 2ULL;

void appendPublicVisibility(Definition &helper) {
  Transform visibilityTransform;
  visibilityTransform.name = "public";
  helper.transforms.push_back(std::move(visibilityTransform));
}

Expr makeTypeBinding(const std::string &bindingName,
                     const std::string &typeName,
                     const std::string &namespacePrefix,
                     bool isMutable = false) {
  Expr binding;
  binding.isBinding = true;
  binding.name = bindingName;
  binding.namespacePrefix = namespacePrefix;
  Transform typeTransform;
  std::string typeBase;
  std::string typeArgText;
  if (splitTemplateTypeName(typeName, typeBase, typeArgText) && !typeBase.empty() && !typeArgText.empty()) {
    typeTransform.name = typeBase;
    std::vector<std::string> typeArgs;
    if (splitTopLevelTemplateArgs(typeArgText, typeArgs)) {
      typeTransform.templateArgs = std::move(typeArgs);
    } else {
      typeTransform.templateArgs.push_back(typeArgText);
    }
  } else {
    typeTransform.name = typeName;
  }
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

Expr makeReturnStatementExpr(Expr valueExpr) {
  Expr returnCall;
  returnCall.kind = Expr::Kind::Call;
  returnCall.name = "return";
  returnCall.args.push_back(std::move(valueExpr));
  returnCall.argNames.push_back(std::nullopt);
  return returnCall;
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

Expr makeFieldAccessExpr(const std::string &receiverName, const std::string &fieldName) {
  Expr fieldAccess;
  fieldAccess.kind = Expr::Kind::Call;
  fieldAccess.isFieldAccess = true;
  fieldAccess.name = fieldName;
  fieldAccess.args.push_back(makeNameExpr(receiverName));
  fieldAccess.argNames.push_back(std::nullopt);
  return fieldAccess;
}

Expr makeOkResultExpr() {
  Expr okResultTypeExpr;
  okResultTypeExpr.kind = Expr::Kind::Name;
  okResultTypeExpr.name = "Result";

  Expr okResultCall;
  okResultCall.kind = Expr::Kind::Call;
  okResultCall.isMethodCall = true;
  okResultCall.name = "ok";
  okResultCall.args.push_back(std::move(okResultTypeExpr));
  okResultCall.argNames.push_back(std::nullopt);
  return okResultCall;
}

} // namespace

bool emitReflectionSerializeHelper(ReflectionGeneratedHelperContext &context) {
  const std::string helperPath = context.def.fullPath + "/Serialize";
  if (context.definitionPaths.count(helperPath) > 0) {
    context.error = "generated reflection helper already exists: " + helperPath;
    return false;
  }

  Definition helper;
  helper.name = "Serialize";
  helper.fullPath = helperPath;
  helper.namespacePrefix = context.def.fullPath;
  helper.sourceLine = context.def.sourceLine;
  helper.sourceColumn = context.def.sourceColumn;

  appendPublicVisibility(helper);
  Transform returnTransform;
  returnTransform.name = "return";
  returnTransform.templateArgs.push_back("array<u64>");
  helper.transforms.push_back(std::move(returnTransform));
  helper.parameters.push_back(makeTypeBinding("value", context.def.fullPath, helper.namespacePrefix));

  Expr payloadCall;
  payloadCall.kind = Expr::Kind::Call;
  payloadCall.name = "array";
  payloadCall.templateArgs.push_back("u64");
  payloadCall.args.push_back(makeU64LiteralExpr(SerializationFormatVersionTag));
  payloadCall.argNames.push_back(std::nullopt);
  for (const auto &fieldName : context.fieldNames) {
    Expr convertFieldExpr;
    convertFieldExpr.kind = Expr::Kind::Call;
    convertFieldExpr.name = "convert";
    convertFieldExpr.templateArgs.push_back("u64");
    convertFieldExpr.args.push_back(makeFieldAccessExpr("value", fieldName));
    convertFieldExpr.argNames.push_back(std::nullopt);
    payloadCall.args.push_back(std::move(convertFieldExpr));
    payloadCall.argNames.push_back(std::nullopt);
  }
  helper.returnExpr = std::move(payloadCall);
  helper.hasReturnStatement = true;

  context.rewrittenDefinitions.push_back(std::move(helper));
  context.definitionPaths.insert(helperPath);
  return true;
}

bool emitReflectionDeserializeHelper(ReflectionGeneratedHelperContext &context) {
  const std::string helperPath = context.def.fullPath + "/Deserialize";
  if (context.definitionPaths.count(helperPath) > 0) {
    context.error = "generated reflection helper already exists: " + helperPath;
    return false;
  }

  Definition helper;
  helper.name = "Deserialize";
  helper.fullPath = helperPath;
  helper.namespacePrefix = context.def.fullPath;
  helper.sourceLine = context.def.sourceLine;
  helper.sourceColumn = context.def.sourceColumn;

  appendPublicVisibility(helper);
  Transform returnTransform;
  returnTransform.name = "return";
  returnTransform.templateArgs.push_back("Result<FileError>");
  helper.transforms.push_back(std::move(returnTransform));
  helper.parameters.push_back(makeTypeBinding("value", context.def.fullPath, helper.namespacePrefix, true));
  helper.parameters.push_back(makeTypeBinding("payload", "array<u64>", helper.namespacePrefix));

  Expr payloadCountExpr;
  payloadCountExpr.kind = Expr::Kind::Call;
  payloadCountExpr.name = "count";
  payloadCountExpr.args.push_back(makeNameExpr("payload"));
  payloadCountExpr.argNames.push_back(std::nullopt);
  Expr sizeCheckExpr =
      makeBinaryCallExpr("not_equal",
                         std::move(payloadCountExpr),
                         makeI32LiteralExpr(static_cast<uint64_t>(context.fieldNames.size() + 1)));
  std::vector<Expr> sizeCheckBody;
  sizeCheckBody.push_back(makeReturnStatementExpr(makeI32LiteralExpr(DeserializePayloadSizeErrorCode)));
  helper.statements.push_back(makeIfStatementExpr(
      std::move(sizeCheckExpr), makeEnvelopeExpr("then", std::move(sizeCheckBody)), makeEnvelopeExpr("else", {})));

  Expr payloadVersionExpr;
  payloadVersionExpr.kind = Expr::Kind::Call;
  payloadVersionExpr.name = "at";
  payloadVersionExpr.args.push_back(makeNameExpr("payload"));
  payloadVersionExpr.argNames.push_back(std::nullopt);
  payloadVersionExpr.args.push_back(makeI32LiteralExpr(0));
  payloadVersionExpr.argNames.push_back(std::nullopt);
  Expr formatCheckExpr =
      makeBinaryCallExpr("not_equal", std::move(payloadVersionExpr), makeU64LiteralExpr(SerializationFormatVersionTag));
  std::vector<Expr> formatCheckBody;
  formatCheckBody.push_back(makeReturnStatementExpr(makeI32LiteralExpr(DeserializeFormatVersionErrorCode)));
  helper.statements.push_back(makeIfStatementExpr(
      std::move(formatCheckExpr), makeEnvelopeExpr("then", std::move(formatCheckBody)), makeEnvelopeExpr("else", {})));

  for (size_t fieldIndex = 0; fieldIndex < context.fieldNames.size(); ++fieldIndex) {
    const std::string &fieldName = context.fieldNames[fieldIndex];
    std::string fieldTypeName = "int";
    const auto fieldTypeIt = context.fieldTypeNames.find(fieldName);
    if (fieldTypeIt != context.fieldTypeNames.end()) {
      fieldTypeName = fieldTypeIt->second;
    }

    Expr payloadFieldExpr;
    payloadFieldExpr.kind = Expr::Kind::Call;
    payloadFieldExpr.name = "at";
    payloadFieldExpr.args.push_back(makeNameExpr("payload"));
    payloadFieldExpr.argNames.push_back(std::nullopt);
    payloadFieldExpr.args.push_back(makeI32LiteralExpr(static_cast<uint64_t>(fieldIndex + 1)));
    payloadFieldExpr.argNames.push_back(std::nullopt);

    Expr convertFieldExpr;
    convertFieldExpr.kind = Expr::Kind::Call;
    convertFieldExpr.name = "convert";
    convertFieldExpr.templateArgs.push_back(fieldTypeName);
    convertFieldExpr.args.push_back(std::move(payloadFieldExpr));
    convertFieldExpr.argNames.push_back(std::nullopt);

    Expr assignExpr;
    assignExpr.kind = Expr::Kind::Call;
    assignExpr.name = "assign";
    assignExpr.args.push_back(makeFieldAccessExpr("value", fieldName));
    assignExpr.argNames.push_back(std::nullopt);
    assignExpr.args.push_back(std::move(convertFieldExpr));
    assignExpr.argNames.push_back(std::nullopt);
    helper.statements.push_back(std::move(assignExpr));
  }

  helper.returnExpr = makeOkResultExpr();
  helper.hasReturnStatement = true;

  context.rewrittenDefinitions.push_back(std::move(helper));
  context.definitionPaths.insert(helperPath);
  return true;
}

} // namespace primec::semantics
