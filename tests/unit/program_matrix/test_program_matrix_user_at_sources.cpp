#include "program_matrix.h"

TEST_SUITE_BEGIN("primestruct.program_matrix.user_at_sources");

// A call shaped like an element access (`maker.at(i)`, `at_unsafe(maker, i)`) is a place only when it is the
// builtin access; a user `at` that returns a value is a temporary, and every site that copies a struct value tells
// the two apart by resolving the call (docs/spec/value-lifecycle.md, Copies). Each program feeds such a result to
// one of those sites and checks that every value is destroyed exactly once: with Destroy helpers that print, and
// with the heap statistics, which a leaked or freed-twice String shows.

namespace {

std::vector<std::string> vmOptexeConfigs() {
  return {"vm-step-O0", "vm-O0", "vm-O2", "optexe-O2"};
}

std::vector<std::string> allBackendConfigs() {
  std::vector<std::string> configs = vmOptexeConfigs();
  if (program_matrix::nativeSupported()) {
    configs.push_back("native-O0");
    configs.push_back("native-O2");
  }
  return configs;
}

// `label(id)` is one byte, the digit `id`; `Box` holds one; `Maker.at` returns a new Box or Maybe each call.
const std::string HeapPrelude = R"(
import /std/text/*
import /std/maybe/*
import /std/collections/*

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
}

[struct]
Maker {
  [i32] base{1i32}

  [effects(heap_alloc), return<Box>]
  at([i32] i) {
    return(Box{[text] label(this.base + i)})
  }

  [effects(heap_alloc), return<Maybe<String>>]
  at_unsafe([i32] i) {
    return(some<String>(label(this.base + i)))
  }

  [effects(heap_alloc), return<Maybe<Box>>]
  tryAt([i32] i) {
    return(some<Box>(Box{[text] label(this.base + i)}))
  }
}

[effects(heap_alloc), return<i32>]
consume([Box] b) {
  return(first_byte(b.text))
}
)";

// Each `body` runs in a scope of its own between readings of the heap statistics, which are printed after it.
std::string heapProgram(const std::string &body) {
  return HeapPrelude + R"(
[effects(heap_alloc, io_out), return<int>]
main() {
  [Maker] maker{Maker{}}
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

void runHeapProgram(const std::string &name, const std::string &body, const std::string &stdoutText) {
  program_matrix::ProgramCase program;
  program.name = name;
  program.source = heapProgram(body);
  program.exitCode = 0;
  program.stdoutText = stdoutText;
  // The heap statistics exist on the VM and optexe only.
  program.onlyConfigs = vmOptexeConfigs();
  program_matrix::runProgramMatrix(program);
}

} // namespace

TEST_CASE("pick on a user at result destroys its payload once") {
  runHeapProgram("user_at_pick_subject", R"(
    pick(maker.at_unsafe(1i32)) {
      none {
      }
      some(line) {
        print_line(first_byte(line))
      }
    }
    pick(maker.tryAt(2i32)) {
      none {
      }
      some(box) {
        print_line(first_byte(box.text))
      }
    }
    pick(maker.at_unsafe(3i32)) {
      none {
      }
      some(line) {
        print_line(first_byte(line))
      }
    })",
                 "50\n51\n52\n0\n0\n");
}

TEST_CASE("a pick value arm that yields a user at result owns it") {
  runHeapProgram("user_at_pick_value_arm", R"(
    [i32] chosen{pick(maker.tryAt(1i32)) { none { 0i32 } some(box) { first_byte(box.text) } }}
    print_line(chosen)
    [Box] kept{pick(maker.tryAt(2i32)) { none { Box{} } some(box) { maker.at(3i32) } }}
    print_line(first_byte(kept.text))
  )",
                 "50\n52\n0\n0\n");
}

TEST_CASE("a user at result feeds a parameter and a sum payload once") {
  runHeapProgram("user_at_arguments_and_payloads", R"(
    print_line(consume(maker.at(1i32)))
    print_line(consume(maker.at(2i32)))
    [Maybe<Box>] kept{some<Box>(maker.at(5i32))}
    pick(kept) {
      none {
      }
      some(box) {
        print_line(first_byte(box.text))
      }
    }
  )",
                 "50\n51\n54\n0\n0\n");
}

TEST_CASE("a user at result feeds a parameter and a copy parameter as a temporary") {
  // `maker.at(i)` and `at(maker, i)` make a new Noisy: a parameter takes it over (no 1000 added by Copy) and
  // destroys it once, at the end of the callee, whether it borrows or copies.
  program_matrix::ProgramCase program;
  program.name = "user_at_noisy_parameters";
  program.source = R"(
[struct]
Noisy() {
  [i32 mut] id{0i32}
  [public] Copy([Reference<Self>] other) { assign(this.id, other.id + 1000i32) }
  [public effects(io_out)] Destroy() { print_line(this.id) }
}

[struct]
Maker() {
  [i32] base{10i32}

  [return<Noisy>]
  at([i32] i) {
    return(Noisy{this.base + i})
  }
}

[return<Noisy>]
at_unsafe([Maker] maker, [i32] i) {
  return(Noisy{maker.base + i})
}

[effects(io_out), return<i32>]
take([Noisy] n) {
  return(n.id)
}

[effects(io_out), return<i32>]
take_copy([Noisy copy] n) {
  return(n.id)
}

[effects(io_out), return<int>]
main() {
  [Maker] maker{Maker{}}
  print_line(take(maker.at(1i32)))
  print_line(100i32)
  print_line(take_copy(maker.at(2i32)))
  print_line(200i32)
  print_line(take(at_unsafe(maker, 3i32)))
  print_line(300i32)
  print_line(take_copy(at_unsafe(maker, 4i32)))
  print_line(400i32)
  return(0i32)
}
)";
  program.exitCode = 0;
  program.stdoutText = "11\n11\n100\n12\n12\n200\n13\n13\n300\n14\n14\n400\n";
  program.onlyConfigs = allBackendConfigs();
  program_matrix::runProgramMatrix(program);
}

TEST_SUITE_END();
