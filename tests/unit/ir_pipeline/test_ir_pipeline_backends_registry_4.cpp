#include "third_party/doctest.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#include "primec/AstMemory.h"
#include "primec/CliDriver.h"
#include "primec/Diagnostics.h"
#include "primec/EmitKind.h"
#include "primec/IrBackends.h"
#include "primec/IrBackendProfiles.h"
#include "primec/IrLowerer.h"
#include "primec/IrPreparation.h"
#include "primec/SemanticValidationPlan.h"
#include "primec/semantic_product/DirectCallFacts.h"
#include "primec/semantic_product/MethodCallFacts.h"
#include "primec/testing/CompilePipelineDumpHelpers.h"
#include "primec/testing/IrLowererHelpers.h"

#include "test_ir_pipeline_backends_helpers.h"
#include "test_ir_pipeline_helpers.h"

TEST_SUITE_BEGIN("primestruct.ir.pipeline.backends.registry");

namespace {

template <typename Entry, typename Predicate>
const Entry *findSemanticEntry(const std::vector<Entry> &entries, const Predicate &predicate) {
  const auto it = std::find_if(entries.begin(), entries.end(), predicate);
  if (it == entries.end()) {
    return nullptr;
  }
  return &*it;
}

template <typename Entry, typename Predicate>
const Entry *findSemanticEntry(const std::vector<const Entry *> &entries, const Predicate &predicate) {
  const auto it = std::find_if(entries.begin(),
                               entries.end(),
                               [&](const Entry *entry) { return entry != nullptr && predicate(*entry); });
  if (it == entries.end()) {
    return nullptr;
  }
  return *it;
}

std::string semanticTextOrFallback(const primec::SemanticProgram &semanticProgram,
                                   primec::SymbolId textId,
                                   std::string_view fallback) {
  if (textId != primec::InvalidSymbolId) {
    const std::string_view resolved =
        primec::semanticProgramResolveCallTargetString(semanticProgram, textId);
    if (!resolved.empty()) {
      return std::string(resolved);
    }
  }
  return std::string(fallback);
}

std::string buildMathStressSemanticSource(std::size_t localDefinitionCount) {
  std::string source = "import /std/math/*\n\n";
  for (std::size_t i = 0; i < localDefinitionCount; ++i) {
    source += "[return<i32>]\n";
    source += "leaf_" + std::to_string(i) + "() {\n";
    source += "  return(/std/math/abs(" + std::to_string(i + 1) + "i32))\n";
    source += "}\n\n";
  }
  source += "[return<i32>]\n";
  source += "main() {\n";
  source += "  [i32 mut] total{0i32}\n";
  for (std::size_t i = 0; i < localDefinitionCount; ++i) {
    source += "  assign(total, plus(total, leaf_" + std::to_string(i) + "()))\n";
  }
  source += "  return(total)\n";
  source += "}\n";
  return source;
}

std::string buildMathStressDiagnosticSource(std::size_t localDefinitionCount) {
  std::string source = "import /std/math/*\n\n";
  for (std::size_t i = 0; i < localDefinitionCount; ++i) {
    source += "[return<void>]\n";
    source += "broken_" + std::to_string(i) + "() {\n";
    source += "  missing_symbol_" + std::to_string(i) + "()\n";
    source += "}\n\n";
  }
  source += "[return<i32>]\n";
  source += "main() {\n";
  source += "  return(0i32)\n";
  source += "}\n";
  return source;
}

std::vector<std::string> compileDiagnosticMessages(const primec::CompilePipelineDiagnosticInfo &diagnostics) {
  std::vector<std::string> messages;
  messages.reserve(diagnostics.records.size());
  for (const auto &record : diagnostics.records) {
    messages.push_back(record.message);
  }
  return messages;
}

std::size_t lineNumberForOffset(std::string_view text, std::size_t offset) {
  std::size_t line = 1;
  for (std::size_t i = 0; i < offset && i < text.size(); ++i) {
    if (text[i] == '\n') {
      ++line;
    }
  }
  return line;
}

std::string lineAtOffset(std::string_view text, std::size_t offset) {
  const std::size_t clampedOffset = std::min(offset, text.size());
  const std::size_t lineStart = text.rfind('\n', clampedOffset);
  const std::size_t start = lineStart == std::string_view::npos ? 0 : lineStart + 1;
  const std::size_t lineEnd = text.find('\n', clampedOffset);
  const std::size_t end = lineEnd == std::string_view::npos ? text.size() : lineEnd;
  return std::string(text.substr(start, end - start));
}

std::string describeSemanticProductDumpMismatch(std::string_view expectedLabel,
                                                std::string_view actualLabel,
                                                std::string_view expected,
                                                std::string_view actual) {
  if (expected == actual) {
    return {};
  }

  const std::size_t sharedSize = std::min(expected.size(), actual.size());
  std::size_t mismatchOffset = 0;
  while (mismatchOffset < sharedSize &&
         expected[mismatchOffset] == actual[mismatchOffset]) {
    ++mismatchOffset;
  }

  const std::size_t expectedLine = lineNumberForOffset(expected, mismatchOffset);
  const std::size_t actualLine = lineNumberForOffset(actual, mismatchOffset);
  return "semantic-product dump mismatch: " + std::string(expectedLabel) +
         " vs " + std::string(actualLabel) + ", offset " +
         std::to_string(mismatchOffset) + ", expected line " +
         std::to_string(expectedLine) + " `" +
         lineAtOffset(expected, mismatchOffset) + "`, actual line " +
         std::to_string(actualLine) + " `" +
         lineAtOffset(actual, mismatchOffset) + "`";
}

void checkOptionalRssCheckpointSnapshot(const primec::SemanticPhaseCounterSnapshot &snapshot) {
  const bool hasCheckpoints = snapshot.rssBeforeBytes > 0 || snapshot.rssAfterBytes > 0;
  if (!hasCheckpoints) {
    CHECK(snapshot.rssBeforeBytes == 0);
    CHECK(snapshot.rssAfterBytes == 0);
    return;
  }
  CHECK(snapshot.rssBeforeBytes > 0);
  CHECK(snapshot.rssAfterBytes > 0);
}

void addVoidCallableSummaryForPath(primec::SemanticProgram &semanticProgram,
                                   std::string_view fullPath,
                                   uint64_t semanticNodeId) {
  const auto fullPathId =
      primec::semanticProgramInternCallTargetString(semanticProgram, std::string(fullPath));
  const auto callableSummaryIndex = semanticProgram.callableSummaries.size();
  semanticProgram.callableSummaries.push_back(primec::SemanticProgramCallableSummary{
      .isExecution = false,
      .returnKind = "void",
      .isCompute = false,
      .isUnsafe = false,
      .activeEffects = {},
      .activeCapabilities = {},
      .hasResultType = false,
      .resultTypeHasValue = false,
      .resultValueType = "",
      .resultErrorType = "",
      .hasOnError = false,
      .onErrorHandlerPath = "",
      .onErrorErrorType = "",
      .onErrorBoundArgCount = 0,
      .semanticNodeId = semanticNodeId,
      .provenanceHandle = 0,
      .fullPathId = fullPathId,
  });
  semanticProgram.publishedRoutingLookups.callableSummaryIndicesByPathId
      .insert_or_assign(fullPathId, callableSummaryIndex);
  const auto returnFactIndex = semanticProgram.returnFacts.size();
  semanticProgram.returnFacts.push_back(primec::SemanticProgramReturnFact{
      .returnKind = "void",
      .structPath = "",
      .bindingTypeText = "void",
      .isMutable = false,
      .isEntryArgString = false,
      .isUnsafeReference = false,
      .referenceRoot = "",
      .sourceLine = 0,
      .sourceColumn = 0,
      .semanticNodeId = semanticNodeId,
      .definitionPathId = fullPathId,
  });
  semanticProgram.publishedRoutingLookups.returnFactIndicesByDefinitionId
      .insert_or_assign(semanticNodeId, returnFactIndex);
  semanticProgram.publishedRoutingLookups.returnFactIndicesByDefinitionPathId
      .insert_or_assign(fullPathId, returnFactIndex);
}

void addVoidCallableSummary(primec::SemanticProgram &semanticProgram, uint64_t semanticNodeId) {
  addVoidCallableSummaryForPath(semanticProgram, "/main", semanticNodeId);
}

void publishFixtureCallableAndReturnRouting(primec::SemanticProgram &semanticProgram) {
  for (std::size_t index = 0; index < semanticProgram.callableSummaries.size(); ++index) {
    const auto fullPathId = semanticProgram.callableSummaries[index].fullPathId;
    if (fullPathId != primec::InvalidSymbolId) {
      semanticProgram.publishedRoutingLookups.callableSummaryIndicesByPathId
          .insert_or_assign(fullPathId, index);
    }
  }
  for (std::size_t index = 0; index < semanticProgram.returnFacts.size(); ++index) {
    const auto definitionPathId = semanticProgram.returnFacts[index].definitionPathId;
    if (definitionPathId != primec::InvalidSymbolId) {
      semanticProgram.publishedRoutingLookups.returnFactIndicesByDefinitionPathId
          .insert_or_assign(definitionPathId, index);
    }
  }
}

const primec::semantics::SemanticValidationPassManifestEntry *findSemanticValidationPass(
    std::string_view name) {
  const auto &manifest = primec::semantics::semanticValidationPassManifest();
  const auto it = std::find_if(
      manifest.begin(),
      manifest.end(),
      [name](const primec::semantics::SemanticValidationPassManifestEntry &entry) {
        return entry.name == name;
      });
  return it == manifest.end() ? nullptr : &*it;
}

const primec::IrPreparationPhaseManifestEntry *findIrPreparationPhase(
    std::string_view name) {
  const auto &manifest = primec::irPreparationPhaseManifest();
  const auto it = std::find_if(
      manifest.begin(),
      manifest.end(),
      [name](const primec::IrPreparationPhaseManifestEntry &entry) {
        return entry.name == name;
      });
  return it == manifest.end() ? nullptr : &*it;
}

const primec::SemanticProgramModuleResolvedArtifacts *findSemanticModuleArtifacts(
    const primec::SemanticProgram &semanticProgram,
    std::string_view moduleKey) {
  const auto it = std::find_if(
      semanticProgram.moduleResolvedArtifacts.begin(),
      semanticProgram.moduleResolvedArtifacts.end(),
      [moduleKey](const primec::SemanticProgramModuleResolvedArtifacts &module) {
        return module.identity.moduleKey == moduleKey;
      });
  return it == semanticProgram.moduleResolvedArtifacts.end() ? nullptr : &*it;
}

} // namespace

TEST_CASE("ir lowerer rejects missing semantic-product return binding types") {
  primec::Program program;

  primec::Definition mainDef;
  mainDef.fullPath = "/main";
  program.definitions.push_back(mainDef);

  primec::SemanticProgram semanticProgram;
  semanticProgram.entryPath = "/main";
  semanticProgram.callableSummaries.push_back(primec::SemanticProgramCallableSummary{
      .isExecution = false,
      .returnKind = "return",
      .isCompute = false,
      .isUnsafe = false,
      .activeEffects = {},
      .activeCapabilities = {},
      .hasResultType = false,
      .resultTypeHasValue = false,
      .resultValueType = "",
      .resultErrorType = "",
      .hasOnError = false,
      .onErrorHandlerPath = "",
      .onErrorErrorType = "",
      .onErrorBoundArgCount = 0,
      .semanticNodeId = 0,
      .provenanceHandle = 0,
      .fullPathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/main"),
  });
  semanticProgram.returnFacts.push_back(primec::SemanticProgramReturnFact{
      .returnKind = "return",
      .structPath = "",
      .bindingTypeText = "",
      .isMutable = false,
      .isEntryArgString = false,
      .isUnsafeReference = false,
      .referenceRoot = "",
      .sourceLine = 0,
      .sourceColumn = 0,
      .semanticNodeId = 0,
      .definitionPathId =
          primec::semanticProgramInternCallTargetString(semanticProgram, "/main"),
  });
  semanticProgram.publishedRoutingLookups.returnFactIndicesByDefinitionId
      .insert_or_assign(83020, semanticProgram.returnFacts.size() - 1);

  primec::IrLowerer lowerer;
  primec::IrModule module;
  primec::DiagnosticSinkReport diagnosticInfo;
  std::string error;

  CHECK_FALSE(lowerer.lower(program, &semanticProgram, "/main", {}, {}, module, error, &diagnosticInfo));
  CHECK(error == "missing semantic-product return binding type: /main");
  CHECK(diagnosticInfo.message == error);
}

TEST_CASE("ir lowerer result completeness requires published return maps") {
  primec::SemanticProgram semanticProgram;
  semanticProgram.entryPath = "/main";
  semanticProgram.callableSummaries.push_back(primec::SemanticProgramCallableSummary{
      .isExecution = false,
      .returnKind = "return",
      .isCompute = false,
      .isUnsafe = false,
      .activeEffects = {},
      .activeCapabilities = {},
      .hasResultType = false,
      .resultTypeHasValue = false,
      .resultValueType = "",
      .resultErrorType = "",
      .hasOnError = false,
      .onErrorHandlerPath = "",
      .onErrorErrorType = "",
      .onErrorBoundArgCount = 0,
      .semanticNodeId = 9401,
      .provenanceHandle = 0,
      .fullPathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/main"),
  });
  semanticProgram.returnFacts.push_back(primec::SemanticProgramReturnFact{
      .returnKind = "return",
      .structPath = "",
      .bindingTypeText = "",
      .isMutable = false,
      .isEntryArgString = false,
      .isUnsafeReference = false,
      .referenceRoot = "",
      .sourceLine = 0,
      .sourceColumn = 0,
      .semanticNodeId = 9401,
      .definitionPathId =
          primec::semanticProgramInternCallTargetString(semanticProgram, "/main"),
  });

  std::string error;
  CHECK(primec::ir_lowerer::validateSemanticProductResultMetadataCompleteness(
      &semanticProgram, error));
  CHECK(error.empty());

  semanticProgram.publishedRoutingLookups.returnFactIndicesByDefinitionId
      .insert_or_assign(9401, 0);
  CHECK_FALSE(primec::ir_lowerer::validateSemanticProductResultMetadataCompleteness(
      &semanticProgram, error));
  CHECK(error == "missing semantic-product return binding type: /main");
}

TEST_CASE("ir lowerer rejects missing semantic-product return definition path id") {
  primec::Program program;

  primec::Definition mainDef;
  mainDef.fullPath = "/main";
  mainDef.semanticNodeId = 9101;
  program.definitions.push_back(mainDef);

  primec::SemanticProgram semanticProgram;
  semanticProgram.entryPath = "/main";
  semanticProgram.callableSummaries.push_back(primec::SemanticProgramCallableSummary{
      .isExecution = false,
      .returnKind = "return",
      .isCompute = false,
      .isUnsafe = false,
      .activeEffects = {},
      .activeCapabilities = {},
      .hasResultType = false,
      .resultTypeHasValue = false,
      .resultValueType = "",
      .resultErrorType = "",
      .hasOnError = false,
      .onErrorHandlerPath = "",
      .onErrorErrorType = "",
      .onErrorBoundArgCount = 0,
      .semanticNodeId = 9101,
      .provenanceHandle = 0,
      .fullPathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/main"),
  });
  semanticProgram.returnFacts.push_back(primec::SemanticProgramReturnFact{
      .returnKind = "return",
      .structPath = "",
      .bindingTypeText = "i32",
      .isMutable = false,
      .isEntryArgString = false,
      .isUnsafeReference = false,
      .referenceRoot = "",
      .sourceLine = 0,
      .sourceColumn = 0,
      .semanticNodeId = 9101,
      .definitionPathId = primec::InvalidSymbolId,
  });
  semanticProgram.publishedRoutingLookups.returnFactIndicesByDefinitionId
      .insert_or_assign(9101, semanticProgram.returnFacts.size() - 1);

  primec::IrLowerer lowerer;
  primec::IrModule module;
  primec::DiagnosticSinkReport diagnosticInfo;
  std::string error;

  CHECK_FALSE(lowerer.lower(program, &semanticProgram, "/main", {}, {}, module, error, &diagnosticInfo));
  CHECK(error == "missing semantic-product return definition path id");
  CHECK(diagnosticInfo.message == error);
}

TEST_CASE("ir lowerer rejects stale semantic-product return facts") {
  primec::SemanticProgram semanticProgram;
  semanticProgram.entryPath = "/main";
  semanticProgram.callableSummaries.push_back(primec::SemanticProgramCallableSummary{
      .isExecution = false,
      .returnKind = "i32",
      .isCompute = false,
      .isUnsafe = false,
      .activeEffects = {},
      .activeCapabilities = {},
      .hasResultType = false,
      .resultTypeHasValue = false,
      .resultValueType = "",
      .resultErrorType = "",
      .hasOnError = false,
      .onErrorHandlerPath = "",
      .onErrorErrorType = "",
      .onErrorBoundArgCount = 0,
      .semanticNodeId = 9102,
      .provenanceHandle = 0,
      .fullPathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/main"),
      .returnKindId = primec::semanticProgramInternCallTargetString(semanticProgram, "i32"),
  });
  semanticProgram.returnFacts.push_back(primec::SemanticProgramReturnFact{
      .returnKind = "i32",
      .structPath = "/i32",
      .bindingTypeText = "",
      .isMutable = false,
      .isEntryArgString = false,
      .isUnsafeReference = false,
      .referenceRoot = "",
      .sourceLine = 0,
      .sourceColumn = 0,
      .semanticNodeId = 9102,
      .definitionPathId =
          primec::semanticProgramInternCallTargetString(semanticProgram, "/main"),
      .returnKindId = primec::semanticProgramInternCallTargetString(semanticProgram, "i32"),
      .structPathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/i32"),
      .bindingTypeTextId = primec::semanticProgramInternCallTargetString(semanticProgram, "i32"),
  });
  semanticProgram.publishedRoutingLookups.returnFactIndicesByDefinitionId
      .insert_or_assign(9102, semanticProgram.returnFacts.size() - 1);
  publishFixtureCallableAndReturnRouting(semanticProgram);

  std::string error;
  CHECK(primec::ir_lowerer::validateSemanticProductResultMetadataCompleteness(
      &semanticProgram, error));
  CHECK(error.empty());

  semanticProgram.returnFacts.back().bindingTypeTextId =
      static_cast<primec::SymbolId>(semanticProgram.callTargetStringTable.size() + 1u);

  CHECK_FALSE(primec::ir_lowerer::validateSemanticProductResultMetadataCompleteness(
      &semanticProgram, error));
  CHECK(error == "missing semantic-product return binding type id: /main");

  semanticProgram.returnFacts.back().bindingTypeText = "i32";
  error.clear();
  CHECK_FALSE(primec::ir_lowerer::validateSemanticProductResultMetadataCompleteness(
      &semanticProgram, error));
  CHECK(error == "missing semantic-product return binding type id: /main");

  semanticProgram.returnFacts.back().bindingTypeTextId =
      primec::semanticProgramInternCallTargetString(semanticProgram, "i64");
  error.clear();

  CHECK_FALSE(primec::ir_lowerer::validateSemanticProductResultMetadataCompleteness(
      &semanticProgram, error));
  CHECK(error == "stale semantic-product return binding type metadata: /main");

  semanticProgram.returnFacts.back().bindingTypeTextId =
      primec::semanticProgramInternCallTargetString(semanticProgram, "i32");
  semanticProgram.returnFacts.back().structPathId =
      primec::semanticProgramInternCallTargetString(semanticProgram, "/Other");
  error.clear();

  CHECK_FALSE(primec::ir_lowerer::validateSemanticProductResultMetadataCompleteness(
      &semanticProgram, error));
  CHECK(error == "stale semantic-product return struct path metadata: /main");

  semanticProgram.returnFacts.back().structPathId =
      primec::semanticProgramInternCallTargetString(semanticProgram, "/i32");
  semanticProgram.returnFacts.back().returnKind = "return";
  semanticProgram.returnFacts.back().returnKindId =
      primec::semanticProgramInternCallTargetString(semanticProgram, "return");
  error.clear();

  CHECK_FALSE(primec::ir_lowerer::validateSemanticProductResultMetadataCompleteness(
      &semanticProgram, error));
  CHECK(error == "stale semantic-product return fact: /main");
}

TEST_CASE("ir lowerer rejects incomplete semantic-product query facts") {
  primec::Program program;

  primec::Definition mainDef;
  mainDef.fullPath = "/main";
  mainDef.semanticNodeId = 83020;
  primec::Transform queryReturnTransform;
  queryReturnTransform.name = "return";
  queryReturnTransform.templateArgs.push_back("i32");
  mainDef.transforms.push_back(queryReturnTransform);
  primec::Expr callExpr;
  callExpr.kind = primec::Expr::Kind::Call;
  callExpr.name = "lookup";
  callExpr.semanticNodeId = 83;
  mainDef.statements.push_back(callExpr);
  program.definitions.push_back(mainDef);

  primec::SemanticProgram semanticProgram;
  semanticProgram.entryPath = "/main";
  semanticProgram.directCallTargets.push_back(primec::SemanticProgramDirectCallTarget{
      .scopePath = "/main",
      .callName = "lookup",
      .sourceLine = 1,
      .sourceColumn = 1,
      .semanticNodeId = 83,
      .resolvedPathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/lookup"),
      .stdlibSurfaceId = std::nullopt,
  });
  semanticProgram.queryFacts.push_back(primec::SemanticProgramQueryFact{
      .scopePath = "/main",
      .callName = "lookup",
      .queryTypeText = "Result<i32, FileError>",
      .bindingTypeText = "Result<i32, FileError>",
      .receiverBindingTypeText = "",
      .hasResultType = true,
      .resultTypeHasValue = true,
      .resultValueType = "",
      .resultErrorType = "FileError",
      .sourceLine = 1,
      .sourceColumn = 1,
      .semanticNodeId = 83,
      .resolvedPathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/lookup"),
  });
  semanticProgram.publishedRoutingLookups.queryFactIndicesByExpr.insert_or_assign(83, 0);
  primec::SemanticProgramCallableSummary callableSummary;
  callableSummary.semanticNodeId = 83020;
  callableSummary.fullPathId =
      primec::semanticProgramInternCallTargetString(semanticProgram, "/main");
  callableSummary.returnKind = "i32";
  semanticProgram.callableSummaries.push_back(std::move(callableSummary));
  semanticProgram.returnFacts.push_back(primec::SemanticProgramReturnFact{
      .returnKind = "i32",
      .structPath = "",
      .bindingTypeText = "i32",
      .isMutable = false,
      .isEntryArgString = false,
      .isUnsafeReference = false,
      .referenceRoot = "",
      .sourceLine = 0,
      .sourceColumn = 0,
      .semanticNodeId = 83020,
      .provenanceHandle = 0,
      .definitionPathId =
          primec::semanticProgramInternCallTargetString(semanticProgram, "/main"),
  });
  semanticProgram.publishedRoutingLookups.returnFactIndicesByDefinitionId
      .insert_or_assign(83020, semanticProgram.returnFacts.size() - 1);

  primec::IrLowerer lowerer;
  primec::IrModule module;
  primec::DiagnosticSinkReport diagnosticInfo;
  std::string error;

  CHECK_FALSE(lowerer.lower(program, &semanticProgram, "/main", {}, {}, module, error, &diagnosticInfo));
  CHECK(error == "incomplete semantic-product query fact: lookup");
  CHECK(diagnosticInfo.message == error);
}

TEST_CASE("ir lowerer rejects missing semantic-product query resolved path id") {
  primec::Program program;

  primec::Definition mainDef;
  mainDef.fullPath = "/main";
  primec::Expr callExpr;
  callExpr.kind = primec::Expr::Kind::Call;
  callExpr.name = "lookup";
  callExpr.semanticNodeId = 8301;
  mainDef.statements.push_back(callExpr);
  program.definitions.push_back(mainDef);

  primec::SemanticProgram semanticProgram;
  semanticProgram.entryPath = "/main";
  semanticProgram.directCallTargets.push_back(primec::SemanticProgramDirectCallTarget{
      .scopePath = "/main",
      .callName = "lookup",
      .sourceLine = 1,
      .sourceColumn = 1,
      .semanticNodeId = 8301,
      .resolvedPathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/lookup"),
      .stdlibSurfaceId = std::nullopt,
  });
  semanticProgram.queryFacts.push_back(primec::SemanticProgramQueryFact{
      .scopePath = "/main",
      .callName = "lookup",
      .queryTypeText = "Result<i32, FileError>",
      .bindingTypeText = "Result<i32, FileError>",
      .receiverBindingTypeText = "",
      .hasResultType = true,
      .resultTypeHasValue = true,
      .resultValueType = "i32",
      .resultErrorType = "FileError",
      .sourceLine = 1,
      .sourceColumn = 1,
      .semanticNodeId = 8301,
      .resolvedPathId = primec::InvalidSymbolId,
  });
  semanticProgram.publishedRoutingLookups.queryFactIndicesByExpr.insert_or_assign(8301, 0);
  primec::SemanticProgramCallableSummary callableSummary;
  callableSummary.fullPathId =
      primec::semanticProgramInternCallTargetString(semanticProgram, "/main");
  callableSummary.returnKind = "i32";
  semanticProgram.callableSummaries.push_back(std::move(callableSummary));
  semanticProgram.returnFacts.push_back(primec::SemanticProgramReturnFact{
      .returnKind = "i32",
      .structPath = "",
      .bindingTypeText = "i32",
      .isMutable = false,
      .isEntryArgString = false,
      .isUnsafeReference = false,
      .referenceRoot = "",
      .sourceLine = 0,
      .sourceColumn = 0,
      .semanticNodeId = 0,
      .provenanceHandle = 0,
      .definitionPathId =
          primec::semanticProgramInternCallTargetString(semanticProgram, "/main"),
  });

  primec::IrLowerer lowerer;
  primec::IrModule module;
  primec::DiagnosticSinkReport diagnosticInfo;
  std::string error;

  CHECK_FALSE(lowerer.lower(program, &semanticProgram, "/main", {}, {}, module, error, &diagnosticInfo));
  CHECK(error == "missing semantic-product query resolved path id: lookup");
  CHECK(diagnosticInfo.message == error);
}

TEST_CASE("ir lowerer rejects stale semantic-product query facts") {
  primec::Program program;

  primec::Definition mainDef;
  mainDef.fullPath = "/main";
  mainDef.semanticNodeId = 83020;
  primec::Expr callExpr;
  callExpr.kind = primec::Expr::Kind::Call;
  callExpr.name = "plus";
  callExpr.semanticNodeId = 8302;
  primec::Expr leftArg;
  leftArg.kind = primec::Expr::Kind::Literal;
  leftArg.literalValue = 1;
  leftArg.intWidth = 32;
  primec::Expr rightArg;
  rightArg.kind = primec::Expr::Kind::Literal;
  rightArg.literalValue = 2;
  rightArg.intWidth = 32;
  callExpr.args.push_back(leftArg);
  callExpr.args.push_back(rightArg);
  mainDef.returnExpr = callExpr;
  program.definitions.push_back(mainDef);

  primec::SemanticProgram semanticProgram;
  semanticProgram.entryPath = "/main";
  semanticProgram.directCallTargets.push_back(primec::SemanticProgramDirectCallTarget{
      .scopePath = "/main",
      .callName = "plus",
      .sourceLine = 1,
      .sourceColumn = 1,
      .semanticNodeId = 8302,
      .resolvedPathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/plus"),
      .stdlibSurfaceId = std::nullopt,
  });
  semanticProgram.publishedRoutingLookups.directCallTargetIdsByExpr.insert_or_assign(
      8302, semanticProgram.directCallTargets.back().resolvedPathId);
  semanticProgram.queryFacts.push_back(primec::SemanticProgramQueryFact{
      .scopePath = "/main",
      .callName = "plus",
      .queryTypeText = "Result<i32, FileError>",
      .bindingTypeText = "Result<i32, FileError>",
      .receiverBindingTypeText = "",
      .hasResultType = true,
      .resultTypeHasValue = true,
      .resultValueType = "i32",
      .resultErrorType = "FileError",
      .sourceLine = 1,
      .sourceColumn = 1,
      .semanticNodeId = 8302,
      .resolvedPathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/stale_lookup"),
  });
  semanticProgram.publishedRoutingLookups.queryFactIndicesByExpr.insert_or_assign(8302, 0);
  primec::SemanticProgramCallableSummary callableSummary;
  callableSummary.semanticNodeId = 83020;
  callableSummary.fullPathId =
      primec::semanticProgramInternCallTargetString(semanticProgram, "/main");
  callableSummary.returnKind = "i32";
  const auto callableSummaryFullPathId = callableSummary.fullPathId;
  semanticProgram.callableSummaries.push_back(std::move(callableSummary));
  semanticProgram.publishedRoutingLookups.callableSummaryIndicesByPathId.insert_or_assign(
      callableSummaryFullPathId, static_cast<std::size_t>(0));
  semanticProgram.returnFacts.push_back(primec::SemanticProgramReturnFact{
      .returnKind = "i32",
      .structPath = "",
      .bindingTypeText = "i32",
      .isMutable = false,
      .isEntryArgString = false,
      .isUnsafeReference = false,
      .referenceRoot = "",
      .sourceLine = 0,
      .sourceColumn = 0,
      .semanticNodeId = 83020,
      .provenanceHandle = 0,
      .definitionPathId =
          primec::semanticProgramInternCallTargetString(semanticProgram, "/main"),
  });
  semanticProgram.publishedRoutingLookups.returnFactIndicesByDefinitionId.insert_or_assign(
      static_cast<uint64_t>(83020), static_cast<std::size_t>(0));

  primec::IrLowerer lowerer;
  primec::IrModule module;
  primec::DiagnosticSinkReport diagnosticInfo;
  std::string error;

  CHECK_FALSE(lowerer.lower(program, &semanticProgram, "/main", {}, {}, module, error, &diagnosticInfo));
  CHECK(error == "stale semantic-product query fact: plus");
  CHECK(diagnosticInfo.message == error);
}

TEST_CASE("ir lowerer accepts query-owned builtin count target metadata") {
  const auto acceptsCountShadow = [](const char *publishedPath) {
    primec::SemanticProgram semanticProgram;
    semanticProgram.entryPath = "/main";
    semanticProgram.directCallTargets.push_back(primec::SemanticProgramDirectCallTarget{
        .scopePath = "/main",
        .callName = "count",
        .sourceLine = 1,
        .sourceColumn = 1,
        .semanticNodeId = 8303,
        .resolvedPathId =
            primec::semanticProgramInternCallTargetString(semanticProgram, publishedPath),
        .stdlibSurfaceId = std::nullopt,
    });
    semanticProgram.queryFacts.push_back(primec::SemanticProgramQueryFact{
        .scopePath = "/main",
        .callName = "count",
        .queryTypeText = "i32",
        .bindingTypeText = "i32",
        .receiverBindingTypeText = "",
        .hasResultType = false,
        .resultTypeHasValue = false,
        .resultValueType = "",
        .resultErrorType = "",
        .sourceLine = 1,
        .sourceColumn = 1,
        .semanticNodeId = 8303,
        .callNameId = primec::semanticProgramInternCallTargetString(semanticProgram, "count"),
        .resolvedPathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/count"),
        .queryTypeTextId = primec::semanticProgramInternCallTargetString(semanticProgram, "i32"),
        .bindingTypeTextId = primec::semanticProgramInternCallTargetString(semanticProgram, "i32"),
        .receiverBindingTypeTextId =
            primec::semanticProgramInternCallTargetString(semanticProgram, ""),
    });
    semanticProgram.publishedRoutingLookups.queryFactIndicesByExpr.insert_or_assign(8303, 0);

    std::string error;
    CHECK(primec::ir_lowerer::validateSemanticProductResultMetadataCompleteness(
        &semanticProgram, error));
    CHECK(error.empty());
  };

  acceptsCountShadow("/array/count");
  acceptsCountShadow("/string/count");
}

TEST_CASE("ir lowerer rejects stale semantic-product query type metadata") {
  primec::SemanticProgram semanticProgram;
  semanticProgram.entryPath = "/main";
  semanticProgram.directCallTargets.push_back(primec::SemanticProgramDirectCallTarget{
      .scopePath = "/main",
      .callName = "lookup",
      .sourceLine = 1,
      .sourceColumn = 1,
      .semanticNodeId = 8304,
      .resolvedPathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/lookup"),
      .stdlibSurfaceId = std::nullopt,
  });
  semanticProgram.queryFacts.push_back(primec::SemanticProgramQueryFact{
      .scopePath = "/main",
      .callName = "lookup",
      .queryTypeText = "i32",
      .bindingTypeText = "i32",
      .receiverBindingTypeText = "Map<string, i32>",
      .hasResultType = false,
      .resultTypeHasValue = false,
      .resultValueType = "",
      .resultErrorType = "",
      .sourceLine = 1,
      .sourceColumn = 1,
      .semanticNodeId = 8304,
      .callNameId = primec::semanticProgramInternCallTargetString(semanticProgram, "lookup"),
      .resolvedPathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/lookup"),
      .queryTypeTextId = primec::semanticProgramInternCallTargetString(semanticProgram, "i32"),
      .bindingTypeTextId = primec::semanticProgramInternCallTargetString(semanticProgram, "i32"),
      .receiverBindingTypeTextId =
          primec::semanticProgramInternCallTargetString(semanticProgram, "Map<string, i32>"),
  });
  semanticProgram.publishedRoutingLookups.queryFactIndicesByExpr.insert_or_assign(8304, 0);

  std::string error;
  CHECK(primec::ir_lowerer::validateSemanticProductResultMetadataCompleteness(
      &semanticProgram, error));
  CHECK(error.empty());

  semanticProgram.queryFacts.back().queryTypeTextId =
      static_cast<primec::SymbolId>(semanticProgram.callTargetStringTable.size() + 1u);

  CHECK_FALSE(primec::ir_lowerer::validateSemanticProductResultMetadataCompleteness(
      &semanticProgram, error));
  CHECK(error == "missing semantic-product query type id: lookup");

  semanticProgram.queryFacts.back().queryTypeTextId =
      primec::semanticProgramInternCallTargetString(semanticProgram, "i64");
  error.clear();

  CHECK_FALSE(primec::ir_lowerer::validateSemanticProductResultMetadataCompleteness(
      &semanticProgram, error));
  CHECK(error == "stale semantic-product query type metadata: lookup");

  semanticProgram.queryFacts.back().queryTypeTextId =
      primec::semanticProgramInternCallTargetString(semanticProgram, "i32");
  semanticProgram.queryFacts.back().bindingTypeTextId =
      primec::semanticProgramInternCallTargetString(semanticProgram, "i64");
  error.clear();

  CHECK_FALSE(primec::ir_lowerer::validateSemanticProductResultMetadataCompleteness(
      &semanticProgram, error));
  CHECK(error == "stale semantic-product query binding type metadata: lookup");

  semanticProgram.queryFacts.back().bindingTypeTextId =
      primec::semanticProgramInternCallTargetString(semanticProgram, "i32");
  semanticProgram.queryFacts.back().receiverBindingTypeTextId =
      primec::semanticProgramInternCallTargetString(semanticProgram, "OtherMap");
  error.clear();

  CHECK_FALSE(primec::ir_lowerer::validateSemanticProductResultMetadataCompleteness(
      &semanticProgram, error));
  CHECK(error == "stale semantic-product query receiver binding type metadata: lookup");
}

TEST_CASE("ir lowerer rejects stale semantic-product query result metadata") {
  primec::SemanticProgram semanticProgram;
  semanticProgram.entryPath = "/main";
  semanticProgram.directCallTargets.push_back(primec::SemanticProgramDirectCallTarget{
      .scopePath = "/main",
      .callName = "lookup",
      .sourceLine = 1,
      .sourceColumn = 1,
      .semanticNodeId = 8303,
      .resolvedPathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/lookup"),
      .stdlibSurfaceId = std::nullopt,
  });
  semanticProgram.queryFacts.push_back(primec::SemanticProgramQueryFact{
      .scopePath = "/main",
      .callName = "lookup",
      .queryTypeText = "Result<i32, FileError>",
      .bindingTypeText = "Result<i32, FileError>",
      .receiverBindingTypeText = "",
      .hasResultType = true,
      .resultTypeHasValue = true,
      .resultValueType = "i32",
      .resultErrorType = "FileError",
      .sourceLine = 1,
      .sourceColumn = 1,
      .semanticNodeId = 8303,
      .callNameId = primec::semanticProgramInternCallTargetString(semanticProgram, "lookup"),
      .resolvedPathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/lookup"),
      .resultValueTypeId = primec::semanticProgramInternCallTargetString(semanticProgram, "i32"),
      .resultErrorTypeId =
          primec::semanticProgramInternCallTargetString(semanticProgram, "FileError"),
  });
  semanticProgram.publishedRoutingLookups.queryFactIndicesByExpr.insert_or_assign(8303, 0);
  semanticProgram.callableSummaries.push_back(primec::SemanticProgramCallableSummary{
      .returnKind = "i32",
      .isCompute = false,
      .isUnsafe = false,
      .activeEffects = {},
      .activeCapabilities = {},
      .hasResultType = true,
      .resultTypeHasValue = true,
      .resultValueType = "i32",
      .resultErrorType = "FileError",
      .hasOnError = false,
      .onErrorHandlerPath = "",
      .onErrorErrorType = "",
      .onErrorBoundArgCount = 0,
      .semanticNodeId = 0,
      .provenanceHandle = 0,
      .fullPathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/lookup"),
      .returnKindId = primec::InvalidSymbolId,
      .activeEffectIds = {},
      .activeCapabilityIds = {},
      .resultValueTypeId = primec::semanticProgramInternCallTargetString(semanticProgram, "i32"),
      .resultErrorTypeId =
          primec::semanticProgramInternCallTargetString(semanticProgram, "FileError"),
      .onErrorHandlerPathId = primec::InvalidSymbolId,
      .onErrorErrorTypeId = primec::InvalidSymbolId,
  });
  publishFixtureCallableAndReturnRouting(semanticProgram);

  std::string error;
  CHECK(primec::ir_lowerer::validateSemanticProductResultMetadataCompleteness(
      &semanticProgram, error));
  CHECK(error.empty());

  semanticProgram.queryFacts.back().resultValueType.clear();
  error.clear();

  CHECK(primec::ir_lowerer::validateSemanticProductResultMetadataCompleteness(
      &semanticProgram, error));
  CHECK(error.empty());

  semanticProgram.queryFacts.back().resultValueType = "i64";
  semanticProgram.queryFacts.back().resultValueTypeId =
      primec::semanticProgramInternCallTargetString(semanticProgram, "i64");

  CHECK_FALSE(primec::ir_lowerer::validateSemanticProductResultMetadataCompleteness(
      &semanticProgram, error));
  CHECK(error == "stale semantic-product query result metadata: lookup");

  semanticProgram.queryFacts.back().resultValueType = "i32";
  semanticProgram.queryFacts.back().resultValueTypeId =
      primec::semanticProgramInternCallTargetString(semanticProgram, "i32");
  semanticProgram.queryFacts.back().resultTypeHasValue = false;
  error.clear();

  CHECK_FALSE(primec::ir_lowerer::validateSemanticProductResultMetadataCompleteness(
      &semanticProgram, error));
  CHECK(error == "stale semantic-product query result metadata: lookup");
}

TEST_CASE("ir lowerer rejects incomplete semantic-product try facts") {
  primec::Program program;

  primec::Definition mainDef;
  mainDef.fullPath = "/main";
  mainDef.semanticNodeId = 8401;
  primec::Expr sourceExpr;
  sourceExpr.kind = primec::Expr::Kind::Name;
  sourceExpr.name = "value";
  primec::Expr tryExpr;
  tryExpr.kind = primec::Expr::Kind::Call;
  tryExpr.name = "try";
  tryExpr.args.push_back(sourceExpr);
  tryExpr.semanticNodeId = 84;
  mainDef.statements.push_back(tryExpr);
  program.definitions.push_back(mainDef);

  primec::SemanticProgram semanticProgram;
  semanticProgram.entryPath = "/main";
  semanticProgram.directCallTargets.push_back(primec::SemanticProgramDirectCallTarget{
      .scopePath = "/main",
      .callName = "try",
      .sourceLine = 1,
      .sourceColumn = 1,
      .semanticNodeId = 84,
      .resolvedPathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/try"),
      .stdlibSurfaceId = std::nullopt,
  });
  semanticProgram.tryFacts.push_back(primec::SemanticProgramTryFact{
      .scopePath = "/main",
      .operandBindingTypeText = "Result<i32, FileError>",
      .operandReceiverBindingTypeText = "",
      .operandQueryTypeText = "Result<i32, FileError>",
      .valueType = "",
      .errorType = "FileError",
      .contextReturnKind = "return",
      .onErrorHandlerPath = "/handler",
      .onErrorErrorType = "FileError",
      .onErrorBoundArgCount = 1,
      .sourceLine = 1,
      .sourceColumn = 1,
      .semanticNodeId = 84,
      .operandResolvedPathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/lookup"),
  });
  semanticProgram.publishedRoutingLookups.tryFactIndicesByExpr.insert_or_assign(84, 0);
  primec::SemanticProgramCallableSummary callableSummary;
  callableSummary.fullPathId =
      primec::semanticProgramInternCallTargetString(semanticProgram, "/main");
  callableSummary.returnKind = "i32";
  semanticProgram.callableSummaries.push_back(std::move(callableSummary));

  primec::IrLowerer lowerer;
  primec::IrModule module;
  primec::DiagnosticSinkReport diagnosticInfo;
  std::string error;

  CHECK_FALSE(lowerer.lower(program, &semanticProgram, "/main", {}, {}, module, error, &diagnosticInfo));
  CHECK(error == "incomplete semantic-product try fact: try");
  CHECK(diagnosticInfo.message == error);
}

TEST_CASE("ir lowerer rejects missing semantic-product try operand resolved path id") {
  primec::Program program;

  primec::Definition mainDef;
  mainDef.fullPath = "/main";
  primec::Expr sourceExpr;
  sourceExpr.kind = primec::Expr::Kind::Name;
  sourceExpr.name = "value";
  primec::Expr tryExpr;
  tryExpr.kind = primec::Expr::Kind::Call;
  tryExpr.name = "try";
  tryExpr.args.push_back(sourceExpr);
  tryExpr.semanticNodeId = 8401;
  mainDef.statements.push_back(tryExpr);
  program.definitions.push_back(mainDef);

  primec::SemanticProgram semanticProgram;
  semanticProgram.entryPath = "/main";
  semanticProgram.directCallTargets.push_back(primec::SemanticProgramDirectCallTarget{
      .scopePath = "/main",
      .callName = "try",
      .sourceLine = 1,
      .sourceColumn = 1,
      .semanticNodeId = 8401,
      .resolvedPathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/try"),
      .stdlibSurfaceId = std::nullopt,
  });
  semanticProgram.tryFacts.push_back(primec::SemanticProgramTryFact{
      .scopePath = "/main",
      .operandBindingTypeText = "Result<i32, FileError>",
      .operandReceiverBindingTypeText = "",
      .operandQueryTypeText = "Result<i32, FileError>",
      .valueType = "i32",
      .errorType = "FileError",
      .contextReturnKind = "return",
      .onErrorHandlerPath = "/handler",
      .onErrorErrorType = "FileError",
      .onErrorBoundArgCount = 1,
      .sourceLine = 1,
      .sourceColumn = 1,
      .semanticNodeId = 8401,
      .operandResolvedPathId = primec::InvalidSymbolId,
  });
  semanticProgram.publishedRoutingLookups.tryFactIndicesByExpr.insert_or_assign(8401, 0);
  primec::SemanticProgramCallableSummary callableSummary;
  callableSummary.fullPathId =
      primec::semanticProgramInternCallTargetString(semanticProgram, "/main");
  callableSummary.returnKind = "i32";
  semanticProgram.callableSummaries.push_back(std::move(callableSummary));

  primec::IrLowerer lowerer;
  primec::IrModule module;
  primec::DiagnosticSinkReport diagnosticInfo;
  std::string error;

  CHECK_FALSE(lowerer.lower(program, &semanticProgram, "/main", {}, {}, module, error, &diagnosticInfo));
  CHECK(error == "missing semantic-product try operand resolved path id: try");
  CHECK(diagnosticInfo.message == error);
}

TEST_CASE("ir lowerer rejects stale semantic-product try operand metadata") {
  primec::SemanticProgram semanticProgram;
  semanticProgram.entryPath = "/main";
  semanticProgram.tryFacts.push_back(primec::SemanticProgramTryFact{
      .scopePath = "/main",
      .operandBindingTypeText = "Result<i32, FileError>",
      .operandReceiverBindingTypeText = "Map<string, Result<i32, FileError>>",
      .operandQueryTypeText = "Result<i32, FileError>",
      .valueType = "i32",
      .errorType = "FileError",
      .contextReturnKind = "return",
      .onErrorHandlerPath = "",
      .onErrorErrorType = "",
      .onErrorBoundArgCount = 0,
      .sourceLine = 1,
      .sourceColumn = 1,
      .semanticNodeId = 8402,
      .operandResolvedPathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/lookup"),
      .operandBindingTypeTextId =
          primec::semanticProgramInternCallTargetString(semanticProgram, "Result<i32, FileError>"),
      .operandReceiverBindingTypeTextId = primec::semanticProgramInternCallTargetString(
          semanticProgram, "Map<string, Result<i32, FileError>>"),
      .operandQueryTypeTextId =
          primec::semanticProgramInternCallTargetString(semanticProgram, "Result<i32, FileError>"),
  });

  std::string error;
  CHECK(primec::ir_lowerer::validateSemanticProductResultMetadataCompleteness(
      &semanticProgram, error));
  CHECK(error.empty());

  semanticProgram.tryFacts.back().operandBindingTypeTextId =
      static_cast<primec::SymbolId>(semanticProgram.callTargetStringTable.size() + 1u);

  CHECK_FALSE(primec::ir_lowerer::validateSemanticProductResultMetadataCompleteness(
      &semanticProgram, error));
  CHECK(error == "missing semantic-product try operand binding type id: try");

  semanticProgram.tryFacts.back().operandBindingTypeTextId =
      primec::semanticProgramInternCallTargetString(semanticProgram, "Result<i64, FileError>");
  error.clear();

  CHECK_FALSE(primec::ir_lowerer::validateSemanticProductResultMetadataCompleteness(
      &semanticProgram, error));
  CHECK(error == "stale semantic-product try operand binding type metadata: try");

  semanticProgram.tryFacts.back().operandBindingTypeTextId =
      primec::semanticProgramInternCallTargetString(semanticProgram, "Result<i32, FileError>");
  semanticProgram.tryFacts.back().operandReceiverBindingTypeTextId =
      primec::semanticProgramInternCallTargetString(semanticProgram, "OtherReceiver");
  error.clear();

  CHECK_FALSE(primec::ir_lowerer::validateSemanticProductResultMetadataCompleteness(
      &semanticProgram, error));
  CHECK(error == "stale semantic-product try operand receiver binding type metadata: try");

  semanticProgram.tryFacts.back().operandReceiverBindingTypeTextId =
      primec::semanticProgramInternCallTargetString(
          semanticProgram, "Map<string, Result<i32, FileError>>");
  semanticProgram.tryFacts.back().operandQueryTypeTextId =
      primec::semanticProgramInternCallTargetString(semanticProgram, "Result<i64, FileError>");
  error.clear();

  CHECK_FALSE(primec::ir_lowerer::validateSemanticProductResultMetadataCompleteness(
      &semanticProgram, error));
  CHECK(error == "stale semantic-product try operand query type metadata: try");
}

TEST_CASE("ir lowerer rejects stale semantic-product try result metadata") {
  primec::SemanticProgram semanticProgram;
  semanticProgram.entryPath = "/main";
  semanticProgram.tryFacts.push_back(primec::SemanticProgramTryFact{
      .scopePath = "/main",
      .operandBindingTypeText = "Result<i32, FileError>",
      .operandReceiverBindingTypeText = "",
      .operandQueryTypeText = "Result<i32, FileError>",
      .valueType = "i32",
      .errorType = "FileError",
      .contextReturnKind = "return",
      .onErrorHandlerPath = "",
      .onErrorErrorType = "",
      .onErrorBoundArgCount = 0,
      .sourceLine = 1,
      .sourceColumn = 1,
      .semanticNodeId = 8404,
      .operandResolvedPathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/lookup"),
      .valueTypeId = primec::semanticProgramInternCallTargetString(semanticProgram, "i32"),
      .errorTypeId = primec::semanticProgramInternCallTargetString(semanticProgram, "FileError"),
  });
  semanticProgram.callableSummaries.push_back(primec::SemanticProgramCallableSummary{
      .returnKind = "return",
      .isCompute = false,
      .isUnsafe = false,
      .activeEffects = {},
      .activeCapabilities = {},
      .hasResultType = true,
      .resultTypeHasValue = true,
      .resultValueType = "i32",
      .resultErrorType = "FileError",
      .hasOnError = false,
      .onErrorHandlerPath = "",
      .onErrorErrorType = "",
      .onErrorBoundArgCount = 0,
      .semanticNodeId = 0,
      .provenanceHandle = 0,
      .fullPathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/lookup"),
      .returnKindId = primec::InvalidSymbolId,
      .activeEffectIds = {},
      .activeCapabilityIds = {},
      .resultValueTypeId = primec::semanticProgramInternCallTargetString(semanticProgram, "i32"),
      .resultErrorTypeId =
          primec::semanticProgramInternCallTargetString(semanticProgram, "FileError"),
      .onErrorHandlerPathId = primec::InvalidSymbolId,
      .onErrorErrorTypeId = primec::InvalidSymbolId,
  });
  publishFixtureCallableAndReturnRouting(semanticProgram);

  std::string error;
  CHECK(primec::ir_lowerer::validateSemanticProductResultMetadataCompleteness(
      &semanticProgram, error));
  CHECK(error.empty());

  semanticProgram.tryFacts.back().valueType = "i64";
  semanticProgram.tryFacts.back().errorType = "OtherError";
  error.clear();
  CHECK(primec::ir_lowerer::validateSemanticProductResultMetadataCompleteness(
      &semanticProgram, error));
  CHECK(error.empty());

  semanticProgram.tryFacts.back().valueType = "i64";
  semanticProgram.tryFacts.back().valueTypeId =
      primec::semanticProgramInternCallTargetString(semanticProgram, "i64");

  CHECK_FALSE(primec::ir_lowerer::validateSemanticProductResultMetadataCompleteness(
      &semanticProgram, error));
  CHECK(error == "stale semantic-product try result metadata: try");

  semanticProgram.tryFacts.back().valueType = "i32";
  semanticProgram.tryFacts.back().valueTypeId =
      primec::semanticProgramInternCallTargetString(semanticProgram, "i32");
  semanticProgram.tryFacts.back().errorType = "OtherError";
  semanticProgram.tryFacts.back().errorTypeId =
      primec::semanticProgramInternCallTargetString(semanticProgram, "OtherError");
  error.clear();

  CHECK_FALSE(primec::ir_lowerer::validateSemanticProductResultMetadataCompleteness(
      &semanticProgram, error));
  CHECK(error == "stale semantic-product try result metadata: try");
}

TEST_CASE("ir lowerer rejects stale semantic-product try context return kind") {
  primec::Program program;

  primec::Definition mainDef;
  mainDef.fullPath = "/main";
  primec::Expr sourceExpr;
  sourceExpr.kind = primec::Expr::Kind::Name;
  sourceExpr.name = "value";
  primec::Expr tryExpr;
  tryExpr.kind = primec::Expr::Kind::Call;
  tryExpr.name = "try";
  tryExpr.args.push_back(sourceExpr);
  tryExpr.semanticNodeId = 8403;
  mainDef.statements.push_back(tryExpr);
  program.definitions.push_back(mainDef);

  primec::SemanticProgram semanticProgram;
  semanticProgram.entryPath = "/main";
  semanticProgram.directCallTargets.push_back(primec::SemanticProgramDirectCallTarget{
      .scopePath = "/main",
      .callName = "try",
      .sourceLine = 1,
      .sourceColumn = 1,
      .semanticNodeId = 8403,
      .resolvedPathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/try"),
      .stdlibSurfaceId = std::nullopt,
  });
  semanticProgram.tryFacts.push_back(primec::SemanticProgramTryFact{
      .scopePath = "/main",
      .operandBindingTypeText = "Result<i32, FileError>",
      .operandReceiverBindingTypeText = "",
      .operandQueryTypeText = "Result<i32, FileError>",
      .valueType = "i32",
      .errorType = "FileError",
      .contextReturnKind = "return",
      .onErrorHandlerPath = "",
      .onErrorErrorType = "",
      .onErrorBoundArgCount = 0,
      .sourceLine = 1,
      .sourceColumn = 1,
      .semanticNodeId = 8403,
      .scopePathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/main"),
      .operandResolvedPathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/lookup"),
      .contextReturnKindId = primec::semanticProgramInternCallTargetString(semanticProgram, "return"),
  });
  semanticProgram.publishedRoutingLookups.tryFactIndicesByExpr.insert_or_assign(8403, 0);
  semanticProgram.callableSummaries.push_back(primec::SemanticProgramCallableSummary{
      .returnKind = "i32",
      .isCompute = false,
      .isUnsafe = false,
      .activeEffects = {},
      .activeCapabilities = {},
      .hasResultType = false,
      .resultTypeHasValue = false,
      .resultValueType = "",
      .resultErrorType = "",
      .hasOnError = false,
      .onErrorHandlerPath = "",
      .onErrorErrorType = "",
      .onErrorBoundArgCount = 0,
      .semanticNodeId = 0,
      .provenanceHandle = 0,
      .fullPathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/main"),
      .returnKindId = primec::semanticProgramInternCallTargetString(semanticProgram, "i32"),
      .activeEffectIds = {},
      .activeCapabilityIds = {},
      .resultValueTypeId = primec::InvalidSymbolId,
      .resultErrorTypeId = primec::InvalidSymbolId,
      .onErrorHandlerPathId = primec::InvalidSymbolId,
      .onErrorErrorTypeId = primec::InvalidSymbolId,
  });
  publishFixtureCallableAndReturnRouting(semanticProgram);

  primec::IrLowerer lowerer;
  primec::IrModule module;
  primec::DiagnosticSinkReport diagnosticInfo;
  std::string error;

  CHECK_FALSE(lowerer.lower(program, &semanticProgram, "/main", {}, {}, module, error, &diagnosticInfo));
  CHECK(error == "stale semantic-product try context return kind: try");
  CHECK(diagnosticInfo.message == error);
}

TEST_CASE("ir lowerer rejects stale semantic-product try on_error facts") {
  primec::Program program;

  primec::Definition mainDef;
  mainDef.fullPath = "/main";
  mainDef.semanticNodeId = 8401;
  primec::Expr sourceExpr;
  sourceExpr.kind = primec::Expr::Kind::Name;
  sourceExpr.name = "value";
  primec::Expr tryExpr;
  tryExpr.kind = primec::Expr::Kind::Call;
  tryExpr.name = "try";
  tryExpr.args.push_back(sourceExpr);
  tryExpr.semanticNodeId = 8402;
  mainDef.statements.push_back(tryExpr);
  program.definitions.push_back(mainDef);

  primec::SemanticProgram semanticProgram;
  semanticProgram.entryPath = "/main";
  semanticProgram.directCallTargets.push_back(primec::SemanticProgramDirectCallTarget{
      .scopePath = "/main",
      .callName = "try",
      .sourceLine = 1,
      .sourceColumn = 1,
      .semanticNodeId = 8402,
      .resolvedPathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/try"),
      .stdlibSurfaceId = std::nullopt,
  });
  semanticProgram.tryFacts.push_back(primec::SemanticProgramTryFact{
      .scopePath = "/main",
      .operandBindingTypeText = "Result<i32, FileError>",
      .operandReceiverBindingTypeText = "",
      .operandQueryTypeText = "Result<i32, FileError>",
      .valueType = "i32",
      .errorType = "FileError",
      .contextReturnKind = "return",
      .onErrorHandlerPath = "/stale_handler",
      .onErrorErrorType = "FileError",
      .onErrorBoundArgCount = 1,
      .sourceLine = 1,
      .sourceColumn = 1,
      .semanticNodeId = 8402,
      .scopePathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/main"),
      .operandResolvedPathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/lookup"),
  });
  semanticProgram.publishedRoutingLookups.tryFactIndicesByExpr.insert_or_assign(8402, 0);
  semanticProgram.callableSummaries.push_back(primec::SemanticProgramCallableSummary{
      .returnKind = "i32",
      .isCompute = false,
      .isUnsafe = false,
      .activeEffects = {},
      .activeCapabilities = {},
      .hasResultType = false,
      .resultTypeHasValue = false,
      .resultValueType = "",
      .resultErrorType = "",
      .hasOnError = true,
      .onErrorHandlerPath = "/handler",
      .onErrorErrorType = "FileError",
      .onErrorBoundArgCount = 1,
      .semanticNodeId = 8401,
      .provenanceHandle = 0,
      .fullPathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/main"),
      .returnKindId = primec::InvalidSymbolId,
      .activeEffectIds = {},
      .activeCapabilityIds = {},
      .resultValueTypeId = primec::InvalidSymbolId,
      .resultErrorTypeId = primec::InvalidSymbolId,
      .onErrorHandlerPathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/handler"),
      .onErrorErrorTypeId = primec::semanticProgramInternCallTargetString(semanticProgram, "FileError"),
  });
  semanticProgram.onErrorFacts.push_back(primec::SemanticProgramOnErrorFact{
      .definitionPath = "/main",
      .returnKind = "i32",
      .errorType = "FileError",
      .boundArgCount = 1,
      .boundArgTexts = {"error"},
      .returnResultHasValue = false,
      .returnResultValueType = "",
      .returnResultErrorType = "",
      .semanticNodeId = 8401,
      .provenanceHandle = 0,
      .definitionPathId =
          primec::semanticProgramInternCallTargetString(semanticProgram, "/main"),
      .returnKindId =
          primec::semanticProgramInternCallTargetString(semanticProgram, "i32"),
      .handlerPathId =
          primec::semanticProgramInternCallTargetString(semanticProgram, "/handler"),
      .errorTypeId =
          primec::semanticProgramInternCallTargetString(semanticProgram, "FileError"),
      .boundArgTextIds = {
          primec::semanticProgramInternCallTargetString(semanticProgram, "error"),
      },
      .returnResultValueTypeId = primec::InvalidSymbolId,
      .returnResultErrorTypeId = primec::InvalidSymbolId,
  });
  semanticProgram.publishedRoutingLookups.onErrorFactIndicesByDefinitionId
      .insert_or_assign(8401, 0);
  publishFixtureCallableAndReturnRouting(semanticProgram);

  primec::IrLowerer lowerer;
  primec::IrModule module;
  primec::DiagnosticSinkReport diagnosticInfo;
  std::string error;

  CHECK_FALSE(lowerer.lower(program, &semanticProgram, "/main", {}, {}, module, error, &diagnosticInfo));
  CHECK(error == "missing semantic-product try on_error handler path id: try");
  CHECK(diagnosticInfo.message == error);

  semanticProgram.tryFacts.back().onErrorHandlerPathId =
      primec::semanticProgramInternCallTargetString(semanticProgram, "/stale_handler");
  semanticProgram.tryFacts.back().onErrorErrorTypeId =
      primec::semanticProgramInternCallTargetString(semanticProgram, "FileError");
  error.clear();
  diagnosticInfo = {};
  CHECK_FALSE(lowerer.lower(program, &semanticProgram, "/main", {}, {}, module, error, &diagnosticInfo));
  CHECK(error == "stale semantic-product try on_error fact: try");
  CHECK(diagnosticInfo.message == error);

  semanticProgram.tryFacts.back().onErrorHandlerPath = "/handler";
  semanticProgram.tryFacts.back().onErrorHandlerPathId =
      primec::semanticProgramInternCallTargetString(semanticProgram, "/handler");
  semanticProgram.tryFacts.back().onErrorBoundArgCount = 2;
  error.clear();
  diagnosticInfo = {};
  CHECK_FALSE(lowerer.lower(program, &semanticProgram, "/main", {}, {}, module, error, &diagnosticInfo));
  CHECK(error == "stale semantic-product try on_error fact: try");
  CHECK(diagnosticInfo.message == error);
}

TEST_CASE("ir lowerer rejects missing semantic-product on_error handler path id") {
  primec::Program program;

  primec::Definition handlerDef;
  handlerDef.fullPath = "/handler";
  handlerDef.semanticNodeId = 9200;
  program.definitions.push_back(handlerDef);

  primec::Definition mainDef;
  mainDef.fullPath = "/main";
  mainDef.semanticNodeId = 9201;
  program.definitions.push_back(mainDef);

  primec::SemanticProgram semanticProgram;
  semanticProgram.entryPath = "/main";
  semanticProgram.callableSummaries.push_back(primec::SemanticProgramCallableSummary{
      .isExecution = false,
      .returnKind = "void",
      .isCompute = false,
      .isUnsafe = false,
      .activeEffects = {},
      .activeCapabilities = {},
      .hasResultType = false,
      .resultTypeHasValue = false,
      .resultValueType = "",
      .resultErrorType = "",
      .hasOnError = false,
      .onErrorHandlerPath = "",
      .onErrorErrorType = "",
      .onErrorBoundArgCount = 0,
      .semanticNodeId = 9200,
      .provenanceHandle = 0,
      .fullPathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/handler"),
  });
  semanticProgram.callableSummaries.push_back(primec::SemanticProgramCallableSummary{
      .isExecution = false,
      .returnKind = "void",
      .isCompute = false,
      .isUnsafe = false,
      .activeEffects = {},
      .activeCapabilities = {},
      .hasResultType = false,
      .resultTypeHasValue = false,
      .resultValueType = "",
      .resultErrorType = "",
      .hasOnError = true,
      .onErrorHandlerPath = "/handler",
      .onErrorErrorType = "FileError",
      .onErrorBoundArgCount = 0,
      .semanticNodeId = 9201,
      .provenanceHandle = 0,
      .fullPathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/main"),
  });
  semanticProgram.returnFacts.push_back(primec::SemanticProgramReturnFact{
      .returnKind = "void",
      .structPath = "",
      .bindingTypeText = "void",
      .isMutable = false,
      .isEntryArgString = false,
      .isUnsafeReference = false,
      .referenceRoot = "",
      .sourceLine = 0,
      .sourceColumn = 0,
      .semanticNodeId = 9201,
      .definitionPathId =
          primec::semanticProgramInternCallTargetString(semanticProgram, "/main"),
  });
  semanticProgram.publishedRoutingLookups.returnFactIndicesByDefinitionId
      .insert_or_assign(9201, semanticProgram.returnFacts.size() - 1);
  semanticProgram.onErrorFacts.push_back(primec::SemanticProgramOnErrorFact{
      .definitionPath = "/main",
      .returnKind = "void",
      .errorType = "FileError",
      .boundArgCount = 0,
      .boundArgTexts = {},
      .returnResultHasValue = false,
      .returnResultValueType = "",
      .returnResultErrorType = "",
      .semanticNodeId = 9201,
      .provenanceHandle = 0,
      .definitionPathId =
          primec::semanticProgramInternCallTargetString(semanticProgram, "/main"),
      .returnKindId =
          primec::semanticProgramInternCallTargetString(semanticProgram, "void"),
      .handlerPathId = primec::InvalidSymbolId,
      .errorTypeId =
          primec::semanticProgramInternCallTargetString(semanticProgram, "FileError"),
  });
  semanticProgram.publishedRoutingLookups.onErrorFactIndicesByDefinitionId
      .insert_or_assign(9201, 0);

  primec::IrLowerer lowerer;
  primec::IrModule module;
  primec::DiagnosticSinkReport diagnosticInfo;
  std::string error;

  CHECK_FALSE(lowerer.lower(program, &semanticProgram, "/main", {}, {}, module, error, &diagnosticInfo));
  CHECK(error == "missing semantic-product on_error handler path id: /main");
  CHECK(diagnosticInfo.message == error);
}

TEST_CASE("ir lowerer rejects missing semantic-product on_error facts") {
  primec::Program program;

  primec::Definition handlerDef;
  handlerDef.fullPath = "/handler";
  handlerDef.semanticNodeId = 90;
  program.definitions.push_back(handlerDef);

  primec::Definition mainDef;
  mainDef.fullPath = "/main";
  mainDef.semanticNodeId = 91;
  program.definitions.push_back(mainDef);

  primec::SemanticProgram semanticProgram;
  semanticProgram.entryPath = "/main";
  semanticProgram.callableSummaries.push_back(primec::SemanticProgramCallableSummary{
      .isExecution = false,
      .returnKind = "void",
      .isCompute = false,
      .isUnsafe = false,
      .activeEffects = {},
      .activeCapabilities = {},
      .hasResultType = false,
      .resultTypeHasValue = false,
      .resultValueType = "",
      .resultErrorType = "",
      .hasOnError = false,
      .onErrorHandlerPath = "",
      .onErrorErrorType = "",
      .onErrorBoundArgCount = 0,
      .semanticNodeId = 90,
      .provenanceHandle = 0,
      .fullPathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/handler"),
  });
  semanticProgram.callableSummaries.push_back(primec::SemanticProgramCallableSummary{
      .isExecution = false,
      .returnKind = "void",
      .isCompute = false,
      .isUnsafe = false,
      .activeEffects = {},
      .activeCapabilities = {},
      .hasResultType = false,
      .resultTypeHasValue = false,
      .resultValueType = "",
      .resultErrorType = "",
      .hasOnError = true,
      .onErrorHandlerPath = "/handler",
      .onErrorErrorType = "FileError",
      .onErrorBoundArgCount = 0,
      .semanticNodeId = 91,
      .provenanceHandle = 0,
      .fullPathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/main"),
  });
  semanticProgram.returnFacts.push_back(primec::SemanticProgramReturnFact{
      .returnKind = "void",
      .structPath = "",
      .bindingTypeText = "void",
      .isMutable = false,
      .isEntryArgString = false,
      .isUnsafeReference = false,
      .referenceRoot = "",
      .sourceLine = 0,
      .sourceColumn = 0,
      .semanticNodeId = 91,
      .definitionPathId =
          primec::semanticProgramInternCallTargetString(semanticProgram, "/semantic/main"),
  });
  semanticProgram.publishedRoutingLookups.returnFactIndicesByDefinitionId
      .insert_or_assign(91, semanticProgram.returnFacts.size() - 1);

  primec::IrLowerer lowerer;
  primec::IrModule module;
  primec::DiagnosticSinkReport diagnosticInfo;
  std::string error;

  CHECK_FALSE(lowerer.lower(program, &semanticProgram, "/main", {}, {}, module, error, &diagnosticInfo));
  CHECK(error == "missing semantic-product on_error fact: /main");
  CHECK(diagnosticInfo.message == error);
}

TEST_CASE("ir lowerer rejects stale semantic-product on_error facts") {
  primec::Program program;

  primec::Definition handlerDef;
  handlerDef.fullPath = "/handler";
  handlerDef.semanticNodeId = 94;
  program.definitions.push_back(handlerDef);

  primec::Definition mainDef;
  mainDef.fullPath = "/main";
  mainDef.semanticNodeId = 95;
  program.definitions.push_back(mainDef);

  primec::SemanticProgram semanticProgram;
  semanticProgram.entryPath = "/main";
  semanticProgram.callableSummaries.push_back(primec::SemanticProgramCallableSummary{
      .isExecution = false,
      .returnKind = "void",
      .isCompute = false,
      .isUnsafe = false,
      .activeEffects = {},
      .activeCapabilities = {},
      .hasResultType = false,
      .resultTypeHasValue = false,
      .resultValueType = "",
      .resultErrorType = "",
      .hasOnError = false,
      .onErrorHandlerPath = "",
      .onErrorErrorType = "",
      .onErrorBoundArgCount = 0,
      .semanticNodeId = 94,
      .provenanceHandle = 0,
      .fullPathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/handler"),
  });
  semanticProgram.callableSummaries.push_back(primec::SemanticProgramCallableSummary{
      .isExecution = false,
      .returnKind = "void",
      .isCompute = false,
      .isUnsafe = false,
      .activeEffects = {},
      .activeCapabilities = {},
      .hasResultType = false,
      .resultTypeHasValue = false,
      .resultValueType = "",
      .resultErrorType = "",
      .hasOnError = true,
      .onErrorHandlerPath = "/handler",
      .onErrorErrorType = "FileError",
      .onErrorBoundArgCount = 0,
      .semanticNodeId = 95,
      .provenanceHandle = 0,
      .fullPathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/main"),
  });
  semanticProgram.returnFacts.push_back(primec::SemanticProgramReturnFact{
      .returnKind = "void",
      .structPath = "",
      .bindingTypeText = "void",
      .isMutable = false,
      .isEntryArgString = false,
      .isUnsafeReference = false,
      .referenceRoot = "",
      .sourceLine = 0,
      .sourceColumn = 0,
      .semanticNodeId = 95,
      .definitionPathId =
          primec::semanticProgramInternCallTargetString(semanticProgram, "/main"),
  });
  semanticProgram.publishedRoutingLookups.returnFactIndicesByDefinitionId
      .insert_or_assign(95, semanticProgram.returnFacts.size() - 1);
  semanticProgram.onErrorFacts.push_back(primec::SemanticProgramOnErrorFact{
      .definitionPath = "/main",
      .returnKind = "void",
      .errorType = "ContainerError",
      .boundArgCount = 0,
      .boundArgTexts = {},
      .returnResultHasValue = false,
      .returnResultValueType = "",
      .returnResultErrorType = "",
      .semanticNodeId = 95,
      .definitionPathId =
          primec::semanticProgramInternCallTargetString(semanticProgram, "/main"),
      .returnKindId =
          primec::semanticProgramInternCallTargetString(semanticProgram, "void"),
      .handlerPathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/handler"),
      .errorTypeId =
          primec::semanticProgramInternCallTargetString(semanticProgram, "ContainerError"),
  });
  semanticProgram.publishedRoutingLookups.onErrorFactIndicesByDefinitionId
      .insert_or_assign(95, 0);

  primec::IrLowerer lowerer;
  primec::IrModule module;
  primec::DiagnosticSinkReport diagnosticInfo;
  std::string error;

  CHECK_FALSE(lowerer.lower(program, &semanticProgram, "/main", {}, {}, module, error, &diagnosticInfo));
  CHECK(error == "stale semantic-product on_error fact: /main");
  CHECK(diagnosticInfo.message == error);
}

TEST_CASE("ir lowerer rejects stale semantic-product on_error result metadata") {
  primec::Program program;

  primec::Definition handlerDef;
  handlerDef.fullPath = "/handler";
  handlerDef.semanticNodeId = 9600;
  program.definitions.push_back(handlerDef);

  primec::Definition mainDef;
  mainDef.fullPath = "/main";
  mainDef.semanticNodeId = 9601;
  program.definitions.push_back(mainDef);

  primec::SemanticProgram semanticProgram;
  semanticProgram.entryPath = "/main";
  semanticProgram.callableSummaries.push_back(primec::SemanticProgramCallableSummary{
      .isExecution = false,
      .returnKind = "void",
      .isCompute = false,
      .isUnsafe = false,
      .activeEffects = {},
      .activeCapabilities = {},
      .hasResultType = false,
      .resultTypeHasValue = false,
      .resultValueType = "",
      .resultErrorType = "",
      .hasOnError = false,
      .onErrorHandlerPath = "",
      .onErrorErrorType = "",
      .onErrorBoundArgCount = 0,
      .semanticNodeId = 9600,
      .provenanceHandle = 0,
      .fullPathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/handler"),
  });
  semanticProgram.callableSummaries.push_back(primec::SemanticProgramCallableSummary{
      .isExecution = false,
      .returnKind = "return",
      .isCompute = false,
      .isUnsafe = false,
      .activeEffects = {},
      .activeCapabilities = {},
      .hasResultType = true,
      .resultTypeHasValue = true,
      .resultValueType = "i32",
      .resultErrorType = "FileError",
      .hasOnError = true,
      .onErrorHandlerPath = "/handler",
      .onErrorErrorType = "FileError",
      .onErrorBoundArgCount = 0,
      .semanticNodeId = 9601,
      .provenanceHandle = 0,
      .fullPathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/main"),
      .returnKindId = primec::semanticProgramInternCallTargetString(semanticProgram, "return"),
      .resultValueTypeId = primec::semanticProgramInternCallTargetString(semanticProgram, "i32"),
      .resultErrorTypeId =
          primec::semanticProgramInternCallTargetString(semanticProgram, "FileError"),
      .onErrorHandlerPathId =
          primec::semanticProgramInternCallTargetString(semanticProgram, "/handler"),
      .onErrorErrorTypeId =
          primec::semanticProgramInternCallTargetString(semanticProgram, "FileError"),
  });
  semanticProgram.returnFacts.push_back(primec::SemanticProgramReturnFact{
      .returnKind = "return",
      .structPath = "",
      .bindingTypeText = "Result<i32, FileError>",
      .isMutable = false,
      .isEntryArgString = false,
      .isUnsafeReference = false,
      .referenceRoot = "",
      .sourceLine = 0,
      .sourceColumn = 0,
      .semanticNodeId = 9601,
      .definitionPathId =
          primec::semanticProgramInternCallTargetString(semanticProgram, "/main"),
      .returnKindId = primec::semanticProgramInternCallTargetString(semanticProgram, "return"),
  });
  semanticProgram.publishedRoutingLookups.returnFactIndicesByDefinitionId
      .insert_or_assign(9601, semanticProgram.returnFacts.size() - 1);
  semanticProgram.onErrorFacts.push_back(primec::SemanticProgramOnErrorFact{
      .definitionPath = "/main",
      .returnKind = "return",
      .errorType = "FileError",
      .boundArgCount = 0,
      .boundArgTexts = {},
      .returnResultHasValue = true,
      .returnResultValueType = "i32",
      .returnResultErrorType = "FileError",
      .semanticNodeId = 9601,
      .definitionPathId =
          primec::semanticProgramInternCallTargetString(semanticProgram, "/main"),
      .returnKindId = primec::semanticProgramInternCallTargetString(semanticProgram, "return"),
      .handlerPathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/handler"),
      .errorTypeId = primec::semanticProgramInternCallTargetString(semanticProgram, "FileError"),
      .returnResultValueTypeId =
          primec::semanticProgramInternCallTargetString(semanticProgram, "i32"),
      .returnResultErrorTypeId =
          primec::semanticProgramInternCallTargetString(semanticProgram, "FileError"),
  });
  semanticProgram.publishedRoutingLookups.onErrorFactIndicesByDefinitionId
      .insert_or_assign(9601, 0);

  primec::IrLowerer lowerer;
  primec::IrModule module;
  primec::DiagnosticSinkReport diagnosticInfo;
  std::string error;

  semanticProgram.onErrorFacts.back().returnResultValueType = "i64";
  semanticProgram.onErrorFacts.back().returnResultValueTypeId =
      primec::semanticProgramInternCallTargetString(semanticProgram, "i64");
  CHECK_FALSE(lowerer.lower(program, &semanticProgram, "/main", {}, {}, module, error, &diagnosticInfo));
  CHECK(error == "stale semantic-product on_error result metadata: /main");
  CHECK(diagnosticInfo.message == error);

  semanticProgram.onErrorFacts.back().returnResultValueType = "i32";
  semanticProgram.onErrorFacts.back().returnResultValueTypeId =
      primec::semanticProgramInternCallTargetString(semanticProgram, "i32");
  semanticProgram.onErrorFacts.back().returnResultHasValue = false;
  error.clear();
  diagnosticInfo = {};
  CHECK_FALSE(lowerer.lower(program, &semanticProgram, "/main", {}, {}, module, error, &diagnosticInfo));
  CHECK(error == "stale semantic-product on_error result metadata: /main");
  CHECK(diagnosticInfo.message == error);
}

TEST_CASE("ir lowerer rejects mismatched semantic-product on_error bound args") {
  primec::Program program;

  primec::Definition handlerDef;
  handlerDef.fullPath = "/handler";
  handlerDef.semanticNodeId = 92;
  program.definitions.push_back(handlerDef);

  primec::Definition mainDef;
  mainDef.fullPath = "/main";
  mainDef.semanticNodeId = 93;
  program.definitions.push_back(mainDef);

  primec::SemanticProgram semanticProgram;
  semanticProgram.entryPath = "/main";
  semanticProgram.callableSummaries.push_back(primec::SemanticProgramCallableSummary{
      .isExecution = false,
      .returnKind = "void",
      .isCompute = false,
      .isUnsafe = false,
      .activeEffects = {},
      .activeCapabilities = {},
      .hasResultType = false,
      .resultTypeHasValue = false,
      .resultValueType = "",
      .resultErrorType = "",
      .hasOnError = false,
      .onErrorHandlerPath = "",
      .onErrorErrorType = "",
      .onErrorBoundArgCount = 0,
      .semanticNodeId = 92,
      .provenanceHandle = 0,
      .fullPathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/handler"),
  });
  semanticProgram.callableSummaries.push_back(primec::SemanticProgramCallableSummary{
      .isExecution = false,
      .returnKind = "void",
      .isCompute = false,
      .isUnsafe = false,
      .activeEffects = {},
      .activeCapabilities = {},
      .hasResultType = false,
      .resultTypeHasValue = false,
      .resultValueType = "",
      .resultErrorType = "",
      .hasOnError = true,
      .onErrorHandlerPath = "/handler",
      .onErrorErrorType = "FileError",
      .onErrorBoundArgCount = 2,
      .semanticNodeId = 93,
      .provenanceHandle = 0,
      .fullPathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/main"),
  });
  semanticProgram.returnFacts.push_back(primec::SemanticProgramReturnFact{
      .returnKind = "void",
      .structPath = "",
      .bindingTypeText = "void",
      .isMutable = false,
      .isEntryArgString = false,
      .isUnsafeReference = false,
      .referenceRoot = "",
      .sourceLine = 0,
      .sourceColumn = 0,
      .semanticNodeId = 93,
      .definitionPathId =
          primec::semanticProgramInternCallTargetString(semanticProgram, "/main"),
  });
  semanticProgram.publishedRoutingLookups.returnFactIndicesByDefinitionId
      .insert_or_assign(93, semanticProgram.returnFacts.size() - 1);
  semanticProgram.onErrorFacts.push_back(primec::SemanticProgramOnErrorFact{
      .definitionPath = "/main",
      .returnKind = "void",
      .errorType = "FileError",
      .boundArgCount = 2,
      .boundArgTexts = {"1i32"},
      .returnResultHasValue = false,
      .returnResultValueType = "",
      .returnResultErrorType = "",
      .semanticNodeId = 93,
      .handlerPathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/handler"),
  });
  semanticProgram.publishedRoutingLookups.onErrorFactIndicesByDefinitionId
      .insert_or_assign(93, 0);

  primec::IrLowerer lowerer;
  primec::IrModule module;
  primec::DiagnosticSinkReport diagnosticInfo;
  std::string error;

  CHECK_FALSE(lowerer.lower(program, &semanticProgram, "/main", {}, {}, module, error, &diagnosticInfo));
  CHECK(error == "semantic-product on_error bound arg mismatch on /main");
  CHECK(diagnosticInfo.message == error);
}

TEST_CASE("cli driver preserves parse-stage diagnostic context") {
  primec::CompilePipelineFailureResult output;
  output.filteredSource = "main() { return(1i32) }";
  output.failure.stage = primec::CompilePipelineErrorStage::Parse;
  output.failure.message = "raw parse failure";
  output.failure.diagnosticInfo.message = "expected token";

  const primec::CliFailure failure = primec::describeCompilePipelineFailure(output);

  CHECK(failure.code == primec::DiagnosticCode::ParseError);
  CHECK(failure.plainPrefix == "Parse error: ");
  CHECK(failure.message == "raw parse failure");
  CHECK(failure.exitCode == 2);
  CHECK(failure.notes == std::vector<std::string>{"stage: parse"});
  REQUIRE(failure.diagnosticInfo.has_value());
  CHECK(failure.diagnosticInfo->message == output.failure.diagnosticInfo.message);
  REQUIRE(failure.sourceText.has_value());
  CHECK(*failure.sourceText == output.filteredSource);
}

TEST_CASE("cli driver reports semantic-product availability on post-semantics failures") {
  primec::CompilePipelineFailureResult output;
  output.filteredSource = "import /std/gfx/*\n[return<int>]\nmain(){ return(0i32) }\n";
  output.semanticProgram.entryPath = "/main";
  output.hasSemanticProgram = true;
  output.failure.stage = primec::CompilePipelineErrorStage::Semantic;
  output.failure.message = "graphics stdlib runtime substrate unavailable for glsl target: /std/gfx/*";
  output.failure.diagnosticInfo.message = output.failure.message;

  const primec::CliFailure failure = primec::describeCompilePipelineFailure(output);

  CHECK(failure.code == primec::DiagnosticCode::SemanticError);
  CHECK(failure.plainPrefix == "Semantic error: ");
  CHECK(failure.message == output.failure.message);
  CHECK(failure.notes == std::vector<std::string>({"stage: semantic", "semantic-product: available"}));
  REQUIRE(failure.diagnosticInfo.has_value());
  CHECK(failure.diagnosticInfo->message == output.failure.message);
  REQUIRE(failure.sourceText.has_value());
  CHECK(*failure.sourceText == output.filteredSource);
}

TEST_CASE("compile pipeline result variants keep explicit success failure contract") {
  using Result = primec::CompilePipelineResult;
  static_assert(std::variant_size_v<Result> == 2);
  static_assert(
      std::is_same_v<std::variant_alternative_t<0, Result>, primec::CompilePipelineSuccessResult>);
  static_assert(
      std::is_same_v<std::variant_alternative_t<1, Result>, primec::CompilePipelineFailureResult>);

  primec::CompilePipelineSuccessResult success;
  success.output.hasSemanticProgram = true;
  Result successResult = success;
  CHECK(std::holds_alternative<primec::CompilePipelineSuccessResult>(successResult));

  primec::CompilePipelineFailureResult failure;
  failure.failure.stage = primec::CompilePipelineErrorStage::Semantic;
  failure.hasSemanticProgram = true;
  Result failureResult = failure;
  const auto *failureView = std::get_if<primec::CompilePipelineFailureResult>(&failureResult);
  REQUIRE(failureView != nullptr);
  CHECK(failureView->failure.stage == primec::CompilePipelineErrorStage::Semantic);
  CHECK(failureView->hasSemanticProgram);
}

TEST_CASE("compile pipeline result variants expose import failures without success state") {
  const std::filesystem::path tempPath = makeTempIrPipelineSourcePath();
  std::error_code ec;
  std::filesystem::remove(tempPath, ec);

  primec::Options options;
  options.inputPath = tempPath.string();
  options.entryPath = "/main";
  options.emitKind = "vm";
  primec::addDefaultStdlibInclude(options.inputPath, options.importPaths);

  primec::CompilePipelineDiagnosticInfo diagnosticInfo;
  primec::CompilePipelineErrorStage errorStage = primec::CompilePipelineErrorStage::None;
  std::string error;
  primec::CompilePipelineResult result =
      primec::runCompilePipelineResult(options, errorStage, error, &diagnosticInfo);

  const auto *failure = std::get_if<primec::CompilePipelineFailureResult>(&result);
  REQUIRE(failure != nullptr);
  CHECK(std::get_if<primec::CompilePipelineSuccessResult>(&result) == nullptr);
  CHECK(errorStage == primec::CompilePipelineErrorStage::Import);
  CHECK(failure->failure.stage == primec::CompilePipelineErrorStage::Import);
  CHECK(failure->failure.message == error);
  CHECK_FALSE(failure->hasSemanticProgram);
  CHECK(failure->filteredSource.empty());

  const primec::CliFailure cliFailure = primec::describeCompilePipelineFailure(*failure);
  CHECK(cliFailure.code == primec::DiagnosticCode::ImportError);
  CHECK(cliFailure.plainPrefix == "Import error: ");
  CHECK(cliFailure.notes == std::vector<std::string>{"stage: import"});
}

TEST_CASE("cpp-ir backend accepts semantic-product prepared IR from compile pipeline helper") {
  const std::string source =
      "[return<T>]\n"
      "id<T>([T] value) {\n"
      "  return(value)\n"
      "}\n"
      "\n"
      "[return<i32>]\n"
      "/vector/count([vector<i32>] self) {\n"
      "  return(17i32)\n"
      "}\n"
      "\n"
      "[return<i32>]\n"
      "main() {\n"
      "  [auto] selected{id(1i32)}\n"
      "  [auto] values{vector<i32>(1i32)}\n"
      "  return(selected + values.count())\n"
      "}\n";

  primec::testing::CompilePipelineBackendConformance conformance;
  std::string error;
  REQUIRE(primec::testing::runCompilePipelineBackendConformanceForTesting(
      source, "/main", "cpp-ir", conformance, error));
  CHECK(error.empty());
  CHECK(conformance.output.hasSemanticProgram);
  CHECK(conformance.backendKind == "cpp-ir");
  const auto *directCall = conformance.findDirectCallTarget("/main", "id");
  REQUIRE(directCall != nullptr);
  CHECK(conformance.resolvedDirectCallPath(*directCall).rfind("/id", 0) == 0);
  const auto *methodCall = conformance.findMethodCallTarget("/main", "count");
  REQUIRE(methodCall != nullptr);
  CHECK(conformance.resolvedMethodCallPath(*methodCall) == "/std/collections/vector/count");
  CHECK(conformance.emitResult.exitCode == 0);

  const std::string cpp = readTextFile(conformance.outputPath);
  CHECK(cpp.find("static int64_t ps_fn_0") != std::string::npos);
  CHECK(cpp.find("return static_cast<int>(ps_entry_0(argc, argv));") != std::string::npos);
}

TEST_CASE("vm backend executes semantic-product prepared IR from compile pipeline helper") {
  const std::string source =
      "[return<T>]\n"
      "id<T>([T] value) {\n"
      "  return(value)\n"
      "}\n"
      "\n"
      "[return<i32>]\n"
      "/vector/count([vector<i32>] self) {\n"
      "  return(17i32)\n"
      "}\n"
      "\n"
      "[return<i32>]\n"
      "main() {\n"
      "  [auto] selected{id(1i32)}\n"
      "  [auto] values{vector<i32>(1i32)}\n"
      "  return(selected + values.count())\n"
      "}\n";

  primec::testing::CompilePipelineBackendConformance conformance;
  std::string error;
  REQUIRE(primec::testing::runCompilePipelineBackendConformanceForTesting(
      source, "/main", "vm", conformance, error));
  CHECK(error.empty());
  CHECK(conformance.output.hasSemanticProgram);
  CHECK(conformance.backendKind == "vm");
  const auto *directCall = conformance.findDirectCallTarget("/main", "id");
  REQUIRE(directCall != nullptr);
  CHECK(conformance.resolvedDirectCallPath(*directCall).rfind("/id", 0) == 0);
  const auto *methodCall = conformance.findMethodCallTarget("/main", "count");
  REQUIRE(methodCall != nullptr);
  CHECK(conformance.resolvedMethodCallPath(*methodCall) == "/std/collections/vector/count");
  CHECK(conformance.emitResult.exitCode == 2);
}

TEST_CASE("semantic-product fact family ownership is explicit") {
  CHECK(primec::SemanticProductContractVersionCurrent ==
        primec::SemanticProductContractVersionV3);
  CHECK(primec::semanticProgramFactFamilyIsSemanticProductOwned("directCallTargets"));
  CHECK(primec::semanticProgramFactFamilyIsSemanticProductOwned("bindingFacts"));
  CHECK(primec::semanticProgramFactFamilyIsSemanticProductOwned("arrayExtentFacts"));
  CHECK(primec::semanticProgramFactFamilyIsSemanticProductOwned("onErrorFacts"));
  CHECK(primec::semanticProgramFactFamilyIsAstProvenanceOwned("definitions"));
  CHECK(primec::semanticProgramFactFamilyIsAstProvenanceOwned("executions"));
  CHECK(primec::semanticProgramFactFamilyOwnership("publishedRoutingLookups") ==
        primec::SemanticProgramFactOwnership::DerivedIndex);
  CHECK_FALSE(primec::semanticProgramFactFamilyOwnership("unknownFacts").has_value());
}

TEST_CASE("vm backend conformance keeps semantic-product contract v3") {
  const std::string source =
      "[return<i32>]\n"
      "main() {\n"
      "  return(0i32)\n"
      "}\n";

  primec::testing::CompilePipelineBackendConformance conformance;
  std::string error;
  REQUIRE(primec::testing::runCompilePipelineBackendConformanceForTesting(
      source, "/main", "vm", conformance, error));
  CHECK(error.empty());
  REQUIRE(conformance.output.hasSemanticProgram);
  CHECK(conformance.output.semanticProgram.contractVersion ==
        primec::SemanticProductContractVersionCurrent);
  CHECK(conformance.emitResult.exitCode == 0);
}

TEST_CASE("native backend emits semantic-product prepared IR from compile pipeline helper") {
  const std::string source =
      "[return<T>]\n"
      "id<T>([T] value) {\n"
      "  return(value)\n"
      "}\n"
      "\n"
      "[return<i32>]\n"
      "/vector/count([vector<i32>] self) {\n"
      "  return(17i32)\n"
      "}\n"
      "\n"
      "[return<i32>]\n"
      "main() {\n"
      "  [auto] selected{id(1i32)}\n"
      "  [auto] values{vector<i32>(1i32)}\n"
      "  return(selected + values.count())\n"
      "}\n";

#if !defined(__APPLE__) || (!defined(__arm64__) && !defined(__aarch64__))
  INFO("SKIP: native backend emit is only supported on macOS arm64");
  return;
#endif
  primec::testing::CompilePipelineBackendConformance conformance;
  std::string error;
  REQUIRE(primec::testing::runCompilePipelineBackendConformanceForTesting(
      source, "/main", "native", conformance, error));
  CHECK(error.empty());
  CHECK(conformance.output.hasSemanticProgram);
  CHECK(conformance.backendKind == "native");
  const auto *directCall = conformance.findDirectCallTarget("/main", "id");
  REQUIRE(directCall != nullptr);
  CHECK(conformance.resolvedDirectCallPath(*directCall).rfind("/id", 0) == 0);
  const auto *methodCall = conformance.findMethodCallTarget("/main", "count");
  REQUIRE(methodCall != nullptr);
  CHECK(conformance.resolvedMethodCallPath(*methodCall) == "/std/collections/vector/count");
  CHECK(conformance.emitResult.exitCode == 0);
  CHECK(std::filesystem::exists(conformance.outputPath));
  CHECK(std::filesystem::is_regular_file(conformance.outputPath));
  CHECK(std::filesystem::file_size(conformance.outputPath) > 0);
}
