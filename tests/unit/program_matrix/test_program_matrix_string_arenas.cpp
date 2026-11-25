#include "program_matrix.h"

TEST_SUITE_BEGIN("primestruct.program_matrix.string_arenas");

// String arenas and the string heap statistics (docs/spec/strings-and-views.md) on the VM, arm64 native and optexe.
// The native backend defines its own figures (docs/spec/vm-design.md, the string heap on the native backend): its pool
// is one shared slot pool where the VM has chunk regions per size class, so these programs print relations and deltas
// (a figure after minus before, a comparison, a balance of zero) and never an absolute chunk count or free byte count.

namespace {

void runStringProgram(program_matrix::ProgramCase program) {
  program.onlyConfigs = {"vm-step-O0", "vm-O0", "vm-O2", "optexe-O2"};
#if defined(__APPLE__) && (defined(__arm64__) || defined(__aarch64__))
  program.onlyConfigs.push_back("native-O0");
  program.onlyConfigs.push_back("native-O2");
#endif
  program_matrix::runProgramMatrix(program);
}

} // namespace

TEST_CASE("string arenas nest, count their text and give every block back at the end") {
  program_matrix::ProgramCase program;
  program.name = "string_arenas_nest";
  program.source = R"(
import /std/text/*
import /std/text/view/*

[return<void> effects(heap_alloc)]
churn([i32] count) {
  for([i32 mut] i{0i32}, i < count, i++) {
    [String] s{stringFromI32(i)}
  }
}

[return<int> effects(heap_alloc, io_out)]
main() {
  [String] keep{stringFromI32(42i32)}
  [u64] inUse{stringHeapBytesInUse()}
  [i32] largeBefore{stringHeapLargeBlocks()}
  print_line(stringArenaDepth())
  if(true) {
    [StringArena] outer{beginStringArena()}
    churn(1000i32)
    [u64] outerBytes{stringArenaBytes()}
    print_line(stringArenaDepth())
    print_line(outerBytes > 0u64)
    print_line(stringHeapBytesInUse() - inUse == outerBytes)
    if(true) {
      [StringArena] inner{beginStringArena()}
      churn(500i32)
      print_line(stringArenaDepth())
      print_line(stringArenaBytes() > outerBytes)
      print_line(stringHeapBytesInUse() - inUse == stringArenaBytes())
    }
    print_line(stringArenaDepth())
    print_line(stringArenaBytes() == outerBytes)
    [String] big{stringFilled(100000i32, 65u8)}
    print_line(big.count())
    print_line(stringHeapLargeBlocks() - largeBefore)
  }
  print_line(stringArenaDepth())
  print_line(stringArenaBytes())
  print_line(stringHeapBytesInUse() == inUse)
  print_line(stringHeapLargeBlocks() - largeBefore)
  print_line(keep.count())
  return(0i32)
}
)";
  program.exitCode = 0;
  program.stdoutText = "0\n1\n1\n1\n2\n1\n1\n1\n1\n100000\n1\n0\n0\n1\n0\n2\n";
  runStringProgram(program);
}

TEST_CASE(
    "many arenas in a row, and an arena of several regions, leave the string heap as it was") {
  program_matrix::ProgramCase program;
  program.name = "string_arenas_many";
  program.source = R"(
import /std/text/*
import /std/text/view/*

[return<i32> effects(heap_alloc)]
churn([i32] count) {
  [i32 mut] total{0i32}
  for([i32 mut] i{0i32}, i < count, i++) {
    [String] s{stringFromI32(i)}
    total = total + s.count()
  }
  return(total)
}

[return<int> effects(heap_alloc, io_out)]
main() {
  [u64] inUse{stringHeapBytesInUse()}
  [i32 mut] total{0i32}
  for([i32 mut] round{0i32}, round < 400i32, round++) {
    [StringArena] arena{beginStringArena()}
    total = total + churn(100i32)
  }
  print_line(total)
  print_line(stringArenaDepth())
  if(true) {
    [StringArena] wide{beginStringArena()}
    print_line(churn(20000i32))
    [String] mid{stringFilled(300000i32, 66u8)}
    print_line(mid.count())
    print_line(churn(3000i32))
  }
  print_line(stringArenaDepth())
  print_line(stringArenaBytes())
  print_line(stringHeapBytesInUse() == inUse)
  return(0i32)
}
)";
  program.exitCode = 0;
  program.stdoutText = "76000\n0\n88890\n300000\n10890\n0\n0\n1\n";
  runStringProgram(program);
}

TEST_CASE("the memory limit counts what an arena holds and applies again when it ends") {
  program_matrix::ProgramCase program;
  program.name = "string_arenas_limit";
  program.extraConfigs = {"wasm-wasi"};
  program.source = R"(
import /std/text/*
import /std/text/view/*

[return<int> effects(heap_alloc, io_out)]
main() {
  setStringMemoryLimit(300000u64)
  if(true) {
    [StringArena] arena{beginStringArena()}
    [String mut] first{String{}}
    [String mut] second{String{}}
    print_line(if(first.tryReserve(200000i32), then() { 1i32 }, else() { 0i32 }))
    print_line(if(second.tryReserve(200000i32), then() { 1i32 }, else() { 0i32 }))
  }
  [String mut] third{String{}}
  print_line(if(third.tryReserve(200000i32), then() { 1i32 }, else() { 0i32 }))
  return(0i32)
}
)";
  program.exitCode = 0;
  program.stdoutText = "1\n0\n1\n";
  runStringProgram(program);
}

TEST_CASE("text used after its arena ended stops the program with the heap's message") {
  program_matrix::ProgramCase program;
  program.name = "string_arenas_use_after_end";
  program.source = R"(
import /std/text/*
import /std/text/view/*

[return<int> effects(heap_alloc, io_out)]
main() {
  [String mut] escaped{String{}}
  if(true) {
    [StringArena] arena{beginStringArena()}
    escaped = stringFromI32(7i32)
    print_line(escaped.count())
  }
  print_line("after")
  return(0i32)
}
)";
  program.exitCode = 3;
  program.stdoutText = "1\nafter\n";
  // The native backend has the heap's own message for a block that is already free; the VM and optexe name the address.
  program.nativeStderr = "invalid heap free address: the block is already free\n";
  runStringProgram(program);
}

TEST_CASE("freed text blocks count as free bytes until the next block takes them back") {
  program_matrix::ProgramCase program;
  program.name = "string_heap_free_bytes";
  program.source = R"(
import /std/text/*
import /std/text/view/*

[return<int> effects(heap_alloc, io_out)]
main() {
  [u64] before{stringHeapBytesFree()}
  if(true) {
    [String mut] s{String{}}
    s.reserve(100i32)
  }
  [u64] after{stringHeapBytesFree()}
  print_line(after > before)
  if(true) {
    [String mut] s{String{}}
    s.reserve(100i32)
    print_line(stringHeapBytesFree() < after)
  }
  print_line(stringHeapBytesFree() == after)
  return(0i32)
}
)";
  program.exitCode = 0;
  program.stdoutText = "1\n1\n1\n";
  runStringProgram(program);
}

TEST_CASE("the live block count of the slot heap goes up by one per buffer and back down") {
  program_matrix::ProgramCase program;
  program.name = "string_heap_live_blocks";
  program.extraConfigs = {"wasm-wasi"};
  program.source = R"(
import /std/text/*
import /std/collections/*

[return<int> effects(heap_alloc, io_out)]
main() {
  [i32] blocks{heapLiveBlocks()}
  if(true) {
    [vector<i32> mut] first{vector<i32>{}}
    first.push(1i32)
    first.push(2i32)
    first.push(3i32)
    print_line(heapLiveBlocks() - blocks)
    [vector<i32> mut] second{vector<i32>{}}
    second.push(4i32)
    print_line(heapLiveBlocks() - blocks)
  }
  print_line(heapLiveBlocks() - blocks)
  return(0i32)
}
)";
  program.exitCode = 0;
  program.stdoutText = "1\n2\n0\n";
  runStringProgram(program);
}

#if defined(__APPLE__) && (defined(__arm64__) || defined(__aarch64__))
namespace {

// The peak resident set size, in bytes, that `/usr/bin/time -l` reports for a native program that opens an arena
// `rounds` times and makes one text larger than a standard arena region in each.
long long peakResidentBytesOfLargeTextArenas(const std::string &name, int rounds) {
  const std::filesystem::path dir =
      primec::testing::testScratchPath("program_matrix/" + name + "/x").parent_path();
  std::filesystem::create_directories(dir);
  const std::filesystem::path source = dir / "program.prime";
  {
    std::ofstream file(source);
    file << R"(
import /std/text/*
import /std/text/view/*

[return<int> effects(heap_alloc, io_out)]
main() {
  [i32 mut] total{0i32}
  for([i32 mut] round{0i32}, round < )"
         << rounds << R"(i32, round++) {
    [StringArena] arena{beginStringArena()}
    [String] big{stringFilled(300000i32, 66u8)}
    total = total + big.count()
  }
  print_line(total)
  return(0i32)
}
)";
  }
  const std::filesystem::path binary = dir / "program.bin";
  const program_matrix::Outcome compile = program_matrix::runCaptured(
      "./primec --emit=native -O2 " + program_matrix::quote(source.string()) + " -o " +
          program_matrix::quote(binary.string()) + " --entry /main",
      dir / "compile");
  REQUIRE(compile.exitCode == 0);
  const program_matrix::Outcome run = program_matrix::runCaptured(
      "/usr/bin/time -l " + program_matrix::quote(binary.string()), dir / "run");
  CHECK(run.exitCode == 0);
  const std::string marker = "maximum resident set size";
  const size_t at = run.err.find(marker);
  REQUIRE(at != std::string::npos);
  // The figure comes first on its line: "  12345678  maximum resident set size".
  const size_t lineStart = run.err.rfind('\n', at);
  return std::atoll(run.err.c_str() + (lineStart == std::string::npos ? 0 : lineStart + 1));
}

} // namespace
#endif

TEST_CASE("native arenas return a region larger than the standard one to the system") {
#if defined(__APPLE__) && (defined(__arm64__) || defined(__aarch64__))
  const long long few = peakResidentBytesOfLargeTextArenas("string_arenas_large_few", 20);
  const long long many = peakResidentBytesOfLargeTextArenas("string_arenas_large_many", 1200);
  // 1200 rounds of 300 KB would hold 360 MB when no region is unmapped; the bound is a fixed 64 MB over 20 rounds.
  CHECK(many < few + 64ll * 1024 * 1024);
#endif
}

TEST_CASE("a module with tasks gives the main task and a task thread arenas of their own") {
  program_matrix::ProgramCase program;
  program.name = "string_arenas_threads";
  program.source = R"(
import /std/text/*
import /std/text/view/*
import /std/sync/*

[return<i32> effects(heap_alloc)]
churn([i32] count) {
  [i32 mut] total{0i32}
  for([i32 mut] i{0i32}, i < count, i++) {
    [String] s{stringFromI32(i)}
    total = total + s.count()
  }
  return(total)
}

[return<i32> effects(heap_alloc)]
arena_task([i32] count) {
  [StringArena] arena{beginStringArena()}
  [i32] total{churn(count)}
  if(stringArenaBytes() > 0u64 && stringArenaDepth() == 1i32) {
    return(total)
  }
  return(0i32)
}

[return<void> effects(heap_alloc)]
idle_task() {
  [String] s{stringFromI32(7i32)}
}

[return<int> effects(heap_alloc, io_out, task)]
main() {
  [Task<i32>] first{[spawn] arena_task(600i32)}
  print_line(wait(first))
  [u64] inUse{stringHeapBytesInUse()}
  [Task<void>] idle{[spawn] idle_task()}
  if(true) {
    [StringArena] arena{beginStringArena()}
    print_line(churn(500i32))
    print_line(stringArenaDepth())
    print_line(stringArenaBytes() > 0u64)
  }
  wait(idle)
  print_line(stringArenaDepth())
  print_line(stringArenaBytes())
  print_line(stringHeapBytesInUse() == inUse)
  return(0i32)
}
)";
  program.exitCode = 0;
  program.stdoutText = "1690\n1390\n1\n1\n0\n0\n1\n";
  runStringProgram(program);
}

TEST_SUITE_END();
