#include "program_matrix.h"

TEST_SUITE_BEGIN("primestruct.program_matrix.frontend_accept");

// Programs the front end once rejected although the specification makes them valid. Each runs through every
// execution config, so a regression in any stage shows here.

TEST_CASE("an unused wildcard import of a stdlib module is not an error") {
  program_matrix::ProgramCase program;
  program.name = "frontend_unused_stdlib_imports";
  program.source = R"(
import /std/result/*
import /std/maybe/*

[return<int> effects(io_out)]
main() {
  print_line(3i32)
  return(0i32)
}
)";
  program.exitCode = 0;
  program.stdoutText = "3\n";
  program_matrix::runProgramMatrix(program);
}

TEST_CASE("return of a moved local and of a constructor given a moved local") {
  program_matrix::ProgramCase program;
  program.name = "frontend_return_move";
  program.source = R"(
[struct]
Holder {
  [i32 mut] n
}

[return<Holder>]
from_local([i32] start) {
  [Holder mut] value{Holder{start}}
  value.n = value.n + 1i32
  return(move(value))
}

[return<Holder>]
from_scalar() {
  [i32 mut] n{5i32}
  return(Holder{move(n)})
}

[return<int> effects(io_out)]
main() {
  [Holder] first{from_local(3i32)}
  [Holder] second{from_scalar()}
  print_line(first.n)
  print_line(second.n)
  return(0i32)
}
)";
  program.exitCode = 0;
  program.stdoutText = "4\n5\n";
  program_matrix::runProgramMatrix(program);
}

TEST_CASE("a sum local takes a new variant by assign and by =") {
  program_matrix::ProgramCase program;
  program.name = "frontend_sum_local_assignment";
  program.source = R"(
import /std/maybe/*

[struct]
Circle {
  [i32] radius
}

[sum]
Shape {
  none
  [Circle] circle
  [i32] side
}

[effects(io_out)]
show([Shape] shape) {
  pick(shape) {
    none {
      print_line(0i32)
    }
    circle(c) {
      print_line(c.radius)
    }
    side(n) {
      print_line(n)
    }
  }
}

[return<int> effects(io_out)]
main() {
  [Shape mut] shape{Shape{none}}
  assign(shape, Shape{[circle] Circle{4i32}})
  show(shape)
  shape = Shape{[side] 3i32}
  show(shape)
  assign(shape, Shape{none})
  show(shape)
  shape = Shape{[circle] Circle{5i32}}
  show(shape)
  [Maybe<i32> mut] maybe{Maybe<i32>{none}}
  maybe = Maybe<i32>{[some] 6i32}
  print_line(pick(maybe) {
    none { 0i32 }
    some(value) { value }
  })
  return(0i32)
}
)";
  program.exitCode = 0;
  program.stdoutText = "4\n3\n0\n5\n6\n";
  program_matrix::runProgramMatrix(program);
}

TEST_SUITE_END();
