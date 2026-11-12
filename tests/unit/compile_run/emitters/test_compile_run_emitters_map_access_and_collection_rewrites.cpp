#include "../test_compile_run_helpers.h"
#include "../test_compile_run_collection_conformance_helpers.h"
#include "test_compile_run_emitters_helpers.h"


TEST_SUITE_BEGIN("primestruct.compile.run.emitters.cpp");

TEST_CASE("rejects user map access unsafe string positional call shadow in C++ emitter") {
  const std::string source = R"(
[return<int>]
/map/at_unsafe([map<string, i32>] values, [string] key) {
  return(85i32)
}

[return<int>]
main() {
  [map<string, i32>] values{map<string, i32>("only"raw_utf8, 2i32)}
  return(at_unsafe("only"raw_utf8, values))
}
)";
  const std::string srcPath = writeTemp("compile_cpp_map_access_unsafe_string_positional_call_shadow.prime", source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_cpp_map_access_unsafe_string_positional_call_shadow_err.txt")
          .string();

  const std::string compileCmd =
      "./primec --emit=vm " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("argument type mismatch for /std/collections/map/at_unsafe") !=
        std::string::npos);
}

TEST_CASE("C++ emitter rejects canonical map access positional reorder") {
  const std::string source = R"(
[return<int>]
/std/collections/map/at([/std/collections/map<string, i32>] values, [string] key) {
  return(86i32)
}

[return<int>]
main() {
  [/std/collections/map<string, i32>] values{map<string, i32>("only"raw_utf8, 2i32)}
  return(/std/collections/map/at("only"raw_utf8, values))
}
)";
  const std::string srcPath = writeTemp("compile_cpp_canonical_map_access_positional_reorder.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_cpp_canonical_map_access_positional_reorder_err.txt")
          .string();

  const std::string compileCmd =
      "./primec --emit=vm " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("argument type mismatch for /std/collections/map/at parameter values") !=
        std::string::npos);
}

TEST_CASE("C++ emitter keeps canonical map access key diagnostics on positional reorder") {
  const std::string source = R"(
[return<int>]
/std/collections/map/at([/std/collections/map<string, i32>] values, [string] key) {
  return(86i32)
}

[return<int>]
main() {
  [/std/collections/map<string, i32>] values{map<string, i32>("only"raw_utf8, 2i32)}
  return(/std/collections/map/at(1i32, values))
}
)";
  const std::string srcPath = writeTemp("compile_cpp_canonical_map_access_positional_reorder_diag.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_cpp_canonical_map_access_positional_reorder_diag_err.txt")
          .string();

  const std::string compileCmd =
      "./primec --emit=vm " + srcPath + " -o /dev/null --entry /main > /dev/null 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("argument type mismatch for /std/collections/map/at parameter key") !=
        std::string::npos);
}

TEST_CASE("C++ emitter access rewrite keeps known collection receiver leading names") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
/i32/at([i32] index, [vector<i32>] values) {
  return(66i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32, 73i32)}
  [i32] index{1i32}
  return(at(values, index))
}
)";
  const std::string srcPath = writeTemp("compile_cpp_access_known_receiver_no_reorder.prime", source);
  const std::string compileCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  // No compile error - the builtin vector `at` is used (not rewritten to
  // the user's reversed-argument-order /i32/at override), so the exit
  // code is the real element value, values[1] == 73; chosen to avoid
  // colliding with this suite's compile-error exit codes (2, 3).
  CHECK(runCommand(compileCmd) == 73);
}

TEST_CASE("stdlib namespaced vector helpers in C++ emitter") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [auto mut] values{/std/collections/vector/vector<i32>(4i32, 5i32)}
  /std/collections/vector/push(values, 6i32)
  [i32] countValue{/std/collections/vector/count(values)}
  [i32] capacityValue{/std/collections/vector/capacity(values)}
  [i32] firstValue{/std/collections/vector/at(values, 0i32)}
  [i32] tailValue{/std/collections/vector/at_unsafe(values, 2i32)}
  return(plus(plus(countValue, tailValue),
              plus(firstValue, minus(capacityValue, capacityValue))))
}
)";
  const std::string srcPath = writeTemp("compile_cpp_stdlib_namespaced_vector_aliases.prime", source);
  const std::string compileCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 13);
}

TEST_CASE("rejects array namespaced vector constructor alias in C++ emitter") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{/array/vector<i32>(4i32, 5i32)}
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_cpp_array_namespaced_vector_constructor_alias.prime", source);
  const std::string outPath =
      (testScratchPath("") / "primec_cpp_array_namespaced_vector_constructor_alias_out.txt")
          .string();

  const std::string compileCmd =
      "./primec --emit=vm " + srcPath + " -o /dev/null --entry /main > " + outPath + " 2>&1";
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(outPath).find("unknown call target: /array/vector") != std::string::npos);
}

TEST_CASE("rejects array namespaced vector at alias in C++ emitter") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [auto mut] values{/std/collections/vector/vector<i32>(4i32, 5i32)}
  [i32] headValue{/array/at(values, 1i32)}
  return(headValue)
}
)";
  const std::string srcPath = writeTemp("compile_cpp_array_namespaced_vector_at_alias.prime", source);
  const std::string outPath =
      (testScratchPath("") / "primec_cpp_array_namespaced_vector_at_alias_out.txt").string();

  const std::string compileCmd =
      "./primec --emit=vm " + srcPath + " -o /dev/null --entry /main > " + outPath + " 2>&1";
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(outPath).find("unknown call target: /array/at") != std::string::npos);
}

TEST_CASE("rejects wrapper array namespaced vector at alias in C++ emitter") {
  const std::string source = R"(
[effects(heap_alloc), return<vector<i32>>]
wrapVector() {
  return(vector<i32>(4i32, 5i32))
}

[effects(heap_alloc), return<int>]
main() {
  return(/array/at(wrapVector(), 1i32))
}
)";
  const std::string srcPath = writeTemp("compile_cpp_wrapper_array_namespaced_vector_at_alias.prime", source);
  const std::string outPath =
      (testScratchPath("") / "primec_cpp_wrapper_array_namespaced_vector_at_alias_out.txt").string();

  const std::string compileCmd =
      "./primec --emit=vm " + srcPath + " -o /dev/null --entry /main > " + outPath + " 2>&1";
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(outPath).find("unknown call target: /array/at") != std::string::npos);
}

TEST_CASE("rejects array namespaced vector count builtin alias in C++ emitter") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [auto mut] values{/std/collections/vector/vector<i32>(4i32, 5i32)}
  return(/array/count(values))
}
)";
  const std::string srcPath = writeTemp("compile_cpp_array_namespaced_vector_count_builtin_alias.prime", source);
  const std::string outPath =
      (testScratchPath("") / "primec_cpp_array_namespaced_vector_count_builtin_alias_out.txt")
          .string();

  const std::string compileCmd =
      "./primec --emit=vm " + srcPath + " -o /dev/null --entry /main > " + outPath + " 2>&1";
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(outPath).find("unknown call target: /array/count") != std::string::npos);
}

TEST_CASE("rejects array namespaced vector count method alias in C++ emitter") {
  const std::string source = R"(
[return<bool>]
/std/collections/vector/count([vector<i32>] values, [bool] marker) {
  return(false)
}

[effects(heap_alloc), return<bool>]
main() {
  [vector<i32>] values{vector<i32>(4i32, 5i32)}
  return(values./array/count(true))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_array_namespaced_vector_count_method_alias.prime", source);
  const std::string outPath =
      (testScratchPath("") / "primec_cpp_array_namespaced_vector_count_method_alias_out.txt")
          .string();

  const std::string compileCmd =
      "./primec --emit=vm " + srcPath + " -o /dev/null --entry /main > " + outPath + " 2>&1";
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(outPath).find("unknown method: /array/count") != std::string::npos);
}

TEST_CASE("rejects array namespaced vector capacity alias in C++ emitter") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [auto mut] values{/std/collections/vector/vector<i32>(4i32, 5i32)}
  return(/array/capacity(values))
}
)";
  const std::string srcPath = writeTemp("compile_cpp_array_namespaced_vector_capacity_alias.prime", source);
  const std::string outPath =
      (testScratchPath("") / "primec_cpp_array_namespaced_vector_capacity_alias_out.txt")
          .string();

  const std::string compileCmd =
      "./primec --emit=vm " + srcPath + " -o /dev/null --entry /main > " + outPath + " 2>&1";
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(outPath).find("unknown call target: /array/capacity") != std::string::npos);
}

TEST_CASE("rejects array namespaced vector capacity method alias in C++ emitter") {
  const std::string source = R"(
[return<bool>]
/std/collections/vector/capacity([vector<i32>] values, [bool] marker) {
  return(false)
}

[effects(heap_alloc), return<bool>]
main() {
  [vector<i32>] values{vector<i32>(4i32, 5i32)}
  return(values./array/capacity(true))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_array_namespaced_vector_capacity_method_alias.prime", source);
  const std::string outPath =
      (testScratchPath("") / "primec_cpp_array_namespaced_vector_capacity_method_alias_out.txt")
          .string();

  const std::string compileCmd =
      "./primec --emit=vm " + srcPath + " -o /dev/null --entry /main > " + outPath + " 2>&1";
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(outPath).find("unknown method: /array/capacity") != std::string::npos);
}

TEST_CASE("rejects array namespaced vector mutator alias in C++ emitter") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [auto mut] values{/std/collections/vector/vector<i32>(4i32, 5i32)}
  /array/push(values, 6i32)
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_cpp_array_namespaced_vector_mutator_alias.prime", source);
  const std::string outPath =
      (testScratchPath("") / "primec_cpp_array_namespaced_vector_mutator_alias_out.txt").string();

  const std::string compileCmd =
      "./primec --emit=vm " + srcPath + " -o /dev/null --entry /main > " + outPath + " 2>&1";
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(outPath).find("unknown call target: /array/push") != std::string::npos);
}

TEST_SUITE_END();
