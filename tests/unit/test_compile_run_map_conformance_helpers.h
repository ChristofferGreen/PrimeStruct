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

inline std::string makeMapHelperSurfaceConformanceSource(const std::string &importPath) {
  return R"(
import )" + importPath + R"(

[return<map<K, V>>]
wrapMap<K, V>([K] leftKey, [V] leftValue, [K] rightKey, [V] rightValue) {
  return(mapPair<K, V>(leftKey, leftValue, rightKey, rightValue))
}

[return<int>]
main() {
  [map<string, i32>] pairs{wrapMap<string, i32>("left"raw_utf8, 4i32, "right"raw_utf8, 7i32)}
  [i32 mut] total{plus(plus(mapCount<string, i32>(pairs), mapAt<string, i32>(pairs, "left"raw_utf8)),
                       mapAtUnsafe<string, i32>(pairs, "right"raw_utf8))}
  if(mapContains<string, i32>(pairs, "left"raw_utf8),
     then() { assign(total, plus(total, 1i32)) },
     else() { })
  if(not(mapContains<string, i32>(pairs, "missing"raw_utf8)),
     then() { assign(total, plus(total, 2i32)) },
     else() { })
  return(total)
}
)";
}

inline std::string makeMapExtendedConstructorConformanceSource(const std::string &importPath) {
  return R"(
import )" + importPath + R"(

[return<map<K, V>>]
wrapMap<K, V>() {
  return(mapOct<K, V>(
      "a"raw_utf8, 1i32, "b"raw_utf8, 2i32, "c"raw_utf8, 3i32, "d"raw_utf8, 4i32,
      "e"raw_utf8, 5i32, "f"raw_utf8, 6i32, "g"raw_utf8, 7i32, "h"raw_utf8, 8i32))
}

[return<int>]
main() {
  [map<string, i32>] direct{mapTriple<string, i32>("left"raw_utf8, 10i32, "mid"raw_utf8, 20i32, "right"raw_utf8, 30i32)}
  [map<string, i32>] wrapped{wrapMap<string, i32>()}
  [i32 mut] directTotal{plus(mapCount<string, i32>(direct), plus(mapAt<string, i32>(direct, "left"raw_utf8),
                                                                mapAtUnsafe<string, i32>(direct, "right"raw_utf8)))}
  if(mapContains<string, i32>(direct, "left"raw_utf8),
     then() { assign(directTotal, plus(directTotal, 1i32)) },
     else() { })
  if(not(mapContains<string, i32>(direct, "missing"raw_utf8)),
     then() { assign(directTotal, plus(directTotal, 2i32)) },
     else() { })
  [i32 mut] wrappedTotal{plus(mapCount<string, i32>(wrapped), plus(mapAt<string, i32>(wrapped, "c"raw_utf8),
                                                                   mapAtUnsafe<string, i32>(wrapped, "h"raw_utf8)))}
  if(mapContains<string, i32>(wrapped, "c"raw_utf8),
     then() { assign(wrappedTotal, plus(wrappedTotal, 4i32)) },
     else() { })
  if(not(mapContains<string, i32>(wrapped, "missing"raw_utf8)),
     then() { assign(wrappedTotal, plus(wrappedTotal, 8i32)) },
     else() { })
  return(plus(directTotal, wrappedTotal))
}
)";
}

inline std::string makeMapTryAtConformanceImportSource(const std::string &importPath,
                                                       bool boolValues) {
  const bool experimental = (importPath == "/std/collections/experimental_map/*");
  const std::string helperPrefix = experimental ? "/std/collections/experimental_map/" : "";
  const std::string valueType = boolValues ? "bool" : "i32";
  const std::string leftValue = boolValues ? "true" : "7i32";
  const std::string rightValue = boolValues ? "false" : "11i32";
  const std::string successExpr = boolValues ? "if(found, then(){ 1i32 }, else(){ 99i32 })" : "found";
  const auto helperCall = [&](const std::string &name) {
    return helperPrefix + name;
  };

  std::string source;
  source += "import /std/collections/*\n";
  if (!experimental) {
    source += "import " + importPath + "\n";
  }
  source += "\n";
  source += "[effects(io_err)]\n";
  source += "unexpectedMapTryAtError([ContainerError] err) {\n";
  source += "  [Result<ContainerError>] status{err.code}\n";
  source += "  print_line_error(Result.why(status))\n";
  source += "}\n\n";
  source += "[return<Result<i32, ContainerError>> effects(io_out) on_error<ContainerError, /unexpectedMapTryAtError>]\n";
  source += "main() {\n";
  source += "  [map<string, " + valueType + ">] values{" + helperCall("mapPair") + "<string, " + valueType +
            ">(\"left\"raw_utf8, " + leftValue + ", \"right\"raw_utf8, " + rightValue + ")}\n";
  source += "  [" + valueType + "] found{try(" + helperCall("mapTryAt") + "<string, " + valueType +
            ">(values, \"left\"raw_utf8))}\n";
  source += "  [Result<" + valueType + ", ContainerError>] missing{" + helperCall("mapTryAt") + "<string, " +
            valueType + ">(values, \"missing\"raw_utf8)}\n";
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
