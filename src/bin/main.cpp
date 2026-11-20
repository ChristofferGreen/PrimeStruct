#include "primec/pipeline/BuildCommand.h"
#include "primec/support/ProgressBeat.h"
#include "primec/pipeline/BuildSubprojects.h"
#include "primec/pipeline/BuildFunctionStepEmbed.h"
#include "primec/pipeline/BuildRunner.h"
#include "primec/pipeline/BuildScriptCache.h"
#include "primec/pipeline/CliDriver.h"
#include "primec/support/CompileArena.h"
#include "primec/pipeline/CompilePipeline.h"
#include "primec/pipeline/LibraryBuild.h"
#include "primec/support/Diagnostics.h"
#include "primec/support/EmitKind.h"
#include "primec/backend/IrBackends.h"
#include "primec/ir/IrPreparation.h"
#include "primec/ir_lowerer/IrLowererLegacyCollectionBranchCounters.h"
#include "primec/pipeline/CliUsage.h"
#include "primec/support/Options.h"
#include "primec/support/OptionsParser.h"

#include <chrono>
#include <cstdint>
#include <atomic>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#if defined(__APPLE__)
#include <mach/mach.h>
#elif defined(__linux__)
#include <unistd.h>
#endif

namespace {
bool hasPathPrefix(const std::filesystem::path &path, const std::filesystem::path &prefix) {
  auto pathIter = path.begin();
  auto prefixIter = prefix.begin();
  for (; prefixIter != prefix.end(); ++prefixIter, ++pathIter) {
    if (pathIter == path.end() || *pathIter != *prefixIter) {
      return false;
    }
  }
  return true;
}

std::filesystem::path resolveOutputPath(const primec::Options &options) {
  std::filesystem::path output(options.outputPath);
  if (output.is_absolute()) {
    return output;
  }
  if (options.outDir.empty() || options.outDir == ".") {
    return output;
  }
  std::filesystem::path outDir(options.outDir);
  if (hasPathPrefix(output.lexically_normal(), outDir.lexically_normal())) {
    return output;
  }
  return outDir / output;
}

bool ensureOutputDirectory(const std::filesystem::path &outputPath, std::string &error) {
  std::filesystem::path parent = outputPath.parent_path();
  if (parent.empty()) {
    return true;
  }
  std::error_code ec;
  std::filesystem::create_directories(parent, ec);
  if (ec) {
    error = "failed to create output directory: " + parent.string();
    return false;
  }
  return true;
}

std::string redirectDevNullOutputPath(const std::string &emitKind) {
  static std::atomic<uint64_t> counter{0};
  const uint64_t id = counter.fetch_add(1, std::memory_order_relaxed);
  const auto tick = std::chrono::steady_clock::now().time_since_epoch().count();
  const std::filesystem::path scratch =
      std::filesystem::temp_directory_path() /
      ("primec_dev_null_" + emitKind + "_" + std::to_string(tick) + "_" + std::to_string(id));
  return scratch.string();
}

enum class IrBackendRunFailureStage {
  None,
  Emit,
  OutputWrite,
};

struct IrBackendRunFailure {
  IrBackendRunFailureStage stage = IrBackendRunFailureStage::None;
  std::string message;
  std::optional<primec::CliFailure> cliFailure;
};

struct ProcessRssSample {
  bool valid = false;
  uint64_t residentBytes = 0;
};

ProcessRssSample captureProcessRssSample() {
  ProcessRssSample sample;
#if defined(__APPLE__)
  mach_task_basic_info info{};
  mach_msg_type_number_t count = MACH_TASK_BASIC_INFO_COUNT;
  const kern_return_t status = task_info(mach_task_self(),
                                         MACH_TASK_BASIC_INFO,
                                         reinterpret_cast<task_info_t>(&info),
                                         &count);
  if (status == KERN_SUCCESS) {
    sample.valid = true;
    sample.residentBytes = static_cast<uint64_t>(info.resident_size);
  }
#elif defined(__linux__)
  std::ifstream statm("/proc/self/statm");
  uint64_t residentPages = 0;
  if (statm.good()) {
    statm.ignore(std::numeric_limits<std::streamsize>::max(), ' ');
    if (statm >> residentPages) {
      const long pageSize = sysconf(_SC_PAGESIZE);
      if (pageSize > 0) {
        sample.valid = true;
        sample.residentBytes = residentPages * static_cast<uint64_t>(pageSize);
      }
    }
  }
#endif
  return sample;
}

struct BenchmarkSemanticRepeatLeakCheck {
  bool enabled = false;
  uint32_t requestedRuns = 0;
  uint64_t rssBeforeBytes = 0;
  uint64_t rssAfterBytes = 0;
  int64_t rssDriftBytes = 0;
  std::vector<uint64_t> rssAfterEachRunBytes;
  std::vector<double> wallSecondsByRun;
};

template <typename PipelineResultLike>
void emitBenchmarkSemanticPhaseCounters(std::ostream &err,
                                        const PipelineResultLike &output) {
  if (!output.hasSemanticPhaseCounters) {
    return;
  }
  const primec::SemanticPhaseCounters &counters = output.semanticPhaseCounters;
  err << "[benchmark-semantic-phase-counters] "
      << "{\"schema\":\"primestruct_semantic_phase_counters_v1\","
      << "\"validation\":{\"calls_visited\":" << counters.validation.callsVisited
      << ",\"facts_produced\":" << counters.validation.factsProduced
      << ",\"peak_local_map_size\":" << counters.validation.peakLocalMapSize
      << ",\"allocation_count\":" << counters.validation.allocationCount
      << ",\"allocated_bytes\":" << counters.validation.allocatedBytes
      << ",\"rss_before_bytes\":" << counters.validation.rssBeforeBytes
      << ",\"rss_after_bytes\":" << counters.validation.rssAfterBytes << "},"
      << "\"semantic_product_build\":{\"calls_visited\":" << counters.semanticProductBuild.callsVisited
      << ",\"facts_produced\":" << counters.semanticProductBuild.factsProduced
      << ",\"peak_local_map_size\":" << counters.semanticProductBuild.peakLocalMapSize
      << ",\"allocation_count\":" << counters.semanticProductBuild.allocationCount
      << ",\"allocated_bytes\":" << counters.semanticProductBuild.allocatedBytes
      << ",\"rss_before_bytes\":" << counters.semanticProductBuild.rssBeforeBytes
      << ",\"rss_after_bytes\":" << counters.semanticProductBuild.rssAfterBytes << "}}"
      << "\n";
}

void emitBenchmarkSemanticRepeatLeakCheck(std::ostream &err,
                                          const BenchmarkSemanticRepeatLeakCheck &repeatLeakCheck) {
  if (!repeatLeakCheck.enabled) {
    return;
  }
  err << "[benchmark-semantic-repeat-leak-check] "
      << "{\"schema\":\"primestruct_semantic_repeat_leak_check_v1\","
      << "\"runs\":" << repeatLeakCheck.requestedRuns
      << ",\"rss_before_bytes\":" << repeatLeakCheck.rssBeforeBytes
      << ",\"rss_after_bytes\":" << repeatLeakCheck.rssAfterBytes
      << ",\"rss_drift_bytes\":" << repeatLeakCheck.rssDriftBytes
      << ",\"rss_by_run_bytes\":[";
  for (size_t i = 0; i < repeatLeakCheck.rssAfterEachRunBytes.size(); ++i) {
    if (i != 0) {
      err << ",";
    }
    err << repeatLeakCheck.rssAfterEachRunBytes[i];
  }
  err << "],\"wall_seconds_by_run\":[";
  for (size_t i = 0; i < repeatLeakCheck.wallSecondsByRun.size(); ++i) {
    if (i != 0) {
      err << ",";
    }
    err << repeatLeakCheck.wallSecondsByRun[i];
  }
  err << "]}\n";
}

bool runIrBackend(const primec::IrBackend &backend,
                  primec::Program &program,
                  primec::SemanticProgram *semanticProgram,
                  const primec::ExpandedSource *expandedSource,
                  const primec::Options &options,
                  const primec::BuildScriptCache *scriptCache,
                  primec::IrBackendEmitResult &result,
                  IrBackendRunFailure &failure) {
  failure = {};
  const primec::IrBackendDiagnostics &diagnostics = backend.diagnostics();

  const primec::IrValidationTarget validationTarget = backend.validationTarget(options);
  primec::IrModule ir;
  primec::IrPreparationFailure prepFailure;
  primec::IrOptimizationReport optimizationReport;
  const bool prepared = primec::prepareIrModule(program,
                                                semanticProgram,
                                                options,
                                                validationTarget,
                                                ir,
                                                prepFailure,
                                                expandedSource,
                                                &optimizationReport);
  if (options.optimization.report) {
    std::cerr << optimizationReport.format(true);
  }
  if (!prepared) {
    failure.cliFailure = primec::describeIrPreparationFailure(prepFailure, backend);
    return false;
  }
  // The script of `primec build` is stored compiled, so the next run does not compile it again.
  if (scriptCache != nullptr && expandedSource != nullptr) {
    (void)scriptCache->store(ir, *expandedSource);
  }
  // IR preparation already released body-heavy AST storage. Drop the remaining
  // AST envelope and semantic-product payload before backend emission to cap RSS.
  program = {};
  if (semanticProgram != nullptr) {
    *semanticProgram = {};
  }

  std::string error;
  primec::IrBackendEmitOptions emitOptions;
  emitOptions.outputPath = options.outputPath;
  emitOptions.inputPath = options.inputPath;
  emitOptions.programArgs = options.programArgs;
  emitOptions.linkArgs = options.linkArgs;
  emitOptions.wasmBrowserProfile = options.wasmProfile == "browser";
  // The optexe kinds hand generated C++ to the host compiler; an explicit -O
  // level carries over, and the default is -O2.
  emitOptions.optimizationLevel = options.optimization.level;
  emitOptions.hostOptimizationLevel =
      options.optimization.levelSpecified ? options.optimization.level : 2;
  emitOptions.collectReport = options.optimization.report;
  emitOptions.vmScheduleSeed = options.vmScheduleSeed;
  emitOptions.vmThreads = options.vmThreads;
  emitOptions.vmRecordSyncPath = options.vmRecordSyncPath;
  emitOptions.vmReplaySyncPath = options.vmReplaySyncPath;
  const bool emitted = backend.emit(ir, emitOptions, result, error);
  if (options.optimization.report && !result.report.empty()) {
    std::cerr << result.report;
  }
  if (!emitted) {
    const std::string_view backendTag = diagnostics.backendTag;
    const bool outputWriteFailure =
        (backendTag == "ir" || backendTag == "wasm" || backendTag == "cpp-ir" ||
         backendTag == "optcpp-ir" || backendTag == "glsl-ir") &&
        error == options.outputPath;
    failure.stage = outputWriteFailure ? IrBackendRunFailureStage::OutputWrite : IrBackendRunFailureStage::Emit;
    failure.message = std::move(error);
    return false;
  }

  return true;
}

} // namespace

// Runs the compile pipeline --benchmark-semantic-repeat-count times (once by default) and
// leaves the final run's output in `pipelineOutput`. Returns an exit code only on failure.
static std::optional<int> runCompileRepeats(primec::Options &options,
                                     primec::CompilePipelineOutput &pipelineOutput,
                                     BenchmarkSemanticRepeatLeakCheck &repeatLeakCheck) {
  std::string error;
  const uint32_t repeatCount = options.benchmarkSemanticRepeatCompileCount.value_or(1u);
  repeatLeakCheck.enabled = options.benchmarkSemanticRepeatCompileCount.has_value();
  repeatLeakCheck.requestedRuns = repeatCount;
  if (repeatLeakCheck.enabled) {
    repeatLeakCheck.rssAfterEachRunBytes.reserve(repeatCount);
    repeatLeakCheck.wallSecondsByRun.reserve(repeatCount);
    const ProcessRssSample rssBefore = captureProcessRssSample();
    if (rssBefore.valid) {
      repeatLeakCheck.rssBeforeBytes = rssBefore.residentBytes;
    }
  }
  for (uint32_t runIndex = 0; runIndex < repeatCount; ++runIndex) {
    primec::CompilePipelineErrorStage runError = primec::CompilePipelineErrorStage::None;
    std::string runErrorText;
    const bool isFinalRun = (runIndex + 1u == repeatCount);
    const auto runStart = std::chrono::steady_clock::now();
    primec::CompilePipelineResult runResult =
        primec::runCompilePipelineResult(options, runError, runErrorText);
    (void)runError;
    const auto runStop = std::chrono::steady_clock::now();
    if (repeatLeakCheck.enabled) {
      repeatLeakCheck.wallSecondsByRun.push_back(
          std::chrono::duration<double>(runStop - runStart).count());
      const ProcessRssSample rssAfterRun = captureProcessRssSample();
      repeatLeakCheck.rssAfterEachRunBytes.push_back(rssAfterRun.valid ? rssAfterRun.residentBytes : 0);
    }
    if (auto *pipelineFailure =
            std::get_if<primec::CompilePipelineFailureResult>(&runResult)) {
      error = std::move(runErrorText);
      if (repeatLeakCheck.enabled) {
        const ProcessRssSample rssAfter = captureProcessRssSample();
        if (rssAfter.valid) {
          repeatLeakCheck.rssAfterBytes = rssAfter.residentBytes;
          repeatLeakCheck.rssDriftBytes = static_cast<int64_t>(repeatLeakCheck.rssAfterBytes) -
                                          static_cast<int64_t>(repeatLeakCheck.rssBeforeBytes);
        }
      }
      emitBenchmarkSemanticPhaseCounters(std::cerr, *pipelineFailure);
      emitBenchmarkSemanticRepeatLeakCheck(std::cerr, repeatLeakCheck);
      return primec::emitCliFailure(
          std::cerr, options, primec::describeCompilePipelineFailure(*pipelineFailure));
    }
    if (isFinalRun) {
      pipelineOutput =
          std::move(std::get<primec::CompilePipelineSuccessResult>(runResult).output);
      error = std::move(runErrorText);
    }
  }
  if (repeatLeakCheck.enabled) {
    const ProcessRssSample rssAfter = captureProcessRssSample();
    if (rssAfter.valid) {
      repeatLeakCheck.rssAfterBytes = rssAfter.residentBytes;
      repeatLeakCheck.rssDriftBytes = static_cast<int64_t>(repeatLeakCheck.rssAfterBytes) -
                                      static_cast<int64_t>(repeatLeakCheck.rssBeforeBytes);
    }
  }
  return std::nullopt;
}

// Writes the compiled program through the selected IR backend (or reports why it cannot).
static int emitCompiledProgram(primec::Options &options,
                               const primec::IrBackend *irBackend,
                               const primec::BuildScriptCache *scriptCache,
                               primec::CompilePipelineOutput &pipelineOutput) {
  std::string error;
  primec::Program &program = pipelineOutput.program;
  primec::emitProgressBeat("primec: lower and emit");
  if (primec::isIrModuleDumpStage(options)) {
    return primec::runIrModuleDump(
        std::cout,
        std::cerr,
        program,
        pipelineOutput.hasSemanticProgram ? &pipelineOutput.semanticProgram : nullptr,
        &pipelineOutput.expandedSource,
        options,
        irBackend != nullptr ? irBackend->validationTarget(options)
                             : primec::IrValidationTarget::Any);
  }

  if (irBackend != nullptr && irBackend->requiresOutputPath() && !options.outputPath.empty()) {
    if (options.outputPath == "/dev/null") {
      options.outputPath = redirectDevNullOutputPath(options.emitKind);
    }
    std::filesystem::path resolved = resolveOutputPath(options);
    options.outputPath = resolved.string();
    if (!ensureOutputDirectory(resolved, error)) {
      primec::CliFailure outputFailure;
      outputFailure.code = primec::DiagnosticCode::OutputError;
      outputFailure.plainPrefix = "Output error: ";
      outputFailure.message = error;
      return primec::emitCliFailure(std::cerr, options, outputFailure);
    }
  }

  if (irBackend != nullptr) {
    const primec::IrBackendDiagnostics &diagnostics = irBackend->diagnostics();
    primec::IrBackendEmitResult emitResult;
    IrBackendRunFailure failure;
    primec::SemanticProgram *semanticProgram =
        pipelineOutput.hasSemanticProgram ? &pipelineOutput.semanticProgram : nullptr;
    if (!runIrBackend(*irBackend,
                      program,
                      semanticProgram,
                      &pipelineOutput.expandedSource,
                      options,
                      scriptCache,
                      emitResult,
                      failure)) {
      if (failure.cliFailure.has_value()) {
        return primec::emitCliFailure(std::cerr, options, *failure.cliFailure);
      }
      if (failure.stage == IrBackendRunFailureStage::OutputWrite) {
        primec::CliFailure outputFailure;
        outputFailure.code = primec::DiagnosticCode::OutputError;
        outputFailure.plainPrefix = "Failed to write output: ";
        outputFailure.message = options.outputPath;
        return primec::emitCliFailure(std::cerr, options, outputFailure);
      }
      const int emitFailureCode = diagnostics.backendTag == std::string_view("vm") ? 3 : 2;
      primec::CliFailure emitFailure;
      emitFailure.code = diagnostics.emitDiagnosticCode;
      emitFailure.plainPrefix = diagnostics.emitErrorPrefix;
      emitFailure.message = failure.message;
      emitFailure.exitCode = emitFailureCode;
      emitFailure.notes = primec::makeIrBackendNotes(diagnostics);
      return primec::emitCliFailure(std::cerr, options, emitFailure);
    }
    return emitResult.exitCode;
  }

  if (!options.outputPath.empty()) {
    std::filesystem::path resolved = resolveOutputPath(options);
    options.outputPath = resolved.string();
    if (!ensureOutputDirectory(resolved, error)) {
      primec::CliFailure outputFailure;
      outputFailure.code = primec::DiagnosticCode::OutputError;
      outputFailure.plainPrefix = "Output error: ";
      outputFailure.message = error;
      return primec::emitCliFailure(std::cerr, options, outputFailure);
    }
  }

  primec::CliFailure emitFailure;
  emitFailure.code = primec::DiagnosticCode::EmitError;
  emitFailure.plainPrefix = "Emit error: ";
  emitFailure.message = "no backend available for emit kind " + options.emitKind;
  return primec::emitCliFailure(std::cerr, options, emitFailure);
}

// `--emit=lib`: writes the library archive of the validated module to the output path.
static int emitLibraryArchive(primec::Options &options,
                              const std::string &compilerPath,
                              primec::CompilePipelineOutput &pipelineOutput) {
  auto fail = [&](primec::DiagnosticCode code, const char *prefix, const std::string &message) {
    primec::CliFailure failure;
    failure.code = code;
    failure.plainPrefix = prefix;
    failure.message = message;
    failure.exitCode = 2;
    return primec::emitCliFailure(std::cerr, options, failure);
  };
  std::string error;
  std::string buildId;
  primec::LibraryArchiveContent content;
  if (!primec::computeCompilerBuildId(compilerPath, buildId, error) ||
      !primec::buildLibraryArchiveContent(pipelineOutput, options, buildId, content, error)) {
    return fail(primec::DiagnosticCode::EmitError, "Emit error: ", error);
  }
  const std::filesystem::path resolved = resolveOutputPath(options);
  options.outputPath = resolved.string();
  if (!ensureOutputDirectory(resolved, error) ||
      !primec::writeLibraryArchiveFile(options.outputPath, content, error)) {
    return fail(primec::DiagnosticCode::OutputError, "Output error: ", error);
  }
  return 0;
}

int main(int argc, char **argv) {
  primec::Options options;
  std::string argError;
  // `primec build`: the compile of make.prime under the build profile (include/primec/pipeline/BuildCommand.h).
  primec::BuildCommandPlan buildPlan;
  const bool isBuildCommand = argc >= 2 && std::string_view(argv[1]) == "build";
  std::vector<char *> buildArgv;
  std::string buildSeparator = "--";
  std::string buildStageArgument;
  std::vector<std::string> buildOptionArguments;
  const std::string selfPath = isBuildCommand ? primec::selfExecutablePath(argv[0]) : std::string();
  if (isBuildCommand) {
    if (!primec::planBuildCommand(
            std::vector<std::string>(argv + 2, argv + argc), buildPlan, argError)) {
      std::cerr << "primec build: " << argError << "\n";
      return 2;
    }
    if (buildPlan.listOptions) {
      std::string graphText;
      std::string graphDiagnostics;
      const int graphStatus =
          primec::resolveBuildGraph(buildPlan, selfPath, 0, graphText, graphDiagnostics);
      std::cerr << graphDiagnostics;
      if (graphStatus != 0) {
        return graphStatus;
      }
      primec::BuildGraphData listed;
      std::string listError;
      if (!primec::parseBuildGraphJson(graphText, listed, listError)) {
        std::cerr << "primec build: " << listError << "\n";
        return 1;
      }
      std::cout << primec::formatBuildOptionList(listed.options);
      return 0;
    }
    if (!buildPlan.dryRun) {
      std::unique_ptr<primec::BuildFunctionStepRunner> functionSteps =
          primec::makeEmbeddedFunctionStepRunner();
      return primec::runBuildCommand(
          buildPlan, selfPath, std::cout, std::cerr, functionSteps.get());
    }
    if (!buildPlan.raw) {
      // The graph of the project and its subprojects, from one raw dry run per script.
      std::string graphText;
      std::string graphDiagnostics;
      const int graphStatus = primec::resolveBuildGraph(
          buildPlan, selfPath, buildPlan.stage, graphText, graphDiagnostics);
      std::cerr << graphDiagnostics;
      if (graphStatus == 0 && graphText.find("\"kind\":\"function\"") != std::string::npos) {
        // A function step names the files of its import closure, which only a compile of its file can tell.
        std::unique_ptr<primec::BuildFunctionStepRunner> functionSteps =
            primec::makeEmbeddedFunctionStepRunner();
        std::string annotated;
        std::string closureDiagnostics;
        if (!primec::addFunctionStepClosures(*functionSteps,
                                             graphText,
                                             buildPlan.projectDir,
                                             buildPlan.outDir,
                                             annotated,
                                             closureDiagnostics)) {
          std::cerr << closureDiagnostics;
          return 1;
        }
        graphText = std::move(annotated);
      }
      if (graphStatus == 0) {
        std::cout << graphText;
      }
      return graphStatus;
    }
    for (std::string &argument : buildPlan.arguments) {
      buildArgv.push_back(argument.data());
    }
    // The stage the script runs to (`Graph.run`) is its first program argument. It is not part of `buildPlan.arguments`,
    // so every stage shares one compiled script in the script cache.
    buildStageArgument = std::to_string(buildPlan.stage);
    buildArgv.push_back(buildSeparator.data());
    buildArgv.push_back(buildStageArgument.data());
    // The options follow as `name=value` arguments (argument 2 on); they are not in the cache key of the script either.
    buildOptionArguments = primec::buildOptionProgramArguments(buildPlan.options);
    for (std::string &optionArgument : buildOptionArguments) {
      buildArgv.push_back(optionArgument.data());
    }
    argc = static_cast<int>(buildArgv.size());
    argv = buildArgv.data();
  }
  if (!primec::parseOptions(argc, argv, primec::OptionsParserMode::Primec, options, argError)) {
    return primec::reportArgumentError(std::cerr, options, argError, primec::OptionsParserMode::Primec);
  }
  if (isBuildCommand) {
    options.inMemorySource = buildPlan.source;
    // The appended runner import is the command's own, not the script's: allow it whatever the allowlist is.
    if (options.allowImport.has_value()) {
      options.allowImport->push_back("/std/build_runner");
    }
  }
  primec::ir_lowerer::setLegacyCollectionBranchCountersEnabled(
      options.benchmarkIrLowererLegacyCollectionBranchCounters);
  if (options.listTransforms) {
    primec::printTransformList(std::cout);
    return 0;
  }
  if (options.listOptimizationPasses) {
    std::cout << primec::formatIrOptimizationPassList();
    return 0;
  }
  const std::string_view irBackendKind = primec::resolveIrBackendEmitKind(options.emitKind);
  const primec::IrBackend *irBackend = primec::findIrBackend(irBackendKind);
  if (irBackend == nullptr && options.dumpStage.empty() &&
      !primec::isPrimecInterfaceEmitKind(options.emitKind)) {
    options.skipSemanticProductForNonConsumingPath = true;
  }
  primec::addDefaultStdlibInclude(options.inputPath, options.importPaths);

  // The compiled script of `primec build --dry-run` is cached in the project (include/primec/pipeline/BuildScriptCache.h):
  // a current entry runs on the VM without compiling. A script that fails to run falls through to the compile, so its
  // diagnostics are the ones of an ordinary run.
  std::optional<primec::BuildScriptCache> scriptCache;
  if (isBuildCommand && irBackend != nullptr && options.dumpStage.empty() &&
      !options.benchmarkSemanticRepeatCompileCount.has_value()) {
    std::string compilerHash;
    std::string compilerHashError;
    if (!primec::computeCompilerBuildId(selfPath, compilerHash, compilerHashError)) {
      compilerHash.clear();
    }
    scriptCache.emplace(primec::BuildScriptCache::cacheDirFor(buildPlan.projectDir),
                        compilerHash,
                        buildPlan.arguments,
                        buildPlan.source,
                        options.importPaths);
    primec::IrModule cachedModule;
    if (scriptCache->load(cachedModule)) {
      primec::IrBackendEmitOptions emitOptions;
      emitOptions.inputPath = options.inputPath;
      emitOptions.programArgs = options.programArgs;
      emitOptions.vmScheduleSeed = options.vmScheduleSeed;
      emitOptions.vmThreads = options.vmThreads;
      emitOptions.vmRecordSyncPath = options.vmRecordSyncPath;
      emitOptions.vmReplaySyncPath = options.vmReplaySyncPath;
      primec::IrBackendEmitResult cachedResult;
      std::string cachedError;
      if (irBackend->emit(cachedModule, emitOptions, cachedResult, cachedError)) {
        return cachedResult.exitCode;
      }
    }
  }

  // TODO-5233/TODO-5234: one process-lifetime ScopedCompileArena, entered
  // once here and never reset - see docs/CompilerArenaAllocator.md for why
  // it's never reset. For --benchmark-semantic-repeat-count > 1 (a
  // diagnostic-only flag), this means each repeat's allocations accumulate
  // in the arena rather than being reclaimed between repeats, so the
  // RSS-checkpoint machinery below will show memory growing across
  // repeats for arena-covered allocations - this is expected under a
  // "never free until process exit" allocator (the same characterization
  // this design always used to justify the CLI-only scope), not a leak,
  // and is called out explicitly here since it's the one place in this
  // binary where that trade-off is directly observable.
  primec::ScopedCompileArena compileArenaScope;
  primec::CompilePipelineOutput pipelineOutput;
  BenchmarkSemanticRepeatLeakCheck repeatLeakCheck;
  if (const std::optional<int> failureCode = runCompileRepeats(options, pipelineOutput, repeatLeakCheck)) {
    return *failureCode;
  }
  emitBenchmarkSemanticPhaseCounters(std::cerr, pipelineOutput);
  emitBenchmarkSemanticRepeatLeakCheck(std::cerr, repeatLeakCheck);
  if (pipelineOutput.hasDumpOutput) {
    std::cout << pipelineOutput.dumpOutput;
    return 0;
  }

  if (options.emitKind == "lib" && options.dumpStage.empty()) {
    return emitLibraryArchive(options, primec::selfExecutablePath(argv[0]), pipelineOutput);
  }

  return emitCompiledProgram(
      options, irBackend, scriptCache ? &*scriptCache : nullptr, pipelineOutput);
}
