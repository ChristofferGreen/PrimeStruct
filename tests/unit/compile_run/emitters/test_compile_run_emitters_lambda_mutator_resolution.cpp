#include "../test_compile_run_helpers.h"
#include "../test_compile_run_collection_conformance_helpers.h"
#include "test_compile_run_emitters_helpers.h"

#include "primec/testing/EmitterHelpers.h"

namespace {

void expectCollectDiagnosticsFailure(const std::string& emitKind,
                                     const char* fileStem,
                                     const std::string& source,
                                     std::initializer_list<const char*> messages) {
  const std::string srcPath = writeTemp(std::string(fileStem) + ".prime", source);
  const std::string diagnosticsPath =
      (testScratchPath("") / (std::string(fileStem) + ".diagnostics.txt")).string();
  const std::string cmd = "./primec --emit=" + emitKind + " " + quoteShellArg(srcPath) +
                          " --entry /main --emit-diagnostics --collect-diagnostics > " +
                          quoteShellArg(diagnosticsPath) + " 2>&1";

  CHECK(runCommand(cmd) == 2);

  const std::string diagnostics = readFile(diagnosticsPath);
  INFO(diagnostics);
  CHECK_FALSE(diagnostics.empty());
  CHECK((diagnostics.find("\"code\":\"PSC1005\"") != std::string::npos ||
         diagnostics.find("[PSC1005]") != std::string::npos));
  CHECK((diagnostics.find("\"label\":\"definition: /main\"") != std::string::npos ||
         diagnostics.find("definition: /main") != std::string::npos));
  for (const char* message : messages) {
    const std::string jsonMessage = "\"message\":\"" + std::string(message) + "\"";
    CHECK((diagnostics.find(jsonMessage) != std::string::npos ||
           diagnostics.find(message) != std::string::npos));
  }
}

}  // namespace

TEST_SUITE_BEGIN("primestruct.compile.run.emitters.cpp");

TEST_CASE("C++ emitter lambda mutators honor user vector helpers") {
  const std::string source = R"(
/vector/push([vector<i32> mut] values, [i32] value) { }
/vector/pop([vector<i32> mut] values) { }
/vector/reserve([vector<i32> mut] values, [i32] target) { }
/vector/clear([vector<i32> mut] values) { }
/vector/remove_at([vector<i32> mut] values, [i32] index) { }
/vector/remove_swap([vector<i32> mut] values, [i32] index) { }

[effects(heap_alloc), return<int>]
main() {
  holder{[]([i32] seed) {
    [vector<i32> mut] values{vector<i32>(1i32, 2i32, 3i32, seed)}
    push(values, 5i32)
    values.push(6i32)
    reserve(values, 10i32)
    values.reserve(11i32)
    remove_at(values, 0i32)
    values.remove_at(0i32)
    remove_swap(values, 0i32)
    values.remove_swap(0i32)
    pop(values)
    values.clear()
    clear(values)
    return(values.count())
  }}
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_cpp_lambda_vector_mutator_shadow_precedence.prime", source);
  const std::string outPath =
      (testScratchPath("") / "primec_lambda_vector_mutator_shadow_precedence.cpp").string();
  const std::string errPath =
      (testScratchPath("") / "primec_lambda_vector_mutator_shadow_precedence.err").string();

  const std::string compileCmd =
      "./primec --emit=cpp " + srcPath + " -o " + outPath + " --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("unknown call target: /std/collections/vector/push") !=
        std::string::npos);
}

TEST_CASE("rejects lambda std namespaced reordered mutator compatibility helper in C++ emitter") {
  const std::string source = R"(
/vector/push([vector<i32> mut] values, [i32] value) { }

[effects(heap_alloc), return<int>]
main() {
  holder{[]() {
    [vector<i32> mut] values{vector<i32>(1i32)}
    /std/collections/vector/push(5i32, values)
    return(values.count())
  }}
  return(holder())
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_lambda_std_namespaced_reordered_mutator_compat_helper.prime", source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_cpp_lambda_std_namespaced_reordered_mutator_compat_helper_err.txt")
          .string();

  const std::string compileCmd =
      "./primec --emit=vm " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("unknown call target: /std/collections/vector/push") !=
        std::string::npos);
}

TEST_CASE("C++ emitter rejects lambda explicit vector mutator statements without helper before emission") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  holder{[]([i32] seed) {
    [vector<i32> mut] values{vector<i32>(1i32, 2i32, seed)}
    /std/collections/vector/push(values, 5i32)
    return(0i32)
  }}
  return(0i32)
}
)";
  expectCollectDiagnosticsFailure(
      "cpp",
      "compile_cpp_lambda_explicit_vector_mutator_same_path_reject",
      source,
      {"unknown call target: /std/collections/vector/push"});
}

TEST_CASE("C++ emitter rejects lambda cross-path explicit vector mutator statements before emission") {
  const std::string source = R"(
[effects(heap_alloc)]
/std/collections/vector/push([vector<i32> mut] values, [i32] value) {
}

[effects(heap_alloc), return<int>]
main() {
  holder{[]([i32] seed) {
    [vector<i32> mut] values{vector<i32>(1i32, 2i32, seed)}
    /vector/push(values, 5i32)
    return(0i32)
  }}
  return(0i32)
}
)";
  expectCollectDiagnosticsFailure(
      "cpp",
      "compile_cpp_lambda_cross_path_vector_mutator_same_path_reject",
      source,
      {"unknown call target: /vector/push"});
}

TEST_CASE("C++ emitter rejects lambda reordered cross-path explicit vector mutator statements before emission") {
  const std::string source = R"(
[effects(heap_alloc)]
/std/collections/vector/push([vector<i32> mut] values, [i32] value) {
}

[effects(heap_alloc), return<int>]
main() {
  holder{[]([i32] seed) {
    [vector<i32> mut] values{vector<i32>(1i32, 2i32, seed)}
    /vector/push(5i32, values)
    return(0i32)
  }}
  return(0i32)
}
)";
  expectCollectDiagnosticsFailure(
      "cpp",
      "compile_cpp_lambda_reordered_cross_path_vector_mutator_same_path_reject",
      source,
      {"unknown call target: /vector/push"});
}

TEST_CASE("C++ emitter rejects lambda explicit vector mutator methods without helper before emission") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  holder{[]([i32] seed) {
    [vector<i32> mut] values{vector<i32>(1i32, 2i32, seed)}
    values./std/collections/vector/push(5i32)
    return(0i32)
  }}
  return(0i32)
}
)";
  expectCollectDiagnosticsFailure(
      "cpp",
      "compile_cpp_lambda_explicit_vector_mutator_method_same_path_reject",
      source,
      {"unknown method: /std/collections/vector/push"});
}

TEST_CASE("C++ emitter rejects lambda cross-path explicit vector mutator methods before emission") {
  const std::string source = R"(
[effects(heap_alloc)]
/std/collections/vector/push([vector<i32> mut] values, [i32] value) {
}

[effects(heap_alloc), return<int>]
main() {
  holder{[]([i32] seed) {
    [vector<i32> mut] values{vector<i32>(1i32, 2i32, seed)}
    values./vector/push(5i32)
    return(0i32)
  }}
  return(0i32)
}
)";
  expectCollectDiagnosticsFailure(
      "cpp",
      "compile_cpp_lambda_cross_path_vector_mutator_method_same_path_reject",
      source,
      {"unknown method: /vector/push"});
}

TEST_CASE("C++ emitter lambda mutator mismatch rejects user helper signatures") {
  const std::string source = R"(
/vector/push([vector<i32> mut] values, [bool] value) { }

[effects(heap_alloc), return<int>]
main() {
  []([i32] seed) {
    [vector<i32> mut] values{vector<i32>(seed)}
    push(values, 1i32)
    values.push(2i32)
    return(0i32)
  }
  return(0i32)
}
)";
  expectCollectDiagnosticsFailure(
      "exe",
      "compile_cpp_lambda_vector_mutator_shadow_mismatch",
      source,
      {"unknown call target: /std/collections/vector/push"});
}

TEST_CASE("import alias in C++ emitter") {
  const std::string source = R"(
import /util
namespace util {
  [public return<int>]
  helper() {
    return(7i32)
  }
}
[return<int>]
main() {
  return(helper())
}
)";
  const std::string srcPath = writeTemp("compile_import_alias_helper_exe.prime", source);
  const std::string compileCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 7);
}

TEST_CASE("array method calls in C++ emitter") {
  const std::string source = R"(
[return<int>]
/array/first([array<i32>] items) {
  return(items[0i32])
}

[return<int>]
main() {
  [array<i32>] items{array<i32>(7i32, 9i32)}
  return(items.first())
}
)";
  const std::string srcPath = writeTemp("compile_array_method.prime", source);
  const std::string compileCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 7);
}

TEST_CASE("array bracket index sugar reads an element") {
  const std::string source = R"(
[return<int>]
main() {
  [array<i32>] values{array<i32>(4i32, 7i32, 9i32)}
  return(values[1i32])
}
)";
  const std::string srcPath = writeTemp("compile_array_index.prime", source);
  const std::string compileCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 7);
}

TEST_CASE("argv helpers in C++ emitter") {
  const std::string source = R"(
[return<int> effects(io_out)]
main([array<string>] args) {
  print_line(args[1i32])
  return(args.count())
}
)";
  const std::string srcPath = writeTemp("compile_emit_argv.prime", source);
  const std::string outPath = (testScratchPath("") / "primec_emit_argv_out.txt").string();

  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main -- alpha beta > " + outPath;
  CHECK(runCommand(runCmd) == 3);
  CHECK(readFile(outPath) == "alpha\n");
}

TEST_CASE("vector helpers in C++ emitter") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32, 2i32, 3i32)}
  /std/collections/vector/push<i32>(values, 4i32)
  /std/collections/vector/remove_at<i32>(values, 1i32)
  /std/collections/vector/remove_swap<i32>(values, 1i32)
  /std/collections/vector/pop<i32>(values)
  /std/collections/vector/reserve<i32>(values, 8i32)
  /std/collections/vector/capacity<i32>(values)
  /std/collections/vector/clear<i32>(values)
  return(/std/collections/vector/count<i32>(values))
}
)";
  const std::string srcPath = writeTemp("compile_vector_helpers_exe.prime", source);
  const std::string compileCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
}

TEST_CASE("canonical vector mutators over imported user shadow helpers in C++ emitter") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc)]
/vector/push([vector<i32> mut] values, [i32] value) { }

[effects(heap_alloc)]
/vector/pop([vector<i32> mut] values) { }

[effects(heap_alloc)]
/vector/reserve([vector<i32> mut] values, [i32] target) { }

[effects(heap_alloc)]
/vector/clear([vector<i32> mut] values) { }

[effects(heap_alloc)]
/vector/remove_at([vector<i32> mut] values, [i32] index) { }

[effects(heap_alloc)]
/vector/remove_swap([vector<i32> mut] values, [i32] index) { }

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32, 2i32, 3i32, 4i32)}
  push(values, 5i32)
  values.push(6i32)
  reserve(values, 10i32)
  values.reserve(11i32)
  /std/collections/vector/remove_at<i32>(values, 0i32)
  values.remove_at(0i32)
  /std/collections/vector/remove_swap<i32>(values, 0i32)
  values.remove_swap(0i32)
  /std/collections/vector/pop<i32>(values)
  values.clear()
  /std/collections/vector/clear<i32>(values)
  return(/std/collections/vector/count<i32>(values))
}
)";
  const std::string srcPath = writeTemp("compile_cpp_user_vector_mutator_shadow_precedence.prime", source);
  const std::string compileCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
}

TEST_CASE("canonical vector mutator named calls over imported user shadow helpers in C++ emitter") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc)]
/vector/push([vector<i32> mut] values, [i32] value) { }

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32, 2i32)}
  push([value] 3i32, [values] values)
  return(/std/collections/vector/count<i32>(values))
}
)";
  const std::string srcPath = writeTemp("compile_cpp_vector_mutator_named_call_shadow.prime", source);
  const std::string compileCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 3);
}

TEST_CASE("rejects imported user vector mutator positional call shadow in C++ emitter") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc)]
/vector/push([vector<i32> mut] values, [i32] value) { }

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32, 2i32)}
  push(3i32, values)
  return(/std/collections/vector/count<i32>(values))
}
)";
  const std::string srcPath = writeTemp("compile_cpp_vector_mutator_positional_call_shadow.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_cpp_vector_mutator_positional_call_shadow_err.txt")
          .string();

  const std::string compileCmd = "./primec --emit=exe " + quoteShellArg(srcPath) +
                                 " -o /dev/null --entry /main --emit-diagnostics > " +
                                 quoteShellArg(errPath) + " 2>&1";
  CHECK(runCommand(compileCmd) == 2);
  const std::string diagnostics = readFile(errPath);
  INFO(diagnostics);
  CHECK_FALSE(diagnostics.empty());
  CHECK((diagnostics.find("\"message\":\"push requires mutable vector binding\"") !=
             std::string::npos ||
         diagnostics.find("push requires mutable vector binding") != std::string::npos ||
         diagnostics.find("\"message\":\"template arguments required for /std/collections/soa/push\"") !=
             std::string::npos ||
         diagnostics.find("template arguments required for /std/collections/soa/push") !=
             std::string::npos ||
         diagnostics.find("\"message\":\"template arguments required for /std/collections/soa/push\"") !=
             std::string::npos ||
         diagnostics.find("template arguments required for /std/collections/soa/push") !=
             std::string::npos ||
         diagnostics.find("\"message\":\"unknown call target: /std/collections/vector/push\"") !=
             std::string::npos ||
         diagnostics.find("unknown call target: /std/collections/vector/push") !=
             std::string::npos ||
         diagnostics.find("native backend only supports arithmetic") !=
             std::string::npos));
}

TEST_SUITE_END();
