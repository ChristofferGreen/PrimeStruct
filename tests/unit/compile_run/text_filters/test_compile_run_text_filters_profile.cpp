#include "test_compile_run_text_filters_helpers.h"

TEST_SUITE_BEGIN("primestruct.compile.run.text_filters");

namespace {

struct ProfileRun {
  int exitCode = 0;
  std::string out;
  std::string err;
};

ProfileRun runProfile(const std::string &name, const std::string &source, const std::string &args) {
  const std::string srcPath = writeTemp(name + ".prime", source);
  const std::string outPath = (testScratchPath("") / (name + "_out.txt")).string();
  const std::string errPath = (testScratchPath("") / (name + "_err.txt")).string();
  const std::string cmd = "./primevm " + quoteShellArg(srcPath) + " " + args + " > " + quoteShellArg(outPath) +
                          " 2> " + quoteShellArg(errPath);
  ProfileRun run;
  run.exitCode = runCommand(cmd);
  run.out = readFile(outPath);
  run.err = readFile(errPath);
  return run;
}

const std::string PureSource = R"(
[return<i32>]
square([i32] x) {
  return(x * x)
}

[return<i32>]
main() {
  return(square(2i32) + 1i32)
}
)";

const std::string PrintSource = R"(
[return<i32>]
main() {
  print_line("hello")
  return(2i32)
}
)";

const std::string NetEffectSource = R"(
[effects(net_io), return<i32>]
main() {
  return(7i32)
}
)";

const std::string NetImportSource = R"(
import /std/net/*

[return<i32>]
main() {
  return(3i32)
}
)";

} // namespace

TEST_CASE("profile build rejects printing without declared effects") {
  const ProfileRun run = runProfile("profile_build_print", PrintSource, "--profile build");
  CHECK(run.exitCode == 2);
  CHECK(run.out.empty());
  CHECK_FALSE(run.err.empty());
}

TEST_CASE("profile program and no flag print as before") {
  const ProfileRun none = runProfile("profile_none_print", PrintSource, "");
  const ProfileRun program = runProfile("profile_program_print", PrintSource, "--profile program");
  CHECK(none.exitCode == 2);
  CHECK(none.out == "hello\n");
  CHECK(program.exitCode == 2);
  CHECK(program.out == none.out);
  CHECK(program.err == none.err);
}

TEST_CASE("profile build rejects an effect outside the ceiling") {
  const ProfileRun run = runProfile("profile_build_effect", NetEffectSource, "--profile build");
  CHECK(run.exitCode == 2);
  CHECK(run.err.find("definition /main declares effect net_io, outside the allowed effects: "
                     "file_read,heap_alloc,build_run") != std::string::npos);
}

TEST_CASE("profile build rejects an import outside the allowlist") {
  const ProfileRun run = runProfile("profile_build_import", NetImportSource, "--profile build");
  CHECK(run.exitCode == 2);
  CHECK(run.err.find("import /std/net/* is outside the allowed imports: /std/build,/std/path,/std/file,"
                     "/std/collections") != std::string::npos);
}

TEST_CASE("a pure program gives the same result under both profiles") {
  const ProfileRun program = runProfile("profile_pure_program", PureSource, "--profile program");
  const ProfileRun build = runProfile("profile_pure_build", PureSource, "--profile build");
  const ProfileRun none = runProfile("profile_pure_none", PureSource, "");
  CHECK(program.exitCode == 5);
  CHECK(build.exitCode == 5);
  CHECK(none.exitCode == 5);
  CHECK(build.out == program.out);
  CHECK(build.err.empty());
}

TEST_CASE("a flag after the profile overrides its value") {
  const ProfileRun effect =
      runProfile("profile_override_after_effect", NetEffectSource, "--profile build --allow-effects net_io");
  CHECK(effect.exitCode == 7);
  const ProfileRun import =
      runProfile("profile_override_after_import", NetImportSource, "--profile=build --allow-import /std/net");
  CHECK(import.exitCode == 3);
  const ProfileRun print = runProfile("profile_override_after_print",
                                      PrintSource,
                                      "--profile build --default-effects io_out --allow-effects io_out");
  CHECK(print.exitCode == 2);
  CHECK(print.out == "hello\n");
}

TEST_CASE("a flag before the profile also overrides its value") {
  const ProfileRun effect =
      runProfile("profile_override_before_effect", NetEffectSource, "--allow-effects net_io --profile build");
  CHECK(effect.exitCode == 7);
  const ProfileRun print = runProfile("profile_override_before_print",
                                      PrintSource,
                                      "--allow-effects io_out --default-effects io_out --profile build");
  CHECK(print.out == "hello\n");
}

TEST_CASE("an unknown profile name is an argument error") {
  const ProfileRun run = runProfile("profile_unknown", PureSource, "--profile fast");
  CHECK(run.exitCode != 0);
  CHECK(run.err.find("unsupported --profile value: fast (expected program|build)") != std::string::npos);
  const ProfileRun missing = runProfile("profile_missing", PureSource, "--profile");
  CHECK(missing.exitCode != 0);
  CHECK(missing.err.find("--profile requires a value") != std::string::npos);
}
