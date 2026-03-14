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
