#include "test_compile_run_helpers.h"
#include "test_compile_run_collection_conformance_helpers.h"
#include "test_compile_run_emitters_helpers.h"

#include "primec/testing/EmitterHelpers.h"

TEST_SUITE_BEGIN("primestruct.compile.run.emitters.cpp");

TEST_CASE("C++ emitter rejects wrapper-returned slash-method map access count with same-path diagnostics") {
  const std::string source = R"(
[return<int>]
/string/count([string] values) {
  return(91i32)
}

[return<string>]
/std/collections/map/at([map<i32, i32>] values, [i32] key) {
  return("abc"raw_utf8)
}

[return<string>]
/std/collections/map/at_unsafe([map<i32, i32>] values, [i32] key) {
  return("abc"raw_utf8)
}

[return</std/collections/map<i32, i32>>]
wrapMap() {
  return(map<i32, i32>(1i32, 4i32))
}

[effects(heap_alloc), return<int>]
main() {
  return(count(wrapMap()./std/collections/map/at(1i32)))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_wrapper_slash_method_map_access_count_diag.prime", source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_cpp_wrapper_slash_method_map_access_count_diag.err")
          .string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("unknown method: /map/at") != std::string::npos);
}

TEST_CASE("C++ emitter rejects wrapper-returned slash-method map access count before deleted stubs") {
  const std::string source = R"(
[return</std/collections/map<i32, string>>]
wrapMap() {
  return(map<i32, string>(1i32, "hello"utf8, 2i32, "bye"utf8))
}

[effects(heap_alloc), return<int>]
main() {
  return(plus(count(wrapMap()./std/collections/map/at(1i32)),
              count(wrapMap()./std/collections/map/at_unsafe(2i32))))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_wrapper_slash_method_map_access_count_deleted_stub.prime", source);
  const std::string outPath =
      (testScratchPath("") /
       "primec_cpp_wrapper_slash_method_map_access_count_deleted_stub.cpp")
          .string();
  const std::string errPath =
      (testScratchPath("") /
       "primec_cpp_wrapper_slash_method_map_access_count_deleted_stub.err")
          .string();

  const std::string compileCmd =
      "./primec --emit=cpp " + srcPath + " -o " + outPath + " --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("unknown method: /map/at") != std::string::npos);
}

TEST_CASE("C++ emitter rejects explicit canonical map count slash-method receiver fallback") {
  const std::string source = R"(
[return<int>]
/std/collections/map/count([map<i32, i32>] values) {
  return(17i32)
}

[return<int>]
/i32/tag([i32] self) {
  return(plus(self, 40i32))
}

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 4i32)}
  return(values./std/collections/map/count().tag())
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_explicit_canonical_map_count_slash_method_receiver_fallback.prime",
                source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_cpp_explicit_canonical_map_count_slash_method_receiver_fallback.err")
          .string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("unknown method: /map/count") != std::string::npos);
}

TEST_CASE("C++ emitter keeps stdlib namespaced vector string access count fallback") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<string>] values{vector<string>("abc"raw_utf8)}
  return(count(/std/collections/vector/at(values, 0i32)))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_stdlib_namespaced_vector_string_access_count_fallback.prime", source);
  const std::string outPath = (testScratchPath("") /
                               "primec_cpp_stdlib_namespaced_vector_string_access_count_fallback.cpp")
                                  .string();

  const std::string compileCmd = "./primec --emit=cpp " + srcPath + " -o " + outPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 2);
}

TEST_CASE("rejects canonical vector access direct-call string count fallback in C++ emitter") {
  const std::string source = R"(
[return<int>]
/string/count([string] values) {
  return(91i32)
}

[return<i32>]
/std/collections/vector/at([vector<string>] values, [i32] index) {
  return(7i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<string>] values{vector<string>("hello"raw_utf8)}
  return(count(/std/collections/vector/at(values, 0i32)))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_canonical_vector_access_direct_count_fallback_reject.prime", source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_cpp_canonical_vector_access_direct_count_fallback_reject.err")
          .string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("unknown method: /i32/count") != std::string::npos);
}

TEST_CASE("C++ emitter rejects wrapper vector direct-call count receivers before deleted access stubs") {
  const std::string source = R"(
[effects(heap_alloc), return<vector<string>>]
wrapValues() {
  return(vector<string>("abc"raw_utf8))
}

[effects(heap_alloc), return<int>]
main() {
  return(plus(count(/vector/at(wrapValues(), 0i32)),
              count(/std/collections/vector/at_unsafe(wrapValues(), 1i32))))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_wrapper_vector_access_count_receiver_same_path_diag.prime", source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_cpp_wrapper_vector_access_count_receiver_same_path_diag.err")
          .string();

  const std::string compileCmd =
      "./primec --emit=cpp " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("unknown call target: /vector/at") != std::string::npos);
}

TEST_CASE("C++ emitter keeps slash-method vector access helper return-kind count forwarding") {
  const std::string source = R"(
[return<int>]
/string/count([string] values) {
  return(91i32)
}

[return<string>]
/vector/at([vector<i32>] values, [i32] index) {
  return("abc"raw_utf8)
}

[return<string>]
/std/collections/vector/at_unsafe([vector<i32>] values, [i32] index) {
  return("abc"raw_utf8)
}

[effects(heap_alloc), return<vector<i32>>]
wrapValues() {
  return(vector<i32>(1i32))
}

[effects(heap_alloc), return<int>]
main() {
  return(plus(count(wrapValues()./vector/at(0i32)),
              count(wrapValues()./std/collections/vector/at_unsafe(0i32))))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_slash_method_vector_access_count_receiver_forwarding.prime", source);
  const std::string exePath =
      (testScratchPath("") /
       "primec_cpp_slash_method_vector_access_count_receiver_forwarding_exe")
          .string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 182);
}

TEST_CASE("C++ emitter rejects slash-method vector count receivers before deleted access stubs") {
  const std::string source = R"(
[effects(heap_alloc), return<vector<string>>]
wrapValues() {
  return(vector<string>("abc"raw_utf8))
}

[effects(heap_alloc), return<int>]
main() {
  return(plus(count(wrapValues()./vector/at(0i32)),
              count(wrapValues()./std/collections/vector/at_unsafe(1i32))))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_slash_method_vector_access_count_receiver_same_path_diag.prime", source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_cpp_slash_method_vector_access_count_receiver_same_path_diag.err")
          .string();

  const std::string compileCmd =
      "./primec --emit=cpp " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("unknown method: /vector/at") != std::string::npos);
}

TEST_CASE("rejects slash-method vector count receivers without helper in C++ emitter") {
  const std::string source = R"(
[effects(heap_alloc), return<vector<string>>]
wrapValues() {
  return(vector<string>("abc"raw_utf8))
}

[effects(heap_alloc), return<int>]
main() {
  return(count(wrapValues()./vector/at(0i32)))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_slash_method_vector_access_count_receiver_deleted_stub_exe.prime", source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_cpp_slash_method_vector_access_count_receiver_deleted_stub.err")
          .string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("unknown method: /vector/at") != std::string::npos);
}

TEST_CASE("rejects wrapper vector alias direct-call count without helper in C++ emitter") {
  const std::string source = R"(
[effects(heap_alloc), return<vector<string>>]
wrapValues() {
  return(vector<string>("abc"raw_utf8))
}

[effects(heap_alloc), return<int>]
main() {
  return(count(/vector/at(wrapValues(), 0i32)))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_wrapper_vector_access_count_receiver_deleted_stub_exe.prime", source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_cpp_wrapper_vector_access_count_receiver_deleted_stub.err")
          .string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("unknown call target: /vector/at") != std::string::npos);
}

TEST_CASE("rejects stdlib namespaced vector access count for non-string element in C++ emitter") {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32)}
  return(count(/std/collections/vector/at(values, 0i32)))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_stdlib_namespaced_vector_access_count_non_string_reject.prime", source);
  const std::string errPath = (testScratchPath("") /
                               "primec_cpp_stdlib_namespaced_vector_access_count_non_string_reject.err")
                                  .string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK_FALSE(readFile(errPath).empty());
}

TEST_CASE("C++ emitter keeps canonical vector unsafe direct-call string count forwarding") {
  const std::string source = R"(
[return<int>]
/string/count([string] values) {
  return(91i32)
}

[return<string>]
/std/collections/vector/at_unsafe([vector<i32>] values, [i32] index) {
  return("abc"raw_utf8)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32)}
  return(count(/std/collections/vector/at_unsafe(values, 0i32)))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_canonical_vector_access_unsafe_direct_count_forwarding.prime", source);
  const std::string exePath =
      (testScratchPath("") /
       "primec_cpp_canonical_vector_access_unsafe_direct_count_forwarding_exe")
          .string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 91);
}

TEST_CASE("C++ emitter keeps canonical vector method helper return precedence over string count shadow") {
  const std::string source = R"(
[return<int>]
/string/count([string] values) {
  return(91i32)
}

[return<i32>]
/std/collections/vector/at([vector<string>] values, [i32] index) {
  return(7i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<string>] values{vector<string>("hello"raw_utf8)}
  return(values.at(0i32).count())
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_canonical_vector_method_access_count_shadow_reject.prime", source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_cpp_canonical_vector_method_access_count_shadow_reject.err")
          .string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("unknown method: /i32/count") != std::string::npos);
}

TEST_CASE("C++ emitter keeps canonical vector unsafe method string count forwarding") {
  const std::string source = R"(
[return<int>]
/string/count([string] values) {
  return(91i32)
}

[return<string>]
/std/collections/vector/at_unsafe([vector<i32>] values, [i32] index) {
  return("abc"raw_utf8)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32)}
  return(values.at_unsafe(0i32).count())
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_canonical_vector_unsafe_method_access_count_shadow_forwarding.prime", source);
  const std::string exePath =
      (testScratchPath("") /
       "primec_cpp_canonical_vector_unsafe_method_access_count_shadow_forwarding_exe")
          .string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 91);
}

TEST_CASE("rejects slash-method vector access element-type count fallback in C++ emitter") {
  const std::string source = R"(
[return<int>]
/string/count([string] values) {
  return(91i32)
}

[return<i32>]
/std/collections/vector/at([vector<string>] values, [i32] index) {
  return(7i32)
}

[return<i32>]
/std/collections/vector/at_unsafe([vector<string>] values, [i32] index) {
  return(7i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<string>] values{vector<string>("hello"raw_utf8)}
  return(plus(values./vector/at(0i32).count(),
              values./std/collections/vector/at_unsafe(0i32).count()))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_slash_method_vector_access_element_type_count_fallback_reject.prime", source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_cpp_slash_method_vector_access_element_type_count_fallback_reject.err")
          .string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("unknown method: /i32/count") != std::string::npos);
}

TEST_CASE("C++ emitter keeps slash-method vector access helper return-kind count forwarding") {
  const std::string source = R"(
[return<int>]
/string/count([string] values) {
  return(91i32)
}

[return<string>]
/std/collections/vector/at([vector<i32>] values, [i32] index) {
  return("abc"raw_utf8)
}

[return<string>]
/std/collections/vector/at_unsafe([vector<i32>] values, [i32] index) {
  return("abc"raw_utf8)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32>] values{vector<i32>(1i32)}
  return(plus(values./vector/at(0i32).count(),
              values./std/collections/vector/at_unsafe(0i32).count()))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_slash_method_vector_access_helper_return_kind_count_forwarding.prime", source);
  const std::string exePath =
      (testScratchPath("") /
       "primec_cpp_slash_method_vector_access_helper_return_kind_count_forwarding_exe")
          .string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 182);
}

TEST_CASE("rejects wrapper-returned vector alias direct-call string count fallback in C++ emitter") {
  const std::string source = R"(
[return<int>]
/string/count([string] values) {
  return(91i32)
}

[return<i32>]
/std/collections/vector/at([vector<string>] values, [i32] index) {
  return(7i32)
}

[return<string>]
/std/collections/vector/at_unsafe([vector<string>] values, [i32] index) {
  return("abc"raw_utf8)
}

[effects(heap_alloc), return<vector<string>>]
wrapValues() {
  return(vector<string>("hello"raw_utf8))
}

[effects(heap_alloc), return<int>]
main() {
  return(plus(count(/vector/at(wrapValues(), 0i32)),
              wrapValues().at_unsafe(0i32).count()))
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_wrapper_vector_access_direct_count_fallback_reject.prime", source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_cpp_wrapper_vector_access_direct_count_fallback_reject.err")
          .string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("unknown method: /i32/count") != std::string::npos);
}

TEST_CASE("rejects bare vector method access receiver fallback without helper in C++ emitter") {
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
  return(values.at(1i32).tag())
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_bare_vector_method_access_receiver_fallback_reject.prime", source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_cpp_bare_vector_method_access_receiver_fallback_reject.err")
          .string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("unknown method: /vector/at") != std::string::npos);
}

TEST_CASE("rejects bare map method access receiver fallback without helper in C++ emitter") {
  const std::string source = R"(
namespace i32 {
  [return<int>]
  tag([i32] value) {
    return(plus(value, 40i32))
  }
}

[effects(heap_alloc), return<int>]
main() {
  [map<i32, i32>] values{map<i32, i32>(1i32, 5i32, 2i32, 6i32)}
  return(values.at(1i32).tag())
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_bare_map_method_access_receiver_fallback_reject.prime", source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_cpp_bare_map_method_access_receiver_fallback_reject.err")
          .string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("unknown call target: /std/collections/map/at") !=
        std::string::npos);
}

TEST_CASE("rejects wrapper bare vector unsafe method access receiver fallback in C++ emitter") {
  const std::string source = R"(
namespace i32 {
  [return<int>]
  tag([i32] value) {
    return(plus(value, 40i32))
  }
}

[effects(heap_alloc), return<vector<i32>>]
wrapValues() {
  return(vector<i32>(5i32, 6i32, 7i32))
}

[effects(heap_alloc), return<int>]
main() {
  return(wrapValues().at_unsafe(1i32).tag())
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_wrapper_vector_unsafe_method_access_receiver_fallback_reject.prime", source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_cpp_wrapper_vector_unsafe_method_access_receiver_fallback_reject.err")
          .string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("unknown method: /vector/at_unsafe") != std::string::npos);
}

TEST_CASE("C++ emitter keeps wrapper vector alias direct-call struct method chain canonical forwarding") {
  const std::string source = R"(
Marker {
  [i32] value
}

[effects(heap_alloc), return<vector<i32>>]
wrapValues() {
  return(vector<i32>(5i32, 6i32, 7i32))
}

[return<Marker>]
/std/collections/vector/at([vector<i32>] values, [i32] index) {
  return(Marker(index))
}

[return<int>]
/Marker/tag([Marker] self) {
  return(self.value)
}

[effects(heap_alloc), return<int>]
main() {
  return(/vector/at(wrapValues(), 2i32).tag())
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_wrapper_vector_alias_direct_struct_method_chain_forwarding.prime", source);
  const std::string exePath =
      (testScratchPath("") /
       "primec_cpp_wrapper_vector_alias_direct_struct_method_chain_forwarding_exe")
          .string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 2);
}

TEST_CASE("rejects wrapper vector alias direct-call method receiver fallback without helper in C++ emitter") {
  const std::string source = R"(
namespace i32 {
  [return<int>]
  tag([i32] value) {
    return(plus(value, 40i32))
  }
}

[effects(heap_alloc), return<vector<i32>>]
wrapValues() {
  return(vector<i32>(5i32, 6i32, 7i32))
}

[effects(heap_alloc), return<int>]
main() {
  return(/vector/at(wrapValues(), 1i32).tag())
}
)";
  const std::string srcPath =
      writeTemp("compile_cpp_wrapper_vector_alias_direct_method_receiver_fallback_reject.prime", source);
  const std::string errPath =
      (testScratchPath("") /
       "primec_cpp_wrapper_vector_alias_direct_method_receiver_fallback_reject.err")
          .string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("unknown call target: /vector/at") != std::string::npos);
}

TEST_SUITE_END();
