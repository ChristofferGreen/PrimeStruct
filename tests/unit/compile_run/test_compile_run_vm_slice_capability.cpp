#include "test_compile_run_helpers.h"

// TODO-5250: Slice<T, Capability> desugars to array<T> (the same physical
// representation slice(...) already produces), scoped to function
// parameters, mirroring TODO-5249's Reference<T, Capability>.

TEST_SUITE_BEGIN("primestruct.compile.run.vm.slice_capability");

TEST_CASE("runs vm with Read-capability slice parameter") {
  const std::string source = R"(
[return<int>]
sum_window([Slice<i32, Read>] window) {
  [i32 mut] total{0i32}
  [i32 mut] i{0i32}
  while(i < count(window)) {
    total = total + window[i]
    i = i + 1i32
  }
  return(total)
}

[return<int>]
main() {
  [array<i32>] values{array<i32>(4i32, 7i32, 9i32, 11i32)}
  [array<i32>] window{slice(values, 1i32, 3i32)}
  return(sum_window(window))
}
)";
  const std::string srcPath = writeTemp("vm_slice_capability_read.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 16);

  const std::string nativePath =
      (testScratchPath("") / "primec_slice_capability_read").string();
  const std::string compileNativeCmd =
      "./primec --emit=native " + srcPath + " -o " + nativePath + " --entry /main";
  CHECK(runCommand(compileNativeCmd) == 0);
  CHECK(runCommand(nativePath) == 16);
}

TEST_CASE("runs vm with ReadWrite-capability mut slice parameter") {
  const std::string source = R"(
[return<int>]
overwrite_first([Slice<i32, ReadWrite> mut] window) {
  window[0i32] = 99i32
  return(window[0i32])
}

[return<int>]
main() {
  [array<i32>] values{array<i32>(4i32, 7i32, 9i32)}
  [array<i32>] window{slice(values, 0i32, 2i32)}
  return(overwrite_first(window))
}
)";
  const std::string srcPath = writeTemp("vm_slice_capability_readwrite.prime", source);
  const std::string runVmCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runVmCmd) == 99);

  const std::string nativePath =
      (testScratchPath("") / "primec_slice_capability_readwrite").string();
  const std::string compileNativeCmd =
      "./primec --emit=native " + srcPath + " -o " + nativePath + " --entry /main";
  CHECK(runCommand(compileNativeCmd) == 0);
  CHECK(runCommand(nativePath) == 99);
}

TEST_CASE("rejects mutation attempted through a Read-capability slice parameter") {
  const std::string source = R"(
[return<int>]
f([Slice<i32, Read>] window) {
  window[0i32] = 99i32
  return(window[0i32])
}

[return<int>]
main() {
  [array<i32>] values{array<i32>(4i32, 7i32)}
  [array<i32>] window{slice(values, 0i32, 2i32)}
  return(f(window))
}
)";
  const std::string srcPath = writeTemp("vm_slice_capability_read_assign_rejected.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_slice_capability_read_assign_err.txt").string();
  const std::string compileCmd =
      "./primec --emit=vm " + srcPath + " --entry /main > /dev/null 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("assign target must be a mutable binding") != std::string::npos);
}

TEST_CASE("rejects Write-capability slice parameter not declared mut") {
  const std::string source = R"(
[return<int>]
f([Slice<i32, Write>] window) {
  return(window[0i32])
}

[return<int>]
main() {
  [array<i32>] values{array<i32>(4i32, 7i32)}
  [array<i32>] window{slice(values, 0i32, 2i32)}
  return(f(window))
}
)";
  const std::string srcPath = writeTemp("vm_slice_capability_write_no_mut_rejected.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_slice_capability_write_no_mut_err.txt").string();
  const std::string compileCmd =
      "./primec --emit=vm " + srcPath + " --entry /main > /dev/null 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("Slice<i32, Write> binding requires mut") != std::string::npos);
}

TEST_CASE("rejects Read-capability slice parameter declared mut") {
  const std::string source = R"(
[return<int>]
f([Slice<i32, Read> mut] window) {
  return(window[0i32])
}

[return<int>]
main() {
  [array<i32>] values{array<i32>(4i32, 7i32)}
  [array<i32>] window{slice(values, 0i32, 2i32)}
  return(f(window))
}
)";
  const std::string srcPath = writeTemp("vm_slice_capability_read_mut_rejected.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_slice_capability_read_mut_err.txt").string();
  const std::string compileCmd =
      "./primec --emit=vm " + srcPath + " --entry /main > /dev/null 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("Slice<i32, Read> binding cannot be mut") != std::string::npos);
}

TEST_CASE("rejects unknown capability name on a slice parameter") {
  const std::string source = R"(
[return<int>]
f([Slice<i32, Bogus>] window) {
  return(window[0i32])
}

[return<int>]
main() {
  [array<i32>] values{array<i32>(4i32, 7i32)}
  [array<i32>] window{slice(values, 0i32, 2i32)}
  return(f(window))
}
)";
  const std::string srcPath = writeTemp("vm_slice_capability_unknown_name_rejected.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_slice_capability_unknown_name_err.txt").string();
  const std::string compileCmd =
      "./primec --emit=vm " + srcPath + " --entry /main > /dev/null 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("unknown Slice capability: Bogus") != std::string::npos);
}

TEST_CASE("rejects Slice with only one template argument") {
  const std::string source = R"(
[return<int>]
f([Slice<i32>] window) {
  return(window[0i32])
}

[return<int>]
main() {
  [array<i32>] values{array<i32>(4i32, 7i32)}
  [array<i32>] window{slice(values, 0i32, 2i32)}
  return(f(window))
}
)";
  const std::string srcPath = writeTemp("vm_slice_capability_single_arg_rejected.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_slice_capability_single_arg_err.txt").string();
  const std::string compileCmd =
      "./primec --emit=vm " + srcPath + " --entry /main > /dev/null 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("Slice requires exactly two template arguments") !=
        std::string::npos);
}

TEST_CASE("runs vm with Read-capability slice local binding") {
  // TODO-5251: local bindings now get real capability support - see the
  // matching Reference<T, Capability> test in
  // test_compile_run_vm_reference_capability.cpp for the root cause.
  const std::string source = R"(
[return<int>]
main() {
  [array<i32>] values{array<i32>(4i32, 7i32, 9i32, 11i32)}
  [Slice<i32, Read>] window{slice(values, 1i32, 3i32)}
  [i32 mut] total{0i32}
  [i32 mut] i{0i32}
  while(i < count(window)) {
    total = total + window[i]
    i = i + 1i32
  }
  return(total)
}
)";
  const std::string srcPath = writeTemp("vm_slice_capability_local_binding_read.prime", source);
  const std::string runVmCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runVmCmd) == 16);

  const std::string nativePath =
      (testScratchPath("") / "primec_slice_capability_local_binding_read").string();
  const std::string compileNativeCmd =
      "./primec --emit=native " + srcPath + " -o " + nativePath + " --entry /main";
  CHECK(runCommand(compileNativeCmd) == 0);
  CHECK(runCommand(nativePath) == 16);
}

TEST_CASE("runs vm with ReadWrite-capability mut slice local binding") {
  const std::string source = R"(
[return<int>]
main() {
  [array<i32>] values{array<i32>(4i32, 7i32, 9i32)}
  [Slice<i32, ReadWrite> mut] window{slice(values, 0i32, 2i32)}
  window[0i32] = 99i32
  return(window[0i32])
}
)";
  const std::string srcPath = writeTemp("vm_slice_capability_local_binding_readwrite.prime", source);
  const std::string runVmCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runVmCmd) == 99);

  const std::string nativePath =
      (testScratchPath("") / "primec_slice_capability_local_binding_readwrite").string();
  const std::string compileNativeCmd =
      "./primec --emit=native " + srcPath + " -o " + nativePath + " --entry /main";
  CHECK(runCommand(compileNativeCmd) == 0);
  CHECK(runCommand(nativePath) == 99);
}

TEST_CASE("rejects mutation through a Read-capability slice local binding") {
  const std::string source = R"(
[return<int>]
main() {
  [array<i32>] values{array<i32>(4i32, 7i32)}
  [Slice<i32, Read>] window{slice(values, 0i32, 2i32)}
  window[0i32] = 99i32
  return(window[0i32])
}
)";
  const std::string srcPath = writeTemp("vm_slice_capability_local_binding_read_assign_rejected.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_slice_capability_local_binding_read_assign_err.txt").string();
  const std::string compileCmd =
      "./primec --emit=vm " + srcPath + " --entry /main > /dev/null 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("assign target must be a mutable binding") != std::string::npos);
}

TEST_CASE("plain array parameters and locals are unaffected by Slice recognition") {
  const std::string source = R"(
[return<int>]
sum_all([array<i32>] values) {
  [i32 mut] total{0i32}
  [i32 mut] i{0i32}
  while(i < count(values)) {
    total = total + values[i]
    i = i + 1i32
  }
  return(total)
}

[return<int>]
main() {
  [array<i32>] values{array<i32>(1i32, 2i32, 3i32, 4i32)}
  return(sum_all(values))
}
)";
  const std::string srcPath = writeTemp("vm_slice_capability_plain_array_unaffected.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 10);
}

TEST_SUITE_END();
