#include "test_compile_run_helpers.h"

#include "test_compile_run_examples_helpers.h"

TEST_SUITE_BEGIN("primestruct.compile.run.examples");

TEST_CASE("native window launcher visual smoke skips on non-macOS runners") {
  std::filesystem::path scriptPath =
      std::filesystem::path("..") / "scripts" / "run_native_spinning_cube_window.sh";
  if (!std::filesystem::exists(scriptPath)) {
    scriptPath = std::filesystem::current_path() / "scripts" / "run_native_spinning_cube_window.sh";
  }
  REQUIRE(std::filesystem::exists(scriptPath));

  const std::filesystem::path outDir =
      testScratchPath("") / "primec_native_window_visual_smoke_skip_non_macos";
  std::error_code ec;
  std::filesystem::remove_all(outDir, ec);
  std::filesystem::create_directories(outDir, ec);
  REQUIRE(!ec);

  const std::filesystem::path binDir = outDir / "bin";
  std::filesystem::create_directories(binDir, ec);
  REQUIRE(!ec);

  const std::filesystem::path fakeUnamePath = binDir / "uname";
  {
    std::ofstream fakeUname(fakeUnamePath);
    fakeUname << "#!/usr/bin/env bash\n";
    fakeUname << "echo Linux\n";
  }
  REQUIRE(runCommand("chmod +x " + quoteShellArg(fakeUnamePath.string())) == 0);

  const std::filesystem::path outPath = outDir / "script.out.txt";
  const std::filesystem::path errPath = outDir / "script.err.txt";
  const std::string command = "PATH=" + quoteShellArg(binDir.string() + ":/usr/bin:/bin") + " " +
                              quoteShellArg(scriptPath.string()) + " --visual-smoke > " +
                              quoteShellArg(outPath.string()) + " 2> " + quoteShellArg(errPath.string());
  CHECK(runCommand(command) == 0);
  CHECK(readFile(errPath.string()).empty());
  CHECK(readFile(outPath.string()).find("[native-window-launcher] VISUAL-SMOKE: SKIP non-macOS runner") !=
        std::string::npos);
}

TEST_CASE("native window launcher visual smoke skips without GUI session") {
  std::filesystem::path scriptPath =
      std::filesystem::path("..") / "scripts" / "run_native_spinning_cube_window.sh";
  if (!std::filesystem::exists(scriptPath)) {
    scriptPath = std::filesystem::current_path() / "scripts" / "run_native_spinning_cube_window.sh";
  }
  REQUIRE(std::filesystem::exists(scriptPath));

  const std::filesystem::path outDir =
      testScratchPath("") / "primec_native_window_visual_smoke_skip_no_gui";
  std::error_code ec;
  std::filesystem::remove_all(outDir, ec);
  std::filesystem::create_directories(outDir, ec);
  REQUIRE(!ec);

  const std::filesystem::path binDir = outDir / "bin";
  std::filesystem::create_directories(binDir, ec);
  REQUIRE(!ec);

  const std::filesystem::path fakeUnamePath = binDir / "uname";
  {
    std::ofstream fakeUname(fakeUnamePath);
    fakeUname << "#!/usr/bin/env bash\n";
    fakeUname << "echo Darwin\n";
  }
  REQUIRE(runCommand("chmod +x " + quoteShellArg(fakeUnamePath.string())) == 0);

  const std::filesystem::path fakeLaunchctlPath = binDir / "launchctl";
  {
    std::ofstream fakeLaunchctl(fakeLaunchctlPath);
    fakeLaunchctl << "#!/usr/bin/env bash\n";
    fakeLaunchctl << "set -euo pipefail\n";
    fakeLaunchctl << "if [[ \"${1:-}\" == \"print\" && \"${2:-}\" == gui/* ]]; then\n";
    fakeLaunchctl << "  exit 1\n";
    fakeLaunchctl << "fi\n";
    fakeLaunchctl << "echo \"unexpected launchctl invocation\" >&2\n";
    fakeLaunchctl << "exit 99\n";
  }
  REQUIRE(runCommand("chmod +x " + quoteShellArg(fakeLaunchctlPath.string())) == 0);

  const std::filesystem::path outPath = outDir / "script.out.txt";
  const std::filesystem::path errPath = outDir / "script.err.txt";
  const std::string command = "PATH=" + quoteShellArg(binDir.string() + ":/usr/bin:/bin") + " " +
                              quoteShellArg(scriptPath.string()) + " --visual-smoke > " +
                              quoteShellArg(outPath.string()) + " 2> " + quoteShellArg(errPath.string());
  CHECK(runCommand(command) == 0);
  CHECK(readFile(errPath.string()).empty());
  CHECK(readFile(outPath.string()).find("[native-window-launcher] VISUAL-SMOKE: SKIP GUI session unavailable") !=
        std::string::npos);
}

TEST_CASE("native window launcher visual smoke validates criteria") {
  std::filesystem::path scriptPath =
      std::filesystem::path("..") / "scripts" / "run_native_spinning_cube_window.sh";
  if (!std::filesystem::exists(scriptPath)) {
    scriptPath = std::filesystem::current_path() / "scripts" / "run_native_spinning_cube_window.sh";
  }
  REQUIRE(std::filesystem::exists(scriptPath));

  const std::filesystem::path tempRoot =
      testScratchPath("") / "primec_native_window_visual_smoke_success";
  std::error_code ec;
  std::filesystem::remove_all(tempRoot, ec);
  std::filesystem::create_directories(tempRoot, ec);
  REQUIRE(!ec);

  const std::filesystem::path binDir = tempRoot / "bin";
  std::filesystem::create_directories(binDir, ec);
  REQUIRE(!ec);

  const std::filesystem::path fakeUnamePath = binDir / "uname";
  {
    std::ofstream fakeUname(fakeUnamePath);
    fakeUname << "#!/usr/bin/env bash\n";
    fakeUname << "echo Darwin\n";
  }
  REQUIRE(runCommand("chmod +x " + quoteShellArg(fakeUnamePath.string())) == 0);

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
    fakePrimec << "cat <<'EOD'\n";
    fakePrimec << "17001\n1\n1280\n720\n0\n0\n0\n50\n70\n100\n1000\n1000\n8\n36\n1\n2\n3\n4\n46\n7\n8\n5\n6\n60\n7\n3\n";
    fakePrimec << "0\n0\n100\n0\n1\n16\n99\n1\n";
    fakePrimec << "EOD\n";
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
    fakeXcrun << "echo \"window_created=1\"\n";
    fakeXcrun << "echo \"startup_success=1\"\n";
    fakeXcrun << "echo \"frame_rendered=1\"\n";
    fakeXcrun << "echo \"exit_reason=max_frames\"\n";
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
  const std::filesystem::path buildDir = tempRoot / "build-output";
  const std::string command =
      "PATH=" + quoteShellArg(binDir.string() + ":/usr/bin:/bin") + " " + quoteShellArg(scriptPath.string()) +
      " --primec " + quoteShellArg(fakePrimecPath.string()) + " --out-dir " + quoteShellArg(buildDir.string()) +
      " --visual-smoke --max-frames 4 > " + quoteShellArg(outPath.string()) + " 2> " +
      quoteShellArg(errPath.string());
  CHECK(runCommand(command) == 0);

  const std::string output = readFile(outPath.string());
  const std::string diagnostics = readFile(errPath.string());
  CHECK(diagnostics.empty());
  CHECK(output.find("[native-window-launcher] VISUAL-SMOKE: window_shown=PASS") != std::string::npos);
  CHECK(output.find("[native-window-launcher] VISUAL-SMOKE: render_loop_alive=PASS") != std::string::npos);
  CHECK(output.find("[native-window-launcher] VISUAL-SMOKE: rotation_changes_over_time=PASS") != std::string::npos);
  CHECK(output.find("[native-window-launcher] VISUAL-SMOKE: PASS") != std::string::npos);
}

TEST_CASE("native window launcher visual smoke fails when rotation does not change") {
  std::filesystem::path scriptPath =
      std::filesystem::path("..") / "scripts" / "run_native_spinning_cube_window.sh";
  if (!std::filesystem::exists(scriptPath)) {
    scriptPath = std::filesystem::current_path() / "scripts" / "run_native_spinning_cube_window.sh";
  }
  REQUIRE(std::filesystem::exists(scriptPath));

  const std::filesystem::path tempRoot =
      testScratchPath("") / "primec_native_window_visual_smoke_rotation_fail";
  std::error_code ec;
  std::filesystem::remove_all(tempRoot, ec);
  std::filesystem::create_directories(tempRoot, ec);
  REQUIRE(!ec);

  const std::filesystem::path binDir = tempRoot / "bin";
  std::filesystem::create_directories(binDir, ec);
  REQUIRE(!ec);

  const std::filesystem::path fakeUnamePath = binDir / "uname";
  {
    std::ofstream fakeUname(fakeUnamePath);
    fakeUname << "#!/usr/bin/env bash\n";
    fakeUname << "echo Darwin\n";
  }
  REQUIRE(runCommand("chmod +x " + quoteShellArg(fakeUnamePath.string())) == 0);

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
    fakePrimec << "cat <<'EOD'\n";
    fakePrimec << "17001\n1\n1280\n720\n0\n0\n0\n50\n70\n100\n1000\n1000\n8\n36\n1\n2\n3\n4\n46\n7\n8\n5\n6\n60\n7\n3\n";
    fakePrimec << "0\n0\n100\n0\n1\n0\n99\n1\n";
    fakePrimec << "EOD\n";
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
    fakeXcrun << "echo \"window_created=1\"\n";
    fakeXcrun << "echo \"startup_success=1\"\n";
    fakeXcrun << "echo \"frame_rendered=1\"\n";
    fakeXcrun << "echo \"exit_reason=max_frames\"\n";
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
  const std::filesystem::path buildDir = tempRoot / "build-output";
  const std::string command =
      "PATH=" + quoteShellArg(binDir.string() + ":/usr/bin:/bin") + " " + quoteShellArg(scriptPath.string()) +
      " --primec " + quoteShellArg(fakePrimecPath.string()) + " --out-dir " + quoteShellArg(buildDir.string()) +
      " --visual-smoke --max-frames 4 > " + quoteShellArg(outPath.string()) + " 2> " +
      quoteShellArg(errPath.string());
  CHECK(runCommand(command) == 2);
  CHECK(readFile(errPath.string()).find(
            "[native-window-launcher] ERROR: visual smoke criterion failed: rotation_changes_over_time") !=
        std::string::npos);
}

TEST_CASE("native window launcher compile run coverage validates host build and visual smoke") {
  if (!spinningCubeBackendsSupportArrayReturns()) {
    INFO("Skipping native window launcher compile/run coverage until browser/native gfx backends are available");
    CHECK(true);
    return;
  }
  std::filesystem::path scriptPath =
      std::filesystem::path("..") / "scripts" / "run_native_spinning_cube_window.sh";
  if (!std::filesystem::exists(scriptPath)) {
    scriptPath = std::filesystem::current_path() / "scripts" / "run_native_spinning_cube_window.sh";
  }
  REQUIRE(std::filesystem::exists(scriptPath));

  if (runCommand("uname -s | grep -Fq Darwin") != 0) {
    INFO("SKIP: non-macOS runner; launcher compile/run coverage requires macOS");
    return;
  }
  if (runCommand("command -v launchctl > /dev/null 2>&1") != 0) {
    INFO("SKIP: launchctl unavailable; launcher compile/run coverage requires GUI-capable macOS");
    return;
  }
  if (runCommand("launchctl print gui/$(id -u) > /dev/null 2>&1") != 0) {
    INFO("SKIP: GUI session unavailable; launcher compile/run coverage requires GUI-capable macOS");
    return;
  }
  if (runCommand("xcrun --version > /dev/null 2>&1") != 0) {
    INFO("SKIP: xcrun unavailable; launcher compile/run coverage requires Xcode command-line tools");
    return;
  }
  if (runCommand("xcrun --find metal > /dev/null 2>&1") != 0) {
    INFO("SKIP: xcrun metal unavailable; launcher compile/run coverage requires Metal tooling");
    return;
  }
  if (runCommand("xcrun --find metallib > /dev/null 2>&1") != 0) {
    INFO("SKIP: xcrun metallib unavailable; launcher compile/run coverage requires Metal tooling");
    return;
  }

  const std::filesystem::path outDir =
      testScratchPath("") / "primec_native_window_launcher_compile_run_coverage";
  std::error_code ec;
  std::filesystem::remove_all(outDir, ec);
  std::filesystem::create_directories(outDir, ec);
  REQUIRE(!ec);

  const std::filesystem::path launcherOutPath = outDir / "launcher.out.txt";
  const std::filesystem::path launcherErrPath = outDir / "launcher.err.txt";
  const std::string command =
      quoteShellArg(scriptPath.string()) + " --primec ./primec --out-dir " + quoteShellArg(outDir.string()) +
      " --visual-smoke --max-frames 120 > " + quoteShellArg(launcherOutPath.string()) + " 2> " +
      quoteShellArg(launcherErrPath.string());
  CHECK(runCommand(command) == 0);

  const std::string output = readFile(launcherOutPath.string());
  const std::string diagnostics = readFile(launcherErrPath.string());
  CHECK(diagnostics.find("[native-window-launcher] ERROR:") == std::string::npos);
  CHECK(output.find("[native-window-launcher] Compiling cube stdlib gfx stream binary") != std::string::npos);
  CHECK(output.find("[native-window-launcher] Compiling native window host") != std::string::npos);
  CHECK(output.find("[native-window-launcher] Launching native window host") != std::string::npos);
  CHECK(output.find("[native-window-launcher] VISUAL-SMOKE: window_shown=PASS") != std::string::npos);
  CHECK(output.find("[native-window-launcher] VISUAL-SMOKE: render_loop_alive=PASS") != std::string::npos);
  CHECK(output.find("[native-window-launcher] VISUAL-SMOKE: rotation_changes_over_time=PASS") !=
        std::string::npos);
  CHECK(output.find("[native-window-launcher] VISUAL-SMOKE: PASS") != std::string::npos);
  CHECK(output.find("[native-window-launcher] PASS: launch completed") != std::string::npos);
  CHECK(output.find("[native-window-launcher] VISUAL-SMOKE: SKIP") == std::string::npos);

  CHECK(std::filesystem::exists(outDir / "cube_stdlib_gfx_stream"));
  CHECK(std::filesystem::exists(outDir / "spinning_cube_window_host"));
  CHECK(std::filesystem::exists(outDir / "native_window_host.log"));
}

TEST_CASE("native window preflight script validates required tools and GUI session") {
  std::filesystem::path scriptPath =
      std::filesystem::path("..") / "scripts" / "preflight_native_spinning_cube_window.sh";
  if (!std::filesystem::exists(scriptPath)) {
    scriptPath = std::filesystem::current_path() / "scripts" / "preflight_native_spinning_cube_window.sh";
  }
  REQUIRE(std::filesystem::exists(scriptPath));

  const std::filesystem::path outDir =
      testScratchPath("") / "primec_native_window_preflight_success";
  std::error_code ec;
  std::filesystem::remove_all(outDir, ec);
  std::filesystem::create_directories(outDir, ec);
  REQUIRE(!ec);

  const std::filesystem::path binDir = outDir / "bin";
  std::filesystem::create_directories(binDir, ec);
  REQUIRE(!ec);

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

  const std::filesystem::path outPath = outDir / "script.out.txt";
  const std::filesystem::path errPath = outDir / "script.err.txt";
  const std::string command =
      "PATH=" + quoteShellArg(binDir.string() + ":/usr/bin:/bin") + " " + quoteShellArg(scriptPath.string()) + " > " +
      quoteShellArg(outPath.string()) + " 2> " + quoteShellArg(errPath.string());
  CHECK(runCommand(command) == 0);

  const std::string output = readFile(outPath.string());
  const std::string diagnostics = readFile(errPath.string());
  CHECK(diagnostics.empty());
  CHECK(output.find("[native-window-preflight] xcrun metal: /fake/toolchain/metal") != std::string::npos);
  CHECK(output.find("[native-window-preflight] xcrun metallib: /fake/toolchain/metallib") != std::string::npos);
  CHECK(output.find("[native-window-preflight] GUI session: gui/") != std::string::npos);
  CHECK(output.find("[native-window-preflight] PASS: prerequisites satisfied") != std::string::npos);
}

TEST_CASE("native window preflight script fails when xcrun metal is unavailable") {
  std::filesystem::path scriptPath =
      std::filesystem::path("..") / "scripts" / "preflight_native_spinning_cube_window.sh";
  if (!std::filesystem::exists(scriptPath)) {
    scriptPath = std::filesystem::current_path() / "scripts" / "preflight_native_spinning_cube_window.sh";
  }
  REQUIRE(std::filesystem::exists(scriptPath));

  const std::filesystem::path outDir =
      testScratchPath("") / "primec_native_window_preflight_missing_metal";
  std::error_code ec;
  std::filesystem::remove_all(outDir, ec);
  std::filesystem::create_directories(outDir, ec);
  REQUIRE(!ec);

  const std::filesystem::path binDir = outDir / "bin";
  std::filesystem::create_directories(binDir, ec);
  REQUIRE(!ec);

  const std::filesystem::path fakeXcrunPath = binDir / "xcrun";
  {
    std::ofstream fakeXcrun(fakeXcrunPath);
    fakeXcrun << "#!/usr/bin/env bash\n";
    fakeXcrun << "set -euo pipefail\n";
    fakeXcrun << "if [[ \"${1:-}\" == \"--find\" && \"${2:-}\" == \"metal\" ]]; then\n";
    fakeXcrun << "  exit 1\n";
    fakeXcrun << "fi\n";
    fakeXcrun << "if [[ \"${1:-}\" == \"--find\" && \"${2:-}\" == \"metallib\" ]]; then\n";
    fakeXcrun << "  echo \"/fake/toolchain/metallib\"\n";
    fakeXcrun << "  exit 0\n";
    fakeXcrun << "fi\n";
    fakeXcrun << "echo \"unexpected xcrun invocation\" >&2\n";
    fakeXcrun << "exit 99\n";
  }
  REQUIRE(runCommand("chmod +x " + quoteShellArg(fakeXcrunPath.string())) == 0);

  const std::filesystem::path outPath = outDir / "script.out.txt";
  const std::filesystem::path errPath = outDir / "script.err.txt";
  const std::string command =
      "PATH=" + quoteShellArg(binDir.string() + ":/usr/bin:/bin") + " " + quoteShellArg(scriptPath.string()) + " > " +
      quoteShellArg(outPath.string()) + " 2> " + quoteShellArg(errPath.string());
  CHECK(runCommand(command) == 2);
  CHECK(readFile(outPath.string()).empty());
  CHECK(readFile(errPath.string()).find(
            "[native-window-preflight] ERROR: xcrun metal unavailable; run 'xcrun --find metal' after installing "
            "Command Line Tools.") != std::string::npos);
}

TEST_CASE("native window preflight script fails when xcrun metallib is unavailable") {
  std::filesystem::path scriptPath =
      std::filesystem::path("..") / "scripts" / "preflight_native_spinning_cube_window.sh";
  if (!std::filesystem::exists(scriptPath)) {
    scriptPath = std::filesystem::current_path() / "scripts" / "preflight_native_spinning_cube_window.sh";
  }
  REQUIRE(std::filesystem::exists(scriptPath));

  const std::filesystem::path outDir =
      testScratchPath("") / "primec_native_window_preflight_missing_metallib";
  std::error_code ec;
  std::filesystem::remove_all(outDir, ec);
  std::filesystem::create_directories(outDir, ec);
  REQUIRE(!ec);

  const std::filesystem::path binDir = outDir / "bin";
  std::filesystem::create_directories(binDir, ec);
  REQUIRE(!ec);

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
    fakeXcrun << "  exit 1\n";
    fakeXcrun << "fi\n";
    fakeXcrun << "echo \"unexpected xcrun invocation\" >&2\n";
    fakeXcrun << "exit 99\n";
  }
  REQUIRE(runCommand("chmod +x " + quoteShellArg(fakeXcrunPath.string())) == 0);

  const std::filesystem::path outPath = outDir / "script.out.txt";
  const std::filesystem::path errPath = outDir / "script.err.txt";
  const std::string command =
      "PATH=" + quoteShellArg(binDir.string() + ":/usr/bin:/bin") + " " + quoteShellArg(scriptPath.string()) + " > " +
      quoteShellArg(outPath.string()) + " 2> " + quoteShellArg(errPath.string());
  CHECK(runCommand(command) == 2);
  CHECK(readFile(outPath.string()).empty());
  CHECK(readFile(errPath.string()).find(
            "[native-window-preflight] ERROR: xcrun metallib unavailable; run 'xcrun --find metallib' after "
            "installing Command Line Tools.") != std::string::npos);
}

TEST_CASE("native window preflight script fails when GUI session is unavailable") {
  std::filesystem::path scriptPath =
      std::filesystem::path("..") / "scripts" / "preflight_native_spinning_cube_window.sh";
  if (!std::filesystem::exists(scriptPath)) {
    scriptPath = std::filesystem::current_path() / "scripts" / "preflight_native_spinning_cube_window.sh";
  }
  REQUIRE(std::filesystem::exists(scriptPath));

  const std::filesystem::path outDir =
      testScratchPath("") / "primec_native_window_preflight_missing_gui";
  std::error_code ec;
  std::filesystem::remove_all(outDir, ec);
  std::filesystem::create_directories(outDir, ec);
  REQUIRE(!ec);

  const std::filesystem::path binDir = outDir / "bin";
  std::filesystem::create_directories(binDir, ec);
  REQUIRE(!ec);

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
    fakeLaunchctl << "  exit 1\n";
    fakeLaunchctl << "fi\n";
    fakeLaunchctl << "echo \"unexpected launchctl invocation\" >&2\n";
    fakeLaunchctl << "exit 99\n";
  }
  REQUIRE(runCommand("chmod +x " + quoteShellArg(fakeLaunchctlPath.string())) == 0);

  const std::filesystem::path outPath = outDir / "script.out.txt";
  const std::filesystem::path errPath = outDir / "script.err.txt";
  const std::string command =
      "PATH=" + quoteShellArg(binDir.string() + ":/usr/bin:/bin") + " " + quoteShellArg(scriptPath.string()) + " > " +
      quoteShellArg(outPath.string()) + " 2> " + quoteShellArg(errPath.string());
  CHECK(runCommand(command) == 2);
  CHECK(readFile(outPath.string()).empty());
  CHECK(readFile(errPath.string()).find(
            "[native-window-preflight] ERROR: GUI session unavailable; log in via macOS desktop session and rerun.") !=
        std::string::npos);
}

TEST_CASE("native window preflight script rejects unknown args") {
  std::filesystem::path scriptPath =
      std::filesystem::path("..") / "scripts" / "preflight_native_spinning_cube_window.sh";
  if (!std::filesystem::exists(scriptPath)) {
    scriptPath = std::filesystem::current_path() / "scripts" / "preflight_native_spinning_cube_window.sh";
  }
  REQUIRE(std::filesystem::exists(scriptPath));

  const std::filesystem::path outDir =
      testScratchPath("") / "primec_native_window_preflight_unknown_arg";
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
  CHECK(readFile(errPath.string()).find("[native-window-preflight] ERROR: unknown arg: --bogus") != std::string::npos);
}


TEST_SUITE_END();
