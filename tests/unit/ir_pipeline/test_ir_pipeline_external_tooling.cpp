#include "third_party/doctest.h"

#include "primec/support/ExternalTooling.h"
#include "primec/support/ProcessRunner.h"

#include <filesystem>
#include <functional>
#include <vector>

namespace {

class RecordingProcessRunner final : public primec::ProcessRunner {
public:
  explicit RecordingProcessRunner(std::function<int(const std::vector<std::string> &)> handler = {})
      : handler_(std::move(handler)) {}

  int run(const std::vector<std::string> &args) const override {
    commands.push_back(args);
    if (handler_) {
      return handler_(args);
    }
    return 1;
  }

  mutable std::vector<std::vector<std::string>> commands;

private:
  std::function<int(const std::vector<std::string> &)> handler_;
};

} // namespace

TEST_SUITE_BEGIN("primestruct.ir.pipeline.validation");

TEST_CASE("external tooling prefers glslang validator when present") {
  RecordingProcessRunner runner([](const std::vector<std::string> &args) {
    if (args.size() == 2 && args[0] == "glslangValidator" && args[1] == "-v") {
      return 0;
    }
    return 1;
  });

  std::string toolName;
  CHECK(primec::findSpirvCompiler(runner, toolName));
  CHECK(toolName == "glslangValidator");
  REQUIRE(runner.commands.size() == 1);
  CHECK(runner.commands.front() == std::vector<std::string>{"glslangValidator", "-v"});
}

TEST_CASE("external tooling falls back to glslc when needed") {
  RecordingProcessRunner runner([](const std::vector<std::string> &args) {
    if (args.size() == 2 && args[0] == "glslangValidator" && args[1] == "-v") {
      return 1;
    }
    if (args.size() == 2 && args[0] == "glslc" && args[1] == "--version") {
      return 0;
    }
    return 1;
  });

  std::string toolName;
  CHECK(primec::findSpirvCompiler(runner, toolName));
  CHECK(toolName == "glslc");
  REQUIRE(runner.commands.size() == 2);
  CHECK(runner.commands[0] == std::vector<std::string>{"glslangValidator", "-v"});
  CHECK(runner.commands[1] == std::vector<std::string>{"glslc", "--version"});
}

TEST_CASE("external tooling rejects unknown spirv compiler names") {
  RecordingProcessRunner runner;
  CHECK_FALSE(primec::compileSpirv(runner, "unknown-tool", "in.comp", "out.spv"));
  CHECK(runner.commands.empty());
}

TEST_CASE("external tooling uses argv process call for glslang spirv compile") {
  RecordingProcessRunner runner([](const std::vector<std::string> &) { return 0; });
  CHECK(primec::compileSpirv(runner, "glslangValidator", "/tmp/in.comp", "/tmp/out.spv"));
  REQUIRE(runner.commands.size() == 1);
  CHECK(runner.commands.front() ==
        std::vector<std::string>{"glslangValidator", "-V", "-S", "comp", "/tmp/in.comp", "-o", "/tmp/out.spv"});
}

TEST_CASE("external tooling uses argv process call for glslc spirv compile") {
  RecordingProcessRunner runner([](const std::vector<std::string> &) { return 0; });
  CHECK(primec::compileSpirv(runner, "glslc", "/tmp/in.comp", "/tmp/out.spv"));
  REQUIRE(runner.commands.size() == 1);
  CHECK(runner.commands.front() ==
        std::vector<std::string>{"glslc", "-fshader-stage=compute", "/tmp/in.comp", "-o", "/tmp/out.spv"});
}

TEST_CASE("external tooling uses injected runner for cpp compile command") {
  // compileCppExecutable conditionally splices an extra "-include-pch
  // <path>" pair into the base command when
  // PRIMEC_GENERATED_CPP_PCH_PATH is defined at compile time (clang++ was
  // found when configuring the build) AND the precompiled header actually
  // exists on disk at test-run time (see ExternalTooling.cpp and
  // docs/todo.md TODO-4736) - this is documented, intended behavior, not a
  // bug. The PCH's absolute path is build-directory-specific (it's not
  // visible to this test target's own compile definitions), so assert on
  // the command's *shape* rather than hardcoding a path: the fixed
  // leading/trailing args must be present verbatim, and if an
  // "-include-pch" flag appears between them it must be followed by a path
  // to a file that actually exists.
  RecordingProcessRunner runner([](const std::vector<std::string> &) { return 0; });
  CHECK(primec::compileCppExecutable(runner, "/tmp/source.cpp", "/tmp/program"));
  REQUIRE(runner.commands.size() == 1);
  const std::vector<std::string> &command = runner.commands.front();
  REQUIRE(command.size() >= 6);
  CHECK(command[0] == "clang++");
  CHECK(command[1] == "-std=c++23");
  CHECK(command[2] == "-O0");
  size_t nextIndex = 3;
  if (nextIndex < command.size() && command[nextIndex] == "-include-pch") {
    REQUIRE(command.size() >= nextIndex + 2);
    CHECK(std::filesystem::exists(command[nextIndex + 1]));
    nextIndex += 2;
  }
  REQUIRE(command.size() == nextIndex + 3);
  CHECK(command[nextIndex] == "/tmp/source.cpp");
  CHECK(command[nextIndex + 1] == "-o");
  CHECK(command[nextIndex + 2] == "/tmp/program");
}

TEST_SUITE_END();
