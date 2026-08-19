#include "../test_compile_run_helpers.h"

#include <utility>
#include <vector>

// Direct compile-and-run coverage for the language level examples under
// examples/. Complements the batch "compiles ... examples to IR" tests in
// test_compile_run_examples_docs.cpp (which parse+lower every example under
// 0.Concrete/, 1.Template/, 2.Inference/, and 3.Surface/) by additionally
// running each example through the VM and checking its expected exit code,
// so a behavior regression (not just a parse/lowering regression) is caught.
//
// examples/4.Transforms/ is intentionally not repeated here: both of its
// files (trace_calls_consumer.prime, trace_calls_transform.prime) already
// have dedicated VM-run coverage in
// "checked-in ast transform example runs in VM" (test_compile_run_examples_docs.cpp).

TEST_SUITE_BEGIN("primestruct.compile.run.examples");

namespace {

std::filesystem::path resolveExamplePath(const std::string &relativeName) {
  std::filesystem::path path = std::filesystem::path("..") / "examples" / relativeName;
  if (!std::filesystem::exists(path)) {
    path = std::filesystem::current_path() / "examples" / relativeName;
  }
  return path;
}

void checkExampleRunsWithExitCode(const std::string &relativeName, int expectedExitCode) {
  const std::filesystem::path examplePath = resolveExamplePath(relativeName);
  CAPTURE(relativeName);
  REQUIRE(std::filesystem::exists(examplePath));
  const std::string runVmCmd =
      "./primec --emit=vm " + quoteShellArg(examplePath.string()) + " --entry /main";
  CHECK(runCommand(runVmCmd) == expectedExitCode);
}

} // namespace

TEST_CASE("0.Concrete examples compile and run in the VM with expected exit codes") {
  const std::vector<std::pair<std::string, int>> examples = {
      {"0.Concrete/assign_mutable.prime", 6},
      {"0.Concrete/binding_value.prime", 7},
      {"0.Concrete/call_statement.prime", 7},
      {"0.Concrete/forward_definition.prime", 42},
      {"0.Concrete/hello_world.prime", 0},
      {"0.Concrete/identity_call.prime", 88},
      {"0.Concrete/namespace_call.prime", 19},
      {"0.Concrete/print_io.prime", 0},
      {"0.Concrete/return_123.prime", 123},
      {"0.Concrete/return_7.prime", 7},
      {"0.Concrete/return_struct.prime", 7},
      {"0.Concrete/return_void.prime", 0}};
  for (const auto &[relativeName, expectedExitCode] : examples) {
    checkExampleRunsWithExitCode(relativeName, expectedExitCode);
  }
}

TEST_CASE("1.Template examples compile and run in the VM with expected exit codes") {
  const std::vector<std::pair<std::string, int>> examples = {
      {"1.Template/math_pow.prime", 72}, {"1.Template/pointers_and_references.prime", 7}};
  for (const auto &[relativeName, expectedExitCode] : examples) {
    checkExampleRunsWithExitCode(relativeName, expectedExitCode);
  }
}

TEST_CASE("2.Inference examples compile and run in the VM with expected exit codes") {
  const std::vector<std::pair<std::string, int>> examples = {
      {"2.Inference/generic_identity.prime", 42},
      {"2.Inference/generic_pair_design.prime", 42},
      {"2.Inference/generic_requirements_design.prime", 50},
      {"2.Inference/infer_method_call.prime", 4},
      {"2.Inference/procedural_generic_box.prime", 23}};
  for (const auto &[relativeName, expectedExitCode] : examples) {
    checkExampleRunsWithExitCode(relativeName, expectedExitCode);
  }
}

TEST_CASE("3.Surface examples compile and run in the VM with expected exit codes") {
  const std::vector<std::pair<std::string, int>> examples = {
      {"3.Surface/argv.prime", 0},
      {"3.Surface/collections.prime", 1},
      {"3.Surface/collections_brackets.prime", 2},
      {"3.Surface/copy_constructor.prime", 0},
      {"3.Surface/features_overview.prime", 0},
      {"3.Surface/gpu_compute.prime", 14},
      {"3.Surface/hello_world_if.prime", 0},
      {"3.Surface/if_else.prime", 9},
      {"3.Surface/if_expression.prime", 5},
      {"3.Surface/loop_while_for.prime", 12},
      {"3.Surface/operator_plus.prime", 3},
      {"3.Surface/param_defaults.prime", 3},
      {"3.Surface/result_helpers.prime", 0},
      {"3.Surface/syntax_braces.prime", 7}};
  for (const auto &[relativeName, expectedExitCode] : examples) {
    checkExampleRunsWithExitCode(relativeName, expectedExitCode);
  }
}

TEST_CASE("3.Surface raytracer example compiles and renders a PPM frame via VM") {
  // Fixed alongside this test: the example used named-field struct
  // construction without the required braces (`Scene(...)` with
  // `[field] value` args), which the compiler rejects with
  // "struct construction requires braces". Kept as its own test case
  // (rather than folded into the table above) because rendering a 128x128
  // frame at 2x2 supersampling through the VM takes several seconds.
  checkExampleRunsWithExitCode("3.Surface/raytracer.prime", 0);
}

TEST_CASE("3.Surface soa_ecs example is pinned to its current known-broken compile state") {
  // soa_ecs.prime declares `particles` with `[auto mut]` bound to
  // `soa</Particle>()`. Method-target resolution on that auto-inferred
  // SoaVector</Particle> receiver currently fails semantic analysis with
  // "unknown call target: push" for `particles.push(...)` -- a tracked
  // SoA/auto-inference method-resolution gap (see docs/todo.md's SoA
  // same-path user shadow / receiver-precedence entries), not something
  // this test coverage leaf is meant to fix. This test pins the current
  // failure so a fix (or further regression) in that resolution logic is
  // caught here instead of silently drifting.
  const std::filesystem::path examplePath = resolveExamplePath("3.Surface/soa_ecs.prime");
  REQUIRE(std::filesystem::exists(examplePath));
  const std::string errPath = (testScratchPath("") / "primec_soa_ecs_known_failure.err.txt").string();
  const std::string compileCmd = "./primec --emit=ir " + quoteShellArg(examplePath.string()) +
                                  " --out-dir " + quoteShellArg((testScratchPath("") / "primec_soa_ecs_known_failure").string()) +
                                  " --entry /main > " + quoteShellArg(errPath) + " 2>&1";
  CHECK(runCommand(compileCmd) != 0);
  const std::string errorText = readFile(errPath);
  CHECK(errorText.find("unknown call target: push") != std::string::npos);
}
