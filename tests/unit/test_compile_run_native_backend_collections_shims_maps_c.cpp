#include "test_compile_run_helpers.h"

#include "test_compile_run_collection_conformance_helpers.h"
#include "test_compile_run_container_error_conformance_helpers.h"
#include "test_compile_run_checked_pointer_conformance_helpers.h"
#include "test_compile_run_unchecked_pointer_conformance_helpers.h"
#include "test_compile_run_native_backend_collections_helpers.h"

#if PRIMESTRUCT_NATIVE_COLLECTIONS_ENABLED
TEST_SUITE_BEGIN("primestruct.compile.run.native_backend.collections");

TEST_CASE("compiles and runs native stdlib collection shim map sept standalone") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
main() {
  [map<i32, i32>] values{
    mapSept<i32, i32>(1i32, 3i32, 2i32, 5i32, 3i32, 7i32, 4i32, 11i32, 5i32, 13i32, 6i32, 17i32, 7i32, 19i32)}
  return(plus(plus(mapAt<i32, i32>(values, 7i32), mapAtUnsafe<i32, i32>(values, 1i32)), mapCount<i32, i32>(values)))
}
)";
  const std::string srcPath =
      writeTemp("compile_native_stdlib_collection_shim_map_sept_standalone.prime", source);
  const std::string exePath = (testScratchPath("") /
                               "primec_native_stdlib_collection_shim_map_sept_standalone_exe")
                                  .string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 29);
}

TEST_CASE("compiles and runs native stdlib collection shim map sept standalone string keys") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
main() {
  [map<string, i32>] values{
    mapSept<string, i32>(
        "a"raw_utf8, 1i32, "b"raw_utf8, 2i32, "c"raw_utf8, 3i32, "d"raw_utf8, 4i32, "e"raw_utf8, 5i32,
        "f"raw_utf8, 6i32, "g"raw_utf8, 7i32)}
  return(plus(plus(mapAt<string, i32>(values, "g"raw_utf8), mapAtUnsafe<string, i32>(values, "a"raw_utf8)),
      mapCount<string, i32>(values)))
}
)";
  const std::string srcPath =
      writeTemp("compile_native_stdlib_collection_shim_map_sept_standalone_string_key.prime", source);
  const std::string exePath = (testScratchPath("") /
                               "primec_native_stdlib_collection_shim_map_sept_standalone_string_key_exe")
                                  .string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 15);
}

TEST_CASE("rejects native stdlib collection shim map sept standalone type mismatch") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
main() {
  [map<i32, i32>] values{
    mapSept<i32, i32>(1i32, 2i32, 3i32, 4i32, 5i32, 6i32, 7i32, 8i32, 9i32, 10i32, 11i32, 12i32, 13i32, false)}
  return(mapCount<i32, i32>(values))
}
)";
  const std::string srcPath =
      writeTemp("compile_native_stdlib_collection_shim_map_sept_standalone_mismatch.prime", source);
  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o /dev/null --entry /main";
  CHECK(runCommand(compileCmd) == 2);
}

TEST_CASE("rejects native stdlib collection shim map sept standalone key type mismatch") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
main() {
  [map<i32, i32>] values{
    mapSept<i32, i32>(
        1i32, 2i32, 3i32, 4i32, 5i32, 6i32, 7i32, 8i32, 9i32, 10i32, 11i32, 12i32, "oops"raw_utf8, 14i32)}
  return(mapCount<i32, i32>(values))
}
)";
  const std::string srcPath =
      writeTemp("compile_native_stdlib_collection_shim_map_sept_standalone_key_mismatch.prime", source);
  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o /dev/null --entry /main";
  CHECK(runCommand(compileCmd) == 2);
}

TEST_CASE("compiles and runs native stdlib collection shim map oct standalone") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
main() {
  [map<i32, i32>] values{
    mapOct<i32, i32>(1i32, 3i32, 2i32, 5i32, 3i32, 7i32, 4i32, 11i32, 5i32, 13i32, 6i32, 17i32, 7i32, 19i32, 8i32,
        23i32)}
  return(plus(plus(mapAt<i32, i32>(values, 8i32), mapAtUnsafe<i32, i32>(values, 1i32)), mapCount<i32, i32>(values)))
}
)";
  const std::string srcPath =
      writeTemp("compile_native_stdlib_collection_shim_map_oct_standalone.prime", source);
  const std::string exePath = (testScratchPath("") /
                               "primec_native_stdlib_collection_shim_map_oct_standalone_exe")
                                  .string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 34);
}

TEST_CASE("compiles and runs native stdlib collection shim map oct standalone string keys") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
main() {
  [map<string, i32>] values{
    mapOct<string, i32>(
        "a"raw_utf8, 1i32, "b"raw_utf8, 2i32, "c"raw_utf8, 3i32, "d"raw_utf8, 4i32, "e"raw_utf8, 5i32, "f"raw_utf8,
        6i32, "g"raw_utf8, 7i32, "h"raw_utf8, 8i32)}
  return(plus(plus(mapAt<string, i32>(values, "h"raw_utf8), mapAtUnsafe<string, i32>(values, "a"raw_utf8)),
      mapCount<string, i32>(values)))
}
)";
  const std::string srcPath =
      writeTemp("compile_native_stdlib_collection_shim_map_oct_standalone_string_key.prime", source);
  const std::string exePath = (testScratchPath("") /
                               "primec_native_stdlib_collection_shim_map_oct_standalone_string_key_exe")
                                  .string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 17);
}

TEST_CASE("rejects native stdlib collection shim map oct standalone type mismatch") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
main() {
  [map<i32, i32>] values{
    mapOct<i32, i32>(
        1i32, 2i32, 3i32, 4i32, 5i32, 6i32, 7i32, 8i32, 9i32, 10i32, 11i32, 12i32, 13i32, 14i32, 15i32, false)}
  return(mapCount<i32, i32>(values))
}
)";
  const std::string srcPath =
      writeTemp("compile_native_stdlib_collection_shim_map_oct_standalone_mismatch.prime", source);
  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o /dev/null --entry /main";
  CHECK(runCommand(compileCmd) == 2);
}

TEST_CASE("rejects native stdlib collection shim map oct standalone key type mismatch") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
main() {
  [map<i32, i32>] values{
    mapOct<i32, i32>(
        1i32, 2i32, 3i32, 4i32, 5i32, 6i32, 7i32, 8i32, 9i32, 10i32, 11i32, 12i32, 13i32, 14i32, "oops"raw_utf8,
        16i32)}
  return(mapCount<i32, i32>(values))
}
)";
  const std::string srcPath =
      writeTemp("compile_native_stdlib_collection_shim_map_oct_standalone_key_mismatch.prime", source);
  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o /dev/null --entry /main";
  CHECK(runCommand(compileCmd) == 2);
}

TEST_CASE("compiles and runs native stdlib collection shim map double") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
main() {
  [map<string, i32>] values{mapDouble<string, i32>("left"raw_utf8, 10i32, "right"raw_utf8, 15i32)}
  return(plus(plus(mapAt<string, i32>(values, "right"raw_utf8), mapAtUnsafe<string, i32>(values, "left"raw_utf8)),
      mapCount<string, i32>(values)))
}
)";
  const std::string srcPath = writeTemp("compile_native_stdlib_collection_shim_map_double.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_stdlib_collection_shim_map_double_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 27);
}

TEST_CASE("rejects native stdlib collection shim map double type mismatch") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
main() {
  [map<i32, i32>] values{mapDouble<i32, i32>(1i32, 2i32, 3i32, false)}
  return(mapCount<i32, i32>(values))
}
)";
  const std::string srcPath = writeTemp("compile_native_stdlib_collection_shim_map_double_mismatch.prime", source);
  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o /dev/null --entry /main";
  CHECK(runCommand(compileCmd) == 2);
}

TEST_CASE("compiles and runs native stdlib collection shim map triple") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
main() {
  [map<string, i32>] values{
    mapTriple<string, i32>("left"raw_utf8, 5i32, "center"raw_utf8, 7i32, "right"raw_utf8, 9i32)}
  return(plus(plus(mapAt<string, i32>(values, "right"raw_utf8), mapAtUnsafe<string, i32>(values, "left"raw_utf8)),
      mapCount<string, i32>(values)))
}
)";
  const std::string srcPath = writeTemp("compile_native_stdlib_collection_shim_map_triple.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_stdlib_collection_shim_map_triple_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 17);
}

TEST_CASE("rejects native stdlib collection shim map triple type mismatch") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
main() {
  [map<i32, i32>] values{mapTriple<i32, i32>(1i32, 2i32, 3i32, 4i32, 5i32, false)}
  return(mapCount<i32, i32>(values))
}
)";
  const std::string srcPath = writeTemp("compile_native_stdlib_collection_shim_map_triple_mismatch.prime", source);
  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o /dev/null --entry /main";
  CHECK(runCommand(compileCmd) == 2);
}

TEST_CASE("compiles and runs native stdlib collection shim extended constructors") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vectorQuad<i32>(2i32, 4i32, 6i32, 8i32)}
  [map<i32, i32>] pairs{mapTriple<i32, i32>(1i32, 10i32, 2i32, 20i32, 3i32, 30i32)}
  [i32] vectorTotal{plus(vectorAt<i32>(values, 1i32), vectorAtUnsafe<i32>(values, 3i32))}
  [i32] mapTotal{plus(mapAt<i32, i32>(pairs, 1i32), mapAtUnsafe<i32, i32>(pairs, 3i32))}
  return(plus(plus(vectorTotal, mapTotal), plus(vectorCount<i32>(values), mapCount<i32, i32>(pairs))))
}
)";
  const std::string srcPath = writeTemp("compile_native_stdlib_collection_shim_extended_ctor.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_stdlib_collection_shim_extended_ctor_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 59);
}

TEST_CASE("rejects native stdlib collection shim extended constructor type mismatch") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] pairs{mapTriple<i32, i32>(1i32, 10i32, 2i32, 20i32, 3i32, true)}
  return(mapCount<i32, i32>(pairs))
}
)";
  const std::string srcPath = writeTemp("compile_native_stdlib_collection_shim_extended_ctor_mismatch.prime", source);
  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o /dev/null --entry /main";
  CHECK(runCommand(compileCmd) == 2);
}

TEST_CASE("compiles and runs native stdlib collection shim vector quint constructor") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vectorQuint<i32>(1i32, 3i32, 5i32, 7i32, 9i32)}
  [i32] picked{plus(vectorAt<i32>(values, 4i32), vectorAtUnsafe<i32>(values, 0i32))}
  return(plus(picked, vectorCount<i32>(values)))
}
)";
  const std::string srcPath = writeTemp("compile_native_stdlib_collection_shim_vector_quint_ctor.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_stdlib_collection_shim_vector_quint_ctor_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 15);
}

TEST_CASE("rejects native stdlib collection shim vector quint type mismatch") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vectorQuint<i32>(1i32, 2i32, 3i32, 4i32, true)}
  return(vectorCount<i32>(values))
}
)";
  const std::string srcPath = writeTemp("compile_native_stdlib_collection_shim_vector_quint_mismatch.prime", source);
  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o /dev/null --entry /main";
  CHECK(runCommand(compileCmd) == 2);
}

TEST_CASE("compiles and runs native stdlib collection shim vector sext constructor") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vectorSext<i32>(2i32, 4i32, 6i32, 8i32, 10i32, 12i32)}
  [i32] picked{plus(vectorAt<i32>(values, 5i32), vectorAtUnsafe<i32>(values, 0i32))}
  return(plus(picked, vectorCount<i32>(values)))
}
)";
  const std::string srcPath = writeTemp("compile_native_stdlib_collection_shim_vector_sext_ctor.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_stdlib_collection_shim_vector_sext_ctor_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 20);
}

TEST_CASE("rejects native stdlib collection shim vector sext type mismatch") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vectorSext<i32>(1i32, 2i32, 3i32, 4i32, 5i32, false)}
  return(vectorCount<i32>(values))
}
)";
  const std::string srcPath = writeTemp("compile_native_stdlib_collection_shim_vector_sext_mismatch.prime", source);
  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o /dev/null --entry /main";
  CHECK(runCommand(compileCmd) == 2);
}

TEST_CASE("compiles and runs native stdlib collection shim vector sept constructor") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vectorSept<i32>(3i32, 6i32, 9i32, 12i32, 15i32, 18i32, 21i32)}
  [i32] picked{plus(vectorAt<i32>(values, 6i32), vectorAtUnsafe<i32>(values, 0i32))}
  return(plus(picked, vectorCount<i32>(values)))
}
)";
  const std::string srcPath = writeTemp("compile_native_stdlib_collection_shim_vector_sept_ctor.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_stdlib_collection_shim_vector_sept_ctor_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 31);
}

TEST_CASE("rejects native stdlib collection shim vector sept type mismatch") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vectorSept<i32>(1i32, 2i32, 3i32, 4i32, 5i32, 6i32, false)}
  return(vectorCount<i32>(values))
}
)";
  const std::string srcPath = writeTemp("compile_native_stdlib_collection_shim_vector_sept_mismatch.prime", source);
  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o /dev/null --entry /main";
  CHECK(runCommand(compileCmd) == 2);
}

TEST_CASE("compiles and runs native stdlib collection shim vector oct constructor") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vectorOct<i32>(4i32, 8i32, 12i32, 16i32, 20i32, 24i32, 28i32, 32i32)}
  [i32] picked{plus(vectorAt<i32>(values, 7i32), vectorAtUnsafe<i32>(values, 0i32))}
  return(plus(picked, vectorCount<i32>(values)))
}
)";
  const std::string srcPath = writeTemp("compile_native_stdlib_collection_shim_vector_oct_ctor.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_stdlib_collection_shim_vector_oct_ctor_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 44);
}

TEST_CASE("rejects native stdlib collection shim vector oct type mismatch") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vectorOct<i32>(1i32, 2i32, 3i32, 4i32, 5i32, 6i32, 7i32, false)}
  return(vectorCount<i32>(values))
}
)";
  const std::string srcPath = writeTemp("compile_native_stdlib_collection_shim_vector_oct_mismatch.prime", source);
  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o /dev/null --entry /main";
  CHECK(runCommand(compileCmd) == 2);
}

TEST_CASE("compiles and runs native stdlib collection shim map pair string keys") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
main() {
  [map<string, i32>] values{mapPair<string, i32>("alpha"raw_utf8, 12i32, "beta"raw_utf8, 20i32)}
  [string] key{"beta"raw_utf8}
  return(plus(plus(mapAt<string, i32>(values, key), mapAtUnsafe<string, i32>(values, "alpha"raw_utf8)),
      mapCount<string, i32>(values)))
}
)";
  const std::string srcPath = writeTemp("compile_native_stdlib_collection_shim_map_pair_string_key.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_stdlib_collection_shim_map_pair_string_key_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 34);
}

TEST_CASE("rejects native stdlib collection shim map pair type mismatch") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
main() {
  [map<i32, i32>] values{mapPair<i32, i32>(1i32, 2i32, 3i32, true)}
  return(mapCount<i32, i32>(values))
}
)";
  const std::string srcPath = writeTemp("compile_native_stdlib_collection_shim_map_pair_mismatch.prime", source);
  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o /dev/null --entry /main";
  CHECK(runCommand(compileCmd) == 2);
}

TEST_CASE("compiles and runs native stdlib collection shim map quad") {
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
  const std::string srcPath = writeTemp("compile_native_stdlib_collection_shim_map_quad.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_stdlib_collection_shim_map_quad_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 9);
}

TEST_CASE("rejects native stdlib collection shim map quad type mismatch") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
main() {
  [map<i32, i32>] values{mapQuad<i32, i32>(1i32, 2i32, 3i32, 4i32, 5i32, 6i32, 7i32, false)}
  return(mapCount<i32, i32>(values))
}
)";
  const std::string srcPath = writeTemp("compile_native_stdlib_collection_shim_map_quad_mismatch.prime", source);
  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o /dev/null --entry /main";
  CHECK(runCommand(compileCmd) == 2);
}

TEST_CASE("compiles and runs native stdlib collection shim map quint") {
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
  const std::string srcPath = writeTemp("compile_native_stdlib_collection_shim_map_quint.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_stdlib_collection_shim_map_quint_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 11);
}

TEST_CASE("rejects native stdlib collection shim map quint type mismatch") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
main() {
  [map<i32, i32>] values{mapQuint<i32, i32>(1i32, 2i32, 3i32, 4i32, 5i32, 6i32, 7i32, 8i32, 9i32, false)}
  return(mapCount<i32, i32>(values))
}
)";
  const std::string srcPath = writeTemp("compile_native_stdlib_collection_shim_map_quint_mismatch.prime", source);
  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o /dev/null --entry /main";
  CHECK(runCommand(compileCmd) == 2);
}

TEST_CASE("compiles and runs native stdlib collection shim map sext") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
main() {
  [map<string, i32>] values{
    mapSext<string, i32>(
        "a"raw_utf8, 1i32, "b"raw_utf8, 2i32, "c"raw_utf8, 3i32, "d"raw_utf8, 4i32, "e"raw_utf8, 5i32, "f"raw_utf8,
        6i32)}
  return(plus(plus(mapAt<string, i32>(values, "f"raw_utf8), mapAtUnsafe<string, i32>(values, "a"raw_utf8)),
      mapCount<string, i32>(values)))
}
)";
  const std::string srcPath = writeTemp("compile_native_stdlib_collection_shim_map_sext.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_stdlib_collection_shim_map_sext_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 13);
}

TEST_CASE("rejects native stdlib collection shim map sext type mismatch") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
main() {
  [map<i32, i32>] values{
    mapSext<i32, i32>(1i32, 2i32, 3i32, 4i32, 5i32, 6i32, 7i32, 8i32, 9i32, 10i32, 11i32, false)}
  return(mapCount<i32, i32>(values))
}
)";
  const std::string srcPath = writeTemp("compile_native_stdlib_collection_shim_map_sext_mismatch.prime", source);
  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o /dev/null --entry /main";
  CHECK(runCommand(compileCmd) == 2);
}

TEST_CASE("compiles and runs native stdlib collection shim map sept") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
main() {
  [map<string, i32>] values{
    mapSept<string, i32>(
        "a"raw_utf8, 1i32, "b"raw_utf8, 2i32, "c"raw_utf8, 3i32, "d"raw_utf8, 4i32, "e"raw_utf8, 5i32, "f"raw_utf8,
        6i32, "g"raw_utf8, 7i32)}
  return(plus(plus(mapAt<string, i32>(values, "g"raw_utf8), mapAtUnsafe<string, i32>(values, "a"raw_utf8)),
      mapCount<string, i32>(values)))
}
)";
  const std::string srcPath = writeTemp("compile_native_stdlib_collection_shim_map_sept.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_stdlib_collection_shim_map_sept_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 15);
}

TEST_CASE("rejects native stdlib collection shim map sept type mismatch") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
main() {
  [map<i32, i32>] values{
    mapSept<i32, i32>(
        1i32, 2i32, 3i32, 4i32, 5i32, 6i32, 7i32, 8i32, 9i32, 10i32, 11i32, 12i32, 13i32, false)}
  return(mapCount<i32, i32>(values))
}
)";
  const std::string srcPath = writeTemp("compile_native_stdlib_collection_shim_map_sept_mismatch.prime", source);
  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o /dev/null --entry /main";
  CHECK(runCommand(compileCmd) == 2);
}

TEST_CASE("compiles and runs native stdlib collection shim map oct") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
main() {
  [map<string, i32>] values{
    mapOct<string, i32>(
        "a"raw_utf8, 1i32, "b"raw_utf8, 2i32, "c"raw_utf8, 3i32, "d"raw_utf8, 4i32, "e"raw_utf8, 5i32, "f"raw_utf8,
        6i32, "g"raw_utf8, 7i32, "h"raw_utf8, 8i32)}
  return(plus(plus(mapAt<string, i32>(values, "h"raw_utf8), mapAtUnsafe<string, i32>(values, "a"raw_utf8)),
      mapCount<string, i32>(values)))
}
)";
  const std::string srcPath = writeTemp("compile_native_stdlib_collection_shim_map_oct.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_stdlib_collection_shim_map_oct_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 17);
}

TEST_CASE("rejects native stdlib collection shim map oct type mismatch") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
main() {
  [map<i32, i32>] values{
    mapOct<i32, i32>(
        1i32, 2i32, 3i32, 4i32, 5i32, 6i32, 7i32, 8i32, 9i32, 10i32, 11i32, 12i32, 13i32, 14i32, 15i32, false)}
  return(mapCount<i32, i32>(values))
}
)";
  const std::string srcPath = writeTemp("compile_native_stdlib_collection_shim_map_oct_mismatch.prime", source);
  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o /dev/null --entry /main";
  CHECK(runCommand(compileCmd) == 2);
}


TEST_SUITE_END();
#endif
