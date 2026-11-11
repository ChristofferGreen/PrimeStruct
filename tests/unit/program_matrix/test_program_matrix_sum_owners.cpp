#include "program_matrix.h"

#include <string>

TEST_SUITE_BEGIN("primestruct.program_matrix.sum_owners");

// Who owns the payload of a sum (docs/spec/value-lifecycle.md, Ownership) when the sum is a parameter, by its
// mode, or the target of an assignment. Each program reads the heap statistics before and after its body and
// prints the differences last; a payload destroyed twice ends it with `string heap block freed twice`.

namespace {

// `label(id)` is one byte, the digit `id`; `get(id)` is `none` below zero; `show(value)` prints the payload's byte.
const std::string SumOwnerPrelude = R"(
import /std/text/*
import /std/maybe/*

[effects(heap_alloc), return<String>]
label([i32] id) {
  [String mut] text{String{}}
  text.pushByte(convert<u8>(48i32 + id))
  return(text)
}

[effects(heap_alloc), return<Maybe<String>>]
get([i32] id) {
  if(id < 0i32) {
    return(none<String>())
  }
  return(some<String>(label(id)))
}

[return<i32>]
first_byte([String] text) {
  return(convert<i32>(text.byteAt(0i32)))
}

[effects(io_out)]
show([Maybe<String>] value) {
  pick(value) {
    some(text) {
      print_line(first_byte(text))
    }
    none {
      print_line(-1i32)
    }
  }
}
)";

void runSumOwnerProgram(const std::string &name,
                        const std::string &definitions,
                        const std::string &body,
                        const std::string &stdoutText) {
  program_matrix::ProgramCase program;
  program.name = name;
  program.source = SumOwnerPrelude + definitions + R"(
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
  // The heap statistics exist on the VM and optexe only.
  program.onlyConfigs = {"vm-step-O0", "vm-O0", "vm-O2", "optexe-O2"};
  program_matrix::runProgramMatrix(program);
}

} // namespace

TEST_CASE("a copy sum parameter given a local has a payload of its own") {
  // The callee replaces its copy; the caller's local still reads the payload it had.
  runSumOwnerProgram("sum_copy_parameter_of_local",
                     R"(
[effects(heap_alloc, io_out)]
take_copy([Maybe<String> copy mut] value) {
  show(value)
  value = some<String>(label(9i32))
  show(value)
}
)",
                     R"(
    [Maybe<String>] named{get(1i32)}
    take_copy(named)
    show(named)
    [String] after{label(5i32)}
    show(named)
    take_copy(get(2i32))
    [Maybe<String>] empty{get(-1i32)}
    take_copy(empty)
    show(empty)
)",
                     "49\n57\n49\n49\n50\n57\n-1\n57\n-1\n");
}

TEST_CASE("assigning to a mut sum parameter or a Reference to a sum destroys the payload it held") {
  runSumOwnerProgram("sum_borrowed_assignment",
                     R"(
[effects(heap_alloc)]
replace([Maybe<String> mut] value, [i32] id) {
  value = some<String>(label(id))
}

[effects(heap_alloc)]
replace_twice([Maybe<String> mut] value, [i32] id) {
  value = some<String>(label(id))
  value = get(id + 1i32)
}

[effects(heap_alloc)]
clear([Maybe<String> mut] value) {
  value = none<String>()
}

[effects(heap_alloc)]
replace_ref([Reference<Maybe<String>> mut] value, [i32] id) {
  value = some<String>(label(id))
}
)",
                     R"(
    [Maybe<String> mut] target{get(1i32)}
    replace(target, 2i32)
    show(target)
    replace(target, 3i32)
    replace(target, 4i32)
    show(target)
    replace_twice(target, 5i32)
    show(target)
    clear(target)
    show(target)
    replace(target, 7i32)
    show(target)
    replace_ref(location(target), 8i32)
    show(target)
    replace_ref(location(target), 9i32)
    show(target)
)",
                     "50\n52\n54\n-1\n55\n56\n57\n");
}

TEST_CASE("a move sum parameter owns the payload it is given") {
  runSumOwnerProgram("sum_move_parameter",
                     R"(
[effects(heap_alloc, io_out)]
take_move([Maybe<String> move] value) {
  show(value)
}

[effects(heap_alloc, io_out), return<Maybe<String>>]
pass_on([Maybe<String> move] value) {
  show(value)
  return(move(value))
}
)",
                     R"(
    [Maybe<String>] given{get(1i32)}
    take_move(move(given))
    take_move(get(4i32))
    take_move(get(-1i32))
    [Maybe<String>] kept{pass_on(get(6i32))}
    show(kept)
    [Maybe<String>] handed{get(7i32)}
    [Maybe<String>] back{pass_on(move(handed))}
    show(back)
)",
                     "49\n52\n-1\n54\n54\n55\n55\n");
}

TEST_SUITE_END();
