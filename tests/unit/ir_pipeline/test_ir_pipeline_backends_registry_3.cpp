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

TEST_CASE("ir lowerer rejects stale semantic-product method-call metadata") {
  primec::Program program;

  primec::Definition mainDef;
  mainDef.fullPath = "/main";
  mainDef.semanticNodeId = 4200;
  primec::Expr receiverExpr;
  receiverExpr.kind = primec::Expr::Kind::Name;
  receiverExpr.name = "values";
  primec::Expr methodCallExpr;
  methodCallExpr.kind = primec::Expr::Kind::Call;
  methodCallExpr.name = "count";
  methodCallExpr.isMethodCall = true;
  methodCallExpr.semanticNodeId = 4201;
  methodCallExpr.args.push_back(receiverExpr);
  mainDef.statements.push_back(methodCallExpr);
  program.definitions.push_back(mainDef);

  primec::SemanticProgram semanticProgram;
  semanticProgram.entryPath = "/main";
  addVoidCallableSummary(semanticProgram, 4200);
  semanticProgram.methodCallTargets.push_back(primec::SemanticProgramMethodCallTarget{
      .scopePath = "/main",
      .methodName = "count",
      .receiverTypeText = "vector<i32>",
      .sourceLine = 1,
      .sourceColumn = 1,
      .semanticNodeId = 4201,
      .provenanceHandle = 0,
      .scopePathId =
          primec::semanticProgramInternCallTargetString(semanticProgram, "/main"),
      .methodNameId =
          primec::semanticProgramInternCallTargetString(semanticProgram, "count"),
      .receiverTypeTextId =
          primec::semanticProgramInternCallTargetString(semanticProgram, "vector<i32>"),
      .resolvedPathId =
          primec::semanticProgramInternCallTargetString(semanticProgram,
                                                        "/std/collections/vector/count"),
      .stdlibSurfaceId = std::nullopt,
  });

  auto resetMethodCallMetadata = [&]() {
    auto &methodCallTarget = semanticProgram.methodCallTargets.back();
    methodCallTarget.scopePathId =
        primec::semanticProgramInternCallTargetString(semanticProgram, "/main");
    methodCallTarget.methodNameId =
        primec::semanticProgramInternCallTargetString(semanticProgram, "count");
    methodCallTarget.receiverTypeTextId =
        primec::semanticProgramInternCallTargetString(semanticProgram, "vector<i32>");
    methodCallTarget.resolvedPathId =
        primec::semanticProgramInternCallTargetString(semanticProgram,
                                                      "/std/collections/vector/count");
    semanticProgram.publishedRoutingLookups.methodCallTargetIdsByExpr.insert_or_assign(
        4201, methodCallTarget.resolvedPathId);
  };

  std::string error;

  resetMethodCallMetadata();
  semanticProgram.methodCallTargets.back().scopePathId =
      primec::semanticProgramInternCallTargetString(semanticProgram, "/other");
  CHECK_FALSE(primec::ir_lowerer::validateSemanticProductMethodCallCoverage(
      program, &semanticProgram, error));
  CHECK(error == "stale semantic-product method-call scope metadata: /main -> count");

  resetMethodCallMetadata();
  semanticProgram.methodCallTargets.back().methodNameId =
      primec::semanticProgramInternCallTargetString(semanticProgram, "other");
  error.clear();
  CHECK_FALSE(primec::ir_lowerer::validateSemanticProductMethodCallCoverage(
      program, &semanticProgram, error));
  CHECK(error == "stale semantic-product method-call name metadata: /main -> count");

  resetMethodCallMetadata();
  semanticProgram.methodCallTargets.back().receiverTypeTextId =
      primec::semanticProgramInternCallTargetString(semanticProgram, "vector<i64>");
  error.clear();
  CHECK_FALSE(primec::ir_lowerer::validateSemanticProductMethodCallCoverage(
      program, &semanticProgram, error));
  CHECK(error == "stale semantic-product method-call receiver metadata: /main -> count");

  resetMethodCallMetadata();
  semanticProgram.publishedRoutingLookups.methodCallTargetIdsByExpr.insert_or_assign(
      4201, primec::semanticProgramInternCallTargetString(semanticProgram, "/stale"));
  error.clear();
  CHECK_FALSE(primec::ir_lowerer::validateSemanticProductMethodCallCoverage(
      program, &semanticProgram, error));
  CHECK(error == "stale semantic-product method-call target metadata: /main -> count");
}

TEST_CASE("ir lowerer production entry rejects missing semantic-product method-call targets") {
  primec::Program program;

  primec::Definition mainDef;
  mainDef.fullPath = "/main";
  mainDef.semanticNodeId = 4200;
  primec::Expr valuesExpr;
  valuesExpr.kind = primec::Expr::Kind::Name;
  valuesExpr.name = "values";
  primec::Expr callExpr;
  callExpr.kind = primec::Expr::Kind::Call;
  callExpr.isMethodCall = true;
  callExpr.name = "count";
  callExpr.semanticNodeId = 4201;
  callExpr.args.push_back(valuesExpr);
  mainDef.statements.push_back(callExpr);
  program.definitions.push_back(mainDef);

  primec::SemanticProgram semanticProgram;
  semanticProgram.entryPath = "/main";
  addVoidCallableSummary(semanticProgram, 4200);

  primec::IrLowerer lowerer;
  primec::IrModule module;
  primec::DiagnosticSinkReport diagnosticInfo;
  std::string error;

  CHECK_FALSE(lowerer.lower(program, &semanticProgram, "/main", {}, {}, module, error, &diagnosticInfo));
  CHECK(error == "missing semantic-product method-call target: /main -> count");
  CHECK(diagnosticInfo.message == error);
}

TEST_CASE("ir lowerer rejects missing semantic-product bridge-path choices") {
  primec::Program program;

  primec::Definition mainDef;
  mainDef.fullPath = "/main";
  mainDef.semanticNodeId = 5203;
  primec::Expr valuesExpr;
  valuesExpr.kind = primec::Expr::Kind::Name;
  valuesExpr.name = "values";
  primec::Expr callExpr;
  callExpr.kind = primec::Expr::Kind::Call;
  callExpr.name = "count";
  callExpr.semanticNodeId = 52;
  callExpr.args.push_back(valuesExpr);
  mainDef.statements.push_back(callExpr);
  program.definitions.push_back(mainDef);

  primec::SemanticProgram semanticProgram;
  semanticProgram.entryPath = "/main";
  addVoidCallableSummary(semanticProgram, 5203);
  semanticProgram.directCallTargets.push_back(primec::SemanticProgramDirectCallTarget{
      .scopePath = "/main",
      .callName = "count",
      .sourceLine = 1,
      .sourceColumn = 1,
      .semanticNodeId = 52,
      .resolvedPathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/vector/count"),
      .stdlibSurfaceId = primec::StdlibSurfaceId::CollectionsManifestSurface0,
  });
  semanticProgram.publishedRoutingLookups.directCallTargetIdsByExpr.insert_or_assign(
      52, semanticProgram.directCallTargets.back().resolvedPathId);
  semanticProgram.publishedRoutingLookups.directCallStdlibSurfaceIdsByExpr.insert_or_assign(
      52, primec::StdlibSurfaceId::CollectionsManifestSurface0);

  primec::IrLowerer lowerer;
  primec::IrModule module;
  primec::DiagnosticSinkReport diagnosticInfo;
  std::string error;

  CHECK_FALSE(lowerer.lower(program, &semanticProgram, "/main", {}, {}, module, error, &diagnosticInfo));
  CHECK(error == "missing semantic-product bridge-path choice: /main -> count");
  CHECK(diagnosticInfo.message == error);
}

TEST_CASE("bridge-path coverage rejects missing helper name ids before lookup gaps") {
  primec::Program program;

  primec::Definition mainDef;
  mainDef.fullPath = "/main";
  mainDef.semanticNodeId = 5200;
  primec::Expr valuesExpr;
  valuesExpr.kind = primec::Expr::Kind::Name;
  valuesExpr.name = "values";
  primec::Expr callExpr;
  callExpr.kind = primec::Expr::Kind::Call;
  callExpr.name = "count";
  callExpr.semanticNodeId = 5201;
  callExpr.args.push_back(valuesExpr);
  mainDef.statements.push_back(callExpr);
  program.definitions.push_back(mainDef);

  primec::SemanticProgram semanticProgram;
  semanticProgram.entryPath = "/main";
  addVoidCallableSummary(semanticProgram, 5200);
  semanticProgram.directCallTargets.push_back(primec::SemanticProgramDirectCallTarget{
      .scopePath = "/main",
      .callName = "count",
      .sourceLine = 1,
      .sourceColumn = 1,
      .semanticNodeId = 5201,
      .resolvedPathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/vector/count"),
      .stdlibSurfaceId = std::nullopt,
  });
  semanticProgram.bridgePathChoices.push_back(primec::SemanticProgramBridgePathChoice{
      .scopePath = "/main",
      .collectionFamily = "vector",
      .sourceLine = 1,
      .sourceColumn = 1,
      .semanticNodeId = 5201,
      .helperNameId = primec::InvalidSymbolId,
      .chosenPathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/vector/count"),
      .stdlibSurfaceId = primec::StdlibSurfaceId::CollectionsManifestSurface0,
  });
  semanticProgram.publishedRoutingLookups.bridgePathChoiceIdsByExpr.insert_or_assign(
      5201, semanticProgram.bridgePathChoices.back().chosenPathId);

  std::string error;

  CHECK_FALSE(primec::ir_lowerer::validateSemanticProductBridgePathCoverage(
      program, &semanticProgram, error));
  CHECK(error == "missing semantic-product bridge helper name id: /main -> count");
}

TEST_CASE("bridge-path coverage rejects helper name ids before direct-call target gaps") {
  primec::Program program;

  primec::Definition mainDef;
  mainDef.fullPath = "/main";
  mainDef.semanticNodeId = 5204;
  primec::Expr valuesExpr;
  valuesExpr.kind = primec::Expr::Kind::Name;
  valuesExpr.name = "values";
  primec::Expr callExpr;
  callExpr.kind = primec::Expr::Kind::Call;
  callExpr.name = "count";
  callExpr.semanticNodeId = 5202;
  callExpr.args.push_back(valuesExpr);
  mainDef.statements.push_back(callExpr);
  program.definitions.push_back(mainDef);

  primec::SemanticProgram semanticProgram;
  semanticProgram.entryPath = "/main";
  addVoidCallableSummary(semanticProgram, 5204);
  semanticProgram.bridgePathChoices.push_back(primec::SemanticProgramBridgePathChoice{
      .scopePath = "/main",
      .collectionFamily = "vector",
      .sourceLine = 1,
      .sourceColumn = 1,
      .semanticNodeId = 5202,
      .helperNameId = primec::InvalidSymbolId,
      .chosenPathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/vector/count"),
      .stdlibSurfaceId = primec::StdlibSurfaceId::CollectionsManifestSurface0,
  });
  semanticProgram.publishedRoutingLookups.bridgePathChoiceIdsByExpr.insert_or_assign(
      5202, semanticProgram.bridgePathChoices.back().chosenPathId);

  std::string error;

  CHECK_FALSE(primec::ir_lowerer::validateSemanticProductBridgePathCoverage(
      program, &semanticProgram, error));
  CHECK(error == "missing semantic-product bridge helper name id: /main -> count");
}

TEST_CASE("ir lowerer production entry reports native diagnostic without bridge-path choice") {
  primec::Program program;

  primec::Definition mainDef;
  mainDef.fullPath = "/main";
  mainDef.semanticNodeId = 5206;
  primec::Expr valuesExpr;
  valuesExpr.kind = primec::Expr::Kind::Name;
  valuesExpr.name = "values";
  primec::Expr callExpr;
  callExpr.kind = primec::Expr::Kind::Call;
  callExpr.name = "contains";
  callExpr.semanticNodeId = 5207;
  callExpr.args.push_back(valuesExpr);
  mainDef.statements.push_back(callExpr);
  program.definitions.push_back(mainDef);

  primec::SemanticProgram semanticProgram;
  semanticProgram.entryPath = "/main";
  addVoidCallableSummary(semanticProgram, 5206);
  semanticProgram.directCallTargets.push_back(primec::SemanticProgramDirectCallTarget{
      .scopePath = "/main",
      .callName = "contains",
      .sourceLine = 1,
      .sourceColumn = 1,
      .semanticNodeId = 5207,
      .resolvedPathId = primec::semanticProgramInternCallTargetString(
          semanticProgram, "/std/collections/map/contains"),
      .stdlibSurfaceId = primec::findStdlibSurfaceMetadataByBridgeKey("collections.map_helpers")->id,
  });
  semanticProgram.publishedRoutingLookups.directCallTargetIdsByExpr.insert_or_assign(
      5207, semanticProgram.directCallTargets.front().resolvedPathId);

  primec::IrLowerer lowerer;
  primec::IrModule module;
  primec::DiagnosticSinkReport diagnosticInfo;
  std::string error;

  CHECK_FALSE(lowerer.lower(program, &semanticProgram, "/main", {}, {}, module, error, &diagnosticInfo));
  CHECK(error.find("native backend only supports arithmetic/comparison/clamp/min/max/abs/sign/"
                   "saturate/convert/pointer/assign/increment/decrement calls in expressions") !=
        std::string::npos);
  CHECK(error.find("call=/std/collections/map/contains") != std::string::npos);
  CHECK(error.find("name=contains") != std::string::npos);
  CHECK(diagnosticInfo.message == error);
}

TEST_CASE("bridge-path coverage rejects invalid helper name ids before lookup gaps") {
  primec::Program program;

  primec::Definition mainDef;
  mainDef.fullPath = "/main";
  mainDef.semanticNodeId = 5205;
  primec::Expr valuesExpr;
  valuesExpr.kind = primec::Expr::Kind::Name;
  valuesExpr.name = "values";
  primec::Expr callExpr;
  callExpr.kind = primec::Expr::Kind::Call;
  callExpr.name = "count";
  callExpr.semanticNodeId = 5203;
  callExpr.args.push_back(valuesExpr);
  mainDef.statements.push_back(callExpr);
  program.definitions.push_back(mainDef);

  primec::SemanticProgram semanticProgram;
  semanticProgram.entryPath = "/main";
  addVoidCallableSummary(semanticProgram, 5205);
  semanticProgram.directCallTargets.push_back(primec::SemanticProgramDirectCallTarget{
      .scopePath = "/main",
      .callName = "count",
      .sourceLine = 1,
      .sourceColumn = 1,
      .semanticNodeId = 5203,
      .resolvedPathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/vector/count"),
      .stdlibSurfaceId = std::nullopt,
  });
  semanticProgram.bridgePathChoices.push_back(primec::SemanticProgramBridgePathChoice{
      .scopePath = "/main",
      .collectionFamily = "vector",
      .sourceLine = 1,
      .sourceColumn = 1,
      .semanticNodeId = 5203,
      .helperNameId =
          static_cast<primec::SymbolId>(semanticProgram.callTargetStringTable.size() + 1u),
      .chosenPathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/vector/count"),
      .stdlibSurfaceId = primec::StdlibSurfaceId::CollectionsManifestSurface0,
  });
  semanticProgram.publishedRoutingLookups.bridgePathChoiceIdsByExpr.insert_or_assign(
      5203, semanticProgram.bridgePathChoices.back().chosenPathId);

  std::string error;

  CHECK_FALSE(primec::ir_lowerer::validateSemanticProductBridgePathCoverage(
      program, &semanticProgram, error));
  CHECK(error == "missing semantic-product bridge helper name id: /main -> count");
}

TEST_CASE("ir lowerer rejects stale semantic-product bridge-path metadata") {
  primec::Program program;

  primec::Definition mainDef;
  mainDef.fullPath = "/main";
  mainDef.semanticNodeId = 5210;
  primec::Expr valuesExpr;
  valuesExpr.kind = primec::Expr::Kind::Name;
  valuesExpr.name = "values";
  primec::Expr callExpr;
  callExpr.kind = primec::Expr::Kind::Call;
  callExpr.name = "count";
  callExpr.semanticNodeId = 5211;
  callExpr.args.push_back(valuesExpr);
  mainDef.statements.push_back(callExpr);
  program.definitions.push_back(mainDef);

  primec::SemanticProgram semanticProgram;
  semanticProgram.entryPath = "/main";
  addVoidCallableSummary(semanticProgram, 5210);
  semanticProgram.bridgePathChoices.push_back(primec::SemanticProgramBridgePathChoice{
      .scopePath = "/main",
      .collectionFamily = "vector",
      .sourceLine = 1,
      .sourceColumn = 1,
      .semanticNodeId = 5211,
      .provenanceHandle = 0,
      .scopePathId =
          primec::semanticProgramInternCallTargetString(semanticProgram, "/main"),
      .collectionFamilyId =
          primec::semanticProgramInternCallTargetString(semanticProgram, "vector"),
      .helperNameId =
          primec::semanticProgramInternCallTargetString(semanticProgram, "count"),
      .chosenPathId =
          primec::semanticProgramInternCallTargetString(semanticProgram, "/vector/count"),
      .stdlibSurfaceId = primec::StdlibSurfaceId::CollectionsManifestSurface0,
  });

  auto resetBridgeMetadata = [&]() {
    auto &bridgeChoice = semanticProgram.bridgePathChoices.back();
    bridgeChoice.scopePathId =
        primec::semanticProgramInternCallTargetString(semanticProgram, "/main");
    bridgeChoice.collectionFamilyId =
        primec::semanticProgramInternCallTargetString(semanticProgram, "vector");
    bridgeChoice.helperNameId =
        primec::semanticProgramInternCallTargetString(semanticProgram, "count");
    bridgeChoice.chosenPathId =
        primec::semanticProgramInternCallTargetString(semanticProgram, "/vector/count");
    semanticProgram.publishedRoutingLookups.bridgePathChoiceIdsByExpr.insert_or_assign(
        5211, bridgeChoice.chosenPathId);
  };

  std::string error;

  resetBridgeMetadata();
  semanticProgram.bridgePathChoices.back().chosenPathId = primec::InvalidSymbolId;
  CHECK_FALSE(primec::ir_lowerer::validateSemanticProductBridgePathCoverage(
      program, &semanticProgram, error));
  CHECK(error == "missing semantic-product bridge chosen path id: /main -> count");

  resetBridgeMetadata();
  semanticProgram.bridgePathChoices.back().scopePathId =
      primec::semanticProgramInternCallTargetString(semanticProgram, "/other");
  error.clear();
  CHECK_FALSE(primec::ir_lowerer::validateSemanticProductBridgePathCoverage(
      program, &semanticProgram, error));
  CHECK(error == "stale semantic-product bridge scope metadata: /main -> count");

  resetBridgeMetadata();
  semanticProgram.bridgePathChoices.back().collectionFamilyId =
      primec::semanticProgramInternCallTargetString(semanticProgram, "map");
  error.clear();
  CHECK_FALSE(primec::ir_lowerer::validateSemanticProductBridgePathCoverage(
      program, &semanticProgram, error));
  CHECK(error ==
        "stale semantic-product bridge collection family metadata: /main -> count");

  resetBridgeMetadata();
  semanticProgram.publishedRoutingLookups.bridgePathChoiceIdsByExpr.insert_or_assign(
      5211, primec::semanticProgramInternCallTargetString(semanticProgram, "/stale"));
  error.clear();
  CHECK_FALSE(primec::ir_lowerer::validateSemanticProductBridgePathCoverage(
      program, &semanticProgram, error));
  CHECK(error == "stale semantic-product bridge target metadata: /main -> count");
}

TEST_CASE("ir lowerer rejects missing semantic-product binding facts") {
  primec::Program program;

  primec::Definition mainDef;
  mainDef.fullPath = "/main";
  primec::Expr param;
  param.name = "value";
  param.semanticNodeId = 7;
  mainDef.parameters.push_back(param);
  program.definitions.push_back(mainDef);

  primec::SemanticProgram semanticProgram;
  semanticProgram.entryPath = "/main";

  primec::IrLowerer lowerer;
  primec::IrModule module;
  primec::DiagnosticSinkReport diagnosticInfo;
  std::string error;

  CHECK_FALSE(lowerer.lower(program, &semanticProgram, "/main", {}, {}, module, error, &diagnosticInfo));
  CHECK(error == "missing semantic-product binding fact: /main -> parameter value");
  CHECK(diagnosticInfo.message == error);
}

TEST_CASE("ir lowerer rejects semantic-product binding facts missing resolved path ids") {
  primec::Program program;

  primec::Definition mainDef;
  mainDef.fullPath = "/main";
  primec::Expr param;
  param.name = "value";
  param.semanticNodeId = 7;
  mainDef.parameters.push_back(param);
  program.definitions.push_back(mainDef);

  primec::SemanticProgram semanticProgram;
  semanticProgram.entryPath = "/main";
  semanticProgram.bindingFacts.push_back(primec::SemanticProgramBindingFact{
      .scopePath = "/main",
      .siteKind = "parameter",
      .name = "value",
      .bindingTypeText = "i32",
      .isMutable = false,
      .isEntryArgString = false,
      .isUnsafeReference = false,
      .referenceRoot = "",
      .sourceLine = 0,
      .sourceColumn = 0,
      .semanticNodeId = 7,
      .provenanceHandle = 0,
      .resolvedPathId = primec::InvalidSymbolId,
  });
  semanticProgram.publishedRoutingLookups.bindingFactIndicesByExpr.insert_or_assign(7, 0);

  primec::IrLowerer lowerer;
  primec::IrModule module;
  primec::DiagnosticSinkReport diagnosticInfo;
  std::string error;

  CHECK_FALSE(lowerer.lower(program, &semanticProgram, "/main", {}, {}, module, error, &diagnosticInfo));
  CHECK(error == "missing semantic-product binding resolved path id: /main -> parameter value");
  CHECK(diagnosticInfo.message == error);
}

TEST_CASE("ir lowerer rejects missing semantic-product collection specializations") {
  primec::Program program;

  primec::Definition mainDef;
  mainDef.fullPath = "/main";
  mainDef.semanticNodeId = 81;
  primec::Transform returnTransform;
  returnTransform.name = "return";
  returnTransform.templateArgs.push_back("void");
  mainDef.transforms.push_back(returnTransform);

  primec::Expr bindingExpr;
  bindingExpr.isBinding = true;
  bindingExpr.name = "values";
  bindingExpr.semanticNodeId = 43;
  primec::Transform vectorTransform;
  vectorTransform.name = "vector";
  vectorTransform.templateArgs = {"i32"};
  bindingExpr.transforms.push_back(vectorTransform);
  mainDef.statements.push_back(bindingExpr);
  program.definitions.push_back(mainDef);

  primec::SemanticProgram semanticProgram;
  semanticProgram.entryPath = "/main";
  addVoidCallableSummary(semanticProgram, 81);
  semanticProgram.bindingFacts.push_back(primec::SemanticProgramBindingFact{
      .scopePath = "/main",
      .siteKind = "local",
      .name = "values",
      .bindingTypeText = "vector<i32>",
      .isMutable = false,
      .isEntryArgString = false,
      .isUnsafeReference = false,
      .referenceRoot = "",
      .sourceLine = 0,
      .sourceColumn = 0,
      .semanticNodeId = 43,
      .provenanceHandle = 0,
      .resolvedPathId =
          primec::semanticProgramInternCallTargetString(semanticProgram, "/main/values"),
  });
  semanticProgram.publishedRoutingLookups.bindingFactIndicesByExpr.insert_or_assign(43, 0);

  primec::IrLowerer lowerer;
  primec::IrModule module;
  primec::DiagnosticSinkReport diagnosticInfo;
  std::string error;

  CHECK_FALSE(lowerer.lower(program, &semanticProgram, "/main", {}, {}, module, error, &diagnosticInfo));
  CHECK(error == "missing semantic-product collection specialization: /main -> local values");
  CHECK(diagnosticInfo.message == error);
}

TEST_CASE("ir lowerer rejects missing semantic-product array extent facts") {
  primec::Program program;

  primec::Definition mainDef;
  mainDef.fullPath = "/main";
  mainDef.semanticNodeId = 81;
  primec::Transform returnTransform;
  returnTransform.name = "return";
  returnTransform.templateArgs.push_back("void");
  mainDef.transforms.push_back(returnTransform);

  primec::Expr bindingExpr;
  bindingExpr.isBinding = true;
  bindingExpr.name = "values";
  bindingExpr.semanticNodeId = 43;
  primec::Transform arrayTransform;
  arrayTransform.name = "array";
  arrayTransform.templateArgs = {"i32"};
  bindingExpr.transforms.push_back(arrayTransform);
  mainDef.statements.push_back(bindingExpr);
  program.definitions.push_back(mainDef);

  primec::SemanticProgram semanticProgram;
  semanticProgram.entryPath = "/main";
  addVoidCallableSummary(semanticProgram, 81);
  semanticProgram.bindingFacts.push_back(primec::SemanticProgramBindingFact{
      .scopePath = "/main",
      .siteKind = "local",
      .name = "values",
      .bindingTypeText = "array<i32>",
      .isMutable = false,
      .isEntryArgString = false,
      .isUnsafeReference = false,
      .referenceRoot = "",
      .sourceLine = 0,
      .sourceColumn = 0,
      .semanticNodeId = 43,
      .provenanceHandle = 0,
      .resolvedPathId =
          primec::semanticProgramInternCallTargetString(semanticProgram, "/main/values"),
  });
  semanticProgram.publishedRoutingLookups.bindingFactIndicesByExpr.insert_or_assign(43, 0);

  primec::IrLowerer lowerer;
  primec::IrModule module;
  primec::DiagnosticSinkReport diagnosticInfo;
  std::string error;

  CHECK_FALSE(lowerer.lower(program, &semanticProgram, "/main", {}, {}, module, error, &diagnosticInfo));
  CHECK(error == "missing semantic-product array extent fact: /main -> local values");
  CHECK(diagnosticInfo.message == error);
}

TEST_CASE("ir lowerer rejects stale semantic-product collection metadata") {
  primec::Program program;

  primec::Definition mainDef;
  mainDef.fullPath = "/main";
  mainDef.semanticNodeId = 81;
  primec::Transform mainReturnTransform;
  mainReturnTransform.name = "return";
  mainReturnTransform.templateArgs.push_back("void");
  mainDef.transforms.push_back(mainReturnTransform);

  primec::Expr bindingExpr;
  bindingExpr.isBinding = true;
  bindingExpr.name = "pairs";
  bindingExpr.semanticNodeId = 43;
  primec::Transform mapTransform;
  mapTransform.name = "map";
  mapTransform.templateArgs = {"i32", "i64"};
  bindingExpr.transforms.push_back(mapTransform);
  mainDef.statements.push_back(bindingExpr);
  program.definitions.push_back(mainDef);

  primec::SemanticProgram semanticProgram;
  semanticProgram.entryPath = "/main";
  addVoidCallableSummary(semanticProgram, 81);
  semanticProgram.bindingFacts.push_back(primec::SemanticProgramBindingFact{
      .scopePath = "/main",
      .siteKind = "local",
      .name = "pairs",
      .bindingTypeText = "map<i32, i64>",
      .isMutable = false,
      .isEntryArgString = false,
      .isUnsafeReference = false,
      .referenceRoot = "",
      .sourceLine = 0,
      .sourceColumn = 0,
      .semanticNodeId = 43,
      .provenanceHandle = 0,
      .resolvedPathId =
          primec::semanticProgramInternCallTargetString(semanticProgram, "/main/pairs"),
  });
  semanticProgram.publishedRoutingLookups.bindingFactIndicesByExpr.insert_or_assign(43, 0);
  semanticProgram.collectionSpecializations.push_back(
      primec::SemanticProgramCollectionSpecialization{
          .scopePath = "/main",
          .siteKind = "local",
          .name = "pairs",
          .collectionFamily = "map",
          .bindingTypeText = "map<i32, i64>",
          .elementTypeText = "",
          .keyTypeText = "i32",
          .valueTypeText = "i64",
          .isReference = false,
          .isPointer = false,
          .sourceLine = 0,
          .sourceColumn = 0,
          .semanticNodeId = 43,
          .provenanceHandle = 0,
          .scopePathId =
              primec::semanticProgramInternCallTargetString(semanticProgram, "/main"),
          .siteKindId =
              primec::semanticProgramInternCallTargetString(semanticProgram, "local"),
          .nameId = primec::semanticProgramInternCallTargetString(semanticProgram, "pairs"),
          .collectionFamilyId =
              primec::semanticProgramInternCallTargetString(semanticProgram, "map"),
          .bindingTypeTextId =
              primec::semanticProgramInternCallTargetString(semanticProgram, "map<i32, i64>"),
          .elementTypeTextId = primec::InvalidSymbolId,
          .keyTypeTextId = primec::semanticProgramInternCallTargetString(semanticProgram, "i32"),
          .valueTypeTextId = primec::semanticProgramInternCallTargetString(semanticProgram, "i64"),
          .helperSurfaceId = std::nullopt,
          .constructorSurfaceId = std::nullopt,
      });
  semanticProgram.publishedRoutingLookups.collectionSpecializationIndicesByExpr.insert_or_assign(
      43, 0);

  auto refreshCollectionIds = [&]() {
    auto &collectionFact = semanticProgram.collectionSpecializations.back();
    collectionFact.collectionFamilyId =
        primec::semanticProgramInternCallTargetString(semanticProgram, "map");
    collectionFact.bindingTypeTextId =
        primec::semanticProgramInternCallTargetString(semanticProgram, "map<i32, i64>");
    collectionFact.elementTypeTextId = primec::InvalidSymbolId;
    collectionFact.keyTypeTextId =
        primec::semanticProgramInternCallTargetString(semanticProgram, "i32");
    collectionFact.valueTypeTextId =
        primec::semanticProgramInternCallTargetString(semanticProgram, "i64");
  };

  auto lowerWithSemanticProduct = [&](std::string &error,
                                      primec::DiagnosticSinkReport &diagnosticInfo) {
    const bool ok = primec::ir_lowerer::validateSemanticProductCollectionSpecializationCoverage(
        program, &semanticProgram, error);
    diagnosticInfo.message = error;
    return ok;
  };

  std::string error;
  primec::DiagnosticSinkReport diagnosticInfo;

  semanticProgram.bindingFacts.back().bindingTypeTextId =
      primec::semanticProgramInternCallTargetString(semanticProgram, "map<i32, i64>");
  semanticProgram.bindingFacts.back().bindingTypeText = "";
  refreshCollectionIds();
  CAPTURE(error);
  CAPTURE(diagnosticInfo.message);
  CHECK(lowerWithSemanticProduct(error, diagnosticInfo));
  CHECK(error.empty());
  CHECK(diagnosticInfo.message.empty());

  refreshCollectionIds();
  error.clear();
  diagnosticInfo = {};
  semanticProgram.collectionSpecializations.back().collectionFamilyId =
      static_cast<primec::SymbolId>(semanticProgram.callTargetStringTable.size() + 1u);
  CHECK_FALSE(lowerWithSemanticProduct(error, diagnosticInfo));
  CHECK(error == "missing semantic-product collection specialization family id: /main -> local pairs");
  CHECK(diagnosticInfo.message == error);

  refreshCollectionIds();
  semanticProgram.collectionSpecializations.back().bindingTypeTextId =
      primec::semanticProgramInternCallTargetString(semanticProgram, "map<i32, i32>");
  error.clear();
  diagnosticInfo = {};
  CHECK_FALSE(lowerWithSemanticProduct(error, diagnosticInfo));
  CHECK(error ==
        "stale semantic-product collection specialization binding type metadata: /main -> local pairs");
  CHECK(diagnosticInfo.message == error);

  refreshCollectionIds();
  semanticProgram.collectionSpecializations.back().elementTypeTextId =
      primec::semanticProgramInternCallTargetString(semanticProgram, "i32");
  error.clear();
  diagnosticInfo = {};
  CHECK_FALSE(lowerWithSemanticProduct(error, diagnosticInfo));
  CHECK(error ==
        "stale semantic-product collection specialization element type metadata: /main -> local pairs");
  CHECK(diagnosticInfo.message == error);

  refreshCollectionIds();
  semanticProgram.collectionSpecializations.back().keyTypeTextId =
      primec::semanticProgramInternCallTargetString(semanticProgram, "i64");
  error.clear();
  diagnosticInfo = {};
  CHECK_FALSE(lowerWithSemanticProduct(error, diagnosticInfo));
  CHECK(error ==
        "stale semantic-product collection specialization key type metadata: /main -> local pairs");
  CHECK(diagnosticInfo.message == error);

  refreshCollectionIds();
  semanticProgram.collectionSpecializations.back().valueTypeTextId =
      primec::semanticProgramInternCallTargetString(semanticProgram, "i32");
  error.clear();
  diagnosticInfo = {};
  CHECK_FALSE(lowerWithSemanticProduct(error, diagnosticInfo));
  CHECK(error ==
        "stale semantic-product collection specialization value type metadata: /main -> local pairs");
  CHECK(diagnosticInfo.message == error);
}

TEST_CASE("ir lowerer rejects missing semantic-product local binding facts") {
  primec::Program program;

  primec::Definition mainDef;
  mainDef.fullPath = "/main";
  mainDef.semanticNodeId = 81;

  primec::Expr initializerExpr;
  initializerExpr.kind = primec::Expr::Kind::Literal;
  initializerExpr.literalValue = 1;
  initializerExpr.intWidth = 32;

  primec::Expr bindingExpr;
  bindingExpr.isBinding = true;
  bindingExpr.name = "selected";
  bindingExpr.semanticNodeId = 43;
  bindingExpr.args.push_back(initializerExpr);
  mainDef.statements.push_back(bindingExpr);
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
      .semanticNodeId = 81,
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
      .semanticNodeId = 81,
      .definitionPathId =
          primec::semanticProgramInternCallTargetString(semanticProgram, "/main"),
  });
  semanticProgram.publishedRoutingLookups.returnFactIndicesByDefinitionId
      .insert_or_assign(81, semanticProgram.returnFacts.size() - 1);
  primec::IrLowerer lowerer;
  primec::IrModule module;
  primec::DiagnosticSinkReport diagnosticInfo;
  std::string error;

  CHECK_FALSE(lowerer.lower(program, &semanticProgram, "/main", {}, {}, module, error, &diagnosticInfo));
  CHECK(error == "missing semantic-product binding fact: /main -> local selected");
  CHECK(diagnosticInfo.message == error);
}

TEST_CASE("ir lowerer rejects missing semantic-product type metadata for struct layouts") {
  primec::Program program;

  primec::Definition structDef;
  structDef.fullPath = "/Point";
  primec::Transform structTransform;
  structTransform.name = "struct";
  structDef.transforms.push_back(structTransform);
  program.definitions.push_back(structDef);

  primec::Definition mainDef;
  mainDef.fullPath = "/main";
  mainDef.semanticNodeId = 81;
  program.definitions.push_back(mainDef);

  primec::SemanticProgram semanticProgram;
  semanticProgram.entryPath = "/main";
  addVoidCallableSummary(semanticProgram, 81);

  primec::IrLowerer lowerer;
  primec::IrModule module;
  primec::DiagnosticSinkReport diagnosticInfo;
  std::string error;

  CHECK_FALSE(lowerer.lower(program, &semanticProgram, "/main", {}, {}, module, error, &diagnosticInfo));
  CHECK(error == "missing semantic-product type metadata: /Point");
  CHECK(diagnosticInfo.message == error);
}

TEST_CASE("ir lowerer rejects missing semantic-product struct field metadata") {
  primec::Program program;

  primec::Definition structDef;
  structDef.fullPath = "/Point";
  primec::Transform structTransform;
  structTransform.name = "struct";
  structDef.transforms.push_back(structTransform);
  primec::Expr field;
  field.kind = primec::Expr::Kind::Name;
  field.isBinding = true;
  field.name = "x";
  structDef.statements.push_back(field);
  program.definitions.push_back(structDef);

  primec::Definition mainDef;
  mainDef.fullPath = "/main";
  mainDef.semanticNodeId = 81;
  program.definitions.push_back(mainDef);

  primec::SemanticProgram semanticProgram;
  semanticProgram.entryPath = "/main";
  addVoidCallableSummary(semanticProgram, 81);
  semanticProgram.typeMetadata.push_back(primec::SemanticProgramTypeMetadata{
      .fullPath = "/Point",
      .category = "struct",
      .isPublic = false,
      .hasNoPadding = false,
      .hasPlatformIndependentPadding = false,
      .hasExplicitAlignment = false,
      .explicitAlignmentBytes = 0,
      .fieldCount = 1,
      .enumValueCount = 0,
      .sourceLine = 0,
      .sourceColumn = 0,
      .semanticNodeId = 82,
      .provenanceHandle = 0,
  });

  primec::IrLowerer lowerer;
  primec::IrModule module;
  primec::DiagnosticSinkReport diagnosticInfo;
  std::string error;

  CHECK_FALSE(lowerer.lower(program, &semanticProgram, "/main", {}, {}, module, error, &diagnosticInfo));
  CHECK(error == "missing semantic-product struct field metadata: /Point/x");
  CHECK(diagnosticInfo.message == error);
}

TEST_CASE("ir lowerer rejects stale semantic-product struct provenance") {
  primec::Program program;

  primec::Definition mainDef;
  mainDef.fullPath = "/main";
  mainDef.semanticNodeId = 81;
  program.definitions.push_back(mainDef);

  primec::SemanticProgram semanticProgram;
  semanticProgram.entryPath = "/main";
  addVoidCallableSummary(semanticProgram, 81);
  semanticProgram.typeMetadata.push_back(primec::SemanticProgramTypeMetadata{
      .fullPath = "/Ghost",
      .category = "struct",
      .isPublic = false,
      .hasNoPadding = false,
      .hasPlatformIndependentPadding = false,
      .hasExplicitAlignment = false,
      .explicitAlignmentBytes = 0,
      .fieldCount = 0,
      .enumValueCount = 0,
      .sourceLine = 0,
      .sourceColumn = 0,
      .semanticNodeId = 82,
      .provenanceHandle = 0,
  });

  primec::IrLowerer lowerer;
  primec::IrModule module;
  primec::DiagnosticSinkReport diagnosticInfo;
  std::string error;

  CHECK_FALSE(lowerer.lower(program, &semanticProgram, "/main", {}, {}, module, error, &diagnosticInfo));
  CHECK(error == "missing semantic-product struct provenance: /Ghost");
  CHECK(diagnosticInfo.message == error);
}

TEST_CASE("ir lowerer completeness checks keep deterministic first-failure order") {
  primec::Program program;

  primec::Definition callee;
  callee.fullPath = "/callee";
  callee.semanticNodeId = 82;
  program.definitions.push_back(callee);

  primec::Definition mainDef;
  mainDef.fullPath = "/main";
  mainDef.semanticNodeId = 81;

  primec::Expr param;
  param.name = "value";
  param.semanticNodeId = 45;
  primec::Transform paramArrayTransform;
  paramArrayTransform.name = "array";
  paramArrayTransform.templateArgs = {"string"};
  param.transforms.push_back(paramArrayTransform);
  mainDef.parameters.push_back(param);

  primec::Expr callExpr;
  callExpr.kind = primec::Expr::Kind::Call;
  callExpr.name = "callee";
  callExpr.semanticNodeId = 46;
  mainDef.statements.push_back(callExpr);

  primec::Expr initializerExpr;
  initializerExpr.kind = primec::Expr::Kind::Literal;
  initializerExpr.literalValue = 1;
  initializerExpr.intWidth = 32;

  primec::Expr bindingExpr;
  bindingExpr.isBinding = true;
  bindingExpr.name = "selected";
  bindingExpr.semanticNodeId = 47;
  bindingExpr.args.push_back(initializerExpr);
  mainDef.statements.push_back(bindingExpr);
  program.definitions.push_back(mainDef);

  primec::IrLowerer lowerer;
  auto lowerWithSemanticProduct = [&](primec::SemanticProgram &semanticProgram,
                                      std::string &errorOut,
                                      primec::DiagnosticSinkReport &diagnosticOut) {
    primec::IrModule module;
    return lowerer.lower(program, &semanticProgram, "/main", {}, {}, module, errorOut, &diagnosticOut);
  };

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
      .semanticNodeId = 81,
      .provenanceHandle = 0,
      .fullPathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/main"),
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
      .hasOnError = false,
      .onErrorHandlerPath = "",
      .onErrorErrorType = "",
      .onErrorBoundArgCount = 0,
      .semanticNodeId = 82,
      .provenanceHandle = 0,
      .fullPathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/callee"),
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
      .semanticNodeId = 81,
      .definitionPathId =
          primec::semanticProgramInternCallTargetString(semanticProgram, "/main"),
  });
  semanticProgram.publishedRoutingLookups.returnFactIndicesByDefinitionId
      .insert_or_assign(81, semanticProgram.returnFacts.size() - 1);
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
      .semanticNodeId = 82,
      .definitionPathId =
          primec::semanticProgramInternCallTargetString(semanticProgram, "/callee"),
  });
  semanticProgram.publishedRoutingLookups.returnFactIndicesByDefinitionId
      .insert_or_assign(82, semanticProgram.returnFacts.size() - 1);
  publishFixtureCallableAndReturnRouting(semanticProgram);

  std::string error;
  primec::DiagnosticSinkReport diagnosticInfo;
  CHECK_FALSE(lowerWithSemanticProduct(semanticProgram, error, diagnosticInfo));
  CHECK(error == "missing semantic-product binding fact: /main -> parameter value");
  CHECK(diagnosticInfo.message == error);

  semanticProgram.directCallTargets.push_back(primec::SemanticProgramDirectCallTarget{
      .scopePath = "/main",
      .callName = "callee",
      .sourceLine = 0,
      .sourceColumn = 0,
      .semanticNodeId = 46,
      .resolvedPathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/callee"),
      .stdlibSurfaceId = std::nullopt,
  });

  error.clear();
  diagnosticInfo = {};
  CHECK_FALSE(lowerWithSemanticProduct(semanticProgram, error, diagnosticInfo));
  CHECK(error == "missing semantic-product binding fact: /main -> parameter value");
  CHECK(diagnosticInfo.message == error);

  semanticProgram.bindingFacts.push_back(primec::SemanticProgramBindingFact{
      .scopePath = "/main",
      .siteKind = "parameter",
      .name = "value",
      .bindingTypeText = "array<string>",
      .isMutable = false,
      .isEntryArgString = false,
      .isUnsafeReference = false,
      .referenceRoot = "",
      .sourceLine = 0,
      .sourceColumn = 0,
      .semanticNodeId = 45,
      .provenanceHandle = 0,
      .resolvedPathId =
          primec::semanticProgramInternCallTargetString(semanticProgram, "/main/value"),
  });
  semanticProgram.publishedRoutingLookups.bindingFactIndicesByExpr.insert_or_assign(45, 0);
  semanticProgram.bindingFacts.push_back(primec::SemanticProgramBindingFact{
      .scopePath = "/main",
      .siteKind = "local",
      .name = "selected",
      .bindingTypeText = "i32",
      .isMutable = false,
      .isEntryArgString = false,
      .isUnsafeReference = false,
      .referenceRoot = "",
      .sourceLine = 0,
      .sourceColumn = 0,
      .semanticNodeId = 47,
      .provenanceHandle = 0,
      .resolvedPathId =
          primec::semanticProgramInternCallTargetString(semanticProgram, "/main/selected"),
  });
  semanticProgram.publishedRoutingLookups.bindingFactIndicesByExpr.insert_or_assign(47, 1);

  semanticProgram.bindingFacts.back().bindingTypeTextId =
      static_cast<primec::SymbolId>(semanticProgram.callTargetStringTable.size() + 1u);
  error.clear();
  diagnosticInfo = {};
  CHECK_FALSE(lowerWithSemanticProduct(semanticProgram, error, diagnosticInfo));
  CHECK(error == "missing semantic-product binding type id: /main -> local selected");
  CHECK(diagnosticInfo.message == error);

  semanticProgram.bindingFacts.back().bindingTypeTextId =
      primec::semanticProgramInternCallTargetString(semanticProgram, "i64");
  error.clear();
  diagnosticInfo = {};
  CHECK_FALSE(lowerWithSemanticProduct(semanticProgram, error, diagnosticInfo));
  CHECK(error == "stale semantic-product binding type metadata: /main -> local selected");
  CHECK(diagnosticInfo.message == error);

  semanticProgram.bindingFacts.back().bindingTypeTextId =
      primec::semanticProgramInternCallTargetString(semanticProgram, "i32");
  semanticProgram.bindingFacts.back().referenceRoot = "selected";
  semanticProgram.bindingFacts.back().referenceRootId =
      primec::semanticProgramInternCallTargetString(semanticProgram, "other");
  error.clear();
  diagnosticInfo = {};
  CHECK_FALSE(lowerWithSemanticProduct(semanticProgram, error, diagnosticInfo));
  CHECK(error ==
        "stale semantic-product binding reference root metadata: /main -> local selected");
  CHECK(diagnosticInfo.message == error);

  semanticProgram.bindingFacts.back().referenceRoot = "";
  semanticProgram.bindingFacts.back().referenceRootId = primec::InvalidSymbolId;
  semanticProgram.bindingFacts.back().resolvedPathId =
      static_cast<primec::SymbolId>(semanticProgram.callTargetStringTable.size() + 1u);
  error.clear();
  diagnosticInfo = {};
  CHECK_FALSE(lowerWithSemanticProduct(semanticProgram, error, diagnosticInfo));
  CHECK(error == "missing semantic-product binding resolved path id: /main -> local selected");
  CHECK(diagnosticInfo.message == error);

  semanticProgram.bindingFacts.back().resolvedPathId =
      primec::semanticProgramInternCallTargetString(semanticProgram, "/main/selected");
  primec::SemanticProgramLocalAutoFact localAutoFact;
  localAutoFact.scopePath = "/main";
  localAutoFact.bindingName = "selected";
  localAutoFact.bindingTypeText = "i32";
  localAutoFact.semanticNodeId = 47;
  localAutoFact.bindingTypeTextId =
      static_cast<primec::SymbolId>(semanticProgram.callTargetStringTable.size() + 1u);
  localAutoFact.initializerResolvedPathId =
      static_cast<primec::SymbolId>(semanticProgram.callTargetStringTable.size() + 1u);
  semanticProgram.localAutoFacts.push_back(std::move(localAutoFact));
  semanticProgram.publishedRoutingLookups.localAutoFactIndicesByExpr.insert_or_assign(47, 0);
  error.clear();
  diagnosticInfo = {};
  CHECK_FALSE(lowerWithSemanticProduct(semanticProgram, error, diagnosticInfo));
  CHECK(error == "missing semantic-product local-auto binding type id: /main -> local selected");
  CHECK(diagnosticInfo.message == error);

  semanticProgram.localAutoFacts.back().bindingTypeTextId =
      primec::semanticProgramInternCallTargetString(semanticProgram, "i64");
  error.clear();
  diagnosticInfo = {};
  CHECK_FALSE(lowerWithSemanticProduct(semanticProgram, error, diagnosticInfo));
  CHECK(error ==
        "stale semantic-product local-auto binding type metadata: /main -> local selected");
  CHECK(diagnosticInfo.message == error);

  semanticProgram.localAutoFacts.back().bindingTypeTextId =
      primec::semanticProgramInternCallTargetString(semanticProgram, "i32");
  semanticProgram.localAutoFacts.back().initializerResolvedPathId =
      static_cast<primec::SymbolId>(semanticProgram.callTargetStringTable.size() + 1u);
  error.clear();
  diagnosticInfo = {};
  CHECK_FALSE(lowerWithSemanticProduct(semanticProgram, error, diagnosticInfo));
  CHECK(error == "missing semantic-product local-auto initializer path id: /main -> local selected");
  CHECK(diagnosticInfo.message == error);

  semanticProgram.localAutoFacts.back().initializerResolvedPathId = primec::InvalidSymbolId;
  error.clear();
  diagnosticInfo = {};
  CHECK(lowerWithSemanticProduct(semanticProgram, error, diagnosticInfo));
  CHECK(error.empty());
  CHECK(diagnosticInfo.message.empty());

  semanticProgram.localAutoFacts.back().initializerResolvedPathId =
      primec::semanticProgramInternCallTargetString(semanticProgram, "/callee");
  semanticProgram.localAutoFacts.back().initializerDirectCallResolvedPathId =
      static_cast<primec::SymbolId>(semanticProgram.callTargetStringTable.size() + 1u);
  error.clear();
  diagnosticInfo = {};
  CHECK_FALSE(lowerWithSemanticProduct(semanticProgram, error, diagnosticInfo));
  CHECK(error ==
        "missing semantic-product local-auto direct-call path id: /main -> local selected");
  CHECK(diagnosticInfo.message == error);

  semanticProgram.localAutoFacts.back().initializerDirectCallResolvedPathId =
      primec::semanticProgramInternCallTargetString(semanticProgram, "/stale_callee");
  error.clear();
  diagnosticInfo = {};
  CHECK_FALSE(lowerWithSemanticProduct(semanticProgram, error, diagnosticInfo));
  CHECK(error ==
        "stale semantic-product local-auto direct-call fact: /main -> local selected");
  CHECK(diagnosticInfo.message == error);

  semanticProgram.localAutoFacts.back().initializerDirectCallResolvedPathId =
      primec::InvalidSymbolId;
  semanticProgram.localAutoFacts.back().initializerMethodCallResolvedPathId =
      static_cast<primec::SymbolId>(semanticProgram.callTargetStringTable.size() + 1u);
  error.clear();
  diagnosticInfo = {};
  CHECK_FALSE(lowerWithSemanticProduct(semanticProgram, error, diagnosticInfo));
  CHECK(error ==
        "missing semantic-product local-auto method-call path id: /main -> local selected");
  CHECK(diagnosticInfo.message == error);

  semanticProgram.localAutoFacts.back().initializerMethodCallResolvedPathId =
      primec::semanticProgramInternCallTargetString(semanticProgram, "/stale_callee");
  error.clear();
  diagnosticInfo = {};
  CHECK_FALSE(lowerWithSemanticProduct(semanticProgram, error, diagnosticInfo));
  CHECK(error ==
        "stale semantic-product local-auto method-call fact: /main -> local selected");
  CHECK(diagnosticInfo.message == error);

  semanticProgram.localAutoFacts.back().initializerMethodCallResolvedPathId =
      primec::InvalidSymbolId;
  semanticProgram.localAutoFacts.back().initializerResolvedPathId =
      primec::semanticProgramInternCallTargetString(semanticProgram, "/callee");
  semanticProgram.localAutoFacts.back().initializerDirectCallResolvedPathId =
      primec::semanticProgramInternCallTargetString(semanticProgram, "/callee");
  semanticProgram.localAutoFacts.back().initializerDirectCallReturnKindId =
      static_cast<primec::SymbolId>(semanticProgram.callTargetStringTable.size() + 1u);
  error.clear();
  diagnosticInfo = {};
  CHECK_FALSE(lowerWithSemanticProduct(semanticProgram, error, diagnosticInfo));
  CHECK(error ==
        "missing semantic-product local-auto direct-call return-kind id: /main -> local selected");
  CHECK(diagnosticInfo.message == error);

  semanticProgram.localAutoFacts.back().initializerDirectCallReturnKind = "i64";
  semanticProgram.localAutoFacts.back().initializerDirectCallReturnKindId =
      primec::semanticProgramInternCallTargetString(semanticProgram, "i64");
  error.clear();
  diagnosticInfo = {};
  CHECK_FALSE(lowerWithSemanticProduct(semanticProgram, error, diagnosticInfo));
  CHECK(error ==
        "stale semantic-product local-auto direct-call return-kind fact: /main -> local selected");
  CHECK(diagnosticInfo.message == error);

  semanticProgram.localAutoFacts.back().initializerDirectCallResolvedPathId =
      primec::InvalidSymbolId;
  semanticProgram.localAutoFacts.back().initializerDirectCallReturnKind = "void";
  semanticProgram.localAutoFacts.back().initializerDirectCallReturnKindId =
      primec::semanticProgramInternCallTargetString(semanticProgram, "void");
  semanticProgram.localAutoFacts.back().initializerMethodCallResolvedPathId =
      primec::semanticProgramInternCallTargetString(semanticProgram, "/callee");
  semanticProgram.localAutoFacts.back().initializerMethodCallReturnKind = "i64";
  semanticProgram.localAutoFacts.back().initializerMethodCallReturnKindId =
      primec::semanticProgramInternCallTargetString(semanticProgram, "i64");
  error.clear();
  diagnosticInfo = {};
  CHECK_FALSE(lowerWithSemanticProduct(semanticProgram, error, diagnosticInfo));
  CHECK(error ==
        "stale semantic-product local-auto method-call return-kind fact: /main -> local selected");
  CHECK(diagnosticInfo.message == error);
}

TEST_CASE("semantic-product local-auto call paths accept stdlib surface equivalents") {
  primec::Program program;
  primec::Definition mainDef;
  mainDef.fullPath = "/main";
  primec::Expr selected;
  selected.isBinding = true;
  selected.name = "selected";
  selected.semanticNodeId = 47;
  primec::Expr initializer;
  initializer.kind = primec::Expr::Kind::Call;
  initializer.name = "/std/collections/vectorAt__t25a78a513414c3bf";
  selected.args.push_back(initializer);
  mainDef.statements.push_back(selected);
  program.definitions.push_back(mainDef);

  primec::SemanticProgram semanticProgram;
  primec::SemanticProgramLocalAutoFact localAutoFact;
  localAutoFact.scopePath = "/main";
  localAutoFact.bindingName = "selected";
  localAutoFact.bindingTypeText = "i32";
  localAutoFact.initializerDirectCallResolvedPath =
      "/std/collections/vectorAt__t25a78a513414c3bf";
  localAutoFact.initializerDirectCallReturnKind = "i32";
  localAutoFact.initializerStdlibSurfaceId =
      primec::StdlibSurfaceId::CollectionsManifestSurface0;
  localAutoFact.initializerDirectCallStdlibSurfaceId =
      primec::StdlibSurfaceId::CollectionsManifestSurface0;
  localAutoFact.semanticNodeId = 47;
  localAutoFact.bindingTypeTextId =
      primec::semanticProgramInternCallTargetString(semanticProgram, "i32");
  localAutoFact.bindingTypeText = "";
  localAutoFact.initializerResolvedPathId =
      primec::semanticProgramInternCallTargetString(semanticProgram,
                                                    "/std/collections/vector/at");
  localAutoFact.initializerDirectCallResolvedPathId =
      primec::semanticProgramInternCallTargetString(
          semanticProgram, "/std/collections/vectorAt__t25a78a513414c3bf");
  localAutoFact.initializerDirectCallReturnKindId =
      primec::semanticProgramInternCallTargetString(semanticProgram, "i32");
  semanticProgram.localAutoFacts.push_back(localAutoFact);
  semanticProgram.publishedRoutingLookups.localAutoFactIndicesByExpr.insert_or_assign(47, 0);

  std::string error;
  CHECK(primec::ir_lowerer::validateSemanticProductLocalAutoCoverage(
      program, &semanticProgram, error));
  CHECK(error.empty());

  semanticProgram.localAutoFacts.back().initializerDirectCallStdlibSurfaceId =
      primec::findStdlibSurfaceMetadataByBridgeKey("collections.map_helpers")->id;
  semanticProgram.localAutoFacts.back().initializerDirectCallResolvedPathId =
      primec::semanticProgramInternCallTargetString(
          semanticProgram, "/std/collections/map/contains__t25a78a513414c3bf");
  CHECK_FALSE(primec::ir_lowerer::validateSemanticProductLocalAutoCoverage(
      program, &semanticProgram, error));
  CHECK(error ==
        "stale semantic-product local-auto direct-call fact: /main -> local selected");
}

TEST_CASE("semantic-product local-auto call paths accept specialized direct calls") {
  primec::Program program;
  primec::Definition mainDef;
  mainDef.fullPath = "/main";
  primec::Expr selected;
  selected.isBinding = true;
  selected.name = "selected";
  selected.semanticNodeId = 47;
  primec::Expr initializer;
  initializer.kind = primec::Expr::Kind::Call;
  initializer.name = "wrapValues";
  selected.args.push_back(initializer);
  mainDef.statements.push_back(selected);
  program.definitions.push_back(mainDef);

  primec::SemanticProgram semanticProgram;
  primec::SemanticProgramLocalAutoFact localAutoFact;
  localAutoFact.scopePath = "/main";
  localAutoFact.bindingName = "selected";
  localAutoFact.bindingTypeText = "map<string, i32>";
  localAutoFact.initializerDirectCallResolvedPath =
      "/wrapValues__t9b7bdbb33f7d43aa";
  localAutoFact.semanticNodeId = 47;
  localAutoFact.bindingTypeTextId =
      primec::semanticProgramInternCallTargetString(semanticProgram,
                                                    "map<string, i32>");
  localAutoFact.initializerResolvedPathId =
      primec::semanticProgramInternCallTargetString(semanticProgram, "/wrapValues");
  localAutoFact.initializerDirectCallResolvedPathId =
      primec::semanticProgramInternCallTargetString(
          semanticProgram, "/wrapValues__t9b7bdbb33f7d43aa");
  semanticProgram.localAutoFacts.push_back(localAutoFact);
  semanticProgram.publishedRoutingLookups.localAutoFactIndicesByExpr.insert_or_assign(47, 0);

  std::string error;
  CHECK(primec::ir_lowerer::validateSemanticProductLocalAutoCoverage(
      program, &semanticProgram, error));
  CHECK(error.empty());

  semanticProgram.localAutoFacts.back().initializerDirectCallResolvedPath =
      "/otherWrap__t9b7bdbb33f7d43aa";
  semanticProgram.localAutoFacts.back().initializerDirectCallResolvedPathId =
      primec::semanticProgramInternCallTargetString(
          semanticProgram, "/otherWrap__t9b7bdbb33f7d43aa");
  CHECK_FALSE(primec::ir_lowerer::validateSemanticProductLocalAutoCoverage(
      program, &semanticProgram, error));
  CHECK(error ==
        "stale semantic-product local-auto direct-call fact: /main -> local selected");
}

TEST_CASE("semantic-product local-auto call paths accept overloaded specialized direct calls") {
  primec::Program program;
  primec::Definition mainDef;
  mainDef.fullPath = "/main";
  primec::Expr selected;
  selected.isBinding = true;
  selected.name = "selected";
  selected.semanticNodeId = 47;
  primec::Expr initializer;
  initializer.kind = primec::Expr::Kind::Call;
  initializer.name = "wrapValues";
  selected.args.push_back(initializer);
  mainDef.statements.push_back(selected);
  program.definitions.push_back(mainDef);

  primec::SemanticProgram semanticProgram;
  primec::SemanticProgramLocalAutoFact localAutoFact;
  localAutoFact.scopePath = "/main";
  localAutoFact.bindingName = "selected";
  localAutoFact.bindingTypeText = "vector<i32>";
  localAutoFact.initializerDirectCallResolvedPath =
      "/wrapValues__ov2__t9b7bdbb33f7d43aa";
  localAutoFact.semanticNodeId = 47;
  localAutoFact.bindingTypeTextId =
      primec::semanticProgramInternCallTargetString(semanticProgram, "vector<i32>");
  localAutoFact.initializerResolvedPathId =
      primec::semanticProgramInternCallTargetString(semanticProgram, "/wrapValues__ov2");
  localAutoFact.initializerDirectCallResolvedPathId =
      primec::semanticProgramInternCallTargetString(
          semanticProgram, "/wrapValues__ov2__t9b7bdbb33f7d43aa");
  semanticProgram.localAutoFacts.push_back(localAutoFact);
  semanticProgram.publishedRoutingLookups.localAutoFactIndicesByExpr.insert_or_assign(47, 0);

  std::string error;
  CHECK(primec::ir_lowerer::validateSemanticProductLocalAutoCoverage(
      program, &semanticProgram, error));
  CHECK(error.empty());
}

TEST_CASE("ir preparation rejects semantic-product local-auto path fallback in production mode") {
  primec::Program program;

  primec::Definition calleeDef;
  calleeDef.fullPath = "/callee";
  calleeDef.semanticNodeId = 183;
  primec::Expr calleeReturnExpr;
  calleeReturnExpr.kind = primec::Expr::Kind::Literal;
  calleeReturnExpr.literalValue = 1;
  calleeReturnExpr.intWidth = 32;
  calleeDef.returnExpr = calleeReturnExpr;
  program.definitions.push_back(calleeDef);

  primec::Definition mainDef;
  mainDef.fullPath = "/main";
  mainDef.semanticNodeId = 181;
  primec::Expr initCall;
  initCall.kind = primec::Expr::Kind::Call;
  initCall.name = "callee";
  initCall.semanticNodeId = 182;
  primec::Expr localBinding;
  localBinding.isBinding = true;
  localBinding.name = "selected";
  localBinding.semanticNodeId = 184;
  localBinding.args.push_back(initCall);
  mainDef.statements.push_back(localBinding);
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
      .semanticNodeId = 183,
      .provenanceHandle = 0,
      .fullPathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/callee"),
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
      .hasOnError = false,
      .onErrorHandlerPath = "",
      .onErrorErrorType = "",
      .onErrorBoundArgCount = 0,
      .semanticNodeId = 181,
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
      .semanticNodeId = 183,
      .definitionPathId =
          primec::semanticProgramInternCallTargetString(semanticProgram, "/callee"),
  });
  semanticProgram.publishedRoutingLookups.returnFactIndicesByDefinitionId
      .insert_or_assign(183, semanticProgram.returnFacts.size() - 1);
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
      .semanticNodeId = 181,
      .definitionPathId =
          primec::semanticProgramInternCallTargetString(semanticProgram, "/main"),
  });
  semanticProgram.publishedRoutingLookups.returnFactIndicesByDefinitionId
      .insert_or_assign(181, semanticProgram.returnFacts.size() - 1);
  semanticProgram.directCallTargets.push_back(primec::SemanticProgramDirectCallTarget{
      .scopePath = "/main",
      .callName = "callee",
      .sourceLine = 0,
      .sourceColumn = 0,
      .semanticNodeId = 182,
      .resolvedPathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/callee"),
      .stdlibSurfaceId = std::nullopt,
  });
  semanticProgram.bindingFacts.push_back(primec::SemanticProgramBindingFact{
      .scopePath = "/main",
      .siteKind = "local",
      .name = "selected",
      .bindingTypeText = "i32",
      .isMutable = false,
      .isEntryArgString = false,
      .isUnsafeReference = false,
      .referenceRoot = "",
      .sourceLine = 0,
      .sourceColumn = 0,
      .semanticNodeId = 184,
      .provenanceHandle = 0,
      .resolvedPathId =
          primec::semanticProgramInternCallTargetString(semanticProgram, "/main/selected"),
  });
  semanticProgram.publishedRoutingLookups.bindingFactIndicesByExpr.insert_or_assign(184, 0);
  semanticProgram.localAutoFacts.push_back(primec::SemanticProgramLocalAutoFact{
      .scopePath = "/main",
      .bindingName = "selected",
      .bindingTypeText = "i32",
      .initializerBindingTypeText = "i32",
      .initializerReceiverBindingTypeText = "",
      .initializerQueryTypeText = "",
      .initializerResultHasValue = false,
      .initializerResultValueType = "",
      .initializerResultErrorType = "",
      .initializerHasTry = false,
      .initializerTryOperandResolvedPath = "",
      .initializerTryOperandBindingTypeText = "",
      .initializerTryOperandReceiverBindingTypeText = "",
      .initializerTryOperandQueryTypeText = "",
      .initializerTryValueType = "",
      .initializerTryErrorType = "",
      .initializerTryContextReturnKind = "",
      .initializerTryOnErrorHandlerPath = "",
      .initializerTryOnErrorErrorType = "",
      .initializerTryOnErrorBoundArgCount = 0,
      .sourceLine = 0,
      .sourceColumn = 0,
      .semanticNodeId = 0,
      .provenanceHandle = 0,
      .initializerDirectCallResolvedPath = "",
      .initializerDirectCallReturnKind = "",
      .initializerMethodCallResolvedPath = "",
      .initializerMethodCallReturnKind = "",
      .initializerStdlibSurfaceId = std::nullopt,
      .initializerDirectCallStdlibSurfaceId = std::nullopt,
      .initializerMethodCallStdlibSurfaceId = std::nullopt,
      .bindingNameId = primec::semanticProgramInternCallTargetString(semanticProgram, "selected"),
      .initializerResolvedPathId =
          primec::semanticProgramInternCallTargetString(semanticProgram, "/callee"),
  });

  primec::Options options;
  options.entryPath = "/main";

  primec::IrModule ir;
  primec::IrPreparationFailure failure;
  CHECK_FALSE(primec::prepareIrModule(
      program, &semanticProgram, options, primec::IrValidationTarget::Vm, ir, failure));
  CHECK(failure.stage == primec::IrPreparationFailureStage::Lowering);
  CHECK(failure.message == "missing semantic-product local-auto fact: /main -> local selected");
  CHECK(failure.diagnosticInfo.message == failure.message);
}

TEST_CASE("ir lowerer rejects missing semantic-product callable summaries") {
  primec::Program program;

  primec::Definition mainDef;
  mainDef.fullPath = "/main";
  program.definitions.push_back(mainDef);

  primec::SemanticProgram semanticProgram;
  semanticProgram.entryPath = "/main";

  primec::IrLowerer lowerer;
  primec::IrModule module;
  primec::DiagnosticSinkReport diagnosticInfo;
  std::string error;

  CHECK_FALSE(lowerer.lower(program, &semanticProgram, "/main", {}, {}, module, error, &diagnosticInfo));
  CHECK(error == "missing semantic-product callable summary: /main");
  CHECK(diagnosticInfo.message == error);
}

TEST_CASE("ir lowerer effect validation skips semantic-product sum definitions") {
  primec::Program program;

  primec::Definition sumDef;
  sumDef.fullPath = "/std/result/Result__arity2__tabc";
  sumDef.semanticNodeId = 81;
  program.definitions.push_back(sumDef);

  primec::Definition mainDef;
  mainDef.fullPath = "/main";
  mainDef.semanticNodeId = 82;
  program.definitions.push_back(mainDef);

  primec::SemanticProgram semanticProgram;
  semanticProgram.entryPath = "/main";
  semanticProgram.typeMetadata.push_back(primec::SemanticProgramTypeMetadata{
      .fullPath = sumDef.fullPath,
      .category = "sum",
      .semanticNodeId = sumDef.semanticNodeId,
  });
  semanticProgram.sumTypeMetadata.push_back(primec::SemanticProgramSumTypeMetadata{
      .fullPath = sumDef.fullPath,
      .isPublic = false,
      .activeTagTypeText = "u32",
      .payloadStorageText = "array",
      .variantCount = 2,
      .semanticNodeId = sumDef.semanticNodeId,
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
      .hasOnError = false,
      .onErrorHandlerPath = "",
      .onErrorErrorType = "",
      .onErrorBoundArgCount = 0,
      .semanticNodeId = mainDef.semanticNodeId,
      .provenanceHandle = 0,
      .fullPathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/main"),
  });
  publishFixtureCallableAndReturnRouting(semanticProgram);

  std::string error;
  CHECK(primec::ir_lowerer::validateProgramEffectsForBackendSurface(
      program, &semanticProgram, "/main", {}, {}, "native backend", error));
  CHECK(error.empty());
}

TEST_CASE("ir lowerer rejects missing semantic-product entry parameter facts") {
  primec::Program program;

  primec::Definition mainDef;
  mainDef.fullPath = "/main";
  mainDef.semanticNodeId = 81;
  primec::Expr entryParam;
  entryParam.name = "argv";
  primec::Transform arrayTransform;
  arrayTransform.name = "array";
  arrayTransform.templateArgs = {"string"};
  entryParam.transforms.push_back(arrayTransform);
  mainDef.parameters.push_back(entryParam);
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
      .semanticNodeId = 81,
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
      .semanticNodeId = 81,
      .definitionPathId =
          primec::semanticProgramInternCallTargetString(semanticProgram, "/main"),
  });
  semanticProgram.publishedRoutingLookups.returnFactIndicesByDefinitionId
      .insert_or_assign(81, semanticProgram.returnFacts.size() - 1);

  primec::IrLowerer lowerer;
  primec::IrModule module;
  primec::DiagnosticSinkReport diagnosticInfo;
  std::string error;

  CHECK_FALSE(lowerer.lower(program, &semanticProgram, "/main", {}, {}, module, error, &diagnosticInfo));
  CHECK(error == "missing semantic-product entry parameter fact: /main");
  CHECK(diagnosticInfo.message == error);
}

TEST_CASE("ir lowerer rejects missing semantic-product return facts") {
  primec::Program program;

  primec::Definition mainDef;
  mainDef.fullPath = "/main";
  mainDef.semanticNodeId = 81;
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
      .semanticNodeId = 81,
      .provenanceHandle = 0,
      .fullPathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/main"),
  });
  publishFixtureCallableAndReturnRouting(semanticProgram);

  primec::IrLowerer lowerer;
  primec::IrModule module;
  primec::DiagnosticSinkReport diagnosticInfo;
  std::string error;

  CHECK_FALSE(lowerer.lower(program, &semanticProgram, "/main", {}, {}, module, error, &diagnosticInfo));
  CHECK(error == "missing semantic-product return fact: /main");
  CHECK(diagnosticInfo.message == error);
}

TEST_CASE("ir lowerer rejects missing semantic-product callable result metadata") {
  primec::Program program;

  primec::Definition mainDef;
  mainDef.fullPath = "/main";
  mainDef.semanticNodeId = 82;
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
      .hasResultType = true,
      .resultTypeHasValue = true,
      .resultValueType = "",
      .resultErrorType = "FileError",
      .hasOnError = false,
      .onErrorHandlerPath = "",
      .onErrorErrorType = "",
      .onErrorBoundArgCount = 0,
      .semanticNodeId = 82,
      .provenanceHandle = 0,
      .fullPathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/main"),
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
      .semanticNodeId = 82,
      .definitionPathId =
          primec::semanticProgramInternCallTargetString(semanticProgram, "/main"),
  });
  semanticProgram.publishedRoutingLookups.returnFactIndicesByDefinitionId
      .insert_or_assign(82, semanticProgram.returnFacts.size() - 1);

  primec::IrLowerer lowerer;
  primec::IrModule module;
  primec::DiagnosticSinkReport diagnosticInfo;
  std::string error;

  CHECK_FALSE(lowerer.lower(program, &semanticProgram, "/main", {}, {}, module, error, &diagnosticInfo));
  CHECK(error == "missing semantic-product callable result metadata: /main");
  CHECK(diagnosticInfo.message == error);
}

TEST_CASE("ir lowerer callable result metadata uses interned value ids") {
  primec::SemanticProgram semanticProgram;
  semanticProgram.entryPath = "/main";
  semanticProgram.callableSummaries.push_back(primec::SemanticProgramCallableSummary{
      .isExecution = false,
      .returnKind = "return",
      .isCompute = false,
      .isUnsafe = false,
      .activeEffects = {},
      .activeCapabilities = {},
      .hasResultType = true,
      .resultTypeHasValue = true,
      .resultValueType = "",
      .resultErrorType = "FileError",
      .hasOnError = false,
      .onErrorHandlerPath = "",
      .onErrorErrorType = "",
      .onErrorBoundArgCount = 0,
      .semanticNodeId = 8202,
      .provenanceHandle = 0,
      .fullPathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/main"),
      .resultValueTypeId = primec::semanticProgramInternCallTargetString(semanticProgram, "i32"),
      .resultErrorTypeId =
          primec::semanticProgramInternCallTargetString(semanticProgram, "FileError"),
  });

  std::string error;
  CHECK(primec::ir_lowerer::validateSemanticProductResultMetadataCompleteness(
      &semanticProgram, error));
  CHECK(error.empty());

  semanticProgram.callableSummaries.back().resultValueTypeId =
      static_cast<primec::SymbolId>(semanticProgram.callTargetStringTable.size() + 1u);
  CHECK_FALSE(primec::ir_lowerer::validateSemanticProductResultMetadataCompleteness(
      &semanticProgram, error));
  CHECK(error == "missing semantic-product callable result metadata: /main");
}

TEST_CASE("ir lowerer rejects missing semantic-product callable summary path id") {
  primec::Program program;

  primec::Definition mainDef;
  mainDef.fullPath = "/main";
  mainDef.semanticNodeId = 8201;
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
      .semanticNodeId = 8201,
      .provenanceHandle = 0,
      .fullPathId = primec::InvalidSymbolId,
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
      .semanticNodeId = 8201,
      .definitionPathId =
          primec::semanticProgramInternCallTargetString(semanticProgram, "/main"),
  });
  semanticProgram.publishedRoutingLookups.returnFactIndicesByDefinitionId
      .insert_or_assign(8201, semanticProgram.returnFacts.size() - 1);

  primec::IrLowerer lowerer;
  primec::IrModule module;
  primec::DiagnosticSinkReport diagnosticInfo;
  std::string error;

  CHECK_FALSE(lowerer.lower(program, &semanticProgram, "/main", {}, {}, module, error, &diagnosticInfo));
  CHECK(error == "missing semantic-product callable summary path id");
  CHECK(diagnosticInfo.message == error);
}

TEST_CASE("ir lowerer rejects invalid semantic-product callable summary path id") {
  primec::Program program;

  primec::Definition mainDef;
  mainDef.fullPath = "/main";
  mainDef.semanticNodeId = 8202;
  primec::Expr returnExpr;
  returnExpr.kind = primec::Expr::Kind::Literal;
  returnExpr.literalValue = 1;
  returnExpr.intWidth = 32;
  mainDef.returnExpr = returnExpr;
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
      .semanticNodeId = 8202,
      .provenanceHandle = 0,
      .fullPathId =
          static_cast<primec::SymbolId>(semanticProgram.callTargetStringTable.size() + 1u),
  });

  primec::IrLowerer lowerer;
  primec::IrModule module;
  primec::DiagnosticSinkReport diagnosticInfo;
  std::string error;

  CHECK_FALSE(lowerer.lower(program, &semanticProgram, "/main", {}, {}, module, error, &diagnosticInfo));
  CHECK(error == "missing semantic-product callable summary path id");
  CHECK(diagnosticInfo.message == error);
}
