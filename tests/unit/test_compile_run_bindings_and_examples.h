TEST_SUITE_BEGIN("primestruct.compile.run.bindings");

TEST_CASE("compiles and runs void main") {
  const std::string source = R"(
[return<void>]
main() {
}
)";
  const std::string srcPath = writeTemp("compile_void.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_void_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 0);
}

TEST_CASE("compiles and runs local binding") {
  const std::string source = R"(
[return<int>]
main() {
  [i32] value{5i32}
  return(value)
}
)";
  const std::string srcPath = writeTemp("compile_binding.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_binding_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 5);
}

TEST_CASE("compiles with struct definition") {
  const std::string source = R"(
[struct]
data() {
  [i32] value{1i32}
}

[return<int>]
main() {
  return(7i32)
}
)";
  const std::string srcPath = writeTemp("compile_struct.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_struct_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 7);
}

TEST_CASE("compiles and runs assign to mutable binding") {
  const std::string source = R"(
[return<int>]
main() {
  [i32 mut] value{1i32}
  assign(value, 6i32)
  return(value)
}
)";
  const std::string srcPath = writeTemp("compile_assign.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_assign_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 6);
}

TEST_CASE("compiles and runs assign to reference") {
  const std::string source = R"(
[return<int>]
main() {
  [i32 mut] value{1i32}
  [Reference<i32> mut] ref{location(value)}
  assign(ref, 9i32)
  return(value)
}
)";
  const std::string srcPath = writeTemp("compile_assign_ref.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_assign_ref_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 9);
}

TEST_CASE("compiles and runs location on reference") {
  const std::string source = R"(
[return<int>]
main() {
  [i32 mut] value{8i32}
  [Reference<i32> mut] ref{location(value)}
  [Pointer<i32>] ptr{location(ref)}
  return(dereference(ptr))
}
)";
  const std::string srcPath = writeTemp("compile_ref_location.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_ref_location_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 8);
}

TEST_CASE("compiles and runs reference arithmetic") {
  const std::string source = R"(
[return<int>]
main() {
  [i32 mut] value{4i32}
  [Reference<i32> mut] ref{location(value)}
  assign(ref, plus(ref, 3i32))
  return(ref)
}
)";
  const std::string srcPath = writeTemp("compile_ref_arith.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_ref_arith_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 7);
}

TEST_CASE("compiles and runs array reference helpers") {
  const std::string source = R"(
[return<int>]
main() {
  [array<i32>] values{array<i32>(2i32, 7i32)}
  [Reference<array<i32>>] ref{location(values)}
  return(plus(count(ref), at(ref, 1i32)))
}
)";
  const std::string srcPath = writeTemp("compile_array_ref_helpers.prime", source);
  const std::string exePath = (std::filesystem::temp_directory_path() / "primec_array_ref_helpers_exe").string();

  const std::string compileCmd = "./primec --emit=exe " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 9);
}

TEST_SUITE_END();

TEST_SUITE_BEGIN("primestruct.compile.run.examples");

TEST_CASE("compiles examples to IR") {
  const std::filesystem::path examplesDir = std::filesystem::path("..") / "examples";
  REQUIRE(std::filesystem::exists(examplesDir));

  const std::filesystem::path outDir = std::filesystem::temp_directory_path() / "primec_examples_ir";
  std::error_code ec;
  std::filesystem::remove_all(outDir, ec);
  std::filesystem::create_directories(outDir, ec);
  REQUIRE(!ec);

  std::vector<std::filesystem::path> exampleFiles;
  for (const auto &entry : std::filesystem::recursive_directory_iterator(examplesDir)) {
    if (!entry.is_regular_file()) {
      continue;
    }
    const std::filesystem::path path = entry.path();
    if (path.extension() != ".prime") {
      continue;
    }
    if (path.string().find("borrow_checker_negative") != std::string::npos) {
      continue;
    }
    if (path.filename() == "result_helpers.prime") {
      continue;
    }
    exampleFiles.push_back(path);
  }
  std::sort(exampleFiles.begin(), exampleFiles.end());
  REQUIRE(!exampleFiles.empty());

  for (const auto &path : exampleFiles) {
    const std::string compileCmd =
        "./primec --emit=ir " + quoteShellArg(path.string()) + " --out-dir " + quoteShellArg(outDir.string()) +
        " --entry /main";
    CHECK(runCommand(compileCmd) == 0);
    CHECK(std::filesystem::exists(outDir / (path.stem().string() + ".psir")));
  }
}

TEST_CASE("borrow checker negative examples fail with expected diagnostics") {
  const std::filesystem::path examplesDir = std::filesystem::path("..") / "examples" / "borrow_checker_negative";
  REQUIRE(std::filesystem::exists(examplesDir));

  std::vector<std::filesystem::path> exampleFiles;
  for (const auto &entry : std::filesystem::directory_iterator(examplesDir)) {
    if (!entry.is_regular_file()) {
      continue;
    }
    if (entry.path().extension() != ".prime") {
      continue;
    }
    exampleFiles.push_back(entry.path());
  }
  std::sort(exampleFiles.begin(), exampleFiles.end());
  REQUIRE(!exampleFiles.empty());

  for (const auto &path : exampleFiles) {
    std::filesystem::path expectedPath = path;
    expectedPath.replace_extension(".expected.txt");
    REQUIRE(std::filesystem::exists(expectedPath));

    const std::string expectedContents = readFile(expectedPath.string());
    std::istringstream expectedLines(expectedContents);
    std::vector<std::string> expectedFragments;
    std::string line;
    while (std::getline(expectedLines, line)) {
      if (line.empty() || line[0] == '#') {
        continue;
      }
      expectedFragments.push_back(line);
    }
    REQUIRE(!expectedFragments.empty());

    const std::string primecErrPath =
        (std::filesystem::temp_directory_path() / ("primec_borrow_checker_negative_" + path.stem().string() + ".json"))
            .string();
    const std::string primevmErrPath = (std::filesystem::temp_directory_path() /
                                        ("primevm_borrow_checker_negative_" + path.stem().string() + ".json"))
                                           .string();

    const std::string primecCmd = "./primec --emit=exe " + quoteShellArg(path.string()) +
                                  " -o /dev/null --entry /main --emit-diagnostics 2> " + quoteShellArg(primecErrPath);
    CHECK(runCommand(primecCmd) == 2);
    const std::string primecDiagnostics = readFile(primecErrPath);
    CHECK(primecDiagnostics.find("\"version\":1") != std::string::npos);
    CHECK(primecDiagnostics.find("\"severity\":\"error\"") != std::string::npos);
    for (const auto &fragment : expectedFragments) {
      CHECK(primecDiagnostics.find(fragment) != std::string::npos);
    }
    CHECK(primecDiagnostics.find("Semantic error: ") == std::string::npos);

    const std::string primevmCmd =
        "./primevm " + quoteShellArg(path.string()) + " --entry /main --emit-diagnostics 2> " + quoteShellArg(primevmErrPath);
    CHECK(runCommand(primevmCmd) == 2);
    const std::string primevmDiagnostics = readFile(primevmErrPath);
    CHECK(primevmDiagnostics.find("\"version\":1") != std::string::npos);
    CHECK(primevmDiagnostics.find("\"severity\":\"error\"") != std::string::npos);
    for (const auto &fragment : expectedFragments) {
      CHECK(primevmDiagnostics.find(fragment) != std::string::npos);
    }
    CHECK(primevmDiagnostics.find("Semantic error: ") == std::string::npos);
  }
}

TEST_SUITE_END();
