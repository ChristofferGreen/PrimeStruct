#include "primec/pipeline/BuildCommand.h"

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
  for (const std::string &argument : buildArguments) {
    if (argument == "--dry-run") {
      plan.dryRun = true;
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
