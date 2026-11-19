#include "program_matrix.h"

TEST_SUITE_BEGIN("primestruct.program_matrix.runtime_faults");

// Integer division by zero is a runtime fault on every backend: the message goes to stderr, the program exits with
// code 3, and what it printed before the fault is not lost (docs/spec/vm-design.md, Errors). The divisor is a `mut`
// local so no pass folds the division away.

namespace {

const std::string DivisionMessage = "VM error: division by zero in IR\n";

void runDivisionFault(const std::string &name, const std::string &type, const std::string &literal) {
  program_matrix::ProgramCase program;
  program.name = name;
  program.source = R"(
[effects(io_out), return<int>]
main() {
  print_line("before")
  [)" + type + R"( mut] zero{0)" + type + R"(}
  [)" + type + R"(] value{)" + literal + R"(}
  print_line(value / zero)
  print_line("after")
  return(0i32)
}
)";
  program.exitCode = 3;
  program.stdoutText = "before\n";
  program.vmStderr = DivisionMessage;
  program.nativeStderr = DivisionMessage;
  program.cppStderr = DivisionMessage;
  program_matrix::runProgramMatrix(program);
}

} // namespace

TEST_CASE("i32 division by zero faults with exit 3 and keeps earlier output") {
  runDivisionFault("fault_div_zero_i32", "i32", "10i32");
}

TEST_CASE("i64 division by zero faults with exit 3 and keeps earlier output") {
  runDivisionFault("fault_div_zero_i64", "i64", "10i64");
}

TEST_CASE("u64 division by zero faults with exit 3 and keeps earlier output") {
  runDivisionFault("fault_div_zero_u64", "u64", "10u64");
}

TEST_CASE("a division by a non-zero value and a float division by zero do not fault") {
  program_matrix::ProgramCase program;
  program.name = "fault_div_no_fault";
  program.source = R"(
[effects(io_out), return<int>]
main() {
  [i32 mut] divisor{3i32}
  [i64 mut] minusOne{-1i64}
  [f64 mut] zero{0.0f64}
  print_line(10i32 / divisor)
  print_line(7i64 / minusOne)
  print_line(1.0f64 / zero > 1000000.0f64)
  return(0i32)
}
)";
  program.exitCode = 0;
  program.stdoutText = "3\n-7\n1\n";
  program_matrix::runProgramMatrix(program);
}

TEST_SUITE_END();
