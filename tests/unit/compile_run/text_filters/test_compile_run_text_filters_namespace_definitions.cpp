#include "test_compile_run_text_filters_helpers.h"

TEST_SUITE_BEGIN("primestruct.compile.run.text_filters");

namespace {

struct NamespaceRun {
  int exitCode = 0;
  std::string out;
  std::string err;
};

NamespaceRun runNamespaceSource(const std::string &name, const std::string &source, const std::string &args) {
  const std::string srcPath = writeTemp(name + ".prime", source);
  const std::string outPath = (testScratchPath("") / (name + "_out.txt")).string();
  const std::string errPath = (testScratchPath("") / (name + "_err.txt")).string();
  const std::string cmd = "./primec " + quoteShellArg(srcPath) + " " + args + " > " + quoteShellArg(outPath) +
                          " 2> " + quoteShellArg(errPath);
  NamespaceRun run;
  run.exitCode = runCommand(cmd);
  run.out = readFile(outPath);
  run.err = readFile(errPath);
  return run;
}

const std::string NamespaceMainSource = R"(
[return<int>]
main() {
  return(1i32)
}
)";

void checkNamespaceDiagnostic(const std::string &name, const std::string &source, const std::string &expected) {
  const NamespaceRun run = runNamespaceSource(name, source + NamespaceMainSource, "--emit=vm --entry /main");
  CHECK(run.exitCode == 2);
  CHECK(run.err.find(expected) != std::string::npos);
}

} // namespace

TEST_CASE("namespace definition holds functions, structs and nested namespaces on the VM") {
  const std::string source = R"(
import /demo/*

[namespace]
demo {
  [public, struct] Point() {
    [public i32] x{3i32}
  }
  [return<i32>] one() { return(7i32) }
  [public, return<i32>] two() { return(plus(one(), 1i32)) }
  [namespace]
  inner {
    [return<i32>] three() { return(3i32) }
  }
}

[return<int>]
main() {
  [Point] point{Point{}}
  return(plus(plus(two(), /demo/inner/three()), point.x))
}
)";
  const NamespaceRun run = runNamespaceSource("namespace_definition_vm", source, "--emit=vm --entry /main");
  CHECK(run.exitCode == 14);
}

TEST_CASE("namespace definition members do not get an implicit this next to a struct") {
  const std::string source = R"(
[namespace]
P {
  [return<i32>] twice([i32] value) { return(plus(value, value)) }
}

[struct]
P() {
  [i32] x{1i32}
}

[return<int>]
main() {
  return(/P/twice(4i32))
}
)";
  const NamespaceRun run = runNamespaceSource("namespace_definition_shared_path", source, "--emit=vm --entry /main");
  CHECK(run.exitCode == 8);
}

TEST_CASE("two namespace definition blocks fold into one in the ast dump") {
  const std::string source = R"(
[namespace]
demo {
  [return<i32>] one() { return(1i32) }
}

[namespace]
demo {
  [return<i32>] two() { return(2i32) }
}

[return<int>]
main() {
  return(plus(/demo/one(), /demo/two()))
}
)";
  const NamespaceRun run = runNamespaceSource("namespace_definition_fold", source, "--dump-stage ast");
  CHECK(run.exitCode == 0);
  CHECK(run.out ==
        "ast {\n"
        "  [return<i32>] /demo/one() {\n"
        "    return 1\n"
        "  }\n"
        "  [namespace] /demo() {\n"
        "  }\n"
        "  [return<i32>] /demo/two() {\n"
        "    return 2\n"
        "  }\n"
        "  [return<int>] /main() {\n"
        "    return plus(/demo/one(), /demo/two())\n"
        "  }\n"
        "}\n");
}

TEST_CASE("namespace definition lowers to no function") {
  const std::string source = R"(
[namespace]
demo {
  [return<i32>] one() { return(1i32) }
}

[return<int>]
main() {
  return(/demo/one())
}
)";
  const NamespaceRun run = runNamespaceSource("namespace_definition_ir", source, "--dump-stage ir-lowered");
  CHECK(run.exitCode == 0);
  CHECK(run.out.find("functions: 1\n") != std::string::npos);
  CHECK(run.out.find("function 0 /main ") != std::string::npos);
  CHECK(run.out.find(" /demo ") == std::string::npos);
}

TEST_CASE("namespace definition rejects parameters") {
  checkNamespaceDiagnostic("namespace_definition_params", "[namespace]\ndemo(x) {\n}\n",
                           "namespace definition cannot have parameters: /demo");
}

TEST_CASE("namespace definition rejects a template list") {
  checkNamespaceDiagnostic("namespace_definition_template", "[namespace]\ndemo<T> {\n}\n",
                           "namespace definition cannot have a template list: /demo");
}

TEST_CASE("namespace definition rejects a return transform") {
  checkNamespaceDiagnostic("namespace_definition_return", "[namespace, return<int>]\ndemo {\n}\n",
                           "namespace definition cannot have a return transform: /demo");
}

TEST_CASE("namespace definition rejects a statement in its body") {
  checkNamespaceDiagnostic("namespace_definition_statement", "[namespace]\ndemo {\n  [i32] x{1i32}\n}\n",
                           "namespace definition body can only hold definitions: /demo");
}

TEST_CASE("namespace definition cannot be called") {
  const NamespaceRun run = runNamespaceSource("namespace_definition_call", R"(
[namespace]
demo {
}

[return<int>]
main() {
  demo()
  return(1i32)
}
)",
                                              "--emit=vm --entry /main");
  CHECK(run.exitCode == 2);
  CHECK(run.err.find("namespace is not callable: /demo") != std::string::npos);
}

TEST_CASE("namespace definition sharing a struct path takes no other transform") {
  checkNamespaceDiagnostic("namespace_definition_shared_transform",
                           "[namespace public]\nP {\n}\n\n[struct]\nP() {\n  [i32] x{1i32}\n}\n",
                           "its transform list must be 'namespace' alone: /P");
}

TEST_SUITE_END();
