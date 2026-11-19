#include "../test_compile_run_helpers.h"

#include "../test_compile_run_collection_conformance_helpers.h"
#include "../test_compile_run_container_error_conformance_helpers.h"
#include "../test_compile_run_checked_pointer_conformance_helpers.h"
#include "../test_compile_run_unchecked_pointer_conformance_helpers.h"
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
                             buildVectorLiteralArgs(1025) +
                             ")}\n"
                             "  return(count(values))\n"
                             "}\n";
  const std::string srcPath = writeTemp("compile_native_vector_literal_local_limit_overflow.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_native_vector_literal_local_limit_err.txt").string();

  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("collection literal exceeds local capacity limit (1024)") != std::string::npos);
}

TEST_CASE("native vector push past former local dynamic limit") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>()}
  repeat(300i32) {
    push(values, 1i32)
  }
  return(plus(at(values, 0i32), at(values, 299i32)))
}
)";
  const std::string srcPath = writeTemp("compile_native_vector_push_past_former_limit.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_vector_push_past_former_limit_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 2);
}

TEST_CASE("rejects native vector reserve dynamic value beyond local dynamic limit") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main([array<string>] args) {
  [vector<i32> mut] values{vector<i32>(1i32)}
  reserve(values, plus(1025i32, count(args)))
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
  // Dynamic vector storage landed: reserving beyond the former local limit
  // now succeeds, so the historical OOB-trap pin no longer applies.
  CHECK(runCommand(exePath) == 0);
}

TEST_CASE("rejects native vector push beyond local dynamic limit") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>()}
  repeat(1025i32) {
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
  // Dynamic vector storage landed: pushing past the former local limit now
  // grows the vector, so the historical OOM-trap pin no longer applies.
  CHECK(runCommand(exePath) == 0);
}

TEST_CASE("rejects native vector reserve literals the lowering folds") {
  // The lowering folds `plus`/`minus`/`negate` trees of integer literals and rejects the ones that are out of
  // range; the VM lowering gives the same diagnostics (the VM suite pins them too). 1024 is the last accepted one.
  const std::string limit = "vector reserve exceeds local capacity limit (1024)";
  const std::string negative = "vector reserve expects non-negative capacity";
  const std::string overflow = "vector reserve literal expression overflow";
  const std::vector<std::pair<std::string, std::string>> cases = {
      {"1025i32", limit},
      {"plus(1000i32, 25i32)", limit},
      {"plus(1000u64, 25u64)", limit},
      {"-1i32", negative},
      {"minus(1i32, 2i32)", negative},
      {"negate(1i32)", negative},
      {"minus(1u64, 2u64)", overflow},
      {"plus(9223372036854775807i64, 1i64)", overflow},
      {"negate(-9223372036854775808i64)", overflow},
      {"plus(18446744073709551615u64, 1u64)", overflow},
  };
  int index = 0;
  for (const auto &[argument, message] : cases) {
    std::string source = "import /std/collections/*\n\n";
    source += "[effects(heap_alloc), return<int>]\nmain() {\n";
    source += "  [vector<i32> mut] values{vector<i32>(1i32)}\n";
    source += "  reserve(values, " + argument + ")\n";
    source += "  return(0i32)\n}\n";
    const std::string name = "native_vector_reserve_fold_" + std::to_string(index++);
    const std::string srcPath = writeTemp(name + ".prime", source);
    const std::string errPath = (testScratchPath("") / (name + ".err")).string();
    const std::string compileCmd =
        "./primec --emit=native " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
    CAPTURE(argument);
    CHECK(runCommand(compileCmd) == 2);
    CHECK(readFile(errPath).find("Native lowering error: " + message) != std::string::npos);
  }
}

TEST_CASE("native vector reserve accepts the local capacity limit") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32)}
  reserve(values, 1024i32)
  return(convert<i32>(and(equal(count(values), 1i32), equal(capacity(values), 1024i32))))
}
)";
  const std::string srcPath = writeTemp("compile_native_vector_reserve_at_limit.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_vector_reserve_at_limit_exe").string();
  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 1);
}

TEST_CASE("native vector shrink helpers") {
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
