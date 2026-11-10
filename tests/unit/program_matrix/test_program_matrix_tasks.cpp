#include "program_matrix.h"

TEST_SUITE_BEGIN("primestruct.program_matrix.tasks");

// Spawned tasks (docs/spec/vm-design.md, Tasks): the VM runs them under its scheduler, on the eager default schedule
// and on seeds 1 to 8, optexe runs each on a thread, and native runs the spawned calls inline. A program free of races
// prints the same on every schedule. Each case builds one optexe binary (host C++ compiler), which takes a few seconds.

namespace {

std::vector<program_matrix::ExecutionConfig> taskConfigs() {
  std::vector<program_matrix::ExecutionConfig> configs = program_matrix::allConfigs();
  for (int seed = 1; seed <= 8; ++seed) {
    program_matrix::ExecutionConfig config{
        "vm-O2-seed" + std::to_string(seed), program_matrix::Backend::Vm, 2, false};
    config.vmFlags = {"--vm-schedule-seed", std::to_string(seed)};
    configs.push_back(std::move(config));
  }
  return configs;
}

void runTaskProgram(program_matrix::ProgramCase program) {
  // Every config but the old C++ emitter (`exe`), which is only run with PRIMESTRUCT_MATRIX_CONFIGS=all.
  for (const program_matrix::ExecutionConfig &config : taskConfigs()) {
    if (config.name != "exe") {
      program.onlyConfigs.push_back(config.name);
    }
  }
  program_matrix::runProgramMatrix(program, taskConfigs());
}

} // namespace

TEST_CASE("two i64 sums run as tasks") {
  program_matrix::ProgramCase program;
  program.name = "task_two_i64_sums";
  program.source = R"(
[return<i64>]
sum_to([i64] n) {
  [i64 mut] total{0i64}
  [i64 mut] i{0i64}
  while(i < n) {
    total = total + i
    i = i + 1i64
  }
  return(total)
}

[effects(task, io_out) return<int>]
main() {
  [Task<i64>] low{[spawn] sum_to(1000i64)};
  [Task<i64>] high{[spawn] sum_to(2000i64)};
  [i64] lowSum{wait(low)}
  [i64] highSum{wait(high)}
  print_line(lowSum)
  print_line(highSum)
  print_line(highSum - lowSum)
  return(0i32)
}
)";
  program.exitCode = 0;
  program.stdoutText = "499500\n1999000\n1499500\n";
  runTaskProgram(program);
}

TEST_CASE("a task without a result runs for its output") {
  program_matrix::ProgramCase program;
  program.name = "task_void_result";
  program.source = R"(
[effects(io_out) return<void>]
report([i32] count) {
  [i32 mut] i{0i32}
  while(i < count) {
    print_line(i)
    i = i + 1i32
  }
}

[effects(task, io_out) return<int>]
main() {
  [Task<void>] printer{[spawn] report(3i32)};
  wait(printer)
  print_line(9i32)
  return(0i32)
}
)";
  program.exitCode = 0;
  program.stdoutText = "0\n1\n2\n9\n";
  runTaskProgram(program);
}

TEST_CASE("a task spawned in a loop is waited in every iteration") {
  program_matrix::ProgramCase program;
  program.name = "task_spawn_in_loop";
  program.source = R"(
[return<i64>]
square_sum([i64] n) {
  [i64 mut] total{0i64}
  [i64 mut] i{1i64}
  while(i <= n) {
    total = total + i * i
    i = i + 1i64
  }
  return(total)
}

[effects(task, io_out) return<int>]
main() {
  [i64 mut] total{0i64}
  [i64 mut] n{1i64}
  while(n <= 20i64) {
    [Task<i64>] part{[spawn] square_sum(n)};
    total = total + wait(part)
    n = n + 1i64
  }
  print_line(total)
  return(0i32)
}
)";
  program.exitCode = 0;
  program.stdoutText = "16170\n";
  runTaskProgram(program);
}

TEST_CASE("tasks are waited together, destructured and returned") {
  program_matrix::ProgramCase program;
  program.name = "task_multi_wait_forms";
  program.source = R"(
import /std/tuple/*

[return<i32>]
add([i32] a, [i32] b) {
  [i32 mut] total{a}
  total = total + b
  return(total)
}

[return<i64>]
widen([i32] value) {
  [i64 mut] result{0i64}
  result = convert<i64>(value) * 1000i64
  return(result)
}

[effects(task) return<i32>]
add_later([i32] a, [i32] b) {
  [Task<i32>] task{[spawn] add(a, b)};
  return(wait(task))
}

[effects(task, io_out) return<int>]
main() {
  [Task<i32>] left{[spawn] add(1i32, 2i32)};
  [Task<i32>] right{[spawn] add(3i32, 4i32)};
  [auto] both{wait(left, right)}
  print_line(both[0i32])
  print_line(both[1i32])
  [Task<i32>] first{[spawn] add(5i32, 6i32)};
  [Task<i64>] second{[spawn] widen(7i32)};
  [firstResult secondResult] wait(first, second)
  print_line(firstResult)
  print_line(secondResult)
  print_line(add_later(8i32, 9i32))
  return(0i32)
}
)";
  program.exitCode = 0;
  program.stdoutText = "3\n7\n11\n7000\n17\n";
  runTaskProgram(program);
}

TEST_CASE("a spawned call a real call cannot take yet still runs") {
  // `copy mut` and `String` parameters keep the spawned call inline on every backend.
  program_matrix::ProgramCase program;
  program.name = "task_inline_fallback";
  program.source = R"(
import /std/text/*

[return<i32>]
bump([i32 copy mut] value) {
  value = value + 1i32
  return(value)
}

[return<i32>]
measure([String] text) {
  return(text.count())
}

[effects(task, heap_alloc, io_out) return<int>]
main() {
  [i32 mut] counter{1i32}
  [Task<i32>] bumped{[spawn] bump(counter)};
  print_line(wait(bumped))
  print_line(counter)
  [String] text{stringFrom("abcd")}
  [Task<i32>] measured{[spawn] measure(text)};
  print_line(wait(measured))
  return(0i32)
}
)";
  program.exitCode = 0;
  program.stdoutText = "2\n1\n4\n";
  runTaskProgram(program);
}

TEST_SUITE_END();
