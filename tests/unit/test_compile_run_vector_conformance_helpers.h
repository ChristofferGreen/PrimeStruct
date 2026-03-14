#pragma once

inline std::string vectorHelperConformanceImportSlug(const std::string &importPath) {
  if (importPath == "/std/collections/*") {
    return "stdlib";
  }
  if (importPath == "/std/collections/experimental_vector/*") {
    return "experimental";
  }
  return "custom";
}

inline bool isExperimentalVectorImport(const std::string &importPath) {
  return importPath == "/std/collections/experimental_vector/*";
}

inline std::string vectorConformanceType(const std::string &importPath,
                                         const std::string &valueType) {
  if (isExperimentalVectorImport(importPath)) {
    return "Vector<" + valueType + ">";
  }
  return "vector<" + valueType + ">";
}

inline std::string makeVectorHelperSurfaceConformanceSource(const std::string &importPath) {
  std::string source;
  source += "import " + importPath + "\n\n";
  source += "[effects(heap_alloc), return<" + vectorConformanceType(importPath, "T") + ">]\n";
  source += "wrapVector<T>([T] first, [T] second, [T] third, [T] fourth) {\n";
  source += "  return(vectorQuad<T>(first, second, third, fourth))\n";
  source += "}\n\n";
  source += "[effects(heap_alloc), return<int>]\n";
  source += "main() {\n";
  source += "  [" + vectorConformanceType(importPath, "i32") + "] empty{vectorNew<i32>()}\n";
  source += "  [" + vectorConformanceType(importPath, "i32") + "] pair{vectorPair<i32>(11i32, 13i32)}\n";
  source += "  [" + vectorConformanceType(importPath, "i32") +
            "] wrapped{wrapVector<i32>(1i32, 3i32, 5i32, 7i32)}\n";
  source += "  return(plus(plus(vectorCount<i32>(empty), vectorCount<i32>(pair)),\n";
  source += "      plus(vectorCapacity<i32>(pair), plus(vectorAt<i32>(pair, 1i32),\n";
  source += "          plus(vectorAtUnsafe<i32>(pair, 0i32), plus(vectorAt<i32>(wrapped, 2i32),\n";
  source += "              vectorAtUnsafe<i32>(wrapped, 3i32)))))))\n";
  source += "}\n";
  return source;
}

inline std::string makeVectorExtendedConstructorConformanceSource(const std::string &importPath) {
  std::string source;
  source += "import " + importPath + "\n\n";
  source += "[effects(heap_alloc), return<int>]\n";
  source += "main() {\n";
  source += "  [" + vectorConformanceType(importPath, "i32") +
            "] values{vectorOct<i32>(2i32, 4i32, 6i32, 8i32, 10i32, 12i32, 14i32, 16i32)}\n";
  source += "  return(plus(plus(vectorCount<i32>(values), vectorCapacity<i32>(values)),\n";
  source += "      plus(vectorAt<i32>(values, 6i32), vectorAtUnsafe<i32>(values, 0i32))))\n";
  source += "}\n";
  return source;
}

inline std::string makeVectorGrowthConformanceSource(const std::string &importPath) {
  std::string source;
  source += "import " + importPath + "\n\n";
  source += "[effects(heap_alloc), return<int>]\n";
  source += "main() {\n";
  source += "  [" + vectorConformanceType(importPath, "i32") + " mut] values{vectorNew<i32>()}\n";
  source += "  vectorReserve<i32>(values, 2i32)\n";
  source += "  [i32] reserved{vectorCapacity<i32>(values)}\n";
  source += "  vectorPush<i32>(values, 11i32)\n";
  source += "  vectorPush<i32>(values, 22i32)\n";
  source += "  vectorPush<i32>(values, 33i32)\n";
  source += "  return(plus(plus(reserved, vectorCount<i32>(values)),\n";
  source += "      plus(vectorAt<i32>(values, 0i32), plus(vectorAtUnsafe<i32>(values, 1i32),\n";
  source += "          vectorAt<i32>(values, 2i32)))))\n";
  source += "}\n";
  return source;
}

inline std::string makeVectorShrinkRemoveConformanceSource(const std::string &importPath) {
  std::string source;
  source += "import " + importPath + "\n\n";
  source += "[effects(heap_alloc), return<int>]\n";
  source += "main() {\n";
  source += "  [" + vectorConformanceType(importPath, "i32") + " mut] values{vectorQuad<i32>(10i32, 20i32, 30i32, 40i32)}\n";
  source += "  [i32 mut] total{0i32}\n";
  source += "  vectorPop<i32>(values)\n";
  source += "  assign(total, plus(total, plus(vectorCount<i32>(values), vectorAt<i32>(values, 1i32))))\n";
  source += "  vectorRemoveAt<i32>(values, 1i32)\n";
  source += "  assign(total, plus(total, plus(vectorCount<i32>(values), vectorAtUnsafe<i32>(values, 1i32))))\n";
  source += "  vectorRemoveSwap<i32>(values, 0i32)\n";
  source += "  assign(total, plus(total, plus(vectorCount<i32>(values), vectorAt<i32>(values, 0i32))))\n";
  source += "  vectorClear<i32>(values)\n";
  source += "  return(plus(total, vectorCount<i32>(values)))\n";
  source += "}\n";
  return source;
}

inline std::string makeVectorTypeMismatchRejectSource(const std::string &importPath) {
  std::string source;
  source += "import " + importPath + "\n\n";
  source += "[effects(heap_alloc), return<int>]\n";
  source += "main() {\n";
  source += "  [" + vectorConformanceType(importPath, "i32") + "] values{vectorPair<i32>(1i32, false)}\n";
  source += "  return(vectorCount<i32>(values))\n";
  source += "}\n";
  return source;
}

inline std::string makeVectorPopTypeMismatchRejectSource(const std::string &importPath) {
  std::string source;
  source += "import " + importPath + "\n\n";
  source += "[effects(heap_alloc), return<int>]\n";
  source += "main() {\n";
  source += "  [" + vectorConformanceType(importPath, "i32") + " mut] values{vectorSingle<i32>(9i32)}\n";
  source += "  vectorPop<bool>(values)\n";
  source += "  return(vectorCount<i32>(values))\n";
  source += "}\n";
  return source;
}

inline std::string makeVectorPushTypeMismatchRejectSource(const std::string &importPath) {
  std::string source;
  source += "import " + importPath + "\n\n";
  source += "[effects(heap_alloc), return<int>]\n";
  source += "main() {\n";
  source += "  [" + vectorConformanceType(importPath, "i32") + " mut] values{vectorNew<i32>()}\n";
  source += "  vectorPush<bool>(values, true)\n";
  source += "  return(vectorCount<i32>(values))\n";
  source += "}\n";
  return source;
}

inline std::string makeVectorHelperRuntimeContractSource(const std::string &importPath,
                                                         const std::string &mode) {
  std::string source;
  source += "import " + importPath + "\n\n";
  source += "[effects(heap_alloc), return<int>]\n";
  source += "main() {\n";
  if (mode == "pop_empty") {
    source += "  [" + vectorConformanceType(importPath, "i32") + " mut] values{vectorNew<i32>()}\n";
    source += "  vectorPop<i32>(values)\n";
  } else if (mode == "remove_at_oob") {
    source += "  [" + vectorConformanceType(importPath, "i32") + " mut] values{vectorSingle<i32>(4i32)}\n";
    source += "  vectorRemoveAt<i32>(values, 1i32)\n";
  } else {
    source += "  [" + vectorConformanceType(importPath, "i32") + " mut] values{vectorSingle<i32>(4i32)}\n";
    source += "  vectorRemoveSwap<i32>(values, 1i32)\n";
  }
  source += "  return(0i32)\n";
  source += "}\n";
  return source;
}

inline void expectVectorConformanceProgramRuns(const std::string &source,
                                               const std::string &nameStem,
                                               const std::string &emitMode,
                                               int expectedExitCode) {
  const std::string srcPath = writeTemp(nameStem + ".prime", source);
  if (emitMode == "vm") {
    const std::string runCmd =
        "./primec --emit=vm " + quoteShellArg(srcPath) + " --entry /main";
    CHECK(runCommand(runCmd) == expectedExitCode);
    return;
  }

  const std::string exePath =
      (std::filesystem::temp_directory_path() / (nameStem + "_exe")).string();
  const std::string compileCmd = "./primec --emit=" + emitMode + " " + quoteShellArg(srcPath) + " -o " +
                                 quoteShellArg(exePath) + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(quoteShellArg(exePath)) == expectedExitCode);
}

inline void expectVectorHelperSurfaceConformance(const std::string &emitMode,
                                                 const std::string &importPath) {
  const std::string slug = vectorHelperConformanceImportSlug(importPath);
  expectVectorConformanceProgramRuns(
      makeVectorHelperSurfaceConformanceSource(importPath),
      "vector_helper_surface_" + slug + "_" + emitMode,
      emitMode,
      40);
}

inline void expectVectorExtendedConstructorConformance(const std::string &emitMode,
                                                       const std::string &importPath) {
  const std::string slug = vectorHelperConformanceImportSlug(importPath);
  expectVectorConformanceProgramRuns(
      makeVectorExtendedConstructorConformanceSource(importPath),
      "vector_extended_ctor_" + slug + "_" + emitMode,
      emitMode,
      32);
}

inline void expectVectorGrowthConformance(const std::string &emitMode,
                                          const std::string &importPath) {
  const std::string slug = vectorHelperConformanceImportSlug(importPath);
  expectVectorConformanceProgramRuns(
      makeVectorGrowthConformanceSource(importPath),
      "vector_growth_" + slug + "_" + emitMode,
      emitMode,
      71);
}

inline void expectVectorShrinkRemoveConformance(const std::string &emitMode,
                                                const std::string &importPath) {
  const std::string slug = vectorHelperConformanceImportSlug(importPath);
  expectVectorConformanceProgramRuns(
      makeVectorShrinkRemoveConformanceSource(importPath),
      "vector_shrink_remove_" + slug + "_" + emitMode,
      emitMode,
      86);
}

inline void expectVectorTypeMismatchReject(const std::string &emitMode,
                                           const std::string &importPath) {
  const std::string slug = vectorHelperConformanceImportSlug(importPath);
  const std::string source = makeVectorTypeMismatchRejectSource(importPath);
  const std::string srcPath = writeTemp("vector_type_mismatch_" + slug + "_" + emitMode + ".prime", source);

  if (emitMode == "vm") {
    const std::string runCmd =
        "./primec --emit=vm " + quoteShellArg(srcPath) + " --entry /main";
    CHECK(runCommand(runCmd) == 2);
    return;
  }

  const std::string compileCmd = "./primec --emit=" + emitMode + " " + quoteShellArg(srcPath) +
                                 " -o /dev/null --entry /main";
  CHECK(runCommand(compileCmd) == 2);
}

inline void expectVectorPopTypeMismatchReject(const std::string &emitMode,
                                              const std::string &importPath) {
  const std::string slug = vectorHelperConformanceImportSlug(importPath);
  const std::string source = makeVectorPopTypeMismatchRejectSource(importPath);
  const std::string srcPath = writeTemp("vector_pop_type_mismatch_" + slug + "_" + emitMode + ".prime", source);

  if (emitMode == "vm") {
    const std::string runCmd =
        "./primec --emit=vm " + quoteShellArg(srcPath) + " --entry /main";
    CHECK(runCommand(runCmd) == 2);
    return;
  }

  const std::string compileCmd = "./primec --emit=" + emitMode + " " + quoteShellArg(srcPath) +
                                 " -o /dev/null --entry /main";
  CHECK(runCommand(compileCmd) == 2);
}

inline void expectVectorPushTypeMismatchReject(const std::string &emitMode,
                                               const std::string &importPath) {
  const std::string slug = vectorHelperConformanceImportSlug(importPath);
  const std::string source = makeVectorPushTypeMismatchRejectSource(importPath);
  const std::string srcPath = writeTemp("vector_push_type_mismatch_" + slug + "_" + emitMode + ".prime", source);

  if (emitMode == "vm") {
    const std::string runCmd =
        "./primec --emit=vm " + quoteShellArg(srcPath) + " --entry /main";
    CHECK(runCommand(runCmd) == 2);
    return;
  }

  const std::string compileCmd = "./primec --emit=" + emitMode + " " + quoteShellArg(srcPath) +
                                 " -o /dev/null --entry /main";
  CHECK(runCommand(compileCmd) == 2);
}

inline void expectVectorHelperRuntimeContract(const std::string &emitMode,
                                              const std::string &importPath,
                                              const std::string &mode) {
  const std::string slug = vectorHelperConformanceImportSlug(importPath);
  const std::string source = makeVectorHelperRuntimeContractSource(importPath, mode);
  const std::string srcPath = writeTemp("vector_helper_runtime_" + slug + "_" + mode + "_" + emitMode + ".prime", source);
  const std::string errPath =
      (std::filesystem::temp_directory_path() /
       ("primec_vector_helper_runtime_" + slug + "_" + mode + "_" + emitMode + "_err.txt"))
          .string();
  const std::string expectedError = mode == "pop_empty" ? "container empty\n" : "container index out of bounds\n";

  if (emitMode == "vm") {
    const std::string runCmd =
        "./primec --emit=vm " + quoteShellArg(srcPath) + " --entry /main 2> " + quoteShellArg(errPath);
    CHECK(runCommand(runCmd) == 3);
    CHECK(readFile(errPath) == expectedError);
    return;
  }

  const std::string exePath =
      (std::filesystem::temp_directory_path() /
       ("primec_vector_helper_runtime_" + slug + "_" + mode + "_" + emitMode + "_exe"))
          .string();
  const std::string compileCmd = "./primec --emit=" + emitMode + " " + quoteShellArg(srcPath) + " -o " +
                                 quoteShellArg(exePath) + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string runCmd = quoteShellArg(exePath) + " 2> " + quoteShellArg(errPath);
  CHECK(runCommand(runCmd) == 3);
  CHECK(readFile(errPath) == expectedError);
}

inline std::string makeVectorPopEmptyRuntimeContractSource(bool methodForm) {
  std::string source;
  source += "[effects(heap_alloc), return<int>]\n";
  source += "main() {\n";
  source += "  [vector<i32> mut] values{vector<i32>()}\n";
  source += methodForm ? "  values.pop()\n" : "  pop(values)\n";
  source += "  return(0i32)\n";
  source += "}\n";
  return source;
}

inline void expectVectorPopEmptyRuntimeContract(const std::string &emitMode,
                                                bool methodForm) {
  const std::string formSlug = methodForm ? "method" : "call";
  const std::string source = makeVectorPopEmptyRuntimeContractSource(methodForm);
  const std::string srcPath = writeTemp("vector_pop_empty_runtime_" + formSlug + "_" + emitMode + ".prime", source);
  const std::string errPath =
      (std::filesystem::temp_directory_path() / ("primec_vector_pop_empty_runtime_" + formSlug + "_" + emitMode +
                                                 "_err.txt"))
          .string();

  if (emitMode == "vm") {
    const std::string runCmd =
        "./primec --emit=vm " + quoteShellArg(srcPath) + " --entry /main 2> " + quoteShellArg(errPath);
    CHECK(runCommand(runCmd) == 3);
    CHECK(readFile(errPath) == "container empty\n");
    return;
  }

  const std::string exePath =
      (std::filesystem::temp_directory_path() / ("primec_vector_pop_empty_runtime_" + formSlug + "_" + emitMode +
                                                 "_exe"))
          .string();
  const std::string compileCmd = "./primec --emit=" + emitMode + " " + quoteShellArg(srcPath) + " -o " +
                                 quoteShellArg(exePath) + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string runCmd = quoteShellArg(exePath) + " 2> " + quoteShellArg(errPath);
  CHECK(runCommand(runCmd) == 3);
  CHECK(readFile(errPath) == "container empty\n");
}

inline std::string makeVectorIndexRuntimeContractSource(const std::string &mode) {
  std::string source;
  source += "[effects(heap_alloc), return<int>]\n";
  source += "main() {\n";
  const bool mutating = mode == "remove_at_call" || mode == "remove_at_method" ||
                        mode == "remove_swap_call" || mode == "remove_swap_method";
  source += mutating ? "  [vector<i32> mut] values{vector<i32>(4i32)}\n"
                     : "  [vector<i32>] values{vector<i32>(4i32)}\n";
  if (mode == "access_call") {
    source += "  return(at(values, 9i32))\n";
  } else if (mode == "access_method") {
    source += "  return(values.at(9i32))\n";
  } else if (mode == "access_bracket") {
    source += "  return(values[9i32])\n";
  } else if (mode == "remove_at_call") {
    source += "  remove_at(values, 1i32)\n";
    source += "  return(0i32)\n";
  } else if (mode == "remove_at_method") {
    source += "  values.remove_at(1i32)\n";
    source += "  return(0i32)\n";
  } else if (mode == "remove_swap_call") {
    source += "  remove_swap(values, 1i32)\n";
    source += "  return(0i32)\n";
  } else {
    source += "  values.remove_swap(1i32)\n";
    source += "  return(0i32)\n";
  }
  source += "}\n";
  return source;
}

inline void expectVectorIndexRuntimeContract(const std::string &emitMode,
                                             const std::string &mode) {
  const std::string source = makeVectorIndexRuntimeContractSource(mode);
  const std::string srcPath = writeTemp("vector_index_runtime_" + mode + "_" + emitMode + ".prime", source);
  const std::string errPath =
      (std::filesystem::temp_directory_path() / ("primec_vector_index_runtime_" + mode + "_" + emitMode +
                                                 "_err.txt"))
          .string();

  if (emitMode == "vm") {
    const std::string runCmd =
        "./primec --emit=vm " + quoteShellArg(srcPath) + " --entry /main 2> " + quoteShellArg(errPath);
    CHECK(runCommand(runCmd) == 3);
    CHECK(readFile(errPath) == "container index out of bounds\n");
    return;
  }

  const std::string exePath =
      (std::filesystem::temp_directory_path() / ("primec_vector_index_runtime_" + mode + "_" + emitMode + "_exe"))
          .string();
  const std::string compileCmd = "./primec --emit=" + emitMode + " " + quoteShellArg(srcPath) + " -o " +
                                 quoteShellArg(exePath) + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string runCmd = quoteShellArg(exePath) + " 2> " + quoteShellArg(errPath);
  CHECK(runCommand(runCmd) == 3);
  CHECK(readFile(errPath) == "container index out of bounds\n");
}
