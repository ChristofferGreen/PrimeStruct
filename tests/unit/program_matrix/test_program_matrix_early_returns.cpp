#include "program_matrix.h"

#include <string>

TEST_SUITE_BEGIN("primestruct.program_matrix.early_returns");

// A body with several `return` statements shares one cleanup chain between its exits instead of destroying the
// live locals at every `return` (docs/spec/vm-design.md, Shared exit paths). Every exit still destroys exactly the
// locals live at it, last declared first, once. `Noisy` prints its id when destroyed, so the order is the output;
// the heap statistics are read around the body and printed last, and a local destroyed twice or never shows there
// (`heap block freed twice`, or a non-zero difference).

namespace {

const std::string EarlyReturnPrelude = R"(
import /std/collections/*
import /std/text/*
import /std/maybe/*
import /std/result/*

[struct]
Noisy() {
  [i32 mut] id{0i32}
  [i64 mut] pad{0i64}

  [public effects(io_out)]
  Destroy() {
    print_line(this.id)
  }
}

[effects(io_out) return<void>]
take([Noisy move] value) {
}

[effects(heap_alloc), return<String>]
label([i32] id) {
  [String mut] text{String{}}
  text.pushByte(convert<u8>(48i32 + id))
  return(text)
}

[return<i32>]
first_byte([String] text) {
  return(convert<i32>(text.byteAt(0i32)))
}
)";

// The programs of this file that run on Wasm like the VM (`PRIMESTRUCT_MATRIX_WASM_RUN=1`).
const std::vector<std::string> WasmPrograms = {"early_return_live_sets",
                                               "early_return_in_loop",
                                               "early_return_nested_scopes",
                                               "early_return_moved_local",
                                               "early_return_void_body",
                                               "early_return_unchanged_shapes"};

void runEarlyReturnProgram(const std::string &name,
                           const std::string &definitions,
                           const std::string &body,
                           const std::string &stdoutText) {
  program_matrix::ProgramCase program;
  program.name = name;
  program.source = EarlyReturnPrelude + definitions + R"(
[effects(heap_alloc, io_out), return<int>]
main() {
  [i32] blocks{heapLiveBlocks()}
  [u64] bytes{stringHeapBytesInUse()}
  if(true) {
)" + body + R"(
  }
  print_line(heapLiveBlocks() - blocks)
  print_line(stringHeapBytesInUse() - bytes)
  return(0i32)
}
)";
  program.exitCode = 0;
  program.stdoutText = stdoutText + "0\n0\n";
  // The heap statistics exist on the VM, optexe and arm64 native.
  program.onlyConfigs = {"vm-step-O0", "vm-O0", "vm-O2", "optexe-O2"};
  if (program_matrix::nativeRunsStringHeap()) {
    program.onlyConfigs.push_back("native-O0");
    program.onlyConfigs.push_back("native-O2");
  }
  program_matrix::listWasmIf(program, WasmPrograms);
  program_matrix::runProgramMatrix(program);
}

} // namespace

TEST_CASE("each exit destroys the locals declared before it, last declared first") {
  runEarlyReturnProgram("early_return_live_sets",
                        R"(
[effects(io_out) return<i32>]
exits([i32] which) {
  [Noisy] a{Noisy{1i32, 0i64}}
  if(which == 0i32) {
    return(10i32)
  }
  [Noisy] b{Noisy{2i32, 0i64}}
  if(which == 1i32) {
    return(11i32)
  }
  [Noisy] c{Noisy{3i32, 0i64}}
  if(which == 2i32) {
    return(12i32)
  }
  return(13i32)
}
)",
                        R"(
    print_line(exits(0i32))
    print_line(exits(1i32))
    print_line(exits(2i32))
    print_line(exits(3i32))
)",
                        "1\n10\n2\n1\n11\n3\n2\n1\n12\n3\n2\n1\n13\n");
}

TEST_CASE("a return from inside a loop destroys the loop body's locals and the enclosing ones") {
  runEarlyReturnProgram(
      "early_return_in_loop",
      R"(
[effects(io_out) return<i32>]
find([i32] limit) {
  [Noisy] outer{Noisy{1i32, 0i64}}
  for([i32 mut] i{0i32}, i < 3i32, i++) {
    [Noisy] each{Noisy{2i32, 0i64}}
    if(i == limit) {
      return(i)
    }
  }
  return(99i32)
}
)",
      R"(
    print_line(find(1i32))
    print_line(find(7i32))
    print_line(find(0i32))
)",
      // find(1): iteration 0 ends (2), iteration 1 returns (2, 1); find(7): three iterations
      // (2 2 2), then the fall-out return (1); find(0): the first iteration returns (2, 1).
      "2\n2\n1\n1\n2\n2\n2\n1\n99\n2\n1\n0\n");
}

TEST_CASE("nested scopes share the chain of the scopes they have in common") {
  runEarlyReturnProgram("early_return_nested_scopes",
                        R"(
[effects(io_out) return<i32>]
nested([i32] depth) {
  [Noisy] a{Noisy{1i32, 0i64}}
  if(depth > 0i32) {
    [Noisy] b{Noisy{2i32, 0i64}}
    if(depth > 1i32) {
      [Noisy] c{Noisy{3i32, 0i64}}
      if(depth > 2i32) {
        return(30i32)
      }
      return(20i32)
    }
    return(10i32)
  }
  [Noisy] d{Noisy{4i32, 0i64}}
  return(0i32)
}
)",
                        R"(
    print_line(nested(3i32))
    print_line(nested(2i32))
    print_line(nested(1i32))
    print_line(nested(0i32))
)",
                        "3\n2\n1\n30\n3\n2\n1\n20\n2\n1\n10\n4\n1\n0\n");
}

TEST_CASE("a local moved out before an exit is not destroyed there") {
  runEarlyReturnProgram(
      "early_return_moved_local",
      R"(
[effects(io_out) return<i32>]
handoff([i32] mode) {
  [Noisy] a{Noisy{1i32, 0i64}}
  [Noisy] b{Noisy{2i32, 0i64}}
  if(mode == 0i32) {
    take(move(b))
    return(5i32)
  }
  if(mode == 1i32) {
    return(6i32)
  }
  take(move(a))
  return(7i32)
}
)",
      R"(
    print_line(handoff(0i32))
    print_line(handoff(1i32))
    print_line(handoff(2i32))
)",
      // mode 0: take destroys b (2), the exit destroys a (1); mode 1: b, a; mode 2: take
      // destroys a (1), the exit destroys b (2).
      "2\n1\n5\n2\n1\n6\n1\n2\n7\n");
}

TEST_CASE("exits of a void body share the cleanup too") {
  runEarlyReturnProgram("early_return_void_body",
                        R"(
[effects(io_out) return<void>]
leave([i32] which) {
  [Noisy] a{Noisy{1i32, 0i64}}
  if(which == 0i32) {
    return()
  }
  [Noisy] b{Noisy{2i32, 0i64}}
  if(which == 1i32) {
    return()
  }
  print_line(100i32)
}
)",
                        R"(
    leave(0i32)
    leave(1i32)
    leave(2i32)
)",
                        "1\n2\n1\n100\n2\n1\n");
}

TEST_CASE(
    "early returns of heap-owning locals free each once and return a local that was handed over") {
  runEarlyReturnProgram("early_return_strings",
                        R"(
[effects(heap_alloc), return<String>]
pick_text([i32] which) {
  [String] a{label(1i32)}
  if(which == 0i32) {
    return(label(5i32))
  }
  [String] b{label(2i32)}
  if(which == 1i32) {
    return(b)
  }
  [vector<String> mut] names{vector<String>{}}
  names.push(label(3i32))
  names.push(label(4i32))
  if(which == 2i32) {
    return(a)
  }
  return(label(6i32))
}
)",
                        R"(
    [String] zero{pick_text(0i32)}
    [String] one{pick_text(1i32)}
    [String] two{pick_text(2i32)}
    [String] three{pick_text(3i32)}
    print_line(first_byte(zero))
    print_line(first_byte(one))
    print_line(first_byte(two))
    print_line(first_byte(three))
)",
                        "53\n50\n49\n54\n");
}

TEST_CASE("a Maybe and a Result returned early destroy the locals live at that exit") {
  runEarlyReturnProgram("early_return_sums",
                        R"(
[effects(io_out, heap_alloc), return<Maybe<String>>]
lookup([i32] which) {
  [Noisy] a{Noisy{1i32, 0i64}}
  if(which < 0i32) {
    return(none<String>())
  }
  [Noisy] b{Noisy{2i32, 0i64}}
  if(which == 0i32) {
    return(some<String>(label(7i32)))
  }
  [String] kept{label(8i32)}
  return(some<String>(kept))
}

[effects(io_out), return<Result<i32, i32>>]
checked([i32] mode) {
  [Noisy] a{Noisy{1i32, 0i64}}
  if(mode == 0i32) {
    return(/std/result/error<i32, i32>(40i32))
  }
  [Noisy] b{Noisy{2i32, 0i64}}
  if(mode == 1i32) {
    return(Result.ok(41i32))
  }
  return(Result.ok(42i32))
}

[return<i32>]
show_result([Result<i32, i32>] r) {
  return(pick(r) {
    ok(v) { v }
    error(e) { 100i32 + e }
  })
}

[effects(io_out)]
show_text([Maybe<String>] value) {
  pick(value) {
    some(text) {
      print_line(first_byte(text))
    }
    none {
      print_line(-1i32)
    }
  }
}
)",
                        R"(
    show_text(lookup(-1i32))
    show_text(lookup(0i32))
    show_text(lookup(1i32))
    print_line(show_result(checked(0i32)))
    print_line(show_result(checked(1i32)))
    print_line(show_result(checked(2i32)))
)",
                        "1\n-1\n2\n1\n55\n2\n1\n56\n1\n140\n2\n1\n41\n2\n1\n42\n");
}

TEST_CASE("a body with one exit and a body with two exits and no locals keep running as before") {
  runEarlyReturnProgram("early_return_unchanged_shapes",
                        R"(
[effects(io_out) return<i32>]
single([i32] which) {
  [Noisy] a{Noisy{1i32, 0i64}}
  [Noisy] b{Noisy{2i32, 0i64}}
  print_line(which)
  return(which + 1i32)
}

[return<i32>]
fib([i32] n) {
  if(n < 2i32) {
    return(n)
  }
  return(fib(n - 1i32) + fib(n - 2i32))
}
)",
                        R"(
    print_line(single(4i32))
    print_line(fib(10i32))
)",
                        "4\n2\n1\n5\n55\n");
}

TEST_CASE("pick arms are scopes: an arm's locals are destroyed where the arm ends, on every config") {
  // The locals of an arm that did not run are not live at the exits of the arms after it (their drop flags are
  // never set, and on the native backend a frame is not zeroed). No heap statistics, so native runs it too.
  program_matrix::ProgramCase program;
  program.name = "early_return_pick_arm_scopes";
  program.extraConfigs = {"wasm-wasi"};
  program.source = R"(
import /std/maybe/*

[struct]
Noisy() {
  [i32 mut] id{0i32}
  [i64 mut] pad{0i64}

  [public effects(io_out)]
  Destroy() {
    print_line(this.id)
  }
}

[effects(io_out) return<i32>]
arms([Maybe<i32>] m, [i32] extra) {
  [Noisy] outer{Noisy{1i32, 0i64}}
  pick(m) {
    none {
      [Noisy] inner{Noisy{2i32, 0i64}}
      if(extra > 0i32) {
        return(20i32)
      }
      print_line(100i32)
    }
    some(v) {
      [Noisy] other{Noisy{3i32, 0i64}}
      return(v)
    }
  }
  return(0i32)
}

[effects(io_out) return<int>]
main() {
  [Maybe<i32>] e{}
  [Maybe<i32>] s{[some] 4i32}
  print_line(arms(e, 0i32))
  print_line(arms(e, 1i32))
  print_line(arms(s, 0i32))
  return(0i32)
}
)";
  program.exitCode = 0;
  // arms(e, 0): the arm prints 100 and destroys `inner` (2) where it ends, then the final exit destroys `outer`
  // (1). arms(e, 1): the return in the arm destroys `inner` and `outer`. arms(s, 0): `other` (3) and `outer`.
  program.stdoutText = "100\n2\n1\n0\n2\n1\n20\n3\n1\n4\n";
  program_matrix::runProgramMatrix(program);
}

TEST_CASE("exits with different live sets run the same on every fast config, native included") {
  // No heap statistics here (they exist on the VM and optexe only), so the default configs run it: the VM kernels
  // and the native backend. The loop exit, the nested exits and the final exit share chains.
  program_matrix::ProgramCase program;
  program.name = "early_return_all_configs";
  program.extraConfigs = {"wasm-wasi"};
  program.source = R"(
[struct]
Noisy() {
  [i32 mut] id{0i32}
  [i64 mut] pad{0i64}

  [public effects(io_out)]
  Destroy() {
    print_line(this.id)
  }
}

[effects(io_out) return<i32>]
walk([i32] limit, [i32] depth) {
  [Noisy] a{Noisy{1i32, 0i64}}
  for([i32 mut] i{0i32}, i < 3i32, i++) {
    [Noisy] each{Noisy{2i32, 0i64}}
    if(i == limit) {
      return(i)
    }
  }
  if(depth > 0i32) {
    [Noisy] b{Noisy{3i32, 0i64}}
    if(depth > 1i32) {
      return(20i32)
    }
    return(10i32)
  }
  return(0i32)
}

[effects(io_out) return<int>]
main() {
  print_line(walk(1i32, 0i32))
  print_line(walk(9i32, 2i32))
  print_line(walk(9i32, 1i32))
  print_line(walk(9i32, 0i32))
  return(0i32)
}
)";
  program.exitCode = 0;
  // walk(1, 0): iteration 0 ends (2), iteration 1 returns (2, 1). walk(9, 2): three iterations (2 2 2), then the
  // exit in the nested block (3, 1). walk(9, 1): the same with the other nested return. walk(9, 0): the last exit.
  program.stdoutText = "2\n2\n1\n1\n2\n2\n2\n3\n1\n20\n2\n2\n2\n3\n1\n10\n2\n2\n2\n1\n0\n";
  program_matrix::runProgramMatrix(program);
}
