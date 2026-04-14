#include "test_compile_run_helpers.h"

#include "test_compile_run_examples_helpers.h"

TEST_SUITE_BEGIN("primestruct.compile.run.examples");

TEST_CASE("spinning cube metal host missing metallib diagnostics stay stable") {
  std::filesystem::path metalSampleDir =
      std::filesystem::path("..") / "examples" / "metal" / "spinning_cube";
  if (!std::filesystem::exists(metalSampleDir)) {
    metalSampleDir = std::filesystem::current_path() / "examples" / "metal" / "spinning_cube";
  }
  REQUIRE(std::filesystem::exists(metalSampleDir));

  const std::filesystem::path metalHostPath = metalSampleDir / "metal_host.mm";
  REQUIRE(std::filesystem::exists(metalHostPath));

  if (runCommand("xcrun --version > /dev/null 2>&1") != 0) {
    INFO("SKIP: xcrun unavailable; cannot run metallib diagnostic regression");
    return;
  }

  const std::filesystem::path outDir =
      testScratchPath("") / "primec_spinning_cube_metal_missing_metallib";
  std::error_code ec;
  std::filesystem::remove_all(outDir, ec);
  std::filesystem::create_directories(outDir, ec);
  REQUIRE(!ec);

  const std::filesystem::path hostBinaryPath = outDir / "metal_host";
  const std::filesystem::path hostStdoutPath = outDir / "metal_host.stdout.txt";
  const std::filesystem::path hostStderrPath = outDir / "metal_host.stderr.txt";
  const std::filesystem::path missingMetallibPath = outDir / "missing.metallib";

  const std::string compileHostCmd =
      "xcrun clang++ -std=c++17 -fobjc-arc " + quoteShellArg(metalHostPath.string()) +
      " -framework Foundation -framework Metal -o " + quoteShellArg(hostBinaryPath.string());
  CHECK(runCommand(compileHostCmd) == 0);
  CHECK(std::filesystem::exists(hostBinaryPath));

  const std::string runHostCmd =
      quoteShellArg(hostBinaryPath.string()) + " " + quoteShellArg(missingMetallibPath.string()) + " > " +
      quoteShellArg(hostStdoutPath.string()) + " 2> " + quoteShellArg(hostStderrPath.string());
  CHECK(runCommand(runHostCmd) == 71);

  const std::string hostStdout = readFile(hostStdoutPath.string());
  const std::string hostStderr = readFile(hostStderrPath.string());
  CHECK(hostStdout.empty());
  CHECK(hostStderr.find("gfx_profile=metal-osx") != std::string::npos);
  CHECK(hostStderr.find("gfx_error_code=pipeline_create_failed") != std::string::npos);
  CHECK(hostStderr.find("gfx_error_why=failed to load metallib:") != std::string::npos);
}

TEST_CASE("spinning cube metal host pipeline creation regression stays fixed") {
  std::filesystem::path metalSampleDir =
      std::filesystem::path("..") / "examples" / "metal" / "spinning_cube";
  if (!std::filesystem::exists(metalSampleDir)) {
    metalSampleDir = std::filesystem::current_path() / "examples" / "metal" / "spinning_cube";
  }
  REQUIRE(std::filesystem::exists(metalSampleDir));

  const std::filesystem::path metalShaderPath = metalSampleDir / "cube.metal";
  const std::filesystem::path metalHostPath = metalSampleDir / "metal_host.mm";
  REQUIRE(std::filesystem::exists(metalShaderPath));
  REQUIRE(std::filesystem::exists(metalHostPath));

  if (runCommand("xcrun --version > /dev/null 2>&1") != 0) {
    INFO("SKIP: xcrun unavailable; cannot run metal pipeline creation regression");
    return;
  }
  if (runCommand("xcrun --find metal > /dev/null 2>&1") != 0) {
    INFO("SKIP: xcrun metal unavailable; cannot run metal pipeline creation regression");
    return;
  }
  if (runCommand("xcrun --find metallib > /dev/null 2>&1") != 0) {
    INFO("SKIP: xcrun metallib unavailable; cannot run metal pipeline creation regression");
    return;
  }

  const std::filesystem::path outDir =
      testScratchPath("") / "primec_spinning_cube_metal_pipeline_regression";
  std::error_code ec;
  std::filesystem::remove_all(outDir, ec);
  std::filesystem::create_directories(outDir, ec);
  REQUIRE(!ec);

  const std::filesystem::path airPath = outDir / "cube.air";
  const std::filesystem::path metallibPath = outDir / "cube.metallib";
  const std::filesystem::path hostBinaryPath = outDir / "metal_host";
  const std::filesystem::path hostStdoutPath = outDir / "metal_host.stdout.txt";
  const std::filesystem::path hostStderrPath = outDir / "metal_host.stderr.txt";

  const std::string compileMetalCmd = "xcrun metal -std=metal3.0 -c " + quoteShellArg(metalShaderPath.string()) +
                                      " -o " + quoteShellArg(airPath.string());
  CHECK(runCommand(compileMetalCmd) == 0);
  CHECK(std::filesystem::exists(airPath));

  const std::string compileLibraryCmd =
      "xcrun metallib " + quoteShellArg(airPath.string()) + " -o " + quoteShellArg(metallibPath.string());
  CHECK(runCommand(compileLibraryCmd) == 0);
  CHECK(std::filesystem::exists(metallibPath));

  const std::string compileHostCmd =
      "xcrun clang++ -std=c++17 -fobjc-arc " + quoteShellArg(metalHostPath.string()) +
      " -framework Foundation -framework Metal -o " + quoteShellArg(hostBinaryPath.string());
  CHECK(runCommand(compileHostCmd) == 0);
  CHECK(std::filesystem::exists(hostBinaryPath));

  const std::string runHostCmd = quoteShellArg(hostBinaryPath.string()) + " " + quoteShellArg(metallibPath.string()) +
                                 " > " + quoteShellArg(hostStdoutPath.string()) + " 2> " +
                                 quoteShellArg(hostStderrPath.string());
  CHECK(runCommand(runHostCmd) == 0);

  const std::string hostStdout = readFile(hostStdoutPath.string());
  const std::string hostStderr = readFile(hostStderrPath.string());
  CHECK(hostStdout.find("gfx_profile=metal-osx") != std::string::npos);
  CHECK(hostStdout.find("frame_rendered=1") != std::string::npos);
  CHECK(hostStderr.find("failed to create render pipeline:") == std::string::npos);
  CHECK(hostStderr.find("Vertex function has input attributes but no vertex descriptor was set.") == std::string::npos);
}

TEST_CASE("spinning cube metal full-path smoke renders frame") {
  std::filesystem::path metalSampleDir =
      std::filesystem::path("..") / "examples" / "metal" / "spinning_cube";
  if (!std::filesystem::exists(metalSampleDir)) {
    metalSampleDir = std::filesystem::current_path() / "examples" / "metal" / "spinning_cube";
  }
  REQUIRE(std::filesystem::exists(metalSampleDir));

  const std::filesystem::path metalShaderPath = metalSampleDir / "cube.metal";
  const std::filesystem::path metalHostPath = metalSampleDir / "metal_host.mm";
  REQUIRE(std::filesystem::exists(metalShaderPath));
  REQUIRE(std::filesystem::exists(metalHostPath));

  if (runCommand("xcrun --version > /dev/null 2>&1") != 0) {
    INFO("SKIP: xcrun unavailable; cannot run spinning cube metal full-path smoke");
    return;
  }
  if (runCommand("xcrun --find metal > /dev/null 2>&1") != 0) {
    INFO("SKIP: xcrun metal unavailable; skipping spinning cube metal full-path smoke");
    return;
  }
  if (runCommand("xcrun --find metallib > /dev/null 2>&1") != 0) {
    INFO("SKIP: xcrun metallib unavailable; skipping spinning cube metal full-path smoke");
    return;
  }

  const std::filesystem::path outDir =
      testScratchPath("") / "primec_spinning_cube_metal_host_runtime";
  std::error_code ec;
  std::filesystem::remove_all(outDir, ec);
  std::filesystem::create_directories(outDir, ec);
  REQUIRE(!ec);

  const std::filesystem::path airPath = outDir / "cube.air";
  const std::filesystem::path metallibPath = outDir / "cube.metallib";
  const std::filesystem::path hostBinaryPath = outDir / "metal_host";
  const std::filesystem::path hostStdoutPath = outDir / "metal_host.stdout.txt";
  const std::filesystem::path hostStderrPath = outDir / "metal_host.stderr.txt";

  const std::string compileMetalCmd = "xcrun metal -std=metal3.0 -c " + quoteShellArg(metalShaderPath.string()) +
                                      " -o " + quoteShellArg(airPath.string());
  CHECK(runCommand(compileMetalCmd) == 0);
  CHECK(std::filesystem::exists(airPath));

  const std::string compileLibraryCmd =
      "xcrun metallib " + quoteShellArg(airPath.string()) + " -o " + quoteShellArg(metallibPath.string());
  CHECK(runCommand(compileLibraryCmd) == 0);
  CHECK(std::filesystem::exists(metallibPath));

  const std::string compileHostCmd =
      "xcrun clang++ -std=c++17 -fobjc-arc " + quoteShellArg(metalHostPath.string()) +
      " -framework Foundation -framework Metal -o " + quoteShellArg(hostBinaryPath.string());
  CHECK(runCommand(compileHostCmd) == 0);
  CHECK(std::filesystem::exists(hostBinaryPath));

  const std::string runHostCmd = quoteShellArg(hostBinaryPath.string()) + " " + quoteShellArg(metallibPath.string()) +
                                 " > " + quoteShellArg(hostStdoutPath.string()) + " 2> " +
                                 quoteShellArg(hostStderrPath.string());
  CHECK(runCommand(runHostCmd) == 0);
  const std::string hostStdout = readFile(hostStdoutPath.string());
  CHECK(hostStdout.find("gfx_profile=metal-osx") != std::string::npos);
  CHECK(hostStdout.find("frame_rendered=1") != std::string::npos);
}

TEST_CASE("spinning cube metal software surface bridge smoke") {
  std::filesystem::path metalSampleDir =
      std::filesystem::path("..") / "examples" / "metal" / "spinning_cube";
  if (!std::filesystem::exists(metalSampleDir)) {
    metalSampleDir = std::filesystem::current_path() / "examples" / "metal" / "spinning_cube";
  }
  REQUIRE(std::filesystem::exists(metalSampleDir));

  const std::filesystem::path metalHostPath = metalSampleDir / "metal_host.mm";
  REQUIRE(std::filesystem::exists(metalHostPath));

  if (runCommand("xcrun --version > /dev/null 2>&1") != 0) {
    INFO("SKIP: xcrun unavailable; cannot run metal software surface bridge smoke");
    return;
  }

  const std::filesystem::path outDir =
      testScratchPath("") / "primec_spinning_cube_metal_software_surface";
  std::error_code ec;
  std::filesystem::remove_all(outDir, ec);
  std::filesystem::create_directories(outDir, ec);
  REQUIRE(!ec);

  const std::filesystem::path hostBinaryPath = outDir / "metal_host";
  const std::filesystem::path hostStdoutPath = outDir / "metal_host.stdout.txt";
  const std::filesystem::path hostStderrPath = outDir / "metal_host.stderr.txt";

  const std::string compileHostCmd =
      "xcrun clang++ -std=c++17 -fobjc-arc " + quoteShellArg(metalHostPath.string()) +
      " -framework Foundation -framework Metal -o " + quoteShellArg(hostBinaryPath.string());
  CHECK(runCommand(compileHostCmd) == 0);
  CHECK(std::filesystem::exists(hostBinaryPath));

  const std::string runHostCmd =
      quoteShellArg(hostBinaryPath.string()) + " --software-surface-demo > " +
      quoteShellArg(hostStdoutPath.string()) + " 2> " + quoteShellArg(hostStderrPath.string());
  CHECK(runCommand(runHostCmd) == 0);
  CHECK(readFile(hostStderrPath.string()).empty());
  const std::string hostStdout = readFile(hostStdoutPath.string());
  CHECK(hostStdout.find("gfx_profile=metal-osx") != std::string::npos);
  CHECK(hostStdout.find("software_surface_bridge=1") != std::string::npos);
  CHECK(hostStdout.find("software_surface_width=64") != std::string::npos);
  CHECK(hostStdout.find("software_surface_height=64") != std::string::npos);
  CHECK(hostStdout.find("software_surface_presented=1") != std::string::npos);
  CHECK(hostStdout.find("frame_rendered=1") != std::string::npos);
}

TEST_CASE("borrow checker negative examples fail with expected diagnostics" * doctest::skip(true)) {
  const std::filesystem::path examplesDir = std::filesystem::path("..") / "examples" / "borrow_checker_negative";
  REQUIRE(std::filesystem::exists(examplesDir));

  std::vector<std::filesystem::path> exampleFiles;
  for (const auto &entry : std::filesystem::directory_iterator(examplesDir)) {
    if (!entry.is_regular_file()) {
      continue;
    }
    if (entry.path().extension() != ".prime") {
      continue;
    }
    exampleFiles.push_back(entry.path());
  }
  std::sort(exampleFiles.begin(), exampleFiles.end());
  REQUIRE(!exampleFiles.empty());

  for (const auto &path : exampleFiles) {
    std::filesystem::path expectedPath = path;
    expectedPath.replace_extension(".expected.txt");
    REQUIRE(std::filesystem::exists(expectedPath));

    const std::string expectedContents = readFile(expectedPath.string());
    std::istringstream expectedLines(expectedContents);
    std::vector<std::string> expectedFragments;
    std::string line;
    while (std::getline(expectedLines, line)) {
      if (line.empty() || line[0] == '#') {
        continue;
      }
      expectedFragments.push_back(line);
    }
    REQUIRE(!expectedFragments.empty());

    const std::string primecErrPath =
        (testScratchPath("") / ("primec_borrow_checker_negative_" + path.stem().string() + ".json"))
            .string();
    const std::string primevmErrPath = (testScratchPath("") /
                                        ("primevm_borrow_checker_negative_" + path.stem().string() + ".json"))
                                           .string();

    const std::string primecCmd = "./primec --emit=exe " + quoteShellArg(path.string()) +
                                  " -o /dev/null --entry /main --emit-diagnostics 2> " + quoteShellArg(primecErrPath);
    CHECK(runCommand(primecCmd) == 2);
    const std::string primecDiagnostics = readFile(primecErrPath);
    CHECK(primecDiagnostics.find("\"version\":1") != std::string::npos);
    CHECK(primecDiagnostics.find("\"severity\":\"error\"") != std::string::npos);
    for (const auto &fragment : expectedFragments) {
      CHECK(primecDiagnostics.find(fragment) != std::string::npos);
    }
    CHECK(primecDiagnostics.find("Semantic error: ") == std::string::npos);

    const std::string primevmCmd =
        "./primevm " + quoteShellArg(path.string()) + " --entry /main --emit-diagnostics 2> " + quoteShellArg(primevmErrPath);
    CHECK(runCommand(primevmCmd) == 2);
    const std::string primevmDiagnostics = readFile(primevmErrPath);
    CHECK(primevmDiagnostics.find("\"version\":1") != std::string::npos);
    CHECK(primevmDiagnostics.find("\"severity\":\"error\"") != std::string::npos);
    for (const auto &fragment : expectedFragments) {
      CHECK(primevmDiagnostics.find(fragment) != std::string::npos);
    }
    CHECK(primevmDiagnostics.find("Semantic error: ") == std::string::npos);
  }
}


TEST_SUITE_END();
