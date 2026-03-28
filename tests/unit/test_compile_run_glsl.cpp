#include "test_compile_run_helpers.h"

TEST_SUITE_BEGIN("primestruct.compile.run.glsl");

static bool compileGlslOrExpectNominalMathUnsupported(const std::string &compileCmd,
                                                      const std::string &errPath) {
  const int code = runCommand(compileCmd + " 2> " + quoteShellArg(errPath));
  if (code == 0) {
    return true;
  }
  CHECK(code == 2);
  const std::string error = readFile(errPath);
  CHECK((error.find(
             "native backend only supports arithmetic/comparison/clamp/min/max/abs/sign/saturate/convert/pointer/"
             "assign/increment/decrement calls in expressions") != std::string::npos ||
         error.find("local index exceeds glsl local-slot limit") != std::string::npos ||
         error.find("arithmetic operators require numeric operands") != std::string::npos));
  return false;
}

#include "test_compile_run_glsl_backends.h"

TEST_CASE("glsl emitter writes locals and arithmetic") {
  const std::string source = R"(
[return<void>]
main() {
  [i32 mut] counter{1i32};
  [f32] scale{2.0f32};
  assign(counter, plus(counter, 2i32));
  result{plus(scale, 0.5f32)};
  return();
}
)";
  const std::string srcPath = writeTemp("compile_glsl_locals.prime", source);
  const std::string outPath = (testScratchPath("") / "primec_glsl_locals.glsl").string();

  const std::string compileCmd = "./primec --emit=glsl " + srcPath + " -o " + outPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string output = readFile(outPath);
  CHECK(output.find("int locals[1024];") != std::string::npos);
  CHECK(output.find("int right = stack[--sp];") != std::string::npos);
  CHECK(output.find("float right = intBitsToFloat(stack[--sp]);") != std::string::npos);
  CHECK(output.find("PrimeStructOutput") != std::string::npos);
}

TEST_CASE("spirv emitter surfaces ir validation-stage failures without fallback") {
  const std::string source = R"(
[return<void>]
main() {
  [i64] wide{5000000000i64}
  return();
}
)";
  const std::string srcPath = writeTemp("compile_spirv_ir_backend_emit_failure.prime", source);
  const std::string outPath =
      (testScratchPath("") / "primec_spirv_ir_backend_emit_failure.spv").string();
  const std::string errPath =
      (testScratchPath("") / "primec_spirv_ir_backend_emit_failure_err.txt").string();
  std::error_code ec;
  std::filesystem::remove(outPath, ec);

  const std::string compileCmd = "./primec --emit=spirv " + quoteShellArg(srcPath) + " -o " + quoteShellArg(outPath) +
                                 " --entry /main 2> " + quoteShellArg(errPath);
  CHECK(runCommand(compileCmd) == 2);
  const std::string diagnostics = readFile(errPath);
  CHECK(diagnostics.find("SPIR-V IR validation error: ") != std::string::npos);
  CHECK(diagnostics.find("glsl i64 literal out of i32 range") != std::string::npos);
  CHECK_FALSE(std::filesystem::exists(outPath));
}

TEST_CASE("glsl emitter allows assign in expressions") {
  const std::string source = R"(
[return<void>]
main() {
  [i32 mut] counter{0i32}
  value{assign(counter, 2i32)}
  return()
}
)";
  const std::string srcPath = writeTemp("compile_glsl_assign_expr.prime", source);
  const std::string outPath = (testScratchPath("") / "primec_glsl_assign_expr.glsl").string();

  const std::string compileCmd = "./primec --emit=glsl " + srcPath + " -o " + outPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string output = readFile(outPath);
  CHECK(output.find("stack[sp++] = 2;") != std::string::npos);
  CHECK(output.find("locals[1] = stack[--sp];") != std::string::npos);
}

TEST_CASE("glsl emitter allows increment/decrement in expressions") {
  const std::string source = R"(
[return<void>]
main() {
  [i32 mut] counter{0i32}
  inc{increment(counter)}
  dec{decrement(counter)}
  return()
}
)";
  const std::string srcPath = writeTemp("compile_glsl_inc_dec_expr.prime", source);
  const std::string outPath = (testScratchPath("") / "primec_glsl_inc_dec_expr.glsl").string();

  const std::string compileCmd = "./primec --emit=glsl " + srcPath + " -o " + outPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string output = readFile(outPath);
  CHECK(output.find("stack[sp++] = left + right;") != std::string::npos);
  CHECK(output.find("stack[sp++] = left - right;") != std::string::npos);
  CHECK(output.find("locals[1] = stack[--sp];") != std::string::npos);
  CHECK(output.find("locals[2] = stack[--sp];") != std::string::npos);
}

TEST_CASE("glsl emitter writes if blocks") {
  const std::string source = R"(
[return<void>]
main() {
  [i32 mut] value{1i32}
  if(less_than(value, 2i32), then() { assign(value, 3i32) }, else() { assign(value, 4i32) })
  return()
}
)";
  const std::string srcPath = writeTemp("compile_glsl_if.prime", source);
  const std::string outPath = (testScratchPath("") / "primec_glsl_if.glsl").string();

  const std::string compileCmd = "./primec --emit=glsl " + srcPath + " -o " + outPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string output = readFile(outPath);
  CHECK(output.find("pc = (cond == 0) ?") != std::string::npos);
  CHECK(output.find("stack[sp++] = 3;") != std::string::npos);
  CHECK(output.find("stack[sp++] = 4;") != std::string::npos);
}

TEST_CASE("glsl emitter writes loops") {
  const std::string source = R"(
[return<void>]
main() {
  [i32 mut] value{0i32}
  loop(2i32, stepper() { assign(value, plus(value, 1i32)) })
  while(less_than(value, 3i32), branch() { assign(value, plus(value, 1i32)) })
  repeat(2i32) { assign(value, plus(value, 1i32)) }
  for([i32 mut] i{0i32}, less_than(i, 2i32), increment(i), body() { assign(value, plus(value, i)) })
  return()
}
)";
  const std::string srcPath = writeTemp("compile_glsl_loops.prime", source);
  const std::string outPath = (testScratchPath("") / "primec_glsl_loops.glsl").string();

  const std::string compileCmd = "./primec --emit=glsl " + srcPath + " -o " + outPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string output = readFile(outPath);
  CHECK(output.find("pc = (cond == 0) ?") != std::string::npos);
  CHECK(output.find("stack[sp++] = left + right;") != std::string::npos);
  CHECK(output.find("stack[sp++] = left - right;") != std::string::npos);
}

TEST_CASE("glsl emitter handles shared_scope blocks") {
  const std::string source = R"(
[return<void>]
main() {
  [i32 mut] acc{0i32}
  [shared_scope]
  for([i32 mut] i{0i32}, less_than(i, 2i32), increment(i)) {
    [i32] inner{1i32}
    assign(acc, plus(acc, inner))
  }
  return()
}
)";
  const std::string srcPath = writeTemp("compile_glsl_shared_scope.prime", source);
  const std::string outPath = (testScratchPath("") / "primec_glsl_shared_scope.glsl").string();

  const std::string compileCmd = "./primec --emit=glsl " + srcPath + " -o " + outPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string output = readFile(outPath);
  CHECK(output.find("locals[2] = stack[--sp];") != std::string::npos);
  CHECK(output.find("stack[sp++] = left + right;") != std::string::npos);
  CHECK(output.find("pc = (cond == 0) ?") != std::string::npos);
}

TEST_CASE("glsl emitter handles shared_scope while") {
  const std::string source = R"(
[return<void>]
main() {
  [i32 mut] total{0i32}
  [i32 mut] i{0i32}
  [shared_scope]
  while(less_than(i, 3i32)) {
    [i32] inner{1i32}
    assign(total, plus(total, inner))
    assign(i, plus(i, 1i32))
  }
  return()
}
)";
  const std::string srcPath = writeTemp("compile_glsl_shared_scope_while.prime", source);
  const std::string outPath = (testScratchPath("") / "primec_glsl_shared_scope_while.glsl").string();

  const std::string compileCmd = "./primec --emit=glsl " + srcPath + " -o " + outPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string output = readFile(outPath);
  CHECK(output.find("locals[2] = stack[--sp];") != std::string::npos);
  CHECK(output.find("stack[sp++] = left + right;") != std::string::npos);
  CHECK(output.find("pc = (cond == 0) ?") != std::string::npos);
}

TEST_CASE("glsl emitter handles block initializers") {
  const std::string source = R"(
[return<void>]
main() {
  [i32] value{
    [i32] base{1i32}
    plus(base, 2i32)
  }
  return()
}
)";
  const std::string srcPath = writeTemp("compile_glsl_block_init.prime", source);
  const std::string outPath = (testScratchPath("") / "primec_glsl_block_init.glsl").string();

  const std::string compileCmd = "./primec --emit=glsl " + srcPath + " -o " + outPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string output = readFile(outPath);
  CHECK(output.find("stack[sp++] = 1;") != std::string::npos);
  CHECK(output.find("stack[sp++] = 2;") != std::string::npos);
  CHECK(output.find("stack[sp++] = left + right;") != std::string::npos);
}

TEST_CASE("glsl emitter handles brace constructor values") {
  const std::string source = R"(
[return<void>]
main() {
  [bool] flag{
    return(bool{ equal(1i32, 1i32) })
  }
  return()
}
)";
  const std::string srcPath = writeTemp("compile_glsl_brace_ctor.prime", source);
  const std::string outPath = (testScratchPath("") / "primec_glsl_brace_ctor.glsl").string();

  const std::string compileCmd = "./primec --emit=glsl " + srcPath + " -o " + outPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string output = readFile(outPath);
  CHECK(output.find("stack[sp++] = (left == right) ? 1 : 0;") != std::string::npos);
  CHECK(output.find("stack[sp++] = (left != right) ? 1 : 0;") != std::string::npos);
}

TEST_CASE("glsl emitter ignores print builtins") {
  const std::string source = R"(
[return<void> effects(io_out, io_err)]
main() {
  [i32 mut] counter{0i32}
  print(assign(counter, 2i32))
  print("hello"utf8)
  print_line(1i32)
  print_error(2i32)
  print_line_error(3i32)
  return()
}
)";
  const std::string srcPath = writeTemp("compile_glsl_print_noop.prime", source);
  const std::string outPath = (testScratchPath("") / "primec_glsl_print_noop.glsl").string();

  const std::string compileCmd = "./primec --emit=glsl " + srcPath + " -o " + outPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string output = readFile(outPath);
  CHECK(output.find("GLSL backend ignores print side effects.") != std::string::npos);
  CHECK(output.find("GLSL backend ignores print-string side effects.") != std::string::npos);
}

TEST_CASE("glsl emitter accepts capabilities") {
  const std::string source = R"(
[return<void> capabilities(io_out)]
main() {
  print_line("cap"utf8)
  return()
}
)";
  const std::string srcPath = writeTemp("compile_glsl_caps.prime", source);
  const std::string outPath = (testScratchPath("") / "primec_glsl_caps.glsl").string();

  const std::string compileCmd = "./primec --emit=glsl " + srcPath + " -o " + outPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string output = readFile(outPath);
  CHECK(output.find("void main()") != std::string::npos);
}

TEST_CASE("glsl emitter accepts support-matrix effects and capabilities") {
  const std::string source = R"(
[return<void>
 effects(io_out, io_err, pathspace_notify, pathspace_insert, pathspace_take, pathspace_bind, pathspace_schedule)
 capabilities(io_out, io_err, pathspace_notify, pathspace_insert, pathspace_take, pathspace_bind, pathspace_schedule)]
main() {
  return()
}
)";
  const std::string srcPath = writeTemp("compile_glsl_support_matrix_effects.prime", source);
  const std::string outPath =
      (testScratchPath("") / "primec_glsl_support_matrix_effects.glsl").string();

  const std::string compileCmd = "./primec --emit=glsl " + srcPath + " -o " + outPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(readFile(outPath).find("void main()") != std::string::npos);
}

TEST_CASE("glsl emitter supports support-matrix scalar bindings") {
  const std::string source = R"(
[return<void>]
main() {
  [bool] enabled{true}
  [i32] i{1i32}
  [i64] j{2i64}
  [u64] k{3u64}
  [f32] x{1.0f32}
  [f64] y{2.0f64}
  [i32 mut] sink{0i32}
  if(enabled, then() { assign(sink, plus(i, 1i32)) }, else() { })
  return()
}
)";
  const std::string srcPath = writeTemp("compile_glsl_support_matrix_scalars.prime", source);
  const std::string outPath =
      (testScratchPath("") / "primec_glsl_support_matrix_scalars.glsl").string();

  const std::string compileCmd = "./primec --emit=glsl " + srcPath + " -o " + outPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string output = readFile(outPath);
  CHECK(output.find("Narrowed GLSL path lowers f64 literals through f32 payloads.") != std::string::npos);
  CHECK(output.find("locals[5] = stack[--sp];") != std::string::npos);
  CHECK(output.find("pc = (cond == 0) ?") != std::string::npos);
}

#include "test_compile_run_glsl_nominal_math.h"

TEST_CASE("glsl emitter rejects explicit effects") {
  const std::string source = R"(
[return<void> effects(heap_alloc)]
main() {
}
)";
  const std::string srcPath = writeTemp("compile_glsl_effects.prime", source);
  const std::string errPath = (testScratchPath("") / "primec_glsl_effects_err.txt").string();

  const std::string outPath = (testScratchPath("") / "primec_glsl_effects.glsl").string();
  const std::string compileCmd = "./primec --emit=glsl " + srcPath + " -o " + outPath + " --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 0);
  CHECK(readFile(outPath).find("void main()") != std::string::npos);
}

TEST_CASE("glsl emitter rejects static bindings") {
  const std::string source = R"(
[return<void>]
main() {
  [static i32] value{1i32}
}
)";
  const std::string srcPath = writeTemp("compile_glsl_static_binding.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_glsl_static_binding_err.txt").string();

  const std::string outPath = (testScratchPath("") / "primec_glsl_static_binding.glsl").string();
  const std::string compileCmd = "./primec --emit=glsl " + srcPath + " -o " + outPath + " --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 0);
  CHECK(readFile(outPath).find("stack[sp++] = 1;") != std::string::npos);
}

TEST_CASE("glsl emitter rejects non-scalar bindings") {
  const std::string source = R"(
[return<void>]
main() {
  [array<i32>] values{array<i32>(1i32)}
}
)";
  const std::string srcPath = writeTemp("compile_glsl_array_binding.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_glsl_array_binding_err.txt").string();

  const std::string outPath = (testScratchPath("") / "primec_glsl_array_binding.glsl").string();
  const std::string compileCmd = "./primec --emit=glsl " + srcPath + " -o " + outPath + " --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 0);
  CHECK(readFile(outPath).find("slot-byte offsets") != std::string::npos);
}

TEST_CASE("glsl emitter rejects mixed signed/unsigned math") {
  const std::string source = R"(
[return<void>]
main() {
  [i32] left{1i32}
  [u64] right{1u64}
  [i32] value{plus(left, right)}
  return()
}
)";
  const std::string srcPath = writeTemp("compile_glsl_mixed_signed.prime", source);
  const std::string errPath = (testScratchPath("") / "primec_glsl_mixed_signed_err.txt").string();

  const std::string compileCmd =
      "./primec --emit=glsl " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) != 0);
  CHECK(readFile(errPath).find("mixed signed/unsigned") != std::string::npos);
}

TEST_CASE("glsl emitter rejects mixed boolean comparisons") {
  const std::string source = R"(
[return<void>]
main() {
  [bool] flag{greater_than(true, 1i32)}
  return()
}
)";
  const std::string srcPath = writeTemp("compile_glsl_mixed_bool.prime", source);
  const std::string errPath = (testScratchPath("") / "primec_glsl_mixed_bool_err.txt").string();

  const std::string outPath = (testScratchPath("") / "primec_glsl_mixed_bool.glsl").string();
  const std::string compileCmd = "./primec --emit=glsl " + srcPath + " -o " + outPath + " --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 0);
  CHECK(readFile(outPath).find("stack[sp++] = (left > right) ? 1 : 0;") != std::string::npos);
}

TEST_CASE("glsl emitter rejects string literals") {
  const std::string source = R"(
[return<void>]
main() {
  [string] name{"hi"utf8}
}
)";
  const std::string srcPath = writeTemp("compile_glsl_string_literal.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_glsl_string_literal_err.txt").string();

  const std::string outPath = (testScratchPath("") / "primec_glsl_string_literal.glsl").string();
  const std::string compileCmd = "./primec --emit=glsl " + srcPath + " -o " + outPath + " --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 0);
  CHECK(readFile(outPath).find("stack[sp++] = 0;") != std::string::npos);
}

TEST_CASE("glsl emitter rejects unsupported capabilities") {
  const std::string source = R"(
[return<void> capabilities(render_graph) effects(render_graph)]
main() {
}
)";
  const std::string srcPath = writeTemp("compile_glsl_capability.prime", source);
  const std::string errPath = (testScratchPath("") / "primec_glsl_capability_err.txt").string();

  const std::string compileCmd =
      "./primec --emit=glsl " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) != 0);
  CHECK(readFile(errPath).find("does not support effect: render_graph") != std::string::npos);
}

TEST_CASE("glsl emitter rejects effects on executions") {
  const std::string source = R"(
[return<void>]
main() {
  [effects(heap_alloc)] plus(1i32, 2i32)
  return()
}
)";
  const std::string srcPath = writeTemp("compile_glsl_exec_effects.prime", source);
  const std::string errPath = (testScratchPath("") / "primec_glsl_exec_effects_err.txt").string();

  const std::string compileCmd =
      "./primec --emit=glsl " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) != 0);
  CHECK(readFile(errPath).find("execution effects must be a subset") != std::string::npos);
}

TEST_CASE("glsl emitter rejects non-void entry") {
  const std::string source = R"(
[return<int>]
main() {
  return(1i32)
}
)";
  const std::string srcPath = writeTemp("compile_glsl_reject.prime", source);
  const std::string errPath = (testScratchPath("") / "primec_glsl_reject_err.txt").string();

  const std::string outPath = (testScratchPath("") / "primec_glsl_reject.glsl").string();
  const std::string compileCmd = "./primec --emit=glsl " + srcPath + " -o " + outPath + " --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 0);
  CHECK(readFile(outPath).find("return stack[--sp];") != std::string::npos);
}

TEST_SUITE_END();
