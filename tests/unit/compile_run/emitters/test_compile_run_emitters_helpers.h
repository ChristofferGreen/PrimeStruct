#pragma once

bool buildCachedEmittedCppExecutableAtO0(const std::string &fixtureName,
                                         const std::string &source,
                                         std::string &exePathOut);

[[maybe_unused]] static void expectCppVectorCountCompatibilityTypeMismatchReject(const std::string &compileCmd) {
  const std::string errPath =
      testScratchPath("primec_cpp_vector_count_compatibility_type_mismatch_reject_err.txt").string();
  const std::string captureCmd = compileCmd + " > /dev/null 2> " + errPath;
  CHECK(runCommand(captureCmd) != 0);
  CHECK_FALSE(readFile(errPath).empty());
}

[[maybe_unused]] static std::string sharedCppEmitterFixtureArgs(int selector) {
  std::string args;
  for (int index = 1; index < selector; ++index) {
    args += " ";
    args += quoteShellArg("case" + std::to_string(index));
  }
  return args;
}

[[maybe_unused]] static std::string requireSharedCppEmitterFixtureExecutable(const std::string &fixtureName,
                                                                             const std::string &source) {
  std::string exePath;
  REQUIRE(buildCachedEmittedCppExecutableAtO0(fixtureName, source, exePath));
  return exePath;
}

[[maybe_unused]] static int runSharedCppEmitterFixture(const std::string &fixtureName,
                                                       const std::string &source,
                                                       int selector) {
  const std::string exePath = requireSharedCppEmitterFixtureExecutable(fixtureName, source);
  return runCommand(quoteShellArg(exePath) + sharedCppEmitterFixtureArgs(selector));
}

[[maybe_unused]] static int runSharedCppEmitterFixtureToFile(const std::string &fixtureName,
                                                             const std::string &source,
                                                             int selector,
                                                             const std::string &outPath) {
  const std::string exePath = requireSharedCppEmitterFixtureExecutable(fixtureName, source);
  return runCommand(quoteShellArg(exePath) + sharedCppEmitterFixtureArgs(selector) + " > " + quoteShellArg(outPath));
}

[[maybe_unused]] static const std::string &sharedCppEmitterResultFixtureSource() {
  static const std::string source = R"(
[return<Result<i32, FileError>>]
lift([i32] value) {
  return(Result.ok(value))
}

[return<int>]
run_result_combinators() {
  [Result<i32, FileError>] first{ Result.ok(2i32) }
  [Result<i32, FileError>] second{ Result.ok(3i32) }
  return(
    Result.and_then(
      Result.map2(
        Result.map(first, []([i32] value) { return(multiply(value, 4i32)) }),
        second,
        []([i32] left, [i32] right) { return(plus(left, right)) }
      ),
      []([i32] total) { return(lift(plus(total, 5i32))) }
    )
  )
}

[return<int>]
run_result_ok_lambda() {
  [Result<i32, FileError>] first{ Result.ok(2i32) }
  [Result<i32, FileError>] second{ Result.ok(3i32) }
  return(
    Result.and_then(
      Result.map2(first, second, []([i32] left, [i32] right) { return(plus(left, right)) }),
      []([i32] total) { return(Result.ok(multiply(total, 4i32))) }
    )
  )
}

[return<int>]
main([array<string>] args) {
  [i32] selector{count(args)}
  if(equal(selector, 2i32)) {
    return(run_result_combinators())
  }
  if(equal(selector, 3i32)) {
    return(run_result_ok_lambda())
  }
  return(91i32)
}
)";
  return source;
}

[[maybe_unused]] static const std::string &sharedCppEmitterImageFixtureSource() {
  static const std::string source = R"(
import /std/image/*

[effects(heap_alloc, io_out, file_write), return<int>]
main([array<string>] args) {
  [i32 mut] width{0i32}
  [i32 mut] height{0i32}
  [vector<i32> mut] pixels{vector<i32>()}
  print_line(Result.why(/std/image/ppm/read(width, height, pixels, "input.ppm"utf8)))
  print_line(Result.why(/std/image/ppm/write("output.ppm"utf8, width, height, pixels)))
  print_line(Result.why(/std/image/png/read(width, height, pixels, "input.png"utf8)))
  print_line(Result.why(/std/image/png/write("output.png"utf8, width, height, pixels)))
  return(plus(width, height))
}
)";
  return source;
}
