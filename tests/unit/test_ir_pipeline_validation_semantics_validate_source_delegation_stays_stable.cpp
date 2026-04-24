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

TEST_CASE("semantics validate publishes binding return and import artifacts through public semantic product views") {
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
      "binding_facts",
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
  const auto *helperBinding = findSemanticEntry(
      primec::semanticProgramBindingFactView(semanticProgram),
      [&semanticProgram](const primec::SemanticProgramBindingFact &entry) {
        return entry.scopePath == "/helper" &&
               entry.siteKind == "local" &&
               entry.name == "normalized" &&
               primec::semanticProgramBindingFactResolvedPath(semanticProgram, entry) ==
                   "/helper/normalized";
      });
  const auto *mainBinding = findSemanticEntry(
      primec::semanticProgramBindingFactView(semanticProgram),
      [&semanticProgram](const primec::SemanticProgramBindingFact &entry) {
        return entry.scopePath == "/main" &&
               entry.siteKind == "local" &&
               entry.name == "value" &&
               primec::semanticProgramBindingFactResolvedPath(semanticProgram, entry) ==
                   "/main/value";
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
  REQUIRE(helperBinding != nullptr);
  REQUIRE(mainBinding != nullptr);
  REQUIRE(helperReturn != nullptr);
  REQUIRE(mainReturn != nullptr);
  CHECK(helperSummary->returnKind == "i32");
  CHECK(mainSummary->returnKind == "i32");
  CHECK(helperBinding->bindingTypeText == "i32");
  CHECK(mainBinding->bindingTypeText == "i32");
  CHECK(helperReturn->returnKind == "i32");
  CHECK(mainReturn->returnKind == "i32");
  CHECK(helperReturn->bindingTypeText == "i32");
  CHECK(mainReturn->bindingTypeText == "i32");

  const auto *rootArtifacts = findModuleArtifacts(semanticProgram, "/");
  const auto *absArtifacts =
      findModuleArtifacts(semanticProgram, "/std/math/abs");
  REQUIRE(rootArtifacts != nullptr);
  REQUIRE(absArtifacts != nullptr);

  CHECK(anySemanticEntry(
      rootArtifacts->bindingFactIndices,
      [&](std::size_t index) {
        return index < semanticProgram.bindingFacts.size() &&
               semanticProgram.bindingFacts[index].scopePath == "/helper" &&
               semanticProgram.bindingFacts[index].name == "normalized";
      }));
  CHECK(anySemanticEntry(
      rootArtifacts->bindingFactIndices,
      [&](std::size_t index) {
        return index < semanticProgram.bindingFacts.size() &&
               semanticProgram.bindingFacts[index].scopePath == "/main" &&
               semanticProgram.bindingFacts[index].name == "value";
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
  CHECK(anySemanticEntry(
      absArtifacts->callableSummaryIndices,
      [&](std::size_t index) {
        return index < semanticProgram.callableSummaries.size() &&
               primec::semanticProgramCallableSummaryFullPath(
                   semanticProgram, semanticProgram.callableSummaries[index]) ==
                   "/std/math/abs";
      }));
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
