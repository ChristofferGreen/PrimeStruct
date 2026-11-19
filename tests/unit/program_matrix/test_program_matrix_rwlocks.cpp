#include "program_matrix.h"

TEST_SUITE_BEGIN("primestruct.program_matrix.rwlocks");

// `/std/sync` `RwLock<T>` (docs/spec/stdlib-reference.md, std/sync): any number of tasks read a value together and a
// writer is alone. The VM runs each program on the eager default schedule and on seeds 1 to 3; optexe and arm64
// native run each task on a thread. Every program has one output on every schedule: the tasks hand each other
// signals through Channels, so what a task sees does not depend on timing.

namespace {

void runRwLockProgram(program_matrix::ProgramCase program) {
  std::vector<program_matrix::ExecutionConfig> configs = {
      {"vm-step-O0", program_matrix::Backend::VmStep, 0, false},
      {"vm-O0", program_matrix::Backend::Vm, 0, false},
      {"vm-O2", program_matrix::Backend::Vm, 2, false},
      {"optexe-O2", program_matrix::Backend::OptExe, 2, true},
  };
  if (program_matrix::nativeSupported()) {
    configs.push_back({"native-O0", program_matrix::Backend::Native, 0, false});
    configs.push_back({"native-O2", program_matrix::Backend::Native, 2, false});
  }
  for (int seed = 1; seed <= 3; ++seed) {
    program_matrix::ExecutionConfig config{
        "vm-O2-seed" + std::to_string(seed), program_matrix::Backend::Vm, 2, false};
    config.vmFlags = {"--vm-schedule-seed", std::to_string(seed)};
    configs.push_back(std::move(config));
  }
  configs.push_back(program_matrix::vmThreadsConfig());
  for (const program_matrix::ExecutionConfig &config : configs) {
    program.onlyConfigs.push_back(config.name);
  }
  program_matrix::runProgramMatrix(program, configs);
}

} // namespace

TEST_CASE("two readers are inside an RwLock together") {
  // Each reader signals that it is inside and then waits for the other's signal, so both complete only when both
  // are inside `read` at once: a lock that admitted one reader at a time would wait for ever.
  program_matrix::ProgramCase program;
  program.name = "rwlock_readers_together";
  program.source = R"(
import /std/sync/*

[effects(channel), return<i64>]
take_or_zero([Channel<i64>] from) {
  [i64 mut] result{0i64}
  pick(from.receive()) {
    none {
    }
    some(got) {
      result = got
    }
  }
  return(result)
}

[effects(lock, channel), return<i64>]
meet([RwLock<i64>] cell, [Channel<i64>] mine, [Channel<i64>] theirs) {
  return(cell.read([mine, theirs]([i64] value) {
    mine.send(value)
    return(value + take_or_zero(theirs))
  }))
}

[effects(task, lock, channel, heap_alloc, io_out), return<int>]
main() {
  cell{RwLock<i64>{20i64}}
  left{Channel<i64>{[capacity] 1i32}}
  right{Channel<i64>{[capacity] 1i32}}
  [Task<i64>] a{[spawn] meet(cell, left, right)}
  [Task<i64>] b{[spawn] meet(cell, right, left)}
  print_line(wait(a))
  print_line(wait(b))
  return(0i32)
}
)";
  program.exitCode = 0;
  program.stdoutText = "40\n40\n";
  runRwLockProgram(program);
}

TEST_CASE("a writer waits for the reader inside and the reader sees the old value") {
  // The reader is inside `read` and holds there until main releases it; the writer is spawned meanwhile. The
  // reader reads the value after the release, so it sees 0 only if the writer is kept out until it leaves.
  program_matrix::ProgramCase program;
  program.name = "rwlock_writer_waits_for_reader";
  program.source = R"(
import /std/sync/*

[effects(channel), return<void>]
await([Channel<i64>] from) {
  pick(from.receive()) {
    none {
    }
    some(got) {
    }
  }
}

[effects(lock, channel), return<i64>]
hold_and_read([RwLock<i64>] cell, [Channel<i64>] inside, [Channel<i64>] release) {
  return(cell.read([inside, release]([i64] value) {
    inside.send(1i64)
    await(release)
    return(value + 0i64)
  }))
}

[effects(lock), return<void>]
store([RwLock<i64>] cell, [i64] next) {
  cell.write([next]([Reference<i64> mut] value) { value = next })
}

[effects(task, lock, channel, heap_alloc, io_out), return<int>]
main() {
  cell{RwLock<i64>{0i64}}
  inside{Channel<i64>{[capacity] 1i32}}
  release{Channel<i64>{[capacity] 1i32}}
  [Task<i64>] reader{[spawn] hold_and_read(cell, inside, release)}
  await(inside)
  [Task<void>] writer{[spawn] store(cell, 9i64)}
  release.send(1i64)
  print_line(wait(reader))
  wait(writer)
  print_line(cell.read([]([i64] value) { return(value + 0i64) }))
  return(0i32)
}
)";
  program.exitCode = 0;
  program.stdoutText = "0\n9\n";
  runRwLockProgram(program);
}

TEST_CASE("a writer excludes another writer") {
  // The first writer is inside `write` until main releases it, and sets the value after the release; the second
  // adds ten. The second runs after the first left, so the value ends 15, not 5.
  program_matrix::ProgramCase program;
  program.name = "rwlock_writer_excludes_writer";
  program.source = R"(
import /std/sync/*

[effects(channel), return<void>]
await([Channel<i64>] from) {
  pick(from.receive()) {
    none {
    }
    some(got) {
    }
  }
}

[effects(lock, channel), return<void>]
hold_and_set([RwLock<i64>] cell, [Channel<i64>] inside, [Channel<i64>] release) {
  cell.write([inside, release]([Reference<i64> mut] value) {
    inside.send(1i64)
    await(release)
    value = 5i64
  })
}

[effects(lock), return<void>]
add_ten([RwLock<i64>] cell) {
  cell.write([]([Reference<i64> mut] value) { value = value + 10i64 })
}

[effects(task, lock, channel, heap_alloc, io_out), return<int>]
main() {
  cell{RwLock<i64>{0i64}}
  inside{Channel<i64>{[capacity] 1i32}}
  release{Channel<i64>{[capacity] 1i32}}
  [Task<void>] first{[spawn] hold_and_set(cell, inside, release)}
  await(inside)
  [Task<void>] second{[spawn] add_ten(cell)}
  release.send(1i64)
  wait(first)
  wait(second)
  print_line(cell.read([]([i64] value) { return(value + 0i64) }))
  return(0i32)
}
)";
  program.exitCode = 0;
  program.stdoutText = "15\n";
  runRwLockProgram(program);
}

TEST_CASE("readers never see a writer's half-finished update") {
  // A writer keeps `low` and `high` equal outside its body and touches an Atomic between the two stores, which is
  // where a reader would see them differ if it were let in. Each reader counts the times it saw them differ.
  program_matrix::ProgramCase program;
  program.name = "rwlock_no_torn_reads";
  program.source = R"(
import /std/sync/*

[struct]
Pair() {
  [i64 mut] low{0i64}
  [i64 mut] high{0i64}
}

[effects(lock, atomic), return<void>]
bump([RwLock<Pair>] pair, [Atomic<i64>] ticks, [i32] rounds) {
  [i32 mut] done{0i32}
  while(done < rounds) {
    pair.write([ticks]([Reference<Pair>] state) {
      state.low = state.low + 1i64
      ticks.add(1i64)
      state.high = state.high + 1i64
    })
    done = done + 1i32
  }
}

[effects(lock, atomic), return<void>]
look([RwLock<Pair>] pair, [Atomic<i64>] ticks, [Atomic<i64>] torn, [i32] rounds) {
  [i32 mut] done{0i32}
  while(done < rounds) {
    pair.read([ticks, torn]([Pair] state) {
      ticks.add(1i64)
      if(state.low != state.high) {
        torn.add(1i64)
      }
    })
    done = done + 1i32
  }
}

[effects(task, lock, atomic, heap_alloc, io_out), return<int>]
main() {
  pair{RwLock<Pair>{Pair{}}}
  ticks{Atomic<i64>{0i64}}
  torn{Atomic<i64>{0i64}}
  [Task<void>] w1{[spawn] bump(pair, ticks, 200i32)}
  [Task<void>] w2{[spawn] bump(pair, ticks, 200i32)}
  [Task<void>] r1{[spawn] look(pair, ticks, torn, 200i32)}
  [Task<void>] r2{[spawn] look(pair, ticks, torn, 200i32)}
  wait(w1)
  wait(w2)
  wait(r1)
  wait(r2)
  print_line(torn.load())
  print_line(pair.read([]([Pair] state) { return(state.low + state.high) }))
  return(0i32)
}
)";
  program.exitCode = 0;
  program.stdoutText = "0\n800\n";
  runRwLockProgram(program);
}

TEST_CASE("a read body that writes the value is a compile error") {
  if (program_matrix::corpusOnly()) {
    return; // not a concurrent program run
  }
  const std::filesystem::path dir =
      primec::testing::testScratchPath("program_matrix/rwlock_read_writes/x").parent_path();
  std::filesystem::create_directories(dir);
  const std::filesystem::path sourcePath = dir / "rwlock_read_writes.prime";
  {
    std::ofstream file(sourcePath, std::ios::binary);
    file << R"(
import /std/sync/*

[effects(lock, heap_alloc), return<int>]
main() {
  cell{RwLock<i64>{1i64}}
  cell.read([]([i64] value) { value = value + 1i64 })
  return(0i32)
}
)";
  }
  const program_matrix::Outcome run = program_matrix::runCaptured(
      "./primec --emit=vm " + program_matrix::quote(sourcePath.string()) + " -o " +
          program_matrix::quote((dir / "rwlock_read_writes.out.bin").string()),
      dir / "rwlock_read_writes");
  CHECK(run.exitCode != 0);
  CHECK_MESSAGE(run.err.find("assign target must be a mutable binding") != std::string::npos, run.err);
}

TEST_SUITE_END();
