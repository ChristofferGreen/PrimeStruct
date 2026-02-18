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
  [return<int>]
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
  [return<int>]
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
import /util, /math/*
namespace util {
  [return<int>]
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

TEST_CASE("compiles and runs include expansion") {
  const std::string libPath = writeTemp("compile_lib.prime", "[return<int>]\nhelper(){ return(5i32) }\n");
  const std::string source = "include<\"" + libPath + "\">\n[return<int>]\nmain(){ return(helper()) }\n";
  const std::string srcPath = writeTemp("compile_include.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_inc_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 5);
}

TEST_CASE("compiles and runs single-quoted include expansion") {
  const std::string libPath = writeTemp("compile_lib_single.prime", "[return<int>]\nhelper(){ return(6i32) }\n");
  const std::string source = "include<'" + libPath + "'>\n[return<int>]\nmain(){ return(helper()) }\n";
  const std::string srcPath = writeTemp("compile_include_single.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_inc_single_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 6);
}

TEST_CASE("compiles and runs with duplicate includes ignored") {
  const std::string libPath = writeTemp("compile_lib_dupe.prime", "[return<int>]\nhelper(){ return(5i32) }\n");
  const std::string source = "include<\"" + libPath + "\">\ninclude<\"" + libPath +
                             "\">\n[return<int>]\nmain(){ return(helper()) }\n";
  const std::string srcPath = writeTemp("compile_include_dupe.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_inc_dupe_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 5);
}

TEST_CASE("rejects include path with suffix") {
  const std::string srcPath =
      writeTemp("compile_include_suffix.prime", "include<\"/std/io\"utf8>\n[return<int>]\nmain(){ return(0i32) }\n");
  const std::string errPath = (std::filesystem::temp_directory_path() / "primec_include_suffix_err.txt").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath) == "Include error: include path cannot have suffix\n");
}

TEST_CASE("compiles and runs unquoted include expansion") {
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

  const std::string source = "include</lib>\n[return<int>]\nmain(){ return(helper()) }\n";
  const std::string srcPath = writeTemp("compile_unquoted_include.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_unquoted_inc_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath +
                                 " --entry /main --include-path " + includeRoot.string();
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 5);
}

TEST_CASE("compiles and runs versioned include expansion") {
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
      "include</lib, version=\"1.2\">\n"
      "[return<int>]\n"
      "main(){ return(helper()) }\n";
  const std::string srcPath = writeTemp("compile_versioned_include.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_versioned_inc_exe").string();
  const std::string nativePath = (std::filesystem::temp_directory_path() / "primec_versioned_inc_native").string();

  const std::string compileCppCmd = "./primec --emit=exe " + srcPath + " -o " + exePath +
                                    " --entry /main --include-path " + includeRoot.string();
  CHECK(runCommand(compileCppCmd) == 0);
  CHECK(runCommand(exePath) == 7);

  const std::string runVmCmd =
      "./primec --emit=vm " + srcPath + " --entry /main --include-path " + includeRoot.string();
  CHECK(runCommand(runVmCmd) == 7);

  const std::string compileNativeCmd = "./primec --emit=native " + srcPath + " -o " + nativePath +
                                       " --entry /main --include-path " + includeRoot.string();
  CHECK(runCommand(compileNativeCmd) == 0);
  CHECK(runCommand(nativePath) == 7);
}

TEST_CASE("compiles and runs versioned include with version first") {
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
      "include<version=\"1.2\", \"/lib\">\n"
      "[return<int>]\n"
      "main(){ return(helper()) }\n";
  const std::string srcPath = writeTemp("compile_versioned_include_first.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_versioned_inc_first_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath +
                                 " --entry /main --include-path " + includeRoot.string();
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 9);
}

TEST_SUITE_END();
#endif
