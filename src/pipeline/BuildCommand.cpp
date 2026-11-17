#include "primec/pipeline/BuildCommand.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>

namespace primec {

bool planBuildCommand(const std::vector<std::string> &buildArguments,
                      BuildCommandPlan &plan,
                      std::string &error) {
  plan = {};
  std::string directory = ".";
  std::vector<std::string> forwarded;
  bool sawOption = false;
  bool sawDirectory = false;
  std::string outDir;
  for (size_t i = 0; i < buildArguments.size(); ++i) {
    const std::string &argument = buildArguments[i];
    if (argument == "--dry-run") {
      plan.dryRun = true;
      continue;
    }
    std::string value;
    bool hasValue = false;
    const bool isJobs = argument == "--jobs" || argument.rfind("--jobs=", 0) == 0;
    const bool isOutDir = argument == "--out-dir" || argument.rfind("--out-dir=", 0) == 0;
    const bool isStage = argument == "--stage" || argument.rfind("--stage=", 0) == 0;
    if (isJobs || isOutDir || isStage) {
      const size_t equals = argument.find('=');
      if (equals != std::string::npos) {
        value = argument.substr(equals + 1);
        hasValue = true;
      } else if (i + 1 < buildArguments.size()) {
        value = buildArguments[++i];
        hasValue = true;
      }
      if (!hasValue) {
        error = argument + " requires a value";
        return false;
      }
      if (isOutDir) {
        outDir = value;
        continue;
      }
      if (isStage) {
        char *stageEnd = nullptr;
        const long stage = std::strtol(value.c_str(), &stageEnd, 10);
        if (value.empty() || *stageEnd != '\0' || stage < 0 || stage > 100000) {
          error = "--stage expects a number";
          return false;
        }
        plan.stage = static_cast<int>(stage);
        continue;
      }
      char *end = nullptr;
      const long jobs = std::strtol(value.c_str(), &end, 10);
      if (value.empty() || *end != '\0' || jobs < 1 || jobs > 1024) {
        error = "--jobs expects a number from 1 to 1024";
        return false;
      }
      plan.jobs = static_cast<int>(jobs);
      continue;
    }
    const bool isOption = !argument.empty() && argument[0] == '-';
    if (!isOption && !sawOption && !sawDirectory) {
      directory = argument;
      sawDirectory = true;
      continue;
    }
    sawOption = sawOption || isOption;
    forwarded.push_back(argument);
  }

  plan.forwarded = forwarded;
  plan.projectDir = directory.empty() ? "." : directory;
  plan.outDir = outDir.empty()
                    ? (std::filesystem::path(plan.projectDir) / "out").lexically_normal().string()
                    : outDir;
  const std::filesystem::path script = std::filesystem::path(directory) / "make.prime";
  plan.scriptPath = script.lexically_normal().string();
  std::error_code ec;
  if (!std::filesystem::is_regular_file(script, ec)) {
    error = "no make.prime in " + (directory.empty() ? std::string(".") : directory);
    return false;
  }
  std::ifstream file(script, std::ios::binary);
  if (!file) {
    error = "cannot read " + plan.scriptPath;
    return false;
  }
  plan.source.assign(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
  if (!plan.source.empty() && plan.source.back() != '\n') {
    plan.source += '\n';
  }
  plan.source += "import /std/build_runner/*\n";

  // The options of the caller come first; the settings that make this a build come last and win.
  plan.arguments.push_back("primec");
  plan.arguments.push_back(plan.scriptPath);
  for (const std::string &argument : forwarded) {
    plan.arguments.push_back(argument);
  }
  plan.arguments.push_back("--profile");
  plan.arguments.push_back("build");
  plan.arguments.push_back("--emit=vm");
  plan.arguments.push_back("--entry");
  plan.arguments.push_back("/std/build_runner/run");
  return true;
}

} // namespace primec
