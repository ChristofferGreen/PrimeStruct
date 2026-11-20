#include "../test_compile_run_helpers.h"

#include "test_compile_run_smoke_helpers.h"

TEST_SUITE_BEGIN("primestruct.compile.run.smoke");

// These cases run the Wasm output under Node's built-in WebAssembly and WASI (nothing to install) and compare it with
// the VM, which the program matrix pins to the language rules: the same stdout and the same exit code. A machine
// without Node skips them with a message.

namespace {

// Calls the module's exported `main`; the exit code is its integer result modulo 256, which is what a native run of the
// program exits with.
constexpr const char *WasmNodeRunner = R"(
"use strict";
const fs = require("node:fs");
process.removeAllListeners("warning");
process.on("warning", () => {});
const { WASI } = require("node:wasi");
const wasi = new WASI({ version: "preview1", args: [process.argv[2]], returnOnExit: true });
// `proc_exit(code)` ends the program (a runtime error calls it with 3): unwind out of the module with the code.
class ProgramExit { constructor(code) { this.code = code; } }
const imports = wasi.getImportObject();
imports.wasi_snapshot_preview1.proc_exit = (code) => { throw new ProgramExit(code); };
const instance = new WebAssembly.Instance(new WebAssembly.Module(fs.readFileSync(process.argv[2])), imports);
if (instance.exports.memory) {
  wasi.initialize(instance);
}
try {
  process.exitCode = Number(BigInt.asUintN(8, BigInt(instance.exports.main())));
} catch (e) {
  if (!(e instanceof ProgramExit)) {
    // A bare wasm trap is not how the WASI profile ends a program.
    console.error("unexpected " + e);
    process.exit(99);
  }
  process.exitCode = e.code;
}
)";

// Under the browser profile a runtime error is a wasm trap (no WASI to call): the host sees a RuntimeError.
constexpr const char *WasmBrowserRunner = R"(
"use strict";
const fs = require("node:fs");
const compiled = new WebAssembly.Module(fs.readFileSync(process.argv[2]));
if (WebAssembly.Module.imports(compiled).length !== 0) {
  console.error("the browser profile imports from the host");
  process.exit(98);
}
const instance = new WebAssembly.Instance(compiled, {});
try {
  process.exitCode = Number(BigInt.asUintN(8, BigInt(instance.exports.main())));
} catch (e) {
  if (!(e instanceof WebAssembly.RuntimeError)) {
    throw e;
  }
  console.log("trapped");
  process.exitCode = 3;
}
)";

bool nodeAvailable() {
  if (hasNode()) {
    return true;
  }
  MESSAGE("SKIP: node is unavailable; the Wasm output was not run");
  return false;
}

// Compiles `source` for the VM and for Wasm (WASI), runs both and checks that they print and exit alike.
void checkWasmMatchesVm(const std::string &name, const std::string &source, int expectedExitCode) {
  if (!nodeAvailable()) {
    return;
  }
  const std::filesystem::path root = testScratchPath("") / ("primec_wasm_node_" + name);
  std::error_code ec;
  std::filesystem::remove_all(root, ec);
  std::filesystem::create_directories(root, ec);
  REQUIRE(!ec);

  const std::string srcPath = writeTemp("compile_wasm_node_" + name + ".prime", source);
  const std::string wasmPath = (root / "program.wasm").string();
  const std::string runnerPath = writeTemp("compile_wasm_node_runner.cjs", WasmNodeRunner);
  const std::string vmOut = (root / "vm_stdout.txt").string();
  const std::string wasmOut = (root / "wasm_stdout.txt").string();
  const std::string wasmErr = (root / "wasm_stderr.txt").string();
  const std::string vmErr = (root / "vm_stderr.txt").string();

  const int vmExit =
      runCommand("./primec --emit=vm " + quoteShellArg(srcPath) + " --entry /main > " +
                 quoteShellArg(vmOut) + " 2> " + quoteShellArg(vmErr));
  CHECK(vmExit == expectedExitCode);
  REQUIRE(runCommand("./primec --emit=wasm --wasm-profile wasi " + quoteShellArg(srcPath) + " -o " +
                     quoteShellArg(wasmPath) + " --entry /main") == 0);
  const int wasmExit =
      runCommand("node " + quoteShellArg(runnerPath) + " " + quoteShellArg(wasmPath) + " > " +
                 quoteShellArg(wasmOut) + " 2> " + quoteShellArg(wasmErr));
  CHECK_MESSAGE(wasmExit == expectedExitCode, readFile(wasmErr));
  CHECK(readFile(wasmOut) == readFile(vmOut));
  CHECK(readFile(wasmErr) == readFile(vmErr));
}

// Compiles `source` for the browser profile and runs it with no WASI host: a runtime error traps.
void checkWasmBrowserRuntimeError(const std::string &name,
                                  const std::string &source,
                                  const std::string &expectedStdout) {
  if (!nodeAvailable()) {
    return;
  }
  const std::filesystem::path root = testScratchPath("") / ("primec_wasm_browser_" + name);
  std::error_code ec;
  std::filesystem::remove_all(root, ec);
  std::filesystem::create_directories(root, ec);
  REQUIRE(!ec);
  const std::string srcPath = writeTemp("compile_wasm_browser_" + name + ".prime", source);
  const std::string wasmPath = (root / "program.wasm").string();
  const std::string runnerPath = writeTemp("compile_wasm_browser_runner.cjs", WasmBrowserRunner);
  const std::string out = (root / "stdout.txt").string();
  REQUIRE(runCommand("./primec --emit=wasm --wasm-profile browser --default-effects=none " +
                     quoteShellArg(srcPath) + " -o " + quoteShellArg(wasmPath) +
                     " --entry /main") == 0);
  CHECK(runCommand("node " + quoteShellArg(runnerPath) + " " + quoteShellArg(wasmPath) + " > " +
                   quoteShellArg(out)) == 3);
  CHECK(readFile(out) == expectedStdout);
}

std::string conversionProgram(const std::vector<std::vector<std::string>> &rows) {
  std::string body;
  int index = 0;
  for (const std::vector<std::string> &row : rows) {
    const std::string var = "v" + std::to_string(index++);
    body += "  [" + row[0] + " mut] " + var + "{" + row[1] + "}\n";
    // A fourth entry converts through that type first (print_line takes no floats).
    std::string value = var;
    if (row.size() > 3) {
      value = "convert<" + row[3] + ">(" + var + ")";
    }
    body += "  print_line(convert<" + row[2] + ">(" + value + "))\n";
  }
  return "[effects(io_out), return<int>]\nmain() {\n" + body + "  return(0i32)\n}\n";
}

} // namespace

TEST_CASE("wasm runs integer narrowing and widening like the vm") {
  checkWasmMatchesVm("int_conversions",
                     conversionProgram({{"i64", "4294967297i64", "i32"},
                                        {"i64", "4294967295i64", "i32"},
                                        {"i64", "2147483648i64", "i32"},
                                        {"i64", "-2147483649i64", "i32"},
                                        {"i64", "9223372036854775807i64", "i32"},
                                        {"u64", "18446744073709551615u64", "i32"},
                                        {"u64", "9223372036854775808u64", "i32"},
                                        {"i32", "-7i32", "i64"},
                                        {"i32", "2147483647i32", "i64"},
                                        {"i32", "-2147483648i32", "i64"},
                                        {"i32", "-1i32", "u64"},
                                        {"i32", "5i32", "u64"}}),
                     0);
}

TEST_CASE("wasm runs float to integer and integer to float conversions like the vm") {
  checkWasmMatchesVm("float_conversions",
                     conversionProgram({{"f64", "3.9f64", "i32"},
                                        {"f64", "-3.9f64", "i64"},
                                        {"f64", "1.0e30f64", "i32"},
                                        {"f64", "0.0f64 / 0.0f64", "i32"},
                                        {"f32", "-1.0f32", "u64"},
                                        {"f32", "4294967296.0f32", "i64"},
                                        {"i64", "9007199254740993i64", "i64", "f64"},
                                        {"i32", "-5i32", "i32", "f32"},
                                        {"u64", "18446744073709551615u64", "u64", "f64"},
                                        {"f32", "1.5f32", "i32", "f64"},
                                        {"f64", "0.1f64", "i32", "f32"}}),
                     0);
}

TEST_CASE("wasm runs mixed width locals, loops and conditions like the vm") {
  checkWasmMatchesVm("mixed_widths",
                     R"(
[return<i64>]
widen_sum([i64] a, [i32] b) {
  return(plus(a, convert<i64>(b)))
}

[return<i32>]
narrow_twice([i64] a) {
  return(convert<i32>(plus(a, a)))
}

[return<f64>]
half([f64] x) {
  return(divide(x, 2.0f64))
}

[effects(io_out), return<int>]
main() {
  [i32 mut] small{-7i32}
  [i64 mut] wide{convert<i64>(small)}
  print_line(wide)
  wide = plus(wide, 5000000000i64)
  print_line(wide)
  print_line(widen_sum(wide, small))
  print_line(narrow_twice(wide))
  [i64 mut] acc{0i64}
  [i32 mut] i{0i32}
  while(less_than(i, 4i32)) {
    acc = plus(acc, convert<i64>(i))
    acc = plus(acc, 1i64)
    i = plus(i, 1i32)
  }
  print_line(acc)
  [f64] h{half(7.0f64)}
  print_line(convert<i32>(h))
  if(greater_than(wide, 100i64), then() { print_line(1i32) }, else() { print_line(0i32) })
  return(convert<i32>(acc))
}
)",
                     10);
}

TEST_CASE("wasm runs a recursive call with i64 and i32 parameters like the vm") {
  checkWasmMatchesVm("recursive_widths",
                     R"(
[return<i64>]
fact([i64] n, [i32] depth) {
  if(less_than(n, 2i64), then() { return(convert<i64>(depth)) }, else() { })
  return(multiply(n, fact(minus(n, 1i64), plus(depth, 1i32))))
}

[effects(io_out), return<int>]
main() {
  [i32] small{5i32}
  print_line(fact(convert<i64>(small), 0i32))
  print_line(fact(20i64, 0i32))
  return(0i32)
}
)",
                     0);
}

// A Result-returning entry exits with the code decoded from its value (docs/spec/errors-and-file-io.md): the Wasm
// module's `main` returns that code.
TEST_CASE("wasm exits with the decoded code of a builtin value Result entry") {
  checkWasmMatchesVm("packed_value_ok",
                     R"(
[return<Result<i32, i32>>]
main() {
  return(Result.ok(5i32))
}
)",
                     5);
  checkWasmMatchesVm("packed_value_ok_large",
                     R"(
[return<Result<i32, i32>>]
main() {
  return(Result.ok(300i32))
}
)",
                     44);
}

// One case per program: each runs three commands (the VM, the Wasm compile and Node), and a case is a unit of liveness.
TEST_CASE("wasm exits with the decoded code of a builtin status Result entry") {
  checkWasmMatchesVm("packed_status_error",
                     R"(
[return<Result<i32>>]
main() {
  return(4i32)
}
)",
                     4);
}

TEST_CASE("wasm exits with a builtin status Result entry code that is too large") {
  checkWasmMatchesVm("packed_status_large",
                     R"(
[return<Result<i32>>]
main() {
  return(300i32)
}
)",
                     1);
}

TEST_CASE("wasm exits with zero for an ok builtin status Result entry") {
  checkWasmMatchesVm("packed_status_ok",
                     R"(
[return<Result<i32>>]
main() {
  return(0i32)
}
)",
                     0);
}

TEST_CASE("wasm renumbers the jumps of an entry around the status decode") {
  // Two returns, one inside a branch, with output before them: the jumps of the entry are renumbered around the decode.
  checkWasmMatchesVm("packed_status_branches",
                     R"(
[effects(io_out), return<Result<i32>>]
main() {
  print_line("before"utf8)
  [i32 mut] n{4i32}
  if(greater_than(n, 3i32), then() { return(7i32) }, else() { })
  print_line("after"utf8)
  return(0i32)
}
)",
                     7);
}

// The inputs of the program matrix case "fma rounds once for f64 and f32": a multiply followed by an add rounds twice
// and fails the first checks, the fused result passes them.
TEST_CASE("wasm computes fma with one rounding like the vm") {
  const std::vector<std::string> checks = {
      "fma(1.0f64 + e, 1.0f64 - e, -1.0f64) == negate(tiny)",
      "fma(1.0f64 - e, 1.0f64 + e, -1.0f64) == negate(tiny)",
      "fma(1.0f64 + e, 1.0f64 + e, -1.0f64 - e - e) == tiny",
      "fma(2.0f64, 3.0f64, 4.0f64) == 10.0f64",
      "fma(0.1f64, 10.0f64, -1.0f64) == 0.000000000000000055511151231257827f64",
      "fma(1.0f32 + e32, 1.0f32 - e32, -1.0f32) == negate(tiny32)",
      "fma(2.0f32, 3.0f32, 4.0f32) == 10.0f32",
      "fma(a, b, 1.0f32) == 1.0f32 + 0.00000011920928955078125f32",
      "fma(b, a, 1.0f32) == 1.0f32 + 0.00000011920928955078125f32",
      "fma(inf, 1.0f64, 2.0f64) == inf",
      "fma(inf, 0.0f64, 1.0f64) != fma(inf, 0.0f64, 1.0f64)",
      "fma(inf, 1.0f64, negate(inf)) != fma(inf, 1.0f64, negate(inf))",
      "fma(1.0e200f64, 1.0e200f64, negate(inf)) == negate(inf)"};
  std::string source = R"(
import /std/math/*

[return<int> effects(io_out)]
main() {
  [f64] inf{1.0f64 / 0.0f64}
  [f64] e{1.0f64 / 1073741824.0f64}
  [f64] tiny{e * e}
  [f32] e32{1.0f32 / 8192.0f32}
  [f32] tiny32{e32 * e32}
  [f32] a{8391483.0f32 / 34359738368.0f32}
  [f32] b{8385734.0f32 / 34359738368.0f32}
)";
  for (const std::string &check : checks) {
    source += "  if(" + check + ", then() { print_line(1i32) }, else() { print_line(0i32) })\n";
  }
  source += "  return(0i32)\n}\n";
  checkWasmMatchesVm("fma_fused", source, 0);
}

// An `if` expression leaves its arm's value on the stack at the join, so the Wasm `if` block has that value's type.
// `round` lowers to nested if/else arms that leave the enclosing arm early (a jump to the end of the construct).
TEST_CASE("wasm runs round and its early exits from an if arm like the vm") {
  checkWasmMatchesVm("round_early_exit",
                     R"(
import /std/math/*

[return<int> effects(io_out)]
main() {
  [f64] up{2.5f64}
  [f64] down{-2.5f64}
  [f64] small{0.4f64}
  print_line(convert<i32>(round(up)))
  print_line(convert<i32>(round(down)))
  print_line(convert<i32>(round(small)))
  print_line(convert<i32>(round(-0.5f64)))
  return(convert<i32>(round(up)))
}
)",
                     3);
}

TEST_CASE("wasm runs a value yielding if with i32 arms and calls like the vm") {
  checkWasmMatchesVm("value_if_i32",
                     R"(
[return<i32>]
double_it([i32] x) {
  return(plus(x, x))
}

[return<i32>]
pick_one([bool] ok) {
  return(if(ok, then() { 1i32 }, else() { 0i32 }))
}

[return<i32>]
classify([i32] n) {
  return(if(less_than(n, 0i32),
    then() { if(equal(n, -1i32), then() { double_it(10i32) }, else() { -5i32 }) },
    else() { if(equal(n, 0i32), then() { 100i32 }, else() { double_it(plus(n, 1i32)) }) }))
}

[return<i32>]
sum_of_ifs([i32] n) {
  return(plus(if(greater_than(n, 0i32), then() { n }, else() { 0i32 }), if(greater_than(n, 5i32), then() { 1000i32 }, else() { 1i32 })))
}

[effects(io_out), return<int>]
main() {
  print_line(pick_one(true))
  print_line(pick_one(false))
  print_line(classify(-1i32))
  print_line(classify(-7i32))
  print_line(classify(0i32))
  print_line(classify(4i32))
  print_line(sum_of_ifs(3i32))
  print_line(sum_of_ifs(9i32))
  print_line(sum_of_ifs(-2i32))
  [i32 mut] total{0i32}
  [i32 mut] i{0i32}
  while(less_than(i, 6i32)) {
    total = plus(total, if(less_than(i, 3i32), then() { double_it(i) }, else() { 1i32 }))
    i = plus(i, 1i32)
  }
  print_line(total)
  return(0i32)
}
)",
                     0);
}

TEST_CASE("wasm runs a value yielding if with i64 and f64 arms like the vm") {
  checkWasmMatchesVm("value_if_wide_float",
                     R"(
[return<i64>]
wide_pick([bool] ok, [i64] big) {
  return(if(ok, then() { plus(big, 1i64) }, else() { 7i64 }))
}

[return<i64>]
mixed_pick([bool] ok, [i32] small) {
  return(if(ok, then() { convert<i64>(small) }, else() { 9000000000i64 }))
}

[return<f64>]
float_pick([bool] ok, [f64] x) {
  return(if(ok, then() { multiply(x, 2.0f64) }, else() { 0.5f64 }))
}

[return<f32>]
float32_pick([bool] ok) {
  return(if(ok, then() { 1.5f32 }, else() { 2.5f32 }))
}

[effects(io_out), return<int>]
main() {
  print_line(wide_pick(true, 4294967296i64))
  print_line(wide_pick(false, 4294967296i64))
  print_line(mixed_pick(true, -3i32))
  print_line(mixed_pick(false, 1i32))
  print_line(convert<i32>(float_pick(true, 4.25f64)))
  print_line(convert<i32>(multiply(float_pick(false, 1.0f64), 10.0f64)))
  print_line(convert<i32>(multiply(float32_pick(true), 10.0f32)))
  return(0i32)
}
)",
                     0);
}

// A runtime error in a real call ends the program with exit code 3 (WASI `proc_exit`): the output before it is out, the
// statement after the call does not run, and stderr carries the VM's message.
TEST_CASE("wasm ends the program on a runtime error in a real call like the vm") {
  checkWasmMatchesVm("runtime_error_real_call",
                     R"(
import /std/math/*

[return<i32>]
power_of_two([i32] exponent) {
  [i32] shifted{exponent + 0i32}
  [i32] base{2i32 * 1i32}
  [i32] result{pow(base, shifted)}
  [i32] doubled{result + result}
  return(doubled - result)
}

[effects(io_out), return<int>]
main() {
  print_line(power_of_two(3i32))
  print_line(power_of_two(0i32 - 1i32))
  print_line(99i32)
  return(0i32)
}
)",
                     3);
}

TEST_CASE("wasm ends the program on a runtime error in a nested real call") {
  checkWasmMatchesVm("runtime_error_nested_call",
                     R"(
import /std/math/*

[return<i32>]
power_of_two([i32] exponent) {
  [i32] shifted{exponent + 0i32}
  [i32] base{2i32 * 1i32}
  [i32] result{pow(base, shifted)}
  [i32] doubled{result + result}
  return(doubled - result)
}

[return<i32>]
scaled([i32] exponent, [i32] factor) {
  return(power_of_two(exponent) * factor)
}

[effects(io_out), return<int>]
main() {
  print_line(scaled(2i32, 5i32))
  [i32] bad{scaled(0i32 - 4i32, 5i32)}
  print_line(bad)
  return(0i32)
}
)",
                     3);
}

TEST_CASE("wasm browser profile traps on a runtime error in a real call") {
  checkWasmBrowserRuntimeError("runtime_error_trap",
                               R"(
import /std/math/*

[return<i32>]
power_of_two([i32] exponent) {
  [i32] shifted{exponent + 0i32}
  [i32] base{2i32 * 1i32}
  [i32] result{pow(base, shifted)}
  [i32] doubled{result + result}
  return(doubled - result)
}

[return<int>]
main() {
  [i32] first{power_of_two(3i32)}
  [i32] second{power_of_two(0i32 - 1i32)}
  return(first + second + 100i32)
}
)",
                               "trapped\n");
}

// The browser profile keeps the raw wasm division: a zero divisor is a trap and the module imports nothing.
TEST_CASE("wasm browser profile traps on integer division by zero") {
  checkWasmBrowserRuntimeError("division_by_zero_trap",
                               R"(
[return<int>]
main() {
  [i32 mut] zero{0i32}
  [i32] value{10i32}
  return(value / zero)
}
)",
                               "trapped\n");
}

// GLSL has no trap and no host to report to, so a runtime error inside a real call is refused at compile time instead
// of letting the program carry on past the failed call.
TEST_CASE("glsl refuses a runtime error inside a real call") {
  const std::string srcPath = writeTemp("compile_glsl_runtime_error_call.prime",
                                        R"(
import /std/math/*

[return<i32>]
power_of_two([i32] exponent) {
  [i32] shifted{exponent + 0i32}
  [i32] base{2i32 * 1i32}
  [i32] result{pow(base, shifted)}
  [i32] doubled{result + result}
  return(doubled - result)
}

[return<int>]
main() {
  [i32] first{power_of_two(3i32)}
  [i32] second{power_of_two(0i32 - 1i32)}
  return(first + second + 100i32)
}
)");
  const std::string outPath =
      (testScratchPath("") / "primec_glsl_runtime_error_call.glsl").string();
  const std::string errPath = (testScratchPath("") / "primec_glsl_runtime_error_call.err").string();
  CHECK(runCommand("./primec --emit=glsl --default-effects=none " + quoteShellArg(srcPath) +
                   " -o " + quoteShellArg(outPath) + " --entry /main 2> " +
                   quoteShellArg(errPath)) != 0);
  CHECK(readFile(errPath).find("cannot end the program on this target") != std::string::npos);
}

TEST_SUITE_END();
