#include "program_matrix.h"

#include <filesystem>
#include <fstream>
#include <string>

TEST_SUITE_BEGIN("primestruct.program_matrix.pick_returns");

// `return` inside an arm of a `pick` statement leaves the function, for a unit arm (`none { return(-1i32) }`,
// which the parser reads as a value block) as for a payload arm; in value position the same `return` is the
// value of the arm. Every program runs through each execution config.

namespace {

const std::string MaybePrelude = R"(
import /std/maybe/*
)";

const std::string OwnerPrelude = R"(
import /std/text/*
import /std/maybe/*
import /std/result/*

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

[effects(heap_alloc), return<int>]
leave_local([i32] id) {
  [Maybe<String>] held{get(id)}
  pick(held) {
    none {
      return(-1i32)
    }
    some(text) {
      return(first_byte(text))
    }
  }
  return(0i32)
}

[effects(heap_alloc), return<int>]
leave_temporary([i32] id) {
  pick(get(id)) {
    none {
      return(-2i32)
    }
    some(text) {
      return(first_byte(text))
    }
  }
  return(0i32)
}

[effects(heap_alloc), return<int>]
leave_after_binding([i32] id) {
  [Maybe<String>] held{get(id)}
  [String] extra{label(8i32)}
  pick(held) {
    none {
      [String] inner{label(7i32)}
      return(first_byte(inner) + first_byte(extra))
    }
    some(text) {
      [String] inner{label(6i32)}
      return(first_byte(text) + first_byte(inner))
    }
  }
  return(0i32)
}
)";

void runPickReturnProgram(const std::string &name, const std::string &source, int exitCode) {
  program_matrix::ProgramCase program;
  program.name = name;
  program.source = source;
  program.exitCode = exitCode;
  // Wasm runs these programs; the others use opcodes it lacks.
  static const std::vector<std::string> wasmPrograms = {"pick_return_unit_and_payload",
                                                        "pick_return_result_ok_arm",
                                                        "pick_return_nested",
                                                        "pick_return_loop_and_value",
                                                        "pick_return_after_binding"};
  if (program_matrix::listed(wasmPrograms, name)) {
    program.extraConfigs = {"wasm-wasi"};
  }
  program_matrix::runProgramMatrix(program);
}

program_matrix::Outcome compilePickReturnProgram(const std::string &name, const std::string &source) {
  const std::filesystem::path dir =
      primec::testing::testScratchPath("program_matrix/" + name + "/x").parent_path();
  std::filesystem::create_directories(dir);
  const std::filesystem::path sourcePath = dir / (name + ".prime");
  {
    std::ofstream file(sourcePath, std::ios::binary);
    file << source;
  }
  return program_matrix::runCaptured("./primec --emit=ir " + program_matrix::quote(sourcePath.string()) +
                                         " -o " + program_matrix::quote((dir / (name + ".psir")).string()),
                                     dir / (name + ".ir"));
}

} // namespace

TEST_CASE("a unit arm and a payload arm of a pick statement return from the function") {
  // 5 + (-1) is 4; before the fix the unit arm fell through to the `return(99i32)`, which made it 104.
  runPickReturnProgram("pick_return_unit_and_payload", MaybePrelude + R"(
[return<int>]
leave([Maybe<i32>] m) {
  pick(m) {
    none {
      return(-1i32)
    }
    some(v) {
      return(v)
    }
  }
  return(99i32)
}

[return<int>]
main() {
  [Maybe<i32>] present{[some] 5i32}
  [Maybe<i32>] absent{}
  return(leave(present) + leave(absent))
}
)",
                       4);
}

TEST_CASE("an ok arm of a status-only Result returns from the function") {
  // status(1) is 7 through the ok arm; status(4) and status(9) are the error values 4 and 9: 20.
  runPickReturnProgram("pick_return_result_ok_arm", R"(
import /std/result/*

[return<Result<i32>>]
check([i32] x) {
  if(x > 3i32) {
    [Result<i32>] bad{[error] x}
    return(bad)
  }
  return(/std/result/ok<i32>())
}

[return<int>]
status([i32] x) {
  pick(check(x)) {
    ok {
      return(7i32)
    }
    error(e) {
      return(e)
    }
  }
  return(0i32)
}

[return<int>]
main() {
  return(status(1i32) + status(4i32) + status(9i32))
}
)",
                       20);
}

TEST_CASE("a return in a nested pick leaves every enclosing pick") {
  // nested(e, e) is 1, nested(e, s) is 4, nested(s, e) is 14, nested(s, s) is 14: 33.
  runPickReturnProgram("pick_return_nested", MaybePrelude + R"(
[return<int>]
nested([Maybe<i32>] a, [Maybe<i32>] b) {
  pick(a) {
    none {
      pick(b) {
        none {
          return(1i32)
        }
        some(y) {
          return(y)
        }
      }
      return(50i32)
    }
    some(x) {
      return(x + 10i32)
    }
  }
  return(99i32)
}

[return<int>]
main() {
  [Maybe<i32>] e{}
  [Maybe<i32>] s{[some] 4i32}
  return(nested(e, e) + nested(e, s) + nested(s, e) + nested(s, s))
}
)",
                       33);
}

TEST_CASE("a return in a loop inside a pick arm leaves the function, and a pick value keeps its arm value") {
  // loop_find(e) is 102, loop_find(s) is 204, value_pick(e) is 1003 and value_pick(s) is 1004; the sum less
  // 2300 is 13.
  runPickReturnProgram("pick_return_loop_and_value", MaybePrelude + R"(
[return<int>]
loop_find([Maybe<i32>] m) {
  [i32 mut] i{0i32}
  while(i < 5i32) {
    pick(m) {
      none {
        if(i == 2i32) {
          return(100i32 + i)
        }
      }
      some(v) {
        if(i == 3i32) {
          return(200i32 + v)
        }
      }
    }
    i = i + 1i32
  }
  return(7i32)
}

[return<int>]
value_pick([Maybe<i32>] m) {
  [i32] r{pick(m) {
    none {
      return(3i32)
    }
    some(v) {
      return(v)
    }
  }}
  return(r + 1000i32)
}

[return<int>]
main() {
  [Maybe<i32>] e{}
  [Maybe<i32>] s{[some] 4i32}
  [i32] total{loop_find(e) + loop_find(s) + value_pick(e) + value_pick(s)}
  return(total - 2300i32)
}
)",
                       13);
}

TEST_CASE("a return from a unit arm with a binding before it leaves the function") {
  // The unit arm's own local is the second statement of the arm; the payload arm reads its payload.
  runPickReturnProgram("pick_return_after_binding", MaybePrelude + R"(
[return<int>]
leave([Maybe<i32>] m) {
  pick(m) {
    none {
      [i32] base{20i32}
      return(base + 1i32)
    }
    some(v) {
      [i32] base{30i32}
      return(base + v)
    }
  }
  return(99i32)
}

[return<int>]
main() {
  [Maybe<i32>] e{}
  [Maybe<i32>] s{[some] 4i32}
  return(leave(e) + leave(s))
}
)",
                       55);
}

TEST_CASE("pick arms that return owned payloads read the right values on every backend") {
  // 49 + 50 + 105 + 111 less 300 is 15, and the two empty cases give -1 and -2: 12. The VM and optexe check
  // the heap statistics in the next case; native has no string heap statistics.
  runPickReturnProgram("pick_return_owned_values", OwnerPrelude + R"(
[effects(heap_alloc), return<int>]
main() {
  [i32] sum{leave_local(1i32) + leave_temporary(2i32) + leave_after_binding(3i32) + leave_after_binding(-1i32)}
  return(sum - 100i32 * 3i32 + leave_local(-1i32) + leave_temporary(-1i32))
}
)",
                       12);
}

TEST_CASE("pick arms that return owned payloads destroy each payload exactly once") {
  program_matrix::ProgramCase program;
  program.name = "pick_return_owned_balanced";
  program.source = OwnerPrelude + R"(
[effects(heap_alloc, io_out), return<int>]
main() {
  [i32] blocks{heapLiveBlocks()}
  [u64] bytes{stringHeapBytesInUse()}
  print_line(leave_local(1i32))
  print_line(leave_local(-1i32))
  print_line(leave_temporary(2i32))
  print_line(leave_temporary(-1i32))
  print_line(leave_after_binding(3i32))
  print_line(leave_after_binding(-1i32))
  print_line(heapLiveBlocks() - blocks)
  print_line(stringHeapBytesInUse() - bytes)
  return(0i32)
}
)";
  program.exitCode = 0;
  program.stdoutText = "49\n-1\n50\n-2\n105\n111\n0\n0\n";
  // The heap statistics exist on the VM and optexe only.
  program.onlyConfigs = {"vm-step-O0", "vm-O0", "vm-O2", "optexe-O2"};
  program_matrix::runProgramMatrix(program);
}

TEST_CASE("a unit arm that returns a value of the wrong type is rejected") {
  const program_matrix::Outcome outcome = compilePickReturnProgram("pick_return_unit_mismatch", MaybePrelude + R"(
[return<int>]
leave([Maybe<i32>] m) {
  pick(m) {
    none {
      return(true)
    }
    some(v) {
      return(v)
    }
  }
  return(0i32)
}

[return<int>]
main() {
  [Maybe<i32>] e{}
  return(leave(e))
}
)");
  CHECK(outcome.exitCode != 0);
  CHECK_MESSAGE(outcome.err.find("return type mismatch") != std::string::npos, outcome.err);
}

TEST_SUITE_END();
