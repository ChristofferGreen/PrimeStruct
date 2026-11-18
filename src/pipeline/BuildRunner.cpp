#include "primec/pipeline/BuildRunner.h"

#include "primec/pipeline/BuildCache.h"
#include "primec/pipeline/BuildCopyStep.h"
#include "primec/pipeline/BuildFunctionStep.h"
#include "primec/pipeline/BuildFunctionSteps.h"
#include "primec/pipeline/BuildStepScheduler.h"
#include "primec/pipeline/BuildSubprojects.h"
#include "primec/support/Sha256.h"

#include <algorithm>
#include <atomic>
#include <cerrno>
#include <cstdlib>
#include <fcntl.h>
#include <filesystem>
#include <mutex>
#include <memory>
#include <ostream>
#include <poll.h>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>
#include <utility>

#if defined(__APPLE__)
#include <mach-o/dyld.h>
#endif

namespace primec {

namespace {

std::string joinList(const std::vector<std::string> &values) {
  std::string joined;
  for (const std::string &value : values) {
    if (!joined.empty()) {
      joined += ',';
    }
    joined += value;
  }
  return joined;
}

// Starting a child and closing the write ends of its pipes happen under one lock, so a child started by another
// thread never holds the write end of this child's pipe (a reader would wait for it).
std::mutex &spawnMutex() {
  static std::mutex mutex;
  return mutex;
}

void drain(int fd, std::string &into) {
  char buffer[4096];
  const ssize_t got = ::read(fd, buffer, sizeof(buffer));
  if (got > 0) {
    into.append(buffer, static_cast<size_t>(got));
  }
}

std::string absoluteAgainst(const std::string &path, const std::string &base) {
  const std::filesystem::path given(path);
  if (given.is_absolute() || base.empty()) {
    return given.lexically_normal().string();
  }
  return (std::filesystem::path(base) / given).lexically_normal().string();
}

// Where `primec` puts the output of `step`: the output name inside the output directory.
std::string buildStepOutputPath(const BuildStep &step, const BuildRunSettings &settings) {
  const std::string outDir = absoluteAgainst(settings.outDir, settings.projectDir);
  return absoluteAgainst(step.output, outDir);
}

// Records what the finished step read, so the next run can tell whether anything changed. The compiler's own list
// (`--emit-deps`) is joined by the files named by link arguments, which the compiler hands to the toolchain.
// A step whose inputs cannot all be hashed gets no entry and runs again next time.
void storeBuildCacheEntry(const BuildStep &step,
                          const BuildRunSettings &settings,
                          const std::string &cacheFile,
                          const std::string &key,
                          const std::string &outputPath) {
  std::vector<std::string> paths;
  if (!readDependencyList(cacheFile + ".deps", paths)) {
    return;
  }
  for (const std::string &arg : step.linkArgs) {
    if (arg.empty() || arg[0] == '-') {
      continue;
    }
    const std::string candidate = absoluteAgainst(arg, settings.projectDir);
    std::error_code ec;
    if (std::filesystem::is_regular_file(candidate, ec)) {
      paths.push_back(candidate);
    }
  }
  BuildCacheEntry entry;
  entry.key = key;
  if (!sha256FileHex(outputPath, entry.outputHash) || !hashBuildDependencies(paths, entry.deps)) {
    return;
  }
  // The directories the script's glob read: an entry that appears in or leaves one makes the step stale.
  std::vector<std::string> inputDirs;
  for (const std::string &dir : step.inputDirs) {
    inputDirs.push_back(absoluteAgainst(dir, settings.projectDir));
  }
  entry.dirs = hashBuildInputDirs(inputDirs);
  writeBuildCacheEntry(cacheFile, entry);
}

enum class CopyOutcome { Copied, UpToDate, Failed };

// Runs a copy step: the file `step.source` (relative to the project) is copied to `step.output` inside the output
// directory. The cache entry holds the content hash of the source and of the copy, so an unchanged source with an
// intact copy is not copied again, and an edited or deleted copy is made again (only this step).
CopyOutcome runCopyStep(const BuildStep &step,
                        const BuildRunSettings &settings,
                        bool caching,
                        std::string &diagnostics) {
  const std::string outDir = absoluteAgainst(settings.outDir, settings.projectDir);
  std::string destination;
  if (!resolveCopyDestination(step.output, outDir, destination, diagnostics)) {
    diagnostics += "\n";
    return CopyOutcome::Failed;
  }
  const std::string source = absoluteAgainst(step.source, settings.projectDir);
  std::string cacheFile;
  std::string key;
  if (caching) {
    cacheFile = buildCacheFilePath(settings.cacheDir, step.id);
    // The compiler does not take part: a copy does not change with its version.
    key = buildStepKey("copy", {step.kind, step.source, step.output});
    BuildCacheEntry entry;
    if (readBuildCacheEntry(cacheFile, entry) &&
        buildCacheEntryIsCurrent(entry, key, destination)) {
      return CopyOutcome::UpToDate;
    }
    std::error_code ec;
    std::filesystem::remove(cacheFile, ec);
    std::filesystem::create_directories(settings.cacheDir, ec);
  }
  if (!copyBuildFile(source, destination, diagnostics)) {
    diagnostics += "\n";
    return CopyOutcome::Failed;
  }
  if (caching) {
    BuildCacheEntry entry;
    entry.key = key;
    if (sha256FileHex(destination, entry.outputHash) &&
        hashBuildDependencies({source}, entry.deps)) {
      writeBuildCacheEntry(cacheFile, entry);
    }
  }
  return CopyOutcome::Copied;
}

} // namespace

std::vector<std::string> buildStepArguments(const BuildStep &step,
                                            const BuildRunSettings &settings) {
  std::vector<std::string> arguments;
  arguments.push_back(settings.primecPath);
  arguments.push_back("--emit=" + step.emitKind);
  arguments.push_back("-O" + std::to_string(step.optimization));
  if (!step.entry.empty()) {
    arguments.push_back("--entry");
    arguments.push_back(step.entry);
  }
  for (const std::string &path : step.importPaths) {
    arguments.push_back("--import-path");
    arguments.push_back(path);
  }
  if (!step.transforms.empty()) {
    arguments.push_back("--transform-list=" + joinList(step.transforms));
  }
  // An empty list leaves the default effects of the compiler as they are.
  if (!step.defaultEffects.empty()) {
    arguments.push_back("--default-effects=" + joinList(step.defaultEffects));
  }
  for (const std::string &arg : step.linkArgs) {
    arguments.push_back("--link-arg=" + arg);
  }
  arguments.push_back("--out-dir");
  arguments.push_back(settings.outDir);
  arguments.push_back("-o");
  arguments.push_back(step.output);
  arguments.push_back(step.source);
  return arguments;
}

int runProgramCaptured(const std::vector<std::string> &arguments,
                       const std::string &workingDir,
                       std::string &capturedOut,
                       std::string &capturedErr) {
  capturedOut.clear();
  capturedErr.clear();
  if (arguments.empty()) {
    return -1;
  }
  std::vector<char *> argv;
  argv.reserve(arguments.size() + 1);
  for (const std::string &argument : arguments) {
    argv.push_back(const_cast<char *>(argument.c_str()));
  }
  argv.push_back(nullptr);

  int outPipe[2] = {-1, -1};
  int errPipe[2] = {-1, -1};
  pid_t pid = -1;
  {
    std::lock_guard<std::mutex> lock(spawnMutex());
    if (::pipe(outPipe) != 0) {
      return -1;
    }
    if (::pipe(errPipe) != 0) {
      ::close(outPipe[0]);
      ::close(outPipe[1]);
      return -1;
    }
    for (int fd : {outPipe[0], errPipe[0]}) {
      ::fcntl(fd, F_SETFD, FD_CLOEXEC);
    }
    pid = ::fork();
    if (pid == 0) {
      // Only calls that are safe between fork and exec.
      if (!workingDir.empty() && ::chdir(workingDir.c_str()) != 0) {
        ::_exit(127);
      }
      ::dup2(outPipe[1], STDOUT_FILENO);
      ::dup2(errPipe[1], STDERR_FILENO);
      ::close(outPipe[0]);
      ::close(outPipe[1]);
      ::close(errPipe[0]);
      ::close(errPipe[1]);
      ::execv(argv[0], argv.data());
      ::_exit(127);
    }
    ::close(outPipe[1]);
    ::close(errPipe[1]);
  }
  if (pid < 0) {
    ::close(outPipe[0]);
    ::close(errPipe[0]);
    return -1;
  }

  bool outOpen = true;
  bool errOpen = true;
  while (outOpen || errOpen) {
    struct pollfd fds[2];
    nfds_t count = 0;
    int outIndex = -1;
    int errIndex = -1;
    if (outOpen) {
      outIndex = static_cast<int>(count);
      fds[count++] = {outPipe[0], POLLIN, 0};
    }
    if (errOpen) {
      errIndex = static_cast<int>(count);
      fds[count++] = {errPipe[0], POLLIN, 0};
    }
    if (::poll(fds, count, -1) < 0) {
      if (errno == EINTR) {
        continue;
      }
      break;
    }
    if (outIndex >= 0 && (fds[outIndex].revents & (POLLIN | POLLHUP | POLLERR)) != 0) {
      const size_t before = capturedOut.size();
      drain(outPipe[0], capturedOut);
      outOpen = capturedOut.size() != before;
    }
    if (errIndex >= 0 && (fds[errIndex].revents & (POLLIN | POLLHUP | POLLERR)) != 0) {
      const size_t before = capturedErr.size();
      drain(errPipe[0], capturedErr);
      errOpen = capturedErr.size() != before;
    }
  }
  ::close(outPipe[0]);
  ::close(errPipe[0]);

  int status = 0;
  while (::waitpid(pid, &status, 0) < 0) {
    if (errno != EINTR) {
      return -1;
    }
  }
  return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
}

std::string selfExecutablePath(const std::string &argv0) {
#if defined(__APPLE__)
  uint32_t size = 0;
  _NSGetExecutablePath(nullptr, &size);
  std::string buffer(size, '\0');
  if (_NSGetExecutablePath(buffer.data(), &size) == 0) {
    buffer.resize(std::char_traits<char>::length(buffer.c_str()));
    std::error_code ec;
    const std::filesystem::path canonical = std::filesystem::canonical(buffer, ec);
    return ec ? buffer : canonical.string();
  }
#else
  std::error_code ec;
  const std::filesystem::path self = std::filesystem::read_symlink("/proc/self/exe", ec);
  if (!ec) {
    return self.string();
  }
#endif
  return argv0;
}

int runBuildGraph(const BuildGraphData &graph,
                  const BuildRunSettings &settings,
                  std::ostream &out,
                  std::ostream &err) {
  if (!graph.problems.empty()) {
    for (const std::string &problem : graph.problems) {
      err << "primec build: " << problem << "\n";
    }
    return 1;
  }
  if (!graph.unsupportedNodes.empty()) {
    for (const std::string &id : graph.unsupportedNodes) {
      err << "primec build: " << id
          << ": static libraries are not supported by this compiler yet\n";
    }
    return 1;
  }

  const std::vector<std::string> orderProblems = checkStepOrder(graph.steps);
  if (!orderProblems.empty()) {
    for (const std::string &problem : orderProblems) {
      err << "primec build: " << problem << "\n";
    }
    return 1;
  }

  enum class State { Pending, Built, UpToDate, Failed };
  struct Result {
    State state = State::Pending;
    std::string diagnostics;
  };
  std::vector<Result> results(graph.steps.size());

  // The compiler executable stands for the compiler version; without a readable one nothing is cached.
  std::string compilerHash;
  const bool caching =
      !settings.cacheDir.empty() && sha256FileHex(settings.primecPath, compilerHash);

  std::atomic<bool> stop{false};

  // Function steps are settled and compiled here, on the calling thread, one at a time: the compile pipeline keeps
  // process-wide state, so two compiles at once would share it. A step that is up to date is not compiled. The run of
  // the compiled definitions is what the workers do.
  std::vector<std::unique_ptr<BuildFunctionStepRunner::Prepared>> prepared(graph.steps.size());
  std::vector<std::vector<std::string>> compiledFiles(graph.steps.size());
  std::vector<std::string> functionCacheFiles(graph.steps.size());
  std::vector<std::string> functionKeys(graph.steps.size());
  for (size_t index = 0; index < graph.steps.size() && !stop.load(); ++index) {
    const BuildStep &step = graph.steps[index];
    if (step.kind != "function") {
      continue;
    }
    Result &result = results[index];
    if (settings.functionSteps == nullptr) {
      result.state = State::Failed;
      result.diagnostics = "function steps are not supported by this build of primec\n";
      stop.store(true);
      break;
    }
    const FunctionStepCheck check = checkFunctionStep(step,
                                                      settings,
                                                      caching,
                                                      compilerHash,
                                                      functionCacheFiles[index],
                                                      functionKeys[index],
                                                      result.diagnostics);
    if (check == FunctionStepCheck::UpToDate) {
      result.state = State::UpToDate;
      continue;
    }
    if (check == FunctionStepCheck::Failed) {
      result.state = State::Failed;
      stop.store(true);
      break;
    }
    prepared[index] =
        settings.functionSteps->prepare(step, settings, compiledFiles[index], result.diagnostics);
    if (prepared[index] == nullptr) {
      result.state = State::Failed;
      stop.store(true);
    }
  }

  // A step starts when the steps it names in `after` have finished successfully; a step settled above (up to date) is
  // finished. A failure stops everything, so the steps behind a failed one are reported as skipped.
  std::vector<bool> settled(graph.steps.size(), false);
  for (size_t i = 0; i < graph.steps.size(); ++i) {
    settled[i] = results[i].state == State::UpToDate;
  }
  BuildStepScheduler scheduler(graph.steps, settled);
  if (stop.load()) {
    scheduler.stop();
  }

  auto execute = [&](size_t index) {
    const BuildStep &step = graph.steps[index];
    Result &result = results[index];
    if (step.kind == "function") {
      // Settled above (up to date) or compiled and waiting to run on this thread.
      if (prepared[index] == nullptr) {
        return;
      }
      const bool ran =
          settings.functionSteps->run(*prepared[index], step, settings, result.diagnostics);
      if (ran && finishFunctionStep(step,
                                    settings,
                                    caching,
                                    functionCacheFiles[index],
                                    functionKeys[index],
                                    compiledFiles[index],
                                    result.diagnostics)) {
        result.state = State::Built;
      } else {
        result.state = State::Failed;
      }
      return;
    }
    if (step.kind == "copy") {
      const CopyOutcome outcome = runCopyStep(step, settings, caching, result.diagnostics);
      if (outcome == CopyOutcome::Failed) {
        result.state = State::Failed;
      } else {
        result.state = outcome == CopyOutcome::UpToDate ? State::UpToDate : State::Built;
      }
      return;
    }
    std::vector<std::string> arguments = buildStepArguments(step, settings);

    std::string cacheFile;
    std::string key;
    const std::string outputPath = buildStepOutputPath(step, settings);
    if (caching) {
      cacheFile = buildCacheFilePath(settings.cacheDir, step.id);
      // The key also names the watched directories, so a script that starts to watch another one (a new pattern
      // that reaches a directory the old one did not) is a different step even when no listing changed.
      std::vector<std::string> keyArguments(arguments.begin() + 1, arguments.end());
      std::vector<std::string> watched = step.inputDirs;
      std::sort(watched.begin(), watched.end());
      watched.erase(std::unique(watched.begin(), watched.end()), watched.end());
      for (const std::string &dir : watched) {
        keyArguments.push_back("input-dir:" + dir);
      }
      key = buildStepKey(compilerHash, keyArguments);
      BuildCacheEntry entry;
      if (readBuildCacheEntry(cacheFile, entry) &&
          buildCacheEntryIsCurrent(entry, key, outputPath)) {
        result.state = State::UpToDate;
        return;
      }
      // The previous run no longer describes the output; a failed or interrupted run must not leave it valid.
      std::error_code ec;
      std::filesystem::remove(cacheFile, ec);
      std::filesystem::create_directories(settings.cacheDir, ec);
      // Before the source, the last argument.
      arguments.insert(arguments.end() - 1, {"--emit-deps", cacheFile + ".deps"});
    }

    std::string capturedOut;
    std::string capturedErr;
    const int status = runProgramCaptured(arguments, settings.projectDir, capturedOut, capturedErr);
    result.diagnostics = capturedErr + capturedOut;
    if (caching && status == 0) {
      storeBuildCacheEntry(step, settings, cacheFile, key, outputPath);
    }
    if (caching) {
      std::error_code ec;
      std::filesystem::remove(cacheFile + ".deps", ec);
    }
    if (status == 0) {
      // `--emit=optexe` leaves the generated C++ next to the executable; the executable is the result, and the cache
      // entry does not know the file (scripts/build_native_app.sh deletes it as well).
      if (step.emitKind == "optexe") {
        std::error_code ec;
        std::filesystem::remove(outputPath + ".cpp", ec);
      }
      result.state = State::Built;
    } else {
      result.state = State::Failed;
      if (status < 0) {
        result.diagnostics += "the compiler could not be run\n";
      }
    }
  };
  auto worker = [&]() {
    while (true) {
      const size_t index = scheduler.take();
      if (index == BuildStepScheduler::None) {
        return;
      }
      execute(index);
      const State state = results[index].state;
      const bool succeeded = state == State::Built || state == State::UpToDate;
      if (!succeeded) {
        scheduler.stop();
      }
      scheduler.finish(index, succeeded);
    }
  };
  const size_t workers =
      std::min(static_cast<size_t>(settings.jobs < 1 ? 1 : settings.jobs), graph.steps.size());
  std::vector<std::thread> threads;
  for (size_t i = 1; i < workers; ++i) {
    threads.emplace_back(worker);
  }
  if (!graph.steps.empty()) {
    worker();
  }
  for (std::thread &thread : threads) {
    thread.join();
  }

  int exitCode = 0;
  for (size_t i = 0; i < graph.steps.size(); ++i) {
    const BuildStep &step = graph.steps[i];
    switch (results[i].state) {
    case State::Built:
      out << "built " << step.id << "\n";
      break;
    case State::UpToDate:
      out << "up to date " << step.id << "\n";
      break;
    case State::Failed:
      err << "primec build: step " << step.name << " (" << step.id << ") failed\n"
          << results[i].diagnostics;
      exitCode = 1;
      break;
    case State::Pending:
      err << "primec build: step " << step.name << " (" << step.id << ") skipped after a failure\n";
      break;
    }
  }
  return exitCode;
}

} // namespace primec

namespace primec {

namespace {

// More stages than this are a script that never finishes (each `run` is one stage).
constexpr int MaxBuildStages = 256;

} // namespace

int runBuildCommand(const BuildCommandPlan &plan,
                    const std::string &selfPath,
                    std::ostream &out,
                    std::ostream &err,
                    BuildFunctionStepRunner *functionSteps) {
  BuildRunSettings settings;
  settings.functionSteps = functionSteps;
  settings.primecPath = selfPath;
  settings.projectDir = plan.projectDir;
  std::error_code ec;
  const std::filesystem::path outDir = std::filesystem::absolute(plan.outDir, ec);
  settings.outDir = ec ? plan.outDir : outDir.lexically_normal().string();
  const unsigned cores = std::thread::hardware_concurrency();
  settings.cacheDir = (std::filesystem::absolute(plan.projectDir, ec) / ".ps" / "cache")
                          .lexically_normal()
                          .string();
  settings.jobs = plan.jobs > 0 ? plan.jobs : static_cast<int>(cores > 0 ? cores : 1);

  // One dry run of the script per stage: stage N stops at the Nth `Graph.run` and exports the graph it ran; the stage
  // after the last `run` is the graph the script returns. Every stage starts the script again, so the stages share
  // the cache of the steps and a step a later graph repeats is up to date.
  std::vector<BuildOption> declaredOptions;
  for (int stage = 0; stage < MaxBuildStages; ++stage) {
    std::string graphText;
    std::string graphDiagnostics;
    const int graphStatus = resolveBuildGraph(plan, selfPath, stage, graphText, graphDiagnostics);
    if (graphStatus != 0) {
      err << graphDiagnostics;
      err << "primec build: the script " << plan.scriptPath << " did not produce a graph\n";
      return graphStatus;
    }
    BuildGraphData graph;
    std::string error;
    if (!parseBuildGraphJson(graphText, graph, error)) {
      err << "primec build: " << error << "\n";
      return 1;
    }
    if (graph.incomplete || stage > 0) {
      out << "stage " << (stage + 1) << "\n";
    }
    // The options of every stage: a script may declare an option in a later stage, so a name nobody declared is only
    // known to be unknown after the last stage (a single-stage script is checked when its graph is resolved).
    for (const BuildOption &option : graph.options) {
      if (std::none_of(declaredOptions.begin(),
                       declaredOptions.end(),
                       [&](const BuildOption &seen) { return seen.name == option.name; })) {
        declaredOptions.push_back(option);
      }
    }
    const int status = runBuildGraph(graph, settings, out, err);
    if (status == 0 && !graph.incomplete && stage > 0) {
      const std::string unknown = checkBuildOptions(plan.options, declaredOptions, true);
      if (!unknown.empty()) {
        err << unknown;
        return 1;
      }
    }
    if (status != 0 || !graph.incomplete) {
      return status;
    }
  }
  err << "primec build: " << plan.scriptPath << " runs more than " << MaxBuildStages << " stages\n";
  return 1;
}

} // namespace primec
