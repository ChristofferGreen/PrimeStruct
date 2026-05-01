#include "third_party/doctest.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#include "primec/AstMemory.h"
#include "primec/CliDriver.h"
#include "primec/EmitKind.h"
#include "primec/IrBackends.h"
#include "primec/IrBackendProfiles.h"
#include "primec/IrLowerer.h"
#include "primec/IrPreparation.h"
#include "primec/SemanticValidationPlan.h"
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
}

void addVoidCallableSummary(primec::SemanticProgram &semanticProgram, uint64_t semanticNodeId) {
  addVoidCallableSummaryForPath(semanticProgram, "/main", semanticNodeId);
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

TEST_CASE("ir backend registry reports deterministic order and lookup") {
  const std::vector<std::string_view> expectedKinds = {
      "vm", "native", "ir", "wasm", "glsl-ir", "spirv-ir", "cpp-ir", "exe-ir"};
  CHECK(primec::listIrBackendKinds() == expectedKinds);

  for (std::string_view kind : expectedKinds) {
    const primec::IrBackend *backend = primec::findIrBackend(kind);
    REQUIRE(backend != nullptr);
    CHECK(backend->emitKind() == kind);
    CHECK(std::string_view(backend->diagnostics().backendTag) == kind);
  }

  CHECK(primec::findIrBackend("cpp") == nullptr);
  CHECK(primec::findIrBackend("glsl") == nullptr);
}

TEST_CASE("emit kind aliases resolve to canonical ir backend kinds") {
  CHECK(primec::resolveIrBackendEmitKind("cpp") == "cpp-ir");
  CHECK(primec::resolveIrBackendEmitKind("exe") == "exe-ir");
  CHECK(primec::resolveIrBackendEmitKind("glsl") == "glsl-ir");
  CHECK(primec::resolveIrBackendEmitKind("spirv") == "spirv-ir");
  CHECK(primec::resolveIrBackendEmitKind("cpp-ir") == "cpp-ir");
  CHECK(primec::resolveIrBackendEmitKind("native") == "native");
  CHECK(primec::resolveIrBackendEmitKind("unknown") == "unknown");
}

TEST_CASE("all production primec emit kinds route through ir backend resolution") {
  const std::vector<std::string_view> expectedKinds = {
      "cpp", "cpp-ir", "exe", "exe-ir", "native", "ir", "vm", "glsl", "spirv", "wasm", "glsl-ir", "spirv-ir"};

  const std::span<const std::string_view> emitKinds = primec::listPrimecEmitKinds();
  CHECK(std::vector<std::string_view>(emitKinds.begin(), emitKinds.end()) == expectedKinds);
  CHECK(primec::primecEmitKindsUsage() == "cpp|cpp-ir|exe|exe-ir|native|ir|vm|glsl|spirv|wasm|glsl-ir|spirv-ir");

  for (const std::string_view emitKind : emitKinds) {
    CAPTURE(emitKind);
    CHECK(primec::isPrimecEmitKind(emitKind));
    const std::string_view resolvedKind = primec::resolveIrBackendEmitKind(emitKind);
    CHECK(primec::findIrBackend(resolvedKind) != nullptr);
  }

  CHECK_FALSE(primec::isPrimecEmitKind("metal"));
}

TEST_CASE("ir preparation helper requires semantic product before lowering") {
  primec::Program program;
  primec::Options options;
  options.entryPath = "/main";
  options.inlineIrCalls = true;

  primec::IrModule ir;
  primec::IrPreparationFailure failure;
  CHECK_FALSE(primec::prepareIrModule(program, nullptr, options, primec::IrValidationTarget::Vm, ir, failure));
  CHECK(failure.stage == primec::IrPreparationFailureStage::Lowering);
  CHECK(failure.message == "semantic product is required for IR preparation");
  CHECK(failure.diagnosticInfo.message == failure.message);
}

TEST_CASE("ir preparation releases lowered AST bodies while preserving source-map provenance") {
  const std::filesystem::path tempPath = makeTempIrPipelineSourcePath();
  {
    std::ofstream file(tempPath);
    REQUIRE(file.good());
    file << R"(
[return<i32>]
helper([i32] input) {
  [i32] local{input}
  return(local)
}

[return<i32>]
main() {
  [i32] value{helper(21i32)}
  return(value)
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

  const primec::ProgramHeapEstimateStats before = primec::estimateProgramHeap(output.program);
  CHECK(before.dynamicBytes > 0);
  CHECK(before.exprs > 0);
  CHECK(std::any_of(output.program.definitions.begin(),
                    output.program.definitions.end(),
                    [](const primec::Definition &definition) {
                      return !definition.parameters.empty() || !definition.statements.empty() ||
                             definition.returnExpr.has_value();
                    }));

  primec::IrModule ir;
  primec::IrPreparationFailure failure;
  REQUIRE(primec::prepareIrModule(
      output.program, &output.semanticProgram, options, primec::IrValidationTarget::Vm, ir, failure));

  const primec::ProgramHeapEstimateStats after = primec::estimateProgramHeap(output.program);
  CHECK(after.dynamicBytes < before.dynamicBytes);
  CHECK(after.exprs < before.exprs);

  for (const auto &definition : output.program.definitions) {
    CHECK(definition.parameters.empty());
    CHECK(definition.statements.empty());
    CHECK_FALSE(definition.returnExpr.has_value());
  }
  for (const auto &execution : output.program.executions) {
    CHECK(execution.arguments.empty());
    CHECK(execution.argumentNames.empty());
    CHECK(execution.bodyArguments.empty());
    CHECK_FALSE(execution.hasBodyArguments);
  }

  REQUIRE_FALSE(ir.instructionSourceMap.empty());
  CHECK(std::any_of(ir.instructionSourceMap.begin(),
                    ir.instructionSourceMap.end(),
                    [](const primec::IrInstructionSourceMapEntry &entry) {
                      return entry.provenance == primec::IrSourceMapProvenance::CanonicalAst && entry.line > 0 &&
                             entry.column > 0;
                    }));
}

TEST_CASE("semantic-product contract rejects missing local-auto facts across entry targets") {
  const std::string source =
      "[return<T>]\n"
      "id<T>([T] value) {\n"
      "  return(value)\n"
      "}\n"
      "\n"
      "[return<i32>]\n"
      "main() {\n"
      "  [auto] selected{id(21i32)}\n"
      "  return(selected)\n"
      "}\n";

  primec::Program program;
  primec::SemanticProgram semanticProgram;
  std::string error;
  REQUIRE(parseAndValidateThroughCompilePipeline(
      source, program, &semanticProgram, error, {}, {}));
  CHECK(error.empty());
  REQUIRE(!semanticProgram.localAutoFacts.empty());

  auto clearPublishedLocalAutoFacts = [](primec::SemanticProgram &semanticProduct) {
    semanticProduct.localAutoFacts.clear();
    semanticProduct.publishedRoutingLookups.localAutoFactIndicesByExpr.clear();
    semanticProduct.publishedRoutingLookups.localAutoFactIndicesByInitPathAndBindingNameId.clear();
    for (auto &moduleArtifacts : semanticProduct.moduleResolvedArtifacts) {
      moduleArtifacts.localAutoFactIndices.clear();
    }
  };

  const std::vector<std::pair<std::string_view, primec::IrValidationTarget>> entryTargets = {
      {"primevm", primec::IrValidationTarget::Vm},
      {"primec-native", primec::IrValidationTarget::Native},
      {"primec-generic", primec::IrValidationTarget::Any},
  };

  for (const auto &[entryLabel, validationTarget] : entryTargets) {
    CAPTURE(entryLabel);
    primec::Program mutatedProgram = program;
    primec::SemanticProgram mutatedSemanticProduct = semanticProgram;
    clearPublishedLocalAutoFacts(mutatedSemanticProduct);

    primec::Options options;
    options.entryPath = "/main";

    primec::IrModule ir;
    primec::IrPreparationFailure failure;
    CHECK_FALSE(primec::prepareIrModule(
        mutatedProgram, &mutatedSemanticProduct, options, validationTarget, ir, failure));
    CHECK(failure.stage == primec::IrPreparationFailureStage::Lowering);
    CHECK(failure.message ==
          "missing semantic-product local-auto fact: /main -> local selected");
    CHECK(failure.diagnosticInfo.message == failure.message);
  }
}

TEST_CASE("semantic-product contract rejects stale local-auto binding types") {
  const std::string source =
      "[return<T>]\n"
      "id<T>([T] value) {\n"
      "  return(value)\n"
      "}\n"
      "\n"
      "[return<i32>]\n"
      "main() {\n"
      "  [auto] selected{id(21i32)}\n"
      "  return(selected)\n"
      "}\n";

  primec::Program program;
  primec::SemanticProgram semanticProgram;
  std::string error;
  REQUIRE(parseAndValidateThroughCompilePipeline(
      source, program, &semanticProgram, error, {}, {}));
  CHECK(error.empty());
  REQUIRE(!semanticProgram.localAutoFacts.empty());

  primec::SemanticProgramLocalAutoFact *selectedFact = nullptr;
  for (auto &entry : semanticProgram.localAutoFacts) {
    if (entry.scopePath == "/main" && entry.bindingName == "selected") {
      selectedFact = &entry;
      break;
    }
  }
  REQUIRE(selectedFact != nullptr);
  selectedFact->bindingTypeText = "f64";

  primec::Options options;
  options.entryPath = "/main";

  primec::IrModule ir;
  primec::IrPreparationFailure failure;
  CHECK_FALSE(primec::prepareIrModule(
      program, &semanticProgram, options, primec::IrValidationTarget::Vm, ir, failure));
  CHECK(failure.stage == primec::IrPreparationFailureStage::Lowering);
  CHECK(failure.message ==
        "stale semantic-product local-auto binding type metadata: /main -> local selected");
  CHECK(failure.diagnosticInfo.message == failure.message);
}

TEST_CASE("compile pipeline semantic handoff gate reaches lowering and rejects stale facts") {
  const std::string source =
      "import /std/math/*\n"
      "\n"
      "[i32]\n"
      "helper([i32] value) {\n"
      "  return(value)\n"
      "}\n"
      "\n"
      "[i32]\n"
      "main() {\n"
      "  [auto] selected{helper(41i32)}\n"
      "  return(selected)\n"
      "}\n";

  primec::Program program;
  primec::SemanticProgram semanticProgram;
  std::string error;
  REQUIRE(parseAndValidateThroughCompilePipeline(
      source, program, &semanticProgram, error, {}, {}));
  CHECK(error.empty());

  const auto *validatorPass = findSemanticValidationPass("validator-passes");
  const auto *publicationPass =
      findSemanticValidationPass("semantic-product-publication");
  REQUIRE(validatorPass != nullptr);
  REQUIRE(publicationPass != nullptr);
  CHECK(validatorPass->action ==
        primec::semantics::SemanticValidationPassAction::PublishesFacts);
  CHECK(publicationPass->kind ==
        primec::semantics::SemanticValidationPassKind::Publication);

  CHECK(std::find(program.imports.begin(), program.imports.end(), "/std/math/*") !=
        program.imports.end());
  const auto *mainDefinition = findSemanticEntry(
      program.definitions,
      [](const primec::Definition &definition) {
        return definition.fullPath == "/main";
      });
  REQUIRE(mainDefinition != nullptr);
  CHECK(std::any_of(mainDefinition->transforms.begin(),
                    mainDefinition->transforms.end(),
                    [](const primec::Transform &transform) {
                      return transform.name == "return" &&
                             transform.templateArgs ==
                                 std::vector<std::string>{"i32"};
                    }));

  const auto *mainModule =
      findSemanticModuleArtifacts(semanticProgram, "/main");
  REQUIRE(mainModule != nullptr);
  CHECK(!mainModule->directCallTargetIndices.empty());
  CHECK(!mainModule->callableSummaryIndices.empty());
  CHECK(!mainModule->returnFactIndices.empty());
  CHECK(!mainModule->localAutoFactIndices.empty());
  CHECK(!semanticProgram.publishedRoutingLookups.directCallTargetIdsByExpr.empty());
  CHECK(!semanticProgram.publishedRoutingLookups.localAutoFactIndicesByExpr.empty());
  CHECK(!semanticProgram.publishedRoutingLookups.callableSummaryIndicesByPathId.empty());

  const auto directCallTargets =
      primec::semanticProgramDirectCallTargetView(semanticProgram);
  const auto *helperCall = findSemanticEntry(
      directCallTargets,
      [&semanticProgram](const primec::SemanticProgramDirectCallTarget &entry) {
        return entry.scopePath == "/main" &&
               entry.callName == "helper" &&
               primec::semanticProgramDirectCallTargetResolvedPath(
                   semanticProgram, entry) == "/helper";
      });
  REQUIRE(helperCall != nullptr);

  primec::Options options;
  options.entryPath = "/main";
  primec::IrModule ir;
  primec::IrPreparationFailure failure;
  primec::Program loweringProgram = program;
  primec::SemanticProgram loweringSemanticProgram = semanticProgram;
  REQUIRE(primec::prepareIrModule(loweringProgram,
                                  &loweringSemanticProgram,
                                  options,
                                  primec::IrValidationTarget::Vm,
                                  ir,
                                  failure));
  CHECK(!ir.functions.empty());
  CHECK(ir.entryIndex >= 0);

  primec::Program staleProgram = program;
  primec::SemanticProgram staleSemanticProgram = semanticProgram;
  staleSemanticProgram.directCallTargets.clear();
  staleSemanticProgram.publishedRoutingLookups.directCallTargetIdsByExpr.clear();
  staleSemanticProgram.publishedRoutingLookups.directCallStdlibSurfaceIdsByExpr.clear();
  for (auto &moduleArtifacts : staleSemanticProgram.moduleResolvedArtifacts) {
    moduleArtifacts.directCallTargetIndices.clear();
  }

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
        "missing semantic-product direct-call target: /main -> helper");
  CHECK(staleFailure.diagnosticInfo.message == staleFailure.message);
}

TEST_CASE("native pick target sum resolution uses semantic-product facts") {
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

[return<i32>]
main() {
  [Choice] choice{[right] 41i32}
  return(pick(choice) {
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
  const auto *choiceFact =
      primec::ir_lowerer::findSemanticProductBindingFactByScopeAndName(
          semanticProductTargets, "/main", "choice");
  REQUIRE(choiceFact != nullptr);
  CHECK(choiceFact->bindingTypeText == "Choice");
  REQUIRE(primec::ir_lowerer::findSemanticProductSumTypeMetadata(
              semanticProductTargets, "/Choice") != nullptr);
  REQUIRE(primec::ir_lowerer::findSemanticProductSumTypeMetadata(
              semanticProductTargets, "/OtherChoice") != nullptr);
  const auto *rightVariantMetadata =
      primec::ir_lowerer::findSemanticProductSumVariantMetadata(
          semanticProductTargets, "/Choice", "right");
  REQUIRE(rightVariantMetadata != nullptr);
  CHECK(rightVariantMetadata->variantIndex == 1);
  CHECK(rightVariantMetadata->tagValue == 1);
  CHECK(rightVariantMetadata->hasPayload);
  CHECK(rightVariantMetadata->payloadTypeText == "i32");
  const auto choiceLeftVariantOrder = std::find_if(
      semanticProgram.sumVariantMetadata.begin(),
      semanticProgram.sumVariantMetadata.end(),
      [](const primec::SemanticProgramSumVariantMetadata &entry) {
        return entry.sumPath == "/Choice" && entry.variantName == "left";
      });
  const auto choiceRightVariantOrder = std::find_if(
      semanticProgram.sumVariantMetadata.begin(),
      semanticProgram.sumVariantMetadata.end(),
      [](const primec::SemanticProgramSumVariantMetadata &entry) {
        return entry.sumPath == "/Choice" && entry.variantName == "right";
      });
  REQUIRE(choiceLeftVariantOrder != semanticProgram.sumVariantMetadata.end());
  REQUIRE(choiceRightVariantOrder != semanticProgram.sumVariantMetadata.end());
  CHECK(choiceLeftVariantOrder < choiceRightVariantOrder);
  const auto choiceOrder = std::find_if(
      semanticProgram.sumTypeMetadata.begin(),
      semanticProgram.sumTypeMetadata.end(),
      [](const primec::SemanticProgramSumTypeMetadata &entry) {
        return entry.fullPath == "/Choice";
      });
  const auto otherOrder = std::find_if(
      semanticProgram.sumTypeMetadata.begin(),
      semanticProgram.sumTypeMetadata.end(),
      [](const primec::SemanticProgramSumTypeMetadata &entry) {
        return entry.fullPath == "/OtherChoice";
      });
  REQUIRE(choiceOrder != semanticProgram.sumTypeMetadata.end());
  REQUIRE(otherOrder != semanticProgram.sumTypeMetadata.end());
  CHECK(choiceOrder < otherOrder);

  primec::Options options;
  options.entryPath = "/main";
  primec::IrModule ir;
  primec::IrPreparationFailure failure;
  primec::Program loweringProgram = program;
  primec::SemanticProgram loweringSemanticProgram = semanticProgram;
  REQUIRE(primec::prepareIrModule(loweringProgram,
                                  &loweringSemanticProgram,
                                  options,
                                  primec::IrValidationTarget::Native,
                                  ir,
                                  failure));
  CHECK(!ir.functions.empty());

  auto rewriteChoiceBindingType =
      [](primec::SemanticProgram &semanticProduct, const std::string &typeText) {
        for (auto &fact : semanticProduct.bindingFacts) {
          if (fact.scopePath == "/main" && fact.name == "choice") {
            fact.bindingTypeText = typeText;
            fact.bindingTypeTextId =
                primec::semanticProgramInternCallTargetString(semanticProduct, typeText);
            return true;
          }
        }
        return false;
      };

  primec::Program staleProgram = program;
  primec::SemanticProgram staleSemanticProgram = semanticProgram;
  REQUIRE(rewriteChoiceBindingType(staleSemanticProgram, "OtherChoice"));

  primec::IrModule staleIr;
  primec::IrPreparationFailure staleFailure;
  CHECK_FALSE(primec::prepareIrModule(staleProgram,
                                      &staleSemanticProgram,
                                      options,
                                      primec::IrValidationTarget::Native,
                                      staleIr,
                                      staleFailure));
  CHECK(staleFailure.stage == primec::IrPreparationFailureStage::Lowering);
  CHECK(staleFailure.message ==
        "stale semantic-product pick target binding type: /main -> choice");
  CHECK(staleFailure.diagnosticInfo.message == staleFailure.message);

  primec::Program missingMetadataProgram = program;
  primec::SemanticProgram missingMetadataSemanticProgram = semanticProgram;
  missingMetadataSemanticProgram.sumTypeMetadata.erase(
      std::remove_if(missingMetadataSemanticProgram.sumTypeMetadata.begin(),
                     missingMetadataSemanticProgram.sumTypeMetadata.end(),
                     [](const primec::SemanticProgramSumTypeMetadata &entry) {
                       return entry.fullPath == "/Choice";
                     }),
      missingMetadataSemanticProgram.sumTypeMetadata.end());

  primec::IrModule missingMetadataIr;
  primec::IrPreparationFailure missingMetadataFailure;
  CHECK_FALSE(primec::prepareIrModule(missingMetadataProgram,
                                      &missingMetadataSemanticProgram,
                                      options,
                                      primec::IrValidationTarget::Native,
                                      missingMetadataIr,
                                      missingMetadataFailure));
  CHECK(missingMetadataFailure.stage == primec::IrPreparationFailureStage::Lowering);
  CHECK(missingMetadataFailure.message ==
        "missing semantic-product sum metadata for pick target: /main -> choice");
  CHECK(missingMetadataFailure.diagnosticInfo.message == missingMetadataFailure.message);

  auto rewriteChoiceRightVariantTag =
      [](primec::SemanticProgram &semanticProduct, uint32_t tagValue) {
        for (auto &entry : semanticProduct.sumVariantMetadata) {
          if (entry.sumPath == "/Choice" && entry.variantName == "right") {
            entry.tagValue = tagValue;
            return true;
          }
        }
        return false;
      };

  primec::Program staleVariantProgram = program;
  primec::SemanticProgram staleVariantSemanticProgram = semanticProgram;
  REQUIRE(rewriteChoiceRightVariantTag(staleVariantSemanticProgram, 42));

  primec::IrModule staleVariantIr;
  primec::IrPreparationFailure staleVariantFailure;
  CHECK_FALSE(primec::prepareIrModule(staleVariantProgram,
                                      &staleVariantSemanticProgram,
                                      options,
                                      primec::IrValidationTarget::Native,
                                      staleVariantIr,
                                      staleVariantFailure));
  CHECK(staleVariantFailure.stage == primec::IrPreparationFailureStage::Lowering);
  CHECK(staleVariantFailure.message ==
        "stale semantic-product sum variant metadata for pick arm: /Choice -> right");
  CHECK(staleVariantFailure.diagnosticInfo.message == staleVariantFailure.message);

  primec::Program missingVariantProgram = program;
  primec::SemanticProgram missingVariantSemanticProgram = semanticProgram;
  missingVariantSemanticProgram.sumVariantMetadata.erase(
      std::remove_if(missingVariantSemanticProgram.sumVariantMetadata.begin(),
                     missingVariantSemanticProgram.sumVariantMetadata.end(),
                     [](const primec::SemanticProgramSumVariantMetadata &entry) {
                       return entry.sumPath == "/Choice" &&
                              entry.variantName == "right";
                     }),
      missingVariantSemanticProgram.sumVariantMetadata.end());

  primec::IrModule missingVariantIr;
  primec::IrPreparationFailure missingVariantFailure;
  CHECK_FALSE(primec::prepareIrModule(missingVariantProgram,
                                      &missingVariantSemanticProgram,
                                      options,
                                      primec::IrValidationTarget::Native,
                                      missingVariantIr,
                                      missingVariantFailure));
  CHECK(missingVariantFailure.stage == primec::IrPreparationFailureStage::Lowering);
  CHECK(missingVariantFailure.message ==
        "missing semantic-product sum variant metadata for pick arm: /Choice -> right");
  CHECK(missingVariantFailure.diagnosticInfo.message == missingVariantFailure.message);
}

TEST_CASE("native pick payload locals use semantic-product variant metadata") {
  const std::filesystem::path cwd = std::filesystem::current_path();
  std::filesystem::path lowerSumHelpersPath =
      cwd / "src" / "ir_lowerer" / "IrLowererLowerSumHelpers.h";
  if (!std::filesystem::exists(lowerSumHelpersPath)) {
    lowerSumHelpersPath =
        cwd.parent_path() / "src" / "ir_lowerer" / "IrLowererLowerSumHelpers.h";
  }
  REQUIRE(std::filesystem::exists(lowerSumHelpersPath));

  const std::string source = readTextFile(lowerSumHelpersPath);
  CHECK(source.find("resolvePublishedSumPayloadStorageInfo") != std::string::npos);
  CHECK(source.find("\"pick payload local\"") != std::string::npos);
  CHECK(source.find("\"pick arm\"") != std::string::npos);
  CHECK(source.find("makePickPayloadLocalInfo(sumDef, *variant, payloadInfo)") !=
        std::string::npos);
  CHECK(source.find("*sumDef, *variant, arm.args.front(), sumPtrLocal, branchLocals") !=
        std::string::npos);
  const size_t aggregateResolverPos =
      source.find("resolveSemanticProductPickAggregateResultStructPath");
  REQUIRE(aggregateResolverPos != std::string::npos);
  const size_t aggregateBindingFactPos =
      source.find("findSemanticProductBindingFact(semanticTargets, valueExpr)",
                  aggregateResolverPos);
  const size_t aggregateQueryFactPos =
      source.find("findSemanticProductQueryFact(semanticTargets, valueExpr)",
                  aggregateResolverPos);
  const size_t aggregateTypeResolverPos =
      source.find("addSemanticProductCandidateTypeText", aggregateResolverPos);
  const size_t aggregateSemanticResolvePos =
      source.find("semanticProgramResolveCallTargetString(",
                  aggregateTypeResolverPos);
  const size_t aggregateBindingIdPos =
      source.find("bindingFact->bindingTypeTextId", aggregateTypeResolverPos);
  const size_t aggregateQueryBindingIdPos =
      source.find("queryFact->bindingTypeTextId", aggregateBindingIdPos);
  const size_t aggregateQueryTypeIdPos =
      source.find("queryFact->queryTypeTextId", aggregateQueryBindingIdPos);
  const size_t staleAggregatePos =
      source.find("stale semantic-product pick aggregate result metadata",
                  aggregateResolverPos);
  const size_t aggregateFallbackPos =
      source.find("branchStructPath = inferStructExprPath(*valueExpr, branchLocals)",
                  aggregateResolverPos);
  REQUIRE(aggregateBindingFactPos != std::string::npos);
  REQUIRE(aggregateQueryFactPos != std::string::npos);
  REQUIRE(aggregateTypeResolverPos != std::string::npos);
  REQUIRE(aggregateSemanticResolvePos != std::string::npos);
  REQUIRE(aggregateBindingIdPos != std::string::npos);
  REQUIRE(aggregateQueryBindingIdPos != std::string::npos);
  REQUIRE(aggregateQueryTypeIdPos != std::string::npos);
  REQUIRE(staleAggregatePos != std::string::npos);
  REQUIRE(aggregateFallbackPos != std::string::npos);
  CHECK(aggregateTypeResolverPos < aggregateBindingFactPos);
  CHECK(aggregateSemanticResolvePos < aggregateBindingIdPos);
  CHECK(aggregateBindingIdPos < aggregateQueryBindingIdPos);
  CHECK(aggregateQueryBindingIdPos < aggregateQueryTypeIdPos);
  CHECK(aggregateBindingFactPos < aggregateFallbackPos);
  CHECK(aggregateQueryFactPos < aggregateFallbackPos);
  CHECK(aggregateQueryTypeIdPos < aggregateFallbackPos);
  CHECK(source.find("if (!resolvedBySemanticProduct.has_value())", aggregateResolverPos) !=
        std::string::npos);
  CHECK(source.find("validateSemanticProductPickArmVariant") == std::string::npos);
  CHECK(source.find("makePickPayloadLocalInfo(sumDef, *variant, publishedVariant, payloadInfo)") ==
        std::string::npos);
  CHECK(source.find("publishedVariant, arm.args.front(), sumPtrLocal, branchLocals") ==
        std::string::npos);
}

TEST_CASE("native pick target sum resolution resolves interned type ids") {
  const std::filesystem::path cwd = std::filesystem::current_path();
  std::filesystem::path lowerSumHelpersPath =
      cwd / "src" / "ir_lowerer" / "IrLowererLowerSumHelpers.h";
  if (!std::filesystem::exists(lowerSumHelpersPath)) {
    lowerSumHelpersPath =
        cwd.parent_path() / "src" / "ir_lowerer" / "IrLowererLowerSumHelpers.h";
  }
  REQUIRE(std::filesystem::exists(lowerSumHelpersPath));

  const std::string source = readTextFile(lowerSumHelpersPath);
  const size_t resolverPos =
      source.find("resolveSemanticProductPickTargetSumDefinition");
  REQUIRE(resolverPos != std::string::npos);
  const size_t typeResolverPos =
      source.find("resolveSemanticProductTypeText", resolverPos);
  const size_t semanticResolvePos =
      source.find("semanticProgramResolveCallTargetString(", typeResolverPos);
  const size_t bindingIdPos =
      source.find("bindingFact->bindingTypeTextId", typeResolverPos);
  const size_t queryBindingIdPos =
      source.find("queryFact->bindingTypeTextId", bindingIdPos);
  const size_t queryTypeIdPos =
      source.find("queryFact->queryTypeTextId", queryBindingIdPos);
  const size_t returnIdPos =
      source.find("returnFact->bindingTypeTextId", queryTypeIdPos);
  const size_t metadataPos =
      source.find("return requirePublishedSumMetadata(*querySumDef, targetExpr)",
                  returnIdPos);
  REQUIRE(typeResolverPos != std::string::npos);
  REQUIRE(semanticResolvePos != std::string::npos);
  REQUIRE(bindingIdPos != std::string::npos);
  REQUIRE(queryBindingIdPos != std::string::npos);
  REQUIRE(queryTypeIdPos != std::string::npos);
  REQUIRE(returnIdPos != std::string::npos);
  REQUIRE(metadataPos != std::string::npos);
  CHECK(typeResolverPos < semanticResolvePos);
  CHECK(semanticResolvePos < bindingIdPos);
  CHECK(bindingIdPos < queryBindingIdPos);
  CHECK(queryBindingIdPos < queryTypeIdPos);
  CHECK(queryTypeIdPos < returnIdPos);
  CHECK(returnIdPos < metadataPos);
}

TEST_CASE("native sum construction uses semantic-product variant tags") {
  const std::string source = R"(
[sum]
Choice {
  [i32] left
  [i32] right
}

[return<i32>]
main() {
  [Choice] choice{[right] 41i32}
  return(0i32)
}
)";

  primec::Program program;
  primec::SemanticProgram semanticProgram;
  std::string error;
  REQUIRE(parseAndValidateThroughCompilePipeline(
      source, program, &semanticProgram, error, {}, {}));
  CHECK(error.empty());

  primec::Options options;
  options.entryPath = "/main";
  primec::IrModule ir;
  primec::IrPreparationFailure failure;
  primec::Program loweringProgram = program;
  primec::SemanticProgram loweringSemanticProgram = semanticProgram;
  REQUIRE(primec::prepareIrModule(loweringProgram,
                                  &loweringSemanticProgram,
                                  options,
                                  primec::IrValidationTarget::Native,
                                  ir,
                                  failure));
  CHECK(!ir.functions.empty());

  auto rewriteChoiceRightVariantTag =
      [](primec::SemanticProgram &semanticProduct, uint32_t tagValue) {
        for (auto &entry : semanticProduct.sumVariantMetadata) {
          if (entry.sumPath == "/Choice" && entry.variantName == "right") {
            entry.tagValue = tagValue;
            return true;
          }
        }
        return false;
      };

  primec::Program staleProgram = program;
  primec::SemanticProgram staleSemanticProgram = semanticProgram;
  REQUIRE(rewriteChoiceRightVariantTag(staleSemanticProgram, 42));

  primec::IrModule staleIr;
  primec::IrPreparationFailure staleFailure;
  CHECK_FALSE(primec::prepareIrModule(staleProgram,
                                      &staleSemanticProgram,
                                      options,
                                      primec::IrValidationTarget::Native,
                                      staleIr,
                                      staleFailure));
  CHECK(staleFailure.stage == primec::IrPreparationFailureStage::Lowering);
  CHECK(staleFailure.message ==
        "stale semantic-product sum variant metadata for sum construction: /Choice -> right");
  CHECK(staleFailure.diagnosticInfo.message == staleFailure.message);
}

TEST_CASE("native sum slot layout uses semantic-product variant metadata") {
  const std::string source = R"(
[struct]
LeftPayload() {
  [i32] value{0i32}
}

[struct]
RightPayload() {
  [i32] value{0i32}
}

[sum]
Choice {
  [LeftPayload] left
  [RightPayload] right
}

[return<i32>]
main() {
  [Choice] choice{[left] LeftPayload{7i32}}
  return(0i32)
}
)";

  primec::Program program;
  primec::SemanticProgram semanticProgram;
  std::string error;
  REQUIRE(parseAndValidateThroughCompilePipeline(
      source, program, &semanticProgram, error, {}, {}));
  CHECK(error.empty());

  primec::Options options;
  options.entryPath = "/main";
  primec::IrModule ir;
  primec::IrPreparationFailure failure;
  primec::Program loweringProgram = program;
  primec::SemanticProgram loweringSemanticProgram = semanticProgram;
  REQUIRE(primec::prepareIrModule(loweringProgram,
                                  &loweringSemanticProgram,
                                  options,
                                  primec::IrValidationTarget::Native,
                                  ir,
                                  failure));
  CHECK(!ir.functions.empty());

  auto rewriteChoiceRightVariantPayload =
      [](primec::SemanticProgram &semanticProduct, const std::string &payloadTypeText) {
        for (auto &entry : semanticProduct.sumVariantMetadata) {
          if (entry.sumPath == "/Choice" && entry.variantName == "right") {
            entry.payloadTypeText = payloadTypeText;
            return true;
          }
        }
        return false;
      };

  primec::Program staleProgram = program;
  primec::SemanticProgram staleSemanticProgram = semanticProgram;
  REQUIRE(rewriteChoiceRightVariantPayload(staleSemanticProgram, "i64"));

  primec::IrModule staleIr;
  primec::IrPreparationFailure staleFailure;
  CHECK_FALSE(primec::prepareIrModule(staleProgram,
                                      &staleSemanticProgram,
                                      options,
                                      primec::IrValidationTarget::Native,
                                      staleIr,
                                      staleFailure));
  CHECK(staleFailure.stage == primec::IrPreparationFailureStage::Lowering);
  CHECK(staleFailure.message ==
        "stale semantic-product sum variant metadata for sum slot layout: /Choice -> right");
  CHECK(staleFailure.diagnosticInfo.message == staleFailure.message);
}

TEST_CASE("native sum variant selection uses semantic-product payload metadata") {
  const std::filesystem::path cwd = std::filesystem::current_path();
  std::filesystem::path lowerSumHelpersPath =
      cwd / "src" / "ir_lowerer" / "IrLowererLowerSumHelpers.h";
  if (!std::filesystem::exists(lowerSumHelpersPath)) {
    lowerSumHelpersPath =
        cwd.parent_path() / "src" / "ir_lowerer" / "IrLowererLowerSumHelpers.h";
  }
  REQUIRE(std::filesystem::exists(lowerSumHelpersPath));

  const std::string source = readTextFile(lowerSumHelpersPath);
  const size_t initializerShapePos =
      source.find("resolveSemanticProductInitializerPayloadShape");
  const size_t initializerBindingFactPos =
      source.find("findSemanticProductBindingFact(semanticTargets, initializer)",
                  initializerShapePos);
  const size_t initializerQueryFactPos =
      source.find("findSemanticProductQueryFact(semanticTargets, initializer)",
                  initializerShapePos);
  const size_t initializerTypeResolverPos =
      source.find("addSemanticProductCandidateTypeText", initializerShapePos);
  const size_t initializerSemanticResolvePos =
      source.find("semanticProgramResolveCallTargetString(",
                  initializerTypeResolverPos);
  const size_t initializerBindingIdPos =
      source.find("bindingFact->bindingTypeTextId", initializerTypeResolverPos);
  const size_t initializerQueryBindingIdPos =
      source.find("queryFact->bindingTypeTextId", initializerBindingIdPos);
  const size_t initializerQueryTypeIdPos =
      source.find("queryFact->queryTypeTextId", initializerQueryBindingIdPos);
  const size_t staleInitializerTypePos =
      source.find("stale semantic-product sum initializer type metadata",
                  initializerShapePos);
  const size_t initializerFallbackKindPos =
      source.find("initializerKind = inferExprKind(initializer, valueLocals)",
                  initializerShapePos);
  REQUIRE(initializerShapePos != std::string::npos);
  REQUIRE(initializerBindingFactPos != std::string::npos);
  REQUIRE(initializerQueryFactPos != std::string::npos);
  REQUIRE(initializerTypeResolverPos != std::string::npos);
  REQUIRE(initializerSemanticResolvePos != std::string::npos);
  REQUIRE(initializerBindingIdPos != std::string::npos);
  REQUIRE(initializerQueryBindingIdPos != std::string::npos);
  REQUIRE(initializerQueryTypeIdPos != std::string::npos);
  REQUIRE(staleInitializerTypePos != std::string::npos);
  REQUIRE(initializerFallbackKindPos != std::string::npos);
  CHECK(initializerTypeResolverPos < initializerBindingFactPos);
  CHECK(initializerSemanticResolvePos < initializerBindingIdPos);
  CHECK(initializerBindingIdPos < initializerQueryBindingIdPos);
  CHECK(initializerQueryBindingIdPos < initializerQueryTypeIdPos);
  CHECK(initializerBindingFactPos < initializerFallbackKindPos);
  CHECK(initializerQueryFactPos < initializerFallbackKindPos);
  CHECK(initializerQueryTypeIdPos < initializerFallbackKindPos);
  CHECK(source.find("if (!resolvedInitializerShape.has_value())") != std::string::npos);
  CHECK(source.find("\"sum constructor selection\"") != std::string::npos);
  CHECK(source.find("\"Result.ok selection\"") != std::string::npos);
  CHECK(source.find("\"sum initializer selection\"") != std::string::npos);
  CHECK(source.find("targetSum, *variant, \"sum constructor selection\", payloadInfo") !=
        std::string::npos);
  CHECK(source.find("targetSum, *variant, \"Result.ok selection\", payloadInfo") !=
        std::string::npos);
  CHECK(source.find("targetSum, variant, \"sum initializer selection\", payloadInfo") !=
        std::string::npos);
  CHECK(source.find("resolveSumPayloadStorageInfo(targetSum, *variant, payloadInfo)") ==
        std::string::npos);
  CHECK(source.find("resolveSumPayloadStorageInfo(targetSum, variant, payloadInfo)") ==
        std::string::npos);
}

TEST_CASE("native Result combinators use semantic-product variant tags") {
  const std::string source = R"(
import /std/file/*

[effects(io_err)]
log_file_error([FileError] err) {
  print_line_error(err.why())
}

[return<i32> effects(io_err) on_error<FileError, /log_file_error>]
main() {
  [Result<i32, FileError>] source{Result.ok(2i32)}
  [Result<i32, FileError>] mapped{
    Result.map(source, []([i32] value) { return(plus(value, 1i32)) })
  }
  [Result<i32, FileError>] chained{
    Result.and_then(mapped, []([i32] value) { return(Result.ok(plus(value, 3i32))) })
  }
  [Result<i32, FileError>] combined{
    Result.map2(chained, Result.ok(5i32), []([i32] left, [i32] right) {
      return(plus(left, right))
    })
  }
  return(try(combined))
}
)";

  primec::Program program;
  primec::SemanticProgram semanticProgram;
  std::string error;
  REQUIRE(parseAndValidateThroughCompilePipeline(
      source, program, &semanticProgram, error, {}, {}));
  CHECK(error.empty());

  primec::Options options;
  options.entryPath = "/main";
  primec::IrModule ir;
  primec::IrPreparationFailure failure;
  primec::Program loweringProgram = program;
  primec::SemanticProgram loweringSemanticProgram = semanticProgram;
  REQUIRE(primec::prepareIrModule(loweringProgram,
                                  &loweringSemanticProgram,
                                  options,
                                  primec::IrValidationTarget::Native,
                                  ir,
                                  failure));
  CHECK(!ir.functions.empty());

  auto rewriteStdlibResultErrorVariantTag =
      [](primec::SemanticProgram &semanticProduct, uint32_t tagValue) {
        std::vector<std::string> valueResultPaths;
        for (const auto &entry : semanticProduct.sumVariantMetadata) {
          const bool isResultSum = entry.sumPath == "/std/result/Result" ||
                                   entry.sumPath.rfind("/std/result/Result__", 0) == 0;
          if (isResultSum && entry.variantName == "ok" &&
              entry.payloadTypeText.find("i32") != std::string::npos) {
            valueResultPaths.push_back(entry.sumPath);
          }
        }
        bool rewritten = false;
        for (auto &entry : semanticProduct.sumVariantMetadata) {
          if (std::find(valueResultPaths.begin(), valueResultPaths.end(), entry.sumPath) !=
                  valueResultPaths.end() &&
              entry.variantName == "error" &&
              entry.payloadTypeText.find("FileError") != std::string::npos) {
            entry.tagValue = tagValue;
            rewritten = true;
          }
        }
        return rewritten;
      };

  primec::Program staleProgram = program;
  primec::SemanticProgram staleSemanticProgram = semanticProgram;
  REQUIRE(rewriteStdlibResultErrorVariantTag(staleSemanticProgram, 42));

  primec::IrModule staleIr;
  primec::IrPreparationFailure staleFailure;
  CHECK_FALSE(primec::prepareIrModule(staleProgram,
                                      &staleSemanticProgram,
                                      options,
                                      primec::IrValidationTarget::Native,
                                      staleIr,
                                      staleFailure));
  CHECK(staleFailure.stage == primec::IrPreparationFailureStage::Lowering);
  CHECK(staleFailure.message.find(
            "stale semantic-product sum variant metadata for Result.map target error: ") !=
        std::string::npos);
  CHECK(staleFailure.diagnosticInfo.message == staleFailure.message);
}

TEST_CASE("native Result combinator payload storage uses semantic-product variant metadata") {
  const std::string source = R"(
import /std/file/*

[effects(io_err)]
log_file_error([FileError] err) {
  print_line_error(err.why())
}

[return<i32> effects(io_err) on_error<FileError, /log_file_error>]
main() {
  [Result<i32, FileError>] source{Result.ok(2i32)}
  [Result<bool, FileError>] mapped{
    Result.map(source, []([i32] value) { return(true) })
  }
  [Result<i32, FileError>] chained{
    Result.and_then(mapped, []([bool] flag) { return(Result.ok(3i32)) })
  }
  [Result<i32, FileError>] combined{
    Result.map2(chained, Result.ok(5i32), []([i32] left, [i32] right) {
      return(plus(left, right))
    })
  }
  return(try(combined))
}
)";

  primec::Program program;
  primec::SemanticProgram semanticProgram;
  std::string error;
  REQUIRE(parseAndValidateThroughCompilePipeline(
      source, program, &semanticProgram, error, {}, {}));
  CHECK(error.empty());

  primec::Options options;
  options.entryPath = "/main";
  primec::IrModule ir;
  primec::IrPreparationFailure failure;
  primec::Program loweringProgram = program;
  primec::SemanticProgram loweringSemanticProgram = semanticProgram;
  REQUIRE(primec::prepareIrModule(loweringProgram,
                                  &loweringSemanticProgram,
                                  options,
                                  primec::IrValidationTarget::Native,
                                  ir,
                                  failure));
  CHECK(!ir.functions.empty());

  auto rewriteSourceResultErrorPayload =
      [](primec::SemanticProgram &semanticProduct, std::string payloadTypeText) {
        std::vector<std::string> sourceResultPaths;
        for (const auto &entry : semanticProduct.sumVariantMetadata) {
          const bool isResultSum = entry.sumPath == "/std/result/Result" ||
                                   entry.sumPath.rfind("/std/result/Result__", 0) == 0;
          if (isResultSum && entry.variantName == "ok" &&
              entry.payloadTypeText.find("i32") != std::string::npos) {
            sourceResultPaths.push_back(entry.sumPath);
          }
        }
        bool rewritten = false;
        for (auto &entry : semanticProduct.sumVariantMetadata) {
          if (std::find(sourceResultPaths.begin(), sourceResultPaths.end(), entry.sumPath) !=
                  sourceResultPaths.end() &&
              entry.variantName == "error" &&
              entry.payloadTypeText.find("FileError") != std::string::npos) {
            entry.payloadTypeText = payloadTypeText;
            rewritten = true;
          }
        }
        return rewritten;
      };

  primec::Program staleProgram = program;
  primec::SemanticProgram staleSemanticProgram = semanticProgram;
  REQUIRE(rewriteSourceResultErrorPayload(staleSemanticProgram, "i64"));

  primec::IrModule staleIr;
  primec::IrPreparationFailure staleFailure;
  CHECK_FALSE(primec::prepareIrModule(staleProgram,
                                      &staleSemanticProgram,
                                      options,
                                      primec::IrValidationTarget::Native,
                                      staleIr,
                                      staleFailure));
  CHECK(staleFailure.stage == primec::IrPreparationFailureStage::Lowering);
  CHECK(staleFailure.message.find(
            "stale semantic-product sum variant metadata for Result.map source error payload: ") !=
        std::string::npos);
  CHECK(staleFailure.diagnosticInfo.message == staleFailure.message);
}

TEST_CASE("native Result combinator sources use semantic-product query facts") {
  const std::filesystem::path cwd = std::filesystem::current_path();
  std::filesystem::path lowerSumHelpersPath =
      cwd / "src" / "ir_lowerer" / "IrLowererLowerSumHelpers.h";
  if (!std::filesystem::exists(lowerSumHelpersPath)) {
    lowerSumHelpersPath =
        cwd.parent_path() / "src" / "ir_lowerer" / "IrLowererLowerSumHelpers.h";
  }
  REQUIRE(std::filesystem::exists(lowerSumHelpersPath));

  const std::string source = readTextFile(lowerSumHelpersPath);
  const size_t semanticResolverPos =
      source.find("resolveSemanticProductResultSumSourceDefinition");
  REQUIRE(semanticResolverPos != std::string::npos);
  const size_t queryFactPos =
      source.find("findSemanticProductQueryFact(", semanticResolverPos);
  const size_t typeResolverPos =
      source.find("auto resolveQueryTypeText", queryFactPos);
  const size_t internedTypeTextPos =
      source.find("semanticProgramResolveCallTargetString(", typeResolverPos);
  const size_t bindingTextIdPos =
      source.find("queryFact->bindingTypeTextId", typeResolverPos);
  const size_t queryTextIdPos =
      source.find("queryFact->queryTypeTextId", bindingTextIdPos);
  const size_t sumDefinitionPos =
      source.find("resolveSumDefinitionForTypeText", typeResolverPos);
  const size_t staleDiagnosticPos =
      source.find("stale semantic-product Result-combinator source query metadata",
                  semanticResolverPos);
  const size_t fallbackStructPathPos =
      source.find("const std::string sourceStructPath = inferStructExprPath(sourceExpr, sourceLocals)",
                  semanticResolverPos);
  REQUIRE(queryFactPos != std::string::npos);
  REQUIRE(typeResolverPos != std::string::npos);
  REQUIRE(internedTypeTextPos != std::string::npos);
  REQUIRE(bindingTextIdPos != std::string::npos);
  REQUIRE(queryTextIdPos != std::string::npos);
  REQUIRE(sumDefinitionPos != std::string::npos);
  REQUIRE(staleDiagnosticPos != std::string::npos);
  REQUIRE(fallbackStructPathPos != std::string::npos);
  CHECK(queryFactPos < typeResolverPos);
  CHECK(typeResolverPos < internedTypeTextPos);
  CHECK(internedTypeTextPos < sumDefinitionPos);
  CHECK(typeResolverPos < bindingTextIdPos);
  CHECK(bindingTextIdPos < queryTextIdPos);
  CHECK(queryFactPos < fallbackStructPathPos);
  CHECK(source.find("if (!resolvedBySemanticProductQuery.has_value())") != std::string::npos);
  CHECK(source.find("resolveQueryTypeText(queryFact->bindingTypeText);",
                    semanticResolverPos) == std::string::npos);
}

TEST_CASE("native Result why direct-call sources use semantic-product query facts") {
  const std::filesystem::path cwd = std::filesystem::current_path();
  std::filesystem::path resultWhyHelpersPath =
      cwd / "src" / "ir_lowerer" / "IrLowererResultWhyHelpers.cpp";
  if (!std::filesystem::exists(resultWhyHelpersPath)) {
    resultWhyHelpersPath =
        cwd.parent_path() / "src" / "ir_lowerer" / "IrLowererResultWhyHelpers.cpp";
  }
  REQUIRE(std::filesystem::exists(resultWhyHelpersPath));

  const std::string source = readTextFile(resultWhyHelpersPath);
  const size_t resolverPos =
      source.find("resolveSemanticQueryResultErrorTypeText");
  const size_t semanticResolvePos =
      source.find("semanticProgramResolveCallTargetString(", resolverPos);
  const size_t errorTypeIdPos =
      source.find("queryFact.resultErrorTypeId", resolverPos);
  const size_t directCallResolverPos =
      source.find("directCallReturnsImportedStdlibResultSum");
  REQUIRE(directCallResolverPos != std::string::npos);
  const size_t queryFactPos =
      source.find("findSemanticProductQueryFact(", directCallResolverPos);
  const size_t missingDiagnosticPos =
      source.find("missing semantic-product Result.why source query fact",
                  directCallResolverPos);
  const size_t staleDiagnosticPos =
      source.find("stale semantic-product Result.why source query metadata",
                  directCallResolverPos);
  const size_t fallbackTransformScanPos =
      source.find("for (const auto &transform : calleeDef->transforms)",
                  directCallResolverPos);
  REQUIRE(queryFactPos != std::string::npos);
  REQUIRE(missingDiagnosticPos != std::string::npos);
  REQUIRE(staleDiagnosticPos != std::string::npos);
  REQUIRE(fallbackTransformScanPos != std::string::npos);
  REQUIRE(resolverPos != std::string::npos);
  REQUIRE(semanticResolvePos != std::string::npos);
  REQUIRE(errorTypeIdPos != std::string::npos);
  CHECK(resolverPos < directCallResolverPos);
  CHECK(resolverPos < errorTypeIdPos);
  CHECK(errorTypeIdPos < semanticResolvePos);
  CHECK(queryFactPos < fallbackTransformScanPos);
  CHECK(staleDiagnosticPos < fallbackTransformScanPos);
  CHECK(source.find("trimTemplateTypeText(queryFact->resultErrorType)") ==
        std::string::npos);
}

TEST_CASE("native Result error direct-call sources use semantic-product query facts") {
  const std::filesystem::path cwd = std::filesystem::current_path();
  std::filesystem::path packedResultHelpersPath =
      cwd / "src" / "ir_lowerer" / "IrLowererPackedResultHelpers.cpp";
  if (!std::filesystem::exists(packedResultHelpersPath)) {
    packedResultHelpersPath =
        cwd.parent_path() / "src" / "ir_lowerer" / "IrLowererPackedResultHelpers.cpp";
  }
  REQUIRE(std::filesystem::exists(packedResultHelpersPath));

  const std::string source = readTextFile(packedResultHelpersPath);
  const size_t resolverPos =
      source.find("resolveSemanticQueryResultErrorTypeText");
  const size_t semanticResolvePos =
      source.find("semanticProgramResolveCallTargetString(", resolverPos);
  const size_t errorTypeIdPos =
      source.find("queryFact.resultErrorTypeId", resolverPos);
  const size_t directCallResolverPos =
      source.find("directCallReturnsImportedStdlibResultSum");
  REQUIRE(directCallResolverPos != std::string::npos);
  const size_t queryFactPos =
      source.find("findSemanticProductQueryFact(", directCallResolverPos);
  const size_t missingDiagnosticPos =
      source.find("missing semantic-product Result.error source query fact",
                  directCallResolverPos);
  const size_t staleDiagnosticPos =
      source.find("stale semantic-product Result.error source query metadata",
                  directCallResolverPos);
  const size_t fallbackTransformScanPos =
      source.find("for (const auto &transform : calleeDef->transforms)",
                  directCallResolverPos);
  REQUIRE(queryFactPos != std::string::npos);
  REQUIRE(missingDiagnosticPos != std::string::npos);
  REQUIRE(staleDiagnosticPos != std::string::npos);
  REQUIRE(fallbackTransformScanPos != std::string::npos);
  REQUIRE(resolverPos != std::string::npos);
  REQUIRE(semanticResolvePos != std::string::npos);
  REQUIRE(errorTypeIdPos != std::string::npos);
  CHECK(resolverPos < directCallResolverPos);
  CHECK(resolverPos < errorTypeIdPos);
  CHECK(errorTypeIdPos < semanticResolvePos);
  CHECK(queryFactPos < fallbackTransformScanPos);
  CHECK(staleDiagnosticPos < fallbackTransformScanPos);
  CHECK(source.find("trimTemplateTypeText(queryFact->resultErrorType)") ==
        std::string::npos);
}

TEST_CASE("native aggregate pointer dereference calls use semantic-product return facts") {
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

  const size_t resolverPos = source.find("resolveAggregatePointerLikeCallExpr");
  REQUIRE(resolverPos != std::string::npos);
  const size_t returnFactPos =
      source.find("findSemanticProductReturnFactByPath", resolverPos);
  const size_t helperCallPos =
      source.find("resolveSemanticReturnFactBindingType", returnFactPos);
  const size_t missingDiagnosticPos =
      source.find("missing semantic-product aggregate pointer return metadata",
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
  const size_t semanticResolverPos =
      source.find("resolveSemanticProductFieldReceiverStructPath");
  REQUIRE(semanticResolverPos != std::string::npos);
  const size_t bindingFactPos =
      source.find("findSemanticProductBindingFact(semanticTargets, receiverExpr)",
                  semanticResolverPos);
  const size_t queryFactPos =
      source.find("findSemanticProductQueryFact(semanticTargets, receiverExpr)",
                  semanticResolverPos);
  const size_t staleDiagnosticPos =
      source.find("stale semantic-product field receiver metadata",
                  semanticResolverPos);
  const size_t fallbackStructPathPos =
      source.find("structPath = inferStructExprPath(receiver, localsIn)",
                  semanticResolverPos);
  const size_t internedTypeResolverPos =
      source.find("semanticProgramResolveCallTargetString(",
                  semanticResolverPos);
  REQUIRE(bindingFactPos != std::string::npos);
  REQUIRE(queryFactPos != std::string::npos);
  REQUIRE(internedTypeResolverPos != std::string::npos);
  REQUIRE(staleDiagnosticPos != std::string::npos);
  REQUIRE(fallbackStructPathPos != std::string::npos);
  CHECK(source.find("addCandidateTypeText(bindingFact->bindingTypeText",
                    semanticResolverPos) != std::string::npos);
  CHECK(source.find("bindingFact->bindingTypeTextId",
                    semanticResolverPos) != std::string::npos);
  CHECK(source.find("addCandidateTypeText(queryFact->bindingTypeText",
                    semanticResolverPos) != std::string::npos);
  CHECK(source.find("queryFact->bindingTypeTextId",
                    semanticResolverPos) != std::string::npos);
  CHECK(source.find("addCandidateTypeText(queryFact->queryTypeText",
                    semanticResolverPos) != std::string::npos);
  CHECK(source.find("queryFact->queryTypeTextId",
                    semanticResolverPos) != std::string::npos);
  CHECK(source.find("splitTemplateTypeName(normalizedTypeText, wrapperBase, wrapperArgs)",
                    semanticResolverPos) != std::string::npos);
  CHECK(internedTypeResolverPos < fallbackStructPathPos);
  CHECK(bindingFactPos < fallbackStructPathPos);
  CHECK(queryFactPos < fallbackStructPathPos);
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
  const size_t semanticResolverPos =
      source.find("resolveSemanticProductPackedResultPayload");
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
                  semanticResolverPos);
  REQUIRE(bindingFactPos != std::string::npos);
  REQUIRE(queryFactPos != std::string::npos);
  REQUIRE(internedTypeResolverPos != std::string::npos);
  REQUIRE(collectionTypePos != std::string::npos);
  REQUIRE(staleDiagnosticPos != std::string::npos);
  REQUIRE(fallbackKindPos != std::string::npos);
  REQUIRE(fallbackStructPos != std::string::npos);
  REQUIRE(collectionFallbackPos != std::string::npos);
  CHECK(source.find("addCandidateTypeText(bindingFact->bindingTypeText",
                    semanticResolverPos) != std::string::npos);
  CHECK(source.find("bindingFact->bindingTypeTextId",
                    semanticResolverPos) != std::string::npos);
  CHECK(source.find("addCandidateTypeText(queryFact->bindingTypeText",
                    semanticResolverPos) != std::string::npos);
  CHECK(source.find("queryFact->bindingTypeTextId",
                    semanticResolverPos) != std::string::npos);
  CHECK(source.find("addCandidateTypeText(queryFact->queryTypeText",
                    semanticResolverPos) != std::string::npos);
  CHECK(source.find("queryFact->queryTypeTextId",
                    semanticResolverPos) != std::string::npos);
  CHECK(internedTypeResolverPos < collectionFallbackPos);
  CHECK(internedTypeResolverPos < fallbackKindPos);
  CHECK(bindingFactPos < collectionFallbackPos);
  CHECK(queryFactPos < collectionFallbackPos);
  CHECK(bindingFactPos < fallbackKindPos);
  CHECK(queryFactPos < fallbackKindPos);
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
  const size_t collectionFallbackPos =
      source.find("inferDirectResultValueCollectionInfo(",
                  metadataPos);
  const size_t structFallbackPos =
      source.find("inferDirectResultValueStructType(",
                  metadataPos);
  const size_t suppressFallbackPos =
      source.find("suppressSemanticCallDefinitionFallback",
                  metadataPos);
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
  const size_t inferKindPos = source.find("inferExprKind(expr.args[1], localsIn)", emitPos);
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
  CHECK(missingDiagnosticPos < rewriteFallbackPos);
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
  const size_t applyPos = source.find("auto applySemanticOperandResultType");
  REQUIRE(applyPos != std::string::npos);
  const size_t resolverPos = source.find("resolveQueryResultTypeText", applyPos);
  const size_t semanticResolvePos =
      source.find("semanticProgramResolveCallTargetString(", resolverPos);
  const size_t errorTypePos =
      source.find("resolveQueryResultTypeText(queryFact->resultErrorType,", resolverPos);
  const size_t errorIdPos = source.find("queryFact->resultErrorTypeId", errorTypePos);
  const size_t valueTypePos =
      source.find("resolveQueryResultTypeText(queryFact->resultValueType,", errorIdPos);
  const size_t valueIdPos = source.find("queryFact->resultValueTypeId", valueTypePos);
  const size_t fallbackPos = source.find("auto resolveLambdaReturnedValueExpr", valueIdPos);
  REQUIRE(resolverPos != std::string::npos);
  REQUIRE(semanticResolvePos != std::string::npos);
  REQUIRE(errorTypePos != std::string::npos);
  REQUIRE(errorIdPos != std::string::npos);
  REQUIRE(valueTypePos != std::string::npos);
  REQUIRE(valueIdPos != std::string::npos);
  REQUIRE(fallbackPos != std::string::npos);
  CHECK(resolverPos < semanticResolvePos);
  CHECK(semanticResolvePos < errorTypePos);
  CHECK(errorTypePos < errorIdPos);
  CHECK(errorIdPos < valueTypePos);
  CHECK(valueTypePos < valueIdPos);
  CHECK(valueIdPos < fallbackPos);
  CHECK(source.find("resultInfoOut.errorType = queryFact->resultErrorType;") ==
        std::string::npos);
  CHECK(source.find("return applySemanticTryValueType(queryFact->resultValueType,") ==
        std::string::npos);
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
  const size_t resolverPos = trySource.find("auto resolveTryFactTypeText");
  const size_t semanticResolvePos =
      trySource.find("semanticProgramResolveCallTargetString(", resolverPos);
  const size_t errorTypePos =
      trySource.find("resolveTryFactTypeText(tryFact->errorType,", semanticResolvePos);
  const size_t errorIdPos = trySource.find("tryFact->errorTypeId", errorTypePos);
  const size_t valueTypePos =
      trySource.find("resolveTryFactTypeText(tryFact->valueType,", errorIdPos);
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
  [Choice] moved{move(original)}
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
        "stale semantic-product sum variant metadata for sum payload move: /Choice -> left");
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
        "stale semantic-product sum variant metadata for sum payload move: /Choice -> right");
  CHECK(staleMovePayloadFailure.diagnosticInfo.message == staleMovePayloadFailure.message);

  const std::string dropSource = R"(
[struct]
LeftPayload() {
  [Reference<i32>] counter

  Destroy() {
    assign(dereference(this.counter), 100i32)
  }
}

[struct]
RightPayload() {
  [Reference<i32>] counter

  Destroy() {
    assign(dereference(this.counter), 1000i32)
  }
}

[sum]
Choice {
  [LeftPayload] left
  [RightPayload] right
}

[return<i32>]
main() {
  [i32 mut] counter{0i32}
  [uninitialized<Choice> mut] storage{uninitialized<Choice>()}
  init(storage, Choice{[left] LeftPayload{location(counter)}})
  drop(storage)
  return(counter)
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
      [](const primec::SemanticProgramQueryFact &entry) {
        return entry.scopePath == "/main" && entry.callName == "makeChoice";
      });
  REQUIRE(makeChoiceQuery != nullptr);
  CHECK(makeChoiceQuery->bindingTypeText == "Choice");
  CHECK(primec::semanticProgramQueryFactResolvedPath(
            semanticProgram, *makeChoiceQuery) == "/makeChoice");
  const auto *makeChoiceReturn =
      primec::ir_lowerer::findSemanticProductReturnFactByPath(
          semanticProductTargets, "/makeChoice");
  REQUIRE(makeChoiceReturn != nullptr);
  CHECK(makeChoiceReturn->bindingTypeText == "Choice");

  primec::Options options;
  options.entryPath = "/main";
  primec::Program loweringProgram = program;
  primec::SemanticProgram loweringSemanticProgram = semanticProgram;
  primec::IrModule ir;
  primec::IrPreparationFailure failure;
  REQUIRE(primec::prepareIrModule(loweringProgram,
                                  &loweringSemanticProgram,
                                  options,
                                  primec::IrValidationTarget::Native,
                                  ir,
                                  failure));
  CHECK(!ir.functions.empty());

  auto rewriteMakeChoiceQueryType =
      [](primec::SemanticProgram &semanticProduct, const std::string &typeText) {
        for (auto &fact : semanticProduct.queryFacts) {
          if (fact.scopePath == "/main" && fact.callName == "makeChoice") {
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
  REQUIRE(rewriteMakeChoiceQueryType(staleSemanticProgram, "OtherChoice"));

  primec::IrModule staleIr;
  primec::IrPreparationFailure staleFailure;
  CHECK_FALSE(primec::prepareIrModule(staleProgram,
                                      &staleSemanticProgram,
                                      options,
                                      primec::IrValidationTarget::Native,
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
                                      primec::IrValidationTarget::Native,
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
        return entry.scopePath == "/main" &&
               primec::semanticProgramQueryFactResolvedPath(
                   semanticProgram, entry) == "/Picker/makeChoice";
      });
  REQUIRE(makeChoiceQuery != nullptr);
  CHECK(makeChoiceQuery->bindingTypeText == "Choice");
  const auto *makeChoiceReturn =
      primec::ir_lowerer::findSemanticProductReturnFactByPath(
          semanticProductTargets, "/Picker/makeChoice");
  REQUIRE(makeChoiceReturn != nullptr);
  CHECK(makeChoiceReturn->bindingTypeText == "Choice");

  primec::Options options;
  options.entryPath = "/main";
  primec::Program loweringProgram = program;
  primec::SemanticProgram loweringSemanticProgram = semanticProgram;
  primec::IrModule ir;
  primec::IrPreparationFailure failure;
  REQUIRE(primec::prepareIrModule(loweringProgram,
                                  &loweringSemanticProgram,
                                  options,
                                  primec::IrValidationTarget::Native,
                                  ir,
                                  failure));
  CHECK(!ir.functions.empty());

  auto rewriteMakeChoiceQueryType =
      [](primec::SemanticProgram &semanticProduct, const std::string &typeText) {
        for (auto &fact : semanticProduct.queryFacts) {
          if (fact.scopePath == "/main" &&
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
  REQUIRE(rewriteMakeChoiceQueryType(staleSemanticProgram, "OtherChoice"));

  primec::IrModule staleIr;
  primec::IrPreparationFailure staleFailure;
  CHECK_FALSE(primec::prepareIrModule(staleProgram,
                                      &staleSemanticProgram,
                                      options,
                                      primec::IrValidationTarget::Native,
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
                                      primec::IrValidationTarget::Native,
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
  CHECK(error == "semantic-product contract version mismatch: expected 2, got 3");
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
    semanticProgram.publishedRoutingLookups.directCallTargetIdsByExpr.clear();
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
  semanticProgram.directCallTargets.push_back(primec::SemanticProgramDirectCallTarget{
      .scopePath = "/main",
      .callName = "/legacy",
      .sourceLine = 1,
      .sourceColumn = 1,
      .semanticNodeId = 41,
      .resolvedPathId =
          primec::semanticProgramInternCallTargetString(semanticProgram, "/semantic/target"),
      .stdlibSurfaceId = std::nullopt,
  });

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

  std::string error;

  CHECK_FALSE(primec::ir_lowerer::validateSemanticProductMethodCallCoverage(
      program, &semanticProgram, error));
  CHECK(error == "missing semantic-product method-call resolved path id: /main -> count");
}

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
    semanticProgram.publishedRoutingLookups.methodCallTargetIdsByExpr.clear();
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
  CHECK(error == "missing semantic-product method-call target: count");
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
      .stdlibSurfaceId = primec::StdlibSurfaceId::CollectionsVectorHelpers,
  });

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
      .stdlibSurfaceId = primec::StdlibSurfaceId::CollectionsVectorHelpers,
  });

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
      .stdlibSurfaceId = primec::StdlibSurfaceId::CollectionsVectorHelpers,
  });

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
      .stdlibSurfaceId = primec::StdlibSurfaceId::CollectionsMapHelpers,
  });
  semanticProgram.publishedRoutingLookups.directCallTargetIdsByExpr.insert_or_assign(
      5207, semanticProgram.directCallTargets.front().resolvedPathId);
  semanticProgram.publishedRoutingLookups.directCallStdlibSurfaceIdsByExpr.insert_or_assign(
      5207, primec::StdlibSurfaceId::CollectionsMapHelpers);

  primec::IrLowerer lowerer;
  primec::IrModule module;
  primec::DiagnosticSinkReport diagnosticInfo;
  std::string error;

  CHECK_FALSE(lowerer.lower(program, &semanticProgram, "/main", {}, {}, module, error, &diagnosticInfo));
  CHECK(error ==
        "native backend only supports arithmetic/comparison/clamp/min/max/abs/sign/saturate/"
        "convert/pointer/assign/increment/decrement calls in expressions "
        "(call=/contains, name=contains, args=1, method=false)");
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
      .stdlibSurfaceId = primec::StdlibSurfaceId::CollectionsVectorHelpers,
  });

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
      .stdlibSurfaceId = primec::StdlibSurfaceId::CollectionsVectorHelpers,
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
    semanticProgram.publishedRoutingLookups.bridgePathChoiceIdsByExpr.clear();
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

  primec::IrLowerer lowerer;
  primec::IrModule module;
  primec::DiagnosticSinkReport diagnosticInfo;
  std::string error;

  CHECK_FALSE(lowerer.lower(program, &semanticProgram, "/main", {}, {}, module, error, &diagnosticInfo));
  CHECK(error == "missing semantic-product collection specialization: /main -> local values");
  CHECK(diagnosticInfo.message == error);
}

TEST_CASE("ir lowerer rejects stale semantic-product collection metadata") {
  primec::Program program;

  primec::Definition mainDef;
  mainDef.fullPath = "/main";
  mainDef.semanticNodeId = 81;

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
    primec::IrLowerer lowerer;
    primec::IrModule module;
    return lowerer.lower(program,
                         &semanticProgram,
                         "/main",
                         {},
                         {},
                         module,
                         error,
                         &diagnosticInfo);
  };

  std::string error;
  primec::DiagnosticSinkReport diagnosticInfo;

  semanticProgram.bindingFacts.back().bindingTypeTextId =
      primec::semanticProgramInternCallTargetString(semanticProgram, "map<i32, i64>");
  semanticProgram.bindingFacts.back().bindingTypeText = "";
  refreshCollectionIds();
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
      primec::StdlibSurfaceId::CollectionsVectorHelpers;
  localAutoFact.initializerDirectCallStdlibSurfaceId =
      primec::StdlibSurfaceId::CollectionsVectorHelpers;
  localAutoFact.semanticNodeId = 47;
  localAutoFact.bindingTypeTextId =
      primec::semanticProgramInternCallTargetString(semanticProgram, "i32");
  localAutoFact.bindingTypeText = "";
  localAutoFact.initializerResolvedPathId =
      primec::semanticProgramInternCallTargetString(semanticProgram,
                                                    "/std/collections/vectorAt");
  localAutoFact.initializerDirectCallResolvedPathId =
      primec::semanticProgramInternCallTargetString(
          semanticProgram, "/std/collections/vectorAt__t25a78a513414c3bf");
  localAutoFact.initializerDirectCallReturnKindId =
      primec::semanticProgramInternCallTargetString(semanticProgram, "i32");
  semanticProgram.localAutoFacts.push_back(localAutoFact);

  std::string error;
  CHECK(primec::ir_lowerer::validateSemanticProductLocalAutoCoverage(
      program, &semanticProgram, error));
  CHECK(error.empty());

  semanticProgram.localAutoFacts.back().initializerDirectCallStdlibSurfaceId =
      primec::StdlibSurfaceId::CollectionsMapHelpers;
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

  primec::IrLowerer lowerer;
  primec::IrModule module;
  primec::DiagnosticSinkReport diagnosticInfo;
  std::string error;

  CHECK_FALSE(lowerer.lower(program, &semanticProgram, "/main", {}, {}, module, error, &diagnosticInfo));
  CHECK(error == "missing semantic-product return binding type: /main");
  CHECK(diagnosticInfo.message == error);
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
  CHECK(error == "missing semantic-product query resolved path id: lookup");
  CHECK(diagnosticInfo.message == error);
}

TEST_CASE("ir lowerer rejects stale semantic-product query facts") {
  primec::Program program;

  primec::Definition mainDef;
  mainDef.fullPath = "/main";
  primec::Expr callExpr;
  callExpr.kind = primec::Expr::Kind::Call;
  callExpr.name = "lookup";
  callExpr.semanticNodeId = 8302;
  mainDef.statements.push_back(callExpr);
  program.definitions.push_back(mainDef);

  primec::SemanticProgram semanticProgram;
  semanticProgram.entryPath = "/main";
  semanticProgram.directCallTargets.push_back(primec::SemanticProgramDirectCallTarget{
      .scopePath = "/main",
      .callName = "lookup",
      .sourceLine = 1,
      .sourceColumn = 1,
      .semanticNodeId = 8302,
      .resolvedPathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/fresh_lookup"),
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
      .semanticNodeId = 8302,
      .resolvedPathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/stale_lookup"),
  });
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
  CHECK(error == "stale semantic-product query fact: lookup");
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
        primec::SemanticProductContractVersionV2);
  CHECK(primec::semanticProgramFactFamilyIsSemanticProductOwned("directCallTargets"));
  CHECK(primec::semanticProgramFactFamilyIsSemanticProductOwned("bindingFacts"));
  CHECK(primec::semanticProgramFactFamilyIsSemanticProductOwned("onErrorFacts"));
  CHECK(primec::semanticProgramFactFamilyIsAstProvenanceOwned("definitions"));
  CHECK(primec::semanticProgramFactFamilyIsAstProvenanceOwned("executions"));
  CHECK(primec::semanticProgramFactFamilyOwnership("publishedRoutingLookups") ==
        primec::SemanticProgramFactOwnership::DerivedIndex);
  CHECK_FALSE(primec::semanticProgramFactFamilyOwnership("unknownFacts").has_value());
}

TEST_CASE("vm backend conformance keeps semantic-product contract v2") {
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
  const auto nativeConformance = runConformance("native");

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
  CHECK(nativeConformance.backendKind == "native");
  CHECK(cppConformance.emitResult.exitCode == 0);
  CHECK(vmConformance.emitResult.exitCode == 7);
  CHECK(nativeConformance.emitResult.exitCode == 0);

  const std::string cpp = readTextFile(cppConformance.outputPath);
  CHECK(cpp.find("static int64_t ps_fn_0") != std::string::npos);
  CHECK(std::filesystem::exists(nativeConformance.outputPath));
  CHECK(std::filesystem::is_regular_file(nativeConformance.outputPath));
  CHECK(std::filesystem::file_size(nativeConformance.outputPath) > 0);
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
  const auto nativeConformance = runConformance("native");

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
  CHECK_FALSE(failure->failure.message.empty());
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
  CHECK(output.semanticPhaseCounters.semanticProductBuild.allocatedBytes > 0);
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

TEST_CASE("compile pipeline benchmark worker-count equivalence keeps semantic-product families stable despite formatting variance across 1,2,4 workers") {
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
  CHECK(source.find("prepareIrModule(program, semanticProgram, options, validationTarget, ir, prepFailure)") !=
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

TEST_CASE("primevm uses shared ir preparation helper") {
  const std::filesystem::path cwd = std::filesystem::current_path();
  std::filesystem::path mainPath = cwd / "src" / "primevm_main.cpp";
  if (!std::filesystem::exists(mainPath)) {
    mainPath = cwd.parent_path() / "src" / "primevm_main.cpp";
  }
  REQUIRE(std::filesystem::exists(mainPath));

  const std::string source = readTextFile(mainPath);
  const size_t replayFastPathPos =
      source.find("if (!options.debugReplayPath.empty() && options.dumpStage.empty())");
  const size_t compilePipelinePos = source.find(
      "runCompilePipelineResult(options, pipelineError, error, &pipelineDiagnosticInfo)");
  REQUIRE(replayFastPathPos != std::string::npos);
  REQUIRE(compilePipelinePos != std::string::npos);
  CHECK(replayFastPathPos < compilePipelinePos);
  CHECK(source.find("vmIrBackendDiagnostics()") != std::string::npos);
  CHECK(source.find("normalizeVmLoweringError") != std::string::npos);
  CHECK(source.find("std::get_if<primec::CompilePipelineFailureResult>(&pipelineResult)") !=
        std::string::npos);
  CHECK(source.find("describeCompilePipelineFailure(*pipelineFailure)") != std::string::npos);
  CHECK(source.find("describeIrPreparationFailure(") != std::string::npos);
  CHECK(source.find("pipelineOutput.hasSemanticProgram ? &pipelineOutput.semanticProgram : nullptr") !=
        std::string::npos);
  CHECK(source.find("prepareIrModule(program, semanticProgram, options, primec::IrValidationTarget::Vm, ir, irFailure)") !=
        std::string::npos);
  CHECK(source.find("findIrBackend(\"vm\")") == std::string::npos);
  CHECK(source.find("CompilePipelineErrorStage::Import") == std::string::npos);
  CHECK(source.find("IrPreparationFailureStage::Validation") == std::string::npos);
  CHECK(source.find("IrLowerer lowerer") == std::string::npos);
  CHECK(source.find("inlineIrModuleCalls(ir, error)") == std::string::npos);
  CHECK(source.find("validateIrModule(ir, primec::IrValidationTarget::Vm, error)") == std::string::npos);
}

TEST_CASE("primevm debug replay uses shared JSON parsing helpers") {
  const std::filesystem::path cwd = std::filesystem::current_path();
  std::filesystem::path mainPath = cwd / "src" / "primevm_main.cpp";
  if (!std::filesystem::exists(mainPath)) {
    mainPath = cwd.parent_path() / "src" / "primevm_main.cpp";
  }
  REQUIRE(std::filesystem::exists(mainPath));

  const std::string source = readTextFile(mainPath);
  CHECK(source.find("parseJsonPayload(line, parsedLine, error)") != std::string::npos);
  CHECK(source.find("jsonObjectField(parsedLine, \"snapshot\")") != std::string::npos);
  CHECK(source.find("jsonObjectField(parsedLine, \"snapshot_payload\")") != std::string::npos);
  CHECK(source.find("jsonObjectField(parsedLine, \"reason\")") != std::string::npos);
  CHECK(source.find("jsonNumberField(object, key, parsedValue)") != std::string::npos);
  CHECK(source.find("extractJsonUnsignedField(std::string_view json") == std::string::npos);
  CHECK(source.find("extractJsonStringField(std::string_view json") == std::string::npos);
  CHECK(source.find("extractJsonObjectField(std::string_view json") == std::string::npos);
}

TEST_CASE("ir pipeline helper skips semantic product for non-consuming requests") {
  const std::filesystem::path cwd = std::filesystem::current_path();
  std::filesystem::path helperPath = cwd / "tests" / "unit" / "test_ir_pipeline_helpers.h";
  if (!std::filesystem::exists(helperPath)) {
    helperPath = cwd.parent_path() / "tests" / "unit" / "test_ir_pipeline_helpers.h";
  }
  REQUIRE(std::filesystem::exists(helperPath));

  const std::string source = readTextFile(helperPath);
  CHECK(source.find("CompilePipelineSemanticProductIntentForTesting::SkipForNonConsumingPath") !=
        std::string::npos);
  CHECK(source.find("semanticProgramOut != nullptr") != std::string::npos);
  CHECK(source.find("applyCompilePipelineSemanticProductIntentForTesting(options, semanticProductIntent);") !=
        std::string::npos);
}

TEST_CASE("semantics helper skips semantic product for non-consuming requests") {
  const std::filesystem::path cwd = std::filesystem::current_path();
  std::filesystem::path helperPath = cwd / "tests" / "unit" / "test_semantics_helpers.h";
  if (!std::filesystem::exists(helperPath)) {
    helperPath = cwd.parent_path() / "tests" / "unit" / "test_semantics_helpers.h";
  }
  REQUIRE(std::filesystem::exists(helperPath));

  const std::string source = readTextFile(helperPath);
  CHECK(source.find("enum class SemanticsCompilePipelineSemanticProductIntentForTesting") !=
        std::string::npos);
  CHECK(source.find("SemanticsCompilePipelineSemanticProductIntentForTesting::SkipForNonConsumingPath") !=
        std::string::npos);
  CHECK(source.find("applySemanticsCompilePipelineSemanticProductIntentForTesting(") != std::string::npos);
}

TEST_CASE("backend entrypoint inventory avoids inactive follow-up pointer") {
  const std::filesystem::path cwd = std::filesystem::current_path();
  std::filesystem::path inventoryPath =
      cwd / "docs" / "semantic_product_backend_entrypoint_inventory.md";
  if (!std::filesystem::exists(inventoryPath)) {
    inventoryPath = cwd.parent_path() / "docs" / "semantic_product_backend_entrypoint_inventory.md";
  }
  REQUIRE(std::filesystem::exists(inventoryPath));

  const std::string source = readTextFile(inventoryPath);
  CHECK(source.find("completed Group 15 non-consuming entrypoint audit") != std::string::npos);
  CHECK(source.find("any new\nnon-consuming family needs a fresh TODO") != std::string::npos);
  CHECK(source.find("no remaining non-consuming families with open follow-up leaves") !=
        std::string::npos);
  CHECK(source.find("It exists to support Group 15 `P1-06` follow-up slicing") == std::string::npos);
}

TEST_CASE("symbol-id migration inventory avoids inactive follow-up pointer") {
  const std::filesystem::path cwd = std::filesystem::current_path();
  std::filesystem::path inventoryPath =
      cwd / "docs" / "semantic_symbol_id_migration_inventory.md";
  if (!std::filesystem::exists(inventoryPath)) {
    inventoryPath = cwd.parent_path() / "docs" / "semantic_symbol_id_migration_inventory.md";
  }
  REQUIRE(std::filesystem::exists(inventoryPath));

  const std::string source = readTextFile(inventoryPath);
  CHECK(source.find("completed semantic-product fact-family migration") !=
        std::string::npos);
  CHECK(source.find("None. The current hot-path SymbolId migration family list is fully landed.") !=
        std::string::npos);
  CHECK(source.find("Add a concrete SymbolId migration TODO before adding another fact family") !=
        std::string::npos);
  CHECK(source.find("## Remaining families and next one-leaf follow-ups") == std::string::npos);
  CHECK(source.find("fact families that still retain") == std::string::npos);
}

TEST_SUITE_END();
