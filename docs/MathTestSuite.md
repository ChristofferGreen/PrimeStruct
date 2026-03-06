# Math Doctest Coverage TODO (Large Suite)

This is an expanded checklist of math doctests to add across C++/exe (reference), VM, and native.

## Harness + Infrastructure
- [x] Add a shared helper that runs the same source under `--emit=exe`, `--emit=vm`, and `--emit=native`.
- [x] Add stdout comparison with per-test tolerances for float outputs.
- [x] Add a small float parser that can handle `nan`, `inf`, `-inf` tokens.
- [x] Add helpers for `abs_diff <= epsilon` and `relative_error <= epsilon`.
- [x] Add helpers for sign-only and range-only checks (avoid overfitting accuracy).
- [x] Add helpers for batch cases to keep test sources compact.
- [x] Add a way to mark expected native deviations (temporary allowlist).
- [x] Add a `--math-conformance` CTest label for the new suite.

## Source Of Truth For Expected Values
- [x] Define reference values using C++/exe as the canonical baseline in tests.
- [x] For numeric literals used as “golden” constants, verify with an external tool:
  - [x] C++ (e.g. `std::sin`, `std::cos`) or Python `math` module.
  - [x] Record the tool and version used for each golden value (comment or doc).
- [x] Keep a small `tools/` helper script to print reference values for a list of inputs.
  - [x] Seed with `tools/print_math_refs.py` and record Python version in output.
- [x] Decide when to use exact comparisons vs tolerance-based checks per function.
  - Use exact comparisons for integer/bool outputs and label ordering.
  - Use tolerances for float outputs unless testing sign/range predicates.

## Constants
- [x] `pi`, `tau`, `e` emit correct values (float32/float64 conversion checks).
- [x] `pi`, `tau`, `e` round-trip through `convert<f32>` and `convert<f64>`.
- [x] Check that `tau == 2 * pi` within tight tolerance.

## Trig: sin/cos/tan
- [x] Quadrant correctness for `sin`, `cos`, `tan` at `0`, `pi/2`, `pi`, `3pi/2`, `2pi`.
- [x] Symmetry: `sin(-x) = -sin(x)`, `cos(-x) = cos(x)`, `tan(-x) = -tan(x)`.
- [x] Range reduction for large inputs: `x = 10, 20, 100, 1e3, 1e4`.
- [x] Pythagorean identity: `sin(x)^2 + cos(x)^2 ~= 1` for a grid of values.
- [x] `tan(x) ~= sin(x) / cos(x)` where `cos(x)` not near zero.
- [x] Continuity checks across quadrant boundaries (avoid extra zero crossings).
- [x] Small-angle behavior: `sin(x) ~= x` for `x in [-1e-4, 1e-4]`.

## Inverse Trig: asin/acos/atan
- [x] Domain edges at `-1`, `0`, `1`.
- [x] Near-edge values: `-0.999`, `0.999`.
- [x] Round-trip: `sin(asin(x)) ~= x` and `cos(acos(x)) ~= x` for `x in [-0.9, 0.9]`.
- [x] `atan(1) ~= pi/4`, `atan(-1) ~= -pi/4`.
- [x] Monotonicity samples for `atan` over `[-10, 10]`.

## atan2
- [x] Quadrant correctness for all four quadrants.
- [x] Axis cases: `atan2(0, x)` and `atan2(y, 0)` with sign checks.
- [x] Symmetry: `atan2(-y, -x) ~= atan2(y, x) +/- pi` where applicable.
- [x] Known values: `atan2(1, 1) ~= pi/4`, `atan2(1, 0) ~= pi/2`.

## Hyperbolic: sinh/cosh/tanh
- [x] Symmetry: `sinh(-x) = -sinh(x)`, `cosh(-x) = cosh(x)`, `tanh(-x) = -tanh(x)`.
- [x] Identity: `cosh(x)^2 - sinh(x)^2 ~= 1` for moderate `x`.
- [x] `tanh(0) = 0`, `cosh(0) = 1`.
- [x] Range sanity for `x = 1, 2, 5` (avoid overflow but test growth).

## Inverse Hyperbolic: asinh/acosh/atanh
- [x] Domain checks for `asinh` (all reals).
- [x] Domain checks for `acosh` (`x >= 1`) including `1` and `1.5`.
- [x] Domain checks for `atanh` (`-1 < x < 1`) and near-edge values `0.99`.
- [x] Round-trip: `sinh(asinh(x)) ~= x`, `cosh(acosh(x)) ~= x`, `tanh(atanh(x)) ~= x`.

## Exp/Log
- [x] `exp(0) = 1`, `exp2(0) = 1`.
- [x] `log(1) = 0`, `log2(1) = 0`, `log10(1) = 0`.
- [x] `exp(log(x)) ~= x` for `x = 0.5, 1, 2, 10`.
- [x] `log(exp(x)) ~= x` for `x = -2, -1, 0, 1, 2`.
- [x] Base relations: `log2(8) = 3`, `log10(1000) = 3`.

## Roots: sqrt/cbrt
- [x] Perfect squares/cubes: `4`, `9`, `27`, `64`.
- [x] Non-perfect values: `2`, `3`, `5`, `10`.
- [x] Negative input handling for `sqrt` (expected policy: error, NaN, or clamp).
- [x] Round-trip: `sqrt(x) * sqrt(x) ~= x` for `x > 0`.
- [x] `cbrt` for negative values: `cbrt(-8) = -2`.

## Pow
- [x] Integer powers: `pow(2, 0)`, `pow(2, 3)`, `pow(10, 2)`.
- [x] Float powers: `pow(2.0, 0.5) ~= sqrt(2)`.
- [x] Edge cases: `pow(0, 0)` policy, `pow(1, x)`.
- [x] Negative exponent policy for ints (error or result).
- [x] Sign behavior: `pow(-2, 3) = -8`, `pow(-2, 2) = 4`.

## Rounding: floor/ceil/round/trunc/fract
- [x] Positive values: `1.1`, `1.5`, `1.9`.
- [x] Negative values: `-1.1`, `-1.5`, `-1.9`.
- [x] Boundary values near integers: `2.0 - 1e-6`, `2.0 + 1e-6`.
- [x] `round` tie-breaking policy for `.5` values.
- [x] `fract(x)` consistency with `x - floor(x)`.
- [x] `trunc` consistency with dropping fractional part toward zero.

## Min/Max/Clamp/Saturate
- [x] Int and float variants for `min` and `max`.
- [x] `clamp` with normal ordering.
- [x] `clamp` with inverted bounds (`min > max`) policy.
- [x] `saturate` for values below `0` and above `1`.
- [x] `saturate` for integer and float inputs.

## Lerp
- [x] `lerp(a, b, 0) = a`, `lerp(a, b, 1) = b`.
- [x] `lerp(a, b, 0.5)` midpoint check for float; int lerp uses integer `t`.
- [x] Out-of-range `t` behavior (if allowed).
- [x] `lerp` with negative values.

## FMA
- [x] `fma(a, b, c)` vs `a*b + c` for representative values.
- [x] Small values to check loss of precision (float32).
- [x] Large values to check overflow behavior.

## Hypot
- [x] Pythagorean triples: `(3,4) -> 5`, `(5,12) -> 13`.
- [x] Symmetry: `hypot(x, y) = hypot(y, x)`.
- [x] Scale invariance: `hypot(kx, ky) = |k| * hypot(x, y)`.

## Copysign
- [x] Sign transfer with positive/negative magnitudes.
- [x] `copysign(0, -1)` behavior (negative zero policy if supported).
- [x] `copysign` with NaN magnitude policy.

## Abs/Sign
- [x] Positive, negative, zero for ints and floats.
- [x] `sign(0)` behavior (0 or 1 policy).
- [x] `abs` on minimum int values (overflow policy).

## Math Predicates: is_nan/is_inf/is_finite
- [x] `0/0` is NaN.
- [x] `1/0` is Inf.
- [x] `is_finite` for normal values.
- [x] NaN propagation through arithmetic operators (if defined).

## Angle Conversions
- [x] `radians(degrees(x)) ~= x` for multiple values.
- [x] `degrees(radians(x)) ~= x` for multiple values.
- [x] Large angle conversions sanity checks.

## Mixed-Type Conversions
- [x] `convert<f32>` and `convert<f64>` from ints for key values.
- [x] `convert<int>` and `convert<i64>` from floats around boundaries.
- [x] Conversion of NaN/Inf to int policy (runtime error).

## Vector/Array Math Usage
- [x] Spot checks where math builtins are used inside vector ops (no ABI surprises).
- [x] Ensure math ops on indexed array values behave identically across backends.

## Matrix/Quaternion Interaction Contract (Draft)
- [ ] Add parser/semantic tests for matrix/quaternion operator allowlist and disallowlist:
  `+`, `-`, `*`, `/` combinations by shape/type.
- [ ] Add shape-mismatch diagnostics for `Mat * Vec` and `Mat * Mat` dimension errors.
- [ ] Add deterministic tests that reject implicit scalar/vector/matrix/quaternion conversions.
- [ ] Add quaternion Hamilton-product correctness tests (`q1 * q2`) against reference values.
- [ ] Add quaternion-vector rotation tests (`Quat * Vec3`) including identity and 90-degree axis rotations.
- [ ] Add matrix/vector convention tests to lock column-vector semantics (`Mat * Vec`) and composition order.
- [ ] Add exact-equality vs tolerance-helper behavior tests for float-backed matrix/quaternion values.
- [ ] Add backend-gating tests verifying unsupported backends emit stable diagnostics for matrix/quaternion ops until support lands.

## Cross-Backend Compliance
- [x] Each category above has a C++/exe baseline test.
- [x] VM and native outputs match baseline within tolerance.
- [x] Dedicated tests for known native approximation limits.
- [x] Stress tests with a grid of values per function (small sample).
  - See `math_conformance_float_samples`, `math_conformance_float_grid`,
    and `math_conformance_native_limits` in the conformance suite.

## Performance Guardrails
- [x] Add a couple of “heavy math” tests to ensure runtime does not regress badly.
- [x] Bound execution time for the math suite (keep CI stable).
  - CTest timeout for `primestruct.compile.run.math_conformance` set to 600s.
