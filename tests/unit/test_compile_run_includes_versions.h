TEST_CASE("compiles and runs versioned include expansion with relative include entry") {
  const std::filesystem::path includeRoot =
      std::filesystem::temp_directory_path() / "primec_tests" / "include_root_versioned_relative";
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
      "include<\"./std/io\", version=\"1.2\">\n"
      "[return<int>]\n"
      "main(){ return(helper()) }\n";
  const std::string srcPath = writeTemp("compile_versioned_include_relative.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() / "primec_versioned_inc_relative_exe").string();
  const std::string nativePath =
      (std::filesystem::temp_directory_path() / "primec_versioned_inc_relative_native").string();

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

TEST_CASE("compiles and runs exact versioned include expansion") {
  const std::filesystem::path includeRoot =
      std::filesystem::temp_directory_path() / "primec_tests" / "include_root_versioned_exact";
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
      "include<\"/std/io\", version=\"1.2.0\">\n"
      "[return<int>]\n"
      "main(){ return(helper()) }\n";
  const std::string srcPath = writeTemp("compile_versioned_include_exact.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() / "primec_versioned_inc_exact_exe").string();
  const std::string nativePath =
      (std::filesystem::temp_directory_path() / "primec_versioned_inc_exact_native").string();

  const std::string compileCppCmd = "./primec --emit=exe " + srcPath + " -o " + exePath +
                                    " --entry /main --include-path " + includeRoot.string();
  CHECK(runCommand(compileCppCmd) == 0);
  CHECK(runCommand(exePath) == 5);

  const std::string runVmCmd =
      "./primec --emit=vm " + srcPath + " --entry /main --include-path " + includeRoot.string();
  CHECK(runCommand(runVmCmd) == 5);

  const std::string compileNativeCmd = "./primec --emit=native " + srcPath + " -o " + nativePath +
                                       " --entry /main --include-path " + includeRoot.string();
  CHECK(runCommand(compileNativeCmd) == 0);
  CHECK(runCommand(nativePath) == 5);
}

TEST_CASE("compiles and runs versioned include expansion with quoted include entries") {
  const std::filesystem::path includeRoot =
      std::filesystem::temp_directory_path() / "primec_tests" / "include_root_versioned_quoted";
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
      "include<\"/std/io\", version=\"1.2\">\n"
      "[return<int>]\n"
      "main(){ return(helper()) }\n";
  const std::string srcPath = writeTemp("compile_versioned_include_quoted.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() / "primec_versioned_inc_quoted_exe").string();
  const std::string nativePath =
      (std::filesystem::temp_directory_path() / "primec_versioned_inc_quoted_native").string();

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

TEST_CASE("compiles and runs include expansion with comments") {
  const std::filesystem::path includeRoot =
      std::filesystem::temp_directory_path() / "primec_tests" / "include_root_versioned_comments";
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
      "include /* tag */ <\"/std/io\" /* entry */, /* gap */ version /* key */ = /* eq */ \"1.2\" /* end */>\n"
      "[return<int>]\n"
      "main(){ return(helper()) }\n";
  const std::string srcPath = writeTemp("compile_versioned_include_comments.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() / "primec_versioned_inc_comments_exe").string();
  const std::string nativePath =
      (std::filesystem::temp_directory_path() / "primec_versioned_inc_comments_native").string();

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

TEST_CASE("compiles and runs duplicate includes once") {
  const std::filesystem::path includeRoot =
      std::filesystem::temp_directory_path() / "primec_tests" / "include_root_duplicate";
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
      "include<\"/std/io\", version=\"1.2.0\">\n"
      "include<\"/std/io\", version=\"1.2.0\">\n"
      "[return<int>]\n"
      "main(){ return(helper()) }\n";
  const std::string srcPath = writeTemp("compile_duplicate_include.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_duplicate_inc_exe").string();
  const std::string nativePath =
      (std::filesystem::temp_directory_path() / "primec_duplicate_inc_native").string();

  const std::string compileCppCmd = "./primec --emit=exe " + srcPath + " -o " + exePath +
                                    " --entry /main --include-path " + includeRoot.string();
  CHECK(runCommand(compileCppCmd) == 0);
  CHECK(runCommand(exePath) == 9);

  const std::string runVmCmd =
      "./primec --emit=vm " + srcPath + " --entry /main --include-path " + includeRoot.string();
  CHECK(runCommand(runVmCmd) == 9);

  const std::string compileNativeCmd = "./primec --emit=native " + srcPath + " -o " + nativePath +
                                       " --entry /main --include-path " + includeRoot.string();
  CHECK(runCommand(compileNativeCmd) == 0);
  CHECK(runCommand(nativePath) == 9);
}

TEST_CASE("compiles and runs versioned include with single quotes") {
  const std::filesystem::path includeRoot =
      std::filesystem::temp_directory_path() / "primec_tests" / "include_root_versioned_single_quotes";
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
      "include< '/std/io' , version = '1.2' >\n"
      "[return<int>]\n"
      "main(){ return(helper()) }\n";
  const std::string srcPath = writeTemp("compile_versioned_include_single_quotes.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() / "primec_versioned_inc_single_quotes_exe").string();
  const std::string nativePath =
      (std::filesystem::temp_directory_path() / "primec_versioned_inc_single_quotes_native").string();

  const std::string compileCppCmd = "./primec --emit=exe " + srcPath + " -o " + exePath +
                                    " --entry /main --include-path " + includeRoot.string();
  CHECK(runCommand(compileCppCmd) == 0);
  CHECK(runCommand(exePath) == 8);

  const std::string runVmCmd =
      "./primec --emit=vm " + srcPath + " --entry /main --include-path " + includeRoot.string();
  CHECK(runCommand(runVmCmd) == 8);

  const std::string compileNativeCmd = "./primec --emit=native " + srcPath + " -o " + nativePath +
                                       " --entry /main --include-path " + includeRoot.string();
  CHECK(runCommand(compileNativeCmd) == 0);
  CHECK(runCommand(nativePath) == 8);
}

TEST_CASE("compiles and runs versioned include with major-only selector") {
  const std::filesystem::path includeRoot =
      std::filesystem::temp_directory_path() / "primec_tests" / "include_root_versioned_major_only";
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
      "include<\"/std/io\", version=\"1\">\n"
      "[return<int>]\n"
      "main(){ return(helper()) }\n";
  const std::string srcPath = writeTemp("compile_versioned_include_major_only.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() / "primec_versioned_inc_major_only_exe").string();
  const std::string nativePath =
      (std::filesystem::temp_directory_path() / "primec_versioned_inc_major_only_native").string();

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

TEST_CASE("compiles and runs versioned include with minor selector") {
  const std::filesystem::path includeRoot =
      std::filesystem::temp_directory_path() / "primec_tests" / "include_root_versioned_minor_only";
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
      "include<\"/std/io\", version=\"1.2\">\n"
      "[return<int>]\n"
      "main(){ return(helper()) }\n";
  const std::string srcPath = writeTemp("compile_versioned_include_minor_only.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() / "primec_versioned_inc_minor_only_exe").string();
  const std::string nativePath =
      (std::filesystem::temp_directory_path() / "primec_versioned_inc_minor_only_native").string();

  const std::string compileCppCmd = "./primec --emit=exe " + srcPath + " -o " + exePath +
                                    " --entry /main --include-path " + includeRoot.string();
  CHECK(runCommand(compileCppCmd) == 0);
  CHECK(runCommand(exePath) == 8);

  const std::string runVmCmd =
      "./primec --emit=vm " + srcPath + " --entry /main --include-path " + includeRoot.string();
  CHECK(runCommand(runVmCmd) == 8);

  const std::string compileNativeCmd = "./primec --emit=native " + srcPath + " -o " + nativePath +
                                       " --entry /main --include-path " + includeRoot.string();
  CHECK(runCommand(compileNativeCmd) == 0);
  CHECK(runCommand(nativePath) == 8);
}

TEST_CASE("compiles and runs versioned include expansion with multiple include entries") {
  const std::filesystem::path includeRoot =
      std::filesystem::temp_directory_path() / "primec_tests" / "include_root_versioned_multi";
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
      "include</std/io, /std/math, version=\"1.2\">\n"
      "[return<int>]\n"
      "main(){ return(plus(io_helper(), math_helper())) }\n";
  const std::string srcPath = writeTemp("compile_versioned_include_multi.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() / "primec_versioned_inc_multi_exe").string();
  const std::string nativePath =
      (std::filesystem::temp_directory_path() / "primec_versioned_inc_multi_native").string();

  const std::string compileCppCmd = "./primec --emit=exe " + srcPath + " -o " + exePath +
                                    " --entry /main --include-path " + includeRoot.string();
  CHECK(runCommand(compileCppCmd) == 0);
  CHECK(runCommand(exePath) == 11);

  const std::string runVmCmd =
      "./primec --emit=vm " + srcPath + " --entry /main --include-path " + includeRoot.string();
  CHECK(runCommand(runVmCmd) == 11);

  const std::string compileNativeCmd = "./primec --emit=native " + srcPath + " -o " + nativePath +
                                       " --entry /main --include-path " + includeRoot.string();
  CHECK(runCommand(compileNativeCmd) == 0);
  CHECK(runCommand(nativePath) == 11);
}

TEST_CASE("rejects versioned include mismatch across roots") {
  const std::filesystem::path includeRootA =
      std::filesystem::temp_directory_path() / "primec_tests" / "include_root_version_mismatch_a";
  const std::filesystem::path includeRootB =
      std::filesystem::temp_directory_path() / "primec_tests" / "include_root_version_mismatch_b";
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
      "include</std/io, /std/math, version=\"1.2\">\n"
      "[return<int>]\n"
      "main(){ return(0i32) }\n";
  const std::string srcPath = writeTemp("compile_versioned_include_mismatch.prime", source);
  const std::string errPath =
      (std::filesystem::temp_directory_path() / "primec_versioned_include_mismatch_err.txt").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath +
                                 " -o /dev/null --entry /main --include-path " + includeRootA.string() +
                                 " --include-path " + includeRootB.string() + " 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("include version mismatch") != std::string::npos);
}

TEST_CASE("rejects missing versioned include in compile") {
  const std::filesystem::path includeRoot =
      std::filesystem::temp_directory_path() / "primec_tests" / "include_root_version_missing_compile";
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
      "include</std/io, version=\"2.0.0\">\n"
      "[return<int>]\n"
      "main(){ return(helper()) }\n";
  const std::string srcPath = writeTemp("compile_versioned_include_missing.prime", source);
  const std::string errPath =
      (std::filesystem::temp_directory_path() / "primec_versioned_include_missing_err.txt").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o /dev/null --entry /main --include-path " +
                                 includeRoot.string() + " 2> " + errPath;
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(errPath).find("Include error: include version not found") != std::string::npos);
}

TEST_CASE("compiles and runs versioned include expansion with mixed quoted and relative entries") {
  const std::filesystem::path includeRoot =
      std::filesystem::temp_directory_path() / "primec_tests" / "include_root_versioned_mixed";
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
      "include<\"/std/io\", \"./local/helpers\", version=\"1.2\">\n"
      "[return<int>]\n"
      "main(){ return(plus(io_helper(), local_helper())) }\n";
  const std::string srcPath = writeTemp("compile_versioned_include_mixed.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() / "primec_versioned_inc_mixed_exe").string();
  const std::string nativePath =
      (std::filesystem::temp_directory_path() / "primec_versioned_inc_mixed_native").string();

  const std::string compileCppCmd = "./primec --emit=exe " + srcPath + " -o " + exePath +
                                    " --entry /main --include-path " + includeRoot.string();
  CHECK(runCommand(compileCppCmd) == 0);
  CHECK(runCommand(exePath) == 11);

  const std::string runVmCmd =
      "./primec --emit=vm " + srcPath + " --entry /main --include-path " + includeRoot.string();
  CHECK(runCommand(runVmCmd) == 11);

  const std::string compileNativeCmd = "./primec --emit=native " + srcPath + " -o " + nativePath +
                                       " --entry /main --include-path " + includeRoot.string();
  CHECK(runCommand(compileNativeCmd) == 0);
  CHECK(runCommand(nativePath) == 11);
}

TEST_CASE("compiles and runs archive include expansion") {
  if (!hasZipTools()) {
    return;
  }
  const std::filesystem::path includeRoot =
      std::filesystem::temp_directory_path() / "primec_tests" / "include_root_archive_compile_run";
  const std::filesystem::path archiveSource = includeRoot / "archive_src";
  std::filesystem::remove_all(includeRoot);
  std::filesystem::create_directories(archiveSource);

  std::filesystem::create_directories(archiveSource / "1.2.0" / "std" / "io");
  {
    std::ofstream libFile(archiveSource / "1.2.0" / "std" / "io" / "lib.prime");
    CHECK(libFile.good());
    libFile << "[return<int>]\nhelper(){ return(5i32) }\n";
    CHECK(libFile.good());
  }
  const std::filesystem::path archivePath = includeRoot / "std_io.zip";
  CHECK(createZip(archivePath, archiveSource));

  const std::string source =
      "include</std/io, version=\"1.2\">\n"
      "[return<int>]\n"
      "main(){ return(helper()) }\n";
  const std::string srcPath = writeTemp("compile_archive_include.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_archive_inc_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath +
                                 " --entry /main --include-path " + includeRoot.string();
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 5);
}

TEST_CASE("compiles and runs exact versioned archive include expansion") {
  if (!hasZipTools()) {
    return;
  }
  const std::filesystem::path includeRoot =
      std::filesystem::temp_directory_path() / "primec_tests" / "include_root_archive_compile_exact";
  const std::filesystem::path archiveSource = includeRoot / "archive_src";
  std::filesystem::remove_all(includeRoot);
  std::filesystem::create_directories(archiveSource);

  std::filesystem::create_directories(archiveSource / "1.2.0" / "std" / "io");
  {
    std::ofstream libFile(archiveSource / "1.2.0" / "std" / "io" / "lib.prime");
    CHECK(libFile.good());
    libFile << "[return<int>]\nhelper(){ return(5i32) }\n";
    CHECK(libFile.good());
  }
  std::filesystem::create_directories(archiveSource / "1.2.3" / "std" / "io");
  {
    std::ofstream libFile(archiveSource / "1.2.3" / "std" / "io" / "lib.prime");
    CHECK(libFile.good());
    libFile << "[return<int>]\nhelper(){ return(7i32) }\n";
    CHECK(libFile.good());
  }

  const std::filesystem::path archivePath = includeRoot / "std_io.zip";
  CHECK(createZip(archivePath, archiveSource));

  const std::string source =
      "include</std/io, version=\"1.2.0\">\n"
      "[return<int>]\n"
      "main(){ return(helper()) }\n";
  const std::string srcPath = writeTemp("compile_archive_include_exact.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() / "primec_archive_inc_exact_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath +
                                 " --entry /main --include-path " + includeRoot.string();
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 5);
}

TEST_CASE("compiles and runs newest archive include expansion") {
  if (!hasZipTools()) {
    return;
  }
  const std::filesystem::path includeRoot =
      std::filesystem::temp_directory_path() / "primec_tests" / "include_root_archive_compile_latest";
  const std::filesystem::path archiveSource = includeRoot / "archive_src";
  std::filesystem::remove_all(includeRoot);
  std::filesystem::create_directories(archiveSource);

  std::filesystem::create_directories(archiveSource / "1.2.0" / "std" / "io");
  {
    std::ofstream libFile(archiveSource / "1.2.0" / "std" / "io" / "lib.prime");
    CHECK(libFile.good());
    libFile << "[return<int>]\nhelper(){ return(5i32) }\n";
    CHECK(libFile.good());
  }
  std::filesystem::create_directories(archiveSource / "1.2.8" / "std" / "io");
  {
    std::ofstream libFile(archiveSource / "1.2.8" / "std" / "io" / "lib.prime");
    CHECK(libFile.good());
    libFile << "[return<int>]\nhelper(){ return(9i32) }\n";
    CHECK(libFile.good());
  }

  const std::filesystem::path archivePath = includeRoot / "std_io.zip";
  CHECK(createZip(archivePath, archiveSource));

  const std::string source =
      "include</std/io, version=\"1.2\">\n"
      "[return<int>]\n"
      "main(){ return(helper()) }\n";
  const std::string srcPath = writeTemp("compile_archive_include_latest.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() / "primec_archive_inc_latest_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath +
                                 " --entry /main --include-path " + includeRoot.string();
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 9);
}
