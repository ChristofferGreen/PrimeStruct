#include "SemanticsValidateReflectionGeneratedHelpersCloneDebug.h"

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

Expr makeNameExpr(const std::string &name) {
  Expr expr;
  expr.kind = Expr::Kind::Name;
  expr.name = name;
  return expr;
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

Expr makeStringLiteralExpr(const std::string &value) {
  Expr expr;
  expr.kind = Expr::Kind::StringLiteral;
  std::string encoded;
  encoded.reserve(value.size() + 8);
  encoded.push_back('"');
  for (const char ch : value) {
    if (ch == '"' || ch == '\\') {
      encoded.push_back('\\');
    }
    encoded.push_back(ch);
  }
  encoded += "\"utf8";
  expr.stringValue = std::move(encoded);
  return expr;
}

Expr makeCallExpr(const std::string &name, Expr arg) {
  Expr call;
  call.kind = Expr::Kind::Call;
  call.name = name;
  call.args.push_back(std::move(arg));
  call.argNames.push_back(std::nullopt);
  return call;
}

} // namespace

bool emitReflectionCloneHelper(ReflectionGeneratedHelperContext &context) {
  const std::string helperPath = context.def.fullPath + "/Clone";
  if (context.definitionPaths.count(helperPath) > 0) {
    context.error = "generated reflection helper already exists: " + helperPath;
    return false;
  }

  Definition helper;
  helper.name = "Clone";
  helper.fullPath = helperPath;
  helper.namespacePrefix = context.def.fullPath;
  helper.sourceLine = context.def.sourceLine;
  helper.sourceColumn = context.def.sourceColumn;

  appendPublicVisibility(helper);
  Transform returnTransform;
  returnTransform.name = "return";
  returnTransform.templateArgs.push_back(context.def.fullPath);
  helper.transforms.push_back(std::move(returnTransform));
  helper.parameters.push_back(makeTypeBinding("value", context.def.fullPath, helper.namespacePrefix));

  Expr cloneCall;
  cloneCall.kind = Expr::Kind::Call;
  cloneCall.name = context.def.fullPath;
  for (const auto &fieldName : context.fieldNames) {
    cloneCall.args.push_back(makeFieldAccessExpr("value", fieldName));
    cloneCall.argNames.push_back(std::nullopt);
  }
  helper.returnExpr = std::move(cloneCall);
  helper.hasReturnStatement = true;

  context.rewrittenDefinitions.push_back(std::move(helper));
  context.definitionPaths.insert(helperPath);
  return true;
}

bool emitReflectionDebugPrintHelper(ReflectionGeneratedHelperContext &context) {
  const std::string helperPath = context.def.fullPath + "/DebugPrint";
  if (context.definitionPaths.count(helperPath) > 0) {
    context.error = "generated reflection helper already exists: " + helperPath;
    return false;
  }

  Definition helper;
  helper.name = "DebugPrint";
  helper.fullPath = helperPath;
  helper.namespacePrefix = context.def.fullPath;
  helper.sourceLine = context.def.sourceLine;
  helper.sourceColumn = context.def.sourceColumn;

  appendPublicVisibility(helper);
  Transform returnTransform;
  returnTransform.name = "return";
  returnTransform.templateArgs.push_back("void");
  helper.transforms.push_back(std::move(returnTransform));
  helper.parameters.push_back(makeTypeBinding("value", context.def.fullPath, helper.namespacePrefix));

  if (context.fieldNames.empty()) {
    helper.statements.push_back(
        makeCallExpr("/print_line",
                     makeStringLiteralExpr(context.def.fullPath + " {}")));
  } else {
    helper.statements.push_back(
        makeCallExpr("/print_line",
                     makeStringLiteralExpr(context.def.fullPath + " {")));
    for (const auto &fieldName : context.fieldNames) {
      helper.statements.push_back(
          makeCallExpr("/print_line",
                       makeStringLiteralExpr("  " + fieldName)));
    }
    helper.statements.push_back(
        makeCallExpr("/print_line", makeStringLiteralExpr("}")));
  }

  context.rewrittenDefinitions.push_back(std::move(helper));
  context.definitionPaths.insert(helperPath);
  return true;
}

} // namespace primec::semantics
