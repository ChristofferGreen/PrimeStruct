#include "primec/pipeline/BuildFunctionSteps.h"

#include "primec/pipeline/BuildCache.h"
#include "primec/pipeline/BuildCopyStep.h"
#include "primec/support/Sha256.h"

#include <algorithm>
#include <filesystem>
#include <system_error>

namespace primec {

namespace {

std::string absoluteAgainst(const std::string &path, const std::string &base) {
  const std::filesystem::path given(path);
  if (given.is_absolute() || base.empty()) {
    return given.lexically_normal().string();
  }
  return (std::filesystem::path(base) / given).lexically_normal().string();
}

std::string outputDirectory(const BuildRunSettings &settings) {
  return absoluteAgainst(settings.outDir, settings.projectDir);
}

// The hash that stands for all outputs of a step: the hash of every output file in the order of the declaration, each
// with its path. False, with the path in `missing`, when an output cannot be read.
bool hashStepOutputs(const BuildStep &step,
                     const BuildRunSettings &settings,
                     std::string &hash,
                     std::string &missing) {
  std::string text;
  for (const std::string &output : step.outputs) {
    std::string resolved;
    std::string ignored;
    if (!resolveStepOutputPath(output, outputDirectory(settings), resolved, ignored)) {
      missing = output;
      return false;
    }
    std::string fileHash;
    if (!sha256FileHex(resolved, fileHash)) {
      missing = output;
      return false;
    }
    text += std::to_string(output.size()) + ":" + output + " " + fileHash + "\n";
  }
  hash = sha256Hex(text);
  return true;
}

std::string functionStepKey(const BuildStep &step,
                            const BuildRunSettings &settings,
                            const std::string &compilerHash) {
  std::vector<std::string> fields = {
      "function", step.source, step.function, outputDirectory(settings)};
  fields.push_back("inputs:" + std::to_string(step.inputs.size()));
  fields.insert(fields.end(), step.inputs.begin(), step.inputs.end());
  fields.push_back("outputs:" + std::to_string(step.outputs.size()));
  fields.insert(fields.end(), step.outputs.begin(), step.outputs.end());
  return buildStepKey(compilerHash, fields);
}

} // namespace

FunctionStepCheck checkFunctionStep(const BuildStep &step,
                                    const BuildRunSettings &settings,
                                    bool caching,
                                    const std::string &compilerHash,
                                    std::string &cacheFile,
                                    std::string &key,
                                    std::string &diagnostics) {
  if (step.function.empty() || step.source.empty()) {
    diagnostics = "a function step needs a file and a function\n";
    return FunctionStepCheck::Failed;
  }
  for (const std::string &input : step.inputs) {
    std::error_code ec;
    if (!std::filesystem::is_regular_file(absoluteAgainst(input, settings.projectDir), ec)) {
      diagnostics = "the declared input " + input + " is not a file\n";
      return FunctionStepCheck::Failed;
    }
  }
  for (const std::string &output : step.outputs) {
    std::string resolved;
    std::string error;
    if (!resolveStepOutputPath(output, outputDirectory(settings), resolved, error)) {
      diagnostics = "the declared output " + output + " is invalid: " + error + "\n";
      return FunctionStepCheck::Failed;
    }
  }
  if (!caching) {
    return FunctionStepCheck::NeedsRun;
  }
  cacheFile = buildCacheFilePath(settings.cacheDir, step.id);
  key = functionStepKey(step, settings, compilerHash);
  BuildCacheEntry entry;
  std::string outputHash;
  std::string missing;
  if (readBuildCacheEntry(cacheFile, entry) &&
      hashStepOutputs(step, settings, outputHash, missing) &&
      buildCacheEntryIsCurrentForOutput(entry, key, outputHash)) {
    return FunctionStepCheck::UpToDate;
  }
  // The previous run no longer describes the outputs; a failed or interrupted run must not leave it valid.
  std::error_code ec;
  std::filesystem::remove(cacheFile, ec);
  std::filesystem::create_directories(settings.cacheDir, ec);
  return FunctionStepCheck::NeedsRun;
}

bool finishFunctionStep(const BuildStep &step,
                        const BuildRunSettings &settings,
                        bool caching,
                        const std::string &cacheFile,
                        const std::string &key,
                        const std::vector<std::string> &files,
                        std::string &diagnostics) {
  std::string outputHash;
  std::string missing;
  if (!hashStepOutputs(step, settings, outputHash, missing)) {
    diagnostics += "the step did not write its declared output " + missing + "\n";
    return false;
  }
  if (!caching) {
    return true;
  }
  std::vector<std::string> paths = files;
  for (const std::string &input : step.inputs) {
    paths.push_back(absoluteAgainst(input, settings.projectDir));
  }
  BuildCacheEntry entry;
  entry.key = key;
  entry.outputHash = outputHash;
  if (hashBuildDependencies(paths, entry.deps)) {
    writeBuildCacheEntry(cacheFile, entry);
  }
  return true;
}

} // namespace primec
