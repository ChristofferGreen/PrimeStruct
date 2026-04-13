#include "test_compile_run_helpers.h"

TEST_SUITE_BEGIN("primestruct.compile.run.imports");

TEST_CASE("compiles and runs versioned import expansion with relative import entry") {
  const std::filesystem::path includeRoot =
      testScratchPath("") / "primec_tests" / "include_root_versioned_relative";
  std::filesystem::remove_all(includeRoot);
  std::filesystem::create_directories(includeRoot);
  {
    std::filesystem::create_directories(includeRoot / "1.2.0" / "std" / "io");
    std::ofstream oldLib(includeRoot / "1.2.0" / "std" / "io" / "lib.prime");
    CHECK(oldLib.good());
    oldLib << "[return<int>]\nhelper(){ return(5i32) }\n";
    CHECK(oldLib.good());
  }
  {
    std::filesystem::create_directories(includeRoot / "1.2.4" / "std" / "io");
    std::ofstream newLib(includeRoot / "1.2.4" / "std" / "io" / "lib.prime");
    CHECK(newLib.good());
    newLib << "[return<int>]\nhelper(){ return(7i32) }\n";
    CHECK(newLib.good());
  }

  const std::string source =
      "import<\"./std/io\", version=\"1.2\">\n"
      "[return<int>]\n"
      "main(){ return(helper()) }\n";
  const std::string srcPath = writeTemp("compile_versioned_include_relative.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_versioned_inc_relative_exe").string();
  const std::string nativePath =
      (testScratchPath("") / "primec_versioned_inc_relative_native").string();

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

TEST_CASE("compiles and runs exact versioned import expansion") {
  const std::filesystem::path includeRoot =
      testScratchPath("") / "primec_tests" / "include_root_versioned_exact";
  std::filesystem::remove_all(includeRoot);
  std::filesystem::create_directories(includeRoot);
  {
    std::filesystem::create_directories(includeRoot / "1.2.0" / "std" / "io");
    std::ofstream oldLib(includeRoot / "1.2.0" / "std" / "io" / "lib.prime");
    CHECK(oldLib.good());
    oldLib << "[return<int>]\nhelper(){ return(5i32) }\n";
    CHECK(oldLib.good());
  }
  {
    std::filesystem::create_directories(includeRoot / "1.2.4" / "std" / "io");
    std::ofstream newLib(includeRoot / "1.2.4" / "std" / "io" / "lib.prime");
    CHECK(newLib.good());
    newLib << "[return<int>]\nhelper(){ return(7i32) }\n";
    CHECK(newLib.good());
  }

  const std::string source =
      "import<\"/std/io\", version=\"1.2.0\">\n"
      "[return<int>]\n"
      "main(){ return(helper()) }\n";
  const std::string srcPath = writeTemp("compile_versioned_include_exact.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_versioned_inc_exact_exe").string();
  const std::string nativePath =
      (testScratchPath("") / "primec_versioned_inc_exact_native").string();

  const std::string compileCppCmd = "./primec --emit=exe " + srcPath + " -o " + exePath +
                                    " --entry /main --import-path " + includeRoot.string();
  CHECK(runCommand(compileCppCmd) == 0);
  CHECK(runCommand(exePath) == 5);

  const std::string runVmCmd =
      "./primec --emit=vm " + srcPath + " --entry /main --import-path " + includeRoot.string();
  CHECK(runCommand(runVmCmd) == 5);

  const std::string compileNativeCmd = "./primec --emit=native " + srcPath + " -o " + nativePath +
                                       " --entry /main --import-path " + includeRoot.string();
  CHECK(runCommand(compileNativeCmd) == 0);
  CHECK(runCommand(nativePath) == 5);
}

TEST_CASE("rejects versioned legacy include alias expansion") {
  const std::string source =
      "include<\"/std/io\", version=\"1.2\">\n"
      "[return<int>]\n"
      "main(){ return(0i32) }\n";
  const std::string srcPath = writeTemp("compile_versioned_include_legacy_alias.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_versioned_inc_legacy_alias_err.txt").string();

  const std::string compileCppCmd = "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCppCmd) == 2);
  CHECK(readFile(errPath) == "Import error: legacy include<...> is no longer supported; use import<...>\n");
}

TEST_CASE("rejects version-first legacy include alias expansion") {
  const std::string source =
      "include<version=\"1.2\", \"/std/io\">\n"
      "[return<int>]\n"
      "main(){ return(0i32) }\n";
  const std::string srcPath = writeTemp("compile_versioned_include_legacy_alias_first.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_versioned_inc_legacy_alias_first_err.txt").string();

  const std::string compileCppCmd = "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main 2> " + errPath;
  CHECK(runCommand(compileCppCmd) == 2);
  CHECK(readFile(errPath) == "Import error: legacy include<...> is no longer supported; use import<...>\n");
}

TEST_CASE("compiles and runs versioned import expansion with quoted import entries") {
  const std::filesystem::path includeRoot =
      testScratchPath("") / "primec_tests" / "include_root_versioned_quoted";
  std::filesystem::remove_all(includeRoot);
  std::filesystem::create_directories(includeRoot);
  {
    std::filesystem::create_directories(includeRoot / "1.2.0" / "std" / "io");
    std::ofstream oldLib(includeRoot / "1.2.0" / "std" / "io" / "lib.prime");
    CHECK(oldLib.good());
    oldLib << "[return<int>]\nhelper(){ return(5i32) }\n";
    CHECK(oldLib.good());
  }
  {
    std::filesystem::create_directories(includeRoot / "1.2.9" / "std" / "io");
    std::ofstream newLib(includeRoot / "1.2.9" / "std" / "io" / "lib.prime");
    CHECK(newLib.good());
    newLib << "[return<int>]\nhelper(){ return(7i32) }\n";
    CHECK(newLib.good());
  }

  const std::string source =
      "import<\"/std/io\", version=\"1.2\">\n"
      "[return<int>]\n"
      "main(){ return(helper()) }\n";
  const std::string srcPath = writeTemp("compile_versioned_include_quoted.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_versioned_inc_quoted_exe").string();
  const std::string nativePath =
      (testScratchPath("") / "primec_versioned_inc_quoted_native").string();

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

TEST_CASE("compiles and runs import expansion with comments") {
  const std::filesystem::path includeRoot =
      testScratchPath("") / "primec_tests" / "include_root_versioned_comments";
  std::filesystem::remove_all(includeRoot);
  std::filesystem::create_directories(includeRoot);
  {
    std::filesystem::create_directories(includeRoot / "1.2.0" / "std" / "io");
    std::ofstream oldLib(includeRoot / "1.2.0" / "std" / "io" / "lib.prime");
    CHECK(oldLib.good());
    oldLib << "[return<int>]\nhelper(){ return(5i32) }\n";
    CHECK(oldLib.good());
  }
  {
    std::filesystem::create_directories(includeRoot / "1.2.9" / "std" / "io");
    std::ofstream newLib(includeRoot / "1.2.9" / "std" / "io" / "lib.prime");
    CHECK(newLib.good());
    newLib << "[return<int>]\nhelper(){ return(7i32) }\n";
    CHECK(newLib.good());
  }

  const std::string source =
      "import /* tag */ <\"/std/io\" /* entry */, /* gap */ version /* key */ = /* eq */ \"1.2\" /* end */>\n"
      "[return<int>]\n"
      "main(){ return(helper()) }\n";
  const std::string srcPath = writeTemp("compile_versioned_include_comments.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_versioned_inc_comments_exe").string();
  const std::string nativePath =
      (testScratchPath("") / "primec_versioned_inc_comments_native").string();

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

TEST_CASE("compiles and runs duplicate imports once") {
  const std::filesystem::path includeRoot =
      testScratchPath("") / "primec_tests" / "include_root_duplicate";
  std::filesystem::remove_all(includeRoot);
  std::filesystem::create_directories(includeRoot);
  {
    std::filesystem::create_directories(includeRoot / "1.2.0" / "std" / "io");
    std::ofstream lib(includeRoot / "1.2.0" / "std" / "io" / "lib.prime");
    CHECK(lib.good());
    lib << "[return<int>]\nhelper(){ return(9i32) }\n";
    CHECK(lib.good());
  }

  const std::string source =
      "import<\"/std/io\", version=\"1.2.0\">\n"
      "import<\"/std/io\", version=\"1.2.0\">\n"
      "[return<int>]\n"
      "main(){ return(helper()) }\n";
  const std::string srcPath = writeTemp("compile_duplicate_include.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_duplicate_inc_exe").string();
  const std::string nativePath =
      (testScratchPath("") / "primec_duplicate_inc_native").string();

  const std::string compileCppCmd = "./primec --emit=exe " + srcPath + " -o " + exePath +
                                    " --entry /main --import-path " + includeRoot.string();
  CHECK(runCommand(compileCppCmd) == 0);
  CHECK(runCommand(exePath) == 9);

  const std::string runVmCmd =
      "./primec --emit=vm " + srcPath + " --entry /main --import-path " + includeRoot.string();
  CHECK(runCommand(runVmCmd) == 9);

  const std::string compileNativeCmd = "./primec --emit=native " + srcPath + " -o " + nativePath +
                                       " --entry /main --import-path " + includeRoot.string();
  CHECK(runCommand(compileNativeCmd) == 0);
  CHECK(runCommand(nativePath) == 9);
}

TEST_CASE("compiles and runs versioned import with single quotes") {
  const std::filesystem::path includeRoot =
      testScratchPath("") / "primec_tests" / "include_root_versioned_single_quotes";
  std::filesystem::remove_all(includeRoot);
  std::filesystem::create_directories(includeRoot);
  {
    std::filesystem::create_directories(includeRoot / "1.2.0" / "std" / "io");
    std::ofstream oldLib(includeRoot / "1.2.0" / "std" / "io" / "lib.prime");
    CHECK(oldLib.good());
    oldLib << "[return<int>]\nhelper(){ return(3i32) }\n";
    CHECK(oldLib.good());
  }
  {
    std::filesystem::create_directories(includeRoot / "1.2.5" / "std" / "io");
    std::ofstream newLib(includeRoot / "1.2.5" / "std" / "io" / "lib.prime");
    CHECK(newLib.good());
    newLib << "[return<int>]\nhelper(){ return(8i32) }\n";
    CHECK(newLib.good());
  }

  const std::string source =
      "import< '/std/io' , version = '1.2' >\n"
      "[return<int>]\n"
      "main(){ return(helper()) }\n";
  const std::string srcPath = writeTemp("compile_versioned_include_single_quotes.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_versioned_inc_single_quotes_exe").string();
  const std::string nativePath =
      (testScratchPath("") / "primec_versioned_inc_single_quotes_native").string();

  const std::string compileCppCmd = "./primec --emit=exe " + srcPath + " -o " + exePath +
                                    " --entry /main --import-path " + includeRoot.string();
  CHECK(runCommand(compileCppCmd) == 0);
  CHECK(runCommand(exePath) == 8);

  const std::string runVmCmd =
      "./primec --emit=vm " + srcPath + " --entry /main --import-path " + includeRoot.string();
  CHECK(runCommand(runVmCmd) == 8);

  const std::string compileNativeCmd = "./primec --emit=native " + srcPath + " -o " + nativePath +
                                       " --entry /main --import-path " + includeRoot.string();
  CHECK(runCommand(compileNativeCmd) == 0);
  CHECK(runCommand(nativePath) == 8);
}

TEST_CASE("compiles and runs versioned import with major-only selector") {
  const std::filesystem::path includeRoot =
      testScratchPath("") / "primec_tests" / "include_root_versioned_major_only";
  std::filesystem::remove_all(includeRoot);
  std::filesystem::create_directories(includeRoot);
  {
    std::filesystem::create_directories(includeRoot / "1.0.0" / "std" / "io");
    std::ofstream oldLib(includeRoot / "1.0.0" / "std" / "io" / "lib.prime");
    CHECK(oldLib.good());
    oldLib << "[return<int>]\nhelper(){ return(2i32) }\n";
    CHECK(oldLib.good());
  }
  {
    std::filesystem::create_directories(includeRoot / "1.9.1" / "std" / "io");
    std::ofstream newLib(includeRoot / "1.9.1" / "std" / "io" / "lib.prime");
    CHECK(newLib.good());
    newLib << "[return<int>]\nhelper(){ return(7i32) }\n";
    CHECK(newLib.good());
  }
  {
    std::filesystem::create_directories(includeRoot / "2.0.0" / "std" / "io");
    std::ofstream otherLib(includeRoot / "2.0.0" / "std" / "io" / "lib.prime");
    CHECK(otherLib.good());
    otherLib << "[return<int>]\nhelper(){ return(9i32) }\n";
    CHECK(otherLib.good());
  }

  const std::string source =
      "import<\"/std/io\", version=\"1\">\n"
      "[return<int>]\n"
      "main(){ return(helper()) }\n";
  const std::string srcPath = writeTemp("compile_versioned_include_major_only.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_versioned_inc_major_only_exe").string();
  const std::string nativePath =
      (testScratchPath("") / "primec_versioned_inc_major_only_native").string();

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

TEST_CASE("compiles and runs versioned import with minor selector") {
  const std::filesystem::path includeRoot =
      testScratchPath("") / "primec_tests" / "include_root_versioned_minor_only";
  std::filesystem::remove_all(includeRoot);
  std::filesystem::create_directories(includeRoot);
  {
    std::filesystem::create_directories(includeRoot / "1.2.1" / "std" / "io");
    std::ofstream oldLib(includeRoot / "1.2.1" / "std" / "io" / "lib.prime");
    CHECK(oldLib.good());
    oldLib << "[return<int>]\nhelper(){ return(3i32) }\n";
    CHECK(oldLib.good());
  }
  {
    std::filesystem::create_directories(includeRoot / "1.2.9" / "std" / "io");
    std::ofstream newLib(includeRoot / "1.2.9" / "std" / "io" / "lib.prime");
    CHECK(newLib.good());
    newLib << "[return<int>]\nhelper(){ return(8i32) }\n";
    CHECK(newLib.good());
  }
  {
    std::filesystem::create_directories(includeRoot / "1.10.0" / "std" / "io");
    std::ofstream otherLib(includeRoot / "1.10.0" / "std" / "io" / "lib.prime");
    CHECK(otherLib.good());
    otherLib << "[return<int>]\nhelper(){ return(9i32) }\n";
    CHECK(otherLib.good());
  }

  const std::string source =
      "import<\"/std/io\", version=\"1.2\">\n"
      "[return<int>]\n"
      "main(){ return(helper()) }\n";
  const std::string srcPath = writeTemp("compile_versioned_include_minor_only.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_versioned_inc_minor_only_exe").string();
  const std::string nativePath =
      (testScratchPath("") / "primec_versioned_inc_minor_only_native").string();

  const std::string compileCppCmd = "./primec --emit=exe " + srcPath + " -o " + exePath +
                                    " --entry /main --import-path " + includeRoot.string();
  CHECK(runCommand(compileCppCmd) == 0);
  CHECK(runCommand(exePath) == 8);

  const std::string runVmCmd =
      "./primec --emit=vm " + srcPath + " --entry /main --import-path " + includeRoot.string();
  CHECK(runCommand(runVmCmd) == 8);

  const std::string compileNativeCmd = "./primec --emit=native " + srcPath + " -o " + nativePath +
                                       " --entry /main --import-path " + includeRoot.string();
  CHECK(runCommand(compileNativeCmd) == 0);
  CHECK(runCommand(nativePath) == 8);
}

TEST_CASE("compiles and runs versioned import expansion with multiple import entries") {
  const std::filesystem::path includeRoot =
      testScratchPath("") / "primec_tests" / "include_root_versioned_multi";
  std::filesystem::remove_all(includeRoot);
  std::filesystem::create_directories(includeRoot);
  {
    std::filesystem::create_directories(includeRoot / "1.2.0" / "std" / "io");
    std::ofstream ioLib(includeRoot / "1.2.0" / "std" / "io" / "lib.prime");
    CHECK(ioLib.good());
    ioLib << "[return<int>]\nio_helper(){ return(6i32) }\n";
    CHECK(ioLib.good());

    std::filesystem::create_directories(includeRoot / "1.2.0" / "std" / "math");
    std::ofstream mathLib(includeRoot / "1.2.0" / "std" / "math" / "lib.prime");
    CHECK(mathLib.good());
    mathLib << "[return<int>]\nmath_helper(){ return(3i32) }\n";
    CHECK(mathLib.good());
  }
  {
    std::filesystem::create_directories(includeRoot / "1.2.1" / "std" / "io");
    std::ofstream ioLib(includeRoot / "1.2.1" / "std" / "io" / "lib.prime");
    CHECK(ioLib.good());
    ioLib << "[return<int>]\nio_helper(){ return(7i32) }\n";
    CHECK(ioLib.good());

    std::filesystem::create_directories(includeRoot / "1.2.1" / "std" / "math");
    std::ofstream mathLib(includeRoot / "1.2.1" / "std" / "math" / "lib.prime");
    CHECK(mathLib.good());
    mathLib << "[return<int>]\nmath_helper(){ return(4i32) }\n";
    CHECK(mathLib.good());
  }

  const std::string source =
      "import</std/io, /std/math, version=\"1.2\">\n"
      "[return<int>]\n"
      "main(){ return(plus(io_helper(), math_helper())) }\n";
  const std::string srcPath = writeTemp("compile_versioned_include_multi.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_versioned_inc_multi_exe").string();
  const std::string nativePath =
      (testScratchPath("") / "primec_versioned_inc_multi_native").string();

  const std::string compileCppCmd = "./primec --emit=exe " + srcPath + " -o " + exePath +
                                    " --entry /main --import-path " + includeRoot.string();
  CHECK(runCommand(compileCppCmd) == 0);
  CHECK(runCommand(exePath) == 11);

  const std::string runVmCmd =
      "./primec --emit=vm " + srcPath + " --entry /main --import-path " + includeRoot.string();
  CHECK(runCommand(runVmCmd) == 11);

  const std::string compileNativeCmd = "./primec --emit=native " + srcPath + " -o " + nativePath +
                                       " --entry /main --import-path " + includeRoot.string();
  CHECK(runCommand(compileNativeCmd) == 0);
  CHECK(runCommand(nativePath) == 11);
}

TEST_CASE("rejects versioned import mismatch across roots") {
  const std::filesystem::path includeRootA =
      testScratchPath("") / "primec_tests" / "include_root_version_mismatch_a";
  const std::filesystem::path includeRootB =
      testScratchPath("") / "primec_tests" / "include_root_version_mismatch_b";
  std::filesystem::remove_all(includeRootA);
  std::filesystem::remove_all(includeRootB);
  std::filesystem::create_directories(includeRootA);
  std::filesystem::create_directories(includeRootB);
  {
    std::filesystem::create_directories(includeRootA / "1.2.1" / "std" / "io");
    std::ofstream ioLib(includeRootA / "1.2.1" / "std" / "io" / "lib.prime");
    CHECK(ioLib.good());
    ioLib << "[return<int>]\nio_helper(){ return(6i32) }\n";
    CHECK(ioLib.good());
  }
  {
    std::filesystem::create_directories(includeRootB / "1.2.0" / "std" / "math");
    std::ofstream mathLib(includeRootB / "1.2.0" / "std" / "math" / "lib.prime");
    CHECK(mathLib.good());
    mathLib << "[return<int>]\nmath_helper(){ return(3i32) }\n";
    CHECK(mathLib.good());
  }

  const std::string source =
      "import</std/io, /std/math, version=\"1.2\">\n"
      "[return<int>]\n"
      "main(){ return(0i32) }\n";
  const std::string srcPath = writeTemp("compile_versioned_include_mismatch.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_versioned_include_mismatch_err.txt").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath +
                                 " -o /dev/null --entry /main --import-path " + includeRootA.string() +
                                 " --import-path " + includeRootB.string() + " 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("import version mismatch") != std::string::npos);
}

TEST_CASE("rejects missing versioned import in compile") {
  const std::filesystem::path includeRoot =
      testScratchPath("") / "primec_tests" / "include_root_version_missing_compile";
  std::filesystem::remove_all(includeRoot);
  std::filesystem::create_directories(includeRoot);
  {
    std::filesystem::create_directories(includeRoot / "1.2.0" / "std" / "io");
    std::ofstream libFile(includeRoot / "1.2.0" / "std" / "io" / "lib.prime");
    CHECK(libFile.good());
    libFile << "[return<int>]\nhelper(){ return(5i32) }\n";
    CHECK(libFile.good());
  }

  const std::string source =
      "import</std/io, version=\"2.0.0\">\n"
      "[return<int>]\n"
      "main(){ return(helper()) }\n";
  const std::string srcPath = writeTemp("compile_versioned_include_missing.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_versioned_include_missing_err.txt").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main --import-path " +
                                 includeRoot.string() + " 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("Import error: import version not found") != std::string::npos);
}

TEST_CASE("compiles and runs versioned import expansion with mixed quoted and relative entries") {
  const std::filesystem::path includeRoot =
      testScratchPath("") / "primec_tests" / "include_root_versioned_mixed";
  std::filesystem::remove_all(includeRoot);
  std::filesystem::create_directories(includeRoot);
  {
    std::filesystem::create_directories(includeRoot / "1.2.0" / "std" / "io");
    std::ofstream ioLib(includeRoot / "1.2.0" / "std" / "io" / "lib.prime");
    CHECK(ioLib.good());
    ioLib << "[return<int>]\nio_helper(){ return(6i32) }\n";
    CHECK(ioLib.good());

    std::filesystem::create_directories(includeRoot / "1.2.0" / "local" / "helpers");
    std::ofstream localLib(includeRoot / "1.2.0" / "local" / "helpers" / "lib.prime");
    CHECK(localLib.good());
    localLib << "[return<int>]\nlocal_helper(){ return(3i32) }\n";
    CHECK(localLib.good());
  }
  {
    std::filesystem::create_directories(includeRoot / "1.2.1" / "std" / "io");
    std::ofstream ioLib(includeRoot / "1.2.1" / "std" / "io" / "lib.prime");
    CHECK(ioLib.good());
    ioLib << "[return<int>]\nio_helper(){ return(7i32) }\n";
    CHECK(ioLib.good());

    std::filesystem::create_directories(includeRoot / "1.2.1" / "local" / "helpers");
    std::ofstream localLib(includeRoot / "1.2.1" / "local" / "helpers" / "lib.prime");
    CHECK(localLib.good());
    localLib << "[return<int>]\nlocal_helper(){ return(4i32) }\n";
    CHECK(localLib.good());
  }

  const std::string source =
      "import<\"/std/io\", \"./local/helpers\", version=\"1.2\">\n"
      "[return<int>]\n"
      "main(){ return(plus(io_helper(), local_helper())) }\n";
  const std::string srcPath = writeTemp("compile_versioned_include_mixed.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_versioned_inc_mixed_exe").string();
  const std::string nativePath =
      (testScratchPath("") / "primec_versioned_inc_mixed_native").string();

  const std::string compileCppCmd = "./primec --emit=exe " + srcPath + " -o " + exePath +
                                    " --entry /main --import-path " + includeRoot.string();
  CHECK(runCommand(compileCppCmd) == 0);
  CHECK(runCommand(exePath) == 11);

  const std::string runVmCmd =
      "./primec --emit=vm " + srcPath + " --entry /main --import-path " + includeRoot.string();
  CHECK(runCommand(runVmCmd) == 11);

  const std::string compileNativeCmd = "./primec --emit=native " + srcPath + " -o " + nativePath +
                                       " --entry /main --import-path " + includeRoot.string();
  CHECK(runCommand(compileNativeCmd) == 0);
  CHECK(runCommand(nativePath) == 11);
}

#include "test_compile_run_imports_versions_archive.h"

TEST_SUITE_END();
