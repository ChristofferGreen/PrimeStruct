#include "test_compile_run_helpers.h"

#include "test_compile_run_examples_helpers.h"

TEST_SUITE_BEGIN("primestruct.compile.run.examples");

TEST_CASE("spinning cube demo script rejects non-integer port base") {
  std::filesystem::path scriptPath =
      std::filesystem::path("..") / "scripts" / "run_spinning_cube_demo.sh";
  if (!std::filesystem::exists(scriptPath)) {
    scriptPath = std::filesystem::current_path() / "scripts" / "run_spinning_cube_demo.sh";
  }
  REQUIRE(std::filesystem::exists(scriptPath));

  const std::filesystem::path outDir =
      testScratchPath("") / "primec_spinning_cube_demo_script_invalid_port";
  std::error_code ec;
  std::filesystem::remove_all(outDir, ec);
  std::filesystem::create_directories(outDir, ec);
  REQUIRE(!ec);

  const std::filesystem::path outPath = outDir / "script.out.txt";
  const std::filesystem::path errPath = outDir / "script.err.txt";
  const std::string command =
      quoteShellArg(scriptPath.string()) + " --primec /bin/true --port-base not-a-number > " +
      quoteShellArg(outPath.string()) + " 2> " + quoteShellArg(errPath.string());
  CHECK(runCommand(command) == 2);
  CHECK(readFile(outPath.string()).empty());
  CHECK(readFile(errPath.string()).find("[spinning-cube-demo] ERROR: --port-base must be an integer: not-a-number") !=
        std::string::npos);
}

TEST_CASE("spinning cube demo script rejects out-of-range port base") {
  std::filesystem::path scriptPath =
      std::filesystem::path("..") / "scripts" / "run_spinning_cube_demo.sh";
  if (!std::filesystem::exists(scriptPath)) {
    scriptPath = std::filesystem::current_path() / "scripts" / "run_spinning_cube_demo.sh";
  }
  REQUIRE(std::filesystem::exists(scriptPath));

  const std::filesystem::path outDir =
      testScratchPath("") / "primec_spinning_cube_demo_script_invalid_port_range";
  std::error_code ec;
  std::filesystem::remove_all(outDir, ec);
  std::filesystem::create_directories(outDir, ec);
  REQUIRE(!ec);

  const std::filesystem::path outPath = outDir / "script.out.txt";
  const std::filesystem::path errPath = outDir / "script.err.txt";
  const std::string command =
      quoteShellArg(scriptPath.string()) + " --primec /bin/true --port-base 70000 > " +
      quoteShellArg(outPath.string()) + " 2> " + quoteShellArg(errPath.string());
  CHECK(runCommand(command) == 2);
  CHECK(readFile(outPath.string()).empty());
  CHECK(readFile(errPath.string()).find("[spinning-cube-demo] ERROR: --port-base out of range (1-65535): 70000") !=
        std::string::npos);
}

TEST_CASE("spinning cube demo script rejects zero port base") {
  std::filesystem::path scriptPath =
      std::filesystem::path("..") / "scripts" / "run_spinning_cube_demo.sh";
  if (!std::filesystem::exists(scriptPath)) {
    scriptPath = std::filesystem::current_path() / "scripts" / "run_spinning_cube_demo.sh";
  }
  REQUIRE(std::filesystem::exists(scriptPath));

  const std::filesystem::path outDir =
      testScratchPath("") / "primec_spinning_cube_demo_script_zero_port";
  std::error_code ec;
  std::filesystem::remove_all(outDir, ec);
  std::filesystem::create_directories(outDir, ec);
  REQUIRE(!ec);

  const std::filesystem::path outPath = outDir / "script.out.txt";
  const std::filesystem::path errPath = outDir / "script.err.txt";
  const std::string command =
      quoteShellArg(scriptPath.string()) + " --primec /bin/true --port-base 0 > " +
      quoteShellArg(outPath.string()) + " 2> " + quoteShellArg(errPath.string());
  CHECK(runCommand(command) == 2);
  CHECK(readFile(outPath.string()).empty());
  CHECK(readFile(errPath.string()).find("[spinning-cube-demo] ERROR: --port-base out of range (1-65535): 0") !=
        std::string::npos);
}

TEST_CASE("spinning cube demo script rejects missing port base value") {
  std::filesystem::path scriptPath =
      std::filesystem::path("..") / "scripts" / "run_spinning_cube_demo.sh";
  if (!std::filesystem::exists(scriptPath)) {
    scriptPath = std::filesystem::current_path() / "scripts" / "run_spinning_cube_demo.sh";
  }
  REQUIRE(std::filesystem::exists(scriptPath));

  const std::filesystem::path outDir =
      testScratchPath("") / "primec_spinning_cube_demo_script_missing_port_value";
  std::error_code ec;
  std::filesystem::remove_all(outDir, ec);
  std::filesystem::create_directories(outDir, ec);
  REQUIRE(!ec);

  const std::filesystem::path outPath = outDir / "script.out.txt";
  const std::filesystem::path errPath = outDir / "script.err.txt";
  const std::string command =
      quoteShellArg(scriptPath.string()) + " --primec /bin/true --port-base > " + quoteShellArg(outPath.string()) +
      " 2> " + quoteShellArg(errPath.string());
  CHECK(runCommand(command) == 2);
  CHECK(readFile(outPath.string()).empty());
  CHECK(readFile(errPath.string()).find("[spinning-cube-demo] ERROR: missing value for --port-base") !=
        std::string::npos);
}

TEST_CASE("spinning cube demo script rejects missing primec value") {
  std::filesystem::path scriptPath =
      std::filesystem::path("..") / "scripts" / "run_spinning_cube_demo.sh";
  if (!std::filesystem::exists(scriptPath)) {
    scriptPath = std::filesystem::current_path() / "scripts" / "run_spinning_cube_demo.sh";
  }
  REQUIRE(std::filesystem::exists(scriptPath));

  const std::filesystem::path outDir =
      testScratchPath("") / "primec_spinning_cube_demo_script_missing_primec_value";
  std::error_code ec;
  std::filesystem::remove_all(outDir, ec);
  std::filesystem::create_directories(outDir, ec);
  REQUIRE(!ec);

  const std::filesystem::path outPath = outDir / "script.out.txt";
  const std::filesystem::path errPath = outDir / "script.err.txt";
  const std::string command =
      quoteShellArg(scriptPath.string()) + " --primec > " + quoteShellArg(outPath.string()) + " 2> " +
      quoteShellArg(errPath.string());
  CHECK(runCommand(command) == 2);
  CHECK(readFile(outPath.string()).empty());
  CHECK(readFile(errPath.string()).find("[spinning-cube-demo] ERROR: missing value for --primec") !=
        std::string::npos);
}

TEST_CASE("spinning cube demo script rejects primec followed by flag") {
  std::filesystem::path scriptPath =
      std::filesystem::path("..") / "scripts" / "run_spinning_cube_demo.sh";
  if (!std::filesystem::exists(scriptPath)) {
    scriptPath = std::filesystem::current_path() / "scripts" / "run_spinning_cube_demo.sh";
  }
  REQUIRE(std::filesystem::exists(scriptPath));

  const std::filesystem::path outDir =
      testScratchPath("") / "primec_spinning_cube_demo_script_primec_followed_by_flag";
  std::error_code ec;
  std::filesystem::remove_all(outDir, ec);
  std::filesystem::create_directories(outDir, ec);
  REQUIRE(!ec);

  const std::filesystem::path outPath = outDir / "script.out.txt";
  const std::filesystem::path errPath = outDir / "script.err.txt";
  const std::string command = quoteShellArg(scriptPath.string()) +
                              " --primec --work-dir /tmp > " + quoteShellArg(outPath.string()) + " 2> " +
                              quoteShellArg(errPath.string());
  CHECK(runCommand(command) == 2);
  CHECK(readFile(outPath.string()).empty());
  CHECK(readFile(errPath.string()).find("[spinning-cube-demo] ERROR: missing value for --primec") !=
        std::string::npos);
}

TEST_CASE("spinning cube demo script rejects primec followed by bare dashdash") {
  std::filesystem::path scriptPath =
      std::filesystem::path("..") / "scripts" / "run_spinning_cube_demo.sh";
  if (!std::filesystem::exists(scriptPath)) {
    scriptPath = std::filesystem::current_path() / "scripts" / "run_spinning_cube_demo.sh";
  }
  REQUIRE(std::filesystem::exists(scriptPath));

  const std::filesystem::path outDir =
      testScratchPath("") / "primec_spinning_cube_demo_script_primec_followed_by_dashdash";
  std::error_code ec;
  std::filesystem::remove_all(outDir, ec);
  std::filesystem::create_directories(outDir, ec);
  REQUIRE(!ec);

  const std::filesystem::path outPath = outDir / "script.out.txt";
  const std::filesystem::path errPath = outDir / "script.err.txt";
  const std::string command =
      quoteShellArg(scriptPath.string()) + " --primec -- > " + quoteShellArg(outPath.string()) + " 2> " +
      quoteShellArg(errPath.string());
  CHECK(runCommand(command) == 2);
  CHECK(readFile(outPath.string()).empty());
  CHECK(readFile(errPath.string()).find("[spinning-cube-demo] ERROR: missing value for --primec") !=
        std::string::npos);
}

TEST_CASE("spinning cube demo script rejects missing primec binary path") {
  std::filesystem::path scriptPath =
      std::filesystem::path("..") / "scripts" / "run_spinning_cube_demo.sh";
  if (!std::filesystem::exists(scriptPath)) {
    scriptPath = std::filesystem::current_path() / "scripts" / "run_spinning_cube_demo.sh";
  }
  REQUIRE(std::filesystem::exists(scriptPath));

  const std::filesystem::path outDir =
      testScratchPath("") / "primec_spinning_cube_demo_script_missing_primec_binary";
  std::error_code ec;
  std::filesystem::remove_all(outDir, ec);
  std::filesystem::create_directories(outDir, ec);
  REQUIRE(!ec);

  const std::filesystem::path outPath = outDir / "script.out.txt";
  const std::filesystem::path errPath = outDir / "script.err.txt";
  const std::string command =
      quoteShellArg(scriptPath.string()) + " --primec /definitely/not/a/real/primec > " +
      quoteShellArg(outPath.string()) + " 2> " + quoteShellArg(errPath.string());
  CHECK(runCommand(command) == 2);
  CHECK(readFile(outPath.string()).empty());
  CHECK(readFile(errPath.string()).find("[spinning-cube-demo] ERROR: primec binary not found: /definitely/not/a/real/primec") !=
        std::string::npos);
}

TEST_CASE("spinning cube demo script rejects missing default primec path") {
  std::filesystem::path scriptPath =
      std::filesystem::path("..") / "scripts" / "run_spinning_cube_demo.sh";
  if (!std::filesystem::exists(scriptPath)) {
    scriptPath = std::filesystem::current_path() / "scripts" / "run_spinning_cube_demo.sh";
  }
  REQUIRE(std::filesystem::exists(scriptPath));

  const std::filesystem::path tempRoot =
      testScratchPath("") / "primec_spinning_cube_demo_script_default_primec_root";
  std::error_code ec;
  std::filesystem::remove_all(tempRoot, ec);
  std::filesystem::create_directories(tempRoot / "scripts", ec);
  REQUIRE(!ec);

  const std::filesystem::path isolatedScriptPath = tempRoot / "scripts" / "run_spinning_cube_demo.sh";
  {
    std::ofstream isolatedScript(isolatedScriptPath);
    isolatedScript << readFile(scriptPath.string());
  }
  REQUIRE(runCommand("chmod +x " + quoteShellArg(isolatedScriptPath.string())) == 0);

  const std::filesystem::path outDir =
      testScratchPath("") / "primec_spinning_cube_demo_script_default_primec_missing";
  std::filesystem::remove_all(outDir, ec);
  std::filesystem::create_directories(outDir, ec);
  REQUIRE(!ec);

  const std::filesystem::path outPath = outDir / "script.out.txt";
  const std::filesystem::path errPath = outDir / "script.err.txt";
  const std::string command =
      quoteShellArg(isolatedScriptPath.string()) + " --work-dir " + quoteShellArg((outDir / "work").string()) + " > " +
      quoteShellArg(outPath.string()) + " 2> " + quoteShellArg(errPath.string());
  CHECK(runCommand(command) == 2);
  CHECK(readFile(outPath.string()).empty());

  const std::string expectedError =
      "[spinning-cube-demo] ERROR: primec binary not found: " + (tempRoot / "build-debug" / "primec").string();
  CHECK(readFile(errPath.string()).find(expectedError) != std::string::npos);
}

TEST_CASE("spinning cube demo script rejects unexecutable default primec path") {
  std::filesystem::path scriptPath =
      std::filesystem::path("..") / "scripts" / "run_spinning_cube_demo.sh";
  if (!std::filesystem::exists(scriptPath)) {
    scriptPath = std::filesystem::current_path() / "scripts" / "run_spinning_cube_demo.sh";
  }
  REQUIRE(std::filesystem::exists(scriptPath));

  const std::filesystem::path tempRoot =
      testScratchPath("") / "primec_spinning_cube_demo_script_default_primec_unexecutable_root";
  std::error_code ec;
  std::filesystem::remove_all(tempRoot, ec);
  std::filesystem::create_directories(tempRoot / "scripts", ec);
  REQUIRE(!ec);

  const std::filesystem::path isolatedScriptPath = tempRoot / "scripts" / "run_spinning_cube_demo.sh";
  {
    std::ofstream isolatedScript(isolatedScriptPath);
    isolatedScript << readFile(scriptPath.string());
  }
  REQUIRE(runCommand("chmod +x " + quoteShellArg(isolatedScriptPath.string())) == 0);

  const std::filesystem::path defaultPrimecPath = tempRoot / "build-debug" / "primec";
  std::filesystem::create_directories(defaultPrimecPath.parent_path(), ec);
  REQUIRE(!ec);
  {
    std::ofstream defaultPrimec(defaultPrimecPath);
    defaultPrimec << "#!/usr/bin/env bash\n";
    defaultPrimec << "exit 0\n";
  }
  REQUIRE(runCommand("chmod 644 " + quoteShellArg(defaultPrimecPath.string())) == 0);

  const std::filesystem::path outDir =
      testScratchPath("") / "primec_spinning_cube_demo_script_default_primec_unexecutable";
  std::filesystem::remove_all(outDir, ec);
  std::filesystem::create_directories(outDir, ec);
  REQUIRE(!ec);

  const std::filesystem::path outPath = outDir / "script.out.txt";
  const std::filesystem::path errPath = outDir / "script.err.txt";
  const std::string command =
      quoteShellArg(isolatedScriptPath.string()) + " --work-dir " + quoteShellArg((outDir / "work").string()) + " > " +
      quoteShellArg(outPath.string()) + " 2> " + quoteShellArg(errPath.string());
  CHECK(runCommand(command) == 2);
  CHECK(readFile(outPath.string()).empty());

  const std::string expectedError =
      "[spinning-cube-demo] ERROR: primec binary not found: " + defaultPrimecPath.string();
  CHECK(readFile(errPath.string()).find(expectedError) != std::string::npos);
}

TEST_CASE("spinning cube demo script rejects default primec directory path") {
  std::filesystem::path scriptPath =
      std::filesystem::path("..") / "scripts" / "run_spinning_cube_demo.sh";
  if (!std::filesystem::exists(scriptPath)) {
    scriptPath = std::filesystem::current_path() / "scripts" / "run_spinning_cube_demo.sh";
  }
  REQUIRE(std::filesystem::exists(scriptPath));

  const std::filesystem::path tempRoot =
      testScratchPath("") / "primec_spinning_cube_demo_script_default_primec_dir_root";
  std::error_code ec;
  std::filesystem::remove_all(tempRoot, ec);
  std::filesystem::create_directories(tempRoot / "scripts", ec);
  REQUIRE(!ec);

  const std::filesystem::path isolatedScriptPath = tempRoot / "scripts" / "run_spinning_cube_demo.sh";
  {
    std::ofstream isolatedScript(isolatedScriptPath);
    isolatedScript << readFile(scriptPath.string());
  }
  REQUIRE(runCommand("chmod +x " + quoteShellArg(isolatedScriptPath.string())) == 0);

  const std::filesystem::path defaultPrimecPath = tempRoot / "build-debug" / "primec";
  std::filesystem::create_directories(defaultPrimecPath, ec);
  REQUIRE(!ec);

  const std::filesystem::path outDir =
      testScratchPath("") / "primec_spinning_cube_demo_script_default_primec_dir";
  std::filesystem::remove_all(outDir, ec);
  std::filesystem::create_directories(outDir, ec);
  REQUIRE(!ec);

  const std::filesystem::path outPath = outDir / "script.out.txt";
  const std::filesystem::path errPath = outDir / "script.err.txt";
  const std::string command =
      quoteShellArg(isolatedScriptPath.string()) + " --work-dir " + quoteShellArg((outDir / "work").string()) + " > " +
      quoteShellArg(outPath.string()) + " 2> " + quoteShellArg(errPath.string());
  CHECK(runCommand(command) == 2);
  CHECK(readFile(outPath.string()).empty());

  const std::string expectedError =
      "[spinning-cube-demo] ERROR: primec binary not found: " + defaultPrimecPath.string();
  CHECK(readFile(errPath.string()).find(expectedError) != std::string::npos);
}

TEST_CASE("spinning cube demo script rejects unexecutable primec path") {
  std::filesystem::path scriptPath =
      std::filesystem::path("..") / "scripts" / "run_spinning_cube_demo.sh";
  if (!std::filesystem::exists(scriptPath)) {
    scriptPath = std::filesystem::current_path() / "scripts" / "run_spinning_cube_demo.sh";
  }
  REQUIRE(std::filesystem::exists(scriptPath));

  const std::filesystem::path outDir =
      testScratchPath("") / "primec_spinning_cube_demo_script_unexecutable_primec";
  std::error_code ec;
  std::filesystem::remove_all(outDir, ec);
  std::filesystem::create_directories(outDir, ec);
  REQUIRE(!ec);

  const std::filesystem::path fakePrimecPath = outDir / "not_executable_primec";
  {
    std::ofstream fakePrimec(fakePrimecPath);
    fakePrimec << "#!/usr/bin/env bash\n";
    fakePrimec << "exit 0\n";
  }
  REQUIRE(runCommand("chmod 644 " + quoteShellArg(fakePrimecPath.string())) == 0);

  const std::filesystem::path outPath = outDir / "script.out.txt";
  const std::filesystem::path errPath = outDir / "script.err.txt";
  const std::string command =
      quoteShellArg(scriptPath.string()) + " --primec " + quoteShellArg(fakePrimecPath.string()) + " > " +
      quoteShellArg(outPath.string()) + " 2> " + quoteShellArg(errPath.string());
  CHECK(runCommand(command) == 2);
  CHECK(readFile(outPath.string()).empty());
  CHECK(readFile(errPath.string()).find("[spinning-cube-demo] ERROR: primec binary not found: " + fakePrimecPath.string()) !=
        std::string::npos);
}

TEST_CASE("spinning cube demo script rejects primec directory path") {
  std::filesystem::path scriptPath =
      std::filesystem::path("..") / "scripts" / "run_spinning_cube_demo.sh";
  if (!std::filesystem::exists(scriptPath)) {
    scriptPath = std::filesystem::current_path() / "scripts" / "run_spinning_cube_demo.sh";
  }
  REQUIRE(std::filesystem::exists(scriptPath));

  const std::filesystem::path outDir =
      testScratchPath("") / "primec_spinning_cube_demo_script_primec_dir";
  std::error_code ec;
  std::filesystem::remove_all(outDir, ec);
  std::filesystem::create_directories(outDir, ec);
  REQUIRE(!ec);

  const std::filesystem::path fakePrimecDir = outDir / "primec_dir";
  std::filesystem::create_directories(fakePrimecDir, ec);
  REQUIRE(!ec);

  const std::filesystem::path outPath = outDir / "script.out.txt";
  const std::filesystem::path errPath = outDir / "script.err.txt";
  const std::string command =
      quoteShellArg(scriptPath.string()) + " --primec " + quoteShellArg(fakePrimecDir.string()) + " > " +
      quoteShellArg(outPath.string()) + " 2> " + quoteShellArg(errPath.string());
  CHECK(runCommand(command) == 2);
  CHECK(readFile(outPath.string()).empty());
  CHECK(readFile(errPath.string()).find("[spinning-cube-demo] ERROR: primec binary not found: " + fakePrimecDir.string()) !=
        std::string::npos);
}

TEST_CASE("spinning cube demo script rejects missing work-dir value") {
  std::filesystem::path scriptPath =
      std::filesystem::path("..") / "scripts" / "run_spinning_cube_demo.sh";
  if (!std::filesystem::exists(scriptPath)) {
    scriptPath = std::filesystem::current_path() / "scripts" / "run_spinning_cube_demo.sh";
  }
  REQUIRE(std::filesystem::exists(scriptPath));

  const std::filesystem::path outDir =
      testScratchPath("") / "primec_spinning_cube_demo_script_missing_workdir_value";
  std::error_code ec;
  std::filesystem::remove_all(outDir, ec);
  std::filesystem::create_directories(outDir, ec);
  REQUIRE(!ec);

  const std::filesystem::path outPath = outDir / "script.out.txt";
  const std::filesystem::path errPath = outDir / "script.err.txt";
  const std::string command =
      quoteShellArg(scriptPath.string()) + " --primec /bin/true --work-dir > " + quoteShellArg(outPath.string()) +
      " 2> " + quoteShellArg(errPath.string());
  CHECK(runCommand(command) == 2);
  CHECK(readFile(outPath.string()).empty());
  CHECK(readFile(errPath.string()).find("[spinning-cube-demo] ERROR: missing value for --work-dir") !=
        std::string::npos);
}

TEST_CASE("spinning cube demo script rejects work-dir followed by bare dashdash") {
  std::filesystem::path scriptPath =
      std::filesystem::path("..") / "scripts" / "run_spinning_cube_demo.sh";
  if (!std::filesystem::exists(scriptPath)) {
    scriptPath = std::filesystem::current_path() / "scripts" / "run_spinning_cube_demo.sh";
  }
  REQUIRE(std::filesystem::exists(scriptPath));

  const std::filesystem::path outDir =
      testScratchPath("") / "primec_spinning_cube_demo_script_workdir_followed_by_dashdash";
  std::error_code ec;
  std::filesystem::remove_all(outDir, ec);
  std::filesystem::create_directories(outDir, ec);
  REQUIRE(!ec);

  const std::filesystem::path outPath = outDir / "script.out.txt";
  const std::filesystem::path errPath = outDir / "script.err.txt";
  const std::string command = quoteShellArg(scriptPath.string()) + " --primec /bin/true --work-dir -- > " +
                              quoteShellArg(outPath.string()) + " 2> " + quoteShellArg(errPath.string());
  CHECK(runCommand(command) == 2);
  CHECK(readFile(outPath.string()).empty());
  CHECK(readFile(errPath.string()).find("[spinning-cube-demo] ERROR: missing value for --work-dir") !=
        std::string::npos);
}

TEST_CASE("spinning cube demo script rejects work-dir followed by flag") {
  std::filesystem::path scriptPath =
      std::filesystem::path("..") / "scripts" / "run_spinning_cube_demo.sh";
  if (!std::filesystem::exists(scriptPath)) {
    scriptPath = std::filesystem::current_path() / "scripts" / "run_spinning_cube_demo.sh";
  }
  REQUIRE(std::filesystem::exists(scriptPath));

  const std::filesystem::path outDir =
      testScratchPath("") / "primec_spinning_cube_demo_script_workdir_followed_by_flag";
  std::error_code ec;
  std::filesystem::remove_all(outDir, ec);
  std::filesystem::create_directories(outDir, ec);
  REQUIRE(!ec);

  const std::filesystem::path outPath = outDir / "script.out.txt";
  const std::filesystem::path errPath = outDir / "script.err.txt";
  const std::string command = quoteShellArg(scriptPath.string()) +
                              " --primec /bin/true --work-dir --port-base 18870 > " +
                              quoteShellArg(outPath.string()) + " 2> " + quoteShellArg(errPath.string());
  CHECK(runCommand(command) == 2);
  CHECK(readFile(outPath.string()).empty());
  CHECK(readFile(errPath.string()).find("[spinning-cube-demo] ERROR: missing value for --work-dir") !=
        std::string::npos);
}

TEST_CASE("spinning cube demo script rejects port-base followed by flag") {
  std::filesystem::path scriptPath =
      std::filesystem::path("..") / "scripts" / "run_spinning_cube_demo.sh";
  if (!std::filesystem::exists(scriptPath)) {
    scriptPath = std::filesystem::current_path() / "scripts" / "run_spinning_cube_demo.sh";
  }
  REQUIRE(std::filesystem::exists(scriptPath));

  const std::filesystem::path outDir =
      testScratchPath("") / "primec_spinning_cube_demo_script_port_base_followed_by_flag";
  std::error_code ec;
  std::filesystem::remove_all(outDir, ec);
  std::filesystem::create_directories(outDir, ec);
  REQUIRE(!ec);

  const std::filesystem::path outPath = outDir / "script.out.txt";
  const std::filesystem::path errPath = outDir / "script.err.txt";
  const std::string command = quoteShellArg(scriptPath.string()) +
                              " --primec /bin/true --port-base --work-dir /tmp > " +
                              quoteShellArg(outPath.string()) + " 2> " + quoteShellArg(errPath.string());
  CHECK(runCommand(command) == 2);
  CHECK(readFile(outPath.string()).empty());
  CHECK(readFile(errPath.string()).find("[spinning-cube-demo] ERROR: missing value for --port-base") !=
        std::string::npos);
}

TEST_CASE("spinning cube demo script rejects port-base followed by bare dashdash") {
  std::filesystem::path scriptPath =
      std::filesystem::path("..") / "scripts" / "run_spinning_cube_demo.sh";
  if (!std::filesystem::exists(scriptPath)) {
    scriptPath = std::filesystem::current_path() / "scripts" / "run_spinning_cube_demo.sh";
  }
  REQUIRE(std::filesystem::exists(scriptPath));

  const std::filesystem::path outDir =
      testScratchPath("") / "primec_spinning_cube_demo_script_port_base_followed_by_dashdash";
  std::error_code ec;
  std::filesystem::remove_all(outDir, ec);
  std::filesystem::create_directories(outDir, ec);
  REQUIRE(!ec);

  const std::filesystem::path outPath = outDir / "script.out.txt";
  const std::filesystem::path errPath = outDir / "script.err.txt";
  const std::string command = quoteShellArg(scriptPath.string()) + " --primec /bin/true --port-base -- > " +
                              quoteShellArg(outPath.string()) + " 2> " + quoteShellArg(errPath.string());
  CHECK(runCommand(command) == 2);
  CHECK(readFile(outPath.string()).empty());
  CHECK(readFile(errPath.string()).find("[spinning-cube-demo] ERROR: missing value for --port-base") !=
        std::string::npos);
}

TEST_CASE("spinning cube demo script rejects unknown arg") {
  std::filesystem::path scriptPath =
      std::filesystem::path("..") / "scripts" / "run_spinning_cube_demo.sh";
  if (!std::filesystem::exists(scriptPath)) {
    scriptPath = std::filesystem::current_path() / "scripts" / "run_spinning_cube_demo.sh";
  }
  REQUIRE(std::filesystem::exists(scriptPath));

  const std::filesystem::path outDir =
      testScratchPath("") / "primec_spinning_cube_demo_script_unknown_arg";
  std::error_code ec;
  std::filesystem::remove_all(outDir, ec);
  std::filesystem::create_directories(outDir, ec);
  REQUIRE(!ec);

  const std::filesystem::path outPath = outDir / "script.out.txt";
  const std::filesystem::path errPath = outDir / "script.err.txt";
  const std::string command =
      quoteShellArg(scriptPath.string()) + " --primec /bin/true --bogus > " + quoteShellArg(outPath.string()) +
      " 2> " + quoteShellArg(errPath.string());
  CHECK(runCommand(command) == 2);
  CHECK(readFile(outPath.string()).empty());
  CHECK(readFile(errPath.string()).find("[spinning-cube-demo] ERROR: unknown arg: --bogus") != std::string::npos);
}

TEST_CASE("spinning cube demo script rejects unknown arg as first token") {
  std::filesystem::path scriptPath =
      std::filesystem::path("..") / "scripts" / "run_spinning_cube_demo.sh";
  if (!std::filesystem::exists(scriptPath)) {
    scriptPath = std::filesystem::current_path() / "scripts" / "run_spinning_cube_demo.sh";
  }
  REQUIRE(std::filesystem::exists(scriptPath));

  const std::filesystem::path outDir =
      testScratchPath("") / "primec_spinning_cube_demo_script_unknown_arg_first";
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
  CHECK(readFile(errPath.string()).find("[spinning-cube-demo] ERROR: unknown arg: --bogus") != std::string::npos);
}

TEST_CASE("spinning cube demo script rejects unknown positional token") {
  std::filesystem::path scriptPath =
      std::filesystem::path("..") / "scripts" / "run_spinning_cube_demo.sh";
  if (!std::filesystem::exists(scriptPath)) {
    scriptPath = std::filesystem::current_path() / "scripts" / "run_spinning_cube_demo.sh";
  }
  REQUIRE(std::filesystem::exists(scriptPath));

  const std::filesystem::path outDir =
      testScratchPath("") / "primec_spinning_cube_demo_script_unknown_positional";
  std::error_code ec;
  std::filesystem::remove_all(outDir, ec);
  std::filesystem::create_directories(outDir, ec);
  REQUIRE(!ec);

  const std::filesystem::path outPath = outDir / "script.out.txt";
  const std::filesystem::path errPath = outDir / "script.err.txt";
  const std::string command =
      quoteShellArg(scriptPath.string()) + " foo > " + quoteShellArg(outPath.string()) + " 2> " +
      quoteShellArg(errPath.string());
  CHECK(runCommand(command) == 2);
  CHECK(readFile(outPath.string()).empty());
  CHECK(readFile(errPath.string()).find("[spinning-cube-demo] ERROR: unknown arg: foo") != std::string::npos);
}

TEST_CASE("spinning cube demo script rejects bare dashdash token") {
  std::filesystem::path scriptPath =
      std::filesystem::path("..") / "scripts" / "run_spinning_cube_demo.sh";
  if (!std::filesystem::exists(scriptPath)) {
    scriptPath = std::filesystem::current_path() / "scripts" / "run_spinning_cube_demo.sh";
  }
  REQUIRE(std::filesystem::exists(scriptPath));

  const std::filesystem::path outDir =
      testScratchPath("") / "primec_spinning_cube_demo_script_unknown_dashdash";
  std::error_code ec;
  std::filesystem::remove_all(outDir, ec);
  std::filesystem::create_directories(outDir, ec);
  REQUIRE(!ec);

  const std::filesystem::path outPath = outDir / "script.out.txt";
  const std::filesystem::path errPath = outDir / "script.err.txt";
  const std::string command =
      quoteShellArg(scriptPath.string()) + " -- > " + quoteShellArg(outPath.string()) + " 2> " +
      quoteShellArg(errPath.string());
  CHECK(runCommand(command) == 2);
  CHECK(readFile(outPath.string()).empty());
  CHECK(readFile(errPath.string()).find("[spinning-cube-demo] ERROR: unknown arg: --") != std::string::npos);
}


TEST_SUITE_END();
