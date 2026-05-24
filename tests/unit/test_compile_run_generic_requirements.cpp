#include "test_compile_run_helpers.h"

#include <string>
#include <string_view>
#include <vector>

TEST_SUITE_BEGIN("primestruct.compile.run.generic_requirements");

namespace {

void expectBackendsExit(std::string_view nameStem,
                        const std::string &source,
                        int expectedExit) {
  const std::string stem{nameStem};
  const std::string srcPath = writeTemp(stem + ".prime", source);
  const std::string exePath =
      (testScratchPath("") / ("primec_" + stem + "_exe")).string();
  const std::string nativePath =
      (testScratchPath("") / ("primec_" + stem + "_native")).string();

  const std::string compileCmd = "./primec --emit=exe " +
                                 quoteShellArg(srcPath) + " -o " +
                                 quoteShellArg(exePath) + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(quoteShellArg(exePath)) == expectedExit);

  const std::string vmCmd =
      "./primec --emit=vm " + quoteShellArg(srcPath) + " --entry /main";
  CHECK(runCommand(vmCmd) == expectedExit);

  const std::string nativeCmd = "./primec --emit=native " +
                                quoteShellArg(srcPath) + " -o " +
                                quoteShellArg(nativePath) +
                                " --entry /main";
  CHECK(runCommand(nativeCmd) == 0);
  CHECK(runCommand(quoteShellArg(nativePath)) == expectedExit);
}

void expectPrimecDiagnostic(std::string_view nameStem,
                            const std::string &source,
                            const std::vector<std::string> &needles) {
  const std::string stem{nameStem};
  const std::string srcPath = writeTemp(stem + ".prime", source);
  const std::string errPath =
      (testScratchPath("") / ("primec_" + stem + "_err.txt")).string();

  const std::string compileCmd =
      "./primec --emit=ir " + quoteShellArg(srcPath) + " --entry /main"
      " > /dev/null 2> " +
      quoteShellArg(errPath);
  CHECK(runCommand(compileCmd) == 2);
  const std::string error = readFile(errPath);
  INFO(error);
  for (const std::string &needle : needles) {
    CHECK(error.find(needle) != std::string::npos);
  }
}

} // namespace

TEST_CASE("generic same-type and value requirements execute across backends") {
  const std::string source = R"(
[return<T> require(typeof<left> == typeof<right>, N > 0)]
checked_sum<N,T>([T] left, [T] right) {
  return(plus(left, right))
}

[return<int>]
main() {
  return(checked_sum<3,i32>(6i32, 7i32))
}
)";

  expectBackendsExit("generic_requirements_same_type_value", source, 13);
}

TEST_CASE("constrained overload selection executes the only viable candidate") {
  const std::string source = R"(
[return<i32> require(type_equals<typeof<value>, i64>())]
classify<T>([T] value) {
  return(1i32)
}

[return<i32> require(type_equals<typeof<value>, i32>())]
classify<T>([T] value) {
  return(plus(value, 1i32))
}

[return<int>]
main() {
  return(classify(40i32))
}
)";

  expectBackendsExit("generic_requirements_overload_selection", source, 41);
}

TEST_CASE("selected ct_if branches execute and discarded branches stay inert") {
  const std::string source = R"(
[return<i32>]
pick_then<T>([T] value) {
  [i32 mut] result{0i32}
  ct_if(type_equals<typeof<value>, i32>()) {
    assign(result, plus(value, 2i32))
  } else {
    missing_runtime_or_lowering_path(value)
  }
  return(result)
}

[return<i32>]
pick_else<T>([T] value) {
  [i32] result{ct_if(type_equals<typeof<value>, i32>()) {
    missing_runtime_or_lowering_path(value)
  } else {
    5i32
  }}
  return(result)
}

[return<int>]
main() {
  return(plus(pick_then(15i32), pick_else(1.5f32)))
}
)";

  expectBackendsExit("generic_requirements_ct_if_selected", source, 22);
}

TEST_CASE("direct requirement failures include call and predicate provenance") {
  const std::string source = R"(
[return<T> require(typeof<value> == i32)]
only_i32<T>([T] value) {
  return(value)
}

[return<int>]
main() {
  [f32] bad{only_i32<f32>(4.0f32)}
  return(0i32)
}
)";

  expectPrimecDiagnostic(
      "generic_requirements_direct_failure",
      source,
      {"direct requirement check failed on /only_i32",
       "category: unsatisfied requirement predicate",
       "require transform: /only_i32",
       "predicate source: /std/meta/type_equals<typeof<value>, i32>()",
       "concrete facts:",
       "type_fact:typeof<value>",
       "type equality failed: f32 != i32",
       "hint: pass values or types that satisfy the require(...) predicate"});
}

TEST_CASE("constrained overload diagnostics cover ambiguity and value failures") {
  const std::string ambiguousSource = R"(
[return<T> require(type_equals<typeof<value>, i32>())]
choose<T>([T] value) {
  return(value)
}

[return<T> require(is_type<i32>())]
choose<T>([T] value) {
  return(value)
}

[return<int>]
main() {
  return(choose(4i32))
}
)";

  expectPrimecDiagnostic(
      "generic_requirements_overload_ambiguous",
      ambiguousSource,
      {"call site:",
       "ambiguous requirement overload for /choose",
       "viable candidates:",
       "requirements satisfied:",
       "concrete inferred argument facts:",
       "arg0 type=i32",
       "hint: make the overload requirements mutually exclusive"});

  const std::string valueFailureSource = R"(
[return<i32> require(N > 0)]
positive_index<N>() {
  return(1i32)
}

[return<int>]
main() {
  return(positive_index<0>())
}
)";

  expectPrimecDiagnostic(
      "generic_requirements_value_failure",
      valueFailureSource,
      {"requirement predicate not satisfied: /std/meta/value_greater",
       "category: unsatisfied requirement predicate",
       "require transform: /positive_index",
       "literal_compile_time_argument:0",
       "value predicate failed: 0 > 0",
       "hint: pass values or types that satisfy the require(...) predicate"});
}

TEST_CASE("compile-time effect rejections surface through primec diagnostics") {
  const std::string source = R"(
[effects(file_read), return<bool>]
needs_file() {
  return(true)
}

[return<int> require(needs_file())]
identity([int] value) {
  return(value)
}

[return<int>]
main() {
  return(identity(4i32))
}
)";

  expectPrimecDiagnostic(
      "generic_requirements_compile_time_effect",
      source,
      {"compile-time effect opt-in rejected: /needs_file",
       "category: denied compile-time effect",
       "denied compile-time effect in user requirement predicate "
       "/needs_file: file_read",
       "missing effects<compiletime>(file_read)",
       "hint: add effects<compiletime>(...) for the required "
       "compile-time host effect"});
}

TEST_SUITE_END();
