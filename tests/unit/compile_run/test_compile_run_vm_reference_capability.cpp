#include "test_compile_run_helpers.h"

// TODO-5249: Reference<T, Capability>/Pointer<T, Capability> parsing and
// compile-time read/write enforcement, scoped to function parameters.

TEST_SUITE_BEGIN("primestruct.compile.run.vm.reference_capability");

TEST_CASE("runs vm with Read-capability reference parameter") {
  const std::string source = R"(
[return<int>]
sum_read_only([Reference<array<i32>, Read>] values) {
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
  [array<i32>] values{array<i32>(1i32, 2i32, 3i32)}
  return(sum_read_only(values))
}
)";
  const std::string srcPath = writeTemp("vm_reference_capability_read.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 6);

  const std::string nativePath =
      (testScratchPath("") / "primec_reference_capability_read").string();
  const std::string compileNativeCmd =
      "./primec --emit=native " + srcPath + " -o " + nativePath + " --entry /main";
  CHECK(runCommand(compileNativeCmd) == 0);
  CHECK(runCommand(nativePath) == 6);
}

TEST_CASE("runs vm with ReadWrite-capability mut reference parameter") {
  const std::string source = R"(
[return<int>]
double_in_place([Reference<i32, ReadWrite> mut] value) {
  assign(dereference(value), dereference(value) + dereference(value))
  return(dereference(value))
}

[return<int>]
main() {
  [i32 mut] value{5i32}
  return(double_in_place(location(value)))
}
)";
  const std::string srcPath = writeTemp("vm_reference_capability_readwrite.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 10);
}

TEST_CASE("rejects Read-capability reference parameter declared mut") {
  const std::string source = R"(
[return<int>]
f([Reference<i32, Read> mut] r) {
  return(dereference(r))
}

[return<int>]
main() {
  [i32 mut] v{4i32}
  return(f(location(v)))
}
)";
  const std::string srcPath = writeTemp("vm_reference_capability_read_mut_rejected.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_reference_capability_read_mut_err.txt").string();
  const std::string compileCmd =
      "./primec --emit=vm " + srcPath + " --entry /main > /dev/null 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("Reference<i32, Read> binding cannot be mut") != std::string::npos);
}

TEST_CASE("rejects Write-capability reference parameter not declared mut") {
  const std::string source = R"(
[return<int>]
f([Reference<i32, Write>] r) {
  return(dereference(r))
}

[return<int>]
main() {
  [i32 mut] v{4i32}
  return(f(location(v)))
}
)";
  const std::string srcPath = writeTemp("vm_reference_capability_write_no_mut_rejected.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_reference_capability_write_no_mut_err.txt").string();
  const std::string compileCmd =
      "./primec --emit=vm " + srcPath + " --entry /main > /dev/null 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("Reference<i32, Write> binding requires mut") != std::string::npos);
}

TEST_CASE("rejects mutation attempted through a Read-capability reference parameter") {
  const std::string source = R"(
[return<int>]
f([Reference<i32, Read>] r) {
  assign(dereference(r), 9i32)
  return(dereference(r))
}

[return<int>]
main() {
  [i32 mut] v{4i32}
  return(f(location(v)))
}
)";
  const std::string srcPath = writeTemp("vm_reference_capability_read_assign_rejected.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_reference_capability_read_assign_err.txt").string();
  const std::string compileCmd =
      "./primec --emit=vm " + srcPath + " --entry /main > /dev/null 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("assign target must be a mutable binding") != std::string::npos);
}

TEST_CASE("rejects unknown capability name on a reference parameter") {
  const std::string source = R"(
[return<int>]
f([Reference<i32, Bogus>] r) {
  return(dereference(r))
}

[return<int>]
main() {
  [i32 mut] v{4i32}
  return(f(location(v)))
}
)";
  const std::string srcPath = writeTemp("vm_reference_capability_unknown_name_rejected.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_reference_capability_unknown_name_err.txt").string();
  const std::string compileCmd =
      "./primec --emit=vm " + srcPath + " --entry /main > /dev/null 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("unknown Reference capability: Bogus") != std::string::npos);
}

TEST_CASE("rejects capability-parameterized reference on a local binding") {
  const std::string source = R"(
[return<int>]
main() {
  [int mut] value{4}
  [Reference<int, Read>] ref{location(value)}
  return(dereference(ref))
}
)";
  const std::string srcPath = writeTemp("vm_reference_capability_local_binding_rejected.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_reference_capability_local_binding_err.txt").string();
  const std::string compileCmd =
      "./primec --emit=vm " + srcPath + " --entry /main > /dev/null 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find(
            "Reference<T, Capability> is only supported for function parameters today") !=
        std::string::npos);
}

TEST_CASE("plain single-argument Reference and Pointer parameters are unaffected") {
  const std::string source = R"(
[return<int>]
read_plain([Reference<i32>] r) {
  return(dereference(r))
}

[return<int>]
main() {
  [i32 mut] v{7i32}
  return(read_plain(location(v)))
}
)";
  const std::string srcPath = writeTemp("vm_reference_capability_plain_unaffected.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 7);
}

TEST_SUITE_END();
