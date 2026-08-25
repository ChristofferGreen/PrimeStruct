#include "../test_compile_run_helpers.h"

#include "../test_compile_run_collection_conformance_helpers.h"
#include "../test_compile_run_container_error_conformance_helpers.h"
#include "../test_compile_run_checked_pointer_conformance_helpers.h"
#include "../test_compile_run_unchecked_pointer_conformance_helpers.h"
#include "test_compile_run_native_backend_collections_helpers.h"

#if PRIMESTRUCT_NATIVE_COLLECTIONS_ENABLED
TEST_SUITE_BEGIN("primestruct.compile.run.native_backend.collections");

TEST_CASE("rejects native templated stdlib vector wrapper temporary call index mismatch") {
  const std::string source = R"(
import /std/collections/*

[return<vector<T>>]
wrapVector<T>([T] value) {
  return(/std/collections/vector/vector<T>(value))
}

[return<int>]
main() {
  return(/std/collections/vector/at<i32>(wrapVector<i32>(4i32), true))
}
)";
  const std::string srcPath =
      writeTemp("compile_native_stdlib_collection_shim_templated_return_vector_temp_call_index_mismatch.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_stdlib_collection_shim_templated_return_vector_temp_call_index_mismatch")
          .string();
  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 3);
}

TEST_SUITE_END();
#endif
