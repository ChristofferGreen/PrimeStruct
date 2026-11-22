#include "program_matrix.h"

#include <string>

TEST_SUITE_BEGIN("primestruct.program_matrix.pick_value_arms");

// Each arm of a `pick` expression is a scope of its own, like an arm of the statement form: the locals an arm
// declares are destroyed where the arm ends (after its value is computed), the local the arm ends with is taken
// over by the result, and no later exit reads the drop flag of an arm that never ran. That flag is stale stack
// memory on the native backend, which does not zero a frame. `Noisy` prints its id when destroyed, so the order
// is the output.

namespace {

const std::string NoisyPrelude = R"(
import /std/collections/*
import /std/text/*
import /std/maybe/*

[struct]
Noisy() {
  [i32 mut] id{0i32}
  [i64 mut] pad{0i64}

  [public effects(io_out)]
  Destroy() {
    print_line(this.id)
  }
}

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

[effects(heap_alloc), return<vector<i32>>]
make_values([i32] first) {
  [vector<i32> mut] values{vector<i32>{}}
  values.push(first)
  values.push(first + 1i32)
  return(values)
}
)";

void runHeapProgram(const std::string &name,
                    const std::string &definitions,
                    const std::string &body,
                    const std::string &stdoutText) {
  program_matrix::ProgramCase program;
  program.name = name;
  program.source = NoisyPrelude + definitions + R"(
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

TEST_CASE("the locals of a pick expression arm are destroyed where the arm ends, on every config") {
  // No heap statistics, so native runs it too. The arm that did not run leaves nothing for the exits after the
  // `pick` to destroy: `inner` (2) is destroyed before `print_line(r)`, not at the exit of the body.
  program_matrix::ProgramCase program;
  program.name = "pick_value_arm_scopes";
  program.extraConfigs = {"wasm-wasi"};
  program.source = NoisyPrelude + R"(
[effects(io_out)]
note([i32] value) {
  print_line(value)
}

[effects(io_out) return<i32>]
value_arms([Maybe<i32>] m, [i32] extra) {
  [Noisy] outer{Noisy{1i32, 0i64}}
  [i32] r{pick(m) {
    none {
      [Noisy] inner{Noisy{2i32, 0i64}}
      note(100i32)
      7i32
    }
    some(v) {
      [Noisy] other{Noisy{3i32, 0i64}}
      v + 1i32
    }
  }}
  print_line(r)
  if(extra > 0i32) {
    return(r)
  }
  return(r + 100i32)
}

[effects(io_out) return<int>]
main() {
  [Maybe<i32>] e{}
  [Maybe<i32>] s{[some] 4i32}
  print_line(value_arms(e, 0i32))
  print_line(value_arms(e, 1i32))
  print_line(value_arms(s, 0i32))
  return(0i32)
}
)";
  program.exitCode = 0;
  program.stdoutText = "100\n2\n7\n1\n107\n100\n2\n7\n1\n7\n3\n5\n1\n105\n";
  program_matrix::runProgramMatrix(program);
}

TEST_CASE("nested pick expressions destroy each arm's locals where that arm ends, native included") {
  program_matrix::ProgramCase program;
  program.name = "pick_value_arm_nested";
  program.extraConfigs = {"wasm-wasi"};
  program.source = NoisyPrelude + R"(
[effects(io_out) return<i32>]
nested([Maybe<i32>] a, [Maybe<i32>] b) {
  [Noisy] outer{Noisy{10i32, 0i64}}
  [i32] r{pick(a) {
    none {
      [Noisy] first{Noisy{11i32, 0i64}}
      0i32
    }
    some(x) {
      [Noisy] second{Noisy{12i32, 0i64}}
      pick(b) {
        none {
          [Noisy] third{Noisy{13i32, 0i64}}
          x
        }
        some(y) {
          [Noisy] fourth{Noisy{14i32, 0i64}}
          x + y
        }
      }
    }
  }}
  print_line(r)
  return(r)
}

[effects(io_out) return<int>]
main() {
  [Maybe<i32>] e{}
  [Maybe<i32>] one{[some] 1i32}
  [Maybe<i32>] two{[some] 2i32}
  print_line(nested(e, e))
  print_line(nested(one, e))
  print_line(nested(one, two))
  return(0i32)
}
)";
  program.exitCode = 0;
  program.stdoutText = "11\n0\n10\n0\n13\n12\n1\n10\n1\n14\n12\n3\n10\n3\n";
  program_matrix::runProgramMatrix(program);
}

TEST_CASE("a String local that ends a pick expression arm is taken over and the others are destroyed") {
  runHeapProgram("pick_value_arm_strings",
                 R"(
[effects(heap_alloc), return<i32>]
text_arms([Maybe<i32>] m, [i32] extra) {
  [String] outer{label(1i32)}
  [String] r{pick(m) {
    none {
      [String] inner{label(2i32)}
      [String mut] built{label(3i32)}
      built.pushByte(convert<u8>(53i32))
      built
    }
    some(v) {
      [String] spare{label(9i32)}
      [String] other{label(v)}
      other
    }
  }}
  if(extra > 0i32) {
    return(first_byte(r) + first_byte(outer))
  }
  return(first_byte(r))
}
)",
                 R"(
    [Maybe<i32>] e{}
    [Maybe<i32>] s{[some] 6i32}
    print_line(text_arms(e, 0i32))
    print_line(text_arms(e, 1i32))
    print_line(text_arms(s, 0i32))
    print_line(text_arms(s, 1i32))
)",
                 "51\n100\n54\n103\n");
}

TEST_CASE("a vector local that ends a pick expression arm is taken over, with an early return after") {
  runHeapProgram("pick_value_arm_vectors",
                 R"(
[effects(heap_alloc), return<i32>]
vector_arms([Maybe<i32>] m, [i32] extra) {
  [vector<i32>] outer{make_values(20i32)}
  [vector<i32>] r{pick(m) {
    none {
      make_values(1i32)
    }
    some(v) {
      [vector<i32> mut] b{make_values(v)}
      [vector<i32>] spare{make_values(2i32)}
      [String] text{label(3i32)}
      b
    }
  }}
  if(extra > 0i32) {
    return(r.count() + outer[0i32])
  }
  return(r.count() + r[0i32])
}
)",
                 R"(
    [Maybe<i32>] e{}
    [Maybe<i32>] s{[some] 4i32}
    print_line(vector_arms(e, 0i32))
    print_line(vector_arms(e, 1i32))
    print_line(vector_arms(s, 0i32))
    print_line(vector_arms(s, 1i32))
)",
                 "3\n22\n6\n22\n");
}

TEST_CASE("a unit arm with owning locals ends with a vector, balanced") {
  // The unit arm `none { ... }` is read as a binding whose initializer is a block: its type is the type of the
  // vector local the block ends with (TODO-6024), and the result takes that local over.
  runHeapProgram("pick_value_unit_arm_vectors",
                 R"(
[effects(heap_alloc), return<i32>]
unit_arm_vectors([Maybe<i32>] m, [i32] extra) {
  [vector<i32>] outer{make_values(20i32)}
  [vector<i32>] r{pick(m) {
    none {
      [vector<i32> mut] a{make_values(1i32)}
      [vector<i32>] spare{make_values(2i32)}
      [String] text{label(3i32)}
      a
    }
    some(v) {
      make_values(v)
    }
  }}
  if(extra > 0i32) {
    return(r.count() + outer[0i32])
  }
  return(r.count() + r[0i32])
}
)",
                 R"(
    [Maybe<i32>] e{}
    [Maybe<i32>] s{[some] 4i32}
    print_line(unit_arm_vectors(e, 0i32))
    print_line(unit_arm_vectors(e, 1i32))
    print_line(unit_arm_vectors(s, 0i32))
    print_line(unit_arm_vectors(s, 1i32))
)",
                 "3\n22\n6\n22\n");
}

TEST_CASE("a unit arm with a vector local runs on every config") {
  program_matrix::ProgramCase program;
  program.name = "pick_value_unit_arm_vector_native";
  program.extraConfigs = {"wasm-wasi"};
  program.source = NoisyPrelude + R"(
[effects(heap_alloc), return<i32>]
unit_arm([Maybe<i32>] m) {
  [vector<i32>] r{pick(m) {
    none {
      [vector<i32>] a{make_values(1i32)}
      a
    }
    some(v) {
      make_values(v)
    }
  }}
  return(r.count() + r[0i32])
}

[effects(heap_alloc, io_out), return<int>]
main() {
  [Maybe<i32>] e{}
  [Maybe<i32>] s{[some] 4i32}
  print_line(unit_arm(e))
  print_line(unit_arm(s))
  return(0i32)
}
)";
  program.exitCode = 0;
  program.stdoutText = "3\n6\n";
  program_matrix::runProgramMatrix(program);
}
