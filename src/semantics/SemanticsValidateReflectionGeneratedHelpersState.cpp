#include "SemanticsValidateReflectionGeneratedHelpersState.h"

#include "SemanticsAstBuilders.h"
#include "SemanticsHelpers.h"

#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace primec::semantics {
namespace {

} // namespace

bool emitReflectionClearHelper(ReflectionGeneratedHelperContext &context) {
  const std::string helperPath = context.def.fullPath + "/Clear";
  if (context.definitionPaths.count(helperPath) > 0) {
    context.error = "generated reflection helper already exists: " + helperPath;
    return false;
  }

  Definition helper;
  helper.name = "Clear";
  helper.fullPath = helperPath;
  helper.namespacePrefix = context.def.fullPath;
  helper.sourceLine = context.def.sourceLine;
  helper.sourceColumn = context.def.sourceColumn;

  appendPublicVisibility(helper);
  Transform returnTransform;
  returnTransform.name = "return";
  returnTransform.templateArgs.push_back("void");
  helper.transforms.push_back(std::move(returnTransform));
  helper.parameters.push_back(makeTypeBinding("value", context.def.fullPath, helper.namespacePrefix, true));

  if (!context.fieldNames.empty()) {
    Expr defaultValueBinding = makeTypeBinding("defaultValue", context.def.fullPath, helper.namespacePrefix);
    defaultValueBinding.args.push_back(makeBraceConstructorCall(context.def.fullPath));
    defaultValueBinding.argNames.push_back(std::nullopt);
    helper.statements.push_back(std::move(defaultValueBinding));

    for (const auto &fieldName : context.fieldNames) {
      Expr assignExpr;
      assignExpr.kind = Expr::Kind::Call;
      assignExpr.name = "assign";
      assignExpr.args.push_back(makeFieldAccessExpr("value", fieldName));
      assignExpr.argNames.push_back(std::nullopt);
      assignExpr.args.push_back(makeFieldAccessExpr("defaultValue", fieldName));
      assignExpr.argNames.push_back(std::nullopt);
      helper.statements.push_back(std::move(assignExpr));
    }
  }

  context.rewrittenDefinitions.push_back(std::move(helper));
  context.definitionPaths.insert(helperPath);
  return true;
}

bool emitReflectionCopyFromHelper(ReflectionGeneratedHelperContext &context) {
  const std::string helperPath = context.def.fullPath + "/CopyFrom";
  if (context.definitionPaths.count(helperPath) > 0) {
    context.error = "generated reflection helper already exists: " + helperPath;
    return false;
  }

  Definition helper;
  helper.name = "CopyFrom";
  helper.fullPath = helperPath;
  helper.namespacePrefix = context.def.fullPath;
  helper.sourceLine = context.def.sourceLine;
  helper.sourceColumn = context.def.sourceColumn;

  appendPublicVisibility(helper);
  Transform returnTransform;
  returnTransform.name = "return";
  returnTransform.templateArgs.push_back("void");
  helper.transforms.push_back(std::move(returnTransform));
  helper.parameters.push_back(makeTypeBinding("value", context.def.fullPath, helper.namespacePrefix, true));
  helper.parameters.push_back(makeTypeBinding("other", context.def.fullPath, helper.namespacePrefix));

  for (const auto &fieldName : context.fieldNames) {
    Expr assignExpr;
    assignExpr.kind = Expr::Kind::Call;
    assignExpr.name = "assign";
    assignExpr.args.push_back(makeFieldAccessExpr("value", fieldName));
    assignExpr.argNames.push_back(std::nullopt);
    assignExpr.args.push_back(makeFieldAccessExpr("other", fieldName));
    assignExpr.argNames.push_back(std::nullopt);
    helper.statements.push_back(std::move(assignExpr));
  }

  context.rewrittenDefinitions.push_back(std::move(helper));
  context.definitionPaths.insert(helperPath);
  return true;
}

bool emitReflectionDefaultHelper(ReflectionGeneratedHelperContext &context) {
  const std::string helperPath = context.def.fullPath + "/Default";
  if (context.definitionPaths.count(helperPath) > 0) {
    context.error = "generated reflection helper already exists: " + helperPath;
    return false;
  }

  Definition helper;
  helper.name = "Default";
  helper.fullPath = helperPath;
  helper.namespacePrefix = context.def.fullPath;
  helper.sourceLine = context.def.sourceLine;
  helper.sourceColumn = context.def.sourceColumn;

  appendPublicVisibility(helper);
  Transform returnTransform;
  returnTransform.name = "return";
  returnTransform.templateArgs.push_back(context.def.fullPath);
  helper.transforms.push_back(std::move(returnTransform));

  helper.returnExpr = makeBraceConstructorCall(context.def.fullPath);
  helper.hasReturnStatement = true;

  context.rewrittenDefinitions.push_back(std::move(helper));
  context.definitionPaths.insert(helperPath);
  return true;
}

bool emitReflectionIsDefaultHelper(ReflectionGeneratedHelperContext &context) {
  const std::string helperPath = context.def.fullPath + "/IsDefault";
  if (context.definitionPaths.count(helperPath) > 0) {
    context.error = "generated reflection helper already exists: " + helperPath;
    return false;
  }

  Definition helper;
  helper.name = "IsDefault";
  helper.fullPath = helperPath;
  helper.namespacePrefix = context.def.fullPath;
  helper.sourceLine = context.def.sourceLine;
  helper.sourceColumn = context.def.sourceColumn;

  appendPublicVisibility(helper);
  Transform returnTransform;
  returnTransform.name = "return";
  returnTransform.templateArgs.push_back("bool");
  helper.transforms.push_back(std::move(returnTransform));
  helper.parameters.push_back(makeTypeBinding("value", context.def.fullPath, helper.namespacePrefix));

  if (context.fieldNames.empty()) {
    helper.returnExpr = makeBoolLiteralExpr(true);
  } else {
    Expr defaultValueBinding = makeTypeBinding("defaultValue", context.def.fullPath, helper.namespacePrefix);
    defaultValueBinding.args.push_back(makeBraceConstructorCall(context.def.fullPath));
    defaultValueBinding.argNames.push_back(std::nullopt);
    helper.statements.push_back(std::move(defaultValueBinding));

    Expr combined = makeFieldComparisonExpr("equal", "value", "defaultValue", context.fieldNames.front());
    for (size_t index = 1; index < context.fieldNames.size(); ++index) {
      Expr compare = makeFieldComparisonExpr("equal", "value", "defaultValue", context.fieldNames[index]);
      combined = makeBinaryCallExpr("and", std::move(combined), std::move(compare));
    }
    helper.returnExpr = std::move(combined);
  }
  helper.hasReturnStatement = true;

  context.rewrittenDefinitions.push_back(std::move(helper));
  context.definitionPaths.insert(helperPath);
  return true;
}

} // namespace primec::semantics
