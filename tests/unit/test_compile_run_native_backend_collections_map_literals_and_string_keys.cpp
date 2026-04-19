#include "test_compile_run_helpers.h"

#include "test_compile_run_collection_conformance_helpers.h"
#include "test_compile_run_container_error_conformance_helpers.h"
#include "test_compile_run_checked_pointer_conformance_helpers.h"
#include "test_compile_run_unchecked_pointer_conformance_helpers.h"
#include "test_compile_run_native_backend_collections_helpers.h"

#if PRIMESTRUCT_NATIVE_COLLECTIONS_ENABLED
TEST_SUITE_BEGIN("primestruct.compile.run.native_backend.collections");

TEST_CASE("compiles and runs native collection syntax parity for call and method forms") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] viaCall{vector<i32>(10i32, 20i32, 30i32)}
  [vector<i32> mut] viaMethod{vector<i32>(10i32, 20i32, 30i32)}
  pop(viaCall)
  viaMethod.pop()
  reserve(viaCall, 3i32)
  viaMethod.reserve(3i32)
  push(viaCall, 40i32)
  viaMethod.push(40i32)
  return(plus(
      plus(at(viaCall, 2i32), viaMethod.at(2i32)),
      plus(viaCall[2i32], plus(viaMethod[2i32], plus(count(viaCall), viaMethod.count())))))
}
)";
  const std::string srcPath = writeTemp("compile_native_collection_syntax_parity.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_collection_syntax_parity_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 166);
}

TEST_CASE("compiles and runs native vector literal count helper") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  return(count(vector<i32>(1i32, 2i32, 3i32)))
}
)";
  const std::string srcPath = writeTemp("compile_native_vector_literal_count_helper.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_vector_literal_count_helper_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 3);
}

TEST_CASE("compiles and runs native collection bracket literals") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [array<i32>] values{array<i32>[1i32, 2i32]}
  [vector<i32>] list{vector<i32>[3i32, 4i32]}
  [map<i32, i32>] table{map<i32, i32>[5i32=6i32]}
  return(plus(plus(values[1i32], list[1i32]), at(table, 5i32)))
}
)";
  const std::string srcPath = writeTemp("compile_native_collection_brackets.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_collection_brackets_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 12);
}

TEST_CASE("compiles and runs native map literals") {
  const std::string source = R"(
[return<int>]
main() {
  return(at(map<i32, i32>{1i32=2i32, 3i32=4i32}, 3i32))
}
)";
  const std::string srcPath = writeTemp("compile_native_map_literal.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_native_map_literal_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 4);
}

TEST_CASE("compiles and runs native map count helper" * doctest::skip(true)) {
  const std::string source = R"(
[return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>{1i32=2i32, 3i32=4i32}}
  return(count(values))
}
)";
  const std::string srcPath = writeTemp("compile_native_map_count_helper.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_native_map_count_helper_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 2);
}

TEST_CASE("compiles and runs native map method call" * doctest::skip(true)) {
  const std::string source = R"(
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
  const std::string srcPath = writeTemp("compile_native_map_method_call.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_map_method_call_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 2);
}

TEST_CASE("compiles and runs native map at helper" * doctest::skip(true)) {
  const std::string source = R"(
[return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>{1i32=2i32, 3i32=4i32}}
  return(at(values, 3i32))
}
)";
  const std::string srcPath = writeTemp("compile_native_map_at.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_native_map_at_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 4);
}

TEST_CASE("compiles and runs native map indexing sugar" * doctest::skip(true)) {
  const std::string source = R"(
[return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>{1i32=2i32, 3i32=4i32}}
  return(values[3i32])
}
)";
  const std::string srcPath = writeTemp("compile_native_map_indexing.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_native_map_indexing_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 4);
}

TEST_CASE("compiles and runs native map at_unsafe helper" * doctest::skip(true)) {
  const std::string source = R"(
[return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>{1i32=2i32, 3i32=4i32}}
  return(at_unsafe(values, 1i32))
}
)";
  const std::string srcPath = writeTemp("compile_native_map_at_unsafe.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_native_map_at_unsafe_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 2);
}

TEST_CASE("compiles and runs native bool map access helpers" * doctest::skip(true)) {
  const std::string source = R"(
[return<int>]
main() {
  [map<bool, i32>] values{map<bool, i32>{true=1i32, false=2i32}}
  return(plus(at(values, true), at_unsafe(values, false)))
}
)";
  const std::string srcPath = writeTemp("compile_native_map_bool_access.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_native_map_bool_access_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 3);
}

TEST_CASE("compiles and runs native u64 map access helpers" * doctest::skip(true)) {
  const std::string source = R"(
[return<int>]
main() {
  [map<u64, i32>] values{map<u64, i32>{2u64=7i32, 11u64=5i32}}
  return(plus(at(values, 2u64), at_unsafe(values, 11u64)))
}
)";
  const std::string srcPath = writeTemp("compile_native_map_u64_access.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_native_map_u64_access_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 12);
}

TEST_CASE("compiles and runs native map at missing key" * doctest::skip(true)) {
  const std::string source = R"(
[return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>{1i32=2i32, 3i32=4i32}}
  return(at(values, 9i32))
}
)";
  const std::string srcPath = writeTemp("compile_native_map_at_missing.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_native_map_at_missing_exe").string();
  const std::string errPath =
      (testScratchPath("") / "primec_native_map_at_missing_err.txt").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string runCmd = exePath + " 2> " + errPath;
  CHECK(runCommand(runCmd) == 3);
  CHECK(readFile(errPath) == "map key not found\n");
}

TEST_CASE("compiles and runs native typed map binding") {
  const std::string source = R"(
[return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>{1i32=2i32, 3i32=4i32}}
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_native_map_binding.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_native_map_binding_exe").string();
  const std::string nativePath = (testScratchPath("") / "primec_native_map_binding_native").string();

  const std::string compileCppCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCppCmd) == 0);
  CHECK(runCommand(exePath) == 0);

  const std::string runVmCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runVmCmd) == 0);

  const std::string compileNativeCmd = "./primec --emit=native " + srcPath + " -o " + nativePath + " --entry /main";
  CHECK(runCommand(compileNativeCmd) == 0);
  CHECK(runCommand(nativePath) == 0);
}

TEST_CASE("rejects native map literal odd args") {
  const std::string source = R"(
[return<int>]
main() {
  map<i32, i32>(1i32)
  return(1i32)
}
)";
  const std::string srcPath = writeTemp("compile_native_map_literal_odd.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_native_map_literal_odd_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 2);
}

TEST_CASE("rejects native map literal type mismatch") {
  const std::string source = R"(
[return<int>]
main() {
  map<i32, i32>(1i32, true)
  return(1i32)
}
)";
  const std::string srcPath = writeTemp("compile_native_map_literal_mismatch.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_map_literal_mismatch_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 2);
}

TEST_CASE("compiles and runs native string-valued map literals" * doctest::skip(true)) {
  const std::string source = R"(
[return<int>]
main() {
  [map<i32, string>] values{map<i32, string>(1i32, "abc"raw_utf8, 2i32, "de"raw_utf8)}
  return(plus(count(at(values, 1i32)), count(at_unsafe(values, 2i32))))
}
)";
  const std::string srcPath = writeTemp("compile_native_map_literal_string_values.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_map_literal_string_values_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 5);
}

TEST_CASE("compiles and runs native string-keyed map literals" * doctest::skip(true)) {
  const std::string source = R"(
[return<int>]
main() {
  [map<string, i32>] values{map<string, i32>("a"raw_utf8, 1i32, "b"raw_utf8, 2i32)}
  [i32] a{at(values, "b"raw_utf8)}
  [i32] b{at_unsafe(values, "a"raw_utf8)}
  return(plus(plus(a, b), count(values)))
}
)";
  const std::string srcPath = writeTemp("compile_native_map_literal_string_key.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_map_literal_string_key_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 5);
}

TEST_CASE("compiles and runs native map literal string binding key" * doctest::skip(true)) {
  const std::string source = R"(
[return<int>]
main() {
  [string] key{"b"raw_utf8}
  [map<string, i32>] values{map<string, i32>(key, 2i32, "a"raw_utf8, 1i32)}
  return(at(values, key))
}
)";
  const std::string srcPath = writeTemp("compile_native_map_literal_string_binding_key.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_map_literal_string_binding_key_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 2);
}

TEST_CASE("compiles and runs native string-keyed map indexing sugar" * doctest::skip(true)) {
  const std::string source = R"(
[return<int>]
main() {
  [map<string, i32>] values{map<string, i32>("a"raw_utf8, 1i32, "b"raw_utf8, 2i32)}
  return(values["b"raw_utf8])
}
)";
  const std::string srcPath = writeTemp("compile_native_map_indexing_string_key.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_map_indexing_string_key_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 2);
}

TEST_CASE("compiles and runs native string-keyed map indexing binding key" * doctest::skip(true)) {
  const std::string source = R"(
[return<int>]
main() {
  [map<string, i32>] values{map<string, i32>("a"raw_utf8, 1i32, "b"raw_utf8, 2i32)}
  [string] key{"b"raw_utf8}
  return(values[key])
}
)";
  const std::string srcPath = writeTemp("compile_native_map_indexing_string_binding.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_map_indexing_string_binding_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 2);
}

TEST_CASE("rejects native map indexing with argv key" * doctest::skip(true)) {
  const std::string source = R"(
[return<int>]
main([array<string>] args) {
  [map<string, i32>] values{map<string, i32>("a"raw_utf8, 1i32)}
  [string] key{args[0i32]}
  return(values[key])
}
)";
  const std::string srcPath = writeTemp("compile_native_map_indexing_argv_key.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_native_map_indexing_argv_key_err.txt").string();
  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  const std::string err = readFile(errPath);
  CHECK(err.find("Semantic error: entry argument strings are only supported in print calls or string bindings") !=
        std::string::npos);
  CHECK(err.find(": error: Semantic error: entry argument strings are only supported in print calls or string "
                 "bindings") != std::string::npos);
  CHECK(err.find("^") != std::string::npos);
}

TEST_CASE("compiles and runs native string-keyed map binding lookup" * doctest::skip(true)) {
  const std::string source = R"(
[return<int>]
main() {
  [map<string, i32>] values{map<string, i32>("a"raw_utf8, 1i32, "b"raw_utf8, 2i32)}
  [string] key{"b"raw_utf8}
  return(plus(at(values, key), count(values)))
}
)";
  const std::string srcPath = writeTemp("compile_native_map_literal_string_binding.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_map_literal_string_binding_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 4);
}

TEST_CASE("rejects native map lookup with argv string key" * doctest::skip(true)) {
  const std::string source = R"(
[return<int>]
main([array<string>] args) {
  [map<string, i32>] values{map<string, i32>("a"raw_utf8, 1i32)}
  [string] key{args[0i32]}
  return(at_unsafe(values, key))
}
)";
  const std::string srcPath = writeTemp("compile_native_map_lookup_argv_key.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_native_map_lookup_argv_key_err.txt").string();
  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  const std::string err = readFile(errPath);
  CHECK(err.find("Semantic error: entry argument strings are only supported in print calls or string bindings") !=
        std::string::npos);
  CHECK(err.find(": error: Semantic error: entry argument strings are only supported in print calls or string "
                 "bindings") != std::string::npos);
  CHECK(err.find("^") != std::string::npos);
}

TEST_CASE("rejects native map literal string key from argv binding") {
  const std::string source = R"(
[return<int>]
main([array<string>] args) {
  [string] key{args[0i32]}
  map<string, i32>(key, 1i32)
  return(1i32)
}
)";
  const std::string srcPath = writeTemp("compile_native_map_literal_string_argv_key.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_map_literal_string_argv_key_exe").string();
  const std::string errPath =
      (testScratchPath("") / "primec_native_map_literal_string_argv_key_err.txt").string();

  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  const std::string err = readFile(errPath);
  CHECK(err.find("Semantic error: entry argument strings are only supported in print calls or string bindings") !=
        std::string::npos);
  CHECK(err.find(": error: Semantic error: entry argument strings are only supported in print calls or string "
                 "bindings") != std::string::npos);
  CHECK(err.find("^") != std::string::npos);
}

TEST_SUITE_END();
#endif
