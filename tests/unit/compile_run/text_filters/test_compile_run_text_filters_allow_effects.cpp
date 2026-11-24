#include "test_compile_run_text_filters_helpers.h"

TEST_SUITE_BEGIN("primestruct.compile.run.text_filters");

namespace {

struct AllowEffectsRun {
  int exitCode = 0;
  std::string out;
  std::string err;
};

AllowEffectsRun
runAllowEffects(const std::string &name, const std::string &source, const std::string &args) {
  const std::string srcPath = writeTemp(name + ".prime", source);
  const std::string outPath = (testScratchPath("") / (name + "_out.txt")).string();
  const std::string errPath = (testScratchPath("") / (name + "_err.txt")).string();
  const std::string cmd = "./primevm " + quoteShellArg(srcPath) + " " + args + " > " +
                          quoteShellArg(outPath) + " 2> " + quoteShellArg(errPath);
  AllowEffectsRun run;
  run.exitCode = runCommand(cmd);
  run.out = readFile(outPath);
  run.err = readFile(errPath);
  return run;
}

const std::string NetIoSource = R"(
[effects(net_io), return<i32>]
reach() {
  return(1i32)
}

[effects(heap_alloc), return<i32>]
main() {
  return(7i32)
}
)";

const std::string WithinListSource = R"(
[effects(file_read), return<i32>]
reader() {
  return(3i32)
}

[effects(heap_alloc, file_read), return<i32>]
main() {
  return(reader() + 4i32)
}
)";

} // namespace

TEST_CASE("allow-effects rejects a declared effect outside the list") {
  const AllowEffectsRun run =
      runAllowEffects("allow_effects_net_io", NetIoSource, "--allow-effects heap_alloc,file_read");
  CHECK(run.exitCode == 2);
  CHECK(run.err.find("definition /reach declares effect net_io, outside the allowed effects: "
                     "heap_alloc,file_read") != std::string::npos);
}

TEST_CASE("allow-effects runs code within the list") {
  const AllowEffectsRun run = runAllowEffects(
      "allow_effects_within_list", WithinListSource, "--allow-effects=heap_alloc,file_read");
  CHECK(run.exitCode == 7);
  CHECK(run.err.empty());
}

TEST_CASE("allow-effects checks a definition without effects against its default set") {
  const std::string source = R"(
[return<i32>]
plain() {
  return(1i32)
}

[return<i32>]
main() {
  return(plain())
}
)";
  const AllowEffectsRun rejected =
      runAllowEffects("allow_effects_default_set", source, "--allow-effects heap_alloc");
  CHECK(rejected.exitCode == 2);
  CHECK(rejected.err.find("definition /plain receives default effect io_out, outside the allowed "
                          "effects: heap_alloc") != std::string::npos);
  const AllowEffectsRun accepted =
      runAllowEffects("allow_effects_default_set_none",
                      source,
                      "--allow-effects heap_alloc --default-effects none");
  CHECK(accepted.exitCode == 1);
  CHECK(accepted.err.empty());
}

TEST_CASE("allow-effects does not check stdlib definitions") {
  const std::string source = R"(
import /std/math/*

[effects(heap_alloc), return<i32>]
main() {
  return(abs(-5i32))
}
)";
  const AllowEffectsRun run =
      runAllowEffects("allow_effects_stdlib", source, "--allow-effects heap_alloc");
  CHECK(run.exitCode == 5);
  CHECK(run.err.empty());
}

TEST_CASE("without allow-effects the declared effects are unchanged") {
  const AllowEffectsRun run = runAllowEffects("allow_effects_absent", NetIoSource, "");
  CHECK(run.exitCode == 7);
  CHECK(run.err.empty());
}

TEST_CASE("allow-effects reports every violation under collect-diagnostics") {
  const std::string source = R"(
[effects(net_io), return<i32>]
first() {
  return(1i32)
}

[effects(file_write), return<i32>]
second() {
  return(2i32)
}

[effects(heap_alloc), return<i32>]
main() {
  return(0i32)
}
)";
  const AllowEffectsRun run =
      runAllowEffects("allow_effects_collect",
                      source,
                      "--allow-effects heap_alloc --emit-diagnostics --collect-diagnostics");
  CHECK(run.exitCode == 2);
  const size_t first = run.err.find("definition /first declares effect net_io");
  const size_t second = run.err.find("definition /second declares effect file_write");
  CHECK(first != std::string::npos);
  CHECK(second != std::string::npos);
  CHECK(first < second);
}

TEST_CASE("allow-effects checks top-level executions") {
  const std::string source = R"(
[effects(heap_alloc), return<void>]
noop() {
  return()
}

[effects(net_io)]
noop()

[effects(heap_alloc), return<i32>]
main() {
  return(0i32)
}
)";
  const AllowEffectsRun run =
      runAllowEffects("allow_effects_execution", source, "--allow-effects heap_alloc");
  CHECK(run.exitCode == 2);
  CHECK(run.err.find("execution /noop declares effect net_io") != std::string::npos);
}

TEST_CASE("allow-effects rejects an unknown effect name in the list") {
  const AllowEffectsRun run = runAllowEffects(
      "allow_effects_unknown", WithinListSource, "--allow-effects heap_alloc,bogus");
  CHECK(run.exitCode != 0);
  CHECK(run.err.find("unknown effect in --allow-effects: bogus") != std::string::npos);
}

TEST_CASE("allow-effects with an empty list allows no effects") {
  const AllowEffectsRun rejected =
      runAllowEffects("allow_effects_empty", WithinListSource, "--allow-effects ''");
  CHECK(rejected.exitCode == 2);
  CHECK(rejected.err.find(
            "definition /reader declares effect file_read, outside the allowed effects: none") !=
        std::string::npos);

  const std::string pure = R"(
[return<i32>]
main() {
  return(9i32)
}
)";
  const AllowEffectsRun accepted = runAllowEffects(
      "allow_effects_empty_pure", pure, "--allow-effects '' --default-effects none");
  CHECK(accepted.exitCode == 9);
  CHECK(accepted.err.empty());
}

TEST_SUITE_END();
