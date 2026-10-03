#include "primec/pipeline/CliUsage.h"

#include "primec/support/Diagnostics.h"
#include "primec/support/EmitKind.h"

#include <iomanip>
#include <iostream>
#include <string_view>

namespace primec {

namespace {

constexpr int FlagColumn = 40;

void flagLine(std::ostream &err, std::string_view flag, std::string_view desc) {
  if (desc.empty()) {
    err << "  " << flag << "\n";
  } else if (static_cast<int>(flag.size()) >= FlagColumn - 2) {
    err << "  " << flag << "\n" << std::string(FlagColumn + 2, ' ') << desc << "\n";
  } else {
    err << "  " << std::left << std::setw(FlagColumn) << std::string(flag) << desc << "\n";
  }
}

void flagOnly(std::ostream &err, std::string_view flag) { err << "  " << flag << "\n"; }

void blankLine(std::ostream &err) { err << "\n"; }

void printTransformsSection(std::ostream &err) {
  err << "Transforms:\n";
  flagLine(err, "--text-transforms <list>", "Enable specific text transforms");
  flagLine(err, "--text-transform-rules <rules>", "Text transform rule overrides");
  flagLine(err, "--semantic-transforms <list>", "Enable specific semantic transforms");
  flagLine(err, "--semantic-transform-rules <rules>", "Semantic transform rule overrides");
  flagLine(err, "--transform-list <list>", "Enable transforms by name (text + semantic)");
  flagLine(err, "--no-text-transforms", "Disable all text transforms");
  flagLine(err, "--no-semantic-transforms", "Disable all semantic transforms");
  flagLine(err, "--no-transforms", "Disable all transforms");
  flagLine(err, "--list-transforms", "List available transforms and exit");
  blankLine(err);
}

void printDiagnosticsSection(std::ostream &err) {
  err << "Diagnostics:\n";
  flagLine(err, "--emit-diagnostics", "Emit machine-readable JSON diagnostics on stderr");
  flagLine(err, "--collect-diagnostics", "Collect diagnostics instead of stopping at the first error");
  blankLine(err);
}

void printEffectsAndIrSection(std::ostream &err) {
  err << "Effects / IR:\n";
  flagLine(err, "--default-effects <list>", "Default effect set for definitions without one");
  flagLine(err, "--ir-inline", "Inline eligible calls during IR lowering");
  flagLine(err, "--dump-stage <stage>", "Dump a compiler stage and exit; one of:");
  flagLine(err, "", "pre_ast, ast, ast-semantic, semantic-product, type-graph, ir");
  flagLine(err, "", "(lowering-facing dumps include semantic-product between");
  flagLine(err, "", "ast-semantic and ir)");
  blankLine(err);
}

void printPassThroughNote(std::ostream &err) {
  err << "Everything after `--` is passed through as program args at runtime.\n";
}

void printPrimecUsage(std::ostream &err) {
  err << "Usage: primec [options] <input.prime> [-- <program args...>]\n\n";

  err << "Output:\n";
  flagLine(err, std::string("--emit=") + std::string(primecEmitKindsUsage()), "Output kind (default: exe)");
  flagLine(err, "-o <output>", "Output file path");
  flagLine(err, "--out-dir <dir>", "Output directory");
  flagLine(err, "--entry /path", "Entry point definition path");
  flagLine(err, "--wasm-profile wasi|browser", "Wasm host profile (with --emit=wasm)");
  blankLine(err);

  err << "Imports:\n";
  flagLine(err, "--import-path <dir>, -I <dir>", "Add an import search directory");
  blankLine(err);

  printTransformsSection(err);
  printDiagnosticsSection(err);
  printEffectsAndIrSection(err);

  err << "Benchmarking (semantic phase):\n";
  flagOnly(err, "--benchmark-semantic-phase-counters");
  flagOnly(err, "--benchmark-semantic-allocation-counters");
  flagOnly(err, "--benchmark-semantic-rss-checkpoints");
  flagOnly(err, "--benchmark-semantic-disable-method-target-memoization");
  flagOnly(err, "--benchmark-semantic-graph-local-auto-legacy-key-shadow");
  flagOnly(err, "--benchmark-semantic-graph-local-auto-legacy-side-channel-shadow");
  flagOnly(err, "--benchmark-semantic-disable-graph-local-auto-dependency-scratch-pmr");
  flagOnly(err, "--benchmark-semantic-definition-validation-workers <n>");
  flagOnly(err, "--benchmark-semantic-repeat-count <n>");
  blankLine(err);

  err << "Benchmarking (IR lowerer):\n";
  flagOnly(err, "--benchmark-ir-lowerer-legacy-collection-branch-counters");
  blankLine(err);

  printPassThroughNote(err);
}

void printPrimevmUsage(std::ostream &err) {
  err << "Usage: primevm [options] <input.prime> [-- <program args...>]\n\n";

  err << "Entry / imports:\n";
  flagLine(err, "--entry /path", "Entry point definition path");
  flagLine(err, "--import-path <dir>, -I <dir>", "Add an import search directory");
  blankLine(err);

  printTransformsSection(err);
  printDiagnosticsSection(err);

  err << "Debugging:\n";
  flagLine(err, "--debug-json", "Emit debugger-facing JSON events on stdout");
  flagLine(err, "--debug-json-snapshots [none|stop|all]", "Control snapshot verbosity for --debug-json");
  flagLine(err, "--debug-trace <path>", "Write an execution trace to <path>");
  flagLine(err, "--debug-dap", "Speak the Debug Adapter Protocol on stdio");
  flagLine(err, "--debug-replay <trace>", "Replay a previously recorded --debug-trace file");
  flagLine(err, "--debug-replay-sequence <n>", "Stop replay at trace sequence number <n>");
  blankLine(err);

  printEffectsAndIrSection(err);
  printPassThroughNote(err);
}

} // namespace

int reportArgumentError(std::ostream &err, const Options &options, std::string argError, OptionsParserMode mode) {
  if (options.emitDiagnostics) {
    if (argError.empty()) {
      argError = "invalid arguments";
    }
    const DiagnosticRecord diagnostic =
        makeDiagnosticRecord(DiagnosticCode::ArgumentError, argError, options.inputPath);
    err << encodeDiagnosticsJson({diagnostic}) << "\n";
    return 2;
  }
  if (!argError.empty()) {
    err << "Argument error: " << argError << "\n";
  }
  if (mode == OptionsParserMode::Primevm) {
    printPrimevmUsage(err);
  } else {
    printPrimecUsage(err);
  }
  return 2;
}

} // namespace primec
