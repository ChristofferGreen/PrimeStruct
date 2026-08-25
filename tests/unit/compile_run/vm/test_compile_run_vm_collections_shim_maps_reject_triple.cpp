#include "../test_compile_run_helpers.h"

#include "../test_compile_run_collection_conformance_helpers.h"
#include "../test_compile_run_container_error_conformance_helpers.h"
#include "../test_compile_run_checked_pointer_conformance_helpers.h"
#include "../test_compile_run_unchecked_pointer_conformance_helpers.h"
#include "test_compile_run_vm_collections_helpers.h"

TEST_SUITE_BEGIN("primestruct.compile.run.vm.collections");

TEST_CASE("rejects vm stdlib collection shim map triple standalone key type mismatch") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
main() {
  [map<i32, i32>] values{mapTriple<i32, i32>(1i32, 2i32, 3i32, 4i32, "oops"raw_utf8, 6i32)}
  return(/std/collections/map/count<i32, i32>(values))
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
  CHECK(error.find("unknown call target: mapTriple") != std::string::npos);
}

TEST_CASE("rejects vm bare stdlib collection shim map quad standalone") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
main() {
  [map<i32, i32>] values{
    mapQuad<i32, i32>(1i32, 3i32, 2i32, 5i32, 3i32, 7i32, 4i32, 11i32)}
  return(plus(plus(/std/collections/map/at<i32, i32>(values, 4i32), /std/collections/map/at_unsafe<i32, i32>(values, 1i32)), /std/collections/map/count<i32, i32>(values)))
}
)";
  const std::string srcPath = writeTemp("vm_stdlib_collection_shim_map_quad_standalone.prime", source);
  const std::string errPath =
      (std::filesystem::temp_directory_path() /
       "primec_vm_stdlib_collection_shim_map_quad_standalone.err")
          .string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main 2> " + errPath;
  CHECK(runCommand(runCmd) == 2);
  CHECK(readFile(errPath).find("unknown call target: mapQuad") != std::string::npos);
}

TEST_CASE("rejects vm bare stdlib collection shim map quad standalone string keys") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
main() {
  [map<string, i32>] values{
    mapQuad<string, i32>("a"raw_utf8, 1i32, "b"raw_utf8, 2i32, "c"raw_utf8, 3i32, "d"raw_utf8, 4i32)}
  return(plus(plus(/std/collections/map/at<string, i32>(values, "d"raw_utf8), /std/collections/map/at_unsafe<string, i32>(values, "a"raw_utf8)),
      /std/collections/map/count<string, i32>(values)))
}
)";
  const std::string srcPath = writeTemp("vm_stdlib_collection_shim_map_quad_standalone_string_key.prime", source);
  const std::string errPath =
      (std::filesystem::temp_directory_path() /
       "primec_vm_stdlib_collection_shim_map_quad_standalone_string_key.err")
          .string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main 2> " + errPath;
  CHECK(runCommand(runCmd) == 2);
  CHECK(readFile(errPath).find("unknown call target: mapQuad") != std::string::npos);
}

TEST_CASE("rejects vm stdlib collection shim map quad standalone type mismatch") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
main() {
  [map<i32, i32>] values{
    mapQuad<i32, i32>(1i32, 2i32, 3i32, 4i32, 5i32, 6i32, 7i32, "bad"raw_utf8)}
  return(/std/collections/map/count<i32, i32>(values))
}
)";
  const std::string srcPath = writeTemp("vm_stdlib_collection_shim_map_quad_standalone_mismatch.prime", source);
  const std::string errPath =
      (std::filesystem::temp_directory_path() /
       "primec_vm_stdlib_collection_shim_map_quad_standalone_mismatch.err")
          .string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main 2> " + errPath;
  CHECK(runCommand(runCmd) == 2);
  const std::string error = readFile(errPath);
  CHECK(error.find("unknown call target: mapQuad") != std::string::npos);
}

TEST_CASE("rejects vm stdlib collection shim map quad standalone key type mismatch") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
main() {
  [map<i32, i32>] values{mapQuad<i32, i32>(1i32, 2i32, 3i32, 4i32, 5i32, 6i32, "oops"raw_utf8, 8i32)}
  return(/std/collections/map/count<i32, i32>(values))
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
  CHECK(error.find("unknown call target: mapQuad") != std::string::npos);
}

TEST_CASE("rejects vm bare stdlib collection shim extended constructors") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{/std/collections/vector/vector<i32>(2i32, 4i32, 6i32, 8i32)}
  [map<i32, i32>] pairs{mapTriple<i32, i32>(1i32, 10i32, 2i32, 20i32, 3i32, 30i32)}
  [i32] vectorTotal{plus(/std/collections/vector/at<i32>(values, 1i32), /std/collections/vector/at_unsafe<i32>(values, 3i32))}
  [i32] mapTotal{plus(/std/collections/map/at<i32, i32>(pairs, 1i32), /std/collections/map/at_unsafe<i32, i32>(pairs, 3i32))}
  return(plus(plus(vectorTotal, mapTotal), plus(/std/collections/vector/count<i32>(values), /std/collections/map/count<i32, i32>(pairs))))
}
)";
  const std::string srcPath = writeTemp("vm_stdlib_collection_shim_extended_ctor.prime", source);
  const std::string errPath =
      (std::filesystem::temp_directory_path() / "primec_vm_stdlib_collection_shim_extended_ctor.err")
          .string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main 2> " + errPath;
  CHECK(runCommand(runCmd) == 2);
  CHECK(readFile(errPath).find("unknown call target: mapTriple") != std::string::npos);
}

TEST_CASE("rejects vm stdlib collection shim extended constructor type mismatch") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] pairs{mapTriple<i32, i32>(1i32, 10i32, 2i32, 20i32, 3i32, "bad"raw_utf8)}
  return(/std/collections/map/count<i32, i32>(pairs))
}
)";
  const std::string srcPath = writeTemp("vm_stdlib_collection_shim_extended_ctor_mismatch.prime", source);
  const std::string errPath =
      (std::filesystem::temp_directory_path() / "primec_vm_stdlib_collection_shim_extended_ctor_mismatch.err")
          .string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + errPath + " 2>&1";
  CHECK(runCommand(runCmd) == 2);
}

TEST_CASE("runs vm with stdlib collection shim vector quint constructor") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{/std/collections/vector/vector<i32>(1i32, 3i32, 5i32, 7i32, 9i32)}
  [i32] picked{plus(/std/collections/vector/at<i32>(values, 4i32), /std/collections/vector/at_unsafe<i32>(values, 0i32))}
  return(plus(picked, /std/collections/vector/count<i32>(values)))
}
)";
  const std::string srcPath = writeTemp("vm_stdlib_collection_shim_vector_quint_ctor.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 15);
}

TEST_CASE("rejects vm stdlib collection shim vector quint type mismatch") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{/std/collections/vector/vector<i32>(1i32, 2i32, 3i32, 4i32, true)}
  return(/std/collections/vector/count<i32>(values))
}
)";
  const std::string srcPath = writeTemp("vm_stdlib_collection_shim_vector_quint_mismatch.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 2);
}

TEST_CASE("runs vm with stdlib collection shim vector oct constructor") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{/std/collections/vector/vector<i32>(4i32, 8i32, 12i32, 16i32, 20i32, 24i32, 28i32, 32i32)}
  [i32] picked{plus(/std/collections/vector/at<i32>(values, 7i32), /std/collections/vector/at_unsafe<i32>(values, 0i32))}
  return(plus(picked, /std/collections/vector/count<i32>(values)))
}
)";
  const std::string srcPath = writeTemp("vm_stdlib_collection_shim_vector_oct_ctor.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 44);
}

TEST_SUITE_END();
