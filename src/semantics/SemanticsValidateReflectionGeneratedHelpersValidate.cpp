#include "SemanticsValidateReflectionGeneratedHelpersValidate.h"

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

Expr makeEqualExpr(Expr leftExpr, Expr rightExpr) {
  Expr equalCall;
  equalCall.kind = Expr::Kind::Call;
  equalCall.name = "equal";
  equalCall.args.push_back(std::move(leftExpr));
  equalCall.argNames.push_back(std::nullopt);
  equalCall.args.push_back(std::move(rightExpr));
  equalCall.argNames.push_back(std::nullopt);
  return equalCall;
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

bool emitReflectionValidateHelper(ReflectionGeneratedHelperContext &context) {
  const std::string helperPath = context.def.fullPath + "/Validate";
  if (context.definitionPaths.count(helperPath) > 0) {
    context.error = "generated reflection helper already exists: " + helperPath;
    return false;
  }

  std::vector<std::string> fieldHookPaths;
  fieldHookPaths.reserve(context.fieldNames.size());
  for (const auto &fieldName : context.fieldNames) {
    const std::string hookPath = context.def.fullPath + "/ValidateField_" + fieldName;
    if (context.definitionPaths.count(hookPath) > 0) {
      context.error = "generated reflection helper already exists: " + hookPath;
      return false;
    }
    fieldHookPaths.push_back(hookPath);
  }

  for (size_t fieldIndex = 0; fieldIndex < context.fieldNames.size(); ++fieldIndex) {
    Definition hook;
    hook.name = "ValidateField_" + context.fieldNames[fieldIndex];
    hook.fullPath = fieldHookPaths[fieldIndex];
    hook.namespacePrefix = context.def.fullPath;
    hook.sourceLine = context.def.sourceLine;
    hook.sourceColumn = context.def.sourceColumn;

    appendPublicVisibility(hook);
    Transform returnTransform;
    returnTransform.name = "return";
    returnTransform.templateArgs.push_back("bool");
    hook.transforms.push_back(std::move(returnTransform));
    hook.parameters.push_back(makeTypeBinding("value", context.def.fullPath, hook.namespacePrefix));
    hook.returnExpr = makeBoolLiteralExpr(true);
    hook.hasReturnStatement = true;

    context.rewrittenDefinitions.push_back(std::move(hook));
    context.definitionPaths.insert(fieldHookPaths[fieldIndex]);
  }

  Definition helper;
  helper.name = "Validate";
  helper.fullPath = helperPath;
  helper.namespacePrefix = context.def.fullPath;
  helper.sourceLine = context.def.sourceLine;
  helper.sourceColumn = context.def.sourceColumn;

  appendPublicVisibility(helper);
  Transform returnTransform;
  returnTransform.name = "return";
  returnTransform.templateArgs.push_back("Result<FileError>");
  helper.transforms.push_back(std::move(returnTransform));
  helper.parameters.push_back(makeTypeBinding("value", context.def.fullPath, helper.namespacePrefix));

  for (size_t fieldIndex = 0; fieldIndex < fieldHookPaths.size(); ++fieldIndex) {
    Expr checkCall;
    checkCall.kind = Expr::Kind::Call;
    checkCall.name = fieldHookPaths[fieldIndex];
    checkCall.args.push_back(makeNameExpr("value"));
    checkCall.argNames.push_back(std::nullopt);

    Expr conditionExpr;
    conditionExpr.kind = Expr::Kind::Call;
    conditionExpr.name = "not";
    conditionExpr.args.push_back(std::move(checkCall));
    conditionExpr.argNames.push_back(std::nullopt);

    std::vector<Expr> thenBody;
    thenBody.push_back(makeReturnStatementExpr(makeI32LiteralExpr(fieldIndex + 1)));
    helper.statements.push_back(makeIfStatementExpr(
        std::move(conditionExpr), makeEnvelopeExpr("then", std::move(thenBody)), makeEnvelopeExpr("else", {})));
  }

  helper.returnExpr = makeOkResultExpr();
  helper.hasReturnStatement = true;

  context.rewrittenDefinitions.push_back(std::move(helper));
  context.definitionPaths.insert(helperPath);
  return true;
}

bool emitReflectionSoaSchemaHelpers(ReflectionGeneratedHelperContext &context) {
  const std::string countHelperPath = context.def.fullPath + "/SoaSchemaFieldCount";
  const std::string nameHelperPath = context.def.fullPath + "/SoaSchemaFieldName";
  const std::string typeHelperPath = context.def.fullPath + "/SoaSchemaFieldType";
  const std::string visibilityHelperPath = context.def.fullPath + "/SoaSchemaFieldVisibility";

  for (const auto *helperPath : {&countHelperPath, &nameHelperPath, &typeHelperPath, &visibilityHelperPath}) {
    if (context.definitionPaths.count(*helperPath) > 0) {
      context.error = "generated reflection helper already exists: " + *helperPath;
      return false;
    }
  }

  auto makeHelper = [&](const std::string &name,
                        const std::string &fullPath,
                        const std::string &returnType,
                        bool takesIndex) {
    Definition helper;
    helper.name = name;
    helper.fullPath = fullPath;
    helper.namespacePrefix = context.def.fullPath;
    helper.sourceLine = context.def.sourceLine;
    helper.sourceColumn = context.def.sourceColumn;
    appendPublicVisibility(helper);
    Transform returnTransform;
    returnTransform.name = "return";
    returnTransform.templateArgs.push_back(returnType);
    helper.transforms.push_back(std::move(returnTransform));
    if (takesIndex) {
      helper.parameters.push_back(makeTypeBinding("index", "i32", helper.namespacePrefix));
    }
    return helper;
  };

  Definition countHelper = makeHelper("SoaSchemaFieldCount", countHelperPath, "i32", false);
  countHelper.returnExpr = makeI32LiteralExpr(context.fieldNames.size());
  countHelper.hasReturnStatement = true;
  context.rewrittenDefinitions.push_back(std::move(countHelper));
  context.definitionPaths.insert(countHelperPath);

  auto appendIndexedStringHelper = [&](const std::string &name,
                                       const std::string &fullPath,
                                       const std::vector<std::string> &values) {
    Definition helper = makeHelper(name, fullPath, "string", true);
    for (size_t fieldIndex = 0; fieldIndex < values.size(); ++fieldIndex) {
      std::vector<Expr> thenBody;
      thenBody.push_back(makeReturnStatementExpr(makeStringLiteralExpr(values[fieldIndex])));
      helper.statements.push_back(makeIfStatementExpr(
          makeEqualExpr(makeNameExpr("index"), makeI32LiteralExpr(fieldIndex)),
          makeEnvelopeExpr("then", std::move(thenBody)),
          makeEnvelopeExpr("else", {})));
    }
    helper.returnExpr = makeStringLiteralExpr("");
    helper.hasReturnStatement = true;
    context.rewrittenDefinitions.push_back(std::move(helper));
    context.definitionPaths.insert(fullPath);
  };

  std::vector<std::string> fieldTypes;
  std::vector<std::string> fieldVisibilities;
  fieldTypes.reserve(context.fieldNames.size());
  fieldVisibilities.reserve(context.fieldNames.size());
  for (const auto &fieldName : context.fieldNames) {
    const auto typeIt = context.fieldTypeNames.find(fieldName);
    fieldTypes.push_back(typeIt == context.fieldTypeNames.end() ? "" : typeIt->second);
    const auto visibilityIt = context.fieldVisibilityNames.find(fieldName);
    fieldVisibilities.push_back(visibilityIt == context.fieldVisibilityNames.end() ? "public" : visibilityIt->second);
  }

  appendIndexedStringHelper("SoaSchemaFieldName", nameHelperPath, context.fieldNames);
  appendIndexedStringHelper("SoaSchemaFieldType", typeHelperPath, fieldTypes);
  appendIndexedStringHelper("SoaSchemaFieldVisibility", visibilityHelperPath, fieldVisibilities);
  return true;
}

} // namespace primec::semantics
