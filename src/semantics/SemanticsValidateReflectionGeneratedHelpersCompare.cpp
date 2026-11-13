#include "SemanticsValidateReflectionGeneratedHelpersCompare.h"

#include "SemanticsAstBuilders.h"
#include "SemanticsHelpers.h"

#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace primec::semantics {
namespace {

} // namespace

bool emitReflectionComparisonHelper(ReflectionGeneratedHelperContext &context,
                                    const std::string &helperName,
                                    const std::string &comparisonName,
                                    const std::string &foldOperatorName,
                                    bool emptyValue) {
  const std::string helperPath = context.def.fullPath + "/" + helperName;
  if (context.definitionPaths.count(helperPath) > 0) {
    context.error = "generated reflection helper already exists: " + helperPath;
    return false;
  }

  Definition helper;
  helper.name = helperName;
  helper.fullPath = helperPath;
  helper.namespacePrefix = context.def.fullPath;
  helper.sourceLine = context.def.sourceLine;
  helper.sourceColumn = context.def.sourceColumn;

  appendPublicVisibility(helper);
  Transform returnTransform;
  returnTransform.name = "return";
  returnTransform.templateArgs.push_back("bool");
  helper.transforms.push_back(std::move(returnTransform));
  helper.parameters.push_back(makeTypeBinding("left", context.def.fullPath, helper.namespacePrefix));
  helper.parameters.push_back(makeTypeBinding("right", context.def.fullPath, helper.namespacePrefix));

  if (context.fieldNames.empty()) {
    Expr emptyLiteral;
    emptyLiteral.kind = Expr::Kind::BoolLiteral;
    emptyLiteral.boolValue = emptyValue;
    helper.returnExpr = std::move(emptyLiteral);
  } else {
    Expr combined = makeFieldComparisonExpr(comparisonName, "left", "right", context.fieldNames.front());
    for (size_t index = 1; index < context.fieldNames.size(); ++index) {
      combined = makeBinaryCallExpr(
          foldOperatorName,
          std::move(combined),
          makeFieldComparisonExpr(comparisonName, "left", "right", context.fieldNames[index]));
    }
    helper.returnExpr = std::move(combined);
  }
  helper.hasReturnStatement = true;

  context.rewrittenDefinitions.push_back(std::move(helper));
  context.definitionPaths.insert(helperPath);
  return true;
}

bool emitReflectionCompareHelper(ReflectionGeneratedHelperContext &context) {
  const std::string helperPath = context.def.fullPath + "/Compare";
  if (context.definitionPaths.count(helperPath) > 0) {
    context.error = "generated reflection helper already exists: " + helperPath;
    return false;
  }

  Definition helper;
  helper.name = "Compare";
  helper.fullPath = helperPath;
  helper.namespacePrefix = context.def.fullPath;
  helper.sourceLine = context.def.sourceLine;
  helper.sourceColumn = context.def.sourceColumn;

  appendPublicVisibility(helper);
  Transform returnTransform;
  returnTransform.name = "return";
  returnTransform.templateArgs.push_back("i32");
  helper.transforms.push_back(std::move(returnTransform));
  helper.parameters.push_back(makeTypeBinding("left", context.def.fullPath, helper.namespacePrefix));
  helper.parameters.push_back(makeTypeBinding("right", context.def.fullPath, helper.namespacePrefix));

  for (const auto &fieldName : context.fieldNames) {
    Expr lessThanExpr = makeFieldComparisonExpr("less_than", "left", "right", fieldName);
    Expr lessThanResultExpr = makeBinaryCallExpr("minus", makeI32LiteralExpr(0), makeI32LiteralExpr(1));
    Expr lessThanReturn;
    lessThanReturn.kind = Expr::Kind::Call;
    lessThanReturn.name = "return";
    lessThanReturn.args.push_back(std::move(lessThanResultExpr));
    lessThanReturn.argNames.push_back(std::nullopt);

    Expr lessThanThen;
    lessThanThen.kind = Expr::Kind::Call;
    lessThanThen.name = "then";
    lessThanThen.hasBodyArguments = true;
    lessThanThen.bodyArguments.push_back(std::move(lessThanReturn));

    Expr lessThanElse;
    lessThanElse.kind = Expr::Kind::Call;
    lessThanElse.name = "else";
    lessThanElse.hasBodyArguments = true;

    Expr lessThanIf;
    lessThanIf.kind = Expr::Kind::Call;
    lessThanIf.name = "if";
    lessThanIf.args.push_back(std::move(lessThanExpr));
    lessThanIf.argNames.push_back(std::nullopt);
    lessThanIf.args.push_back(std::move(lessThanThen));
    lessThanIf.argNames.push_back(std::nullopt);
    lessThanIf.args.push_back(std::move(lessThanElse));
    lessThanIf.argNames.push_back(std::nullopt);
    helper.statements.push_back(std::move(lessThanIf));

    Expr greaterThanExpr = makeFieldComparisonExpr("greater_than", "left", "right", fieldName);
    Expr greaterThanReturn;
    greaterThanReturn.kind = Expr::Kind::Call;
    greaterThanReturn.name = "return";
    greaterThanReturn.args.push_back(makeI32LiteralExpr(1));
    greaterThanReturn.argNames.push_back(std::nullopt);

    Expr greaterThanThen;
    greaterThanThen.kind = Expr::Kind::Call;
    greaterThanThen.name = "then";
    greaterThanThen.hasBodyArguments = true;
    greaterThanThen.bodyArguments.push_back(std::move(greaterThanReturn));

    Expr greaterThanElse;
    greaterThanElse.kind = Expr::Kind::Call;
    greaterThanElse.name = "else";
    greaterThanElse.hasBodyArguments = true;

    Expr greaterThanIf;
    greaterThanIf.kind = Expr::Kind::Call;
    greaterThanIf.name = "if";
    greaterThanIf.args.push_back(std::move(greaterThanExpr));
    greaterThanIf.argNames.push_back(std::nullopt);
    greaterThanIf.args.push_back(std::move(greaterThanThen));
    greaterThanIf.argNames.push_back(std::nullopt);
    greaterThanIf.args.push_back(std::move(greaterThanElse));
    greaterThanIf.argNames.push_back(std::nullopt);
    helper.statements.push_back(std::move(greaterThanIf));
  }

  helper.returnExpr = makeI32LiteralExpr(0);
  helper.hasReturnStatement = true;

  context.rewrittenDefinitions.push_back(std::move(helper));
  context.definitionPaths.insert(helperPath);
  return true;
}

bool emitReflectionHash64Helper(ReflectionGeneratedHelperContext &context) {
  const std::string helperPath = context.def.fullPath + "/Hash64";
  if (context.definitionPaths.count(helperPath) > 0) {
    context.error = "generated reflection helper already exists: " + helperPath;
    return false;
  }

  Definition helper;
  helper.name = "Hash64";
  helper.fullPath = helperPath;
  helper.namespacePrefix = context.def.fullPath;
  helper.sourceLine = context.def.sourceLine;
  helper.sourceColumn = context.def.sourceColumn;

  appendPublicVisibility(helper);
  Transform returnTransform;
  returnTransform.name = "return";
  returnTransform.templateArgs.push_back("u64");
  helper.transforms.push_back(std::move(returnTransform));
  helper.parameters.push_back(makeTypeBinding("value", context.def.fullPath, helper.namespacePrefix));

  Expr combined = makeU64LiteralExpr(1469598103934665603ULL);
  for (const auto &fieldName : context.fieldNames) {
    Expr convertFieldExpr;
    convertFieldExpr.kind = Expr::Kind::Call;
    convertFieldExpr.name = "convert";
    convertFieldExpr.templateArgs.push_back("u64");
    convertFieldExpr.args.push_back(makeFieldAccessExpr("value", fieldName));
    convertFieldExpr.argNames.push_back(std::nullopt);

    combined = makeBinaryCallExpr(
        "plus",
        makeBinaryCallExpr("multiply", std::move(combined), makeU64LiteralExpr(1099511628211ULL)),
        std::move(convertFieldExpr));
  }
  helper.returnExpr = std::move(combined);
  helper.hasReturnStatement = true;

  context.rewrittenDefinitions.push_back(std::move(helper));
  context.definitionPaths.insert(helperPath);
  return true;
}

} // namespace primec::semantics
