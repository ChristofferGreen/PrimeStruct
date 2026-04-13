#include "test_compile_run_helpers.h"

#include "test_compile_run_examples_helpers.h"

TEST_SUITE_BEGIN("primestruct.compile.run.examples");

TEST_CASE("spinning cube integration artifact matrix stays valid") {
  if (!spinningCubeBackendsSupportArrayReturns()) {
    INFO("Skipping spinning cube artifact matrix checks until array-return lowering support lands");
    CHECK(true);
    return;
  }
  std::filesystem::path webSampleDir =
      std::filesystem::path("..") / "examples" / "web" / "spinning_cube";
  std::filesystem::path webSharedDir =
      std::filesystem::path("..") / "examples" / "web" / "shared";
  std::filesystem::path metalSampleDir =
      std::filesystem::path("..") / "examples" / "metal" / "spinning_cube";
  if (!std::filesystem::exists(webSampleDir)) {
    webSampleDir = std::filesystem::current_path() / "examples" / "web" / "spinning_cube";
  }
  if (!std::filesystem::exists(webSharedDir)) {
    webSharedDir = std::filesystem::current_path() / "examples" / "web" / "shared";
  }
  if (!std::filesystem::exists(metalSampleDir)) {
    metalSampleDir = std::filesystem::current_path() / "examples" / "metal" / "spinning_cube";
  }
  REQUIRE(std::filesystem::exists(webSampleDir));
  REQUIRE(std::filesystem::exists(webSharedDir));
  REQUIRE(std::filesystem::exists(metalSampleDir));

  const std::filesystem::path cubePath = webSampleDir / "cube.prime";
  const std::filesystem::path indexPath = webSampleDir / "index.html";
  const std::filesystem::path mainJsPath = webSampleDir / "main.js";
  const std::filesystem::path wgslPath = webSampleDir / "cube.wgsl";
  const std::filesystem::path sharedRuntimePath = webSharedDir / "browser_runtime_shared.js";
  const std::filesystem::path metalPath = metalSampleDir / "cube.metal";
  REQUIRE(std::filesystem::exists(cubePath));
  REQUIRE(std::filesystem::exists(indexPath));
  REQUIRE(std::filesystem::exists(mainJsPath));
  REQUIRE(std::filesystem::exists(wgslPath));
  REQUIRE(std::filesystem::exists(sharedRuntimePath));
  REQUIRE(std::filesystem::exists(metalPath));

  const std::filesystem::path outDir =
      testScratchPath("") / "primec_spinning_cube_artifact_matrix";
  std::error_code ec;
  std::filesystem::remove_all(outDir, ec);
  std::filesystem::create_directories(outDir, ec);
  REQUIRE(!ec);

  const std::filesystem::path loaderDir = outDir / "loader";
  const std::filesystem::path loaderSampleDir = loaderDir / "spinning_cube";
  const std::filesystem::path loaderSharedDir = loaderDir / "shared";
  const std::filesystem::path wasmPath = loaderSampleDir / "cube.wasm";
  const std::filesystem::path nativePath = outDir / "cube_native";
  std::filesystem::create_directories(loaderSampleDir, ec);
  REQUIRE(!ec);
  ec.clear();
  std::filesystem::create_directories(loaderSharedDir, ec);
  REQUIRE(!ec);

  const std::string compileWasmCmd =
      "./primec --emit=wasm --wasm-profile browser " + quoteShellArg(cubePath.string()) + " -o " +
      quoteShellArg(wasmPath.string()) + " --entry /main";
  CHECK(runCommand(compileWasmCmd) == 0);
  CHECK(std::filesystem::exists(wasmPath));
  CHECK(std::filesystem::file_size(wasmPath) > 8);

  const std::string compileNativeCmd =
      "./primec --emit=native " + quoteShellArg(cubePath.string()) + " -o " + quoteShellArg(nativePath.string()) +
      " --entry /mainNative";
  CHECK(runCommand(compileNativeCmd) == 0);
  CHECK(std::filesystem::exists(nativePath));
  CHECK(std::filesystem::file_size(nativePath) > 0);

  std::filesystem::copy_file(indexPath, loaderSampleDir / "index.html",
                             std::filesystem::copy_options::overwrite_existing, ec);
  CHECK(!ec);
  ec.clear();
  std::filesystem::copy_file(mainJsPath, loaderSampleDir / "main.js",
                             std::filesystem::copy_options::overwrite_existing, ec);
  CHECK(!ec);
  ec.clear();
  std::filesystem::copy_file(wgslPath, loaderSampleDir / "cube.wgsl",
                             std::filesystem::copy_options::overwrite_existing, ec);
  CHECK(!ec);
  ec.clear();
  std::filesystem::copy_file(sharedRuntimePath, loaderSharedDir / "browser_runtime_shared.js",
                             std::filesystem::copy_options::overwrite_existing, ec);
  CHECK(!ec);

  CHECK(std::filesystem::exists(loaderSampleDir / "index.html"));
  CHECK(std::filesystem::exists(loaderSampleDir / "main.js"));
  CHECK(std::filesystem::exists(loaderSampleDir / "cube.wgsl"));
  CHECK(std::filesystem::exists(loaderSampleDir / "cube.wasm"));
  CHECK(std::filesystem::exists(loaderSharedDir / "browser_runtime_shared.js"));
  CHECK(readFile((loaderSampleDir / "index.html").string()).find("cube-canvas") != std::string::npos);
  CHECK(readFile((loaderSampleDir / "main.js").string()).find("../shared/browser_runtime_shared.js") !=
        std::string::npos);
  CHECK(readFile((loaderSharedDir / "browser_runtime_shared.js").string()).find("launchBrowserRuntime") !=
        std::string::npos);
  CHECK(readFile((loaderSampleDir / "cube.wgsl").string()).find("@vertex") != std::string::npos);

  const auto readBinary = [](const std::filesystem::path &path, std::vector<unsigned char> &bytes) {
    bytes.clear();
    std::ifstream file(path, std::ios::binary);
    if (!file.good()) {
      return false;
    }
    file.seekg(0, std::ios::end);
    const std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);
    if (size <= 0) {
      return false;
    }
    bytes.resize(static_cast<size_t>(size));
    file.read(reinterpret_cast<char *>(bytes.data()), size);
    return file.good() || file.eof();
  };

  const auto fnv1a64 = [](const std::vector<unsigned char> &bytes) {
    unsigned long long hash = 1469598103934665603ull;
    for (unsigned char b : bytes) {
      hash ^= static_cast<unsigned long long>(b);
      hash *= 1099511628211ull;
    }
    return hash;
  };

  std::vector<unsigned char> wasmBytes;
  REQUIRE(readBinary(wasmPath, wasmBytes));
  REQUIRE(wasmBytes.size() >= 8);
  CHECK(wasmBytes[0] == 0x00);
  CHECK(wasmBytes[1] == 0x61);
  CHECK(wasmBytes[2] == 0x73);
  CHECK(wasmBytes[3] == 0x6d);

  std::vector<unsigned char> nativeBytes;
  REQUIRE(readBinary(nativePath, nativeBytes));

  std::vector<std::pair<std::string, std::filesystem::path>> artifacts = {
      {"cube_native", nativePath},
      {"loader/spinning_cube/index.html", loaderSampleDir / "index.html"},
      {"loader/spinning_cube/main.js", loaderSampleDir / "main.js"},
      {"loader/spinning_cube/cube.wgsl", loaderSampleDir / "cube.wgsl"},
      {"loader/spinning_cube/cube.wasm", loaderSampleDir / "cube.wasm"},
      {"loader/shared/browser_runtime_shared.js", loaderSharedDir / "browser_runtime_shared.js"},
  };

  const bool hasXcrun = runCommand("xcrun --version > /dev/null 2>&1") == 0;
  if (hasXcrun) {
    const std::filesystem::path airPath = outDir / "cube.air";
    const std::filesystem::path metallibPath = outDir / "cube.metallib";
    const std::string compileMetalCmd = "xcrun metal -std=metal3.0 -c " + quoteShellArg(metalPath.string()) + " -o " +
                                        quoteShellArg(airPath.string());
    CHECK(runCommand(compileMetalCmd) == 0);
    CHECK(std::filesystem::exists(airPath));
    const std::string compileMetallibCmd =
        "xcrun metallib " + quoteShellArg(airPath.string()) + " -o " + quoteShellArg(metallibPath.string());
    CHECK(runCommand(compileMetallibCmd) == 0);
    CHECK(std::filesystem::exists(metallibPath));
    artifacts.push_back({"cube.air", airPath});
    artifacts.push_back({"cube.metallib", metallibPath});
  }

  const std::filesystem::path manifestPath = outDir / "artifact_manifest.json";
  std::ofstream manifest(manifestPath);
  REQUIRE(manifest.good());
  manifest << "{\n";
  manifest << "  \"version\": 1,\n";
  manifest << "  \"profile\": \"spinning_cube\",\n";
  manifest << "  \"artifacts\": [\n";
  for (size_t i = 0; i < artifacts.size(); ++i) {
    const auto &entry = artifacts[i];
    std::vector<unsigned char> bytes;
    REQUIRE(readBinary(entry.second, bytes));
    const unsigned long long hash = fnv1a64(bytes);
    manifest << "    {\"name\":\"" << entry.first << "\",\"size\":" << bytes.size() << ",\"hash_fnv1a64\":\"" << hash
             << "\"}";
    if (i + 1 != artifacts.size()) {
      manifest << ",";
    }
    manifest << "\n";
  }
  manifest << "  ]\n";
  manifest << "}\n";
  manifest.close();
  CHECK(std::filesystem::exists(manifestPath));

  const std::string manifestText = readFile(manifestPath.string());
  CHECK(manifestText.find("\"version\": 1") != std::string::npos);
  CHECK(manifestText.find("\"profile\": \"spinning_cube\"") != std::string::npos);
  CHECK(manifestText.find("\"artifacts\": [") != std::string::npos);
  CHECK(manifestText.find("\"name\":\"cube_native\"") != std::string::npos);
  CHECK(manifestText.find("\"name\":\"loader/spinning_cube/index.html\"") != std::string::npos);
  CHECK(manifestText.find("\"name\":\"loader/spinning_cube/main.js\"") != std::string::npos);
  CHECK(manifestText.find("\"name\":\"loader/spinning_cube/cube.wgsl\"") != std::string::npos);
  CHECK(manifestText.find("\"name\":\"loader/spinning_cube/cube.wasm\"") != std::string::npos);
  CHECK(manifestText.find("\"name\":\"loader/shared/browser_runtime_shared.js\"") != std::string::npos);
  CHECK(manifestText.find("\"hash_fnv1a64\":\"0\"") == std::string::npos);
}

TEST_CASE("spinning cube optional startup visual smoke checks") {
  std::filesystem::path webSampleDir =
      std::filesystem::path("..") / "examples" / "web" / "spinning_cube";
  std::filesystem::path webSharedDir =
      std::filesystem::path("..") / "examples" / "web" / "shared";
  std::filesystem::path nativeSampleDir =
      std::filesystem::path("..") / "examples" / "native" / "spinning_cube";
  std::filesystem::path metalSampleDir =
      std::filesystem::path("..") / "examples" / "metal" / "spinning_cube";
  if (!std::filesystem::exists(webSampleDir)) {
    webSampleDir = std::filesystem::current_path() / "examples" / "web" / "spinning_cube";
  }
  if (!std::filesystem::exists(webSharedDir)) {
    webSharedDir = std::filesystem::current_path() / "examples" / "web" / "shared";
  }
  if (!std::filesystem::exists(nativeSampleDir)) {
    nativeSampleDir = std::filesystem::current_path() / "examples" / "native" / "spinning_cube";
  }
  if (!std::filesystem::exists(metalSampleDir)) {
    metalSampleDir = std::filesystem::current_path() / "examples" / "metal" / "spinning_cube";
  }
  REQUIRE(std::filesystem::exists(webSampleDir));
  REQUIRE(std::filesystem::exists(webSharedDir));
  REQUIRE(std::filesystem::exists(nativeSampleDir));
  REQUIRE(std::filesystem::exists(metalSampleDir));

  const std::filesystem::path cubePath = webSampleDir / "cube.prime";
  const std::filesystem::path indexPath = webSampleDir / "index.html";
  const std::filesystem::path mainJsPath = webSampleDir / "main.js";
  const std::filesystem::path wgslPath = webSampleDir / "cube.wgsl";
  const std::filesystem::path sharedRuntimePath = webSharedDir / "browser_runtime_shared.js";
  const std::filesystem::path nativeHostPath = nativeSampleDir / "main.cpp";
  const std::filesystem::path metalHostPath = metalSampleDir / "metal_host.mm";
  const std::filesystem::path metalShaderPath = metalSampleDir / "cube.metal";
  REQUIRE(std::filesystem::exists(cubePath));
  REQUIRE(std::filesystem::exists(indexPath));
  REQUIRE(std::filesystem::exists(mainJsPath));
  REQUIRE(std::filesystem::exists(wgslPath));
  REQUIRE(std::filesystem::exists(sharedRuntimePath));
  REQUIRE(std::filesystem::exists(nativeHostPath));
  REQUIRE(std::filesystem::exists(metalHostPath));
  REQUIRE(std::filesystem::exists(metalShaderPath));

  // Native visual startup proxy: host bootstrap prints a stable ready marker.
  {
    const std::filesystem::path nativeExePath =
        testScratchPath("") / "primec_spinning_cube_visual_native";
    const std::string compileNativeCmd =
        "./primec --emit=native " + quoteShellArg(cubePath.string()) + " -o " + quoteShellArg(nativeExePath.string()) +
        " --entry /mainNative";
    CHECK(runCommand(compileNativeCmd) == 0);
    CHECK(std::filesystem::exists(nativeExePath));

    std::string cxx = "";
    if (runCommand("c++ --version > /dev/null 2>&1") == 0) {
      cxx = "c++";
    } else if (runCommand("clang++ --version > /dev/null 2>&1") == 0) {
      cxx = "clang++";
    } else if (runCommand("g++ --version > /dev/null 2>&1") == 0) {
      cxx = "g++";
    } else {
      INFO("no C++ compiler found; skipping native startup visual smoke");
      cxx.clear();
    }
    if (!cxx.empty()) {
      const std::filesystem::path hostPath =
          testScratchPath("") / "primec_spinning_cube_visual_native_host";
      const std::string compileHostCmd = cxx + " -std=c++17 " + quoteShellArg(nativeHostPath.string()) + " -o " +
                                         quoteShellArg(hostPath.string());
      CHECK(runCommand(compileHostCmd) == 0);
      const std::filesystem::path hostOutPath =
          testScratchPath("") / "primec_spinning_cube_visual_native_host.out.txt";
      const std::string runHostCmd = quoteShellArg(hostPath.string()) + " " + quoteShellArg(nativeExePath.string()) +
                                     " > " + quoteShellArg(hostOutPath.string());
      CHECK(runCommand(runHostCmd) == 0);
      CHECK(readFile(hostOutPath.string()).find("native host verified cube simulation output") != std::string::npos);
    }
  }

  // Metal visual startup smoke (headless GPU where available).
  if (runCommand("xcrun --version > /dev/null 2>&1") == 0) {
    const std::filesystem::path outDir =
        testScratchPath("") / "primec_spinning_cube_visual_metal";
    std::error_code ec;
    std::filesystem::remove_all(outDir, ec);
    std::filesystem::create_directories(outDir, ec);
    REQUIRE(!ec);

    const std::filesystem::path airPath = outDir / "cube.air";
    const std::filesystem::path metallibPath = outDir / "cube.metallib";
    const std::filesystem::path hostPath = outDir / "metal_host";
    const std::filesystem::path hostOutPath = outDir / "metal_host.out.txt";

    const std::string compileMetalCmd = "xcrun metal -std=metal3.0 -c " + quoteShellArg(metalShaderPath.string()) +
                                        " -o " + quoteShellArg(airPath.string());
    CHECK(runCommand(compileMetalCmd) == 0);
    const std::string compileLibCmd =
        "xcrun metallib " + quoteShellArg(airPath.string()) + " -o " + quoteShellArg(metallibPath.string());
    CHECK(runCommand(compileLibCmd) == 0);
    const std::string compileHostCmd =
        "xcrun clang++ -std=c++17 -fobjc-arc " + quoteShellArg(metalHostPath.string()) +
        " -framework Foundation -framework Metal -o " + quoteShellArg(hostPath.string());
    CHECK(runCommand(compileHostCmd) == 0);
    const std::string runHostCmd =
        quoteShellArg(hostPath.string()) + " " + quoteShellArg(metallibPath.string()) + " > " +
        quoteShellArg(hostOutPath.string());
    CHECK(runCommand(runHostCmd) == 0);
    CHECK(readFile(hostOutPath.string()).find("frame_rendered=1") != std::string::npos);
  } else {
    INFO("xcrun unavailable; skipping metal startup visual smoke");
  }

  // Browser visual smoke (headless where possible, explicit skip otherwise).
  std::string browserCmd = "";
  if (runCommand("chromium --version > /dev/null 2>&1") == 0) {
    browserCmd = "chromium";
  } else if (runCommand("google-chrome --version > /dev/null 2>&1") == 0) {
    browserCmd = "google-chrome";
  } else if (runCommand("chrome --version > /dev/null 2>&1") == 0) {
    browserCmd = "chrome";
  } else if (runCommand("google-chrome-stable --version > /dev/null 2>&1") == 0) {
    browserCmd = "google-chrome-stable";
  }

  if (browserCmd.empty() || runCommand("python3 --version > /dev/null 2>&1") != 0) {
    INFO("no headless browser+python available; skip browser startup visual smoke");
    INFO("interactive fallback: run python3 -m http.server in examples/web and open /spinning_cube/index.html");
    return;
  }

  if (runCommand(browserCmd + " --headless --disable-gpu --dump-dom about:blank > /dev/null 2>&1") != 0) {
    INFO("browser runtime lacks headless mode; skip browser startup visual smoke");
    INFO("interactive fallback: run python3 -m http.server in examples/web and open /spinning_cube/index.html");
    return;
  }

  const std::filesystem::path browserOutDir =
      testScratchPath("") / "primec_spinning_cube_visual_browser";
  const std::filesystem::path stagedSampleDir = browserOutDir / "spinning_cube";
  const std::filesystem::path stagedSharedDir = browserOutDir / "shared";
  std::error_code ec;
  std::filesystem::remove_all(browserOutDir, ec);
  std::filesystem::create_directories(stagedSampleDir, ec);
  REQUIRE(!ec);
  ec.clear();
  std::filesystem::create_directories(stagedSharedDir, ec);
  REQUIRE(!ec);

  const std::filesystem::path wasmPath = stagedSampleDir / "cube.wasm";
  const std::string compileWasmCmd =
      "./primec --emit=wasm --wasm-profile browser " + quoteShellArg(cubePath.string()) + " -o " +
      quoteShellArg(wasmPath.string()) + " --entry /main";
  CHECK(runCommand(compileWasmCmd) == 0);

  std::filesystem::copy_file(indexPath, stagedSampleDir / "index.html",
                             std::filesystem::copy_options::overwrite_existing, ec);
  CHECK(!ec);
  ec.clear();
  std::filesystem::copy_file(mainJsPath, stagedSampleDir / "main.js",
                             std::filesystem::copy_options::overwrite_existing, ec);
  CHECK(!ec);
  ec.clear();
  std::filesystem::copy_file(wgslPath, stagedSampleDir / "cube.wgsl",
                             std::filesystem::copy_options::overwrite_existing, ec);
  CHECK(!ec);
  ec.clear();
  std::filesystem::copy_file(sharedRuntimePath, stagedSharedDir / "browser_runtime_shared.js",
                             std::filesystem::copy_options::overwrite_existing, ec);
  CHECK(!ec);

  const std::filesystem::path serverLogPath = browserOutDir / "server.log";
  const std::filesystem::path serverPidPath = browserOutDir / "server.pid";
  const std::filesystem::path browserDomPath = browserOutDir / "browser.dom.txt";
  const std::filesystem::path browserErrPath = browserOutDir / "browser.err.txt";

  const int port = 18765;
  const std::string startServerCmd = "python3 -m http.server " + std::to_string(port) + " --bind 127.0.0.1 --directory " +
                                     quoteShellArg(browserOutDir.string()) + " > " + quoteShellArg(serverLogPath.string()) +
                                     " 2>&1 & echo $! > " + quoteShellArg(serverPidPath.string());
  CHECK(runCommand(startServerCmd) == 0);
  CHECK(runCommand("sleep 1") == 0);

  const std::string runBrowserCmd = browserCmd + " --headless --disable-gpu --virtual-time-budget=5000 --dump-dom " +
                                    "http://127.0.0.1:" + std::to_string(port) + "/spinning_cube/index.html > " +
                                    quoteShellArg(browserDomPath.string()) + " 2> " + quoteShellArg(browserErrPath.string());
  const int browserCode = runCommand(runBrowserCmd);

  const std::string stopServerCmd = "kill $(cat " + quoteShellArg(serverPidPath.string()) + ") > /dev/null 2>&1";
  runCommand(stopServerCmd);

  CHECK(browserCode == 0);
  const std::string dom = readFile(browserDomPath.string());
  CHECK(dom.find("PrimeStruct Spinning Cube Host") != std::string::npos);
  const bool hasHostRunning = dom.find("Host running") != std::string::npos;
  const bool hasWasmLoadSkipped = dom.find("Wasm load skipped") != std::string::npos;
  const bool hasExpectedStatus = hasHostRunning || hasWasmLoadSkipped;
  CHECK(hasExpectedStatus);
}

TEST_CASE("spinning cube browser startup smoke proves wasm bootstrap") {
  std::filesystem::path sampleDir =
      std::filesystem::path("..") / "examples" / "web" / "spinning_cube";
  std::filesystem::path sharedDir =
      std::filesystem::path("..") / "examples" / "web" / "shared";
  if (!std::filesystem::exists(sampleDir)) {
    sampleDir = std::filesystem::current_path() / "examples" / "web" / "spinning_cube";
  }
  if (!std::filesystem::exists(sharedDir)) {
    sharedDir = std::filesystem::current_path() / "examples" / "web" / "shared";
  }
  REQUIRE(std::filesystem::exists(sampleDir));
  REQUIRE(std::filesystem::exists(sharedDir));

  const std::filesystem::path cubePath = sampleDir / "cube.prime";
  const std::filesystem::path indexPath = sampleDir / "index.html";
  const std::filesystem::path mainJsPath = sampleDir / "main.js";
  const std::filesystem::path wgslPath = sampleDir / "cube.wgsl";
  const std::filesystem::path sharedRuntimePath = sharedDir / "browser_runtime_shared.js";
  REQUIRE(std::filesystem::exists(cubePath));
  REQUIRE(std::filesystem::exists(indexPath));
  REQUIRE(std::filesystem::exists(mainJsPath));
  REQUIRE(std::filesystem::exists(wgslPath));
  REQUIRE(std::filesystem::exists(sharedRuntimePath));

  std::string browserCmd = "";
  if (runCommand("chromium --version > /dev/null 2>&1") == 0) {
    browserCmd = "chromium";
  } else if (runCommand("google-chrome --version > /dev/null 2>&1") == 0) {
    browserCmd = "google-chrome";
  } else if (runCommand("chrome --version > /dev/null 2>&1") == 0) {
    browserCmd = "chrome";
  } else if (runCommand("google-chrome-stable --version > /dev/null 2>&1") == 0) {
    browserCmd = "google-chrome-stable";
  }

  if (browserCmd.empty() || runCommand("python3 --version > /dev/null 2>&1") != 0) {
    INFO("no headless browser+python available; skip spinning cube browser startup smoke");
    return;
  }
  if (runCommand(browserCmd + " --headless --disable-gpu --dump-dom about:blank > /dev/null 2>&1") != 0) {
    INFO("browser runtime lacks headless mode; skip spinning cube browser startup smoke");
    return;
  }

  const std::filesystem::path outDir =
      testScratchPath("") / "primec_spinning_cube_browser_startup_smoke";
  const std::filesystem::path stagedSampleDir = outDir / "spinning_cube";
  const std::filesystem::path stagedSharedDir = outDir / "shared";
  std::error_code ec;
  std::filesystem::remove_all(outDir, ec);
  std::filesystem::create_directories(stagedSampleDir, ec);
  REQUIRE(!ec);
  ec.clear();
  std::filesystem::create_directories(stagedSharedDir, ec);
  REQUIRE(!ec);

  const std::filesystem::path wasmPath = stagedSampleDir / "cube.wasm";
  const std::string compileWasmCmd =
      "./primec --emit=wasm --wasm-profile browser " + quoteShellArg(cubePath.string()) + " -o " +
      quoteShellArg(wasmPath.string()) + " --entry /main";
  CHECK(runCommand(compileWasmCmd) == 0);
  CHECK(std::filesystem::exists(wasmPath));

  std::filesystem::copy_file(indexPath, stagedSampleDir / "index.html", std::filesystem::copy_options::overwrite_existing,
                             ec);
  CHECK(!ec);
  ec.clear();
  std::filesystem::copy_file(mainJsPath, stagedSampleDir / "main.js", std::filesystem::copy_options::overwrite_existing,
                             ec);
  CHECK(!ec);
  ec.clear();
  std::filesystem::copy_file(wgslPath, stagedSampleDir / "cube.wgsl", std::filesystem::copy_options::overwrite_existing,
                             ec);
  CHECK(!ec);
  ec.clear();
  std::filesystem::copy_file(sharedRuntimePath, stagedSharedDir / "browser_runtime_shared.js",
                             std::filesystem::copy_options::overwrite_existing, ec);
  CHECK(!ec);

  const std::filesystem::path serverLogPath = outDir / "server.log";
  const std::filesystem::path serverPidPath = outDir / "server.pid";
  const std::filesystem::path browserDomPath = outDir / "browser.dom.txt";
  const std::filesystem::path browserErrPath = outDir / "browser.err.txt";

  const int port = 18767;
  const std::string startServerCmd = "python3 -m http.server " + std::to_string(port) + " --bind 127.0.0.1 --directory " +
                                     quoteShellArg(outDir.string()) + " > " + quoteShellArg(serverLogPath.string()) +
                                     " 2>&1 & echo $! > " + quoteShellArg(serverPidPath.string());
  CHECK(runCommand(startServerCmd) == 0);
  CHECK(runCommand("sleep 1") == 0);

  const std::string runBrowserCmd = browserCmd + " --headless --disable-gpu --virtual-time-budget=6000 --dump-dom " +
                                    "http://127.0.0.1:" + std::to_string(port) + "/spinning_cube/index.html > " +
                                    quoteShellArg(browserDomPath.string()) + " 2> " + quoteShellArg(browserErrPath.string());
  const int browserCode = runCommand(runBrowserCmd);
  runCommand("kill $(cat " + quoteShellArg(serverPidPath.string()) + ") > /dev/null 2>&1");

  CHECK(browserCode == 0);
  const std::string dom = readFile(browserDomPath.string());
  CHECK(dom.find("Host running with cube.wasm and cube.wgsl bootstrap.") != std::string::npos);
  CHECK(dom.find("Wasm load skipped") == std::string::npos);
}


TEST_SUITE_END();
