#include "SemanticsValidateReflectionGeneratedHelpersSerialization.h"

#include "SemanticsAstBuilders.h"
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
  // Backends reject non-arithmetic/comparison calls nested inside another
  // expression (e.g. `not_equal(count(payload), N)`) - bind the count to a
  // local first, matching the shape ordinary hand-written code would use.
  Expr payloadCountBinding;
  payloadCountBinding.isBinding = true;
  payloadCountBinding.name = "payloadCount";
  Transform payloadCountType;
  payloadCountType.name = "i32";
  payloadCountBinding.transforms.push_back(std::move(payloadCountType));
  payloadCountBinding.args.push_back(std::move(payloadCountExpr));
  payloadCountBinding.argNames.push_back(std::nullopt);
  helper.statements.push_back(std::move(payloadCountBinding));
  Expr sizeCheckExpr =
      makeBinaryCallExpr("not_equal",
                         makeNameExpr("payloadCount"),
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
