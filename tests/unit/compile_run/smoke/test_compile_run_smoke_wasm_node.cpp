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
const instance = new WebAssembly.Instance(new WebAssembly.Module(fs.readFileSync(process.argv[2])), wasi.getImportObject());
if (instance.exports.memory) {
  wasi.initialize(instance);
}
process.exitCode = Number(BigInt.asUintN(8, BigInt(instance.exports.main())));
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

  const int vmExit = runCommand("./primec --emit=vm " + quoteShellArg(srcPath) +
                                " --entry /main > " + quoteShellArg(vmOut));
  CHECK(vmExit == expectedExitCode);
  REQUIRE(runCommand("./primec --emit=wasm --wasm-profile wasi " + quoteShellArg(srcPath) + " -o " +
                     quoteShellArg(wasmPath) + " --entry /main") == 0);
  const int wasmExit =
      runCommand("node " + quoteShellArg(runnerPath) + " " + quoteShellArg(wasmPath) + " > " +
                 quoteShellArg(wasmOut) + " 2> " + quoteShellArg(wasmErr));
  CHECK_MESSAGE(wasmExit == expectedExitCode, readFile(wasmErr));
  CHECK(readFile(wasmOut) == readFile(vmOut));
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

TEST_SUITE_END();
