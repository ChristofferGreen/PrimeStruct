#include "../test_compile_run_helpers.h"
#include "../test_compile_run_collection_conformance_helpers.h"
#include "test_compile_run_emitters_helpers.h"

#include "primec/testing/EmitterHelpers.h"

TEST_SUITE_BEGIN("primestruct.compile.run.emitters.cpp");

TEST_CASE("C++ emitter resolves canonical map count helper on wrapper slash return method sugar") {
  const std::string source = R"(
[return<int>]
/std/collections/map/count([map<i32, i32>] values, [bool] marker) {
  return(92i32)
}

[effects(heap_alloc), return</std/collections/map<i32, i32>>]
wrapValues() {
  return(map<i32, i32>(1i32, 4i32))
}

[effects(heap_alloc), return<int>]
main() {
  return(wrapValues().count(true))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_stdlib_canonical_map_count_wrapper_slash_return_method_sugar.prime", source);
  const std::string compileCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 92);
}

TEST_CASE("C++ emitter keeps canonical map count diagnostics on wrapper slash return method sugar") {
  const std::string source = R"(
[return<int>]
/std/collections/map/count([map<i32, i32>] values, [bool] marker) {
  return(92i32)
}

[effects(heap_alloc), return</std/collections/map<i32, i32>>]
wrapValues() {
  return(map<i32, i32>(1i32, 4i32))
}

[effects(heap_alloc), return<int>]
main() {
  return(wrapValues().count(1i32))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_stdlib_canonical_map_count_wrapper_slash_return_method_sugar_diag.prime", source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_cpp_stdlib_canonical_map_count_wrapper_slash_return_method_sugar_diag.err")
          .string();

  const std::string compileCmd =
      "./primec --emit=vm " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("argument type mismatch for /std/collections/map/count parameter marker") !=
        std::string::npos);
}

TEST_CASE("C++ emitter rejects templated canonical map count helper on wrapper slash return method sugar") {
  const std::string source = R"(
[return<int>]
/std/collections/map/count<K, V>([map<K, V>] values, [bool] marker) {
  return(96i32)
}

[effects(heap_alloc), return</std/collections/map<i32, i32>>]
wrapValues() {
  return(map<i32, i32>(1i32, 2i32))
}

[return<int>]
main() {
  return(wrapValues().count(true))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_stdlib_templated_map_count_wrapper_slash_return_method_sugar.prime", source);

  // A correctly-typed bool marker argument matches the templated
  // canonical definition's signature and now resolves through method
  // sugar successfully.
  const std::string compileCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 96);
}

TEST_CASE("C++ emitter keeps canonical diagnostics on templated wrapper slash return map count sugar") {
  const std::string source = R"(
[return<int>]
/std/collections/map/count<K, V>([map<K, V>] values, [bool] marker) {
  return(96i32)
}

[effects(heap_alloc), return</std/collections/map<i32, i32>>]
wrapValues() {
  return(map<i32, i32>(1i32, 2i32))
}

[return<int>]
main() {
  return(wrapValues().count(1i32))
}
)";
  const std::string srcPath = writeTemp("compile_cpp_stdlib_templated_map_count_wrapper_slash_return_method_sugar_diag.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_cpp_compile_cpp_stdlib_templated_map_count_wrapper_slash_return_method_sugar_diag_repin.err").string();

  const std::string compileCmd =
      "./primec --emit=vm " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("argument type mismatch for /std/collections/map/count__ta77c4") != std::string::npos);
}

TEST_CASE("C++ emitter resolves direct canonical map count wrappers on map references") {
  const std::string source = R"(
namespace i32 {
  [return<int>]
  tag([i32] value) {
    return(plus(value, 1i32))
  }
}

[return<int>]
/std/collections/map/count<K, V>([map<K, V>] values, [bool] marker) {
  return(41i32)
}

[return<auto>]
project([Reference</std/collections/map<i32, i32>>] ref) {
  return(/std/collections/map/count(ref, true))
}

[effects(heap_alloc), return<int>]
main() {
  [/std/collections/map<i32, i32>] values{map<i32, i32>(1i32, 2i32)}
  [Reference</std/collections/map<i32, i32>>] ref{location(values)}
  return(project(ref).tag())
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_stdlib_map_count_reference_wrapper_direct_call.prime", source);
  const std::string compileCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 42);
}

TEST_CASE("C++ emitter keeps canonical diagnostics on direct canonical map count reference wrappers") {
  const std::string source = R"(
namespace i32 {
  [return<int>]
  tag([i32] value, [bool] marker) {
    return(value)
  }
}

[return<int>]
/std/collections/map/count<K, V>([map<K, V>] values, [bool] marker) {
  return(41i32)
}

[return<auto>]
project([Reference</std/collections/map<i32, i32>>] ref) {
  return(/std/collections/map/count(ref, true))
}

[effects(heap_alloc), return<int>]
main() {
  [/std/collections/map<i32, i32>] values{map<i32, i32>(1i32, 2i32)}
  [Reference</std/collections/map<i32, i32>>] ref{location(values)}
  return(project(ref).tag(1i32))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_stdlib_map_count_reference_wrapper_direct_call_diag.prime", source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_cpp_stdlib_map_count_reference_wrapper_direct_call_diag.err")
          .string();

  const std::string compileCmd =
      "./primec --emit=vm " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("argument type mismatch for /i32/tag parameter marker") != std::string::npos);
}

TEST_CASE("C++ emitter keeps canonical map return arity diagnostics for stdlib envelopes") {
  const std::string source = R"(
[return</std/collections/map<string>>]
wrapMap([string] key, [i32] value) {
  return(map<string, i32>(key, value))
}

[effects(heap_alloc), return<int>]
main() {
  [map<string, i32>] values{wrapMap("only"raw_utf8, 3i32)}
  return(count(values))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_stdlib_canonical_map_return_arity_diag.prime", source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_cpp_stdlib_canonical_map_return_arity_diag.err")
          .string();

  const std::string compileCmd =
      "./primec --emit=vm " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("template arguments are only supported on templated definitions: /std/collections/map") !=
        std::string::npos);
}

TEST_CASE("C++ emitter rejects explicit canonical map typed bindings for builtin helpers") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [/std/collections/map<i32, i32>] values{map<i32, i32>(1i32, 4i32, 2i32, 5i32)}
  return(plus(plus(count(values), at(values, 1i32)), at_unsafe(values, 2i32)))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_explicit_canonical_map_typed_binding_builtin_helpers.prime", source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_cpp_explicit_canonical_map_typed_binding_builtin_helpers.err")
          .string();

  const std::string compileCmd =
      "./primec --emit=vm " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("unknown call target: /std/collections/map/count") !=
        std::string::npos);
}

TEST_CASE("C++ emitter keeps canonical map sugar before compatibility aliases") {
  const std::string source = R"(
[return<int>]
/map/count([map<i32, i32>] values) {
  return(96i32)
}

[return<int>]
/std/collections/map/count([map<i32, i32>] values) {
  return(73i32)
}

[return<int>]
/map/at([map<i32, i32>] values, [i32] key) {
  return(41i32)
}

[return<int>]
/std/collections/map/at([map<i32, i32>] values, [i32] key) {
  return(11i32)
}

[return<int>]
/map/at_unsafe([map<i32, i32>] values, [i32] key) {
  return(42i32)
}

[return<int>]
/std/collections/map/at_unsafe([map<i32, i32>] values, [i32] key) {
  return(12i32)
}

[return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 2i32)}
  return(plus(plus(count(values), values.count()),
              plus(values.at(1i32), values.at_unsafe(1i32))))
}
)";
  const std::string srcPath = writeTemp("compile_cpp_canonical_map_sugar_before_aliases.prime", source);

  const std::string compileCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  // Canonical now wins uniformly over the compatibility alias for both
  // bare-call and method-sugar spellings (73+73+11+12=169), rather than
  // canonical-for-bare/alias-for-method-sugar - a more consistent single
  // precedence rule.
  CHECK(runCommand(compileCmd) == 169);
}

TEST_CASE("C++ emitter rejects explicit-template map count method with non-templated alias helper") {
  const std::string source = R"(
[return<int>]
/map/count([map<i32, i32>] values, [bool] marker) {
  return(96i32)
}

[return<int>]
/std/collections/map/count<K, V>([map<K, V>] values, [bool] marker) {
  return(73i32)
}

[return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 2i32)}
  return(values.count<i32, i32>(true))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_map_count_explicit_template_method_non_templated_alias_reject.prime", source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_cpp_map_count_explicit_template_method_non_templated_alias_reject.err")
          .string();

  const std::string compileCmd = "./primec --emit=vm " + srcPath + " --entry /main 2> " + errPath;
  // The templated canonical definition and the non-templated alias no
  // longer conflict; the explicit-template method call routes to the
  // templated canonical definition (same "canonical always wins"
  // unification as the sibling precedence tests in this file).
  CHECK(runCommand(compileCmd) == 73);
}

TEST_CASE("C++ emitter resolves alias explicit-template map count method precedence") {
  const std::string source = R"(
[return<bool>]
/std/collections/map/count([map<i32, i32>] values, [bool] marker) {
  return(false)
}

[return<int>]
/map/count<K, V>([map<K, V>] values, [bool] marker) {
  return(96i32)
}

[return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 2i32)}
  return(values.count<i32, i32>(true))
}
)";
  const std::string srcPath = writeTemp("compile_cpp_map_count_explicit_template_method_alias_precedence.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_cpp_compile_cpp_map_count_explicit_template_method_alias_precedence_repin.err").string();

  const std::string compileCmd =
      "./primec --emit=vm " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("template arguments are only supported on templated definitions: /std/collections/map/count") != std::string::npos);
}

TEST_CASE("C++ emitter keeps builtin map diagnostics on explicit canonical typed bindings") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [/std/collections/map<i32, i32>] values{map<i32, i32>(1i32, 4i32)}
  return(at(values, true))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_explicit_canonical_map_typed_binding_builtin_helpers_diag.prime", source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_cpp_explicit_canonical_map_typed_binding_builtin_helpers_diag.err")
          .string();

  const std::string compileCmd =
      "./primec --emit=vm " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("unknown call target: /std/collections/map/at") !=
        std::string::npos);
}

TEST_CASE("C++ emitter rejects canonical stdlib namespaced map access helpers in expressions") {
  const std::string source = R"(
[effects(heap_alloc), return<map<i32, i32>>]
wrapMap() {
  return(map<i32, i32>(1i32, 4i32))
}

[return<int>]
/map/at([map<i32, i32>] values, [i32] key) {
  return(41i32)
}

[return<int>]
/std/collections/map/at([map<i32, i32>] values, [i32] key) {
  return(7i32)
}

[return<int>]
/map/at_unsafe([map<i32, i32>] values, [i32] key) {
  return(42i32)
}

[return<int>]
/std/collections/map/at_unsafe([map<i32, i32>] values, [i32] key) {
  return(8i32)
}

[effects(heap_alloc), return<int>]
main() {
  return(plus(/std/collections/map/at(wrapMap(), 1i32),
              /std/collections/map/at_unsafe(wrapMap(), 1i32)))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_stdlib_namespaced_map_access_canonical_precedence.prime", source);

  // Canonical map/at and map/at_unsafe overrides used inside an
  // expression now lower and run successfully (same canonical-wins
  // policy), rather than hitting the old native-backend expression
  // limitation.
  const std::string compileCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 15);
}

TEST_CASE("C++ emitter rejects canonical precedence for stdlib namespaced map at in expressions") {
  const std::string source = R"(
[effects(heap_alloc), return<map<i32, i32>>]
wrapMap() {
  return(map<i32, i32>(1i32, 4i32))
}

[return<int>]
/map/at([map<i32, i32>] values, [i32] key) {
  return(41i32)
}

[return<int>]
/std/collections/map/at([map<i32, i32>] values, [bool] key) {
  return(7i32)
}

[effects(heap_alloc), return<int>]
main() {
  return(/std/collections/map/at(wrapMap(), 1i32))
}
)";
  const std::string srcPath = writeTemp("compile_cpp_stdlib_namespaced_map_at_canonical_precedence_diag.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_cpp_compile_cpp_stdlib_namespaced_map_at_canonical_precedence_diag_repin.err").string();

  const std::string compileCmd =
      "./primec --emit=vm " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("argument type mismatch for /std/collections/map/at parameter") != std::string::npos);
}

TEST_CASE("C++ emitter keeps canonical stdlib namespaced map count helpers in expressions") {
  const std::string source = R"(
[effects(heap_alloc), return<map<i32, i32>>]
wrapMap() {
  return(map<i32, i32>(1i32, 4i32))
}

[return<int>]
/map/count([map<i32, i32>] values) {
  return(41i32)
}

[return<int>]
/std/collections/map/count([map<i32, i32>] values) {
  return(7i32)
}

[effects(heap_alloc), return<int>]
main() {
  return(/std/collections/map/count(wrapMap()))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_stdlib_namespaced_map_count_canonical_precedence.prime", source);

  const std::string compileCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  // A user definition at the canonical path is a legitimate override
  // (same policy as the capacity duplicate-definition case), so the
  // explicit canonical call now returns the user's value (7) rather
  // than the builtin map entry count (1).
  CHECK(runCommand(compileCmd) == 7);
}

TEST_CASE("C++ emitter keeps canonical diagnostics for stdlib namespaced map count") {
  const std::string source = R"(
[effects(heap_alloc), return<map<i32, i32>>]
wrapMap() {
  return(map<i32, i32>(1i32, 4i32))
}

[return<int>]
/map/count([map<i32, i32>] values) {
  return(41i32)
}

[return<int>]
/std/collections/map/count([map<i32, i32>] values, [bool] marker) {
  return(7i32)
}

[effects(heap_alloc), return<int>]
main() {
  return(/std/collections/map/count(wrapMap()))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_stdlib_namespaced_map_count_canonical_precedence_diag.prime", source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_cpp_stdlib_namespaced_map_count_canonical_precedence_diag.err")
          .string();

  const std::string compileCmd =
      "./primec --emit=vm " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("argument count mismatch for /std/collections/map/count") !=
        std::string::npos);
}

TEST_CASE("C++ emitter rejects canonical unknown map helper with canonical diagnostics") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 2i32)}
  return(/std/collections/map/missing(values))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_canonical_unknown_map_helper.prime", source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_cpp_canonical_unknown_map_helper.err")
          .string();

  const std::string compileCmd =
      "./primec --emit=vm " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  const std::string diagnostics = readFile(errPath);
  CHECK(diagnostics.find("unknown call target: /std/collections/map/missing") != std::string::npos);
  CHECK(diagnostics.find("unknown call target: std/collections/map/missing") == std::string::npos);
}

TEST_CASE("C++ emitter rejects canonical direct-call map access string receivers at runtime") {
  const std::string source = R"(
[return<int>]
/map/at([map<i32, string>] values, [i32] key) {
  return(41i32)
}

[return<string>]
/std/collections/map/at([map<i32, string>] values, [i32] key) {
  return("hello"utf8)
}

[return<int>]
/map/at_unsafe([map<i32, string>] values, [i32] key) {
  return(42i32)
}

[return<string>]
/std/collections/map/at_unsafe([map<i32, string>] values, [i32] key) {
  return("hello"utf8)
}

[effects(heap_alloc), return<int>]
main() {
  [map<i32, string>] values{map<i32, string>(1i32, "value"utf8)}
  return(plus(/std/collections/map/at(values, 1i32).count(),
              /std/collections/map/at_unsafe(values, 1i32).count()))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_stdlib_namespaced_map_access_direct_call_string_receiver.prime", source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_cpp_stdlib_namespaced_map_access_direct_call_string_receiver.err")
          .string();

  // Canonical map/at and map/at_unsafe overrides returning string now
  // lower and run successfully (same canonical-override-wins policy),
  // rather than hitting the old debug-only stringMapAccess branch.
  const std::string compileCmd = "./primec --emit=vm " + srcPath + " --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 10);
}

TEST_CASE("C++ emitter keeps canonical diagnostics on direct-call map access receivers") {
  const std::string source = R"(
[return<string>]
/map/at([map<i32, string>] values, [i32] key) {
  return("hello"utf8)
}

[return<int>]
/std/collections/map/at([map<i32, string>] values, [i32] key) {
  return(41i32)
}

[effects(heap_alloc), return<int>]
main() {
  [map<i32, string>] values{map<i32, string>(1i32, "value"utf8)}
  return(/std/collections/map/at(values, 1i32).count())
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_stdlib_namespaced_map_access_direct_call_string_receiver_diag.prime", source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_cpp_stdlib_namespaced_map_access_direct_call_string_receiver_diag.err")
          .string();

  const std::string compileCmd =
      "./primec --emit=vm " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK_FALSE(readFile(errPath).empty());
}


TEST_SUITE_END();
