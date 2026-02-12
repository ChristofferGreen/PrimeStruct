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

TEST_CASE("compiles and runs include inside namespace") {
  const std::string libPath =
      writeTemp("compile_namespace_lib.prime", "[return<int>]\nhelper(){ return(9i32) }\n");
  const std::string source =
      "namespace outer {\n"
      "  include<\"" + libPath + "\">\n"
      "}\n"
      "[return<int>]\n"
      "main(){ return(/outer/helper()) }\n";
  const std::string srcPath = writeTemp("compile_namespace_include.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_namespace_inc_exe").string();
  const std::string nativePath =
      (std::filesystem::temp_directory_path() / "primec_namespace_inc_native").string();

  const std::string compileCppCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCppCmd) == 0);
  CHECK(runCommand(exePath) == 9);

  const std::string runVmCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runVmCmd) == 9);

  const std::string compileNativeCmd =
      "./primec --emit=native " + srcPath + " -o " + nativePath + " --entry /main";
  CHECK(runCommand(compileNativeCmd) == 0);
  CHECK(runCommand(nativePath) == 9);
}

TEST_CASE("compiles and runs include with import aliases") {
  const std::string libSource = R"(
namespace lib {
  [return<int>]
  add([i32] a, [i32] b) {
    return(plus(a, b))
  }
}
)";
  const std::string libPath = writeTemp("compile_lib_imports.prime", libSource);
  const std::string source =
      "include<\"" + libPath + "\">\n"
      "import /lib\n"
      "[return<int>]\n"
      "main(){ return(add(4i32, 3i32)) }\n";
  const std::string srcPath = writeTemp("compile_include_imports.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_inc_imports_exe").string();
  const std::string nativePath = (std::filesystem::temp_directory_path() / "primec_inc_imports_native").string();

  const std::string compileCppCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCppCmd) == 0);
  CHECK(runCommand(exePath) == 7);

  const std::string runVmCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runVmCmd) == 7);

  const std::string compileNativeCmd =
      "./primec --emit=native " + srcPath + " -o " + nativePath + " --entry /main";
  CHECK(runCommand(compileNativeCmd) == 0);
  CHECK(runCommand(nativePath) == 7);
}

TEST_CASE("compiles and runs block expression with multiline body") {
  const std::string source = R"(
[return<int>]
main() {
  return(block(){
    [i32] x{1i32}
    plus(x, 2i32)
  })
}
)";
  const std::string srcPath = writeTemp("compile_block_expr_multiline.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_block_expr_multiline_exe").string();
  const std::string nativePath =
      (std::filesystem::temp_directory_path() / "primec_block_expr_multiline_native").string();

  const std::string compileCppCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCppCmd) == 0);
  CHECK(runCommand(exePath) == 3);

  const std::string runVmCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runVmCmd) == 3);

  const std::string compileNativeCmd = "./primec --emit=native " + srcPath + " -o " + nativePath + " --entry /main";
  CHECK(runCommand(compileNativeCmd) == 0);
  CHECK(runCommand(nativePath) == 3);
}

TEST_CASE("compiles and runs block binding inference for method calls") {
  const std::string source = R"(
namespace i64 {
  [return<i64>]
  inc([i64] self) {
    return(plus(self, 1i64))
  }
}

[return<int>]
main() {
  return(convert<i32>(block(){
    [mut] value{plus(1i64, 2i64)}
    value.inc()
  }))
}
)";
  const std::string srcPath = writeTemp("compile_block_binding_infer_method.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() / "primec_block_binding_infer_method_exe").string();
  const std::string nativePath =
      (std::filesystem::temp_directory_path() / "primec_block_binding_infer_method_native").string();

  const std::string compileCppCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCppCmd) == 0);
  CHECK(runCommand(exePath) == 4);

  const std::string runVmCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runVmCmd) == 4);

  const std::string compileNativeCmd = "./primec --emit=native " + srcPath + " -o " + nativePath + " --entry /main";
  CHECK(runCommand(compileNativeCmd) == 0);
  CHECK(runCommand(nativePath) == 4);
}

TEST_CASE("compiles and runs block binding inference for mixed if numeric types") {
  const std::string source = R"(
[return<int> effects(io_out)]
main() {
  print_line(block(){
    [mut] value{if(false, 1i32, 5000000000i64)}
    value
  })
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_block_infer_if_numeric.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_block_infer_if_numeric_exe").string();
  const std::string outPath =
      (std::filesystem::temp_directory_path() / "primec_block_infer_if_numeric_out.txt").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath + " > " + outPath) == 0);
  CHECK(readFile(outPath) == "5000000000\n");
}

TEST_CASE("compiles and runs operator rewrite") {
  const std::string source = R"(
[return<int>]
main() {
  return(1i32+2i32)
}
)";
  const std::string srcPath = writeTemp("compile_ops.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_ops_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 3);
}

TEST_CASE("compiles and runs operator rewrite with calls") {
  const std::string source = R"(
[return<int>]
helper() {
  return(5i32)
}

[return<int>]
main() {
  return(helper()+2i32)
}
)";
  const std::string srcPath = writeTemp("compile_ops_call.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_ops_call_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 7);
}

TEST_CASE("compiles and runs operator rewrite with parentheses") {
  const std::string source = R"(
[return<int>]
main() {
  return((1i32+2i32)*3i32)
}
)";
  const std::string srcPath = writeTemp("compile_ops_paren.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_ops_paren_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 9);
}

TEST_CASE("compiles and runs operator rewrite with unary minus operand") {
  const std::string source = R"(
[return<int>]
main() {
  [i32] value{2i32}
  return(3i32+-value)
}
)";
  const std::string srcPath = writeTemp("compile_ops_unary_minus.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_ops_unary_minus_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 1);
}

TEST_CASE("rejects negate on u64") {
  const std::string source = R"(
[return<u64>]
main() {
  return(negate(2u64))
}
)";
  const std::string srcPath = writeTemp("compile_negate_u64.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_negate_u64_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 2);
}

TEST_CASE("compiles and runs short-circuit and") {
  const std::string source = R"(
[return<int>]
main() {
  [i32 mut] value{1i32}
  [i32 mut] witness{0i32}
  assign(value, and(equal(value, 0i32), assign(witness, 9i32)))
  return(witness)
}
)";
  const std::string srcPath = writeTemp("compile_and_short.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_and_short_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 0);
}

TEST_CASE("compiles and runs short-circuit or") {
  const std::string source = R"(
[return<int>]
main() {
  [i32 mut] value{1i32}
  [i32 mut] witness{0i32}
  assign(value, or(equal(value, 1i32), assign(witness, 9i32)))
  return(witness)
}
)";
  const std::string srcPath = writeTemp("compile_or_short.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_or_short_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 0);
}

TEST_CASE("compiles and runs numeric boolean ops") {
  const std::string source = R"(
[return<bool>]
main() {
  return(or(0i32, 2i32))
}
)";
  const std::string srcPath = writeTemp("compile_bool_numeric.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_bool_numeric_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 1);
}

TEST_CASE("compiles and runs convert<bool>") {
  const std::string source = R"(
[return<bool>]
main() {
  return(convert<bool>(0i32))
}
)";
  const std::string srcPath = writeTemp("compile_convert_bool.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_convert_bool_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 0);
}

TEST_CASE("compiles and runs convert<i64>") {
  const std::string source = R"(
[return<i64>]
main() {
  return(convert<i64>(9i64))
}
)";
  const std::string srcPath = writeTemp("compile_convert_i64.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_convert_i64_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 9);
}

TEST_CASE("compiles and runs convert<u64>") {
  const std::string source = R"(
[return<u64>]
main() {
  return(convert<u64>(10u64))
}
)";
  const std::string srcPath = writeTemp("compile_convert_u64.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_convert_u64_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 10);
}

TEST_CASE("compiles and runs convert<bool> from u64") {
  const std::string source = R"(
[return<bool>]
main() {
  return(convert<bool>(1u64))
}
)";
  const std::string srcPath = writeTemp("compile_convert_bool_u64.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_convert_bool_u64_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 1);
}

TEST_CASE("compiles and runs convert<bool> from negative i64") {
  const std::string source = R"(
[return<bool>]
main() {
  return(convert<bool>(-1i64))
}
)";
  const std::string srcPath = writeTemp("compile_convert_bool_i64_neg.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_convert_bool_i64_neg_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 1);
}

TEST_CASE("compiles and runs pointer helpers") {
  const std::string source = R"(
[return<int>]
main() {
  [i32] value{4i32}
  return(dereference(location(value)))
}
)";
  const std::string srcPath = writeTemp("compile_pointer_helpers.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_pointer_helpers_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 4);
}

TEST_CASE("compiles and runs pointer plus helper") {
  const std::string source = R"(
[return<int>]
main() {
  [i32] value{5i32}
  return(dereference(plus(location(value), 0i32)))
}
)";
  const std::string srcPath = writeTemp("compile_pointer_plus.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_pointer_plus_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 5);
}

TEST_CASE("compiles and runs pointer plus on reference") {
  const std::string source = R"(
[return<int>]
main() {
  [i32] value{8i32}
  [Reference<i32>] ref{location(value)}
  return(dereference(plus(location(ref), 0i32)))
}
)";
  const std::string srcPath = writeTemp("compile_pointer_plus_ref.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_pointer_plus_ref_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 8);
}

TEST_CASE("compiles and runs pointer minus helper") {
  const std::string source = R"(
[return<int>]
main() {
  [i32] first{4i32}
  [i32] second{9i32}
  return(dereference(minus(location(second), 0i32)))
}
)";
  const std::string srcPath = writeTemp("compile_pointer_minus.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_pointer_minus_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 9);
}

TEST_CASE("compiles and runs pointer minus u64 offset") {
  const std::string source = R"(
[return<int>]
main() {
  [i32] value{5i32}
  return(dereference(minus(location(value), 0u64)))
}
)";
  const std::string srcPath = writeTemp("compile_pointer_minus_u64.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_pointer_minus_u64_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 5);
}

TEST_CASE("compiles and runs collection literals in C++ emitter") {
  const std::string source = R"(
[return<int>]
main() {
  array<i32>{1i32, 2i32, 3i32}
  map<i32, i32>{1i32=10i32, 2i32=20i32}
  return(1i32)
}
)";
  const std::string srcPath = writeTemp("compile_collections_exe.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_collections_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 1);
}

TEST_CASE("compiles and runs string-keyed map literals in C++ emitter") {
  const std::string source = R"(
[return<int>]
main() {
  map<string, i32>{"a"utf8=1i32, "b"utf8=2i32}
  return(1i32)
}
)";
  const std::string srcPath = writeTemp("compile_collections_string_map.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() / "primec_collections_string_map_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 1);
}

TEST_CASE("compiles and runs string-keyed map literals with bracket sugar in C++ emitter") {
  const std::string source = R"(
[return<int>]
main() {
  map<string, i32>["a"=1i32, "b"=2i32]
  return(1i32)
}
)";
  const std::string srcPath = writeTemp("compile_collections_string_map_brackets.prime", source);
  const std::string exePath =
      (std::filesystem::temp_directory_path() / "primec_collections_string_map_brackets_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 1);
}

TEST_CASE("compiles with executions using collection arguments") {
  const std::string source = R"(
[return<int>]
main() {
  return(1i32)
}

execute_task([i32] items, [i32] pairs) {
  return(1i32)
}

execute_task([items] array<i32>(1i32, 2i32), [pairs] map<i32, i32>(1i32, 2i32)) { }
)";
  const std::string srcPath = writeTemp("compile_exec_collections.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_exec_collections_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 1);
}

TEST_CASE("compiles with execution body arguments") {
  const std::string source = R"(
[return<int>]
main() {
  return(1i32)
}

execute_repeat([i32] count) {
  return(1i32)
}

execute_repeat(2i32) {
  main(),
  main()
}
)";
  const std::string srcPath = writeTemp("compile_exec_body.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_exec_body_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 1);
}

TEST_CASE("compiles and runs pointer plus u64 offset") {
  const std::string source = R"(
[return<int>]
main() {
  [i32] value{5i32}
  return(dereference(plus(location(value), 0u64)))
}
)";
  const std::string srcPath = writeTemp("compile_pointer_plus_u64.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_pointer_plus_u64_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 5);
}

TEST_CASE("compiles and runs i64 literals") {
  const std::string source = R"(
[return<i64>]
main() {
  return(9i64)
}
)";
  const std::string srcPath = writeTemp("compile_i64_literal.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_i64_literal_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 9);
}

TEST_CASE("compiles and runs u64 literals") {
  const std::string source = R"(
[return<u64>]
main() {
  return(10u64)
}
)";
  const std::string srcPath = writeTemp("compile_u64_literal.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_u64_literal_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 10);
}

TEST_CASE("compiles and runs assignment operator rewrite") {
  const std::string source = R"(
[return<int>]
main() {
  [i32 mut] value{1i32}
  value=2i32
  return(value)
}
)";
  const std::string srcPath = writeTemp("compile_assign_op.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_assign_op_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 2);
}

TEST_CASE("compiles and runs comparison operator rewrite") {
  const std::string source = R"(
[return<bool>]
main() {
  return(2i32>1i32)
}
)";
  const std::string srcPath = writeTemp("compile_gt_op.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_gt_op_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 1);
}

TEST_CASE("compiles and runs less_than operator rewrite") {
  const std::string source = R"(
[return<bool>]
main() {
  return(1i32<2i32)
}
)";
  const std::string srcPath = writeTemp("compile_lt_op.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_lt_op_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 1);
}

TEST_CASE("compiles and runs greater_equal operator rewrite") {
  const std::string source = R"(
[return<bool>]
main() {
  return(2i32>=2i32)
}
)";
  const std::string srcPath = writeTemp("compile_ge_op.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_ge_op_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 1);
}

TEST_CASE("compiles and runs less_equal operator rewrite") {
  const std::string source = R"(
[return<bool>]
main() {
  return(2i32<=2i32)
}
)";
  const std::string srcPath = writeTemp("compile_le_op.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_le_op_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 1);
}

TEST_CASE("compiles and runs and operator rewrite") {
  const std::string source = R"(
[return<bool>]
main() {
  return(1i32&&1i32)
}
)";
  const std::string srcPath = writeTemp("compile_and_op.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_and_op_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 1);
}

TEST_CASE("compiles and runs or operator rewrite") {
  const std::string source = R"(
[return<bool>]
main() {
  return(0i32||1i32)
}
)";
  const std::string srcPath = writeTemp("compile_or_op.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_or_op_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 1);
}

TEST_CASE("compiles and runs not operator rewrite") {
  const std::string source = R"(
[return<bool>]
main() {
  return(!0i32)
}
)";
  const std::string srcPath = writeTemp("compile_not_op.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_not_op_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 1);
}

TEST_CASE("compiles and runs not operator with parentheses") {
  const std::string source = R"(
[return<bool>]
main() {
  return(!(0i32))
}
)";
  const std::string srcPath = writeTemp("compile_not_paren.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_not_paren_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 1);
}

TEST_CASE("compiles and runs unary minus operator rewrite") {
  const std::string source = R"(
[return<int>]
main() {
  [i32] value{3i32}
  return(plus(-value, 5i32))
}
)";
  const std::string srcPath = writeTemp("compile_unary_minus.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_unary_minus_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 2);
}

TEST_CASE("compiles and runs equality operator rewrite") {
  const std::string source = R"(
[return<bool>]
main() {
  return(2i32==2i32)
}
)";
  const std::string srcPath = writeTemp("compile_eq_op.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_eq_op_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 1);
}

TEST_CASE("compiles and runs not_equal operator rewrite") {
  const std::string source = R"(
[return<bool>]
main() {
  return(2i32!=3i32)
}
)";
  const std::string srcPath = writeTemp("compile_neq_op.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_neq_op_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 1);
}
