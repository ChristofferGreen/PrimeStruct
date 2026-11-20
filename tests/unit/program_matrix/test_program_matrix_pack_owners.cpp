#include "program_matrix.h"

TEST_SUITE_BEGIN("primestruct.program_matrix.pack_owners");

// An `[args<T>]` pack of a struct that holds something to destroy owns its elements (docs/spec/value-lifecycle.md,
// Ownership): a temporary argument is taken over, a place is copied into the pack through the struct's `Copy` helper
// (a `Copy` that prints 7000 + the first byte), a `[spread]` forwarded pack is copied element by element, and the
// callee destroys every element, first element first, when its body ends (a `Destroy` that prints 8000 + the first
// byte). The heap statistics, which exist on the VM and optexe only, show a leaked or freed-twice `String`.

namespace {

std::vector<std::string> vmOptexeConfigs() {
  return {"vm-step-O0", "vm-O0", "vm-O2", "optexe-O2"};
}

const std::string PackPrelude = R"(
import /std/text/*

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

[struct]
Box {
  [String] text{String{}}

  [public effects(io_out)]
  Copy(other) {
    print_line(7000i32 + first_byte(this.text))
  }

  [public effects(io_out)]
  Destroy() {
    print_line(8000i32 + first_byte(this.text))
  }
}

[effects(io_out), return<i32>]
total([args<Box>] xs) {
  return(first_byte(xs[0i32].text) + first_byte(xs[1i32].text))
}

[effects(io_out), return<i32>]
forward([args<Box>] xs) {
  return(total([spread] xs))
}

[effects(heap_alloc, io_out), return<i32>]
forward_mixed([args<Box>] xs) {
  return(total(Box{[text] label(9i32)}, [spread] xs))
}
)";

// Each `body` runs in a scope of its own between readings of the heap statistics, which are printed after it.
void runPackProgram(const std::string &name, const std::string &body, const std::string &stdoutText) {
  program_matrix::ProgramCase program;
  program.name = name;
  program.source = PackPrelude + R"(
[effects(heap_alloc, io_out), return<int>]
main() {
  [i32] blocks{heapLiveBlocks()}
  [u64] bytes{stringHeapBytesInUse()}
  if(true) {
)" + body +
                   R"(
  }
  print_line(heapLiveBlocks() - blocks)
  print_line(stringHeapBytesInUse() - bytes)
  return(0i32)
}
)";
  program.exitCode = 0;
  program.stdoutText = stdoutText;
  program.onlyConfigs = vmOptexeConfigs();
  program_matrix::runProgramMatrix(program);
}

} // namespace

TEST_CASE("an args pack of struct temporaries destroys its elements once") {
  runPackProgram("pack_temporaries", R"(
    print_line(total(Box{[text] label(3i32)}, Box{[text] label(4i32)}))
)",
                 "8051\n8052\n103\n0\n0\n");
}

TEST_CASE("an args pack of named structs copies each element and leaves the originals usable") {
  runPackProgram("pack_named", R"(
    [Box] a{Box{[text] label(1i32)}}
    [Box] b{Box{[text] label(2i32)}}
    print_line(total(a, b))
    print_line(first_byte(a.text) + first_byte(b.text))
)",
                 "7049\n7050\n8049\n8050\n99\n99\n8050\n8049\n0\n0\n");
}

TEST_CASE("a forwarded args pack is copied and every pack destroys its elements once") {
  runPackProgram("pack_forwarded", R"(
    print_line(forward(Box{[text] label(5i32)}, Box{[text] label(6i32)}))
)",
                 "7053\n7054\n8053\n8054\n8053\n8054\n107\n0\n0\n");
}

TEST_CASE("a pack forwarded with a leading temporary and a kept pack stays usable") {
  runPackProgram("pack_forwarded_mixed", R"(
    print_line(forward_mixed(Box{[text] label(1i32)}))
)",
                 "7049\n8057\n8049\n8049\n106\n0\n0\n");
}

TEST_CASE("an args pack of a struct with nothing to destroy copies and destroys nothing") {
  program_matrix::ProgramCase program;
  program.name = "pack_plain_struct";
  program.source = R"(
[struct]
Point {
  [i32] x{0i32}
  [i32] y{0i32}
}

[return<i32>]
sum([args<Point>] points) {
  return(points[0i32].x + points[1i32].y)
}

[return<i32>]
forward([args<Point>] points) {
  return(sum([spread] points))
}

[effects(io_out), return<int>]
main() {
  [Point] a{Point{[x] 1i32, [y] 2i32}}
  print_line(sum(a, Point{[x] 5i32, [y] 6i32}))
  print_line(forward(a, a))
  return(0i32)
}
)";
  program.exitCode = 0;
  program.stdoutText = "7\n3\n";
  program_matrix::runProgramMatrix(program);
}

TEST_SUITE_END();
