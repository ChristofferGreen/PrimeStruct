#include "primec/pipeline/BuildFunctionStepEmbed.h"

#include "primec/embed/ScriptEngine.h"
#include "primec/pipeline/BuildCopyStep.h"
#include "primec/pipeline/BuildRunner.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <map>
#include <string_view>
#include <system_error>
#include <utility>

namespace primec {

namespace {

// The effects a function step may use. `file_write` is missing on purpose: files are written through
// `/std/build_step`, whose host call checks the path. There is no network and no `build_run`.
const std::vector<std::string> &stepEffectCeiling() {
  static const std::vector<std::string> ceiling = {
      "heap_alloc", "file_read", "io_out", "io_err", "task", "atomic", "lock", "channel"};
  return ceiling;
}

std::string absolutePath(const std::string &path, const std::string &base) {
  const std::filesystem::path given(path);
  if (given.is_absolute() || base.empty()) {
    return given.lexically_normal().string();
  }
  std::error_code ec;
  const std::filesystem::path joined =
      std::filesystem::absolute(std::filesystem::path(base) / given, ec);
  return (ec ? std::filesystem::path(base) / given : joined).lexically_normal().string();
}

std::string absoluteDirectory(const std::string &path) {
  std::error_code ec;
  const std::filesystem::path absolute = std::filesystem::absolute(path.empty() ? "." : path, ec);
  return (ec ? std::filesystem::path(path) : absolute).lexically_normal().string();
}

class PreparedStep final : public BuildFunctionStepRunner::Prepared {
public:
  explicit PreparedStep(embed::Script compiled) : script(std::move(compiled)) {
  }
  embed::Script script;
};

// The host calls of `/std/build_step` for one step.
void bindStepHostCalls(embed::Script &script,
                       const std::string &stepName,
                       const std::string &projectDir,
                       const std::string &outDir) {
  script.bindRaw(
      "ps_build_write_output",
      {embed::HostType::Text, embed::HostType::Text},
      embed::HostType::Void,
      [stepName, outDir](const uint64_t *args, uint64_t &result, std::string &error) {
        const std::string path(embed::detail::fromSlot<embed::HostTextView>(args[0]).value);
        const std::string_view text = embed::detail::fromSlot<embed::HostTextView>(args[1]).value;
        result = 0;
        std::string resolved;
        std::string why;
        if (!resolveStepOutputPath(path, outDir, resolved, why)) {
          error = "build step " + stepName + " " + why;
          return false;
        }
        std::error_code ec;
        std::filesystem::create_directories(std::filesystem::path(resolved).parent_path(), ec);
        std::ofstream file(resolved, std::ios::binary | std::ios::trunc);
        file.write(text.data(), static_cast<std::streamsize>(text.size()));
        file.flush();
        if (!file) {
          error = "build step " + stepName + " cannot write " + path;
          return false;
        }
        return true;
      });
  script.bindRawString("ps_build_project_dir",
                       {},
                       [projectDir](const uint64_t *, std::string &result, std::string &) {
                         result = projectDir;
                         return true;
                       });
  // Both are safe to enter from several tasks of the step at once.
  script.setHostThreading("ps_build_write_output", embed::HostThreading::Reentrant);
  script.setHostThreading("ps_build_project_dir", embed::HostThreading::Reentrant);
}

class EmbeddedFunctionStepRunner final : public BuildFunctionStepRunner {
public:
  std::unique_ptr<Prepared> prepare(const BuildStep &step,
                                    const BuildRunSettings &settings,
                                    std::vector<std::string> &files,
                                    std::string &diagnostics) override {
    const std::string projectDir = absoluteDirectory(settings.projectDir);
    embed::ScriptEngine engine;
    engine.setEntryPath(step.function);
    engine.setAllowEffects(stepEffectCeiling());
    engine.addImportPath(projectDir);
    embed::Script script = engine.compileFile(absolutePath(step.source, projectDir));
    if (!script.valid()) {
      diagnostics = script.diagnostics();
      if (!diagnostics.empty() && diagnostics.back() != '\n') {
        diagnostics += '\n';
      }
      return nullptr;
    }
    files = script.sourceFiles();
    const std::string outDir = absolutePath(settings.outDir, projectDir);
    bindStepHostCalls(script, step.name, projectDir, outDir);
    return std::make_unique<PreparedStep>(std::move(script));
  }

  bool run(Prepared &prepared,
           const BuildStep &step,
           const BuildRunSettings &,
           std::string &diagnostics) override {
    const embed::Script &script = static_cast<PreparedStep &>(prepared).script;
    std::string printed;
    embed::RunOptions options;
    options.threads = true;
    options.output = [&printed](int, std::string_view chunk) { printed.append(chunk); };
    const embed::ScriptResult result = script.run({}, options);
    bool ok = result.ok && result.exitCode == 0;
    if (!result.ok) {
      diagnostics = result.diagnostics;
    } else if (result.exitCode != 0) {
      diagnostics = "build step " + step.name + ": " + step.function + " returned " +
                    std::to_string(result.exitCode);
    }
    if (!ok && !diagnostics.empty() && diagnostics.back() != '\n') {
      diagnostics += '\n';
    }
    if (!ok) {
      diagnostics += printed;
    }
    return ok;
  }
};

} // namespace

std::unique_ptr<BuildFunctionStepRunner> makeEmbeddedFunctionStepRunner() {
  return std::make_unique<EmbeddedFunctionStepRunner>();
}

bool addFunctionStepClosures(BuildFunctionStepRunner &runner,
                             const std::string &graphText,
                             const std::string &projectDir,
                             const std::string &outDir,
                             std::string &annotated,
                             std::string &diagnostics) {
  annotated = graphText;
  BuildGraphData graph;
  std::string error;
  if (!parseBuildGraphJson(graphText, graph, error)) {
    diagnostics += "primec build: " + error + "\n";
    return false;
  }
  BuildRunSettings settings;
  settings.projectDir = projectDir;
  settings.outDir = outDir;
  const std::filesystem::path root = std::filesystem::path(absoluteDirectory(projectDir));
  std::map<std::string, std::vector<std::string>> closures;
  for (const BuildStep &step : graph.steps) {
    if (step.kind != "function") {
      continue;
    }
    std::vector<std::string> files;
    std::string compileDiagnostics;
    if (runner.prepare(step, settings, files, compileDiagnostics) == nullptr) {
      diagnostics += "primec build: step " + step.name + " (" + step.id + ") does not compile\n" +
                     compileDiagnostics;
      return false;
    }
    std::vector<std::string> inside;
    for (const std::string &file : files) {
      const std::filesystem::path relative = std::filesystem::path(file).lexically_relative(root);
      if (!relative.empty() && *relative.begin() != "..") {
        inside.push_back(relative.generic_string());
      }
    }
    std::sort(inside.begin(), inside.end());
    closures[step.id] = std::move(inside);
  }
  if (closures.empty()) {
    return true;
  }
  if (!addBuildClosures(graphText, closures, annotated, error)) {
    diagnostics += "primec build: " + error + "\n";
    return false;
  }
  return true;
}

} // namespace primec
