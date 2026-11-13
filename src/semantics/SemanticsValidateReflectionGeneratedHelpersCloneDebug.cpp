#include "SemanticsValidateReflectionGeneratedHelpersCloneDebug.h"

#include "SemanticsAstBuilders.h"
#include "SemanticsHelpers.h"

#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace primec::semantics {
namespace {

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
  cloneCall.isBraceConstructor = true;
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
