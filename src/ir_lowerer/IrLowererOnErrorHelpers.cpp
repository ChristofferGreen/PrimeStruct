#include "IrLowererOnErrorHelpers.h"

#include "primec/Lexer.h"
#include "primec/Parser.h"

#include <memory>
#include <utility>

namespace primec::ir_lowerer {
namespace {

bool buildOnErrorHandlerFromSemanticFact(const Definition &def,
                                         const SemanticProgram &semanticProgram,
                                         const SemanticProgramOnErrorFact &fact,
                                         std::optional<OnErrorHandler> &out,
                                         std::string &error) {
  out = OnErrorHandler{};
  out->errorType = fact.errorType;
  out->handlerPath =
      std::string(semanticProgramOnErrorFactHandlerPath(semanticProgram, fact));
  out->boundArgs.clear();
  out->boundArgs.reserve(fact.boundArgCount);

  if (fact.boundArgCount == 0) {
    return true;
  }

  if (fact.boundArgTexts.size() != fact.boundArgCount) {
    error = "semantic-product on_error bound arg mismatch on " + def.fullPath;
    return false;
  }
  for (const auto &argText : fact.boundArgTexts) {
    Expr argExpr;
    if (!parseTransformArgumentExpr(argText, def.namespacePrefix, argExpr, error)) {
      return false;
    }
    out->boundArgs.push_back(std::move(argExpr));
  }
  return true;
}

} // namespace

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
    handler.errorType = transform.templateArgs.front();
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
  return buildOnErrorByDefinition(program, nullptr, resolveExprPath, definitionExists, out, error);
}

bool buildOnErrorByDefinition(const Program &program,
                              const SemanticProgram *semanticProgram,
                              const ResolveExprPathFn &resolveExprPath,
                              const DefinitionExistsFn &definitionExists,
                              OnErrorByDefinition &out,
                              std::string &error) {
  const SemanticProductTargetAdapter semanticProductTargets =
      buildSemanticProductTargetAdapter(semanticProgram);
  out.clear();
  out.reserve(program.definitions.size());
  for (const auto &def : program.definitions) {
    std::optional<OnErrorHandler> handler;
    if (semanticProgram != nullptr) {
      const auto *callableSummary = findSemanticProductCallableSummary(semanticProductTargets, def.fullPath);
      if (callableSummary == nullptr) {
        error = "missing semantic-product callable summary: " + def.fullPath;
        return false;
      }
      if (callableSummary->hasOnError) {
        const auto *onErrorFact = findSemanticProductOnErrorFact(semanticProductTargets, def);
        if (onErrorFact == nullptr) {
          error = "missing semantic-product on_error fact: " + def.fullPath;
          return false;
        }
        if (!buildOnErrorHandlerFromSemanticFact(
                def, *semanticProgram, *onErrorFact, handler, error)) {
          return false;
        }
      }
    } else {
      if (!parseOnErrorTransform(
              def.transforms, def.namespacePrefix, def.fullPath, resolveExprPath, definitionExists, handler, error)) {
        return false;
      }
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
  return buildOnErrorByDefinition(
      program, nullptr, callResolutionAdapters.resolveExprPath, callResolutionAdapters.definitionExists, out, error);
}

bool buildOnErrorByDefinitionFromCallResolutionAdapters(
    const Program &program,
    const SemanticProgram *semanticProgram,
    const CallResolutionAdapters &callResolutionAdapters,
    OnErrorByDefinition &out,
    std::string &error) {
  return buildOnErrorByDefinition(program,
                                  semanticProgram,
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
  return buildEntryCallOnErrorSetup(
      program, entryDef, definitionReturnsVoid, defMap, importAliases, nullptr, out, error);
}

bool buildEntryCallOnErrorSetup(const Program &program,
                                const Definition &entryDef,
                                bool definitionReturnsVoid,
                                const std::unordered_map<std::string, const Definition *> &defMap,
                                const std::unordered_map<std::string, std::string> &importAliases,
                                const SemanticProgram *semanticProgram,
                                EntryCallOnErrorSetup &out,
                                std::string &error) {
  out = {};
  const EntryCallResolutionSetup entryCallResolutionSetup = buildEntryCallResolutionSetup(
      entryDef, definitionReturnsVoid, defMap, importAliases, semanticProgram);
  out.callResolutionAdapters = entryCallResolutionSetup.adapters;
  out.hasTailExecution = entryCallResolutionSetup.hasTailExecution;
  if (!buildOnErrorByDefinitionFromCallResolutionAdapters(
          program, semanticProgram, out.callResolutionAdapters, out.onErrorByDefinition, error)) {
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
  return buildEntryCountCallOnErrorSetup(
      program, entryDef, definitionReturnsVoid, defMap, importAliases, nullptr, out, error);
}

bool buildEntryCountCallOnErrorSetup(const Program &program,
                                     const Definition &entryDef,
                                     bool definitionReturnsVoid,
                                     const std::unordered_map<std::string, const Definition *> &defMap,
                                     const std::unordered_map<std::string, std::string> &importAliases,
                                     const SemanticProgram *semanticProgram,
                                     EntryCountCallOnErrorSetup &out,
                                     std::string &error) {
  out = {};
  if (!buildEntryCountAccessSetup(entryDef, semanticProgram, out.countAccessSetup, error)) {
    return false;
  }
  if (!buildEntryCallOnErrorSetup(
          program,
          entryDef,
          definitionReturnsVoid,
          defMap,
          importAliases,
          semanticProgram,
          out.callOnErrorSetup,
          error)) {
    return false;
  }
  return true;
}

} // namespace primec::ir_lowerer
