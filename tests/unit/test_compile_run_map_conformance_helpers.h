#pragma once

inline std::string mapHelperConformanceImportSlug(const std::string &importPath) {
  if (importPath == "/std/collections/*") {
    return "stdlib";
  }
  if (importPath == "/std/collections/experimental_map/*") {
    return "experimental";
  }
  return "custom";
}

inline bool isExperimentalMapImport(const std::string &importPath) {
  return importPath == "/std/collections/experimental_map/*";
}

inline std::string mapConformanceType(const std::string &importPath,
                                      const std::string &keyType,
                                      const std::string &valueType) {
  if (isExperimentalMapImport(importPath)) {
    return "Map<" + keyType + ", " + valueType + ">";
  }
  return "map<" + keyType + ", " + valueType + ">";
}

inline std::string mapConformanceKeyType(const std::string &importPath) {
  return isExperimentalMapImport(importPath) ? "i32" : "string";
}

inline std::string mapConformanceLiteral(const std::string &importPath,
                                         const std::string &numericValue,
                                         const std::string &stringValue) {
  if (isExperimentalMapImport(importPath)) {
    return numericValue;
  }
  return stringValue;
}

inline std::string makeMapHelperSurfaceConformanceSource(const std::string &importPath) {
  const std::string keyType = mapConformanceKeyType(importPath);
  const std::string leftKey = mapConformanceLiteral(importPath, "11i32", "\"left\"raw_utf8");
  const std::string rightKey = mapConformanceLiteral(importPath, "22i32", "\"right\"raw_utf8");
  const std::string missingKey = mapConformanceLiteral(importPath, "99i32", "\"missing\"raw_utf8");

  std::string source;
  source += "import " + importPath + "\n\n";
  source += "[effects(heap_alloc), return<" + mapConformanceType(importPath, "K", "V") + "> Comparable<K>]\n";
  source += "wrapMap<K, V>([K] leftKey, [V] leftValue, [K] rightKey, [V] rightValue) {\n";
  source += "  return(mapPair<K, V>(leftKey, leftValue, rightKey, rightValue))\n";
  source += "}\n\n";
  source += "[effects(heap_alloc), return<int>]\n";
  source += "main() {\n";
  source += "  [" + mapConformanceType(importPath, keyType, "i32") + "] pairs{wrapMap<" + keyType +
            ", i32>(" + leftKey + ", 4i32, " + rightKey + ", 7i32)}\n";
  source += "  [i32 mut] total{plus(plus(mapCount<" + keyType + ", i32>(pairs), mapAt<" + keyType +
            ", i32>(pairs, " + leftKey + ")),\n";
  source += "                       mapAtUnsafe<" + keyType + ", i32>(pairs, " + rightKey + "))}\n";
  source += "  if(mapContains<" + keyType + ", i32>(pairs, " + leftKey + "),\n";
  source += "     then() { assign(total, plus(total, 1i32)) },\n";
  source += "     else() { })\n";
  source += "  if(not(mapContains<" + keyType + ", i32>(pairs, " + missingKey + ")),\n";
  source += "     then() { assign(total, plus(total, 2i32)) },\n";
  source += "     else() { })\n";
  source += "  return(total)\n";
  source += "}\n";
  return source;
}

inline std::string makeMapExtendedConstructorConformanceSource(const std::string &importPath) {
  const std::string keyType = mapConformanceKeyType(importPath);
  const std::string directLeftKey = mapConformanceLiteral(importPath, "1i32", "\"left\"raw_utf8");
  const std::string directMidKey = mapConformanceLiteral(importPath, "2i32", "\"mid\"raw_utf8");
  const std::string directRightKey = mapConformanceLiteral(importPath, "3i32", "\"right\"raw_utf8");
  const std::string wrappedAKey = mapConformanceLiteral(importPath, "1i32", "\"a\"raw_utf8");
  const std::string wrappedBKey = mapConformanceLiteral(importPath, "2i32", "\"b\"raw_utf8");
  const std::string wrappedCKey = mapConformanceLiteral(importPath, "3i32", "\"c\"raw_utf8");
  const std::string wrappedDKey = mapConformanceLiteral(importPath, "4i32", "\"d\"raw_utf8");
  const std::string wrappedEKey = mapConformanceLiteral(importPath, "5i32", "\"e\"raw_utf8");
  const std::string wrappedFKey = mapConformanceLiteral(importPath, "6i32", "\"f\"raw_utf8");
  const std::string wrappedGKey = mapConformanceLiteral(importPath, "7i32", "\"g\"raw_utf8");
  const std::string wrappedHKey = mapConformanceLiteral(importPath, "8i32", "\"h\"raw_utf8");
  const std::string missingKey = mapConformanceLiteral(importPath, "99i32", "\"missing\"raw_utf8");

  std::string source;
  source += "import " + importPath + "\n\n";
  source += "[effects(heap_alloc), return<" + mapConformanceType(importPath, "K", "V") + "> Comparable<K>]\n";
  source += "wrapMap<K, V>() {\n";
  source += "  return(mapOct<K, V>(" + wrappedAKey + ", 1i32, " + wrappedBKey + ", 2i32, " + wrappedCKey + ", 3i32, " +
            wrappedDKey + ", 4i32,\n";
  source += "      " + wrappedEKey + ", 5i32, " + wrappedFKey + ", 6i32, " + wrappedGKey + ", 7i32, " + wrappedHKey +
            ", 8i32))\n";
  source += "}\n\n";
  source += "[effects(heap_alloc), return<int>]\n";
  source += "main() {\n";
  source += "  [" + mapConformanceType(importPath, keyType, "i32") + "] direct{mapTriple<" + keyType +
            ", i32>(" + directLeftKey + ", 10i32, " + directMidKey + ", 20i32, " + directRightKey + ", 30i32)}\n";
  source += "  [" + mapConformanceType(importPath, keyType, "i32") + "] wrapped{wrapMap<" + keyType + ", i32>()}\n";
  source += "  [i32 mut] directTotal{plus(mapCount<" + keyType + ", i32>(direct), plus(mapAt<" + keyType +
            ", i32>(direct, " + directLeftKey + "),\n";
  source += "                                                                mapAtUnsafe<" + keyType + ", i32>(direct, " +
            directRightKey + ")))}\n";
  source += "  if(mapContains<" + keyType + ", i32>(direct, " + directLeftKey + "),\n";
  source += "     then() { assign(directTotal, plus(directTotal, 1i32)) },\n";
  source += "     else() { })\n";
  source += "  if(not(mapContains<" + keyType + ", i32>(direct, " + missingKey + ")),\n";
  source += "     then() { assign(directTotal, plus(directTotal, 2i32)) },\n";
  source += "     else() { })\n";
  source += "  [i32 mut] wrappedTotal{plus(mapCount<" + keyType + ", i32>(wrapped), plus(mapAt<" + keyType +
            ", i32>(wrapped, " + wrappedCKey + "),\n";
  source += "                                                                   mapAtUnsafe<" + keyType + ", i32>(wrapped, " +
            wrappedHKey + ")))}\n";
  source += "  if(mapContains<" + keyType + ", i32>(wrapped, " + wrappedCKey + "),\n";
  source += "     then() { assign(wrappedTotal, plus(wrappedTotal, 4i32)) },\n";
  source += "     else() { })\n";
  source += "  if(not(mapContains<" + keyType + ", i32>(wrapped, " + missingKey + ")),\n";
  source += "     then() { assign(wrappedTotal, plus(wrappedTotal, 8i32)) },\n";
  source += "     else() { })\n";
  source += "  return(plus(directTotal, wrappedTotal))\n";
  source += "}\n";
  return source;
}

inline std::string makeMapOverwriteConformanceSource(const std::string &importPath) {
  const std::string keyType = mapConformanceKeyType(importPath);
  const std::string repeatedKey = mapConformanceLiteral(importPath, "7i32", "\"dup\"raw_utf8");

  std::string source;
  source += "import " + importPath + "\n\n";
  source += "[effects(heap_alloc), return<int>]\n";
  source += "main() {\n";
  source += "  [" + mapConformanceType(importPath, keyType, "i32") + "] values{mapPair<" + keyType +
            ", i32>(" + repeatedKey + ", 4i32, " + repeatedKey + ", 9i32)}\n";
  source += "  return(plus(plus(mapCount<" + keyType + ", i32>(values), mapAt<" + keyType + ", i32>(values, " +
            repeatedKey + ")),\n";
  source += "      mapAtUnsafe<" + keyType + ", i32>(values, " + repeatedKey + ")))\n";
  source += "}\n";
  return source;
}

inline std::string makeMapTryAtConformanceImportSource(const std::string &importPath,
                                                       bool boolValues) {
  const bool experimental = isExperimentalMapImport(importPath);
  const std::string keyType = mapConformanceKeyType(importPath);
  const std::string leftKey = mapConformanceLiteral(importPath, "11i32", "\"left\"raw_utf8");
  const std::string rightKey = mapConformanceLiteral(importPath, "22i32", "\"right\"raw_utf8");
  const std::string missingKey = mapConformanceLiteral(importPath, "99i32", "\"missing\"raw_utf8");
  const std::string valueType = boolValues ? "bool" : "i32";
  const std::string leftValue = boolValues ? "true" : "7i32";
  const std::string rightValue = boolValues ? "false" : "11i32";
  const std::string successExpr = boolValues ? "if(found, then(){ 1i32 }, else(){ 99i32 })" : "found";

  std::string source;
  if (experimental) {
    source += "import /std/collections/errors/*\n";
  }
  source += "import " + importPath + "\n\n";
  source += "[effects(io_err)]\n";
  source += "unexpectedMapTryAtError([ContainerError] err) {\n";
  source += "  [Result<ContainerError>] status{err.code}\n";
  source += "  print_line_error(Result.why(status))\n";
  source += "}\n\n";
  source +=
      "[return<Result<i32, ContainerError>> effects(io_out, heap_alloc) on_error<ContainerError, /unexpectedMapTryAtError>]\n";
  source += "main() {\n";
  source += "  [" + mapConformanceType(importPath, keyType, valueType) + "] values{mapPair<" + keyType + ", " +
            valueType + ">(" + leftKey + ", " + leftValue + ", " + rightKey + ", " + rightValue + ")}\n";
  source += "  [" + valueType + "] found{try(mapTryAt<" + keyType + ", " + valueType + ">(values, " + leftKey +
            "))}\n";
  source += "  [Result<" + valueType + ", ContainerError>] missing{mapTryAt<" + keyType + ", " + valueType +
            ">(values, " + missingKey + ")}\n";
  source += "  print_line(Result.why(missing))\n";
  source += "  return(Result.ok(plus(" + successExpr + ", 21i32)))\n";
  source += "}\n";
  return source;
}

inline void expectMapConformanceProgramRuns(const std::string &source,
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

inline void expectMapHelperSurfaceConformance(const std::string &emitMode,
                                              const std::string &importPath) {
  const std::string slug = mapHelperConformanceImportSlug(importPath);
  expectMapConformanceProgramRuns(
      makeMapHelperSurfaceConformanceSource(importPath),
      "map_helper_surface_" + slug + "_" + emitMode,
      emitMode,
      16);
}

inline void expectMapExtendedConstructorConformance(const std::string &emitMode,
                                                    const std::string &importPath) {
  const std::string slug = mapHelperConformanceImportSlug(importPath);
  expectMapConformanceProgramRuns(
      makeMapExtendedConstructorConformanceSource(importPath),
      "map_extended_ctor_" + slug + "_" + emitMode,
      emitMode,
      77);
}

inline void expectMapOverwriteConformance(const std::string &emitMode,
                                          const std::string &importPath) {
  const std::string slug = mapHelperConformanceImportSlug(importPath);
  expectMapConformanceProgramRuns(
      makeMapOverwriteConformanceSource(importPath),
      "map_overwrite_" + slug + "_" + emitMode,
      emitMode,
      19);
}

inline void expectMapTryAtConformance(const std::string &emitMode,
                                      const std::string &importPath,
                                      bool boolValues) {
  const std::string slug = mapHelperConformanceImportSlug(importPath);
  const std::string valueSlug = boolValues ? "bool" : "i32";
  const int expectedExitCode = boolValues ? 22 : 28;
  const std::string source = makeMapTryAtConformanceImportSource(importPath, boolValues);
  const std::string srcPath = writeTemp("map_try_at_" + slug + "_" + valueSlug + "_" + emitMode + ".prime", source);
  const std::string outPath =
      (std::filesystem::temp_directory_path() / ("primec_map_try_at_" + slug + "_" + valueSlug + "_" + emitMode +
                                                 "_out.txt"))
          .string();

  if (emitMode == "vm") {
    const std::string runCmd =
        "./primec --emit=vm " + quoteShellArg(srcPath) + " --entry /main > " + quoteShellArg(outPath);
    CHECK(runCommand(runCmd) == expectedExitCode);
    CHECK(readFile(outPath) == "container missing key\n");
    return;
  }

  const std::string exePath =
      (std::filesystem::temp_directory_path() / ("primec_map_try_at_" + slug + "_" + valueSlug + "_" + emitMode +
                                                 "_exe"))
          .string();
  const std::string compileCmd = "./primec --emit=" + emitMode + " " + quoteShellArg(srcPath) + " -o " +
                                 quoteShellArg(exePath) + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string runCmd = quoteShellArg(exePath) + " > " + quoteShellArg(outPath);
  CHECK(runCommand(runCmd) == expectedExitCode);
  CHECK(readFile(outPath) == "container missing key\n");
}
