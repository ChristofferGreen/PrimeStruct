#include "program_matrix.h"

TEST_SUITE_BEGIN("primestruct.program_matrix.mutexes");

// `/std/sync` `Mutex<T>` (docs/spec/stdlib-reference.md, std/sync): tasks reach one value through copies of a
// `Mutex`, one at a time. The VM runs each program on the eager default schedule and on seeds 1 to 8, where every
// lock and unlock is a switch point; optexe runs each task on a thread. Nothing here depends on how the threads are
// scheduled or on time: a program has one output on every schedule, or (the opposite-order case, VM only) one
// outcome per seed. Each matrix case builds one optexe binary (host C++ compiler), which takes a few seconds.

namespace {

void runMutexProgram(program_matrix::ProgramCase program) {
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

std::filesystem::path writeSource(const std::string &name, const std::string &source) {
  const std::filesystem::path dir =
      primec::testing::testScratchPath("program_matrix/" + name + "/x").parent_path();
  std::filesystem::create_directories(dir);
  const std::filesystem::path sourcePath = dir / (name + ".prime");
  std::ofstream file(sourcePath, std::ios::binary);
  file << source;
  return sourcePath;
}

// What `primec --emit=<kind>` says about `source`.
program_matrix::Outcome
compileFor(const std::string &name, const std::string &kind, const std::string &source) {
  const std::filesystem::path sourcePath = writeSource(name, source);
  const std::filesystem::path outPath = sourcePath.parent_path() / (name + "." + kind + ".out.bin");
  return program_matrix::runCaptured("./primec --emit=" + kind + " " +
                                         program_matrix::quote(sourcePath.string()) + " -o " +
                                         program_matrix::quote(outPath.string()),
                                     sourcePath.parent_path() / (name + "." + kind));
}

const char *const SmallMutexProgram = R"(
import /std/sync/*

[effects(lock, heap_alloc, io_out), return<int>]
main() {
  cell{Mutex<i64>{5i64}}
  print_line(cell.locked([]([Reference<i64>] value) { return(value + 0i64) }))
  return(0i32)
}
)";

} // namespace

TEST_CASE("two tasks record into one Mutex<Tally>") {
  program_matrix::ProgramCase program;
  program.name = "mutex_tally";
  program.source = R"(
import /std/sync/*

[struct]
Tally() {
  [i64 mut] total{0i64}
  [i32 mut] entries{0i32}
}

[effects(lock), return<void>]
record([Mutex<Tally>] tally, [i64] amount) {
  tally.locked([amount]([Reference<Tally>] state) {
    state.total = state.total + amount
    state.entries = state.entries + 1i32
  })
}

[effects(task, lock, heap_alloc, io_out), return<int>]
main() {
  tally{Mutex<Tally>{Tally{}}}
  [Task<void>] left{[spawn] record(tally, 3i64)}
  [Task<void>] right{[spawn] record(tally, 4i64)}
  wait(left)
  wait(right)
  [i64] total{tally.locked([]([Reference<Tally>] state) { return(state.total) })}
  print_line(total)
  print_line(tally.locked([]([Reference<Tally>] state) { return(state.entries) }))
  return(0i32)
}
)";
  program.exitCode = 0;
  program.stdoutText = "7\n2\n";
  runMutexProgram(program);
}

TEST_CASE("eight tasks append a thousand entries each to one Mutex<vector<i64>>") {
  program_matrix::ProgramCase program;
  program.name = "mutex_vector_append";
  program.source = R"(
import /std/collections/*
import /std/sync/*

[effects(lock, heap_alloc), return<void>]
append_many([Mutex<vector<i64>>] entries, [i64] base) {
  [i64 mut] i{0i64}
  while(i < 1000i64) {
    [i64] entry{base + i}
    entries.locked([entry]([Reference<vector<i64>>] values) {
      values.push(entry)
    })
    i = i + 1i64
  }
}

[effects(task, lock, heap_alloc, io_out), return<int>]
main() {
  entries{Mutex<vector<i64>>{vector<i64>{}}}
  [Task<void>] a{[spawn] append_many(entries, 0i64)}
  [Task<void>] b{[spawn] append_many(entries, 1000i64)}
  [Task<void>] c{[spawn] append_many(entries, 2000i64)}
  [Task<void>] d{[spawn] append_many(entries, 3000i64)}
  [Task<void>] e{[spawn] append_many(entries, 4000i64)}
  [Task<void>] f{[spawn] append_many(entries, 5000i64)}
  [Task<void>] g{[spawn] append_many(entries, 6000i64)}
  [Task<void>] h{[spawn] append_many(entries, 7000i64)}
  wait(a)
  wait(b)
  wait(c)
  wait(d)
  wait(e)
  wait(f)
  wait(g)
  wait(h)
  print_line(entries.locked([]([Reference<vector<i64>>] values) { return(values.count()) }))
  // Every entry arrived once: the sum of 0 to 7999.
  print_line(entries.locked([]([Reference<vector<i64>>] values) {
    [i64 mut] sum{0i64}
    [i32 mut] at{0i32}
    while(at < values.count()) {
      sum = sum + values[at]
      at = at + 1i32
    }
    return(sum)
  }))
  return(0i32)
}
)";
  program.exitCode = 0;
  program.stdoutText = "8000\n31996000\n";
  runMutexProgram(program);
}

TEST_CASE("locked returns the body's value and a body borrows the caller's locals") {
  // `Noisy` reports its copies and destroys. Making the mutex copies the value into its cell; each handle also
  // keeps the value it was made from, so a handle copy copies that one. The cell's value and the two handles' values
  // are the three destroys.
  program_matrix::ProgramCase program;
  program.name = "mutex_locked_values";
  program.source = R"(
import /std/sync/*
import /std/text/*

[struct]
Noisy() {
  [i64 mut] value{0i64}

  [mut effects(io_out)]
  Copy([Reference<Self>] other) {
    this.value = other.value
    print_line("copy"utf8)
  }

  [mut effects(io_out)]
  Destroy() {
    print_line("destroy"utf8)
  }
}

[effects(lock), return<void>]
bump([Mutex<i64>] cell) {
  cell.locked([]([Reference<i64> mut] value) { value = value + 1i64 })
}

[effects(task, lock, heap_alloc), return<i64>]
round() {
  cell{Mutex<i64>{1i64}}
  other{cell}
  [Task<void>] task{[spawn] bump(other)}
  wait(task)
  bump(cell)
  [Mutex<i64>] moved{move(other)}
  return(moved.locked([]([Reference<i64>] value) { return(value + 0i64) }))
}

[effects(lock, heap_alloc, io_out), return<void>]
noisy() {
  cell{Mutex<Noisy>{Noisy{}}}
  print_line("made"utf8)
  [i64 mut] seen{0i64}
  [mut] look{[ref seen]([Reference<Noisy>] state) {
    state.value = state.value + 5i64
    seen = state.value
  }}
  cell.locked(look)
  cell.locked(look)
  print_line(seen)
  other{cell}
  print_line("copied"utf8)
  [i64] total{other.locked([]([Reference<Noisy>] state) { return(state.value) })}
  print_line(total)
}

[effects(task, lock, heap_alloc, io_out), return<int>]
main() {
  noisy()
  print_line(round())
  [i32] before{heapLiveBlocks()}
  repeat(20i32) {
    round()
  }
  print_line(heapLiveBlocks() - before)
  return(0i32)
}
)";
  program.exitCode = 0;
  program.stdoutText = "copy\nmade\n10\ncopy\ncopied\n10\ndestroy\ndestroy\ndestroy\n3\n0\n";
  runMutexProgram(program);
}

TEST_CASE("locking a mutex again inside its own body faults") {
  program_matrix::ProgramCase program;
  program.name = "mutex_locked_again";
  program.source = R"(
import /std/sync/*

[effects(lock, heap_alloc, io_out), return<int>]
main() {
  cell{Mutex<i64>{1i64}}
  other{cell}
  [i64] got{cell.locked([other]([Reference<i64>] value) {
    return(other.locked([]([Reference<i64>] inner) { return(inner + 0i64) }))
  })}
  print_line(got)
  return(0i32)
}
)";
  program.exitCode = 3;
  program.stdoutText = "";
  program.vmStderr = "VM error: mutex locked again by the task that holds it\n";
  runMutexProgram(program);
}

TEST_CASE(
    "two tasks locking two mutexes in opposite order deadlock on a VM seed that interleaves them") {
  // A seed fixes the schedule, so each run below has one outcome: the tasks either run one after the other (both
  // counters reach 2) or each takes its first lock and waits for the other's.
  const std::filesystem::path sourcePath = writeSource("mutex_opposite_order", R"(
import /std/sync/*

[effects(lock), return<void>]
both([Mutex<i64>] first, [Mutex<i64>] second) {
  first.locked([second]([Reference<i64> mut] a) {
    second.locked([]([Reference<i64> mut] b) {
      b = b + 1i64
    })
    a = a + 1i64
  })
}

[effects(task, lock, heap_alloc, io_out), return<int>]
main() {
  left{Mutex<i64>{0i64}}
  right{Mutex<i64>{0i64}}
  [Task<void>] forward{[spawn] both(left, right)}
  [Task<void>] backward{[spawn] both(right, left)}
  wait(forward)
  wait(backward)
  print_line(left.locked([]([Reference<i64>] value) { return(value + 0i64) }))
  return(0i32)
}
)");
  int deadlocks = 0;
  int finished = 0;
  for (int seed = 1; seed <= 16; ++seed) {
    INFO("seed " << seed);
    const program_matrix::Outcome run =
        program_matrix::runCaptured("./primevm " + program_matrix::quote(sourcePath.string()) +
                                        " --entry /main --vm-schedule-seed " + std::to_string(seed),
                                    sourcePath.parent_path() / ("seed" + std::to_string(seed)));
    if (run.exitCode == 0) {
      CHECK(run.out == "2\n");
      ++finished;
    } else {
      CHECK(run.exitCode == 3);
      CHECK(run.out.empty());
      CHECK(run.err == "VM error: deadlock: every task is waiting\n");
      ++deadlocks;
    }
  }
  CHECK(deadlocks > 0);
  CHECK(finished > 0);
}

TEST_CASE("the native and Wasm backends reject a program with a mutex by name") {
  // Making a mutex stores its lock and handle count with atomic opcodes, which these backends meet first; the
  // mutex opcodes' own message is checked on hand-built IR in tests/unit/runtime/test_vm_mutexes.cpp.
  const program_matrix::Outcome wasm = compileFor("mutex_rejected_wasm", "wasm", SmallMutexProgram);
  CHECK(wasm.exitCode != 0);
  CHECK_MESSAGE(wasm.err.find("atomic operations are not supported by the wasm backend") !=
                    std::string::npos,
                wasm.err);
  if (program_matrix::nativeSupported()) {
    const program_matrix::Outcome native =
        compileFor("mutex_rejected_native", "native", SmallMutexProgram);
    CHECK(native.exitCode != 0);
    CHECK_MESSAGE(native.err.find("atomic operations are not supported by the native backend") !=
                      std::string::npos,
                  native.err);
  }
}

TEST_SUITE_END();
