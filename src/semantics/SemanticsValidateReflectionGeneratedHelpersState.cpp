#include "SemanticsValidateReflectionGeneratedHelpersState.h"

#include "SemanticsHelpers.h"

#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace primec::semantics {
namespace {

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
    std::vector<std::string> typeArgs;
    typeTransform.name = typeBase;
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

Expr makeFieldAccessExpr(const std::string &receiverName, const std::string &fieldName) {
  Expr fieldAccess;
  fieldAccess.kind = Expr::Kind::Call;
  fieldAccess.isFieldAccess = true;
  fieldAccess.name = fieldName;
  Expr receiverExpr;
  receiverExpr.kind = Expr::Kind::Name;
  receiverExpr.name = receiverName;
  fieldAccess.args.push_back(std::move(receiverExpr));
  fieldAccess.argNames.push_back(std::nullopt);
  return fieldAccess;
}

Expr makeFieldComparisonExpr(const std::string &comparisonName,
                             const std::string &leftReceiverName,
                             const std::string &rightReceiverName,
                             const std::string &fieldName) {
  Expr compare;
  compare.kind = Expr::Kind::Call;
  compare.name = comparisonName;
  compare.args.push_back(makeFieldAccessExpr(leftReceiverName, fieldName));
  compare.argNames.push_back(std::nullopt);
  compare.args.push_back(makeFieldAccessExpr(rightReceiverName, fieldName));
  compare.argNames.push_back(std::nullopt);
  return compare;
}

Expr makeBinaryBoolExpr(const std::string &operatorName, Expr left, Expr right) {
  Expr call;
  call.kind = Expr::Kind::Call;
  call.name = operatorName;
  call.args.push_back(std::move(left));
  call.argNames.push_back(std::nullopt);
  call.args.push_back(std::move(right));
  call.argNames.push_back(std::nullopt);
  return call;
}

Expr makeBoolLiteralExpr(bool value) {
  Expr expr;
  expr.kind = Expr::Kind::BoolLiteral;
  expr.boolValue = value;
  return expr;
}

Expr makeBraceConstructorCall(const std::string &typeName) {
  Expr call;
  call.kind = Expr::Kind::Call;
  call.isBraceConstructor = true;
  call.name = typeName;
  return call;
}

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
      combined = makeBinaryBoolExpr("and", std::move(combined), std::move(compare));
    }
    helper.returnExpr = std::move(combined);
  }
  helper.hasReturnStatement = true;

  context.rewrittenDefinitions.push_back(std::move(helper));
  context.definitionPaths.insert(helperPath);
  return true;
}

} // namespace primec::semantics
