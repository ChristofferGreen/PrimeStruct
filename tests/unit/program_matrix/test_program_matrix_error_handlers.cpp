#include "program_matrix.h"

#include <string>

TEST_SUITE_BEGIN("primestruct.program_matrix.error_handlers");

// The `on_error` handler of a definition flows into every block nested in its body (if branches, loop bodies,
// statement blocks), and a block-level `[on_error<E, handler>] block() {...}` overrides it for that block and what is
// nested in it (docs/spec/errors-and-file-io.md, Local handlers). A `?` in a nested block runs the handler of the
// nearest enclosing block that has one, then leaves the definition like `return`: the owning locals of the nested
// blocks and of the body are destroyed exactly once, innermost first. `Noisy` prints its id when destroyed, so the
// order is the output.

namespace {

const std::string HandlerPrelude = R"(
import /std/result/*

[struct]
Noisy() {
  [i32 mut] id{0i32}

  [public effects(io_out)]
  Destroy() {
    print_line(this.id)
  }
}

[return<Result<i32, i32>>]
check([i32] value) {
  if(value > 2i32) {
    return(/std/result/error<i32, i32>(value + 4i32))
  }
  return(Result.ok(value))
}

[effects(io_out)]
on_outer([i32] err) {
  print_line(100i32 + err)
}

[effects(io_out)]
on_inner([i32] err) {
  print_line(200i32 + err)
}
)";

void runHandlerProgram(const std::string &name, const std::string &definitions, const std::string &mainBody,
                       const std::string &stdoutText) {
  program_matrix::ProgramCase program;
  program.name = name;
  program.source = HandlerPrelude + definitions + "\n[return<int> effects(io_out)]\nmain() {\n" + mainBody +
                   "  return(0i32)\n}\n";
  program.exitCode = 0;
  program.stdoutText = stdoutText;
  program_matrix::runProgramMatrix(program);
}

} // namespace

TEST_CASE("the handler of a definition flows into an if branch") {
  runHandlerProgram("handler_flows_into_if",
                    R"(
[return<i32> effects(io_out) on_error<i32, /on_outer>]
in_if([i32] value) {
  [Noisy] a{Noisy{1i32}}
  if(value > 0i32) {
    [Noisy] b{Noisy{2i32}}
    [i32] x{check(value)?}
    print_line(x)
  }
  return(0i32)
}
)",
                    "  print_line(in_if(1i32))\n  print_line(in_if(5i32))\n",
                    // 1: x, then b and a end. 5: the handler runs, then b and a are destroyed, the error code returns.
                    "1\n2\n1\n0\n109\n2\n1\n9\n");
}

TEST_CASE("the handler of a definition flows into a loop body") {
  runHandlerProgram("handler_flows_into_loop",
                    R"(
[return<i32> effects(io_out) on_error<i32, /on_outer>]
in_loop([i32] limit) {
  [Noisy] a{Noisy{1i32}}
  [i32 mut] i{0i32}
  while(i < limit) {
    [Noisy] b{Noisy{2i32}}
    [i32] x{check(i)?}
    print_line(x)
    i = i + 1i32
  }
  return(0i32)
}
)",
                    "  print_line(in_loop(2i32))\n  print_line(in_loop(9i32))\n",
                    "0\n2\n1\n2\n1\n0\n0\n2\n1\n2\n2\n2\n107\n2\n1\n7\n");
}

TEST_CASE("the handler of a definition flows into a plain block") {
  runHandlerProgram("handler_flows_into_block",
                    R"(
[return<i32> effects(io_out) on_error<i32, /on_outer>]
in_block([i32] value) {
  [Noisy] a{Noisy{1i32}}
  block() {
    [Noisy] b{Noisy{2i32}}
    [i32] x{check(value)?}
    print_line(x)
  }
  return(0i32)
}
)",
                    "  print_line(in_block(1i32))\n  print_line(in_block(5i32))\n",
                    "1\n2\n1\n0\n109\n2\n1\n9\n");
}

TEST_CASE("the handler of a definition flows into a pick arm") {
  runHandlerProgram("handler_flows_into_pick_arm",
                    R"(
import /std/maybe/*

[return<Maybe<i32>>]
get([i32] value) {
  return(some<i32>(value))
}

[return<i32> effects(io_out) on_error<i32, /on_outer>]
in_pick([i32] value) {
  [Noisy] a{Noisy{1i32}}
  pick(get(value)) {
    none {
      print_line(0i32)
    }
    some(v) {
      [Noisy] b{Noisy{2i32}}
      [i32] x{check(v)?}
      print_line(x)
    }
  }
  return(0i32)
}
)",
                    "  print_line(in_pick(1i32))\n  print_line(in_pick(5i32))\n",
                    "1\n2\n1\n0\n109\n2\n1\n9\n");
}

TEST_CASE("the handler of a definition flows into the block arguments of a call") {
  runHandlerProgram("handler_flows_into_block_arguments",
                    R"(
[return<i32> effects(io_out)]
visit() {
  print_line(7i32)
  return(3i32)
}

[return<i32> effects(io_out) on_error<i32, /on_outer>]
in_args([i32] value) {
  [Noisy] a{Noisy{1i32}}
  [i32] got{visit() {
    [Noisy] b{Noisy{2i32}}
    [i32] x{check(value)?}
    print_line(x)
  }}
  print_line(got)
  return(0i32)
}
)",
                    "  print_line(in_args(1i32))\n  print_line(in_args(5i32))\n",
                    "7\n1\n2\n3\n1\n0\n7\n109\n2\n1\n9\n");
}

TEST_CASE("a block on_error overrides the handler and the outer one applies after the block") {
  runHandlerProgram("handler_override_ends",
                    R"(
[return<i32> effects(io_out) on_error<i32, /on_outer>]
overridden([i32] value) {
  [Noisy] a{Noisy{1i32}}
  [on_error<i32, /on_inner>] block() {
    [Noisy] b{Noisy{2i32}}
    [i32] x{check(value)?}
    print_line(x)
  }
  [i32] y{check(value + 3i32)?}
  print_line(y)
  return(0i32)
}
)",
                    "  print_line(overridden(1i32))\n  print_line(overridden(5i32))\n",
                    // 1: the block ends, then the outer handler takes the error of the second check.
                    // 5: the inner handler takes the first error.
                    "1\n2\n108\n1\n8\n209\n2\n1\n9\n");
}

TEST_CASE("an override applies to the loops and branches nested in its block") {
  runHandlerProgram("handler_override_nested",
                    R"(
[return<i32> effects(io_out) on_error<i32, /on_outer>]
nested_override([i32] value) {
  [on_error<i32, /on_inner>] block() {
    [i32 mut] i{0i32}
    while(i < 2i32) {
      if(value > 0i32) {
        [Noisy] b{Noisy{2i32}}
        [i32] x{check(value)?}
        print_line(x)
      }
      i = i + 1i32
    }
  }
  [i32] y{check(value + 4i32)?}
  return(y)
}
)",
                    "  print_line(nested_override(1i32))\n  print_line(nested_override(5i32))\n",
                    "1\n2\n1\n2\n109\n9\n209\n2\n9\n");
}

TEST_CASE("a block on_error inside an if branch covers a definition without a handler") {
  runHandlerProgram("handler_override_in_if",
                    R"(
[return<i32> effects(io_out)]
guarded([i32] value) {
  [Noisy] a{Noisy{1i32}}
  if(value > 0i32) {
    [on_error<i32, /on_inner>] block() {
      [i32] x{check(value)?}
      print_line(x)
    }
  }
  return(0i32)
}
)",
                    "  print_line(guarded(1i32))\n  print_line(guarded(5i32))\n",
                    "1\n1\n0\n209\n1\n9\n");
}

TEST_SUITE_END();
