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

TEST_CASE("backend conformance keeps semantic-product-owned facts aligned across backends") {
  const std::string source = R"(
import /std/collections/*

[return<void>]
unexpectedError([i32] err) {
}

[return<T>]
id<T>([T] value) {
  return(value)
}

[return<Result<int, i32>>]
lookup() {
  return(Result.ok(4i32))
}

[return<i32> effects(heap_alloc) on_error<i32, /unexpectedError>]
main() {
  [auto] direct{id(1i32)}
  [auto] values{vector<i32>(1i32)}
  [i32] method{values.count()}
  [i32] bridge{count(values)}
  [auto] selected{try(lookup())}
  return(direct + method + bridge + selected)
}
)";

  auto runConformance = [&](std::string_view emitKind) {
    primec::testing::CompilePipelineBackendConformance conformance;
    std::string error;
    REQUIRE(primec::testing::runCompilePipelineBackendConformanceForTesting(
        source, "/main", emitKind, conformance, error));
    CHECK(error.empty());
    CHECK(conformance.output.hasSemanticProgram);
    return conformance;
  };

  const auto cppConformance = runConformance("cpp-ir");
  const auto vmConformance = runConformance("vm");
#if defined(__APPLE__) && (defined(__arm64__) || defined(__aarch64__))
  const auto nativeConformance = runConformance("native");
#else
  const auto &nativeConformance = cppConformance;
#endif

  const auto *cppDirect = cppConformance.findDirectCallTarget("/main", "id");
  const auto *vmDirect = vmConformance.findDirectCallTarget("/main", "id");
  const auto *nativeDirect = nativeConformance.findDirectCallTarget("/main", "id");
  REQUIRE(cppDirect != nullptr);
  REQUIRE(vmDirect != nullptr);
  REQUIRE(nativeDirect != nullptr);
  const std::string_view cppDirectPath = cppConformance.resolvedDirectCallPath(*cppDirect);
  const std::string_view vmDirectPath = vmConformance.resolvedDirectCallPath(*vmDirect);
  const std::string_view nativeDirectPath = nativeConformance.resolvedDirectCallPath(*nativeDirect);
  CHECK(cppDirectPath.rfind("/id", 0) == 0);
  CHECK(vmDirectPath == cppDirectPath);
  CHECK(nativeDirectPath == cppDirectPath);

  const auto *cppMethod = cppConformance.findMethodCallTarget("/main", "count");
  const auto *vmMethod = vmConformance.findMethodCallTarget("/main", "count");
  const auto *nativeMethod = nativeConformance.findMethodCallTarget("/main", "count");
  REQUIRE(cppMethod != nullptr);
  REQUIRE(vmMethod != nullptr);
  REQUIRE(nativeMethod != nullptr);
  const std::string_view cppMethodPath = cppConformance.resolvedMethodCallPath(*cppMethod);
  const std::string_view vmMethodPath = vmConformance.resolvedMethodCallPath(*vmMethod);
  const std::string_view nativeMethodPath = nativeConformance.resolvedMethodCallPath(*nativeMethod);
  CHECK(cppMethodPath == "/std/collections/vector/count");
  CHECK(vmMethodPath == cppMethodPath);
  CHECK(nativeMethodPath == cppMethodPath);

  const auto *cppBridge = findSemanticEntry(primec::semanticProgramBridgePathChoiceView(cppConformance.output.semanticProgram),
      [&cppConformance](const primec::SemanticProgramBridgePathChoice &entry) {
        return entry.scopePath == "/main" &&
               primec::semanticProgramBridgePathChoiceHelperName(
                   cppConformance.output.semanticProgram, entry) == "count";
      });
  const auto *vmBridge = findSemanticEntry(primec::semanticProgramBridgePathChoiceView(vmConformance.output.semanticProgram),
      [&vmConformance](const primec::SemanticProgramBridgePathChoice &entry) {
        return entry.scopePath == "/main" &&
               primec::semanticProgramBridgePathChoiceHelperName(
                   vmConformance.output.semanticProgram, entry) == "count";
      });
  const auto *nativeBridge = findSemanticEntry(primec::semanticProgramBridgePathChoiceView(nativeConformance.output.semanticProgram),
      [&nativeConformance](const primec::SemanticProgramBridgePathChoice &entry) {
        return entry.scopePath == "/main" &&
               primec::semanticProgramBridgePathChoiceHelperName(
                   nativeConformance.output.semanticProgram, entry) == "count";
      });
  CHECK((cppBridge != nullptr) == (vmBridge != nullptr));
  CHECK((cppBridge != nullptr) == (nativeBridge != nullptr));
  if (cppBridge != nullptr) {
    const std::string_view cppBridgePath = primec::semanticProgramResolveCallTargetString(
        cppConformance.output.semanticProgram, cppBridge->chosenPathId);
    const std::string_view vmBridgePath = primec::semanticProgramResolveCallTargetString(
        vmConformance.output.semanticProgram, vmBridge->chosenPathId);
    const std::string_view nativeBridgePath = primec::semanticProgramResolveCallTargetString(
        nativeConformance.output.semanticProgram, nativeBridge->chosenPathId);
    CHECK(cppBridgePath.rfind("/std/collections/vector/count", 0) == 0);
    CHECK(vmBridgePath == cppBridgePath);
    CHECK(nativeBridgePath == cppBridgePath);
  }

  const auto *cppLocalAuto = findSemanticEntry(primec::semanticProgramLocalAutoFactView(cppConformance.output.semanticProgram),
      [](const primec::SemanticProgramLocalAutoFact &entry) {
        return entry.scopePath == "/main" && entry.bindingName == "selected";
      });
  const auto *vmLocalAuto = findSemanticEntry(primec::semanticProgramLocalAutoFactView(vmConformance.output.semanticProgram),
      [](const primec::SemanticProgramLocalAutoFact &entry) {
        return entry.scopePath == "/main" && entry.bindingName == "selected";
      });
  const auto *nativeLocalAuto = findSemanticEntry(primec::semanticProgramLocalAutoFactView(nativeConformance.output.semanticProgram),
      [](const primec::SemanticProgramLocalAutoFact &entry) {
        return entry.scopePath == "/main" && entry.bindingName == "selected";
      });
  REQUIRE(cppLocalAuto != nullptr);
  REQUIRE(vmLocalAuto != nullptr);
  REQUIRE(nativeLocalAuto != nullptr);
  CHECK(primec::semanticProgramLocalAutoFactInitializerResolvedPath(
            cppConformance.output.semanticProgram, *cppLocalAuto) == "/lookup");
  CHECK(cppLocalAuto->initializerHasTry);
  CHECK(cppLocalAuto->initializerTryValueType == "int");
  CHECK(vmLocalAuto->bindingTypeText == cppLocalAuto->bindingTypeText);
  CHECK(nativeLocalAuto->bindingTypeText == cppLocalAuto->bindingTypeText);

  const auto *cppVector =
      cppConformance.findCollectionSpecialization("/main", "values");
  const auto *vmVector =
      vmConformance.findCollectionSpecialization("/main", "values");
  const auto *nativeVector =
      nativeConformance.findCollectionSpecialization("/main", "values");
  REQUIRE(cppVector != nullptr);
  REQUIRE(vmVector != nullptr);
  REQUIRE(nativeVector != nullptr);
  CHECK(cppVector->collectionFamily == "vector");
  CHECK(cppVector->elementTypeText == "i32");
  CHECK(vmVector->collectionFamily == cppVector->collectionFamily);
  CHECK(nativeVector->collectionFamily == cppVector->collectionFamily);
  CHECK(vmVector->elementTypeText == cppVector->elementTypeText);
  CHECK(nativeVector->elementTypeText == cppVector->elementTypeText);

  const auto *cppQuery = findSemanticEntry(primec::semanticProgramQueryFactView(cppConformance.output.semanticProgram),
      [&cppConformance](const primec::SemanticProgramQueryFact &entry) {
        const std::string_view scopePath =
            entry.scopePathId != primec::InvalidSymbolId
                ? primec::semanticProgramResolveCallTargetString(
                      cppConformance.output.semanticProgram, entry.scopePathId)
                : std::string_view(entry.scopePath);
        return scopePath == "/main" &&
               primec::semanticProgramQueryFactResolvedPath(cppConformance.output.semanticProgram, entry) ==
                   "/lookup";
      });
  const auto *vmQuery = findSemanticEntry(primec::semanticProgramQueryFactView(vmConformance.output.semanticProgram),
      [&vmConformance](const primec::SemanticProgramQueryFact &entry) {
        const std::string_view scopePath =
            entry.scopePathId != primec::InvalidSymbolId
                ? primec::semanticProgramResolveCallTargetString(
                      vmConformance.output.semanticProgram, entry.scopePathId)
                : std::string_view(entry.scopePath);
        return scopePath == "/main" &&
               primec::semanticProgramQueryFactResolvedPath(vmConformance.output.semanticProgram, entry) ==
                   "/lookup";
      });
  const auto *nativeQuery = findSemanticEntry(primec::semanticProgramQueryFactView(nativeConformance.output.semanticProgram),
      [&nativeConformance](const primec::SemanticProgramQueryFact &entry) {
        const std::string_view scopePath =
            entry.scopePathId != primec::InvalidSymbolId
                ? primec::semanticProgramResolveCallTargetString(
                      nativeConformance.output.semanticProgram, entry.scopePathId)
                : std::string_view(entry.scopePath);
        return scopePath == "/main" &&
               primec::semanticProgramQueryFactResolvedPath(nativeConformance.output.semanticProgram, entry) ==
                   "/lookup";
      });
  REQUIRE(cppQuery != nullptr);
  REQUIRE(vmQuery != nullptr);
  REQUIRE(nativeQuery != nullptr);
  CHECK(cppQuery->bindingTypeText == "Result<int, i32>");
  CHECK(cppQuery->resultValueType == "int");
  CHECK(cppQuery->resultErrorType == "i32");
  CHECK(vmQuery->bindingTypeText == cppQuery->bindingTypeText);
  CHECK(nativeQuery->bindingTypeText == cppQuery->bindingTypeText);

  const auto *cppTry = findSemanticEntry(primec::semanticProgramTryFactView(cppConformance.output.semanticProgram),
      [&cppConformance](const primec::SemanticProgramTryFact &entry) {
        return entry.scopePath == "/main" &&
               primec::semanticProgramTryFactOperandResolvedPath(cppConformance.output.semanticProgram, entry) ==
                   "/lookup";
      });
  const auto *vmTry = findSemanticEntry(primec::semanticProgramTryFactView(vmConformance.output.semanticProgram),
      [&vmConformance](const primec::SemanticProgramTryFact &entry) {
        return entry.scopePath == "/main" &&
               primec::semanticProgramTryFactOperandResolvedPath(vmConformance.output.semanticProgram, entry) ==
                   "/lookup";
      });
  const auto *nativeTry = findSemanticEntry(primec::semanticProgramTryFactView(nativeConformance.output.semanticProgram),
      [&nativeConformance](const primec::SemanticProgramTryFact &entry) {
        return entry.scopePath == "/main" &&
               primec::semanticProgramTryFactOperandResolvedPath(nativeConformance.output.semanticProgram, entry) ==
                   "/lookup";
      });
  REQUIRE(cppTry != nullptr);
  REQUIRE(vmTry != nullptr);
  REQUIRE(nativeTry != nullptr);
  CHECK(cppTry->valueType == "int");
  CHECK(cppTry->errorType == "i32");
  CHECK(cppTry->onErrorHandlerPath == "/unexpectedError");
  CHECK(vmTry->valueType == cppTry->valueType);
  CHECK(nativeTry->valueType == cppTry->valueType);

  const auto *cppOnError = findSemanticEntry(primec::semanticProgramOnErrorFactView(cppConformance.output.semanticProgram),
      [](const primec::SemanticProgramOnErrorFact &entry) {
        return entry.definitionPath == "/main";
      });
  const auto *vmOnError = findSemanticEntry(primec::semanticProgramOnErrorFactView(vmConformance.output.semanticProgram),
      [](const primec::SemanticProgramOnErrorFact &entry) {
        return entry.definitionPath == "/main";
      });
  const auto *nativeOnError = findSemanticEntry(primec::semanticProgramOnErrorFactView(nativeConformance.output.semanticProgram),
      [](const primec::SemanticProgramOnErrorFact &entry) {
        return entry.definitionPath == "/main";
      });
  REQUIRE(cppOnError != nullptr);
  REQUIRE(vmOnError != nullptr);
  REQUIRE(nativeOnError != nullptr);
  CHECK(cppOnError->returnKind == "i32");
  CHECK(primec::semanticProgramOnErrorFactHandlerPath(cppConformance.output.semanticProgram, *cppOnError) ==
        "/unexpectedError");
  CHECK(primec::semanticProgramOnErrorFactHandlerPath(vmConformance.output.semanticProgram, *vmOnError) ==
        primec::semanticProgramOnErrorFactHandlerPath(cppConformance.output.semanticProgram, *cppOnError));
  CHECK(primec::semanticProgramOnErrorFactHandlerPath(nativeConformance.output.semanticProgram, *nativeOnError) ==
        primec::semanticProgramOnErrorFactHandlerPath(cppConformance.output.semanticProgram, *cppOnError));

  const auto *cppReturn = findSemanticEntry(primec::semanticProgramReturnFactView(cppConformance.output.semanticProgram),
      [&cppConformance](const primec::SemanticProgramReturnFact &entry) {
        return primec::semanticProgramReturnFactDefinitionPath(
                   cppConformance.output.semanticProgram, entry) == "/main";
      });
  const auto *vmReturn = findSemanticEntry(primec::semanticProgramReturnFactView(vmConformance.output.semanticProgram),
      [&vmConformance](const primec::SemanticProgramReturnFact &entry) {
        return primec::semanticProgramReturnFactDefinitionPath(
                   vmConformance.output.semanticProgram, entry) == "/main";
      });
  const auto *nativeReturn = findSemanticEntry(primec::semanticProgramReturnFactView(nativeConformance.output.semanticProgram),
      [&nativeConformance](const primec::SemanticProgramReturnFact &entry) {
        return primec::semanticProgramReturnFactDefinitionPath(
                   nativeConformance.output.semanticProgram, entry) == "/main";
      });
  REQUIRE(cppReturn != nullptr);
  REQUIRE(vmReturn != nullptr);
  REQUIRE(nativeReturn != nullptr);
  CHECK(cppReturn->bindingTypeText == "i32");
  CHECK(vmReturn->bindingTypeText == cppReturn->bindingTypeText);
  CHECK(nativeReturn->bindingTypeText == cppReturn->bindingTypeText);

  CHECK(cppConformance.backendKind == "cpp-ir");
  CHECK(vmConformance.backendKind == "vm");
  CHECK(cppConformance.emitResult.exitCode == 0);
  CHECK(vmConformance.emitResult.exitCode == 7);

  const std::string cpp = readTextFile(cppConformance.outputPath);
  CHECK(cpp.find("static int64_t ps_fn_0") != std::string::npos);
#if defined(__APPLE__) && (defined(__arm64__) || defined(__aarch64__))
  CHECK(nativeConformance.backendKind == "native");
  CHECK(nativeConformance.emitResult.exitCode == 0);
  CHECK(std::filesystem::exists(nativeConformance.outputPath));
  CHECK(std::filesystem::is_regular_file(nativeConformance.outputPath));
  CHECK(std::filesystem::file_size(nativeConformance.outputPath) > 0);
#endif
}

TEST_CASE("backend conformance keeps auto-bound Result combinator facts aligned across backends") {
  const std::string source = R"(
import /std/file/*

[effects(io_err)]
log_file_error([FileError] err) {
  print_line_error(err.why())
}

[return<int> effects(io_out, io_err) on_error<FileError, /log_file_error>]
main() {
  [auto] mapped{ Result.map(Result.ok(2i32), []([i32] value) { return(multiply(value, 4i32)) }) }
  [auto] chained{ Result.and_then(Result.ok(2i32), []([i32] value) { return(Result.ok(plus(value, 3i32))) }) }
  [auto] summed{
    Result.map2(Result.ok(2i32), Result.ok(3i32), []([i32] left, [i32] right) { return(plus(left, right)) })
  }
  [i32] first{try(mapped)}
  [i32] second{try(chained)}
  [i32] third{try(summed)}
  return(first + second + third)
}
)";

  auto runConformance = [&](std::string_view emitKind) {
    primec::testing::CompilePipelineBackendConformance conformance;
    std::string error;
    REQUIRE(primec::testing::runCompilePipelineBackendConformanceForTesting(
        source, "/main", emitKind, conformance, error));
    CHECK(error.empty());
    CHECK(conformance.output.hasSemanticProgram);
    return conformance;
  };

  const auto cppConformance = runConformance("cpp-ir");
  const auto vmConformance = runConformance("vm");
#if defined(__APPLE__) && (defined(__arm64__) || defined(__aarch64__))
  const auto nativeConformance = runConformance("native");
#else
  const auto &nativeConformance = cppConformance;
#endif

  auto requireLocalAutoFact = [](const primec::SemanticProgram &semanticProgram,
                                 std::string_view bindingName) {
    const auto *localAuto = findSemanticEntry(
        primec::semanticProgramLocalAutoFactView(semanticProgram),
        [bindingName](const primec::SemanticProgramLocalAutoFact &entry) {
          return entry.scopePath == "/main" && entry.bindingName == bindingName;
        });
    REQUIRE(localAuto != nullptr);
    return localAuto;
  };

  auto checkLocalAutoFact = [&](std::string_view bindingName) {
    const auto *cppLocalAuto = requireLocalAutoFact(cppConformance.output.semanticProgram, bindingName);
    const auto *vmLocalAuto = requireLocalAutoFact(vmConformance.output.semanticProgram, bindingName);
    const auto *nativeLocalAuto = requireLocalAutoFact(nativeConformance.output.semanticProgram, bindingName);

    CHECK(cppLocalAuto->bindingTypeText == "Result<i32, FileError>");
    CHECK(vmLocalAuto->bindingTypeText == cppLocalAuto->bindingTypeText);
    CHECK(nativeLocalAuto->bindingTypeText == cppLocalAuto->bindingTypeText);

    const std::string_view cppResolved =
        primec::semanticProgramLocalAutoFactInitializerResolvedPath(
            cppConformance.output.semanticProgram, *cppLocalAuto);
    const std::string_view vmResolved =
        primec::semanticProgramLocalAutoFactInitializerResolvedPath(
            vmConformance.output.semanticProgram, *vmLocalAuto);
    const std::string_view nativeResolved =
        primec::semanticProgramLocalAutoFactInitializerResolvedPath(
            nativeConformance.output.semanticProgram, *nativeLocalAuto);
    CHECK(!cppResolved.empty());
    CHECK(vmResolved == cppResolved);
    CHECK(nativeResolved == cppResolved);
  };

  checkLocalAutoFact("mapped");
  checkLocalAutoFact("chained");
  checkLocalAutoFact("summed");

  const auto *cppReturn = findSemanticEntry(primec::semanticProgramReturnFactView(cppConformance.output.semanticProgram),
      [&cppConformance](const primec::SemanticProgramReturnFact &entry) {
        return primec::semanticProgramReturnFactDefinitionPath(
                   cppConformance.output.semanticProgram, entry) == "/main";
      });
  const auto *vmReturn = findSemanticEntry(primec::semanticProgramReturnFactView(vmConformance.output.semanticProgram),
      [&vmConformance](const primec::SemanticProgramReturnFact &entry) {
        return primec::semanticProgramReturnFactDefinitionPath(
                   vmConformance.output.semanticProgram, entry) == "/main";
      });
  const auto *nativeReturn = findSemanticEntry(primec::semanticProgramReturnFactView(nativeConformance.output.semanticProgram),
      [&nativeConformance](const primec::SemanticProgramReturnFact &entry) {
        return primec::semanticProgramReturnFactDefinitionPath(
                   nativeConformance.output.semanticProgram, entry) == "/main";
      });
  REQUIRE(cppReturn != nullptr);
  REQUIRE(vmReturn != nullptr);
  REQUIRE(nativeReturn != nullptr);
  CHECK(cppReturn->bindingTypeText == "i32");
  CHECK(vmReturn->bindingTypeText == cppReturn->bindingTypeText);
  CHECK(nativeReturn->bindingTypeText == cppReturn->bindingTypeText);
}

TEST_CASE("compile pipeline preserves semantic product on post-semantics failure") {
  const std::filesystem::path tempPath = makeTempIrPipelineSourcePath();
  {
    std::ofstream file(tempPath);
    REQUIRE(file.good());
    file << R"(import /std/gfx/experimental/*

[return<i32>]
main() {
  return(0i32)
}
)";
  }

  primec::Options options;
  options.inputPath = tempPath.string();
  options.entryPath = "/main";
  options.emitKind = "glsl";
  options.collectDiagnostics = true;
  primec::addDefaultStdlibInclude(options.inputPath, options.importPaths);

  primec::CompilePipelineDiagnosticInfo diagnosticInfo;
  primec::CompilePipelineErrorStage errorStage = primec::CompilePipelineErrorStage::None;
  std::string error;
  primec::CompilePipelineResult result =
      primec::runCompilePipelineResult(options, errorStage, error, &diagnosticInfo);

  std::error_code ec;
  std::filesystem::remove(tempPath, ec);

  const auto *failure = std::get_if<primec::CompilePipelineFailureResult>(&result);
  REQUIRE(failure != nullptr);
  CHECK(errorStage == primec::CompilePipelineErrorStage::Semantic);
  CHECK(failure->failure.stage == primec::CompilePipelineErrorStage::Semantic);
  CHECK(error == "graphics stdlib runtime substrate unavailable for glsl target: /std/gfx/experimental/*");
  CHECK(failure->failure.message == error);
  REQUIRE_FALSE(failure->failure.diagnosticInfo.records.empty());
  CHECK(failure->failure.diagnosticInfo.records.front().message == error);
  REQUIRE(failure->hasSemanticProgram);
  CHECK(failure->semanticProgram.entryPath == "/main");
  CHECK(std::find(failure->semanticProgram.imports.begin(),
                  failure->semanticProgram.imports.end(),
                  "/std/gfx/experimental/*") != failure->semanticProgram.imports.end());
  CHECK(diagnosticInfo.message == failure->failure.diagnosticInfo.message);
  REQUIRE_FALSE(diagnosticInfo.records.empty());
  CHECK(diagnosticInfo.records.front().message == error);
}

TEST_CASE("compile pipeline skips semantic product for ast-semantic dumps") {
  const std::filesystem::path tempPath = makeTempIrPipelineSourcePath();
  {
    std::ofstream file(tempPath);
    REQUIRE(file.good());
    file << R"(namespace bench {
[i32]
main() {
  return(0)
}
}
)";
  }

  primec::Options options;
  options.inputPath = tempPath.string();
  options.entryPath = "/bench/main";
  options.emitKind = "native";
  options.dumpStage = "ast-semantic";
  options.collectDiagnostics = true;
  primec::addDefaultStdlibInclude(options.inputPath, options.importPaths);

  primec::CompilePipelineOutput output;
  primec::CompilePipelineDiagnosticInfo diagnosticInfo;
  primec::CompilePipelineErrorStage errorStage = primec::CompilePipelineErrorStage::None;
  std::string error;
  const bool ok = primec::runCompilePipeline(options, output, errorStage, error, &diagnosticInfo);

  std::error_code ec;
  std::filesystem::remove(tempPath, ec);

  REQUIRE(ok);
  CHECK(output.hasDumpOutput);
  CHECK(errorStage == primec::CompilePipelineErrorStage::None);
  CHECK_FALSE(output.hasFailure);
  CHECK(output.semanticProductRequested == false);
  CHECK(output.semanticProductBuilt == false);
  CHECK(output.semanticProductDecision ==
        primec::CompilePipelineSemanticProductDecision::SkipForAstSemanticDump);
  CHECK_FALSE(output.hasSemanticProgram);
  CHECK(output.dumpOutput.find("ast {") != std::string::npos);
  CHECK(output.dumpOutput.find("/bench/main()") != std::string::npos);
}

TEST_CASE("compile pipeline builds semantic product for semantic-product dumps") {
  const std::filesystem::path tempPath = makeTempIrPipelineSourcePath();
  {
    std::ofstream file(tempPath);
    REQUIRE(file.good());
    file << R"(namespace bench {
[void]
callee() {}
[i32]
main() {
  callee()
  return(0)
}
}
)";
  }

  primec::Options options;
  options.inputPath = tempPath.string();
  options.entryPath = "/bench/main";
  options.emitKind = "native";
  options.dumpStage = "semantic-product";
  options.collectDiagnostics = true;
  primec::addDefaultStdlibInclude(options.inputPath, options.importPaths);

  primec::CompilePipelineOutput output;
  primec::CompilePipelineDiagnosticInfo diagnosticInfo;
  primec::CompilePipelineErrorStage errorStage = primec::CompilePipelineErrorStage::None;
  std::string error;
  const bool ok = primec::runCompilePipeline(options, output, errorStage, error, &diagnosticInfo);

  std::error_code ec;
  std::filesystem::remove(tempPath, ec);

  REQUIRE(ok);
  CHECK(output.hasDumpOutput);
  CHECK(errorStage == primec::CompilePipelineErrorStage::None);
  CHECK_FALSE(output.hasFailure);
  CHECK(output.semanticProductRequested);
  CHECK(output.semanticProductBuilt);
  CHECK(output.semanticProductDecision ==
        primec::CompilePipelineSemanticProductDecision::RequireForConsumingPath);
  REQUIRE(output.hasSemanticProgram);
  CHECK(output.semanticProgram.entryPath == "/bench/main");
  CHECK(output.dumpOutput.find("semantic_product {") != std::string::npos);
  CHECK(output.dumpOutput.find("direct_call_targets[0]:") != std::string::npos);
}

TEST_CASE("compile pipeline keeps semantic product for emit paths") {
  const std::filesystem::path tempPath = makeTempIrPipelineSourcePath();
  {
    std::ofstream file(tempPath);
    REQUIRE(file.good());
    file << R"(namespace bench {
[i32]
main() {
  return(0)
}
}
)";
  }

  primec::Options options;
  options.inputPath = tempPath.string();
  options.entryPath = "/bench/main";
  options.emitKind = "vm";
  options.collectDiagnostics = true;
  primec::addDefaultStdlibInclude(options.inputPath, options.importPaths);

  primec::CompilePipelineOutput output;
  primec::CompilePipelineDiagnosticInfo diagnosticInfo;
  primec::CompilePipelineErrorStage errorStage = primec::CompilePipelineErrorStage::None;
  std::string error;
  const bool ok = primec::runCompilePipeline(options, output, errorStage, error, &diagnosticInfo);

  std::error_code ec;
  std::filesystem::remove(tempPath, ec);

  REQUIRE(ok);
  CHECK_FALSE(output.hasDumpOutput);
  CHECK(errorStage == primec::CompilePipelineErrorStage::None);
  CHECK_FALSE(output.hasFailure);
  CHECK(output.semanticProductRequested);
  CHECK(output.semanticProductBuilt);
  CHECK(output.semanticProductDecision ==
        primec::CompilePipelineSemanticProductDecision::RequireForConsumingPath);
  REQUIRE(output.hasSemanticProgram);
  CHECK(output.semanticProgram.entryPath == "/bench/main");
}

TEST_CASE("compile pipeline semantic-product generation stays limited to semantic-product dumps and consuming emit paths") {
  const std::filesystem::path tempPath = makeTempIrPipelineSourcePath();
  {
    std::ofstream file(tempPath);
    REQUIRE(file.good());
    file << R"(namespace bench {
[i32]
main() {
  return(0)
}
}
)";
  }

  struct Scenario {
    std::string_view name;
    std::string_view emitKind;
    std::string_view dumpStage;
    bool expectDumpOutput;
    bool expectSemanticProductRequested;
    bool expectSemanticProductBuilt;
    bool expectSemanticProgram;
  };

  const std::vector<Scenario> scenarios = {
      {"pre-ast dump", "native", "pre_ast", true, false, false, false},
      {"ast dump", "native", "ast", true, false, false, false},
      {"ir dump", "native", "ir", true, false, false, false},
      {"type-graph dump", "native", "type-graph", true, false, false, false},
      {"ast-semantic dump", "native", "ast-semantic", true, false, false, false},
      {"semantic-product dump", "native", "semantic-product", true, true, true, true},
      {"native emit", "native", "", false, true, true, true},
      {"vm emit", "vm", "", false, true, true, true},
  };

  for (const Scenario &scenario : scenarios) {
    primec::Options options;
    options.inputPath = tempPath.string();
    options.entryPath = "/bench/main";
    options.emitKind = std::string(scenario.emitKind);
    options.dumpStage = std::string(scenario.dumpStage);
    options.collectDiagnostics = true;
    primec::addDefaultStdlibInclude(options.inputPath, options.importPaths);

    primec::CompilePipelineOutput output;
    primec::CompilePipelineDiagnosticInfo diagnosticInfo;
    primec::CompilePipelineErrorStage errorStage = primec::CompilePipelineErrorStage::None;
    std::string error;
    const bool ok = primec::runCompilePipeline(options, output, errorStage, error, &diagnosticInfo);

    INFO(scenario.name);
    INFO(error);
    REQUIRE(ok);
    CHECK(error.empty());
    CHECK(errorStage == primec::CompilePipelineErrorStage::None);
    CHECK_FALSE(output.hasFailure);
    CHECK(output.hasDumpOutput == scenario.expectDumpOutput);
    CHECK(output.semanticProductRequested == scenario.expectSemanticProductRequested);
    CHECK(output.semanticProductBuilt == scenario.expectSemanticProductBuilt);
    CHECK(output.hasSemanticProgram == scenario.expectSemanticProgram);
    if (scenario.expectSemanticProgram) {
      CHECK(output.semanticProgram.entryPath == "/bench/main");
    }
  }

  std::error_code ec;
  std::filesystem::remove(tempPath, ec);
}

TEST_CASE("compile pipeline pre-ast dump returns before parser failures") {
  const std::filesystem::path tempPath = makeTempIrPipelineSourcePath();
  {
    std::ofstream file(tempPath);
    REQUIRE(file.good());
    file << R"(namespace bench {
[return<i32>]
main( {
  return(0i32)
}
}
)";
  }

  primec::Options options;
  options.inputPath = tempPath.string();
  options.entryPath = "/bench/main";
  options.emitKind = "native";
  options.dumpStage = "pre_ast";
  options.collectDiagnostics = true;
  primec::addDefaultStdlibInclude(options.inputPath, options.importPaths);

  primec::CompilePipelineOutput output;
  primec::CompilePipelineDiagnosticInfo diagnosticInfo;
  primec::CompilePipelineErrorStage errorStage = primec::CompilePipelineErrorStage::None;
  std::string error;
  const bool ok = primec::runCompilePipeline(options, output, errorStage, error, &diagnosticInfo);

  std::error_code ec;
  std::filesystem::remove(tempPath, ec);

  INFO(error);
  REQUIRE(ok);
  CHECK(error.empty());
  CHECK(errorStage == primec::CompilePipelineErrorStage::None);
  CHECK_FALSE(output.hasFailure);
  REQUIRE(output.hasDumpOutput);
  CHECK(output.dumpOutput == output.filteredSource);
  CHECK(output.dumpOutput.find("main( {") != std::string::npos);
  CHECK(output.program.definitions.empty());
  CHECK(output.program.executions.empty());
  CHECK_FALSE(output.hasSemanticProgram);
  CHECK(diagnosticInfo.records.empty());
}

TEST_CASE("compile pipeline explicit skip mode omits semantic product for non-consuming paths") {
  const std::filesystem::path tempPath = makeTempIrPipelineSourcePath();
  {
    std::ofstream file(tempPath);
    REQUIRE(file.good());
    file << R"(namespace bench {
[i32]
main() {
  return(0)
}
}
)";
  }

  primec::Options options;
  options.inputPath = tempPath.string();
  options.entryPath = "/bench/main";
  options.emitKind = "vm";
  options.skipSemanticProductForNonConsumingPath = true;
  options.collectDiagnostics = true;
  primec::addDefaultStdlibInclude(options.inputPath, options.importPaths);

  primec::CompilePipelineOutput output;
  primec::CompilePipelineDiagnosticInfo diagnosticInfo;
  primec::CompilePipelineErrorStage errorStage = primec::CompilePipelineErrorStage::None;
  std::string error;
  const bool ok = primec::runCompilePipeline(options, output, errorStage, error, &diagnosticInfo);

  std::error_code ec;
  std::filesystem::remove(tempPath, ec);

  REQUIRE(ok);
  CHECK_FALSE(output.hasDumpOutput);
  CHECK(errorStage == primec::CompilePipelineErrorStage::None);
  CHECK_FALSE(output.hasFailure);
  CHECK_FALSE(output.semanticProductRequested);
  CHECK_FALSE(output.semanticProductBuilt);
  CHECK(output.semanticProductDecision ==
        primec::CompilePipelineSemanticProductDecision::SkipForNonConsumingPath);
  CHECK_FALSE(output.hasSemanticProgram);
}

TEST_CASE("ir pipeline helper parseAndValidate skips semantic product when not requested") {
  const std::string source = R"(import /std/math/Vec2

[i32]
main() {
  return(0i32)
}
)";

  primec::Program program;
  std::string error;
  const bool ok = parseAndValidate(source, program, error, {"io_out", "io_err"});
  INFO(error);
  REQUIRE(ok);
  CHECK(error.empty());
  CHECK_FALSE(program.definitions.empty());
}

TEST_CASE("compile pipeline explicit skip mode rejects semantic-product dump requests") {
  const std::filesystem::path tempPath = makeTempIrPipelineSourcePath();
  {
    std::ofstream file(tempPath);
    REQUIRE(file.good());
    file << R"(namespace bench {
[return<i32>]
main() {
  return(0i32)
}
}
)";
  }

  primec::Options options;
  options.inputPath = tempPath.string();
  options.entryPath = "/bench/main";
  options.emitKind = "vm";
  options.dumpStage = "semantic-product";
  options.skipSemanticProductForNonConsumingPath = true;
  primec::addDefaultStdlibInclude(options.inputPath, options.importPaths);

  primec::CompilePipelineOutput output;
  primec::CompilePipelineErrorStage errorStage = primec::CompilePipelineErrorStage::None;
  std::string error;
  const bool ok = primec::runCompilePipeline(options, output, errorStage, error);

  std::error_code ec;
  std::filesystem::remove(tempPath, ec);

  CHECK_FALSE(ok);
  CHECK(errorStage == primec::CompilePipelineErrorStage::Semantic);
  CHECK(error == "semantic-product dump requested without semantic product");
  CHECK(output.semanticProductDecision ==
        primec::CompilePipelineSemanticProductDecision::SkipForNonConsumingPath);
  CHECK_FALSE(output.hasSemanticProgram);
}

TEST_CASE("compile pipeline benchmark force-on keeps semantic product for ast-semantic dumps") {
  const std::filesystem::path tempPath = makeTempIrPipelineSourcePath();
  {
    std::ofstream file(tempPath);
    REQUIRE(file.good());
    file << R"(namespace bench {
[return<i32>]
main() {
  return(0i32)
}
}
)";
  }

  primec::Options options;
  options.inputPath = tempPath.string();
  options.entryPath = "/bench/main";
  options.emitKind = "native";
  options.dumpStage = "ast-semantic";
  options.benchmarkForceSemanticProduct = true;
  primec::addDefaultStdlibInclude(options.inputPath, options.importPaths);

  primec::CompilePipelineOutput output;
  primec::CompilePipelineErrorStage errorStage = primec::CompilePipelineErrorStage::None;
  std::string error;
  const bool ok = primec::runCompilePipeline(options, output, errorStage, error);

  std::error_code ec;
  std::filesystem::remove(tempPath, ec);

  REQUIRE(ok);
  CHECK(output.hasDumpOutput);
  CHECK(output.semanticProductRequested);
  CHECK(output.semanticProductBuilt);
  CHECK(output.semanticProductDecision ==
        primec::CompilePipelineSemanticProductDecision::ForcedOnForBenchmark);
  CHECK(output.hasSemanticProgram);
}

TEST_CASE("compile pipeline benchmark force-off rejects semantic-product dump") {
  const std::filesystem::path tempPath = makeTempIrPipelineSourcePath();
  {
    std::ofstream file(tempPath);
    REQUIRE(file.good());
    file << R"(namespace bench {
[return<i32>]
main() {
  return(0i32)
}
}
)";
  }

  primec::Options options;
  options.inputPath = tempPath.string();
  options.entryPath = "/bench/main";
  options.emitKind = "native";
  options.dumpStage = "semantic-product";
  options.benchmarkForceSemanticProduct = false;
  primec::addDefaultStdlibInclude(options.inputPath, options.importPaths);

  primec::CompilePipelineOutput output;
  primec::CompilePipelineErrorStage errorStage = primec::CompilePipelineErrorStage::None;
  std::string error;
  const bool ok = primec::runCompilePipeline(options, output, errorStage, error);

  std::error_code ec;
  std::filesystem::remove(tempPath, ec);

  CHECK_FALSE(ok);
  CHECK(errorStage == primec::CompilePipelineErrorStage::Semantic);
  CHECK(error == "semantic-product dump requested without semantic product");
  CHECK(output.semanticProductDecision ==
        primec::CompilePipelineSemanticProductDecision::ForcedOffForBenchmark);
  CHECK_FALSE(output.hasSemanticProgram);
}

TEST_CASE("compile pipeline benchmark no-fact-emission keeps semantic product shells empty") {
  const std::filesystem::path tempPath = makeTempIrPipelineSourcePath();
  {
    std::ofstream file(tempPath);
    REQUIRE(file.good());
    file << R"(namespace bench {
[void]
callee() {}
[return<i32>]
main() {
  callee()
  return(0i32)
}
}
)";
  }

  primec::Options options;
  options.inputPath = tempPath.string();
  options.entryPath = "/bench/main";
  options.emitKind = "native";
  options.dumpStage = "semantic-product";
  options.benchmarkSemanticNoFactEmission = true;
  primec::addDefaultStdlibInclude(options.inputPath, options.importPaths);

  primec::CompilePipelineOutput output;
  primec::CompilePipelineErrorStage errorStage = primec::CompilePipelineErrorStage::None;
  std::string error;
  const bool ok = primec::runCompilePipeline(options, output, errorStage, error);

  std::error_code ec;
  std::filesystem::remove(tempPath, ec);

  REQUIRE(ok);
  REQUIRE(output.hasSemanticProgram);
  CHECK_FALSE(output.hasSemanticPhaseCounters);
  CHECK(output.semanticProgram.directCallTargets.empty());
  CHECK(output.semanticProgram.callableSummaries.empty());
  CHECK(output.semanticProgram.bindingFacts.empty());
  CHECK(output.semanticProgram.queryFacts.empty());
}

TEST_CASE("compile pipeline ast-semantic phase counters prove semantic-product build skip") {
  const std::filesystem::path tempPath = makeTempIrPipelineSourcePath();
  {
    std::ofstream file(tempPath);
    REQUIRE(file.good());
    file << R"(namespace bench {
[return<i32>]
callee([i32] value) {
  return(value)
}
[return<i32>]
main() {
  [i32 mut] value{4i32}
  return(callee(value))
}
}
)";
  }

  primec::Options options;
  options.inputPath = tempPath.string();
  options.entryPath = "/bench/main";
  options.emitKind = "native";
  options.dumpStage = "ast-semantic";
  options.benchmarkSemanticPhaseCounters = true;
  primec::addDefaultStdlibInclude(options.inputPath, options.importPaths);

  primec::CompilePipelineOutput output;
  primec::CompilePipelineErrorStage errorStage = primec::CompilePipelineErrorStage::None;
  std::string error;
  const bool ok = primec::runCompilePipeline(options, output, errorStage, error);

  std::error_code ec;
  std::filesystem::remove(tempPath, ec);

  REQUIRE(ok);
  CHECK(error.empty());
  CHECK(output.semanticProductDecision ==
        primec::CompilePipelineSemanticProductDecision::SkipForAstSemanticDump);
  CHECK_FALSE(output.semanticProductRequested);
  CHECK_FALSE(output.semanticProductBuilt);
  CHECK_FALSE(output.hasSemanticProgram);
  REQUIRE(output.hasSemanticPhaseCounters);
  CHECK(output.semanticPhaseCounters.validation.callsVisited > 0);
  CHECK(output.semanticPhaseCounters.semanticProductBuild.callsVisited == 0);
  CHECK(output.semanticPhaseCounters.semanticProductBuild.factsProduced == 0);
}

TEST_CASE("compile pipeline benchmark semantic phase counters are opt-in and populated") {
  const std::filesystem::path tempPath = makeTempIrPipelineSourcePath();
  {
    std::ofstream file(tempPath);
    REQUIRE(file.good());
    file << R"(namespace bench {
[return<i32>]
callee([i32] value) {
  return(value)
}
[return<i32>]
main() {
  [i32 mut] value{4i32}
  return(callee(value))
}
}
)";
  }

  primec::Options baseOptions;
  baseOptions.inputPath = tempPath.string();
  baseOptions.entryPath = "/bench/main";
  baseOptions.emitKind = "native";
  baseOptions.dumpStage = "semantic-product";
  primec::addDefaultStdlibInclude(baseOptions.inputPath, baseOptions.importPaths);

  primec::CompilePipelineOutput defaultOutput;
  primec::CompilePipelineErrorStage defaultErrorStage = primec::CompilePipelineErrorStage::None;
  std::string defaultError;
  const bool defaultOk =
      primec::runCompilePipeline(baseOptions, defaultOutput, defaultErrorStage, defaultError);
  REQUIRE(defaultOk);
  CHECK(defaultError.empty());
  CHECK_FALSE(defaultOutput.hasSemanticPhaseCounters);

  primec::Options countersOptions = baseOptions;
  countersOptions.benchmarkSemanticPhaseCounters = true;

  primec::CompilePipelineOutput countersOutput;
  primec::CompilePipelineErrorStage countersErrorStage = primec::CompilePipelineErrorStage::None;
  std::string countersError;
  const bool countersOk =
      primec::runCompilePipeline(countersOptions, countersOutput, countersErrorStage, countersError);

  std::error_code ec;
  std::filesystem::remove(tempPath, ec);

  REQUIRE(countersOk);
  CHECK(countersError.empty());
  REQUIRE(countersOutput.hasSemanticPhaseCounters);
  CHECK(countersOutput.semanticPhaseCounters.validation.callsVisited > 0);
  CHECK(countersOutput.semanticPhaseCounters.validation.peakLocalMapSize > 0);
  CHECK(countersOutput.semanticPhaseCounters.validation.factsProduced == 0);
  CHECK(countersOutput.semanticPhaseCounters.semanticProductBuild.callsVisited > 0);
  CHECK(countersOutput.semanticPhaseCounters.semanticProductBuild.peakLocalMapSize == 0);
  CHECK(countersOutput.semanticPhaseCounters.semanticProductBuild.factsProduced > 0);
}

TEST_CASE("compile pipeline benchmark semantic allocation counters are opt-in and populated") {
  const std::filesystem::path tempPath = makeTempIrPipelineSourcePath();
  {
    std::ofstream file(tempPath);
    REQUIRE(file.good());
    file << R"(namespace bench {
[return<i32>]
callee([i32] value) {
  return(value)
}
[return<i32>]
main() {
  [i32 mut] value{4i32}
  return(callee(value))
}
}
)";
  }

  primec::Options options;
  options.inputPath = tempPath.string();
  options.entryPath = "/bench/main";
  options.emitKind = "native";
  options.dumpStage = "semantic-product";
  options.benchmarkSemanticAllocationCounters = true;
  primec::addDefaultStdlibInclude(options.inputPath, options.importPaths);

  primec::CompilePipelineOutput output;
  primec::CompilePipelineErrorStage errorStage = primec::CompilePipelineErrorStage::None;
  std::string error;
  const bool ok = primec::runCompilePipeline(options, output, errorStage, error);

  std::error_code ec;
  std::filesystem::remove(tempPath, ec);

  REQUIRE(ok);
  CHECK(error.empty());
  REQUIRE(output.hasSemanticPhaseCounters);
  CHECK(output.semanticPhaseCounters.validation.allocationCount > 0);
  CHECK(output.semanticPhaseCounters.validation.allocatedBytes > 0);
  CHECK(output.semanticPhaseCounters.semanticProductBuild.allocationCount > 0);
}

TEST_CASE("compile pipeline benchmark semantic rss checkpoints are opt-in and populated") {
  const std::filesystem::path tempPath = makeTempIrPipelineSourcePath();
  {
    std::ofstream file(tempPath);
    REQUIRE(file.good());
    file << R"(namespace bench {
[return<i32>]
callee([i32] value) {
  return(value)
}
[return<i32>]
main() {
  [i32 mut] value{4i32}
  return(callee(value))
}
}
)";
  }

  primec::Options options;
  options.inputPath = tempPath.string();
  options.entryPath = "/bench/main";
  options.emitKind = "native";
  options.dumpStage = "semantic-product";
  options.benchmarkSemanticRssCheckpoints = true;
  primec::addDefaultStdlibInclude(options.inputPath, options.importPaths);

  primec::CompilePipelineOutput output;
  primec::CompilePipelineErrorStage errorStage = primec::CompilePipelineErrorStage::None;
  std::string error;
  const bool ok = primec::runCompilePipeline(options, output, errorStage, error);

  std::error_code ec;
  std::filesystem::remove(tempPath, ec);

  REQUIRE(ok);
  CHECK(error.empty());
  REQUIRE(output.hasSemanticPhaseCounters);
  checkOptionalRssCheckpointSnapshot(output.semanticPhaseCounters.validation);
  checkOptionalRssCheckpointSnapshot(output.semanticPhaseCounters.semanticProductBuild);
}

TEST_CASE("compile pipeline benchmark fact-family allowlist keeps only selected collectors") {
  const std::filesystem::path tempPath = makeTempIrPipelineSourcePath();
  {
    std::ofstream file(tempPath);
    REQUIRE(file.good());
    file << R"(namespace bench {
[void]
callee() {}
[return<i32>]
main() {
  callee()
  return(0i32)
}
}
)";
  }

  primec::Options options;
  options.inputPath = tempPath.string();
  options.entryPath = "/bench/main";
  options.emitKind = "native";
  options.dumpStage = "semantic-product";
  options.benchmarkSemanticFactFamiliesSpecified = true;
  options.benchmarkSemanticFactFamilies = {"callable_summaries"};
  primec::addDefaultStdlibInclude(options.inputPath, options.importPaths);

  primec::CompilePipelineOutput output;
  primec::CompilePipelineErrorStage errorStage = primec::CompilePipelineErrorStage::None;
  std::string error;
  const bool ok = primec::runCompilePipeline(options, output, errorStage, error);

  std::error_code ec;
  std::filesystem::remove(tempPath, ec);

  REQUIRE(ok);
  REQUIRE(output.hasSemanticProgram);
  CHECK_FALSE(output.semanticProgram.callableSummaries.empty());
  CHECK(output.semanticProgram.directCallTargets.empty());
  CHECK(output.semanticProgram.bindingFacts.empty());
  CHECK(output.semanticProgram.returnFacts.empty());
  CHECK(output.semanticProgram.queryFacts.empty());
}

TEST_CASE("compile pipeline direct and bridge collector merge keeps output-order parity") {
  const std::filesystem::path tempPath = makeTempIrPipelineSourcePath();
  {
    std::ofstream file(tempPath);
    REQUIRE(file.good());
    file << R"(namespace bench {
[void]
callee() {}
[void]
main() {
  [vector<i32>] values{vector<i32>()}
  callee()
  [i32] countA{count(values)}
  [i32] countB{count(values)}
  [i32] capA{capacity(values)}
}
}
)";
  }

  const auto runWithFamilies = [&](const std::vector<std::string> &families) {
    primec::Options options;
    options.inputPath = tempPath.string();
    options.entryPath = "/bench/main";
    options.emitKind = "native";
    options.dumpStage = "semantic-product";
    options.benchmarkSemanticFactFamiliesSpecified = true;
    options.benchmarkSemanticFactFamilies = families;
    primec::addDefaultStdlibInclude(options.inputPath, options.importPaths);

    primec::CompilePipelineOutput output;
    primec::CompilePipelineErrorStage errorStage = primec::CompilePipelineErrorStage::None;
    std::string error;
    const bool ok = primec::runCompilePipeline(options, output, errorStage, error);
    REQUIRE(ok);
    CHECK(error.empty());
    REQUIRE(output.hasSemanticProgram);
    return output;
  };

  const primec::CompilePipelineOutput combinedOutput =
      runWithFamilies({"direct_call_targets", "bridge_path_choices"});
  const primec::CompilePipelineOutput directOnlyOutput =
      runWithFamilies({"direct_call_targets"});
  const primec::CompilePipelineOutput bridgeOnlyOutput =
      runWithFamilies({"bridge_path_choices"});

  std::error_code ec;
  std::filesystem::remove(tempPath, ec);

  const auto combinedDirectTargets =
      primec::semanticProgramDirectCallTargetView(combinedOutput.semanticProgram);
  const auto directOnlyTargets =
      primec::semanticProgramDirectCallTargetView(directOnlyOutput.semanticProgram);
  REQUIRE(combinedDirectTargets.size() == directOnlyTargets.size());
  for (std::size_t i = 0; i < combinedDirectTargets.size(); ++i) {
    const auto *combinedEntry = combinedDirectTargets[i];
    const auto *directOnlyEntry = directOnlyTargets[i];
    REQUIRE(combinedEntry != nullptr);
    REQUIRE(directOnlyEntry != nullptr);
    CHECK(combinedEntry->scopePath == directOnlyEntry->scopePath);
    CHECK(combinedEntry->callName == directOnlyEntry->callName);
    CHECK(primec::semanticProgramDirectCallTargetResolvedPath(combinedOutput.semanticProgram, *combinedEntry) ==
          primec::semanticProgramDirectCallTargetResolvedPath(directOnlyOutput.semanticProgram, *directOnlyEntry));
  }

  const auto combinedBridgeChoices =
      primec::semanticProgramBridgePathChoiceView(combinedOutput.semanticProgram);
  const auto bridgeOnlyChoices =
      primec::semanticProgramBridgePathChoiceView(bridgeOnlyOutput.semanticProgram);
  REQUIRE(combinedBridgeChoices.size() == bridgeOnlyChoices.size());
  for (std::size_t i = 0; i < combinedBridgeChoices.size(); ++i) {
    const auto *combinedEntry = combinedBridgeChoices[i];
    const auto *bridgeOnlyEntry = bridgeOnlyChoices[i];
    REQUIRE(combinedEntry != nullptr);
    REQUIRE(bridgeOnlyEntry != nullptr);
    CHECK(combinedEntry->scopePath == bridgeOnlyEntry->scopePath);
    CHECK(combinedEntry->collectionFamily == bridgeOnlyEntry->collectionFamily);
    CHECK(primec::semanticProgramBridgePathChoiceHelperName(combinedOutput.semanticProgram, *combinedEntry) ==
          primec::semanticProgramBridgePathChoiceHelperName(bridgeOnlyOutput.semanticProgram, *bridgeOnlyEntry));
    CHECK(primec::semanticProgramResolveCallTargetString(combinedOutput.semanticProgram, combinedEntry->chosenPathId) ==
          primec::semanticProgramResolveCallTargetString(bridgeOnlyOutput.semanticProgram,
                                                         bridgeOnlyEntry->chosenPathId));
  }
}

TEST_CASE("compile pipeline benchmark worker-count stress keeps /std/math/* semantic-product dumps deterministic") {
  constexpr std::size_t localDefinitionCount = 64;
  const std::filesystem::path tempPath = makeTempIrPipelineSourcePath();
  {
    std::ofstream file(tempPath);
    REQUIRE(file.good());
    file << buildMathStressSemanticSource(localDefinitionCount);
  }

  struct StressSnapshot {
    std::string dumpOutput;
    std::size_t definitionCount = 0;
    std::size_t callableSummaryCount = 0;
    std::size_t directCallTargetCount = 0;
  };

  const auto runStress = [&]() {
    primec::Options options;
    options.inputPath = tempPath.string();
    options.entryPath = "/main";
    options.emitKind = "native";
    options.dumpStage = "semantic-product";
    options.collectDiagnostics = true;
    options.benchmarkSemanticDefinitionValidationWorkerCount = 4;
    primec::addDefaultStdlibInclude(options.inputPath, options.importPaths);

    primec::CompilePipelineOutput output;
    primec::CompilePipelineDiagnosticInfo diagnosticInfo;
    primec::CompilePipelineErrorStage errorStage = primec::CompilePipelineErrorStage::None;
    std::string error;
    const bool ok = primec::runCompilePipeline(options, output, errorStage, error, &diagnosticInfo);

    REQUIRE(ok);
    CHECK(error.empty());
    CHECK(errorStage == primec::CompilePipelineErrorStage::None);
    CHECK_FALSE(output.hasFailure);
    CHECK(output.semanticProductRequested);
    CHECK(output.semanticProductBuilt);
    REQUIRE(output.hasSemanticProgram);
    REQUIRE(output.hasDumpOutput);
    CHECK(std::find(output.program.imports.begin(),
                    output.program.imports.end(),
                    "/std/math/*") != output.program.imports.end());
    CHECK(output.program.definitions.size() >= localDefinitionCount + 1);
    CHECK(output.semanticProgram.callableSummaries.size() >= localDefinitionCount + 1);
    CHECK(output.semanticProgram.directCallTargets.size() >= localDefinitionCount);

    StressSnapshot snapshot;
    snapshot.dumpOutput = output.dumpOutput;
    snapshot.definitionCount = output.program.definitions.size();
    snapshot.callableSummaryCount = output.semanticProgram.callableSummaries.size();
    snapshot.directCallTargetCount = output.semanticProgram.directCallTargets.size();
    return snapshot;
  };

  const StressSnapshot firstRun = runStress();
  const StressSnapshot secondRun = runStress();

  std::error_code ec;
  std::filesystem::remove(tempPath, ec);

  CHECK(firstRun.dumpOutput == secondRun.dumpOutput);
  CHECK(firstRun.definitionCount == secondRun.definitionCount);
  CHECK(firstRun.callableSummaryCount == secondRun.callableSummaryCount);
  CHECK(firstRun.directCallTargetCount == secondRun.directCallTargetCount);
}

TEST_CASE("compile pipeline full semantic-product dump golden stays stable across 1,2,4 workers") {
  constexpr std::size_t localDefinitionCount = 64;
  const std::filesystem::path tempPath = makeTempIrPipelineSourcePath();
  {
    std::ofstream file(tempPath);
    REQUIRE(file.good());
    file << buildMathStressSemanticSource(localDefinitionCount);
  }

  struct StressSnapshot {
    std::string dumpOutput;
    std::size_t definitionCount = 0;
    std::size_t callableSummaryCount = 0;
    std::size_t directCallTargetCount = 0;
  };

  const auto runWithWorkerCount = [&](int workerCount) {
    primec::Options options;
    options.inputPath = tempPath.string();
    options.entryPath = "/main";
    options.emitKind = "native";
    options.dumpStage = "semantic-product";
    options.collectDiagnostics = true;
    options.benchmarkSemanticDefinitionValidationWorkerCount = workerCount;
    primec::addDefaultStdlibInclude(options.inputPath, options.importPaths);

    primec::CompilePipelineOutput output;
    primec::CompilePipelineDiagnosticInfo diagnosticInfo;
    primec::CompilePipelineErrorStage errorStage = primec::CompilePipelineErrorStage::None;
    std::string error;
    const bool ok = primec::runCompilePipeline(options, output, errorStage, error, &diagnosticInfo);

    REQUIRE(ok);
    CHECK(error.empty());
    CHECK(errorStage == primec::CompilePipelineErrorStage::None);
    CHECK_FALSE(output.hasFailure);
    CHECK(output.semanticProductRequested);
    CHECK(output.semanticProductBuilt);
    REQUIRE(output.hasSemanticProgram);
    REQUIRE(output.hasDumpOutput);
    CHECK(std::find(output.program.imports.begin(),
                    output.program.imports.end(),
                    "/std/math/*") != output.program.imports.end());
    CHECK(output.program.definitions.size() >= localDefinitionCount + 1);
    CHECK(output.semanticProgram.callableSummaries.size() >= localDefinitionCount + 1);
    CHECK(output.semanticProgram.directCallTargets.size() >= localDefinitionCount);

    StressSnapshot snapshot;
    snapshot.dumpOutput = output.dumpOutput;
    snapshot.definitionCount = output.program.definitions.size();
    snapshot.callableSummaryCount = output.semanticProgram.callableSummaries.size();
    snapshot.directCallTargetCount = output.semanticProgram.directCallTargets.size();
    return snapshot;
  };

  const StressSnapshot singleWorker = runWithWorkerCount(1);
  const StressSnapshot twoWorkers = runWithWorkerCount(2);
  const StressSnapshot fourWorkers = runWithWorkerCount(4);

  std::error_code ec;
  std::filesystem::remove(tempPath, ec);

  CHECK_FALSE(singleWorker.dumpOutput.empty());
  CHECK(singleWorker.dumpOutput.find("semantic_product {") != std::string::npos);
  CHECK(singleWorker.dumpOutput.find("/std/math/abs") != std::string::npos);
  CHECK(singleWorker.dumpOutput.find("/main") != std::string::npos);
  const std::string singleTwoDiff = describeSemanticProductDumpMismatch(
      "1-worker golden", "2-worker", singleWorker.dumpOutput, twoWorkers.dumpOutput);
  const std::string singleFourDiff = describeSemanticProductDumpMismatch(
      "1-worker golden", "4-worker", singleWorker.dumpOutput, fourWorkers.dumpOutput);
  const std::string twoFourDiff = describeSemanticProductDumpMismatch(
      "2-worker", "4-worker", twoWorkers.dumpOutput, fourWorkers.dumpOutput);
  CHECK_MESSAGE(singleTwoDiff.empty(), singleTwoDiff);
  CHECK_MESSAGE(singleFourDiff.empty(), singleFourDiff);
  CHECK_MESSAGE(twoFourDiff.empty(), twoFourDiff);
  CHECK(singleWorker.definitionCount == twoWorkers.definitionCount);
  CHECK(singleWorker.definitionCount == fourWorkers.definitionCount);
  CHECK(twoWorkers.definitionCount == fourWorkers.definitionCount);
  CHECK(singleWorker.callableSummaryCount == twoWorkers.callableSummaryCount);
  CHECK(singleWorker.callableSummaryCount == fourWorkers.callableSummaryCount);
  CHECK(twoWorkers.callableSummaryCount == fourWorkers.callableSummaryCount);
  CHECK(singleWorker.directCallTargetCount == twoWorkers.directCallTargetCount);
  CHECK(singleWorker.directCallTargetCount == fourWorkers.directCallTargetCount);
  CHECK(twoWorkers.directCallTargetCount == fourWorkers.directCallTargetCount);
}

TEST_CASE("compile pipeline benchmark worker-count stress keeps /std/math/* diagnostics deterministic") {
  constexpr std::size_t localDefinitionCount = 64;
  const std::filesystem::path tempPath = makeTempIrPipelineSourcePath();
  {
    std::ofstream file(tempPath);
    REQUIRE(file.good());
    file << buildMathStressDiagnosticSource(localDefinitionCount);
  }

  const auto runStress = [&]() {
    primec::Options options;
    options.inputPath = tempPath.string();
    options.entryPath = "/main";
    options.emitKind = "native";
    options.collectDiagnostics = true;
    options.benchmarkSemanticDefinitionValidationWorkerCount = 4;
    primec::addDefaultStdlibInclude(options.inputPath, options.importPaths);

    primec::CompilePipelineOutput output;
    primec::CompilePipelineDiagnosticInfo diagnosticInfo;
    primec::CompilePipelineErrorStage errorStage = primec::CompilePipelineErrorStage::None;
    std::string error;
    const bool ok = primec::runCompilePipeline(options, output, errorStage, error, &diagnosticInfo);

    CHECK_FALSE(ok);
    CHECK(errorStage == primec::CompilePipelineErrorStage::Semantic);
    REQUIRE(output.hasFailure);
    CHECK(output.failure.stage == primec::CompilePipelineErrorStage::Semantic);
    CHECK(output.failure.message == error);
    CHECK_FALSE(output.failure.diagnosticInfo.records.empty());
    CHECK_FALSE(diagnosticInfo.records.empty());
    return compileDiagnosticMessages(output.failure.diagnosticInfo);
  };

  const std::vector<std::string> firstRunMessages = runStress();
  const std::vector<std::string> secondRunMessages = runStress();

  std::error_code ec;
  std::filesystem::remove(tempPath, ec);

  CHECK(firstRunMessages == secondRunMessages);
  CHECK(firstRunMessages.size() >= 2);
}

TEST_CASE("compile pipeline benchmark worker-count equivalence keeps /std/math/* diagnostics stable across 1,2,4 workers") {
  constexpr std::size_t localDefinitionCount = 64;
  const std::filesystem::path tempPath = makeTempIrPipelineSourcePath();
  {
    std::ofstream file(tempPath);
    REQUIRE(file.good());
    file << buildMathStressDiagnosticSource(localDefinitionCount);
  }

  const auto runWithWorkerCount = [&](int workerCount) {
    primec::Options options;
    options.inputPath = tempPath.string();
    options.entryPath = "/main";
    options.emitKind = "native";
    options.collectDiagnostics = true;
    options.benchmarkSemanticDefinitionValidationWorkerCount = workerCount;
    primec::addDefaultStdlibInclude(options.inputPath, options.importPaths);

    primec::CompilePipelineOutput output;
    primec::CompilePipelineDiagnosticInfo diagnosticInfo;
    primec::CompilePipelineErrorStage errorStage = primec::CompilePipelineErrorStage::None;
    std::string error;
    const bool ok = primec::runCompilePipeline(options, output, errorStage, error, &diagnosticInfo);

    CHECK_FALSE(ok);
    CHECK(errorStage == primec::CompilePipelineErrorStage::Semantic);
    REQUIRE(output.hasFailure);
    CHECK(output.failure.stage == primec::CompilePipelineErrorStage::Semantic);
    CHECK(output.failure.message == error);
    CHECK_FALSE(output.failure.diagnosticInfo.records.empty());
    CHECK_FALSE(diagnosticInfo.records.empty());
    return compileDiagnosticMessages(output.failure.diagnosticInfo);
  };

  const std::vector<std::string> singleWorkerMessages = runWithWorkerCount(1);
  const std::vector<std::string> twoWorkerMessages = runWithWorkerCount(2);
  const std::vector<std::string> fourWorkerMessages = runWithWorkerCount(4);

  std::error_code ec;
  std::filesystem::remove(tempPath, ec);

  CHECK(singleWorkerMessages == twoWorkerMessages);
  CHECK(singleWorkerMessages == fourWorkerMessages);
  CHECK(twoWorkerMessages == fourWorkerMessages);
  CHECK(singleWorkerMessages.size() >= 2);
}

TEST_CASE("compile pipeline benchmark worker-count equivalence keeps semantic-product") {
  const std::filesystem::path tempPath = makeTempIrPipelineSourcePath();
  {
    std::ofstream file(tempPath);
    REQUIRE(file.good());
    file << R"(
[return<Result<int, i32>>]
id([i32] value) {
  return(Result.ok(value))
}

[return<i32>]
unexpected_error([i32] err) {
  return(err)
}

[return<i32> effects(heap_alloc) on_error<i32, /unexpected_error>]
main() {
  [auto] direct{id(1i32)}
  [i32] tried{try(direct)}
  return(tried)
}
)";
  }

  struct CallableSummarySnapshot {
    bool present = false;
    bool isExecution = false;
    std::string returnKind;
    bool isCompute = false;
    bool isUnsafe = false;
    std::vector<std::string> activeEffects;
    bool hasResultType = false;
    bool resultTypeHasValue = false;
    std::string resultValueType;
    std::string resultErrorType;
    bool hasOnError = false;
    std::string onErrorHandlerPath;
    std::string onErrorErrorType;
    std::size_t onErrorBoundArgCount = 0;
    uint64_t semanticNodeId = 0;
  };

  struct OnErrorFactSnapshot {
    bool present = false;
    std::string definitionPath;
    std::string returnKind;
    std::string handlerPath;
    std::string errorType;
    std::size_t boundArgCount = 0;
    std::vector<std::string> boundArgTexts;
    bool returnResultHasValue = false;
    std::string returnResultValueType;
    std::string returnResultErrorType;
    uint64_t semanticNodeId = 0;
  };

  struct IndexFamilySnapshot {
    std::string formattedSemanticProduct;
    std::vector<std::string> callTargetStringTable;
    std::size_t directCallTargetCount = 0;
    std::size_t methodCallTargetCount = 0;
    std::size_t bridgePathChoiceCount = 0;
    std::size_t bindingFactCount = 0;
    std::size_t localAutoFactCount = 0;
    std::size_t queryFactCount = 0;
    std::size_t tryFactCount = 0;
    std::size_t onErrorFactCount = 0;
    std::size_t returnFactCount = 0;
    CallableSummarySnapshot idCallableSummary;
    CallableSummarySnapshot mainCallableSummary;
    OnErrorFactSnapshot mainOnErrorFact;
  };

  const auto captureCallableSummary =
      [](const primec::SemanticProgram &semanticProgram, std::string_view fullPath) {
        CallableSummarySnapshot snapshot;
        const auto *entry =
            primec::semanticProgramLookupPublishedCallableSummary(semanticProgram, fullPath);
        if (entry == nullptr) {
          return snapshot;
        }

        snapshot.present = true;
        snapshot.isExecution = entry->isExecution;
        snapshot.returnKind = entry->returnKind;
        snapshot.isCompute = entry->isCompute;
        snapshot.isUnsafe = entry->isUnsafe;
        snapshot.activeEffects = entry->activeEffects;
        snapshot.hasResultType = entry->hasResultType;
        snapshot.resultTypeHasValue = entry->resultTypeHasValue;
        snapshot.resultValueType = entry->resultValueType;
        snapshot.resultErrorType = entry->resultErrorType;
        snapshot.hasOnError = entry->hasOnError;
        snapshot.onErrorHandlerPath = entry->onErrorHandlerPath;
        snapshot.onErrorErrorType = entry->onErrorErrorType;
        snapshot.onErrorBoundArgCount = entry->onErrorBoundArgCount;
        snapshot.semanticNodeId = entry->semanticNodeId;
        return snapshot;
      };

  const auto runWithWorkerCount = [&](int workerCount) {
    primec::Options options;
    options.inputPath = tempPath.string();
    options.entryPath = "/main";
    options.emitKind = "native";
    options.collectDiagnostics = true;
    options.benchmarkSemanticDefinitionValidationWorkerCount = workerCount;
    primec::addDefaultStdlibInclude(options.inputPath, options.importPaths);

    primec::CompilePipelineOutput output;
    primec::CompilePipelineDiagnosticInfo diagnosticInfo;
    primec::CompilePipelineErrorStage errorStage = primec::CompilePipelineErrorStage::None;
    std::string error;
    const bool ok = primec::runCompilePipeline(options, output, errorStage, error, &diagnosticInfo);

    REQUIRE(ok);
    CHECK(error.empty());
    CHECK(errorStage == primec::CompilePipelineErrorStage::None);
    CHECK_FALSE(output.hasFailure);
    REQUIRE(output.hasSemanticProgram);

    IndexFamilySnapshot snapshot;
    snapshot.formattedSemanticProduct = primec::formatSemanticProgram(output.semanticProgram);
    snapshot.callTargetStringTable = output.semanticProgram.callTargetStringTable;
    snapshot.directCallTargetCount = output.semanticProgram.directCallTargets.size();
    snapshot.methodCallTargetCount = output.semanticProgram.methodCallTargets.size();
    snapshot.bridgePathChoiceCount = output.semanticProgram.bridgePathChoices.size();
    snapshot.bindingFactCount = output.semanticProgram.bindingFacts.size();
    snapshot.localAutoFactCount = output.semanticProgram.localAutoFacts.size();
    snapshot.queryFactCount = output.semanticProgram.queryFacts.size();
    snapshot.tryFactCount = output.semanticProgram.tryFacts.size();
    snapshot.onErrorFactCount = output.semanticProgram.onErrorFacts.size();
    snapshot.returnFactCount = output.semanticProgram.returnFacts.size();
    snapshot.idCallableSummary =
        captureCallableSummary(output.semanticProgram, "/id");
    snapshot.mainCallableSummary =
        captureCallableSummary(output.semanticProgram, "/main");
    if (const auto *entry = findSemanticEntry(
            primec::semanticProgramOnErrorFactView(output.semanticProgram),
            [&output](const primec::SemanticProgramOnErrorFact &candidate) {
              return primec::semanticProgramOnErrorFactDefinitionPath(
                         output.semanticProgram, candidate) == "/main";
            });
        entry != nullptr) {
      snapshot.mainOnErrorFact.present = true;
      snapshot.mainOnErrorFact.definitionPath = std::string(
          primec::semanticProgramOnErrorFactDefinitionPath(
              output.semanticProgram, *entry));
      snapshot.mainOnErrorFact.returnKind = entry->returnKind;
      snapshot.mainOnErrorFact.handlerPath = std::string(
          primec::semanticProgramOnErrorFactHandlerPath(
              output.semanticProgram, *entry));
      snapshot.mainOnErrorFact.errorType = entry->errorType;
      snapshot.mainOnErrorFact.boundArgCount = entry->boundArgCount;
      snapshot.mainOnErrorFact.boundArgTexts = entry->boundArgTexts;
      snapshot.mainOnErrorFact.returnResultHasValue =
          entry->returnResultHasValue;
      snapshot.mainOnErrorFact.returnResultValueType =
          entry->returnResultValueType;
      snapshot.mainOnErrorFact.returnResultErrorType =
          entry->returnResultErrorType;
      snapshot.mainOnErrorFact.semanticNodeId = entry->semanticNodeId;
    }
    return snapshot;
  };

  const IndexFamilySnapshot singleWorker = runWithWorkerCount(1);
  const IndexFamilySnapshot twoWorkers = runWithWorkerCount(2);
  const IndexFamilySnapshot fourWorkers = runWithWorkerCount(4);

  std::error_code ec;
  std::filesystem::remove(tempPath, ec);

  CHECK_FALSE(singleWorker.formattedSemanticProduct.empty());
  CHECK_FALSE(twoWorkers.formattedSemanticProduct.empty());
  CHECK_FALSE(fourWorkers.formattedSemanticProduct.empty());
  CHECK(singleWorker.formattedSemanticProduct.find("/id") != std::string::npos);
  CHECK(singleWorker.formattedSemanticProduct.find("/main") != std::string::npos);
  CHECK(singleWorker.formattedSemanticProduct.find("/unexpected_error") != std::string::npos);
  CHECK(twoWorkers.formattedSemanticProduct.find("/id") != std::string::npos);
  CHECK(twoWorkers.formattedSemanticProduct.find("/main") != std::string::npos);
  CHECK(twoWorkers.formattedSemanticProduct.find("/unexpected_error") != std::string::npos);
  CHECK(fourWorkers.formattedSemanticProduct.find("/id") != std::string::npos);
  CHECK(fourWorkers.formattedSemanticProduct.find("/main") != std::string::npos);
  CHECK(fourWorkers.formattedSemanticProduct.find("/unexpected_error") != std::string::npos);
  CHECK_FALSE(singleWorker.callTargetStringTable.empty());
  CHECK_FALSE(twoWorkers.callTargetStringTable.empty());
  CHECK_FALSE(fourWorkers.callTargetStringTable.empty());

  CHECK(singleWorker.directCallTargetCount == twoWorkers.directCallTargetCount);
  CHECK(singleWorker.directCallTargetCount == fourWorkers.directCallTargetCount);
  CHECK(singleWorker.methodCallTargetCount == twoWorkers.methodCallTargetCount);
  CHECK(singleWorker.methodCallTargetCount == fourWorkers.methodCallTargetCount);
  CHECK(singleWorker.bridgePathChoiceCount == twoWorkers.bridgePathChoiceCount);
  CHECK(singleWorker.bridgePathChoiceCount == fourWorkers.bridgePathChoiceCount);
  CHECK(singleWorker.bindingFactCount == twoWorkers.bindingFactCount);
  CHECK(singleWorker.bindingFactCount == fourWorkers.bindingFactCount);
  CHECK(singleWorker.localAutoFactCount == twoWorkers.localAutoFactCount);
  CHECK(singleWorker.localAutoFactCount == fourWorkers.localAutoFactCount);
  CHECK(singleWorker.queryFactCount == twoWorkers.queryFactCount);
  CHECK(singleWorker.queryFactCount == fourWorkers.queryFactCount);
  CHECK(singleWorker.tryFactCount == twoWorkers.tryFactCount);
  CHECK(singleWorker.tryFactCount == fourWorkers.tryFactCount);
  CHECK(singleWorker.onErrorFactCount == twoWorkers.onErrorFactCount);
  CHECK(singleWorker.onErrorFactCount == fourWorkers.onErrorFactCount);
  CHECK(singleWorker.returnFactCount == twoWorkers.returnFactCount);
  CHECK(singleWorker.returnFactCount == fourWorkers.returnFactCount);

  CHECK(singleWorker.idCallableSummary.present);
  CHECK(twoWorkers.idCallableSummary.present);
  CHECK(fourWorkers.idCallableSummary.present);
  CHECK(singleWorker.idCallableSummary.isExecution == twoWorkers.idCallableSummary.isExecution);
  CHECK(singleWorker.idCallableSummary.isExecution == fourWorkers.idCallableSummary.isExecution);
  CHECK(singleWorker.idCallableSummary.returnKind == twoWorkers.idCallableSummary.returnKind);
  CHECK(singleWorker.idCallableSummary.returnKind == fourWorkers.idCallableSummary.returnKind);
  CHECK(singleWorker.idCallableSummary.hasResultType == twoWorkers.idCallableSummary.hasResultType);
  CHECK(singleWorker.idCallableSummary.hasResultType == fourWorkers.idCallableSummary.hasResultType);
  CHECK(singleWorker.idCallableSummary.resultTypeHasValue ==
        twoWorkers.idCallableSummary.resultTypeHasValue);
  CHECK(singleWorker.idCallableSummary.resultTypeHasValue ==
        fourWorkers.idCallableSummary.resultTypeHasValue);
  CHECK(singleWorker.idCallableSummary.resultValueType == twoWorkers.idCallableSummary.resultValueType);
  CHECK(singleWorker.idCallableSummary.resultValueType == fourWorkers.idCallableSummary.resultValueType);
  CHECK(singleWorker.idCallableSummary.resultErrorType == twoWorkers.idCallableSummary.resultErrorType);
  CHECK(singleWorker.idCallableSummary.resultErrorType == fourWorkers.idCallableSummary.resultErrorType);
  CHECK(singleWorker.idCallableSummary.semanticNodeId == twoWorkers.idCallableSummary.semanticNodeId);
  CHECK(singleWorker.idCallableSummary.semanticNodeId == fourWorkers.idCallableSummary.semanticNodeId);
  CHECK(singleWorker.idCallableSummary.semanticNodeId > 0);

  CHECK(singleWorker.mainCallableSummary.present);
  CHECK(twoWorkers.mainCallableSummary.present);
  CHECK(fourWorkers.mainCallableSummary.present);
  CHECK(singleWorker.mainCallableSummary.isExecution == twoWorkers.mainCallableSummary.isExecution);
  CHECK(singleWorker.mainCallableSummary.isExecution == fourWorkers.mainCallableSummary.isExecution);
  CHECK(singleWorker.mainCallableSummary.returnKind == twoWorkers.mainCallableSummary.returnKind);
  CHECK(singleWorker.mainCallableSummary.returnKind == fourWorkers.mainCallableSummary.returnKind);
  CHECK(singleWorker.mainCallableSummary.isCompute == twoWorkers.mainCallableSummary.isCompute);
  CHECK(singleWorker.mainCallableSummary.isCompute == fourWorkers.mainCallableSummary.isCompute);
  CHECK(singleWorker.mainCallableSummary.isUnsafe == twoWorkers.mainCallableSummary.isUnsafe);
  CHECK(singleWorker.mainCallableSummary.isUnsafe == fourWorkers.mainCallableSummary.isUnsafe);
  CHECK(singleWorker.mainCallableSummary.activeEffects == twoWorkers.mainCallableSummary.activeEffects);
  CHECK(singleWorker.mainCallableSummary.activeEffects == fourWorkers.mainCallableSummary.activeEffects);
  CHECK(singleWorker.mainCallableSummary.hasOnError == twoWorkers.mainCallableSummary.hasOnError);
  CHECK(singleWorker.mainCallableSummary.hasOnError == fourWorkers.mainCallableSummary.hasOnError);
  CHECK(singleWorker.mainCallableSummary.onErrorHandlerPath ==
        twoWorkers.mainCallableSummary.onErrorHandlerPath);
  CHECK(singleWorker.mainCallableSummary.onErrorHandlerPath ==
        fourWorkers.mainCallableSummary.onErrorHandlerPath);
  CHECK(singleWorker.mainCallableSummary.onErrorErrorType ==
        twoWorkers.mainCallableSummary.onErrorErrorType);
  CHECK(singleWorker.mainCallableSummary.onErrorErrorType ==
        fourWorkers.mainCallableSummary.onErrorErrorType);
  CHECK(singleWorker.mainCallableSummary.onErrorBoundArgCount ==
        twoWorkers.mainCallableSummary.onErrorBoundArgCount);
  CHECK(singleWorker.mainCallableSummary.onErrorBoundArgCount ==
        fourWorkers.mainCallableSummary.onErrorBoundArgCount);
  CHECK(singleWorker.mainCallableSummary.semanticNodeId ==
        twoWorkers.mainCallableSummary.semanticNodeId);
  CHECK(singleWorker.mainCallableSummary.semanticNodeId ==
        fourWorkers.mainCallableSummary.semanticNodeId);
  CHECK(singleWorker.mainCallableSummary.semanticNodeId > 0);

  CHECK(singleWorker.mainOnErrorFact.present);
  CHECK(twoWorkers.mainOnErrorFact.present);
  CHECK(fourWorkers.mainOnErrorFact.present);
  CHECK(singleWorker.mainOnErrorFact.definitionPath ==
        twoWorkers.mainOnErrorFact.definitionPath);
  CHECK(singleWorker.mainOnErrorFact.definitionPath ==
        fourWorkers.mainOnErrorFact.definitionPath);
  CHECK(singleWorker.mainOnErrorFact.returnKind ==
        twoWorkers.mainOnErrorFact.returnKind);
  CHECK(singleWorker.mainOnErrorFact.returnKind ==
        fourWorkers.mainOnErrorFact.returnKind);
  CHECK(singleWorker.mainOnErrorFact.handlerPath ==
        twoWorkers.mainOnErrorFact.handlerPath);
  CHECK(singleWorker.mainOnErrorFact.handlerPath ==
        fourWorkers.mainOnErrorFact.handlerPath);
  CHECK(singleWorker.mainOnErrorFact.errorType ==
        twoWorkers.mainOnErrorFact.errorType);
  CHECK(singleWorker.mainOnErrorFact.errorType ==
        fourWorkers.mainOnErrorFact.errorType);
  CHECK(singleWorker.mainOnErrorFact.boundArgCount ==
        twoWorkers.mainOnErrorFact.boundArgCount);
  CHECK(singleWorker.mainOnErrorFact.boundArgCount ==
        fourWorkers.mainOnErrorFact.boundArgCount);
  CHECK(singleWorker.mainOnErrorFact.boundArgTexts ==
        twoWorkers.mainOnErrorFact.boundArgTexts);
  CHECK(singleWorker.mainOnErrorFact.boundArgTexts ==
        fourWorkers.mainOnErrorFact.boundArgTexts);
  CHECK(singleWorker.mainOnErrorFact.returnResultHasValue ==
        twoWorkers.mainOnErrorFact.returnResultHasValue);
  CHECK(singleWorker.mainOnErrorFact.returnResultHasValue ==
        fourWorkers.mainOnErrorFact.returnResultHasValue);
  CHECK(singleWorker.mainOnErrorFact.returnResultValueType ==
        twoWorkers.mainOnErrorFact.returnResultValueType);
  CHECK(singleWorker.mainOnErrorFact.returnResultValueType ==
        fourWorkers.mainOnErrorFact.returnResultValueType);
  CHECK(singleWorker.mainOnErrorFact.returnResultErrorType ==
        twoWorkers.mainOnErrorFact.returnResultErrorType);
  CHECK(singleWorker.mainOnErrorFact.returnResultErrorType ==
        fourWorkers.mainOnErrorFact.returnResultErrorType);
  CHECK(singleWorker.directCallTargetCount > 0);
  CHECK(singleWorker.methodCallTargetCount > 0);
  CHECK(singleWorker.bridgePathChoiceCount == 0);
  CHECK(singleWorker.bindingFactCount > 0);
  CHECK(singleWorker.localAutoFactCount > 0);
  CHECK(singleWorker.queryFactCount > 0);
  CHECK(singleWorker.tryFactCount > 0);
  CHECK(singleWorker.onErrorFactCount > 0);
  CHECK(singleWorker.returnFactCount > 0);
  CHECK_FALSE(singleWorker.idCallableSummary.isExecution);
  CHECK(singleWorker.idCallableSummary.returnKind == "i64");
  CHECK(singleWorker.idCallableSummary.hasResultType);
  CHECK(singleWorker.idCallableSummary.resultTypeHasValue);
  CHECK(singleWorker.idCallableSummary.resultValueType == "int");
  CHECK(singleWorker.idCallableSummary.resultErrorType == "i32");
  CHECK_FALSE(singleWorker.mainCallableSummary.isExecution);
  CHECK(singleWorker.mainCallableSummary.returnKind == "i32");
  CHECK(singleWorker.mainCallableSummary.activeEffects ==
        std::vector<std::string>{"heap_alloc"});
  CHECK(singleWorker.mainCallableSummary.hasOnError);
  CHECK(singleWorker.mainCallableSummary.onErrorHandlerPath == "/unexpected_error");
  CHECK(singleWorker.mainCallableSummary.onErrorErrorType == "i32");
  CHECK(singleWorker.mainCallableSummary.onErrorBoundArgCount == 0);
  CHECK(singleWorker.mainOnErrorFact.definitionPath == "/main");
  CHECK(singleWorker.mainOnErrorFact.returnKind == "i32");
  CHECK(singleWorker.mainOnErrorFact.handlerPath == "/unexpected_error");
  CHECK(singleWorker.mainOnErrorFact.errorType == "i32");
  CHECK(singleWorker.mainOnErrorFact.boundArgCount == 0);
  CHECK(singleWorker.mainOnErrorFact.boundArgTexts.empty());
  CHECK_FALSE(singleWorker.mainOnErrorFact.returnResultHasValue);
  CHECK(singleWorker.mainOnErrorFact.returnResultValueType.empty());
  CHECK(singleWorker.mainOnErrorFact.returnResultErrorType.empty());
}

TEST_CASE("compile pipeline graph-local-auto benchmark shadows preserve published local-auto facts") {
  const std::filesystem::path tempPath = makeTempIrPipelineSourcePath();
  {
    std::ofstream file(tempPath);
    REQUIRE(file.good());
    file << R"(
import /std/file/*

[return<Result<i32, FileError>>]
lookup([i32] input) {
  return(Result.ok(plus(input, 1i32)))
}

[effects(io_err)]
log_file_error([FileError] err) {
  print_line_error(err.why())
}

[return<i32> effects(io_out, io_err) on_error<FileError, /log_file_error>]
main() {
  [auto] initial{lookup(40i32)}
  [auto] mapped{Result.map(initial, []([i32] value) { return(plus(value, 1i32)) })}
  [i32] selected{try(mapped)}
  return(selected)
}
)";
  }

  struct LocalAutoPublicationSnapshot {
    std::string bindingName;
    std::string bindingTypeText;
    std::string initializerResolvedPath;
    std::string initializerBindingTypeText;
    std::string initializerResultValueType;
    std::string initializerResultErrorType;
    bool initializerResultHasValue = false;
    bool initializerHasTry = false;
    uint64_t semanticNodeId = 0;

    bool operator==(const LocalAutoPublicationSnapshot &) const = default;
  };

  struct GraphLocalAutoShadowSnapshot {
    std::string formattedSemanticProduct;
    std::vector<LocalAutoPublicationSnapshot> localAutos;
  };

  const auto runWithShadowOptions = [&](bool legacyKeyShadow,
                                        bool legacySideChannelShadow,
                                        bool disableDependencyScratchPmr) {
    primec::Options options;
    options.inputPath = tempPath.string();
    options.entryPath = "/main";
    options.emitKind = "native";
    options.collectDiagnostics = true;
    options.benchmarkSemanticGraphLocalAutoLegacyKeyShadow = legacyKeyShadow;
    options.benchmarkSemanticGraphLocalAutoLegacySideChannelShadow =
        legacySideChannelShadow;
    options.benchmarkSemanticDisableGraphLocalAutoDependencyScratchPmr =
        disableDependencyScratchPmr;
    primec::addDefaultStdlibInclude(options.inputPath, options.importPaths);

    primec::CompilePipelineOutput output;
    primec::CompilePipelineDiagnosticInfo diagnosticInfo;
    primec::CompilePipelineErrorStage errorStage =
        primec::CompilePipelineErrorStage::None;
    std::string error;
    const bool ok =
        primec::runCompilePipeline(options, output, errorStage, error, &diagnosticInfo);

    REQUIRE(ok);
    CHECK(error.empty());
    CHECK(errorStage == primec::CompilePipelineErrorStage::None);
    CHECK_FALSE(output.hasFailure);
    REQUIRE(output.hasSemanticProgram);
    CHECK(diagnosticInfo.records.empty());

    GraphLocalAutoShadowSnapshot snapshot;
    snapshot.formattedSemanticProduct =
        primec::formatSemanticProgram(output.semanticProgram);

    for (const primec::SemanticProgramLocalAutoFact *entry :
         primec::semanticProgramLocalAutoFactView(output.semanticProgram)) {
      REQUIRE(entry != nullptr);
      if (entry->scopePath != "/main") {
        continue;
      }

      const std::string_view initializerResolvedPath =
          primec::semanticProgramLocalAutoFactInitializerResolvedPath(
              output.semanticProgram, *entry);
      const auto *bySemanticId =
          primec::semanticProgramLookupPublishedLocalAutoFactBySemanticId(
              output.semanticProgram, entry->semanticNodeId);
      REQUIRE(bySemanticId != nullptr);
      CHECK(bySemanticId->bindingName == entry->bindingName);
      CHECK(primec::semanticProgramLocalAutoFactInitializerResolvedPath(
                output.semanticProgram, *bySemanticId) == initializerResolvedPath);

      const auto initializerPathId =
          primec::semanticProgramLookupCallTargetStringId(output.semanticProgram,
                                                          initializerResolvedPath);
      const auto bindingNameId =
          primec::semanticProgramLookupCallTargetStringId(output.semanticProgram,
                                                          entry->bindingName);
      REQUIRE(initializerPathId.has_value());
      REQUIRE(bindingNameId.has_value());
      const auto *byInitPathAndName =
          primec::semanticProgramLookupPublishedLocalAutoFactByInitializerPathAndBindingNameId(
              output.semanticProgram, *initializerPathId, *bindingNameId);
      REQUIRE(byInitPathAndName != nullptr);
      CHECK(byInitPathAndName->semanticNodeId == entry->semanticNodeId);

      snapshot.localAutos.push_back(LocalAutoPublicationSnapshot{
          .bindingName = entry->bindingName,
          .bindingTypeText = entry->bindingTypeText,
          .initializerResolvedPath = std::string(initializerResolvedPath),
          .initializerBindingTypeText = entry->initializerBindingTypeText,
          .initializerResultValueType = entry->initializerResultValueType,
          .initializerResultErrorType = entry->initializerResultErrorType,
          .initializerResultHasValue = entry->initializerResultHasValue,
          .initializerHasTry = entry->initializerHasTry,
          .semanticNodeId = entry->semanticNodeId,
      });
    }

    std::sort(snapshot.localAutos.begin(),
              snapshot.localAutos.end(),
              [](const LocalAutoPublicationSnapshot &left,
                 const LocalAutoPublicationSnapshot &right) {
                return std::pair(left.bindingName, left.semanticNodeId) <
                       std::pair(right.bindingName, right.semanticNodeId);
              });
    return snapshot;
  };

  const GraphLocalAutoShadowSnapshot baseline =
      runWithShadowOptions(false, false, false);
  const GraphLocalAutoShadowSnapshot legacyKeyShadow =
      runWithShadowOptions(true, false, false);
  const GraphLocalAutoShadowSnapshot legacySideChannelShadow =
      runWithShadowOptions(false, true, false);
  const GraphLocalAutoShadowSnapshot dependencyScratchDisabled =
      runWithShadowOptions(false, false, true);
  const GraphLocalAutoShadowSnapshot combinedShadows =
      runWithShadowOptions(true, true, true);

  std::error_code ec;
  std::filesystem::remove(tempPath, ec);

  REQUIRE(baseline.localAutos.size() >= 2);
  CHECK(baseline.formattedSemanticProduct ==
        legacyKeyShadow.formattedSemanticProduct);
  CHECK(baseline.formattedSemanticProduct ==
        legacySideChannelShadow.formattedSemanticProduct);
  CHECK(baseline.formattedSemanticProduct ==
        dependencyScratchDisabled.formattedSemanticProduct);
  CHECK(baseline.formattedSemanticProduct ==
        combinedShadows.formattedSemanticProduct);
  CHECK(baseline.localAutos == legacyKeyShadow.localAutos);
  CHECK(baseline.localAutos == legacySideChannelShadow.localAutos);
  CHECK(baseline.localAutos == dependencyScratchDisabled.localAutos);
  CHECK(baseline.localAutos == combinedShadows.localAutos);

  const auto baselineInitial = std::find_if(
      baseline.localAutos.begin(),
      baseline.localAutos.end(),
      [](const LocalAutoPublicationSnapshot &entry) {
        return entry.bindingName == "initial";
      });
  const auto baselineMapped = std::find_if(
      baseline.localAutos.begin(),
      baseline.localAutos.end(),
      [](const LocalAutoPublicationSnapshot &entry) {
        return entry.bindingName == "mapped";
      });
  REQUIRE(baselineInitial != baseline.localAutos.end());
  REQUIRE(baselineMapped != baseline.localAutos.end());
  CHECK(baselineInitial->initializerResolvedPath == "/lookup");
  CHECK(baselineInitial->bindingTypeText == "Result<i32, FileError>");
  CHECK(baselineInitial->initializerResultHasValue);
  CHECK(baselineInitial->initializerResultValueType == "i32");
  CHECK(baselineInitial->initializerResultErrorType == "FileError");
  CHECK_FALSE(baselineInitial->initializerHasTry);
  CHECK(baselineMapped->bindingTypeText == "Result<i32, FileError>");
  CHECK_FALSE(baselineMapped->initializerResolvedPath.empty());
  CHECK_FALSE(baselineMapped->initializerHasTry);
}

TEST_CASE("lowering variadic reference-pack diagnostic exposes stable public payload") {
  const std::filesystem::path tempPath = makeTempIrPipelineSourcePath();
  {
    std::ofstream file(tempPath);
    REQUIRE(file.good());
    file << R"([return<i32>]
score([args<Reference<i32>>] values) {
  return(count(values))
}

[return<i32>]
main() {
  return(score(1i32, 2i32))
}
)";
  }

  primec::Options options;
  options.inputPath = tempPath.string();
  options.entryPath = "/main";
  options.emitKind = "vm";
  options.collectDiagnostics = true;
  primec::addDefaultStdlibInclude(options.inputPath, options.importPaths);

  primec::CompilePipelineOutput output;
  primec::CompilePipelineDiagnosticInfo diagnosticInfo;
  primec::CompilePipelineErrorStage errorStage =
      primec::CompilePipelineErrorStage::None;
  std::string error;
  REQUIRE(primec::runCompilePipeline(options, output, errorStage, error, &diagnosticInfo));
  CHECK(error.empty());
  REQUIRE(output.hasSemanticProgram);

  const primec::IrBackend *vmBackend = primec::findIrBackend("vm");
  REQUIRE(vmBackend != nullptr);

  primec::IrModule ir;
  primec::IrPreparationFailure failure;
  CHECK_FALSE(primec::prepareIrModule(output.program,
                                      &output.semanticProgram,
                                      options,
                                      vmBackend->validationTarget(options),
                                      ir,
                                      failure,
                                      &output.expandedSource));
  CHECK(failure.stage == primec::IrPreparationFailureStage::Lowering);
  CHECK(failure.message ==
        std::string(primec::VariadicArgsReferenceForwardingDiagnosticMessage));
  CHECK(failure.diagnosticInfo.message == failure.message);
  REQUIRE(failure.diagnosticInfo.hasPrimarySpan);
  CHECK(failure.diagnosticInfo.primarySpan.file ==
        std::filesystem::absolute(tempPath).string());
  CHECK(failure.diagnosticInfo.primarySpan.line == 8);
  CHECK(failure.diagnosticInfo.primarySpan.column == 16);
  CHECK(failure.diagnosticInfo.primarySpan.endLine == 8);
  CHECK(failure.diagnosticInfo.primarySpan.endColumn == 16);
  CHECK(failure.diagnosticInfo.relatedSpans.empty());

  const primec::CliFailure cliFailure =
      primec::describeIrPreparationFailure(failure, *vmBackend);
  CHECK(cliFailure.code == primec::DiagnosticCode::LoweringError);
  CHECK(cliFailure.notes == std::vector<std::string>{"backend: vm"});
  REQUIRE(cliFailure.diagnosticInfo.has_value());

  const primec::DiagnosticStabilityContract contract =
      primec::diagnosticStabilityContract(cliFailure.code, cliFailure.message);
  CHECK(contract.message == primec::DiagnosticStabilityTier::Stable);
  CHECK(contract.primarySpan == primec::DiagnosticStabilityTier::Stable);
  CHECK(contract.notes == primec::DiagnosticStabilityTier::Stable);

  std::ostringstream err;
  options.emitDiagnostics = true;
  CHECK(primec::emitCliFailure(err, options, cliFailure) == 2);
  const std::string publicJson = err.str();
  CHECK(publicJson.find("\"code\":\"PSC2001\"") != std::string::npos);
  CHECK(publicJson.find("\"message\":\"" +
                        std::string(primec::VariadicArgsReferenceForwardingDiagnosticMessage) +
                        "\"") != std::string::npos);
  CHECK(publicJson.find("\"line\":8") != std::string::npos);
  CHECK(publicJson.find("\"column\":16") != std::string::npos);
  CHECK(publicJson.find("\"notes\":[\"backend: vm\"]") != std::string::npos);

  std::error_code ec;
  std::filesystem::remove(tempPath, ec);
}

TEST_CASE("cli driver maps ir preparation failures through backend diagnostics") {
  const primec::IrBackend *vmBackend = primec::findIrBackend("vm");
  REQUIRE(vmBackend != nullptr);

  primec::IrPreparationFailure loweringFailure;
  loweringFailure.stage = primec::IrPreparationFailureStage::Lowering;
  loweringFailure.message = "native backend rejected entry";

  const primec::CliFailure loweringCliFailure = primec::describeIrPreparationFailure(loweringFailure, *vmBackend, 7);
  CHECK(loweringCliFailure.code == vmBackend->diagnostics().loweringDiagnosticCode);
  CHECK(loweringCliFailure.plainPrefix == vmBackend->diagnostics().loweringErrorPrefix);
  CHECK(loweringCliFailure.exitCode == 7);
  CHECK(loweringCliFailure.notes == std::vector<std::string>{"backend: vm"});
  CHECK(loweringCliFailure.message.find("vm backend") != std::string::npos);
  CHECK(loweringCliFailure.message.find("native backend") == std::string::npos);

  primec::IrPreparationFailure validationFailure;
  validationFailure.stage = primec::IrPreparationFailureStage::Validation;
  validationFailure.message = "bad ir";

  const primec::CliFailure validationCliFailure = primec::describeIrPreparationFailure(validationFailure, *vmBackend);
  CHECK(validationCliFailure.code == vmBackend->diagnostics().validationDiagnosticCode);
  CHECK(validationCliFailure.plainPrefix == vmBackend->diagnostics().validationErrorPrefix);
  CHECK(validationCliFailure.notes == std::vector<std::string>({"backend: vm", "stage: ir-validate"}));
  CHECK(validationCliFailure.message == "bad ir");
}

TEST_CASE("shared vm backend profile exposes canonical diagnostics") {
  const primec::IrBackendDiagnostics &diagnostics = primec::vmIrBackendDiagnostics();
  CHECK(diagnostics.loweringDiagnosticCode == primec::DiagnosticCode::LoweringError);
  CHECK(std::string_view(diagnostics.emitErrorPrefix) == "VM error: ");
  CHECK(std::string_view(diagnostics.backendTag) == "vm");

  primec::IrPreparationFailure loweringFailure;
  loweringFailure.stage = primec::IrPreparationFailureStage::Lowering;
  loweringFailure.message = "native backend rejected entry";
  const primec::CliFailure cliFailure =
      primec::describeIrPreparationFailure(loweringFailure, diagnostics, &primec::normalizeVmLoweringError, 5);
  CHECK(cliFailure.code == primec::DiagnosticCode::LoweringError);
  CHECK(cliFailure.exitCode == 5);
  CHECK(cliFailure.message.find("vm backend") != std::string::npos);

  std::string loweringError = "native backend rejected entry";
  primec::normalizeVmLoweringError(loweringError);
  CHECK(loweringError.find("vm backend") != std::string::npos);
  CHECK(loweringError.find("native backend") == std::string::npos);
}

TEST_CASE("main routes cpp and exe through ir backend alias lookup") {
  const std::filesystem::path cwd = std::filesystem::current_path();
  std::filesystem::path mainPath = cwd / "src" / "main.cpp";
  if (!std::filesystem::exists(mainPath)) {
    mainPath = cwd.parent_path() / "src" / "main.cpp";
  }
  REQUIRE(std::filesystem::exists(mainPath));

  const std::string source = readTextFile(mainPath);
  CHECK(source.find("resolveIrBackendEmitKind(options.emitKind)") != std::string::npos);
  CHECK(source.find("findIrBackend(irBackendKind)") != std::string::npos);
  CHECK(source.find("if (irBackend == nullptr && options.dumpStage.empty())") != std::string::npos);
  CHECK(source.find("options.skipSemanticProductForNonConsumingPath = true;") != std::string::npos);
  CHECK(source.find("runCompilePipelineResult(options, runError, runErrorText)") != std::string::npos);
  CHECK(source.find("std::get_if<primec::CompilePipelineFailureResult>(&runResult)") !=
        std::string::npos);
  CHECK(source.find("describeCompilePipelineFailure(*pipelineFailure)") != std::string::npos);
  CHECK(source.find("std::get<primec::CompilePipelineSuccessResult>(runResult).output") !=
        std::string::npos);
  CHECK(source.find("runCompilePipeline(options, runOutput") == std::string::npos);
  CHECK(source.find("describeCompilePipelineFailure(pipelineOutput)") == std::string::npos);
  CHECK(source.find("describeIrPreparationFailure(") != std::string::npos);
  CHECK(source.find("pipelineOutput.hasSemanticProgram ? &pipelineOutput.semanticProgram : nullptr") !=
        std::string::npos);
  CHECK(source.find("prepareIrModule(program, semanticProgram, options, validationTarget, ir, prepFailure, expandedSource)") !=
        std::string::npos);
  CHECK(source.find("IrLowerer lowerer") == std::string::npos);
  CHECK(source.find("inlineIrModuleCalls(ir, error)") == std::string::npos);
  CHECK(source.find("validateIrModule(ir, validationTarget, error)") == std::string::npos);
  CHECK(source.find("if (options.emitKind == \"cpp\" || options.emitKind == \"exe\")") == std::string::npos);
  CHECK(source.find("isProductionCppOrExeEmitKind(") == std::string::npos);
  CHECK(source.find("CompilePipelineErrorStage::Import") == std::string::npos);
  CHECK(source.find("IrPreparationFailureStage::Validation") == std::string::npos);
  CHECK(source.find("scanSoftwareNumericTypes(") == std::string::npos);
  CHECK(source.find("software numeric types are not supported:") == std::string::npos);
  CHECK(source.find("emitter.emitCpp(program, options.entryPath)") == std::string::npos);
  CHECK(source.find("compileCppExecutable(") == std::string::npos);
  CHECK(source.find("#include \"primec/Emitter.h\"") == std::string::npos);
}
