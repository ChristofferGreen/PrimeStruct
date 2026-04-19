#include "test_compile_run_helpers.h"

#include "test_compile_run_examples_helpers.h"

#include <array>

namespace {

struct ScriptRunResult {
  int exitCode = -1;
  std::string stdoutText;
  std::string stderrText;
};

enum class InvalidPrimecPathMode {
  Missing,
  UnexecutableFile,
  Directory,
};

struct ArgumentErrorScenario {
  std::string_view label;
  std::string_view outputDirName;
  std::string_view args;
  std::string_view expectedError;
};

std::filesystem::path resolveSpinningCubeDemoScriptPath() {
  std::filesystem::path scriptPath =
      std::filesystem::path("..") / "scripts" / "run_spinning_cube_demo.sh";
  if (!std::filesystem::exists(scriptPath)) {
    scriptPath =
        std::filesystem::current_path() / "scripts" / "run_spinning_cube_demo.sh";
  }
  REQUIRE(std::filesystem::exists(scriptPath));
  return scriptPath;
}

std::filesystem::path recreateScratchDir(std::string_view name) {
  const std::filesystem::path dir = testScratchPath("") / std::string(name);
  std::error_code ec;
  std::filesystem::remove_all(dir, ec);
  std::filesystem::create_directories(dir, ec);
  REQUIRE(!ec);
  return dir;
}

void writeShellStub(const std::filesystem::path &path, std::string_view mode) {
  std::error_code ec;
  std::filesystem::create_directories(path.parent_path(), ec);
  REQUIRE(!ec);

  std::ofstream file(path);
  REQUIRE(file.good());
  file << "#!/usr/bin/env bash\n";
  file << "exit 0\n";
  REQUIRE(file.good());

  REQUIRE(runCommand("chmod " + std::string(mode) + " " +
                     quoteShellArg(path.string())) == 0);
}

std::filesystem::path createInvalidPrimecPath(std::string_view fixtureDirName,
                                              std::string_view leafName,
                                              InvalidPrimecPathMode mode) {
  const std::filesystem::path fixtureDir = recreateScratchDir(fixtureDirName);
  const std::filesystem::path primecPath = fixtureDir / leafName;

  switch (mode) {
  case InvalidPrimecPathMode::Missing:
    break;
  case InvalidPrimecPathMode::UnexecutableFile:
    writeShellStub(primecPath, "644");
    break;
  case InvalidPrimecPathMode::Directory: {
    std::error_code ec;
    std::filesystem::create_directories(primecPath, ec);
    REQUIRE(!ec);
    break;
  }
  }

  return primecPath;
}

struct IsolatedDemoScript {
  std::filesystem::path rootDir;
  std::filesystem::path scriptPath;
  std::filesystem::path defaultPrimecPath() const {
    return rootDir / "build-debug" / "primec";
  }
};

IsolatedDemoScript createIsolatedDemoScript(std::string_view fixtureDirName) {
  const std::filesystem::path sourceScriptPath = resolveSpinningCubeDemoScriptPath();
  const std::filesystem::path rootDir = recreateScratchDir(fixtureDirName);

  std::error_code ec;
  std::filesystem::create_directories(rootDir / "scripts", ec);
  REQUIRE(!ec);

  const std::filesystem::path isolatedScriptPath =
      rootDir / "scripts" / "run_spinning_cube_demo.sh";
  std::ofstream isolatedScript(isolatedScriptPath);
  REQUIRE(isolatedScript.good());
  isolatedScript << readFile(sourceScriptPath.string());
  REQUIRE(isolatedScript.good());

  REQUIRE(runCommand("chmod +x " + quoteShellArg(isolatedScriptPath.string())) == 0);
  return {rootDir, isolatedScriptPath};
}

ScriptRunResult runSpinningCubeDemoScript(const std::filesystem::path &scriptPath,
                                          std::string_view outputDirName,
                                          const std::string &args) {
  const std::filesystem::path outDir = recreateScratchDir(outputDirName);
  const std::filesystem::path outPath = outDir / "script.out.txt";
  const std::filesystem::path errPath = outDir / "script.err.txt";

  std::string command = quoteShellArg(scriptPath.string());
  if (!args.empty()) {
    command += " " + args;
  }
  command += " > " + quoteShellArg(outPath.string()) + " 2> " +
             quoteShellArg(errPath.string());

  return {
      runCommand(command),
      readFile(outPath.string()),
      readFile(errPath.string()),
  };
}

void expectScriptArgumentError(const std::filesystem::path &scriptPath,
                               std::string_view outputDirName,
                               const std::string &args,
                               const std::string &expectedError) {
  const ScriptRunResult result =
      runSpinningCubeDemoScript(scriptPath, outputDirName, args);
  CHECK(result.exitCode == 2);
  CHECK(result.stdoutText.empty());
  CHECK(result.stderrText.find(expectedError) != std::string::npos);
}

} // namespace

TEST_SUITE_BEGIN("primestruct.compile.run.examples");

TEST_CASE("spinning cube demo script rejects invalid port-base values") {
  const std::filesystem::path scriptPath = resolveSpinningCubeDemoScriptPath();
  const std::array<ArgumentErrorScenario, 3> scenarios{{
      {"non-integer",
       "primec_spinning_cube_demo_script_invalid_port",
       "--primec /bin/true --port-base not-a-number",
       "[spinning-cube-demo] ERROR: --port-base must be an integer: not-a-number"},
      {"high out-of-range",
       "primec_spinning_cube_demo_script_invalid_port_range",
       "--primec /bin/true --port-base 70000",
       "[spinning-cube-demo] ERROR: --port-base out of range (1-65535): 70000"},
      {"zero",
       "primec_spinning_cube_demo_script_zero_port",
       "--primec /bin/true --port-base 0",
       "[spinning-cube-demo] ERROR: --port-base out of range (1-65535): 0"},
  }};

  for (const auto &scenario : scenarios) {
    INFO("scenario=" << scenario.label << " args=" << scenario.args
                      << " expectedError=" << scenario.expectedError);
    expectScriptArgumentError(scriptPath, scenario.outputDirName,
                              std::string(scenario.args),
                              std::string(scenario.expectedError));
  }
}

TEST_CASE("spinning cube demo script rejects missing primec values") {
  const std::filesystem::path scriptPath = resolveSpinningCubeDemoScriptPath();
  const std::array<ArgumentErrorScenario, 3> scenarios{{
      {"missing value",
       "primec_spinning_cube_demo_script_missing_primec_value",
       "--primec",
       "[spinning-cube-demo] ERROR: missing value for --primec"},
      {"followed by flag",
       "primec_spinning_cube_demo_script_primec_followed_by_flag",
       "--primec --work-dir /tmp",
       "[spinning-cube-demo] ERROR: missing value for --primec"},
      {"followed by bare dashdash",
       "primec_spinning_cube_demo_script_primec_followed_by_dashdash",
       "--primec --",
       "[spinning-cube-demo] ERROR: missing value for --primec"},
  }};

  for (const auto &scenario : scenarios) {
    INFO("scenario=" << scenario.label << " args=" << scenario.args
                      << " expectedError=" << scenario.expectedError);
    expectScriptArgumentError(scriptPath, scenario.outputDirName,
                              std::string(scenario.args),
                              std::string(scenario.expectedError));
  }
}

TEST_CASE("spinning cube demo script rejects missing work-dir values") {
  const std::filesystem::path scriptPath = resolveSpinningCubeDemoScriptPath();
  const std::array<ArgumentErrorScenario, 3> scenarios{{
      {"missing value",
       "primec_spinning_cube_demo_script_missing_workdir_value",
       "--primec /bin/true --work-dir",
       "[spinning-cube-demo] ERROR: missing value for --work-dir"},
      {"followed by bare dashdash",
       "primec_spinning_cube_demo_script_workdir_followed_by_dashdash",
       "--primec /bin/true --work-dir --",
       "[spinning-cube-demo] ERROR: missing value for --work-dir"},
      {"followed by flag",
       "primec_spinning_cube_demo_script_workdir_followed_by_flag",
       "--primec /bin/true --work-dir --port-base 18870",
       "[spinning-cube-demo] ERROR: missing value for --work-dir"},
  }};

  for (const auto &scenario : scenarios) {
    INFO("scenario=" << scenario.label << " args=" << scenario.args
                      << " expectedError=" << scenario.expectedError);
    expectScriptArgumentError(scriptPath, scenario.outputDirName,
                              std::string(scenario.args),
                              std::string(scenario.expectedError));
  }
}

TEST_CASE("spinning cube demo script rejects missing port-base values") {
  const std::filesystem::path scriptPath = resolveSpinningCubeDemoScriptPath();
  const std::array<ArgumentErrorScenario, 3> scenarios{{
      {"missing value",
       "primec_spinning_cube_demo_script_missing_port_value",
       "--primec /bin/true --port-base",
       "[spinning-cube-demo] ERROR: missing value for --port-base"},
      {"followed by flag",
       "primec_spinning_cube_demo_script_port_base_followed_by_flag",
       "--primec /bin/true --port-base --work-dir /tmp",
       "[spinning-cube-demo] ERROR: missing value for --port-base"},
      {"followed by bare dashdash",
       "primec_spinning_cube_demo_script_port_base_followed_by_dashdash",
       "--primec /bin/true --port-base --",
       "[spinning-cube-demo] ERROR: missing value for --port-base"},
  }};

  for (const auto &scenario : scenarios) {
    INFO("scenario=" << scenario.label << " args=" << scenario.args
                      << " expectedError=" << scenario.expectedError);
    expectScriptArgumentError(scriptPath, scenario.outputDirName,
                              std::string(scenario.args),
                              std::string(scenario.expectedError));
  }
}

TEST_CASE("spinning cube demo script rejects explicit primec paths that are not executable files") {
  const std::filesystem::path scriptPath = resolveSpinningCubeDemoScriptPath();

  struct Scenario {
    std::string_view label;
    std::string_view fixtureDirName;
    std::string_view outputDirName;
    InvalidPrimecPathMode mode;
  };

  const std::array<Scenario, 3> scenarios{{
      {"missing file",
       "primec_spinning_cube_demo_script_missing_primec_fixture",
       "primec_spinning_cube_demo_script_missing_primec_binary",
       InvalidPrimecPathMode::Missing},
      {"unexecutable file",
       "primec_spinning_cube_demo_script_unexecutable_primec_fixture",
       "primec_spinning_cube_demo_script_unexecutable_primec",
       InvalidPrimecPathMode::UnexecutableFile},
      {"directory path",
       "primec_spinning_cube_demo_script_primec_dir_fixture",
       "primec_spinning_cube_demo_script_primec_dir",
       InvalidPrimecPathMode::Directory},
  }};

  for (const auto &scenario : scenarios) {
    const std::filesystem::path primecPath =
        createInvalidPrimecPath(scenario.fixtureDirName, "primec_candidate",
                                scenario.mode);
    const std::string expectedError =
        "[spinning-cube-demo] ERROR: primec binary not found: " +
        primecPath.string();
    const std::string args = "--primec " + quoteShellArg(primecPath.string());

    INFO("scenario=" << scenario.label << " args=" << args
                      << " expectedError=" << expectedError);
    expectScriptArgumentError(scriptPath, scenario.outputDirName, args,
                              expectedError);
  }
}

TEST_CASE("spinning cube demo script rejects invalid default primec paths") {
  struct Scenario {
    std::string_view label;
    std::string_view fixtureDirName;
    std::string_view outputDirName;
    InvalidPrimecPathMode mode;
  };

  const std::array<Scenario, 3> scenarios{{
      {"missing file",
       "primec_spinning_cube_demo_script_default_primec_root",
       "primec_spinning_cube_demo_script_default_primec_missing",
       InvalidPrimecPathMode::Missing},
      {"unexecutable file",
       "primec_spinning_cube_demo_script_default_primec_unexecutable_root",
       "primec_spinning_cube_demo_script_default_primec_unexecutable",
       InvalidPrimecPathMode::UnexecutableFile},
      {"directory path",
       "primec_spinning_cube_demo_script_default_primec_dir_root",
       "primec_spinning_cube_demo_script_default_primec_dir",
       InvalidPrimecPathMode::Directory},
  }};

  for (const auto &scenario : scenarios) {
    const IsolatedDemoScript isolatedScript =
        createIsolatedDemoScript(scenario.fixtureDirName);
    const std::filesystem::path defaultPrimecPath =
        isolatedScript.defaultPrimecPath();

    switch (scenario.mode) {
    case InvalidPrimecPathMode::Missing:
      break;
    case InvalidPrimecPathMode::UnexecutableFile:
      writeShellStub(defaultPrimecPath, "644");
      break;
    case InvalidPrimecPathMode::Directory: {
      std::error_code ec;
      std::filesystem::create_directories(defaultPrimecPath, ec);
      REQUIRE(!ec);
      break;
    }
    }

    const std::string expectedError =
        "[spinning-cube-demo] ERROR: primec binary not found: " +
        defaultPrimecPath.string();
    const std::string args =
        "--work-dir " + quoteShellArg((isolatedScript.rootDir / "work").string());

    INFO("scenario=" << scenario.label << " args=" << args
                      << " expectedError=" << expectedError);
    expectScriptArgumentError(isolatedScript.scriptPath, scenario.outputDirName,
                              args, expectedError);
  }
}

TEST_CASE("spinning cube demo script rejects unknown tokens") {
  const std::filesystem::path scriptPath = resolveSpinningCubeDemoScriptPath();
  const std::array<ArgumentErrorScenario, 4> scenarios{{
      {"unknown flag after valid option",
       "primec_spinning_cube_demo_script_unknown_arg",
       "--primec /bin/true --bogus",
       "[spinning-cube-demo] ERROR: unknown arg: --bogus"},
      {"unknown flag as first token",
       "primec_spinning_cube_demo_script_unknown_arg_first",
       "--bogus",
       "[spinning-cube-demo] ERROR: unknown arg: --bogus"},
      {"unknown positional token",
       "primec_spinning_cube_demo_script_unknown_positional",
       "foo",
       "[spinning-cube-demo] ERROR: unknown arg: foo"},
      {"bare dashdash token",
       "primec_spinning_cube_demo_script_unknown_dashdash",
       "--",
       "[spinning-cube-demo] ERROR: unknown arg: --"},
  }};

  for (const auto &scenario : scenarios) {
    INFO("scenario=" << scenario.label << " args=" << scenario.args
                      << " expectedError=" << scenario.expectedError);
    expectScriptArgumentError(scriptPath, scenario.outputDirName,
                              std::string(scenario.args),
                              std::string(scenario.expectedError));
  }
}

TEST_SUITE_END();
