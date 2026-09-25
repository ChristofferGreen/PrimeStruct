#include "../test_compile_run_helpers.h"

TEST_SUITE_BEGIN("primestruct.compile.run.vm.maps");

TEST_CASE("runs vm with map constructor") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  /std/collections/map/map<i32, i32>(1i32, 2i32, 3i32, 4i32)
  return(1i32)
}
)";
  const std::string srcPath = writeTemp("vm_map_literal.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 1);
}

TEST_CASE("runs vm with map constructor count helper") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{/std/collections/map/map<i32, i32>(1i32, 2i32, 3i32, 4i32)}
  return(count(values))
}
)";
  const std::string srcPath = writeTemp("vm_map_literal_count.prime", source);
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

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{/std/collections/map/map<i32, i32>(1i32, 2i32, 3i32, 4i32)}
  return(values.size())
}
)";
  const std::string srcPath = writeTemp("vm_map_method_call.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 2);
}

TEST_CASE("rejects vm map indexing sugar without canonical helper") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{/std/collections/map/map<i32, i32>(1i32, 2i32, 3i32, 4i32)}
  return(values[3i32])
}
)";
  const std::string srcPath = writeTemp("vm_map_indexing.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_vm_map_indexing_err.txt").string();
  const std::string runCmd =
      "./primec --emit=vm " + srcPath + " --entry /main > " + errPath + " 2>&1";
  CHECK(runCommand(runCmd) == 2);
}

TEST_CASE("runs vm with map at_unsafe helper") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{/std/collections/map/map<i32, i32>(1i32, 2i32, 3i32, 4i32)}
  return(at_unsafe(values, 1i32))
}
)";
  const std::string srcPath = writeTemp("vm_map_at_unsafe.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 2);
}

TEST_CASE("rejects vm bool map access helpers without canonical helper") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [map<bool, i32>] values{/std/collections/map/map<bool, i32>(true, 1i32, false, 2i32)}
  return(plus(at(values, true), at_unsafe(values, false)))
}
)";
  const std::string srcPath = writeTemp("vm_map_bool_access.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_vm_map_bool_access_err.txt").string();
  const std::string runCmd =
      "./primec --emit=vm " + srcPath + " --entry /main > " + errPath + " 2>&1";
  CHECK(runCommand(runCmd) == 2);
}

TEST_CASE("rejects vm map constructor odd args") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  /std/collections/map/map<i32, i32>(1i32)
  return(1i32)
}
)";
  const std::string srcPath = writeTemp("vm_map_literal_odd.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 2);
}

TEST_CASE("rejects vm map constructor type mismatch") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  /std/collections/map/map<i32, i32>(1i32, true)
  return(1i32)
}
)";
  const std::string srcPath = writeTemp("vm_map_literal_mismatch.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 2);
}

TEST_CASE("runs vm with map constructor string binding key") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [string] key{"b"raw_utf8}
  [map<string, i32>] values{/std/collections/map/map<string, i32>(key, 2i32, "a"raw_utf8, 1i32)}
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

[effects(heap_alloc), return<int>]
main() {
  [map<string, i32>] values{/std/collections/map/map<string, i32>("a"raw_utf8, 1i32, "b"raw_utf8, 2i32)}
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

[effects(heap_alloc), return<int>]
main() {
  [map<string, i32>] values{/std/collections/map/map<string, i32>("a"raw_utf8, 1i32, "b"raw_utf8, 2i32)}
  [string] key{"b"raw_utf8}
  return(values[key])
}
)";
  const std::string srcPath = writeTemp("vm_map_indexing_string_binding.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 2);
}

TEST_CASE("rejects vm map constructor string key from argv binding") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main([array<string>] args) {
  [string] key{args[0i32]}
  /std/collections/map/map<string, i32>(key, 1i32)
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

namespace {

// TODO-5311: string-keyed `.prime` maps used to fail VM/native lowering.
// Instantiating /std/collections/map/at<string, V> let the bare
// `at(self, index)` calls inside /string/equal resolve onto that map
// specialization, and a blanket string-key constructor rejection hid the
// map<string, V> spelling entirely. Both binding spellings must now match
// their i32-key equivalents (output 3/9/13/11, exit 36).
std::string makeStringKeyedMapInsertAtSource(const std::string &bindingType) {
  return R"(
import /std/collections/*
import /std/collections/map/*

[effects(heap_alloc, io_out), return<int>]
main() {
  [)" + bindingType + R"( mut] values{map<string, i32>("left"raw_utf8, 4i32)}
  /std/collections/map/insert<string, i32>(values, "right"raw_utf8, 7i32)
  /std/collections/map/insert<string, i32>(values, "left"raw_utf8, 9i32)
  [i32] a{plus(count(values), 1i32)}
  [i32] b{/std/collections/map/at<string, i32>(values, "left"raw_utf8)}
  [i32] c{plus(/std/collections/map/at_ref<string, i32>(values, "right"raw_utf8), 6i32)}
  [i32] d{plus(b, 2i32)}
  print_line(a)
  print_line(b)
  print_line(c)
  print_line(d)
  return(plus(plus(a, b), plus(c, d)))
}
)";
}

void expectStringKeyedMapInsertAtRuns(const std::string &bindingType,
                                      const std::string &nameStem,
                                      const std::string &emitMode) {
  const std::string srcPath =
      writeTemp(nameStem + ".prime", makeStringKeyedMapInsertAtSource(bindingType));
  const std::string outPath =
      (testScratchPath("") / (nameStem + "_" + emitMode + "_out.txt")).string();
  if (emitMode == "vm") {
    const std::string runCmd =
        "./primec --emit=vm " + srcPath + " --entry /main > " + outPath;
    CHECK(runCommand(runCmd) == 36);
  } else {
    const std::string exePath =
        (testScratchPath("") / (nameStem + "_" + emitMode + "_exe")).string();
    const std::string compileCmd = "./primec --emit=" + emitMode + " " + srcPath +
                                   " -o " + exePath + " --entry /main";
    CHECK(runCommand(compileCmd) == 0);
    CHECK(runCommand(exePath + " > " + outPath) == 36);
  }
  CHECK(readFile(outPath) == "3\n9\n13\n11\n");
}

} // namespace

TEST_CASE("runs vm string-keyed map insert and at helpers on map binding") {
  expectStringKeyedMapInsertAtRuns("map<string, i32>", "vm_string_keyed_map_insert_at", "vm");
}

TEST_CASE("runs vm string-keyed map insert and at helpers on MapValue binding") {
  expectStringKeyedMapInsertAtRuns("MapValue<string, i32>",
                                   "vm_string_keyed_map_value_insert_at", "vm");
}

TEST_CASE("runs native string-keyed map insert and at helpers on map binding") {
  expectStringKeyedMapInsertAtRuns("map<string, i32>", "native_string_keyed_map_insert_at",
                                   "native");
}

TEST_CASE("runs native string-keyed map insert and at helpers on MapValue binding") {
  expectStringKeyedMapInsertAtRuns("MapValue<string, i32>",
                                   "native_string_keyed_map_value_insert_at", "native");
}

TEST_CASE("runs vm string-keyed map lookups distinguishing prefix keys") {
  // Exercises both /string/equal mismatch paths (length and byte) once a
  // string-keyed map specialization exists.
  const std::string source = R"(
import /std/collections/*
import /std/collections/map/*

[effects(heap_alloc), return<int>]
main() {
  [map<string, i32> mut] values{map<string, i32>("ab"raw_utf8, 1i32)}
  /std/collections/map/insert<string, i32>(values, "abc"raw_utf8, 20i32)
  /std/collections/map/insert<string, i32>(values, "ac"raw_utf8, 100i32)
  /std/collections/map/insert<string, i32>(values, "abc"raw_utf8, 40i32)
  if(/std/collections/map/contains<string, i32>(values, "a"raw_utf8)) {
    return(0i32)
  }
  [i32] total{plus(/std/collections/map/at<string, i32>(values, "ab"raw_utf8),
      plus(/std/collections/map/at<string, i32>(values, "abc"raw_utf8),
          /std/collections/map/at<string, i32>(values, "ac"raw_utf8)))}
  return(plus(total, count(values)))
}
)";
  const std::string srcPath = writeTemp("vm_string_keyed_map_prefix_keys.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 144);
}

TEST_SUITE_END();
