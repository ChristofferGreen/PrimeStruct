#include "test_compile_run_helpers.h"

#include "test_compile_run_collection_conformance_helpers.h"
#include "test_compile_run_container_error_conformance_helpers.h"
#include "test_compile_run_checked_pointer_conformance_helpers.h"
#include "test_compile_run_unchecked_pointer_conformance_helpers.h"
#include "test_compile_run_native_backend_collections_helpers.h"

#if PRIMESTRUCT_NATIVE_COLLECTIONS_ENABLED
TEST_SUITE_BEGIN("primestruct.compile.run.native_backend.collections");

TEST_CASE("rejects native vector literal above local dynamic limit") {
  auto buildVectorLiteralArgs = [](int count) {
    std::string args;
    args.reserve(static_cast<size_t>(count) * 6);
    for (int i = 0; i < count; ++i) {
      if (i > 0) {
        args += ", ";
      }
      args += "1i32";
    }
    return args;
  };

  const std::string source = std::string(
      "import /std/collections/*\n\n"
      "[effects(heap_alloc), return<int>]\n"
      "main() {\n"
      "  [vector<i32> mut] values{vector<i32>(") +
                             buildVectorLiteralArgs(257) +
                             ")}\n"
                             "  return(count(values))\n"
                             "}\n";
  const std::string srcPath = writeTemp("compile_native_vector_literal_local_limit_overflow.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_native_vector_literal_local_limit_err.txt").string();

  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("vector literal exceeds local capacity limit (256)") != std::string::npos);
}

TEST_CASE("rejects native vector reserve beyond local dynamic limit") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32)}
  reserve(values, 257i32)
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_native_vector_reserve_local_limit.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_native_vector_reserve_limit_err.txt").string();

  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("vector reserve exceeds local capacity limit (256)") != std::string::npos);
}

TEST_CASE("rejects native vector reserve negative literal at lowering") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32)}
  reserve(values, -1i32)
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_native_vector_reserve_negative_literal.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_native_vector_reserve_negative_literal_err.txt").string();

  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("vector reserve expects non-negative capacity") != std::string::npos);
}

TEST_CASE("rejects native vector reserve folded expression beyond local dynamic limit") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32)}
  reserve(values, plus(200i32, 57i32))
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_native_vector_reserve_folded_limit.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_native_vector_reserve_folded_limit_err.txt").string();

  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("vector reserve exceeds local capacity limit (256)") != std::string::npos);
}

TEST_CASE("rejects native vector reserve folded negative expression at lowering") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32)}
  reserve(values, minus(1i32, 2i32))
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_native_vector_reserve_folded_negative.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_native_vector_reserve_folded_negative_err.txt").string();

  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("vector reserve expects non-negative capacity") != std::string::npos);
}

TEST_CASE("rejects native vector reserve folded signed overflow at lowering") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32)}
  reserve(values, plus(9223372036854775807i64, 1i64))
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_native_vector_reserve_folded_signed_overflow.prime", source);
  const std::string errPath = (testScratchPath("") /
                               "primec_native_vector_reserve_folded_signed_overflow_err.txt")
                                  .string();

  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("vector reserve literal expression overflow") != std::string::npos);
}

TEST_CASE("rejects native vector reserve folded negate negative at lowering") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32)}
  reserve(values, negate(1i32))
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_native_vector_reserve_folded_negate_negative.prime", source);
  const std::string errPath = (testScratchPath("") /
                               "primec_native_vector_reserve_folded_negate_negative_err.txt")
                                  .string();

  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("vector reserve expects non-negative capacity") != std::string::npos);
}

TEST_CASE("rejects native vector reserve folded negate overflow at lowering") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32)}
  reserve(values, negate(-9223372036854775808i64))
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_native_vector_reserve_folded_negate_overflow.prime", source);
  const std::string errPath = (testScratchPath("") /
                               "primec_native_vector_reserve_folded_negate_overflow_err.txt")
                                  .string();

  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("vector reserve literal expression overflow") != std::string::npos);
}

TEST_CASE("rejects native vector reserve folded unsigned expression beyond local dynamic limit") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32)}
  reserve(values, plus(200u64, 57u64))
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_native_vector_reserve_folded_unsigned_limit.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_native_vector_reserve_folded_unsigned_limit_err.txt").string();

  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("vector reserve exceeds local capacity limit (256)") != std::string::npos);
}

TEST_CASE("rejects native vector reserve folded unsigned wraparound at lowering") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32)}
  reserve(values, minus(1u64, 2u64))
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_native_vector_reserve_folded_unsigned_wrap.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_native_vector_reserve_folded_unsigned_wrap_err.txt").string();

  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("vector reserve literal expression overflow") != std::string::npos);
}

TEST_CASE("rejects native vector reserve folded unsigned add overflow at lowering") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32)}
  reserve(values, plus(18446744073709551615u64, 1u64))
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_native_vector_reserve_folded_unsigned_add_overflow.prime", source);
  const std::string errPath = (testScratchPath("") /
                               "primec_native_vector_reserve_folded_unsigned_add_overflow_err.txt")
                                  .string();

  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("vector reserve literal expression overflow") != std::string::npos);
}

TEST_CASE("rejects native vector reserve dynamic value beyond local dynamic limit") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main([array<string>] args) {
  [vector<i32> mut] values{vector<i32>(1i32)}
  reserve(values, plus(257i32, count(args)))
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_native_vector_reserve_dynamic_limit.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_vector_reserve_dynamic_limit_exe").string();
  const std::string errPath =
      (testScratchPath("") / "primec_native_vector_reserve_dynamic_limit_err.txt").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string runCmd = exePath + " 2> " + errPath;
  CHECK(runCommand(runCmd) == 3);
  CHECK(readFile(errPath) == "array index out of bounds\n");
}

TEST_CASE("rejects native vector push beyond local dynamic limit") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>()}
  repeat(257i32) {
    push(values, 1i32)
  }
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_native_vector_push_local_limit.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_vector_push_local_limit_exe").string();
  const std::string errPath =
      (testScratchPath("") / "primec_native_vector_push_limit_err.txt").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string runCmd = exePath + " 2> " + errPath;
  CHECK(runCommand(runCmd) == 3);
  CHECK(readFile(errPath) == "vector push allocation failed (out of memory)\n");
}

TEST_CASE("compiles and runs native vector shrink helpers") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32, 2i32, 3i32, 4i32)}
  pop(values)
  remove_at(values, 1i32)
  remove_swap(values, 0i32)
  last{at(values, 0i32)}
  size{count(values)}
  clear(values)
  return(plus(plus(last, size), count(values)))
}
)";
  const std::string srcPath = writeTemp("compile_native_vector_shrink_helpers.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_vector_shrink_helpers_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 4);
}

TEST_SUITE_END();
#endif
