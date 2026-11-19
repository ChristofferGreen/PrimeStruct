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

TEST_CASE("an if yields struct values in a binding, a return and an argument") {
  program_matrix::ProgramCase program;
  program.name = "frontend_if_yields_struct";
  program.source = R"(
[struct]
Point {
  [i32] x
  [i32] y
}

[return<i32>]
sum([Point] p) {
  return(plus(p.x, p.y))
}

[return<Point>]
pick_point([bool] flag) {
  [Point] a{Point{1i32, 2i32}}
  [Point] b{Point{10i32, 20i32}}
  return(if(flag) { a } else { b })
}

[return<int> effects(io_out)]
main() {
  [bool] yes{true}
  [bool] no{false}
  [Point] p{if(yes) { Point{3i32, 4i32} } else { Point{5i32, 6i32} }}
  [Point] q{if(no) { Point{3i32, 4i32} } else { Point{5i32, 6i32} }}
  print_line(p.x)
  print_line(q.y)
  print_line(sum(if(no) { p } else { q }))
  [Point] r{pick_point(no)}
  print_line(r.y)
  return(0i32)
}
)";
  program.exitCode = 0;
  program.stdoutText = "3\n6\n11\n20\n";
  program_matrix::runProgramMatrix(program);
}

TEST_CASE("an if of struct temporaries with a Destroy helper destroys the chosen value once") {
  program_matrix::ProgramCase program;
  program.name = "frontend_if_struct_destroy";
  program.source = R"(
[struct]
Res {
  [i32] id

  Destroy() {
    print_line(this.id)
  }
}

[return<int> effects(io_out)]
main() {
  [bool] yes{true}
  [bool] no{false}
  [Res] first{if(yes) { Res{1i32} } else { Res{2i32} }}
  [Res] second{if(no) { Res{3i32} } else { Res{4i32} }}
  print_line(100i32)
  return(0i32)
}
)";
  program.exitCode = 0;
  // Locals are destroyed last declared first.
  program.stdoutText = "100\n4\n1\n";
  program_matrix::runProgramMatrix(program);
}

TEST_CASE("an if whose branches are different struct types is still rejected") {
  program_matrix::ProgramCase program;
  program.name = "frontend_if_struct_mismatch";
  program.source = R"(
[struct]
Point {
  [i32] x
}

[struct]
Other {
  [i32] x
}

[return<int>]
main() {
  [bool] yes{true}
  [Point] p{if(yes) { Point{3i32} } else { Other{5i32} }}
  return(p.x)
}
)";
  program.exitCode = 2;
  program.stdoutText = "";
  program.vmStderr = "{program}:15:13: error: VM lowering error: if branches must return "
                     "compatible types [PSC2001]\n"
                     "note: backend: vm\n";
  program.onlyConfigs = {"vm-step-O0"};
  program_matrix::runProgramMatrix(program);
}

TEST_CASE("a struct holds sum fields: read, pick, replace and copy") {
  program_matrix::ProgramCase program;
  program.name = "frontend_sum_struct_field";
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

[struct]
Holder {
  [i32] before
  [Shape mut] shape
  [Maybe<i32> mut] extra
  [i32] after
}

[return<i32>]
area([Shape] shape) {
  return(pick(shape) {
    none { 0i32 }
    circle(c) { c.radius * 3i32 }
    side(n) { n * n }
  })
}

[return<Holder>]
make([i32] k) {
  return(Holder{1i32, Shape{[circle] Circle{k}}, Maybe<i32>{[some] 7i32}, 2i32})
}

[return<int> effects(io_out)]
main() {
  [Holder mut] h{make(5i32)}
  print_line(h.before)
  print_line(area(h.shape))
  print_line(pick(h.extra) { none { 0i32 } some(v) { v } })
  print_line(h.after)
  [Holder] copy{h}
  h.shape = Shape{[side] 4i32}
  print_line(area(h.shape))
  print_line(area(copy.shape))
  h.extra = Maybe<i32>{none}
  print_line(pick(h.extra) { none { 0i32 } some(v) { v } })
  print_line(pick(copy.extra) { none { 0i32 } some(v) { v } })
  return(0i32)
}
)";
  program.exitCode = 0;
  program.stdoutText = "1\n15\n7\n2\n16\n15\n0\n7\n";
  program_matrix::runProgramMatrix(program);
}

TEST_CASE("a sum field destroys the owning payload it replaces and the one it ends with") {
  program_matrix::ProgramCase program;
  program.name = "frontend_sum_struct_field_destroy";
  program.source = R"(
[struct]
Res {
  [i32] id

  Destroy() {
    print_line(this.id)
  }
}

[sum]
Slot {
  none
  [Res] res
}

[struct]
Box {
  [Slot mut] slot
}

[return<int> effects(io_out)]
main() {
  [Box mut] box{Box{Slot{[res] Res{1i32}}}}
  print_line(100i32)
  box.slot = Slot{[res] Res{2i32}}
  print_line(200i32)
  return(0i32)
}
)";
  program.exitCode = 0;
  program.stdoutText = "100\n1\n200\n2\n";
  program_matrix::runProgramMatrix(program);
}

TEST_SUITE_END();
