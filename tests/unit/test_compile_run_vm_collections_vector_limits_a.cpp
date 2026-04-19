#include "test_compile_run_helpers.h"

#include "test_compile_run_collection_conformance_helpers.h"
#include "test_compile_run_container_error_conformance_helpers.h"
#include "test_compile_run_checked_pointer_conformance_helpers.h"
#include "test_compile_run_unchecked_pointer_conformance_helpers.h"
#include "test_compile_run_vm_collections_helpers.h"

TEST_SUITE_BEGIN("primestruct.compile.run.vm.collections");

TEST_CASE("runs vm with user vector pop call shadow") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<void>]
/vector/pop([vector<i32> mut] values) {
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32, 2i32)}
  pop(values)
  return(count(values))
}
)";
  const std::string srcPath = writeTemp("vm_user_vector_pop_call_shadow.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 2);
}

TEST_CASE("runs vm with user vector pop method shadow") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<void>]
/vector/pop([vector<i32> mut] values) {
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32, 2i32)}
  values.pop()
  return(count(values))
}
)";
  const std::string srcPath = writeTemp("vm_user_vector_pop_method_shadow.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 2);
}

TEST_CASE("runs vm user vector pop call expression shadow" * doctest::skip(true)) {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/vector/pop([vector<i32> mut] values) {
  return(7i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32, 2i32)}
  return(pop(values))
}
)";
  const std::string srcPath = writeTemp("vm_user_vector_pop_call_expr_shadow.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 7);
}

TEST_CASE("runs vm with user vector reserve call shadow") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<void>]
/vector/reserve([vector<i32> mut] values, [i32] capacity) {
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32, 2i32)}
  reserve(values, 3i32)
  return(count(values))
}
)";
  const std::string srcPath = writeTemp("vm_user_vector_reserve_call_shadow.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 2);
}

TEST_CASE("runs vm with user vector reserve method shadow") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<void>]
/vector/reserve([vector<i32> mut] values, [i32] capacity) {
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32, 2i32)}
  values.reserve(3i32)
  return(count(values))
}
)";
  const std::string srcPath = writeTemp("vm_user_vector_reserve_method_shadow.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 2);
}

TEST_CASE("runs vm user vector reserve call expression shadow" * doctest::skip(true)) {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/vector/reserve([vector<i32> mut] values, [i32] capacity) {
  return(capacity)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32, 2i32)}
  return(reserve(values, 9i32))
}
)";
  const std::string srcPath = writeTemp("vm_user_vector_reserve_call_expr_shadow.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 9);
}

TEST_CASE("runs vm with user vector clear call shadow") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<void>]
/vector/clear([vector<i32> mut] values) {
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32, 2i32)}
  clear(values)
  return(count(values))
}
)";
  const std::string srcPath = writeTemp("vm_user_vector_clear_call_shadow.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 2);
}

TEST_CASE("runs vm user vector clear call expression shadow" * doctest::skip(true)) {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/vector/clear([vector<i32> mut] values) {
  return(8i32)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32, 2i32)}
  return(clear(values))
}
)";
  const std::string srcPath = writeTemp("vm_user_vector_clear_call_expr_shadow.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 8);
}

TEST_CASE("runs vm user vector remove_at call expression shadow" * doctest::skip(true)) {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/vector/remove_at([vector<i32> mut] values, [i32] index) {
  return(index)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32, 2i32)}
  return(remove_at(values, 6i32))
}
)";
  const std::string srcPath = writeTemp("vm_user_vector_remove_at_call_expr_shadow.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 6);
}

TEST_CASE("runs vm user vector remove_swap call expression shadow" * doctest::skip(true)) {
  const std::string source = R"(
[effects(heap_alloc), return<int>]
/vector/remove_swap([vector<i32> mut] values, [i32] index) {
  return(index)
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32, 2i32)}
  return(remove_swap(values, 5i32))
}
)";
  const std::string srcPath = writeTemp("vm_user_vector_remove_swap_call_expr_shadow.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 5);
}

TEST_CASE("runs vm with user vector clear method shadow") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<void>]
/vector/clear([vector<i32> mut] values) {
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32, 2i32)}
  values.clear()
  return(count(values))
}
)";
  const std::string srcPath = writeTemp("vm_user_vector_clear_method_shadow.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 2);
}

TEST_CASE("runs vm with user vector remove_at call shadow" * doctest::skip(true)) {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<void>]
/vector/remove_at([vector<i32> mut] values, [i32] index) {
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32, 2i32)}
  remove_at(values, 1i32)
  return(count(values))
}
)";
  const std::string srcPath = writeTemp("vm_user_vector_remove_at_call_shadow.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 2);
}

TEST_CASE("runs vm with user vector remove_at method shadow") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<void>]
/vector/remove_at([vector<i32> mut] values, [i32] index) {
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32, 2i32)}
  values.remove_at(1i32)
  return(count(values))
}
)";
  const std::string srcPath = writeTemp("vm_user_vector_remove_at_method_shadow.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 2);
}

TEST_CASE("runs vm with user vector remove_swap call shadow" * doctest::skip(true)) {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<void>]
/vector/remove_swap([vector<i32> mut] values, [i32] index) {
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32, 2i32)}
  remove_swap(values, 1i32)
  return(count(values))
}
)";
  const std::string srcPath = writeTemp("vm_user_vector_remove_swap_call_shadow.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 2);
}

TEST_CASE("runs vm with user vector remove_swap method shadow") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<void>]
/vector/remove_swap([vector<i32> mut] values, [i32] index) {
}

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32, 2i32)}
  values.remove_swap(1i32)
  return(count(values))
}
)";
  const std::string srcPath = writeTemp("vm_user_vector_remove_swap_method_shadow.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 2);
}

TEST_CASE("rejects vm vector reserve growth during lowering" * doctest::skip(true)) {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32, 2i32)}
  reserve(values, 3i32)
  push(values, 9i32)
  return(plus(count(values), capacity(values)))
}
)";
  const std::string srcPath = writeTemp("vm_vector_reserve_grows.prime", source);
  const std::string errPath =
      (std::filesystem::temp_directory_path() / "primec_vm_vector_reserve_grows_err.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main 2> " + errPath;
  CHECK(runCommand(runCmd) == 2);
  CHECK(readFile(errPath).find("count requires array, vector, map, or string target") != std::string::npos);
}

TEST_CASE("preserves vm vector values across reserve growth") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(4i32, 8i32)}
  reserve(values, 4i32)
  push(values, 16i32)
  return(plus(plus(values[0i32], values[1i32]), values[2i32]))
}
)";
  const std::string srcPath = writeTemp("vm_vector_reserve_growth_preserves_values.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 28);
}

TEST_CASE("rejects vm vector push growth during lowering" * doctest::skip(true)) {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32)}
  push(values, 2i32)
  return(plus(count(values), capacity(values)))
}
)";
  const std::string srcPath = writeTemp("vm_vector_push_grows.prime", source);
  const std::string errPath =
      (std::filesystem::temp_directory_path() / "primec_vm_vector_push_grows_err.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main 2> " + errPath;
  CHECK(runCommand(runCmd) == 2);
  CHECK(readFile(errPath).find("count requires array, vector, map, or string target") != std::string::npos);
}

TEST_CASE("preserves vm vector values across push growth") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(5i32)}
  push(values, 7i32)
  return(plus(values[0i32], values[1i32]))
}
)";
  const std::string srcPath = writeTemp("vm_vector_push_growth_preserves_values.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 12);
}

TEST_CASE("rejects vm vector literal at local dynamic limit during lowering" * doctest::skip(true)) {
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
                             buildVectorLiteralArgs(256) +
                             ")}\n"
                             "  return(convert<i32>(and(equal(count(values), 256i32), equal(capacity(values), "
                             "256i32))))\n"
                             "}\n";
  const std::string srcPath = writeTemp("vm_vector_literal_local_limit.prime", source);
  const std::string errPath =
      (std::filesystem::temp_directory_path() / "primec_vm_vector_literal_local_limit_err.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main 2> " + errPath;
  CHECK(runCommand(runCmd) == 2);
  CHECK(readFile(errPath).find("count requires array, vector, map, or string target") != std::string::npos);
}

TEST_CASE("rejects vm vector literal above local dynamic limit") {
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
  const std::string srcPath = writeTemp("vm_vector_literal_local_limit_overflow.prime", source);
  const std::string errPath = (std::filesystem::temp_directory_path() / "primec_vm_vector_literal_limit_err.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main 2> " + errPath;
  CHECK(runCommand(runCmd) == 2);
  CHECK(readFile(errPath).find("vector literal exceeds local capacity limit (256)") != std::string::npos);
}

TEST_CASE("rejects vm vector reserve beyond local dynamic limit") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32)}
  reserve(values, 257i32)
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("vm_vector_reserve_local_limit.prime", source);
  const std::string errPath = (std::filesystem::temp_directory_path() / "primec_vm_vector_reserve_limit_err.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main 2> " + errPath;
  CHECK(runCommand(runCmd) == 2);
  CHECK(readFile(errPath).find("vector reserve exceeds local capacity limit (256)") != std::string::npos);
}

TEST_CASE("rejects vm vector reserve negative literal at lowering") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32)}
  reserve(values, -1i32)
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("vm_vector_reserve_negative_literal.prime", source);
  const std::string errPath =
      (std::filesystem::temp_directory_path() / "primec_vm_vector_reserve_negative_literal_err.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main 2> " + errPath;
  CHECK(runCommand(runCmd) == 2);
  CHECK(readFile(errPath).find("vector reserve expects non-negative capacity") != std::string::npos);
}

TEST_CASE("rejects vm vector reserve folded expression beyond local dynamic limit") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32)}
  reserve(values, plus(200i32, 57i32))
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("vm_vector_reserve_folded_limit.prime", source);
  const std::string errPath =
      (std::filesystem::temp_directory_path() / "primec_vm_vector_reserve_folded_limit_err.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main 2> " + errPath;
  CHECK(runCommand(runCmd) == 2);
  CHECK(readFile(errPath).find("vector reserve exceeds local capacity limit (256)") != std::string::npos);
}

TEST_CASE("rejects vm vector reserve folded negative expression at lowering") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32)}
  reserve(values, minus(1i32, 2i32))
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("vm_vector_reserve_folded_negative.prime", source);
  const std::string errPath =
      (std::filesystem::temp_directory_path() / "primec_vm_vector_reserve_folded_negative_err.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main 2> " + errPath;
  CHECK(runCommand(runCmd) == 2);
  CHECK(readFile(errPath).find("vector reserve expects non-negative capacity") != std::string::npos);
}

TEST_CASE("rejects vm vector reserve folded signed overflow at lowering") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32)}
  reserve(values, plus(9223372036854775807i64, 1i64))
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("vm_vector_reserve_folded_signed_overflow.prime", source);
  const std::string errPath =
      (std::filesystem::temp_directory_path() / "primec_vm_vector_reserve_folded_signed_overflow_err.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main 2> " + errPath;
  CHECK(runCommand(runCmd) == 2);
  CHECK(readFile(errPath).find("vector reserve literal expression overflow") != std::string::npos);
}

TEST_CASE("rejects vm vector reserve folded negate negative at lowering") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32)}
  reserve(values, negate(1i32))
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("vm_vector_reserve_folded_negate_negative.prime", source);
  const std::string errPath =
      (std::filesystem::temp_directory_path() / "primec_vm_vector_reserve_folded_negate_negative_err.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main 2> " + errPath;
  CHECK(runCommand(runCmd) == 2);
  CHECK(readFile(errPath).find("vector reserve expects non-negative capacity") != std::string::npos);
}

TEST_CASE("rejects vm vector reserve folded negate overflow at lowering") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32)}
  reserve(values, negate(-9223372036854775808i64))
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("vm_vector_reserve_folded_negate_overflow.prime", source);
  const std::string errPath =
      (std::filesystem::temp_directory_path() / "primec_vm_vector_reserve_folded_negate_overflow_err.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main 2> " + errPath;
  CHECK(runCommand(runCmd) == 2);
  CHECK(readFile(errPath).find("vector reserve literal expression overflow") != std::string::npos);
}

TEST_CASE("rejects vm vector reserve folded unsigned expression beyond local dynamic limit") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32)}
  reserve(values, plus(200u64, 57u64))
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("vm_vector_reserve_folded_unsigned_limit.prime", source);
  const std::string errPath =
      (std::filesystem::temp_directory_path() / "primec_vm_vector_reserve_folded_unsigned_limit_err.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main 2> " + errPath;
  CHECK(runCommand(runCmd) == 2);
  CHECK(readFile(errPath).find("vector reserve exceeds local capacity limit (256)") != std::string::npos);
}

TEST_CASE("rejects vm vector reserve folded unsigned wraparound at lowering") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32)}
  reserve(values, minus(1u64, 2u64))
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("vm_vector_reserve_folded_unsigned_wrap.prime", source);
  const std::string errPath =
      (std::filesystem::temp_directory_path() / "primec_vm_vector_reserve_folded_unsigned_wrap_err.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main 2> " + errPath;
  CHECK(runCommand(runCmd) == 2);
  CHECK(readFile(errPath).find("vector reserve literal expression overflow") != std::string::npos);
}

TEST_CASE("rejects vm vector reserve folded unsigned add overflow at lowering") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>(1i32)}
  reserve(values, plus(18446744073709551615u64, 1u64))
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("vm_vector_reserve_folded_unsigned_add_overflow.prime", source);
  const std::string errPath =
      (std::filesystem::temp_directory_path() / "primec_vm_vector_reserve_folded_unsigned_add_overflow_err.txt")
          .string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main 2> " + errPath;
  CHECK(runCommand(runCmd) == 2);
  CHECK(readFile(errPath).find("vector reserve literal expression overflow") != std::string::npos);
}

TEST_CASE("rejects vm vector reserve dynamic value beyond local dynamic limit") {
  const std::string source = R"(
import /std/collections/*

[effects(heap_alloc), return<int>]
main([array<string>] args) {
  [vector<i32> mut] values{vector<i32>(1i32)}
  reserve(values, plus(257i32, count(args)))
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("vm_vector_reserve_dynamic_limit.prime", source);
  const std::string errPath = (std::filesystem::temp_directory_path() / "primec_vm_vector_reserve_dynamic_limit_err.txt")
                                  .string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main 2> " + errPath;
  CHECK(runCommand(runCmd) == 3);
  CHECK(readFile(errPath) == "array index out of bounds\n");
}

TEST_SUITE_END();
