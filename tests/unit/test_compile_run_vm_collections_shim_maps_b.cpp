#include "test_compile_run_helpers.h"

#include "test_compile_run_collection_conformance_helpers.h"
#include "test_compile_run_container_error_conformance_helpers.h"
#include "test_compile_run_checked_pointer_conformance_helpers.h"
#include "test_compile_run_unchecked_pointer_conformance_helpers.h"
#include "test_compile_run_vm_collections_helpers.h"

TEST_SUITE_BEGIN("primestruct.compile.run.vm.collections");

TEST_CASE("rejects vm stdlib collection shim map triple standalone key type mismatch") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
main() {
  [map<i32, i32>] values{mapTriple<i32, i32>(1i32, 2i32, 3i32, 4i32, "oops"raw_utf8, 6i32)}
  return(mapCount<i32, i32>(values))
}
)";
  const std::string srcPath = writeTemp("vm_stdlib_collection_shim_map_triple_standalone_key_mismatch.prime", source);
  const std::string errPath =
      (std::filesystem::temp_directory_path() /
       "primec_vm_stdlib_collection_shim_map_triple_standalone_key_mismatch.err")
          .string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main 2> " + errPath;
  CHECK(runCommand(runCmd) == 2);
  const std::string error = readFile(errPath);
  CHECK(error.find("argument type mismatch for /std/collections/mapTriple__") !=
        std::string::npos);
  CHECK(error.find("parameter thirdKey: expected i32") != std::string::npos);
}

TEST_CASE("runs vm with stdlib collection shim map quad standalone") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
main() {
  [map<i32, i32>] values{
    mapQuad<i32, i32>(1i32, 3i32, 2i32, 5i32, 3i32, 7i32, 4i32, 11i32)}
  return(plus(plus(mapAt<i32, i32>(values, 4i32), mapAtUnsafe<i32, i32>(values, 1i32)), mapCount<i32, i32>(values)))
}
)";
  const std::string srcPath = writeTemp("vm_stdlib_collection_shim_map_quad_standalone.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 18);
}

TEST_CASE("runs vm with stdlib collection shim map quad standalone string keys") {
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
  const std::string srcPath = writeTemp("vm_stdlib_collection_shim_map_quad_standalone_string_key.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 9);
}

TEST_CASE("rejects vm stdlib collection shim map quad standalone type mismatch") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
main() {
  [map<i32, i32>] values{
    mapQuad<i32, i32>(1i32, 2i32, 3i32, 4i32, 5i32, 6i32, 7i32, false)}
  return(mapCount<i32, i32>(values))
}
)";
  const std::string srcPath = writeTemp("vm_stdlib_collection_shim_map_quad_standalone_mismatch.prime", source);
  const std::string errPath =
      (std::filesystem::temp_directory_path() /
       "primec_vm_stdlib_collection_shim_map_quad_standalone_mismatch.err")
          .string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main 2> " + errPath;
  CHECK(runCommand(runCmd) == 2);
  CHECK(readFile(errPath).find("map literal value type mismatch") != std::string::npos);
}

TEST_CASE("rejects vm stdlib collection shim map quad standalone key type mismatch") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
main() {
  [map<i32, i32>] values{mapQuad<i32, i32>(1i32, 2i32, 3i32, 4i32, 5i32, 6i32, "oops"raw_utf8, 8i32)}
  return(mapCount<i32, i32>(values))
}
)";
  const std::string srcPath = writeTemp("vm_stdlib_collection_shim_map_quad_standalone_key_mismatch.prime", source);
  const std::string errPath =
      (std::filesystem::temp_directory_path() /
       "primec_vm_stdlib_collection_shim_map_quad_standalone_key_mismatch.err")
          .string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main 2> " + errPath;
  CHECK(runCommand(runCmd) == 2);
  const std::string error = readFile(errPath);
  CHECK(error.find("argument type mismatch for /std/collections/mapQuad__") != std::string::npos);
  CHECK(error.find("parameter fourthKey: expected i32") != std::string::npos);
}

TEST_CASE("runs vm with stdlib collection shim map quint standalone") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
main() {
  [map<i32, i32>] values{
    mapQuint<i32, i32>(1i32, 3i32, 2i32, 5i32, 3i32, 7i32, 4i32, 11i32, 5i32, 13i32)}
  return(plus(plus(mapAt<i32, i32>(values, 5i32), mapAtUnsafe<i32, i32>(values, 1i32)), mapCount<i32, i32>(values)))
}
)";
  const std::string srcPath = writeTemp("vm_stdlib_collection_shim_map_quint_standalone.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 21);
}

TEST_CASE("runs vm with stdlib collection shim map quint standalone string keys") {
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
  const std::string srcPath = writeTemp("vm_stdlib_collection_shim_map_quint_standalone_string_key.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 11);
}

TEST_CASE("rejects vm stdlib collection shim map quint standalone type mismatch") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
main() {
  [map<i32, i32>] values{
    mapQuint<i32, i32>(1i32, 2i32, 3i32, 4i32, 5i32, 6i32, 7i32, 8i32, 9i32, false)}
  return(mapCount<i32, i32>(values))
}
)";
  const std::string srcPath = writeTemp("vm_stdlib_collection_shim_map_quint_standalone_mismatch.prime", source);
  const std::string errPath =
      (std::filesystem::temp_directory_path() /
       "primec_vm_stdlib_collection_shim_map_quint_standalone_mismatch.err")
          .string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main 2> " + errPath;
  CHECK(runCommand(runCmd) == 2);
  CHECK(readFile(errPath).find("map literal value type mismatch") != std::string::npos);
}

TEST_CASE("rejects vm stdlib collection shim map quint standalone key type mismatch") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
main() {
  [map<i32, i32>] values{
    mapQuint<i32, i32>(1i32, 2i32, 3i32, 4i32, 5i32, 6i32, 7i32, 8i32, "oops"raw_utf8, 10i32)}
  return(mapCount<i32, i32>(values))
}
)";
  const std::string srcPath = writeTemp("vm_stdlib_collection_shim_map_quint_standalone_key_mismatch.prime", source);
  const std::string errPath =
      (std::filesystem::temp_directory_path() /
       "primec_vm_stdlib_collection_shim_map_quint_standalone_key_mismatch.err")
          .string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main 2> " + errPath;
  CHECK(runCommand(runCmd) == 2);
  const std::string error = readFile(errPath);
  CHECK(error.find("argument type mismatch for /std/collections/mapQuint__") != std::string::npos);
  CHECK(error.find("parameter fifthKey: expected i32") != std::string::npos);
}

TEST_CASE("runs vm with stdlib collection shim map sext standalone") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
main() {
  [map<i32, i32>] values{
    mapSext<i32, i32>(1i32, 3i32, 2i32, 5i32, 3i32, 7i32, 4i32, 11i32, 5i32, 13i32, 6i32, 17i32)}
  return(plus(plus(mapAt<i32, i32>(values, 6i32), mapAtUnsafe<i32, i32>(values, 1i32)), mapCount<i32, i32>(values)))
}
)";
  const std::string srcPath = writeTemp("vm_stdlib_collection_shim_map_sext_standalone.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 26);
}

TEST_CASE("runs vm with stdlib collection shim map sext standalone string keys") {
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
  const std::string srcPath = writeTemp("vm_stdlib_collection_shim_map_sext_standalone_string_key.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 13);
}

TEST_CASE("rejects vm stdlib collection shim map sext standalone type mismatch") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
main() {
  [map<i32, i32>] values{
    mapSext<i32, i32>(1i32, 2i32, 3i32, 4i32, 5i32, 6i32, 7i32, 8i32, 9i32, 10i32, 11i32, false)}
  return(mapCount<i32, i32>(values))
}
)";
  const std::string srcPath = writeTemp("vm_stdlib_collection_shim_map_sext_standalone_mismatch.prime", source);
  const std::string errPath =
      (std::filesystem::temp_directory_path() /
       "primec_vm_stdlib_collection_shim_map_sext_standalone_mismatch.err")
          .string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main 2> " + errPath;
  CHECK(runCommand(runCmd) == 2);
  CHECK(readFile(errPath).find("map literal value type mismatch") != std::string::npos);
}

TEST_CASE("rejects vm stdlib collection shim map sext standalone key type mismatch") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
main() {
  [map<i32, i32>] values{
    mapSext<i32, i32>(1i32, 2i32, 3i32, 4i32, 5i32, 6i32, 7i32, 8i32, 9i32, 10i32, "oops"raw_utf8, 12i32)}
  return(mapCount<i32, i32>(values))
}
)";
  const std::string srcPath = writeTemp("vm_stdlib_collection_shim_map_sext_standalone_key_mismatch.prime", source);
  const std::string errPath =
      (std::filesystem::temp_directory_path() /
       "primec_vm_stdlib_collection_shim_map_sext_standalone_key_mismatch.err")
          .string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main 2> " + errPath;
  CHECK(runCommand(runCmd) == 2);
  const std::string error = readFile(errPath);
  CHECK(error.find("argument type mismatch for /std/collections/mapSext__") != std::string::npos);
  CHECK(error.find("parameter sixthKey: expected i32") != std::string::npos);
}

TEST_CASE("runs vm with stdlib collection shim map sept standalone") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
main() {
  [map<i32, i32>] values{
    mapSept<i32, i32>(1i32, 3i32, 2i32, 5i32, 3i32, 7i32, 4i32, 11i32, 5i32, 13i32, 6i32, 17i32, 7i32, 19i32)}
  return(plus(plus(mapAt<i32, i32>(values, 7i32), mapAtUnsafe<i32, i32>(values, 1i32)), mapCount<i32, i32>(values)))
}
)";
  const std::string srcPath = writeTemp("vm_stdlib_collection_shim_map_sept_standalone.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 29);
}

TEST_CASE("runs vm with stdlib collection shim map sept standalone string keys") {
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
  const std::string srcPath = writeTemp("vm_stdlib_collection_shim_map_sept_standalone_string_key.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 15);
}

TEST_CASE("rejects vm stdlib collection shim map sept standalone type mismatch") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
main() {
  [map<i32, i32>] values{
    mapSept<i32, i32>(1i32, 2i32, 3i32, 4i32, 5i32, 6i32, 7i32, 8i32, 9i32, 10i32, 11i32, 12i32, 13i32, false)}
  return(mapCount<i32, i32>(values))
}
)";
  const std::string srcPath = writeTemp("vm_stdlib_collection_shim_map_sept_standalone_mismatch.prime", source);
  const std::string errPath =
      (std::filesystem::temp_directory_path() /
       "primec_vm_stdlib_collection_shim_map_sept_standalone_mismatch.err")
          .string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main 2> " + errPath;
  CHECK(runCommand(runCmd) == 2);
  CHECK(readFile(errPath).find("map literal value type mismatch") != std::string::npos);
}

TEST_CASE("rejects vm stdlib collection shim map sept standalone key type mismatch") {
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
  const std::string srcPath = writeTemp("vm_stdlib_collection_shim_map_sept_standalone_key_mismatch.prime", source);
  const std::string errPath =
      (std::filesystem::temp_directory_path() /
       "primec_vm_stdlib_collection_shim_map_sept_standalone_key_mismatch.err")
          .string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main 2> " + errPath;
  CHECK(runCommand(runCmd) == 2);
  const std::string error = readFile(errPath);
  CHECK(error.find("argument type mismatch for /std/collections/mapSept__") != std::string::npos);
  CHECK(error.find("parameter seventhKey: expected i32") != std::string::npos);
}

TEST_CASE("runs vm with stdlib collection shim map oct standalone") {
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
  const std::string srcPath = writeTemp("vm_stdlib_collection_shim_map_oct_standalone.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 34);
}

TEST_CASE("runs vm with stdlib collection shim map oct standalone string keys") {
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
  const std::string srcPath = writeTemp("vm_stdlib_collection_shim_map_oct_standalone_string_key.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 17);
}

TEST_CASE("rejects vm stdlib collection shim map oct standalone type mismatch") {
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
  const std::string srcPath = writeTemp("vm_stdlib_collection_shim_map_oct_standalone_mismatch.prime", source);
  const std::string errPath =
      (std::filesystem::temp_directory_path() /
       "primec_vm_stdlib_collection_shim_map_oct_standalone_mismatch.err")
          .string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main 2> " + errPath;
  CHECK(runCommand(runCmd) == 2);
  CHECK(readFile(errPath).find("map literal value type mismatch") != std::string::npos);
}

TEST_CASE("rejects vm stdlib collection shim map oct standalone key type mismatch") {
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
  const std::string srcPath = writeTemp("vm_stdlib_collection_shim_map_oct_standalone_key_mismatch.prime", source);
  const std::string errPath =
      (std::filesystem::temp_directory_path() /
       "primec_vm_stdlib_collection_shim_map_oct_standalone_key_mismatch.err")
          .string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main 2> " + errPath;
  CHECK(runCommand(runCmd) == 2);
  const std::string error = readFile(errPath);
  CHECK(error.find("argument type mismatch for /std/collections/mapOct__") != std::string::npos);
  CHECK(error.find("parameter eighthKey: expected i32") != std::string::npos);
}

TEST_CASE("runs vm with stdlib collection shim map double") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
main() {
  [map<string, i32>] values{mapDouble<string, i32>("left"raw_utf8, 10i32, "right"raw_utf8, 15i32)}
  return(plus(plus(mapAt<string, i32>(values, "right"raw_utf8), mapAtUnsafe<string, i32>(values, "left"raw_utf8)),
      mapCount<string, i32>(values)))
}
)";
  const std::string srcPath = writeTemp("vm_stdlib_collection_shim_map_double.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 27);
}

TEST_CASE("rejects vm stdlib collection shim map double type mismatch") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
main() {
  [map<i32, i32>] values{mapDouble<i32, i32>(1i32, 2i32, 3i32, false)}
  return(mapCount<i32, i32>(values))
}
)";
  const std::string srcPath = writeTemp("vm_stdlib_collection_shim_map_double_mismatch.prime", source);
  const std::string errPath =
      (std::filesystem::temp_directory_path() / "primec_vm_stdlib_collection_shim_map_double_mismatch.err")
          .string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main 2> " + errPath;
  CHECK(runCommand(runCmd) == 2);
  CHECK(readFile(errPath).find("map literal value type mismatch") != std::string::npos);
}

TEST_CASE("runs vm with stdlib collection shim map triple") {
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
  const std::string srcPath = writeTemp("vm_stdlib_collection_shim_map_triple.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 17);
}

TEST_CASE("rejects vm stdlib collection shim map triple type mismatch") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
main() {
  [map<i32, i32>] values{mapTriple<i32, i32>(1i32, 2i32, 3i32, 4i32, 5i32, false)}
  return(mapCount<i32, i32>(values))
}
)";
  const std::string srcPath = writeTemp("vm_stdlib_collection_shim_map_triple_mismatch.prime", source);
  const std::string errPath =
      (std::filesystem::temp_directory_path() / "primec_vm_stdlib_collection_shim_map_triple_mismatch.err")
          .string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main 2> " + errPath;
  CHECK(runCommand(runCmd) == 2);
  CHECK(readFile(errPath).find("map literal value type mismatch") != std::string::npos);
}

TEST_CASE("runs vm with stdlib collection shim extended constructors") {
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
  const std::string srcPath = writeTemp("vm_stdlib_collection_shim_extended_ctor.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 59);
}

TEST_CASE("rejects vm stdlib collection shim extended constructor type mismatch") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] pairs{mapTriple<i32, i32>(1i32, 10i32, 2i32, 20i32, 3i32, true)}
  return(mapCount<i32, i32>(pairs))
}
)";
  const std::string srcPath = writeTemp("vm_stdlib_collection_shim_extended_ctor_mismatch.prime", source);
  const std::string errPath =
      (std::filesystem::temp_directory_path() / "primec_vm_stdlib_collection_shim_extended_ctor_mismatch.err")
          .string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main 2> " + errPath;
  CHECK(runCommand(runCmd) == 2);
  CHECK(readFile(errPath).find("map literal value type mismatch") != std::string::npos);
}

TEST_CASE("runs vm with stdlib collection shim vector quint constructor") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vectorQuint<i32>(1i32, 3i32, 5i32, 7i32, 9i32)}
  [i32] picked{plus(vectorAt<i32>(values, 4i32), vectorAtUnsafe<i32>(values, 0i32))}
  return(plus(picked, vectorCount<i32>(values)))
}
)";
  const std::string srcPath = writeTemp("vm_stdlib_collection_shim_vector_quint_ctor.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 15);
}

TEST_CASE("runs vm with stdlib collection shim vector quint bool tail") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vectorQuint<i32>(1i32, 2i32, 3i32, 4i32, true)}
  return(vectorCount<i32>(values))
}
)";
  const std::string srcPath = writeTemp("vm_stdlib_collection_shim_vector_quint_mismatch.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 5);
}

TEST_CASE("runs vm with stdlib collection shim vector sext constructor") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vectorSext<i32>(2i32, 4i32, 6i32, 8i32, 10i32, 12i32)}
  [i32] picked{plus(vectorAt<i32>(values, 5i32), vectorAtUnsafe<i32>(values, 0i32))}
  return(plus(picked, vectorCount<i32>(values)))
}
)";
  const std::string srcPath = writeTemp("vm_stdlib_collection_shim_vector_sext_ctor.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 20);
}

TEST_CASE("runs vm with stdlib collection shim vector sext bool tail") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vectorSext<i32>(1i32, 2i32, 3i32, 4i32, 5i32, false)}
  return(vectorCount<i32>(values))
}
)";
  const std::string srcPath = writeTemp("vm_stdlib_collection_shim_vector_sext_mismatch.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 6);
}

TEST_CASE("runs vm with stdlib collection shim vector sept constructor") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vectorSept<i32>(3i32, 6i32, 9i32, 12i32, 15i32, 18i32, 21i32)}
  [i32] picked{plus(vectorAt<i32>(values, 6i32), vectorAtUnsafe<i32>(values, 0i32))}
  return(plus(picked, vectorCount<i32>(values)))
}
)";
  const std::string srcPath = writeTemp("vm_stdlib_collection_shim_vector_sept_ctor.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 31);
}

TEST_CASE("runs vm with stdlib collection shim vector sept bool tail") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vectorSept<i32>(1i32, 2i32, 3i32, 4i32, 5i32, 6i32, false)}
  return(vectorCount<i32>(values))
}
)";
  const std::string srcPath = writeTemp("vm_stdlib_collection_shim_vector_sept_mismatch.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 7);
}

TEST_CASE("runs vm with stdlib collection shim vector oct constructor") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vectorOct<i32>(4i32, 8i32, 12i32, 16i32, 20i32, 24i32, 28i32, 32i32)}
  [i32] picked{plus(vectorAt<i32>(values, 7i32), vectorAtUnsafe<i32>(values, 0i32))}
  return(plus(picked, vectorCount<i32>(values)))
}
)";
  const std::string srcPath = writeTemp("vm_stdlib_collection_shim_vector_oct_ctor.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 44);
}

TEST_CASE("runs vm with stdlib collection shim vector oct bool tail") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vectorOct<i32>(1i32, 2i32, 3i32, 4i32, 5i32, 6i32, 7i32, false)}
  return(vectorCount<i32>(values))
}
)";
  const std::string srcPath = writeTemp("vm_stdlib_collection_shim_vector_oct_mismatch.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 8);
}

TEST_CASE("runs vm with stdlib collection shim map pair string keys") {
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
  const std::string srcPath = writeTemp("vm_stdlib_collection_shim_map_pair_string_key.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 34);
}

TEST_CASE("rejects vm stdlib collection shim map pair type mismatch") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
main() {
  [map<i32, i32>] values{mapPair<i32, i32>(1i32, 2i32, 3i32, true)}
  return(mapCount<i32, i32>(values))
}
)";
  const std::string srcPath = writeTemp("vm_stdlib_collection_shim_map_pair_mismatch.prime", source);
  const std::string errPath =
      (std::filesystem::temp_directory_path() /
       "primec_vm_stdlib_collection_shim_map_pair_mismatch.err")
          .string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main 2> " + errPath;
  CHECK(runCommand(runCmd) == 2);
  CHECK(readFile(errPath).find("map literal value type mismatch") != std::string::npos);
}

TEST_CASE("runs vm with stdlib collection shim map quad") {
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
  const std::string srcPath = writeTemp("vm_stdlib_collection_shim_map_quad.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 9);
}

TEST_CASE("rejects vm stdlib collection shim map quad type mismatch") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
main() {
  [map<i32, i32>] values{mapQuad<i32, i32>(1i32, 2i32, 3i32, 4i32, 5i32, 6i32, 7i32, false)}
  return(mapCount<i32, i32>(values))
}
)";
  const std::string srcPath = writeTemp("vm_stdlib_collection_shim_map_quad_mismatch.prime", source);
  const std::string errPath =
      (std::filesystem::temp_directory_path() /
       "primec_vm_stdlib_collection_shim_map_quad_mismatch.err")
          .string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main 2> " + errPath;
  CHECK(runCommand(runCmd) == 2);
  CHECK(readFile(errPath).find("map literal value type mismatch") != std::string::npos);
}


TEST_SUITE_END();
