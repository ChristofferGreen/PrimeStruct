#include "program_matrix.h"

TEST_SUITE_BEGIN("primestruct.program_matrix.mutexes");

// `/std/sync` `Mutex<T>` (docs/spec/stdlib-reference.md, std/sync): tasks reach one value through copies of a
// `Mutex`, one at a time. The VM runs each program on the eager default schedule and on seeds 1 to 3, where every
// lock and unlock is a switch point; optexe and arm64 native run each task on a thread. Nothing here depends on how the threads are
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
  // `Noisy` reports its copies and destroys. Making the mutex moves the value into its cell: the handle keeps
  // nothing (its `initial` field is construction-only), so a handle copy copies nothing and the cell's value is the
  // one destroy.
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
  cell.locked([ref seen]([Reference<Noisy>] state) {
    state.value = state.value + 5i64
    seen = state.value
  })
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
  program.stdoutText = "made\n10\ncopied\n10\ndestroy\n3\n0\n";
  // The native backend has no heap statistics.
  program.skipConfigs = {"native-O0", "native-O2"};
  runMutexProgram(program);
}

TEST_CASE("a mutex made from a local copies it once and the cell owns the only copy") {
  // `cell{Mutex<Noisy>{n}}` copies the place `n` into the cell (one `copy`); `n` and the cell's value are the two
  // destroys, and the handle holds nothing of its own.
  program_matrix::ProgramCase program;
  program.name = "mutex_made_from_local";
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

[effects(lock, heap_alloc, io_out), return<int>]
main() {
  [Noisy] n{Noisy{[value] 4i64}}
  cell{Mutex<Noisy>{n}}
  other{cell}
  print_line("made"utf8)
  print_line(other.locked([]([Reference<Noisy>] state) { return(state.value) }))
  return(0i32)
}
)";
  program.exitCode = 0;
  program.stdoutText = "copy\nmade\n4\ndestroy\ndestroy\n";
  runMutexProgram(program);
}

TEST_CASE("a Mutex or Atomic handle is two pointers and a flag, whatever it holds") {
  if (program_matrix::corpusOnly()) {
    return; // not a concurrent program run (TODO-5900)
  }
  // The value a handle was made from is a construction-only field: the struct layout has no slot for it, so a
  // `Mutex` of a 64-byte struct is the same 24 bytes as a `Mutex<i64>`, and an `Atomic` is 16.
  const std::filesystem::path sourcePath = writeSource("mutex_handle_layout", R"(
import /std/sync/*

[struct]
Wide() {
  [i64] a{1i64}
  [i64] b{2i64}
  [i64] c{3i64}
  [i64] d{4i64}
  [i64] e{5i64}
  [i64] f{6i64}
  [i64] g{7i64}
  [i64] h{8i64}
}

[effects(lock, atomic, heap_alloc), return<int>]
main() {
  wide{Mutex<Wide>{Wide{}}}
  small{Mutex<i64>{1i64}}
  counter{Atomic<i64>{2i64}}
  return(0i32)
}
)");
  const program_matrix::Outcome dump = program_matrix::runCaptured(
      "./primec --dump-stage ir-lowered " + program_matrix::quote(sourcePath.string()),
      sourcePath.parent_path() / "dump");
  CHECK_MESSAGE(dump.exitCode == 0, dump.err);
  size_t mutexLayouts = 0;
  size_t atomicLayouts = 0;
  std::istringstream lines(dump.out);
  for (std::string line; std::getline(lines, line);) {
    if (line.find("/std/sync/Mutex__t") != std::string::npos) {
      CHECK_MESSAGE(line.find(" size=24 align=8 ") != std::string::npos, line);
      ++mutexLayouts;
    } else if (line.find("/std/sync/Atomic__t") != std::string::npos) {
      CHECK_MESSAGE(line.find(" size=16 align=8 ") != std::string::npos, line);
      ++atomicLayouts;
    }
  }
  CHECK(mutexLayouts == 2);
  CHECK(atomicLayouts == 1);
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
  if (program_matrix::corpusOnly()) {
    return; // not a concurrent program run (TODO-5900)
  }
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

// A body that names a struct of the namespace it is written in (`[Reference<Job>]` in `namespace zed`) resolves it
// there (TODO-5935).
TEST_CASE("a locked body names a struct of its namespace") {
  program_matrix::ProgramCase program;
  program.name = "mutex_namespace_struct_body";
  program.source = R"(
import /std/sync/*

namespace zed {
  [public struct]
  Job {
    [i32 mut] id{0i32}
    [i32 mut] weight{0i32}
  }

  [effects(lock, heap_alloc, io_out), return<int>]
  run_job() {
    cell{Mutex<Job>{Job{[id] 3i32, [weight] 4i32}}}
    cell.locked([]([Reference<Job>] job) { job.weight = job.weight + 1i32 })
    print_line(cell.locked([]([Reference<Job>] job) { return(job.id + job.weight) }))
    return(0i32)
  }
}

[effects(lock, heap_alloc, io_out), return<int>]
main() {
  return(/zed/run_job())
}
)";
  program.exitCode = 0;
  program.stdoutText = "8\n";
  runMutexProgram(program);
}

TEST_CASE("the Wasm backend rejects a program with a mutex by name") {
  if (program_matrix::corpusOnly()) {
    return; // not a concurrent program run (TODO-5900)
  }
  // Making a mutex stores its lock and handle count with atomic opcodes, which the WASI profile runs (TODO-6321), so
  // the first refusal is the lock opcode (TODO-6322 runs it).
  const program_matrix::Outcome wasm = compileFor("mutex_rejected_wasm", "wasm", SmallMutexProgram);
  CHECK(wasm.exitCode != 0);
  CHECK_MESSAGE(wasm.err.find("mutex operations are not supported by the wasm backend") !=
                    std::string::npos,
                wasm.err);
}

// `init(*slot, v)` of a builtin `vector<T>` stores the whole five-slot record (count, capacity, data, owner flag): a
// named owner and a temporary hand their buffer to the slot (a temporary's owner flag is cleared), a view or a borrowed
// parameter is copied with its elements. Dropping the slot frees the buffer, so the live block count of the slot heap
// is back where it started. The heap statistic exists on the VM and optexe only; the values case below runs on every
// config.
TEST_CASE("init of a builtin vector into uninitialized storage stores the whole record") {
  program_matrix::ProgramCase program;
  program.name = "init_builtin_vector_slot";
  program.source = R"(
import /std/text/*
import /std/collections/*
import /std/sync/*

[unsafe effects(heap_alloc), return<void>]
release([Pointer<uninitialized<vector<i64>>> mut] slot) {
  drop(*slot)
  /std/intrinsics/memory/free(slot)
}

[effects(heap_alloc), return<vector<i64>>]
make() {
  return(vector<i64>(7i64, 8i64, 9i64, 10i64))
}

[effects(heap_alloc), return<void>]
store_borrowed([Pointer<uninitialized<vector<i64>>> mut] slot, [vector<i64> mut] borrowed) {
  init(*slot, borrowed)
}

[effects(heap_alloc), return<void>]
store_view([Pointer<uninitialized<vector<i64>>> mut] slot, [vector<i64>] shared) {
  init(*slot, shared)
}

[effects(heap_alloc, lock, io_out), return<int>]
main() {
  [i32] start{heapLiveBlocks()}
  if(true) {
    [Pointer<uninitialized<vector<i64>>> mut] slot{/std/intrinsics/memory/alloc<uninitialized<vector<i64>>>(1i32)}
    init(*slot, make())
    [Reference<vector<i64>>] stored{borrow(*slot)}
    print_line(stored.count() * 100i32 + convert<i32>(stored[3i32]))
    release(slot)
  }
  print_line(heapLiveBlocks() - start)
  if(true) {
    [vector<i64> mut] owner{vector<i64>(1i64, 2i64, 3i64)}
    [Pointer<uninitialized<vector<i64>>> mut] slot{/std/intrinsics/memory/alloc<uninitialized<vector<i64>>>(1i32)}
    init(*slot, owner)
    [Reference<vector<i64>>] stored{borrow(*slot)}
    print_line(stored.count() * 100i32 + convert<i32>(stored[2i32]))
    release(slot)
  }
  print_line(heapLiveBlocks() - start)
  if(true) {
    [vector<i64> mut] a{vector<i64>(1i64, 2i64, 3i64)}
    [Pointer<uninitialized<vector<i64>>> mut] slot{/std/intrinsics/memory/alloc<uninitialized<vector<i64>>>(1i32)}
    store_borrowed(slot, a)
    [Reference<vector<i64>> mut] stored{borrow(*slot)}
    stored.push(4i64)
    a.push(9i64)
    a.push(10i64)
    print_line(a.count() * 100i32 + stored.count())
    release(slot)
    print_line(a[4i32])
  }
  print_line(heapLiveBlocks() - start)
  if(true) {
    [vector<i64>] shared{vector<i64>(5i64, 6i64)}
    [Pointer<uninitialized<vector<i64>>> mut] slot{/std/intrinsics/memory/alloc<uninitialized<vector<i64>>>(1i32)}
    store_view(slot, shared)
    [Reference<vector<i64>>] stored{borrow(*slot)}
    print_line(stored.count() * 10i32 + shared.count())
    release(slot)
  }
  print_line(heapLiveBlocks() - start)
  if(true) {
    cell{Mutex<vector<i64>>{vector<i64>(1i64, 2i64, 3i64)}}
    copy{cell}
    print_line(cell.locked([]([Reference<vector<i64>> mut] values) { values.push(4i64) return(values.count()) }))
    print_line(copy.locked([]([Reference<vector<i64>>] values) { return(values[3i32]) }))
  }
  print_line(heapLiveBlocks() - start)
  return(0i32)
}
)";
  program.exitCode = 0;
  program.stdoutText = "410\n0\n303\n0\n"
                       "504\n10\n0\n"
                       "22\n0\n"
                       "4\n4\n0\n";
  program.onlyConfigs = {"vm-step-O0", "vm-O0", "vm-O2", "optexe-O2"};
  program_matrix::runProgramMatrix(program);
}

TEST_CASE("a Mutex made from a non-empty vector reads its elements back") {
  program_matrix::ProgramCase program;
  program.name = "mutex_vector_from_elements";
  program.source = R"(
import /std/collections/*
import /std/sync/*

[effects(heap_alloc, lock, io_out), return<int>]
main() {
  [vector<i64> mut] seed{vector<i64>(4i64, 5i64, 6i64)}
  cell{Mutex<vector<i64>>{seed}}
  copy{cell}
  print_line(cell.locked([]([Reference<vector<i64>>] values) { return(values.count()) }))
  print_line(copy.locked([]([Reference<vector<i64>> mut] values) { values.push(7i64) return(values[2i32]) }))
  print_line(cell.locked([]([Reference<vector<i64>>] values) { return(values[3i32]) }))
  return(0i32)
}
)";
  program.exitCode = 0;
  program.stdoutText = "3\n6\n7\n";
  runMutexProgram(program);
}

TEST_SUITE_END();
