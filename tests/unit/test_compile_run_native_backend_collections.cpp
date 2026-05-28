#include "test_compile_run_helpers.h"
#include "test_compile_run_native_backend_collections_helpers.h"

TEST_SUITE_BEGIN("primestruct.compile.run.native_backend.collections");

TEST_CASE("compiles and runs native array slice count and indexed access") {
  const std::string source = R"(
[return<int>]
main() {
  [array<i32>] values{array<i32>(4i32, 7i32, 9i32, 11i32)}
  [array<i32>] window{slice(values, 1i32, 3i32)}
  return(plus(count(window), window[1i32]))
}
)";
  const std::string srcPath = writeTemp("compile_native_array_slice.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_array_slice_exe").string();
  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 11);
}

TEST_SUITE_END();
