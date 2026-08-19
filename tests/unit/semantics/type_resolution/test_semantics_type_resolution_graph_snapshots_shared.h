#pragma once

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string_view>

#include "primec/ir/IrLowerer.h"
#include "primec/ir/IrSerializer.h"
#include "primec/testing/CompilePipelineDumpHelpers.h"
#include "primec/testing/SemanticsGraphHelpers.h"

#include "third_party/doctest.h"
#include "../test_semantics_helpers.h"
#include "test_semantics_type_resolution_graph_helpers.h"

[[maybe_unused]] static inline const primec::Definition *findDefinitionByPath(const primec::Program &program, std::string_view fullPath) {
  const auto it =
      std::find_if(program.definitions.begin(),
                   program.definitions.end(),
                   [fullPath](const primec::Definition &definition) { return definition.fullPath == fullPath; });
  return it == program.definitions.end() ? nullptr : &*it;
}

static inline primec::Definition *findDefinitionByPathMutable(primec::Program &program, std::string_view fullPath) {
  const auto it =
      std::find_if(program.definitions.begin(),
                   program.definitions.end(),
                   [fullPath](const primec::Definition &definition) { return definition.fullPath == fullPath; });
  return it == program.definitions.end() ? nullptr : &*it;
}

static inline std::string_view semanticTextOrFallback(const primec::SemanticProgram &semanticProgram,
                                        primec::SymbolId textId,
                                        const std::string &fallback) {
  if (textId == primec::InvalidSymbolId) {
    return fallback;
  }
  const std::string_view resolved =
      primec::semanticProgramResolveCallTargetString(semanticProgram, textId);
  return resolved.empty() ? std::string_view(fallback) : resolved;
}

[[maybe_unused]] static inline const primec::Expr *findBindingStatementByName(const primec::Definition &definition, std::string_view name) {
  const auto it =
      std::find_if(definition.statements.begin(),
                   definition.statements.end(),
                   [name](const primec::Expr &statement) { return statement.isBinding && statement.name == name; });
  return it == definition.statements.end() ? nullptr : &*it;
}

template <typename Predicate>
const primec::Expr *findExprRecursive(const primec::Expr &expr, const Predicate &predicate) {
  if (predicate(expr)) {
    return &expr;
  }
  for (const auto &arg : expr.args) {
    if (const primec::Expr *found = findExprRecursive(arg, predicate)) {
      return found;
    }
  }
  for (const auto &bodyExpr : expr.bodyArguments) {
    if (const primec::Expr *found = findExprRecursive(bodyExpr, predicate)) {
      return found;
    }
  }
  return nullptr;
}

template <typename Predicate>
primec::Expr *findExprRecursiveMutable(primec::Expr &expr, const Predicate &predicate) {
  if (predicate(expr)) {
    return &expr;
  }
  for (auto &arg : expr.args) {
    if (primec::Expr *found = findExprRecursiveMutable(arg, predicate)) {
      return found;
    }
  }
  for (auto &bodyExpr : expr.bodyArguments) {
    if (primec::Expr *found = findExprRecursiveMutable(bodyExpr, predicate)) {
      return found;
    }
  }
  return nullptr;
}

template <typename Predicate>
const primec::Expr *findExprInDefinition(const primec::Definition &definition, const Predicate &predicate) {
  for (const auto &parameter : definition.parameters) {
    if (const primec::Expr *found = findExprRecursive(parameter, predicate)) {
      return found;
    }
  }
  for (const auto &statement : definition.statements) {
    if (const primec::Expr *found = findExprRecursive(statement, predicate)) {
      return found;
    }
  }
  if (definition.returnExpr.has_value()) {
    return findExprRecursive(*definition.returnExpr, predicate);
  }
  return nullptr;
}

template <typename Predicate>
primec::Expr *findExprInDefinitionMutable(primec::Definition &definition, const Predicate &predicate) {
  for (auto &parameter : definition.parameters) {
    if (primec::Expr *found = findExprRecursiveMutable(parameter, predicate)) {
      return found;
    }
  }
  for (auto &statement : definition.statements) {
    if (primec::Expr *found = findExprRecursiveMutable(statement, predicate)) {
      return found;
    }
  }
  if (definition.returnExpr.has_value()) {
    return findExprRecursiveMutable(*definition.returnExpr, predicate);
  }
  return nullptr;
}

template <typename Entry, typename Predicate>
const Entry *findSemanticEntry(const std::vector<Entry> &entries, const Predicate &predicate) {
  const auto it = std::find_if(entries.begin(), entries.end(), predicate);
  return it == entries.end() ? nullptr : &*it;
}

template <typename Entry, typename Predicate>
const Entry *findSemanticEntry(const std::vector<const Entry *> &entries, const Predicate &predicate) {
  const auto it = std::find_if(entries.begin(),
                               entries.end(),
                               [&](const Entry *entry) { return entry != nullptr && predicate(*entry); });
  return it == entries.end() ? nullptr : *it;
}

static inline std::string_view resolveDirectCallPath(const primec::SemanticProgram &semanticProgram,
                                       const primec::SemanticProgramDirectCallTarget &entry) {
  return primec::semanticProgramDirectCallTargetResolvedPath(semanticProgram, entry);
}

static inline bool hasCanonicalSourceMapEntry(const primec::IrModule &module, int sourceLine, int sourceColumn) {
  return std::any_of(module.instructionSourceMap.begin(),
                     module.instructionSourceMap.end(),
                     [sourceLine, sourceColumn](const primec::IrInstructionSourceMapEntry &entry) {
                       return entry.provenance == primec::IrSourceMapProvenance::CanonicalAst &&
                              entry.line == static_cast<uint32_t>(sourceLine) &&
                              entry.column == static_cast<uint32_t>(sourceColumn);
                     });
}

static inline std::vector<uint8_t> serializeIrIgnoringSourceMapsAndDebug(const primec::IrModule &module) {
  primec::IrModule sanitized = module;
  sanitized.instructionSourceMap.clear();
  for (auto &function : sanitized.functions) {
    function.localDebugSlots.clear();
    for (auto &instruction : function.instructions) {
      instruction.debugId = 0;
    }
  }

  std::vector<uint8_t> encoded;
  std::string error;
  REQUIRE(primec::serializeIr(sanitized, encoded, error));
  CHECK(error.empty());
  return encoded;
}

static inline bool runDumpStageForSource(const std::string &source,
                           std::string_view dumpStage,
                           primec::CompilePipelineOutput &output,
                           primec::CompilePipelineErrorStage &errorStage,
                           std::string &error) {
  const std::filesystem::path sourcePath =
      primec::testing::detail::makeCompilePipelineDumpSourcePath();
  {
    std::ofstream file(sourcePath);
    if (!file) {
      error = "failed to write compile-pipeline boundary test source";
      return false;
    }
    file << source;
  }

  primec::Options options;
  options.inputPath = sourcePath.string();
  options.entryPath = "/main";
  options.emitKind = "native";
  options.wasmProfile = "wasi";
  options.defaultEffects =
      primec::testing::detail::defaultCompilePipelineTestingEffects();
  options.entryDefaultEffects = options.defaultEffects;
  options.dumpStage = std::string(dumpStage);
  primec::addDefaultStdlibInclude(options.inputPath, options.importPaths);

  const bool ok = primec::runCompilePipeline(options, output, errorStage, error);

  std::error_code ec;
  std::filesystem::remove(sourcePath, ec);
  return ok;
}

// TODO-5050: the SoA-compatibility snapshot cases below need genuine
// SoaVector<T>/soa<T> stdlib content and a populated SemanticProgram to
// inspect method/direct-call routing facts, so unlike the plain
// parseProgram()+Semantics::validate() cases elsewhere in this file they
// must route through the real compile pipeline (which expands imports
// against the actual stdlib source) rather than validating a bare-parsed
// Program in isolation.
static inline bool validateSoaCompatSourceForTesting(const std::string &source,
                                       primec::CompilePipelineOutput &output,
                                       std::string &error) {
  const std::filesystem::path sourcePath =
      primec::testing::detail::makeCompilePipelineDumpSourcePath();
  {
    std::ofstream file(sourcePath);
    if (!file) {
      error = "failed to write soa-compat test source";
      return false;
    }
    file << source;
  }

  primec::Options options;
  options.inputPath = sourcePath.string();
  options.entryPath = "/main";
  options.emitKind = "native";
  options.wasmProfile = "wasi";
  options.defaultEffects = {"io_out", "io_err"};
  options.entryDefaultEffects = options.defaultEffects;
  primec::addDefaultStdlibInclude(options.inputPath, options.importPaths);

  primec::CompilePipelineErrorStage errorStage = primec::CompilePipelineErrorStage::None;
  const bool ok = primec::runCompilePipeline(options, output, errorStage, error);

  std::error_code ec;
  std::filesystem::remove(sourcePath, ec);
  return ok;
}
