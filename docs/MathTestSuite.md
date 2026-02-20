# Math Doctest Coverage TODO (Large Suite)

This is an expanded checklist of math doctests to add across C++/exe (reference), VM, and native.

## Harness + Infrastructure
- [ ] Add a shared helper that runs the same source under `--emit=exe`, `--emit=vm`, and `--emit=native`.
- [ ] Add stdout comparison with per-test tolerances for float outputs.
- [ ] Add a small float parser that can handle `nan`, `inf`, `-inf` tokens.
- [ ] Add helpers for `abs_diff <= epsilon` and `relative_error <= epsilon`.
- [ ] Add helpers for sign-only and range-only checks (avoid overfitting accuracy).
- [ ] Add helpers for batch cases to keep test sources compact.
- [ ] Add a way to mark expected native deviations (temporary allowlist).
- [ ] Add a `--math-conformance` CTest label for the new suite.

## Source Of Truth For Expected Values
- [ ] Define reference values using C++/exe as the canonical baseline in tests.
- [ ] For numeric literals used as “golden” constants, verify with an external tool:
  - [ ] C++ (e.g. `std::sin`, `std::cos`) or Python `math` module.
  - [ ] Record the tool and version used for each golden value (comment or doc).
- [ ] Keep a small `tools/` helper script to print reference values for a list of inputs.
  - [ ] Seed with `tools/print_math_refs.py` and record Python version in output.
- [ ] Decide when to use exact comparisons vs tolerance-based checks per function.

## Constants
- [ ] `pi`, `tau`, `e` emit correct values (float32/float64 conversion checks).
- [ ] `pi`, `tau`, `e` round-trip through `convert<f32>` and `convert<f64>`.
- [ ] Check that `tau == 2 * pi` within tight tolerance.

## Trig: sin/cos/tan
- [ ] Quadrant correctness for `sin`, `cos`, `tan` at `0`, `pi/2`, `pi`, `3pi/2`, `2pi`.
- [ ] Symmetry: `sin(-x) = -sin(x)`, `cos(-x) = cos(x)`, `tan(-x) = -tan(x)`.
- [ ] Range reduction for large inputs: `x = 10, 20, 100, 1e3, 1e4`.
- [ ] Pythagorean identity: `sin(x)^2 + cos(x)^2 ~= 1` for a grid of values.
- [ ] `tan(x) ~= sin(x) / cos(x)` where `cos(x)` not near zero.
- [ ] Continuity checks across quadrant boundaries (avoid extra zero crossings).
- [ ] Small-angle behavior: `sin(x) ~= x` for `x in [-1e-4, 1e-4]`.

## Inverse Trig: asin/acos/atan
- [ ] Domain edges at `-1`, `0`, `1`.
- [ ] Near-edge values: `-0.999`, `0.999`.
- [ ] Round-trip: `sin(asin(x)) ~= x` and `cos(acos(x)) ~= x` for `x in [-0.9, 0.9]`.
- [ ] `atan(1) ~= pi/4`, `atan(-1) ~= -pi/4`.
- [ ] Monotonicity samples for `atan` over `[-10, 10]`.

## atan2
- [ ] Quadrant correctness for all four quadrants.
- [ ] Axis cases: `atan2(0, x)` and `atan2(y, 0)` with sign checks.
- [ ] Symmetry: `atan2(-y, -x) ~= atan2(y, x) +/- pi` where applicable.
- [ ] Known values: `atan2(1, 1) ~= pi/4`, `atan2(1, 0) ~= pi/2`.

## Hyperbolic: sinh/cosh/tanh
- [ ] Symmetry: `sinh(-x) = -sinh(x)`, `cosh(-x) = cosh(x)`, `tanh(-x) = -tanh(x)`.
- [ ] Identity: `cosh(x)^2 - sinh(x)^2 ~= 1` for moderate `x`.
- [ ] `tanh(0) = 0`, `cosh(0) = 1`.
- [ ] Range sanity for `x = 1, 2, 5` (avoid overflow but test growth).

## Inverse Hyperbolic: asinh/acosh/atanh
- [ ] Domain checks for `asinh` (all reals).
- [ ] Domain checks for `acosh` (`x >= 1`) including `1` and `1.5`.
- [ ] Domain checks for `atanh` (`-1 < x < 1`) and near-edge values `0.99`.
- [ ] Round-trip: `sinh(asinh(x)) ~= x`, `cosh(acosh(x)) ~= x`, `tanh(atanh(x)) ~= x`.

## Exp/Log
- [ ] `exp(0) = 1`, `exp2(0) = 1`.
- [ ] `log(1) = 0`, `log2(1) = 0`, `log10(1) = 0`.
- [ ] `exp(log(x)) ~= x` for `x = 0.5, 1, 2, 10`.
- [ ] `log(exp(x)) ~= x` for `x = -2, -1, 0, 1, 2`.
- [ ] Base relations: `log2(8) = 3`, `log10(1000) = 3`.

## Roots: sqrt/cbrt
- [ ] Perfect squares/cubes: `4`, `9`, `27`, `64`.
- [ ] Non-perfect values: `2`, `3`, `5`, `10`.
- [ ] Negative input handling for `sqrt` (expected policy: error, NaN, or clamp).
- [ ] Round-trip: `sqrt(x) * sqrt(x) ~= x` for `x > 0`.
- [ ] `cbrt` for negative values: `cbrt(-8) = -2`.

## Pow
- [ ] Integer powers: `pow(2, 0)`, `pow(2, 3)`, `pow(10, 2)`.
- [ ] Float powers: `pow(2.0, 0.5) ~= sqrt(2)`.
- [ ] Edge cases: `pow(0, 0)` policy, `pow(1, x)`.
- [ ] Negative exponent policy for ints (error or result).
- [ ] Sign behavior: `pow(-2, 3) = -8`, `pow(-2, 2) = 4`.

## Rounding: floor/ceil/round/trunc/fract
- [ ] Positive values: `1.1`, `1.5`, `1.9`.
- [ ] Negative values: `-1.1`, `-1.5`, `-1.9`.
- [ ] Boundary values near integers: `2.0 - 1e-6`, `2.0 + 1e-6`.
- [ ] `round` tie-breaking policy for `.5` values.
- [ ] `fract(x)` consistency with `x - floor(x)`.
- [ ] `trunc` consistency with dropping fractional part toward zero.

## Min/Max/Clamp/Saturate
- [ ] Int and float variants for `min` and `max`.
- [ ] `clamp` with normal ordering.
- [ ] `clamp` with inverted bounds (`min > max`) policy.
- [ ] `saturate` for values below `0` and above `1`.
- [ ] `saturate` for integer and float inputs.

## Lerp
- [ ] `lerp(a, b, 0) = a`, `lerp(a, b, 1) = b`.
- [ ] `lerp(a, b, 0.5)` midpoint check for int and float.
- [ ] Out-of-range `t` behavior (if allowed).
- [ ] `lerp` with negative values.

## FMA
- [ ] `fma(a, b, c)` vs `a*b + c` for representative values.
- [ ] Small values to check loss of precision (float32).
- [ ] Large values to check overflow behavior.

## Hypot
- [ ] Pythagorean triples: `(3,4) -> 5`, `(5,12) -> 13`.
- [ ] Symmetry: `hypot(x, y) = hypot(y, x)`.
- [ ] Scale invariance: `hypot(kx, ky) = |k| * hypot(x, y)`.

## Copysign
- [ ] Sign transfer with positive/negative magnitudes.
- [ ] `copysign(0, -1)` behavior (negative zero policy if supported).
- [ ] `copysign` with NaN magnitude policy.

## Abs/Sign
- [ ] Positive, negative, zero for ints and floats.
- [ ] `sign(0)` behavior (0 or 1 policy).
- [ ] `abs` on minimum int values (overflow policy).

## Math Predicates: is_nan/is_inf/is_finite
- [ ] `0/0` is NaN.
- [ ] `1/0` is Inf.
- [ ] `is_finite` for normal values.
- [ ] NaN propagation through arithmetic operators (if defined).

## Angle Conversions
- [ ] `radians(degrees(x)) ~= x` for multiple values.
- [ ] `degrees(radians(x)) ~= x` for multiple values.
- [ ] Large angle conversions sanity checks.

## Mixed-Type Conversions
- [ ] `convert<f32>` and `convert<f64>` from ints for key values.
- [ ] `convert<int>` and `convert<i64>` from floats around boundaries.
- [ ] Conversion of NaN/Inf to int policy (error or truncation).

## Vector/Array Math Usage
- [ ] Spot checks where math builtins are used inside vector ops (no ABI surprises).
- [ ] Ensure math ops on indexed array values behave identically across backends.

## Cross-Backend Compliance
- [ ] Each category above has a C++/exe baseline test.
- [ ] VM and native outputs match baseline within tolerance.
- [ ] Dedicated tests for known native approximation limits.
- [ ] Stress tests with a grid of values per function (small sample).

## Performance Guardrails
- [ ] Add a couple of “heavy math” tests to ensure runtime does not regress badly.
- [ ] Bound execution time for the math suite (keep CI stable).
