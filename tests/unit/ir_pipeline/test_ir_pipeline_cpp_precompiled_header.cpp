#include "third_party/doctest.h"

#include "primec/backend/IrToOptCppEmitter.h"
#include "primec/support/CppPrecompiledHeader.h"
#include "primec/support/ExternalTooling.h"
#include "primec/support/ProcessRunner.h"
#include "primec/testing/TestScratch.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace {

// Stands in for clang++: `-dM -E` writes the identity it is given to the `-o` file, `-x c++-header` writes the
// bytes of a precompiled header, and a compile is answered by `compileResult`.
class FakeCompiler final : public primec::ProcessRunner {
public:
  std::string identity = "#define __clang_version__ \"fake 1\"\n";
  bool failHeaderBuild = false;
  std::function<int(const std::vector<std::string> &)> compileResult = [](const std::vector<std::string> &) {
    return 0;
  };
  mutable int headerBuilds = 0;
  mutable std::vector<std::string> headerSources;
  mutable std::vector<std::vector<std::string>> compiles;

  int run(const std::vector<std::string> &args) const override {
    const auto has = [&](const std::string &flag) {
      for (const std::string &arg : args) {
        if (arg == flag) {
          return true;
        }
      }
      return false;
    };
    const std::string &out = args[args.size() >= 2 && args[args.size() - 2] == "-o" ? args.size() - 1 : 0];
    if (has("-dM")) {
      std::ofstream(args[args.size() - 1], std::ios::binary) << identity;
      return 0;
    }
    if (has("c++-header")) {
      ++headerBuilds;
      if (args.size() >= 3) {
        headerSources.push_back(args[args.size() - 3]);
      }
      if (failHeaderBuild) {
        return 1;
      }
      std::ofstream(out, std::ios::binary) << "precompiled";
      return 0;
    }
    compiles.push_back(args);
    return compileResult(args);
  }
};

// The system compiler, with every command and its exit code kept.
class RecordingCompiler final : public primec::ProcessRunner {
public:
  mutable std::vector<std::pair<std::vector<std::string>, int>> commands;

  int run(const std::vector<std::string> &args) const override {
    const int code = primec::systemProcessRunner().run(args);
    commands.emplace_back(args, code);
    return code;
  }
};

std::string readText(const std::filesystem::path &path) {
  std::ifstream file(path, std::ios::binary);
  std::ostringstream buffer;
  buffer << file.rdbuf();
  return buffer.str();
}

std::filesystem::path pchScratch(const std::string &name) {
  const std::filesystem::path dir = primec::testing::testScratchPath("pch/" + name);
  std::error_code ec;
  std::filesystem::remove_all(dir, ec);
  std::filesystem::create_directories(dir);
  return dir;
}

size_t fileCount(const std::filesystem::path &dir) {
  size_t count = 0;
  for (const auto &entry : std::filesystem::directory_iterator(dir)) {
    (void)entry;
    ++count;
  }
  return count;
}

const std::vector<std::string> Flags = {"-std=c++23", "-O2", "-ffp-contract=off", "-pthread"};

} // namespace

TEST_SUITE_BEGIN("primestruct.ir.pipeline.validation");

TEST_CASE("precompiled header key changes with the compiler, the flags and the header text") {
  const std::string base = primec::cppPrecompiledHeaderKey("identity", Flags, "#include <vector>\n");
  CHECK(base == primec::cppPrecompiledHeaderKey("identity", Flags, "#include <vector>\n"));
  CHECK(base != primec::cppPrecompiledHeaderKey("other identity", Flags, "#include <vector>\n"));
  CHECK(base != primec::cppPrecompiledHeaderKey("identity", {"-std=c++23", "-O3", "-ffp-contract=off", "-pthread"},
                                                "#include <vector>\n"));
  CHECK(base != primec::cppPrecompiledHeaderKey("identity", Flags, "#include <map>\n"));
  // The pieces are length-prefixed, so moving text between them is a different key.
  CHECK(primec::cppPrecompiledHeaderKey("ab", {"c"}, "d") != primec::cppPrecompiledHeaderKey("a", {"bc"}, "d"));
}

TEST_CASE("precompiled header is built once and reused") {
  const std::filesystem::path cache = pchScratch("reuse");
  FakeCompiler compiler;
  const std::filesystem::path first = primec::ensureCppPrecompiledHeader(compiler, cache, Flags, "#include <map>\n");
  REQUIRE_FALSE(first.empty());
  CHECK(std::filesystem::exists(first));
  CHECK(compiler.headerBuilds == 1);
  const std::filesystem::path second = primec::ensureCppPrecompiledHeader(compiler, cache, Flags, "#include <map>\n");
  CHECK(second == first);
  CHECK(compiler.headerBuilds == 1);
  // Only the precompiled header and the header text it names are left behind: no probe or temporary files.
  CHECK(fileCount(cache) == 2);
}

TEST_CASE("the header a precompiled header is built from stays next to it under a stable name") {
  const std::filesystem::path cache = pchScratch("source");
  FakeCompiler compiler;
  const std::filesystem::path pch =
      primec::ensureCppPrecompiledHeader(compiler, cache, Flags, "#include <map>\n");
  REQUIRE_FALSE(pch.empty());
  std::filesystem::path header = pch;
  header.replace_extension(".h");
  REQUIRE(compiler.headerSources.size() == 1);
  CHECK(compiler.headerSources.front() == header.string());
  CHECK(readText(header) == "#include <map>\n");

  // The compiler refuses a precompiled header whose source is gone or changed, so neither is reused.
  std::filesystem::remove(header);
  CHECK(primec::ensureCppPrecompiledHeader(compiler, cache, Flags, "#include <map>\n") == pch);
  CHECK(compiler.headerBuilds == 2);
  CHECK(readText(header) == "#include <map>\n");
  std::ofstream(header, std::ios::binary | std::ios::trunc) << "#include <set>\n";
  CHECK(primec::ensureCppPrecompiledHeader(compiler, cache, Flags, "#include <map>\n") == pch);
  CHECK(compiler.headerBuilds == 3);
  CHECK(readText(header) == "#include <map>\n");
  CHECK(primec::ensureCppPrecompiledHeader(compiler, cache, Flags, "#include <map>\n") == pch);
  CHECK(compiler.headerBuilds == 3);
  CHECK(fileCount(cache) == 2);
}

TEST_CASE("the system compiler accepts the precompiled header it built") {
  if (!primec::processSpawningAvailable() ||
      primec::systemProcessRunner().run({"clang++", "--version"}) != 0) {
    return;
  }
  const std::filesystem::path cache = pchScratch("accepted");
  const std::filesystem::path source = cache / "program.cpp";
  std::ofstream(source, std::ios::binary)
      << "int main() { std::vector<int> values; return int(values.size()); }\n";
  RecordingCompiler compiler;
  const std::filesystem::path pch =
      primec::ensureCppPrecompiledHeader(compiler, cache / "cache", Flags, "#include <vector>\n");
  REQUIRE_FALSE(pch.empty());
  // The program names `std::vector` without an include, so it only compiles when the header is taken.
  std::vector<std::string> command = {"clang++"};
  command.insert(command.end(), Flags.begin(), Flags.end());
  command.insert(
      command.end(),
      {"-include-pch", pch.string(), source.string(), "-o", (cache / "program").string()});
  CHECK(compiler.run(command) == 0);
  // A second process finds the header and the compiler still takes it.
  CHECK(primec::ensureCppPrecompiledHeader(
            compiler, cache / "cache", Flags, "#include <vector>\n") == pch);
  CHECK(compiler.run(command) == 0);
}

TEST_CASE("the optimized compile of a program runs once with the precompiled header of the runtime "
          "headers") {
  if (!primec::processSpawningAvailable() ||
      primec::systemProcessRunner().run({"clang++", "--version"}) != 0) {
    return;
  }
  const char *disabled = std::getenv("PRIMEC_OPTEXE_PCH");
  if (disabled != nullptr && std::string(disabled) == "0") {
    return;
  }
  const std::filesystem::path dir = pchScratch("once");
  const std::filesystem::path source = dir / "program.cpp";
  std::ofstream(source, std::ios::binary) << "int main() { return 0; }\n";
  for (int round = 0; round < 2; ++round) {
    RecordingCompiler compiler;
    CHECK(primec::compileCppExecutableOptimizedWithHeaderCache(
        compiler, source, dir / "program", 2, dir / "cache"));
    size_t compiles = 0;
    size_t withHeader = 0;
    for (const auto &command : compiler.commands) {
      bool probeOrHeader = false;
      bool usesHeader = false;
      for (const std::string &arg : command.first) {
        probeOrHeader = probeOrHeader || arg == "-dM" || arg == "c++-header";
        usesHeader = usesHeader || arg == "-include-pch";
      }
      if (!probeOrHeader) {
        ++compiles;
        withHeader += usesHeader ? 1 : 0;
        CHECK(command.second == 0);
      }
    }
    // One compile, and it took the header: a refused header shows as a second compile without it.
    CHECK(compiles == 1);
    CHECK(withHeader == 1);
  }
}

TEST_CASE("a changed compiler, flags or header text never reuses a precompiled header") {
  const std::filesystem::path cache = pchScratch("stale");
  FakeCompiler compiler;
  const std::filesystem::path first = primec::ensureCppPrecompiledHeader(compiler, cache, Flags, "#include <map>\n");
  compiler.identity = "#define __clang_version__ \"fake 2\"\n";
  const std::filesystem::path upgraded = primec::ensureCppPrecompiledHeader(compiler, cache, Flags, "#include <map>\n");
  CHECK(upgraded != first);
  CHECK(compiler.headerBuilds == 2);
  const std::filesystem::path otherFlags =
      primec::ensureCppPrecompiledHeader(compiler, cache, {"-std=c++23", "-O3"}, "#include <map>\n");
  CHECK(otherFlags != upgraded);
  CHECK(compiler.headerBuilds == 3);
  const std::filesystem::path otherText = primec::ensureCppPrecompiledHeader(compiler, cache, Flags, "#include <set>\n");
  CHECK(otherText != upgraded);
  CHECK(compiler.headerBuilds == 4);
  // The compiler that built the first one is back: its header is found, not rebuilt.
  compiler.identity = "#define __clang_version__ \"fake 1\"\n";
  CHECK(primec::ensureCppPrecompiledHeader(compiler, cache, Flags, "#include <map>\n") == first);
  CHECK(compiler.headerBuilds == 4);
}

TEST_CASE("an empty precompiled header file is rebuilt") {
  const std::filesystem::path cache = pchScratch("empty");
  FakeCompiler compiler;
  const std::filesystem::path first = primec::ensureCppPrecompiledHeader(compiler, cache, Flags, "#include <map>\n");
  REQUIRE_FALSE(first.empty());
  std::ofstream(first, std::ios::binary | std::ios::trunc).close();
  const std::filesystem::path again = primec::ensureCppPrecompiledHeader(compiler, cache, Flags, "#include <map>\n");
  CHECK(again == first);
  CHECK(compiler.headerBuilds == 2);
  CHECK(std::filesystem::file_size(again) > 0);
}

TEST_CASE(
    "a failed precompiled header build leaves only the header text behind and returns no path") {
  const std::filesystem::path cache = pchScratch("failed");
  FakeCompiler compiler;
  compiler.failHeaderBuild = true;
  CHECK(primec::ensureCppPrecompiledHeader(compiler, cache, Flags, "#include <map>\n").empty());
  // Only the header text stays: a concurrent compile may be building from it.
  CHECK(fileCount(cache) == 1);
}

TEST_CASE("link arguments that can change how headers are read refuse the precompiled header") {
  CHECK(primec::linkArgsAllowPrecompiledHeader({}));
  CHECK(primec::linkArgsAllowPrecompiledHeader({"-lm", "-L/opt/lib", "-Wl,-rpath,/opt/lib", "libhost.a", "host.o"}));
  CHECK(primec::linkArgsAllowPrecompiledHeader({"-framework", "CoreFoundation", "-rpath", "/opt/lib"}));
  CHECK_FALSE(primec::linkArgsAllowPrecompiledHeader({"-DHOST=1"}));
  CHECK_FALSE(primec::linkArgsAllowPrecompiledHeader({"-I/opt/include"}));
  CHECK_FALSE(primec::linkArgsAllowPrecompiledHeader({"-std=c++20"}));
  CHECK_FALSE(primec::linkArgsAllowPrecompiledHeader({"host.c"}));
  CHECK_FALSE(primec::linkArgsAllowPrecompiledHeader({"-framework"}));
}

TEST_CASE("the optimized compile of a program uses a precompiled header of the runtime headers") {
  const std::filesystem::path cache = pchScratch("compile");
  FakeCompiler compiler;
  CHECK(primec::compileCppExecutableOptimizedWithHeaderCache(
      compiler, "pch_source.cpp", "pch_program", 2, cache));
  REQUIRE(compiler.compiles.size() == 1);
  const std::vector<std::string> &command = compiler.compiles.front();
  CHECK(command[0] == "clang++");
  size_t pchIndex = command.size();
  for (size_t i = 0; i < command.size(); ++i) {
    if (command[i] == "-include-pch") {
      pchIndex = i;
    }
  }
  REQUIRE(pchIndex + 1 < command.size());
  const std::filesystem::path pch = command[pchIndex + 1];
  CHECK(std::filesystem::exists(pch));
  CHECK(pch.parent_path() == cache);
  CHECK(pch.filename().string().starts_with("pch-"));
  REQUIRE(compiler.headerSources.size() == 1);
  CHECK(readText(compiler.headerSources.front()) == primec::optCppRuntimeHeaders());
}

TEST_CASE("the optimized compile of a program without a header cache uses no precompiled header") {
  FakeCompiler compiler;
  CHECK(primec::compileCppExecutableOptimizedWithHeaderCache(
      compiler, "pch_source.cpp", "pch_program", 2, {}));
  REQUIRE(compiler.compiles.size() == 1);
  CHECK(compiler.headerBuilds == 0);
  for (const std::string &arg : compiler.compiles.front()) {
    CHECK(arg != "-include-pch");
  }
}

TEST_CASE(
    "a compile that fails with the precompiled header is repeated without it and the header goes") {
  const std::filesystem::path cache = pchScratch("refused");
  FakeCompiler compiler;
  compiler.compileResult = [](const std::vector<std::string> &args) {
    for (const std::string &arg : args) {
      if (arg == "-include-pch") {
        return 1;
      }
    }
    return 0;
  };
  CHECK(primec::compileCppExecutableOptimizedWithHeaderCache(
      compiler, "pch_source.cpp", "pch_program", 2, cache));
  REQUIRE(compiler.compiles.size() == 2);
  for (const std::string &arg : compiler.compiles.back()) {
    CHECK(arg != "-include-pch");
  }
  // The refused header is not kept, so the next compile builds it again instead of being refused again.
  for (const std::string &arg : compiler.compiles.front()) {
    if (arg.ends_with(".pch")) {
      CHECK_FALSE(std::filesystem::exists(arg));
    }
  }
  CHECK(primec::compileCppExecutableOptimizedWithHeaderCache(
      compiler, "pch_source.cpp", "pch_program", 2, cache));
  CHECK(compiler.headerBuilds == 2);
}

TEST_CASE("a program that does not compile keeps the precompiled header") {
  const std::filesystem::path cache = pchScratch("broken_program");
  FakeCompiler compiler;
  compiler.compileResult = [](const std::vector<std::string> &) { return 1; };
  CHECK_FALSE(primec::compileCppExecutableOptimizedWithHeaderCache(
      compiler, "pch_source.cpp", "pch_program", 2, cache));
  CHECK_FALSE(primec::compileCppExecutableOptimizedWithHeaderCache(
      compiler, "pch_source.cpp", "pch_program", 2, cache));
  CHECK(compiler.headerBuilds == 1);
}

TEST_CASE("the runtime headers of a generated program start the precompiled header text") {
  const std::string_view headers = primec::optCppRuntimeHeaders();
  CHECK(headers.starts_with("#include <algorithm>"));
}

TEST_SUITE_END();
