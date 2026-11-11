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

TEST_CASE("the arms of an assigned pick may open on the line after the subject") {
  runSumOwnerProgram("sum_pick_arms_on_next_line",
                     "",
                     R"(
    [i32 mut] value{0i32}
    value = pick(get(1i32))
    {
      some(text) { first_byte(text) }
      none { -1i32 }
    }
    print_line(value)
    value = 1i32 + pick(get(-1i32))

    {
      some(text) { first_byte(text) }
      none { -1i32 }
    }
    print_line(value)
    [i32] bound{pick(get(2i32))
    {
      some(text) { first_byte(text) }
      none { -1i32 }
    }}
    print_line(bound)
    pick(get(3i32))
    {
      some(text) { print_line(first_byte(text)) }
      none { print_line(-1i32) }
    }
)",
                     "49\n0\n50\n51\n");
}

// The arguments written at a call are evaluated before the callee starts, so a call to the same definition (or to
// the same instantiation of a template) inside one of them is not recursion.
TEST_CASE("a call takes a call to the same definition as its argument") {
  runSumOwnerProgram("sum_owner_same_definition_argument",
                     R"(
[effects(heap_alloc), return<String>]
first_or([Maybe<String>] value) {
  return(pick(value) {
    some(text) { text }
    none { label(0i32) }
  })
}

[effects(heap_alloc), return<Maybe<String>>]
wrap_again([i32] id) {
  return(some<String>(first_or(get(id))))
}

[effects(heap_alloc), return<Maybe<vector<i32>>>]
get_values([i32] id) {
  if(id < 0i32) {
    return(none<vector<i32>>())
  }
  return(some<vector<i32>>(vector<i32>(id, id + 1i32)))
}

[effects(heap_alloc), return<Maybe<vector<i32>>>]
values_or_empty([i32] id) {
  return(some<vector<i32>>(pick(get_values(id)) {
    some(values) { values }
    none { vector<i32>() }
  }))
}

[effects(io_out)]
show_count([Maybe<vector<i32>>] value) {
  pick(value) {
    some(values) {
      print_line(values.count())
    }
    none {
      print_line(-1i32)
    }
  }
}

[return<i32>]
twice([i32] value) {
  return(value * 2i32)
}
)",
                     R"(
    [Maybe<String>] again{some<String>(first_or(get(3i32)))}
    show(again)
    show(wrap_again(4i32))
    show(wrap_again(-1i32))
    show(some<String>(first_or(some<String>(first_or(get(5i32))))))
    show_count(values_or_empty(7i32))
    show_count(values_or_empty(-1i32))
    print_line(twice(twice(twice(3i32))))
)",
                     "51\n52\n48\n53\n2\n0\n24\n");
}

TEST_CASE("a definition inlined into itself through an argument is still rejected on the VM") {
  program_matrix::ProgramCase program;
  program.name = "sum_owner_recursive_argument_rejected";
  program.source = R"(
[return<int>]
same([int] value) {
  return(value)
}

[return<int>]
main() {
  return(same(same(main())))
}
)";
  program.exitCode = 2;
  program.stdoutText = "";
  program.vmStderr = "VM lowering error: vm backend does not support recursive calls: /main\n";
  program.onlyConfigs = {"vm-step-O0", "vm-O0", "vm-O2"};
  program_matrix::runProgramMatrix(program);
}

TEST_SUITE_END();
