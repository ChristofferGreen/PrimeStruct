#include "program_matrix.h"

TEST_SUITE_BEGIN("primestruct.program_matrix.pick_subjects");

// A `pick` on a temporary (a call result, a constructor) owns it: the active payload is destroyed
// when the `pick` ends, unless an arm moved it out, and then it is destroyed once by its new owner.
// Every program prints what the arms read and then how many heap blocks and string-heap bytes are
// still in use; a payload destroyed twice ends the program with `string heap block freed twice`.

namespace {

// `label(id)` is one byte, the digit `id`; `get(id)` is `none` below zero.
const std::string PickSubjectPrelude = R"(
import /std/text/*
import /std/maybe/*
import /std/result/*
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
)";

// Wraps `body` between two readings of the heap statistics and prints the differences.
std::string pickSubjectProgram(const std::string &definitions, const std::string &body) {
  return PickSubjectPrelude + definitions + R"(
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

void runPickSubjectProgram(const std::string &name,
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
program_matrix::Outcome compilePickProgram(const std::string &name, const std::string &source) {
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

// A struct with a method, and a user sum with a variant of that struct.
const std::string BoxDefinitions = R"(
[struct]
Box {
  [i32] value{3i32}

  [return<i32>]
  twice() {
    return(this.value * 2i32)
  }
}

[sum]
Shape {
  [Box] boxed
  [i32] plain
}
)";

} // namespace

TEST_CASE("pick on a call result frees a String payload") {
  const std::string source = pickSubjectProgram(R"(
[struct]
Source {
  [i32] base{0i32}

  [effects(heap_alloc), return<Maybe<String>>]
  next([i32] id) {
    return(get(this.base + id))
  }
}
)",
                                                R"(
    pick(get(3i32)) {
      none {
      }
      some(line) {
        print_line(first_byte(line))
      }
    }
    [i32 mut] i{0i32}
    [i32 mut] total{0i32}
    while(i < 4i32) {
      pick(get(i - 1i32)) {
        none {
          total = total + 1000i32
        }
        some(line) {
          total = total + first_byte(line)
        }
      }
      i = i + 1i32
    }
    print_line(total)
    [i32] chosen{pick(get(5i32)) { none { 0i32 } some(line) { first_byte(line) } }}
    print_line(chosen)
    [Source] source{Source{[base] 1i32}}
    pick(source.next(3i32)) {
      none {
      }
      some(line) {
        print_line(first_byte(line))
      }
    }
    pick(some<String>(label(7i32))) {
      none {
      }
      some(line) {
        print_line(first_byte(line))
      }
    })");
  runPickSubjectProgram("pick_subject_string", source, "51\n1147\n53\n52\n55\n0\n0\n");
}

TEST_CASE("pick on a call result frees struct, vector and user sum payloads") {
  const std::string source = pickSubjectProgram(R"(
[struct]
Named {
  [String] name{String{}}
  [i32] id{0i32}
}

[effects(heap_alloc), return<Maybe<Named>>]
get_named([i32] id) {
  if(id < 0i32) {
    return(none<Named>())
  }
  return(some<Named>(Named{[name] label(id), [id] id}))
}

[effects(heap_alloc), return<Maybe<vector<i32>>>]
get_values([i32] first) {
  if(first < 0i32) {
    return(none<vector<i32>>())
  }
  [vector<i32> mut] values{vector<i32>{}}
  values.push(first)
  values.push(first + 1i32)
  return(some<vector<i32>>(values))
}

[sum]
Token {
  [String] word
  [i32] number
}

[effects(heap_alloc), return<Token>]
get_token([i32] id) {
  if(id < 0i32) {
    [Token] token{[number] id}
    return(token)
  }
  [Token] token{[word] label(id)}
  return(token)
}

[effects(heap_alloc, io_out), return<void>]
show_token([i32] id) {
  pick(get_token(id)) {
    word(text) {
      print_line(first_byte(text))
    }
    number(value) {
      print_line(value)
    }
  }
}
)",
                                                R"(
    pick(get_named(4i32)) {
      none {
      }
      some(named) {
        print_line(named.id)
      }
    }
    [i32] chosen{pick(get_named(5i32)) { none { 0i32 } some(named) { first_byte(named.name) } }}
    print_line(chosen)
    pick(get_values(4i32)) {
      none {
        print_line(0i32)
      }
      some(values) {
        print_line(1i32)
      }
    }
    [i32] found{pick(get_values(-1i32)) { none { 2i32 } some(values) { 3i32 } }}
    print_line(found)
    show_token(4i32)
    show_token(-4i32))");
  runPickSubjectProgram("pick_subject_payload_kinds", source, "4\n53\n1\n2\n52\n-4\n0\n0\n");
}

TEST_CASE("a payload moved out of a pick on a call result is destroyed once") {
  const std::string source = pickSubjectProgram(R"(
[effects(heap_alloc), return<String>]
take([i32] id) {
  pick(get(id)) {
    none {
    }
    some(line) {
      return(line)
    }
  }
  return(String{})
}
)",
                                                R"(
    [String] yielded{pick(get(3i32)) { none { String{} } some(line) { line } }}
    print_line(first_byte(yielded))
    [String] empty{pick(get(-1i32)) { none { String{} } some(line) { line } }}
    print_line(empty.count())
    [String] returned{take(4i32)}
    print_line(first_byte(returned))
    [String] missing{take(-1i32)}
    print_line(missing.count())
    pick(get(5i32)) {
      none {
      }
      some(taken) {
        [String] owned{move(taken)}
        print_line(first_byte(owned))
      }
    }
    pick(get(6i32)) {
      none {
      }
      some(line) {
        [String] copied{line}
        print_line(first_byte(copied))
      }
    })");
  runPickSubjectProgram("pick_subject_moved_out", source, "51\n0\n52\n0\n53\n54\n0\n0\n");
}

TEST_CASE("a payload moved out of a pick on a sum local is destroyed once") {
  const std::string source = pickSubjectProgram(R"(
[effects(heap_alloc), return<String>]
take([i32] id) {
  [Maybe<String>] got{get(id)}
  pick(got) {
    none {
    }
    some(line) {
      return(line)
    }
  }
  return(String{})
}
)",
                                                R"(
    [Maybe<String>] first{get(3i32)}
    [String] yielded{pick(first) { none { String{} } some(line) { line } }}
    print_line(first_byte(yielded))
    [String] returned{take(4i32)}
    print_line(first_byte(returned))
    [Maybe<String>] second{get(5i32)}
    pick(second) {
      none {
      }
      some(taken) {
        [String] owned{move(taken)}
        print_line(first_byte(owned))
      }
    }
    [Maybe<String>] third{get(6i32)}
    pick(third) {
      none {
      }
      some(line) {
        [String] copied{line}
        print_line(first_byte(copied))
      }
    })");
  runPickSubjectProgram("pick_local_moved_out", source, "51\n52\n53\n54\n0\n0\n");
}

TEST_CASE("pick on a call returning a Result frees the payload of either variant") {
  const std::string source = pickSubjectProgram(R"(
[effects(heap_alloc), return<Result<String, i32>>]
get_text([i32] id) {
  if(id < 0i32) {
    return(error<String, i32>(id))
  }
  return(ok<String, i32>(label(id)))
}

[effects(heap_alloc), return<Result<i32, String>>]
get_number([i32] id) {
  if(id < 0i32) {
    return(error<i32, String>(label(0i32 - id)))
  }
  return(ok<i32, String>(id))
}

[effects(heap_alloc, io_out), return<void>]
show_text([i32] id) {
  pick(get_text(id)) {
    ok(text) {
      print_line(first_byte(text))
    }
    error(code) {
      print_line(code)
    }
  }
}

[effects(heap_alloc, io_out), return<void>]
show_number([i32] id) {
  pick(get_number(id)) {
    ok(value) {
      print_line(value)
    }
    error(text) {
      print_line(first_byte(text))
    }
  }
}
)",
                                                R"(
    show_text(4i32)
    show_text(-4i32)
    show_number(5i32)
    show_number(-5i32)
    [String] kept{pick(get_text(6i32)) { ok(text) { text } error(code) { String{} } }}
    print_line(first_byte(kept)))");
  runPickSubjectProgram("pick_subject_result", source, "52\n-4\n5\n53\n54\n0\n0\n");
}

TEST_CASE("a method is called on a struct payload binding") {
  // No heap and no String, so this one runs on every backend.
  program_matrix::ProgramCase program;
  program.name = "pick_payload_struct_method";
  program.source = "import /std/maybe/*\n" + BoxDefinitions + R"(
[return<i32>]
measure([Shape] shape) {
  return(pick(shape) { boxed(inner) { inner.twice() } plain(number) { number } })
}

[effects(io_out), return<int>]
main() {
  [Maybe<Box>] held{[some] Box{}}
  pick(held) {
    none {
    }
    some(box) {
      print_line(box.twice())
    }
  }
  [Shape] shape{[boxed] Box{[value] 7i32}}
  print_line(measure(shape))
  [Shape] other{[plain] 5i32}
  print_line(measure(other))
  return(0i32)
}
)";
  program.exitCode = 0;
  program.stdoutText = "6\n14\n5\n";
  program_matrix::runProgramMatrix(program);
}

TEST_CASE("a method is called on a String payload binding and on a payload of a call result") {
  const std::string source = pickSubjectProgram(BoxDefinitions + R"(
[effects(heap_alloc), return<Maybe<Box>>]
get_box([i32] value) {
  return(some<Box>(Box{[value] value}))
}
)",
                                                R"(
    [Maybe<String>] text{get(5i32)}
    pick(text) {
      none {
      }
      some(line) {
        print_line(convert<i32>(line.byteAt(0i32)))
        print_line(line.count())
      }
    }
    pick(get(6i32)) {
      none {
      }
      some(line) {
        print_line(convert<i32>(line.byteAt(0i32)))
      }
    }
    [i32] doubled{pick(get_box(4i32)) { none { 0i32 } some(box) { box.twice() } }}
    print_line(doubled))");
  runPickSubjectProgram("pick_payload_string_method", source, "53\n1\n54\n8\n0\n0\n");
}

TEST_CASE("a method the payload's struct does not have is still unknown") {
  const program_matrix::Outcome outcome = compilePickProgram(
      "pick_payload_unknown_method", "import /std/maybe/*\n" + BoxDefinitions + R"(
[effects(io_out), return<int>]
main() {
  [Maybe<Box>] held{[some] Box{}}
  pick(held) {
    none {
    }
    some(box) {
      print_line(box.thrice())
    }
  }
  return(0i32)
}
)");
  CHECK(outcome.exitCode != 0);
  CHECK_MESSAGE(outcome.err.find("unknown method: /Box/thrice") != std::string::npos, outcome.err);
}

TEST_SUITE_END();
