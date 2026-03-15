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
  (void)importPath;
  return "string";
}

inline std::string mapConformanceLiteral(const std::string &importPath,
                                         const std::string &numericValue,
                                         const std::string &stringValue) {
  (void)importPath;
  (void)numericValue;
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
    source += "import /std/collections/*\n";
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

inline std::string makeExperimentalMapMethodConformanceSource() {
  std::string source;
  source += "import /std/collections/*\n";
  source += "import /std/collections/experimental_map/*\n\n";
  source += "[effects(io_err)]\n";
  source += "unexpectedExperimentalMapMethodError([ContainerError] err) {\n";
  source += "  [Result<ContainerError>] status{err.code}\n";
  source += "  print_line_error(Result.why(status))\n";
  source += "}\n\n";
  source +=
      "[return<Result<int, ContainerError>> effects(io_out, heap_alloc) on_error<ContainerError, /unexpectedExperimentalMapMethodError>]\n";
  source += "main() {\n";
  source += "  [Map<string, i32>] values{mapPair<string, i32>(\"left\"raw_utf8, 4i32, \"right\"raw_utf8, 7i32)}\n";
  source += "  [i32] found{try(values.tryAt(\"left\"raw_utf8))}\n";
  source += "  [Result<i32, ContainerError>] missing{values.tryAt(\"missing\"raw_utf8)}\n";
  source += "  [i32 mut] total{plus(values.count(), found)}\n";
  source += "  assign(total, plus(total, values.at(\"left\"raw_utf8)))\n";
  source += "  assign(total, plus(total, values.at_unsafe(\"right\"raw_utf8)))\n";
  source += "  if(values.contains(\"left\"raw_utf8),\n";
  source += "     then() { assign(total, plus(total, 1i32)) },\n";
  source += "     else() { })\n";
  source += "  if(not(values.contains(\"missing\"raw_utf8)),\n";
  source += "     then() { assign(total, plus(total, 2i32)) },\n";
  source += "     else() { })\n";
  source += "  print_line(Result.why(missing))\n";
  source += "  return(Result.ok(total))\n";
  source += "}\n";
  return source;
}

inline std::string makeExperimentalMapReferenceHelperConformanceSource() {
  std::string source;
  source += "import /std/collections/*\n";
  source += "import /std/collections/experimental_map/*\n\n";
  source += "[effects(io_err)]\n";
  source += "unexpectedExperimentalMapReferenceHelperError([ContainerError] err) {\n";
  source += "  [Result<ContainerError>] status{err.code}\n";
  source += "  print_line_error(Result.why(status))\n";
  source += "}\n\n";
  source +=
      "[return<Result<int, ContainerError>> effects(io_out, heap_alloc) on_error<ContainerError, /unexpectedExperimentalMapReferenceHelperError>]\n";
  source += "main() {\n";
  source += "  [Map<string, i32>] values{mapPair<string, i32>(\"left\"raw_utf8, 4i32, \"right\"raw_utf8, 7i32)}\n";
  source += "  [Reference<Map<string, i32>>] ref{location(values)}\n";
  source += "  [i32] found{try(mapTryAtRef<string, i32>(ref, \"left\"raw_utf8))}\n";
  source += "  [Result<i32, ContainerError>] missing{mapTryAtRef<string, i32>(ref, \"missing\"raw_utf8)}\n";
  source += "  [i32 mut] total{plus(mapCountRef<string, i32>(ref), found)}\n";
  source += "  assign(total, plus(total, mapAtRef<string, i32>(ref, \"left\"raw_utf8)))\n";
  source += "  assign(total, plus(total, mapAtUnsafeRef<string, i32>(ref, \"right\"raw_utf8)))\n";
  source += "  if(mapContainsRef<string, i32>(ref, \"left\"raw_utf8),\n";
  source += "     then() { assign(total, plus(total, 1i32)) },\n";
  source += "     else() { })\n";
  source += "  if(not(mapContainsRef<string, i32>(ref, \"missing\"raw_utf8)),\n";
  source += "     then() { assign(total, plus(total, 2i32)) },\n";
  source += "     else() { })\n";
  source += "  print_line(Result.why(missing))\n";
  source += "  return(Result.ok(total))\n";
  source += "}\n";
  return source;
}

inline std::string makeExperimentalMapReferenceMethodConformanceSource() {
  std::string source;
  source += "import /std/collections/*\n";
  source += "import /std/collections/experimental_map/*\n\n";
  source += "[return<Reference<Map<string, i32>>>]\n";
  source += "borrowExperimentalMap([Reference<Map<string, i32>>] values) {\n";
  source += "  return(values)\n";
  source += "}\n\n";
  source += "[effects(io_err)]\n";
  source += "unexpectedExperimentalMapReferenceMethodError([ContainerError] err) {\n";
  source += "  [Result<ContainerError>] status{err.code}\n";
  source += "  print_line_error(Result.why(status))\n";
  source += "}\n\n";
  source +=
      "[return<Result<int, ContainerError>> effects(io_out, heap_alloc) on_error<ContainerError, /unexpectedExperimentalMapReferenceMethodError>]\n";
  source += "main() {\n";
  source += "  [Map<string, i32>] values{mapPair<string, i32>(\"left\"raw_utf8, 4i32, \"right\"raw_utf8, 7i32)}\n";
  source += "  [Reference<Map<string, i32>>] ref{borrowExperimentalMap(location(values))}\n";
  source += "  [i32] found{try(ref.tryAt(\"left\"raw_utf8))}\n";
  source += "  [Result<i32, ContainerError>] missing{ref.tryAt(\"missing\"raw_utf8)}\n";
  source += "  [i32 mut] total{plus(ref.count(), found)}\n";
  source += "  assign(total, plus(total, ref.at(\"left\"raw_utf8)))\n";
  source += "  assign(total, plus(total, ref.at_unsafe(\"right\"raw_utf8)))\n";
  source += "  if(ref.contains(\"left\"raw_utf8),\n";
  source += "     then() { assign(total, plus(total, 1i32)) },\n";
  source += "     else() { })\n";
  source += "  if(not(ref.contains(\"missing\"raw_utf8)),\n";
  source += "     then() { assign(total, plus(total, 2i32)) },\n";
  source += "     else() { })\n";
  source += "  print_line(Result.why(missing))\n";
  source += "  return(Result.ok(total))\n";
  source += "}\n";
  return source;
}

inline std::string makeExperimentalMapInsertConformanceSource() {
  std::string source;
  source += "import /std/collections/*\n";
  source += "import /std/collections/experimental_map/*\n\n";
  source += "[effects(heap_alloc), return<int>]\n";
  source += "main() {\n";
  source += "  [Map<string, i32> mut] values{mapSingle<string, i32>(\"left\"raw_utf8, 4i32)}\n";
  source += "  mapInsert<string, i32>(values, \"right\"raw_utf8, 7i32)\n";
  source += "  values.insert(\"left\"raw_utf8, 9i32)\n";
  source += "  [Reference<Map<string, i32>> mut] ref{location(values)}\n";
  source += "  mapInsertRef<string, i32>(ref, \"third\"raw_utf8, 11i32)\n";
  source += "  ref.insert(\"right\"raw_utf8, 13i32)\n";
  source += "  return(plus(values.count(),\n";
  source += "              plus(values.at(\"left\"raw_utf8),\n";
  source += "                   plus(ref.at(\"right\"raw_utf8), values.at(\"third\"raw_utf8)))))\n";
  source += "}\n";
  return source;
}

inline std::string makeExperimentalMapIndexConformanceSource() {
  std::string source;
  source += "import /std/collections/*\n";
  source += "import /std/collections/experimental_map/*\n\n";
  source += "[return<Reference<Map<string, i32>>>]\n";
  source += "borrowExperimentalMap([Reference<Map<string, i32>>] values) {\n";
  source += "  return(values)\n";
  source += "}\n\n";
  source += "[effects(heap_alloc), return<int>]\n";
  source += "main() {\n";
  source += "  [Map<string, i32>] values{mapPair<string, i32>(\"left\"raw_utf8, 4i32, \"right\"raw_utf8, 7i32)}\n";
  source += "  [i32] left{values[\"left\"raw_utf8]}\n";
  source += "  [i32] right{borrowExperimentalMap(location(values))[\"right\"raw_utf8]}\n";
  source += "  return(plus(left, right))\n";
  source += "}\n";
  return source;
}

inline std::string makeCanonicalMapNamespaceConformanceSource() {
  std::string source;
  source += "import /std/collections/*\n\n";
  source += "[effects(io_err)]\n";
  source += "unexpectedCanonicalMapError([ContainerError] err) {\n";
  source += "  [Result<ContainerError>] status{err.code}\n";
  source += "  print_line_error(Result.why(status))\n";
  source += "}\n\n";
  source +=
      "[return<Result<int, ContainerError>> effects(io_out, heap_alloc) on_error<ContainerError, /unexpectedCanonicalMapError>]\n";
  source += "main() {\n";
  source +=
      "  [map<string, i32>] values{/std/collections/map/map<string, i32>(\"left\"raw_utf8, 4i32, \"right\"raw_utf8, 7i32)}\n";
  source += "  [i32] found{try(/std/collections/map/tryAt<string, i32>(values, \"left\"raw_utf8))}\n";
  source +=
      "  [Result<i32, ContainerError>] missing{/std/collections/map/tryAt<string, i32>(values, \"missing\"raw_utf8)}\n";
  source += "  [i32 mut] total{plus(/std/collections/map/count<string, i32>(values), found)}\n";
  source += "  assign(total, plus(total, /std/collections/map/at<string, i32>(values, \"left\"raw_utf8)))\n";
  source +=
      "  assign(total, plus(total, /std/collections/map/at_unsafe<string, i32>(values, \"right\"raw_utf8)))\n";
  source += "  if(/std/collections/map/contains<string, i32>(values, \"left\"raw_utf8),\n";
  source += "     then() { assign(total, plus(total, 1i32)) },\n";
  source += "     else() { })\n";
  source += "  if(not(/std/collections/map/contains<string, i32>(values, \"missing\"raw_utf8)),\n";
  source += "     then() { assign(total, plus(total, 2i32)) },\n";
  source += "     else() { })\n";
  source += "  return(Result.ok(total))\n";
  source += "}\n";
  return source;
}

inline std::string makeCanonicalMapNamespaceExperimentalReferenceConformanceSource() {
  std::string source;
  source += "import /std/collections/*\n";
  source += "import /std/collections/experimental_map/*\n\n";
  source += "[return<Reference<Map<string, i32>>>]\n";
  source += "borrowExperimentalMap([Reference<Map<string, i32>>] values) {\n";
  source += "  return(values)\n";
  source += "}\n\n";
  source += "[return<i32> effects(heap_alloc)]\n";
  source += "main() {\n";
  source += "  [Map<string, i32>] values{mapPair<string, i32>(\"left\"raw_utf8, 4i32, \"right\"raw_utf8, 7i32)}\n";
  source += "  [Reference<Map<string, i32>>] ref{borrowExperimentalMap(location(values))}\n";
  source += "  return(/std/collections/map/count<string, i32>(ref))\n";
  source += "}\n";
  return source;
}

inline std::string makeCanonicalMapNamespaceExperimentalValueConformanceSource() {
  std::string source;
  source += "import /std/collections/*\n";
  source += "import /std/collections/experimental_map/*\n\n";
  source += "[effects(io_err)]\n";
  source += "unexpectedCanonicalExperimentalMapValueError([ContainerError] err) {\n";
  source += "  [Result<ContainerError>] status{err.code}\n";
  source += "  print_line_error(Result.why(status))\n";
  source += "}\n\n";
  source +=
      "[return<Result<int, ContainerError>> effects(io_out, heap_alloc) on_error<ContainerError, /unexpectedCanonicalExperimentalMapValueError>]\n";
  source += "main() {\n";
  source += "  [Map<string, i32>] values{mapPair<string, i32>(\"left\"raw_utf8, 4i32, \"right\"raw_utf8, 7i32)}\n";
  source += "  [i32] found{try(/std/collections/map/tryAt<string, i32>(values, \"left\"raw_utf8))}\n";
  source +=
      "  [Result<i32, ContainerError>] missing{/std/collections/map/tryAt<string, i32>(values, \"missing\"raw_utf8)}\n";
  source += "  [i32 mut] total{plus(/std/collections/map/count<string, i32>(values), found)}\n";
  source += "  assign(total, plus(total, /std/collections/map/at<string, i32>(values, \"left\"raw_utf8)))\n";
  source +=
      "  assign(total, plus(total, /std/collections/map/at_unsafe<string, i32>(values, \"right\"raw_utf8)))\n";
  source += "  if(/std/collections/map/contains<string, i32>(values, \"left\"raw_utf8),\n";
  source += "     then() { assign(total, plus(total, 1i32)) },\n";
  source += "     else() { })\n";
  source += "  if(not(/std/collections/map/contains<string, i32>(values, \"missing\"raw_utf8)),\n";
  source += "     then() { assign(total, plus(total, 2i32)) },\n";
  source += "     else() { })\n";
  source += "  print_line(Result.why(missing))\n";
  source += "  return(Result.ok(total))\n";
  source += "}\n";
  return source;
}

inline std::string makeCanonicalMapNamespaceExperimentalConstructorConformanceSource() {
  std::string source;
  source += "import /std/collections/*\n";
  source += "import /std/collections/experimental_map/*\n\n";
  source += "[effects(io_err)]\n";
  source += "unexpectedCanonicalExperimentalMapConstructorError([ContainerError] err) {\n";
  source += "  [Result<ContainerError>] status{err.code}\n";
  source += "  print_line_error(Result.why(status))\n";
  source += "}\n\n";
  source +=
      "[return<Result<int, ContainerError>> effects(io_out, heap_alloc) on_error<ContainerError, /unexpectedCanonicalExperimentalMapConstructorError>]\n";
  source += "main() {\n";
  source +=
      "  [Map<string, i32>] values{/std/collections/map/map<string, i32>(\"left\"raw_utf8, 4i32, \"right\"raw_utf8, 7i32)}\n";
  source += "  [i32] found{try(/std/collections/map/tryAt<string, i32>(values, \"left\"raw_utf8))}\n";
  source +=
      "  [Result<i32, ContainerError>] missing{/std/collections/map/tryAt<string, i32>(values, \"missing\"raw_utf8)}\n";
  source += "  [i32 mut] total{plus(/std/collections/map/count<string, i32>(values), found)}\n";
  source += "  assign(total, plus(total, /std/collections/map/at<string, i32>(values, \"left\"raw_utf8)))\n";
  source +=
      "  assign(total, plus(total, /std/collections/map/at_unsafe<string, i32>(values, \"right\"raw_utf8)))\n";
  source += "  if(/std/collections/map/contains<string, i32>(values, \"left\"raw_utf8),\n";
  source += "     then() { assign(total, plus(total, 1i32)) },\n";
  source += "     else() { })\n";
  source += "  if(not(/std/collections/map/contains<string, i32>(values, \"missing\"raw_utf8)),\n";
  source += "     then() { assign(total, plus(total, 2i32)) },\n";
  source += "     else() { })\n";
  source += "  print_line(Result.why(missing))\n";
  source += "  return(Result.ok(total))\n";
  source += "}\n";
  return source;
}

inline std::string makeCanonicalMapNamespaceExperimentalReturnConformanceSource() {
  std::string source;
  source += "import /std/collections/*\n";
  source += "import /std/collections/experimental_map/*\n\n";
  source += "[effects(io_err)]\n";
  source += "unexpectedCanonicalExperimentalMapReturnError([ContainerError] err) {\n";
  source += "  [Result<ContainerError>] status{err.code}\n";
  source += "  print_line_error(Result.why(status))\n";
  source += "}\n\n";
  source += "[return<Map<string, i32>> effects(heap_alloc)]\n";
  source += "buildValues() {\n";
  source += "  return(/std/collections/map/map<string, i32>(\"left\"raw_utf8, 4i32, \"right\"raw_utf8, 7i32))\n";
  source += "}\n\n";
  source +=
      "[return<Result<int, ContainerError>> effects(io_out, heap_alloc) on_error<ContainerError, /unexpectedCanonicalExperimentalMapReturnError>]\n";
  source += "main() {\n";
  source += "  [Map<string, i32>] values{buildValues()}\n";
  source += "  [i32] found{try(/std/collections/map/tryAt<string, i32>(values, \"left\"raw_utf8))}\n";
  source += "  [i32 mut] total{plus(/std/collections/map/count<string, i32>(values), found)}\n";
  source += "  assign(total, plus(total, /std/collections/map/at<string, i32>(values, \"left\"raw_utf8)))\n";
  source +=
      "  assign(total, plus(total, /std/collections/map/at_unsafe<string, i32>(values, \"right\"raw_utf8)))\n";
  source += "  if(/std/collections/map/contains<string, i32>(values, \"left\"raw_utf8),\n";
  source += "     then() { assign(total, plus(total, 1i32)) },\n";
  source += "     else() { })\n";
  source += "  return(Result.ok(total))\n";
  source += "}\n";
  return source;
}

inline std::string makeCanonicalMapNamespaceExperimentalParameterConformanceSource() {
  std::string source;
  source += "import /std/collections/*\n";
  source += "import /std/collections/experimental_map/*\n\n";
  source += "[effects(io_err)]\n";
  source += "unexpectedCanonicalExperimentalMapParameterError([ContainerError] err) {\n";
  source += "  [Result<ContainerError>] status{err.code}\n";
  source += "  print_line_error(Result.why(status))\n";
  source += "}\n\n";
  source +=
      "[return<Result<int, ContainerError>> effects(io_out, heap_alloc) on_error<ContainerError, /unexpectedCanonicalExperimentalMapParameterError>]\n";
  source += "scoreValues([Map<string, i32>] values) {\n";
  source += "  [i32] found{try(/std/collections/map/tryAt<string, i32>(values, \"left\"raw_utf8))}\n";
  source += "  [i32 mut] total{plus(/std/collections/map/count<string, i32>(values), found)}\n";
  source += "  assign(total, plus(total, /std/collections/map/at<string, i32>(values, \"left\"raw_utf8)))\n";
  source +=
      "  assign(total, plus(total, /std/collections/map/at_unsafe<string, i32>(values, \"right\"raw_utf8)))\n";
  source += "  if(/std/collections/map/contains<string, i32>(values, \"left\"raw_utf8),\n";
  source += "     then() { assign(total, plus(total, 1i32)) },\n";
  source += "     else() { })\n";
  source += "  return(Result.ok(total))\n";
  source += "}\n\n";
  source +=
      "[return<Result<int, ContainerError>> effects(io_out, heap_alloc) on_error<ContainerError, /unexpectedCanonicalExperimentalMapParameterError>]\n";
  source += "main() {\n";
  source +=
      "  return(scoreValues(/std/collections/map/map<string, i32>(\"left\"raw_utf8, 4i32, \"right\"raw_utf8, 7i32)))\n";
  source += "}\n";
  return source;
}

inline std::string makeWrapperMapConstructorExperimentalBindingConformanceSource() {
  std::string source;
  source += "import /std/collections/*\n";
  source += "import /std/collections/experimental_map/*\n\n";
  source += "[effects(io_err)]\n";
  source += "unexpectedWrapperExperimentalMapConstructorError([ContainerError] err) {\n";
  source += "  [Result<ContainerError>] status{err.code}\n";
  source += "  print_line_error(Result.why(status))\n";
  source += "}\n\n";
  source +=
      "[return<Result<int, ContainerError>> effects(io_out, heap_alloc) on_error<ContainerError, /unexpectedWrapperExperimentalMapConstructorError>]\n";
  source += "main() {\n";
  source +=
      "  [Map<string, i32>] values{/std/collections/mapPair<string, i32>(\"left\"raw_utf8, 4i32, \"right\"raw_utf8, 7i32)}\n";
  source += "  [i32] found{try(/std/collections/map/tryAt<string, i32>(values, \"left\"raw_utf8))}\n";
  source +=
      "  [Result<i32, ContainerError>] missing{/std/collections/map/tryAt<string, i32>(values, \"missing\"raw_utf8)}\n";
  source += "  [i32 mut] total{plus(/std/collections/map/count<string, i32>(values), found)}\n";
  source += "  assign(total, plus(total, /std/collections/map/at<string, i32>(values, \"left\"raw_utf8)))\n";
  source +=
      "  assign(total, plus(total, /std/collections/map/at_unsafe<string, i32>(values, \"right\"raw_utf8)))\n";
  source += "  if(/std/collections/map/contains<string, i32>(values, \"left\"raw_utf8),\n";
  source += "     then() { assign(total, plus(total, 1i32)) },\n";
  source += "     else() { })\n";
  source += "  if(not(/std/collections/map/contains<string, i32>(values, \"missing\"raw_utf8)),\n";
  source += "     then() { assign(total, plus(total, 2i32)) },\n";
  source += "     else() { })\n";
  source += "  print_line(Result.why(missing))\n";
  source += "  return(Result.ok(total))\n";
  source += "}\n";
  return source;
}

inline std::string makeWrapperMapConstructorExperimentalReturnConformanceSource() {
  std::string source;
  source += "import /std/collections/*\n";
  source += "import /std/collections/experimental_map/*\n\n";
  source += "[effects(io_err)]\n";
  source += "unexpectedWrapperExperimentalMapReturnError([ContainerError] err) {\n";
  source += "  [Result<ContainerError>] status{err.code}\n";
  source += "  print_line_error(Result.why(status))\n";
  source += "}\n\n";
  source += "[return<Map<string, i32>> effects(heap_alloc)]\n";
  source += "buildValues() {\n";
  source +=
      "  return(/std/collections/mapPair<string, i32>(\"left\"raw_utf8, 4i32, \"right\"raw_utf8, 7i32))\n";
  source += "}\n\n";
  source +=
      "[return<Result<int, ContainerError>> effects(io_out, heap_alloc) on_error<ContainerError, /unexpectedWrapperExperimentalMapReturnError>]\n";
  source += "main() {\n";
  source += "  [Map<string, i32>] values{buildValues()}\n";
  source += "  [i32] found{try(/std/collections/map/tryAt<string, i32>(values, \"left\"raw_utf8))}\n";
  source += "  [i32 mut] total{plus(/std/collections/map/count<string, i32>(values), found)}\n";
  source += "  assign(total, plus(total, /std/collections/map/at<string, i32>(values, \"left\"raw_utf8)))\n";
  source +=
      "  assign(total, plus(total, /std/collections/map/at_unsafe<string, i32>(values, \"right\"raw_utf8)))\n";
  source += "  if(/std/collections/map/contains<string, i32>(values, \"left\"raw_utf8),\n";
  source += "     then() { assign(total, plus(total, 1i32)) },\n";
  source += "     else() { })\n";
  source += "  return(Result.ok(total))\n";
  source += "}\n";
  return source;
}

inline std::string makeWrapperMapConstructorExperimentalParameterConformanceSource() {
  std::string source;
  source += "import /std/collections/*\n";
  source += "import /std/collections/experimental_map/*\n\n";
  source += "[effects(io_err)]\n";
  source += "unexpectedWrapperExperimentalMapParameterError([ContainerError] err) {\n";
  source += "  [Result<ContainerError>] status{err.code}\n";
  source += "  print_line_error(Result.why(status))\n";
  source += "}\n\n";
  source +=
      "[return<Result<int, ContainerError>> effects(io_out, heap_alloc) on_error<ContainerError, /unexpectedWrapperExperimentalMapParameterError>]\n";
  source += "scoreValues([Map<string, i32>] values) {\n";
  source += "  [i32] found{try(/std/collections/map/tryAt<string, i32>(values, \"left\"raw_utf8))}\n";
  source += "  [i32 mut] total{plus(/std/collections/map/count<string, i32>(values), found)}\n";
  source += "  assign(total, plus(total, /std/collections/map/at<string, i32>(values, \"left\"raw_utf8)))\n";
  source +=
      "  assign(total, plus(total, /std/collections/map/at_unsafe<string, i32>(values, \"right\"raw_utf8)))\n";
  source += "  if(/std/collections/map/contains<string, i32>(values, \"left\"raw_utf8),\n";
  source += "     then() { assign(total, plus(total, 1i32)) },\n";
  source += "     else() { })\n";
  source += "  return(Result.ok(total))\n";
  source += "}\n\n";
  source +=
      "[return<Result<int, ContainerError>> effects(io_out, heap_alloc) on_error<ContainerError, /unexpectedWrapperExperimentalMapParameterError>]\n";
  source += "main() {\n";
  source +=
      "  return(scoreValues(/std/collections/mapPair<string, i32>(\"left\"raw_utf8, 4i32, \"right\"raw_utf8, 7i32)))\n";
  source += "}\n";
  return source;
}

inline std::string makeExperimentalMapAssignConformanceSource() {
  std::string source;
  source += "import /std/collections/*\n";
  source += "import /std/collections/experimental_map/*\n\n";
  source += "[effects(io_err)]\n";
  source += "unexpectedExperimentalMapAssignError([ContainerError] err) {\n";
  source += "  [Result<ContainerError>] status{err.code}\n";
  source += "  print_line_error(Result.why(status))\n";
  source += "}\n\n";
  source +=
      "[return<Result<int, ContainerError>> effects(io_out, heap_alloc) on_error<ContainerError, /unexpectedExperimentalMapAssignError>]\n";
  source += "scoreValues([Map<string, i32>] values) {\n";
  source += "  [i32] found{try(/std/collections/map/tryAt<string, i32>(values, \"left\"raw_utf8))}\n";
  source += "  [i32 mut] total{plus(/std/collections/map/count<string, i32>(values), found)}\n";
  source += "  assign(total, plus(total, /std/collections/map/at<string, i32>(values, \"left\"raw_utf8)))\n";
  source +=
      "  assign(total, plus(total, /std/collections/map/at_unsafe<string, i32>(values, \"right\"raw_utf8)))\n";
  source += "  if(/std/collections/map/contains<string, i32>(values, \"left\"raw_utf8),\n";
  source += "     then() { assign(total, plus(total, 1i32)) },\n";
  source += "     else() { })\n";
  source += "  return(Result.ok(total))\n";
  source += "}\n\n";
  source +=
      "[return<Result<int, ContainerError>> effects(io_out, heap_alloc) on_error<ContainerError, /unexpectedExperimentalMapAssignError>]\n";
  source += "replaceAndScore([Map<string, i32> mut] values) {\n";
  source +=
      "  assign(values, /std/collections/mapPair<string, i32>(\"left\"raw_utf8, 4i32, \"right\"raw_utf8, 7i32))\n";
  source += "  return(scoreValues(values))\n";
  source += "}\n\n";
  source +=
      "[return<Result<int, ContainerError>> effects(io_out, heap_alloc) on_error<ContainerError, /unexpectedExperimentalMapAssignError>]\n";
  source += "main() {\n";
  source += "  [Map<string, i32> mut] values{mapNew<string, i32>()}\n";
  source += "  [Map<string, i32> mut] other{mapNew<string, i32>()}\n";
  source +=
      "  assign(values, /std/collections/map/map<string, i32>(\"left\"raw_utf8, 4i32, \"right\"raw_utf8, 7i32))\n";
  source += "  [i32] localScore{try(scoreValues(values))}\n";
  source += "  [i32] paramScore{try(replaceAndScore(other))}\n";
  source += "  return(Result.ok(plus(localScore, paramScore)))\n";
  source += "}\n";
  return source;
}

inline std::string makeImplicitMapAutoInferenceConformanceSource() {
  std::string source;
  source += "import /std/collections/*\n";
  source += "import /std/collections/experimental_map/*\n\n";
  source += "[return<auto> effects(heap_alloc)]\n";
  source += "buildValues() {\n";
  source +=
      "  return(/std/collections/mapPair(\"left\"raw_utf8, 4i32, \"right\"raw_utf8, 7i32))\n";
  source += "}\n\n";
  source += "[effects(io_err)]\n";
  source += "unexpectedExperimentalMapAutoError([ContainerError] err) {\n";
  source += "  [Result<ContainerError>] status{err.code}\n";
  source += "  print_line_error(Result.why(status))\n";
  source += "}\n\n";
  source +=
      "[return<Result<int, ContainerError>> effects(io_out, heap_alloc) on_error<ContainerError, /unexpectedExperimentalMapAutoError>]\n";
  source += "main() {\n";
  source += "  [auto mut] values{/std/collections/map/map(\"seed\"raw_utf8, 1i32)}\n";
  source += "  mapInsert<string, i32>(values, \"left\"raw_utf8, 4i32)\n";
  source += "  mapInsert<string, i32>(values, \"right\"raw_utf8, 7i32)\n";
  source += "  [auto mut] built{buildValues()}\n";
  source += "  mapInsert<string, i32>(built, \"extra\"raw_utf8, 9i32)\n";
  source += "  [i32] left{try(/std/collections/map/tryAt<string, i32>(values, \"left\"raw_utf8))}\n";
  source += "  [i32] extra{try(/std/collections/map/tryAt<string, i32>(built, \"extra\"raw_utf8))}\n";
  source +=
      "  return(Result.ok(plus(plus(/std/collections/map/count<string, i32>(values), /std/collections/map/count<string, i32>(built)), plus(left, extra))))\n";
  source += "}\n";
  return source;
}

inline std::string makeInferredExperimentalMapReturnConformanceSource() {
  std::string source;
  source += "import /std/collections/*\n";
  source += "import /std/collections/experimental_map/*\n\n";
  source += "[return<auto> effects(heap_alloc)]\n";
  source += "buildValues() {\n";
  source += "  return(/std/collections/map/map(\"left\"raw_utf8, 4i32, \"right\"raw_utf8, 7i32))\n";
  source += "}\n\n";
  source += "[effects(io_err)]\n";
  source += "unexpectedInferredExperimentalMapReturnError([ContainerError] err) {\n";
  source += "  [Result<ContainerError>] status{err.code}\n";
  source += "  print_line_error(Result.why(status))\n";
  source += "}\n\n";
  source +=
      "[return<Result<int, ContainerError>> effects(io_out, heap_alloc) on_error<ContainerError, /unexpectedInferredExperimentalMapReturnError>]\n";
  source += "main() {\n";
  source += "  [Map<string, i32> mut] values{buildValues()}\n";
  source += "  mapInsert<string, i32>(values, \"extra\"raw_utf8, 9i32)\n";
  source += "  [i32] left{try(/std/collections/map/tryAt<string, i32>(values, \"left\"raw_utf8))}\n";
  source += "  [i32] extra{try(/std/collections/map/tryAt<string, i32>(values, \"extra\"raw_utf8))}\n";
  source += "  return(Result.ok(plus(/std/collections/map/count<string, i32>(values), plus(left, extra))))\n";
  source += "}\n";
  return source;
}

inline std::string makeBlockInferredExperimentalMapReturnConformanceSource() {
  std::string source;
  source += "import /std/collections/*\n";
  source += "import /std/collections/experimental_map/*\n\n";
  source += "[return<auto> effects(heap_alloc)]\n";
  source += "buildValues([bool] useCanonical) {\n";
  source += "  if(useCanonical,\n";
  source += "     then() { /std/collections/map/map(\"left\"raw_utf8, 4i32, \"right\"raw_utf8, 7i32) },\n";
  source += "     else() { /std/collections/mapPair(\"left\"raw_utf8, 4i32, \"other\"raw_utf8, 2i32) })\n";
  source += "}\n\n";
  source += "[effects(io_err)]\n";
  source += "unexpectedBlockExperimentalMapReturnError([ContainerError] err) {\n";
  source += "  [Result<ContainerError>] status{err.code}\n";
  source += "  print_line_error(Result.why(status))\n";
  source += "}\n\n";
  source +=
      "[return<Result<int, ContainerError>> effects(io_out, heap_alloc) on_error<ContainerError, /unexpectedBlockExperimentalMapReturnError>]\n";
  source += "main() {\n";
  source += "  [Map<string, i32> mut] values{buildValues(true)}\n";
  source += "  mapInsert<string, i32>(values, \"extra\"raw_utf8, 9i32)\n";
  source += "  [i32] left{try(/std/collections/map/tryAt<string, i32>(values, \"left\"raw_utf8))}\n";
  source += "  [i32] extra{try(/std/collections/map/tryAt<string, i32>(values, \"extra\"raw_utf8))}\n";
  source += "  return(Result.ok(plus(/std/collections/map/count<string, i32>(values), plus(left, extra))))\n";
  source += "}\n";
  return source;
}

inline std::string makeAutoBlockInferredExperimentalMapReturnConformanceSource() {
  std::string source;
  source += "import /std/collections/*\n";
  source += "import /std/collections/experimental_map/*\n\n";
  source += "[return<auto> effects(heap_alloc)]\n";
  source += "buildValues([bool] useCanonical) {\n";
  source += "  if(useCanonical,\n";
  source += "     then() {\n";
  source += "       [auto mut] values{/std/collections/map/map(\"left\"raw_utf8, 4i32, \"right\"raw_utf8, 7i32)}\n";
  source += "       mapInsert<string, i32>(values, \"extra\"raw_utf8, 9i32)\n";
  source += "       values\n";
  source += "     },\n";
  source += "     else() {\n";
  source += "       [auto mut] values{/std/collections/mapPair(\"left\"raw_utf8, 4i32, \"other\"raw_utf8, 2i32)}\n";
  source += "       values\n";
  source += "     })\n";
  source += "}\n\n";
  source += "[effects(io_err)]\n";
  source += "unexpectedAutoBlockExperimentalMapReturnError([ContainerError] err) {\n";
  source += "  [Result<ContainerError>] status{err.code}\n";
  source += "  print_line_error(Result.why(status))\n";
  source += "}\n\n";
  source +=
      "[return<Result<int, ContainerError>> effects(io_out, heap_alloc) on_error<ContainerError, /unexpectedAutoBlockExperimentalMapReturnError>]\n";
  source += "main() {\n";
  source += "  [Map<string, i32>] values{buildValues(true)}\n";
  source += "  [i32] left{try(/std/collections/map/tryAt<string, i32>(values, \"left\"raw_utf8))}\n";
  source += "  [i32] extra{try(/std/collections/map/tryAt<string, i32>(values, \"extra\"raw_utf8))}\n";
  source += "  return(Result.ok(plus(/std/collections/map/count<string, i32>(values), plus(left, extra))))\n";
  source += "}\n";
  return source;
}

inline std::string makeInferredExperimentalMapCallReceiverConformanceSource() {
  std::string source;
  source += "import /std/collections/*\n";
  source += "import /std/collections/experimental_map/*\n\n";
  source += "[return<auto> effects(heap_alloc)]\n";
  source += "buildValues([bool] useCanonical) {\n";
  source += "  if(useCanonical,\n";
  source += "     then() { /std/collections/map/map(\"left\"raw_utf8, 4i32, \"right\"raw_utf8, 7i32) },\n";
  source += "     else() { /std/collections/mapPair(\"left\"raw_utf8, 4i32, \"other\"raw_utf8, 2i32) })\n";
  source += "}\n\n";
  source += "[effects(io_err)]\n";
  source += "unexpectedInferredExperimentalMapReceiverError([ContainerError] err) {\n";
  source += "  [Result<ContainerError>] status{err.code}\n";
  source += "  print_line_error(Result.why(status))\n";
  source += "}\n\n";
  source +=
      "[return<Result<int, ContainerError>> effects(io_out, heap_alloc) on_error<ContainerError, /unexpectedInferredExperimentalMapReceiverError>]\n";
  source += "main() {\n";
  source += "  [i32] left{try(buildValues(true).tryAt(\"left\"raw_utf8))}\n";
  source += "  [i32 mut] total{plus(buildValues(true).count(), left)}\n";
  source += "  if(buildValues(true).contains(\"right\"raw_utf8),\n";
  source += "     then() { assign(total, plus(total, 1i32)) },\n";
  source += "     else() { })\n";
  source += "  return(Result.ok(total))\n";
  source += "}\n";
  return source;
}

inline std::string makeExperimentalMapStructFieldConformanceSource() {
  std::string source;
  source += "import /std/collections/*\n";
  source += "import /std/collections/experimental_map/*\n\n";
  source += "[struct]\n";
  source += "Holder() {\n";
  source += "  [Map<string, i32>] primary{}\n";
  source += "  [Map<string, i32>] secondary{}\n";
  source += "}\n\n";
  source += "[effects(io_err)]\n";
  source += "unexpectedExperimentalMapStructFieldError([ContainerError] err) {\n";
  source += "  [Result<ContainerError>] status{err.code}\n";
  source += "  print_line_error(Result.why(status))\n";
  source += "}\n\n";
  source +=
      "[return<Result<int, ContainerError>> effects(io_out, heap_alloc) on_error<ContainerError, /unexpectedExperimentalMapStructFieldError>]\n";
  source += "main() {\n";
  source +=
      "  [Holder] holder{Holder(/std/collections/map/map(\"left\"raw_utf8, 4i32, \"right\"raw_utf8, 7i32),\n";
  source +=
      "                        /std/collections/mapPair(\"other\"raw_utf8, 2i32, \"extra\"raw_utf8, 9i32))}\n";
  source += "  [i32] left{try(/std/collections/map/tryAt(holder.primary, \"left\"raw_utf8))}\n";
  source += "  [i32] extra{try(/std/collections/map/tryAt(holder.secondary, \"extra\"raw_utf8))}\n";
  source += "  return(Result.ok(plus(left, extra)))\n";
  source += "}\n";
  return source;
}

inline std::string makeInferredExperimentalMapStructFieldConformanceSource() {
  std::string source;
  source += "import /std/collections/*\n";
  source += "import /std/collections/experimental_map/*\n\n";
  source += "[struct]\n";
  source += "Holder() {\n";
  source += "  primary{mapNew<string, i32>()}\n";
  source += "  secondary{mapNew<string, i32>()}\n";
  source += "}\n\n";
  source += "[effects(heap_alloc), return<int>]\n";
  source += "main() {\n";
  source += "  [Holder mut] holder{Holder(/std/collections/map/map(\"left\"raw_utf8, 4i32, \"right\"raw_utf8, 7i32))}\n";
  source +=
      "  assign(holder.secondary, /std/collections/mapPair(\"extra\"raw_utf8, 9i32, \"other\"raw_utf8, 2i32))\n";
  source +=
      "  return(plus(/std/collections/map/at(holder.primary, \"left\"raw_utf8), /std/collections/map/at(holder.secondary, \"extra\"raw_utf8)))\n";
  source += "}\n";
  return source;
}

inline std::string makeExperimentalMapMethodParameterConformanceSource() {
  std::string source;
  source += "import /std/collections/*\n";
  source += "import /std/collections/experimental_map/*\n\n";
  source += "Holder() {}\n\n";
  source += "[return<int> effects(heap_alloc)]\n";
  source += "/Holder/score([Holder] self, [Map<string, i32>] values) {\n";
  source +=
      "  return(plus(/std/collections/map/count(values), /std/collections/map/at_unsafe(values, \"left\"raw_utf8)))\n";
  source += "}\n\n";
  source += "[effects(heap_alloc), return<int>]\n";
  source += "main() {\n";
  source += "  [Holder] holder{Holder()}\n";
  source +=
      "  return(plus(holder.score(/std/collections/map/map(\"left\"raw_utf8, 4i32, \"right\"raw_utf8, 7i32)),\n";
  source +=
      "              holder.score(/std/collections/mapPair(\"left\"raw_utf8, 2i32, \"extra\"raw_utf8, 9i32))))\n";
  source += "}\n";
  return source;
}

inline std::string makeInferredExperimentalMapParameterConformanceSource() {
  std::string source;
  source += "import /std/collections/*\n";
  source += "import /std/collections/experimental_map/*\n\n";
  source += "Holder() {}\n\n";
  source += "[return<int> effects(heap_alloc)]\n";
  source += "scoreValues([auto mut] values{mapNew<string, i32>()}) {\n";
  source += "  mapInsert<string, i32>(values, \"extra\"raw_utf8, 9i32)\n";
  source +=
      "  return(plus(/std/collections/map/count(values), /std/collections/map/at(values, \"left\"raw_utf8)))\n";
  source += "}\n\n";
  source += "[return<int> effects(heap_alloc)]\n";
  source += "/Holder/score([Holder] self, [auto mut] values{mapNew<string, i32>()}) {\n";
  source += "  mapInsert<string, i32>(values, \"bonus\"raw_utf8, 5i32)\n";
  source +=
      "  return(plus(/std/collections/map/count(values), /std/collections/map/at(values, \"extra\"raw_utf8)))\n";
  source += "}\n\n";
  source += "[effects(heap_alloc), return<int>]\n";
  source += "main() {\n";
  source += "  [Holder] holder{Holder()}\n";
  source +=
      "  return(plus(scoreValues(/std/collections/map/map(\"left\"raw_utf8, 4i32, \"right\"raw_utf8, 7i32)),\n";
  source +=
      "              holder.score(/std/collections/mapPair(\"left\"raw_utf8, 2i32, \"extra\"raw_utf8, 9i32))))\n";
  source += "}\n";
  return source;
}

inline std::string makeExperimentalMapFieldAssignConformanceSource() {
  std::string source;
  source += "import /std/collections/*\n";
  source += "import /std/collections/experimental_map/*\n\n";
  source += "Holder() {\n";
  source += "  [Map<string, i32> mut] primary{mapNew<string, i32>()}\n";
  source += "  [Map<string, i32> mut] secondary{mapNew<string, i32>()}\n";
  source += "}\n\n";
  source += "[effects(heap_alloc), return<int>]\n";
  source += "main() {\n";
  source += "  [Holder mut] holder{Holder()}\n";
  source +=
      "  assign(holder.primary, /std/collections/map/map(\"left\"raw_utf8, 4i32, \"right\"raw_utf8, 7i32))\n";
  source +=
      "  assign(holder.secondary, /std/collections/mapPair(\"extra\"raw_utf8, 9i32, \"other\"raw_utf8, 2i32))\n";
  source +=
      "  return(plus(/std/collections/map/at(holder.primary, \"left\"raw_utf8), /std/collections/map/at(holder.secondary, \"extra\"raw_utf8)))\n";
  source += "}\n";
  return source;
}

inline std::string makeCanonicalMapNamespaceExperimentalBorrowedRefConformanceSource() {
  std::string source;
  source += "import /std/collections/*\n";
  source += "import /std/collections/experimental_map/*\n\n";
  source += "[return<Reference<Map<string, i32>>>]\n";
  source += "borrowExperimentalMap([Reference<Map<string, i32>>] values) {\n";
  source += "  return(values)\n";
  source += "}\n\n";
  source += "[effects(io_err)]\n";
  source += "unexpectedCanonicalExperimentalMapBorrowedRefError([ContainerError] err) {\n";
  source += "  [Result<ContainerError>] status{err.code}\n";
  source += "  print_line_error(Result.why(status))\n";
  source += "}\n\n";
  source +=
      "[return<Result<int, ContainerError>> effects(io_out, heap_alloc) on_error<ContainerError, /unexpectedCanonicalExperimentalMapBorrowedRefError>]\n";
  source += "main() {\n";
  source += "  [Map<string, i32>] values{mapPair<string, i32>(\"left\"raw_utf8, 4i32, \"right\"raw_utf8, 7i32)}\n";
  source += "  [Reference<Map<string, i32>>] ref{borrowExperimentalMap(location(values))}\n";
  source += "  [i32] found{try(/std/collections/map/tryAt_ref<string, i32>(ref, \"left\"raw_utf8))}\n";
  source +=
      "  [Result<i32, ContainerError>] missing{/std/collections/map/tryAt_ref<string, i32>(ref, \"missing\"raw_utf8)}\n";
  source += "  [i32 mut] total{plus(/std/collections/map/count_ref<string, i32>(ref), found)}\n";
  source += "  assign(total, plus(total, /std/collections/map/at_ref<string, i32>(ref, \"left\"raw_utf8)))\n";
  source +=
      "  assign(total, plus(total, /std/collections/map/at_unsafe_ref<string, i32>(ref, \"right\"raw_utf8)))\n";
  source += "  if(/std/collections/map/contains_ref<string, i32>(ref, \"left\"raw_utf8),\n";
  source += "     then() { assign(total, plus(total, 1i32)) },\n";
  source += "     else() { })\n";
  source += "  if(not(/std/collections/map/contains_ref<string, i32>(ref, \"missing\"raw_utf8)),\n";
  source += "     then() { assign(total, plus(total, 2i32)) },\n";
  source += "     else() { })\n";
  source += "  print_line(Result.why(missing))\n";
  source += "  return(Result.ok(total))\n";
  source += "}\n";
  return source;
}

inline std::string makeCanonicalMapNamespaceNamedArgsSource() {
  std::string source;
  source += "import /std/collections/*\n\n";
  source += "[effects(io_err)]\n";
  source += "unexpectedCanonicalMapNamedArgsError([ContainerError] err) {\n";
  source += "  [Result<ContainerError>] status{err.code}\n";
  source += "  print_line_error(Result.why(status))\n";
  source += "}\n\n";
  source +=
      "[return<Result<int, ContainerError>> effects(io_out, heap_alloc) on_error<ContainerError, /unexpectedCanonicalMapNamedArgsError>]\n";
  source += "main() {\n";
  source +=
      "  [map<string, i32>] values{/std/collections/map/map<string, i32>([secondKey] \"right\"raw_utf8, [secondValue] 7i32, [firstKey] \"left\"raw_utf8, [firstValue] 4i32)}\n";
  source +=
      "  [i32] found{try(/std/collections/map/tryAt<string, i32>([key] \"left\"raw_utf8, [values] values))}\n";
  source += "  [i32 mut] total{/std/collections/map/count<string, i32>([values] values)}\n";
  source +=
      "  assign(total, plus(total, /std/collections/map/at<string, i32>([key] \"right\"raw_utf8, [values] values)))\n";
  source +=
      "  assign(total, plus(total, /std/collections/map/at_unsafe<string, i32>([key] \"left\"raw_utf8, [values] values)))\n";
  source += "  if(/std/collections/map/contains<string, i32>([key] \"right\"raw_utf8, [values] values),\n";
  source += "     then() { assign(total, plus(total, found)) },\n";
  source += "     else() { })\n";
  source += "  return(Result.ok(total))\n";
  source += "}\n";
  return source;
}

inline std::string makeCanonicalMapNamespaceTypeMismatchRejectSource() {
  std::string source;
  source += "import /std/collections/*\n\n";
  source += "[effects(heap_alloc), return<int>]\n";
  source += "main() {\n";
  source +=
      "  [map<string, i32>] values{/std/collections/map/map<string, i32>(\"left\"raw_utf8, 4i32, \"right\"raw_utf8, false)}\n";
  source += "  return(/std/collections/map/count<string, i32>(values))\n";
  source += "}\n";
  return source;
}

inline std::string makeCanonicalMapNamespaceImportRequirementSource() {
  std::string source;
  source += "[effects(heap_alloc), return<int>]\n";
  source += "main() {\n";
  source +=
      "  [map<string, i32>] values{/std/collections/map/map<string, i32>(\"left\"raw_utf8, 4i32, \"right\"raw_utf8, 7i32)}\n";
  source += "  return(count(values))\n";
  source += "}\n";
  return source;
}

inline std::string makeCanonicalMapCountImportRequirementSource() {
  std::string source;
  source += "[effects(heap_alloc), return<int>]\n";
  source += "main() {\n";
  source += "  [map<string, i32>] values{map<string, i32>(\"left\"raw_utf8, 4i32, \"right\"raw_utf8, 7i32)}\n";
  source += "  return(/std/collections/map/count<string, i32>(values))\n";
  source += "}\n";
  return source;
}

inline std::string makeCanonicalMapContainsImportRequirementSource() {
  std::string source;
  source += "[effects(heap_alloc), return<int>]\n";
  source += "main() {\n";
  source += "  [map<string, i32>] values{map<string, i32>(\"left\"raw_utf8, 4i32, \"right\"raw_utf8, 7i32)}\n";
  source += "  [bool] found{/std/collections/map/contains<string, i32>(values, \"left\"raw_utf8)}\n";
  source += "  return(0i32)\n";
  source += "}\n";
  return source;
}

inline std::string makeCanonicalMapTryAtImportRequirementSource() {
  std::string source;
  source += "[effects(heap_alloc), return<int>]\n";
  source += "main() {\n";
  source += "  [map<string, i32>] values{map<string, i32>(\"left\"raw_utf8, 4i32, \"right\"raw_utf8, 7i32)}\n";
  source += "  return(try(/std/collections/map/tryAt<string, i32>(values, \"left\"raw_utf8)))\n";
  source += "}\n";
  return source;
}

inline std::string makeCanonicalMapAccessImportRequirementSource(const std::string &helperName) {
  std::string source;
  source += "[effects(heap_alloc), return<int>]\n";
  source += "main() {\n";
  source += "  [map<string, i32>] values{map<string, i32>(\"left\"raw_utf8, 4i32, \"right\"raw_utf8, 7i32)}\n";
  source += "  return(/std/collections/map/" + helperName +
            "<string, i32>(values, \"left\"raw_utf8))\n";
  source += "}\n";
  return source;
}

inline std::string makeCanonicalMapNamespaceCountShadowSource() {
  std::string source;
  source += "import /std/collections/*\n\n";
  source += "[return<int>]\n";
  source += "/count([map<string, i32>] values) {\n";
  source += "  return(91i32)\n";
  source += "}\n\n";
  source += "[effects(heap_alloc), return<int>]\n";
  source += "main() {\n";
  source +=
      "  [map<string, i32>] values{/std/collections/map/map<string, i32>(\"left\"raw_utf8, 4i32, \"right\"raw_utf8, 7i32)}\n";
  source += "  return(/std/collections/map/count<string, i32>(values))\n";
  source += "}\n";
  return source;
}

inline std::string makeCanonicalMapNamespaceAccessShadowSource() {
  std::string source;
  source += "import /std/collections/*\n\n";
  source += "[return<int>]\n";
  source += "/at([map<string, i32>] values, [string] key) {\n";
  source += "  return(88i32)\n";
  source += "}\n\n";
  source += "[return<int>]\n";
  source += "/at_unsafe([map<string, i32>] values, [string] key) {\n";
  source += "  return(33i32)\n";
  source += "}\n\n";
  source += "[effects(heap_alloc), return<int>]\n";
  source += "main() {\n";
  source +=
      "  [map<string, i32>] values{/std/collections/map/map<string, i32>(\"left\"raw_utf8, 4i32, \"right\"raw_utf8, 7i32)}\n";
  source += "  return(plus(/std/collections/map/at<string, i32>(values, \"left\"raw_utf8),\n";
  source += "      /std/collections/map/at_unsafe<string, i32>(values, \"right\"raw_utf8)))\n";
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

inline void expectExperimentalMapMethodConformance(const std::string &emitMode) {
  const std::string source = makeExperimentalMapMethodConformanceSource();
  const std::string srcPath = writeTemp("experimental_map_methods_" + emitMode + ".prime", source);
  const std::string outPath =
      (std::filesystem::temp_directory_path() / ("primec_experimental_map_methods_" + emitMode + "_out.txt"))
          .string();

  if (emitMode == "vm") {
    const std::string runCmd =
        "./primec --emit=vm " + quoteShellArg(srcPath) + " --entry /main > " + quoteShellArg(outPath);
    CHECK(runCommand(runCmd) == 20);
    CHECK(readFile(outPath) == "container missing key\n");
    return;
  }

  const std::string exePath =
      (std::filesystem::temp_directory_path() / ("primec_experimental_map_methods_" + emitMode + "_exe")).string();
  const std::string compileCmd = "./primec --emit=" + emitMode + " " + quoteShellArg(srcPath) + " -o " +
                                 quoteShellArg(exePath) + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string runCmd = quoteShellArg(exePath) + " > " + quoteShellArg(outPath);
  CHECK(runCommand(runCmd) == 20);
  CHECK(readFile(outPath) == "container missing key\n");
}

inline void expectExperimentalMapReferenceHelperConformance(const std::string &emitMode) {
  const std::string source = makeExperimentalMapReferenceHelperConformanceSource();
  const std::string srcPath = writeTemp("experimental_map_reference_helpers_" + emitMode + ".prime", source);
  const std::string outPath =
      (std::filesystem::temp_directory_path() /
       ("primec_experimental_map_reference_helpers_" + emitMode + "_out.txt"))
          .string();

  if (emitMode == "vm") {
    const std::string runCmd =
        "./primec --emit=vm " + quoteShellArg(srcPath) + " --entry /main > " + quoteShellArg(outPath);
    CHECK(runCommand(runCmd) == 20);
    CHECK(readFile(outPath) == "container missing key\n");
    return;
  }

  const std::string exePath =
      (std::filesystem::temp_directory_path() /
       ("primec_experimental_map_reference_helpers_" + emitMode + "_exe"))
          .string();
  const std::string compileCmd = "./primec --emit=" + emitMode + " " + quoteShellArg(srcPath) + " -o " +
                                 quoteShellArg(exePath) + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string runCmd = quoteShellArg(exePath) + " > " + quoteShellArg(outPath);
  CHECK(runCommand(runCmd) == 20);
  CHECK(readFile(outPath) == "container missing key\n");
}

inline void expectExperimentalMapReferenceMethodConformance(const std::string &emitMode) {
  const std::string source = makeExperimentalMapReferenceMethodConformanceSource();
  const std::string srcPath = writeTemp("experimental_map_reference_methods_" + emitMode + ".prime", source);
  const std::string outPath =
      (std::filesystem::temp_directory_path() /
       ("primec_experimental_map_reference_methods_" + emitMode + "_out.txt"))
          .string();

  if (emitMode == "vm") {
    const std::string runCmd =
        "./primec --emit=vm " + quoteShellArg(srcPath) + " --entry /main > " + quoteShellArg(outPath);
    CHECK(runCommand(runCmd) == 20);
    CHECK(readFile(outPath) == "container missing key\n");
    return;
  }

  const std::string exePath =
      (std::filesystem::temp_directory_path() /
       ("primec_experimental_map_reference_methods_" + emitMode + "_exe"))
          .string();
  const std::string compileCmd = "./primec --emit=" + emitMode + " " + quoteShellArg(srcPath) + " -o " +
                                 quoteShellArg(exePath) + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string runCmd = quoteShellArg(exePath) + " > " + quoteShellArg(outPath);
  CHECK(runCommand(runCmd) == 20);
  CHECK(readFile(outPath) == "container missing key\n");
}

inline void expectExperimentalMapInsertConformance(const std::string &emitMode) {
  expectMapConformanceProgramRuns(
      makeExperimentalMapInsertConformanceSource(),
      "experimental_map_insert_" + emitMode,
      emitMode,
      36);
}

inline void expectExperimentalMapIndexConformance(const std::string &emitMode) {
  expectMapConformanceProgramRuns(
      makeExperimentalMapIndexConformanceSource(),
      "experimental_map_index_" + emitMode,
      emitMode,
      11);
}

inline void expectCanonicalMapNamespaceConformance(const std::string &emitMode) {
  expectMapConformanceProgramRuns(
      makeCanonicalMapNamespaceConformanceSource(),
      "map_namespace_canonical_" + emitMode,
      emitMode,
      20);
}

inline void expectCanonicalMapNamespaceExperimentalReferenceConformance(const std::string &emitMode) {
  const std::string source = makeCanonicalMapNamespaceExperimentalReferenceConformanceSource();
  const std::string srcPath =
      writeTemp("map_namespace_canonical_experimental_reference_" + emitMode + ".prime", source);
  const std::string outPath =
      (std::filesystem::temp_directory_path() /
       ("primec_map_namespace_canonical_experimental_reference_" + emitMode + "_out.txt"))
          .string();

  if (emitMode == "vm") {
    const std::string runCmd = "./primec --emit=vm " + quoteShellArg(srcPath) + " --entry /main > " +
                               quoteShellArg(outPath) + " 2>&1";
    CHECK(runCommand(runCmd) == 2);
    CHECK(readFile(outPath).find("unknown call target: /std/collections/map/count") != std::string::npos);
    return;
  }

  const std::string compileCmd = "./primec --emit=" + emitMode + " " + quoteShellArg(srcPath) +
                                 " -o /dev/null --entry /main > " + quoteShellArg(outPath) + " 2>&1";
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(outPath).find("unknown call target: /std/collections/map/count") != std::string::npos);
}

inline void expectCanonicalMapNamespaceExperimentalValueConformance(const std::string &emitMode) {
  expectMapConformanceProgramRuns(
      makeCanonicalMapNamespaceExperimentalValueConformanceSource(),
      "map_namespace_canonical_experimental_value_" + emitMode,
      emitMode,
      20);
}

inline void expectCanonicalMapNamespaceExperimentalConstructorConformance(const std::string &emitMode) {
  expectMapConformanceProgramRuns(
      makeCanonicalMapNamespaceExperimentalConstructorConformanceSource(),
      "map_namespace_canonical_experimental_constructor_" + emitMode,
      emitMode,
      20);
}

inline void expectCanonicalMapNamespaceExperimentalReturnConformance(const std::string &emitMode) {
  expectMapConformanceProgramRuns(
      makeCanonicalMapNamespaceExperimentalReturnConformanceSource(),
      "map_namespace_canonical_experimental_return_" + emitMode,
      emitMode,
      20);
}

inline void expectCanonicalMapNamespaceExperimentalParameterConformance(const std::string &emitMode) {
  expectMapConformanceProgramRuns(
      makeCanonicalMapNamespaceExperimentalParameterConformanceSource(),
      "map_namespace_canonical_experimental_parameter_" + emitMode,
      emitMode,
      20);
}

inline void expectWrapperMapConstructorExperimentalBindingConformance(const std::string &emitMode) {
  expectMapConformanceProgramRuns(
      makeWrapperMapConstructorExperimentalBindingConformanceSource(),
      "map_wrapper_constructor_experimental_binding_" + emitMode,
      emitMode,
      20);
}

inline void expectWrapperMapConstructorExperimentalReturnConformance(const std::string &emitMode) {
  expectMapConformanceProgramRuns(
      makeWrapperMapConstructorExperimentalReturnConformanceSource(),
      "map_wrapper_constructor_experimental_return_" + emitMode,
      emitMode,
      20);
}

inline void expectWrapperMapConstructorExperimentalParameterConformance(const std::string &emitMode) {
  expectMapConformanceProgramRuns(
      makeWrapperMapConstructorExperimentalParameterConformanceSource(),
      "map_wrapper_constructor_experimental_parameter_" + emitMode,
      emitMode,
      20);
}

inline void expectExperimentalMapAssignConformance(const std::string &emitMode) {
  expectMapConformanceProgramRuns(
      makeExperimentalMapAssignConformanceSource(),
      "map_experimental_assign_" + emitMode,
      emitMode,
      36);
}

inline void expectImplicitMapAutoInferenceConformance(const std::string &emitMode) {
  expectMapConformanceProgramRuns(
      makeImplicitMapAutoInferenceConformanceSource(),
      "map_implicit_auto_inference_" + emitMode,
      emitMode,
      19);
}

inline void expectInferredExperimentalMapReturnConformance(const std::string &emitMode) {
  expectMapConformanceProgramRuns(
      makeInferredExperimentalMapReturnConformanceSource(),
      "map_inferred_experimental_return_" + emitMode,
      emitMode,
      16);
}

inline void expectBlockInferredExperimentalMapReturnConformance(const std::string &emitMode) {
  expectMapConformanceProgramRuns(
      makeBlockInferredExperimentalMapReturnConformanceSource(),
      "map_block_inferred_experimental_return_" + emitMode,
      emitMode,
      16);
}

inline void expectAutoBlockInferredExperimentalMapReturnConformance(const std::string &emitMode) {
  expectMapConformanceProgramRuns(
      makeAutoBlockInferredExperimentalMapReturnConformanceSource(),
      "map_auto_block_inferred_experimental_return_" + emitMode,
      emitMode,
      16);
}

inline void expectInferredExperimentalMapCallReceiverConformance(const std::string &emitMode) {
  expectMapConformanceProgramRuns(
      makeInferredExperimentalMapCallReceiverConformanceSource(),
      "map_inferred_experimental_call_receiver_" + emitMode,
      emitMode,
      7);
}

inline void expectExperimentalMapStructFieldConformance(const std::string &emitMode) {
  expectMapConformanceProgramRuns(
      makeExperimentalMapStructFieldConformanceSource(),
      "map_experimental_struct_fields_" + emitMode,
      emitMode,
      13);
}

inline void expectInferredExperimentalMapStructFieldConformance(const std::string &emitMode) {
  expectMapConformanceProgramRuns(
      makeInferredExperimentalMapStructFieldConformanceSource(),
      "map_experimental_inferred_struct_fields_" + emitMode,
      emitMode,
      13);
}

inline void expectExperimentalMapMethodParameterConformance(const std::string &emitMode) {
  expectMapConformanceProgramRuns(
      makeExperimentalMapMethodParameterConformanceSource(),
      "map_experimental_method_parameter_" + emitMode,
      emitMode,
      10);
}

inline void expectInferredExperimentalMapParameterConformance(const std::string &emitMode) {
  expectMapConformanceProgramRuns(
      makeInferredExperimentalMapParameterConformanceSource(),
      "map_experimental_inferred_parameter_" + emitMode,
      emitMode,
      19);
}

inline void expectExperimentalMapFieldAssignConformance(const std::string &emitMode) {
  expectMapConformanceProgramRuns(
      makeExperimentalMapFieldAssignConformanceSource(),
      "map_experimental_field_assign_" + emitMode,
      emitMode,
      13);
}

inline void expectCanonicalMapNamespaceExperimentalBorrowedRefConformance(const std::string &emitMode) {
  const std::string source = makeCanonicalMapNamespaceExperimentalBorrowedRefConformanceSource();
  const std::string srcPath =
      writeTemp("map_namespace_canonical_experimental_borrowed_ref_" + emitMode + ".prime", source);
  const std::string outPath =
      (std::filesystem::temp_directory_path() /
       ("primec_map_namespace_canonical_experimental_borrowed_ref_" + emitMode + "_out.txt"))
          .string();

  if (emitMode == "vm") {
    const std::string runCmd = "./primec --emit=vm " + quoteShellArg(srcPath) + " --entry /main > " +
                               quoteShellArg(outPath);
    CHECK(runCommand(runCmd) == 20);
    CHECK(readFile(outPath) == "container missing key\n");
    return;
  }

  const std::string exePath =
      (std::filesystem::temp_directory_path() /
       ("primec_map_namespace_canonical_experimental_borrowed_ref_" + emitMode + "_exe"))
          .string();
  const std::string compileCmd = "./primec --emit=" + emitMode + " " + quoteShellArg(srcPath) + " -o " +
                                 quoteShellArg(exePath) + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string runCmd = quoteShellArg(exePath) + " > " + quoteShellArg(outPath);
  CHECK(runCommand(runCmd) == 20);
  CHECK(readFile(outPath) == "container missing key\n");
}

inline void expectCanonicalMapNamespaceNamedArgsConformance(const std::string &emitMode) {
  expectMapConformanceProgramRuns(
      makeCanonicalMapNamespaceNamedArgsSource(),
      "map_namespace_canonical_named_args_" + emitMode,
      emitMode,
      17);
}

inline void expectCanonicalMapNamespaceTypeMismatchReject(const std::string &emitMode) {
  const std::string source = makeCanonicalMapNamespaceTypeMismatchRejectSource();
  const std::string srcPath = writeTemp("map_namespace_canonical_type_mismatch_" + emitMode + ".prime", source);
  const std::string outPath =
      (std::filesystem::temp_directory_path() /
       ("primec_map_namespace_canonical_type_mismatch_" + emitMode + "_out.txt"))
          .string();

  if (emitMode == "vm") {
    const std::string runCmd = "./primec --emit=vm " + quoteShellArg(srcPath) + " --entry /main > " +
                               quoteShellArg(outPath) + " 2>&1";
    CHECK(runCommand(runCmd) == 2);
    CHECK(readFile(outPath).find("mismatch") != std::string::npos);
    return;
  }

  const std::string compileCmd = "./primec --emit=" + emitMode + " " + quoteShellArg(srcPath) +
                                 " -o /dev/null --entry /main > " + quoteShellArg(outPath) + " 2>&1";
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(outPath).find("mismatch") != std::string::npos);
}

inline void expectCanonicalMapNamespaceImportRequirement(const std::string &emitMode) {
  const std::string source = makeCanonicalMapNamespaceImportRequirementSource();
  const std::string srcPath = writeTemp("map_namespace_canonical_import_requirement_" + emitMode + ".prime", source);
  const std::string outPath =
      (std::filesystem::temp_directory_path() /
       ("primec_map_namespace_canonical_import_requirement_" + emitMode + "_out.txt"))
          .string();

  if (emitMode == "vm") {
    const std::string runCmd = "./primec --emit=vm " + quoteShellArg(srcPath) + " --entry /main > " +
                               quoteShellArg(outPath) + " 2>&1";
    CHECK(runCommand(runCmd) == 2);
    CHECK(readFile(outPath).find("unknown call target: /std/collections/map/map") != std::string::npos);
    return;
  }

  const std::string compileCmd = "./primec --emit=" + emitMode + " " + quoteShellArg(srcPath) +
                                 " -o /dev/null --entry /main > " + quoteShellArg(outPath) + " 2>&1";
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(outPath).find("unknown call target: /std/collections/map/map") != std::string::npos);
}

inline void expectCanonicalMapCountImportRequirement(const std::string &emitMode) {
  const std::string source = makeCanonicalMapCountImportRequirementSource();
  const std::string srcPath = writeTemp("map_count_canonical_import_requirement_" + emitMode + ".prime", source);
  const std::string outPath =
      (std::filesystem::temp_directory_path() /
       ("primec_map_count_canonical_import_requirement_" + emitMode + "_out.txt"))
          .string();

  if (emitMode == "vm") {
    const std::string runCmd = "./primec --emit=vm " + quoteShellArg(srcPath) + " --entry /main > " +
                               quoteShellArg(outPath) + " 2>&1";
    CHECK(runCommand(runCmd) == 2);
    CHECK(readFile(outPath).find("unknown call target: /std/collections/map/count") != std::string::npos);
    return;
  }

  const std::string compileCmd = "./primec --emit=" + emitMode + " " + quoteShellArg(srcPath) +
                                 " -o /dev/null --entry /main > " + quoteShellArg(outPath) + " 2>&1";
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(outPath).find("unknown call target: /std/collections/map/count") != std::string::npos);
}

inline void expectCanonicalMapContainsImportRequirement(const std::string &emitMode) {
  const std::string source = makeCanonicalMapContainsImportRequirementSource();
  const std::string srcPath =
      writeTemp("map_contains_canonical_import_requirement_" + emitMode + ".prime", source);
  const std::string outPath =
      (std::filesystem::temp_directory_path() /
       ("primec_map_contains_canonical_import_requirement_" + emitMode + "_out.txt"))
          .string();

  if (emitMode == "vm") {
    const std::string runCmd = "./primec --emit=vm " + quoteShellArg(srcPath) + " --entry /main > " +
                               quoteShellArg(outPath) + " 2>&1";
    CHECK(runCommand(runCmd) == 2);
    CHECK(readFile(outPath).find("unknown call target: /std/collections/map/contains") !=
          std::string::npos);
    return;
  }

  const std::string compileCmd = "./primec --emit=" + emitMode + " " + quoteShellArg(srcPath) +
                                 " -o /dev/null --entry /main > " + quoteShellArg(outPath) + " 2>&1";
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(outPath).find("unknown call target: /std/collections/map/contains") !=
        std::string::npos);
}

inline void expectCanonicalMapTryAtImportRequirement(const std::string &emitMode) {
  const std::string source = makeCanonicalMapTryAtImportRequirementSource();
  const std::string srcPath = writeTemp("map_try_at_canonical_import_requirement_" + emitMode + ".prime", source);
  const std::string outPath =
      (std::filesystem::temp_directory_path() /
       ("primec_map_try_at_canonical_import_requirement_" + emitMode + "_out.txt"))
          .string();

  if (emitMode == "vm") {
    const std::string runCmd = "./primec --emit=vm " + quoteShellArg(srcPath) + " --entry /main > " +
                               quoteShellArg(outPath) + " 2>&1";
    CHECK(runCommand(runCmd) == 2);
    CHECK(readFile(outPath).find("unknown call target: /std/collections/map/tryAt") !=
          std::string::npos);
    return;
  }

  const std::string compileCmd = "./primec --emit=" + emitMode + " " + quoteShellArg(srcPath) +
                                 " -o /dev/null --entry /main > " + quoteShellArg(outPath) + " 2>&1";
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(outPath).find("unknown call target: /std/collections/map/tryAt") !=
        std::string::npos);
}

inline void expectCanonicalMapAccessImportRequirement(const std::string &emitMode,
                                                      const std::string &helperName) {
  const std::string source = makeCanonicalMapAccessImportRequirementSource(helperName);
  const std::string srcPath =
      writeTemp("map_" + helperName + "_canonical_import_requirement_" + emitMode + ".prime", source);
  const std::string outPath =
      (std::filesystem::temp_directory_path() /
       ("primec_map_" + helperName + "_canonical_import_requirement_" + emitMode + "_out.txt"))
          .string();
  const std::string expected = "unknown call target: /std/collections/map/" + helperName;

  if (emitMode == "vm") {
    const std::string runCmd = "./primec --emit=vm " + quoteShellArg(srcPath) + " --entry /main > " +
                               quoteShellArg(outPath) + " 2>&1";
    CHECK(runCommand(runCmd) == 2);
    CHECK(readFile(outPath).find(expected) != std::string::npos);
    return;
  }

  const std::string compileCmd = "./primec --emit=" + emitMode + " " + quoteShellArg(srcPath) +
                                 " -o /dev/null --entry /main > " + quoteShellArg(outPath) + " 2>&1";
  CHECK(runCommand(compileCmd) == 2);
  CHECK(readFile(outPath).find(expected) != std::string::npos);
}

inline void expectCanonicalMapNamespaceCountShadow(const std::string &emitMode) {
  expectMapConformanceProgramRuns(
      makeCanonicalMapNamespaceCountShadowSource(),
      "map_namespace_canonical_count_shadow_" + emitMode,
      emitMode,
      91);
}

inline void expectCanonicalMapNamespaceAccessShadow(const std::string &emitMode) {
  expectMapConformanceProgramRuns(
      makeCanonicalMapNamespaceAccessShadowSource(),
      "map_namespace_canonical_access_shadow_" + emitMode,
      emitMode,
      121);
}

inline std::string makeExperimentalMapTryAtStringConformanceSource() {
  std::string source;
  source += "import /std/collections/*\n";
  source += "import /std/collections/experimental_map/*\n\n";
  source += "[effects(io_err)]\n";
  source += "unexpectedMapTryAtStringError([ContainerError] err) {\n";
  source += "  [Result<ContainerError>] status{err.code}\n";
  source += "  print_line_error(Result.why(status))\n";
  source += "}\n\n";
  source +=
      "[return<Result<i32, ContainerError>> effects(io_out, heap_alloc) on_error<ContainerError, /unexpectedMapTryAtStringError>]\n";
  source += "main() {\n";
  source += "  [Map<i32, string>] values{mapPair<i32, string>(11i32, \"alpha\"utf8, 22i32, \"beta\"utf8)}\n";
  source += "  [string] found{try(mapTryAt<i32, string>(values, 11i32))}\n";
  source += "  [Result<string, ContainerError>] missing{mapTryAt<i32, string>(values, 99i32)}\n";
  source += "  print_line(found)\n";
  source += "  print_line(Result.why(missing))\n";
  source += "  return(Result.ok(plus(count(found), 23i32)))\n";
  source += "}\n";
  return source;
}

inline void expectExperimentalMapTryAtStringConformance(const std::string &emitMode) {
  const std::string source = makeExperimentalMapTryAtStringConformanceSource();
  const std::string srcPath = writeTemp("map_try_at_experimental_string_" + emitMode + ".prime", source);
  const std::string outPath =
      (std::filesystem::temp_directory_path() / ("primec_map_try_at_experimental_string_" + emitMode + "_out.txt"))
          .string();

  if (emitMode == "vm") {
    const std::string runCmd =
        "./primec --emit=vm " + quoteShellArg(srcPath) + " --entry /main > " + quoteShellArg(outPath);
    CHECK(runCommand(runCmd) == 28);
    CHECK(readFile(outPath) == "alpha\ncontainer missing key\n");
    return;
  }

  const std::string exePath =
      (std::filesystem::temp_directory_path() / ("primec_map_try_at_experimental_string_" + emitMode + "_exe"))
          .string();
  const std::string compileCmd = "./primec --emit=" + emitMode + " " + quoteShellArg(srcPath) + " -o " +
                                 quoteShellArg(exePath) + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string runCmd = quoteShellArg(exePath) + " > " + quoteShellArg(outPath);
  CHECK(runCommand(runCmd) == 28);
  CHECK(readFile(outPath) == "alpha\ncontainer missing key\n");
}

inline std::string makeExperimentalMapAtMissingConformanceSource() {
  std::string source;
  source += "import /std/collections/experimental_map/*\n\n";
  source += "[return<int>]\n";
  source += "main() {\n";
  source += "  [Map<i32, i32>] values{mapPair<i32, i32>(11i32, 7i32, 22i32, 9i32)}\n";
  source += "  return(mapAt<i32, i32>(values, 99i32))\n";
  source += "}\n";
  return source;
}

inline void expectExperimentalMapAtMissingConformance(const std::string &emitMode) {
  const std::string source = makeExperimentalMapAtMissingConformanceSource();
  const std::string srcPath = writeTemp("map_at_missing_experimental_" + emitMode + ".prime", source);
  const std::string errPath =
      (std::filesystem::temp_directory_path() / ("primec_map_at_missing_experimental_" + emitMode + "_err.txt"))
          .string();

  if (emitMode == "vm") {
    const std::string runCmd =
        "./primec --emit=vm " + quoteShellArg(srcPath) + " --entry /main 2> " + quoteShellArg(errPath);
    CHECK(runCommand(runCmd) == 3);
    CHECK(readFile(errPath) == "map key not found\n");
    return;
  }

  const std::string exePath =
      (std::filesystem::temp_directory_path() / ("primec_map_at_missing_experimental_" + emitMode + "_exe"))
          .string();
  const std::string compileCmd = "./primec --emit=" + emitMode + " " + quoteShellArg(srcPath) + " -o " +
                                 quoteShellArg(exePath) + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string runCmd = quoteShellArg(exePath) + " 2> " + quoteShellArg(errPath);
  CHECK(runCommand(runCmd) == 3);
  CHECK(readFile(errPath) == "map key not found\n");
}

inline std::string makeExperimentalMapStringKeyRejectSource(const std::string &mode) {
  std::string source;
  source += "import /std/collections/experimental_map/*\n\n";
  source += "[return<int>]\n";
  source += "main([array<string>] args) {\n";
  source += "  [string] key{args[0i32]}\n";
  if (mode == "lookup_argv") {
    source += "  [Map<string, i32>] values{mapSingle<string, i32>(\"a\"raw_utf8, 1i32)}\n";
    source += "  return(mapAtUnsafe<string, i32>(values, key))\n";
  } else {
    source += "  [Map<string, i32>] values{mapSingle<string, i32>(key, 1i32)}\n";
    source += "  return(mapCount<string, i32>(values))\n";
  }
  source += "}\n";
  return source;
}

inline void expectExperimentalMapStringKeyReject(const std::string &emitMode,
                                                 const std::string &mode) {
  const std::string source = makeExperimentalMapStringKeyRejectSource(mode);
  const std::string srcPath =
      writeTemp("map_string_key_" + mode + "_experimental_" + emitMode + ".prime", source);
  const std::string errPath =
      (std::filesystem::temp_directory_path() /
       ("primec_map_string_key_" + mode + "_experimental_" + emitMode + "_err.txt"))
          .string();
  const std::string expectedError =
      "Semantic error: entry argument strings are only supported in print calls or string bindings";

  if (emitMode == "vm") {
    const std::string runCmd =
        "./primec --emit=vm " + quoteShellArg(srcPath) + " --entry /main 2> " + quoteShellArg(errPath);
    CHECK(runCommand(runCmd) == 2);
    CHECK(readFile(errPath).find(expectedError) != std::string::npos);
    return;
  }

  const std::string compileCmd =
      "./primec --emit=" + emitMode + " " + quoteShellArg(srcPath) + " -o /dev/null --entry /main 2> " +
      quoteShellArg(errPath);
  CHECK(runCommand(compileCmd) == 2);
  const std::string diagnostics = readFile(errPath);
  CHECK(diagnostics.find(expectedError) != std::string::npos);
  if (emitMode == "native") {
    CHECK(diagnostics.find(": error: " + expectedError) != std::string::npos);
    CHECK(diagnostics.find("^") != std::string::npos);
  }
}
