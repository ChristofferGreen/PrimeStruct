#include "primec/pipeline/BuildSubprojects.h"

#include "primec/pipeline/BuildGraphJson.h"
#include "primec/pipeline/BuildRunner.h"

#include <algorithm>
#include <filesystem>
#include <map>
#include <set>
#include <thread>
#include <utility>

namespace primec {

namespace {

constexpr const char *DefaultCeiling = "file_read,heap_alloc";

std::vector<std::string> splitList(const std::string &text) {
  std::vector<std::string> parts;
  size_t start = 0;
  while (start <= text.size()) {
    size_t end = text.find(',', start);
    if (end == std::string::npos) {
      end = text.size();
    }
    if (end > start) {
      parts.push_back(text.substr(start, end - start));
    }
    start = end + 1;
  }
  return parts;
}

std::string scriptOf(const std::string &directory) {
  return directory == "." ? std::string("make.prime") : directory + "/make.prime";
}

// One script of the walk: the folder relative to the project root ("." for the project), what its raw dry run printed
// and the folders it includes (normalized, relative to the project root).
struct ScriptRun {
  std::string directory;
  int status = 0;
  std::string text;
  std::string diagnostics;
  std::vector<std::string> includes;
};

// Starts the raw dry run of the script in `directory`: the project's own options for the root, the options without
// `build_run` for a subproject.
void runScript(const BuildCommandPlan &plan,
               const std::string &selfPath,
               int stage,
               ScriptRun &run) {
  const bool isRoot = run.directory == ".";
  const std::filesystem::path folder =
      std::filesystem::path(plan.projectDir) / (isRoot ? "" : run.directory);
  std::vector<std::string> command = {selfPath,
                                      "build",
                                      "--dry-run",
                                      folder.lexically_normal().string(),
                                      "--raw",
                                      "--stage",
                                      std::to_string(stage)};
  const std::vector<std::string> options =
      isRoot ? plan.forwarded : subprojectForwardedOptions(plan.forwarded);
  command.insert(command.end(), options.begin(), options.end());
  std::string out;
  std::string err;
  const int status = runProgramCaptured(command, "", out, err);
  if (status != 0) {
    run.status = status > 0 ? status : 1;
    run.diagnostics = err + out;
    return;
  }
  BuildGraphData graph;
  std::string error;
  if (!parseBuildGraphJson(out, graph, error)) {
    run.status = 1;
    run.diagnostics = "primec build: " + scriptOf(run.directory) + ": " + error + "\n";
    return;
  }
  run.text = std::move(out);
  run.includes = graph.subprojects;
}

// The folder `name`, named by the script of `from`, as a path relative to the project root; empty with `error` set when
// it is not a folder inside the project.
std::string includedFolder(const std::string &from, const std::string &name, std::string &error) {
  const std::filesystem::path given(name);
  std::string normalized =
      (std::filesystem::path(from) / given).lexically_normal().generic_string();
  while (normalized.size() > 1 && normalized.back() == '/') {
    normalized.pop_back();
  }
  if (name.empty() || given.is_absolute() || normalized == ".." ||
      normalized.rfind("../", 0) == 0) {
    error = "primec build: " + scriptOf(from) + ": subproject \"" + name +
            "\" is not a folder inside the project\n";
    return std::string();
  }
  return normalized;
}

// The first inclusion cycle reachable from the project, as the scripts along it; empty when there is none. The edges are
// followed in sorted order, so the same graph always names the same cycle.
class CycleFinder {
public:
  explicit CycleFinder(const std::map<std::string, ScriptRun> &runs) : runs_(runs) {
  }

  std::string find() {
    std::vector<std::string> stack;
    return visit(".", stack);
  }

private:
  const std::map<std::string, ScriptRun> &runs_;
  std::set<std::string> done_;

  std::string visit(const std::string &directory, std::vector<std::string> &stack) {
    const auto onStack = std::find(stack.begin(), stack.end(), directory);
    if (onStack != stack.end()) {
      std::string chain;
      for (auto it = onStack; it != stack.end(); ++it) {
        chain += scriptOf(*it) + " -> ";
      }
      return chain + scriptOf(directory);
    }
    if (!done_.insert(directory).second) {
      return std::string();
    }
    stack.push_back(directory);
    std::vector<std::string> children = runs_.at(directory).includes;
    std::sort(children.begin(), children.end());
    for (const std::string &child : children) {
      const std::string cycle = visit(child, stack);
      if (!cycle.empty()) {
        return cycle;
      }
    }
    stack.pop_back();
    return std::string();
  }
};

} // namespace

std::vector<std::string> subprojectForwardedOptions(const std::vector<std::string> &forwarded) {
  std::string list = DefaultCeiling;
  for (size_t i = 0; i < forwarded.size(); ++i) {
    const std::string &argument = forwarded[i];
    if (argument == "--allow-effects" && i + 1 < forwarded.size()) {
      list = forwarded[i + 1];
    } else if (argument.rfind("--allow-effects=", 0) == 0) {
      list = argument.substr(std::string("--allow-effects=").size());
    }
  }
  std::string kept;
  for (const std::string &effect : splitList(list)) {
    if (effect == "build_run" || effect == "none") {
      continue;
    }
    kept += (kept.empty() ? "" : ",") + effect;
  }
  std::vector<std::string> options = forwarded;
  options.push_back("--allow-effects=" + (kept.empty() ? std::string("none") : kept));
  return options;
}

int resolveBuildGraph(const BuildCommandPlan &plan,
                      const std::string &selfPath,
                      int stage,
                      std::string &graphText,
                      std::string &diagnostics) {
  // The scripts run level by level: every script a level names runs at once, each in a process of its own, and a folder
  // that two scripts name is run once, by the first level that reaches it.
  std::map<std::string, ScriptRun> runs;
  std::vector<std::string> level = {"."};
  std::set<std::string> seen = {"."};
  while (!level.empty()) {
    std::vector<ScriptRun> batch(level.size());
    std::vector<std::thread> threads;
    for (size_t i = 0; i < level.size(); ++i) {
      batch[i].directory = level[i];
      threads.emplace_back(
          [&plan, &selfPath, stage, &batch, i]() { runScript(plan, selfPath, stage, batch[i]); });
    }
    for (std::thread &thread : threads) {
      thread.join();
    }
    int failure = 0;
    for (const ScriptRun &run : batch) {
      if (run.status != 0) {
        failure = failure == 0 ? run.status : failure;
        diagnostics += run.diagnostics;
      }
    }
    if (failure != 0) {
      return failure;
    }
    std::vector<std::string> next;
    for (ScriptRun &run : batch) {
      std::vector<std::string> folders;
      for (const std::string &name : run.includes) {
        std::string error;
        const std::string folder = includedFolder(run.directory, name, error);
        if (folder.empty()) {
          diagnostics += error;
          return 1;
        }
        folders.push_back(folder);
        if (!seen.insert(folder).second) {
          continue;
        }
        std::error_code ec;
        const std::filesystem::path script =
            std::filesystem::path(plan.projectDir) / folder / "make.prime";
        if (!std::filesystem::is_regular_file(script, ec)) {
          diagnostics += "primec build: " + scriptOf(run.directory) + ": subproject \"" + name +
                         "\" has no make.prime (" + script.lexically_normal().string() + ")\n";
          return 1;
        }
        next.push_back(folder);
      }
      run.includes = std::move(folders);
      const std::string directory = run.directory;
      runs[directory] = std::move(run);
    }
    std::sort(next.begin(), next.end());
    level = std::move(next);
  }
  const std::string cycle = CycleFinder(runs).find();
  if (!cycle.empty()) {
    diagnostics += "primec build: subproject cycle: " + cycle + "\n";
    return 1;
  }
  ScriptRun &root = runs.at(".");
  if (runs.size() == 1) {
    graphText = std::move(root.text);
    return 0;
  }
  // `runs` is sorted by folder, so the merge order is the same on every run.
  std::vector<BuildSubgraph> subgraphs;
  for (auto &entry : runs) {
    if (entry.first != ".") {
      subgraphs.push_back({entry.first, std::move(entry.second.text)});
    }
  }
  std::string error;
  if (!mergeBuildGraphJson(root.text, subgraphs, plan.projectDir, graphText, error)) {
    diagnostics += "primec build: " + error + "\n";
    return 1;
  }
  return 0;
}

} // namespace primec
