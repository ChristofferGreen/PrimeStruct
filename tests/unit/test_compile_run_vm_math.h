TEST_CASE("runs vm with math abs/sign/min/max") {
  const std::string source = R"(
import /math/*
[return<int>]
main() {
  [i32] a{abs(-5i32)}
  [i32] b{sign(-5i32)}
  [i32] c{min(7i32, 2i32)}
  [i32] d{max(7i32, 2i32)}
  return(plus(plus(a, b), plus(c, d)))
}
)";
  const std::string srcPath = writeTemp("vm_math_basic.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 13);
}

TEST_CASE("runs vm with qualified math names") {
  const std::string source = R"(
[return<int>]
main() {
  [i32] a{/math/abs(-5i32)}
  [i32] b{/math/sign(-5i32)}
  [i32] c{/math/min(7i32, 2i32)}
  [i32] d{/math/max(7i32, 2i32)}
  [i32] e{convert<int>(/math/pi)}
  return(plus(plus(plus(a, b), plus(c, d)), e))
}
)";
  const std::string srcPath = writeTemp("vm_math_qualified.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 16);
}

TEST_CASE("runs vm with math saturate/lerp") {
  const std::string source = R"(
import /math/*
[return<int>]
main() {
  [i32] a{saturate(-2i32)}
  [i32] b{saturate(2i32)}
  [i32] c{lerp(0i32, 10i32, 1i32)}
  return(plus(plus(a, b), c))
}
)";
  const std::string srcPath = writeTemp("vm_math_saturate_lerp.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 11);
}

TEST_CASE("runs vm with math clamp") {
  const std::string source = R"(
import /math/*
[return<int>]
main() {
  [f32] a{clamp(2.5f32, 0.0f32, 1.0f32)}
  [u64] b{clamp(9u64, 2u64, 6u64)}
  return(plus(convert<int>(a), convert<int>(b)))
}
)";
  const std::string srcPath = writeTemp("vm_math_clamp.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 7);
}

TEST_CASE("runs vm with math pow") {
  const std::string source = R"(
import /math/*
[return<int>]
main() {
  return(pow(2i32, 6i32))
}
)";
  const std::string srcPath = writeTemp("vm_math_pow.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 64);
}

TEST_CASE("runs vm with math pow rejects negative exponent") {
  const std::string source = R"(
import /math/*
[return<int>]
main() {
  return(pow(2i32, -1i32))
}
)";
  const std::string srcPath = writeTemp("vm_math_pow_negative.prime", source);
  const std::string errPath = (std::filesystem::temp_directory_path() / "primec_vm_math_pow_negative_err.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main 2> " + errPath;
  CHECK(runCommand(runCmd) == 3);
  CHECK(readFile(errPath) == "pow exponent must be non-negative\n");
}

TEST_CASE("runs vm with math constant conversions") {
  const std::string source = R"(
import /math/*
[return<int>]
main() {
  return(plus(convert<int>(pi), plus(convert<int>(tau), convert<int>(e))))
}
)";
  const std::string srcPath = writeTemp("vm_math_constants_convert.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 11);
}

TEST_CASE("runs vm with math constants") {
  const std::string source = R"(
import /math/*
[return<int>]
main() {
  [f64] sum{plus(pi, plus(tau, e))}
  return(convert<int>(sum))
}
)";
  const std::string srcPath = writeTemp("vm_math_constants_direct.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 12);
}

TEST_CASE("runs vm with math predicates") {
  const std::string source = R"(
import /math/*
[return<int>]
main() {
  [f32] nan{divide(0.0f32, 0.0f32)}
  [f32] inf{divide(1.0f32, 0.0f32)}
  return(plus(
    plus(convert<int>(is_nan(nan)), convert<int>(is_inf(inf))),
    convert<int>(is_finite(1.0f32))
  ))
}
)";
  const std::string srcPath = writeTemp("vm_math_predicates.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 3);
}

TEST_CASE("runs vm with math rounding") {
  const std::string source = R"(
import /math/*
[return<int>]
main() {
  [f32] a{floor(1.9f32)}
  [f32] b{ceil(1.1f32)}
  [f32] c{round(2.5f32)}
  [f32] d{trunc(-1.7f32)}
  [f32] e{fract(2.25f32)}
  return(plus(
    plus(convert<int>(a), convert<int>(b)),
    plus(convert<int>(c), plus(convert<int>(d), convert<int>(multiply(e, 4.0f32))))
  ))
}
)";
  const std::string srcPath = writeTemp("vm_math_rounding.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 6);
}

TEST_CASE("runs vm with math roots") {
  const std::string source = R"(
import /math/*
[return<int>]
main() {
  [f32] a{sqrt(9.0f32)}
  [f32] b{cbrt(27.0f32)}
  return(plus(convert<int>(a), convert<int>(b)))
}
)";
  const std::string srcPath = writeTemp("vm_math_roots.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 6);
}

TEST_CASE("runs vm with math fma/hypot") {
  const std::string source = R"(
import /math/*
[return<int>]
main() {
  [f32] a{fma(2.0f32, 3.0f32, 4.0f32)}
  [f32] b{hypot(1.0f32, 1.0f32)}
  return(plus(convert<int>(a), convert<int>(b)))
}
)";
  const std::string srcPath = writeTemp("vm_math_fma_hypot.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 11);
}

TEST_CASE("runs vm with math copysign") {
  const std::string source = R"(
import /math/*
[return<int>]
main() {
  [f32] a{copysign(3.0f32, -1.0f32)}
  [f32] b{copysign(4.0f32, 1.0f32)}
  return(convert<int>(plus(a, b)))
}
)";
  const std::string srcPath = writeTemp("vm_math_copysign.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 1);
}

TEST_CASE("runs vm with math angle helpers") {
  const std::string source = R"(
import /math/*
[return<int>]
main() {
  [f32] a{radians(90.0f32)}
  [f32] b{degrees(1.0f32)}
  return(plus(convert<int>(a), convert<int>(b)))
}
)";
  const std::string srcPath = writeTemp("vm_math_angles.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 58);
}

TEST_CASE("runs vm with math trig helpers") {
  const std::string source = R"(
import /math/*
[return<int>]
main() {
  [f32] a{sin(0.0f32)}
  [f32] b{cos(0.0f32)}
  [f32] c{tan(0.0f32)}
  [f32] d{atan2(1.0f32, 0.0f32)}
  [f32] sum{plus(plus(a, b), c)}
  return(plus(convert<int>(sum), convert<int>(d)))
}
)";
  const std::string srcPath = writeTemp("vm_math_trig.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 2);
}

TEST_CASE("runs vm with math arc trig helpers") {
  const std::string source = R"(
import /math/*
[return<int>]
main() {
  [f32] a{asin(0.0f32)}
  [f32] b{acos(0.0f32)}
  [f32] c{atan(1.0f32)}
  return(plus(plus(convert<int>(a), convert<int>(b)), convert<int>(c)))
}
)";
  const std::string srcPath = writeTemp("vm_math_arc_trig.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 1);
}

TEST_CASE("runs vm with math exp/log") {
  const std::string source = R"(
import /math/*
[return<int>]
main() {
  [f32] a{exp(0.0f32)}
  [f32] b{exp2(1.0f32)}
  [f32] c{log(1.0f32)}
  [f32] d{log2(2.0f32)}
  [f32] e{log10(1.0f32)}
  [f32] sum{plus(plus(a, b), plus(c, plus(d, e)))}
  return(convert<int>(plus(sum, 0.5f32)))
}
)";
  const std::string srcPath = writeTemp("vm_math_exp_log.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 4);
}

TEST_CASE("runs vm with math hyperbolic") {
  const std::string source = R"(
import /math/*
[return<int>]
main() {
  [f32] a{sinh(0.0f32)}
  [f32] b{cosh(0.0f32)}
  [f32] c{tanh(0.0f32)}
  [f32] d{asinh(0.0f32)}
  [f32] e{acosh(1.0f32)}
  [f32] f{atanh(0.0f32)}
  [f32] sum{plus(plus(a, b), plus(c, plus(d, plus(e, f))))}
  return(convert<int>(sum))
}
)";
  const std::string srcPath = writeTemp("vm_math_hyperbolic.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 1);
}

TEST_CASE("runs vm with float pow") {
  const std::string source = R"(
import /math/*
[return<int>]
main() {
  return(convert<int>(plus(pow(2.0f32, 3.0f32), 0.5f32)))
}
)";
  const std::string srcPath = writeTemp("vm_math_pow_float.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 8);
}

TEST_CASE("runs vm with convert bool from integers") {
  const std::string source = R"(
[return<int>]
main() {
  return(plus(
    convert<int>(convert<bool>(0i32)),
    plus(convert<int>(convert<bool>(-1i32)), convert<int>(convert<bool>(5u64)))
  ))
}
)";
  const std::string srcPath = writeTemp("vm_convert_bool.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 2);
}

TEST_CASE("runs vm float literals") {
  const std::string source = R"(
[return<int>]
main() {
  return(convert<int>(1.5f))
}
)";
  const std::string srcPath = writeTemp("vm_float_literal.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 1);
}

TEST_CASE("runs vm float bindings") {
  const std::string source = R"(
[return<int>]
main() {
  [float] value{1.5f}
  [float] other{2.0f32}
  return(convert<int>(plus(value, other)))
}
)";
  const std::string srcPath = writeTemp("vm_float_binding.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 3);
}
