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

// A struct field `[uninitialized<vector<T>>]` is the five-slot record: `init` stores it, `take` moves it out,
// `drop` frees it. Neither leaves an operand on the stack, so a block of the program joins cleanly.
TEST_CASE("a struct field of an uninitialized vector stores the whole record") {
  const std::string source = R"(
import /std/text/*
import /std/collections/*

[struct]
Holder() {
  [uninitialized<vector<i64>>] items{uninitialized<vector<i64>>()}
  Create() {
  }
}

[effects(heap_alloc, io_out), return<int>]
main() {
  [i32] start{heapLiveBlocks()}
  if(true) {
    [Holder mut] h{Holder{}}
    init(h.items, vector<i64>(1i64, 2i64, 3i64))
    print_line(heapLiveBlocks() - start)
    [vector<i64> mut] out{take(h.items)}
    out.push(4i64)
    print_line(out.count())
    print_line(out[2i32] * 10i64 + out[3i32])
  }
  print_line(heapLiveBlocks() - start)
  if(true) {
    [Holder mut] h{Holder{}}
    init(h.items, vector<i64>(5i64, 6i64))
    print_line(heapLiveBlocks() - start)
    drop(h.items)
  }
  print_line(heapLiveBlocks() - start)
  return(0i32)
}
)";
  runSlotProgram("uninitialized_vector_field", source, "1h 4 34 0h 1h 0h");
}

// A slot of a vector of structs: `init` of a named owner or a temporary hands the buffer over, of a place copies each
// element; `drop` destroys each element once (`Destroy` prints its id) and frees every buffer.
TEST_CASE("a slot of a vector of structs copies on init and destroys on drop") {
  const std::string source = R"(
import /std/text/*
import /std/collections/*

[struct]
Noisy() {
  [i32] id{0i32}
  [vector<i64>] data{vector<i64>()}
  Destroy() { print_line(this.id) }
}

[effects(heap_alloc), return<vector<Noisy>>]
make([i32] first, [i32] second) {
  [vector<Noisy> mut] out{vector<Noisy>()}
  out.push(Noisy{first, vector<i64>(5i64)})
  out.push(Noisy{second, vector<i64>(6i64)})
  return(out)
}

[effects(heap_alloc), return<void>]
store_copy([Pointer<uninitialized<vector<Noisy>>> mut] slot, [vector<Noisy>] shared) {
  init(*slot, shared)
}

[unsafe effects(heap_alloc), return<void>]
release([Pointer<uninitialized<vector<Noisy>>> mut] slot) {
  drop(*slot)
  /std/intrinsics/memory/free(slot)
}

[effects(heap_alloc, io_out), return<int>]
main() {
  [i32] start{heapLiveBlocks()}
  if(true) {
    [vector<Noisy> mut] owner{make(11i32, 22i32)}
    [Pointer<uninitialized<vector<Noisy>>> mut] slot{/std/intrinsics/memory/alloc<uninitialized<vector<Noisy>>>(1i32)}
    init(*slot, owner)
    [Reference<vector<Noisy>>] stored{borrow(*slot)}
    print_line(stored.count())
    print_line(900i32)
    release(slot)
    print_line(901i32)
  }
  print_line(heapLiveBlocks() - start)
  if(true) {
    [Pointer<uninitialized<vector<Noisy>>> mut] slot{/std/intrinsics/memory/alloc<uninitialized<vector<Noisy>>>(1i32)}
    init(*slot, make(33i32, 44i32))
    print_line(910i32)
    release(slot)
    print_line(911i32)
  }
  print_line(heapLiveBlocks() - start)
  if(true) {
    [vector<Noisy> mut] place{make(55i32, 66i32)}
    [Pointer<uninitialized<vector<Noisy>>> mut] slot{/std/intrinsics/memory/alloc<uninitialized<vector<Noisy>>>(1i32)}
    store_copy(slot, place)
    print_line(920i32)
    release(slot)
    print_line(921i32)
  }
  print_line(heapLiveBlocks() - start)
  return(0i32)
}
)";
  runSlotProgram("slot_of_vector_of_structs",
                 source,
                 "2 900 11 22 901 0h 910 33 44 911 0h 920 55 66 921 55 66 0h");
}

// `rows[i]`, `rows[i].count()` and `rows[i][j]` read an element of a builtin vector of vectors through a
// `Reference<vector<vector<i64>>>` parameter in place (TODO-6104): the owner keeps every buffer, so repeated reads leave
// the heap alone and the owner's destroy at scope end frees each buffer once.
TEST_CASE("an element of a vector of vectors read through a Reference stays owned") {
  const std::string source = R"(
import /std/text/*
import /std/collections/*

[effects(heap_alloc), return<i32>]
peek_count([Reference<vector<vector<i64>>>] rows, [i32] index) {
  return(rows[index].count())
}

[effects(heap_alloc), return<i64>]
peek_value([Reference<vector<vector<i64>>>] rows, [i32] index, [i32] at) {
  return(rows[index][at])
}

[effects(heap_alloc), return<i64>]
peek_sum([Reference<vector<vector<i64>>>] rows) {
  [i64 mut] total{0i64}
  for([i32 mut] i{0i32}, i < rows.count(), i++) {
    for([i32 mut] j{0i32}, j < rows[i].count(), j++) {
      [i64] value{rows[i][j]}
      total = total + value
    }
  }
  return(total)
}

[effects(heap_alloc), return<i32>]
peek_bound([Reference<vector<vector<i64>>>] rows) {
  [vector<i64>] copied{rows[1i32]}
  return(copied.count())
}

[effects(heap_alloc, io_out), return<int>]
main() {
  [i32] start{heapLiveBlocks()}
  if(true) {
    [vector<vector<i64>> mut] rows{vector<vector<i64>>()}
    rows.push(vector<i64>(7i64, 8i64))
    rows.push(vector<i64>(9i64, 10i64, 11i64))
    print_line(peek_count(rows, 0i32))
    print_line(heapLiveBlocks() - start)
    print_line(peek_count(rows, 1i32))
    print_line(peek_value(rows, 1i32, 2i32))
    print_line(peek_value(rows, 0i32, 0i32))
    print_line(peek_sum(rows))
    print_line(peek_bound(rows))
    print_line(heapLiveBlocks() - start)
    print_line(rows[0i32].count() + rows[1i32].count())
  }
  print_line(heapLiveBlocks() - start)
  return(0i32)
}
)";
  runSlotProgram("reference_vector_element_reads", source, "2 3h 3 11 7 45 3 3h 5 0h");
}

TEST_SUITE_END();
