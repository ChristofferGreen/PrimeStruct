#include "test_ir_pipeline_validation_helpers.h"

TEST_SUITE_BEGIN("primestruct.ir.pipeline.validation");

namespace {

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

template <typename Entry, typename Predicate>
bool anySemanticEntry(const std::vector<Entry> &entries, const Predicate &predicate) {
  return std::any_of(entries.begin(), entries.end(), predicate);
}

const primec::SemanticProgramModuleResolvedArtifacts *findModuleArtifacts(const primec::SemanticProgram &semanticProgram,
                                                                          std::string_view moduleKey) {
  return findSemanticEntry(
      semanticProgram.moduleResolvedArtifacts,
      [moduleKey](const primec::SemanticProgramModuleResolvedArtifacts &entry) {
        return entry.identity.moduleKey == moduleKey;
      });
}

std::string_view resolveSemanticText(const primec::SemanticProgram &semanticProgram,
                                     primec::SymbolId id,
                                     const std::string &fallback) {
  const std::string_view resolved =
      primec::semanticProgramResolveCallTargetString(semanticProgram, id);
  return resolved.empty() ? std::string_view(fallback) : resolved;
}

std::string_view bindingFactScopePath(const primec::SemanticProgram &semanticProgram,
                                      const primec::SemanticProgramBindingFact &entry) {
  return resolveSemanticText(semanticProgram, entry.scopePathId, entry.scopePath);
}

std::string_view bindingFactSiteKind(const primec::SemanticProgram &semanticProgram,
                                     const primec::SemanticProgramBindingFact &entry) {
  return resolveSemanticText(semanticProgram, entry.siteKindId, entry.siteKind);
}

std::string_view bindingFactName(const primec::SemanticProgram &semanticProgram,
                                 const primec::SemanticProgramBindingFact &entry) {
  return resolveSemanticText(semanticProgram, entry.nameId, entry.name);
}

} // namespace

TEST_CASE("semantics validate publishes allowlisted pilot routing artifacts") {
  const std::string source = R"(
import /std/collections/*

[return<i32>]
id_i32([i32] value) {
  return(value)
}

[effects(heap_alloc), return<i32>]
main() {
  [auto] selected{id_i32(1i32)}
  [auto] values{vector<i32>(1i32)}
  [i32] viaMethod{values.count()}
  [i32] viaBridge{count(values)}
  return(plus(selected, plus(viaMethod, viaBridge)))
}
)";

  primec::Program program;
  std::string error;
  const std::vector<std::string> collectorAllowlist = {
      "direct_call_targets",
      "method_call_targets",
      "bridge_path_choices",
      "callable_summaries",
  };
  const std::vector<std::string> defaults = {"io_out", "io_err"};
  primec::SemanticProgram semanticProgram;
  const std::filesystem::path tempPath = makeTempIrPipelineSourcePath();
  {
    std::ofstream file(tempPath);
    REQUIRE(file.good());
    file << source;
  }

  primec::Options options;
  options.inputPath = tempPath.string();
  options.entryPath = "/main";
  options.defaultEffects = defaults;
  options.entryDefaultEffects = defaults;
  options.benchmarkSemanticFactFamiliesSpecified = true;
  options.benchmarkSemanticFactFamilies = collectorAllowlist;
  applyCompilePipelineSemanticProductIntentForTesting(
      options, CompilePipelineSemanticProductIntentForTesting::Require);
  primec::addDefaultStdlibInclude(options.inputPath, options.importPaths);

  primec::CompilePipelineOutput output;
  primec::CompilePipelineErrorStage errorStage = primec::CompilePipelineErrorStage::None;
  const bool ok = primec::runCompilePipeline(options, output, errorStage, error);
  std::error_code ec;
  std::filesystem::remove(tempPath, ec);
  REQUIRE(ok);
  CHECK(error.empty());
  REQUIRE(output.hasSemanticProgram);
  program = std::move(output.program);
  semanticProgram = std::move(output.semanticProgram);

  const auto *directEntry = findSemanticEntry(
      primec::semanticProgramDirectCallTargetView(semanticProgram),
      [&semanticProgram](const primec::SemanticProgramDirectCallTarget &entry) {
        return entry.scopePath == "/main" && entry.callName == "id_i32" &&
               primec::semanticProgramDirectCallTargetResolvedPath(semanticProgram, entry) == "/id_i32";
      });
  const auto *methodEntry = findSemanticEntry(
      primec::semanticProgramMethodCallTargetView(semanticProgram),
      [&semanticProgram](const primec::SemanticProgramMethodCallTarget &entry) {
        return entry.scopePath == "/main" && entry.methodName == "count" &&
               primec::semanticProgramMethodCallTargetResolvedPath(semanticProgram, entry) ==
                   "/std/collections/vector/count";
      });
  const auto *bridgeEntry = findSemanticEntry(
      primec::semanticProgramBridgePathChoiceView(semanticProgram),
      [&semanticProgram](const primec::SemanticProgramBridgePathChoice &entry) {
        return primec::semanticProgramBridgePathChoiceHelperName(semanticProgram, entry) == "count" &&
               primec::semanticProgramResolveCallTargetString(semanticProgram, entry.chosenPathId) ==
                   "/std/collections/vector/count";
      });
  const auto *summaryEntry = findSemanticEntry(
      primec::semanticProgramCallableSummaryView(semanticProgram),
      [&semanticProgram](const primec::SemanticProgramCallableSummary &entry) {
        return primec::semanticProgramCallableSummaryFullPath(semanticProgram, entry) == "/main";
      });

  REQUIRE(directEntry != nullptr);
  REQUIRE(methodEntry != nullptr);
  REQUIRE(bridgeEntry != nullptr);
  REQUIRE(summaryEntry != nullptr);
  CHECK_FALSE(summaryEntry->isExecution);
  CHECK(summaryEntry->returnKind == "i32");

  CHECK(semanticProgram.bindingFacts.empty());
  CHECK(semanticProgram.returnFacts.empty());
  CHECK(semanticProgram.localAutoFacts.empty());
  CHECK(semanticProgram.queryFacts.empty());
  CHECK(semanticProgram.tryFacts.empty());
  CHECK(semanticProgram.onErrorFacts.empty());

  const auto *mainArtifacts = findModuleArtifacts(semanticProgram, "/");
  REQUIRE(mainArtifacts != nullptr);
  CHECK(anySemanticEntry(
      mainArtifacts->directCallTargetIndices,
      [&](std::size_t index) {
        return index < semanticProgram.directCallTargets.size() &&
               primec::semanticProgramDirectCallTargetResolvedPath(semanticProgram,
                                                                   semanticProgram.directCallTargets[index]) ==
                   "/id_i32";
      }));
  CHECK(anySemanticEntry(
      mainArtifacts->methodCallTargetIndices,
      [&](std::size_t index) {
        return index < semanticProgram.methodCallTargets.size() &&
               primec::semanticProgramMethodCallTargetResolvedPath(semanticProgram,
                                                                   semanticProgram.methodCallTargets[index]) ==
                   "/std/collections/vector/count";
      }));
  CHECK(anySemanticEntry(
      mainArtifacts->callableSummaryIndices,
      [&](std::size_t index) {
        return index < semanticProgram.callableSummaries.size() &&
               primec::semanticProgramCallableSummaryFullPath(semanticProgram,
                                                              semanticProgram.callableSummaries[index]) == "/main";
      }));
  CHECK_FALSE(anySemanticEntry(
      mainArtifacts->callableSummaryIndices,
      [&](std::size_t index) {
        return index < semanticProgram.callableSummaries.size() &&
               primec::semanticProgramCallableSummaryFullPath(semanticProgram,
                                                              semanticProgram.callableSummaries[index])
                   .starts_with("/std/collections/");
      }));

  CHECK(anySemanticEntry(
      semanticProgram.moduleResolvedArtifacts,
      [&](const primec::SemanticProgramModuleResolvedArtifacts &module) {
        return module.identity.moduleKey != "/" &&
               anySemanticEntry(
                   module.callableSummaryIndices,
                   [&](std::size_t index) {
                     return index < semanticProgram.callableSummaries.size() &&
                            primec::semanticProgramCallableSummaryFullPath(
                                semanticProgram, semanticProgram.callableSummaries[index])
                                .starts_with("/std/collections/");
                   });
      }));
  CHECK(anySemanticEntry(
      semanticProgram.moduleResolvedArtifacts,
      [&](const primec::SemanticProgramModuleResolvedArtifacts &module) {
        return anySemanticEntry(
            module.bridgePathChoiceIndices,
            [&](std::size_t index) {
              return index < semanticProgram.bridgePathChoices.size() &&
                     primec::semanticProgramResolveCallTargetString(
                         semanticProgram, semanticProgram.bridgePathChoices[index].chosenPathId) ==
                         "/std/collections/vector/count";
            });
      }));
}

TEST_CASE("semantics validate publishes return and import artifacts through public semantic product views") {
  const std::string source = R"(
import /std/math/abs

[return<i32>]
helper([i32] input) {
  [i32] normalized{abs(input)}
  return(normalized)
}

[return<i32>]
main() {
  [i32] value{helper(-3i32)}
  return(value)
}
)";

  const std::vector<std::string> collectorAllowlist = {
      "return_facts",
      "callable_summaries",
  };
  const std::vector<std::string> defaults = {"io_out", "io_err"};
  const std::filesystem::path tempPath = makeTempIrPipelineSourcePath();
  {
    std::ofstream file(tempPath);
    REQUIRE(file.good());
    file << source;
  }

  primec::Options options;
  options.inputPath = tempPath.string();
  options.entryPath = "/main";
  options.defaultEffects = defaults;
  options.entryDefaultEffects = defaults;
  options.benchmarkSemanticFactFamiliesSpecified = true;
  options.benchmarkSemanticFactFamilies = collectorAllowlist;
  applyCompilePipelineSemanticProductIntentForTesting(
      options, CompilePipelineSemanticProductIntentForTesting::Require);
  primec::addDefaultStdlibInclude(options.inputPath, options.importPaths);

  primec::CompilePipelineOutput output;
  primec::CompilePipelineErrorStage errorStage =
      primec::CompilePipelineErrorStage::None;
  std::string error;
  const bool ok = primec::runCompilePipeline(options, output, errorStage, error);
  std::error_code ec;
  std::filesystem::remove(tempPath, ec);

  INFO(error);
  REQUIRE(ok);
  CHECK(error.empty());
  REQUIRE(output.hasSemanticProgram);
  const primec::SemanticProgram &semanticProgram = output.semanticProgram;

  const auto *helperSummary = findSemanticEntry(
      primec::semanticProgramCallableSummaryView(semanticProgram),
      [&semanticProgram](const primec::SemanticProgramCallableSummary &entry) {
        return primec::semanticProgramCallableSummaryFullPath(semanticProgram, entry) ==
               "/helper";
      });
  const auto *mainSummary = findSemanticEntry(
      primec::semanticProgramCallableSummaryView(semanticProgram),
      [&semanticProgram](const primec::SemanticProgramCallableSummary &entry) {
        return primec::semanticProgramCallableSummaryFullPath(semanticProgram, entry) ==
               "/main";
      });
  const auto *helperReturn = findSemanticEntry(
      primec::semanticProgramReturnFactView(semanticProgram),
      [&semanticProgram](const primec::SemanticProgramReturnFact &entry) {
        return primec::semanticProgramReturnFactDefinitionPath(semanticProgram, entry) ==
               "/helper";
      });
  const auto *mainReturn = findSemanticEntry(
      primec::semanticProgramReturnFactView(semanticProgram),
      [&semanticProgram](const primec::SemanticProgramReturnFact &entry) {
        return primec::semanticProgramReturnFactDefinitionPath(semanticProgram, entry) ==
               "/main";
      });
  REQUIRE(helperSummary != nullptr);
  REQUIRE(mainSummary != nullptr);
  REQUIRE(helperReturn != nullptr);
  REQUIRE(mainReturn != nullptr);
  CHECK(helperSummary->returnKind == "i32");
  CHECK(mainSummary->returnKind == "i32");
  CHECK(helperReturn->returnKind == "i32");
  CHECK(mainReturn->returnKind == "i32");
  CHECK(helperReturn->bindingTypeText == "i32");
  CHECK(mainReturn->bindingTypeText == "i32");

  const auto *rootArtifacts = findModuleArtifacts(semanticProgram, "/");
  REQUIRE(rootArtifacts != nullptr);
  CHECK(anySemanticEntry(
      semanticProgram.sourceImports,
      [](const std::string &importPath) {
        return importPath == "/std/math/abs";
      }));
  CHECK(anySemanticEntry(
      semanticProgram.imports,
      [](const std::string &importPath) {
        return importPath == "/std/math/abs";
      }));

  CHECK(anySemanticEntry(
      rootArtifacts->returnFactIndices,
      [&](std::size_t index) {
        return index < semanticProgram.returnFacts.size() &&
               primec::semanticProgramReturnFactDefinitionPath(
                   semanticProgram, semanticProgram.returnFacts[index]) == "/helper";
      }));
  CHECK(anySemanticEntry(
      rootArtifacts->returnFactIndices,
      [&](std::size_t index) {
        return index < semanticProgram.returnFacts.size() &&
               primec::semanticProgramReturnFactDefinitionPath(
                   semanticProgram, semanticProgram.returnFacts[index]) == "/main";
      }));
  CHECK(anySemanticEntry(
      rootArtifacts->callableSummaryIndices,
      [&](std::size_t index) {
        return index < semanticProgram.callableSummaries.size() &&
               primec::semanticProgramCallableSummaryFullPath(
                   semanticProgram, semanticProgram.callableSummaries[index]) == "/helper";
      }));
  CHECK(anySemanticEntry(
      rootArtifacts->callableSummaryIndices,
      [&](std::size_t index) {
        return index < semanticProgram.callableSummaries.size() &&
               primec::semanticProgramCallableSummaryFullPath(
                   semanticProgram, semanticProgram.callableSummaries[index]) == "/main";
      }));
}

TEST_CASE("semantics validate publishes struct field metadata and parameter binding facts") {
  const std::string source = R"(
Particle {
  [i32] left
  [i64] right
}

[return<i32>]
sum([Particle] particle, [i32] extra) {
  return(plus(particle.left, plus(convert<i32>(particle.right), extra)))
}

[return<i32>]
main() {
  return(0i32)
}
)";

  const std::vector<std::string> collectorAllowlist = {
      "binding_facts",
      "type_metadata",
      "struct_field_metadata",
  };
  const std::vector<std::string> defaults = {"io_out", "io_err"};
  const std::filesystem::path tempPath = makeTempIrPipelineSourcePath();
  {
    std::ofstream file(tempPath);
    REQUIRE(file.good());
    file << source;
  }

  primec::Options options;
  options.inputPath = tempPath.string();
  options.entryPath = "/main";
  options.defaultEffects = defaults;
  options.entryDefaultEffects = defaults;
  options.benchmarkSemanticFactFamiliesSpecified = true;
  options.benchmarkSemanticFactFamilies = collectorAllowlist;
  applyCompilePipelineSemanticProductIntentForTesting(
      options, CompilePipelineSemanticProductIntentForTesting::Require);

  primec::CompilePipelineOutput output;
  primec::CompilePipelineErrorStage errorStage =
      primec::CompilePipelineErrorStage::None;
  std::string error;
  const bool ok = primec::runCompilePipeline(options, output, errorStage, error);
  std::error_code ec;
  std::filesystem::remove(tempPath, ec);

  INFO(error);
  REQUIRE(ok);
  CHECK(error.empty());
  REQUIRE(output.hasSemanticProgram);
  const primec::SemanticProgram &semanticProgram = output.semanticProgram;
  INFO(primec::formatSemanticProgram(semanticProgram));

  const auto *particleType =
      primec::semanticProgramLookupTypeMetadata(semanticProgram, "/Particle");
  REQUIRE(particleType != nullptr);
  CHECK(particleType->category == "struct");
  CHECK(particleType->fieldCount == 2);

  const auto particleFields =
      primec::semanticProgramStructFieldMetadataView(semanticProgram, "/Particle");
  REQUIRE(particleFields.size() == 2);
  const auto *leftField = findSemanticEntry(
      particleFields,
      [](const primec::SemanticProgramStructFieldMetadata &entry) {
        return entry.structPath == "/Particle" && entry.fieldName == "left";
      });
  const auto *rightField = findSemanticEntry(
      particleFields,
      [](const primec::SemanticProgramStructFieldMetadata &entry) {
        return entry.structPath == "/Particle" && entry.fieldName == "right";
      });
  REQUIRE(leftField != nullptr);
  REQUIRE(rightField != nullptr);
  CHECK(leftField->fieldIndex == 0);
  CHECK(leftField->bindingTypeText == "i32");
  CHECK(rightField->fieldIndex == 1);
  CHECK(rightField->bindingTypeText == "i64");

  const auto *particleParameter = findSemanticEntry(
      primec::semanticProgramBindingFactView(semanticProgram),
      [&semanticProgram](const primec::SemanticProgramBindingFact &entry) {
        return bindingFactScopePath(semanticProgram, entry) == "/sum" &&
               bindingFactSiteKind(semanticProgram, entry) == "parameter" &&
               bindingFactName(semanticProgram, entry) == "particle";
      });
  const auto *extraParameter = findSemanticEntry(
      primec::semanticProgramBindingFactView(semanticProgram),
      [&semanticProgram](const primec::SemanticProgramBindingFact &entry) {
        return bindingFactScopePath(semanticProgram, entry) == "/sum" &&
               bindingFactSiteKind(semanticProgram, entry) == "parameter" &&
               bindingFactName(semanticProgram, entry) == "extra";
      });
  REQUIRE(particleParameter != nullptr);
  REQUIRE(extraParameter != nullptr);
  CHECK((particleParameter->bindingTypeText == "Particle" ||
         particleParameter->bindingTypeText == "/Particle"));
  CHECK(extraParameter->bindingTypeText == "i32");
  CHECK(primec::semanticProgramBindingFactResolvedPath(
            semanticProgram, *particleParameter) == "/sum/particle");
  CHECK(primec::semanticProgramBindingFactResolvedPath(
            semanticProgram, *extraParameter) == "/sum/extra");
}

TEST_CASE("semantics validate publishes module artifacts in import order") {
  const std::string source = R"(
import /std/math/max
import /std/math/abs

[return<i32>]
main() {
  return(plus(max(3i32, 1i32), abs(-4i32)))
}
)";

  const std::vector<std::string> collectorAllowlist = {
      "callable_summaries",
  };
  const std::vector<std::string> defaults = {"io_out", "io_err"};
  const std::filesystem::path tempPath = makeTempIrPipelineSourcePath();
  {
    std::ofstream file(tempPath);
    REQUIRE(file.good());
    file << source;
  }

  primec::Options options;
  options.inputPath = tempPath.string();
  options.entryPath = "/main";
  options.defaultEffects = defaults;
  options.entryDefaultEffects = defaults;
  options.benchmarkSemanticFactFamiliesSpecified = true;
  options.benchmarkSemanticFactFamilies = collectorAllowlist;
  applyCompilePipelineSemanticProductIntentForTesting(
      options, CompilePipelineSemanticProductIntentForTesting::Require);
  primec::addDefaultStdlibInclude(options.inputPath, options.importPaths);

  primec::CompilePipelineOutput output;
  primec::CompilePipelineErrorStage errorStage =
      primec::CompilePipelineErrorStage::None;
  std::string error;
  const bool ok = primec::runCompilePipeline(options, output, errorStage, error);
  std::error_code ec;
  std::filesystem::remove(tempPath, ec);

  REQUIRE(ok);
  CHECK(error.empty());
  REQUIRE(output.hasSemanticProgram);
  const primec::SemanticProgram &semanticProgram = output.semanticProgram;

  const auto *rootArtifacts = findModuleArtifacts(semanticProgram, "/");
  const auto *maxArtifacts =
      findModuleArtifacts(semanticProgram, "/std/math/max");
  const auto *absArtifacts =
      findModuleArtifacts(semanticProgram, "/std/math/abs");
  REQUIRE(rootArtifacts != nullptr);
  REQUIRE(maxArtifacts != nullptr);
  REQUIRE(absArtifacts != nullptr);

  CHECK(rootArtifacts->identity.stableOrder < maxArtifacts->identity.stableOrder);
  CHECK(maxArtifacts->identity.stableOrder < absArtifacts->identity.stableOrder);

  const auto rootIt = std::find_if(
      semanticProgram.moduleResolvedArtifacts.begin(),
      semanticProgram.moduleResolvedArtifacts.end(),
      [](const primec::SemanticProgramModuleResolvedArtifacts &module) {
        return module.identity.moduleKey == "/";
      });
  const auto maxIt = std::find_if(
      semanticProgram.moduleResolvedArtifacts.begin(),
      semanticProgram.moduleResolvedArtifacts.end(),
      [](const primec::SemanticProgramModuleResolvedArtifacts &module) {
        return module.identity.moduleKey == "/std/math/max";
      });
  const auto absIt = std::find_if(
      semanticProgram.moduleResolvedArtifacts.begin(),
      semanticProgram.moduleResolvedArtifacts.end(),
      [](const primec::SemanticProgramModuleResolvedArtifacts &module) {
        return module.identity.moduleKey == "/std/math/abs";
      });
  REQUIRE(rootIt != semanticProgram.moduleResolvedArtifacts.end());
  REQUIRE(maxIt != semanticProgram.moduleResolvedArtifacts.end());
  REQUIRE(absIt != semanticProgram.moduleResolvedArtifacts.end());
  CHECK(rootIt < maxIt);
  CHECK(maxIt < absIt);

  CHECK(anySemanticEntry(
      rootArtifacts->callableSummaryIndices,
      [&](std::size_t index) {
        return index < semanticProgram.callableSummaries.size() &&
               primec::semanticProgramCallableSummaryFullPath(
                   semanticProgram, semanticProgram.callableSummaries[index]) ==
                   "/main";
      }));
  CHECK(anySemanticEntry(
      maxArtifacts->callableSummaryIndices,
      [&](std::size_t index) {
        return index < semanticProgram.callableSummaries.size() &&
               primec::semanticProgramCallableSummaryFullPath(
                   semanticProgram, semanticProgram.callableSummaries[index]) ==
                   "/std/math/max";
      }));
  CHECK(anySemanticEntry(
      absArtifacts->callableSummaryIndices,
      [&](std::size_t index) {
        return index < semanticProgram.callableSummaries.size() &&
               primec::semanticProgramCallableSummaryFullPath(
                   semanticProgram, semanticProgram.callableSummaries[index]) ==
                   "/std/math/abs";
      }));
}

TEST_SUITE_END();
