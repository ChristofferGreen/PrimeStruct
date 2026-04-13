#include "test_compile_run_helpers.h"
#include "test_compile_run_examples_helpers.h"

static std::string spinningCubeBackendProbeCacheSignature(const std::filesystem::path &cubePath) {
  std::string signature = cubePath.lexically_normal().string();
  auto appendPathStamp = [&](const std::filesystem::path &path) {
    signature += "\n";
    signature += path.lexically_normal().string();
    std::error_code ec;
    const auto size = std::filesystem::file_size(path, ec);
    signature += "\nsize=";
    signature += ec ? "missing" : std::to_string(size);
    ec.clear();
    const auto lastWrite = std::filesystem::last_write_time(path, ec);
    signature += "\nmtime=";
    signature += ec ? "missing" : std::to_string(lastWrite.time_since_epoch().count());
  };

  appendPathStamp(cubePath);
  appendPathStamp(std::filesystem::current_path() / "primec");
  return signature;
}

static bool readSpinningCubeBackendProbeCache(const std::filesystem::path &cachePath,
                                              const std::string &signature,
                                              int &cachedValue) {
  std::ifstream cache(cachePath);
  if (!cache.good()) {
    return false;
  }

  std::string resultLine;
  if (!std::getline(cache, resultLine)) {
    return false;
  }

  std::stringstream buffer;
  buffer << cache.rdbuf();
  if (buffer.str() != signature) {
    return false;
  }

  if (resultLine == "1") {
    cachedValue = 1;
    return true;
  }
  if (resultLine == "0") {
    cachedValue = 0;
    return true;
  }
  return false;
}

static void writeSpinningCubeBackendProbeCache(const std::filesystem::path &cachePath,
                                               const std::string &signature,
                                               int cachedValue) {
  std::error_code ec;
  std::filesystem::create_directories(cachePath.parent_path(), ec);
  if (ec) {
    return;
  }

  const std::filesystem::path tmpPath = cachePath.string() + ".tmp";
  {
    std::ofstream cache(tmpPath);
    if (!cache.good()) {
      return;
    }
    cache << cachedValue << "\n" << signature;
    if (!cache.good()) {
      return;
    }
  }

  std::filesystem::rename(tmpPath, cachePath, ec);
  if (ec) {
    ec.clear();
    std::filesystem::remove(tmpPath, ec);
  }
}

bool spinningCubeBackendsSupportArrayReturns() {
  static int cached = -1;
  if (cached != -1) {
    return cached == 1;
  }

  std::filesystem::path cubePath = std::filesystem::path("..") / "examples" / "web" / "spinning_cube" / "cube.prime";
  if (!std::filesystem::exists(cubePath)) {
    cubePath = std::filesystem::current_path() / "examples" / "web" / "spinning_cube" / "cube.prime";
  }
  if (!std::filesystem::exists(cubePath)) {
    cached = 1;
    return true;
  }

  const std::filesystem::path cachePath = std::filesystem::current_path() / ".primec_test_cache" /
                                          "spinning_cube_backend_probe.txt";
  const std::string cacheSignature = spinningCubeBackendProbeCacheSignature(cubePath);
  if (readSpinningCubeBackendProbeCache(cachePath, cacheSignature, cached)) {
    return cached == 1;
  }

  auto storeResult = [&](int value) {
    cached = value;
    writeSpinningCubeBackendProbeCache(cachePath, cacheSignature, value);
    return value == 1;
  };

  const std::string errPath =
      (testScratchPath("") / "primec_spinning_cube_backend_probe.err.txt").string();
  const std::string cmd =
      "./primec --emit=ir " + quoteShellArg(cubePath.string()) + " --entry /main > " + quoteShellArg(errPath) +
      " 2>&1";
  const int code = runCommand(cmd);
  const std::string errorText = readFile(errPath);
  if (code != 0 && errorText.find("only supports returning array values") != std::string::npos) {
    return storeResult(0);
  }

  const std::string wasmProbePath =
      (testScratchPath("") / "primec_spinning_cube_backend_probe.wasm").string();
  const std::string wasmErrPath =
      (testScratchPath("") / "primec_spinning_cube_backend_probe.wasm.err.txt").string();
  const std::string wasmCmd =
      "./primec --emit=wasm --wasm-profile browser " + quoteShellArg(cubePath.string()) + " -o " +
      quoteShellArg(wasmProbePath) + " --entry /main > " + quoteShellArg(wasmErrPath) + " 2>&1";
  const int wasmCode = runCommand(wasmCmd);
  const std::string wasmErrorText = readFile(wasmErrPath);
  if (wasmCode != 0 &&
      (wasmErrorText.find("unsupported effect mask bits for wasm-browser target") != std::string::npos ||
       wasmErrorText.find("graphics stdlib runtime substrate unavailable for wasm-browser target: /std/gfx/*") !=
           std::string::npos)) {
    return storeResult(0);
  }

  return storeResult(1);
}

std::vector<std::filesystem::path> collectExamplePrimeFiles(const std::filesystem::path &examplesDir) {
  std::vector<std::filesystem::path> exampleFiles;
  for (const auto &entry : std::filesystem::recursive_directory_iterator(examplesDir)) {
    if (!entry.is_regular_file()) {
      continue;
    }
    const std::filesystem::path path = entry.path();
    if (path.extension() != ".prime") {
      continue;
    }
    if (path.string().find("borrow_checker_negative") != std::string::npos) {
      continue;
    }
    if (path.filename() == "result_helpers.prime") {
      continue;
    }
    if (path.filename() == "soa_vector_ecs_draft.prime") {
      continue;
    }
    exampleFiles.push_back(path);
  }
  std::sort(exampleFiles.begin(), exampleFiles.end());
  return exampleFiles;
}

void compileExampleIrBatch(const std::filesystem::path &examplesDir,
                           const std::vector<std::filesystem::path> &exampleFiles,
                           const std::string &outDirName,
                           const std::vector<std::string> &prefixes,
                           bool supportsSpinningCube) {
  const std::filesystem::path outDir = testScratchPath("") / outDirName;
  std::error_code ec;
  std::filesystem::remove_all(outDir, ec);
  std::filesystem::create_directories(outDir, ec);
  REQUIRE(!ec);

  bool sawPrefixMatch = false;
  bool compiledMatch = false;
  for (const auto &path : exampleFiles) {
    const std::filesystem::path relativePath = std::filesystem::relative(path, examplesDir);
    const std::string relativeText = relativePath.generic_string();
    bool matchesPrefix = false;
    for (const std::string &prefix : prefixes) {
      if (relativeText.rfind(prefix, 0) == 0) {
        matchesPrefix = true;
        break;
      }
    }
    if (!matchesPrefix) {
      continue;
    }
    sawPrefixMatch = true;
    if (!supportsSpinningCube && path.string().find("spinning_cube") != std::string::npos) {
      continue;
    }
    compiledMatch = true;
    const std::string compileCmd =
        "./primec --emit=ir " + quoteShellArg(path.string()) + " --out-dir " + quoteShellArg(outDir.string()) +
        " --entry /main";
    CHECK(runCommand(compileCmd) == 0);
    CHECK(std::filesystem::exists(outDir / (path.stem().string() + ".psir")));
  }
  CHECK(sawPrefixMatch);
  if (!compiledMatch) {
    INFO("All matching example files were gated by the spinning cube backend capability check");
  }
}
