#include "test_compile_run_helpers.h"
#include "test_compile_run_collection_conformance_helpers.h"
#include "test_compile_run_emitters_helpers.h"

#include "primec/testing/EmitterHelpers.h"

namespace {

void expectCollectDiagnosticsFailure(const std::string& emitKind,
                                     const char* fileStem,
                                     const std::string& source,
                                     std::initializer_list<const char*> messages) {
  const std::string srcPath = writeTemp(std::string(fileStem) + ".prime", source);
  const std::string errPath = (testScratchPath("") / (std::string(fileStem) + ".json")).string();
  const std::string cmd = "./primec --emit=" + emitKind + " " + quoteShellArg(srcPath) +
                          " --entry /main --emit-diagnostics --collect-diagnostics > " +
                          quoteShellArg(errPath) + " 2>&1";

  CHECK(runCommand(cmd) == 2);

  const std::string diagnostics = readFile(errPath);
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

  const std::string compileCmd = "./primec --emit=cpp " + srcPath + " -o " + outPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 2);
}

TEST_CASE("C++ emitter lambda mutator positional call resolves user helper") {
  const std::string source = R"(
/vector/push([vector<i32> mut] values, [i32] value) { }

[effects(heap_alloc), return<int>]
main() {
  []() {
    [vector<i32> mut] values{vector<i32>(1i32)}
    push(5i32, values)
    return(values.count())
  }
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_cpp_lambda_vector_mutator_positional_shadow.prime", source);
  const std::string outPath =
      (testScratchPath("") / "primec_lambda_vector_mutator_positional_shadow.cpp").string();

  const std::string compileCmd = "./primec --emit=cpp " + srcPath + " -o " + outPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 2);
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
      "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK_FALSE(readFile(errPath).empty());
}

TEST_CASE("C++ emitter lambda mutator bool positional call resolves user helper") {
  const std::string source = R"(
/vector/push([vector<i32> mut] values, [bool] value) { }

[effects(heap_alloc), return<int>]
main() {
  []() {
    [vector<i32> mut] values{vector<i32>(1i32)}
    push(true, values)
    return(values.count())
  }
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_cpp_lambda_vector_mutator_bool_positional_shadow.prime", source);
  const std::string outPath =
      (testScratchPath("") / "primec_lambda_vector_mutator_bool_positional_shadow.cpp").string();

  const std::string compileCmd = "./primec --emit=cpp " + srcPath + " -o " + outPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 2);
}

TEST_CASE("C++ emitter lambda mutator named call prefers values receiver") {
  const std::string source = R"(
[effects(heap_alloc)]
/vector/push([vector<i32> mut] values, [vector<i32> mut] value) {
  pop(values)
}

[effects(heap_alloc), return<int>]
main() {
  holder{[]() {
    [vector<i32> mut] values{vector<i32>(1i32, 2i32)}
    [vector<i32> mut] payload{vector<i32>(7i32, 8i32)}
    push([value] payload, [values] values)
    return(values.count())
  }}
  return(holder())
}
)";
  const std::string srcPath = writeTemp("compile_cpp_lambda_vector_mutator_named_values_receiver.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_lambda_vector_mutator_named_values_receiver_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 2);
}

TEST_CASE("C++ emitter lambda mutator rewrite keeps known vector receiver leading names") {
  const std::string source = R"(
/i32/push([i32] value, [vector<i32> mut] values) { }

[effects(heap_alloc), return<int>]
main() {
  []() {
    [vector<i32> mut] values{vector<i32>(1i32)}
    [i32] index{8i32}
    push(values, index)
    return(values.count())
  }
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_cpp_lambda_vector_mutator_known_receiver_no_reorder.prime", source);
  const std::string outPath =
      (testScratchPath("") / "primec_lambda_vector_mutator_known_receiver_no_reorder.cpp")
          .string();

  const std::string compileCmd = "./primec --emit=cpp " + srcPath + " -o " + outPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 2);
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
      {"unknown method:"});
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
  const std::string srcPath = writeTemp("compile_cpp_lambda_vector_mutator_shadow_mismatch.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_lambda_vector_mutator_shadow_mismatch_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 2);
}

TEST_CASE("C++ emitter lambda mutator mismatch rejects call-form helper") {
  const std::string source = R"(
/vector/push([vector<i32> mut] values, [bool] value) { }

[effects(heap_alloc), return<int>]
main() {
  []() {
    [vector<i32> mut] values{vector<i32>()}
    push(values, 1i32)
    return(0i32)
  }
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_cpp_lambda_vector_mutator_call_mismatch.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_lambda_vector_mutator_call_mismatch_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 2);
}

TEST_CASE("compiles and runs import alias in C++ emitter") {
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
  const std::string exePath = (testScratchPath("") / "primec_import_alias_helper_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 7);
}

TEST_CASE("compiles and runs array method calls in C++ emitter") {
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
  const std::string exePath = (testScratchPath("") / "primec_array_method_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 7);
}

TEST_CASE("compiles and runs array index sugar") {
  const std::string source = R"(
[return<int>]
main() {
  [array<i32>] values{array<i32>(4i32, 7i32, 9i32)}
  return(values[1i32])
}
)";
  const std::string srcPath = writeTemp("compile_array_index.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_array_index_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 7);
}

TEST_CASE("compiles and runs argv helpers in C++ emitter") {
  const std::string source = R"(
[return<int> effects(io_out)]
main([array<string>] args) {
  print_line(args[1i32])
  return(args.count())
}
)";
  const std::string srcPath = writeTemp("compile_emit_argv.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_emit_argv_exe").string();
  const std::string outPath = (testScratchPath("") / "primec_emit_argv_out.txt").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);

  const std::string runCmd = exePath + " alpha beta > " + outPath;
  CHECK(runCommand(runCmd) == 3);
  CHECK(readFile(outPath) == "alpha\n");
}

TEST_CASE("compiles and runs array index sugar with u64") {
  const std::string source = R"(
[return<int>]
main() {
  [array<i32>] values{array<i32>(4i32, 7i32, 9i32)}
  return(values[1u64])
}
)";
  const std::string srcPath = writeTemp("compile_array_index_u64.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_array_index_u64_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 7);
}

TEST_CASE("compiles and runs vector helpers in C++ emitter") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32, 2i32, 3i32)}
  vectorPush<i32>(values, 4i32)
  vectorRemoveAt<i32>(values, 1i32)
  vectorRemoveSwap<i32>(values, 1i32)
  vectorPop<i32>(values)
  vectorReserve<i32>(values, 8i32)
  vectorCapacity<i32>(values)
  vectorClear<i32>(values)
  return(vectorCount<i32>(values))
}
)";
  const std::string srcPath = writeTemp("compile_vector_helpers_exe.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_vector_helpers_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 0);
}

TEST_CASE("compiles and runs canonical vector mutators over imported user shadow helpers in C++ emitter") {
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
  remove_at(values, 0i32)
  values.remove_at(0i32)
  remove_swap(values, 0i32)
  values.remove_swap(0i32)
  pop(values)
  values.clear()
  clear(values)
  return(vectorCount<i32>(values))
}
)";
  const std::string srcPath = writeTemp("compile_cpp_user_vector_mutator_shadow_precedence.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_cpp_user_vector_mutator_shadow_precedence_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 0);
}

TEST_CASE("C++ emitter statement mutator call-form rejects shadow helper") {
  const std::string source = R"(
/vector/push([vector<i32> mut] values, [i32] value) { }

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32)}
  push(values, 2i32)
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_cpp_vector_mutator_call_shadow_precedence.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_vector_mutator_call_shadow_precedence.err").string();

  const std::string compileCmd =
      "./primec --emit=cpp " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("unknown call target: /std/collections/vector/push") !=
        std::string::npos);
}

TEST_CASE("compiles and runs canonical vector mutator named calls over imported user shadow helpers in C++ emitter") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc)]
/vector/push([vector<i32> mut] values, [i32] value) { }

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32, 2i32)}
  push([value] 3i32, [values] values)
  return(vectorCount<i32>(values))
}
)";
  const std::string srcPath = writeTemp("compile_cpp_vector_mutator_named_call_shadow.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_cpp_vector_mutator_named_call_shadow_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 3);
}

TEST_CASE("C++ emitter statement mutator named call rejects shadow helper without import") {
  const std::string source = R"(
/vector/push([vector<i32> mut] values, [vector<i32> mut] value) { }

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32, 2i32)}
  [vector<i32> mut] payload{vector<i32>(9i32, 10i32)}
  push([value] payload, [values] values)
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_cpp_vector_mutator_named_values_receiver.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_cpp_vector_mutator_named_values_receiver.err").string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("unknown call target: /std/collections/vector/push") !=
        std::string::npos);
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
  return(vectorCount<i32>(values))
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
  CHECK((diagnostics.find("\"message\":\"push requires mutable vector binding\"") !=
             std::string::npos ||
         diagnostics.find("push requires mutable vector binding") != std::string::npos));
}

TEST_SUITE_END();
