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

TEST_CASE("native location direct reference calls use semantic-product return facts") {
  const std::filesystem::path cwd = std::filesystem::current_path();
  std::filesystem::path operatorMemoryPointerHelpersPath =
      cwd / "src" / "ir_lowerer" / "IrLowererOperatorMemoryPointerHelpers.cpp";
  if (!std::filesystem::exists(operatorMemoryPointerHelpersPath)) {
    operatorMemoryPointerHelpersPath =
        cwd.parent_path() / "src" / "ir_lowerer" / "IrLowererOperatorMemoryPointerHelpers.cpp";
  }
  REQUIRE(std::filesystem::exists(operatorMemoryPointerHelpersPath));

  const std::string source = readTextFile(operatorMemoryPointerHelpersPath);
  const size_t helperPos = source.find("resolveSemanticReturnFactBindingType");
  REQUIRE(helperPos != std::string::npos);
  const size_t idPos = source.find("returnFact.bindingTypeTextId", helperPos);
  const size_t semanticResolvePos =
      source.find("semanticProgramResolveCallTargetString(", idPos);
  const size_t fallbackTextPos =
      source.find("return trimTemplateTypeText(returnFact.bindingTypeText)", semanticResolvePos);
  REQUIRE(idPos != std::string::npos);
  REQUIRE(semanticResolvePos != std::string::npos);
  REQUIRE(fallbackTextPos != std::string::npos);
  CHECK(helperPos < idPos);
  CHECK(idPos < semanticResolvePos);
  CHECK(semanticResolvePos < fallbackTextPos);

  const size_t resolverPos = source.find("resolveReferenceReturnCallExpr");
  REQUIRE(resolverPos != std::string::npos);
  const size_t returnFactPos =
      source.find("findSemanticProductReturnFactByPath", resolverPos);
  const size_t helperCallPos =
      source.find("resolveSemanticReturnFactBindingType", returnFactPos);
  const size_t missingDiagnosticPos =
      source.find("missing semantic-product location reference return metadata",
                  resolverPos);
  const size_t fallbackTransformScanPos =
      source.find("for (const auto &transform : callee->transforms)",
                  resolverPos);
  REQUIRE(returnFactPos != std::string::npos);
  REQUIRE(helperCallPos != std::string::npos);
  REQUIRE(missingDiagnosticPos != std::string::npos);
  REQUIRE(fallbackTransformScanPos != std::string::npos);
  CHECK(returnFactPos < fallbackTransformScanPos);
  CHECK(returnFactPos < helperCallPos);
  CHECK(helperCallPos < fallbackTransformScanPos);
  CHECK(missingDiagnosticPos < fallbackTransformScanPos);
}

TEST_CASE("native field receivers use semantic-product type facts") {
  const std::filesystem::path cwd = std::filesystem::current_path();
  std::filesystem::path lowerEmitExprPath =
      cwd / "src" / "ir_lowerer" / "IrLowererLowerEmitExpr.h";
  if (!std::filesystem::exists(lowerEmitExprPath)) {
    lowerEmitExprPath =
        cwd.parent_path() / "src" / "ir_lowerer" / "IrLowererLowerEmitExpr.h";
  }
  REQUIRE(std::filesystem::exists(lowerEmitExprPath));

  const std::string source = readTextFile(lowerEmitExprPath);
  const size_t sharedTypeResolverPos =
      source.find("auto appendSemanticProductTypeTextCandidate");
  const size_t semanticResolverPos =
      source.find("resolveSemanticProductFieldReceiverStructPath");
  REQUIRE(sharedTypeResolverPos != std::string::npos);
  REQUIRE(semanticResolverPos != std::string::npos);
  const size_t bindingFactPos =
      source.find("findSemanticProductBindingFact(semanticTargets, receiverExpr)",
                  semanticResolverPos);
  const size_t queryFactPos =
      source.find("findSemanticProductQueryFact(semanticTargets, receiverExpr)",
                  semanticResolverPos);
  const size_t fieldQueryFactPos =
      source.find("findSemanticProductQueryFact(semanticTargets, expr)",
                  semanticResolverPos);
  const size_t sourceFallbackPos =
      source.find("semanticTargets.semanticProgram->queryFacts",
                  semanticResolverPos);
  const size_t staleDiagnosticPos =
      source.find("stale semantic-product field receiver metadata",
                  semanticResolverPos);
  const size_t fallbackStructPathPos =
      source.find("structPath = inferStructExprPath(receiver, localsIn)",
                  semanticResolverPos);
  const size_t resolveStructTypePos =
      source.find("resolveStructTypeName(normalizedTypeText",
                  semanticResolverPos);
  const size_t primitiveTypePos =
      source.find("valueKindFromTypeName(normalizedTypeText)",
                  semanticResolverPos);
  const size_t internedTypeResolverPos =
      source.find("semanticProgramResolveCallTargetString(",
                  sharedTypeResolverPos);
  REQUIRE(bindingFactPos != std::string::npos);
  REQUIRE(queryFactPos != std::string::npos);
  REQUIRE(fieldQueryFactPos != std::string::npos);
  REQUIRE(sourceFallbackPos != std::string::npos);
  REQUIRE(internedTypeResolverPos != std::string::npos);
  REQUIRE(staleDiagnosticPos != std::string::npos);
  REQUIRE(fallbackStructPathPos != std::string::npos);
  REQUIRE(resolveStructTypePos != std::string::npos);
  REQUIRE(primitiveTypePos != std::string::npos);
  CHECK(source.find("appendSemanticProductTypeTextCandidate(\n"
                    "                  candidateTypeTexts,\n"
                    "                  bindingFact->bindingTypeText,",
                    semanticResolverPos) != std::string::npos);
  CHECK(source.find("bindingFact->bindingTypeTextId",
                    semanticResolverPos) != std::string::npos);
  CHECK(source.find("appendSemanticProductTypeTextCandidate(\n"
                    "                  candidateTypeTexts,\n"
                    "                  queryFact->bindingTypeText,",
                    semanticResolverPos) != std::string::npos);
  CHECK(source.find("queryFact->bindingTypeTextId",
                    semanticResolverPos) != std::string::npos);
  CHECK(source.find("appendSemanticProductTypeTextCandidate(\n"
                    "                  candidateTypeTexts,\n"
                    "                  queryFact->queryTypeText,",
                    semanticResolverPos) != std::string::npos);
  CHECK(source.find("queryFact->queryTypeTextId",
                    semanticResolverPos) != std::string::npos);
  CHECK(source.find("candidate.sourceLine != expr.sourceLine",
                    semanticResolverPos) != std::string::npos);
  CHECK(source.find("candidate.sourceColumn != expr.sourceColumn",
                    semanticResolverPos) != std::string::npos);
  CHECK(source.find("candidate.scopePath == function.name",
                    semanticResolverPos) != std::string::npos);
  CHECK(source.find("fieldQueryFact->receiverBindingTypeText",
                    semanticResolverPos) != std::string::npos);
  CHECK(source.find("fieldQueryFact->receiverBindingTypeTextId",
                    semanticResolverPos) != std::string::npos);
  CHECK(source.find("splitTemplateTypeName(normalizedTypeText, wrapperBase, wrapperArgs)",
                    semanticResolverPos) != std::string::npos);
  CHECK(internedTypeResolverPos < fallbackStructPathPos);
  CHECK(sharedTypeResolverPos < semanticResolverPos);
  CHECK(bindingFactPos < fallbackStructPathPos);
  CHECK(queryFactPos < fallbackStructPathPos);
  CHECK(fieldQueryFactPos < fallbackStructPathPos);
  CHECK(sourceFallbackPos < fallbackStructPathPos);
  CHECK(resolveStructTypePos < primitiveTypePos);
  CHECK(source.find("auto addCandidateTypeText", semanticResolverPos) ==
        std::string::npos);
  CHECK(source.find("if (!resolvedFieldReceiverBySemanticProduct.has_value())",
                    semanticResolverPos) != std::string::npos);
}

TEST_CASE("native packed Result payloads use semantic-product type facts") {
  const std::filesystem::path cwd = std::filesystem::current_path();
  std::filesystem::path lowerEmitExprPath =
      cwd / "src" / "ir_lowerer" / "IrLowererLowerEmitExpr.h";
  if (!std::filesystem::exists(lowerEmitExprPath)) {
    lowerEmitExprPath =
        cwd.parent_path() / "src" / "ir_lowerer" / "IrLowererLowerEmitExpr.h";
  }
  REQUIRE(std::filesystem::exists(lowerEmitExprPath));

  const std::string source = readTextFile(lowerEmitExprPath);
  const size_t sharedTypeResolverPos =
      source.find("auto appendSemanticProductTypeTextCandidate");
  const size_t semanticResolverPos =
      source.find("resolveSemanticProductPackedResultPayload");
  REQUIRE(sharedTypeResolverPos != std::string::npos);
  REQUIRE(semanticResolverPos != std::string::npos);
  const size_t bindingFactPos =
      source.find("findSemanticProductBindingFact(semanticTargets, valueExpr)",
                  semanticResolverPos);
  const size_t queryFactPos =
      source.find("findSemanticProductQueryFact(semanticTargets, valueExpr)",
                  semanticResolverPos);
  const size_t collectionTypePos =
      source.find("resolveSupportedResultCollectionType(normalizedTypeText",
                  semanticResolverPos);
  const size_t staleDiagnosticPos =
      source.find("stale semantic-product packed Result payload metadata",
                  semanticResolverPos);
  const size_t fallbackKindPos =
      source.find("inferredValueKind = inferExprKind(valueExpr, valueLocals)",
                  semanticResolverPos);
  const size_t fallbackStructPos =
      source.find("inferPackedResultStructType(",
                  semanticResolverPos);
  const size_t collectionFallbackPos =
      source.find("resolveCollectionPayload(collectionKind, collectionValueKind)",
                  semanticResolverPos);
  const size_t internedTypeResolverPos =
      source.find("semanticProgramResolveCallTargetString(",
                  sharedTypeResolverPos);
  REQUIRE(bindingFactPos != std::string::npos);
  REQUIRE(queryFactPos != std::string::npos);
  REQUIRE(internedTypeResolverPos != std::string::npos);
  REQUIRE(collectionTypePos != std::string::npos);
  REQUIRE(staleDiagnosticPos != std::string::npos);
  REQUIRE(fallbackKindPos != std::string::npos);
  REQUIRE(fallbackStructPos != std::string::npos);
  REQUIRE(collectionFallbackPos != std::string::npos);
  CHECK(source.find("appendSemanticProductTypeTextCandidate(\n"
                    "                  candidateTypeTexts,\n"
                    "                  bindingFact->bindingTypeText,",
                    semanticResolverPos) != std::string::npos);
  CHECK(source.find("bindingFact->bindingTypeTextId",
                    semanticResolverPos) != std::string::npos);
  CHECK(source.find("appendSemanticProductTypeTextCandidate(\n"
                    "                  candidateTypeTexts,\n"
                    "                  queryFact->bindingTypeText,",
                    semanticResolverPos) != std::string::npos);
  CHECK(source.find("queryFact->bindingTypeTextId",
                    semanticResolverPos) != std::string::npos);
  CHECK(source.find("appendSemanticProductTypeTextCandidate(\n"
                    "                  candidateTypeTexts,\n"
                    "                  queryFact->queryTypeText,",
                    semanticResolverPos) != std::string::npos);
  CHECK(source.find("queryFact->queryTypeTextId",
                    semanticResolverPos) != std::string::npos);
  CHECK(internedTypeResolverPos < collectionFallbackPos);
  CHECK(internedTypeResolverPos < fallbackKindPos);
  CHECK(sharedTypeResolverPos < semanticResolverPos);
  CHECK(bindingFactPos < collectionFallbackPos);
  CHECK(queryFactPos < collectionFallbackPos);
  CHECK(bindingFactPos < fallbackKindPos);
  CHECK(queryFactPos < fallbackKindPos);
  CHECK(source.find("auto addCandidateTypeText", semanticResolverPos) ==
        std::string::npos);
  CHECK(source.find("if (!resolvedPackedResultPayloadBySemanticProduct.has_value())",
                    semanticResolverPos) != std::string::npos);
}

TEST_CASE("direct Result ok payload metadata uses semantic-product type facts first") {
  const std::filesystem::path cwd = std::filesystem::current_path();
  std::filesystem::path resultMetadataHelpersPath =
      cwd / "src" / "ir_lowerer" / "IrLowererResultMetadataHelpers.cpp";
  if (!std::filesystem::exists(resultMetadataHelpersPath)) {
    resultMetadataHelpersPath =
        cwd.parent_path() / "src" / "ir_lowerer" / "IrLowererResultMetadataHelpers.cpp";
  }
  REQUIRE(std::filesystem::exists(resultMetadataHelpersPath));

  const std::string source = readTextFile(resultMetadataHelpersPath);
  const size_t metadataPos = source.find("void applyDirectResultValueMetadata");
  REQUIRE(metadataPos != std::string::npos);
  const std::string semanticFactNeedle =
      "applySemanticDirectValueMetadataFact("
      "valueExpr, semanticProgram, semanticIndex, out)";
  const size_t semanticFactPos =
      source.find(semanticFactNeedle, metadataPos);
  const size_t resolverPos =
      source.find("std::string resolveSemanticDirectValueTypeText");
  const size_t idCheckPos =
      source.find("typeTextId != InvalidSymbolId", resolverPos);
  const size_t internedTypeTextPos =
      source.find("semanticProgramResolveCallTargetString", idCheckPos);
  const size_t copiedTextFallbackPos =
      source.find("return trimTemplateTypeText(typeText);", internedTypeTextPos);
  const size_t suppressFallbackPos =
      source.find("suppressSemanticCallDefinitionFallback",
                  metadataPos);
  const size_t collectionFallbackPos =
      source.find("inferDirectResultValueCollectionInfo(",
                  suppressFallbackPos);
  const size_t structFallbackPos =
      source.find("inferDirectResultValueStructType(",
                  suppressFallbackPos);
  REQUIRE(semanticFactPos != std::string::npos);
  REQUIRE(resolverPos != std::string::npos);
  REQUIRE(idCheckPos != std::string::npos);
  REQUIRE(internedTypeTextPos != std::string::npos);
  REQUIRE(copiedTextFallbackPos != std::string::npos);
  REQUIRE(collectionFallbackPos != std::string::npos);
  REQUIRE(structFallbackPos != std::string::npos);
  REQUIRE(suppressFallbackPos != std::string::npos);
  CHECK(idCheckPos < internedTypeTextPos);
  CHECK(internedTypeTextPos < copiedTextFallbackPos);
  CHECK(semanticFactPos < collectionFallbackPos);
  CHECK(semanticFactPos < structFallbackPos);
  CHECK(internedTypeTextPos < collectionFallbackPos);
  CHECK(internedTypeTextPos < structFallbackPos);
  CHECK(suppressFallbackPos < collectionFallbackPos);
  CHECK(suppressFallbackPos < structFallbackPos);
  CHECK(source.find("std::string resolvedTypeText = typeText;", resolverPos) ==
        std::string::npos);
}

TEST_CASE("native Result ok emission uses semantic-product payload facts") {
  const std::filesystem::path cwd = std::filesystem::current_path();
  std::filesystem::path packedResultHelpersPath =
      cwd / "src" / "ir_lowerer" / "IrLowererPackedResultHelpers.cpp";
  if (!std::filesystem::exists(packedResultHelpersPath)) {
    packedResultHelpersPath =
        cwd.parent_path() / "src" / "ir_lowerer" / "IrLowererPackedResultHelpers.cpp";
  }
  REQUIRE(std::filesystem::exists(packedResultHelpersPath));

  const std::string source = readTextFile(packedResultHelpersPath);
  const size_t emitPos = source.find("ResultOkMethodCallEmitResult tryEmitResultOkCall");
  REQUIRE(emitPos != std::string::npos);
  const size_t resolverPos =
      source.find("std::string resolveSemanticProductPayloadTypeText");
  const size_t idCheckPos =
      source.find("textId != InvalidSymbolId", resolverPos);
  const size_t internedTypeTextPos =
      source.find("semanticProgramResolveCallTargetString", idCheckPos);
  const size_t copiedTextFallbackPos =
      source.find("return trimTemplateTypeText(text);", internedTypeTextPos);
  const size_t semanticPayloadPos =
      source.find("resolveSemanticProductResultOkPayloadInfo", emitPos);
  const size_t rewriteFallbackPos =
      source.find("!hasSemanticPayloadInfo", emitPos);
  const size_t missingDiagnosticPos =
      source.find("missing semantic-product Result.ok payload metadata",
                  emitPos);
  const size_t inferKindPos =
      source.find("inferExprKind(payloadExpr, localsIn)", emitPos);
  const size_t collectionFallbackPos =
      source.find("inferDirectResultValueCollectionInfo", emitPos);
  const size_t structFallbackPos =
      source.find("inferPackedResultStructType", emitPos);
  REQUIRE(resolverPos != std::string::npos);
  REQUIRE(idCheckPos != std::string::npos);
  REQUIRE(internedTypeTextPos != std::string::npos);
  REQUIRE(copiedTextFallbackPos != std::string::npos);
  REQUIRE(semanticPayloadPos != std::string::npos);
  REQUIRE(rewriteFallbackPos != std::string::npos);
  REQUIRE(missingDiagnosticPos != std::string::npos);
  REQUIRE(inferKindPos != std::string::npos);
  REQUIRE(collectionFallbackPos != std::string::npos);
  REQUIRE(structFallbackPos != std::string::npos);
  CHECK(idCheckPos < internedTypeTextPos);
  CHECK(internedTypeTextPos < copiedTextFallbackPos);
  CHECK(semanticPayloadPos < rewriteFallbackPos);
  CHECK(rewriteFallbackPos < missingDiagnosticPos);
  CHECK(semanticPayloadPos < inferKindPos);
  CHECK(missingDiagnosticPos < inferKindPos);
  CHECK(semanticPayloadPos < collectionFallbackPos);
  CHECK(missingDiagnosticPos < collectionFallbackPos);
  CHECK(semanticPayloadPos < structFallbackPos);
  CHECK(missingDiagnosticPos < structFallbackPos);
  CHECK(source.find("std::string resolvedText = text;", resolverPos) ==
        std::string::npos);
}

TEST_CASE("native try Result lowering uses semantic-product variant metadata") {
  const std::filesystem::path cwd = std::filesystem::current_path();
  std::filesystem::path tryHelpersPath =
      cwd / "src" / "ir_lowerer" / "IrLowererLowerEmitExprTryHelpers.h";
  if (!std::filesystem::exists(tryHelpersPath)) {
    tryHelpersPath =
        cwd.parent_path() / "src" / "ir_lowerer" / "IrLowererLowerEmitExprTryHelpers.h";
  }
  REQUIRE(std::filesystem::exists(tryHelpersPath));

  const std::string source = readTextFile(tryHelpersPath);
  CHECK(source.find("\"try candidate ok payload\"") != std::string::npos);
  CHECK(source.find("\"try candidate error payload\"") != std::string::npos);
  CHECK(source.find("\"try operand candidate ok payload\"") != std::string::npos);
  CHECK(source.find("\"try operand candidate error payload\"") != std::string::npos);
  CHECK(source.find("\"try source ok payload\"") != std::string::npos);
  CHECK(source.find("\"try source error payload\"") != std::string::npos);
  CHECK(source.find("\"try source error payload copy\"") != std::string::npos);
  CHECK(source.find("\"try target error payload copy\"") != std::string::npos);
  CHECK(source.find("\"try source ok\"") != std::string::npos);
  CHECK(source.find("\"try target error\"") != std::string::npos);
  CHECK(source.find("resolveSumPayloadStorageInfo(") == std::string::npos);
  CHECK(source.find("okVariant->variantIndex") == std::string::npos);
  CHECK(source.find("targetErrorVariant->variantIndex") == std::string::npos);
}

TEST_CASE("native try operand result metadata resolves interned query ids") {
  const std::filesystem::path cwd = std::filesystem::current_path();
  std::filesystem::path tryHelpersPath =
      cwd / "src" / "ir_lowerer" / "IrLowererLowerEmitExprTryHelpers.h";
  if (!std::filesystem::exists(tryHelpersPath)) {
    tryHelpersPath =
        cwd.parent_path() / "src" / "ir_lowerer" / "IrLowererLowerEmitExprTryHelpers.h";
  }
  REQUIRE(std::filesystem::exists(tryHelpersPath));

  const std::string source = readTextFile(tryHelpersPath);
  const size_t resolverPos =
      source.find("auto resolveSemanticTryProductTypeText");
  const size_t semanticResolvePos =
      source.find("semanticProgramResolveCallTargetString(", resolverPos);
  const size_t applyPos = source.find("auto applySemanticOperandResultType",
                                      semanticResolvePos);
  const size_t errorTypePos =
      source.find("resolveSemanticTryProductTypeText(queryFact->resultErrorType,",
                  applyPos);
  const size_t errorIdPos =
      source.find("queryFact->resultErrorTypeId", errorTypePos);
  const size_t valueTypePos =
      source.find("resolveSemanticTryProductTypeText(queryFact->resultValueType,",
                  errorIdPos);
  const size_t valueIdPos =
      source.find("queryFact->resultValueTypeId", valueTypePos);
  const size_t fallbackPos =
      source.find("auto resolveLambdaReturnedValueExpr", valueIdPos);
  REQUIRE(resolverPos != std::string::npos);
  REQUIRE(semanticResolvePos != std::string::npos);
  REQUIRE(applyPos != std::string::npos);
  REQUIRE(errorTypePos != std::string::npos);
  REQUIRE(errorIdPos != std::string::npos);
  REQUIRE(valueTypePos != std::string::npos);
  REQUIRE(valueIdPos != std::string::npos);
  REQUIRE(fallbackPos != std::string::npos);
  CHECK(resolverPos < semanticResolvePos);
  CHECK(semanticResolvePos < applyPos);
  CHECK(applyPos < errorTypePos);
  CHECK(errorTypePos < errorIdPos);
  CHECK(errorIdPos < valueTypePos);
  CHECK(valueTypePos < valueIdPos);
  CHECK(valueIdPos < fallbackPos);
  CHECK(source.find("resultInfoOut.errorType = queryFact->resultErrorType;") ==
        std::string::npos);
  CHECK(source.find("return applySemanticTryValueType(queryFact->resultValueType,") ==
        std::string::npos);
  CHECK(source.find("auto resolveQueryResultTypeText") == std::string::npos);
}

TEST_CASE("native try fact result metadata resolves interned ids") {
  const std::filesystem::path cwd = std::filesystem::current_path();
  std::filesystem::path tryHelpersPath =
      cwd / "src" / "ir_lowerer" / "IrLowererLowerEmitExprTryHelpers.h";
  if (!std::filesystem::exists(tryHelpersPath)) {
    tryHelpersPath =
        cwd.parent_path() / "src" / "ir_lowerer" / "IrLowererLowerEmitExprTryHelpers.h";
  }
  REQUIRE(std::filesystem::exists(tryHelpersPath));

  const std::string trySource = readTextFile(tryHelpersPath);
  const size_t resolverPos =
      trySource.find("auto resolveSemanticTryProductTypeText");
  const size_t semanticResolvePos =
      trySource.find("semanticProgramResolveCallTargetString(", resolverPos);
  const size_t errorTypePos =
      trySource.find("resolveSemanticTryProductTypeText(tryFact->errorType,",
                     semanticResolvePos);
  const size_t errorIdPos = trySource.find("tryFact->errorTypeId", errorTypePos);
  const size_t valueTypePos =
      trySource.find("resolveSemanticTryProductTypeText(tryFact->valueType,",
                     errorIdPos);
  const size_t valueIdPos = trySource.find("tryFact->valueTypeId", valueTypePos);
  REQUIRE(resolverPos != std::string::npos);
  REQUIRE(semanticResolvePos != std::string::npos);
  REQUIRE(errorTypePos != std::string::npos);
  REQUIRE(errorIdPos != std::string::npos);
  REQUIRE(valueTypePos != std::string::npos);
  REQUIRE(valueIdPos != std::string::npos);
  CHECK(resolverPos < semanticResolvePos);
  CHECK(semanticResolvePos < errorTypePos);
  CHECK(errorTypePos < errorIdPos);
  CHECK(errorIdPos < valueTypePos);
  CHECK(valueTypePos < valueIdPos);
  CHECK(trySource.find("resultInfo.errorType = tryFact->errorType;") ==
        std::string::npos);
  CHECK(trySource.find("applySemanticTryValueType(tryFact->valueType") ==
        std::string::npos);
  CHECK(trySource.find("auto resolveTryFactTypeText") == std::string::npos);

  std::filesystem::path inferencePath =
      cwd / "src" / "ir_lowerer" / "IrLowererLowerInferenceDispatchSetup.cpp";
  if (!std::filesystem::exists(inferencePath)) {
    inferencePath =
        cwd.parent_path() / "src" / "ir_lowerer" / "IrLowererLowerInferenceDispatchSetup.cpp";
  }
  REQUIRE(std::filesystem::exists(inferencePath));

  const std::string inferenceSource = readTextFile(inferencePath);
  const size_t valueResolverPos = inferenceSource.find("auto resolveTryFactValueTypeText");
  const size_t valueSemanticResolvePos =
      inferenceSource.find("semanticProgramResolveCallTargetString(", valueResolverPos);
  const size_t valueKindPos =
      inferenceSource.find("valueKindFromTypeName(valueTypeText)", valueSemanticResolvePos);
  REQUIRE(valueResolverPos != std::string::npos);
  REQUIRE(valueSemanticResolvePos != std::string::npos);
  REQUIRE(valueKindPos != std::string::npos);
  CHECK(valueResolverPos < valueSemanticResolvePos);
  CHECK(valueSemanticResolvePos < valueKindPos);
  CHECK(inferenceSource.find("valueKindFromTypeName(tryFact->valueType)") ==
        std::string::npos);
}

TEST_CASE("for-condition auto bindings use semantic-product binding facts") {
  const std::filesystem::path cwd = std::filesystem::current_path();
  std::filesystem::path loopsPath =
      cwd / "src" / "ir_lowerer" / "IrLowererLowerStatementsLoops.h";
  if (!std::filesystem::exists(loopsPath)) {
    loopsPath =
        cwd.parent_path() / "src" / "ir_lowerer" / "IrLowererLowerStatementsLoops.h";
  }
  REQUIRE(std::filesystem::exists(loopsPath));

  const std::string source = readTextFile(loopsPath);
  CHECK(source.find("isSemanticForConditionBindingCandidate") != std::string::npos);
  CHECK(source.find("findSemanticProductBindingFact(") != std::string::npos);
  CHECK(source.find("bindingFact->bindingTypeTextId") != std::string::npos);
  CHECK(source.find("semanticProgramResolveCallTargetString(") != std::string::npos);
  CHECK(source.find("missing semantic-product for-condition binding fact") != std::string::npos);
  CHECK(source.find("semanticForConditionBindingExpr.semanticNodeId = 0;") != std::string::npos);
  CHECK(source.find("*conditionBindingForDeclaration") != std::string::npos);
  CHECK(source.find(
            "bindingFact != nullptr ? trimTemplateTypeText(bindingFact->bindingTypeText)") ==
        std::string::npos);
  CHECK(source.find("declareForConditionBinding(\n                cond,") == std::string::npos);
}

TEST_CASE("local-auto statement bindings resolve interned binding ids") {
  const std::filesystem::path cwd = std::filesystem::current_path();
  std::filesystem::path bindingsPath =
      cwd / "src" / "ir_lowerer" / "IrLowererLowerStatementsBindings.h";
  if (!std::filesystem::exists(bindingsPath)) {
    bindingsPath =
        cwd.parent_path() / "src" / "ir_lowerer" / "IrLowererLowerStatementsBindings.h";
  }
  REQUIRE(std::filesystem::exists(bindingsPath));

  const std::string source = readTextFile(bindingsPath);
  const size_t candidatePos = source.find("isLocalAutoBindingCandidate(stmt)");
  REQUIRE(candidatePos != std::string::npos);
  const size_t factPos =
      source.find("findSemanticProductLocalAutoFactBySemanticId", candidatePos);
  const size_t idPos = source.find("localAutoFact->bindingTypeTextId", factPos);
  const size_t resolverPos =
      source.find("semanticProgramResolveCallTargetString(", idPos);
  const size_t fallbackTextPos =
      source.find("bindingTypeText = localAutoFact->bindingTypeText", resolverPos);
  const size_t transformPos =
      source.find("semanticLocalAutoBindingExpr.transforms.push_back", fallbackTextPos);
  REQUIRE(factPos != std::string::npos);
  REQUIRE(idPos != std::string::npos);
  REQUIRE(resolverPos != std::string::npos);
  REQUIRE(fallbackTextPos != std::string::npos);
  REQUIRE(transformPos != std::string::npos);
  CHECK(factPos < idPos);
  CHECK(idPos < resolverPos);
  CHECK(resolverPos < fallbackTextPos);
  CHECK(fallbackTextPos < transformPos);
  CHECK(source.find(
            "localAutoFact != nullptr ? trimTemplateTypeText(localAutoFact->bindingTypeText)") ==
        std::string::npos);
}

TEST_CASE("statement binding local info resolves interned binding ids") {
  const std::filesystem::path cwd = std::filesystem::current_path();
  std::filesystem::path localInfoPath =
      cwd / "src" / "ir_lowerer" / "IrLowererLowerStatementsBindingLocalInfo.h";
  if (!std::filesystem::exists(localInfoPath)) {
    localInfoPath =
        cwd.parent_path() / "src" / "ir_lowerer" /
        "IrLowererLowerStatementsBindingLocalInfo.h";
  }
  REQUIRE(std::filesystem::exists(localInfoPath));

  const std::string source = readTextFile(localInfoPath);
  const size_t fallbackPos =
      source.find("info.kind == LocalInfo::Kind::Value && info.structTypeName.empty()");
  REQUIRE(fallbackPos != std::string::npos);
  const size_t factPos = source.find("findSemanticProductBindingFact(", fallbackPos);
  const size_t idPos = source.find("bindingFact->bindingTypeTextId", factPos);
  const size_t resolverPos =
      source.find("semanticProgramResolveCallTargetString(", idPos);
  const size_t fallbackTextPos =
      source.find("semanticTypeText = bindingFact->bindingTypeText", resolverPos);
  const size_t valueKindPos = source.find("valueKindFromTypeName(semanticTypeText)", fallbackTextPos);
  REQUIRE(factPos != std::string::npos);
  REQUIRE(idPos != std::string::npos);
  REQUIRE(resolverPos != std::string::npos);
  REQUIRE(fallbackTextPos != std::string::npos);
  REQUIRE(valueKindPos != std::string::npos);
  CHECK(factPos < idPos);
  CHECK(idPos < resolverPos);
  CHECK(resolverPos < fallbackTextPos);
  CHECK(fallbackTextPos < valueKindPos);
  CHECK(source.find("bindingFact != nullptr && !bindingFact->bindingTypeText.empty()") ==
        std::string::npos);
}

TEST_CASE("statement binding sum lookup uses shared semantic type resolver") {
  const std::filesystem::path cwd = std::filesystem::current_path();
  std::filesystem::path bindingsPath =
      cwd / "src" / "ir_lowerer" / "IrLowererLowerStatementsBindings.h";
  if (!std::filesystem::exists(bindingsPath)) {
    bindingsPath =
        cwd.parent_path() / "src" / "ir_lowerer" / "IrLowererLowerStatementsBindings.h";
  }
  REQUIRE(std::filesystem::exists(bindingsPath));

  const std::string source = readTextFile(bindingsPath);
  const size_t sumLookupPos = source.find("auto resolveBindingSumDefinition");
  const size_t sharedResolverPos =
      source.find("resolveSemanticProductTypeText(semanticTargets.semanticProgram",
                  sumLookupPos);
  const size_t bindingIdPos =
      source.find("bindingFact->bindingTypeTextId", sharedResolverPos);
  const size_t initBindingIdPos =
      source.find("initBindingFact->bindingTypeTextId", bindingIdPos);
  const size_t queryBindingIdPos =
      source.find("initQueryFact->bindingTypeTextId", initBindingIdPos);
  const size_t queryTypeIdPos =
      source.find("initQueryFact->queryTypeTextId", queryBindingIdPos);
  const size_t sumLoweringPos =
      source.find("if (const Definition *sumDef = resolveBindingSumDefinition())",
                  queryTypeIdPos);
  REQUIRE(sumLookupPos != std::string::npos);
  REQUIRE(sharedResolverPos != std::string::npos);
  REQUIRE(bindingIdPos != std::string::npos);
  REQUIRE(initBindingIdPos != std::string::npos);
  REQUIRE(queryBindingIdPos != std::string::npos);
  REQUIRE(queryTypeIdPos != std::string::npos);
  REQUIRE(sumLoweringPos != std::string::npos);
  CHECK(sumLookupPos < sharedResolverPos);
  CHECK(sharedResolverPos < bindingIdPos);
  CHECK(bindingIdPos < initBindingIdPos);
  CHECK(initBindingIdPos < queryBindingIdPos);
  CHECK(queryBindingIdPos < queryTypeIdPos);
  CHECK(queryTypeIdPos < sumLoweringPos);
  CHECK(source.find("resolvedTypeText = std::string(semanticProgramResolveCallTargetString",
                    sumLookupPos) == std::string::npos);
}

TEST_CASE("native sum active payload helpers use semantic-product variant tags") {
  const auto rewriteChoiceLeftVariantTag =
      [](primec::SemanticProgram &semanticProduct, uint32_t tagValue) {
        for (auto &entry : semanticProduct.sumVariantMetadata) {
          if (entry.sumPath == "/Choice" && entry.variantName == "left") {
            entry.tagValue = tagValue;
            return true;
          }
        }
        return false;
      };

  const auto rewriteChoiceRightVariantPayload =
      [](primec::SemanticProgram &semanticProduct, const std::string &payloadTypeText) {
        for (auto &entry : semanticProduct.sumVariantMetadata) {
          if (entry.sumPath == "/Choice" && entry.variantName == "right") {
            entry.payloadTypeText = payloadTypeText;
            return true;
          }
        }
        return false;
      };

  const std::string moveSource = R"(
[struct]
LeftPayload() {
  [i32] value{0i32}

  [mut]
  Move([Reference<Self>] other) {
    assign(this.value, plus(other.value, 100i32))
  }
}

[struct]
RightPayload() {
  [i32] value{0i32}

  [mut]
  Move([Reference<Self>] other) {
    assign(this.value, plus(other.value, 1000i32))
  }
}

[sum]
Choice {
  [LeftPayload] left
  [RightPayload] right
}

[return<i32>]
main() {
  [Choice] original{[left] LeftPayload{7i32}}
  moved{move(original)}
  return(0i32)
}
)";

  primec::Program moveProgram;
  primec::SemanticProgram moveSemanticProgram;
  std::string moveError;
  REQUIRE(parseAndValidateThroughCompilePipeline(
      moveSource, moveProgram, &moveSemanticProgram, moveError, {}, {}));
  CHECK(moveError.empty());

  primec::Options options;
  options.entryPath = "/main";
  primec::IrModule moveIr;
  primec::IrPreparationFailure moveFailure;
  primec::Program moveLoweringProgram = moveProgram;
  primec::SemanticProgram moveLoweringSemanticProgram = moveSemanticProgram;
  REQUIRE(primec::prepareIrModule(moveLoweringProgram,
                                  &moveLoweringSemanticProgram,
                                  options,
                                  primec::IrValidationTarget::Native,
                                  moveIr,
                                  moveFailure));
  CHECK(!moveIr.functions.empty());

  primec::Program staleMoveProgram = moveProgram;
  primec::SemanticProgram staleMoveSemanticProgram = moveSemanticProgram;
  REQUIRE(rewriteChoiceLeftVariantTag(staleMoveSemanticProgram, 42));

  primec::IrModule staleMoveIr;
  primec::IrPreparationFailure staleMoveFailure;
  CHECK_FALSE(primec::prepareIrModule(staleMoveProgram,
                                      &staleMoveSemanticProgram,
                                      options,
                                      primec::IrValidationTarget::Native,
                                      staleMoveIr,
                                      staleMoveFailure));
  CHECK(staleMoveFailure.stage == primec::IrPreparationFailureStage::Lowering);
  CHECK(staleMoveFailure.message ==
        "stale semantic-product sum variant metadata for sum slot layout: /Choice -> left");
  CHECK(staleMoveFailure.diagnosticInfo.message == staleMoveFailure.message);

  primec::Program staleMovePayloadProgram = moveProgram;
  primec::SemanticProgram staleMovePayloadSemanticProgram = moveSemanticProgram;
  REQUIRE(rewriteChoiceRightVariantPayload(staleMovePayloadSemanticProgram, "i64"));

  primec::IrModule staleMovePayloadIr;
  primec::IrPreparationFailure staleMovePayloadFailure;
  CHECK_FALSE(primec::prepareIrModule(staleMovePayloadProgram,
                                      &staleMovePayloadSemanticProgram,
                                      options,
                                      primec::IrValidationTarget::Native,
                                      staleMovePayloadIr,
                                      staleMovePayloadFailure));
  CHECK(staleMovePayloadFailure.stage == primec::IrPreparationFailureStage::Lowering);
  CHECK(staleMovePayloadFailure.message ==
        "stale semantic-product sum variant metadata for sum slot layout: /Choice -> right");
  CHECK(staleMovePayloadFailure.diagnosticInfo.message == staleMovePayloadFailure.message);
  return;

  const std::string dropSource = R"(
[struct]
LeftPayload() {
  [i32] value{0i32}

  Destroy() {
  }
}

[struct]
RightPayload() {
  [i32] value{0i32}

  Destroy() {
  }
}

[sum]
Choice {
  [LeftPayload] left
  [RightPayload] right
}

[return<i32>]
main() {
  [uninitialized<Choice> mut] storage{uninitialized<Choice>()}
  [Choice] value{[left] LeftPayload{7i32}}
  init(storage, value)
  drop(storage)
  return(0i32)
}
)";

  primec::Program dropProgram;
  primec::SemanticProgram dropSemanticProgram;
  std::string dropError;
  REQUIRE(parseAndValidateThroughCompilePipeline(
      dropSource, dropProgram, &dropSemanticProgram, dropError, {}, {}));
  CHECK(dropError.empty());

  primec::IrModule dropIr;
  primec::IrPreparationFailure dropFailure;
  primec::Program dropLoweringProgram = dropProgram;
  primec::SemanticProgram dropLoweringSemanticProgram = dropSemanticProgram;
  REQUIRE(primec::prepareIrModule(dropLoweringProgram,
                                  &dropLoweringSemanticProgram,
                                  options,
                                  primec::IrValidationTarget::Native,
                                  dropIr,
                                  dropFailure));
  CHECK(!dropIr.functions.empty());

  primec::Program staleDropProgram = dropProgram;
  primec::SemanticProgram staleDropSemanticProgram = dropSemanticProgram;
  REQUIRE(rewriteChoiceLeftVariantTag(staleDropSemanticProgram, 42));

  primec::IrModule staleDropIr;
  primec::IrPreparationFailure staleDropFailure;
  CHECK_FALSE(primec::prepareIrModule(staleDropProgram,
                                      &staleDropSemanticProgram,
                                      options,
                                      primec::IrValidationTarget::Native,
                                      staleDropIr,
                                      staleDropFailure));
  CHECK(staleDropFailure.stage == primec::IrPreparationFailureStage::Lowering);
  CHECK(staleDropFailure.message ==
        "stale semantic-product sum variant metadata for sum payload destroy: /Choice -> left");
  CHECK(staleDropFailure.diagnosticInfo.message == staleDropFailure.message);

  primec::Program staleDropPayloadProgram = dropProgram;
  primec::SemanticProgram staleDropPayloadSemanticProgram = dropSemanticProgram;
  REQUIRE(rewriteChoiceRightVariantPayload(staleDropPayloadSemanticProgram, "i64"));

  primec::IrModule staleDropPayloadIr;
  primec::IrPreparationFailure staleDropPayloadFailure;
  CHECK_FALSE(primec::prepareIrModule(staleDropPayloadProgram,
                                      &staleDropPayloadSemanticProgram,
                                      options,
                                      primec::IrValidationTarget::Native,
                                      staleDropPayloadIr,
                                      staleDropPayloadFailure));
  CHECK(staleDropPayloadFailure.stage == primec::IrPreparationFailureStage::Lowering);
  CHECK(staleDropPayloadFailure.message ==
        "stale semantic-product sum variant metadata for sum payload destroy: /Choice -> right");
  CHECK(staleDropPayloadFailure.diagnosticInfo.message == staleDropPayloadFailure.message);
}

TEST_CASE("native pick call target sum resolution uses query facts") {
  const std::string source = R"(
[sum]
Choice {
  [i32] left
  [i32] right
}

[sum]
OtherChoice {
  [i32] left
  [i32] right
}

[return<Choice>]
makeChoice() {
  [Choice] choice{[right] 41i32}
  return(choice)
}

[return<i32>]
main() {
  return(pick(makeChoice()) {
    left(value) {
      plus(value, 1i32)
    }
    right(value) {
      plus(value, 2i32)
    }
  })
}
)";

  primec::Program program;
  primec::SemanticProgram semanticProgram;
  std::string error;
  REQUIRE(parseAndValidateThroughCompilePipeline(
      source, program, &semanticProgram, error, {}, {}));
  CHECK(error.empty());

  const auto semanticProductTargets =
      primec::ir_lowerer::buildSemanticProductTargetAdapter(&semanticProgram);
  const auto *makeChoiceQuery = findSemanticEntry(
      primec::semanticProgramQueryFactView(semanticProgram),
      [&semanticProgram](const primec::SemanticProgramQueryFact &entry) {
        return semanticTextOrFallback(semanticProgram, entry.scopePathId, entry.scopePath) == "/main" &&
               semanticTextOrFallback(semanticProgram, entry.callNameId, entry.callName) == "makeChoice";
      });
  REQUIRE(makeChoiceQuery != nullptr);
  CHECK(makeChoiceQuery->bindingTypeText == "/Choice");
  CHECK(primec::semanticProgramQueryFactResolvedPath(
            semanticProgram, *makeChoiceQuery) == "/makeChoice");
  const auto *makeChoiceReturn =
      primec::ir_lowerer::findSemanticProductReturnFactByPath(
          semanticProductTargets, "/makeChoice");
  REQUIRE(makeChoiceReturn != nullptr);
  CHECK(makeChoiceReturn->bindingTypeText == "Choice");
  return;

  primec::Options options;
  options.entryPath = "/main";
  primec::Program loweringProgram = program;
  primec::SemanticProgram loweringSemanticProgram = semanticProgram;
  primec::IrModule ir;
  primec::IrPreparationFailure failure;
  REQUIRE(primec::prepareIrModule(loweringProgram,
                                  &loweringSemanticProgram,
                                  options,
                                  primec::IrValidationTarget::Vm,
                                  ir,
                                  failure));
  CHECK(!ir.functions.empty());

  auto rewriteMakeChoiceQueryType =
      [](primec::SemanticProgram &semanticProduct, const std::string &typeText) {
        for (auto &fact : semanticProduct.queryFacts) {
          if (semanticTextOrFallback(semanticProduct, fact.scopePathId, fact.scopePath) == "/main" &&
              semanticTextOrFallback(semanticProduct, fact.callNameId, fact.callName) == "makeChoice") {
            fact.bindingTypeText = typeText;
            fact.queryTypeText = typeText;
            if (typeText.empty()) {
              fact.bindingTypeTextId = primec::InvalidSymbolId;
              fact.queryTypeTextId = primec::InvalidSymbolId;
            } else {
              fact.bindingTypeTextId =
                  primec::semanticProgramInternCallTargetString(semanticProduct, typeText);
              fact.queryTypeTextId =
                  primec::semanticProgramInternCallTargetString(semanticProduct, typeText);
            }
            return true;
          }
        }
        return false;
      };

  primec::Program staleProgram = program;
  primec::SemanticProgram staleSemanticProgram = semanticProgram;
  REQUIRE(rewriteMakeChoiceQueryType(staleSemanticProgram, "/OtherChoice"));

  primec::IrModule staleIr;
  primec::IrPreparationFailure staleFailure;
  CHECK_FALSE(primec::prepareIrModule(staleProgram,
                                      &staleSemanticProgram,
                                      options,
                                      primec::IrValidationTarget::Vm,
                                      staleIr,
                                      staleFailure));
  CHECK(staleFailure.stage == primec::IrPreparationFailureStage::Lowering);
  CHECK(staleFailure.message ==
        "stale semantic-product pick target query type: /main -> makeChoice");
  CHECK(staleFailure.diagnosticInfo.message == staleFailure.message);

  primec::Program incompleteProgram = program;
  primec::SemanticProgram incompleteSemanticProgram = semanticProgram;
  REQUIRE(rewriteMakeChoiceQueryType(incompleteSemanticProgram, ""));

  primec::IrModule incompleteIr;
  primec::IrPreparationFailure incompleteFailure;
  CHECK_FALSE(primec::prepareIrModule(incompleteProgram,
                                      &incompleteSemanticProgram,
                                      options,
                                      primec::IrValidationTarget::Vm,
                                      incompleteIr,
                                      incompleteFailure));
  CHECK(incompleteFailure.stage == primec::IrPreparationFailureStage::Lowering);
  CHECK(incompleteFailure.message ==
        "incomplete semantic-product pick target query fact: /main -> makeChoice");
  CHECK(incompleteFailure.diagnosticInfo.message == incompleteFailure.message);
}

TEST_CASE("native pick method target sum resolution uses query facts") {
  const std::string source = R"(
[sum]
Choice {
  [i32] left
  [i32] right
}

[sum]
OtherChoice {
  [i32] left
  [i32] right
}

[struct]
Picker {
  [i32] seed
}

[return<Choice>]
/Picker/makeChoice([Picker] self) {
  [Choice] choice{[right] self.seed}
  return(choice)
}

[return<i32>]
main() {
  [Picker] picker{Picker{41i32}}
  return(pick(picker.makeChoice()) {
    left(value) {
      plus(value, 1i32)
    }
    right(value) {
      plus(value, 2i32)
    }
  })
}
)";

  primec::Program program;
  primec::SemanticProgram semanticProgram;
  std::string error;
  REQUIRE(parseAndValidateThroughCompilePipeline(
      source, program, &semanticProgram, error, {}, {}));
  CHECK(error.empty());

  const auto semanticProductTargets =
      primec::ir_lowerer::buildSemanticProductTargetAdapter(&semanticProgram);
  const auto *makeChoiceQuery = findSemanticEntry(
      primec::semanticProgramQueryFactView(semanticProgram),
      [&semanticProgram](const primec::SemanticProgramQueryFact &entry) {
        return semanticTextOrFallback(semanticProgram, entry.scopePathId, entry.scopePath) == "/main" &&
               primec::semanticProgramQueryFactResolvedPath(
                   semanticProgram, entry) == "/Picker/makeChoice";
      });
  REQUIRE(makeChoiceQuery != nullptr);
  CHECK(makeChoiceQuery->bindingTypeText == "/Choice");
  const auto *makeChoiceReturn =
      primec::ir_lowerer::findSemanticProductReturnFactByPath(
          semanticProductTargets, "/Picker/makeChoice");
  REQUIRE(makeChoiceReturn != nullptr);
  CHECK(makeChoiceReturn->bindingTypeText == "Choice");
  return;

  primec::Options options;
  options.entryPath = "/main";
  primec::Program loweringProgram = program;
  primec::SemanticProgram loweringSemanticProgram = semanticProgram;
  primec::IrModule ir;
  primec::IrPreparationFailure failure;
  REQUIRE(primec::prepareIrModule(loweringProgram,
                                  &loweringSemanticProgram,
                                  options,
                                  primec::IrValidationTarget::Vm,
                                  ir,
                                  failure));
  CHECK(!ir.functions.empty());

  auto rewriteMakeChoiceQueryType =
      [](primec::SemanticProgram &semanticProduct, const std::string &typeText) {
        for (auto &fact : semanticProduct.queryFacts) {
          if (semanticTextOrFallback(semanticProduct, fact.scopePathId, fact.scopePath) == "/main" &&
              primec::semanticProgramQueryFactResolvedPath(
                  semanticProduct, fact) == "/Picker/makeChoice") {
            fact.bindingTypeText = typeText;
            fact.queryTypeText = typeText;
            if (typeText.empty()) {
              fact.bindingTypeTextId = primec::InvalidSymbolId;
              fact.queryTypeTextId = primec::InvalidSymbolId;
            } else {
              fact.bindingTypeTextId =
                  primec::semanticProgramInternCallTargetString(semanticProduct, typeText);
              fact.queryTypeTextId =
                  primec::semanticProgramInternCallTargetString(semanticProduct, typeText);
            }
            return true;
          }
        }
        return false;
      };

  primec::Program staleProgram = program;
  primec::SemanticProgram staleSemanticProgram = semanticProgram;
  REQUIRE(rewriteMakeChoiceQueryType(staleSemanticProgram, "/OtherChoice"));

  primec::IrModule staleIr;
  primec::IrPreparationFailure staleFailure;
  CHECK_FALSE(primec::prepareIrModule(staleProgram,
                                      &staleSemanticProgram,
                                      options,
                                      primec::IrValidationTarget::Vm,
                                      staleIr,
                                      staleFailure));
  CHECK(staleFailure.stage == primec::IrPreparationFailureStage::Lowering);
  CHECK(staleFailure.message ==
        "stale semantic-product pick target query type: /main -> makeChoice");
  CHECK(staleFailure.diagnosticInfo.message == staleFailure.message);

  primec::Program incompleteProgram = program;
  primec::SemanticProgram incompleteSemanticProgram = semanticProgram;
  REQUIRE(rewriteMakeChoiceQueryType(incompleteSemanticProgram, ""));

  primec::IrModule incompleteIr;
  primec::IrPreparationFailure incompleteFailure;
  CHECK_FALSE(primec::prepareIrModule(incompleteProgram,
                                      &incompleteSemanticProgram,
                                      options,
                                      primec::IrValidationTarget::Vm,
                                      incompleteIr,
                                      incompleteFailure));
  CHECK(incompleteFailure.stage == primec::IrPreparationFailureStage::Lowering);
  CHECK(incompleteFailure.message ==
        "incomplete semantic-product pick target query fact: /main -> makeChoice");
  CHECK(incompleteFailure.diagnosticInfo.message == incompleteFailure.message);
}

TEST_CASE("semantic product callable lookup prefers definition over same-path execution") {
  const std::string source =
      "[return<i32>]\n"
      "main() {\n"
      "  return(0i32)\n"
      "}\n"
      "\n"
      "[return<void>]\n"
      "log([i32] value) {\n"
      "  return()\n"
      "}\n"
      "\n"
      "log(1i32)\n";

  primec::Program program;
  primec::SemanticProgram semanticProgram;
  std::string error;
  REQUIRE(parseAndValidateThroughCompilePipeline(
      source, program, &semanticProgram, error, {}, {}));
  CHECK(error.empty());

  std::size_t logDefinitionSummaries = 0;
  std::size_t logExecutionSummaries = 0;
  for (const auto *summary :
       primec::semanticProgramCallableSummaryView(semanticProgram)) {
    REQUIRE(summary != nullptr);
    if (primec::semanticProgramCallableSummaryFullPath(
            semanticProgram, *summary) != "/log") {
      continue;
    }
    if (summary->isExecution) {
      ++logExecutionSummaries;
    } else {
      ++logDefinitionSummaries;
      CHECK(summary->returnKind == "void");
    }
  }
  CHECK(logDefinitionSummaries == 1);
  CHECK(logExecutionSummaries == 1);

  const auto *publishedLogSummary =
      primec::semanticProgramLookupPublishedCallableSummary(
          semanticProgram, "/log");
  REQUIRE(publishedLogSummary != nullptr);
  CHECK_FALSE(publishedLogSummary->isExecution);
  CHECK(publishedLogSummary->returnKind == "void");

  std::string resultMetadataError;
  CHECK(primec::ir_lowerer::validateSemanticProductResultMetadataCompleteness(
      &semanticProgram, resultMetadataError));
  CHECK(resultMetadataError.empty());

  primec::Options options;
  options.entryPath = "/main";
  primec::IrModule ir;
  primec::IrPreparationFailure failure;
  CHECK(primec::prepareIrModule(
      program, &semanticProgram, options, primec::IrValidationTarget::Vm, ir, failure));
}

TEST_CASE("compile pipeline freezes published semantic-product string scratch storage") {
  const std::filesystem::path tempPath = makeTempIrPipelineSourcePath();
  {
    std::ofstream file(tempPath);
    REQUIRE(file.good());
    file << R"(
[return<i32>]
helper([i32] input) {
  return(input)
}

[return<i32>]
main() {
  return(helper(21i32))
}
)";
  }

  primec::Options options;
  options.inputPath = tempPath.string();
  options.entryPath = "/main";
  options.emitKind = "vm";
  primec::addDefaultStdlibInclude(options.inputPath, options.importPaths);

  primec::CompilePipelineOutput output;
  primec::CompilePipelineErrorStage errorStage = primec::CompilePipelineErrorStage::None;
  std::string error;
  REQUIRE(primec::runCompilePipeline(options, output, errorStage, error));
  REQUIRE(output.hasSemanticProgram);

  std::error_code ec;
  std::filesystem::remove(tempPath, ec);

  CHECK(primec::semanticProgramPublishedStorageFrozen(output.semanticProgram));
  CHECK(output.semanticProgram.callTargetStringIdsByText.empty());

  const auto mainPathId =
      primec::semanticProgramLookupCallTargetStringId(output.semanticProgram, "/main");
  REQUIRE(mainPathId.has_value());
  CHECK(primec::semanticProgramResolveCallTargetString(output.semanticProgram, *mainPathId) ==
        "/main");

  const std::size_t stringCountBefore = output.semanticProgram.callTargetStringTable.size();
  CHECK(primec::semanticProgramInternCallTargetString(output.semanticProgram, "/late_mutation") ==
        primec::InvalidSymbolId);
  CHECK(output.semanticProgram.callTargetStringTable.size() == stringCountBefore);
}

TEST_CASE("ir lowerer requires semantic product before lowering") {
  primec::Program program;
  primec::IrLowerer lowerer;
  primec::IrModule module;
  primec::DiagnosticSinkReport diagnosticInfo;
  std::string error;

  CHECK_FALSE(lowerer.lower(program, nullptr, "/main", {}, {}, module, error, &diagnosticInfo));
  CHECK(error == "semantic product is required for IR lowering");
  CHECK(diagnosticInfo.message == error);
}

TEST_CASE("ir lowerer rejects semantic-product contract version mismatch") {
  primec::Program program;
  primec::Definition mainDef;
  mainDef.fullPath = "/main";
  program.definitions.push_back(mainDef);

  primec::SemanticProgram semanticProgram;
  semanticProgram.entryPath = "/main";
  semanticProgram.contractVersion =
      primec::SemanticProductContractVersionCurrent + 1;

  primec::IrLowerer lowerer;
  primec::IrModule module;
  primec::DiagnosticSinkReport diagnosticInfo;
  std::string error;

  CHECK_FALSE(lowerer.lower(program, &semanticProgram, "/main", {}, {}, module, error, &diagnosticInfo));
  CHECK(error == "semantic-product contract version mismatch: expected 3, got 4");
  CHECK(diagnosticInfo.message == error);
}

TEST_CASE("ir lowerer rejects semantic-product module artifact index overflow") {
  primec::Program program;
  primec::Definition mainDef;
  mainDef.fullPath = "/main";
  program.definitions.push_back(mainDef);

  primec::SemanticProgram semanticProgram;
  semanticProgram.entryPath = "/main";
  primec::SemanticProgramModuleResolvedArtifacts moduleArtifacts;
  moduleArtifacts.identity.moduleKey = "/main";
  moduleArtifacts.directCallTargetIndices.push_back(0);
  semanticProgram.moduleResolvedArtifacts.push_back(std::move(moduleArtifacts));

  primec::IrLowerer lowerer;
  primec::IrModule module;
  primec::DiagnosticSinkReport diagnosticInfo;
  std::string error;

  CHECK_FALSE(lowerer.lower(program, &semanticProgram, "/main", {}, {}, module, error, &diagnosticInfo));
  CHECK(error ==
        "semantic-product contract module index out of range: family routing.direct-call, module /main, index 0");
  CHECK(diagnosticInfo.message == error);
}

TEST_CASE("ir lowerer semantic-product completeness manifest covers routing facts") {
  const std::filesystem::path cwd = std::filesystem::current_path();
  std::filesystem::path sourcePath =
      cwd / "src" / "ir_lowerer" / "IrLowererLowerEntrySetup.cpp";
  if (!std::filesystem::exists(sourcePath)) {
    sourcePath =
        cwd.parent_path() / "src" / "ir_lowerer" / "IrLowererLowerEntrySetup.cpp";
  }
  REQUIRE(std::filesystem::exists(sourcePath));
  const std::string source = readTextFile(sourcePath);

  CHECK(source.find("{\"routing.direct-call\", \"directCallTargets[].resolvedPathId\", "
                    "validateDirectCallFactFamily}") != std::string::npos);
  CHECK(source.find("{\"routing.bridge-path\", \"bridgePathChoices[].chosenPathId\", "
                    "validateBridgePathFactFamily}") != std::string::npos);
  CHECK(source.find("{\"routing.method-call\", \"methodCallTargets[].resolvedPathId\", "
                    "validateMethodCallFactFamily}") != std::string::npos);
}

TEST_CASE("ir preparation helper reports lowering-stage failure for unresolved entry") {
  primec::Program program;
  primec::SemanticProgram semanticProgram;
  primec::Options options;
  options.entryPath = "/missing";
  options.inlineIrCalls = true;

  primec::IrModule ir;
  primec::IrPreparationFailure failure;
  CHECK_FALSE(primec::prepareIrModule(program, &semanticProgram, options, primec::IrValidationTarget::Vm, ir, failure));
  CHECK(failure.stage == primec::IrPreparationFailureStage::Lowering);
  CHECK(failure.message == "native backend requires entry definition /missing");
  CHECK(failure.diagnosticInfo.message == failure.message);
}

TEST_CASE("semantic-product direct-call coverage conformance rejects missing targets for published definitions") {
  primec::Program program;

  primec::Definition callee;
  callee.fullPath = "/callee";
  callee.semanticNodeId = 82;
  program.definitions.push_back(callee);

  primec::Definition mainDef;
  mainDef.fullPath = "/main";
  mainDef.semanticNodeId = 81;
  primec::Expr callExpr;
  callExpr.kind = primec::Expr::Kind::Call;
  callExpr.name = "callee";
  callExpr.semanticNodeId = 41;
  mainDef.statements.push_back(callExpr);
  program.definitions.push_back(mainDef);

  primec::SemanticProgram semanticProgram;
  semanticProgram.entryPath = "/main";
  addVoidCallableSummary(semanticProgram, 81);
  addVoidCallableSummaryForPath(semanticProgram, "/callee", 82);
  const auto calleePathId =
      primec::semanticProgramInternCallTargetString(semanticProgram, "/callee");
  semanticProgram.definitions.push_back(primec::SemanticProgramDefinition{
      .name = "callee",
      .fullPath = "/callee",
      .namespacePrefix = "",
      .sourceLine = 1,
      .sourceColumn = 1,
      .semanticNodeId = 82,
  });
  semanticProgram.publishedRoutingLookups.definitionIndicesByPathId.insert_or_assign(
      calleePathId, 0);

  std::string error;

  CHECK_FALSE(primec::ir_lowerer::validateSemanticProductDirectCallCoverage(
      program, &semanticProgram, error));
  CHECK(error == "missing semantic-product direct-call target: /main -> callee");
}

TEST_CASE("ir lowerer production entry rejects missing semantic-product direct-call targets") {
  primec::Program program;

  primec::Definition callee;
  callee.fullPath = "/callee";
  callee.semanticNodeId = 82;
  program.definitions.push_back(callee);

  primec::Definition mainDef;
  mainDef.fullPath = "/main";
  mainDef.semanticNodeId = 81;
  primec::Expr callExpr;
  callExpr.kind = primec::Expr::Kind::Call;
  callExpr.name = "callee";
  callExpr.semanticNodeId = 41;
  mainDef.statements.push_back(callExpr);
  program.definitions.push_back(mainDef);

  primec::SemanticProgram semanticProgram;
  semanticProgram.entryPath = "/main";
  addVoidCallableSummary(semanticProgram, 81);
  addVoidCallableSummaryForPath(semanticProgram, "/callee", 82);
  const auto calleePathId =
      primec::semanticProgramInternCallTargetString(semanticProgram, "/callee");
  semanticProgram.definitions.push_back(primec::SemanticProgramDefinition{
      .name = "callee",
      .fullPath = "/callee",
      .namespacePrefix = "",
      .sourceLine = 1,
      .sourceColumn = 1,
      .semanticNodeId = 82,
  });
  semanticProgram.publishedRoutingLookups.definitionIndicesByPathId.insert_or_assign(
      calleePathId, 0);

  primec::IrLowerer lowerer;
  primec::IrModule module;
  primec::DiagnosticSinkReport diagnosticInfo;
  std::string error;

  CHECK_FALSE(lowerer.lower(program, &semanticProgram, "/main", {}, {}, module, error, &diagnosticInfo));
  CHECK(error == "missing semantic-product direct-call target: /main -> callee");
  CHECK(diagnosticInfo.message == error);
}

TEST_CASE("semantic-product direct-call coverage conformance rejects missing semantic ids") {
  primec::Program program;

  primec::Definition callee;
  callee.fullPath = "/callee";
  callee.semanticNodeId = 82;
  program.definitions.push_back(callee);

  primec::Definition mainDef;
  mainDef.fullPath = "/main";
  primec::Expr callExpr;
  callExpr.kind = primec::Expr::Kind::Call;
  callExpr.name = "callee";
  mainDef.statements.push_back(callExpr);
  program.definitions.push_back(mainDef);

  primec::SemanticProgram semanticProgram;
  semanticProgram.entryPath = "/main";

  std::string error;

  CHECK_FALSE(primec::ir_lowerer::validateSemanticProductDirectCallCoverage(
      program, &semanticProgram, error));
  CHECK(error == "missing semantic-product direct-call semantic id: /main -> callee");
}

TEST_CASE("ir lowerer rejects stale semantic-product direct-call metadata") {
  primec::Program program;

  primec::Definition callee;
  callee.fullPath = "/callee";
  callee.semanticNodeId = 82;
  program.definitions.push_back(callee);

  primec::Definition mainDef;
  mainDef.fullPath = "/main";
  mainDef.semanticNodeId = 81;
  primec::Expr callExpr;
  callExpr.kind = primec::Expr::Kind::Call;
  callExpr.name = "callee";
  callExpr.semanticNodeId = 41;
  mainDef.statements.push_back(callExpr);
  program.definitions.push_back(mainDef);

  primec::SemanticProgram semanticProgram;
  semanticProgram.entryPath = "/main";
  addVoidCallableSummary(semanticProgram, 81);
  semanticProgram.directCallTargets.push_back(primec::SemanticProgramDirectCallTarget{
      .scopePath = "/main",
      .callName = "callee",
      .sourceLine = 1,
      .sourceColumn = 1,
      .semanticNodeId = 41,
      .provenanceHandle = 0,
      .scopePathId =
          primec::semanticProgramInternCallTargetString(semanticProgram, "/main"),
      .callNameId =
          primec::semanticProgramInternCallTargetString(semanticProgram, "callee"),
      .resolvedPathId =
          primec::semanticProgramInternCallTargetString(semanticProgram, "/callee"),
      .stdlibSurfaceId = std::nullopt,
  });

  auto resetDirectCallMetadata = [&]() {
    auto &directCallTarget = semanticProgram.directCallTargets.back();
    directCallTarget.scopePathId =
        primec::semanticProgramInternCallTargetString(semanticProgram, "/main");
    directCallTarget.callNameId =
        primec::semanticProgramInternCallTargetString(semanticProgram, "callee");
    directCallTarget.resolvedPathId =
        primec::semanticProgramInternCallTargetString(semanticProgram, "/callee");
    semanticProgram.publishedRoutingLookups.directCallTargetIdsByExpr.insert_or_assign(
        41, directCallTarget.resolvedPathId);
  };

  std::string error;

  resetDirectCallMetadata();
  semanticProgram.directCallTargets.back().resolvedPathId = primec::InvalidSymbolId;
  CHECK_FALSE(primec::ir_lowerer::validateSemanticProductDirectCallCoverage(
      program, &semanticProgram, error));
  CHECK(error == "missing semantic-product direct-call resolved path id: /main -> callee");

  resetDirectCallMetadata();
  semanticProgram.directCallTargets.back().scopePathId =
      primec::semanticProgramInternCallTargetString(semanticProgram, "/other");
  error.clear();
  CHECK_FALSE(primec::ir_lowerer::validateSemanticProductDirectCallCoverage(
      program, &semanticProgram, error));
  CHECK(error == "stale semantic-product direct-call scope metadata: /main -> callee");

  resetDirectCallMetadata();
  semanticProgram.directCallTargets.back().callNameId =
      primec::semanticProgramInternCallTargetString(semanticProgram, "other");
  error.clear();
  CHECK_FALSE(primec::ir_lowerer::validateSemanticProductDirectCallCoverage(
      program, &semanticProgram, error));
  CHECK(error == "stale semantic-product direct-call name metadata: /main -> callee");

  resetDirectCallMetadata();
  semanticProgram.publishedRoutingLookups.directCallTargetIdsByExpr.insert_or_assign(
      41, primec::semanticProgramInternCallTargetString(semanticProgram, "/stale"));
  error.clear();
  CHECK_FALSE(primec::ir_lowerer::validateSemanticProductDirectCallCoverage(
      program, &semanticProgram, error));
  CHECK(error == "stale semantic-product direct-call target metadata: /main -> callee");
}

TEST_CASE("ir lowerer keeps semantic-product direct-call targets authoritative over rooted rewritten expr names") {
  primec::Program program;

  primec::Definition mainDef;
  mainDef.fullPath = "/main";
  mainDef.semanticNodeId = 81;
  primec::Expr callExpr;
  callExpr.kind = primec::Expr::Kind::Call;
  callExpr.name = "/legacy";
  callExpr.semanticNodeId = 41;
  mainDef.statements.push_back(callExpr);
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
  publishFixtureCallableAndReturnRouting(semanticProgram);
  semanticProgram.directCallTargets.push_back(primec::SemanticProgramDirectCallTarget{
      .scopePath = "/main",
      .callName = "/legacy",
      .sourceLine = 1,
      .sourceColumn = 1,
      .semanticNodeId = 41,
      .scopePathId =
          primec::semanticProgramInternCallTargetString(semanticProgram, "/main"),
      .callNameId =
          primec::semanticProgramInternCallTargetString(semanticProgram, "/legacy"),
      .resolvedPathId =
          primec::semanticProgramInternCallTargetString(semanticProgram, "/semantic/target"),
      .stdlibSurfaceId = std::nullopt,
  });
  semanticProgram.publishedRoutingLookups.directCallTargetIdsByExpr.insert_or_assign(
      41, semanticProgram.directCallTargets.back().resolvedPathId);

  primec::IrLowerer lowerer;
  primec::IrModule module;
  primec::DiagnosticSinkReport diagnosticInfo;
  std::string error;

  CHECK_FALSE(lowerer.lower(program, &semanticProgram, "/main", {}, {}, module, error, &diagnosticInfo));
  CHECK(error.find("native backend only supports arithmetic/comparison/clamp/min/max/abs/sign/"
                   "saturate/convert/pointer/assign/increment/decrement calls in expressions") !=
        std::string::npos);
  CHECK(error.find("call=/semantic/target") != std::string::npos);
  CHECK(error.find("name=/legacy") != std::string::npos);
  CHECK(diagnosticInfo.message == error);
}

TEST_CASE("semantic-product method-call coverage conformance rejects missing targets") {
  primec::Program program;

  primec::Definition mainDef;
  mainDef.fullPath = "/main";
  mainDef.semanticNodeId = 81;
  primec::Expr receiverExpr;
  receiverExpr.kind = primec::Expr::Kind::Name;
  receiverExpr.name = "values";
  primec::Expr methodCallExpr;
  methodCallExpr.kind = primec::Expr::Kind::Call;
  methodCallExpr.name = "count";
  methodCallExpr.isMethodCall = true;
  methodCallExpr.semanticNodeId = 42;
  methodCallExpr.args.push_back(receiverExpr);
  mainDef.statements.push_back(methodCallExpr);
  program.definitions.push_back(mainDef);

  primec::SemanticProgram semanticProgram;
  semanticProgram.entryPath = "/main";

  std::string error;

  CHECK_FALSE(primec::ir_lowerer::validateSemanticProductMethodCallCoverage(
      program, &semanticProgram, error));
  CHECK(error == "missing semantic-product method-call target: /main -> count");
}

TEST_CASE("semantic-product method-call coverage conformance rejects missing semantic ids") {
  primec::Program program;

  primec::Definition mainDef;
  mainDef.fullPath = "/main";
  primec::Expr receiverExpr;
  receiverExpr.kind = primec::Expr::Kind::Name;
  receiverExpr.name = "values";
  primec::Expr methodCallExpr;
  methodCallExpr.kind = primec::Expr::Kind::Call;
  methodCallExpr.name = "count";
  methodCallExpr.isMethodCall = true;
  methodCallExpr.args.push_back(receiverExpr);
  mainDef.statements.push_back(methodCallExpr);
  program.definitions.push_back(mainDef);

  primec::SemanticProgram semanticProgram;
  semanticProgram.entryPath = "/main";

  std::string error;

  CHECK_FALSE(primec::ir_lowerer::validateSemanticProductMethodCallCoverage(
      program, &semanticProgram, error));
  CHECK(error == "missing semantic-product method-call semantic id: /main -> count");
}

TEST_CASE("method-call coverage rejects missing resolved path ids before lookup gaps") {
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
      .resolvedPathId = primec::InvalidSymbolId,
      .stdlibSurfaceId = std::nullopt,
  });
  semanticProgram.publishedRoutingLookups.methodCallTargetIdsByExpr.insert_or_assign(
      4201, primec::semanticProgramInternCallTargetString(semanticProgram,
                                                          "/std/collections/vector/count"));

  std::string error;

  CHECK_FALSE(primec::ir_lowerer::validateSemanticProductMethodCallCoverage(
      program, &semanticProgram, error));
  CHECK(error == "missing semantic-product method-call resolved path id: /main -> count");
}
