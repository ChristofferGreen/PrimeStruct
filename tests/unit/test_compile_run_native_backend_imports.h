#if defined(__APPLE__) && (defined(__arm64__) || defined(__aarch64__))
TEST_SUITE_BEGIN("primestruct.compile.run.native_backend.imports");

TEST_CASE("rejects unsupported effects in native backend") {
  const std::string source = R"(
[return<int> effects(render_graph)]
main() {
  return(1i32)
}
)";
  const std::string srcPath = writeTemp("compile_native_unsupported_effect.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() / "primec_native_unsupported_effect_exe").string();
  const std::string errPath =
      (std::filesystem::temp_directory_path() / "primec_native_unsupported_effect_err.txt").string();

  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath) ==
        "Native lowering error: native backend does not support effect: render_graph on /main\n");
}

TEST_CASE("rejects unsupported default effects in native backend") {
  const std::string source = R"(
[return<int>]
main() {
  return(1i32)
}
)";
  const std::string srcPath = writeTemp("compile_native_unsupported_default_effect.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() / "primec_native_unsupported_default_effect_exe").string();
  const std::string errPath =
      (std::filesystem::temp_directory_path() / "primec_native_unsupported_default_effect_err.txt").string();

  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main --default-effects=render_graph 2> " +
      errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath) ==
        "Native lowering error: native backend does not support effect: render_graph on /main\n");
}

TEST_CASE("rejects unsupported default effects in vm backend") {
  const std::string source = R"(
[return<int>]
main() {
  return(1i32)
}
)";
  const std::string srcPath = writeTemp("compile_vm_unsupported_default_effect.prime", source);
  const std::string errPath =
      (std::filesystem::temp_directory_path() / "primec_vm_unsupported_default_effect_err.txt").string();

  const std::string compileCmd =
      "./primec --emit=vm " + srcPath + " --entry /main --default-effects=render_graph 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath) ==
        "VM lowering error: vm backend does not support effect: render_graph on /main\n");
}

TEST_CASE("rejects unsupported execution effects in native backend") {
  const std::string source = R"(
[return<int>]
main() {
  [effects(render_graph)] plus(1i32, 2i32)
  return(1i32)
}
)";
  const std::string srcPath = writeTemp("compile_native_unsupported_exec_effect.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() / "primec_native_unsupported_exec_effect_exe").string();
  const std::string errPath =
      (std::filesystem::temp_directory_path() / "primec_native_unsupported_exec_effect_err.txt").string();

  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main --default-effects=render_graph 2> " +
      errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath) ==
        "Native lowering error: native backend does not support effect: render_graph on /main\n");
}

TEST_CASE("rejects unsupported execution effects in vm backend") {
  const std::string source = R"(
[return<int>]
main() {
  [effects(render_graph)] plus(1i32, 2i32)
  return(1i32)
}
)";
  const std::string srcPath = writeTemp("compile_vm_unsupported_exec_effect.prime", source);
  const std::string errPath =
      (std::filesystem::temp_directory_path() / "primec_vm_unsupported_exec_effect_err.txt").string();

  const std::string compileCmd =
      "./primec --emit=vm " + srcPath + " --entry /main --default-effects=render_graph 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath) ==
        "VM lowering error: vm backend does not support effect: render_graph on /main\n");
}

TEST_CASE("accepts vm support-matrix effects") {
  const std::string source = R"(
[return<int> effects(io_out, io_err, heap_alloc, file_write, gpu_dispatch,
                     pathspace_notify, pathspace_insert, pathspace_take,
                     pathspace_bind, pathspace_schedule)]
main() {
  return(1i32)
}
)";
  const std::string srcPath = writeTemp("compile_vm_support_matrix_effects.prime", source);
  const std::string compileCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(compileCmd) == 1);
}

TEST_CASE("accepts native support-matrix effects") {
  const std::string source = R"(
[return<int> effects(io_out, io_err, heap_alloc, file_write, gpu_dispatch,
                     pathspace_notify, pathspace_insert, pathspace_take,
                     pathspace_bind, pathspace_schedule)]
main() {
  return(1i32)
}
)";
  const std::string srcPath = writeTemp("compile_native_support_matrix_effects.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() / "primec_native_support_matrix_effects_exe").string();
  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 1);
}

TEST_CASE("rejects vm support-matrix effect outside allowlist") {
  const std::string source = R"(
[return<int> effects(global_write)]
main() {
  return(1i32)
}
)";
  const std::string srcPath = writeTemp("compile_vm_support_matrix_effect_reject.prime", source);
  const std::string errPath =
      (std::filesystem::temp_directory_path() / "primec_vm_support_matrix_effect_reject_err.txt").string();
  const std::string compileCmd = "./primec --emit=vm " + srcPath + " --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath) ==
        "VM lowering error: vm backend does not support effect: global_write on /main\n");
}

TEST_CASE("rejects native support-matrix effect outside allowlist") {
  const std::string source = R"(
[return<int> effects(global_write)]
main() {
  return(1i32)
}
)";
  const std::string srcPath = writeTemp("compile_native_support_matrix_effect_reject.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() / "primec_native_support_matrix_effect_reject_exe").string();
  const std::string errPath =
      (std::filesystem::temp_directory_path() / "primec_native_support_matrix_effect_reject_err.txt").string();
  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath) ==
        "Native lowering error: native backend does not support effect: global_write on /main\n");
}

TEST_CASE("compiles and runs namespace entry") {
  const std::string source = R"(
namespace demo {
  [return<int>]
  main() {
    return(9i32)
  }
}
)";
  const std::string srcPath = writeTemp("compile_ns.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_ns_exe").string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /demo/main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 9);
}

TEST_CASE("compiles and runs native import alias in namespace") {
  const std::string source = R"(
import /util
namespace util {
  [public return<int>]
  helper() {
    return(7i32)
  }
}
namespace demo {
  [return<int>]
  main() {
    return(helper())
  }
}
)";
  const std::string srcPath = writeTemp("compile_import_alias_namespace.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_import_alias_namespace").string();

  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /demo/main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 7);
}

TEST_CASE("compiles and runs native import alias") {
  const std::string source = R"(
import /util
namespace util {
  [public return<int>]
  inc([i32] value) {
    return(plus(value, 1i32))
  }
}
[return<int>]
main() {
  return(inc(4i32))
}
)";
  const std::string srcPath = writeTemp("compile_native_import_alias.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_import_alias_exe").string();

  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 5);
}

TEST_CASE("compiles and runs native with multiple imports") {
  const std::string source = R"(
import /util, /std/math/*
namespace util {
  [public return<int>]
  add([i32] a, [i32] b) {
    return(plus(a, b))
  }
}
[return<int>]
main() {
  return(plus(add(2i32, 3i32), min(7i32, 3i32)))
}
)";
  const std::string srcPath = writeTemp("compile_native_import_multiple.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_import_multiple_exe").string();

  const std::string compileCmd =
      "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 8);
}

TEST_CASE("compiles and runs import expansion") {
  const std::string libPath = writeTemp("compile_lib.prime", "[return<int>]\nhelper(){ return(5i32) }\n");
  const std::string source = "import<\"" + libPath + "\">\n[return<int>]\nmain(){ return(helper()) }\n";
  const std::string srcPath = writeTemp("compile_include.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_inc_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 5);
}

TEST_CASE("rejects legacy include expansion alias") {
  const std::string source = "include<\"/std/io\">\n[return<int>]\nmain(){ return(0i32) }\n";
  const std::string srcPath = writeTemp("compile_legacy_include_alias.prime", source);
  const std::string errPath =
      (std::filesystem::temp_directory_path() / "primec_legacy_include_alias_err.txt").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath) == "Import error: legacy include<...> is no longer supported; use import<...>\n");
}

TEST_CASE("emit-diagnostics reports legacy include alias rejection payload") {
  const std::string source = "include<\"/std/io\">\n[return<int>]\nmain(){ return(0i32) }\n";
  const std::string srcPath = writeTemp("emit_diagnostics_legacy_include_alias.prime", source);
  const std::string primecErrPath =
      (std::filesystem::temp_directory_path() / "primec_emit_diagnostics_legacy_include_alias_err.json").string();
  const std::string primevmErrPath =
      (std::filesystem::temp_directory_path() / "primevm_emit_diagnostics_legacy_include_alias_err.json").string();

  const std::string primecCmd = "./primec --emit=exe " + quoteShellArg(srcPath) +
                                " -o /dev/null --entry /main --emit-diagnostics 2> " + quoteShellArg(primecErrPath);
  CHECK(runCommand(primecCmd) == 2);
  const std::string primecDiagnostics = readFile(primecErrPath);
  CHECK(primecDiagnostics.find("\"version\":1") != std::string::npos);
  CHECK(primecDiagnostics.find("\"code\":\"PSC1001\"") != std::string::npos);
  CHECK(primecDiagnostics.find("\"severity\":\"error\"") != std::string::npos);
  CHECK(primecDiagnostics.find("\"message\":\"legacy include<...> is no longer supported; use import<...>\"") !=
        std::string::npos);
  CHECK(primecDiagnostics.find("\"notes\":[\"stage: import\"]") != std::string::npos);
  CHECK(primecDiagnostics.find("Import error: ") == std::string::npos);

  const std::string primevmCmd =
      "./primevm " + quoteShellArg(srcPath) + " --entry /main --emit-diagnostics 2> " + quoteShellArg(primevmErrPath);
  CHECK(runCommand(primevmCmd) == 2);
  const std::string primevmDiagnostics = readFile(primevmErrPath);
  CHECK(primevmDiagnostics.find("\"version\":1") != std::string::npos);
  CHECK(primevmDiagnostics.find("\"code\":\"PSC1001\"") != std::string::npos);
  CHECK(primevmDiagnostics.find("\"severity\":\"error\"") != std::string::npos);
  CHECK(primevmDiagnostics.find("\"message\":\"legacy include<...> is no longer supported; use import<...>\"") !=
        std::string::npos);
  CHECK(primevmDiagnostics.find("\"notes\":[\"stage: import\"]") != std::string::npos);
  CHECK(primevmDiagnostics.find("Import error: ") == std::string::npos);
}

TEST_CASE("compiles and runs single-quoted import expansion") {
  const std::string libPath = writeTemp("compile_lib_single.prime", "[return<int>]\nhelper(){ return(6i32) }\n");
  const std::string source = "import<'" + libPath + "'>\n[return<int>]\nmain(){ return(helper()) }\n";
  const std::string srcPath = writeTemp("compile_include_single.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_inc_single_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 6);
}

TEST_CASE("compiles and runs with duplicate imports ignored") {
  const std::string libPath = writeTemp("compile_lib_dupe.prime", "[return<int>]\nhelper(){ return(5i32) }\n");
  const std::string source = "import<\"" + libPath + "\">\nimport<\"" + libPath +
                             "\">\n[return<int>]\nmain(){ return(helper()) }\n";
  const std::string srcPath = writeTemp("compile_include_dupe.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_inc_dupe_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 5);
}

TEST_CASE("rejects import path with suffix") {
  const std::string srcPath =
      writeTemp("compile_include_suffix.prime", "import<\"/std/io\"utf8>\n[return<int>]\nmain(){ return(0i32) }\n");
  const std::string errPath = (std::filesystem::temp_directory_path() / "primec_include_suffix_err.txt").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath) == "Import error: import path cannot have suffix\n");
}

TEST_CASE("compiles and runs unquoted import expansion") {
  const std::filesystem::path includeRoot =
      std::filesystem::temp_directory_path() / "primec_tests" / "include_root_unquoted";
  std::filesystem::create_directories(includeRoot);
  {
    std::filesystem::create_directories(includeRoot / "lib");
    std::ofstream libFile(includeRoot / "lib" / "lib.prime");
    CHECK(libFile.good());
    libFile << "[return<int>]\nhelper(){ return(5i32) }\n";
    CHECK(libFile.good());
  }

  const std::string source = "import</lib>\n[return<int>]\nmain(){ return(helper()) }\n";
  const std::string srcPath = writeTemp("compile_unquoted_include.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_unquoted_inc_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath +
                                 " --entry /main --import-path " + includeRoot.string();
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 5);
}

TEST_CASE("compiles and runs unquoted import expansion with -I") {
  const std::filesystem::path importRoot =
      std::filesystem::temp_directory_path() / "primec_tests" / "import_root_short_flag";
  std::filesystem::remove_all(importRoot);
  std::filesystem::create_directories(importRoot);
  {
    std::filesystem::create_directories(importRoot / "lib");
    std::ofstream libFile(importRoot / "lib" / "lib.prime");
    CHECK(libFile.good());
    libFile << "[return<int>]\nhelper(){ return(13i32) }\n";
    CHECK(libFile.good());
  }

  const std::string source = "import</lib>\n[return<int>]\nmain(){ return(helper()) }\n";
  const std::string srcPath = writeTemp("compile_unquoted_import_short_flag.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_unquoted_import_short_exe").string();

  const std::string compileCmd =
      "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main -I " + importRoot.string();
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 13);

  const std::string runVmViaPrimecCmd =
      "./primec --emit=vm " + srcPath + " --entry /main -I " + importRoot.string();
  CHECK(runCommand(runVmViaPrimecCmd) == 13);

  const std::string runPrimevmCmd =
      "./primevm " + srcPath + " --entry /main -I " + importRoot.string();
  CHECK(runCommand(runPrimevmCmd) == 13);
}

TEST_CASE("legacy include-path alias is rejected in primec and primevm") {
  const std::filesystem::path includeRoot =
      std::filesystem::temp_directory_path() / "primec_tests" / "include_root_legacy_flag_alias";
  std::filesystem::remove_all(includeRoot);
  std::filesystem::create_directories(includeRoot);
  {
    std::filesystem::create_directories(includeRoot / "lib");
    std::ofstream libFile(includeRoot / "lib" / "lib.prime");
    CHECK(libFile.good());
    libFile << "[return<int>]\nhelper(){ return(12i32) }\n";
    CHECK(libFile.good());
  }

  const std::string source = "import</lib>\n[return<int>]\nmain(){ return(helper()) }\n";
  const std::string srcPath = writeTemp("compile_unquoted_include_legacy_flag.prime", source);
  const std::string primecErrPath =
      (std::filesystem::temp_directory_path() / "primec_include_path_legacy_err.txt").string();
  const std::string primecVmErrPath =
      (std::filesystem::temp_directory_path() / "primec_vm_include_path_legacy_err.txt").string();
  const std::string primevmErrPath =
      (std::filesystem::temp_directory_path() / "primevm_include_path_legacy_err.txt").string();

  const std::string compileCppCmd = "./primec --emit=exe " + srcPath + " -o /dev/null " +
                                    " --entry /main --include-path " + includeRoot.string();
  CHECK(runCommand(compileCppCmd + " 2> " + primecErrPath) == 2);
  const std::string primecErr = readFile(primecErrPath);
  CHECK(primecErr.find("Argument error: unknown option: --include-path\n") != std::string::npos);

  const std::string runVmViaPrimecCmd =
      "./primec --emit=vm " + srcPath + " --entry /main --include-path=" + includeRoot.string();
  CHECK(runCommand(runVmViaPrimecCmd + " 2> " + primecVmErrPath) == 2);
  const std::string primecVmErr = readFile(primecVmErrPath);
  CHECK(primecVmErr.find("Argument error: unknown option: --include-path=" + includeRoot.string() + "\n") !=
        std::string::npos);

  const std::string runPrimevmCmd =
      "./primevm " + srcPath + " --entry /main --include-path=" + includeRoot.string();
  CHECK(runCommand(runPrimevmCmd + " 2> " + primevmErrPath) == 2);
  const std::string primevmErr = readFile(primevmErrPath);
  CHECK(primevmErr.find("Argument error: unknown option: --include-path=" + includeRoot.string() + "\n") !=
        std::string::npos);
}

TEST_CASE("emit-diagnostics reports argument payload for removed include-path option") {
  const std::filesystem::path includeRoot =
      std::filesystem::temp_directory_path() / "primec_tests" / "include_root_legacy_flag_diagnostics";
  std::filesystem::remove_all(includeRoot);
  std::filesystem::create_directories(includeRoot);

  const std::string source = R"(
[return<int>]
main() {
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("emit_diagnostics_include_path_removed.prime", source);
  const std::string primecErrPath =
      (std::filesystem::temp_directory_path() / "primec_emit_diagnostics_include_path_removed_err.json").string();
  const std::string primecBareErrPath =
      (std::filesystem::temp_directory_path() / "primec_emit_diagnostics_include_path_removed_bare_err.json")
          .string();
  const std::string primevmErrPath =
      (std::filesystem::temp_directory_path() / "primevm_emit_diagnostics_include_path_removed_err.json").string();
  const std::string primevmBareErrPath =
      (std::filesystem::temp_directory_path() / "primevm_emit_diagnostics_include_path_removed_bare_err.json")
          .string();

  const std::string primecCmd = "./primec " + quoteShellArg(srcPath) + " --emit-diagnostics --include-path=" +
                                includeRoot.string() + " 2> " + quoteShellArg(primecErrPath);
  CHECK(runCommand(primecCmd) == 2);
  const std::string primecDiagnostics = readFile(primecErrPath);
  CHECK(primecDiagnostics.find("\"code\":\"PSC0001\"") != std::string::npos);
  CHECK(primecDiagnostics.find("\"message\":\"unknown option: --include-path=" + includeRoot.string() + "\"") !=
        std::string::npos);
  CHECK(primecDiagnostics.find("\"severity\":\"error\"") != std::string::npos);
  CHECK(primecDiagnostics.find("Usage: primec") == std::string::npos);

  const std::string primecBareCmd = "./primec " + quoteShellArg(srcPath) +
                                    " --emit-diagnostics --include-path 2> " + quoteShellArg(primecBareErrPath);
  CHECK(runCommand(primecBareCmd) == 2);
  const std::string primecBareDiagnostics = readFile(primecBareErrPath);
  CHECK(primecBareDiagnostics.find("\"code\":\"PSC0001\"") != std::string::npos);
  CHECK(primecBareDiagnostics.find("\"message\":\"unknown option: --include-path\"") != std::string::npos);
  CHECK(primecBareDiagnostics.find("\"severity\":\"error\"") != std::string::npos);
  CHECK(primecBareDiagnostics.find("Usage: primec") == std::string::npos);

  const std::string primevmCmd = "./primevm " + quoteShellArg(srcPath) + " --emit-diagnostics --include-path=" +
                                 includeRoot.string() + " 2> " + quoteShellArg(primevmErrPath);
  CHECK(runCommand(primevmCmd) == 2);
  const std::string primevmDiagnostics = readFile(primevmErrPath);
  CHECK(primevmDiagnostics.find("\"code\":\"PSC0001\"") != std::string::npos);
  CHECK(primevmDiagnostics.find("\"message\":\"unknown option: --include-path=" + includeRoot.string() + "\"") !=
        std::string::npos);
  CHECK(primevmDiagnostics.find("\"severity\":\"error\"") != std::string::npos);
  CHECK(primevmDiagnostics.find("Usage: primevm") == std::string::npos);

  const std::string primevmBareCmd = "./primevm " + quoteShellArg(srcPath) +
                                     " --emit-diagnostics --include-path 2> " + quoteShellArg(primevmBareErrPath);
  CHECK(runCommand(primevmBareCmd) == 2);
  const std::string primevmBareDiagnostics = readFile(primevmBareErrPath);
  CHECK(primevmBareDiagnostics.find("\"code\":\"PSC0001\"") != std::string::npos);
  CHECK(primevmBareDiagnostics.find("\"message\":\"unknown option: --include-path\"") != std::string::npos);
  CHECK(primevmBareDiagnostics.find("\"severity\":\"error\"") != std::string::npos);
  CHECK(primevmBareDiagnostics.find("Usage: primevm") == std::string::npos);
}

TEST_CASE("compiles and runs versioned import expansion") {
  const std::filesystem::path includeRoot =
      std::filesystem::temp_directory_path() / "primec_tests" / "include_root_versioned";
  std::filesystem::remove_all(includeRoot);
  std::filesystem::create_directories(includeRoot);
  {
    std::filesystem::create_directories((includeRoot / "1.2.0" / "lib"));
    std::ofstream oldLib(includeRoot / "1.2.0" / "lib" / "lib.prime");
    CHECK(oldLib.good());
    oldLib << "[return<int>]\nhelper(){ return(5i32) }\n";
    CHECK(oldLib.good());
  }
  {
    std::filesystem::create_directories((includeRoot / "1.2.3" / "lib"));
    std::ofstream newLib(includeRoot / "1.2.3" / "lib" / "lib.prime");
    CHECK(newLib.good());
    newLib << "[return<int>]\nhelper(){ return(7i32) }\n";
    CHECK(newLib.good());
  }

  const std::string source =
      "import</lib, version=\"1.2\">\n"
      "[return<int>]\n"
      "main(){ return(helper()) }\n";
  const std::string srcPath = writeTemp("compile_versioned_include.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_versioned_inc_exe").string();
  const std::string nativePath = (std::filesystem::temp_directory_path() / "primec_versioned_inc_native").string();

  const std::string compileCppCmd = "./primec --emit=exe " + srcPath + " -o " + exePath +
                                    " --entry /main --import-path " + includeRoot.string();
  CHECK(runCommand(compileCppCmd) == 0);
  CHECK(runCommand(exePath) == 7);

  const std::string runVmCmd =
      "./primec --emit=vm " + srcPath + " --entry /main --import-path " + includeRoot.string();
  CHECK(runCommand(runVmCmd) == 7);

  const std::string compileNativeCmd = "./primec --emit=native " + srcPath + " -o " + nativePath +
                                       " --entry /main --import-path " + includeRoot.string();
  CHECK(runCommand(compileNativeCmd) == 0);
  CHECK(runCommand(nativePath) == 7);
}

TEST_CASE("compiles and runs versioned import with version first") {
  const std::filesystem::path includeRoot =
      std::filesystem::temp_directory_path() / "primec_tests" / "include_root_versioned_first";
  std::filesystem::remove_all(includeRoot);
  std::filesystem::create_directories(includeRoot);
  {
    std::filesystem::create_directories((includeRoot / "1.2.0" / "lib"));
    std::ofstream oldLib(includeRoot / "1.2.0" / "lib" / "lib.prime");
    CHECK(oldLib.good());
    oldLib << "[return<int>]\nhelper(){ return(4i32) }\n";
    CHECK(oldLib.good());
  }
  {
    std::filesystem::create_directories((includeRoot / "1.2.5" / "lib"));
    std::ofstream newLib(includeRoot / "1.2.5" / "lib" / "lib.prime");
    CHECK(newLib.good());
    newLib << "[return<int>]\nhelper(){ return(9i32) }\n";
    CHECK(newLib.good());
  }

  const std::string source =
      "import<version=\"1.2\", \"/lib\">\n"
      "[return<int>]\n"
      "main(){ return(helper()) }\n";
  const std::string srcPath = writeTemp("compile_versioned_include_first.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_versioned_inc_first_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath +
                                 " --entry /main --import-path " + includeRoot.string();
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 9);
}

TEST_SUITE_END();
#endif
