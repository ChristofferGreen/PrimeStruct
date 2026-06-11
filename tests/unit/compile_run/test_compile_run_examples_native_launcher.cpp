#include "test_compile_run_helpers.h"

#include "test_compile_run_examples_helpers.h"

TEST_SUITE_BEGIN("primestruct.compile.run.examples");

TEST_CASE("native window launcher wrapper delegates to shared gfx helper") {
  std::filesystem::path wrapperPath =
      std::filesystem::path("..") / "scripts" / "run_native_spinning_cube_window.sh";
  std::filesystem::path helperPath =
      std::filesystem::path("..") / "scripts" / "run_canonical_gfx_native_window.sh";
  if (!std::filesystem::exists(wrapperPath)) {
    wrapperPath = std::filesystem::current_path() / "scripts" / "run_native_spinning_cube_window.sh";
  }
  if (!std::filesystem::exists(helperPath)) {
    helperPath = std::filesystem::current_path() / "scripts" / "run_canonical_gfx_native_window.sh";
  }
  REQUIRE(std::filesystem::exists(wrapperPath));
  REQUIRE(std::filesystem::exists(helperPath));

  const std::string wrapperSource = readFile(wrapperPath.string());
  const std::string helperSource = readFile(helperPath.string());

  CHECK(wrapperSource.find("exec bash \"$ROOT_DIR/scripts/run_canonical_gfx_native_window.sh\"") != std::string::npos);
  CHECK(wrapperSource.find("--usage-name \"run_native_spinning_cube_window.sh\"") != std::string::npos);
  CHECK(wrapperSource.find("--prime-source \"$ROOT_DIR/examples/web/spinning_cube/cube.prime\"") !=
        std::string::npos);
  CHECK(wrapperSource.find("--entry /cubeStdGfxEmitFrameStream") != std::string::npos);
  CHECK(wrapperSource.find("--host-source \"$ROOT_DIR/examples/native/spinning_cube/window_host.mm\"") !=
        std::string::npos);
  CHECK(wrapperSource.find("--preflight-script \"$ROOT_DIR/scripts/preflight_native_spinning_cube_window.sh\"") !=
        std::string::npos);
  CHECK(wrapperSource.find("--out-subdir \"spinning-cube-native-window\"") != std::string::npos);
  CHECK(wrapperSource.find("xcrun clang++") == std::string::npos);
  CHECK(wrapperSource.find("launchctl print") == std::string::npos);
  CHECK(wrapperSource.find("PRIMEC_BIN=") == std::string::npos);

  CHECK(helperSource.find("USAGE_NAME=\"$(basename \"$0\")\"") != std::string::npos);
  CHECK(helperSource.find("usage: ${USAGE_NAME}") != std::string::npos);
  CHECK(helperSource.find("Compiling ${STREAM_DESCRIPTION}") != std::string::npos);
  CHECK(helperSource.find("Compiling ${HOST_DESCRIPTION}") != std::string::npos);
  CHECK(helperSource.find("Launching ${HOST_DESCRIPTION}") != std::string::npos);
  CHECK(helperSource.find("launchctl print \"gui/$userId\"") != std::string::npos);
  CHECK(helperSource.find("visual smoke criterion failed: rotation_changes_over_time") != std::string::npos);
  CHECK(helperSource.find("xcrun clang++ -std=c++17 -fobjc-arc") != std::string::npos);
}

TEST_CASE("native window launcher script builds and launches with preflight") {
  std::filesystem::path scriptPath =
      std::filesystem::path("..") / "scripts" / "run_native_spinning_cube_window.sh";
  if (!std::filesystem::exists(scriptPath)) {
    scriptPath = std::filesystem::current_path() / "scripts" / "run_native_spinning_cube_window.sh";
  }
  REQUIRE(std::filesystem::exists(scriptPath));

  const std::filesystem::path tempRoot =
      testScratchPath("") / "primec_native_window_launcher_success";
  std::error_code ec;
  std::filesystem::remove_all(tempRoot, ec);
  std::filesystem::create_directories(tempRoot, ec);
  REQUIRE(!ec);

  const std::filesystem::path binDir = tempRoot / "bin";
  std::filesystem::create_directories(binDir, ec);
  REQUIRE(!ec);

  const std::filesystem::path fakePrimecPath = binDir / "primec";
  {
    std::ofstream fakePrimec(fakePrimecPath);
    fakePrimec << "#!/usr/bin/env bash\n";
    fakePrimec << "set -euo pipefail\n";
    fakePrimec << "out=\"\"\n";
    fakePrimec << "while [[ $# -gt 0 ]]; do\n";
    fakePrimec << "  if [[ \"$1\" == \"-o\" ]]; then\n";
    fakePrimec << "    out=\"$2\"\n";
    fakePrimec << "    shift 2\n";
    fakePrimec << "    continue\n";
    fakePrimec << "  fi\n";
    fakePrimec << "  shift\n";
    fakePrimec << "done\n";
    fakePrimec << "if [[ -z \"$out\" ]]; then\n";
    fakePrimec << "  echo \"missing -o\" >&2\n";
    fakePrimec << "  exit 91\n";
    fakePrimec << "fi\n";
    fakePrimec << "cat > \"$out\" <<'EOS'\n";
    fakePrimec << "#!/usr/bin/env bash\n";
    fakePrimec << "exit 0\n";
    fakePrimec << "EOS\n";
    fakePrimec << "chmod +x \"$out\"\n";
  }
  REQUIRE(runCommand("chmod +x " + quoteShellArg(fakePrimecPath.string())) == 0);

  const std::filesystem::path fakeXcrunPath = binDir / "xcrun";
  {
    std::ofstream fakeXcrun(fakeXcrunPath);
    fakeXcrun << "#!/usr/bin/env bash\n";
    fakeXcrun << "set -euo pipefail\n";
    fakeXcrun << "if [[ \"${1:-}\" == \"--find\" && \"${2:-}\" == \"metal\" ]]; then\n";
    fakeXcrun << "  echo \"/fake/toolchain/metal\"\n";
    fakeXcrun << "  exit 0\n";
    fakeXcrun << "fi\n";
    fakeXcrun << "if [[ \"${1:-}\" == \"--find\" && \"${2:-}\" == \"metallib\" ]]; then\n";
    fakeXcrun << "  echo \"/fake/toolchain/metallib\"\n";
    fakeXcrun << "  exit 0\n";
    fakeXcrun << "fi\n";
    fakeXcrun << "if [[ \"${1:-}\" == \"clang++\" ]]; then\n";
    fakeXcrun << "  shift\n";
    fakeXcrun << "  out=\"\"\n";
    fakeXcrun << "  while [[ $# -gt 0 ]]; do\n";
    fakeXcrun << "    if [[ \"$1\" == \"-o\" ]]; then\n";
    fakeXcrun << "      out=\"$2\"\n";
    fakeXcrun << "      shift 2\n";
    fakeXcrun << "      continue\n";
    fakeXcrun << "    fi\n";
    fakeXcrun << "    shift\n";
    fakeXcrun << "  done\n";
    fakeXcrun << "  if [[ -z \"$out\" ]]; then\n";
    fakeXcrun << "    echo \"missing -o\" >&2\n";
    fakeXcrun << "    exit 92\n";
    fakeXcrun << "  fi\n";
    fakeXcrun << "  cat > \"$out\" <<'EOS'\n";
    fakeXcrun << "#!/usr/bin/env bash\n";
    fakeXcrun << "set -euo pipefail\n";
    fakeXcrun << "echo \"window_host_invoked=1\"\n";
    fakeXcrun << "echo \"window_host_args=$*\"\n";
    fakeXcrun << "if [[ \"${1:-}\" != \"--gfx\" || -z \"${2:-}\" ]]; then\n";
    fakeXcrun << "  echo \"bad args\" >&2\n";
    fakeXcrun << "  exit 93\n";
    fakeXcrun << "fi\n";
    fakeXcrun << "if [[ -x \"${2}\" ]]; then\n";
    fakeXcrun << "  echo \"gfx_stream_exists=1\"\n";
    fakeXcrun << "else\n";
    fakeXcrun << "  echo \"gfx_stream_exists=0\"\n";
    fakeXcrun << "fi\n";
    fakeXcrun << "exit 0\n";
    fakeXcrun << "EOS\n";
    fakeXcrun << "  chmod +x \"$out\"\n";
    fakeXcrun << "  exit 0\n";
    fakeXcrun << "fi\n";
    fakeXcrun << "echo \"unexpected xcrun invocation\" >&2\n";
    fakeXcrun << "exit 99\n";
  }
  REQUIRE(runCommand("chmod +x " + quoteShellArg(fakeXcrunPath.string())) == 0);

  const std::filesystem::path fakeLaunchctlPath = binDir / "launchctl";
  {
    std::ofstream fakeLaunchctl(fakeLaunchctlPath);
    fakeLaunchctl << "#!/usr/bin/env bash\n";
    fakeLaunchctl << "set -euo pipefail\n";
    fakeLaunchctl << "if [[ \"${1:-}\" == \"print\" && \"${2:-}\" == gui/* ]]; then\n";
    fakeLaunchctl << "  exit 0\n";
    fakeLaunchctl << "fi\n";
    fakeLaunchctl << "echo \"unexpected launchctl invocation\" >&2\n";
    fakeLaunchctl << "exit 99\n";
  }
  REQUIRE(runCommand("chmod +x " + quoteShellArg(fakeLaunchctlPath.string())) == 0);

  const std::filesystem::path outPath = tempRoot / "script.out.txt";
  const std::filesystem::path errPath = tempRoot / "script.err.txt";
  const std::filesystem::path buildDir = tempRoot / "build output";
  const std::string command =
      "PATH=" + quoteShellArg(binDir.string() + ":/usr/bin:/bin") + " " + quoteShellArg(scriptPath.string()) +
      " --primec " + quoteShellArg(fakePrimecPath.string()) + " --out-dir " + quoteShellArg(buildDir.string()) +
      " --max-frames 8 --simulation-smoke > " + quoteShellArg(outPath.string()) + " 2> " +
      quoteShellArg(errPath.string());
  CHECK(runCommand(command) == 0);

  const std::string output = readFile(outPath.string());
  const std::string diagnostics = readFile(errPath.string());
  CHECK(diagnostics.empty());
  CHECK(output.find("[native-window-preflight] PASS: prerequisites satisfied") != std::string::npos);
  CHECK(output.find("[native-window-launcher] Compiling cube stdlib gfx stream binary") != std::string::npos);
  CHECK(output.find("[native-window-launcher] Compiling native window host") != std::string::npos);
  CHECK(output.find("window_host_invoked=1") != std::string::npos);
  CHECK(output.find("window_host_args=--gfx") != std::string::npos);
  CHECK(output.find("--max-frames 8 --simulation-smoke") != std::string::npos);
  CHECK(output.find("gfx_stream_exists=1") != std::string::npos);
  CHECK(output.find("[native-window-launcher] PASS: launch completed") != std::string::npos);
}

TEST_CASE("native window launcher script rejects missing primec binary path") {
  std::filesystem::path scriptPath =
      std::filesystem::path("..") / "scripts" / "run_native_spinning_cube_window.sh";
  if (!std::filesystem::exists(scriptPath)) {
    scriptPath = std::filesystem::current_path() / "scripts" / "run_native_spinning_cube_window.sh";
  }
  REQUIRE(std::filesystem::exists(scriptPath));

  const std::filesystem::path outDir =
      testScratchPath("") / "primec_native_window_launcher_missing_primec";
  std::error_code ec;
  std::filesystem::remove_all(outDir, ec);
  std::filesystem::create_directories(outDir, ec);
  REQUIRE(!ec);

  const std::filesystem::path missingPrimecPath = outDir / "missing-primec";
  const std::filesystem::path outPath = outDir / "script.out.txt";
  const std::filesystem::path errPath = outDir / "script.err.txt";
  const std::string command =
      quoteShellArg(scriptPath.string()) + " --primec " + quoteShellArg(missingPrimecPath.string()) + " > " +
      quoteShellArg(outPath.string()) + " 2> " + quoteShellArg(errPath.string());
  CHECK(runCommand(command) == 2);
  CHECK(readFile(outPath.string()).empty());
  CHECK(readFile(errPath.string()).find("[native-window-launcher] ERROR: primec binary not found: ") !=
        std::string::npos);
}

TEST_CASE("native window launcher script rejects unknown args") {
  std::filesystem::path scriptPath =
      std::filesystem::path("..") / "scripts" / "run_native_spinning_cube_window.sh";
  if (!std::filesystem::exists(scriptPath)) {
    scriptPath = std::filesystem::current_path() / "scripts" / "run_native_spinning_cube_window.sh";
  }
  REQUIRE(std::filesystem::exists(scriptPath));

  const std::filesystem::path outDir =
      testScratchPath("") / "primec_native_window_launcher_unknown_arg";
  std::error_code ec;
  std::filesystem::remove_all(outDir, ec);
  std::filesystem::create_directories(outDir, ec);
  REQUIRE(!ec);

  const std::filesystem::path outPath = outDir / "script.out.txt";
  const std::filesystem::path errPath = outDir / "script.err.txt";
  const std::string command =
      quoteShellArg(scriptPath.string()) + " --bogus > " + quoteShellArg(outPath.string()) + " 2> " +
      quoteShellArg(errPath.string());
  CHECK(runCommand(command) == 2);
  CHECK(readFile(outPath.string()).empty());
  CHECK(readFile(errPath.string()).find("[native-window-launcher] ERROR: unknown arg: --bogus") != std::string::npos);
}

TEST_CASE("native window launcher script rejects incompatible smoke flags") {
  std::filesystem::path scriptPath =
      std::filesystem::path("..") / "scripts" / "run_native_spinning_cube_window.sh";
  if (!std::filesystem::exists(scriptPath)) {
    scriptPath = std::filesystem::current_path() / "scripts" / "run_native_spinning_cube_window.sh";
  }
  REQUIRE(std::filesystem::exists(scriptPath));

  const std::filesystem::path outDir =
      testScratchPath("") / "primec_native_window_launcher_incompatible_smoke_flags";
  std::error_code ec;
  std::filesystem::remove_all(outDir, ec);
  std::filesystem::create_directories(outDir, ec);
  REQUIRE(!ec);

  const std::filesystem::path outPath = outDir / "script.out.txt";
  const std::filesystem::path errPath = outDir / "script.err.txt";
  const std::string command =
      quoteShellArg(scriptPath.string()) + " --simulation-smoke --visual-smoke > " + quoteShellArg(outPath.string()) +
      " 2> " + quoteShellArg(errPath.string());
  CHECK(runCommand(command) == 2);
  CHECK(readFile(outPath.string()).empty());
  CHECK(readFile(errPath.string()).find(
            "[native-window-launcher] ERROR: --simulation-smoke and --visual-smoke cannot be combined") !=
        std::string::npos);
}

TEST_CASE("native window launcher defaults to ten-second bounded run") {
  std::filesystem::path scriptPath =
      std::filesystem::path("..") / "scripts" / "run_native_spinning_cube_window.sh";
  if (!std::filesystem::exists(scriptPath)) {
    scriptPath = std::filesystem::current_path() / "scripts" / "run_native_spinning_cube_window.sh";
  }
  REQUIRE(std::filesystem::exists(scriptPath));

  const std::filesystem::path tempRoot =
      testScratchPath("") / "primec_native_window_launcher_default_ten_second_run";
  std::error_code ec;
  std::filesystem::remove_all(tempRoot, ec);
  std::filesystem::create_directories(tempRoot, ec);
  REQUIRE(!ec);

  const std::filesystem::path binDir = tempRoot / "bin";
  std::filesystem::create_directories(binDir, ec);
  REQUIRE(!ec);

  const std::filesystem::path fakePrimecPath = binDir / "primec";
  {
    std::ofstream fakePrimec(fakePrimecPath);
    fakePrimec << "#!/usr/bin/env bash\n";
    fakePrimec << "set -euo pipefail\n";
    fakePrimec << "out=\"\"\n";
    fakePrimec << "while [[ $# -gt 0 ]]; do\n";
    fakePrimec << "  if [[ \"$1\" == \"-o\" ]]; then\n";
    fakePrimec << "    out=\"$2\"\n";
    fakePrimec << "    shift 2\n";
    fakePrimec << "    continue\n";
    fakePrimec << "  fi\n";
    fakePrimec << "  shift\n";
    fakePrimec << "done\n";
    fakePrimec << "cat > \"$out\" <<'EOS'\n";
    fakePrimec << "#!/usr/bin/env bash\n";
    fakePrimec << "exit 0\n";
    fakePrimec << "EOS\n";
    fakePrimec << "chmod +x \"$out\"\n";
  }
  REQUIRE(runCommand("chmod +x " + quoteShellArg(fakePrimecPath.string())) == 0);

  const std::filesystem::path fakeXcrunPath = binDir / "xcrun";
  {
    std::ofstream fakeXcrun(fakeXcrunPath);
    fakeXcrun << "#!/usr/bin/env bash\n";
    fakeXcrun << "set -euo pipefail\n";
    fakeXcrun << "if [[ \"${1:-}\" == \"--find\" && \"${2:-}\" == \"metal\" ]]; then\n";
    fakeXcrun << "  echo \"/fake/toolchain/metal\"\n";
    fakeXcrun << "  exit 0\n";
    fakeXcrun << "fi\n";
    fakeXcrun << "if [[ \"${1:-}\" == \"--find\" && \"${2:-}\" == \"metallib\" ]]; then\n";
    fakeXcrun << "  echo \"/fake/toolchain/metallib\"\n";
    fakeXcrun << "  exit 0\n";
    fakeXcrun << "fi\n";
    fakeXcrun << "if [[ \"${1:-}\" == \"clang++\" ]]; then\n";
    fakeXcrun << "  shift\n";
    fakeXcrun << "  out=\"\"\n";
    fakeXcrun << "  while [[ $# -gt 0 ]]; do\n";
    fakeXcrun << "    if [[ \"$1\" == \"-o\" ]]; then\n";
    fakeXcrun << "      out=\"$2\"\n";
    fakeXcrun << "      shift 2\n";
    fakeXcrun << "      continue\n";
    fakeXcrun << "    fi\n";
    fakeXcrun << "    shift\n";
    fakeXcrun << "  done\n";
    fakeXcrun << "  cat > \"$out\" <<'EOS'\n";
    fakeXcrun << "#!/usr/bin/env bash\n";
    fakeXcrun << "set -euo pipefail\n";
    fakeXcrun << "echo \"window_host_args=$*\"\n";
    fakeXcrun << "exit 0\n";
    fakeXcrun << "EOS\n";
    fakeXcrun << "  chmod +x \"$out\"\n";
    fakeXcrun << "  exit 0\n";
    fakeXcrun << "fi\n";
    fakeXcrun << "echo \"unexpected xcrun invocation\" >&2\n";
    fakeXcrun << "exit 99\n";
  }
  REQUIRE(runCommand("chmod +x " + quoteShellArg(fakeXcrunPath.string())) == 0);

  const std::filesystem::path fakeLaunchctlPath = binDir / "launchctl";
  {
    std::ofstream fakeLaunchctl(fakeLaunchctlPath);
    fakeLaunchctl << "#!/usr/bin/env bash\n";
    fakeLaunchctl << "set -euo pipefail\n";
    fakeLaunchctl << "if [[ \"${1:-}\" == \"print\" && \"${2:-}\" == gui/* ]]; then\n";
    fakeLaunchctl << "  exit 0\n";
    fakeLaunchctl << "fi\n";
    fakeLaunchctl << "echo \"unexpected launchctl invocation\" >&2\n";
    fakeLaunchctl << "exit 99\n";
  }
  REQUIRE(runCommand("chmod +x " + quoteShellArg(fakeLaunchctlPath.string())) == 0);

  const std::filesystem::path outPath = tempRoot / "script.out.txt";
  const std::filesystem::path errPath = tempRoot / "script.err.txt";
  const std::filesystem::path buildDir = tempRoot / "build output";
  const std::string command =
      "PATH=" + quoteShellArg(binDir.string() + ":/usr/bin:/bin") + " " + quoteShellArg(scriptPath.string()) +
      " --primec " + quoteShellArg(fakePrimecPath.string()) + " --out-dir " + quoteShellArg(buildDir.string()) +
      " > " + quoteShellArg(outPath.string()) + " 2> " + quoteShellArg(errPath.string());
  CHECK(runCommand(command) == 0);

  const std::string output = readFile(outPath.string());
  const std::string diagnostics = readFile(errPath.string());
  CHECK(diagnostics.empty());
  CHECK(output.find("[native-window-launcher] Defaulting to --max-frames 600 (~10s at 60fps)") != std::string::npos);
  CHECK(output.find("window_host_args=--gfx") != std::string::npos);
  CHECK(output.find("--max-frames 600") != std::string::npos);
  CHECK(output.find("[native-window-launcher] PASS: launch completed") != std::string::npos);
}


TEST_SUITE_END();
