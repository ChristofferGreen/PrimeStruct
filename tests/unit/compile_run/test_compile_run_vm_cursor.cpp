#include "test_compile_run_helpers.h"

TEST_SUITE_BEGIN("primestruct.compile.run.vm.cursor");

TEST_CASE("vm forward cursor traversal sums a vector without skipping the final element") {
  // TODO-4610: while(it != limit(values)) must visit every element,
  // including the last one, and stop before reading limit(values) itself.
  const std::string source = R"(
import /std/collections/*
import /std/cursor/*

[return<int>]
sum_vector([vector<i32>] values) {
  [Cursor<i32> mut] it{startVector<i32>(values)}
  [Cursor<i32>] lim{limitVector<i32>(values)}
  [i32 mut] total{0i32}

  while(cursorNotEqual<i32>(it, lim)) {
    total = total + readVector<i32>(values, it)
    it = advance<i32>(it)
  }

  return(total)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(4i32, 7i32, 9i32, 11i32)}
  return(sum_vector(values))
}
)";
  const std::string srcPath = writeTemp("vm_cursor_forward_sum_vector.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 31);
}

TEST_CASE("native forward cursor traversal sums a vector without skipping the final element") {
  const std::string source = R"(
import /std/collections/*
import /std/cursor/*

[return<int>]
sum_vector([vector<i32>] values) {
  [Cursor<i32> mut] it{startVector<i32>(values)}
  [Cursor<i32>] lim{limitVector<i32>(values)}
  [i32 mut] total{0i32}

  while(cursorNotEqual<i32>(it, lim)) {
    total = total + readVector<i32>(values, it)
    it = advance<i32>(it)
  }

  return(total)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(4i32, 7i32, 9i32, 11i32)}
  return(sum_vector(values))
}
)";
  const std::string srcPath = writeTemp("native_cursor_forward_sum_vector.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_native_cursor_forward_sum_vector").string();
  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath;
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 31);
}

TEST_CASE("vm forward cursor traversal sums an array without skipping the final element") {
  // TODO-5247: array<T> cursor traversal via the same Cursor<T> type,
  // using startArray/limitArray/readArray instead of the vector-specific
  // functions (array<T> carries no runtime length field, so read/advance
  // take the array as an explicit parameter alongside the cursor rather
  // than storing a pointer back to it inside the cursor).
  const std::string source = R"(
import /std/cursor/*

[return<int>]
sum_array([array<i32>] values) {
  [Cursor<i32> mut] it{startArray<i32>(values)}
  [Cursor<i32>] lim{limitArray<i32>(values)}
  [i32 mut] total{0i32}

  while(cursorNotEqual<i32>(it, lim)) {
    total = total + readArray<i32>(values, it)
    it = advance<i32>(it)
  }

  return(total)
}

[return<int>]
main() {
  [array<i32>] values{array<i32>(4i32, 7i32, 9i32, 11i32)}
  return(sum_array(values))
}
)";
  const std::string srcPath = writeTemp("vm_cursor_forward_sum_array.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 31);
}

TEST_CASE("native forward cursor traversal sums an array without skipping the final element") {
  const std::string source = R"(
import /std/cursor/*

[return<int>]
sum_array([array<i32>] values) {
  [Cursor<i32> mut] it{startArray<i32>(values)}
  [Cursor<i32>] lim{limitArray<i32>(values)}
  [i32 mut] total{0i32}

  while(cursorNotEqual<i32>(it, lim)) {
    total = total + readArray<i32>(values, it)
    it = advance<i32>(it)
  }

  return(total)
}

[return<int>]
main() {
  [array<i32>] values{array<i32>(4i32, 7i32, 9i32, 11i32)}
  return(sum_array(values))
}
)";
  const std::string srcPath = writeTemp("native_cursor_forward_sum_array.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_native_cursor_forward_sum_array").string();
  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath;
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 31);
}

TEST_CASE("vector cursor read at limit fails deterministically") {
  // TODO-4610 acceptance: read(limit(values)) is rejected or fails
  // deterministically rather than reading past the last element.
  const std::string source = R"(
import /std/collections/*
import /std/cursor/*

[return<int>]
readAtLimit([vector<i32>] values) {
  [Cursor<i32>] lim{limitVector<i32>(values)}
  return(readVector<i32>(values, lim))
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(4i32, 7i32, 9i32)}
  return(readAtLimit(values))
}
)";
  const std::string srcPath = writeTemp("vm_cursor_read_at_limit_vector.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) != 0);
}

TEST_CASE("array cursor read at limit fails deterministically") {
  // TODO-5247 acceptance: same guarantee as the vector path, for array<T>.
  const std::string source = R"(
import /std/cursor/*

[return<int>]
readAtLimit([array<i32>] values) {
  [Cursor<i32>] lim{limitArray<i32>(values)}
  return(readArray<i32>(values, lim))
}

[return<int>]
main() {
  [array<i32>] values{array<i32>(4i32, 7i32, 9i32)}
  return(readAtLimit(values))
}
)";
  const std::string srcPath = writeTemp("vm_cursor_read_at_limit_array.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) != 0);
}

TEST_CASE("cursor comparisons reject unrelated element types at compile time") {
  // TODO-4610 acceptance: cursor comparisons require compatible provenance;
  // Cursor<i32> and Cursor<f32> are different instantiations and cannot be
  // compared.
  const std::string source = R"(
import /std/collections/*
import /std/cursor/*

[return<bool>]
compareUnrelated([vector<i32>] intValues, [vector<f32>] floatValues) {
  [Cursor<i32>] a{startVector<i32>(intValues)}
  [Cursor<f32>] b{startVector<f32>(floatValues)}
  return(cursorNotEqual<i32>(a, b))
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] intValues{vector<i32>(1i32)}
  [vector<f32> mut] floatValues{vector<f32>(1.0f32)}
  return(if(compareUnrelated(intValues, floatValues), then() { 1i32 }, else() { 0i32 }))
}
)";
  const std::string srcPath = writeTemp("vm_cursor_unrelated_comparison.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_vm_cursor_unrelated_comparison_err.txt").string();
  const std::string compileCmd =
      "./primec --emit=vm " + srcPath + " --entry /main > /dev/null 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("argument type mismatch") != std::string::npos);
}

TEST_CASE("indexing through a struct-field Reference<vector<T>> chain fails cleanly instead of hanging") {
  // Regression guard for a compiler bug found while implementing TODO-4610:
  // bracket-indexing through a struct field access chain into a
  // Reference<vector<T>> field (h.owner[h.position]) triggered unbounded
  // mutual recursion between validateExpr and
  // validateExprLateUnknownTargetFallbacks, hanging/crashing the compiler.
  // Full support for this indexing pattern is a separate, deeper
  // architectural gap (the receiver never resolves to the canonical
  // Vector<T> wrapper struct); this test only asserts the compiler now
  // fails deterministically instead of hanging - Cursor<T> avoids the
  // pattern entirely by keeping the owning collection as an ordinary
  // parameter rather than storing a pointer to it inside the cursor.
  const std::string source = R"(
import /std/collections/*

[struct]
Holder {
  [Reference<vector<i32>>] owner
  [i32] position{0i32}
}

[return<int>]
readIt([Holder] h) {
  return(h.owner[h.position])
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(4i32, 7i32, 9i32)}
  [Holder] h{Holder{location(values), 1i32}}
  return(readIt(h))
}
)";
  const std::string srcPath = writeTemp("vm_cursor_field_chain_reference_index.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 2);
}

TEST_CASE("bare count() on an array parameter works inside a namespaced function") {
  // Regression guard for a compiler bug found while implementing TODO-5247:
  // isUnqualifiedCollectionBuiltinName (two copies, in
  // IrLowererCountAccessClassifiers.cpp and
  // IrLowererSetupTypeCollectionHelpers.cpp) rejected any bare count(...)/
  // capacity(...)/push(...)/etc. call made from inside a `namespace` block,
  // because it treated the call's inherited namespacePrefix (populated for
  // every call parsed inside any namespace, not just explicitly-qualified
  // ones) as if it meant the call was explicitly qualified. This silently
  // misrouted count(arrayParam) to a broken fallback that crashed the VM
  // with "unaligned indirect address in IR" instead of returning the count.
  const std::string source = R"(
namespace outer {
namespace inner {

[return<int>]
countArray([array<i32>] values) {
  return(count(values))
}

}
}

[return<int>]
main() {
  [array<i32>] values{array<i32>(4i32, 7i32, 9i32)}
  return(/outer/inner/countArray(values))
}
)";
  const std::string srcPath = writeTemp("vm_cursor_namespaced_array_count.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 3);
}

TEST_SUITE_END();
