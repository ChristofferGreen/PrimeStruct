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
  return(plus(plus(mapCount<string, i32>(pairs), mapAt<string, i32>(pairs, "left"raw_utf8)),
              mapAtUnsafe<string, i32>(pairs, "right"raw_utf8)))
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
  [i32] directTotal{plus(mapCount<string, i32>(direct), plus(mapAt<string, i32>(direct, "left"raw_utf8),
                                                            mapAtUnsafe<string, i32>(direct, "right"raw_utf8)))}
  [i32] wrappedTotal{plus(mapCount<string, i32>(wrapped), plus(mapAt<string, i32>(wrapped, "c"raw_utf8),
                                                               mapAtUnsafe<string, i32>(wrapped, "h"raw_utf8)))}
  return(plus(directTotal, wrappedTotal))
}
)";
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
      13);
}

inline void expectMapExtendedConstructorConformance(const std::string &emitMode,
                                                    const std::string &importPath) {
  const std::string slug = mapHelperConformanceImportSlug(importPath);
  expectMapConformanceProgramRuns(
      makeMapExtendedConstructorConformanceSource(importPath),
      "map_extended_ctor_" + slug + "_" + emitMode,
      emitMode,
      62);
}
