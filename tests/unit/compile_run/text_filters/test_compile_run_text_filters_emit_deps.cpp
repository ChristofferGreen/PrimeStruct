#include "test_compile_run_text_filters_helpers.h"

#include "primec/testing/DumpNormalization.h"

using primec::testing::stripDumpTimings;

TEST_SUITE_BEGIN("primestruct.compile.run.text_filters");

namespace {

struct EmitDepsRun {
  int exitCode = 0;
  std::string out;
  std::string err;
  std::string deps;
  std::string srcPath;
};

// Runs `primevm <source> --emit-deps <file> <args>` and returns what it printed and the file it wrote.
EmitDepsRun
runEmitDeps(const std::string &name, const std::string &source, const std::string &args = "") {
  const std::string srcPath = writeTemp(name + ".prime", source);
  const std::string depsPath = (testScratchPath("") / (name + "_deps.txt")).string();
  const std::string outPath = (testScratchPath("") / (name + "_out.txt")).string();
  const std::string errPath = (testScratchPath("") / (name + "_err.txt")).string();
  std::filesystem::remove(depsPath);
  const std::string cmd = "./primevm " + quoteShellArg(srcPath) + " --emit-deps " +
                          quoteShellArg(depsPath) + " " + args + " > " + quoteShellArg(outPath) +
                          " 2> " + quoteShellArg(errPath);
  EmitDepsRun run;
  run.srcPath = srcPath;
  run.exitCode = runCommand(cmd);
  run.out = readFile(outPath);
  run.err = readFile(errPath);
  if (std::filesystem::exists(depsPath)) {
    run.deps = readFile(depsPath);
  }
  return run;
}

std::string absolutePath(const std::string &path) {
  return std::filesystem::absolute(path).lexically_normal().string();
}

const std::string MainUsing = "\n[return<i32>]\nmain() {\n  return(helper_a() + helper_b())\n}\n";

} // namespace

TEST_CASE("emit-deps lists the primary file and its project imports sorted") {
  const std::string pathA =
      writeTemp("emit_deps_zeta.prime", "[return<i32>]\nhelper_a() {\n  return(1i32)\n}\n");
  const std::string pathB =
      writeTemp("emit_deps_alpha.prime", "[return<i32>]\nhelper_b() {\n  return(2i32)\n}\n");
  const std::string source = "import<\"" + pathA + "\">\nimport<\"" + pathB + "\">\n" + MainUsing;
  const EmitDepsRun first = runEmitDeps("emit_deps_two_imports", source);
  CHECK(first.exitCode == 3);
  const std::string primary = absolutePath(first.srcPath);
  std::vector<std::string> expected = {primary, absolutePath(pathA), absolutePath(pathB)};
  std::sort(expected.begin(), expected.end());
  std::string expectedText;
  for (const std::string &path : expected) {
    expectedText += path + "\n";
  }
  CHECK(first.deps == expectedText);
  const EmitDepsRun second = runEmitDeps("emit_deps_two_imports", source);
  CHECK(second.deps == first.deps);
}

TEST_CASE("emit-deps lists only the primary file when nothing is imported") {
  const EmitDepsRun run =
      runEmitDeps("emit_deps_alone", "[return<i32>]\nmain() {\n  return(4i32)\n}\n");
  CHECK(run.exitCode == 4);
  CHECK(run.deps == absolutePath(run.srcPath) + "\n");
}

TEST_CASE("emit-deps lists the stdlib files a lazy import included") {
  const EmitDepsRun run =
      runEmitDeps("emit_deps_stdlib",
                  "import /std/collections/vector\n\n[return<i32> effects(heap_alloc)]\nmain() {\n"
                  "  [vector<i32> mut] v{vector<i32>{}}\n  v.push(4i32)\n  return(v.count())\n}\n");
  CHECK(run.exitCode == 1);
  CHECK(run.deps.find("/stdlib/std/collections/vector.prime\n") != std::string::npos);
  CHECK(run.deps.find("emit_deps_stdlib.prime\n") != std::string::npos);
  CHECK(run.deps.find("/std/net") == std::string::npos);
}

namespace {

// `import</std/math>` splices the math files by the import resolver; the language import then asks the stdlib pass
// for the same files (TODO-6110).
const std::string MathBothImports =
    "import</std/math>\nimport /std/math/*\n\n[return<i32>]\nmain() {\n"
    "  return(convert<i32>(7.5f32 * linearToSrgbChannel(1.0f32) + "
    "srgbToLinearChannel(0.0f32)))\n}\n";

size_t countOccurrences(const std::string &text, const std::string &needle) {
  size_t count = 0;
  for (size_t at = text.find(needle); at != std::string::npos;
       at = text.find(needle, at + needle.size())) {
    ++count;
  }
  return count;
}

} // namespace

TEST_CASE("a logical import of a stdlib module and its wildcard import splice each file once") {
  const EmitDepsRun run = runEmitDeps("emit_deps_math_both_imports", MathBothImports);
  CHECK(run.err.find("duplicate definition") == std::string::npos);
  CHECK(run.err.empty());
  CHECK(run.exitCode == 7);
  CHECK(countOccurrences(run.deps, "/stdlib/std/math/color.prime\n") == 1);
  CHECK(countOccurrences(run.deps, "/stdlib/std/math/vector.prime\n") == 1);
}

TEST_CASE("the wildcard import of a stdlib module alone still splices it") {
  const EmitDepsRun run = runEmitDeps("emit_deps_math_wildcard_only",
                                      "import /std/math/*\n\n[return<i32>]\nmain() {\n"
                                      "  return(convert<i32>(7.5f32 * linearToSrgbChannel(1.0f32) "
                                      "+ srgbToLinearChannel(0.0f32)))\n}\n");
  CHECK(run.exitCode == 7);
  CHECK(countOccurrences(run.deps, "/stdlib/std/math/color.prime\n") == 1);
}

TEST_CASE("emit-deps does not change the program output") {
  const std::string source = "[return<i32>]\nmain() {\n  return(7i32)\n}\n";
  const std::string srcPath = writeTemp("emit_deps_same_out.prime", source);
  const std::string plainPath = (testScratchPath("") / "emit_deps_same_plain.txt").string();
  const std::string flagPath = (testScratchPath("") / "emit_deps_same_flag.txt").string();
  const std::string depsPath = (testScratchPath("") / "emit_deps_same_deps.txt").string();
  CHECK(runCommand("./primevm " + quoteShellArg(srcPath) + " --dump-stage ast > " +
                   quoteShellArg(plainPath)) == 0);
  CHECK(runCommand("./primevm " + quoteShellArg(srcPath) + " --dump-stage ast --emit-deps " +
                   quoteShellArg(depsPath) + " > " + quoteShellArg(flagPath)) == 0);
  CHECK(stripDumpTimings(readFile(plainPath)) == stripDumpTimings(readFile(flagPath)));
  CHECK(!readFile(depsPath).empty());
}

TEST_CASE("emit-deps reports a file it cannot write") {
  const std::string srcPath =
      writeTemp("emit_deps_bad_target2.prime", "[return<i32>]\nmain() {\n  return(1i32)\n}\n");
  const std::string errPath = (testScratchPath("") / "emit_deps_bad_target2_err.txt").string();
  const int code =
      runCommand("./primevm " + quoteShellArg(srcPath) +
                 " --emit-deps /nonexistent_dir_primestruct/x.txt 2> " + quoteShellArg(errPath));
  CHECK(code == 2);
  CHECK(readFile(errPath).find("failed to write --emit-deps file") != std::string::npos);
}

TEST_SUITE_END();
