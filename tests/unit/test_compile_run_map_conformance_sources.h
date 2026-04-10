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
  source += "  [" + mapConformanceType(importPath, "K", "V") + "] values{mapPair<K, V>(leftKey, leftValue, rightKey, rightValue)}\n";
  source += "  return(values)\n";
  source += "}\n\n";
  source += "[effects(io_out, heap_alloc), return<int>]\n";
  source += "main() {\n";
  source += "  [" + mapConformanceType(importPath, keyType, "i32") + "] pairs{wrapMap<" + keyType +
            ", i32>(" + leftKey + ", 4i32, " + rightKey + ", 7i32)}\n";
  source += "  [i32] count{mapCount<" + keyType + ", i32>(pairs)}\n";
  source += "  [i32] left{mapAt<" + keyType + ", i32>(pairs, " + leftKey + ")}\n";
  source += "  [i32] right{mapAtUnsafe<" + keyType + ", i32>(pairs, " + rightKey + ")}\n";
  source += "  [i32 mut] total{plus(plus(count, left), right)}\n";
  source += "  [i32 mut] containsBonus{0i32}\n";
  source += "  if(mapContains<" + keyType + ", i32>(pairs, " + leftKey + "),\n";
  source += "     then() { assign(containsBonus, 1i32) assign(total, plus(total, containsBonus)) },\n";
  source += "     else() { })\n";
  source += "  [i32 mut] missingBonus{0i32}\n";
  source += "  if(not(mapContains<" + keyType + ", i32>(pairs, " + missingKey + ")),\n";
  source += "     then() { assign(missingBonus, 2i32) assign(total, plus(total, missingBonus)) },\n";
  source += "     else() { })\n";
  source += "  print_line(count)\n";
  source += "  print_line(left)\n";
  source += "  print_line(right)\n";
  source += "  print_line(containsBonus)\n";
  source += "  print_line(missingBonus)\n";
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
  source += "  [" + mapConformanceType(importPath, "K", "V") + "] values{mapOct<K, V>(" + wrappedAKey + ", 1i32, " +
            wrappedBKey + ", 2i32, " + wrappedCKey + ", 3i32, " + wrappedDKey + ", 4i32,\n";
  source += "      " + wrappedEKey + ", 5i32, " + wrappedFKey + ", 6i32, " + wrappedGKey + ", 7i32, " + wrappedHKey +
            ", 8i32)}\n";
  source += "  return(values)\n";
  source += "}\n\n";
  source += "[effects(io_out, heap_alloc), return<int>]\n";
  source += "main() {\n";
  source += "  [" + mapConformanceType(importPath, keyType, "i32") + "] direct{mapTriple<" + keyType +
            ", i32>(" + directLeftKey + ", 10i32, " + directMidKey + ", 20i32, " + directRightKey + ", 30i32)}\n";
  source += "  [" + mapConformanceType(importPath, keyType, "i32") + "] wrapped{wrapMap<" + keyType + ", i32>()}\n";
  source += "  [i32] directCount{mapCount<" + keyType + ", i32>(direct)}\n";
  source += "  [i32] directLeft{mapAt<" + keyType + ", i32>(direct, " + directLeftKey + ")}\n";
  source += "  [i32] directRight{mapAtUnsafe<" + keyType + ", i32>(direct, " + directRightKey + ")}\n";
  source += "  [i32 mut] directTotal{plus(directCount, plus(directLeft, directRight))}\n";
  source += "  [i32 mut] directContainsBonus{0i32}\n";
  source += "  if(mapContains<" + keyType + ", i32>(direct, " + directLeftKey + "),\n";
  source += "     then() { assign(directContainsBonus, 1i32) assign(directTotal, plus(directTotal, directContainsBonus)) },\n";
  source += "     else() { })\n";
  source += "  [i32 mut] directMissingBonus{0i32}\n";
  source += "  if(not(mapContains<" + keyType + ", i32>(direct, " + missingKey + ")),\n";
  source += "     then() { assign(directMissingBonus, 2i32) assign(directTotal, plus(directTotal, directMissingBonus)) },\n";
  source += "     else() { })\n";
  source += "  [i32] wrappedCount{mapCount<" + keyType + ", i32>(wrapped)}\n";
  source += "  [i32] wrappedCenter{mapAt<" + keyType + ", i32>(wrapped, " + wrappedCKey + ")}\n";
  source += "  [i32] wrappedEdge{mapAtUnsafe<" + keyType + ", i32>(wrapped, " + wrappedHKey + ")}\n";
  source += "  [i32 mut] wrappedTotal{plus(wrappedCount, plus(wrappedCenter, wrappedEdge))}\n";
  source += "  [i32 mut] wrappedContainsBonus{0i32}\n";
  source += "  if(mapContains<" + keyType + ", i32>(wrapped, " + wrappedCKey + "),\n";
  source += "     then() { assign(wrappedContainsBonus, 4i32) assign(wrappedTotal, plus(wrappedTotal, wrappedContainsBonus)) },\n";
  source += "     else() { })\n";
  source += "  [i32 mut] wrappedMissingBonus{0i32}\n";
  source += "  if(not(mapContains<" + keyType + ", i32>(wrapped, " + missingKey + ")),\n";
  source += "     then() { assign(wrappedMissingBonus, 8i32) assign(wrappedTotal, plus(wrappedTotal, wrappedMissingBonus)) },\n";
  source += "     else() { })\n";
  source += "  print_line(directCount)\n";
  source += "  print_line(directLeft)\n";
  source += "  print_line(directRight)\n";
  source += "  print_line(directContainsBonus)\n";
  source += "  print_line(directMissingBonus)\n";
  source += "  print_line(wrappedCount)\n";
  source += "  print_line(wrappedCenter)\n";
  source += "  print_line(wrappedEdge)\n";
  source += "  print_line(wrappedContainsBonus)\n";
  source += "  print_line(wrappedMissingBonus)\n";
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

inline std::string makeExperimentalMapVariadicConstructorConformanceSource() {
  std::string source;
  source += "import /std/collections/experimental_map/*\n\n";
  source += "[effects(heap_alloc), return<Map<K, V>> Comparable<K>]\n";
  source += "wrapMap<K, V>([args<Entry<K, V>>] entries) {\n";
  source += "  [Map<K, V>] values{map<K, V>([spread] entries)}\n";
  source += "  return(values)\n";
  source += "}\n\n";
  source += "[effects(heap_alloc), return<int>]\n";
  source += "main() {\n";
  source += "  [Map<string, i32>] empty{map<string, i32>()}\n";
  source +=
      "  [Map<string, i32>] direct{map<string, i32>(entry(\"left\"raw_utf8, 4i32), entry(\"right\"raw_utf8, 7i32), entry(\"bonus\"raw_utf8, 9i32))}\n";
  source +=
      "  [Map<string, i32>] wrapped{wrapMap<string, i32>(entry(\"alpha\"raw_utf8, 2i32), entry(\"beta\"raw_utf8, 5i32))}\n";
  source += "  [i32 mut] total{plus(mapCount<string, i32>(empty), mapCount<string, i32>(direct))}\n";
  source += "  assign(total, plus(total, mapAt<string, i32>(direct, \"right\"raw_utf8)))\n";
  source += "  assign(total, plus(total, mapAtUnsafe<string, i32>(wrapped, \"beta\"raw_utf8)))\n";
  source += "  if(mapContains<string, i32>(direct, \"bonus\"raw_utf8),\n";
  source += "     then() { assign(total, plus(total, mapCount<string, i32>(wrapped))) },\n";
  source += "     else() { })\n";
  source += "  return(total)\n";
  source += "}\n";
  return source;
}

inline std::string makeExperimentalMapVariadicConstructorMismatchSource() {
  std::string source;
  source += "import /std/collections/experimental_map/*\n\n";
  source += "[effects(heap_alloc), return<int>]\n";
  source += "main() {\n";
  source +=
      "  [Map<string, i32>] values{map<string, i32>(entry(\"left\"raw_utf8, 4i32), entry(\"wrong\"raw_utf8, false))}\n";
  source += "  return(mapCount<string, i32>(values))\n";
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
  source += "[struct]\n";
  source += "Owned() {\n";
  source += "  [i32 mut] value{0i32}\n\n";
  source += "  [mut]\n";
  source += "  Move([Reference<Self>] other) {\n";
  source += "    assign(this.value, other.value)\n";
  source += "    assign(other.value, 0i32)\n";
  source += "  }\n\n";
  source += "  Destroy() {\n";
  source += "  }\n";
  source += "}\n\n";
  source += "[return<Reference<Map<string, Owned>>>]\n";
  source += "borrowExperimentalMap([Reference<Map<string, Owned>>] values) {\n";
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
  source +=
      "  [Map<string, Owned> mut] values{mapSingle<string, Owned>(\"left\"raw_utf8, Owned(4i32))}\n";
  source += "  borrowExperimentalMap(location(values)).insert(\"right\"raw_utf8, Owned(7i32))\n";
  source += "  borrowExperimentalMap(location(values)).insert(\"left\"raw_utf8, Owned(9i32))\n";
  source += "  borrowExperimentalMap(location(values)).insert(\"third\"raw_utf8, Owned(11i32))\n";
  source += "  [Owned] found{try(borrowExperimentalMap(location(values)).tryAt(\"left\"raw_utf8))}\n";
  source +=
      "  [Result<Owned, ContainerError>] missing{borrowExperimentalMap(location(values)).tryAt(\"missing\"raw_utf8)}\n";
  source += "  [i32 mut] total{plus(borrowExperimentalMap(location(values)).count(), found.value)}\n";
  source +=
      "  assign(total, plus(total, borrowExperimentalMap(location(values)).at(\"right\"raw_utf8).value))\n";
  source +=
      "  assign(total, plus(total, borrowExperimentalMap(location(values)).at_unsafe(\"third\"raw_utf8).value))\n";
  source += "  if(borrowExperimentalMap(location(values)).contains(\"right\"raw_utf8),\n";
  source += "     then() { assign(total, plus(total, 1i32)) },\n";
  source += "     else() { })\n";
  source += "  if(not(borrowExperimentalMap(location(values)).contains(\"missing\"raw_utf8)),\n";
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
  source += "[effects(io_out, heap_alloc), return<int>]\n";
  source += "main() {\n";
  source += "  [Map<string, i32> mut] values{mapSingle<string, i32>(\"left\"raw_utf8, 4i32)}\n";
  source += "  mapInsert<string, i32>(values, \"right\"raw_utf8, 7i32)\n";
  source += "  mapInsert<string, i32>(values, \"left\"raw_utf8, 9i32)\n";
  source += "  [Reference<Map<string, i32>> mut] ref{location(values)}\n";
  source += "  mapInsertRef<string, i32>(ref, \"third\"raw_utf8, 11i32)\n";
  source += "  mapInsertRef<string, i32>(ref, \"right\"raw_utf8, 13i32)\n";
  source += "  [i32] count{mapCount<string, i32>(values)}\n";
  source += "  [i32] left{mapAt<string, i32>(values, \"left\"raw_utf8)}\n";
  source += "  [i32] right{mapAtRef<string, i32>(ref, \"right\"raw_utf8)}\n";
  source += "  [i32] third{mapAt<string, i32>(values, \"third\"raw_utf8)}\n";
  source += "  print_line(count)\n";
  source += "  print_line(left)\n";
  source += "  print_line(right)\n";
  source += "  print_line(third)\n";
  source += "  return(plus(count, plus(left, plus(right, third))))\n";
  source += "}\n";
  return source;
}

inline std::string makeExperimentalMapStorageReferenceConformanceSource() {
  std::string source;
  source += "import /std/collections/*\n";
  source += "import /std/collections/experimental_map/*\n\n";
  source += "[effects(io_out, heap_alloc), return<int>]\n";
  source += "main() {\n";
  source += "  [uninitialized<Map<string, i32>> mut] storage{uninitialized<Map<string, i32>>()}\n";
  source += "  [Reference<uninitialized<Map<string, i32>>>] ref{location(storage)}\n";
  source +=
      "  init(dereference(ref), /std/collections/mapPair(\"left\"raw_utf8, 5i32, \"right\"raw_utf8, 8i32))\n";
  source += "  [Map<string, i32>] values{take(storage)}\n";
  source += "  [i32] count{/std/collections/map/count(values)}\n";
  source += "  [i32] left{/std/collections/map/at(values, \"left\"raw_utf8)}\n";
  source += "  print_line(count)\n";
  source += "  print_line(left)\n";
  source += "  return(plus(count, left))\n";
  source += "}\n";
  return source;
}

inline std::string makeExperimentalMapOwnershipConformanceSource() {
  std::string source;
  source += "import /std/collections/*\n";
  source += "import /std/collections/experimental_map/*\n\n";
  source += "[struct]\n";
  source += "Tracked() {\n";
  source += "  [i32 mut] value{0i32}\n";
  source += "\n";
  source += "  [mut]\n";
  source += "  Move([Reference<Self>] other) {\n";
  source += "    assign(this.value, other.value)\n";
  source += "  }\n\n";
  source += "  Destroy() {\n";
  source += "  }\n";
  source += "}\n\n";
  source += "[effects(heap_alloc), return<int>]\n";
  source += "main() {\n";
  source +=
      "  [Map<string, Tracked> mut] values{mapSingle<string, Tracked>(\"left\"raw_utf8, Tracked(4i32))}\n";
  source += "  mapInsert<string, Tracked>(values, \"right\"raw_utf8, Tracked(7i32))\n";
  source += "  return(plus(mapCount<string, Tracked>(values),\n";
  source += "      plus(mapAt<string, Tracked>(values, \"left\"raw_utf8).value,\n";
  source += "           mapAtUnsafe<string, Tracked>(values, \"right\"raw_utf8).value)))\n";
  source += "}\n";
  return source;
}

inline std::string makeCanonicalMapNamespaceExperimentalInsertConformanceSource() {
  std::string source;
  source += "import /std/collections/*\n";
  source += "import /std/collections/experimental_map/*\n\n";
  source += "[struct]\n";
  source += "Tracked() {\n";
  source += "  [i32 mut] value{0i32}\n";
  source += "\n";
  source += "  [mut]\n";
  source += "  Move([Reference<Self>] other) {\n";
  source += "    assign(this.value, other.value)\n";
  source += "    assign(other.value, 0i32)\n";
  source += "  }\n\n";
  source += "  Destroy() {\n";
  source += "  }\n";
  source += "}\n\n";
  source += "[effects(heap_alloc), return<int>]\n";
  source += "main() {\n";
  source +=
      "  [Map<string, Tracked> mut] values{mapSingle<string, Tracked>(\"left\"raw_utf8, Tracked(4i32))}\n";
  source +=
      "  /std/collections/map/insert<string, Tracked>(values, \"right\"raw_utf8, Tracked(7i32))\n";
  source +=
      "  /std/collections/map/insert<string, Tracked>(values, \"left\"raw_utf8, Tracked(9i32))\n";
  source += "  return(plus(/std/collections/map/count<string, Tracked>(values),\n";
  source += "      plus(/std/collections/map/at<string, Tracked>(values, \"left\"raw_utf8).value,\n";
  source += "           /std/collections/map/at_unsafe<string, Tracked>(values, \"right\"raw_utf8).value)))\n";
  source += "}\n";
  return source;
}

inline std::string makeBuiltinCanonicalMapInsertFirstGrowthConformanceSource() {
  std::string source;
  source += "import /std/collections/*\n\n";
  source += "[effects(heap_alloc), return<int>]\n";
  source += "main() {\n";
  source += "  [map<i32, i32> mut] values{map<i32, i32>()}\n";
  source += "  /std/collections/map/insert<i32, i32>(values, 1i32, 4i32)\n";
  source += "  values.insert(1i32, 7i32)\n";
  source += "  return(plus(values.count(), values.at(1i32)))\n";
  source += "}\n";
  return source;
}

inline std::string makeBuiltinCanonicalMapInsertRepeatedGrowthConformanceSource() {
  std::string source;
  source += "import /std/collections/*\n\n";
  source += "[effects(heap_alloc), return<int>]\n";
  source += "main() {\n";
  source += "  [map<i32, i32> mut] values{map<i32, i32>(1i32, 4i32, 2i32, 7i32, 3i32, 11i32, 4i32, 13i32, 5i32, 15i32, 6i32, 17i32, 7i32, 19i32, 8i32, 21i32, 9i32, 23i32, 10i32, 25i32, 11i32, 27i32, 12i32, 29i32, 13i32, 31i32, 14i32, 33i32, 15i32, 35i32, 16i32, 37i32, 17i32, 39i32, 18i32, 41i32, 19i32, 43i32, 20i32, 45i32)}\n";
  source += "  /std/collections/map/insert<i32, i32>(values, 21i32, 47i32)\n";
  source += "  values.insert(22i32, 49i32)\n";
  source += "  values.insert(1i32, 9i32)\n";
  source += "  return(plus(values.count(), plus(values.at(1i32), plus(values.at(10i32), plus(values.at(20i32), plus(values.at_unsafe(21i32), values.at(22i32)))))))\n";
  source += "}\n";
  return source;
}

inline std::string makeBuiltinCanonicalMapInsertPairGrowthConformanceSource() {
  std::string source;
  source += "import /std/collections/*\n\n";
  source += "[effects(heap_alloc), return<int>]\n";
  source += "main() {\n";
  source += "  [map<i32, i32> mut] values{map<i32, i32>(1i32, 4i32)}\n";
  source += "  /std/collections/map/insert<i32, i32>(values, 2i32, 7i32)\n";
  source += "  values.insert(1i32, 9i32)\n";
  source += "  return(plus(values.count(), plus(values.at(1i32), values.at_unsafe(2i32))))\n";
  source += "}\n";
  return source;
}

inline std::string makeBuiltinCanonicalMapInsertTripleGrowthConformanceSource() {
  std::string source;
  source += "import /std/collections/*\n\n";
  source += "[effects(heap_alloc), return<int>]\n";
  source += "main() {\n";
  source += "  [map<i32, i32> mut] values{map<i32, i32>(1i32, 4i32, 2i32, 7i32)}\n";
  source += "  /std/collections/map/insert<i32, i32>(values, 3i32, 11i32)\n";
  source += "  values.insert(1i32, 9i32)\n";
  source += "  return(plus(values.count(), plus(values.at(1i32), plus(values.at(2i32), values.at_unsafe(3i32)))))\n";
  source += "}\n";
  return source;
}

inline std::string makeBuiltinCanonicalMapInsertQuadGrowthConformanceSource() {
  std::string source;
  source += "import /std/collections/*\n\n";
  source += "[effects(heap_alloc), return<int>]\n";
  source += "main() {\n";
  source += "  [map<i32, i32> mut] values{map<i32, i32>(1i32, 4i32, 2i32, 7i32, 3i32, 11i32)}\n";
  source += "  /std/collections/map/insert<i32, i32>(values, 4i32, 13i32)\n";
  source += "  values.insert(1i32, 9i32)\n";
  source += "  return(plus(values.count(), plus(values.at(1i32), plus(values.at(2i32), plus(values.at(3i32), values.at_unsafe(4i32))))))\n";
  source += "}\n";
  return source;
}

inline std::string makeBuiltinCanonicalMapInsertQuintGrowthConformanceSource() {
  std::string source;
  source += "import /std/collections/*\n\n";
  source += "[effects(heap_alloc), return<int>]\n";
  source += "main() {\n";
  source += "  [map<i32, i32> mut] values{map<i32, i32>(1i32, 4i32, 2i32, 7i32, 3i32, 11i32, 4i32, 13i32)}\n";
  source += "  /std/collections/map/insert<i32, i32>(values, 5i32, 15i32)\n";
  source += "  values.insert(1i32, 9i32)\n";
  source += "  return(plus(values.count(), plus(values.at(1i32), plus(values.at(2i32), plus(values.at(3i32), plus(values.at(4i32), values.at_unsafe(5i32)))))))\n";
  source += "}\n";
  return source;
}

inline std::string makeBuiltinCanonicalMapInsertSextGrowthConformanceSource() {
  std::string source;
  source += "import /std/collections/*\n\n";
  source += "[effects(heap_alloc), return<int>]\n";
  source += "main() {\n";
  source += "  [map<i32, i32> mut] values{map<i32, i32>(1i32, 4i32, 2i32, 7i32, 3i32, 11i32, 4i32, 13i32, 5i32, 15i32)}\n";
  source += "  /std/collections/map/insert<i32, i32>(values, 6i32, 17i32)\n";
  source += "  values.insert(1i32, 9i32)\n";
  source += "  return(plus(values.count(), plus(values.at(1i32), plus(values.at(2i32), plus(values.at(3i32), plus(values.at(4i32), plus(values.at(5i32), values.at_unsafe(6i32))))))))\n";
  source += "}\n";
  return source;
}

inline std::string makeBuiltinCanonicalMapInsertSeptGrowthConformanceSource() {
  std::string source;
  source += "import /std/collections/*\n\n";
  source += "[effects(heap_alloc), return<int>]\n";
  source += "main() {\n";
  source += "  [map<i32, i32> mut] values{map<i32, i32>(1i32, 4i32, 2i32, 7i32, 3i32, 11i32, 4i32, 13i32, 5i32, 15i32, 6i32, 17i32)}\n";
  source += "  /std/collections/map/insert<i32, i32>(values, 7i32, 19i32)\n";
  source += "  values.insert(1i32, 9i32)\n";
  source += "  return(plus(values.count(), plus(values.at(1i32), plus(values.at(2i32), plus(values.at(3i32), plus(values.at(4i32), plus(values.at(5i32), plus(values.at(6i32), values.at_unsafe(7i32)))))))))\n";
  source += "}\n";
  return source;
}

inline std::string makeBuiltinCanonicalMapInsertOctGrowthConformanceSource() {
  std::string source;
  source += "import /std/collections/*\n\n";
  source += "[effects(heap_alloc), return<int>]\n";
  source += "main() {\n";
  source += "  [map<i32, i32> mut] values{map<i32, i32>(1i32, 4i32, 2i32, 7i32, 3i32, 11i32, 4i32, 13i32, 5i32, 15i32, 6i32, 17i32, 7i32, 19i32)}\n";
  source += "  /std/collections/map/insert<i32, i32>(values, 8i32, 21i32)\n";
  source += "  values.insert(1i32, 9i32)\n";
  source += "  return(plus(values.count(), plus(values.at(1i32), plus(values.at(2i32), plus(values.at(3i32), plus(values.at(4i32), plus(values.at(5i32), plus(values.at(6i32), plus(values.at(7i32), values.at_unsafe(8i32))))))))))\n";
  source += "}\n";
  return source;
}

inline std::string makeBuiltinCanonicalMapInsertNinthGrowthConformanceSource() {
  std::string source;
  source += "import /std/collections/*\n\n";
  source += "[effects(heap_alloc), return<int>]\n";
  source += "main() {\n";
  source += "  [map<i32, i32> mut] values{map<i32, i32>(1i32, 4i32, 2i32, 7i32, 3i32, 11i32, 4i32, 13i32, 5i32, 15i32, 6i32, 17i32, 7i32, 19i32, 8i32, 21i32)}\n";
  source += "  /std/collections/map/insert<i32, i32>(values, 9i32, 23i32)\n";
  source += "  values.insert(1i32, 9i32)\n";
  source += "  return(plus(values.count(), plus(values.at(1i32), plus(values.at(2i32), plus(values.at(3i32), plus(values.at(4i32), plus(values.at(5i32), plus(values.at(6i32), plus(values.at(7i32), plus(values.at(8i32), values.at_unsafe(9i32)))))))))))\n";
  source += "}\n";
  return source;
}

inline std::string makeBuiltinCanonicalMapInsertTenthGrowthConformanceSource() {
  std::string source;
  source += "import /std/collections/*\n\n";
  source += "[effects(heap_alloc), return<int>]\n";
  source += "main() {\n";
  source += "  [map<i32, i32> mut] values{map<i32, i32>(1i32, 4i32, 2i32, 7i32, 3i32, 11i32, 4i32, 13i32, 5i32, 15i32, 6i32, 17i32, 7i32, 19i32, 8i32, 21i32, 9i32, 23i32)}\n";
  source += "  /std/collections/map/insert<i32, i32>(values, 10i32, 25i32)\n";
  source += "  values.insert(1i32, 9i32)\n";
  source += "  return(plus(values.count(), plus(values.at(1i32), plus(values.at(2i32), plus(values.at(3i32), plus(values.at(4i32), plus(values.at(5i32), plus(values.at(6i32), plus(values.at(7i32), plus(values.at(8i32), plus(values.at(9i32), values.at_unsafe(10i32))))))))))))\n";
  source += "}\n";
  return source;
}

inline std::string makeBuiltinCanonicalMapInsertEleventhGrowthConformanceSource() {
  std::string source;
  source += "import /std/collections/*\n\n";
  source += "[effects(heap_alloc), return<int>]\n";
  source += "main() {\n";
  source += "  [map<i32, i32> mut] values{map<i32, i32>(1i32, 4i32, 2i32, 7i32, 3i32, 11i32, 4i32, 13i32, 5i32, 15i32, 6i32, 17i32, 7i32, 19i32, 8i32, 21i32, 9i32, 23i32, 10i32, 25i32)}\n";
  source += "  /std/collections/map/insert<i32, i32>(values, 11i32, 27i32)\n";
  source += "  values.insert(1i32, 9i32)\n";
  source += "  return(plus(values.count(), plus(values.at(1i32), plus(values.at(2i32), plus(values.at(3i32), plus(values.at(4i32), plus(values.at(5i32), plus(values.at(6i32), plus(values.at(7i32), plus(values.at(8i32), plus(values.at(9i32), plus(values.at(10i32), values.at_unsafe(11i32)))))))))))))\n";
  source += "}\n";
  return source;
}

inline std::string makeBuiltinCanonicalMapInsertTwelfthGrowthConformanceSource() {
  std::string source;
  source += "import /std/collections/*\n\n";
  source += "[effects(heap_alloc), return<int>]\n";
  source += "main() {\n";
  source += "  [map<i32, i32> mut] values{map<i32, i32>(1i32, 4i32, 2i32, 7i32, 3i32, 11i32, 4i32, 13i32, 5i32, 15i32, 6i32, 17i32, 7i32, 19i32, 8i32, 21i32, 9i32, 23i32, 10i32, 25i32, 11i32, 27i32)}\n";
  source += "  /std/collections/map/insert<i32, i32>(values, 12i32, 29i32)\n";
  source += "  values.insert(1i32, 9i32)\n";
  source += "  return(plus(values.count(), plus(values.at(1i32), plus(values.at(2i32), plus(values.at(3i32), plus(values.at(4i32), plus(values.at(5i32), plus(values.at(6i32), plus(values.at(7i32), plus(values.at(8i32), plus(values.at(9i32), plus(values.at(10i32), plus(values.at(11i32), values.at_unsafe(12i32))))))))))))))\n";
  source += "}\n";
  return source;
}

inline std::string makeBuiltinCanonicalMapInsertThirteenthGrowthConformanceSource() {
  std::string source;
  source += "import /std/collections/*\n\n";
  source += "[effects(heap_alloc), return<int>]\n";
  source += "main() {\n";
  source += "  [map<i32, i32> mut] values{map<i32, i32>(1i32, 4i32, 2i32, 7i32, 3i32, 11i32, 4i32, 13i32, 5i32, 15i32, 6i32, 17i32, 7i32, 19i32, 8i32, 21i32, 9i32, 23i32, 10i32, 25i32, 11i32, 27i32, 12i32, 29i32)}\n";
  source += "  /std/collections/map/insert<i32, i32>(values, 13i32, 31i32)\n";
  source += "  values.insert(1i32, 9i32)\n";
  source += "  return(plus(values.count(), plus(values.at(1i32), plus(values.at(3i32), plus(values.at(4i32), plus(values.at(5i32), plus(values.at(6i32), plus(values.at(7i32), plus(values.at(8i32), plus(values.at(9i32), plus(values.at(10i32), plus(values.at(11i32), plus(values.at(12i32), values.at_unsafe(13i32))))))))))))))\n";
  source += "}\n";
  return source;
}

inline std::string makeBuiltinCanonicalMapInsertFourteenthGrowthConformanceSource() {
  std::string source;
  source += "import /std/collections/*\n\n";
  source += "[effects(heap_alloc), return<int>]\n";
  source += "main() {\n";
  source += "  [map<i32, i32> mut] values{map<i32, i32>(1i32, 4i32, 2i32, 7i32, 3i32, 11i32, 4i32, 13i32, 5i32, 15i32, 6i32, 17i32, 7i32, 19i32, 8i32, 21i32, 9i32, 23i32, 10i32, 25i32, 11i32, 27i32, 12i32, 29i32, 13i32, 31i32)}\n";
  source += "  /std/collections/map/insert<i32, i32>(values, 14i32, 33i32)\n";
  source += "  values.insert(1i32, 9i32)\n";
  source += "  return(plus(values.count(), plus(values.at(1i32), plus(values.at(5i32), plus(values.at(9i32), plus(values.at(13i32), values.at_unsafe(14i32)))))))\n";
  source += "}\n";
  return source;
}

inline std::string makeBuiltinCanonicalMapInsertFifteenthGrowthConformanceSource() {
  std::string source;
  source += "import /std/collections/*\n\n";
  source += "[effects(heap_alloc), return<int>]\n";
  source += "main() {\n";
  source += "  [map<i32, i32> mut] values{map<i32, i32>(1i32, 4i32, 2i32, 7i32, 3i32, 11i32, 4i32, 13i32, 5i32, 15i32, 6i32, 17i32, 7i32, 19i32, 8i32, 21i32, 9i32, 23i32, 10i32, 25i32, 11i32, 27i32, 12i32, 29i32, 13i32, 31i32, 14i32, 33i32)}\n";
  source += "  /std/collections/map/insert<i32, i32>(values, 15i32, 35i32)\n";
  source += "  values.insert(1i32, 9i32)\n";
  source += "  return(plus(values.count(), plus(values.at(1i32), plus(values.at(5i32), plus(values.at(10i32), plus(values.at(14i32), values.at_unsafe(15i32)))))))\n";
  source += "}\n";
  return source;
}

inline std::string makeBuiltinCanonicalMapInsertSixteenthGrowthConformanceSource() {
  std::string source;
  source += "import /std/collections/*\n\n";
  source += "[effects(heap_alloc), return<int>]\n";
  source += "main() {\n";
  source += "  [map<i32, i32> mut] values{map<i32, i32>(1i32, 4i32, 2i32, 7i32, 3i32, 11i32, 4i32, 13i32, 5i32, 15i32, 6i32, 17i32, 7i32, 19i32, 8i32, 21i32, 9i32, 23i32, 10i32, 25i32, 11i32, 27i32, 12i32, 29i32, 13i32, 31i32, 14i32, 33i32, 15i32, 35i32)}\n";
  source += "  /std/collections/map/insert<i32, i32>(values, 16i32, 37i32)\n";
  source += "  values.insert(1i32, 9i32)\n";
  source += "  return(plus(values.count(), plus(values.at(1i32), plus(values.at(5i32), plus(values.at(10i32), plus(values.at(15i32), values.at_unsafe(16i32)))))))\n";
  source += "}\n";
  return source;
}

inline std::string makeBuiltinCanonicalMapInsertSeventeenthGrowthConformanceSource() {
  std::string source;
  source += "import /std/collections/*\n\n";
  source += "[effects(heap_alloc), return<int>]\n";
  source += "main() {\n";
  source += "  [map<i32, i32> mut] values{map<i32, i32>(1i32, 4i32, 2i32, 7i32, 3i32, 11i32, 4i32, 13i32, 5i32, 15i32, 6i32, 17i32, 7i32, 19i32, 8i32, 21i32, 9i32, 23i32, 10i32, 25i32, 11i32, 27i32, 12i32, 29i32, 13i32, 31i32, 14i32, 33i32, 15i32, 35i32, 16i32, 37i32)}\n";
  source += "  /std/collections/map/insert<i32, i32>(values, 17i32, 39i32)\n";
  source += "  values.insert(1i32, 9i32)\n";
  source += "  return(plus(values.count(), plus(values.at(1i32), plus(values.at(5i32), plus(values.at(10i32), plus(values.at(16i32), values.at_unsafe(17i32)))))))\n";
  source += "}\n";
  return source;
}

inline std::string makeBuiltinCanonicalMapInsertEighteenthGrowthConformanceSource() {
  std::string source;
  source += "import /std/collections/*\n\n";
  source += "[effects(heap_alloc), return<int>]\n";
  source += "main() {\n";
  source += "  [map<i32, i32> mut] values{map<i32, i32>(1i32, 4i32, 2i32, 7i32, 3i32, 11i32, 4i32, 13i32, 5i32, 15i32, 6i32, 17i32, 7i32, 19i32, 8i32, 21i32, 9i32, 23i32, 10i32, 25i32, 11i32, 27i32, 12i32, 29i32, 13i32, 31i32, 14i32, 33i32, 15i32, 35i32, 16i32, 37i32, 17i32, 39i32)}\n";
  source += "  /std/collections/map/insert<i32, i32>(values, 18i32, 41i32)\n";
  source += "  values.insert(1i32, 9i32)\n";
  source += "  return(plus(values.count(), plus(values.at(1i32), plus(values.at(5i32), plus(values.at(10i32), plus(values.at(17i32), values.at_unsafe(18i32)))))))\n";
  source += "}\n";
  return source;
}

inline std::string makeBuiltinCanonicalMapInsertNineteenthGrowthConformanceSource() {
  std::string source;
  source += "import /std/collections/*\n\n";
  source += "[effects(heap_alloc), return<int>]\n";
  source += "main() {\n";
  source += "  [map<i32, i32> mut] values{map<i32, i32>(1i32, 4i32, 2i32, 7i32, 3i32, 11i32, 4i32, 13i32, 5i32, 15i32, 6i32, 17i32, 7i32, 19i32, 8i32, 21i32, 9i32, 23i32, 10i32, 25i32, 11i32, 27i32, 12i32, 29i32, 13i32, 31i32, 14i32, 33i32, 15i32, 35i32, 16i32, 37i32, 17i32, 39i32, 18i32, 41i32)}\n";
  source += "  /std/collections/map/insert<i32, i32>(values, 19i32, 43i32)\n";
  source += "  values.insert(1i32, 9i32)\n";
  source += "  return(plus(values.count(), plus(values.at(1i32), plus(values.at(5i32), plus(values.at(10i32), plus(values.at(18i32), values.at_unsafe(19i32)))))))\n";
  source += "}\n";
  return source;
}

inline std::string makeBuiltinCanonicalMapInsertTwentiethGrowthConformanceSource() {
  std::string source;
  source += "import /std/collections/*\n\n";
  source += "[effects(heap_alloc), return<int>]\n";
  source += "main() {\n";
  source += "  [map<i32, i32> mut] values{map<i32, i32>(1i32, 4i32, 2i32, 7i32, 3i32, 11i32, 4i32, 13i32, 5i32, 15i32, 6i32, 17i32, 7i32, 19i32, 8i32, 21i32, 9i32, 23i32, 10i32, 25i32, 11i32, 27i32, 12i32, 29i32, 13i32, 31i32, 14i32, 33i32, 15i32, 35i32, 16i32, 37i32, 17i32, 39i32, 18i32, 41i32, 19i32, 43i32)}\n";
  source += "  /std/collections/map/insert<i32, i32>(values, 20i32, 45i32)\n";
  source += "  values.insert(1i32, 9i32)\n";
  source += "  return(plus(values.count(), plus(values.at(1i32), plus(values.at(5i32), plus(values.at(10i32), plus(values.at(19i32), values.at_unsafe(20i32)))))))\n";
  source += "}\n";
  return source;
}

inline std::string makeBuiltinCanonicalMapInsertOverwriteConformanceSource() {
  std::string source;
  source += "import /std/collections/*\n\n";
  source += "[effects(heap_alloc), return<int>]\n";
  source += "main() {\n";
  source += "  [map<i32, i32> mut] values{map<i32, i32>(1i32, 4i32, 2i32, 7i32)}\n";
  source += "  /std/collections/map/insert<i32, i32>(values, 1i32, 9i32)\n";
  source += "  values.insert(2i32, 11i32)\n";
  source += "  return(plus(values.count(), plus(values.at(1i32), values.at_unsafe(2i32))))\n";
  source += "}\n";
  return source;
}

inline std::string makeBuiltinCanonicalMapInsertNonLocalGrowthConformanceSource() {
  std::string source;
  source += "import /std/collections/*\n\n";
  source += "[struct]\n";
  source += "Holder() {\n";
  source += "  [map<i32, i32> mut] values{map<i32, i32>()}\n";
  source += "}\n\n";
  source += "[effects(heap_alloc), return<int>]\n";
  source += "main() {\n";
  source += "  [Holder mut] holder{Holder()}\n";
  source += "  /std/collections/map/insert<i32, i32>(holder.values, 1i32, 4i32)\n";
  source += "  holder.values.insert(2i32, 7i32)\n";
  source += "  [Reference<map<i32, i32>> mut] ref{location(holder.values)}\n";
  source += "  /std/collections/map/insert_ref<i32, i32>(ref, 3i32, 11i32)\n";
  source += "  /std/collections/map/insert_ref<i32, i32>(ref, 2i32, 13i32)\n";
  source += "  return(plus(holder.values.count(), plus(holder.values.at(1i32), plus(holder.values.at_unsafe(2i32), holder.values.at(3i32)))))\n";
  source += "}\n";
  return source;
}

inline std::string makeBuiltinCanonicalMapInsertNestedNonLocalGrowthConformanceSource() {
  std::string source;
  source += "import /std/collections/*\n\n";
  source += "[struct]\n";
  source += "Holder() {\n";
  source += "  [map<i32, i32> mut] values{map<i32, i32>()}\n";
  source += "}\n\n";
  source += "[struct]\n";
  source += "Outer() {\n";
  source += "  [Holder mut] holder{Holder()}\n";
  source += "}\n\n";
  source += "[return<Reference<map<i32, i32>>>]\n";
  source += "borrowValues([Reference<map<i32, i32>>] values) {\n";
  source += "  return(values)\n";
  source += "}\n\n";
  source += "[effects(heap_alloc), return<int>]\n";
  source += "main() {\n";
  source += "  [Outer mut] outer{Outer()}\n";
  source += "  /std/collections/map/insert<i32, i32>(outer.holder.values, 1i32, 4i32)\n";
  source += "  /std/collections/map/insert<i32, i32>(outer.holder.values, 2i32, 7i32)\n";
  source += "  [Reference<map<i32, i32>> mut] ref{borrowValues(location(outer.holder.values))}\n";
  source += "  /std/collections/map/insert_ref<i32, i32>(ref, 3i32, 11i32)\n";
  source += "  /std/collections/map/insert_ref<i32, i32>(ref, 2i32, 13i32)\n";
  source +=
      "  return(plus(outer.holder.values.count(), plus(outer.holder.values.at(1i32), plus(outer.holder.values.at_unsafe(2i32), outer.holder.values.at(3i32)))))\n";
  source += "}\n";
  return source;
}

inline std::string makeBuiltinCanonicalMapInsertHelperReturnBorrowedMethodConformanceSource() {
  std::string source;
  source += "import /std/collections/*\n\n";
  source += "[struct]\n";
  source += "Holder() {\n";
  source += "  [map<i32, i32> mut] values{map<i32, i32>()}\n";
  source += "}\n\n";
  source += "[return<Reference<map<i32, i32>>>]\n";
  source += "borrowValues([Reference<map<i32, i32>>] values) {\n";
  source += "  return(values)\n";
  source += "}\n\n";
  source += "[effects(heap_alloc), return<int>]\n";
  source += "main() {\n";
  source += "  [Holder mut] holder{Holder()}\n";
  source += "  [Reference<map<i32, i32>> mut] ref{borrowValues(location(holder.values))}\n";
  source += "  ref.insert(1i32, 4i32)\n";
  source += "  ref.insert(2i32, 7i32)\n";
  source += "  ref.insert(1i32, 9i32)\n";
  source += "  return(plus(holder.values.count(), plus(holder.values.at(1i32), holder.values.at_unsafe(2i32))))\n";
  source += "}\n";
  return source;
}

inline std::string makeBuiltinCanonicalMapStructFieldInitializerConformanceSource() {
  std::string source;
  source += "import /std/collections/*\n\n";
  source += "[struct]\n";
  source += "Holder() {\n";
  source += "  [map<i32, i32> mut] values{map<i32, i32>()}\n";
  source += "}\n\n";
  source += "[effects(heap_alloc), return<int>]\n";
  source += "main() {\n";
  source += "  [Holder mut] holder{Holder()}\n";
  source += "  [Reference<map<i32, i32>>] valuesRef{location(holder.values)}\n";
  source += "  return(/std/collections/map/count<i32, i32>(valuesRef))\n";
  source += "}\n";
  return source;
}

inline std::string makeBuiltinCanonicalMapInsertHelperReturnValueDirectRejectSource() {
  std::string source;
  source += "import /std/collections/*\n\n";
  source += "[return<map<i32, i32>> effects(heap_alloc)]\n";
  source += "makeValues() {\n";
  source += "  [map<i32, i32>] values{map<i32, i32>()}\n";
  source += "  return(values)\n";
  source += "}\n\n";
  source += "[effects(heap_alloc), return<int>]\n";
  source += "main() {\n";
  source += "  /std/collections/map/insert<i32, i32>(makeValues(), 1i32, 4i32)\n";
  source += "  return(0i32)\n";
  source += "}\n";
  return source;
}

inline std::string makeBuiltinCanonicalMapInsertHelperReturnValueMethodRejectSource() {
  std::string source;
  source += "import /std/collections/*\n\n";
  source += "[return<map<i32, i32>> effects(heap_alloc)]\n";
  source += "makeValues() {\n";
  source += "  [map<i32, i32>] values{map<i32, i32>()}\n";
  source += "  return(values)\n";
  source += "}\n\n";
  source += "[effects(heap_alloc), return<int>]\n";
  source += "main() {\n";
  source += "  makeValues().insert(1i32, 4i32)\n";
  source += "  return(0i32)\n";
  source += "}\n";
  return source;
}

inline std::string makeBuiltinCanonicalMapInsertBorrowedHolderFieldDirectRejectSource() {
  std::string source;
  source += "import /std/collections/*\n\n";
  source += "[struct]\n";
  source += "Holder() {\n";
  source += "  [map<i32, i32> mut] values{map<i32, i32>()}\n";
  source += "}\n\n";
  source += "[return<Reference<Holder>>]\n";
  source += "borrowHolder([Reference<Holder>] holder) {\n";
  source += "  return(holder)\n";
  source += "}\n\n";
  source += "[effects(heap_alloc), return<int>]\n";
  source += "main() {\n";
  source += "  [Holder mut] holder{Holder()}\n";
  source += "  /std/collections/map/insert<i32, i32>(borrowHolder(location(holder)).values, 1i32, 4i32)\n";
  source += "  return(0i32)\n";
  source += "}\n";
  return source;
}

inline std::string makeExperimentalMapOwnershipMethodConformanceSource() {
  std::string source;
  source += "import /std/collections/*\n";
  source += "import /std/collections/experimental_map/*\n\n";
  source += "[struct]\n";
  source += "Tracked() {\n";
  source += "  [i32 mut] value{0i32}\n";
  source += "\n";
  source += "  [mut]\n";
  source += "  Move([Reference<Self>] other) {\n";
  source += "    assign(this.value, other.value)\n";
  source += "    assign(other.value, 0i32)\n";
  source += "  }\n\n";
  source += "  Destroy() {\n";
  source += "  }\n";
  source += "}\n\n";
  source += "[effects(io_err)]\n";
  source += "unexpectedExperimentalMapOwnershipMethodError([ContainerError] err) {\n";
  source += "  [Result<ContainerError>] status{err.code}\n";
  source += "  print_line_error(Result.why(status))\n";
  source += "}\n\n";
  source +=
      "[return<Result<int, ContainerError>> effects(io_out, heap_alloc) on_error<ContainerError, /unexpectedExperimentalMapOwnershipMethodError>]\n";
  source += "main() {\n";
  source +=
      "  [Map<string, Tracked> mut] values{mapSingle<string, Tracked>(\"left\"raw_utf8, Tracked(4i32))}\n";
  source += "  values.insert(\"right\"raw_utf8, Tracked(7i32))\n";
  source += "  values.insert(\"left\"raw_utf8, Tracked(9i32))\n";
  source += "  values.insert(\"third\"raw_utf8, Tracked(11i32))\n";
  source += "  [Tracked] found{try(values.tryAt(\"left\"raw_utf8))}\n";
  source +=
      "  [Result<Tracked, ContainerError>] missing{values.tryAt(\"missing\"raw_utf8)}\n";
  source += "  [i32 mut] total{plus(values.count(), found.value)}\n";
  source += "  assign(total, plus(total, values.at(\"right\"raw_utf8).value))\n";
  source += "  assign(total, plus(total, values.at_unsafe(\"third\"raw_utf8).value))\n";
  source += "  if(values.contains(\"right\"raw_utf8),\n";
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
  source += "  [i32] count{/std/collections/map/count<string, i32>(values)}\n";
  source += "  [i32] left{/std/collections/map/at<string, i32>(values, \"left\"raw_utf8)}\n";
  source += "  [i32] right{/std/collections/map/at_unsafe<string, i32>(values, \"right\"raw_utf8)}\n";
  source += "  [i32 mut] total{plus(count, found)}\n";
  source += "  assign(total, plus(total, left))\n";
  source += "  assign(total, plus(total, right))\n";
  source += "  [i32 mut] containsBonus{0i32}\n";
  source += "  if(/std/collections/map/contains<string, i32>(values, \"left\"raw_utf8),\n";
  source += "     then() { assign(containsBonus, 1i32) assign(total, plus(total, containsBonus)) },\n";
  source += "     else() { })\n";
  source += "  [i32 mut] missingBonus{0i32}\n";
  source += "  if(not(/std/collections/map/contains<string, i32>(values, \"missing\"raw_utf8)),\n";
  source += "     then() { assign(missingBonus, 2i32) assign(total, plus(total, missingBonus)) },\n";
  source += "     else() { })\n";
  source += "  print_line(found)\n";
  source += "  print_line(Result.why(missing))\n";
  source += "  print_line(count)\n";
  source += "  print_line(left)\n";
  source += "  print_line(right)\n";
  source += "  print_line(containsBonus)\n";
  source += "  print_line(missingBonus)\n";
  source += "  return(Result.ok(total))\n";
  source += "}\n";
  return source;
}

inline std::string makeCanonicalMapNamespaceOwnershipRejectSource() {
  std::string source;
  source += "import /std/collections/*\n\n";
  source += "[struct]\n";
  source += "Owned() {\n";
  source += "  [i32 mut] value{0i32}\n\n";
  source += "  [mut]\n";
  source += "  Move([Reference<Self>] other) {\n";
  source += "    assign(this.value, other.value)\n";
  source += "    assign(other.value, 0i32)\n";
  source += "  }\n";
  source += "}\n\n";
  source += "[effects(heap_alloc), return<int>]\n";
  source += "main() {\n";
  source +=
      "  [map<string, Owned>] values{/std/collections/map/map<string, Owned>(\"left\"raw_utf8, Owned(4i32), "
      "\"right\"raw_utf8, Owned(7i32))}\n";
  source += "  return(/std/collections/map/count<string, Owned>(values))\n";
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
  source += "  [i32] count{/std/collections/map/count<string, i32>(values)}\n";
  source += "  [i32] left{/std/collections/map/at<string, i32>(values, \"left\"raw_utf8)}\n";
  source += "  [i32] right{/std/collections/map/at_unsafe<string, i32>(values, \"right\"raw_utf8)}\n";
  source += "  [i32 mut] total{plus(count, found)}\n";
  source += "  assign(total, plus(total, left))\n";
  source +=
      "  assign(total, plus(total, right))\n";
  source += "  [i32 mut] containsBonus{0i32}\n";
  source += "  if(/std/collections/map/contains<string, i32>(values, \"left\"raw_utf8),\n";
  source += "     then() { assign(containsBonus, 1i32) assign(total, plus(total, containsBonus)) },\n";
  source += "     else() { })\n";
  source += "  [i32 mut] missingBonus{0i32}\n";
  source += "  if(not(/std/collections/map/contains<string, i32>(values, \"missing\"raw_utf8)),\n";
  source += "     then() { assign(missingBonus, 2i32) assign(total, plus(total, missingBonus)) },\n";
  source += "     else() { })\n";
  source += "  print_line(found)\n";
  source += "  print_line(Result.why(missing))\n";
  source += "  print_line(count)\n";
  source += "  print_line(left)\n";
  source += "  print_line(right)\n";
  source += "  print_line(containsBonus)\n";
  source += "  print_line(missingBonus)\n";
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
  source += "  [i32] count{/std/collections/map/count<string, i32>(values)}\n";
  source += "  [i32] left{/std/collections/map/at<string, i32>(values, \"left\"raw_utf8)}\n";
  source += "  [i32] right{/std/collections/map/at_unsafe<string, i32>(values, \"right\"raw_utf8)}\n";
  source += "  [i32 mut] total{plus(count, found)}\n";
  source += "  assign(total, plus(total, left))\n";
  source +=
      "  assign(total, plus(total, right))\n";
  source += "  [i32 mut] containsBonus{0i32}\n";
  source += "  if(/std/collections/map/contains<string, i32>(values, \"left\"raw_utf8),\n";
  source += "     then() { assign(containsBonus, 1i32) assign(total, plus(total, containsBonus)) },\n";
  source += "     else() { })\n";
  source += "  [i32 mut] missingBonus{0i32}\n";
  source += "  if(not(/std/collections/map/contains<string, i32>(values, \"missing\"raw_utf8)),\n";
  source += "     then() { assign(missingBonus, 2i32) assign(total, plus(total, missingBonus)) },\n";
  source += "     else() { })\n";
  source += "  print_line(found)\n";
  source += "  print_line(Result.why(missing))\n";
  source += "  print_line(count)\n";
  source += "  print_line(left)\n";
  source += "  print_line(right)\n";
  source += "  print_line(containsBonus)\n";
  source += "  print_line(missingBonus)\n";
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
  source +=
      "  [Map<string, i32>] out{/std/collections/map/map<string, i32>(\"left\"raw_utf8, 4i32, \"right\"raw_utf8, 7i32)}\n";
  source += "  return(out)\n";
  source += "}\n\n";
  source +=
      "[return<Result<int, ContainerError>> effects(io_out, heap_alloc) on_error<ContainerError, /unexpectedCanonicalExperimentalMapReturnError>]\n";
  source += "main() {\n";
  source += "  [Map<string, i32>] values{buildValues()}\n";
  source += "  [i32] found{try(/std/collections/map/tryAt<string, i32>(values, \"left\"raw_utf8))}\n";
  source += "  [i32] count{/std/collections/map/count<string, i32>(values)}\n";
  source += "  [i32] left{/std/collections/map/at<string, i32>(values, \"left\"raw_utf8)}\n";
  source += "  [i32] right{/std/collections/map/at_unsafe<string, i32>(values, \"right\"raw_utf8)}\n";
  source += "  [i32 mut] total{plus(count, found)}\n";
  source += "  assign(total, plus(total, left))\n";
  source +=
      "  assign(total, plus(total, right))\n";
  source += "  [i32 mut] containsBonus{0i32}\n";
  source += "  if(/std/collections/map/contains<string, i32>(values, \"left\"raw_utf8),\n";
  source += "     then() { assign(containsBonus, 1i32) assign(total, plus(total, containsBonus)) },\n";
  source += "     else() { })\n";
  source += "  print_line(count)\n";
  source += "  print_line(found)\n";
  source += "  print_line(left)\n";
  source += "  print_line(right)\n";
  source += "  print_line(containsBonus)\n";
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
  source += "  [i32] count{/std/collections/map/count<string, i32>(values)}\n";
  source += "  [i32] left{/std/collections/map/at<string, i32>(values, \"left\"raw_utf8)}\n";
  source += "  [i32] right{/std/collections/map/at_unsafe<string, i32>(values, \"right\"raw_utf8)}\n";
  source += "  [i32 mut] total{plus(count, found)}\n";
  source += "  assign(total, plus(total, left))\n";
  source +=
      "  assign(total, plus(total, right))\n";
  source += "  [i32 mut] containsBonus{0i32}\n";
  source += "  if(/std/collections/map/contains<string, i32>(values, \"left\"raw_utf8),\n";
  source += "     then() { assign(containsBonus, 1i32) assign(total, plus(total, containsBonus)) },\n";
  source += "     else() { })\n";
  source += "  print_line(count)\n";
  source += "  print_line(found)\n";
  source += "  print_line(left)\n";
  source += "  print_line(right)\n";
  source += "  print_line(containsBonus)\n";
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
