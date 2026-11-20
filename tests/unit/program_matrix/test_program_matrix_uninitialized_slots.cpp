#include "program_matrix.h"

TEST_SUITE_BEGIN("primestruct.program_matrix.uninitialized_slots");

// Slots of builtin vectors (docs/spec/value-lifecycle.md, Uninitialized Storage) and `block() { place }` given to a
// `move` parameter (Block expressions). Each program prints lines that name the live block count
// (`heapLiveBlocks() - start`), which only the VM and optexe have. The heap form runs on those; the values form
// (the same program without the heap lines) runs on native, which has no heap statistic. A value line the program
// prints is the same in both.

namespace {

struct Line {
  std::string text;
  bool heap = false;
};

// The expected output as space-separated lines; a line that ends in `h` reads the heap statistic (`3h` is the line
// `3`, printed only where the heap statistic exists).
std::vector<Line> parseLines(const std::string &tokens) {
  std::vector<Line> lines;
  std::istringstream stream(tokens);
  std::string token;
  while (stream >> token) {
    const bool heap = token.back() == 'h';
    lines.push_back({heap ? token.substr(0, token.size() - 1) : token, heap});
  }
  return lines;
}

std::string joinLines(const std::vector<Line> &lines, bool withHeap) {
  std::string out;
  for (const Line &line : lines) {
    if (withHeap || !line.heap) {
      out += line.text + "\n";
    }
  }
  return out;
}

// Drops every source line that reads the heap statistic.
std::string withoutHeapLines(const std::string &source) {
  std::string out;
  size_t at = 0;
  while (at < source.size()) {
    size_t end = source.find('\n', at);
    if (end == std::string::npos) {
      end = source.size();
    }
    const std::string line = source.substr(at, end - at);
    if (line.find("heapLiveBlocks") == std::string::npos) {
      out += line + "\n";
    }
    at = end + 1;
  }
  return out;
}

void runSlotProgram(const std::string &name,
                    const std::string &source,
                    const std::string &expectedTokens) {
  const std::vector<Line> expected = parseLines(expectedTokens);
  program_matrix::ProgramCase heap;
  heap.name = name;
  heap.source = source;
  heap.exitCode = 0;
  heap.stdoutText = joinLines(expected, true);
  heap.onlyConfigs = {"vm-step-O0", "vm-O0", "vm-O2", "optexe-O2"};
  program_matrix::runProgramMatrix(heap);
  if (!program_matrix::nativeSupported()) {
    return;
  }
  program_matrix::ProgramCase values;
  values.name = name + "_values";
  values.source = withoutHeapLines(source);
  values.exitCode = 0;
  values.stdoutText = joinLines(expected, false);
  values.onlyConfigs = {"native-O0", "native-O2"};
  program_matrix::runProgramMatrix(values);
}

} // namespace

// `at_unsafe`, `at`, `realloc` and `plus` on a `Pointer<uninitialized<vector<T>>>` step one five-slot record per
// element. The stdlib `Vector<vector<i64>>` a `Reference<vector<vector<i64>>>` helper pushes into grows by moving each
// record to its own place in the new buffer, so every pushed vector stays readable and the heap returns to its start.
TEST_CASE("a slot pointer of a builtin vector steps one record per element") {
  const std::string source = R"(
import /std/text/*
import /std/collections/*

[effects(heap_alloc), return<void>]
add([Reference<vector<vector<i64>>> mut] rows, [i64] first, [i64] second) {
  [vector<i64> mut] pair{vector<i64>(first)}
  pair.push(second)
  rows.push(pair)
}

[effects(heap_alloc), return<void>]
add_fresh([Reference<vector<vector<i64>>> mut] rows, [i64] value) {
  rows.push(vector<i64>(value))
}

[unsafe effects(heap_alloc), return<Pointer<uninitialized<vector<i64>>>>]
nth([Pointer<uninitialized<vector<i64>>> mut] base, [i32] index) {
  return(/std/intrinsics/memory/at_unsafe(base, index))
}

[unsafe effects(heap_alloc), return<void>]
release([Pointer<uninitialized<vector<i64>>> mut] base) {
  /std/intrinsics/memory/free(base)
}

[effects(heap_alloc, io_out), return<int>]
main() {
  [i32] start{heapLiveBlocks()}
  if(true) {
    [vector<vector<i64>> mut] rows{vector<vector<i64>>()}
    add(rows, 1i64, 2i64)
    add_fresh(rows, 3i64)
    add(rows, 4i64, 5i64)
    add_fresh(rows, 6i64)
    add_fresh(rows, 7i64)
    print_line(rows.count())
    print_line(rows[0i32][1i32] * 1000i64 + rows[1i32][0i32] * 100i64 + rows[2i32][1i32] * 10i64 + rows[4i32][0i32])
  }
  print_line(heapLiveBlocks() - start)
  if(true) {
    [Pointer<uninitialized<vector<i64>>> mut] base{/std/intrinsics/memory/alloc<uninitialized<vector<i64>>>(2i32)}
    [Pointer<uninitialized<vector<i64>>> mut] first{nth(base, 0i32)}
    [Pointer<uninitialized<vector<i64>>> mut] second{nth(base, 1i32)}
    init(*first, vector<i64>(1i64, 2i64, 3i64))
    init(*second, vector<i64>(4i64, 5i64))
    [Reference<vector<i64>>] a{borrow(*first)}
    [Reference<vector<i64>>] b{borrow(*second)}
    print_line(a.count() * 10i32 + b.count())
    print_line(heapLiveBlocks() - start)
    drop(*first)
    drop(*second)
    release(base)
  }
  print_line(heapLiveBlocks() - start)
  return(0i32)
}
)";
  runSlotProgram("slot_pointer_stride", source, "5 2357 0h 32 3h 0h");
}

// `consume(block() { seed })` into a `move` parameter is a copy of the place `seed` (the owner keeps it, Block
// expressions); a bare name and `move(x)` hand it over. The callee destroys its value once and the owner its own once.
TEST_CASE("a block place given to a move parameter is copied") {
  const std::string source = R"(
import /std/text/*
import /std/collections/*

[struct]
Holder() {
  [vector<i64>] items{vector<i64>()}
  [i32] id{0i32}
}

[struct]
Noisy() {
  [i32] id{0i32}
  Destroy() { print_line(this.id) }
}

[effects(heap_alloc, io_out), return<void>]
consume_holder([Holder move] h) {
  print_line(h.items.count())
}

[effects(heap_alloc, io_out), return<void>]
consume_vector([vector<i64> move] v) {
  print_line(v.count())
}

[effects(io_out), return<void>]
consume_noisy([Noisy move] n) {
  print_line(100i32)
}

[effects(heap_alloc, io_out), return<int>]
main() {
  [i32] start{heapLiveBlocks()}
  if(true) {
    [Holder] h{Holder{vector<i64>(1i64, 2i64), 1i32}}
    consume_holder(block() { h })
    print_line(h.items.count())
    print_line(heapLiveBlocks() - start)
  }
  print_line(heapLiveBlocks() - start)
  if(true) {
    [vector<i64> mut] v{vector<i64>(1i64, 2i64, 3i64)}
    consume_vector(block() { v })
    print_line(v.count())
    print_line(heapLiveBlocks() - start)
  }
  print_line(heapLiveBlocks() - start)
  if(true) {
    [Holder] h{Holder{vector<i64>(1i64, 2i64), 1i32}}
    consume_holder(h)
  }
  print_line(heapLiveBlocks() - start)
  if(true) {
    [Holder] h{Holder{vector<i64>(1i64), 2i32}}
    consume_holder(move(h))
  }
  print_line(heapLiveBlocks() - start)
  [Noisy] a{Noisy{1i32}}
  consume_noisy(block() { a })
  print_line(200i32)
  [Noisy] b{Noisy{2i32}}
  consume_noisy(move(b))
  print_line(300i32)
  return(0i32)
}
)";
  runSlotProgram(
      "block_place_move_parameter", source, "2 2 1h 0h 3 3 1h 0h 2 0h 1 0h 100 1 200 100 2 300 1");
}

TEST_SUITE_END();
