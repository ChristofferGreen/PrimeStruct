#include "test_compile_run_helpers.h"

#include "test_compile_run_native_backend_collections_helpers.h"

#include <filesystem>

#if PRIMESTRUCT_NATIVE_COLLECTIONS_ENABLED
TEST_SUITE_BEGIN("primestruct.compile.run.native_backend.collections");

namespace {
std::filesystem::path map2RepoRoot() {
  std::filesystem::path cwd = std::filesystem::current_path();
  if (std::filesystem::exists(cwd / "stdlib")) {
    return cwd;
  }
  if (std::filesystem::exists(cwd.parent_path() / "stdlib")) {
    return cwd.parent_path();
  }
  return cwd;
}

std::string map2PrimecCompileCommand(const std::string &srcPath,
                                     const std::string &exePath,
                                     const std::string &outPath) {
  const std::filesystem::path repoRoot = map2RepoRoot();
  const std::filesystem::path primecPath =
      std::filesystem::exists(repoRoot / "build-release/primec")
          ? repoRoot / "build-release/primec"
          : repoRoot / "primec";
  return "cd " + quoteShellArg(repoRoot.string()) + " && " +
         quoteShellArg(primecPath.string()) + " --emit=native " +
         quoteShellArg(srcPath) + " -o " + quoteShellArg(exePath) +
         " --entry /main > " + quoteShellArg(outPath) + " 2>&1";
}
} // namespace

TEST_CASE("compiles and runs isolated map2 string key helpers") {
  const std::string source = R"(
import /std/collections/map2/*

[effects(heap_alloc), return<int>]
main() {
  [Map2<string, i32> mut] values{map2<string, i32>("left"raw_utf8, 4i32, "right"raw_utf8, 7i32)}
  /std/collections/map2/map2Insert<string, i32>(values, "left"raw_utf8, 9i32)
  [i32] c{/std/collections/map2/map2Count<string, i32>(values)}
  [i32] left{/std/collections/map2/map2Get<string, i32>(values, "left"raw_utf8)}
  [i32] right{/std/collections/map2/map2GetUnsafe<string, i32>(values, "right"raw_utf8)}
  [i32 mut] total{plus(c, plus(left, right))}
  if(/std/collections/map2/map2Contains<string, i32>(values, "missing"raw_utf8),
     then() { assign(total, 99i32) },
     else() { })
  return(total)
}
)";
  const std::string srcPath = writeTemp("compile_native_map2_isolated_string_keys.prime", source);
  const std::string outPath =
      (testScratchPath("") / "primec_native_map2_isolated_string_keys_out.txt").string();
  const std::string exePath =
      (testScratchPath("") / "primec_native_map2_isolated_string_keys_exe").string();

  const std::string compileCmd = map2PrimecCompileCommand(srcPath, exePath, outPath);
  CHECK(runCommand(compileCmd) == 0);
  CHECK(readFile(outPath).empty());
  CHECK(runCommand(exePath) == 18);
}

TEST_CASE("compiles and runs isolated map2 try-get missing key path") {
  const std::string source = R"(
import /std/collections/map2/*

[effects(io_out, heap_alloc), return<Result<i32, /std/collections/ContainerError>>]
main() {
  [Map2<i32, i32>] values{map2<i32, i32>(1i32, 4i32, 2i32, 7i32)}
  [i32] found{/std/collections/map2/map2Get<i32, i32>(values, 2i32)}
  [Result<i32, /std/collections/ContainerError>] missing{/std/collections/map2/map2TryGet<i32, i32>(values, 9i32)}
  print_line(Result.why(missing))
  return(Result.ok(plus(found, 20i32)))
}
)";
  const std::string srcPath = writeTemp("compile_native_map2_isolated_try_get.prime", source);
  const std::string outPath =
      (testScratchPath("") / "primec_native_map2_isolated_try_get_out.txt").string();
  const std::string exePath =
      (testScratchPath("") / "primec_native_map2_isolated_try_get_exe").string();
  const std::string runOutPath =
      (testScratchPath("") / "primec_native_map2_isolated_try_get_run.txt").string();

  const std::string compileCmd = map2PrimecCompileCommand(srcPath, exePath, outPath);
  CHECK(runCommand(compileCmd) == 0);
  CHECK(readFile(outPath).empty());
  CHECK(runCommand(exePath + " > " + runOutPath) == 27);
  CHECK(readFile(runOutPath).find("container missing key") != std::string::npos);
}

TEST_CASE("map2 stdlib source stays isolated from legacy map implementation") {
  const std::filesystem::path sourcePath =
      map2RepoRoot() / "stdlib/std/collections/map2.prime";
  const std::string source = readFile(sourcePath.string());

  CHECK(source.find("internal_map") == std::string::npos);
  CHECK(source.find("experimental_map") == std::string::npos);
  CHECK(source.find("/std/collections/map/") == std::string::npos);
  CHECK(source.find("/map/") == std::string::npos);
  CHECK(source.find("Map__") == std::string::npos);
  CHECK(source.find("CollectionsMap") == std::string::npos);
}

TEST_SUITE_END();
#endif
