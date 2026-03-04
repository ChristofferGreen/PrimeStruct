#include "IrLowererOnErrorHelpers.h"

#include "primec/Lexer.h"
#include "primec/Parser.h"

#include <utility>

namespace primec::ir_lowerer {

bool parseTransformArgumentExpr(const std::string &text,
                                const std::string &namespacePrefix,
                                Expr &out,
                                std::string &error) {
  Lexer lexer(text);
  Parser parser(lexer.tokenize());
  std::string parseError;
  if (!parser.parseExpression(out, namespacePrefix, parseError)) {
    error = parseError.empty() ? "invalid transform argument expression" : parseError;
    return false;
  }
  return true;
}

bool parseOnErrorTransform(const std::vector<Transform> &transforms,
                           const std::string &namespacePrefix,
                           const std::string &context,
                           const ResolveExprPathFn &resolveExprPath,
                           const DefinitionExistsFn &definitionExists,
                           std::optional<OnErrorHandler> &out,
                           std::string &error) {
  out.reset();
  for (const auto &transform : transforms) {
    if (transform.name != "on_error") {
      continue;
    }
    if (out.has_value()) {
      error = "duplicate on_error transform on " + context;
      return false;
    }
    if (transform.templateArgs.size() != 2) {
      error = "on_error requires exactly two template arguments on " + context;
      return false;
    }
    Expr handlerExpr;
    handlerExpr.kind = Expr::Kind::Name;
    handlerExpr.name = transform.templateArgs[1];
    handlerExpr.namespacePrefix = namespacePrefix;
    std::string handlerPath = resolveExprPath(handlerExpr);
    if (!definitionExists(handlerPath)) {
      error = "unknown on_error handler: " + handlerPath;
      return false;
    }
    OnErrorHandler handler;
    handler.handlerPath = handlerPath;
    handler.boundArgs.reserve(transform.arguments.size());
    for (const auto &argText : transform.arguments) {
      Expr argExpr;
      if (!parseTransformArgumentExpr(argText, namespacePrefix, argExpr, error)) {
        return false;
      }
      handler.boundArgs.push_back(std::move(argExpr));
    }
    out = std::move(handler);
  }
  return true;
}

bool buildOnErrorByDefinition(const Program &program,
                              const ResolveExprPathFn &resolveExprPath,
                              const DefinitionExistsFn &definitionExists,
                              OnErrorByDefinition &out,
                              std::string &error) {
  out.clear();
  out.reserve(program.definitions.size());
  for (const auto &def : program.definitions) {
    std::optional<OnErrorHandler> handler;
    if (!parseOnErrorTransform(
            def.transforms, def.namespacePrefix, def.fullPath, resolveExprPath, definitionExists, handler, error)) {
      return false;
    }
    out.emplace(def.fullPath, std::move(handler));
  }
  return true;
}

bool buildOnErrorByDefinitionFromCallResolutionAdapters(
    const Program &program,
    const CallResolutionAdapters &callResolutionAdapters,
    OnErrorByDefinition &out,
    std::string &error) {
  return buildOnErrorByDefinition(program,
                                  callResolutionAdapters.resolveExprPath,
                                  callResolutionAdapters.definitionExists,
                                  out,
                                  error);
}

bool buildEntryCallOnErrorSetup(const Program &program,
                                const Definition &entryDef,
                                bool definitionReturnsVoid,
                                const std::unordered_map<std::string, const Definition *> &defMap,
                                const std::unordered_map<std::string, std::string> &importAliases,
                                EntryCallOnErrorSetup &out,
                                std::string &error) {
  out = EntryCallOnErrorSetup{};
  const EntryCallResolutionSetup entryCallResolutionSetup = buildEntryCallResolutionSetup(
      entryDef, definitionReturnsVoid, defMap, importAliases);
  out.callResolutionAdapters = entryCallResolutionSetup.adapters;
  out.hasTailExecution = entryCallResolutionSetup.hasTailExecution;
  if (!buildOnErrorByDefinitionFromCallResolutionAdapters(
          program, out.callResolutionAdapters, out.onErrorByDefinition, error)) {
    return false;
  }
  return true;
}

bool buildEntryCountCallOnErrorSetup(const Program &program,
                                     const Definition &entryDef,
                                     bool definitionReturnsVoid,
                                     const std::unordered_map<std::string, const Definition *> &defMap,
                                     const std::unordered_map<std::string, std::string> &importAliases,
                                     EntryCountCallOnErrorSetup &out,
                                     std::string &error) {
  out = EntryCountCallOnErrorSetup{};
  if (!buildEntryCountAccessSetup(entryDef, out.countAccessSetup, error)) {
    return false;
  }
  if (!buildEntryCallOnErrorSetup(
          program, entryDef, definitionReturnsVoid, defMap, importAliases, out.callOnErrorSetup, error)) {
    return false;
  }
  return true;
}

} // namespace primec::ir_lowerer
