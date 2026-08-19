#include "third_party/doctest.h"

#include "test_stdlib_map_ownership_shared.h"

TEST_SUITE_BEGIN("primestruct.stdlib.map_ownership");

TEST_CASE("experimental map production traces are classified as backing substrate") {
  std::vector<std::string> unclassified;
  for (const auto &file : productionMapTraceFiles()) {
    std::ifstream input(file);
    REQUIRE(input.is_open());
    const std::string relativePath = relativePathString(file);
    std::string line;
    size_t lineNumber = 0;
    while (std::getline(input, line)) {
      ++lineNumber;
      if (line.find("experimental_map") == std::string::npos) {
        continue;
      }
      if (!isAllowedExperimentalMapTrace(relativePath, line)) {
        unclassified.push_back(relativePath + ":" + std::to_string(lineNumber) +
                               ": " + line);
      }
    }
  }

  INFO("unclassified production experimental_map traces:");
  for (const auto &trace : unclassified) {
    INFO(trace);
  }
  CHECK(unclassified.empty());
}

TEST_SUITE_END();
