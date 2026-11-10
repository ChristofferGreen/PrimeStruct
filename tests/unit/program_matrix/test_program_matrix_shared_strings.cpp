#include "program_matrix.h"

TEST_SUITE_BEGIN("primestruct.program_matrix.shared_strings");

// `SharedString` owners on several tasks (docs/spec/strings-and-views.md): the owner count is an atomic cell, so a
// copy made on one task and a destroy on another both count, and the last owner frees the text wherever it runs.
// The VM runs the programs on the eager default schedule and on seeds 1 to 8, where every atomic operation is a
// switch point; optexe runs each task on a thread. Each program prints the heap blocks and string heap bytes it
// left behind (0 and 0). Each matrix case builds one optexe binary (host C++ compiler), which takes a few seconds.

namespace {

void runSharedStringProgram(program_matrix::ProgramCase program) {
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

} // namespace

TEST_CASE("four tasks copy and destroy one SharedString and leave one owner") {
  program_matrix::ProgramCase program;
  program.name = "shared_string_four_tasks";
  program.source = R"(
import /std/text/*
import /std/text/view/*
import /std/text/shared/*

[return<i32>]
copy_many([SharedString] text, [i32] rounds) {
  [i32 mut] total{0i32}
  [i32 mut] i{0i32}
  while(i < rounds) {
    [SharedString] copy{text}
    total = total + copy.count()
    i = i + 1i32
  }
  return(total)
}

// The tasks and their argument copies end with this call, so the caller is the only owner afterwards.
[effects(heap_alloc, task), return<i32>]
copy_on_four_tasks([SharedString] text) {
  [Task<i32>] a{[spawn] copy_many(text, 100000i32)}
  [Task<i32>] b{[spawn] copy_many(text, 100000i32)}
  [Task<i32>] c{[spawn] copy_many(text, 100000i32)}
  [Task<i32>] d{[spawn] copy_many(text, 100000i32)}
  return(wait(a) + wait(b) + wait(c) + wait(d))
}

[effects(heap_alloc, io_out, task), return<int>]
main() {
  [i32] blocks{heapLiveBlocks()}
  [u64] bytes{stringHeapBytesInUse()}
  if(true) {
    [String] owner{stringFrom("hello")}
    [SharedString] kept{sharedFrom(owner.view())}
    print_line(copy_on_four_tasks(kept))
    print_line(kept.owners())
  }
  print_line(heapLiveBlocks() - blocks)
  print_line(stringHeapBytesInUse() - bytes)
  return(0i32)
}
)";
  program.exitCode = 0;
  program.stdoutText = "2000000\n1\n0\n0\n";
  runSharedStringProgram(program);
}

TEST_CASE("a SharedString is a spawn argument and crosses a Channel") {
  program_matrix::ProgramCase program;
  program.name = "shared_string_spawn_and_channel";
  program.source = R"(
import /std/sync/*
import /std/text/*
import /std/text/view/*
import /std/text/shared/*

[return<i32>]
measure([SharedString] text) {
  return(text.count())
}

[effects(channel, heap_alloc), return<void>]
produce([Channel<SharedString>] texts, [SharedString] text, [i32] rounds) {
  [i32 mut] i{0i32}
  while(i < rounds) {
    [SharedString] item{text}
    texts.send(item)
    i = i + 1i32
  }
  texts.close()
}

[effects(channel, heap_alloc, task), return<i32>]
through_a_channel([SharedString] text) {
  texts{Channel<SharedString>{[capacity] 2i32}}
  [Task<void>] producer{[spawn] produce(texts, text, 50i32)}
  [i32 mut] received{0i32}
  [bool mut] open{true}
  while(open) {
    pick(texts.receive()) {
      none {
        open = false
      }
      some(item) {
        received = received + measure(item)
      }
    }
  }
  wait(producer)
  return(received)
}

[effects(heap_alloc, task), return<i32>]
as_a_spawn_argument([SharedString] text) {
  [Task<i32>] task{[spawn] measure(text)}
  return(wait(task))
}

[effects(channel, heap_alloc, io_out, task), return<int>]
main() {
  [i32] blocks{heapLiveBlocks()}
  [u64] bytes{stringHeapBytesInUse()}
  if(true) {
    [String] owner{stringFrom("hello")}
    [SharedString] kept{sharedFrom(owner.view())}
    print_line(as_a_spawn_argument(kept))
    print_line(kept.owners())
    print_line(through_a_channel(kept))
    print_line(kept.owners())
  }
  print_line(heapLiveBlocks() - blocks)
  print_line(stringHeapBytesInUse() - bytes)
  return(0i32)
}
)";
  program.exitCode = 0;
  program.stdoutText = "5\n1\n250\n1\n0\n0\n";
  runSharedStringProgram(program);
}

TEST_CASE("the last owner of a SharedString frees it on another task") {
  program_matrix::ProgramCase program;
  program.name = "shared_string_freed_elsewhere";
  // On optexe a block freed by another thread waits on its owner's remote-free list until the owner next
  // allocates (docs/MultithreadingPrototype.md, Heaps), so main allocates once more (`drain`) after the consumer
  // has finished and before it reads the heap counts.
  program.source = R"(
import /std/sync/*
import /std/text/*
import /std/text/view/*
import /std/text/shared/*

[return<i32>]
measure([SharedString] text) {
  return(text.count())
}

[effects(channel, heap_alloc), return<i32>]
consume([Channel<SharedString>] texts) {
  [i32 mut] received{0i32}
  [bool mut] open{true}
  while(open) {
    pick(texts.receive()) {
      none {
        open = false
      }
      some(item) {
        received = received + measure(item)
      }
    }
  }
  return(received)
}

[effects(channel, heap_alloc, task), return<i32>]
made_here_freed_there([String] owner) {
  texts{Channel<SharedString>{[capacity] 2i32}}
  [Task<i32>] consumer{[spawn] consume(texts)}
  [i32 mut] i{0i32}
  while(i < 50i32) {
    [SharedString] made{sharedFrom(owner.view())}
    texts.send(made)
    i = i + 1i32
  }
  texts.close()
  return(wait(consumer))
}

[effects(channel, heap_alloc, io_out, task), return<int>]
main() {
  [i32] blocks{heapLiveBlocks()}
  [u64] bytes{stringHeapBytesInUse()}
  if(true) {
    [String] owner{stringFrom("hello")}
    print_line(made_here_freed_there(owner))
    [SharedString] drain{sharedFrom(owner.view())}
    print_line(drain.owners())
  }
  print_line(heapLiveBlocks() - blocks)
  print_line(stringHeapBytesInUse() - bytes)
  return(0i32)
}
)";
  program.exitCode = 0;
  program.stdoutText = "250\n1\n0\n0\n";
  runSharedStringProgram(program);
}

TEST_SUITE_END();
