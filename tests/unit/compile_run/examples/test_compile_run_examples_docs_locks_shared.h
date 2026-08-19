#pragma once

#include "../test_compile_run_helpers.h"

#include "test_compile_run_examples_helpers.h"

#include <algorithm>
#include <sstream>
#include <vector>

static inline std::filesystem::path resolveUnitTestsPath() {
  std::filesystem::path testsPath = std::filesystem::path("..") / "tests" / "unit";
  if (!std::filesystem::exists(testsPath)) {
    testsPath = std::filesystem::current_path() / "tests" / "unit";
  }
  return testsPath;
}

static inline std::filesystem::path resolveRepoPath(const std::filesystem::path &relativePath) {
  std::filesystem::path path = std::filesystem::path("..") / relativePath;
  if (!std::filesystem::exists(path)) {
    path = std::filesystem::current_path() / relativePath;
  }
  return path;
}

static inline std::string readRepoShardsConcat(const std::filesystem::path &directory,
                                        const std::string &filenamePrefix) {
  std::vector<std::filesystem::path> matches;
  for (const auto &entry : std::filesystem::directory_iterator(directory)) {
    if (!entry.is_regular_file()) {
      continue;
    }
    const std::string name = entry.path().filename().string();
    if (name.rfind(filenamePrefix, 0) == 0) {
      matches.push_back(entry.path());
    }
  }
  REQUIRE(!matches.empty());
  std::sort(matches.begin(), matches.end());
  std::string combined;
  for (const auto &path : matches) {
    combined += readFile(path.string());
  }
  return combined;
}

static inline std::vector<std::filesystem::path> filesWithRetainedDoctestSkips(
    const std::filesystem::path &testsPath) {
  std::vector<std::filesystem::path> paths;
  for (const auto &entry : std::filesystem::recursive_directory_iterator(testsPath)) {
    if (!entry.is_regular_file() ||
        entry.path().filename().string().rfind("test_compile_run_examples_docs_locks", 0) == 0) {
      continue;
    }
    const std::string source = readFile(entry.path().string());
    if (source.find("doctest::skip(true)") != std::string::npos) {
      paths.push_back(entry.path());
    }
  }
  std::sort(paths.begin(), paths.end());
  return paths;
}

static inline std::vector<std::string> productionFilesContainingAny(
    const std::filesystem::path &repoRoot,
    const std::vector<std::string> &needles) {
  std::vector<std::string> paths;
  for (const char *dirname : {"src", "include"}) {
    const std::filesystem::path root = repoRoot / dirname;
    if (!std::filesystem::exists(root)) {
      continue;
    }
    for (const auto &entry : std::filesystem::recursive_directory_iterator(root)) {
      if (!entry.is_regular_file()) {
        continue;
      }
      const std::string ext = entry.path().extension().string();
      if (ext != ".cpp" && ext != ".h" && ext != ".hpp") {
        continue;
      }
      const std::string source = readFile(entry.path().string());
      for (const std::string &needle : needles) {
        if (source.find(needle) != std::string::npos) {
          paths.push_back(entry.path().lexically_relative(repoRoot).generic_string());
          break;
        }
      }
    }
  }
  std::sort(paths.begin(), paths.end());
  return paths;
}

static inline std::string currentTestCaseNameFromLine(const std::string &line,
                                               const std::string &currentName) {
  const std::string prefix = "TEST_CASE(\"";
  const std::size_t start = line.find(prefix);
  if (start == std::string::npos) {
    return currentName;
  }
  const std::size_t nameStart = start + prefix.size();
  const std::size_t nameEnd = line.find('"', nameStart);
  if (nameEnd == std::string::npos) {
    return currentName;
  }
  return line.substr(nameStart, nameEnd - nameStart);
}

static inline bool isDirectOldSoaImportLine(const std::string &line) {
  const std::size_t first = line.find_first_not_of(" \t");
  if (first == std::string::npos) {
    return false;
  }
  const std::string trimmed = line.substr(first);
  return trimmed.rfind("import /std/collections/experimental_soa", 0) == 0 ||
         trimmed.rfind("import /std/collections/soa_vector", 0) == 0;
}

static inline bool isExplicitSoaRejectionFixtureName(const std::string &testName) {
  for (const char *marker :
       {"reject", "rejection", "legacy", "old-surface", "builtin", "substrate"}) {
    if (testName.find(marker) != std::string::npos) {
      return true;
    }
  }
  return false;
}

static inline std::vector<std::string> directOldSoaImportFixtureViolations(
    const std::filesystem::path &testsPath) {
  std::vector<std::string> violations;
  for (const auto &entry : std::filesystem::recursive_directory_iterator(testsPath)) {
    if (!entry.is_regular_file()) {
      continue;
    }
    const std::string ext = entry.path().extension().string();
    if (ext != ".cpp" && ext != ".h") {
      continue;
    }
    std::istringstream stream(readFile(entry.path().string()));
    std::string line;
    std::string currentTestName;
    int lineNumber = 0;
    while (std::getline(stream, line)) {
      ++lineNumber;
      currentTestName = currentTestCaseNameFromLine(line, currentTestName);
      if (!isDirectOldSoaImportLine(line) ||
          isExplicitSoaRejectionFixtureName(currentTestName)) {
        continue;
      }
      violations.push_back(entry.path().lexically_relative(testsPath).generic_string() + ":" +
                           std::to_string(lineNumber) + " in " + currentTestName);
    }
  }
  std::sort(violations.begin(), violations.end());
  return violations;
}
