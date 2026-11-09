#include "../test_compile_run_helpers.h"

#include "test_compile_run_native_backend_core_helpers.h"
#include "../test_compile_run_scene_model_helpers.h"

#if PRIMESTRUCT_NATIVE_CORE_ENABLED
TEST_SUITE_BEGIN("primestruct.compile.run.native_backend.core");

TEST_CASE("native void call with string param") {
  const std::string source = R"(
[return<void> effects(io_out)]
echo([string] msg) {
  print_line(msg)
}

[return<int> effects(io_out)]
main() {
  echo("alpha"utf8)
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_native_string_call.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_native_string_call_exe").string();
  const std::string outPath = (testScratchPath("") / "primec_native_string_call_out.txt").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string runCmd = exePath + " > " + outPath;
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(outPath) == "alpha\n");
}

TEST_CASE("native string indexing") {
  const std::string source = R"(
[return<int>]
main() {
  [string] text{"abc"utf8}
  [i32] a{at(text, 0i32)}
  [i32] b{at_unsafe(text, 1i32)}
  [i32] len{count(text)}
  return(plus(plus(a, b), len))
}
)";
  const std::string srcPath = writeTemp("compile_native_string_index_builtin.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_string_index_builtin_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == (97 + 98 + 3));
}

TEST_CASE("native string parameter indexing") {
  const std::string source = R"(
[return<i32>]
pick([string] text, [i32] index) {
  return(at(text, index))
}

[return<int>]
main() {
  return(pick("abc"utf8, 1i32))
}
)";
  const std::string srcPath = writeTemp("compile_native_string_param_index.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_native_string_param_index_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 98);
}

TEST_CASE("native scene model authoring deterministically") {
  const std::string srcPath =
      writeTemp("compile_native_scene_model_descriptors.prime", sceneModelNativeDescriptorSource());
  const std::string exePath =
      (testScratchPath("") / "primec_native_scene_model_descriptors").string();

  const std::string compileCmd =
      "./primec --emit=native " + quoteShellArg(srcPath) + " -o " + quoteShellArg(exePath) + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(quoteShellArg(exePath)) == 14);
}

TEST_CASE("native exe handles a 300-local stack frame") {
  std::ostringstream source;
  source << "[return<int>]\n";
  source << "main() {\n";
  for (int i = 0; i < 300; ++i) {
    source << "  [i32] v" << i << "{" << i << "i32}\n";
  }
  source << "  return(v0)\n";
  source << "}\n";
  const std::string srcPath = writeTemp("compile_native_large_frame.prime", source.str());
  const std::string exePath = (testScratchPath("") / "primec_native_large_frame_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 0);
}

TEST_CASE("rejects native recursive calls") {
  const std::string source = R"(
[return<int>]
main() {
  return(main())
}
)";
  const std::string srcPath = writeTemp("compile_native_recursive.prime", source);
  const std::string errPath = (testScratchPath("") / "primec_native_recursive_err.txt").string();
  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath) == "Native lowering error: native backend does not support recursive calls: /main\n");
}

TEST_CASE("native accepts string pointers") {
  const std::string source = R"(
[return<void>]
main() {
  [string] name{"hi"utf8}
  [Pointer<string>] ptr{location(name)}
}
)";
  const std::string srcPath = writeTemp("compile_native_string_ptr.prime", source);
  const std::string errPath = (testScratchPath("") / "primec_native_string_ptr_err.txt").string();
  const std::string exePath = (testScratchPath("") / "primec_native_string_ptr_exe").string();
  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 0);
  CHECK(readFile(errPath).empty());
}

TEST_CASE("native rejects variadic pointer string packs") {
  const std::string source = R"(
[return<int>]
score([args<Pointer<string>>] values) {
  return(count(values))
}

[return<int>]
main() {
  [string] first{"first"utf8}
  [Pointer<string>] p0{location(first)}
  score(p0)
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_native_variadic_args_pointer_string_reject.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_native_variadic_args_pointer_string_reject_err.txt").string();
  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("variadic args<T> does not support string pointers or references") != std::string::npos);
}

TEST_CASE("native rejects variadic reference string packs") {
  const std::string source = R"(
[return<int>]
score([args<Reference<string>>] values) {
  return(count(values))
}

[return<int>]
main() {
  [string] first{"first"utf8}
  [Reference<string>] r0{location(first)}
  score(r0)
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_native_variadic_args_reference_string_reject.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_native_variadic_args_reference_string_reject_err.txt").string();
  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("variadic args<T> does not support string pointers or references") != std::string::npos);
}

TEST_CASE("native ignores top-level executions") {
  const std::string source = R"(
[return<bool>]
unused() {
  return(equal("a"utf8, "b"utf8))
}

[return<int>]
main() {
  return(0i32)
}

unused()
)";
  const std::string srcPath = writeTemp("compile_native_exec_ignored.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_native_exec_ignored_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 0);
}

TEST_SUITE_END();
#endif
