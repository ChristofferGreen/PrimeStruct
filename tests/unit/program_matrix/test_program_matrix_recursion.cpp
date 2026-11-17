#include "program_matrix.h"

#include <string>

TEST_SUITE_BEGIN("primestruct.program_matrix.recursion");

// A recursive definition runs on the VM only as a real call, and the address ABI passes no builtin vector or reference
// to a vector (TODO-6012): the definition stays inline and the recursion is rejected. The diagnostic names the
// parameter that kept it inline and the rule, so the author knows which parameter to change.

namespace {

void runRejectedRecursion(const std::string &name,
                          const std::string &parameter,
                          const std::string &reason) {
  program_matrix::ProgramCase program;
  program.name = name;
  program.source = R"(
import /std/collections/vector/*

[return<void> effects(heap_alloc)]
walk([i32] depth, )" +
                   parameter + R"() {
  if(depth > 0i32) {
    out.push(depth)
    walk(depth - 1i32, out)
  }
}

[return<int> effects(heap_alloc)]
main() {
  [vector<i32> mut] out{vector<i32>{}}
  walk(3i32, out)
  return(out.count())
}
)";
  program.exitCode = 2;
  program.stdoutText = "";
  program.vmStderr = "VM lowering error: vm backend does not support recursive calls: /walk (it "
                     "stays inline because "
                     "parameter `out` " +
                     reason + ")\n";
  program.onlyConfigs = {"vm-step-O0", "vm-O0", "vm-O2"};
  program_matrix::runProgramMatrix(program);
}

} // namespace

TEST_CASE("a recursive definition with a vector parameter says the vector kept it inline") {
  runRejectedRecursion(
      "recursion_vector_parameter",
      "[vector<i32> mut] out",
      "is a `vector<i32>`: only a spawned call passes a builtin vector, a direct call keeps its "
      "definition inline");
}

TEST_CASE("a recursive definition with a reference to a vector says the templated target kept it "
          "inline") {
  runRejectedRecursion("recursion_reference_to_vector_parameter",
                       "[Reference<vector<i32>> mut] out",
                       "is a `Reference<vector<i32>>`: the address ABI passes a reference or "
                       "pointer to a scalar or a "
                       "struct, not to a templated type such as a vector");
}

TEST_SUITE_END();
