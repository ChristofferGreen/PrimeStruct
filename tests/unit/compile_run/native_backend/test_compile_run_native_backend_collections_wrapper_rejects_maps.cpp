#include "../test_compile_run_helpers.h"

#include "../test_compile_run_collection_conformance_helpers.h"
#include "../test_compile_run_container_error_conformance_helpers.h"
#include "../test_compile_run_checked_pointer_conformance_helpers.h"
#include "../test_compile_run_unchecked_pointer_conformance_helpers.h"
#include "test_compile_run_native_backend_collections_helpers.h"

#if PRIMESTRUCT_NATIVE_COLLECTIONS_ENABLED
TEST_SUITE_BEGIN("primestruct.compile.run.native_backend.collections");

TEST_CASE("rejects native templated stdlib collection return envelope unsupported arg") {
  const std::string source = R"(
import /std/collections/*

[return<vector<Unknown>>]
wrapUnknown([i32] value) {
  return(/std/collections/vector/vector<i32>(value))
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{wrapUnknown(3i32)}
  return(/std/collections/vector/count<i32>(values))
}
)";
  const std::string srcPath = writeTemp("compile_native_stdlib_collection_shim_templated_return_bad_arg.prime", source);
  const std::string errPath = (testScratchPath("") /
                               "primec_native_stdlib_collection_shim_templated_return_bad_arg_err.txt")
                                  .string();
  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("native backend does not support return type on /wrapUnknown") !=
        std::string::npos);
}

TEST_CASE("rejects native templated stdlib map wrapper temporary call key mismatch") {
  const std::string source = R"(
import /std/collections/*

[return<map<K, V>>]
wrapMap<K, V>([K] key, [V] value) {
  return(mapSingle<K, V>(key, value))
}

[return<int>]
main() {
  return(/std/collections/map/at<string, i32>(wrapMap<string, i32>("only"raw_utf8, 4i32), 1i32))
}
)";
  const std::string srcPath =
      writeTemp("compile_native_stdlib_collection_shim_templated_return_temp_call_key_mismatch.prime", source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_native_stdlib_collection_shim_templated_return_temp_call_key_mismatch.err")
          .string();
  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o /dev/null --entry /main > " +
                                 quoteShellArg(errPath) + " 2>&1";
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("argument type mismatch for /std/collections/map/at__") !=
        std::string::npos);
}

TEST_CASE("rejects native templated stdlib map wrapper temporary unsafe call key mismatch") {
  const std::string source = R"(
import /std/collections/*

[return<map<K, V>>]
wrapMap<K, V>([K] key, [V] value) {
  return(mapSingle<K, V>(key, value))
}

[return<int>]
main() {
  return(/std/collections/map/at_unsafe<string, i32>(wrapMap<string, i32>("only"raw_utf8, 4i32), 1i32))
}
)";
  const std::string srcPath =
      writeTemp("compile_native_stdlib_collection_shim_templated_return_temp_unsafe_call_key_mismatch.prime", source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_native_stdlib_collection_shim_templated_return_temp_unsafe_call_key_mismatch.err")
          .string();
  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o /dev/null --entry /main > " +
                                 quoteShellArg(errPath) + " 2>&1";
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("argument type mismatch for /std/collections/map/at_unsafe__") !=
        std::string::npos);
}

TEST_SUITE_END();
#endif
