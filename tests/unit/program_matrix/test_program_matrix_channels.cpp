#include "program_matrix.h"

TEST_SUITE_BEGIN("primestruct.program_matrix.channels");

// `/std/sync` `Channel<T>` (docs/spec/stdlib-reference.md, std/sync): tasks hand values to each other through
// copies of a bounded queue. The VM runs each program on the eager default schedule and on seeds 1 to 3, where
// every lock, unlock and condition wait is a switch point; optexe runs each task on a thread. Nothing here depends
// on how the threads are scheduled or on time: every program has one output on every schedule (a total, a count
// of the items that arrived exactly once, a fault). Each matrix case builds one optexe binary (host C++
// compiler), which takes a few seconds.

namespace {

std::vector<program_matrix::ExecutionConfig> seedConfigs() {
  std::vector<program_matrix::ExecutionConfig> configs;
  for (int seed = 1; seed <= 3; ++seed) {
    program_matrix::ExecutionConfig config{
        "vm-O2-seed" + std::to_string(seed), program_matrix::Backend::Vm, 2, false};
    config.vmFlags = {"--vm-schedule-seed", std::to_string(seed)};
    configs.push_back(std::move(config));
  }
  return configs;
}

void runOn(program_matrix::ProgramCase program,
           std::vector<program_matrix::ExecutionConfig> configs) {
  for (const program_matrix::ExecutionConfig &config : configs) {
    program.onlyConfigs.push_back(config.name);
  }
  program_matrix::runProgramMatrix(program, configs);
}

void runChannelProgram(program_matrix::ProgramCase program) {
  std::vector<program_matrix::ExecutionConfig> configs = {
      {"vm-step-O0", program_matrix::Backend::VmStep, 0, false},
      {"vm-O0", program_matrix::Backend::Vm, 0, false},
      {"vm-O2", program_matrix::Backend::Vm, 2, false},
      {"optexe-O2", program_matrix::Backend::OptExe, 2, true},
  };
  for (program_matrix::ExecutionConfig &config : seedConfigs()) {
    configs.push_back(std::move(config));
  }
  runOn(std::move(program), std::move(configs));
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

std::string replaceAll(std::string text, const std::string &from, const std::string &to) {
  for (size_t at = text.find(from); at != std::string::npos; at = text.find(from, at + to.size())) {
    text.replace(at, from.size(), to);
  }
  return text;
}

const char *const SmallChannelProgram = R"(
import /std/sync/*

[effects(channel, heap_alloc, io_out), return<int>]
main() {
  jobs{Channel<i64>{[capacity] 2i32}}
  jobs.send(7i64)
  jobs.close()
  pick(jobs.receive()) {
    none {
      print_line(0i64)
    }
    some(job) {
      print_line(job)
    }
  }
  return(0i32)
}
)";

// Three producers send PER_PRODUCER five-digit labels each and three consumers take them; each consumer records
// the ids it got in one `Mutex<vector<i32>>`. Prints how many were received and how many ids were seen exactly
// once: both are the number sent on every schedule.
const char *const StringFanProgram = R"(
import /std/collections/*
import /std/sync/*
import /std/text/*

[effects(heap_alloc), return<String>]
label([i32] id) {
  [String mut] text{String{}}
  [i32 mut] rest{id}
  repeat(5i32) {
    text.pushByte(convert<u8>(48i32 + rest - (rest / 10i32) * 10i32))
    rest = rest / 10i32
  }
  return(text)
}

[return<i32>]
label_id([String] text) {
  [i32 mut] id{0i32}
  [i32 mut] at{4i32}
  while(at >= 0i32) {
    id = id * 10i32 + convert<i32>(text.byteAt(at)) - 48i32
    at = at - 1i32
  }
  return(id)
}

[effects(channel, heap_alloc), return<void>]
produce([Channel<String>] lines, [i32] first, [i32] count) {
  [i32 mut] i{0i32}
  while(i < count) {
    lines.send(label(first + i))
    i = i + 1i32
  }
}

[effects(channel, lock, heap_alloc), return<i32>]
consume([Channel<String>] lines, [Mutex<vector<i32>>] seen) {
  [i32 mut] received{0i32}
  [bool mut] open{true}
  while(open) {
    pick(lines.receive()) {
      none {
        open = false
      }
      some(line) {
        [i32] id{label_id(line)}
        seen.locked([id]([Reference<vector<i32>>] ids) {
          ids.push(id)
        })
        received = received + 1i32
      }
    }
  }
  return(received)
}

[effects(task, channel, lock, heap_alloc, io_out), return<int>]
main() {
  [i32] each{PER_PRODUCERi32}
  seen{Mutex<vector<i32>>{vector<i32>{}}}
  lines{Channel<String>{[capacity] 8i32}}
  [Task<void>] p0{[spawn] produce(lines, 0i32, each)}
  [Task<void>] p1{[spawn] produce(lines, each, each)}
  [Task<void>] p2{[spawn] produce(lines, each * 2i32, each)}
  [Task<i32>] c0{[spawn] consume(lines, seen)}
  [Task<i32>] c1{[spawn] consume(lines, seen)}
  [Task<i32>] c2{[spawn] consume(lines, seen)}
  wait(p0)
  wait(p1)
  wait(p2)
  lines.close()
  print_line(wait(c0) + wait(c1) + wait(c2))
  print_line(seen.locked([each]([Reference<vector<i32>>] ids) {
    [vector<i32> mut] counts{vector<i32>{}}
    repeat(each * 3i32) {
      counts.push(0i32)
    }
    [i32 mut] at{0i32}
    while(at < ids.count()) {
      [i32] id{ids[at]}
      counts[id] = counts[id] + 1i32
      at = at + 1i32
    }
    [i32 mut] once{0i32}
    at = 0i32
    while(at < each * 3i32) {
      if(counts[at] == 1i32) {
        once = once + 1i32
      }
      at = at + 1i32
    }
    return(once)
  }))
  return(0i32)
}
)";

} // namespace

TEST_CASE("a producer sends 100 jobs through a channel of capacity 4 and a consumer sums them") {
  program_matrix::ProgramCase program;
  program.name = "channel_sum_jobs";
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
  runChannelProgram(program);
}

TEST_CASE("three producers and three consumers move 9000 Strings and each arrives exactly once") {
  // One VM run and one optexe run of the full count; the next case runs the same program on every schedule.
  program_matrix::ProgramCase program;
  program.name = "channel_string_fan_9000";
  program.source = replaceAll(StringFanProgram, "PER_PRODUCER", "3000");
  program.exitCode = 0;
  program.stdoutText = "9000\n9000\n";
  runOn(program,
        {{"vm-O2", program_matrix::Backend::Vm, 2, false},
         {"optexe-O2", program_matrix::Backend::OptExe, 2, true}});
}

TEST_CASE("three producers and three consumers move 1500 Strings exactly once on every VM seed") {
  program_matrix::ProgramCase program;
  program.name = "channel_string_fan_seeds";
  program.source = replaceAll(StringFanProgram, "PER_PRODUCER", "500");
  program.exitCode = 0;
  program.stdoutText = "1500\n1500\n";
  std::vector<program_matrix::ExecutionConfig> configs = {
      {"vm-step-O0", program_matrix::Backend::VmStep, 0, false},
      {"vm-O0", program_matrix::Backend::Vm, 0, false},
  };
  for (program_matrix::ExecutionConfig &config : seedConfigs()) {
    configs.push_back(std::move(config));
  }
  runOn(program, std::move(configs));
}

TEST_CASE("Strings sent, received and left in a channel are all freed") {
  // The first block leaves a String in the channel, which the last handle frees; the second sends five through a
  // task and frees each where it is received. Slot-heap blocks and string-heap bytes end where they began.
  program_matrix::ProgramCase program;
  program.name = "channel_string_heap";
  program.source = R"(
import /std/sync/*
import /std/text/*

[effects(heap_alloc), return<String>]
label([i32] id) {
  [String mut] text{String{}}
  text.pushByte(convert<u8>(48i32 + id))
  return(text)
}

[effects(channel, heap_alloc), return<void>]
produce([Channel<String>] lines, [i32] count) {
  [i32 mut] i{0i32}
  while(i < count) {
    lines.send(label(i))
    i = i + 1i32
  }
}

[effects(channel, heap_alloc), return<i32>]
consume([Channel<String>] lines) {
  [i32 mut] total{0i32}
  [bool mut] open{true}
  while(open) {
    pick(lines.receive()) {
      none {
        open = false
      }
      some(line) {
        [String] owned{line}
        total = total + convert<i32>(owned.byteAt(0i32)) - 47i32
      }
    }
  }
  return(total)
}

[effects(task, channel, heap_alloc, io_out), return<int>]
main() {
  [i32] blocks{heapLiveBlocks()}
  [u64] bytes{stringHeapBytesInUse()}
  if(true) {
    lines{Channel<String>{[capacity] 2i32}}
    other{lines}
    other.send(label(1i32))
    print_line(heapLiveBlocks() - blocks)
  }
  print_line(heapLiveBlocks() - blocks)
  print_line(stringHeapBytesInUse() - bytes)
  if(true) {
    lines{Channel<String>{[capacity] 2i32}}
    [Task<void>] producer{[spawn] produce(lines, 5i32)}
    [Task<i32>] consumer{[spawn] consume(lines)}
    wait(producer)
    lines.close()
    print_line(wait(consumer))
  }
  print_line(heapLiveBlocks() - blocks)
  print_line(stringHeapBytesInUse() - bytes)
  return(0i32)
}
)";
  program.exitCode = 0;
  program.stdoutText = "2\n0\n0\n15\n0\n0\n";
  runChannelProgram(program);
}

TEST_CASE("copies of a channel share one queue, which gives what was sent in order and then none") {
  program_matrix::ProgramCase program;
  program.name = "channel_copies_order";
  program.source = R"(
import /std/sync/*

[effects(channel), return<i64>]
next([Channel<i64>] jobs) {
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
  jobs{Channel<i64>{[capacity] 3i32}}
  other{jobs}
  jobs.send(5i64)
  other.send(6i64)
  jobs.send(7i64)
  print_line(next(other))
  jobs.send(8i64)
  jobs.close()
  other.close()
  print_line(next(jobs))
  print_line(next(other))
  print_line(next(jobs))
  print_line(next(other))
  print_line(next(jobs))
  // A capacity below one is a capacity of one: the sender waits for the receiver each time.
  single{Channel<i64>{[capacity] 0i32}}
  [Task<i64>] first{[spawn] next(single)}
  single.send(41i64)
  print_line(wait(first))
  [Task<i64>] last{[spawn] next(single)}
  single.close()
  print_line(wait(last))
  return(0i32)
}
)";
  program.exitCode = 0;
  program.stdoutText = "5\n6\n7\n8\n-1\n-1\n41\n-1\n";
  runChannelProgram(program);
}

TEST_CASE("a send on a closed channel faults") {
  program_matrix::ProgramCase program;
  program.name = "channel_send_closed";
  program.source = R"(
import /std/sync/*

[effects(channel, heap_alloc, io_out), return<int>]
main() {
  jobs{Channel<i64>{[capacity] 2i32}}
  jobs.send(1i64)
  jobs.close()
  print_line("closed"utf8)
  jobs.send(2i64)
  print_line("unreachable"utf8)
  return(0i32)
}
)";
  program.exitCode = 3;
  program.stdoutText = "closed\n";
  program.vmStderr = "send on a closed channel\n";
  runChannelProgram(program);
}

TEST_CASE("a sender waiting on a full channel faults when the channel is closed") {
  // The task cannot send its second value before main closes the channel: nothing receives.
  program_matrix::ProgramCase program;
  program.name = "channel_close_wakes_sender";
  program.source = R"(
import /std/sync/*

[effects(channel), return<void>]
send_two([Channel<i64>] jobs, [Channel<i64>] sent) {
  jobs.send(1i64)
  sent.send(1i64)
  jobs.send(2i64)
}

[effects(task, channel, heap_alloc, io_out), return<int>]
main() {
  jobs{Channel<i64>{[capacity] 1i32}}
  sent{Channel<i64>{[capacity] 1i32}}
  [Task<void>] sender{[spawn] send_two(jobs, sent)}
  sent.receive()
  jobs.close()
  wait(sender)
  print_line("unreachable"utf8)
  return(0i32)
}
)";
  program.exitCode = 3;
  program.stdoutText = "";
  program.vmStderr = "send on a closed channel\n";
  runChannelProgram(program);
}

TEST_CASE("a task receiving from a channel nobody sends on is the deadlock fault") {
  program_matrix::ProgramCase program;
  program.name = "channel_receive_deadlock";
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
  runChannelProgram(program);
}

TEST_CASE("the native and Wasm backends reject a program with a channel by name") {
  const program_matrix::Outcome wasm =
      compileFor("channel_rejected_wasm", "wasm", SmallChannelProgram);
  CHECK(wasm.exitCode != 0);
  CHECK_MESSAGE(wasm.err.find("channel operations are not supported by the wasm backend") !=
                    std::string::npos,
                wasm.err);
  if (program_matrix::nativeSupported()) {
    const program_matrix::Outcome native =
        compileFor("channel_rejected_native", "native", SmallChannelProgram);
    CHECK(native.exitCode != 0);
    CHECK_MESSAGE(native.err.find("channel operations are not supported by the native backend") !=
                      std::string::npos,
                  native.err);
  }
}

TEST_SUITE_END();
