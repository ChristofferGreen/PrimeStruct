#include "test_compile_run_text_filters_helpers.h"

TEST_SUITE_BEGIN("primestruct.compile.run.text_filters");

namespace {

// Feeds every program to `primec --emit=vm` and checks that it ends with a diagnostic or a success:
// exit code 0 or 2, never a signal or an uncaught C++ exception.
void checkOddProgramsEndCleanly(const char *name, const std::vector<std::string> &programs) {
  const std::string errPath = (testScratchPath("") / (std::string(name) + "_err.txt")).string();
  for (const std::string &program : programs) {
    INFO("program: " << program);
    const std::string srcPath = writeTemp(std::string(name) + ".prime", program);
    const std::string cmd =
        "./primec --emit=vm " + quoteShellArg(srcPath) + " --entry /main 2> " + quoteShellArg(errPath);
    const int code = runCommand(cmd);
    CHECK((code == 0 || code == 2));
    const std::string diagnostics = readFile(errPath);
    CHECK(diagnostics.find("terminating") == std::string::npos);
    CHECK(diagnostics.find("uncaught") == std::string::npos);
    CHECK(diagnostics.find("std::") == std::string::npos);
    if (code == 2) {
      CHECK(diagnostics.find("error") != std::string::npos);
    }
  }
}

std::string mainReturning(const std::string &literal) {
  return "[return<int>]\nmain() {\n  return(" + literal + ")\n}\n";
}

} // namespace

TEST_CASE("primec ends empty and truncated programs cleanly") {
  checkOddProgramsEndCleanly(
      "primec_robust_truncated",
      {"", "\n", " ", "import", "import ", "[", "]", "{", "}", "(", ")", "[]", "/", "/main", "[return<int>]",
       "[return<int>] main", "[return<int>] main(", "[return<int>] main()", "[return<int>] main() {",
       "[return<int>] main() { return(", "[return<int>] main() { return(0i32"});
}

TEST_CASE("primec ends empty and odd transform lists cleanly") {
  checkOddProgramsEndCleanly(
      "primec_robust_transforms",
      {"[] main() {}", "[]\nmain() {}", "[,] main() {}", "[return<int>,] main() { return(0i32) }",
       "[,return<int>] main() { return(0i32) }", "[return<>] main() { return(0i32) }",
       "[return<int] main() { return(0i32) }", "[return<int>>] main() { return(0i32) }",
       "[<int>] main() { return(0i32) }", "[return<int>]\n[return<int>]\nmain() { return(0i32) }",
       "[[return<int>]] main() { return(0i32) }", "[return<int>(] main() { return(0i32) }",
       "[effects()] main() {}", "[effects(] main() {}", "[align_bytes()] main() {}", "[align_bytes(]\nmain() {}"});
}

TEST_CASE("primec ends huge and odd numeric literals cleanly") {
  std::vector<std::string> programs;
  for (const char *literal :
       {"99999999999999999999999i32", "99999999999999999999999i64", "99999999999999999999999u64",
        "99999999999999999999999", "0x", "0b", "0xFFFFFFFFFFFFFFFFFFi64", "0b2", "0xg", "1e", "1e99999999999f32",
        "1.5e999f64", "1e-99999999999f64", "1u", "1i", "1i33", "1i0", "1f", "1f8", "1f128", "1__2i32", ".5f32",
        "1.f32", "1.5i32", "-9223372036854775809i64", "-2147483649i32", "2147483648i32", "4294967296u32",
        "18446744073709551616u64", "1i64i64", "00000000000000000000000000000000000i32", "1e+f32", "0x.8f32",
        "1.0e1i32", "1_000i32", "9223372036854775808i64"}) {
    programs.push_back(mainReturning(literal));
  }
  programs.push_back("[return<int>]\nmain() { [array<99999999999999999999999>] a{0i32}\n return(0i32) }\n");
  programs.push_back("[Foo<99999999999999999999999>]\nmain() { return(0i32) }\n");
  const std::size_t half = programs.size() / 2;
  checkOddProgramsEndCleanly("primec_robust_numbers_a", {programs.begin(), programs.begin() + half});
  checkOddProgramsEndCleanly("primec_robust_numbers_b", {programs.begin() + half, programs.end()});
}

TEST_CASE("primec ends unterminated strings and comments cleanly") {
  checkOddProgramsEndCleanly(
      "primec_robust_text",
      {mainReturning("\"abc"), mainReturning("\"abc\\"), mainReturning("\"\\"), mainReturning("\"\\u"),
       mainReturning("\"\\x"), mainReturning("\"\\xZZ\""), mainReturning("\"abc\"utf8"),
       mainReturning("\"abc\"raw_utf8"), mainReturning("\"abc\"ascii"), mainReturning("R\"("), mainReturning("'"),
       mainReturning("'a"), "/*", "/* unterminated\n[return<int>] main() { return(0i32) }", "// only a comment", "//",
       "/* nested /* inner */", mainReturning("0i32 /*"), "\"", "\"\\"});
}

TEST_CASE("primec ends odd import shapes cleanly") {
  checkOddProgramsEndCleanly(
      "primec_robust_imports",
      {"import /", "import /*", "import //", "import /x/", "import /x//", "import x", "import /std/*/x",
       "import /std/nope/*", "import /std/nope", "import /std/", "import /std//x", "import /std/math/",
       "import /std/math//*", "import /std/*/*", "import /std/../x", "import /std/./x", "import /std/math/*/",
       "import /std/math/**", "import /std/math/* /std", "import /std/math/* /std/*",
       "import /a/b/c/d/e/f/g/h/i/j/k/l/m/n/o/p/*", "import /std/collections/vector/", "import</>", "import</std>",
       "import<", "import<>", "import <", "import /std/math/*,", "import /std/math/*, /std/math/*"});
}

TEST_CASE("primec reports unresolvable stdlib imports with a position and the path") {
  const std::string errPath = (testScratchPath("") / "primec_stdlib_import_position_err.txt").string();
  for (const char *spelling : {"/std/nope/*", "/std/*/x", "/std//x", "/std/"}) {
    INFO("import: " << std::string(spelling));
    const std::string source = std::string("\nimport ") + spelling + "\n" + mainReturning("0i32");
    const std::string srcPath = writeTemp("primec_stdlib_import_position.prime", source);
    const std::string cmd =
        "./primec --emit=vm " + quoteShellArg(srcPath) + " --entry /main 2> " + quoteShellArg(errPath);
    CHECK(runCommand(cmd) == 2);
    const std::string diagnostics = readFile(errPath);
    CHECK(diagnostics.find("primec_stdlib_import_position.prime:2:") != std::string::npos);
    CHECK(diagnostics.find(spelling) != std::string::npos);
    CHECK(diagnostics.find("terminating") == std::string::npos);
  }
}

TEST_SUITE_END();
