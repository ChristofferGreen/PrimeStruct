#include "IrLowererOnErrorHelpers.h"

#include "primec/Lexer.h"
#include "primec/Parser.h"
#include "primec/SemanticProduct.h"

#include <memory>
#include <string_view>
#include <utility>

namespace primec::ir_lowerer {
namespace {

bool validateSemanticProductCallableSummaryPathIds(const SemanticProgram &semanticProgram,
                                                   std::string &error) {
  const auto callableSummaries = semanticProgramCallableSummaryView(semanticProgram);
  for (const auto *summary : callableSummaries) {
    if (summary == nullptr) {
      continue;
    }
    const std::string_view callablePath =
        semanticProgramCallableSummaryFullPath(semanticProgram, *summary);
    if (summary->fullPathId == InvalidSymbolId || callablePath.empty()) {
      error = "missing semantic-product callable summary path id";
      return false;
    }
  }
  return true;
}

bool definitionCanCarryOnErrorHandler(const Definition &def) {
  return !definitionHasTransform(def, "sum");
}

bool definitionCanCarryOnErrorHandler(const Definition &def,
                                      const SemanticProgram *semanticProgram) {
  if (!definitionCanCarryOnErrorHandler(def)) {
    return false;
  }
  if (semanticProgram == nullptr) {
    return true;
  }
  if (const auto *typeMetadata =
          semanticProgramLookupTypeMetadata(*semanticProgram, def.fullPath);
      typeMetadata != nullptr && typeMetadata->category == "sum") {
    return false;
  }
  if (semanticProgramLookupPublishedSumTypeMetadata(*semanticProgram, def.fullPath) != nullptr) {
    return false;
  }
  return true;
}

bool buildOnErrorHandlerFromSemanticFact(const Definition &def,
                                         const SemanticProgram &semanticProgram,
                                         const SemanticProgramOnErrorFact &fact,
                                         std::optional<OnErrorHandler> &out,
                                         std::string &error) {
  out = OnErrorHandler{};
  if (fact.errorTypeId != InvalidSymbolId) {
    const std::string_view resolvedErrorType =
        semanticProgramResolveCallTargetString(semanticProgram, fact.errorTypeId);
    if (resolvedErrorType.empty()) {
      error = "missing semantic-product on_error error type id: " + def.fullPath;
      return false;
    }
    out->errorType = std::string(resolvedErrorType);
  } else {
    out->errorType = fact.errorType;
  }
  const auto handlerPath = semanticProgramOnErrorFactHandlerPath(semanticProgram, fact);
  if (fact.handlerPathId == InvalidSymbolId || handlerPath.empty()) {
    error = "missing semantic-product on_error handler path id: " + def.fullPath;
    return false;
  }
  out->handlerPath = std::string(handlerPath);
  out->boundArgs.clear();
  out->boundArgs.reserve(fact.boundArgCount);

  std::vector<std::string> boundArgTexts;
  boundArgTexts.reserve(fact.boundArgCount);
  if (!fact.boundArgTextIds.empty()) {
    if (fact.boundArgTextIds.size() != fact.boundArgCount) {
      error = "semantic-product on_error bound arg id mismatch on " + def.fullPath;
      return false;
    }
    for (const SymbolId argTextId : fact.boundArgTextIds) {
      const std::string_view argText =
          semanticProgramResolveCallTargetString(semanticProgram, argTextId);
      if (argTextId == InvalidSymbolId || argText.empty()) {
        error = "missing semantic-product on_error bound arg text id: " + def.fullPath;
        return false;
      }
      boundArgTexts.emplace_back(argText);
    }
  } else {
    if (fact.boundArgTexts.size() != fact.boundArgCount) {
      error = "semantic-product on_error bound arg mismatch on " + def.fullPath;
      return false;
    }
    boundArgTexts = fact.boundArgTexts;
  }

  for (const auto &argText : boundArgTexts) {
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
    if (!resolveExprPath || !definitionExists) {
      error = "internal error: missing on_error resolution adapters on " + context;
      return false;
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
  if (semanticProgram != nullptr &&
      !validateSemanticProductCallableSummaryPathIds(*semanticProgram, error)) {
    return false;
  }
  const SemanticProductIndex semanticIndex = buildSemanticProductIndex(semanticProgram);
  out.clear();
  out.reserve(program.definitions.size());
  for (const auto &def : program.definitions) {
    if (!definitionCanCarryOnErrorHandler(def, semanticProgram)) {
      continue;
    }
    std::optional<OnErrorHandler> handler;
    if (semanticProgram != nullptr) {
      const auto *callableSummary = findSemanticProductCallableSummary(semanticProgram, def.fullPath);
      if (callableSummary == nullptr) {
        error = "missing semantic-product callable summary: " + def.fullPath;
        return false;
      }
      if (callableSummary->hasOnError) {
        const auto *onErrorFact =
            findSemanticProductOnErrorFact(semanticProgram, semanticIndex, def);
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
