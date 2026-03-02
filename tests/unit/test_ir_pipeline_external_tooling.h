#include "primec/ExternalTooling.h"
#include "primec/ProcessRunner.h"

#include <functional>
#include <vector>

namespace {

class RecordingProcessRunner final : public primec::ProcessRunner {
public:
  explicit RecordingProcessRunner(std::function<int(const std::string &)> handler = {})
      : handler_(std::move(handler)) {}

  int run(const std::string &command) const override {
    commands.push_back(command);
    if (handler_) {
      return handler_(command);
    }
    return 1;
  }

  mutable std::vector<std::string> commands;

private:
  std::function<int(const std::string &)> handler_;
};

} // namespace

TEST_SUITE_BEGIN("primestruct.ir.pipeline.validation");

TEST_CASE("external tooling prefers glslang validator when present") {
  RecordingProcessRunner runner([](const std::string &command) {
    if (command.find("glslangValidator -v") == 0) {
      return 0;
    }
    return 1;
  });

  std::string toolName;
  CHECK(primec::findSpirvCompiler(runner, toolName));
  CHECK(toolName == "glslangValidator");
  REQUIRE(runner.commands.size() == 1);
  CHECK(runner.commands.front().find("glslangValidator -v") == 0);
}

TEST_CASE("external tooling falls back to glslc when needed") {
  RecordingProcessRunner runner([](const std::string &command) {
    if (command.find("glslangValidator -v") == 0) {
      return 1;
    }
    if (command.find("glslc --version") == 0) {
      return 0;
    }
    return 1;
  });

  std::string toolName;
  CHECK(primec::findSpirvCompiler(runner, toolName));
  CHECK(toolName == "glslc");
  REQUIRE(runner.commands.size() == 2);
  CHECK(runner.commands[0].find("glslangValidator -v") == 0);
  CHECK(runner.commands[1].find("glslc --version") == 0);
}

TEST_CASE("external tooling rejects unknown spirv compiler names") {
  RecordingProcessRunner runner;
  CHECK_FALSE(primec::compileSpirv(runner, "unknown-tool", "in.comp", "out.spv"));
  CHECK(runner.commands.empty());
}

TEST_CASE("external tooling uses injected runner for cpp compile command") {
  RecordingProcessRunner runner([](const std::string &) { return 0; });
  CHECK(primec::compileCppExecutable(runner, "/tmp/source.cpp", "/tmp/program"));
  REQUIRE(runner.commands.size() == 1);
  CHECK(runner.commands.front().find("clang++ -std=c++23 -O2 ") == 0);
  CHECK(runner.commands.front().find("\"/tmp/source.cpp\"") != std::string::npos);
  CHECK(runner.commands.front().find("\"/tmp/program\"") != std::string::npos);
}

TEST_SUITE_END();
