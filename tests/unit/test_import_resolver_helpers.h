#pragma once

#include "primec/ImportResolver.h"
#include "primec/ProcessRunner.h"
#include "primec/testing/TestScratch.h"

#include "third_party/doctest.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <string>
#include <vector>

namespace {
std::filesystem::path importResolverPath(std::string_view relativePath) {
  return primec::testing::testScratchPath("imports_resolver/" + std::string(relativePath));
}

[[maybe_unused]] std::string writeTemp(const std::string &name, const std::string &contents) {
  const auto path = importResolverPath(name);
  std::ofstream file(path);
  CHECK(file.good());
  file << contents;
  CHECK(file.good());
  return path.string();
}

std::string writeFile(const std::filesystem::path &path, const std::string &contents) {
  std::filesystem::create_directories(path.parent_path());
  std::ofstream file(path);
  CHECK(file.good());
  file << contents;
  CHECK(file.good());
  return path.string();
}

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
