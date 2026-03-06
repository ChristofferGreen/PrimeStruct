#include "primec/ExternalTooling.h"
#include "primec/ProcessRunner.h"

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
  RecordingProcessRunner runner([](const std::vector<std::string> &) { return 0; });
  CHECK(primec::compileCppExecutable(runner, "/tmp/source.cpp", "/tmp/program"));
  REQUIRE(runner.commands.size() == 1);
  CHECK(runner.commands.front() ==
        std::vector<std::string>{"clang++", "-std=c++23", "-O2", "/tmp/source.cpp", "-o", "/tmp/program"});
}

TEST_SUITE_END();
