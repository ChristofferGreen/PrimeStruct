#include "program_matrix.h"

TEST_SUITE_BEGIN("primestruct.program_matrix.atomics");

// `/std/sync` atomics (docs/spec/stdlib-reference.md, std/sync): tasks share one cell through copies of an `Atomic`.
// The VM runs them on the eager default schedule and on seeds 1 to 8, where every atomic operation is a switch point;
// optexe runs each task on a thread. The native and Wasm backends reject a program that uses atomics. Each matrix
// case builds one optexe binary (host C++ compiler), which takes a few seconds.

namespace {

void runAtomicProgram(program_matrix::ProgramCase program) {
  std::vector<program_matrix::ExecutionConfig> configs = {
      {"vm-step-O0", program_matrix::Backend::VmStep, 0, false},
      {"vm-O0", program_matrix::Backend::Vm, 0, false},
      {"vm-O2", program_matrix::Backend::Vm, 2, false},
      {"optexe-O2", program_matrix::Backend::OptExe, 2, true},
  };
  for (int seed = 1; seed <= 8; ++seed) {
    program_matrix::ExecutionConfig config{
        "vm-O2-seed" + std::to_string(seed), program_matrix::Backend::Vm, 2, false};
    config.vmFlags = {"--vm-schedule-seed", std::to_string(seed)};
    configs.push_back(std::move(config));
  }
  for (const program_matrix::ExecutionConfig &config : configs) {
    program.onlyConfigs.push_back(config.name);
  }
  program_matrix::runProgramMatrix(program, configs);
}

// What `primec --emit=<kind>` says about `source`.
program_matrix::Outcome
compileFor(const std::string &name, const std::string &kind, const std::string &source) {
  const std::filesystem::path dir =
      primec::testing::testScratchPath("program_matrix/" + name + "/x").parent_path();
  std::filesystem::create_directories(dir);
  const std::filesystem::path sourcePath = dir / (name + ".prime");
  {
    std::ofstream file(sourcePath, std::ios::binary);
    file << source;
  }
  const std::filesystem::path outPath = dir / (name + "." + kind + ".out.bin");
  return program_matrix::runCaptured("./primec --emit=" + kind + " " +
                                         program_matrix::quote(sourcePath.string()) + " -o " +
                                         program_matrix::quote(outPath.string()),
                                     dir / (name + "." + kind));
}

const char *const SmallAtomicProgram = R"(
import /std/sync/*

[effects(atomic, heap_alloc, io_out), return<int>]
main() {
  counter{Atomic<i64>{0i64}}
  counter.add(5i64)
  print_line(counter.load())
  return(0i32)
}
)";

} // namespace

TEST_CASE("four tasks add to one shared Atomic<i64>") {
  program_matrix::ProgramCase program;
  program.name = "atomic_shared_add";
  program.source = R"(
import /std/sync/*

[effects(atomic), return<void>]
add_many([Atomic<i64>] counter, [i32] rounds) {
  [i32 mut] i{0i32}
  while(i < rounds) {
    counter.add(1i64)
    i = i + 1i32
  }
}

[effects(atomic, heap_alloc, io_out, task), return<int>]
main() {
  counter{Atomic<i64>{0i64}}
  [Task<void>] a{[spawn] add_many(counter, 100000i32)}
  [Task<void>] b{[spawn] add_many(counter, 100000i32)}
  [Task<void>] c{[spawn] add_many(counter, 100000i32)}
  [Task<void>] d{[spawn] add_many(counter, 100000i32)}
  wait(a)
  wait(b)
  wait(c)
  wait(d)
  print_line(counter.load())
  return(0i32)
}
)";
  program.exitCode = 0;
  program.stdoutText = "400000\n";
  runAtomicProgram(program);
}

TEST_CASE("a compare-exchange spin counter loses no addition") {
  program_matrix::ProgramCase program;
  program.name = "atomic_compare_exchange_spin";
  program.source = R"(
import /std/sync/*

[effects(atomic), return<void>]
spin_many([Atomic<i64>] counter, [i32] rounds) {
  [i32 mut] i{0i32}
  while(i < rounds) {
    [i64 mut] seen{counter.load()}
    while(not(counter.compareExchange(seen, seen + 1i64))) {
      seen = counter.load()
    }
    i = i + 1i32
  }
}

[effects(atomic, heap_alloc, io_out, task), return<int>]
main() {
  counter{Atomic<i64>{0i64}}
  [Task<void>] a{[spawn] spin_many(counter, 100000i32)}
  [Task<void>] b{[spawn] spin_many(counter, 100000i32)}
  [Task<void>] c{[spawn] spin_many(counter, 100000i32)}
  [Task<void>] d{[spawn] spin_many(counter, 100000i32)}
  wait(a)
  wait(b)
  wait(c)
  wait(d)
  print_line(counter.load())
  return(0i32)
}
)";
  program.exitCode = 0;
  program.stdoutText = "400000\n";
  runAtomicProgram(program);
}

TEST_CASE("Atomic<i32> and Atomic<i64> operations return the documented values") {
  program_matrix::ProgramCase program;
  program.name = "atomic_operations";
  program.source = R"(
import /std/sync/*

[effects(atomic), return<i32>]
take([Atomic<i32>] flag) {
  return(flag.exchange(1i32))
}

[effects(atomic, heap_alloc, io_out, task), return<int>]
main() {
  small{Atomic<i32>{7i32}}
  print_line(small.exchange(9i32))
  print_line(small.add(-10i32))
  print_line(small.load())
  small.store(2147483647i32)
  small.add(1i32)
  print_line(small.load())
  print_line(small.compareExchange(5i32, 6i32))
  print_line(small.compareExchange(-2147483648i32, 6i32))
  print_line(small.load())
  wide{Atomic<i64>{1099511627776i64}}
  print_line(wide.add(-1i64))
  print_line(wide.exchange(-3i64))
  print_line(wide.compareExchange(-3i64, 12i64))
  print_line(wide.load())
  // Exactly one of two tasks takes the flag.
  flag{Atomic<i32>{0i32}}
  [Task<i32>] first{[spawn] take(flag)}
  [Task<i32>] second{[spawn] take(flag)}
  print_line(wait(first) + wait(second))
  print_line(flag.load())
  return(0i32)
}
)";
  program.exitCode = 0;
  program.stdoutText =
      "7\n9\n-1\n-2147483648\n0\n1\n6\n1099511627776\n1099511627775\n1\n12\n1\n1\n";
  runAtomicProgram(program);
}

TEST_CASE("dropping every copy of an Atomic frees its cell") {
  // `heapLiveBlocks()` counts the live slot-heap blocks of the thread it runs on. Both tasks of a round are waited
  // before the round's handles go, so the spawner frees every cell it allocated.
  program_matrix::ProgramCase program;
  program.name = "atomic_cell_freed";
  program.source = R"(
import /std/sync/*
import /std/text/*

[effects(atomic), return<void>]
bump([Atomic<i32>] counter, [i32] rounds) {
  [i32 mut] i{0i32}
  while(i < rounds) {
    counter.add(1i32)
    i = i + 1i32
  }
}

[effects(atomic), return<i32>]
keep_copy([Atomic<i32>] counter) {
  [Atomic<i32>] again{counter}
  [Atomic<i32>] moved{move(again)}
  return(moved.load())
}

[effects(atomic, heap_alloc, task), return<i32>]
round() {
  counter{Atomic<i32>{5i32}}
  [Task<void>] a{[spawn] bump(counter, 10i32)}
  [Task<void>] b{[spawn] bump(counter, 10i32)}
  wait(a)
  wait(b)
  [Atomic<i32>] copy{counter}
  copy.add(100i32)
  return(keep_copy(copy) + counter.load())
}

[effects(atomic, heap_alloc, io_out, task), return<int>]
main() {
  [i32] first{round()}
  [i32] before{heapLiveBlocks()}
  [i32 mut] sum{0i32}
  repeat(50i32) {
    sum = sum + round()
  }
  print_line(first)
  print_line(sum)
  print_line(heapLiveBlocks() - before)
  [i32] outer{heapLiveBlocks()}
  if(true) {
    held{Atomic<i64>{1i64}}
    print_line(heapLiveBlocks() - outer)
  }
  print_line(heapLiveBlocks() - outer)
  return(0i32)
}
)";
  program.exitCode = 0;
  program.stdoutText = "250\n12500\n0\n1\n0\n";
  runAtomicProgram(program);
}

TEST_CASE("the native and Wasm backends reject a program with atomics by name") {
  const program_matrix::Outcome wasm =
      compileFor("atomic_rejected_wasm", "wasm", SmallAtomicProgram);
  CHECK(wasm.exitCode != 0);
  CHECK_MESSAGE(wasm.err.find("atomic operations are not supported by the wasm backend") !=
                    std::string::npos,
                wasm.err);
  if (program_matrix::nativeSupported()) {
    const program_matrix::Outcome native =
        compileFor("atomic_rejected_native", "native", SmallAtomicProgram);
    CHECK(native.exitCode != 0);
    CHECK_MESSAGE(native.err.find("atomic operations are not supported by the native backend") !=
                      std::string::npos,
                  native.err);
  }
}

TEST_SUITE_END();
