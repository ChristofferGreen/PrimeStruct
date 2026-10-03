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

#include "third_party/doctest.h"

#include "primec/testing/TestScratch.h"

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

enum class Backend { VmStep, Vm, Native, OptExe, Exe };

struct ExecutionConfig {
  std::string name;
  Backend backend = Backend::Vm;
  int level = 0;
  // Slow to build (host C++ compiler): only with PRIMESTRUCT_MATRIX_CONFIGS=all
  // or when a case lists the config explicitly.
  bool slow = false;
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
  std::vector<std::string> flags; // extra primec/primevm flags, e.g. --default-effects=io_out
  std::vector<std::string> skipConfigs;
  std::vector<std::string> onlyConfigs; // restrict to these when non-empty
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
  return configs;
}

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

inline Outcome runConfig(const ProgramCase &program, const ExecutionConfig &config) {
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
  }
  return {};
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

// Runs the program through the configs. The caller's doctest case reports each
// config separately through INFO/CHECK.
inline void runProgramMatrix(const ProgramCase &program,
                             std::vector<ExecutionConfig> configs = allConfigs()) {
  const bool slow = includeSlowConfigs();
  std::vector<ExecutionConfig> selected;
  for (const ExecutionConfig &config : configs) {
    if (listed(program.skipConfigs, config.name)) {
      continue;
    }
    if (!program.onlyConfigs.empty() && !listed(program.onlyConfigs, config.name)) {
      continue;
    }
    if (config.slow && !slow && !listed(program.onlyConfigs, config.name)) {
      continue;
    }
    selected.push_back(config);
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
    const std::optional<std::string> &stderrText =
        isVmFamily(config.backend) ? program.vmStderr : program.nativeStderr;
    if (stderrText.has_value() &&
        (config.backend != Backend::OptExe && config.backend != Backend::Exe)) {
      CHECK(outcome.err == *stderrText);
    }
  }
}

} // namespace program_matrix
