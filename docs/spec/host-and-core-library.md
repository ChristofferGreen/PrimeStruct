# Host Functions and Core Library Surface

> Part of the [PrimeStruct specification index](../PrimeStruct.md). Classification: **normative (draft)**.

### Host functions (embedding)

A script can call functions its embedding application provides. Declare each one
as a `host` definition: a primitive signature with an empty body.

```prime
[host return<int>]
host_add([i32] a, [i32] b) {
}

[host return<void>]
host_log([i32] value) {
}

[return<int>]
main() {
  [i32] total{host_add(40i32, 2i32)}
  host_log(total)
  return(total)
}
```

Rules:
- The return type is explicit (`return<T>`) and, like every parameter, one of
  `i32` (`int`), `i64`, `u64`, `f32` (`float`), `f64`, or `bool` (parameters may
  also be `string`; `void` is allowed for the return, strings are not except for the
  engine-reserved `__psarg_*` wrapper functions used by `Script::call`). Parameters cannot be `mut` or have defaults, and host definitions cannot
  be generic, struct members, or have a body.
- Each call lowers to `CallHost` (arguments on the stack, `imm` = index into the
  module's host import table; the import name is the definition path without the
  leading `/`). Expected IR for `host_add(40i32, 2i32)`:
  `PushI32 40`, `PushI32 2`, `CallHost 0`, with `host_imports[0] = host_add(i32, i32) -> i32`.
- Only the VM target (and `--emit=ir` serialized bytecode) accepts host calls;
  native, C++, wasm, and GLSL emission reject them with
  `host calls are only supported by the vm target and serialized bytecode`.
- The embedder binds every declared function, by name, with a matching signature
  (`Script::bind` in `docs/Embedding.md`). A missing or mismatched binding fails
  before any instruction runs. A declared host function that is never called adds
  no requirement.
- `primevm` has no bindings, so running a script that calls a host function there
  reports `unbound host function: <name>`.

### Core library surface (draft)
- **Standard math (draft):** the core math set lives under `/std/math/*` (e.g., `/std/math/sin`, `/std/math/pi`).
  `import /std/math/*` brings these names into the root namespace so `sin(...)`/`pi` resolve without qualification.
  Unsupported envelope/operation pairs produce diagnostics. Only fixed-width numeric envelopes are supported today;
  software numeric envelopes are parsed/typed but are rejected by current backends.
  - **Constants:** `/std/math/pi`, `/std/math/tau`, `/std/math/e`.
  - **Basic:** `/std/math/abs`, `/std/math/sign`, `/std/math/min`, `/std/math/max`, `/std/math/clamp`, `/std/math/lerp`,
    `/std/math/saturate`.
  - **Rounding:** `/std/math/floor`, `/std/math/ceil`, `/std/math/round`, `/std/math/trunc`, `/std/math/fract`.
  - **Power/log:** `/std/math/sqrt`, `/std/math/cbrt`, `/std/math/pow`, `/std/math/exp`, `/std/math/exp2`,
    `/std/math/log`, `/std/math/log2`, `/std/math/log10`.
  - **Operand rules (current):** `abs`, `sign`, `saturate`, `min`, `max`, `clamp`, `lerp`, and `pow` accept numeric
    operands (`i32`, `i64`, `u64`, `f32`, `f64`). `min`, `max`, `clamp`, `lerp`, and `pow` reject mixed signed/unsigned
    or mixed integer/float operands. All remaining `/std/math/*` builtins require float operands (`atan2`, `hypot`,
    `copysign` are binary; `fma` is ternary).
  - **Integer pow:** for integer operands, `pow` requires a non-negative exponent; negative exponents abort in VM/native
    (stderr + exit code `3`), and the C++ emitter mirrors this behavior.
  - **Trig:** `/std/math/sin`, `/std/math/cos`, `/std/math/tan`, `/std/math/asin`, `/std/math/acos`, `/std/math/atan`,
    `/std/math/atan2`, `/std/math/radians`, `/std/math/degrees`.
  - **Hyperbolic:** `/std/math/sinh`, `/std/math/cosh`, `/std/math/tanh`, `/std/math/asinh`, `/std/math/acosh`,
    `/std/math/atanh`.
  - **Float utils:** `/std/math/fma`, `/std/math/hypot`, `/std/math/copysign`.
  - **Predicates:** `/std/math/is_nan`, `/std/math/is_inf`, `/std/math/is_finite`.
  - **Backend limits (current):** VM/native math uses fast approximations and is validated against the C++/exe baseline
    within tolerances. Large-magnitude trig/log/exp inputs are only required to stay finite and within basic range
    bounds (e.g., `|sin(x)| <= 1`), and may diverge from the C++/exe baseline. Conformance tests use tolerance or range
    checks for these cases. Float matrix/quaternion reference suites currently use an absolute `1e-4` tolerance policy.
  - **Vector, color, matrix, quaternion types (draft):** stdlib ships `.prime` definitions for `Vec2`, `Vec3`, `Vec4`,
    `ColorRGB`, `ColorRGBA`, `ColorSRGB`, `ColorSRGBA`, `Mat2`, `Mat3`, `Mat4`, and `Quat`. These are distinct nominal
    types; colors are not aliases of vectors, matrices are not aliases of arrays/vectors, and quaternions are not
    aliases of `Vec4`.
    - **Vectors:** constructors, component accessors, and member methods like `length()`, `normalize()` (in-place), and
      `toNormalized()` (returns a new value).
    - **Colors:** constructors plus color-space helpers (e.g., sRGB/linear conversions) and per-channel ops. sRGB types
      remain distinct from linear `ColorRGB`/`ColorRGBA`.
    - **Matrices:** constructors plus direct public scalar component access via `mRC` field names (`m00`, `m01`, ...),
      where the first index is the row and the second index is the column.
    - **Quaternions:** constructors plus direct public scalar component access via `x`, `y`, `z`, and `w`, along with
      `toNormalized()` / `normalize()` helpers. The stdlib now also ships `quat_to_mat3(q)`, `quat_to_mat4(q)`, and
      `mat3_to_quat(m)`.
  - **Matrix/quaternion interaction contract (draft):**
    - No implicit conversion between scalar/vector/matrix/quaternion families; use explicit constructor/helper calls.
    - `plus`/`minus` require identical operand envelopes (`VecN` with same `N`, `MatRxC` with same shape, or `Quat` with
      `Quat`).
    - Current implementation status: semantics already enforces the `Mat*`/`Quat` `plus` and `minus` rules, the
      documented `Mat*`/`Quat` `multiply` allowlist, `Mat* / scalar` plus `Quat / scalar` divide validation, and
      deterministic binding/return/call diagnostics for implicit `Mat*`/`Quat` family conversions. VM/native, Wasm, and
      the C++ emitter now also lower component-wise `Mat2`/`Mat3`/`Mat4` and `Quat` `plus` + `minus`, scalar-left/right
      matrix/quaternion scaling, matrix/quaternion-by-scalar divide, matrix-vector multiply, matching matrix-matrix
      multiply, quaternion-quaternion Hamilton products, and quaternion-`Vec3` rotation through the documented contract.
      GLSL now lowers nominal `Vec2`/`Vec3`/`Vec4`, `Quat`, `Mat2`/`Mat3`/`Mat4` values, direct vector/quaternion/matrix
      field access, component-wise vector/quaternion `plus`/`minus`, vector/quaternion scalar scale/divide, `MatN *
      VecN` interop, matching matrix-matrix multiply, quaternion-quaternion Hamilton products, quaternion-`Vec3`
      rotation, and the explicit conversion helpers `quat_to_mat3`, `quat_to_mat4`, and `mat3_to_quat`.
    - `multiply` is allowed for: scalar scaling (`S * VecN`, `VecN * S`, `S * Mat`, `Mat * S`, `S * Quat`, `Quat * S`),
      matrix-vector (`Mat * VecN` with compatible inner dimension), matrix-matrix (`MatRxC * MatCxK`),
      quaternion-quaternion (Hamilton product), and quaternion-vector rotation (`Quat * Vec3`).
    - `divide` is allowed only as composite-by-scalar (`VecN / S`, `Mat / S`, `Quat / S`); scalar/composite and
      composite/composite division are diagnostics unless explicitly documented.
    - Matrix/vector multiplication uses a column-vector convention (`result = Mat * Vec`), and transform composition
      order follows canonical call nesting (`MatA * (MatB * Vec)`).
    - Conformance reference tests for float matrix/quaternion outputs use an absolute `1e-4` tolerance policy.
    - Equality is component-wise exact (`equal`, `not_equal`); tolerance-based comparison is explicit helper API, not
      operator sugar.
    - Scalar `/std/math/*` builtins remain scalar-only unless a specific builtin documents vector/matrix/quaternion
      overloads.
- **`assign(target, value)`:** canonical mutation primitive; only valid when `target` carried `mut` at declaration time.
  The call evaluates to `value`, so it can be nested or returned.
- **`increment(target)` / `decrement(target)`:** canonical mutation helpers used by `++`/`--` desugaring. Only valid on
  mutable numeric bindings; they evaluate to the updated value.
- **`count(value)` / `value.count()` / `contains(value, key)` / `at(value, index)` / `value.at(index)` / `value[index]`
  / `at_unsafe(value, index)` / `value.at_unsafe(index)`:** collection helpers. Method-call and index forms are
  preferred surface syntax where supported; helper-call forms are canonical equivalents. `contains` is currently
  map-only and returns `bool`. `at` performs bounds checking; `at_unsafe` does not. In VM/native backends, an
  out-of-bounds `at` aborts execution (prints a diagnostic to stderr and returns exit code `3`).
- **Runtime error reporting (VM/native):** runtime guards (bounds checks, missing map keys, invalid integer `pow`
  exponents, and the current vector dynamic-capacity guard) abort execution, print a diagnostic to stderr, and
  return exit code `3`. Parse/semantic/lowering errors remain exit code `2`.
- **`print(value)` / `print_line(value)` / `print_error(value)` / `print_line_error(value)`:** stdout/stderr output
  primitives (statement-only). `print`/`print_line` require `io_out`, and `print_error`/`print_line_error` require
  `io_err`. VM/native backends support integer/bool values plus string literals/bindings; other string operations still
  require the C++ emitter.
- **`plus`, `minus`, `multiply`, `divide`, `negate`:** arithmetic wrappers used after operator desugaring. Operands must
  be numeric (`i32`, `i64`, `u64`, `f32`, `f64`); bool/string/pointer operands are rejected. Mixed signed/unsigned
  integer operands are rejected in VM/native lowering (`u64` only combines with `u64`), and `negate` rejects unsigned
  operands. Pointer arithmetic is only defined for `plus`/`minus` with a pointer on the left and an integer offset (see
  Pointer arithmetic below).
- **`greater_than(left, right)`, `less_than(left, right)`, `greater_equal(left, right)`, `less_equal(left, right)`,
  `equal(left, right)`, `not_equal(left, right)`, `and(left, right)`, `or(left, right)`, `not(value)`:** comparison
  wrappers used after operator/control-flow desugaring. Comparisons respect operand signedness (`u64` uses unsigned
  ordering; `i32`/`i64` use signed ordering), and mixed signed/unsigned comparisons are rejected in the current
  IR/native subset; `bool` participates as a signed `0/1`, so `bool` with `u64` is rejected as mixed signedness. Boolean
  combinators accept `bool` inputs only. Control-flow conditions (`if`/`while`/`for`) require `bool` results; use
  comparisons or `bool{value}` when needed. The current IR/native subset accepts integer/bool/float operands for
  comparisons, plus `equal`/`not_equal` on two string operands (lowered to a byte-by-byte comparison shared across
  every backend); ordered string comparisons (`less_than`, `greater_than`, `less_equal`, `greater_equal`) still
  require the C++ emitter.
- **`/std/math/clamp(value, min, max)`:** numeric helper used heavily in rendering scripts. VM/native lowering supports
  integer clamps (`i32`, `i64`, `u64`) and float clamps (`f32`, `f64`) and follows the usual integer promotion rules
  (`i32` mixed with `i64` yields `i64`, while `u64` requires all operands to be `u64`). Mixed signed/unsigned clamps are
  rejected.
- **`if(condition, then() { ... }, else() { ... })`:** canonical conditional form after control-flow desugaring.
  - Signature: `if(Envelope, Envelope, Envelope)`
  - 1) must evaluate to a boolean (`bool`), either a boolean value or a function returning boolean
  - 2) must be a definition envelope; its body yields the `if` result when the condition is `true`
  - 3) must be a definition envelope; its body yields the `if` result when the condition is `false`
  - Surface `if(condition) { ... }` is statement-only sugar and lowers to `if(condition, then() { ... }, else() { })`.
  - Evaluation is lazy: the condition is evaluated first, then exactly one of the two definition bodies is evaluated.
- **`loop(count) { ... }`:** statement-only loop helper. `count` must be an integer envelope (`i32`, `i64`, `u64`), and
  the body is required. Negative counts are errors.
- **`while(condition) { ... }`:** statement-only loop helper. `condition` must evaluate to `bool` (or a function
  returning `bool`).
- **`for(init cond step) { ... }`:** statement-only loop helper. `init`, `cond`, and `step` are evaluated in order;
  `cond` must evaluate to `bool`. Bindings are allowed in any slot. Semicolons and commas are optional separators (e.g.,
  `for(init; cond; step)`).
- **Loop scope:** loop bodies default to per-iteration scope. Add `[shared_scope]` before the loop to share one scope
  across all iterations.
  - Example:
    ```
    [shared_scope]
    loop(3i32) {
      [i32 mut] total{0i32}
      assign(total, plus(total, 1i32))
    }
    ```

### Loop/For/While Examples (surface → canonical)
Surface forms:
```
loop(5i32) { work() }

while(i < 10i32) { work(i) }

for([i32 mut] i{0i32}; i < 10; ++i) {
  work(i)
}
```

Canonical (after desugaring):
```
loop(5i32, do() { work() })

while(less_than(i, 10i32), do() { work(i) })

for(
  [i32 mut] i{0i32},
  less_than(i, 10i32),
  increment(i),
  do() { work(i) }
)
```
- **`return(value)`:** explicit return primitive; may appear as a statement inside control-flow blocks. For `void`
  definitions, `return()` is allowed. Implicit `return(void)` fires at end-of-body when omitted. Non-void definitions
  must return on all control paths; fallthrough is a compile-time error. Inside value blocks (binding initializers /
  brace constructors), `return(value)` returns from the block and yields its value.
- **IR note:** VM/native IR lowering supports numeric/bool `array<T>{...}` and `vector<T>{...}` construction plus their
  current call-shaped compatibility helper paths, along with `count`/`at`/`at_unsafe` on those sequences. Map literals
  are supported in VM/native for numeric/bool values, and string-keyed maps work when the keys are string literals or
  bindings backed by literals (string table entries). VM/native vectors now lower through an explicit heap-backed
  record header (`count`, `capacity`, `data_ptr`): `push`/`reserve` reallocate backing storage while capacity remains
  within the current `1024` dynamic-capacity ceiling and error once exceeded, and shrinking helpers (`pop`, `clear`,
  `remove_at`, `remove_swap`) work against that heap-backed record. Vector element access/mutation uses `data_ptr`
  indirection instead of fixed in-header element offsets. Vector constructors above the current VM/native local capacity
  limit (`1024`) are rejected during lowering with `collection literal exceeds local capacity limit (1024)`,
  and `reserve` with out-of-range or negative integer literal expressions (including folded signed/unsigned
  `plus`/`minus`/`negate`, such as `plus(1000i32, 25i32)`, `minus(1u64, 2u64)`, `plus(18446744073709551615u64, 1u64)`, or
  `negate(1i32)`) is also rejected at lowering time (`vector reserve exceeds local capacity limit (1024)` / `vector
  reserve expects non-negative capacity` / `vector reserve literal expression overflow`). Folded signed and unsigned
  literal-expression overflow now emits the deterministic lowering diagnostic `vector reserve literal expression
  overflow` instead of deferring to backend/runtime integer-overflow behavior.
