#include "SemanticsValidateReflectionGeneratedHelpersValidate.h"

#include "SemanticsHelpers.h"

#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace primec::semantics {
namespace {

std::string formatTemplateArgs(const std::vector<std::string> &args) {
  std::string out;
  for (size_t i = 0; i < args.size(); ++i) {
    if (i > 0) {
      out += ", ";
    }
    out += args[i];
  }
  return out;
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

void appendPublicVisibility(Definition &helper) {
  Transform visibilityTransform;
  visibilityTransform.name = "public";
  helper.transforms.push_back(std::move(visibilityTransform));
}

void appendPublicVisibility(Expr &binding) {
  Transform visibilityTransform;
  visibilityTransform.name = "public";
  binding.transforms.push_back(std::move(visibilityTransform));
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

Expr makeConstructorCall(const std::string &typeName) {
  Expr call;
  call.kind = Expr::Kind::Call;
  std::string typeBase;
  std::string typeArgText;
  if (splitTemplateTypeName(typeName, typeBase, typeArgText) && !typeBase.empty() && !typeArgText.empty()) {
    call.name = typeBase;
    std::vector<std::string> typeArgs;
    if (splitTopLevelTemplateArgs(typeArgText, typeArgs)) {
      call.templateArgs = std::move(typeArgs);
    } else {
      call.templateArgs.push_back(typeArgText);
    }
  } else {
    call.name = typeName;
  }
  return call;
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
  const std::string chunkCountHelperPath = context.def.fullPath + "/SoaSchemaChunkCount";
  const std::string chunkStartHelperPath = context.def.fullPath + "/SoaSchemaChunkFieldStart";
  const std::string chunkFieldCountHelperPath = context.def.fullPath + "/SoaSchemaChunkFieldCount";

  for (const auto *helperPath : {&countHelperPath,
                                 &nameHelperPath,
                                 &typeHelperPath,
                                 &visibilityHelperPath,
                                 &chunkCountHelperPath,
                                 &chunkStartHelperPath,
                                 &chunkFieldCountHelperPath}) {
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

  auto appendIndexedI32Helper = [&](const std::string &name, const std::string &fullPath, const std::vector<uint64_t> &values) {
    Definition helper = makeHelper(name, fullPath, "i32", true);
    for (size_t fieldIndex = 0; fieldIndex < values.size(); ++fieldIndex) {
      std::vector<Expr> thenBody;
      thenBody.push_back(makeReturnStatementExpr(makeI32LiteralExpr(values[fieldIndex])));
      helper.statements.push_back(makeIfStatementExpr(
          makeEqualExpr(makeNameExpr("index"), makeI32LiteralExpr(fieldIndex)),
          makeEnvelopeExpr("then", std::move(thenBody)),
          makeEnvelopeExpr("else", {})));
    }
    helper.returnExpr = makeI32LiteralExpr(0);
    helper.hasReturnStatement = true;
    context.rewrittenDefinitions.push_back(std::move(helper));
    context.definitionPaths.insert(fullPath);
  };

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

  const size_t chunkWidth = 16;
  const size_t chunkCount = (context.fieldNames.size() + chunkWidth - 1) / chunkWidth;
  Definition chunkCountHelper = makeHelper("SoaSchemaChunkCount", chunkCountHelperPath, "i32", false);
  chunkCountHelper.returnExpr = makeI32LiteralExpr(chunkCount);
  chunkCountHelper.hasReturnStatement = true;
  context.rewrittenDefinitions.push_back(std::move(chunkCountHelper));
  context.definitionPaths.insert(chunkCountHelperPath);

  std::vector<uint64_t> chunkStarts;
  std::vector<uint64_t> chunkFieldCounts;
  chunkStarts.reserve(chunkCount);
  chunkFieldCounts.reserve(chunkCount);
  for (size_t chunkIndex = 0; chunkIndex < chunkCount; ++chunkIndex) {
    const size_t start = chunkIndex * chunkWidth;
    const size_t remaining = context.fieldNames.size() > start ? context.fieldNames.size() - start : 0;
    chunkStarts.push_back(start);
    chunkFieldCounts.push_back(std::min(chunkWidth, remaining));
  }

  appendIndexedI32Helper("SoaSchemaChunkFieldStart", chunkStartHelperPath, chunkStarts);
  appendIndexedI32Helper("SoaSchemaChunkFieldCount", chunkFieldCountHelperPath, chunkFieldCounts);
  return true;
}

bool emitReflectionSoaSchemaStorageHelpers(ReflectionGeneratedHelperContext &context) {
  const std::string storageStructPath = context.def.fullPath + "/SoaSchemaStorage";
  const std::string storageNewHelperPath = context.def.fullPath + "/SoaSchemaStorageNew";
  for (const auto *helperPath : {&storageStructPath, &storageNewHelperPath}) {
    if (context.definitionPaths.count(*helperPath) > 0) {
      context.error = "generated reflection helper already exists: " + *helperPath;
      return false;
    }
  }

  auto makeChunkTypeName = [](const std::vector<std::string> &fieldTypes) {
    if (fieldTypes.size() == 1) {
      return std::string("/std/collections/experimental_soa_storage/SoaColumn<") + formatTemplateArgs(fieldTypes) +
             ">";
    }
    return std::string("/std/collections/experimental_soa_storage/SoaColumns") +
           std::to_string(fieldTypes.size()) + "<" + formatTemplateArgs(fieldTypes) + ">";
  };

  std::vector<std::string> chunkTypeNames;
  const size_t chunkWidth = 16;
  for (size_t start = 0; start < context.fieldNames.size(); start += chunkWidth) {
    std::vector<std::string> chunkFieldTypes;
    const size_t end = std::min(start + chunkWidth, context.fieldNames.size());
    chunkFieldTypes.reserve(end - start);
    for (size_t fieldIndex = start; fieldIndex < end; ++fieldIndex) {
      const auto &fieldName = context.fieldNames[fieldIndex];
      const auto typeIt = context.fieldTypeNames.find(fieldName);
      if (typeIt == context.fieldTypeNames.end() || typeIt->second.empty()) {
        context.error = "generated reflection helper " + storageStructPath + " does not support field envelope: " +
                        fieldName;
        return false;
      }
      chunkFieldTypes.push_back(typeIt->second);
    }
    chunkTypeNames.push_back(makeChunkTypeName(chunkFieldTypes));
  }

  Definition storageStruct;
  storageStruct.name = "SoaSchemaStorage";
  storageStruct.fullPath = storageStructPath;
  storageStruct.namespacePrefix = context.def.fullPath;
  storageStruct.sourceLine = context.def.sourceLine;
  storageStruct.sourceColumn = context.def.sourceColumn;
  appendPublicVisibility(storageStruct);
  Transform structTransform;
  structTransform.name = "struct";
  storageStruct.transforms.push_back(std::move(structTransform));

  for (size_t chunkIndex = 0; chunkIndex < chunkTypeNames.size(); ++chunkIndex) {
    Expr chunkBinding =
        makeTypeBinding("chunk" + std::to_string(chunkIndex), chunkTypeNames[chunkIndex], storageStruct.namespacePrefix, true);
    appendPublicVisibility(chunkBinding);
    chunkBinding.args.push_back(makeConstructorCall(chunkTypeNames[chunkIndex]));
    chunkBinding.argNames.push_back(std::nullopt);
    storageStruct.statements.push_back(std::move(chunkBinding));
  }

  context.rewrittenDefinitions.push_back(std::move(storageStruct));
  context.definitionPaths.insert(storageStructPath);

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

  Definition storageNewHelper = makeHelper("SoaSchemaStorageNew", storageNewHelperPath, storageStructPath, false);
  storageNewHelper.returnExpr = makeConstructorCall(storageStructPath);
  storageNewHelper.hasReturnStatement = true;
  context.rewrittenDefinitions.push_back(std::move(storageNewHelper));
  context.definitionPaths.insert(storageNewHelperPath);
  return true;
}

} // namespace primec::semantics
