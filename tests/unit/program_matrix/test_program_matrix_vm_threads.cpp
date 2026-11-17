#include "program_matrix.h"

TEST_SUITE_BEGIN("primestruct.program_matrix.vm_threads");

// `/std/sync` Atomic, Mutex and Channel programs under `primevm --vm-threads` (docs/spec/vm-design.md, Tasks on
// threads; TODO-5897), each run Repeats times in a row: a race, a lost wake-up or a false deadlock shows as a failed
// run. Nothing here sleeps or measures time; each program has one answer on every interleaving. Only the threads
// config runs (the other configs run these programs in their own matrices).

namespace {

constexpr int Repeats = 20;

void runRepeated(const program_matrix::ProgramCase &program) {
  // The eager run, then `Repeats` threads runs, each printing what the eager run printed.
  program_matrix::runThreadsCorpus(program, Repeats);
}

} // namespace

TEST_CASE("four tasks add to one shared Atomic<i64> under --vm-threads") {
  program_matrix::ProgramCase program;
  program.name = "threads_atomic_shared_add";
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
  [Task<void>] a{[spawn] add_many(counter, 20000i32)}
  [Task<void>] b{[spawn] add_many(counter, 20000i32)}
  [Task<void>] c{[spawn] add_many(counter, 20000i32)}
  [Task<void>] d{[spawn] add_many(counter, 20000i32)}
  wait(a)
  wait(b)
  wait(c)
  wait(d)
  print_line(counter.load())
  return(0i32)
}
)";
  program.exitCode = 0;
  program.stdoutText = "80000\n";
  runRepeated(program);
}

TEST_CASE("two tasks record into one Mutex<Tally> under --vm-threads") {
  program_matrix::ProgramCase program;
  program.name = "threads_mutex_tally";
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
  runRepeated(program);
}

TEST_CASE("a producer and a consumer move 100 jobs through a Channel under --vm-threads") {
  program_matrix::ProgramCase program;
  program.name = "threads_channel_sum_jobs";
  program.source = R"(
import /std/sync/*

[effects(channel)]
produce([Channel<i64>] jobs, [i64] count) {
  [i64 mut] i{0i64}
  while(i < count) {
    jobs.send(i)
    i = i + 1i64
  }
  jobs.close()
}

[effects(channel), return<i64>]
sum_jobs([Channel<i64>] jobs) {
  [i64 mut] sum{0i64}
  [bool mut] open{true}
  while(open) {
    pick(jobs.receive()) {
      none {
        open = false
      }
      some(job) {
        sum = sum + job
      }
    }
  }
  return(sum)
}

[effects(task, channel, heap_alloc, io_out), return<int>]
main() {
  jobs{Channel<i64>{[capacity] 4i32}}
  [Task<void>] producer{[spawn] produce(jobs, 100i64)}
  [Task<i64>] consumer{[spawn] sum_jobs(jobs)}
  wait(producer)
  print_line(wait(consumer))
  return(0i32)
}
)";
  program.exitCode = 0;
  program.stdoutText = "4950\n";
  runRepeated(program);
}

TEST_CASE(
    "a task receiving from a channel nobody sends on is the deadlock fault under --vm-threads") {
  program_matrix::ProgramCase program;
  program.name = "threads_channel_receive_deadlock";
  program.source = R"(
import /std/sync/*

[effects(channel), return<i64>]
take([Channel<i64>] jobs) {
  [i64 mut] got{-1i64}
  pick(jobs.receive()) {
    none {
    }
    some(job) {
      got = job
    }
  }
  return(got)
}

[effects(task, channel, heap_alloc, io_out), return<int>]
main() {
  jobs{Channel<i64>{[capacity] 2i32}}
  [Task<i64>] taker{[spawn] take(jobs)}
  print_line(wait(taker))
  return(0i32)
}
)";
  program.exitCode = 3;
  program.stdoutText = "";
  program.vmStderr = "VM error: deadlock: every task is waiting\n";
  runRepeated(program);
}

TEST_SUITE_END();
