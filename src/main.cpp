#include "primec/CliDriver.h"
#include "primec/CompilePipeline.h"
#include "primec/Diagnostics.h"
#include "primec/EmitKind.h"
#include "primec/IrBackends.h"
#include "primec/IrPreparation.h"
#include "primec/Options.h"
#include "primec/OptionsParser.h"

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
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

void emitBenchmarkSemanticPhaseCounters(std::ostream &err,
                                        const primec::CompilePipelineOutput &output) {
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
                  const primec::SemanticProgram *semanticProgram,
                  const primec::Options &options,
                  primec::IrBackendEmitResult &result,
                  IrBackendRunFailure &failure) {
  failure = {};
  const primec::IrBackendDiagnostics &diagnostics = backend.diagnostics();

  const primec::IrValidationTarget validationTarget = backend.validationTarget(options);
  primec::IrModule ir;
  primec::IrPreparationFailure prepFailure;
  if (!primec::prepareIrModule(program, semanticProgram, options, validationTarget, ir, prepFailure)) {
    failure.cliFailure = primec::describeIrPreparationFailure(prepFailure, backend);
    return false;
  }

  std::string error;
  primec::IrBackendEmitOptions emitOptions;
  emitOptions.outputPath = options.outputPath;
  emitOptions.inputPath = options.inputPath;
  emitOptions.programArgs = options.programArgs;
  if (!backend.emit(ir, emitOptions, result, error)) {
    const std::string_view backendTag = diagnostics.backendTag;
    const bool outputWriteFailure =
        (backendTag == "ir" || backendTag == "wasm" || backendTag == "cpp-ir" || backendTag == "glsl-ir") &&
        error == options.outputPath;
    failure.stage = outputWriteFailure ? IrBackendRunFailureStage::OutputWrite : IrBackendRunFailureStage::Emit;
    failure.message = std::move(error);
    return false;
  }

  return true;
}

} // namespace

int main(int argc, char **argv) {
  primec::Options options;
  std::string argError;
  if (!primec::parseOptions(argc, argv, primec::OptionsParserMode::Primec, options, argError)) {
    if (options.emitDiagnostics) {
      if (argError.empty()) {
        argError = "invalid arguments";
      }
      const primec::DiagnosticRecord diagnostic =
          primec::makeDiagnosticRecord(primec::DiagnosticCode::ArgumentError, argError, options.inputPath);
      std::cerr << primec::encodeDiagnosticsJson({diagnostic}) << "\n";
    } else {
      if (!argError.empty()) {
        std::cerr << "Argument error: " << argError << "\n";
      }
      std::cerr << "Usage: primec [--emit=" << primec::primecEmitKindsUsage() << "] <input.prime> [-o <output>] "
                << "[--entry /path] [--import-path <dir>] [-I <dir>] "
                << "[--wasm-profile wasi|browser] "
                << "[--text-transforms <list>] [--text-transform-rules <rules>] "
                << "[--semantic-transform-rules <rules>] [--semantic-transforms <list>] "
                << "[--transform-list <list>] [--no-text-transforms] [--no-semantic-transforms] "
                << "[--no-transforms] [--out-dir <dir>] [--list-transforms] [--emit-diagnostics] "
                << "[--collect-diagnostics] "
                << "[--default-effects <list>] [--ir-inline] "
                << "[--benchmark-semantic-phase-counters] "
                << "[--benchmark-semantic-allocation-counters] "
                << "[--benchmark-semantic-rss-checkpoints] "
                << "[--benchmark-semantic-disable-method-target-memoization] "
                << "[--benchmark-semantic-graph-local-auto-legacy-key-shadow] "
                << "[--benchmark-semantic-graph-local-auto-legacy-side-channel-shadow] "
                << "[--benchmark-semantic-disable-graph-local-auto-dependency-scratch-pmr] "
                << "[--benchmark-semantic-definition-validation-workers <n>] "
                << "[--benchmark-semantic-repeat-count <n>] "
                << "[--dump-stage pre_ast|ast|ast-semantic|semantic-product|type-graph|ir] [-- <program args...>]\n"
                << "Dump-stage note: lowering-facing dumps now include semantic-product between ast-semantic and ir.\n";
    }
    return 2;
  }
  if (options.listTransforms) {
    primec::printTransformList(std::cout);
    return 0;
  }
  const std::string_view irBackendKind = primec::resolveIrBackendEmitKind(options.emitKind);
  const primec::IrBackend *irBackend = primec::findIrBackend(irBackendKind);
  if (irBackend == nullptr && options.dumpStage.empty()) {
    options.skipSemanticProductForNonConsumingPath = true;
  }
  std::string error;
  primec::addDefaultStdlibInclude(options.inputPath, options.importPaths);

  primec::CompilePipelineOutput pipelineOutput;
  BenchmarkSemanticRepeatLeakCheck repeatLeakCheck;
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
    primec::CompilePipelineOutput runOutput;
    primec::CompilePipelineErrorStage runError = primec::CompilePipelineErrorStage::None;
    std::string runErrorText;
    const auto runStart = std::chrono::steady_clock::now();
    const bool ok = primec::runCompilePipeline(options, runOutput, runError, runErrorText);
    (void)runError;
    const auto runStop = std::chrono::steady_clock::now();
    if (repeatLeakCheck.enabled) {
      repeatLeakCheck.wallSecondsByRun.push_back(
          std::chrono::duration<double>(runStop - runStart).count());
      const ProcessRssSample rssAfterRun = captureProcessRssSample();
      repeatLeakCheck.rssAfterEachRunBytes.push_back(rssAfterRun.valid ? rssAfterRun.residentBytes : 0);
    }
    pipelineOutput = std::move(runOutput);
    error = std::move(runErrorText);
    if (!ok) {
      if (repeatLeakCheck.enabled) {
        const ProcessRssSample rssAfter = captureProcessRssSample();
        if (rssAfter.valid) {
          repeatLeakCheck.rssAfterBytes = rssAfter.residentBytes;
          repeatLeakCheck.rssDriftBytes = static_cast<int64_t>(repeatLeakCheck.rssAfterBytes) -
                                          static_cast<int64_t>(repeatLeakCheck.rssBeforeBytes);
        }
      }
      emitBenchmarkSemanticPhaseCounters(std::cerr, pipelineOutput);
      emitBenchmarkSemanticRepeatLeakCheck(std::cerr, repeatLeakCheck);
      return primec::emitCliFailure(std::cerr, options, primec::describeCompilePipelineFailure(pipelineOutput));
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
  emitBenchmarkSemanticPhaseCounters(std::cerr, pipelineOutput);
  emitBenchmarkSemanticRepeatLeakCheck(std::cerr, repeatLeakCheck);
  if (pipelineOutput.hasDumpOutput) {
    std::cout << pipelineOutput.dumpOutput;
    return 0;
  }

  primec::Program &program = pipelineOutput.program;

  if (irBackend != nullptr && irBackend->requiresOutputPath() && !options.outputPath.empty()) {
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
    const primec::SemanticProgram *semanticProgram =
        pipelineOutput.hasSemanticProgram ? &pipelineOutput.semanticProgram : nullptr;
    if (!runIrBackend(*irBackend, program, semanticProgram, options, emitResult, failure)) {
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
