// Finds and loads the script runner library (include/primec/ui/ScriptRunner.h).

#include "primec/ui/ScriptRunner.h"

#include <cstdlib>
#include <dlfcn.h>
#include <mutex>
#include <string>

namespace primec::ui {

namespace {

struct Loaded {
  ScriptRunnerApi api;
  bool ok = false;
  std::string error;
};

std::string directoryOf(const std::string &path) {
  const size_t slash = path.rfind('/');
  return slash == std::string::npos ? std::string(".") : path.substr(0, slash);
}

void *tryOpen(const std::string &path, std::string &error) {
  void *handle = dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL);
  if (handle == nullptr) {
    const char *reason = dlerror();
    error += (error.empty() ? "" : "; ") + path + ": " + (reason != nullptr ? reason : "cannot be loaded");
  }
  return handle;
}

Loaded load() {
  Loaded out;
  void *handle = nullptr;
  if (const char *fromEnv = std::getenv("PRIMESTRUCT_SCRIPT_RUNNER"); fromEnv != nullptr && *fromEnv != '\0') {
    handle = tryOpen(fromEnv, out.error);
  }
  if (handle == nullptr) {
    Dl_info info{};
    if (dladdr(reinterpret_cast<void *>(&scriptRunnerApi), &info) != 0 && info.dli_fname != nullptr) {
      handle = tryOpen(directoryOf(info.dli_fname) + "/libps_script.dylib", out.error);
    }
  }
#ifdef PRIMESTRUCT_SCRIPT_RUNNER_BUILD_PATH
  if (handle == nullptr) {
    handle = tryOpen(PRIMESTRUCT_SCRIPT_RUNNER_BUILD_PATH, out.error);
  }
#endif
  if (handle == nullptr) {
    return out;
  }
  out.api.start = reinterpret_cast<decltype(out.api.start)>(dlsym(handle, "ps_script_start"));
  out.api.stop = reinterpret_cast<decltype(out.api.stop)>(dlsym(handle, "ps_script_stop"));
  out.api.release = reinterpret_cast<decltype(out.api.release)>(dlsym(handle, "ps_script_release"));
  out.ok = out.api.start != nullptr && out.api.stop != nullptr && out.api.release != nullptr;
  if (!out.ok) {
    out.error = "the script runner library lacks its functions";
  }
  return out;
}

const Loaded &loaded() {
  static std::once_flag once;
  static Loaded value;
  std::call_once(once, [] { value = load(); });
  return value;
}

} // namespace

namespace {
const ScriptRunnerApi *installed = nullptr;

bool disabled = false;
}

void setScriptRunnerApi(const ScriptRunnerApi *api) {
  installed = api;
  disabled = false;
}

void disableScriptRunner() {
  installed = nullptr;
  disabled = true;
}

const ScriptRunnerApi *scriptRunnerApi() {
  if (installed != nullptr) {
    return installed;
  }
  if (disabled) {
    return nullptr;
  }
  const Loaded &l = loaded();
  return l.ok ? &l.api : nullptr;
}

const char *scriptRunnerError() {
  if (disabled) {
    return "this program has no interpreter";
  }
  return installed != nullptr ? "" : loaded().error.c_str();
}

} // namespace primec::ui
