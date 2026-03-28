#pragma once

inline std::string makeCanonicalMapNamespaceExperimentalBorrowedRefConformanceSource() {
  std::string source;
  source += "import /std/collections/*\n";
  source += "import /std/collections/experimental_map/*\n\n";
  source += "[effects(io_err)]\n";
  source += "unexpectedCanonicalExperimentalMapBorrowedRefError([ContainerError] err) {\n";
  source += "  [Result<ContainerError>] status{err.code}\n";
  source += "  print_line_error(Result.why(status))\n";
  source += "}\n\n";
  source +=
      "[return<Result<int, ContainerError>> effects(io_out, heap_alloc) on_error<ContainerError, /unexpectedCanonicalExperimentalMapBorrowedRefError>]\n";
  source += "main() {\n";
  source += "  [Map<string, i32>] values{mapPair<string, i32>(\"left\"raw_utf8, 4i32, \"right\"raw_utf8, 7i32)}\n";
  source += "  [Reference<Map<string, i32>>] ref{location(values)}\n";
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
  source += "  [i32] count{/std/collections/map/count<string, i32>([values] values)}\n";
  source += "  [i32] right{/std/collections/map/at<string, i32>([key] \"right\"raw_utf8, [values] values)}\n";
  source += "  [i32] left{/std/collections/map/at_unsafe<string, i32>([key] \"left\"raw_utf8, [values] values)}\n";
  source += "  [i32 mut] total{count}\n";
  source +=
      "  assign(total, plus(total, right))\n";
  source +=
      "  assign(total, plus(total, left))\n";
  source += "  [i32 mut] containsBonus{0i32}\n";
  source += "  if(/std/collections/map/contains<string, i32>([key] \"right\"raw_utf8, [values] values),\n";
  source += "     then() { assign(containsBonus, found) assign(total, plus(total, containsBonus)) },\n";
  source += "     else() { })\n";
  source += "  print_line(count)\n";
  source += "  print_line(right)\n";
  source += "  print_line(left)\n";
  source += "  print_line(containsBonus)\n";
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
  source += "import /std/collections/*\n\n";
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

