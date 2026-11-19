#include "program_matrix.h"

TEST_SUITE_BEGIN("primestruct.program_matrix.math");

// The math builtins return the C library result, with the signs of zero and the infinities and NaN the standard
// defines (docs/spec/host-and-core-library.md, Math). Each program prints 1 per check that holds, so a wrong value
// shows as a 0 on the line of its check. Atan2 agrees within a few ulp, not bit for bit, so its checks use a bound.

namespace {

void runMathProgram(const std::string &name, const std::string &body, int checks) {
  program_matrix::ProgramCase program;
  program.name = name;
  program.source = R"(
import /std/math/*

[return<int>]
check([bool] ok) {
  return(if(ok, then() { 1i32 }, else() { 0i32 }))
}

[return<int> effects(io_out)]
main() {
  [f64] nan{0.0f64 / 0.0f64}
  [f64] inf{1.0f64 / 0.0f64}
  [f64] zero{0.0f64}
  [f64] neg_zero{negate(zero)}
)" + body + R"(
  return(0i32)
}
)";
  program.exitCode = 0;
  std::string expected;
  for (int i = 0; i < checks; ++i) {
    expected += "1\n";
  }
  program.stdoutText = expected;
  program.onlyConfigs = {
      "vm-step-O0", "vm-O0", "vm-O2", "native-O0", "native-O2", "optexe-O2", "exe"};
  program_matrix::runProgramMatrix(program);
}

} // namespace

TEST_CASE("copysign copies the sign of a negative zero") {
  runMathProgram("math_copysign_zero_sign", R"(
  print_line(check(1.0f64 / copysign(3.0f64, neg_zero) < 0.0f64 && copysign(3.0f64, neg_zero) == -3.0f64))
  print_line(check(copysign(3.0f64, zero) == 3.0f64))
  print_line(check(copysign(-3.0f64, zero) == 3.0f64))
  print_line(check(copysign(-3.0f64, 2.0f64) == 3.0f64))
  print_line(check(copysign(3.0f64, -2.0f64) == -3.0f64))
  print_line(check(1.0f64 / copysign(neg_zero, 1.0f64) > 0.0f64))
  print_line(check(1.0f64 / copysign(zero, -1.0f64) < 0.0f64))
  print_line(check(1.0f64 / copysign(neg_zero, neg_zero) < 0.0f64))
  print_line(check(copysign(inf, neg_zero) == negate(inf)))
  print_line(check(copysign(nan, neg_zero) != copysign(nan, neg_zero)))
  [f32] neg_zero_f{negate(0.0f32)}
  print_line(check(copysign(3.0f32, neg_zero_f) == -3.0f32))
  print_line(check(copysign(-3.0f32, 0.0f32) == 3.0f32))
)", 12);
}

TEST_CASE("round is half away from zero without double rounding") {
  runMathProgram("math_round_half_away", R"(
  print_line(check(round(0.49999999999999994f64) == 0.0f64))
  print_line(check(round(-0.49999999999999994f64) == 0.0f64))
  print_line(check(round(0.5f64) == 1.0f64))
  print_line(check(round(-0.5f64) == -1.0f64))
  print_line(check(round(2.5f64) == 3.0f64))
  print_line(check(round(-2.5f64) == -3.0f64))
  print_line(check(round(2.4999999999999996f64) == 2.0f64))
  print_line(check(round(1.5f64) == 2.0f64))
  print_line(check(round(-1.4f64) == -1.0f64))
  print_line(check(1.0f64 / round(-0.4f64) < 0.0f64))
  print_line(check(1.0f64 / round(neg_zero) < 0.0f64))
  print_line(check(1.0f64 / round(0.4f64) > 0.0f64))
  print_line(check(round(4503599627370497.0f64) == 4503599627370497.0f64))
  print_line(check(round(1.0e300f64) == 1.0e300f64))
  print_line(check(round(-1.0e300f64) == -1.0e300f64))
  print_line(check(round(inf) == inf))
  print_line(check(round(nan) != round(nan)))
  print_line(check(round(0.49999997f32) == 0.0f32))
  print_line(check(round(2.5f32) == 3.0f32))
  print_line(check(round(-2.5f32) == -3.0f32))
  print_line(check(round(16777217.0f32) == 16777216.0f32))
)", 21);
}

TEST_CASE("atan2 returns the angle of every quadrant, axis and zero") {
  runMathProgram("math_atan2_angles", R"(
  [f64] pi_value{3.141592653589793f64}
  [f64] tol{0.000000000000002f64}
  print_line(check(abs(atan2(1.0f64, 1.0f64) - pi_value / 4.0f64) < tol))
  print_line(check(abs(atan2(1.0f64, -1.0f64) - 3.0f64 * pi_value / 4.0f64) < tol))
  print_line(check(abs(atan2(-1.0f64, -1.0f64) + 3.0f64 * pi_value / 4.0f64) < tol))
  print_line(check(abs(atan2(-1.0f64, 1.0f64) + pi_value / 4.0f64) < tol))
  print_line(check(abs(atan2(2.0f64, -5.0f64) - 2.761086276477428f64) < tol))
  print_line(check(abs(atan2(-5.0f64, 2.0f64) + 1.1902899496825317f64) < tol))
  print_line(check(abs(atan2(1.0f64, 0.0f64) - pi_value / 2.0f64) < tol))
  print_line(check(abs(atan2(-1.0f64, 0.0f64) + pi_value / 2.0f64) < tol))
  print_line(check(atan2(0.0f64, 1.0f64) == 0.0f64 && 1.0f64 / atan2(zero, 1.0f64) > 0.0f64))
  print_line(check(1.0f64 / atan2(neg_zero, 1.0f64) < 0.0f64))
  print_line(check(abs(atan2(zero, -1.0f64) - pi_value) < tol))
  print_line(check(abs(atan2(neg_zero, -1.0f64) + pi_value) < tol))
  print_line(check(abs(atan2(zero, neg_zero) - pi_value) < tol))
  print_line(check(atan2(zero, zero) == 0.0f64))
  print_line(check(abs(atan2(inf, inf) - pi_value / 4.0f64) < tol))
  print_line(check(abs(atan2(inf, negate(inf)) - 3.0f64 * pi_value / 4.0f64) < tol))
  print_line(check(abs(atan2(1.0f64, inf)) < tol))
  print_line(check(abs(atan2(1.0f64, negate(inf)) - pi_value) < tol))
  print_line(check(abs(atan2(inf, 1.0f64) - pi_value / 2.0f64) < tol))
  print_line(check(abs(atan2(0.001f64, -1000.0f64) - 3.1415916535897934f64) < tol))
  print_line(check(atan2(nan, 1.0f64) != atan2(nan, 1.0f64) && atan2(1.0f64, nan) != atan2(1.0f64, nan)))
  print_line(check(abs(atan2(1.0f32, -1.0f32) - 2.3561945f32) < 0.000001f32))
  print_line(check(abs(atan2(-3.0f32, 4.0f32) + 0.6435011f32) < 0.000001f32))
)", 23);
}
