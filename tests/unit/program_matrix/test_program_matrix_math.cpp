#include "program_matrix.h"

TEST_SUITE_BEGIN("primestruct.program_matrix.math");

// The math builtins return the C library result, with the signs of zero and the infinities and NaN the standard
// defines (docs/spec/host-and-core-library.md, Math). Each program prints 1 per check that holds, so a wrong value
// shows as a 0 on the line of its check. Atan2 agrees within a few ulp, not bit for bit, so its checks use a bound.

namespace {

// `nativeRoundsOnce` is false for a program whose native config does not give the fused result on this host.
void runMathProgram(const std::string &name,
                    const std::string &body,
                    int checks,
                    bool nativeRoundsOnce = true) {
  program_matrix::ProgramCase program;
  program.name = name;
  // Wasm runs these programs; the others use opcodes it lacks.
  if (name == "math_atan2_angles" || name == "math_fma_fused" || name == "math_round_half_away") {
    program.extraConfigs = {"wasm-wasi"};
  }
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
  if (!nativeRoundsOnce) {
    program.onlyConfigs = {"vm-step-O0", "vm-O0", "vm-O2", "optexe-O2", "exe"};
  }
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

// fma rounds once. Each case below has a product that does not fit the type, so a multiply followed by an add
// (two roundings) gives a different value than the fused result; every backend and the constant folder must give the
// fused one (docs/spec/host-and-core-library.md, Exact helpers).
TEST_CASE("fma rounds once for f64 and f32") {
#if defined(__x86_64__)
  // x86_64 native computes the opcode as a multiply and an add until its FMA3 form lands (docs/spec/vm-design.md,
  // Fused multiply-add).
  const bool nativeRoundsOnce = false;
#else
  const bool nativeRoundsOnce = true;
#endif
  runMathProgram("math_fma_fused",
                 R"(
  [f64] e{1.0f64 / 1073741824.0f64}
  [f64] tiny{e * e}
  print_line(check(fma(1.0f64 + e, 1.0f64 - e, -1.0f64) == negate(tiny)))
  print_line(check(fma(1.0f64 - e, 1.0f64 + e, -1.0f64) == negate(tiny)))
  print_line(check(fma(1.0f64 + e, 1.0f64 + e, -1.0f64 - e - e) == tiny))
  print_line(check(fma(2.0f64, 3.0f64, 4.0f64) == 10.0f64))
  print_line(check(fma(0.1f64, 10.0f64, -1.0f64) == 0.000000000000000055511151231257827f64))
  [f32] e32{1.0f32 / 8192.0f32}
  [f32] tiny32{e32 * e32}
  print_line(check(fma(1.0f32 + e32, 1.0f32 - e32, -1.0f32) == negate(tiny32)))
  print_line(check(fma(2.0f32, 3.0f32, 4.0f32) == 10.0f32))
  [f32] a{8391483.0f32 / 34359738368.0f32}
  [f32] b{8385734.0f32 / 34359738368.0f32}
  print_line(check(fma(a, b, 1.0f32) == 1.0f32 + 0.00000011920928955078125f32))
  print_line(check(fma(b, a, 1.0f32) == 1.0f32 + 0.00000011920928955078125f32))
  print_line(check(fma(inf, 1.0f64, 2.0f64) == inf))
  print_line(check(fma(inf, 0.0f64, 1.0f64) != fma(inf, 0.0f64, 1.0f64)))
  print_line(check(fma(inf, 1.0f64, negate(inf)) != fma(inf, 1.0f64, negate(inf))))
  print_line(check(fma(1.0e200f64, 1.0e200f64, negate(inf)) == negate(inf)))
)",
                 13,
                 nativeRoundsOnce);
}
