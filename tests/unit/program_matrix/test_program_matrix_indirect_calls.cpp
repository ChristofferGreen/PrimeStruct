#include "program_matrix.h"

TEST_SUITE_BEGIN("primestruct.program_matrix.indirect_calls");

// The function address markers (primec/semantics/FunctionAddressDefinitions.h): `[function_address("/path")]` is the
// address of a definition and `[indirect_call]` calls through one, with the parameters of a real call. The programs run
// on the VM, `optexe` and, where it runs, native code and Wasm; the targets without the opcodes take their expansion.

namespace {

std::vector<std::string> indirectConfigs() {
  std::vector<std::string> configs = {"vm-step-O0", "vm-O0", "vm-O2", "optexe-O2", "exe"};
  if (program_matrix::nativeSupported()) {
    configs.push_back("native-O0");
    configs.push_back("native-O2");
  }
  return configs;
}

void runIndirectProgram(const std::string &name,
                        const std::string &source,
                        int exitCode,
                        bool wasm = true) {
  program_matrix::ProgramCase program;
  program.name = name;
  program.source = source;
  program.exitCode = exitCode;
  program.onlyConfigs = indirectConfigs();
  if (wasm) {
    program.extraConfigs = {"wasm-wasi"};
  }
  program_matrix::runProgramMatrix(program);
}

} // namespace

TEST_CASE("an indirect call enters the function its address names") {
  runIndirectProgram("indirect_call_scalars",
                     R"(
[return<i32>]
add_one([i32] x) { return(x + 1i32) }

[return<i32>]
double_it([i32] x) { return(x + x) }

[function_address("/add_one") return<u64>]
add_one_address() {}

[function_address("/double_it") return<u64>]
double_it_address() {}

[indirect_call return<i32>]
call_one([u64] address, [i32] x) {}

[return<int>]
main() {
  return(call_one(add_one_address(), 4i32) + call_one(double_it_address(), 4i32))
}
)",
                     13);
}

TEST_CASE("an address is a value chosen at run time") {
  runIndirectProgram("indirect_call_chosen_at_run_time",
                     R"(
[return<i32>]
add_one([i32] x) { return(x + 1i32) }

[return<i32>]
double_it([i32] x) { return(x + x) }

[function_address("/add_one") return<u64>]
add_one_address() {}

[function_address("/double_it") return<u64>]
double_it_address() {}

[indirect_call return<i32>]
call_one([u64] address, [i32] x) {}

[return<u64>]
pick_address([i32] which) {
  if(which == 0i32) {
    return(add_one_address())
  }
  return(double_it_address())
}

[return<int>]
main() {
  [u64] first{pick_address(0i32)}
  [u64] second{pick_address(1i32)}
  return(call_one(first, 10i32) * 100i32 + call_one(second, 10i32))
}
)",
                     1120);
}

TEST_CASE("an indirect call passes a struct and returns one") {
  runIndirectProgram("indirect_call_struct_value", R"(
[struct]
Point {
  [i32] x{0i32}
  [i32] y{0i32}
}

[return<Point>]
shift([Point] point) { return(Point{[x] point.x + 1i32, [y] point.y + 10i32}) }

[function_address("/shift") return<u64>]
shift_address() {}

[indirect_call return<Point>]
call_shift([u64] address, [Point] point) {}

[return<int>]
main() {
  [Point] moved{call_shift(shift_address(), Point{[x] 4i32, [y] 5i32})}
  return(moved.x * 100i32 + moved.y)
}
)",
                     515);
}

TEST_CASE("the address of a definition that cannot be a real call is refused") {
  program_matrix::ProgramCase program;
  program.name = "indirect_call_address_of_entry";
  program.source = R"(
[function_address("/main") return<u64>]
main_address() {}

[return<int>]
main() {
  [u64] address{main_address()}
  return(0i32)
}
)";
  program.exitCode = 2;
  program.stdoutText = "";
  program.vmStderr = "{program}:7:17: error: VM lowering error: function_address of /main_address names the entry "
                     "definition /main, which is not a function a program can call through an address [PSC2001]\n"
                     "note: backend: vm\n";
  program.onlyConfigs = {"vm-step-O0"};
  program_matrix::runProgramMatrix(program);
}

TEST_CASE("the address of a definition whose parameters no real call takes is refused") {
  program_matrix::ProgramCase program;
  program.name = "indirect_call_address_of_string_printer";
  program.source = R"(
[return<void> effects(io_out)]
say([string] text) { print_line(text) }

[function_address("/say") return<u64>]
say_address() {}

[return<int>]
main() {
  [u64] address{say_address()}
  return(0i32)
}
)";
  program.exitCode = 2;
  program.stdoutText = "";
  program.vmStderr = "{program}:10:17: error: VM lowering error: function_address of /say_address names /say, which "
                     "cannot be a real call on this target: its parameters or result do not fit the call ABI "
                     "[PSC2001]\nnote: backend: vm\n";
  program.onlyConfigs = {"vm-step-O0"};
  program_matrix::runProgramMatrix(program);
}

TEST_CASE("a real call whose struct result is discarded keeps its block inside the frame") {
  // `discard` ends in a call whose two-slot result nobody reads: the block is the last thing of its frame, so native
  // code, which sizes the frame by its highest local, must see a local after the block (TODO-6316).
  runIndirectProgram("discarded_struct_result_block", R"(
[struct]
Pair {
  [i32] first{0i32}
  [i32] second{0i32}
}

[return<Pair>]
make([i32 mut] total, [i32] x) {
  total = total + x
  total = total + 1i32
  return(Pair{[first] total, [second] x})
}

[return<void>]
discard([i32 mut] total, [i32] x) {
  total = total + x
  total = total + 1i32
  total = total + 2i32
  make(total, x)
}

[return<int>]
main() {
  [i32 mut] total{1i32}
  discard(total, 3i32)
  discard(total, 4i32)
  return(total)
}
)",
                     23);
}

TEST_CASE("an indirect call writes a place of the caller") {
  runIndirectProgram("indirect_call_mut_place", R"(
[return<void>]
bump([i32 mut] value) { value = value + 5i32 }

[function_address("/bump") return<u64>]
bump_address() {}

[indirect_call return<void>]
call_bump([u64] address, [i32 mut] value) {}

[return<int>]
main() {
  [i32 mut] count{4i32}
  call_bump(bump_address(), count)
  call_bump(bump_address(), count)
  return(count)
}
)",
                     14);
}

namespace {

// The address is chosen at run time, so it is not folded: 0 is no function and 999 is past the table.
void runFaultingIndirectProgram(const std::string &name, const std::string &address) {
  runIndirectProgram(name,
                     R"(
[return<i32>]
add_one([i32] x) { return(x + 1i32) }

[function_address("/add_one") return<u64>]
add_one_address() {}

[indirect_call return<i32>]
call_one([u64] address, [i32] x) {}

[return<u64>]
bad_address([i32] which) {
  if(which == 0i32) {
    return(add_one_address())
  }
  return()" + address + R"()
}

[return<int>]
main() {
  return(call_one(bad_address(1i32), 4i32))
}
)",
                     3,
                     false);
}

} // namespace

TEST_CASE("a call through a null address ends the program with exit code 3") {
  runFaultingIndirectProgram("indirect_call_null_address", "0u64");
}

TEST_CASE("a call through an address past the table ends the program with exit code 3") {
  runFaultingIndirectProgram("indirect_call_address_past_table", "999u64");
}

namespace {

// Wasm has no runtime-error exit for a bad address: `call_indirect` traps, which the node runner reports as 99.
void runWasmTrappingIndirectProgram(const std::string &name, const std::string &address) {
  program_matrix::ProgramCase program;
  program.name = name;
  program.source = R"(
[return<i32>]
add_one([i32] x) { return(x + 1i32) }

[function_address("/add_one") return<u64>]
add_one_address() {}

[indirect_call return<i32>]
call_one([u64] address, [i32] x) {}

[return<u64>]
bad_address([i32] which) {
  if(which == 0i32) {
    return(add_one_address())
  }
  return()" + address + R"()
}

[return<int>]
main() {
  return(call_one(bad_address(1i32), 4i32))
}
)";
  program.exitCode = 99;
  program.onlyConfigs = {"wasm-wasi"};
  program_matrix::runProgramMatrix(program);
}

} // namespace

TEST_CASE("wasm traps on a call through a null address") {
  runWasmTrappingIndirectProgram("indirect_call_wasm_null_address", "0u64");
}

TEST_CASE("wasm traps on a call through an address past the table") {
  runWasmTrappingIndirectProgram("indirect_call_wasm_address_past_table", "999u64");
}

TEST_CASE("an indirect call passes floats and a 64-bit integer") {
  runIndirectProgram("indirect_call_mixed_parameter_types",
                     R"(
[return<f64>]
blend([f64] a, [i64] n, [f32] b) { return(a * convert<f64>(n) + convert<f64>(b)) }

[return<i32>]
bump([i32] x) { return(x + 1i32) }

[function_address("/blend") return<u64>]
blend_address() {}

[function_address("/bump") return<u64>]
bump_address() {}

[indirect_call return<f64>]
call_blend([u64] address, [f64] a, [i64] n, [f32] b) {}

[indirect_call return<i32>]
call_bump([u64] address, [i32] x) {}

[return<int>]
main() {
  [f64] value{call_blend(blend_address(), 2.5f64, 4i64, 1.5f32)}
  return(convert<int>(value) + call_bump(bump_address(), 0i32))
}
)",
                     12);
}

// A `Function` call is a call through an address with the signature of its instance, so struct, `Reference` and struct
// result parameters take the address ABI on every target that runs it, `exe` and `cpp` included.
TEST_CASE("a Function takes a struct and a Reference parameter") {
  runIndirectProgram("function_struct_and_reference_parameters",
                     R"(
[struct]
State() {
  [i32 mut] total{0i32}
}

[struct]
Event() {
  [i32] x{0i32}
  [i32] y{0i32}
}

[return<void>]
bump([Reference<State> mut] state, [Event] event) {
  assign(state.total, state.total + event.x + event.y)
}

[return<int> effects(heap_alloc)]
main() {
  [State mut] state{State{}}
  [Function<void, Reference<State>, Event> mut] handler{bump}
  handler(state, Event{3i32, 4i32})
  handler(state, Event{1i32, 1i32})
  return(state.total)
}
)",
                     9,
                     false);
}

TEST_CASE("a Function returns a struct and holds a lambda without captures") {
  runIndirectProgram("function_struct_result_and_lambda",
                     R"(
[struct]
Point() {
  [i32] x{0i32}
  [i32] y{0i32}
}

[return<Point>]
shift([Point] point) {
  return(Point{[x] point.x + 1i32, [y] point.y + 10i32})
}

[return<int> effects(heap_alloc)]
main() {
  [Function<Point, Point> mut] move_point{shift}
  [Function<i32, i32> mut] twice{Function<i32, i32>{[]([i32] x) { return(x * 2i32) }}}
  [Point] moved{move_point(Point{[x] 4i32, [y] 5i32})}
  return(moved.x * 100i32 + moved.y + twice(3i32))
}
)",
                     521,
                     false);
}
