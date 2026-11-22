#pragma once

// One program, many execution forms (docs/OptimizingBackendsPlan.md, section 8).
//
// A ProgramCase is PrimeStruct source plus what it must do: an exit code and
// optionally stdout and stderr. runProgramMatrix runs it through every
// ExecutionConfig - the VM step kernel and flat loop at -O0 and -O2, the native
// backend at -O0 and -O2, optexe, and the old C++ emitter - by invoking the
// built `primevm` and `primec` from the working directory, as the compile-run
// suites do, and checks each result. Without an expected exit code the cases must
// agree with the first config (the oracle: the checked VM step kernel at -O0).
//
// Configs a program cannot run on are named in `skipConfigs`, with the reason as
// a comment at the case, so a backend gap stays visible instead of disappearing.
// PRIMESTRUCT_MATRIX_CONFIGS=all adds the slow configs (optexe, exe) to every case.
// The Wasm config (`wasm-wasi`, run under node) is opt-in per case: a program that Wasm supports lists it in
// `extraConfigs` and then agrees with the VM on stdout and exit code; a machine without node skips it with a message.

#include "third_party/doctest.h"

#include "primec/testing/TestScratch.h"

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

#if defined(__unix__) || defined(__APPLE__)
#include <sys/wait.h>
#endif

namespace program_matrix {

enum class Backend { VmStep, Vm, Native, OptExe, Exe, Wasm };

struct ExecutionConfig {
  std::string name;
  Backend backend = Backend::Vm;
  int level = 0;
  // Slow to build (host C++ compiler): only with PRIMESTRUCT_MATRIX_CONFIGS=all
  // or when a case lists the config explicitly.
  bool slow = false;
  // Extra primevm flags of a VM config, e.g. a task schedule seed.
  std::vector<std::string> vmFlags = {};
  // Runs only for a case that lists the config in `extraConfigs` or `onlyConfigs`, never by default or with
  // PRIMESTRUCT_MATRIX_CONFIGS=all: the Wasm config, whose supported programs are listed one by one (TODO-6090).
  bool optIn = false;
};

struct ProgramCase {
  std::string name; // used for scratch file names; keep it a plain identifier
  std::string source;
  std::optional<int> exitCode;
  std::optional<std::string> stdoutText;
  // stderr is backend specific (VM faults read "VM error: ..."), so each family
  // has its own expectation.
  std::optional<std::string> vmStderr;
  std::optional<std::string> nativeStderr;
  // The C++ emitters (`optexe`, `exe`) write the VM's fault messages; checked on those two configs only.
  std::optional<std::string> cppStderr;
  // The Wasm config (`wasm-wasi`): the WASI profile writes the VM's fault message to fd 2 before `proc_exit(3)`.
  std::optional<std::string> wasmStderr;
  std::vector<std::string> flags; // extra primec/primevm flags, e.g. --default-effects=io_out
  std::vector<std::string> skipConfigs;
  std::vector<std::string> onlyConfigs; // restrict to these when non-empty
  // Opt-in configs (`wasm-wasi`) that run in addition to the default set; a program lists it here when Wasm supports
  // it, and an unsupported program stays visible as the absence of the entry (TODO-6090).
  std::vector<std::string> extraConfigs;
};

struct Outcome {
  bool ran = false;
  int exitCode = -1;
  std::string out;
  std::string err;
  std::string note;
};

inline bool nativeSupported() {
#if (defined(__APPLE__) && (defined(__arm64__) || defined(__aarch64__))) ||                        \
    (defined(__linux__) && defined(__x86_64__))
  return true;
#else
  return false;
#endif
}

// Whether the native backend runs programs that use the String heap and read its statistics (arm64 macOS only).
inline bool nativeRunsStringHeap() {
#if defined(__APPLE__) && (defined(__arm64__) || defined(__aarch64__))
  return true;
#else
  return false;
#endif
}

// The VM with each task on an OS thread of its own (--vm-threads; docs/spec/vm-design.md, Tasks on threads).
inline ExecutionConfig vmThreadsConfig() {
  ExecutionConfig config{"vm-O2-threads", Backend::Vm, 2, false};
  config.vmFlags = {"--vm-threads"};
  return config;
}

inline bool isThreadsConfig(const ExecutionConfig &config) {
  for (const std::string &flag : config.vmFlags) {
    if (flag == "--vm-threads") {
      return true;
    }
  }
  return false;
}

// Corpus mode (TODO-5900): PRIMESTRUCT_VM_THREADS_CORPUS=<n> makes every concurrent case run only the eager schedule
// once and the threads config n times, each run printing what the eager run printed (runThreadsCorpus). The ctest
// entries of tests/vm_threads_corpus.txt and scripts/tsan_vm_check.sh set it; without it the cases run their configs.
inline int threadsCorpusRepeats() {
  const char *value = std::getenv("PRIMESTRUCT_VM_THREADS_CORPUS");
  if (value == nullptr || *value == '\0') {
    return 0;
  }
  const int repeats = std::atoi(value);
  return repeats > 0 ? repeats : 0;
}

// True in corpus mode: a case that is not a concurrent program (a compile-only check, a seed-dependent outcome)
// returns at once.
inline bool corpusOnly() {
  return threadsCorpusRepeats() > 0;
}

inline std::vector<ExecutionConfig> allConfigs() {
  std::vector<ExecutionConfig> configs = {
      {"vm-step-O0", Backend::VmStep, 0, false},
      {"vm-O0", Backend::Vm, 0, false},
      {"vm-O2", Backend::Vm, 2, false},
  };
  if (nativeSupported()) {
    configs.push_back({"native-O0", Backend::Native, 0, false});
    configs.push_back({"native-O2", Backend::Native, 2, false});
  }
  configs.push_back({"optexe-O2", Backend::OptExe, 2, true});
  configs.push_back({"exe", Backend::Exe, 0, true});
  ExecutionConfig wasm{"wasm-wasi", Backend::Wasm, 0, true};
  wasm.optIn = true;
  configs.push_back(wasm);
  return configs;
}

// Whether node (the host of the Wasm config) is on the PATH; checked once.
inline bool nodeAvailable() {
  static const bool available = std::system("node --version > /dev/null 2>&1") == 0;
  return available;
}

// Runs the exported `main` of a `--wasm-profile wasi` module under Node's built-in WebAssembly and WASI, as the VM
// would run the program: stdout through `fd_write`, `proc_exit` (a runtime error calls it with 3) as the exit code, and
// the result of `main` modulo 256 otherwise (a `void` main exits 0). The working directory is preopened (as `.`), so a
// program that opens files finds the same ones as on the VM, and a missing path answers `ENOENT`.
constexpr const char *WasmNodeRunner = R"(
"use strict";
const fs = require("node:fs");
process.removeAllListeners("warning");
process.on("warning", () => {});
const { WASI } = require("node:wasi");
// The working directory is the one preopened directory, as the VM resolves a relative path against it.
const wasi = new WASI({ version: "preview1", args: [process.argv[2]], preopens: { ".": process.cwd() }, returnOnExit: true });
class ProgramExit { constructor(code) { this.code = code; } }
const imports = wasi.getImportObject();
imports.wasi_snapshot_preview1.proc_exit = (code) => { throw new ProgramExit(code); };
const instance = new WebAssembly.Instance(new WebAssembly.Module(fs.readFileSync(process.argv[2])), imports);
if (instance.exports.memory) {
  wasi.initialize(instance);
}
try {
  const result = instance.exports.main();
  process.exitCode = result === undefined ? 0 : Number(BigInt.asUintN(8, BigInt(result)));
} catch (e) {
  if (!(e instanceof ProgramExit)) {
    console.error("unexpected " + e);
    process.exit(99);
  }
  process.exitCode = e.code;
}
)";

inline bool includeSlowConfigs() {
  const char *value = std::getenv("PRIMESTRUCT_MATRIX_CONFIGS");
  return value != nullptr && std::string(value) == "all";
}

inline std::string quote(const std::string &value) {
  std::string quoted = "'";
  for (const char c : value) {
    if (c == '\'') {
      quoted += "'\\''";
    } else {
      quoted += c;
    }
  }
  return quoted + "'";
}

inline int runShell(const std::string &command) {
  const int status = std::system(command.c_str());
#if defined(__unix__) || defined(__APPLE__)
  return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
#else
  return status;
#endif
}

inline std::string readText(const std::filesystem::path &path) {
  std::ifstream file(path, std::ios::binary);
  std::stringstream buffer;
  buffer << file.rdbuf();
  return buffer.str();
}

// Runs `command` with stdout and stderr captured next to `stem`.
inline Outcome runCaptured(const std::string &command, const std::filesystem::path &stem) {
  Outcome outcome;
  const std::filesystem::path outPath = stem.string() + ".out";
  const std::filesystem::path errPath = stem.string() + ".err";
  outcome.exitCode =
      runShell(command + " > " + quote(outPath.string()) + " 2> " + quote(errPath.string()));
  outcome.out = readText(outPath);
  outcome.err = readText(errPath);
  outcome.ran = true;
  return outcome;
}

inline Outcome runConfigUnnormalized(const ProgramCase &program, const ExecutionConfig &config) {
  const std::filesystem::path dir =
      primec::testing::testScratchPath("program_matrix/" + program.name + "/x").parent_path();
  std::filesystem::create_directories(dir);
  const std::filesystem::path source = dir / "program.prime";
  {
    std::ofstream file(source);
    file << program.source;
  }
  std::string flags;
  for (const std::string &flag : program.flags) {
    flags += " " + quote(flag);
  }
  const std::string entry = " --entry /main";
  const std::string src = quote(source.string());
  const std::filesystem::path stem = dir / config.name;
  switch (config.backend) {
  case Backend::VmStep:
  case Backend::Vm: {
    const std::string env = config.backend == Backend::VmStep ? "env PRIMEVM_KERNEL=step " : "";
    for (const std::string &flag : config.vmFlags) {
      flags += " " + quote(flag);
    }
    return runCaptured(
        env + "./primevm " + src + entry + " -O" + std::to_string(config.level) + flags, stem);
  }
  case Backend::Native:
  case Backend::OptExe:
  case Backend::Exe: {
    const std::string kind = config.backend == Backend::Native
                                 ? "native"
                                 : (config.backend == Backend::OptExe ? "optexe" : "exe");
    const std::filesystem::path binary = dir / (config.name + ".bin");
    const std::string level =
        config.backend == Backend::Exe ? "" : " -O" + std::to_string(config.level);
    const Outcome compile = runCaptured("./primec --emit=" + kind + level + " " + src + " -o " +
                                            quote(binary.string()) + entry + flags,
                                        dir / (config.name + ".compile"));
    if (compile.exitCode != 0) {
      Outcome failed = compile;
      failed.ran = false;
      failed.note = "compile failed: " + compile.err;
      return failed;
    }
    return runCaptured(quote(binary.string()), stem);
  }
  case Backend::Wasm: {
    const std::filesystem::path module = dir / (config.name + ".wasm");
    const Outcome compile = runCaptured("./primec --emit=wasm --wasm-profile wasi " + src + " -o " +
                                            quote(module.string()) + entry + flags,
                                        dir / (config.name + ".compile"));
    if (compile.exitCode != 0) {
      Outcome failed = compile;
      failed.ran = false;
      failed.note = "compile failed: " + compile.err;
      return failed;
    }
    const std::filesystem::path runner = dir / "wasm_runner.cjs";
    {
      std::ofstream file(runner);
      file << WasmNodeRunner;
    }
    return runCaptured("node " + quote(runner.string()) + " " + quote(module.string()), stem);
  }
  }
  return {};
}

// Where `runConfig` writes the source of a program.
inline std::filesystem::path programSourcePath(const ProgramCase &program) {
  return primec::testing::testScratchPath("program_matrix/" + program.name + "/x").parent_path() /
         "program.prime";
}

// Runs the program. The scratch directory differs per session, so a diagnostic that names the source file (`file:line:column:
// error: ...`) reads `{program}` in the captured stderr, and the expected text of a case is written with it.
inline Outcome runConfig(const ProgramCase &program, const ExecutionConfig &config) {
  Outcome outcome = runConfigUnnormalized(program, config);
  const std::string path = programSourcePath(program).string();
  for (size_t at = outcome.err.find(path); at != std::string::npos;
       at = outcome.err.find(path, at)) {
    outcome.err.replace(at, path.size(), "{program}");
  }
  return outcome;
}

inline bool isVmFamily(Backend backend) {
  return backend == Backend::VmStep || backend == Backend::Vm;
}

inline bool listed(const std::vector<std::string> &names, const std::string &name) {
  for (const std::string &candidate : names) {
    if (candidate == name) {
      return true;
    }
  }
  return false;
}

inline int expectedExit(int code) {
#if defined(__unix__) || defined(__APPLE__)
  return code & 0xff;
#else
  return code;
#endif
}

// The corpus check of one concurrent program: the eager schedule (`vm-O2`) runs once and must give the expected
// results; the threads config then runs `repeats` times and each run must print and exit as the eager run did. A
// program whose output depends on the interleaving does not belong here: it is made schedule-independent.
inline void runThreadsCorpus(const ProgramCase &program, int repeats) {
  const ExecutionConfig eagerConfig{"vm-O2", Backend::Vm, 2, false};
  const Outcome eager = runConfig(program, eagerConfig);
  INFO("program " << program.name << " on the eager schedule");
  REQUIRE_MESSAGE(eager.ran, eager.note);
  if (program.exitCode.has_value()) {
    CHECK(eager.exitCode == expectedExit(*program.exitCode));
  }
  if (program.stdoutText.has_value()) {
    CHECK(eager.out == *program.stdoutText);
  }
  if (program.vmStderr.has_value()) {
    CHECK(eager.err == *program.vmStderr);
  }
  // One primevm process compiles the program once and runs the module `repeats` times under --vm-threads
  // (--vm-repeat), so the front end is paid once, not per run. The runs' output follows one another and the exit
  // code is the runs' common one; a run that exits otherwise ends the process with a "vm-repeat:" line.
  ExecutionConfig threads = vmThreadsConfig();
  threads.vmFlags.push_back("--vm-repeat");
  threads.vmFlags.push_back(std::to_string(repeats));
  INFO("program " << program.name << " under --vm-threads, " << repeats << " runs");
  const Outcome outcome = runConfig(program, threads);
  REQUIRE_MESSAGE(outcome.ran, outcome.note);
  CHECK(outcome.err.find("vm-repeat:") == std::string::npos);
  CHECK(outcome.exitCode == eager.exitCode);
  std::string expectedOut;
  std::string expectedErr;
  for (int repeat = 0; repeat < repeats; ++repeat) {
    expectedOut += eager.out;
    expectedErr += eager.err;
  }
  CHECK(outcome.out == expectedOut);
  if (program.vmStderr.has_value()) {
    CHECK(outcome.err == expectedErr);
  }
}

// Counting mode (PRIMESTRUCT_MATRIX_WASM_COMPILE_ONLY=1): every case compiles for `--emit=wasm --wasm-profile wasi`
// and nothing runs, not even the other configs. One line per case goes to stderr, `wasm-compile: <name>: ok` or
// `wasm-compile: <name>: refused: <first line of the error>`, so the programs Wasm refuses can be counted per opcode;
// PRIMESTRUCT_MATRIX_WASM_DUMP_DIR=<dir> also copies each program's source to `<dir>/<name>.prime`.
inline bool wasmCompileOnly() {
  const char *value = std::getenv("PRIMESTRUCT_MATRIX_WASM_COMPILE_ONLY");
  return value != nullptr && *value != '\0';
}

inline void reportWasmCompile(const ProgramCase &program) {
  const std::filesystem::path source = programSourcePath(program);
  std::filesystem::create_directories(source.parent_path());
  {
    std::ofstream file(source);
    file << program.source;
  }
  if (const char *dumpDir = std::getenv("PRIMESTRUCT_MATRIX_WASM_DUMP_DIR");
      dumpDir != nullptr && *dumpDir != '\0') {
    // The program sources, one `<name>.prime` each, for reading the IR of a refused program.
    std::filesystem::create_directories(dumpDir);
    std::ofstream copy(std::filesystem::path(dumpDir) / (program.name + ".prime"));
    copy << program.source;
  }
  std::string flags;
  for (const std::string &flag : program.flags) {
    flags += " " + quote(flag);
  }
  const std::filesystem::path module = source.parent_path() / "wasm-count.wasm";
  const Outcome compile =
      runCaptured("./primec --emit=wasm --wasm-profile wasi " + quote(source.string()) + " -o " +
                      quote(module.string()) + " --entry /main" + flags,
                  source.parent_path() / "wasm-count.compile");
  std::string line = "wasm-compile: " + program.name + ": ";
  if (compile.exitCode == 0) {
    line += "ok";
  } else {
    line += "refused: " + compile.err.substr(0, compile.err.find('\n'));
  }
  std::fprintf(stderr, "%s\n", line.c_str());
}

// Probe mode (PRIMESTRUCT_MATRIX_WASM_RUN=1): every case runs on the VM (`vm-O2`) and on Wasm under node, and nothing
// is checked. One line per case goes to stderr, `wasm-run: <name>: agree`, `wasm-run: <name>: differ (...)` or
// `wasm-run: <name>: refused` (`agree (listed)` when the case already lists `wasm-wasi`), so the programs that may list
// `wasm-wasi` in `extraConfigs` can be found.
inline bool wasmRunProbe() {
  const char *value = std::getenv("PRIMESTRUCT_MATRIX_WASM_RUN");
  return value != nullptr && *value != '\0';
}

inline void reportWasmRun(const ProgramCase &program) {
  const Outcome oracle = runConfig(program, ExecutionConfig{"vm-O2", Backend::Vm, 2, false});
  const Outcome wasm = runConfig(program, ExecutionConfig{"wasm-wasi", Backend::Wasm, 0, true});
  std::string line = "wasm-run: " + program.name + ": ";
  if (!oracle.ran) {
    line += "no vm run";
  } else if (!wasm.ran) {
    line += "refused";
  } else if (wasm.exitCode != oracle.exitCode) {
    line += "differ (exit " + std::to_string(wasm.exitCode) + " not " +
            std::to_string(oracle.exitCode) + ")";
  } else if (wasm.out != oracle.out) {
    line += "differ (stdout)";
  } else {
    line += "agree";
    if (listed(program.extraConfigs, "wasm-wasi") || listed(program.onlyConfigs, "wasm-wasi")) {
      line += " (listed)";
    }
  }
  std::fprintf(stderr, "%s\n", line.c_str());
}

// Lists `wasm-wasi` for a program whose name is in `wasmPrograms` (the ones `PRIMESTRUCT_MATRIX_WASM_RUN=1` finds
// agreeing with the VM): in `onlyConfigs` when the case restricts its configs, in `extraConfigs` otherwise.
inline void listWasmIf(ProgramCase &program, const std::vector<std::string> &wasmPrograms) {
  if (!listed(wasmPrograms, program.name)) {
    return;
  }
  (program.onlyConfigs.empty() ? program.extraConfigs : program.onlyConfigs).push_back("wasm-wasi");
}

// Runs the program through the configs. The caller's doctest case reports each
// config separately through INFO/CHECK.
inline void runProgramMatrix(const ProgramCase &program,
                             std::vector<ExecutionConfig> configs = allConfigs()) {
  if (wasmCompileOnly()) {
    reportWasmCompile(program);
    return;
  }
  if (wasmRunProbe()) {
    reportWasmRun(program);
    return;
  }
  if (corpusOnly()) {
    // Corpus mode runs the programs that have a threads config, and nothing else.
    for (const ExecutionConfig &config : configs) {
      if (isThreadsConfig(config) && !listed(program.skipConfigs, config.name) &&
          (program.onlyConfigs.empty() || listed(program.onlyConfigs, config.name))) {
        runThreadsCorpus(program, threadsCorpusRepeats());
        return;
      }
    }
    return;
  }
  const bool slow = includeSlowConfigs();
  std::vector<ExecutionConfig> selected;
  for (const ExecutionConfig &config : configs) {
    if (listed(program.skipConfigs, config.name)) {
      continue;
    }
    if (!program.onlyConfigs.empty() && !listed(program.onlyConfigs, config.name) &&
        !listed(program.extraConfigs, config.name)) {
      continue;
    }
    const bool asked =
        listed(program.onlyConfigs, config.name) || listed(program.extraConfigs, config.name);
    if ((config.optIn || (config.slow && !slow)) && !asked) {
      continue;
    }
    if (config.backend == Backend::Wasm && !nodeAvailable()) {
      MESSAGE("SKIP: node is unavailable; ", config.name, " did not run ", program.name);
      continue;
    }
    selected.push_back(config);
  }
  if (selected.empty() && !nodeAvailable() && listed(program.onlyConfigs, "wasm-wasi")) {
    return; // only the Wasm config was asked for and node is missing: skipped above with a message
  }
  REQUIRE_MESSAGE(!selected.empty(), "no execution configs selected for ", program.name);

  std::optional<Outcome> oracle;
  for (const ExecutionConfig &config : selected) {
    INFO("program " << program.name << " on " << config.name);
    const Outcome outcome = runConfig(program, config);
    REQUIRE_MESSAGE(outcome.ran, outcome.note);
    if (!oracle.has_value()) {
      oracle = outcome;
    }
    if (program.exitCode.has_value()) {
      CHECK(outcome.exitCode == expectedExit(*program.exitCode));
    } else {
      CHECK(outcome.exitCode == oracle->exitCode);
    }
    if (program.stdoutText.has_value()) {
      CHECK(outcome.out == *program.stdoutText);
    } else if (!program.exitCode.has_value()) {
      CHECK(outcome.out == oracle->out);
    }
    const bool cppFamily = config.backend == Backend::OptExe || config.backend == Backend::Exe;
    const std::optional<std::string> &stderrText =
        config.backend == Backend::Wasm
            ? program.wasmStderr
            : (isVmFamily(config.backend) ? program.vmStderr
                                          : (cppFamily ? program.cppStderr : program.nativeStderr));
    // The Wasm config checks stderr only when the case sets `wasmStderr` (node adds its own text to other runs).
    if (stderrText.has_value()) {
      CHECK(outcome.err == *stderrText);
    }
  }
}

} // namespace program_matrix
