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
  // `copy mut` and `string` parameters keep the spawned call inline on every backend.
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
measure([string] text) {
  return(text.count())
}

[effects(task, heap_alloc, io_out) return<int>]
main() {
  [i32 mut] counter{1i32}
  [Task<i32>] bumped{[spawn] bump(counter)};
  print_line(wait(bumped))
  print_line(counter)
  [Task<i32>] measured{[spawn] measure("abcd")};
  print_line(wait(measured))
  return(0i32)
}
)";
  program.exitCode = 0;
  program.stdoutText = "2\n1\n4\n";
  runTaskProgram(program);
}

// A `String`, a struct or a `Function` parameter or result crosses in a heap block the spawner allocates and the wait
// frees (docs/spec/vm-design.md, Tasks). Native runs these spawned calls inline.

TEST_CASE("a task takes a String and hands a String or a struct to its waiter") {
  program_matrix::ProgramCase program;
  program.name = "task_string_values";
  program.source = R"(
import /std/text/*

[struct]
Reply() {
  [String] body{String{}}
  [i32] code{0i32}
}

[return<String> effects(heap_alloc)]
greet([String] name, [i32] times) {
  [String mut] out{String{}}
  [i32 mut] i{0i32}
  while(i < times) {
    out.append("hello ")
    out.appendString(name)
    out.append("; ")
    i = i + 1i32
  }
  return(out)
}

[return<Reply> effects(heap_alloc)]
make_reply([String] text, [i32] code) {
  [String mut] body{String{}}
  body.append("body of ")
  body.appendString(text)
  return(Reply{body, code})
}

[return<i32> effects(heap_alloc)]
text_size([String] text) {
  return(text.count())
}

[return<int> effects(task, io_out, heap_alloc)]
main() {
  [String mut] who{String{}}
  who.append("world, a name long enough to live on the string heap")
  [Task<String>] t{[spawn] greet(who, 2i32)};
  [Task<Reply>] r{[spawn] make_reply(who, 7i32)};
  [Task<i32>] n{[spawn] text_size(who)};
  [String] s{wait(t)}
  [Reply] reply{wait(r)}
  print_line(s.count())
  print_line(s.equalsText("hello world, a name long enough to live on the string heap; hello world, a name long enough to live on the string heap; "))
  print_line(reply.body.count())
  print_line(reply.code)
  print_line(wait(n))
  return(0i32)
}
)";
  program.exitCode = 0;
  program.stdoutText = "120\n1\n60\n7\n52\n";
  runTaskProgram(program);
}

TEST_CASE("String and struct values of tasks leave the heap empty") {
  // `probe` allocates, which makes its arena free the blocks its waiters freed, and then reads the heap statistics of
  // the thread it runs on. Every task of a round is waited before the next is spawned, so on optexe each runs in the
  // arena the probe tasks run in, and a result built there is freed by the waiter after that arena was released.
  program_matrix::ProgramCase program;
  program.name = "task_string_values_heap";
  program.source = R"(
import /std/text/*

[struct]
Reply() {
  [String] body{String{}}
  [i32] code{0i32}
}

[return<String> effects(heap_alloc)]
greet([String] name, [i32] times) {
  [String mut] out{String{}}
  [i32 mut] i{0i32}
  while(i < times) {
    out.append("hello ")
    out.appendString(name)
    out.append("; ")
    i = i + 1i32
  }
  return(out)
}

[return<Reply> effects(heap_alloc)]
make_reply([String] text, [i32] code) {
  [String mut] body{String{}}
  body.append("body of ")
  body.appendString(text)
  return(Reply{body, code})
}

[return<i32> effects(heap_alloc)]
reply_size([Reply] reply, [String] extra) {
  return(reply.body.count() + reply.code + extra.count())
}

[return<i32> effects(heap_alloc)]
text_size([String] text) {
  return(text.count())
}

[return<String> effects(task, heap_alloc)]
relay([String] name) {
  [Task<String>] t{[spawn] greet(name, 1i32)};
  [String] out{wait(t)}
  return(out)
}

[return<i32> effects(task, heap_alloc)]
round() {
  [String mut] who{String{}}
  who.append("world, a name long enough to live on the string heap")
  [i32 mut] total{0i32}
  [Task<String>] t{[spawn] greet(who, 2i32)};
  [String] s{wait(t)}
  total = total + s.count()
  [Task<Reply>] r{[spawn] make_reply(who, 7i32)};
  [Reply] reply{wait(r)}
  total = total + reply.body.count() + reply.code
  [Task<i32>] n{[spawn] text_size(who)};
  total = total + wait(n)
  [Task<i32>] temp{[spawn] text_size(stringFrom("a temporary argument that is long enough for the heap"))};
  total = total + wait(temp)
  [Task<Reply>] field{[spawn] make_reply(who, 9i32)};
  total = total + wait(field).code
  [Task<String>] passed{[spawn] greet(who, 1i32)};
  total = total + text_size(wait(passed))
  [Task<String>] dropped{[spawn] greet(who, 3i32)};
  wait(dropped)
  [String mut] assigned{String{}}
  assigned.append("an old value that is long enough to live on the heap")
  [Task<String>] next{[spawn] greet(who, 1i32)};
  assigned = wait(next)
  total = total + assigned.count()
  [String] relayed{relay(who)}
  total = total + relayed.count()
  [Task<i32>] both{[spawn] reply_size(reply, s)};
  total = total + wait(both)
  [Task<String>] a{[spawn] greet(who, 1i32)};
  [Task<Reply>] b{[spawn] make_reply(s, 3i32)};
  [String] left{wait(a)}
  [Reply] right{wait(b)}
  total = total + left.count() + right.body.count() + right.code
  return(total)
}

[return<i32> effects(heap_alloc)]
probe() {
  if(true) {
    [String mut] scratch{String{}}
    scratch.append("an allocation that makes this arena free what waiters freed")
  }
  return(heapLiveBlocks() * 1000i32 + convert<i32>(stringHeapBytesInUse()))
}

[return<int> effects(task, io_out, heap_alloc)]
main() {
  [i32] first{round()}
  [Task<i32>] probeBefore{[spawn] probe()};
  [i32] arenaBefore{wait(probeBefore)}
  [i32] mainBefore{probe()}
  [i32 mut] sum{0i32}
  repeat(40i32) {
    sum = sum + round()
  }
  [Task<i32>] probeAfter{[spawn] probe()};
  [i32] arenaAfter{wait(probeAfter)}
  [i32] mainAfter{probe()}
  print_line(first)
  print_line(sum - first * 40i32)
  print_line(arenaAfter - arenaBefore)
  print_line(mainAfter - mainBefore)
  return(0i32)
}
)";
  program.exitCode = 0;
  program.stdoutText = "859\n0\n0\n0\n";
  // The native backend has no heap statistics.
  program.skipConfigs = {"native-O0", "native-O2"};
  runTaskProgram(program);
}

TEST_CASE("a waited String, struct or i32 result is returned directly") {
  program_matrix::ProgramCase program;
  program.name = "task_return_wait";
  program.source = R"(
import /std/text/*

[struct]
Reply() {
  [String] body{String{}}
  [i32] code{0i32}
}

[return<String> effects(heap_alloc)]
greet([String] name) {
  [String mut] out{String{}}
  out.append("hello ")
  out.appendString(name)
  return(out)
}

[return<Reply> effects(heap_alloc)]
make_reply([String] text, [i32] code) {
  [String mut] body{String{}}
  body.append("body of ")
  body.appendString(text)
  return(Reply{body, code})
}

[return<i32>]
twice([i32] value) {
  [i32 mut] result{value}
  result = result + value
  return(result)
}

[return<String> effects(task, heap_alloc)]
relay([String] name) {
  [Task<String>] t{[spawn] greet(name)};
  return(wait(t))
}

[return<Reply> effects(task, heap_alloc)]
relay_reply([String] name) {
  [Task<Reply>] t{[spawn] make_reply(name, 7i32)};
  return(wait(t))
}

[return<i32> effects(task)]
relay_twice([i32] value) {
  [Task<i32>] t{[spawn] twice(value)};
  return(wait(t))
}

[return<int> effects(task, io_out, heap_alloc)]
main() {
  [i32] blocks{heapLiveBlocks()}
  [u64] bytes{stringHeapBytesInUse()}
  if(true) {
    [String mut] who{String{}}
    who.append("world, a name long enough to live on the string heap")
    [String] s{relay(who)}
    [Reply] reply{relay_reply(who)}
    print_line(s.count())
    print_line(reply.body.count())
    print_line(reply.code)
    print_line(relay_twice(21i32))
  }
  print_line(heapLiveBlocks() - blocks)
  print_line(stringHeapBytesInUse() - bytes)
  return(0i32)
}
)";
  program.exitCode = 0;
  program.stdoutText = "58\n60\n7\n42\n0\n0\n";
  // The native backend has no heap statistics.
  program.skipConfigs = {"native-O0", "native-O2"};
  runTaskProgram(program);
}

TEST_CASE("a destructured multi-wait gives each binding its own result type") {
  program_matrix::ProgramCase program;
  program.name = "task_destructured_wait_types";
  program.source = R"(
import /std/text/*
import /std/tuple/*

[struct]
Reply() {
  [String] body{String{}}
  [i32] code{0i32}
}

[return<String> effects(heap_alloc)]
greet([String] name) {
  [String mut] out{String{}}
  out.append("hello ")
  out.appendString(name)
  return(out)
}

[return<Reply> effects(heap_alloc)]
make_reply([String] text, [i32] code) {
  [String mut] body{String{}}
  body.append("body of ")
  body.appendString(text)
  return(Reply{body, code})
}

[return<i64>]
widen([i32] value) {
  [i64 mut] result{0i64}
  result = convert<i64>(value) * 1000i64
  return(result)
}

// Destructuring is a statement of the definition's own body (not of a nested block, TODO-5894).
[return<void> effects(task, io_out, heap_alloc)]
round() {
  [String mut] who{String{}}
  who.append("world, a name long enough to live on the string heap")
  [Task<String>] x{[spawn] greet(who)};
  [Task<Reply>] y{[spawn] make_reply(who, 7i32)};
  [a b] wait(x, y)
  print_line(a.count())
  print_line(b.code)
  [Task<i64>] w{[spawn] widen(3i32)};
  [Task<String>] x2{[spawn] greet(who)};
  [Task<Reply>] y2{[spawn] make_reply(who, 9i32)};
  [n line reply] wait(w, x2, y2)
  print_line(n)
  print_line(line.count())
  print_line(reply.code)
  print_line(reply.body.count())
  [auto] pair{make_tuple(widen(2i32), make_reply(who, 5i32))}
  [wide made] pair
  print_line(wide)
  print_line(made.code)
}

[return<int> effects(task, io_out, heap_alloc)]
main() {
  [i32] blocks{heapLiveBlocks()}
  [u64] bytes{stringHeapBytesInUse()}
  round()
  print_line(heapLiveBlocks() - blocks)
  print_line(stringHeapBytesInUse() - bytes)
  return(0i32)
}
)";
  program.exitCode = 0;
  program.stdoutText = "58\n7\n3000\n58\n9\n60\n2000\n5\n0\n0\n";
  // The native backend has no heap statistics.
  program.skipConfigs = {"native-O0", "native-O2"};
  runTaskProgram(program);
}

TEST_CASE("struct results of tasks are waited together and in a loop") {
  // Two tasks of a pair run at once here, so only the spawner's heap is measured.
  program_matrix::ProgramCase program;
  program.name = "task_struct_multi_wait";
  program.source = R"(
import /std/text/*
import /std/tuple/*

[struct]
Reply() {
  [String] body{String{}}
  [i32] code{0i32}
}

[return<String> effects(heap_alloc)]
greet([String] name, [i32] times) {
  [String mut] out{String{}}
  [i32 mut] i{0i32}
  while(i < times) {
    out.append("hello ")
    out.appendString(name)
    out.append("; ")
    i = i + 1i32
  }
  return(out)
}

[return<Reply> effects(heap_alloc)]
make_reply([String] text, [i32] code) {
  [String mut] body{String{}}
  body.append("body of ")
  body.appendString(text)
  return(Reply{body, code})
}

[return<i32> effects(task, heap_alloc)]
round() {
  [String mut] who{String{}}
  who.append("world, a name long enough to live on the string heap")
  [i32 mut] total{0i32}
  [Task<String>] a{[spawn] greet(who, 1i32)};
  [Task<String>] b{[spawn] greet(who, 3i32)};
  [left right] wait(a, b)
  total = total + left.count() + right.count()
  [Task<String>] c{[spawn] greet(who, 2i32)};
  [Task<Reply>] d{[spawn] make_reply(who, 4i32)};
  [auto] both{wait(c, d)}
  total = total + both[0i32].count() + both[1i32].code
  [i32 mut] i{0i32}
  while(i < 3i32) {
    [Task<String>] looped{[spawn] greet(who, i)};
    [String] text{wait(looped)}
    total = total + text.count()
    i = i + 1i32
  }
  return(total)
}

[return<i32> effects(heap_alloc)]
probe() {
  if(true) {
    [String mut] scratch{String{}}
    scratch.append("an allocation that makes this arena free what waiters freed")
  }
  return(heapLiveBlocks() * 1000i32 + convert<i32>(stringHeapBytesInUse()))
}

[return<int> effects(task, io_out, heap_alloc)]
main() {
  [i32] first{round()}
  [i32] mainBefore{probe()}
  [i32 mut] sum{0i32}
  repeat(40i32) {
    sum = sum + round()
  }
  [i32] mainAfter{probe()}
  print_line(first)
  print_line(sum - first * 40i32)
  print_line(mainAfter - mainBefore)
  return(0i32)
}
)";
  program.exitCode = 0;
  program.stdoutText = "544\n0\n0\n";
  // The native backend has no heap statistics.
  program.skipConfigs = {"native-O0", "native-O2"};
  runTaskProgram(program);
}

TEST_CASE("a task takes a closure it borrows or owns") {
  program_matrix::ProgramCase program;
  program.name = "task_closure_arguments";
  program.source = R"(
import /std/text/*

[return<i32>]
apply([Function<i32, i32>] step, [i32] value) {
  return(step(value))
}

[return<i32>]
apply_owned([Function<i32, i32> move] step, [i32] value) {
  return(step(value))
}

[return<i32> effects(heap_alloc)]
probe() {
  if(true) {
    [String mut] scratch{String{}}
    scratch.append("an allocation that makes this arena free what waiters freed")
  }
  return(heapLiveBlocks() * 1000i32 + convert<i32>(stringHeapBytesInUse()))
}

[return<i32> effects(task, heap_alloc)]
round() {
  [i32] base{40i32}
  [String mut] label{String{}}
  label.append("a captured label that is long enough to live on the heap")
  [Function<i32, i32>] add{[=]([i32] x) { return(x + base + label.count()) }}
  [Task<i32>] a{[spawn] apply(add, 2i32)};
  [i32] first{wait(a)}
  [Task<i32>] b{[spawn] apply_owned(add, 3i32)};
  [i32] second{wait(b)}
  [Task<i32>] c{[spawn] apply_owned([=]([i32] x) { return(x * base + label.count()) }, 3i32)};
  return(first + second + wait(c))
}

[return<int> effects(task, io_out, heap_alloc)]
main() {
  [i32] first{round()}
  [Task<i32>] probeBefore{[spawn] probe()};
  [i32] arenaBefore{wait(probeBefore)}
  [i32] mainBefore{probe()}
  [i32 mut] sum{0i32}
  repeat(20i32) {
    sum = sum + round()
  }
  [Task<i32>] probeAfter{[spawn] probe()};
  [i32] arenaAfter{wait(probeAfter)}
  [i32] mainAfter{probe()}
  print_line(first)
  print_line(sum - first * 20i32)
  print_line(arenaAfter - arenaBefore)
  print_line(mainAfter - mainBefore)
  return(0i32)
}
)";
  program.exitCode = 0;
  program.stdoutText = "373\n0\n0\n0\n";
  // The native backend has no heap statistics.
  program.skipConfigs = {"native-O0", "native-O2"};
  runTaskProgram(program);
}

TEST_SUITE_END();
