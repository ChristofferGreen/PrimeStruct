#include "test_compile_run_helpers.h"

#include "test_compile_run_collection_conformance_helpers.h"
#include "test_compile_run_container_error_conformance_helpers.h"
#include "test_compile_run_checked_pointer_conformance_helpers.h"
#include "test_compile_run_unchecked_pointer_conformance_helpers.h"
#include "test_compile_run_native_backend_collections_helpers.h"

#if PRIMESTRUCT_NATIVE_COLLECTIONS_ENABLED
TEST_SUITE_BEGIN("primestruct.compile.run.native_backend.collections");

TEST_CASE("compiles and runs native stdlib collection shim map at") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
main() {
  [map<i32, i32>] values{mapTriple<i32, i32>(1i32, 10i32, 2i32, 20i32, 3i32, 30i32)}
  return(plus(mapAt<i32, i32>(values, 2i32), mapCount<i32, i32>(values)))
}
)";
  const std::string srcPath = writeTemp("compile_native_stdlib_collection_shim_map_at.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_stdlib_collection_shim_map_at_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 23);
}

TEST_CASE("compiles and runs native stdlib collection shim map at string keys") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
main() {
  [map<string, i32>] values{mapDouble<string, i32>("left"raw_utf8, 10i32, "right"raw_utf8, 15i32)}
  return(plus(mapAt<string, i32>(values, "right"raw_utf8), mapCount<string, i32>(values)))
}
)";
  const std::string srcPath = writeTemp("compile_native_stdlib_collection_shim_map_at_string_key.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_stdlib_collection_shim_map_at_string_key_exe")
          .string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 17);
}

TEST_CASE("rejects native stdlib collection shim map at type mismatch") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
main() {
  [map<i32, i32>] values{mapTriple<i32, i32>(1i32, 10i32, 2i32, 20i32, 3i32, 30i32)}
  return(mapAt<bool, i32>(values, true))
}
)";
  const std::string srcPath = writeTemp("compile_native_stdlib_collection_shim_map_at_mismatch.prime", source);
  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o /dev/null --entry /main";
  CHECK(runCommand(compileCmd) == 2);
}

TEST_CASE("rejects native stdlib collection shim map at string key type mismatch") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
main() {
  [map<string, i32>] values{mapDouble<string, i32>("left"raw_utf8, 10i32, "right"raw_utf8, 15i32)}
  return(mapAt<string, i32>(values, 1i32))
}
)";
  const std::string srcPath =
      writeTemp("compile_native_stdlib_collection_shim_map_at_string_key_mismatch.prime", source);
  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o /dev/null --entry /main";
  CHECK(runCommand(compileCmd) == 2);
}

TEST_CASE("compiles and runs native stdlib collection shim map at unsafe") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
main() {
  [map<i32, i32>] values{mapTriple<i32, i32>(1i32, 10i32, 2i32, 20i32, 3i32, 30i32)}
  return(plus(mapAtUnsafe<i32, i32>(values, 3i32), mapCount<i32, i32>(values)))
}
)";
  const std::string srcPath = writeTemp("compile_native_stdlib_collection_shim_map_at_unsafe.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_stdlib_collection_shim_map_at_unsafe_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 33);
}

TEST_CASE("compiles and runs native stdlib collection shim map at unsafe string keys") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
main() {
  [map<string, i32>] values{mapDouble<string, i32>("left"raw_utf8, 10i32, "right"raw_utf8, 15i32)}
  return(plus(mapAtUnsafe<string, i32>(values, "left"raw_utf8), mapCount<string, i32>(values)))
}
)";
  const std::string srcPath =
      writeTemp("compile_native_stdlib_collection_shim_map_at_unsafe_string_key.prime", source);
  const std::string exePath = (testScratchPath("") /
                               "primec_native_stdlib_collection_shim_map_at_unsafe_string_key_exe")
                                  .string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 12);
}

TEST_CASE("rejects native stdlib collection shim map at unsafe type mismatch") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
main() {
  [map<i32, i32>] values{mapTriple<i32, i32>(1i32, 10i32, 2i32, 20i32, 3i32, 30i32)}
  return(mapAtUnsafe<bool, i32>(values, true))
}
)";
  const std::string srcPath = writeTemp("compile_native_stdlib_collection_shim_map_at_unsafe_mismatch.prime", source);
  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o /dev/null --entry /main";
  CHECK(runCommand(compileCmd) == 2);
}

TEST_CASE("rejects native stdlib collection shim map at unsafe string key type mismatch") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
main() {
  [map<string, i32>] values{mapDouble<string, i32>("left"raw_utf8, 10i32, "right"raw_utf8, 15i32)}
  return(mapAtUnsafe<string, i32>(values, 1i32))
}
)";
  const std::string srcPath =
      writeTemp("compile_native_stdlib_collection_shim_map_at_unsafe_string_key_mismatch.prime", source);
  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o /dev/null --entry /main";
  CHECK(runCommand(compileCmd) == 2);
}

TEST_CASE("compiles and runs native stdlib collection shim map method access string keys") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
main() {
  [map<string, i32>] values{mapDouble<string, i32>("left"raw_utf8, 10i32, "right"raw_utf8, 15i32)}
  return(plus(plus(values.at("right"raw_utf8), values.at_unsafe("left"raw_utf8)), values.count()))
}
)";
  const std::string srcPath =
      writeTemp("compile_native_stdlib_collection_shim_map_method_access_string_key.prime", source);
  const std::string exePath = (testScratchPath("") /
                               "primec_native_stdlib_collection_shim_map_method_access_string_key_exe")
                                  .string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 27);
}

TEST_CASE("rejects native stdlib collection shim map method at string key type mismatch") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
main() {
  [map<string, i32>] values{mapDouble<string, i32>("left"raw_utf8, 10i32, "right"raw_utf8, 15i32)}
  return(values.at(1i32))
}
)";
  const std::string srcPath =
      writeTemp("compile_native_stdlib_collection_shim_map_method_at_string_key_mismatch.prime", source);
  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o /dev/null --entry /main";
  CHECK(runCommand(compileCmd) == 2);
}

TEST_CASE("rejects native stdlib collection shim map method at unsafe string key type mismatch") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
main() {
  [map<string, i32>] values{mapDouble<string, i32>("left"raw_utf8, 10i32, "right"raw_utf8, 15i32)}
  return(values.at_unsafe(1i32))
}
)";
  const std::string srcPath =
      writeTemp("compile_native_stdlib_collection_shim_map_method_at_unsafe_string_key_mismatch.prime", source);
  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o /dev/null --entry /main";
  CHECK(runCommand(compileCmd) == 2);
}

TEST_CASE("compiles and runs native stdlib collection shim map method call parity string keys") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
main() {
  [map<string, i32>] viaCall{mapDouble<string, i32>("left"raw_utf8, 10i32, "right"raw_utf8, 15i32)}
  [map<string, i32>] viaMethod{mapDouble<string, i32>("left"raw_utf8, 10i32, "right"raw_utf8, 15i32)}
  return(plus(
      plus(mapAt<string, i32>(viaCall, "right"raw_utf8), viaMethod.at("right"raw_utf8)),
      plus(mapAtUnsafe<string, i32>(viaCall, "left"raw_utf8),
          plus(viaMethod.at_unsafe("left"raw_utf8), plus(mapCount<string, i32>(viaCall), viaMethod.count())))))
}
)";
  const std::string srcPath =
      writeTemp("compile_native_stdlib_collection_shim_map_method_call_parity_string_key.prime", source);
  const std::string exePath = (testScratchPath("") /
                               "primec_native_stdlib_collection_shim_map_method_call_parity_string_key_exe")
                                  .string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 54);
}

TEST_CASE("rejects native stdlib collection shim map method call parity key type mismatch") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
main() {
  [map<string, i32>] values{mapDouble<string, i32>("left"raw_utf8, 10i32, "right"raw_utf8, 15i32)}
  return(plus(mapAt<string, i32>(values, 1i32), values.at(1i32)))
}
)";
  const std::string srcPath =
      writeTemp("compile_native_stdlib_collection_shim_map_method_call_parity_key_type_mismatch.prime", source);
  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o /dev/null --entry /main";
  CHECK(runCommand(compileCmd) == 2);
}

TEST_CASE("rejects native stdlib collection shim map method call parity unsafe key type mismatch") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
main() {
  [map<string, i32>] values{mapDouble<string, i32>("left"raw_utf8, 10i32, "right"raw_utf8, 15i32)}
  return(plus(mapAtUnsafe<string, i32>(values, 1i32), values.at_unsafe(1i32)))
}
)";
  const std::string srcPath = writeTemp(
      "compile_native_stdlib_collection_shim_map_method_call_parity_unsafe_key_type_mismatch.prime", source);
  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o /dev/null --entry /main";
  CHECK(runCommand(compileCmd) == 2);
}

TEST_CASE("compiles and runs native stdlib collection shim map single standalone string keys") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
main() {
  [map<string, i32>] values{mapSingle<string, i32>("only"raw_utf8, 21i32)}
  return(plus(plus(mapAt<string, i32>(values, "only"raw_utf8), mapAtUnsafe<string, i32>(values, "only"raw_utf8)),
      mapCount<string, i32>(values)))
}
)";
  const std::string srcPath =
      writeTemp("compile_native_stdlib_collection_shim_map_single_standalone_string_key.prime", source);
  const std::string exePath = (testScratchPath("") /
                               "primec_native_stdlib_collection_shim_map_single_standalone_string_key_exe")
                                  .string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 43);
}

TEST_CASE("rejects native stdlib collection shim map single standalone key type mismatch") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
main() {
  [map<i32, i32>] values{mapSingle<i32, i32>("oops"raw_utf8, 4i32)}
  return(mapCount<i32, i32>(values))
}
)";
  const std::string srcPath =
      writeTemp("compile_native_stdlib_collection_shim_map_single_standalone_key_mismatch.prime", source);
  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o /dev/null --entry /main";
  CHECK(runCommand(compileCmd) == 2);
}

TEST_CASE("compiles and runs native stdlib collection shim map pair standalone") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
main() {
  [map<i32, i32>] values{mapPair<i32, i32>(1i32, 7i32, 2i32, 10i32)}
  return(plus(plus(mapAt<i32, i32>(values, 2i32), mapAtUnsafe<i32, i32>(values, 1i32)), mapCount<i32, i32>(values)))
}
)";
  const std::string srcPath =
      writeTemp("compile_native_stdlib_collection_shim_map_pair_standalone.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_stdlib_collection_shim_map_pair_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 19);
}

TEST_CASE("rejects native stdlib collection shim map pair standalone type mismatch") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
main() {
  [map<i32, i32>] values{mapPair<i32, i32>(1i32, 2i32, 3i32, false)}
  return(mapCount<i32, i32>(values))
}
)";
  const std::string srcPath =
      writeTemp("compile_native_stdlib_collection_shim_map_pair_standalone_mismatch.prime", source);
  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o /dev/null --entry /main";
  CHECK(runCommand(compileCmd) == 2);
}

TEST_CASE("compiles and runs native stdlib collection shim map pair standalone string keys") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
main() {
  [map<string, i32>] values{mapPair<string, i32>("alpha"raw_utf8, 12i32, "beta"raw_utf8, 20i32)}
  return(plus(plus(mapAt<string, i32>(values, "beta"raw_utf8), mapAtUnsafe<string, i32>(values, "alpha"raw_utf8)),
      mapCount<string, i32>(values)))
}
)";
  const std::string srcPath =
      writeTemp("compile_native_stdlib_collection_shim_map_pair_standalone_string_key.prime", source);
  const std::string exePath = (testScratchPath("") /
                               "primec_native_stdlib_collection_shim_map_pair_standalone_string_key_exe")
                                  .string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 34);
}

TEST_CASE("rejects native stdlib collection shim map pair standalone key type mismatch") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
main() {
  [map<i32, i32>] values{mapPair<i32, i32>(1i32, 2i32, "oops"raw_utf8, 4i32)}
  return(mapCount<i32, i32>(values))
}
)";
  const std::string srcPath =
      writeTemp("compile_native_stdlib_collection_shim_map_pair_standalone_key_mismatch.prime", source);
  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o /dev/null --entry /main";
  CHECK(runCommand(compileCmd) == 2);
}

TEST_CASE("compiles and runs native stdlib collection shim map double standalone string keys") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
main() {
  [map<string, i32>] values{mapDouble<string, i32>("left"raw_utf8, 10i32, "right"raw_utf8, 15i32)}
  return(plus(plus(mapAt<string, i32>(values, "right"raw_utf8), mapAtUnsafe<string, i32>(values, "left"raw_utf8)),
      mapCount<string, i32>(values)))
}
)";
  const std::string srcPath =
      writeTemp("compile_native_stdlib_collection_shim_map_double_standalone_string_key.prime", source);
  const std::string exePath = (testScratchPath("") /
                               "primec_native_stdlib_collection_shim_map_double_standalone_string_key_exe")
                                  .string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 27);
}

TEST_CASE("rejects native stdlib collection shim map double standalone key type mismatch") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
main() {
  [map<i32, i32>] values{mapDouble<i32, i32>(1i32, 2i32, "oops"raw_utf8, 4i32)}
  return(mapCount<i32, i32>(values))
}
)";
  const std::string srcPath =
      writeTemp("compile_native_stdlib_collection_shim_map_double_standalone_key_mismatch.prime", source);
  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o /dev/null --entry /main";
  CHECK(runCommand(compileCmd) == 2);
}

TEST_CASE("compiles and runs native stdlib collection shim map triple standalone string keys") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
main() {
  [map<string, i32>] values{
    mapTriple<string, i32>("a"raw_utf8, 1i32, "b"raw_utf8, 2i32, "c"raw_utf8, 3i32)}
  return(plus(plus(mapAt<string, i32>(values, "c"raw_utf8), mapAtUnsafe<string, i32>(values, "a"raw_utf8)),
      mapCount<string, i32>(values)))
}
)";
  const std::string srcPath =
      writeTemp("compile_native_stdlib_collection_shim_map_triple_standalone_string_key.prime", source);
  const std::string exePath = (testScratchPath("") /
                               "primec_native_stdlib_collection_shim_map_triple_standalone_string_key_exe")
                                  .string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 7);
}

TEST_CASE("rejects native stdlib collection shim map triple standalone key type mismatch") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
main() {
  [map<i32, i32>] values{mapTriple<i32, i32>(1i32, 2i32, 3i32, 4i32, "oops"raw_utf8, 6i32)}
  return(mapCount<i32, i32>(values))
}
)";
  const std::string srcPath =
      writeTemp("compile_native_stdlib_collection_shim_map_triple_standalone_key_mismatch.prime", source);
  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o /dev/null --entry /main";
  CHECK(runCommand(compileCmd) == 2);
}

TEST_CASE("compiles and runs native stdlib collection shim map quad standalone") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
main() {
  [map<i32, i32>] values{
    mapQuad<i32, i32>(1i32, 3i32, 2i32, 5i32, 3i32, 7i32, 4i32, 11i32)}
  return(plus(plus(mapAt<i32, i32>(values, 4i32), mapAtUnsafe<i32, i32>(values, 1i32)), mapCount<i32, i32>(values)))
}
)";
  const std::string srcPath =
      writeTemp("compile_native_stdlib_collection_shim_map_quad_standalone.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_stdlib_collection_shim_map_quad_standalone_exe")
          .string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 18);
}

TEST_CASE("compiles and runs native stdlib collection shim map quad standalone string keys") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
main() {
  [map<string, i32>] values{
    mapQuad<string, i32>("a"raw_utf8, 1i32, "b"raw_utf8, 2i32, "c"raw_utf8, 3i32, "d"raw_utf8, 4i32)}
  return(plus(plus(mapAt<string, i32>(values, "d"raw_utf8), mapAtUnsafe<string, i32>(values, "a"raw_utf8)),
      mapCount<string, i32>(values)))
}
)";
  const std::string srcPath =
      writeTemp("compile_native_stdlib_collection_shim_map_quad_standalone_string_key.prime", source);
  const std::string exePath = (testScratchPath("") /
                               "primec_native_stdlib_collection_shim_map_quad_standalone_string_key_exe")
                                  .string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 9);
}

TEST_CASE("rejects native stdlib collection shim map quad standalone type mismatch") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
main() {
  [map<i32, i32>] values{
    mapQuad<i32, i32>(1i32, 2i32, 3i32, 4i32, 5i32, 6i32, 7i32, false)}
  return(mapCount<i32, i32>(values))
}
)";
  const std::string srcPath =
      writeTemp("compile_native_stdlib_collection_shim_map_quad_standalone_mismatch.prime", source);
  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o /dev/null --entry /main";
  CHECK(runCommand(compileCmd) == 2);
}

TEST_CASE("rejects native stdlib collection shim map quad standalone key type mismatch") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
main() {
  [map<i32, i32>] values{mapQuad<i32, i32>(1i32, 2i32, 3i32, 4i32, 5i32, 6i32, "oops"raw_utf8, 8i32)}
  return(mapCount<i32, i32>(values))
}
)";
  const std::string srcPath =
      writeTemp("compile_native_stdlib_collection_shim_map_quad_standalone_key_mismatch.prime", source);
  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o /dev/null --entry /main";
  CHECK(runCommand(compileCmd) == 2);
}

TEST_CASE("compiles and runs native stdlib collection shim map quint standalone") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
main() {
  [map<i32, i32>] values{
    mapQuint<i32, i32>(1i32, 3i32, 2i32, 5i32, 3i32, 7i32, 4i32, 11i32, 5i32, 13i32)}
  return(plus(plus(mapAt<i32, i32>(values, 5i32), mapAtUnsafe<i32, i32>(values, 1i32)), mapCount<i32, i32>(values)))
}
)";
  const std::string srcPath =
      writeTemp("compile_native_stdlib_collection_shim_map_quint_standalone.prime", source);
  const std::string exePath = (testScratchPath("") /
                               "primec_native_stdlib_collection_shim_map_quint_standalone_exe")
                                  .string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 21);
}

TEST_CASE("compiles and runs native stdlib collection shim map quint standalone string keys") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
main() {
  [map<string, i32>] values{
    mapQuint<string, i32>(
        "a"raw_utf8, 1i32, "b"raw_utf8, 2i32, "c"raw_utf8, 3i32, "d"raw_utf8, 4i32, "e"raw_utf8, 5i32)}
  return(plus(plus(mapAt<string, i32>(values, "e"raw_utf8), mapAtUnsafe<string, i32>(values, "a"raw_utf8)),
      mapCount<string, i32>(values)))
}
)";
  const std::string srcPath =
      writeTemp("compile_native_stdlib_collection_shim_map_quint_standalone_string_key.prime", source);
  const std::string exePath = (testScratchPath("") /
                               "primec_native_stdlib_collection_shim_map_quint_standalone_string_key_exe")
                                  .string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 11);
}

TEST_CASE("rejects native stdlib collection shim map quint standalone type mismatch") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
main() {
  [map<i32, i32>] values{
    mapQuint<i32, i32>(1i32, 2i32, 3i32, 4i32, 5i32, 6i32, 7i32, 8i32, 9i32, false)}
  return(mapCount<i32, i32>(values))
}
)";
  const std::string srcPath =
      writeTemp("compile_native_stdlib_collection_shim_map_quint_standalone_mismatch.prime", source);
  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o /dev/null --entry /main";
  CHECK(runCommand(compileCmd) == 2);
}

TEST_CASE("rejects native stdlib collection shim map quint standalone key type mismatch") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
main() {
  [map<i32, i32>] values{
    mapQuint<i32, i32>(1i32, 2i32, 3i32, 4i32, 5i32, 6i32, 7i32, 8i32, "oops"raw_utf8, 10i32)}
  return(mapCount<i32, i32>(values))
}
)";
  const std::string srcPath =
      writeTemp("compile_native_stdlib_collection_shim_map_quint_standalone_key_mismatch.prime", source);
  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o /dev/null --entry /main";
  CHECK(runCommand(compileCmd) == 2);
}

TEST_CASE("compiles and runs native stdlib collection shim map sext standalone") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
main() {
  [map<i32, i32>] values{
    mapSext<i32, i32>(1i32, 3i32, 2i32, 5i32, 3i32, 7i32, 4i32, 11i32, 5i32, 13i32, 6i32, 17i32)}
  return(plus(plus(mapAt<i32, i32>(values, 6i32), mapAtUnsafe<i32, i32>(values, 1i32)), mapCount<i32, i32>(values)))
}
)";
  const std::string srcPath =
      writeTemp("compile_native_stdlib_collection_shim_map_sext_standalone.prime", source);
  const std::string exePath = (testScratchPath("") /
                               "primec_native_stdlib_collection_shim_map_sext_standalone_exe")
                                  .string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 26);
}

TEST_CASE("compiles and runs native stdlib collection shim map sext standalone string keys") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
main() {
  [map<string, i32>] values{
    mapSext<string, i32>(
        "a"raw_utf8, 1i32, "b"raw_utf8, 2i32, "c"raw_utf8, 3i32, "d"raw_utf8, 4i32, "e"raw_utf8, 5i32,
        "f"raw_utf8, 6i32)}
  return(plus(plus(mapAt<string, i32>(values, "f"raw_utf8), mapAtUnsafe<string, i32>(values, "a"raw_utf8)),
      mapCount<string, i32>(values)))
}
)";
  const std::string srcPath =
      writeTemp("compile_native_stdlib_collection_shim_map_sext_standalone_string_key.prime", source);
  const std::string exePath = (testScratchPath("") /
                               "primec_native_stdlib_collection_shim_map_sext_standalone_string_key_exe")
                                  .string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 13);
}

TEST_CASE("rejects native stdlib collection shim map sext standalone type mismatch") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
main() {
  [map<i32, i32>] values{
    mapSext<i32, i32>(1i32, 2i32, 3i32, 4i32, 5i32, 6i32, 7i32, 8i32, 9i32, 10i32, 11i32, false)}
  return(mapCount<i32, i32>(values))
}
)";
  const std::string srcPath =
      writeTemp("compile_native_stdlib_collection_shim_map_sext_standalone_mismatch.prime", source);
  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o /dev/null --entry /main";
  CHECK(runCommand(compileCmd) == 2);
}

TEST_CASE("rejects native stdlib collection shim map sext standalone key type mismatch") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
main() {
  [map<i32, i32>] values{
    mapSext<i32, i32>(1i32, 2i32, 3i32, 4i32, 5i32, 6i32, 7i32, 8i32, 9i32, 10i32, "oops"raw_utf8, 12i32)}
  return(mapCount<i32, i32>(values))
}
)";
  const std::string srcPath =
      writeTemp("compile_native_stdlib_collection_shim_map_sext_standalone_key_mismatch.prime", source);
  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o /dev/null --entry /main";
  CHECK(runCommand(compileCmd) == 2);
}


TEST_SUITE_END();
#endif
