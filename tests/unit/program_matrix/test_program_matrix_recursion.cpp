#include "program_matrix.h"

#include <string>

TEST_SUITE_BEGIN("primestruct.program_matrix.recursion");

// A recursive definition runs on the VM only as a real call. A builtin `vector<T>` parameter taken by `mut` or read
// only, and a `Reference<vector<T>>`, pass the caller's record address (docs/spec/vm-design.md, Real calls), so the
// recursion pushes to and reads the caller's vector, growth included. Each program reads the heap statistics before
// and after its body and prints the differences last.

namespace {

const std::string RecursionPrelude = R"(
import /std/text/*
import /std/collections/*

[struct]
Named() {
  [String] text{String{}}
}

[effects(heap_alloc), return<String>]
label([i32] id) {
  [String mut] text{String{}}
  text.pushByte(convert<u8>(48i32 + id))
  return(text)
}
)";

void runRecursionProgram(const std::string &name,
                         const std::string &definitions,
                         const std::string &body,
                         const std::string &stdoutText) {
  program_matrix::ProgramCase program;
  program.name = name;
  program.source = RecursionPrelude + definitions + R"(
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
  program.onlyConfigs = {"vm-step-O0", "vm-O0", "vm-O2", "optexe-O2"};
  program_matrix::runProgramMatrix(program);
}

} // namespace

TEST_CASE("a recursive definition pushes to a mut vector parameter past its capacity") {
  runRecursionProgram("recursion_mut_vector_parameter",
                      R"(
[effects(heap_alloc), return<void>]
walk([i32] depth, [vector<i32> mut] out) {
  if(depth > 0i32) {
    out.push(depth)
    walk(depth - 1i32, out)
  }
}
)",
                      R"(
    [vector<i32> mut] out{vector<i32>{}}
    walk(20i32, out)
    print_line(out.count())
    print_line(out[0i32])
    print_line(out[19i32])
)",
                      "20\n20\n1\n");
}

TEST_CASE("a recursive definition pushes strings through a reference to a vector") {
  runRecursionProgram("recursion_reference_to_vector_parameter",
                      R"(
[effects(heap_alloc), return<void>]
walk([i32] depth, [Reference<vector<String>>] out) {
  if(depth > 0i32) {
    out.push(label(depth))
    walk(depth - 1i32, out)
  }
}
)",
                      R"(
    [vector<String> mut] out{vector<String>{}}
    walk(9i32, out)
    print_line(out.count())
    print_line(convert<i32>(out[0i32].byteAt(0i32)))
    print_line(convert<i32>(out[8i32].byteAt(0i32)))
)",
                      "9\n57\n49\n");
}

TEST_CASE("a recursive definition pushes destroyable elements and sums a read-only vector") {
  runRecursionProgram("recursion_struct_elements",
                      R"(
[effects(heap_alloc), return<void>]
collect([i32] depth, [vector<Named> mut] out) {
  if(depth > 0i32) {
    out.push(Named{label(depth)})
    collect(depth - 1i32, out)
  }
}

[return<i32>]
sum_from([vector<Named>] items, [i32] index) {
  if(index >= items.count()) {
    return(0i32)
  }
  return(convert<i32>(items[index].text.byteAt(0i32)) + sum_from(items, index + 1i32))
}
)",
                      R"(
    [vector<Named> mut] out{vector<Named>{}}
    collect(6i32, out)
    print_line(out.count())
    print_line(sum_from(out, 0i32))
)",
                      "6\n309\n");
}

TEST_CASE("a recursive definition replaces the caller's vector through a mut parameter") {
  runRecursionProgram("recursion_mut_vector_replace",
                      R"(
[effects(heap_alloc), return<vector<i32>>]
make_values([i32] first) {
  [vector<i32> mut] values{vector<i32>{}}
  values.push(first)
  values.push(first + 1i32)
  return(values)
}

[effects(heap_alloc), return<void>]
rebuild([i32] depth, [vector<i32> mut] out) {
  if(depth > 0i32) {
    out = make_values(depth)
    out.push(100i32)
    rebuild(depth - 1i32, out)
  }
}
)",
                      R"(
    [vector<i32> mut] out{vector<i32>{}}
    rebuild(3i32, out)
    print_line(out.count())
    print_line(out[0i32])
    print_line(out[2i32])
)",
                      "3\n1\n100\n");
}

TEST_SUITE_END();
