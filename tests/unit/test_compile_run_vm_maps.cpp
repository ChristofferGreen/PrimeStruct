#include "test_compile_run_helpers.h"

TEST_SUITE_BEGIN("primestruct.compile.run.vm.maps");

TEST_CASE("runs vm with map literal") {
  const std::string source = R"(
[return<int>]
main() {
  map<i32, i32>{1i32=2i32, 3i32=4i32}
  return(1i32)
}
)";
  const std::string srcPath = writeTemp("vm_map_literal.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 1);
}

TEST_CASE("runs vm with map literal count helper") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
main() {
  return(count(map<i32, i32>{1i32=2i32, 3i32=4i32}))
}
)";
  const std::string srcPath = writeTemp("vm_map_literal_count.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 2);
}

TEST_CASE("runs vm with map count helper") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>{1i32=2i32, 3i32=4i32}}
  return(count(values))
}
)";
  const std::string srcPath = writeTemp("vm_map_count_helper.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 2);
}

TEST_CASE("runs vm with map method call") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
/map/size([map<i32, i32>] items) {
  return(count(items))
}

[return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>{1i32=2i32, 3i32=4i32}}
  return(values.size())
}
)";
  const std::string srcPath = writeTemp("vm_map_method_call.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 2);
}

TEST_CASE("rejects vm map indexing sugar without canonical helper") {
  const std::string source = R"(
[return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>{1i32=2i32, 3i32=4i32}}
  return(values[3i32])
}
)";
  const std::string srcPath = writeTemp("vm_map_indexing.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_vm_map_indexing_err.txt").string();
  const std::string runCmd =
      "./primec --emit=vm " + srcPath + " --entry /main 2> " + errPath;
  CHECK(runCommand(runCmd) == 2);
  CHECK(readFile(errPath).find("unknown call target: /std/collections/map/at") !=
        std::string::npos);
}

TEST_CASE("runs vm with map at_unsafe helper") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>{1i32=2i32, 3i32=4i32}}
  return(at_unsafe(values, 1i32))
}
)";
  const std::string srcPath = writeTemp("vm_map_at_unsafe.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 2);
}

TEST_CASE("rejects vm bool map access helpers without canonical helper") {
  const std::string source = R"(
[return<int>]
main() {
  [map<bool, i32>] values{map<bool, i32>{true=1i32, false=2i32}}
  return(plus(at(values, true), at_unsafe(values, false)))
}
)";
  const std::string srcPath = writeTemp("vm_map_bool_access.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_vm_map_bool_access_err.txt").string();
  const std::string runCmd =
      "./primec --emit=vm " + srcPath + " --entry /main 2> " + errPath;
  CHECK(runCommand(runCmd) == 2);
  CHECK(readFile(errPath).find("unknown call target: /std/collections/map/at") !=
        std::string::npos);
}

TEST_CASE("rejects vm u64 map access helpers without canonical helper") {
  const std::string source = R"(
[return<int>]
main() {
  [map<u64, i32>] values{map<u64, i32>{2u64=7i32, 11u64=5i32}}
  return(plus(at(values, 2u64), at_unsafe(values, 11u64)))
}
)";
  const std::string srcPath = writeTemp("vm_map_u64_access.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_vm_map_u64_access_err.txt").string();
  const std::string runCmd =
      "./primec --emit=vm " + srcPath + " --entry /main 2> " + errPath;
  CHECK(runCommand(runCmd) == 2);
  CHECK(readFile(errPath).find("unknown call target: /std/collections/map/at") !=
        std::string::npos);
}

TEST_CASE("rejects vm map literal odd args") {
  const std::string source = R"(
[return<int>]
main() {
  map<i32, i32>(1i32)
  return(1i32)
}
)";
  const std::string srcPath = writeTemp("vm_map_literal_odd.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 2);
}

TEST_CASE("rejects vm map literal type mismatch") {
  const std::string source = R"(
[return<int>]
main() {
  map<i32, i32>(1i32, true)
  return(1i32)
}
)";
  const std::string srcPath = writeTemp("vm_map_literal_mismatch.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 2);
}

TEST_CASE("runs vm with map literal string binding key") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
main() {
  [string] key{"b"raw_utf8}
  [map<string, i32>] values{map<string, i32>(key, 2i32, "a"raw_utf8, 1i32)}
  return(at(values, key))
}
)";
  const std::string srcPath = writeTemp("vm_map_literal_string_binding_key.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 2);
}

TEST_CASE("runs vm with string-keyed map indexing sugar") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
main() {
  [map<string, i32>] values{map<string, i32>("a"raw_utf8, 1i32, "b"raw_utf8, 2i32)}
  return(values["b"raw_utf8])
}
)";
  const std::string srcPath = writeTemp("vm_map_indexing_string_key.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 2);
}

TEST_CASE("runs vm with string-keyed map indexing binding key") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
main() {
  [map<string, i32>] values{map<string, i32>("a"raw_utf8, 1i32, "b"raw_utf8, 2i32)}
  [string] key{"b"raw_utf8}
  return(values[key])
}
)";
  const std::string srcPath = writeTemp("vm_map_indexing_string_binding.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 2);
}

TEST_CASE("rejects vm map literal string key from argv binding") {
  const std::string source = R"(
[return<int>]
main([array<string>] args) {
  [string] key{args[0i32]}
  map<string, i32>(key, 1i32)
  return(1i32)
}
)";
  const std::string srcPath = writeTemp("vm_map_literal_string_argv_key.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_vm_map_literal_string_argv_key_err.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main 2> " + errPath;
  CHECK(runCommand(runCmd) == 2);
  CHECK(readFile(errPath).find("Semantic error: entry argument strings are only supported in print calls or string bindings") !=
        std::string::npos);
}

TEST_SUITE_END();
