#include "test_compile_run_helpers.h"

#include "test_compile_run_examples_helpers.h"

TEST_SUITE_BEGIN("primestruct.compile.run.examples");

TEST_CASE("spinning cube demo script emits deterministic summary") {
  if (!spinningCubeBackendsSupportArrayReturns()) {
    INFO("Skipping spinning cube demo script summary until browser/native gfx backends are available");
    CHECK(true);
    return;
  }
  std::filesystem::path scriptPath =
      std::filesystem::path("..") / "scripts" / "run_spinning_cube_demo.sh";
  if (!std::filesystem::exists(scriptPath)) {
    scriptPath = std::filesystem::current_path() / "scripts" / "run_spinning_cube_demo.sh";
  }
  REQUIRE(std::filesystem::exists(scriptPath));

  const std::filesystem::path workDir =
      testScratchPath("") / "primec_spinning_cube_demo_script";
  std::error_code ec;
  std::filesystem::remove_all(workDir, ec);
  std::filesystem::create_directories(workDir, ec);
  REQUIRE(!ec);

  const std::filesystem::path outPath = workDir / "script.out.txt";
  const std::filesystem::path errPath = workDir / "script.err.txt";
  const std::string command = quoteShellArg(scriptPath.string()) + " --primec ./primec --work-dir " +
                              quoteShellArg(workDir.string()) + " > " + quoteShellArg(outPath.string()) + " 2> " +
                              quoteShellArg(errPath.string());
  CHECK(runCommand(command) == 0);

  const std::string output = readFile(outPath.string());
  const std::string diagnostics = readFile(errPath.string());
  CHECK(diagnostics.empty());
  CHECK(output.find("[spinning-cube-demo] SUMMARY") != std::string::npos);

  const bool webStatusPresent = output.find("[spinning-cube-demo] WEB: PASS (") != std::string::npos ||
                                output.find("[spinning-cube-demo] WEB: SKIP (") != std::string::npos;
  const bool nativeStatusPresent = output.find("[spinning-cube-demo] NATIVE: PASS (") != std::string::npos ||
                                   output.find("[spinning-cube-demo] NATIVE: SKIP (") != std::string::npos;
  const bool metalStatusPresent = output.find("[spinning-cube-demo] METAL: PASS (") != std::string::npos ||
                                  output.find("[spinning-cube-demo] METAL: SKIP (") != std::string::npos;
  CHECK(webStatusPresent);
  CHECK(nativeStatusPresent);
  CHECK(metalStatusPresent);
  CHECK(output.find("[spinning-cube-demo] RESULT: PASS") != std::string::npos);
}

TEST_CASE("spinning cube demo script skips known native backend limitation") {
  if (!spinningCubeBackendsSupportArrayReturns()) {
    INFO("Skipping spinning cube demo skip checks until browser/native gfx backends are available");
    CHECK(true);
    return;
  }
  std::filesystem::path scriptPath =
      std::filesystem::path("..") / "scripts" / "run_spinning_cube_demo.sh";
  if (!std::filesystem::exists(scriptPath)) {
    scriptPath = std::filesystem::current_path() / "scripts" / "run_spinning_cube_demo.sh";
  }
  REQUIRE(std::filesystem::exists(scriptPath));

  const std::filesystem::path outDir =
      testScratchPath("") / "primec_spinning_cube_demo_script_native_skip";
  std::error_code ec;
  std::filesystem::remove_all(outDir, ec);
  std::filesystem::create_directories(outDir, ec);
  REQUIRE(!ec);

  const std::filesystem::path binDir = outDir / "bin";
  std::filesystem::create_directories(binDir, ec);
  REQUIRE(!ec);

  const std::filesystem::path fakePrimecPath = binDir / "primec";
  {
    std::ofstream fakePrimec(fakePrimecPath);
    fakePrimec << "#!/usr/bin/env bash\n";
    fakePrimec << "set -euo pipefail\n";
    fakePrimec << "for arg in \"$@\"; do\n";
    fakePrimec << "  if [[ \"$arg\" == \"--emit=native\" ]]; then\n";
    fakePrimec << "    echo \"backend does not support return type\" >&2\n";
    fakePrimec << "    exit 1\n";
    fakePrimec << "  fi\n";
    fakePrimec << "done\n";
    fakePrimec << "echo \"unexpected invocation\" >&2\n";
    fakePrimec << "exit 99\n";
  }
  REQUIRE(runCommand("chmod +x " + quoteShellArg(fakePrimecPath.string())) == 0);

  const std::filesystem::path fakePythonPath = binDir / "python3";
  {
    std::ofstream fakePython(fakePythonPath);
    fakePython << "#!/usr/bin/env bash\n";
    fakePython << "exit 1\n";
  }
  REQUIRE(runCommand("chmod +x " + quoteShellArg(fakePythonPath.string())) == 0);

  const std::filesystem::path fakeXcrunPath = binDir / "xcrun";
  {
    std::ofstream fakeXcrun(fakeXcrunPath);
    fakeXcrun << "#!/usr/bin/env bash\n";
    fakeXcrun << "exit 1\n";
  }
  REQUIRE(runCommand("chmod +x " + quoteShellArg(fakeXcrunPath.string())) == 0);

  const std::filesystem::path fakeCxxPath = binDir / "c++";
  {
    std::ofstream fakeCxx(fakeCxxPath);
    fakeCxx << "#!/usr/bin/env bash\n";
    fakeCxx << "if [[ \"${1:-}\" == \"--version\" ]]; then\n";
    fakeCxx << "  echo \"fake-cxx\"\n";
    fakeCxx << "  exit 0\n";
    fakeCxx << "fi\n";
    fakeCxx << "echo \"unexpected c++ invocation\" >&2\n";
    fakeCxx << "exit 99\n";
  }
  REQUIRE(runCommand("chmod +x " + quoteShellArg(fakeCxxPath.string())) == 0);

  const std::filesystem::path outPath = outDir / "script.out.txt";
  const std::filesystem::path errPath = outDir / "script.err.txt";
  const std::string command =
      "PATH=" + quoteShellArg(binDir.string() + ":/usr/bin:/bin") + " " + quoteShellArg(scriptPath.string()) +
      " --primec " + quoteShellArg(fakePrimecPath.string()) + " --work-dir " + quoteShellArg((outDir / "work").string()) +
      " > " + quoteShellArg(outPath.string()) + " 2> " + quoteShellArg(errPath.string());
  CHECK(runCommand(command) == 0);

  const std::string output = readFile(outPath.string());
  const std::string diagnostics = readFile(errPath.string());
  CHECK(diagnostics.empty());
  CHECK(output.find("[spinning-cube-demo] WEB: SKIP (python3 unavailable)") != std::string::npos);
  CHECK(output.find("[spinning-cube-demo] NATIVE: SKIP (native backend limitation: backend does not support return type)") !=
        std::string::npos);
  CHECK(output.find("[spinning-cube-demo] METAL: SKIP (xcrun unavailable)") != std::string::npos);
  CHECK(output.find("[spinning-cube-demo] RESULT: PASS") != std::string::npos);
}

TEST_CASE("spinning cube demo script accepts primec path with spaces") {
  if (!spinningCubeBackendsSupportArrayReturns()) {
    INFO("Skipping spinning cube demo path quoting checks until browser/native gfx backends are available");
    CHECK(true);
    return;
  }
  std::filesystem::path scriptPath =
      std::filesystem::path("..") / "scripts" / "run_spinning_cube_demo.sh";
  if (!std::filesystem::exists(scriptPath)) {
    scriptPath = std::filesystem::current_path() / "scripts" / "run_spinning_cube_demo.sh";
  }
  REQUIRE(std::filesystem::exists(scriptPath));

  const std::filesystem::path outDir =
      testScratchPath("") / "primec_spinning_cube_demo_script_primec_path_spaces";
  std::error_code ec;
  std::filesystem::remove_all(outDir, ec);
  std::filesystem::create_directories(outDir, ec);
  REQUIRE(!ec);

  const std::filesystem::path binDir = outDir / "bin with spaces";
  std::filesystem::create_directories(binDir, ec);
  REQUIRE(!ec);

  const std::filesystem::path fakePrimecPath = binDir / "primec stub";
  {
    std::ofstream fakePrimec(fakePrimecPath);
    fakePrimec << "#!/usr/bin/env bash\n";
    fakePrimec << "set -euo pipefail\n";
    fakePrimec << "for arg in \"$@\"; do\n";
    fakePrimec << "  if [[ \"$arg\" == \"--emit=native\" ]]; then\n";
    fakePrimec << "    echo \"backend does not support return type\" >&2\n";
    fakePrimec << "    exit 1\n";
    fakePrimec << "  fi\n";
    fakePrimec << "done\n";
    fakePrimec << "echo \"unexpected invocation\" >&2\n";
    fakePrimec << "exit 99\n";
  }
  REQUIRE(runCommand("chmod +x " + quoteShellArg(fakePrimecPath.string())) == 0);

  const std::filesystem::path fakePythonPath = binDir / "python3";
  {
    std::ofstream fakePython(fakePythonPath);
    fakePython << "#!/usr/bin/env bash\n";
    fakePython << "exit 1\n";
  }
  REQUIRE(runCommand("chmod +x " + quoteShellArg(fakePythonPath.string())) == 0);

  const std::filesystem::path fakeXcrunPath = binDir / "xcrun";
  {
    std::ofstream fakeXcrun(fakeXcrunPath);
    fakeXcrun << "#!/usr/bin/env bash\n";
    fakeXcrun << "exit 1\n";
  }
  REQUIRE(runCommand("chmod +x " + quoteShellArg(fakeXcrunPath.string())) == 0);

  const std::filesystem::path fakeCxxPath = binDir / "c++";
  {
    std::ofstream fakeCxx(fakeCxxPath);
    fakeCxx << "#!/usr/bin/env bash\n";
    fakeCxx << "if [[ \"${1:-}\" == \"--version\" ]]; then\n";
    fakeCxx << "  echo \"fake-cxx\"\n";
    fakeCxx << "  exit 0\n";
    fakeCxx << "fi\n";
    fakeCxx << "echo \"unexpected c++ invocation\" >&2\n";
    fakeCxx << "exit 99\n";
  }
  REQUIRE(runCommand("chmod +x " + quoteShellArg(fakeCxxPath.string())) == 0);

  const std::filesystem::path outPath = outDir / "script.out.txt";
  const std::filesystem::path errPath = outDir / "script.err.txt";
  const std::string command =
      "PATH=" + quoteShellArg(binDir.string() + ":/usr/bin:/bin") + " " + quoteShellArg(scriptPath.string()) +
      " --primec " + quoteShellArg(fakePrimecPath.string()) + " --work-dir " + quoteShellArg((outDir / "work").string()) +
      " > " + quoteShellArg(outPath.string()) + " 2> " + quoteShellArg(errPath.string());
  CHECK(runCommand(command) == 0);

  const std::string output = readFile(outPath.string());
  const std::string diagnostics = readFile(errPath.string());
  CHECK(diagnostics.empty());
  CHECK(output.find("[spinning-cube-demo] WEB: SKIP (python3 unavailable)") != std::string::npos);
  CHECK(output.find("[spinning-cube-demo] NATIVE: SKIP (native backend limitation: backend does not support return type)") !=
        std::string::npos);
  CHECK(output.find("[spinning-cube-demo] METAL: SKIP (xcrun unavailable)") != std::string::npos);
  CHECK(output.find("[spinning-cube-demo] RESULT: PASS") != std::string::npos);
}

TEST_CASE("spinning cube demo script accepts work-dir path with spaces") {
  if (!spinningCubeBackendsSupportArrayReturns()) {
    INFO("Skipping spinning cube demo work-dir quoting checks until browser/native gfx backends are available");
    CHECK(true);
    return;
  }
  std::filesystem::path scriptPath =
      std::filesystem::path("..") / "scripts" / "run_spinning_cube_demo.sh";
  if (!std::filesystem::exists(scriptPath)) {
    scriptPath = std::filesystem::current_path() / "scripts" / "run_spinning_cube_demo.sh";
  }
  REQUIRE(std::filesystem::exists(scriptPath));

  const std::filesystem::path outDir =
      testScratchPath("") / "primec_spinning_cube_demo_script_work_dir_spaces";
  std::error_code ec;
  std::filesystem::remove_all(outDir, ec);
  std::filesystem::create_directories(outDir, ec);
  REQUIRE(!ec);

  const std::filesystem::path binDir = outDir / "bin";
  std::filesystem::create_directories(binDir, ec);
  REQUIRE(!ec);

  const std::filesystem::path fakePrimecPath = binDir / "primec";
  {
    std::ofstream fakePrimec(fakePrimecPath);
    fakePrimec << "#!/usr/bin/env bash\n";
    fakePrimec << "set -euo pipefail\n";
    fakePrimec << "for arg in \"$@\"; do\n";
    fakePrimec << "  if [[ \"$arg\" == \"--emit=native\" ]]; then\n";
    fakePrimec << "    echo \"backend does not support return type\" >&2\n";
    fakePrimec << "    exit 1\n";
    fakePrimec << "  fi\n";
    fakePrimec << "done\n";
    fakePrimec << "echo \"unexpected invocation\" >&2\n";
    fakePrimec << "exit 99\n";
  }
  REQUIRE(runCommand("chmod +x " + quoteShellArg(fakePrimecPath.string())) == 0);

  const std::filesystem::path fakePythonPath = binDir / "python3";
  {
    std::ofstream fakePython(fakePythonPath);
    fakePython << "#!/usr/bin/env bash\n";
    fakePython << "exit 1\n";
  }
  REQUIRE(runCommand("chmod +x " + quoteShellArg(fakePythonPath.string())) == 0);

  const std::filesystem::path fakeXcrunPath = binDir / "xcrun";
  {
    std::ofstream fakeXcrun(fakeXcrunPath);
    fakeXcrun << "#!/usr/bin/env bash\n";
    fakeXcrun << "exit 1\n";
  }
  REQUIRE(runCommand("chmod +x " + quoteShellArg(fakeXcrunPath.string())) == 0);

  const std::filesystem::path fakeCxxPath = binDir / "c++";
  {
    std::ofstream fakeCxx(fakeCxxPath);
    fakeCxx << "#!/usr/bin/env bash\n";
    fakeCxx << "if [[ \"${1:-}\" == \"--version\" ]]; then\n";
    fakeCxx << "  echo \"fake-cxx\"\n";
    fakeCxx << "  exit 0\n";
    fakeCxx << "fi\n";
    fakeCxx << "echo \"unexpected c++ invocation\" >&2\n";
    fakeCxx << "exit 99\n";
  }
  REQUIRE(runCommand("chmod +x " + quoteShellArg(fakeCxxPath.string())) == 0);

  const std::filesystem::path workDirPath = outDir / "work dir with spaces";
  const std::filesystem::path outPath = outDir / "script.out.txt";
  const std::filesystem::path errPath = outDir / "script.err.txt";
  const std::string command =
      "PATH=" + quoteShellArg(binDir.string() + ":/usr/bin:/bin") + " " + quoteShellArg(scriptPath.string()) +
      " --primec " + quoteShellArg(fakePrimecPath.string()) + " --work-dir " + quoteShellArg(workDirPath.string()) +
      " > " + quoteShellArg(outPath.string()) + " 2> " + quoteShellArg(errPath.string());
  CHECK(runCommand(command) == 0);

  const std::string output = readFile(outPath.string());
  const std::string diagnostics = readFile(errPath.string());
  CHECK(diagnostics.empty());
  CHECK(output.find("[spinning-cube-demo] WEB: SKIP (python3 unavailable)") != std::string::npos);
  CHECK(output.find("[spinning-cube-demo] NATIVE: SKIP (native backend limitation: backend does not support return type)") !=
        std::string::npos);
  CHECK(output.find("[spinning-cube-demo] METAL: SKIP (xcrun unavailable)") != std::string::npos);
  CHECK(output.find("[spinning-cube-demo] RESULT: PASS") != std::string::npos);
  CHECK(std::filesystem::exists(workDirPath / "native"));
}

TEST_CASE("spinning cube demo script skips when browser command is unavailable") {
  if (!spinningCubeBackendsSupportArrayReturns()) {
    INFO("Skipping spinning cube demo browser-skip checks until browser/native gfx backends are available");
    CHECK(true);
    return;
  }
  std::filesystem::path scriptPath =
      std::filesystem::path("..") / "scripts" / "run_spinning_cube_demo.sh";
  if (!std::filesystem::exists(scriptPath)) {
    scriptPath = std::filesystem::current_path() / "scripts" / "run_spinning_cube_demo.sh";
  }
  REQUIRE(std::filesystem::exists(scriptPath));

  const std::filesystem::path outDir =
      testScratchPath("") / "primec_spinning_cube_demo_script_browser_unavailable";
  std::error_code ec;
  std::filesystem::remove_all(outDir, ec);
  std::filesystem::create_directories(outDir, ec);
  REQUIRE(!ec);

  const std::filesystem::path binDir = outDir / "bin";
  std::filesystem::create_directories(binDir, ec);
  REQUIRE(!ec);

  const std::filesystem::path fakePrimecPath = binDir / "primec";
  {
    std::ofstream fakePrimec(fakePrimecPath);
    fakePrimec << "#!/usr/bin/env bash\n";
    fakePrimec << "set -euo pipefail\n";
    fakePrimec << "for arg in \"$@\"; do\n";
    fakePrimec << "  if [[ \"$arg\" == \"--emit=native\" ]]; then\n";
    fakePrimec << "    echo \"backend does not support return type\" >&2\n";
    fakePrimec << "    exit 1\n";
    fakePrimec << "  fi\n";
    fakePrimec << "done\n";
    fakePrimec << "echo \"unexpected invocation\" >&2\n";
    fakePrimec << "exit 99\n";
  }
  REQUIRE(runCommand("chmod +x " + quoteShellArg(fakePrimecPath.string())) == 0);

  const std::filesystem::path fakePythonPath = binDir / "python3";
  {
    std::ofstream fakePython(fakePythonPath);
    fakePython << "#!/usr/bin/env bash\n";
    fakePython << "if [[ \"${1:-}\" == \"--version\" ]]; then\n";
    fakePython << "  echo \"Python 3.11.0\"\n";
    fakePython << "  exit 0\n";
    fakePython << "fi\n";
    fakePython << "exit 1\n";
  }
  REQUIRE(runCommand("chmod +x " + quoteShellArg(fakePythonPath.string())) == 0);

  const char *browserNames[] = {"chromium", "google-chrome", "chrome", "google-chrome-stable"};
  for (const char *name : browserNames) {
    const std::filesystem::path browserPath = binDir / name;
    std::ofstream browserStub(browserPath);
    browserStub << "#!/usr/bin/env bash\n";
    browserStub << "exit 1\n";
    REQUIRE(runCommand("chmod +x " + quoteShellArg(browserPath.string())) == 0);
  }

  const std::filesystem::path fakeXcrunPath = binDir / "xcrun";
  {
    std::ofstream fakeXcrun(fakeXcrunPath);
    fakeXcrun << "#!/usr/bin/env bash\n";
    fakeXcrun << "exit 1\n";
  }
  REQUIRE(runCommand("chmod +x " + quoteShellArg(fakeXcrunPath.string())) == 0);

  const std::filesystem::path fakeCxxPath = binDir / "c++";
  {
    std::ofstream fakeCxx(fakeCxxPath);
    fakeCxx << "#!/usr/bin/env bash\n";
    fakeCxx << "if [[ \"${1:-}\" == \"--version\" ]]; then\n";
    fakeCxx << "  echo \"fake-cxx\"\n";
    fakeCxx << "  exit 0\n";
    fakeCxx << "fi\n";
    fakeCxx << "echo \"unexpected c++ invocation\" >&2\n";
    fakeCxx << "exit 99\n";
  }
  REQUIRE(runCommand("chmod +x " + quoteShellArg(fakeCxxPath.string())) == 0);

  const std::filesystem::path outPath = outDir / "script.out.txt";
  const std::filesystem::path errPath = outDir / "script.err.txt";
  const std::string command =
      "PATH=" + quoteShellArg(binDir.string() + ":/usr/bin:/bin") + " " + quoteShellArg(scriptPath.string()) +
      " --primec " + quoteShellArg(fakePrimecPath.string()) + " --work-dir " + quoteShellArg((outDir / "work").string()) +
      " > " + quoteShellArg(outPath.string()) + " 2> " + quoteShellArg(errPath.string());
  CHECK(runCommand(command) == 0);

  const std::string output = readFile(outPath.string());
  const std::string diagnostics = readFile(errPath.string());
  CHECK(diagnostics.empty());
  CHECK(output.find("[spinning-cube-demo] WEB: SKIP (headless browser unavailable)") != std::string::npos);
  CHECK(output.find("[spinning-cube-demo] NATIVE: SKIP (native backend limitation: backend does not support return type)") !=
        std::string::npos);
  CHECK(output.find("[spinning-cube-demo] METAL: SKIP (xcrun unavailable)") != std::string::npos);
  CHECK(output.find("[spinning-cube-demo] RESULT: PASS") != std::string::npos);
}

TEST_CASE("spinning cube demo script reports fail when native compile fails") {
  if (!spinningCubeBackendsSupportArrayReturns()) {
    INFO("Skipping spinning cube demo native-fail checks until browser/native gfx backends are available");
    CHECK(true);
    return;
  }
  std::filesystem::path scriptPath =
      std::filesystem::path("..") / "scripts" / "run_spinning_cube_demo.sh";
  if (!std::filesystem::exists(scriptPath)) {
    scriptPath = std::filesystem::current_path() / "scripts" / "run_spinning_cube_demo.sh";
  }
  REQUIRE(std::filesystem::exists(scriptPath));

  const std::filesystem::path outDir =
      testScratchPath("") / "primec_spinning_cube_demo_script_native_fail";
  std::error_code ec;
  std::filesystem::remove_all(outDir, ec);
  std::filesystem::create_directories(outDir, ec);
  REQUIRE(!ec);

  const std::filesystem::path binDir = outDir / "bin";
  std::filesystem::create_directories(binDir, ec);
  REQUIRE(!ec);

  const std::filesystem::path fakePrimecPath = binDir / "primec";
  {
    std::ofstream fakePrimec(fakePrimecPath);
    fakePrimec << "#!/usr/bin/env bash\n";
    fakePrimec << "set -euo pipefail\n";
    fakePrimec << "for arg in \"$@\"; do\n";
    fakePrimec << "  if [[ \"$arg\" == \"--emit=native\" ]]; then\n";
    fakePrimec << "    echo \"synthetic native compile failure\" >&2\n";
    fakePrimec << "    exit 1\n";
    fakePrimec << "  fi\n";
    fakePrimec << "done\n";
    fakePrimec << "echo \"unexpected invocation\" >&2\n";
    fakePrimec << "exit 99\n";
  }
  REQUIRE(runCommand("chmod +x " + quoteShellArg(fakePrimecPath.string())) == 0);

  const std::filesystem::path fakePythonPath = binDir / "python3";
  {
    std::ofstream fakePython(fakePythonPath);
    fakePython << "#!/usr/bin/env bash\n";
    fakePython << "exit 1\n";
  }
  REQUIRE(runCommand("chmod +x " + quoteShellArg(fakePythonPath.string())) == 0);

  const std::filesystem::path fakeXcrunPath = binDir / "xcrun";
  {
    std::ofstream fakeXcrun(fakeXcrunPath);
    fakeXcrun << "#!/usr/bin/env bash\n";
    fakeXcrun << "exit 1\n";
  }
  REQUIRE(runCommand("chmod +x " + quoteShellArg(fakeXcrunPath.string())) == 0);

  const std::filesystem::path fakeCxxPath = binDir / "c++";
  {
    std::ofstream fakeCxx(fakeCxxPath);
    fakeCxx << "#!/usr/bin/env bash\n";
    fakeCxx << "if [[ \"${1:-}\" == \"--version\" ]]; then\n";
    fakeCxx << "  echo \"fake-cxx\"\n";
    fakeCxx << "  exit 0\n";
    fakeCxx << "fi\n";
    fakeCxx << "echo \"unexpected c++ invocation\" >&2\n";
    fakeCxx << "exit 99\n";
  }
  REQUIRE(runCommand("chmod +x " + quoteShellArg(fakeCxxPath.string())) == 0);

  const std::filesystem::path outPath = outDir / "script.out.txt";
  const std::filesystem::path errPath = outDir / "script.err.txt";
  const std::string command =
      "PATH=" + quoteShellArg(binDir.string() + ":/usr/bin:/bin") + " " + quoteShellArg(scriptPath.string()) +
      " --primec " + quoteShellArg(fakePrimecPath.string()) + " --work-dir " + quoteShellArg((outDir / "work").string()) +
      " > " + quoteShellArg(outPath.string()) + " 2> " + quoteShellArg(errPath.string());
  CHECK(runCommand(command) == 1);

  const std::string output = readFile(outPath.string());
  const std::string diagnostics = readFile(errPath.string());
  CHECK(diagnostics.empty());
  CHECK(output.find("[spinning-cube-demo] WEB: SKIP (python3 unavailable)") != std::string::npos);
  CHECK(output.find("[spinning-cube-demo] NATIVE: FAIL (failed to compile cube native binary)") != std::string::npos);
  CHECK(output.find("[spinning-cube-demo] METAL: SKIP (xcrun unavailable)") != std::string::npos);
  CHECK(output.find("[spinning-cube-demo] RESULT: FAIL") != std::string::npos);
}


TEST_SUITE_END();
