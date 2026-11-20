#include "test_compile_run_text_filters_helpers.h"

TEST_SUITE_BEGIN("primestruct.compile.run.text_filters");

// A diagnostic points at the source the user wrote (TODO-6080): the text filters rewrite operators into calls
// (`a + b` is `plus(a, b)`), but the column and the snippet of an error inside or after such a span are those of the
// user's text. Every program here has an unknown identifier `nope` that the filtered text spells at another column.

namespace {

const char *const OperatorSource = R"([return<int>]
main() {
  [i32] a{1i32}
  [i32] b{a + 2i32 * nope}
  return(b)
}
)";

const char *const CallSource = R"([return<int>]
pick_one([i32] x, [i32] y) {
  return(x)
}

[return<int>]
main() {
  [i32] a{1i32}
  [i32] d{pick_one(a * 2i32, nope)}
  return(d)
}
)";

const char *const PrefixSource = R"([return<int>]
main() {
  [i32] a{1i32}
  [i32] c{-a + -nope}
  return(c)
}
)";

const char *const PlainSource = R"([return<int>]
main() {
  [i32] a{1i32}
  [i32] b{plus(a, nope)}
  return(b)
}
)";

struct Run {
  std::string path;
  std::string text;
};

Run runPrimec(const std::string &name, const std::string &source, const std::string &extraFlags) {
  Run run;
  run.path = writeTemp(name + ".prime", source);
  const std::string errPath = (testScratchPath("") / (name + "_err.txt")).string();
  const std::string cmd = "./primec --emit=vm " + quoteShellArg(run.path) + " --entry /main " +
                          extraFlags + " 2> " + quoteShellArg(errPath);
  CHECK(runCommand(cmd) == 2);
  run.text = readFile(errPath);
  return run;
}

// The snippet lines of a diagnostic at `line` and `column`: the source line and the caret under `column`.
std::string snippetOf(int line, const std::string &sourceLine, int column) {
  const std::string number = std::to_string(line);
  return " " + number + " | " + sourceLine + "\n " + std::string(number.size(), ' ') + " | " +
         std::string(static_cast<size_t>(column - 1), ' ') + "^\n";
}

} // namespace

TEST_CASE("an error inside an operator expression is at the column of the user's text") {
  const Run run = runPrimec("source_columns_operator", OperatorSource, "");
  INFO(run.text);
  CHECK(run.text.find(run.path +
                      ":4:22: error: Semantic error: unknown identifier: nope [PSC1005]\n") !=
        std::string::npos);
  CHECK(run.text.find(snippetOf(4, "  [i32] b{a + 2i32 * nope}", 22)) != std::string::npos);
  CHECK(run.text.find("plus(") == std::string::npos);
  CHECK(run.text.find("multiply(") == std::string::npos);
}

TEST_CASE("an error after a rewritten span on the same line is at the column of the user's text") {
  const Run run = runPrimec("source_columns_call_argument", CallSource, "");
  INFO(run.text);
  CHECK(run.text.find(run.path +
                      ":9:30: error: Semantic error: unknown identifier: nope [PSC1005]\n") !=
        std::string::npos);
  CHECK(run.text.find(snippetOf(9, "  [i32] d{pick_one(a * 2i32, nope)}", 30)) !=
        std::string::npos);
  CHECK(run.text.find("multiply(") == std::string::npos);
}

TEST_CASE("an error inside a rewritten prefix operator is at the column of the user's text") {
  const Run run = runPrimec("source_columns_prefix", PrefixSource, "");
  INFO(run.text);
  CHECK(run.text.find(run.path +
                      ":4:17: error: Semantic error: unknown identifier: nope [PSC1005]\n") !=
        std::string::npos);
  CHECK(run.text.find(snippetOf(4, "  [i32] c{-a + -nope}", 17)) != std::string::npos);
  CHECK(run.text.find("negate(") == std::string::npos);
}

TEST_CASE("structured diagnostics carry the line and column of the user's text") {
  const Run run = runPrimec("source_columns_json", OperatorSource, "--emit-diagnostics");
  INFO(run.text);
  CHECK(run.text.find("\"message\":\"unknown identifier: nope\"") != std::string::npos);
  CHECK(run.text.find("\"line\":4,\"column\":22,") != std::string::npos);
}

TEST_CASE("an error in a program without rewrites keeps its column and snippet") {
  const Run run = runPrimec("source_columns_plain", PlainSource, "");
  INFO(run.text);
  CHECK(run.text.find(run.path +
                      ":4:19: error: Semantic error: unknown identifier: nope [PSC1005]\n") !=
        std::string::npos);
  CHECK(run.text.find(snippetOf(4, "  [i32] b{plus(a, nope)}", 19)) != std::string::npos);
}

TEST_SUITE_END();
