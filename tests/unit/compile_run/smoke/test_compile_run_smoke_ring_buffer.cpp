#include "../test_compile_run_helpers.h"

#include "test_compile_run_smoke_helpers.h"

// TODO-4702: proves the generic collection registry (TODO-4686..4698) lets a
// brand-new [collection_type] struct be added purely in stdlib (see
// stdlib/std/collections/ring_buffer.prime), with zero changes to src/ or
// include/. RingBuffer<T> uses explicit free-function calls
// (push<T>(buf, value), count<T>(buf), at<T>(buf, index)) rather than
// `.method()` call-sugar: method-call sugar for a brand-new (not
// vector/map/soa) collection type hits a real, pre-existing compiler
// limitation ("semantic-product method-call target missing lowered
// definition") unrelated to this proof - the explicit call form fully
// works end to end (VM and native) and is what this file exercises.

TEST_SUITE_BEGIN("primestruct.compile.run.smoke");

TEST_CASE("ring buffer construct, push, count, at overwrite oldest when full") {
  const std::string source = R"(
import /std/collections/ring_buffer/*

[return<int>]
main() {
  [RingBuffer<i32> mut] buf{ring_buffer<i32>(3i32)}
  push<i32>(buf, 10i32)
  push<i32>(buf, 20i32)
  push<i32>(buf, 30i32)
  [i32] fullCount{count<i32>(buf)}
  push<i32>(buf, 40i32)
  [i32] afterOverwriteCount{count<i32>(buf)}
  [i32] oldest{at<i32>(buf, 0i32)}
  [i32] middle{at<i32>(buf, 1i32)}
  [i32] newest{at<i32>(buf, 2i32)}
  return(fullCount + afterOverwriteCount + oldest + middle + newest)
}
)";
  const std::string srcPath = writeTemp("compile_ring_buffer_wrap.prime", source);
  const std::string nativePath = (testScratchPath("") / "primec_ring_buffer_wrap_native").string();

  // Expected: fullCount=3, afterOverwriteCount=3, oldest(20)+middle(30)+newest(40)=90
  // total: 3 + 3 + 90 = 96
  const std::string runVmCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runVmCmd) == 96);

  const std::string compileNativeCmd =
      "./primec --emit=native " + srcPath + " -o " + nativePath + " --entry /main";
  CHECK(runCommand(compileNativeCmd) == 0);
  CHECK(runCommand(nativePath) == 96);
}

TEST_CASE("ring buffer count grows without overwrite before reaching capacity") {
  const std::string source = R"(
import /std/collections/ring_buffer/*

[return<int>]
main() {
  [RingBuffer<i32> mut] buf{ring_buffer<i32>(4i32)}
  [i32] emptyCount{count<i32>(buf)}
  push<i32>(buf, 1i32)
  [i32] oneCount{count<i32>(buf)}
  push<i32>(buf, 2i32)
  [i32] twoCount{count<i32>(buf)}
  [i32] cap{capacity<i32>(buf)}
  [i32] first{at<i32>(buf, 0i32)}
  [i32] second{at<i32>(buf, 1i32)}
  return(emptyCount + oneCount + twoCount + cap + first + second)
}
)";
  const std::string srcPath = writeTemp("compile_ring_buffer_grow.prime", source);
  const std::string nativePath = (testScratchPath("") / "primec_ring_buffer_grow_native").string();

  // Expected: 0 + 1 + 2 + 4 + 1 + 2 = 10
  const std::string runVmCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runVmCmd) == 10);

  const std::string compileNativeCmd =
      "./primec --emit=native " + srcPath + " -o " + nativePath + " --entry /main";
  CHECK(runCommand(compileNativeCmd) == 0);
  CHECK(runCommand(nativePath) == 10);
}

TEST_CASE("ring buffer at panics past logical count") {
  const std::string source = R"(
import /std/collections/ring_buffer/*

[return<int>]
main() {
  [RingBuffer<i32> mut] buf{ring_buffer<i32>(2i32)}
  push<i32>(buf, 5i32)
  return(at<i32>(buf, 1i32))
}
)";
  const std::string srcPath = writeTemp("compile_ring_buffer_out_of_range.prime", source);

  const std::string runVmCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runVmCmd) != 0);
}

TEST_SUITE_END();
