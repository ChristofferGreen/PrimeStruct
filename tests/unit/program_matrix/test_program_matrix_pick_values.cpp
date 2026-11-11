#include "program_matrix.h"

TEST_SUITE_BEGIN("primestruct.program_matrix.pick_values");

// What a `pick` expression yields is a value of its own: a fresh value and a local that ends with the arm are
// taken over, any other place (a local of the enclosing scope, the payload of a sum somebody else owns, a
// field, an element) is copied, and the result is destroyed by whoever it initializes. Every program prints
// what it reads and then how many heap blocks and string-heap bytes are still in use; a value destroyed twice
// ends the program with `string heap block freed twice`, and one read after its owner freed it prints another
// block's bytes.

namespace {

// `label(id)` is one byte, the digit `id`; `get(id)` and `get_values(first)` are `none` below zero.
const std::string PickValuePrelude = R"(
import /std/text/*
import /std/maybe/*
import /std/collections/*

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

[effects(heap_alloc), return<vector<i32>>]
make_values([i32] first) {
  [vector<i32> mut] values{vector<i32>{}}
  values.push(first)
  values.push(first + 1i32)
  return(values)
}

[effects(heap_alloc), return<Maybe<vector<i32>>>]
get_values([i32] first) {
  if(first < 0i32) {
    return(none<vector<i32>>())
  }
  return(some<vector<i32>>(make_values(first)))
}
)";

// Wraps `body` between two readings of the heap statistics and prints the differences.
std::string pickValueProgram(const std::string &definitions, const std::string &body) {
  return PickValuePrelude + definitions + R"(
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
}

void runPickValueProgram(const std::string &name,
                         const std::string &source,
                         const std::string &stdoutText) {
  program_matrix::ProgramCase program;
  program.name = name;
  program.source = source;
  program.exitCode = 0;
  program.stdoutText = stdoutText;
  // The heap statistics exist on the VM and optexe only.
  program.onlyConfigs = {"vm-step-O0", "vm-O0", "vm-O2", "optexe-O2"};
  program_matrix::runProgramMatrix(program);
}

// What the compiler says about `source`, which it must reject.
program_matrix::Outcome compilePickValueProgram(const std::string &name,
                                                const std::string &source) {
  const std::filesystem::path dir =
      primec::testing::testScratchPath("program_matrix/" + name + "/x").parent_path();
  std::filesystem::create_directories(dir);
  const std::filesystem::path sourcePath = dir / (name + ".prime");
  {
    std::ofstream file(sourcePath, std::ios::binary);
    file << source;
  }
  return program_matrix::runCaptured("./primec --emit=ir " +
                                         program_matrix::quote(sourcePath.string()) + " -o " +
                                         program_matrix::quote((dir / (name + ".psir")).string()),
                                     dir / (name + ".ir"));
}

const std::string HolderDefinitions = R"(
[struct]
Holder() {
  [String] name{label(0i32)}
  [vector<i32>] items{vector<i32>{}}
}
)";

} // namespace

TEST_CASE("a discarded pick expression that yields a vector frees it") {
  const std::string source = pickValueProgram("", R"(
    pick(get_values(7i32)) { none { vector<i32>{} } some(values) { values } }
    pick(get_values(7i32)) { none { make_values(1i32) } some(values) { make_values(2i32) } }
    pick(get_values(0i32 - 1i32)) { none { make_values(1i32) } some(values) { make_values(2i32) } }
    [vector<i32>] mine{make_values(5i32)}
    for([i32 mut] round{0i32}, round < 3i32, round = round + 1i32) {
      pick(get_values(round)) { none { vector<i32>{} } some(values) { mine } }
      pick(get_values(round)) { none { vector<i32>{} } some(values) { values } }
    }
    print_line(mine[0i32] + mine.count())
)");
  runPickValueProgram("pick_value_discarded_vector", source, "7\n0\n0\n");
}

TEST_CASE("a pick expression arm copies a borrowed vector and an element") {
  const std::string source = pickValueProgram(R"(
[effects(heap_alloc), return<i32>]
pick_param([vector<i32>] items, [i32] id) {
  [vector<i32>] kept{pick(get_values(id)) { none { vector<i32>{} } some(values) { items } }}
  return(kept.count() + items.count())
}

[effects(heap_alloc), return<i32>]
count_of([vector<i32>] items) {
  return(items.count())
}

[effects(heap_alloc), return<i32>]
pick_param_arg([vector<i32>] items, [i32] id) {
  return(count_of(pick(get_values(id)) { none { vector<i32>{} } some(values) { items } }) + items.count())
}
)",
                                              R"(
    [vector<i32> mut] mine{vector<i32>{}}
    mine.push(5i32)
    print_line(pick_param(mine, 3i32))
    print_line(pick_param(mine, 0i32 - 1i32))
    print_line(pick_param_arg(mine, 3i32))
    print_line(pick_param(make_values(1i32), 3i32))
    print_line(mine.count())
    print_line(mine[0i32])
    [vector<vector<i32>> mut] nested{vector<vector<i32>>{}}
    nested.push(make_values(4i32))
    [vector<i32>] inner{pick(get_values(3i32)) { none { vector<i32>{} } some(values) { nested[0i32] } }}
    print_line(inner.count())
    print_line(nested[0i32].count())
)");
  runPickValueProgram("pick_value_borrowed_vector", source, "2\n1\n2\n4\n1\n5\n2\n2\n0\n0\n");
}

TEST_CASE("a pick expression arm copies a local of the enclosing scope") {
  // The result ends before the local does, and the blocks it frees are handed out again: a result that took
  // the local over would leave `mine` and `text` reading the new owner's bytes.
  const std::string source = pickValueProgram("", R"(
    [vector<i32> mut] mine{make_values(5i32)}
    if(true) {
      [vector<i32>] kept{pick(get_values(3i32)) { none { vector<i32>{} } some(values) { mine } }}
      print_line(kept[0i32])
    }
    [vector<i32> mut] again{make_values(7i32)}
    print_line(mine[0i32])
    [String] text{label(4i32)}
    if(true) {
      [String] kept{pick(get(3i32)) { none { label(0i32) } some(line) { text } }}
      print_line(first_byte(kept))
    }
    [String] other{label(6i32)}
    print_line(first_byte(text))
)");
  runPickValueProgram("pick_value_outer_local", source, "5\n5\n52\n52\n0\n0\n");
}

TEST_CASE("a pick expression arm copies the payload of a sum local") {
  const std::string source = pickValueProgram("", R"(
    [Maybe<String>] held{get(3i32)}
    if(true) {
      [String] kept{pick(held) { none { label(0i32) } some(line) { line } }}
      print_line(first_byte(kept))
    }
    [String] other{label(6i32)}
    pick(held) { none { print_line(0i32) } some(line) { print_line(first_byte(line)) } }
    [Maybe<vector<i32>>] values_held{get_values(3i32)}
    if(true) {
      [vector<i32>] kept{pick(values_held) { none { vector<i32>{} } some(values) { values } }}
      print_line(kept[0i32])
    }
    [vector<i32>] more{make_values(8i32)}
    pick(values_held) { none { print_line(0i32) } some(values) { print_line(values[0i32]) } }
)");
  runPickValueProgram("pick_value_sum_local_payload", source, "51\n51\n3\n3\n0\n0\n");
}

TEST_CASE("a pick expression arm copies the payload of a borrowed sum") {
  const std::string source = pickValueProgram(R"(
[effects(heap_alloc), return<i32>]
unwrap_first([Maybe<String>] held) {
  [String] kept{pick(held) { none { label(0i32) } some(line) { line } }}
  return(first_byte(kept))
}

[effects(heap_alloc), return<i32>]
choose_first([String] fallback, [i32] id) {
  [String] kept{pick(get(id)) { none { fallback } some(line) { fallback } }}
  return(first_byte(kept))
}
)",
                                              R"(
    [Maybe<String>] held{get(3i32)}
    print_line(unwrap_first(held))
    print_line(unwrap_first(held))
    [String] text{label(4i32)}
    print_line(choose_first(text, 1i32))
    print_line(choose_first(text, 0i32 - 1i32))
    print_line(first_byte(text))
)");
  runPickValueProgram("pick_value_borrowed_sum", source, "51\n51\n52\n52\n52\n0\n0\n");
}

TEST_CASE("a pick expression arm copies a field and an element") {
  const std::string source = pickValueProgram(HolderDefinitions, R"(
    [Holder mut] holder{Holder{label(7i32), make_values(9i32)}}
    [String] kept{pick(get(3i32)) { none { label(0i32) } some(line) { holder.name } }}
    print_line(first_byte(kept))
    [vector<i32>] values{pick(get_values(3i32)) { none { vector<i32>{} } some(found) { holder.items } }}
    print_line(values[0i32])
    [vector<String> mut] names{vector<String>{}}
    names.push(label(2i32))
    [String] second{pick(get(3i32)) { none { label(0i32) } some(line) { names[0i32] } }}
    print_line(first_byte(second))
    print_line(first_byte(holder.name) + holder.items[0i32] + first_byte(names[0i32]))
)");
  runPickValueProgram("pick_value_field_and_element", source, "55\n9\n50\n114\n0\n0\n");
}

TEST_CASE("pick arms that yield a String field and an integer are rejected") {
  const program_matrix::Outcome outcome = compilePickValueProgram(
      "pick_value_field_mismatch", PickValuePrelude + HolderDefinitions + R"(
[effects(heap_alloc, io_out), return<int>]
main() {
  [Holder] holder{Holder{label(7i32), make_values(9i32)}}
  [String] kept{pick(get(3i32)) { none { 1i32 } some(line) { holder.name } }}
  print_line(first_byte(kept))
  return(0i32)
}
)");
  CHECK(outcome.exitCode != 0);
  CHECK_MESSAGE(outcome.err.find("pick branches must return compatible types") != std::string::npos,
                outcome.err);
}

TEST_CASE("a pick expression passed as an argument is destroyed after the call") {
  const std::string source = pickValueProgram("", R"(
    print_line(first_byte(pick(get(3i32)) { none { label(0i32) } some(line) { line } }))
    print_line(first_byte(pick(get(0i32 - 3i32)) { none { label(0i32) } some(line) { line } }))
    [String] text{label(4i32)}
    print_line(first_byte(pick(get(3i32)) { none { label(0i32) } some(line) { text } }))
    print_line(first_byte(text))
)");
  runPickValueProgram("pick_value_argument", source, "51\n48\n52\n52\n0\n0\n");
}

TEST_CASE("a payload moved out of a sum local is destroyed by its new owner") {
  const std::string source = pickValueProgram(R"(
[effects(heap_alloc), return<i32>]
consume([String move] text) {
  return(first_byte(text))
}
)",
                                              R"(
    [Maybe<String> mut] held{get(3i32)}
    pick(held) {
      none { }
      some(line) {
        [String] taken{move(line)}
        print_line(first_byte(taken))
      }
    }
    [Maybe<String>] sent{get(4i32)}
    pick(sent) {
      none { }
      some(line) { print_line(consume(line)) }
    }
    [Maybe<String>] kept{get(5i32)}
    [String] copy{pick(kept) { none { label(0i32) } some(line) { line } }}
    pick(kept) { none { print_line(0i32) } some(line) { print_line(first_byte(line) + first_byte(copy)) } }
)");
  runPickValueProgram("pick_value_moved_payload", source, "51\n52\n106\n0\n0\n");
}

namespace {

// The diagnostic for `main`'s `body` after `definitions`, which the compiler must reject.
std::string rejectedPickValueProgram(const std::string &name,
                                     const std::string &definitions,
                                     const std::string &body) {
  const program_matrix::Outcome outcome =
      compilePickValueProgram(name, pickValueProgram(definitions, body));
  CHECK(outcome.exitCode != 0);
  return outcome.err;
}

} // namespace

TEST_CASE("a sum local whose payload was moved out is moved-from") {
  const std::string moved = rejectedPickValueProgram("pick_value_moved_local", "", R"(
    [Maybe<String>] held{get(3i32)}
    pick(held) {
      none { }
      some(line) {
        [String] taken{move(line)}
        print_line(first_byte(taken))
      }
    }
    pick(held) { none { print_line(0i32) } some(line) { print_line(first_byte(line)) } }
)");
  CHECK_MESSAGE(moved.find("use-after-move: held") != std::string::npos, moved);

  const std::string sent = rejectedPickValueProgram("pick_value_sent_local",
                                                    R"(
[effects(heap_alloc), return<i32>]
consume([String move] text) {
  return(first_byte(text))
}
)",
                                                    R"(
    [Maybe<String>] held{get(3i32)}
    [i32] byte{pick(held) { none { 0i32 } some(line) { consume(line) } }}
    pick(held) { none { print_line(byte) } some(line) { print_line(first_byte(line)) } }
)");
  CHECK_MESSAGE(sent.find("use-after-move: held") != std::string::npos, sent);
}

TEST_CASE("the payload of a borrowed sum is not moved or returned") {
  const std::string returned = rejectedPickValueProgram("pick_value_lent_return",
                                                        R"(
[effects(heap_alloc), return<String>]
unwrap([Maybe<String>] held) {
  pick(held) {
    none { }
    some(line) { return(line) }
  }
  return(label(0i32))
}
)",
                                                        R"(
    [Maybe<String>] held{get(3i32)}
    print_line(first_byte(unwrap(held)))
)");
  CHECK_MESSAGE(returned.find("borrowed pick payload escapes via return: line") !=
                    std::string::npos,
                returned);

  const std::string moved = rejectedPickValueProgram("pick_value_lent_move",
                                                     R"(
[effects(heap_alloc), return<i32>]
take([Maybe<String>] held) {
  pick(held) {
    none { }
    some(line) {
      [String] taken{move(line)}
      return(first_byte(taken))
    }
  }
  return(0i32)
}
)",
                                                     R"(
    [Maybe<String>] held{get(3i32)}
    print_line(take(held))
)");
  CHECK_MESSAGE(moved.find("borrowed pick payload cannot be moved: line") != std::string::npos,
                moved);

  const std::string field = rejectedPickValueProgram("pick_value_lent_field",
                                                     R"(
[struct]
Slot() {
  [Maybe<String>] held{none<String>()}
}

[effects(heap_alloc), return<i32>]
consume([String move] text) {
  return(first_byte(text))
}
)",
                                                     R"(
    [Slot] slot{Slot{get(3i32)}}
    pick(slot.held) {
      none { }
      some(line) { print_line(consume(line)) }
    }
)");
  CHECK_MESSAGE(field.find("borrowed pick payload cannot be moved: line") != std::string::npos,
                field);
}

TEST_CASE("an imported String passed to a move parameter is moved-from") {
  // The payload rules rest on this: `String` is an owning type where it is imported, not only in `/std/text`.
  const std::string moved = rejectedPickValueProgram("pick_value_moved_string",
                                                     R"(
[effects(heap_alloc), return<i32>]
consume([String move] text) {
  return(first_byte(text))
}
)",
                                                     R"(
    [String] text{label(3i32)}
    print_line(consume(text))
    print_line(first_byte(text))
)");
  CHECK_MESSAGE(moved.find("use-after-move: text") != std::string::npos, moved);
}

namespace {

const std::string SumArgumentDefinitions = R"(
[effects(heap_alloc), return<i32>]
first_of([Maybe<String>] held) {
  return(pick(held) { none { 0i32 } some(line) { first_byte(line) } })
}

[effects(heap_alloc), return<i32>]
count_of([Maybe<vector<i32>>] held) {
  return(pick(held) { none { 0i32 } some(values) { values.count() } })
}

[effects(heap_alloc), return<i32>]
copied([Maybe<String> copy] held) {
  return(pick(held) { none { 0i32 } some(line) { first_byte(line) } })
}

[effects(heap_alloc), return<i32>]
taken([Maybe<String> move] held) {
  return(pick(held) { none { 0i32 } some(line) { first_byte(line) } })
}

[effects(heap_alloc), return<Maybe<String>>]
pass_on([Maybe<String> move] held) {
  return(held)
}

[effects(heap_alloc), return<i32>]
twice([Maybe<String>] held) {
  return(first_of(held) + first_of(held))
}
)";

} // namespace

TEST_CASE("a sum temporary passed as an argument is destroyed by the callee") {
  // A call result nobody names belongs to the parameter it is passed to, whatever the parameter's kind; a
  // named sum local stays the caller's and is destroyed once, by its scope.
  runPickValueProgram("pick_value_sum_temporary_argument",
                      pickValueProgram(SumArgumentDefinitions,
                                       R"(
    print_line(first_of(get(8i32)))
    print_line(first_of(get(0i32 - 1i32)))
    print_line(count_of(get_values(8i32)))
    print_line(copied(get(6i32)))
    print_line(twice(get(1i32)))
    print_line(first_of(pick(get(4i32)) { none { get(0i32) } some(line) { get(3i32) } }))
    [Maybe<String>] named{get(5i32)}
    print_line(first_of(named))
    print_line(copied(named))
    print_line(first_of(named))
)"),
                      "56\n0\n2\n54\n98\n51\n53\n53\n53\n0\n0\n");
}

TEST_CASE("a sum handed to a move parameter is destroyed by its new owner") {
  runPickValueProgram("pick_value_sum_move_argument",
                      pickValueProgram(SumArgumentDefinitions,
                                       R"(
    print_line(taken(get(7i32)))
    print_line(first_of(pass_on(get(8i32))))
    [Maybe<String>] named{get(5i32)}
    print_line(taken(move(named)))
    [Maybe<String>] other{get(4i32)}
    print_line(taken(other))
    [Maybe<String>] third{get(3i32)}
    if(first_of(third) > 0i32) {
      print_line(taken(third))
    }
    [Maybe<String>] last{get(2i32)}
    print_line(first_of(pass_on(last)))
)"),
                      "55\n56\n53\n52\n51\n50\n0\n0\n");
}

TEST_CASE("a sum that owns a payload is moved-from and does not escape a borrow") {
  const std::string moved = rejectedPickValueProgram("pick_value_sum_moved_argument",
                                                     SumArgumentDefinitions,
                                                     R"(
    [Maybe<String>] held{get(3i32)}
    print_line(taken(held))
    print_line(first_of(held))
)");
  CHECK_MESSAGE(moved.find("use-after-move: held") != std::string::npos, moved);

  const std::string escaped = rejectedPickValueProgram("pick_value_sum_escaping_parameter",
                                                       R"(
[effects(heap_alloc), return<Maybe<String>>]
hand_back([Maybe<String>] held) {
  return(held)
}
)",
                                                       R"(
    [Maybe<String>] held{get(3i32)}
    [Maybe<String>] other{hand_back(held)}
)");
  CHECK_MESSAGE(escaped.find("borrowed parameter escapes via return: held") != std::string::npos,
                escaped);
}

namespace {

const std::string BoxedSumDefinitions = R"(
[struct]
Box() {
  [String] name{label(0i32)}
  Destroy() { }
}
[effects(heap_alloc), return<Maybe<Box>>]
get_box([i32] id) {
  return(some<Box>(Box{label(id)}))
}
)";

} // namespace

TEST_CASE("assigning to a mut sum local destroys the payload it held") {
  // A `String`, a builtin vector and a struct with `Destroy` as the payload; `held = held` keeps its payload.
  runPickValueProgram("pick_value_sum_assignment",
                      pickValueProgram(BoxedSumDefinitions,
                                       R"(
    [Maybe<String> mut] held{get(4i32)}
    held = get(6i32)
    pick(held) { none { print_line(0i32) } some(line) { print_line(first_byte(line)) } }
    held = none<String>()
    held = get(7i32)
    held = held
    pick(held) { none { print_line(0i32) } some(line) { print_line(first_byte(line)) } }
    [Maybe<vector<i32>> mut] values_held{get_values(4i32)}
    values_held = get_values(6i32)
    values_held = none<vector<i32>>()
    [Maybe<Box> mut] boxed{get_box(1i32)}
    boxed = get_box(2i32)
    pick(boxed) { none { print_line(0i32) } some(box) { print_line(first_byte(box.name)) } }
)"),
                      "54\n55\n50\n0\n0\n");
}

TEST_CASE("a sum local assigned from a place gets a payload of its own") {
  // The place keeps its payload; a `mut` sum local built from a place owns nothing until it is assigned, and
  // `move(first)` hands over what `first` owned.
  runPickValueProgram("pick_value_sum_assignment_from_place",
                      pickValueProgram(BoxedSumDefinitions,
                                       R"(
    [Maybe<vector<i32>>] source{get_values(4i32)}
    [Maybe<vector<i32>> mut] values_held{get_values(6i32)}
    values_held = source
    values_held = values_held
    pick(values_held) { none { print_line(0i32) } some(values) { print_line(values.count()) } }
    pick(source) { none { print_line(0i32) } some(values) { print_line(values[0i32]) } }
    [Maybe<Box>] box_source{get_box(3i32)}
    [Maybe<Box> mut] boxed{get_box(1i32)}
    boxed = box_source
    pick(boxed) { none { print_line(0i32) } some(box) { print_line(first_byte(box.name)) } }
    [Maybe<String> mut] held{none<String>()}
    [i32 mut] index{0i32}
    while(index < 3i32) {
      held = get(index)
      index = index + 1i32
    }
    pick(held) { none { print_line(0i32) } some(line) { print_line(first_byte(line)) } }
    [Maybe<String>] first{get(8i32)}
    [Maybe<String> mut] alias{first}
    alias = get(9i32)
    pick(alias) { none { print_line(0i32) } some(line) { print_line(first_byte(line)) } }
    pick(first) { none { print_line(0i32) } some(line) { print_line(first_byte(line)) } }
    [Maybe<String>] moved{move(first)}
    pick(moved) { none { print_line(0i32) } some(line) { print_line(first_byte(line)) } }
)"),
                      "2\n4\n51\n50\n57\n56\n56\n0\n0\n");
}

namespace {

const std::string ReturnedPickDefinitions = R"(
[effects(heap_alloc), return<vector<i32>>]
values_or_empty([i32] first) {
  return(pick(get_values(first)) { none { vector<i32>{} } some(values) { values } })
}

[effects(heap_alloc), return<vector<i32>>]
values_or([i32] first, [vector<i32>] fallback) {
  return(pick(get_values(first)) {
    none { fallback }
    some(values) { values }
  })
}

[effects(heap_alloc), return<String>]
line_or_zero([i32] id) {
  return(pick(get(id)) { none { label(0i32) } some(line) { line } })
}
)";

} // namespace

TEST_CASE("a pick expression is returned as a vector and as a String") {
  // An arm that yields a parameter of the definition returns a copy of it.
  runPickValueProgram("pick_value_returned",
                      pickValueProgram(ReturnedPickDefinitions,
                                       R"(
    [vector<i32>] found{values_or_empty(2i32)}
    print_line(found[1i32])
    [vector<i32>] empty{values_or_empty(-1i32)}
    print_line(empty.count())
    [vector<i32>] fallback{make_values(8i32)}
    [vector<i32>] copied{values_or(-1i32, fallback)}
    print_line(copied[0i32] + fallback[1i32])
    [vector<i32>] picked{values_or(5i32, fallback)}
    print_line(picked[0i32])
    print_line(first_byte(line_or_zero(3i32)))
    print_line(first_byte(line_or_zero(-1i32)))
)"),
                      "3\n0\n17\n5\n51\n48\n0\n0\n");
}

TEST_CASE("a pick expression assigned to a mut local replaces what it held") {
  // The arms may sit on the assignment's line or on lines of their own; an arm that yields a local of the
  // enclosing scope assigns a copy of it.
  runPickValueProgram("pick_value_assigned",
                      pickValueProgram("",
                                       R"(
    [String mut] kept{label(1i32)}
    kept = pick(get(3i32)) { none { label(0i32) } some(line) { line } }
    print_line(first_byte(kept))
    kept = pick(get(-1i32)) {
      none { label(0i32) }
      some(line) { line }
    }
    print_line(first_byte(kept))
    [String] spare{label(7i32)}
    kept = pick(get(-1i32)) { none { spare } some(line) { line } }
    print_line(first_byte(kept) + first_byte(spare))
    [vector<i32> mut] items{make_values(1i32)}
    items = pick(get_values(5i32)) { none { vector<i32>{} } some(values) { values } }
    print_line(items[0i32])
    items = pick(get_values(-1i32)) { none { vector<i32>{} } some(values) { values } }
    print_line(items.count())
    [vector<i32>] others{make_values(8i32)}
    items = pick(get_values(-1i32)) { none { others } some(values) { values } }
    items.push(1i32)
    print_line(items.count() + others.count())
    [i32 mut] number{1i32}
    number = pick(get(4i32)) { none { 0i32 } some(line) { first_byte(line) } } + 1i32
    print_line(number)
)"),
                      "51\n48\n110\n5\n0\n5\n53\n0\n0\n");
}

TEST_CASE("a returned pick expression with integer arms is not a vector") {
  const std::string mismatch = rejectedPickValueProgram("pick_value_returned_mismatch",
                                                        R"(
[effects(heap_alloc), return<vector<i32>>]
values_or_empty([i32] first) {
  return(pick(get_values(first)) { none { 0i32 } some(values) { values.count() } })
}
)",
                                                        R"(
    print_line(values_or_empty(1i32).count())
)");
  CHECK_MESSAGE(mismatch.find("return type mismatch") != std::string::npos, mismatch);
}

namespace {

// `Vector<T>` is the stdlib struct; `count()` on it is the stdlib helper, not the builtin vector count.
const std::string StdlibVectorDefinitions = R"(
[effects(heap_alloc), return<Maybe<Vector<i32>>>]
get_stdlib([i32] first) {
  if(first < 0i32) {
    return(none<Vector<i32>>())
  }
  return(some<Vector<i32>>(vector<i32>(first, 2i32)))
}
)";

} // namespace

TEST_CASE("count and an index read a local and a pick payload declared Vector") {
  runPickValueProgram("pick_value_stdlib_vector_count",
                      pickValueProgram(StdlibVectorDefinitions,
                                       R"(
    [Vector<i32>] local{vector<i32>(1i32, 2i32)}
    print_line(local.count())
    print_line(local[1i32])
    print_line(local.capacity())
    pick(get_stdlib(5i32)) {
      none { print_line(0i32) }
      some(values) {
        print_line(values.count())
        print_line(values[1i32])
      }
    }
    [Maybe<Vector<i32>>] held{get_stdlib(6i32)}
    pick(held) {
      none { print_line(0i32) }
      some(values) { print_line(values.count() + values[0i32]) }
    }
)"),
                      "2\n2\n2\n2\n2\n8\n0\n0\n");
}

TEST_CASE("a method a Vector local does not have is an unknown call target") {
  const std::string missing = rejectedPickValueProgram("pick_value_stdlib_vector_missing", "", R"(
    [Vector<i32>] local{vector<i32>(1i32, 2i32)}
    print_line(local.missing())
)");
  CHECK_MESSAGE(missing.find("unknown call target: missing") != std::string::npos, missing);
}

TEST_SUITE_END();
