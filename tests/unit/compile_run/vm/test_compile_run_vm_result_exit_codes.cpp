#include "../test_compile_run_helpers.h"

#include "../native_backend/test_compile_run_native_backend_core_helpers.h"

TEST_SUITE_BEGIN("primestruct.compile.run.vm.core");

namespace {

// A Result-returning entry ends the process with the same exit code on the VM, native, C++ and optimized C++
// executables.
void checkEntryResultExitCode(const std::string &name, const std::string &source, int expected) {
  const std::string srcPath = writeTemp("entry_result_exit_" + name + ".prime", source);
  CHECK(runCommand("./primec --emit=vm " + srcPath + " --entry /main") == expected);

  const std::string exePath =
      (testScratchPath("") / ("primec_entry_result_exit_" + name + "_exe")).string();
  CHECK(runCommand("./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main") == 0);
  CHECK(runCommand(exePath) == expected);

  const std::string optExePath =
      (testScratchPath("") / ("primec_entry_result_exit_" + name + "_optexe")).string();
  CHECK(runCommand("./primec --emit=optexe " + srcPath + " -o " + optExePath + " --entry /main") ==
        0);
  CHECK(runCommand(optExePath) == expected);

#if PRIMESTRUCT_NATIVE_CORE_ENABLED
  const std::string nativePath =
      (testScratchPath("") / ("primec_entry_result_exit_" + name + "_native")).string();
  CHECK(runCommand("./primec --emit=native " + srcPath + " -o " + nativePath + " --entry /main") ==
        0);
  CHECK(runCommand(nativePath) == expected);
#endif
}

} // namespace

TEST_CASE("entry returning a stdlib Result ok exits with its payload") {
  checkEntryResultExitCode("sum_ok",
                           R"(
import /std/result/*
[return<Result<i32, i32>>]
main() {
  return(ok<i32, i32>(5i32))
}
)",
                           5);
}

TEST_CASE("entry returning a stdlib Result error exits with the error code") {
  checkEntryResultExitCode("sum_error",
                           R"(
import /std/result/*
[return<Result<i32, i32>>]
main() {
  return(error<i32, i32>(3i32))
}
)",
                           3);
}

TEST_CASE("entry forwarding a stdlib Result call exits with its error code") {
  checkEntryResultExitCode("sum_forwarded",
                           R"(
import /std/result/*
[return<Result<i32, i32>>]
inner([i32] x) {
  if(x > 1i32) {
    return(error<i32, i32>(x))
  }
  return(ok<i32, i32>(x))
}
[return<Result<i32, i32>>]
main() {
  return(inner(2i32))
}
)",
                           2);
}

TEST_CASE("entry returning a struct error exits 1") {
  checkEntryResultExitCode("sum_struct_error",
                           R"(
import /std/result/*
[struct]
Oops() {
  [i32] code{7i32}
}
[return<Result<i32, Oops>>]
main() {
  return(error<i32, Oops>(Oops{9i32}))
}
)",
                           1);
}

TEST_CASE("entry returning an error code of 0 exits 1, not as ok") {
  checkEntryResultExitCode("sum_error_zero",
                           R"(
import /std/result/*
[return<Result<i32, i32>>]
main() {
  return(error<i32, i32>(0i32))
}
)",
                           1);
}

TEST_CASE("entry returning an error code above 255 exits 1") {
  checkEntryResultExitCode("sum_error_large",
                           R"(
import /std/result/*
[return<Result<i32, i32>>]
main() {
  return(error<i32, i32>(300i32))
}
)",
                           1);
}

TEST_CASE("entry returning a status-only stdlib Result ok exits 0") {
  checkEntryResultExitCode("sum_status_ok",
                           R"(
import /std/result/*
[return<Result<i32>>]
main() {
  return(ok<i32>())
}
)",
                           0);
}

TEST_CASE("entry returning a builtin value Result ok exits with its payload") {
  checkEntryResultExitCode("packed_value_ok",
                           R"(
[return<Result<i32, i32>>]
main() {
  return(Result.ok(5i32))
}
)",
                           5);
}

TEST_CASE("entry returning a builtin status Result exits with its code") {
  checkEntryResultExitCode("packed_status_error",
                           R"(
[return<Result<i32>>]
main() {
  return(4i32)
}
)",
                           4);
}

TEST_CASE("entry returning a builtin status Result above 255 exits 1") {
  checkEntryResultExitCode("packed_status_large",
                           R"(
[return<Result<i32>>]
main() {
  return(300i32)
}
)",
                           1);
}

TEST_CASE("entry returning Result.ok under the stdlib import exits with its payload") {
  checkEntryResultExitCode("sum_legacy_ok",
                           R"(
import /std/result/*
[return<Result<i32, i32>>]
main() {
  return(Result.ok(6i32))
}
)",
                           6);
}

TEST_CASE("entry returning a stdlib Result ok above 255 exits with the low 8 bits") {
  checkEntryResultExitCode("sum_ok_large",
                           R"(
import /std/result/*
[return<Result<i32, i32>>]
main() {
  return(ok<i32, i32>(300i32))
}
)",
                           44);
}

TEST_SUITE_END();
