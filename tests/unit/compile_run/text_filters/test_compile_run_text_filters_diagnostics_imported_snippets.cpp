#include "test_compile_run_text_filters_helpers.h"

TEST_SUITE_BEGIN("primestruct.compile.run.text_filters");

// The snippet under a diagnostic is a line of the file the diagnostic names. A span in an imported file is a line of
// that file, and it used to be cut from the expanded text the parser read, at the same line number, so the snippet was
// a line of whatever file happened to hold that line number in the expanded text.

namespace {

const char *const LibSource = R"([return<int>]
helper([i32] a) {
  [i32] b{a + 2i32 * nope}
  return(b)
}
)";

struct Run {
  std::string libPath;
  std::string mainPath;
  std::string text;
};

// The imported file is `lib.prime`; `mainBody` is the primary file's own code, with the import before or after it.
Run runWithImport(const std::string &name,
                  const std::string &mainBody,
                  bool importFirst,
                  const std::string &libSource = LibSource) {
  Run run;
  run.libPath = writeTemp(name + "_lib.prime", libSource);
  const std::string importLine = "import<\"" + run.libPath + "\">\n";
  run.mainPath =
      writeTemp(name + "_main.prime", importFirst ? importLine + mainBody : mainBody + importLine);
  const std::string errPath = (testScratchPath("") / (name + "_err.txt")).string();
  CHECK(runCommand("./primec --emit=vm " + quoteShellArg(run.mainPath) + " --entry /main 2> " +
                   quoteShellArg(errPath)) == 2);
  run.text = readFile(errPath);
  return run;
}

std::string snippetOf(int line, const std::string &sourceLine, int column) {
  const std::string number = std::to_string(line);
  return " " + number + " | " + sourceLine + "\n " + std::string(number.size(), ' ') + " | " +
         std::string(static_cast<size_t>(column - 1), ' ') + "^\n";
}

const char *const MainCallingHelper = R"([return<int>]
main() {
  [i32] x{1i32 + 2i32}
  [i32] y{x}
  [i32] z{y}
  return(helper(z))
}
)";

} // namespace

TEST_CASE("an error in an imported file after the primary code shows that file's line") {
  const Run run = runWithImport("imported_snippet_after", MainCallingHelper, false);
  INFO(run.text);
  CHECK(run.text.find(run.libPath +
                      ":3:22: error: Semantic error: unknown identifier: nope [PSC1005]\n") !=
        std::string::npos);
  CHECK(run.text.find(snippetOf(3, "  [i32] b{a + 2i32 * nope}", 22)) != std::string::npos);
  // Line 3 of the primary file, which the snippet used to show.
  CHECK(run.text.find("[i32] x{1i32 + 2i32}") == std::string::npos);
}

TEST_CASE("a related span in an imported file shows that file's line too") {
  const Run run = runWithImport("imported_snippet_related", MainCallingHelper, false);
  INFO(run.text);
  CHECK(run.text.find(run.libPath + ":2:1: note: definition: /helper\n") != std::string::npos);
  CHECK(run.text.find(snippetOf(2, "helper([i32] a) {", 1)) != std::string::npos);
  CHECK(run.text.find("main() {") == std::string::npos);
}

TEST_CASE("an error in an imported file placed before the primary code shows that file's line") {
  const Run run = runWithImport("imported_snippet_first", MainCallingHelper, true);
  INFO(run.text);
  CHECK(run.text.find(run.libPath +
                      ":3:22: error: Semantic error: unknown identifier: nope [PSC1005]\n") !=
        std::string::npos);
  CHECK(run.text.find(snippetOf(3, "  [i32] b{a + 2i32 * nope}", 22)) != std::string::npos);
}

TEST_CASE("an error in the primary file after an import shows the primary file's line") {
  const char *const brokenMain = R"([return<int>]
main() {
  [i32] q{missing_name}
  return(helper(1i32))
}
)";
  // The import is the first line, so the primary file's lines are shifted by the imported text in the expanded source.
  const Run run = runWithImport("imported_snippet_primary",
                                brokenMain,
                                true,
                                "[return<int>]\nhelper([i32] a) {\n  return(a)\n}\n");
  INFO(run.text);
  CHECK(run.text.find(run.mainPath + ":4:") != std::string::npos);
  CHECK(run.text.find(snippetOf(4, "  [i32] q{missing_name}", 11)) != std::string::npos);
}

TEST_SUITE_END();
