#include "test_compile_run_helpers.h"
#include "test_compile_run_collection_conformance_helpers.h"
#include "test_compile_run_emitters_helpers.h"

#include "primec/testing/EmitterHelpers.h"

TEST_SUITE_BEGIN("primestruct.compile.run.emitters.cpp");

TEST_CASE("rejects canonical map slash-method unsafe struct helper receiver mismatch in C++ emitter") {
  const std::string source = R"(
Marker {
  [i32] value
}

[return<Marker>]
/std/collections/map/at_unsafe([map<i32, i32>] values, [i32] key) {
  return(Marker(key))
}

[return<int>]
/Marker/tag([Marker] self) {
  return(self.value)
}

[return<auto>]
project([map<i32, i32>] values) {
  return(values./std/collections/map/at_unsafe(2i32).tag())
}

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(2i32, 7i32)}
  return(project(values))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_canonical_map_method_alias_access_unsafe_struct_method_chain_forwarding.prime",
                source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_cpp_canonical_map_method_alias_access_unsafe_struct_method_chain_forwarding.err")
          .string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("argument type mismatch for /Marker/tag parameter self: expected /Marker got i32") !=
        std::string::npos);
}

TEST_CASE("rejects wrapper-returned map method alias primitive receiver fallback in C++ emitter") {
  const std::string source = R"(
[effects(heap_alloc), return</std/collections/map<i32, i32>>]
wrapMap() {
  return(map<i32, i32>(2i32, 7i32))
}

[return<int>]
/i32/tag([i32] self) {
  return(plus(self, 40i32))
}

[effects(heap_alloc), return<int>]
main() {
  return(wrapMap()./std/collections/map/at(2i32).tag())
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_wrapper_map_method_alias_primitive_receiver_fallback.prime", source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_cpp_wrapper_map_method_alias_primitive_receiver_fallback.err")
          .string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("unknown call target: /std/collections/map/at") !=
        std::string::npos);
}

TEST_CASE("rejects wrapper-returned canonical map slash-method struct helper receiver mismatch in C++ emitter") {
  const std::string source = R"(
Marker {
  [i32] value
}

[effects(heap_alloc), return</std/collections/map<i32, i32>>]
wrapMap() {
  return(map<i32, i32>(2i32, 7i32))
}

[return<Marker>]
/std/collections/map/at([map<i32, i32>] values, [i32] key) {
  return(Marker(key))
}

[return<int>]
/Marker/tag([Marker] self) {
  return(self.value)
}

[effects(heap_alloc), return<int>]
main() {
  return(wrapMap()./std/collections/map/at(2i32).tag())
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_wrapper_map_method_alias_struct_receiver_forwarding.prime", source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_cpp_wrapper_map_method_alias_struct_receiver_forwarding.err")
          .string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("argument type mismatch for /Marker/tag parameter self: expected /Marker got i32") !=
        std::string::npos);
}

TEST_CASE("C++ emitter keeps std-namespaced vector method alias access helper receiver forwarding") {
  const std::string source = R"(
Marker {
  [i32] value
}

[return<Marker>]
/std/collections/vector/at([vector<i32>] values, [i32] index) {
  return(Marker(index))
}

[return<int>]
/Marker/tag([Marker] self) {
  return(self.value)
}

[return<auto>]
project([vector<i32>] values) {
  return(values./std/collections/vector/at(2i32).tag())
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  return(project(values))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_std_namespaced_vector_method_alias_access_struct_method_chain_forwarding.prime",
                source);
  const std::string exePath =
      (testScratchPath("") /
       "primec_cpp_std_namespaced_vector_method_alias_access_struct_method_chain_forwarding_exe")
          .string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 2);
}

TEST_CASE("rejects std-namespaced vector method alias access receiver fallback without helper in C++ emitter") {
  const std::string source = R"(
namespace i32 {
  [return<int>]
  tag([i32] value) {
    return(plus(value, 40i32))
  }
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  return(values./std/collections/vector/at(1i32).tag())
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_std_namespaced_vector_method_alias_access_receiver_fallback_reject.prime",
                source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_cpp_std_namespaced_vector_method_alias_access_receiver_fallback_reject.err")
          .string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("unknown method: /std/collections/vector/at") != std::string::npos);
}

TEST_CASE("rejects std-namespaced vector method alias access struct method chain with helper missing-method diagnostics in C++ emitter") {
  const std::string source = R"(
Marker {
  [i32] value
}

[return<Marker>]
/std/collections/vector/at([vector<i32>] values, [i32] index) {
  return(Marker(index))
}

[return<int>]
/i32/tag([i32] self, [bool] marker) {
  return(self)
}

[return<auto>]
project([vector<i32>] values) {
  return(values./std/collections/vector/at(2i32).tag(1i32))
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  return(project(values))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_std_namespaced_vector_method_alias_access_struct_method_chain_diag.prime", source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_cpp_std_namespaced_vector_method_alias_access_struct_method_chain_diag.err")
          .string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("unknown method: /Marker/tag") != std::string::npos);
}

TEST_CASE("rejects std-namespaced vector unsafe method alias access receiver fallback without helper in C++ emitter") {
  const std::string source = R"(
namespace i32 {
  [return<int>]
  tag([i32] value) {
    return(plus(value, 40i32))
  }
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  return(values./std/collections/vector/at_unsafe(1i32).tag())
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_std_namespaced_vector_method_alias_access_unsafe_receiver_fallback_reject.prime",
                source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_cpp_std_namespaced_vector_method_alias_access_unsafe_receiver_fallback_reject.err")
          .string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("unknown method: /std/collections/vector/at_unsafe") != std::string::npos);
}

TEST_CASE("C++ emitter forwards explicit-template vector count wrappers through canonical return kinds") {
  const std::string source = R"(
namespace i32 {
  [return<int>]
  tag([i32] value) {
    return(plus(value, 1i32))
  }
}

/vector/count([vector<i32>] values, [bool] marker) {
}

[return<int>]
/std/collections/vector/count<T>([vector<T>] values, [bool] marker) {
  return(41i32)
}

[return<auto>]
project([vector<i32>] values) {
  return(/vector/count<i32>(values, true))
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  return(project(values).tag())
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_vector_alias_count_explicit_template_wrapper_canonical_return.prime", source);
  const std::string exePath = (testScratchPath("") /
                               "primec_cpp_vector_alias_count_explicit_template_wrapper_canonical_return_exe")
                                  .string();
  const std::string errPath = (testScratchPath("") /
                               "primec_cpp_vector_alias_count_explicit_template_wrapper_canonical_return.err")
                                  .string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("template arguments are only supported on templated definitions: /vector/count") !=
        std::string::npos);
}

TEST_CASE("C++ emitter keeps canonical diagnostics for explicit-template vector count wrappers") {
  const std::string source = R"(
namespace i32 {
  [return<int>]
  tag([i32] value, [bool] marker) {
    return(value)
  }
}

/vector/count([vector<i32>] values, [bool] marker) {
}

[return<int>]
/std/collections/vector/count<T>([vector<T>] values, [bool] marker) {
  return(41i32)
}

[return<auto>]
project([vector<i32>] values) {
  return(/vector/count<i32>(values, true))
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(5i32, 6i32, 7i32)}
  return(project(values).tag(1i32))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_vector_alias_count_explicit_template_wrapper_canonical_diagnostic.prime", source);
  const std::string errPath = (testScratchPath("") /
                               "primec_cpp_vector_alias_count_explicit_template_wrapper_canonical_diagnostic.err")
                                  .string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("template arguments are only supported on templated definitions: /vector/count") !=
        std::string::npos);
}

TEST_CASE("rejects namespaced access method chain non-collection target in C++ emitter") {
  const std::string source = R"(
namespace i32 {
  [return<int>]
  tag([i32] value) {
    return(plus(value, 1i32))
  }
}

[return<int>]
main() {
  return(/std/collections/vector/at(7i32, 0i32).tag())
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_namespaced_access_method_chain_non_collection_reject.prime", source);
  const std::string errPath = (testScratchPath("") /
                               "primec_cpp_namespaced_access_method_chain_non_collection_reject.err")
                                  .string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("unknown method: /i32/at") != std::string::npos);
}

TEST_CASE("rejects namespaced map capacity method chain target in C++ emitter") {
  const std::string source = R"(
namespace i32 {
  [return<int>]
  tag([i32] value) {
    return(plus(value, 1i32))
  }
}

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 2i32)}
  return(/std/collections/vector/capacity(values).tag())
}
)";
  const std::string srcPath = writeTemp("compile_cpp_namespaced_map_capacity_method_chain_reject.prime", source);
  const std::string errPath = (testScratchPath("") /
                               "primec_cpp_namespaced_map_capacity_method_chain_reject.err")
                                  .string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("unknown call target: /std/collections/vector/capacity") != std::string::npos);
}

TEST_CASE("C++ emitter keeps canonical direct-call vector capacity same-path helper on map receiver") {
  const std::string source = R"(
[return<map<i32, i32>>]
wrapMap() {
  return(map<i32, i32>(1i32, 2i32))
}

[return<int>]
/std/collections/vector/capacity([map<i32, i32>] values) {
  return(93i32)
}

[return<int>]
main() {
  return(/std/collections/vector/capacity(wrapMap()))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_canonical_direct_vector_capacity_map_same_path_helper.prime", source);
  const std::string exePath =
      (testScratchPath("") /
       "primec_cpp_canonical_direct_vector_capacity_map_same_path_helper_exe")
          .string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 93);
}

TEST_CASE("C++ emitter keeps alias direct-call vector capacity same-path helper on map receiver") {
  const std::string source = R"(
[return<map<i32, i32>>]
wrapMap() {
  return(map<i32, i32>(1i32, 2i32))
}

[return<int>]
/vector/capacity([map<i32, i32>] values) {
  return(94i32)
}

[return<int>]
main() {
  return(/vector/capacity(wrapMap()))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_alias_direct_vector_capacity_map_same_path_helper.prime", source);
  const std::string exePath =
      (testScratchPath("") /
       "primec_cpp_alias_direct_vector_capacity_map_same_path_helper_exe")
          .string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 94);
}

TEST_SUITE_END();
