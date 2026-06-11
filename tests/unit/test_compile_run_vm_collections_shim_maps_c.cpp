#include "test_compile_run_helpers.h"

#include "test_compile_run_collection_conformance_helpers.h"
#include "test_compile_run_container_error_conformance_helpers.h"
#include "test_compile_run_checked_pointer_conformance_helpers.h"
#include "test_compile_run_unchecked_pointer_conformance_helpers.h"
#include "test_compile_run_vm_collections_helpers.h"

TEST_SUITE_BEGIN("primestruct.compile.run.vm.collections");

TEST_CASE("rejects vm bare stdlib collection shim map quint") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
main() {
  [map<string, i32>] values{
    mapQuint<string, i32>(
        "a"raw_utf8, 1i32, "b"raw_utf8, 2i32, "c"raw_utf8, 3i32, "d"raw_utf8, 4i32, "e"raw_utf8, 5i32)}
  return(plus(plus(/std/collections/map/at<string, i32>(values, "e"raw_utf8), /std/collections/map/at_unsafe<string, i32>(values, "a"raw_utf8)),
      /std/collections/map/count<string, i32>(values)))
}
)";
  const std::string srcPath = writeTemp("vm_stdlib_collection_shim_map_quint.prime", source);
  const std::string errPath =
      (std::filesystem::temp_directory_path() / "primec_vm_stdlib_collection_shim_map_quint.err")
          .string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main 2> " + errPath;
  CHECK(runCommand(runCmd) == 2);
  CHECK(readFile(errPath).find("unknown call target: mapQuint") != std::string::npos);
}

TEST_CASE("rejects vm stdlib collection shim map quint type mismatch") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
main() {
  [map<i32, i32>] values{
    mapQuint<i32, i32>(1i32, 2i32, 3i32, 4i32, 5i32, 6i32, 7i32, 8i32, 9i32, "bad"raw_utf8)}
  return(/std/collections/map/count<i32, i32>(values))
}
)";
  const std::string srcPath = writeTemp("vm_stdlib_collection_shim_map_quint_mismatch.prime", source);
  const std::string errPath =
      (std::filesystem::temp_directory_path() / "primec_vm_stdlib_collection_shim_map_quint_mismatch.err")
          .string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main 2> " + errPath;
  CHECK(runCommand(runCmd) == 2);
  const std::string error = readFile(errPath);
  CHECK(error.find("unknown call target: mapQuint") != std::string::npos);
}

TEST_CASE("rejects vm bare stdlib collection shim map sext") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
main() {
  [map<string, i32>] values{
    mapSext<string, i32>(
        "a"raw_utf8, 1i32, "b"raw_utf8, 2i32, "c"raw_utf8, 3i32, "d"raw_utf8, 4i32, "e"raw_utf8, 5i32, "f"raw_utf8,
        6i32)}
  return(plus(plus(/std/collections/map/at<string, i32>(values, "f"raw_utf8), /std/collections/map/at_unsafe<string, i32>(values, "a"raw_utf8)),
      /std/collections/map/count<string, i32>(values)))
}
)";
  const std::string srcPath = writeTemp("vm_stdlib_collection_shim_map_sext.prime", source);
  const std::string errPath =
      (std::filesystem::temp_directory_path() / "primec_vm_stdlib_collection_shim_map_sext.err")
          .string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main 2> " + errPath;
  CHECK(runCommand(runCmd) == 2);
  CHECK(readFile(errPath).find("unknown call target: mapSext") != std::string::npos);
}

TEST_CASE("rejects vm stdlib collection shim map sext type mismatch") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
main() {
  [map<i32, i32>] values{
    mapSext<i32, i32>(
        1i32, 2i32, 3i32, 4i32, 5i32, 6i32, 7i32, 8i32, 9i32, 10i32, 11i32, "bad"raw_utf8)}
  return(/std/collections/map/count<i32, i32>(values))
}
)";
  const std::string srcPath = writeTemp("vm_stdlib_collection_shim_map_sext_mismatch.prime", source);
  const std::string errPath =
      (std::filesystem::temp_directory_path() / "primec_vm_stdlib_collection_shim_map_sext_mismatch.err")
          .string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main 2> " + errPath;
  CHECK(runCommand(runCmd) == 2);
  const std::string error = readFile(errPath);
  CHECK(error.find("unknown call target: mapSext") != std::string::npos);
}

TEST_CASE("rejects vm bare stdlib collection shim map sept") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
main() {
  [map<string, i32>] values{
    mapSept<string, i32>(
        "a"raw_utf8, 1i32, "b"raw_utf8, 2i32, "c"raw_utf8, 3i32, "d"raw_utf8, 4i32, "e"raw_utf8, 5i32, "f"raw_utf8,
        6i32, "g"raw_utf8, 7i32)}
  return(plus(plus(/std/collections/map/at<string, i32>(values, "g"raw_utf8), /std/collections/map/at_unsafe<string, i32>(values, "a"raw_utf8)),
      /std/collections/map/count<string, i32>(values)))
}
)";
  const std::string srcPath = writeTemp("vm_stdlib_collection_shim_map_sept.prime", source);
  const std::string errPath =
      (std::filesystem::temp_directory_path() / "primec_vm_stdlib_collection_shim_map_sept.err")
          .string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main 2> " + errPath;
  CHECK(runCommand(runCmd) == 2);
  CHECK(readFile(errPath).find("unknown call target: mapSept") != std::string::npos);
}

TEST_CASE("rejects vm stdlib collection shim map sept type mismatch") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
main() {
  [map<i32, i32>] values{
    mapSept<i32, i32>(
        1i32, 2i32, 3i32, 4i32, 5i32, 6i32, 7i32, 8i32, 9i32, 10i32, 11i32, 12i32, 13i32,
        "bad"raw_utf8)}
  return(/std/collections/map/count<i32, i32>(values))
}
)";
  const std::string srcPath = writeTemp("vm_stdlib_collection_shim_map_sept_mismatch.prime", source);
  const std::string errPath =
      (std::filesystem::temp_directory_path() / "primec_vm_stdlib_collection_shim_map_sept_mismatch.err")
          .string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main 2> " + errPath;
  CHECK(runCommand(runCmd) == 2);
  const std::string error = readFile(errPath);
  CHECK(error.find("unknown call target: mapSept") != std::string::npos);
}

TEST_CASE("rejects vm bare stdlib collection shim map oct") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
main() {
  [map<string, i32>] values{
    mapOct<string, i32>(
        "a"raw_utf8, 1i32, "b"raw_utf8, 2i32, "c"raw_utf8, 3i32, "d"raw_utf8, 4i32, "e"raw_utf8, 5i32, "f"raw_utf8,
        6i32, "g"raw_utf8, 7i32, "h"raw_utf8, 8i32)}
  return(plus(plus(/std/collections/map/at<string, i32>(values, "h"raw_utf8), /std/collections/map/at_unsafe<string, i32>(values, "a"raw_utf8)),
      /std/collections/map/count<string, i32>(values)))
}
)";
  const std::string srcPath = writeTemp("vm_stdlib_collection_shim_map_oct.prime", source);
  const std::string errPath =
      (std::filesystem::temp_directory_path() / "primec_vm_stdlib_collection_shim_map_oct.err")
          .string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main 2> " + errPath;
  CHECK(runCommand(runCmd) == 2);
  CHECK(readFile(errPath).find("unknown call target: mapOct") != std::string::npos);
}

TEST_CASE("rejects vm stdlib collection shim map oct type mismatch") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
main() {
  [map<i32, i32>] values{
    mapOct<i32, i32>(
        1i32, 2i32, 3i32, 4i32, 5i32, 6i32, 7i32, 8i32, 9i32, 10i32, 11i32, 12i32, 13i32, 14i32, 15i32,
        "bad"raw_utf8)}
  return(/std/collections/map/count<i32, i32>(values))
}
)";
  const std::string srcPath = writeTemp("vm_stdlib_collection_shim_map_oct_mismatch.prime", source);
  const std::string errPath =
      (std::filesystem::temp_directory_path() / "primec_vm_stdlib_collection_shim_map_oct_mismatch.err")
          .string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main 2> " + errPath;
  CHECK(runCommand(runCmd) == 2);
  const std::string error = readFile(errPath);
  CHECK(error.find("unknown call target: mapOct") != std::string::npos);
}

TEST_CASE("rejects vm bare stdlib collection shim access helper map source") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{/std/collections/vector/vector<i32>(7i32)}
  [map<i32, i32>] pairs{mapSingle<i32, i32>(3i32, 9i32)}
  [i32] a{/std/collections/vector/at<i32>(values, 0i32)}
  [i32] b{/std/collections/vector/at_unsafe<i32>(values, 0i32)}
  [i32] c{/std/collections/map/at<i32, i32>(pairs, 3i32)}
  [i32] d{/std/collections/map/at_unsafe<i32, i32>(pairs, 3i32)}
  return(plus(plus(a, b), plus(c, d)))
}
)";
  const std::string srcPath = writeTemp("vm_stdlib_collection_shim_access.prime", source);
  const std::string errPath =
      (std::filesystem::temp_directory_path() / "primec_vm_stdlib_collection_shim_access.err")
          .string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main 2> " + errPath;
  CHECK(runCommand(runCmd) == 2);
  CHECK(readFile(errPath).find("unknown call target: mapSingle") != std::string::npos);
}

TEST_CASE("runs vm with stdlib collection shim capacity helper") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{/std/collections/vector/vector<i32>(7i32)}
  reserve(values, 4i32)
  return(/std/collections/vector/capacity<i32>(values))
}
)";
  const std::string srcPath = writeTemp("vm_stdlib_collection_shim_capacity.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 4);
}

TEST_CASE("runs vm with stdlib collection shim vector capacity") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{/std/collections/vector/vector<i32>(9i32)}
  /std/collections/vector/reserve<i32>(values, 5i32)
  return(plus(/std/collections/vector/capacity<i32>(values), /std/collections/vector/count<i32>(values)))
}
)";
  const std::string srcPath = writeTemp("vm_stdlib_collection_shim_vector_capacity.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 6);
}

TEST_CASE("rejects vm stdlib collection shim vector capacity type mismatch") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{/std/collections/vector/vector<i32>(9i32)}
  return(/std/collections/vector/capacity<bool>(values))
}
)";
  const std::string srcPath = writeTemp("vm_stdlib_collection_shim_vector_capacity_mismatch.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_vm_stdlib_collection_shim_vector_capacity_mismatch.err")
          .string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main 2> " + errPath;
  CHECK(runCommand(runCmd) == 2);
  const std::string error = readFile(errPath);
  CHECK(error.find("argument type mismatch for /std/collections/vector/capacity parameter values") !=
        std::string::npos);
}

TEST_CASE("runs vm with stdlib collection shim vector count") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{/std/collections/vector/vector<i32>(2i32, 4i32, 6i32)}
  return(plus(/std/collections/vector/count<i32>(values), 10i32))
}
)";
  const std::string srcPath = writeTemp("vm_stdlib_collection_shim_vector_count.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 13);
}

TEST_CASE("runs vm published vector count and capacity on mutable locals") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{/std/collections/vector/vector<i32>()}
  [i32 mut] total{plus(/std/collections/vector/count<i32>(values), /std/collections/vector/capacity<i32>(values))}
  /std/collections/vector/push(values, 4i32)
  assign(total, plus(total, plus(/std/collections/vector/count<i32>(values), /std/collections/vector/capacity<i32>(values))))
  /std/collections/vector/push(values, 8i32)
  assign(total, plus(total, plus(/std/collections/vector/count<i32>(values), /std/collections/vector/capacity<i32>(values))))
  return(total)
}
)";
  const std::string srcPath = writeTemp("vm_stdlib_vector_count_capacity_mut_local.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 6);
}

TEST_CASE("rejects vm stdlib collection shim vector count type mismatch") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{/std/collections/vector/vector<i32>(2i32, 4i32, 6i32)}
  return(/std/collections/vector/count<bool>(values))
}
)";
  const std::string srcPath = writeTemp("vm_stdlib_collection_shim_vector_count_mismatch.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_vm_stdlib_collection_shim_vector_count_mismatch.err")
          .string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main 2> " + errPath;
  CHECK(runCommand(runCmd) == 2);
  const std::string error = readFile(errPath);
  CHECK(error.find("argument type mismatch for /std/collections/vector/count parameter values") !=
        std::string::npos);
}

TEST_CASE("runs vm with stdlib collection shim vector at") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{/std/collections/vector/vector<i32>(2i32, 4i32, 6i32)}
  return(plus(/std/collections/vector/at<i32>(values, 1i32), /std/collections/vector/count<i32>(values)))
}
)";
  const std::string srcPath = writeTemp("vm_stdlib_collection_shim_vector_at.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 7);
}

TEST_CASE("rejects vm stdlib collection shim vector at type mismatch") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{/std/collections/vector/vector<i32>(2i32, 4i32, 6i32)}
  return(/std/collections/vector/at<bool>(values, 1i32))
}
)";
  const std::string srcPath = writeTemp("vm_stdlib_collection_shim_vector_at_mismatch.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_vm_stdlib_collection_shim_vector_at_mismatch.err")
          .string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main 2> " + errPath;
  CHECK(runCommand(runCmd) == 2);
  const std::string error = readFile(errPath);
  CHECK(error.find("argument type mismatch for /std/collections/vector/at__") !=
        std::string::npos);
  CHECK(error.find("parameter values: expected /std/collections/vector/Vector__") !=
        std::string::npos);
  CHECK(error.find("got vector<i32>") != std::string::npos);
}

TEST_CASE("runs vm with stdlib collection shim vector at unsafe") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{/std/collections/vector/vector<i32>(2i32, 4i32, 6i32)}
  return(plus(/std/collections/vector/at_unsafe<i32>(values, 2i32), /std/collections/vector/count<i32>(values)))
}
)";
  const std::string srcPath = writeTemp("vm_stdlib_collection_shim_vector_at_unsafe.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 9);
}

TEST_CASE("rejects vm stdlib collection shim vector at unsafe type mismatch") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{/std/collections/vector/vector<i32>(2i32, 4i32, 6i32)}
  return(/std/collections/vector/at_unsafe<bool>(values, 2i32))
}
)";
  const std::string srcPath = writeTemp("vm_stdlib_collection_shim_vector_at_unsafe_mismatch.prime", source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_vm_stdlib_collection_shim_vector_at_unsafe_mismatch.err")
          .string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main 2> " + errPath;
  CHECK(runCommand(runCmd) == 2);
  const std::string error = readFile(errPath);
  CHECK(error.find("argument type mismatch for /std/collections/vector/at_unsafe__") !=
        std::string::npos);
  CHECK(error.find("parameter values: expected /std/collections/vector/Vector__") !=
        std::string::npos);
  CHECK(error.find("got vector<i32>") != std::string::npos);
}

TEST_CASE("runs vm with stdlib collection shim vector push") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{/std/collections/vector/vector<i32>()}
  /std/collections/vector/push<i32>(values, 5i32)
  /std/collections/vector/push<i32>(values, 8i32)
  return(plus(/std/collections/vector/at<i32>(values, 1i32), /std/collections/vector/count<i32>(values)))
}
)";
  const std::string srcPath = writeTemp("vm_stdlib_collection_shim_vector_push.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 10);
}

TEST_CASE("rejects vm stdlib collection shim vector push type mismatch") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{/std/collections/vector/vector<i32>()}
  /std/collections/vector/push<bool>(values, true)
  return(/std/collections/vector/count<i32>(values))
  }
)";
  const std::string srcPath = writeTemp("vm_stdlib_collection_shim_vector_push_mismatch.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_vm_stdlib_collection_shim_vector_push_mismatch.err")
          .string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main 2> " + errPath;
  CHECK(runCommand(runCmd) == 2);
  const std::string error = readFile(errPath);
  CHECK(error.find("argument type mismatch for /std/collections/vector/push__") !=
        std::string::npos);
  CHECK(error.find("parameter values: expected /std/collections/vector/Vector__") !=
        std::string::npos);
  CHECK(error.find("got vector<i32>") != std::string::npos);
}

TEST_CASE("runs vm with stdlib collection shim vector pop") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{/std/collections/vector/vector<i32>(2i32, 4i32, 6i32)}
  /std/collections/vector/pop<i32>(values)
  return(plus(/std/collections/vector/at<i32>(values, 1i32), /std/collections/vector/count<i32>(values)))
}
)";
  const std::string srcPath = writeTemp("vm_stdlib_collection_shim_vector_pop.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 6);
}

TEST_CASE("rejects vm stdlib collection shim vector pop type mismatch") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{/std/collections/vector/vector<i32>(2i32, 4i32, 6i32)}
  /std/collections/vector/pop<bool>(values)
  return(/std/collections/vector/count<i32>(values))
}
)";
  const std::string srcPath = writeTemp("vm_stdlib_collection_shim_vector_pop_mismatch.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_vm_stdlib_collection_shim_vector_pop_mismatch.err")
          .string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main 2> " + errPath;
  CHECK(runCommand(runCmd) == 2);
  const std::string error = readFile(errPath);
  CHECK(error.find("argument type mismatch for /std/collections/vector/pop__") !=
        std::string::npos);
  CHECK(error.find("parameter values: expected /std/collections/vector/Vector__") !=
        std::string::npos);
  CHECK(error.find("got vector<i32>") != std::string::npos);
}

TEST_CASE("runs vm with stdlib collection shim vector reserve") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{/std/collections/vector/vector<i32>()}
  /std/collections/vector/reserve<i32>(values, 6i32)
  return(plus(/std/collections/vector/capacity<i32>(values), /std/collections/vector/count<i32>(values)))
}
)";
  const std::string srcPath = writeTemp("vm_stdlib_collection_shim_vector_reserve.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 6);
}

TEST_CASE("rejects vm stdlib collection shim vector reserve type mismatch") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{/std/collections/vector/vector<i32>()}
  /std/collections/vector/reserve<bool>(values, 6i32)
  return(/std/collections/vector/count<i32>(values))
  }
)";
  const std::string srcPath = writeTemp("vm_stdlib_collection_shim_vector_reserve_mismatch.prime", source);
  const std::string errPath =
      (std::filesystem::temp_directory_path() / "primec_vm_stdlib_collection_shim_vector_reserve_mismatch.err")
          .string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main 2> " + errPath;
  CHECK(runCommand(runCmd) == 2);
  const std::string error = readFile(errPath);
  CHECK(error.find("argument type mismatch for /std/collections/vector/reserve__") !=
        std::string::npos);
  CHECK(error.find("parameter values: expected /std/collections/vector/Vector__") !=
        std::string::npos);
  CHECK(error.find("got vector<i32>") != std::string::npos);
}

TEST_CASE("runs vm with stdlib collection shim vector clear") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{/std/collections/vector/vector<i32>(2i32, 4i32, 6i32)}
  /std/collections/vector/clear<i32>(values)
  return(plus(/std/collections/vector/count<i32>(values), 1i32))
}
)";
  const std::string srcPath = writeTemp("vm_stdlib_collection_shim_vector_clear.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 1);
}

TEST_CASE("rejects vm stdlib collection shim vector clear type mismatch") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{/std/collections/vector/vector<i32>(2i32, 4i32, 6i32)}
  /std/collections/vector/clear<bool>(values)
  return(/std/collections/vector/count<i32>(values))
  }
)";
  const std::string srcPath = writeTemp("vm_stdlib_collection_shim_vector_clear_mismatch.prime", source);
  const std::string errPath =
      (std::filesystem::temp_directory_path() / "primec_vm_stdlib_collection_shim_vector_clear_mismatch.err")
          .string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main 2> " + errPath;
  CHECK(runCommand(runCmd) == 2);
  const std::string error = readFile(errPath);
  CHECK(error.find("argument type mismatch for /std/collections/vector/clear__") !=
        std::string::npos);
  CHECK(error.find("parameter values: expected /std/collections/vector/Vector__") !=
        std::string::npos);
  CHECK(error.find("got vector<i32>") != std::string::npos);
}

TEST_CASE("runs vm with stdlib collection shim vector remove at") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{/std/collections/vector/vector<i32>(2i32, 4i32, 6i32)}
  /std/collections/vector/remove_at<i32>(values, 1i32)
  return(plus(/std/collections/vector/at<i32>(values, 1i32), /std/collections/vector/count<i32>(values)))
}
)";
  const std::string srcPath = writeTemp("vm_stdlib_collection_shim_vector_remove_at.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 8);
}

TEST_CASE("rejects vm stdlib collection shim vector remove at type mismatch") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{/std/collections/vector/vector<i32>(2i32, 4i32, 6i32)}
  /std/collections/vector/remove_at<bool>(values, 1i32)
  return(/std/collections/vector/count<i32>(values))
}
)";
  const std::string srcPath = writeTemp("vm_stdlib_collection_shim_vector_remove_at_mismatch.prime", source);
  const std::string errPath =
      (std::filesystem::temp_directory_path() / "primec_vm_stdlib_collection_shim_vector_remove_at_mismatch.err")
          .string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main 2> " + errPath;
  CHECK(runCommand(runCmd) == 2);
  const std::string error = readFile(errPath);
  CHECK(error.find("argument type mismatch for /std/collections/vector/remove_at__") !=
        std::string::npos);
  CHECK(error.find("parameter values: expected /std/collections/vector/Vector__") !=
        std::string::npos);
  CHECK(error.find("got vector<i32>") != std::string::npos);
}

TEST_CASE("runs vm with stdlib collection shim vector remove swap") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{/std/collections/vector/vector<i32>(2i32, 4i32, 6i32)}
  /std/collections/vector/remove_swap<i32>(values, 0i32)
  return(plus(/std/collections/vector/at<i32>(values, 0i32), /std/collections/vector/count<i32>(values)))
}
)";
  const std::string srcPath = writeTemp("vm_stdlib_collection_shim_vector_remove_swap.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 8);
}

TEST_CASE("rejects vm stdlib collection shim vector remove swap type mismatch") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{/std/collections/vector/vector<i32>(2i32, 4i32, 6i32)}
  /std/collections/vector/remove_swap<bool>(values, 0i32)
  return(/std/collections/vector/count<i32>(values))
}
)";
  const std::string srcPath = writeTemp("vm_stdlib_collection_shim_vector_remove_swap_mismatch.prime", source);
  const std::string errPath =
      (std::filesystem::temp_directory_path() /
       "primec_vm_stdlib_collection_shim_vector_remove_swap_mismatch.err")
          .string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main 2> " + errPath;
  CHECK(runCommand(runCmd) == 2);
  const std::string error = readFile(errPath);
  CHECK(error.find("argument type mismatch for /std/collections/vector/remove_swap__") !=
        std::string::npos);
  CHECK(error.find("parameter values: expected /std/collections/vector/Vector__") !=
        std::string::npos);
  CHECK(error.find("got vector<i32>") != std::string::npos);
}

TEST_CASE("runs vm with stdlib collection shim vector mutators") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{/std/collections/vector/vector<i32>()}
  /std/collections/vector/reserve<i32>(values, 4i32)
  /std/collections/vector/push<i32>(values, 11i32)
  /std/collections/vector/push<i32>(values, 22i32)
  /std/collections/vector/push<i32>(values, 33i32)
  [i32 mut] snapshot{plus(/std/collections/vector/count<i32>(values), /std/collections/vector/capacity<i32>(values))}
  /std/collections/vector/pop<i32>(values)
  /std/collections/vector/remove_at<i32>(values, 0i32)
  /std/collections/vector/push<i32>(values, 44i32)
  /std/collections/vector/remove_swap<i32>(values, 0i32)
  assign(snapshot, plus(snapshot, /std/collections/vector/count<i32>(values)))
  /std/collections/vector/clear<i32>(values)
  return(plus(snapshot, /std/collections/vector/count<i32>(values)))
}
)";
  const std::string srcPath = writeTemp("vm_stdlib_collection_shim_vector_mutators.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 8);
}

TEST_CASE("runs vm with bare vector capacity through imported stdlib helper") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32, 3i32)}
  [i32] a{/std/collections/vector/capacity<i32>(values)}
  return(plus(a, /std/collections/vector/capacity<i32>(values)))
}
)";
  const std::string srcPath = writeTemp("vm_vector_capacity_helper.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 6);
}

TEST_CASE("rejects vm bare vector capacity without imported helper") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32, 3i32)}
  return(capacity(values))
}
)";
  const std::string srcPath = writeTemp("vm_vector_capacity_import_requirement.prime", source);
  const std::string errPath =
      (std::filesystem::temp_directory_path() / "primec_vm_vector_capacity_import_requirement_err.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main 2> " + errPath;
  CHECK(runCommand(runCmd) == 2);
  CHECK(readFile(errPath).find("unknown call target: /std/collections/vector/capacity") != std::string::npos);
}

TEST_CASE("runs vm bare vector capacity method without imported helper") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 2i32, 3i32)}
  return(values.capacity())
}
)";
  const std::string srcPath = writeTemp("vm_vector_capacity_method_import_requirement.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 3);
}

TEST_CASE("rejects vm wrapper temporary vector capacity method without helper") {
  const std::string source = R"(
[effects(heap_alloc), return<vector<i32>>]
wrapVector() {
  return(vector<i32>(1i32, 2i32, 3i32))
}

[effects(heap_alloc), return<int>]
main() {
  return(wrapVector().capacity())
}
)";
  const std::string srcPath = writeTemp("vm_wrapper_vector_capacity_method_import_requirement.prime", source);
  const std::string errPath =
      (std::filesystem::temp_directory_path() /
       "primec_vm_wrapper_vector_capacity_method_import_requirement_err.txt")
          .string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main 2> " + errPath;
  CHECK(runCommand(runCmd) == 2);
  CHECK(readFile(errPath).find("unknown method: /vector/capacity") != std::string::npos);
}

TEST_CASE("runs vm bare vector capacity after pop through imported stdlib helper") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32, 2i32, 3i32)}
  pop(values)
  return(capacity(values))
}
)";
  const std::string srcPath = writeTemp("vm_vector_capacity_after_pop.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 3);
}

TEST_CASE("runs vm bare vector mutators without imported helpers") {
  expectBareVectorMutatorImportRequirement("vm", "push", "values, 7i32");
  expectBareVectorMutatorImportRequirement("vm", "pop", "values");
  expectBareVectorMutatorImportRequirement("vm", "reserve", "values, 8i32");
  expectBareVectorMutatorImportRequirement("vm", "clear", "values");
  expectBareVectorMutatorImportRequirement("vm", "remove_at", "values, 1i32");
  expectBareVectorMutatorImportRequirement("vm", "remove_swap", "values, 1i32");
}

TEST_CASE("rejects vm bare vector mutator methods without imported helpers") {
  expectBareVectorMutatorMethodImportRequirement("vm", "push", "7i32");
  expectBareVectorMutatorMethodImportRequirement("vm", "pop", "");
  expectBareVectorMutatorMethodImportRequirement("vm", "reserve", "8i32");
  expectBareVectorMutatorMethodImportRequirement("vm", "clear", "");
  expectBareVectorMutatorMethodImportRequirement("vm", "remove_at", "1i32");
  expectBareVectorMutatorMethodImportRequirement("vm", "remove_swap", "1i32");
}

TEST_CASE("runs vm with user array count method shadow") {
  const std::string source = R"(
[return<int>]
/array/count([array<i32>] values) {
  return(99i32)
}

[return<int>]
main() {
  [array<i32>] values{array<i32>(1i32, 2i32)}
  return(values.count())
}
)";
  const std::string srcPath = writeTemp("vm_user_array_count_method_shadow.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 2);
}

TEST_SUITE_END();
