#include "test_compile_run_helpers.h"

#include "test_compile_run_collection_conformance_helpers.h"
#include "test_compile_run_container_error_conformance_helpers.h"
#include "test_compile_run_checked_pointer_conformance_helpers.h"
#include "test_compile_run_unchecked_pointer_conformance_helpers.h"
#include "test_compile_run_vm_collections_helpers.h"

TEST_SUITE_BEGIN("primestruct.compile.run.vm.collections");

TEST_CASE("rejects vm stdlib collection shim vector new type mismatch") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vectorNew<bool>()}
  return(vectorCount<i32>(values))
}
)";
  const std::string srcPath = writeTemp("vm_stdlib_collection_shim_vector_new_mismatch.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 0);
}

TEST_CASE("rejects vm stdlib collection shim vector single type mismatch") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vectorSingle<i32>(false)}
  return(vectorCount<i32>(values))
}
)";
  const std::string srcPath = writeTemp("vm_stdlib_collection_shim_vector_single_mismatch.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 1);
}

TEST_CASE("runs vm with stdlib collection shim vector pair") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vectorPair<i32>(11i32, 13i32)}
  return(plus(plus(vectorAt<i32>(values, 1i32), vectorAtUnsafe<i32>(values, 0i32)), vectorCount<i32>(values)))
}
)";
  const std::string srcPath = writeTemp("vm_stdlib_collection_shim_vector_pair.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 26);
}

TEST_CASE("rejects vm stdlib collection shim vector pair type mismatch") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vectorPair<i32>(1i32, false)}
  return(vectorCount<i32>(values))
}
)";
  const std::string srcPath = writeTemp("vm_stdlib_collection_shim_vector_pair_mismatch.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 2);
}

TEST_CASE("runs vm with stdlib collection shim vector triple") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vectorTriple<i32>(10i32, 20i32, 30i32)}
  return(plus(plus(vectorAt<i32>(values, 2i32), vectorAtUnsafe<i32>(values, 0i32)), vectorCount<i32>(values)))
}
)";
  const std::string srcPath = writeTemp("vm_stdlib_collection_shim_vector_triple.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 43);
}

TEST_CASE("rejects vm stdlib collection shim vector triple type mismatch") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vectorTriple<i32>(1i32, 2i32, false)}
  return(vectorCount<i32>(values))
}
)";
  const std::string srcPath = writeTemp("vm_stdlib_collection_shim_vector_triple_mismatch.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 3);
}

TEST_CASE("runs vm with stdlib collection shim vector quad") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vectorQuad<i32>(3i32, 6i32, 9i32, 12i32)}
  return(plus(plus(vectorAt<i32>(values, 3i32), vectorAtUnsafe<i32>(values, 0i32)), vectorCount<i32>(values)))
}
)";
  const std::string srcPath = writeTemp("vm_stdlib_collection_shim_vector_quad.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 19);
}

TEST_CASE("rejects vm stdlib collection shim vector quad type mismatch") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vectorQuad<i32>(1i32, 2i32, 3i32, false)}
  return(vectorCount<i32>(values))
}
)";
  const std::string srcPath = writeTemp("vm_stdlib_collection_shim_vector_quad_mismatch.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 4);
}

TEST_CASE("runs vm with stdlib collection shim map single") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
main() {
  [map<string, i32>] values{mapSingle<string, i32>("only"raw_utf8, 21i32)}
  return(plus(mapAt<string, i32>(values, "only"raw_utf8), mapCount<string, i32>(values)))
}
)";
  const std::string srcPath = writeTemp("vm_stdlib_collection_shim_map_single.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 22);
}

TEST_CASE("rejects vm stdlib collection shim map single type mismatch") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
main() {
  [map<i32, i32>] values{mapSingle<i32, i32>(1i32, false)}
  return(mapCount<i32, i32>(values))
}
)";
  const std::string srcPath = writeTemp("vm_stdlib_collection_shim_map_single_mismatch.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 2);
}

TEST_CASE("rejects vm stdlib collection shim map single key type mismatch") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
main() {
  [map<i32, i32>] values{mapSingle<i32, i32>("oops"raw_utf8, 4i32)}
  return(mapCount<i32, i32>(values))
}
)";
  const std::string srcPath = writeTemp("vm_stdlib_collection_shim_map_single_key_mismatch.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 2);
}

TEST_CASE("runs vm with stdlib collection shim map new") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
main() {
  [map<i32, i32>] values{mapNew<i32, i32>()}
  return(plus(mapCount<i32, i32>(values), 1i32))
}
)";
  const std::string srcPath = writeTemp("vm_stdlib_collection_shim_map_new.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 1);
}

TEST_CASE("runs vm with stdlib collection shim map new string key envelope") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
main() {
  [map<string, i32>] values{mapNew<string, i32>()}
  return(plus(mapCount<string, i32>(values), 1i32))
}
)";
  const std::string srcPath = writeTemp("vm_stdlib_collection_shim_map_new_string.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 1);
}

TEST_CASE("rejects vm stdlib collection shim map new type mismatch") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
main() {
  [map<i32, i32>] values{mapNew<bool, i32>()}
  return(mapCount<i32, i32>(values))
}
)";
  const std::string srcPath = writeTemp("vm_stdlib_collection_shim_map_new_mismatch.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 0);
}

TEST_CASE("rejects vm stdlib collection shim map new string key type mismatch") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
main() {
  [map<i32, i32>] values{mapNew<string, i32>()}
  return(mapCount<i32, i32>(values))
}
)";
  const std::string srcPath = writeTemp("vm_stdlib_collection_shim_map_new_string_mismatch.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 0);
}

TEST_CASE("runs vm with stdlib collection shim map count") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
main() {
  [map<i32, i32>] values{mapTriple<i32, i32>(1i32, 10i32, 2i32, 20i32, 3i32, 30i32)}
  return(plus(mapCount<i32, i32>(values), 9i32))
}
)";
  const std::string srcPath = writeTemp("vm_stdlib_collection_shim_map_count.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 12);
}

TEST_CASE("runs vm with stdlib collection shim map count string keys") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
main() {
  [map<string, i32>] values{mapDouble<string, i32>("left"raw_utf8, 10i32, "right"raw_utf8, 15i32)}
  return(plus(mapCount<string, i32>(values), 9i32))
}
)";
  const std::string srcPath = writeTemp("vm_stdlib_collection_shim_map_count_string_key.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 11);
}

TEST_CASE("rejects vm stdlib collection shim map count type mismatch") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
main() {
  [map<i32, i32>] values{mapTriple<i32, i32>(1i32, 10i32, 2i32, 20i32, 3i32, 30i32)}
  return(mapCount<bool, i32>(values))
}
)";
  const std::string srcPath = writeTemp("vm_stdlib_collection_shim_map_count_mismatch.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 2);
}

TEST_CASE("rejects vm stdlib collection shim map count string key type mismatch") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
main() {
  [map<string, i32>] values{mapDouble<string, i32>("left"raw_utf8, 10i32, "right"raw_utf8, 15i32)}
  return(mapCount<i32, i32>(values))
}
)";
  const std::string srcPath = writeTemp("vm_stdlib_collection_shim_map_count_string_key_mismatch.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 2);
}

TEST_CASE("runs vm with stdlib collection shim map at") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
main() {
  [map<i32, i32>] values{mapTriple<i32, i32>(1i32, 10i32, 2i32, 20i32, 3i32, 30i32)}
  return(plus(mapAt<i32, i32>(values, 2i32), mapCount<i32, i32>(values)))
}
)";
  const std::string srcPath = writeTemp("vm_stdlib_collection_shim_map_at.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 23);
}

TEST_CASE("runs vm with stdlib collection shim map at string keys") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
main() {
  [map<string, i32>] values{mapDouble<string, i32>("left"raw_utf8, 10i32, "right"raw_utf8, 15i32)}
  return(plus(mapAt<string, i32>(values, "right"raw_utf8), mapCount<string, i32>(values)))
}
)";
  const std::string srcPath = writeTemp("vm_stdlib_collection_shim_map_at_string_key.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 17);
}

TEST_CASE("rejects vm stdlib collection shim map at type mismatch") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
main() {
  [map<i32, i32>] values{mapTriple<i32, i32>(1i32, 10i32, 2i32, 20i32, 3i32, 30i32)}
  return(mapAt<bool, i32>(values, true))
}
)";
  const std::string srcPath = writeTemp("vm_stdlib_collection_shim_map_at_mismatch.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 2);
}

TEST_CASE("rejects vm stdlib collection shim map at string key type mismatch") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
main() {
  [map<string, i32>] values{mapDouble<string, i32>("left"raw_utf8, 10i32, "right"raw_utf8, 15i32)}
  return(mapAt<string, i32>(values, 1i32))
}
)";
  const std::string srcPath = writeTemp("vm_stdlib_collection_shim_map_at_string_key_mismatch.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 2);
}

TEST_CASE("runs vm with stdlib collection shim map at unsafe") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
main() {
  [map<i32, i32>] values{mapTriple<i32, i32>(1i32, 10i32, 2i32, 20i32, 3i32, 30i32)}
  return(plus(mapAtUnsafe<i32, i32>(values, 3i32), mapCount<i32, i32>(values)))
}
)";
  const std::string srcPath = writeTemp("vm_stdlib_collection_shim_map_at_unsafe.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 33);
}

TEST_CASE("runs vm with stdlib collection shim map at unsafe string keys") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
main() {
  [map<string, i32>] values{mapDouble<string, i32>("left"raw_utf8, 10i32, "right"raw_utf8, 15i32)}
  return(plus(mapAtUnsafe<string, i32>(values, "left"raw_utf8), mapCount<string, i32>(values)))
}
)";
  const std::string srcPath = writeTemp("vm_stdlib_collection_shim_map_at_unsafe_string_key.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 12);
}

TEST_CASE("rejects vm stdlib collection shim map at unsafe type mismatch") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
main() {
  [map<i32, i32>] values{mapTriple<i32, i32>(1i32, 10i32, 2i32, 20i32, 3i32, 30i32)}
  return(mapAtUnsafe<bool, i32>(values, true))
}
)";
  const std::string srcPath = writeTemp("vm_stdlib_collection_shim_map_at_unsafe_mismatch.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 2);
}

TEST_CASE("rejects vm stdlib collection shim map at unsafe string key type mismatch") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
main() {
  [map<string, i32>] values{mapDouble<string, i32>("left"raw_utf8, 10i32, "right"raw_utf8, 15i32)}
  return(mapAtUnsafe<string, i32>(values, 1i32))
}
)";
  const std::string srcPath = writeTemp("vm_stdlib_collection_shim_map_at_unsafe_string_key_mismatch.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 2);
}

TEST_CASE("runs vm with stdlib collection shim map method access string keys") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
main() {
  [map<string, i32>] values{mapDouble<string, i32>("left"raw_utf8, 10i32, "right"raw_utf8, 15i32)}
  return(plus(plus(values.at("right"raw_utf8), values.at_unsafe("left"raw_utf8)), values.count()))
}
)";
  const std::string srcPath = writeTemp("vm_stdlib_collection_shim_map_method_access_string_key.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 27);
}

TEST_CASE("rejects vm stdlib collection shim map method at string key type mismatch") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
main() {
  [map<string, i32>] values{mapDouble<string, i32>("left"raw_utf8, 10i32, "right"raw_utf8, 15i32)}
  return(values.at(1i32))
}
)";
  const std::string srcPath =
      writeTemp("vm_stdlib_collection_shim_map_method_at_string_key_mismatch.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 2);
}

TEST_CASE("rejects vm stdlib collection shim map method at unsafe string key type mismatch") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
main() {
  [map<string, i32>] values{mapDouble<string, i32>("left"raw_utf8, 10i32, "right"raw_utf8, 15i32)}
  return(values.at_unsafe(1i32))
}
)";
  const std::string srcPath =
      writeTemp("vm_stdlib_collection_shim_map_method_at_unsafe_string_key_mismatch.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 2);
}

TEST_CASE("runs vm with stdlib collection shim map method call parity string keys") {
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
  const std::string srcPath = writeTemp("vm_stdlib_collection_shim_map_method_call_parity_string_key.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 54);
}

TEST_CASE("rejects vm stdlib collection shim map method call parity key type mismatch") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
main() {
  [map<string, i32>] values{mapDouble<string, i32>("left"raw_utf8, 10i32, "right"raw_utf8, 15i32)}
  return(plus(mapAt<string, i32>(values, 1i32), values.at(1i32)))
}
)";
  const std::string srcPath =
      writeTemp("vm_stdlib_collection_shim_map_method_call_parity_key_type_mismatch.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 2);
}

TEST_CASE("rejects vm stdlib collection shim map method call parity unsafe key type mismatch") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
main() {
  [map<string, i32>] values{mapDouble<string, i32>("left"raw_utf8, 10i32, "right"raw_utf8, 15i32)}
  return(plus(mapAtUnsafe<string, i32>(values, 1i32), values.at_unsafe(1i32)))
}
)";
  const std::string srcPath =
      writeTemp("vm_stdlib_collection_shim_map_method_call_parity_unsafe_key_type_mismatch.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 2);
}

TEST_CASE("runs vm with stdlib collection shim map single standalone string keys") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
main() {
  [map<string, i32>] values{mapSingle<string, i32>("only"raw_utf8, 21i32)}
  return(plus(plus(mapAt<string, i32>(values, "only"raw_utf8), mapAtUnsafe<string, i32>(values, "only"raw_utf8)),
      mapCount<string, i32>(values)))
}
)";
  const std::string srcPath = writeTemp("vm_stdlib_collection_shim_map_single_standalone_string_key.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 43);
}

TEST_CASE("rejects vm stdlib collection shim map single standalone key type mismatch") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
main() {
  [map<i32, i32>] values{mapSingle<i32, i32>("oops"raw_utf8, 4i32)}
  return(mapCount<i32, i32>(values))
}
)";
  const std::string srcPath = writeTemp("vm_stdlib_collection_shim_map_single_standalone_key_mismatch.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 2);
}

TEST_CASE("runs vm with stdlib collection shim map pair standalone") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
main() {
  [map<i32, i32>] values{mapPair<i32, i32>(1i32, 7i32, 2i32, 10i32)}
  return(plus(plus(mapAt<i32, i32>(values, 2i32), mapAtUnsafe<i32, i32>(values, 1i32)), mapCount<i32, i32>(values)))
}
)";
  const std::string srcPath = writeTemp("vm_stdlib_collection_shim_map_pair_standalone.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 19);
}

TEST_CASE("rejects vm stdlib collection shim map pair standalone type mismatch") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
main() {
  [map<i32, i32>] values{mapPair<i32, i32>(1i32, 2i32, 3i32, false)}
  return(mapCount<i32, i32>(values))
}
)";
  const std::string srcPath = writeTemp("vm_stdlib_collection_shim_map_pair_standalone_mismatch.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 2);
}

TEST_CASE("runs vm with stdlib collection shim map pair standalone string keys") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
main() {
  [map<string, i32>] values{mapPair<string, i32>("alpha"raw_utf8, 12i32, "beta"raw_utf8, 20i32)}
  return(plus(plus(mapAt<string, i32>(values, "beta"raw_utf8), mapAtUnsafe<string, i32>(values, "alpha"raw_utf8)),
      mapCount<string, i32>(values)))
}
)";
  const std::string srcPath = writeTemp("vm_stdlib_collection_shim_map_pair_standalone_string_key.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 34);
}

TEST_CASE("rejects vm stdlib collection shim map pair standalone key type mismatch") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
main() {
  [map<i32, i32>] values{mapPair<i32, i32>(1i32, 2i32, "oops"raw_utf8, 4i32)}
  return(mapCount<i32, i32>(values))
}
)";
  const std::string srcPath = writeTemp("vm_stdlib_collection_shim_map_pair_standalone_key_mismatch.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 2);
}

TEST_CASE("runs vm with stdlib collection shim map double standalone string keys") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
main() {
  [map<string, i32>] values{mapDouble<string, i32>("left"raw_utf8, 10i32, "right"raw_utf8, 15i32)}
  return(plus(plus(mapAt<string, i32>(values, "right"raw_utf8), mapAtUnsafe<string, i32>(values, "left"raw_utf8)),
      mapCount<string, i32>(values)))
}
)";
  const std::string srcPath = writeTemp("vm_stdlib_collection_shim_map_double_standalone_string_key.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 27);
}

TEST_CASE("rejects vm stdlib collection shim map double standalone key type mismatch") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
main() {
  [map<i32, i32>] values{mapDouble<i32, i32>(1i32, 2i32, "oops"raw_utf8, 4i32)}
  return(mapCount<i32, i32>(values))
}
)";
  const std::string srcPath = writeTemp("vm_stdlib_collection_shim_map_double_standalone_key_mismatch.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 2);
}

TEST_CASE("runs vm with stdlib collection shim map triple standalone string keys") {
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
  const std::string srcPath = writeTemp("vm_stdlib_collection_shim_map_triple_standalone_string_key.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 7);
}


TEST_SUITE_END();
