#include "../test_compile_run_helpers.h"

// TODO-4751: the public `Map<K, V>` wrapper struct in
// `stdlib/std/collections/map.prime` owns one `MapValue<K, V>` and routes
// method calls, `values[key]`, bare helper calls, explicit
// `/std/collections/map/<helper><K, V>(values, ...)` calls and borrowed
// `*_ref` helpers to its own methods.

TEST_SUITE_BEGIN("primestruct.compile.run.vm.maps");

namespace {

std::string mapWrapperAcceptanceSource() {
  return R"(
import /std/collections/*
import /std/collections/map/*

[effects(heap_alloc), return<int>]
main() {
  [Map<i32, i32> mut] values{mapSingle<i32, i32>(1i32, 4i32)}
  values.insert(2i32, 7i32)
  /std/collections/map/insert<i32, i32>(values, 3i32, 9i32)
  [Reference<Map<i32, i32>> mut] ref{location(values)}
  /std/collections/map/insert_ref<i32, i32>(ref, 1i32, 5i32)
  [i32] methodCount{values.count()}
  [i32] helperCount{count(values)}
  [i32] indexed{values[2i32]}
  [i32] borrowedAt{/std/collections/map/at_ref<i32, i32>(ref, 3i32)}
  [i32] borrowedCount{/std/collections/map/count_ref<i32, i32>(ref)}
  [i32] unsafeAt{values.at_unsafe(1i32)}
  [i32 mut] containsBonus{0i32}
  if(/std/collections/map/contains_ref<i32, i32>(ref, 2i32),
     then() { assign(containsBonus, 1i32) },
     else() { })
  return(methodCount + helperCount + indexed + borrowedAt + borrowedCount + unsafeAt + containsBonus)
}
)";
}

void expectMapWrapperAcceptanceRuns(const std::string &emitMode) {
  // 3 + 3 + 7 + 9 + 3 + 5 + 1: insert overwrote key 1 with 5.
  const std::string srcPath = writeTemp("map_wrapper_acceptance_" + emitMode + ".prime",
                                        mapWrapperAcceptanceSource());
  if (emitMode == "vm") {
    CHECK(runCommand("./primec --emit=vm " + quoteShellArg(srcPath) + " --entry /main") == 31);
    return;
  }
  const std::string exePath =
      (testScratchPath("") / ("map_wrapper_acceptance_" + emitMode + "_exe")).string();
  CHECK(runCommand("./primec --emit=" + emitMode + " " + quoteShellArg(srcPath) + " -o " +
                   quoteShellArg(exePath) + " --entry /main") == 0);
  CHECK(runCommand(quoteShellArg(exePath)) == 31);
}

} // namespace

TEST_CASE("runs vm public Map wrapper methods and helpers") {
  expectMapWrapperAcceptanceRuns("vm");
}

TEST_CASE("runs native public Map wrapper methods and helpers") {
  expectMapWrapperAcceptanceRuns("native");
}

TEST_CASE("runs exe public Map wrapper methods and helpers") {
  expectMapWrapperAcceptanceRuns("exe");
}

TEST_CASE("runs vm public Map wrapper with custom comparable struct keys") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/map/*

[struct]
Key() {
  [i32] value{0i32}
}

[return<bool>]
/Key/equal([Key] left, [Key] right) {
  return(equal(left.value, right.value))
}

[return<bool>]
/Key/less_than([Key] left, [Key] right) {
  return(less_than(left.value, right.value))
}

[effects(heap_alloc), return<int>]
main() {
  [Map<Key, i32>] values{mapPair<Key, i32>(Key{2i32}, 7i32, Key{5i32}, 11i32)}
  [i32 mut] total{/std/collections/map/count<Key, i32>(values)}
  assign(total, plus(total, /std/collections/map/at<Key, i32>(values, Key{2i32})))
  assign(total, plus(total, /std/collections/map/at_unsafe<Key, i32>(values, Key{5i32})))
  if(/std/collections/map/contains<Key, i32>(values, Key{2i32}),
     then() { assign(total, plus(total, 1i32)) },
     else() { })
  return(total)
}
)";
  const std::string srcPath = writeTemp("vm_map_wrapper_custom_key.prime", source);
  CHECK(runCommand("./primec --emit=vm " + quoteShellArg(srcPath) + " --entry /main") == 21);
}

TEST_CASE("rejects vm public Map wrapper key type mismatch") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/map/*

[effects(heap_alloc), return<int>]
main() {
  [Map<i32, i32> mut] values{mapSingle<i32, i32>(1i32, 4i32)}
  values.insert("left"raw_utf8, 7i32)
  return(values.count())
}
)";
  const std::string srcPath = writeTemp("vm_map_wrapper_key_mismatch.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_vm_map_wrapper_key_mismatch_err.txt").string();
  CHECK(runCommand("./primec --emit=vm " + quoteShellArg(srcPath) + " --entry /main 2> " +
                   quoteShellArg(errPath)) == 2);
  const std::string error = readFile(errPath);
  CHECK(error.find("argument type mismatch for /std/collections/map/Map__t") != std::string::npos);
  CHECK(error.find("/insert parameter key: expected i32") != std::string::npos);
}

TEST_CASE("rejects vm Map parameter without a visible Map struct") {
  const std::string source = R"(
import /std/collections/*

[return<int>]
scoreValues([Map<i32, i32>] values) {
  return(/std/collections/map/count<i32, i32>(values))
}

[effects(heap_alloc), return<int>]
main() {
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("vm_map_wrapper_unresolved_parameter.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_vm_map_wrapper_unresolved_parameter_err.txt").string();
  CHECK(runCommand("./primec --emit=vm " + quoteShellArg(srcPath) + " --entry /main 2> " +
                   quoteShellArg(errPath)) == 2);
  const std::string error = readFile(errPath);
  CHECK(error.find("Semantic error: template arguments are only supported on templated definitions: /Map") !=
        std::string::npos);
  CHECK(error.find("VM lowering error") == std::string::npos);
}

TEST_SUITE_END();
