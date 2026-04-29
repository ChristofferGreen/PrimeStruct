#pragma once

inline std::string makeWrapperMapConstructorExperimentalBindingConformanceSource() {
  std::string source;
  source += "import /std/collections/*\n";
  source += "import /std/collections/map/*\n";
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

inline std::string makeWrapperMapConstructorExperimentalReturnConformanceSource() {
  std::string source;
  source += "import /std/collections/*\n";
  source += "import /std/collections/map/*\n";
  source += "import /std/collections/experimental_map/*\n\n";
  source += "[effects(io_err)]\n";
  source += "unexpectedWrapperExperimentalMapReturnError([ContainerError] err) {\n";
  source += "  [Result<ContainerError>] status{err.code}\n";
  source += "  print_line_error(Result.why(status))\n";
  source += "}\n\n";
  source += "[return<Map<string, i32>> effects(heap_alloc)]\n";
  source += "buildValues() {\n";
  source +=
      "  [Map<string, i32>] out{/std/collections/mapPair<string, i32>(\"left\"raw_utf8, 4i32, \"right\"raw_utf8, 7i32)}\n";
  source += "  return(out)\n";
  source += "}\n\n";
  source +=
      "[return<Result<int, ContainerError>> effects(io_out, heap_alloc) on_error<ContainerError, /unexpectedWrapperExperimentalMapReturnError>]\n";
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

inline std::string makeWrapperMapConstructorExperimentalParameterConformanceSource() {
  std::string source;
  source += "import /std/collections/*\n";
  source += "import /std/collections/map/*\n";
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
      "[return<Result<int, ContainerError>> effects(io_out, heap_alloc) on_error<ContainerError, /unexpectedWrapperExperimentalMapParameterError>]\n";
  source += "main() {\n";
  source +=
      "  return(scoreValues(/std/collections/mapPair<string, i32>(\"left\"raw_utf8, 4i32, \"right\"raw_utf8, 7i32)))\n";
  source += "}\n";
  return source;
}

inline std::string makeWrappedExperimentalMapParameterConformanceSource() {
  std::string source;
  source += "import /std/collections/*\n";
  source += "import /std/collections/map/*\n";
  source += "import /std/collections/experimental_map/*\n\n";
  source += "Holder() {}\n\n";
  source += "[return<Map<string, i32>> effects(heap_alloc)]\n";
  source += "wrapPrimaryValues() {\n";
  source +=
      "  [Map<string, i32>] out{/std/collections/experimental_map/mapPair<string, i32>(\"left\"raw_utf8, 4i32, \"right\"raw_utf8, 7i32)}\n";
  source += "  return(out)\n";
  source += "}\n\n";
  source += "[return<Map<string, i32>> effects(heap_alloc)]\n";
  source += "wrapSecondaryValues() {\n";
  source +=
      "  [Map<string, i32>] out{/std/collections/experimental_map/mapPair<string, i32>(\"left\"raw_utf8, 2i32, \"extra\"raw_utf8, 9i32)}\n";
  source += "  return(out)\n";
  source += "}\n\n";
  source += "[return<int> effects(io_out, heap_alloc)]\n";
  source += "scoreValues([Map<string, i32> mut] values) {\n";
  source += "  mapInsert<string, i32>(values, \"extra\"raw_utf8, 9i32)\n";
  source += "  [i32] count{/std/collections/map/count(values)}\n";
  source += "  [i32] left{/std/collections/map/at(values, \"left\"raw_utf8)}\n";
  source += "  print_line(count)\n";
  source += "  print_line(left)\n";
  source += "  return(plus(count, left))\n";
  source += "}\n\n";
  source += "[return<int> effects(io_out, heap_alloc)]\n";
  source += "/Holder/score([Holder] self, [Map<string, i32> mut] values) {\n";
  source += "  mapInsert<string, i32>(values, \"bonus\"raw_utf8, 5i32)\n";
  source += "  [i32] count{/std/collections/map/count(values)}\n";
  source += "  [i32] extra{/std/collections/map/at(values, \"extra\"raw_utf8)}\n";
  source += "  print_line(count)\n";
  source += "  print_line(extra)\n";
  source += "  return(plus(count, extra))\n";
  source += "}\n\n";
  source += "[effects(io_out, heap_alloc), return<int>]\n";
  source += "main() {\n";
  source += "  [Holder] holder{Holder()}\n";
  source += "  [Map<string, i32>] primary{wrapPrimaryValues()}\n";
  source += "  [Map<string, i32>] secondary{wrapSecondaryValues()}\n";
  source += "  return(plus(scoreValues(primary), holder.score(secondary)))\n";
  source += "}\n";
  return source;
}

inline std::string makeWrappedExperimentalMapBindingConformanceSource() {
  std::string source;
  source += "import /std/collections/*\n";
  source += "import /std/collections/map/*\n";
  source += "import /std/collections/experimental_map/*\n\n";
  source += "[return<T> effects(heap_alloc)]\n";
  source += "wrapValues<T>([T] values) {\n";
  source += "  return(values)\n";
  source += "}\n\n";
  source += "[effects(io_err)]\n";
  source += "unexpectedWrappedExperimentalMapBindingError([ContainerError] err) {\n";
  source += "  [Result<ContainerError>] status{err.code}\n";
  source += "  print_line_error(Result.why(status))\n";
  source += "}\n\n";
  source +=
      "[return<Result<int, ContainerError>> effects(io_out, heap_alloc) on_error<ContainerError, /unexpectedWrappedExperimentalMapBindingError>]\n";
  source += "main() {\n";
  source +=
      "  [Map<string, i32>] explicit{wrapValues(/std/collections/mapPair(\"left\"raw_utf8, 4i32, \"right\"raw_utf8, 7i32))}\n";
  source +=
      "  [auto] inferred{wrapValues(/std/collections/map/map(\"extra\"raw_utf8, 9i32, \"other\"raw_utf8, 2i32))}\n";
  source += "  [i32] left{try(/std/collections/map/tryAt(explicit, \"left\"raw_utf8))}\n";
  source += "  [i32] extra{try(/std/collections/map/tryAt(inferred, \"extra\"raw_utf8))}\n";
  source += "  print_line(left)\n";
  source += "  print_line(extra)\n";
  source += "  return(Result.ok(plus(left, extra)))\n";
  source += "}\n";
  return source;
}

inline std::string makeWrappedExperimentalMapAssignConformanceSource() {
  std::string source;
  source += "import /std/collections/*\n";
  source += "import /std/collections/map/*\n";
  source += "import /std/collections/experimental_map/*\n\n";
  source += "[return<T> effects(heap_alloc)]\n";
  source += "wrapValues<T>([T] values) {\n";
  source += "  return(values)\n";
  source += "}\n\n";
  source += "[effects(io_err)]\n";
  source += "unexpectedWrappedExperimentalMapAssignError([ContainerError] err) {\n";
  source += "  [Result<ContainerError>] status{err.code}\n";
  source += "  print_line_error(Result.why(status))\n";
  source += "}\n\n";
  source +=
      "[return<Result<int, ContainerError>> effects(io_out, heap_alloc) on_error<ContainerError, /unexpectedWrappedExperimentalMapAssignError>]\n";
  source += "main() {\n";
  source += "  [Map<string, i32> mut] values{mapNew<string, i32>()}\n";
  source +=
      "  assign(values, wrapValues(/std/collections/mapPair(\"left\"raw_utf8, 4i32, \"right\"raw_utf8, 7i32)))\n";
  source += "  [i32] first{try(/std/collections/map/tryAt(values, \"left\"raw_utf8))}\n";
  source +=
      "  assign(values, wrapValues(/std/collections/map/map(\"extra\"raw_utf8, 9i32, \"other\"raw_utf8, 2i32)))\n";
  source += "  [i32] second{try(/std/collections/map/tryAt(values, \"extra\"raw_utf8))}\n";
  source += "  print_line(first)\n";
  source += "  print_line(second)\n";
  source += "  return(Result.ok(plus(first, second)))\n";
  source += "}\n";
  return source;
}

inline std::string makeWrappedExperimentalMapResultFieldAssignConformanceSource() {
  std::string source;
  source += "import /std/collections/*\n";
  source += "import /std/collections/map/*\n";
  source += "import /std/collections/experimental_map/*\n\n";
  source += "[return<T> effects(heap_alloc)]\n";
  source += "wrapStatus<T>([T] status) {\n";
  source += "  return(status)\n";
  source += "}\n\n";
  source += "Holder() {\n";
  source += "  [Result<Map<string, i32>, ContainerError> mut] status{Result.ok(mapNew<string, i32>())}\n";
  source += "}\n\n";
  source += "[effects(io_err)]\n";
  source += "unexpectedWrappedExperimentalMapResultFieldAssignError([ContainerError] err) {\n";
  source += "  [Result<ContainerError>] current{err.code}\n";
  source += "  print_line_error(Result.why(current))\n";
  source += "}\n\n";
  source +=
      "[return<Result<int, ContainerError>> effects(io_out, heap_alloc) on_error<ContainerError, /unexpectedWrappedExperimentalMapResultFieldAssignError>]\n";
  source += "main() {\n";
  source += "  [Holder mut] holder{Holder()}\n";
  source +=
      "  assign(holder.status, wrapStatus(Result.ok(/std/collections/mapPair(\"left\"raw_utf8, 4i32, \"right\"raw_utf8, 7i32))))\n";
  source += "  [Map<string, i32>] values{try(holder.status)}\n";
  source += "  [i32] score{plus(/std/collections/map/count(values), /std/collections/map/at(values, \"left\"raw_utf8))}\n";
  source += "  print_line(score)\n";
  source += "  return(Result.ok(score))\n";
  source += "}\n";
  return source;
}

inline std::string makeWrappedExperimentalMapResultDerefFieldAssignConformanceSource() {
  std::string source;
  source += "import /std/collections/*\n";
  source += "import /std/collections/map/*\n";
  source += "import /std/collections/experimental_map/*\n\n";
  source += "[return<T> effects(heap_alloc)]\n";
  source += "wrapStatus<T>([T] status) {\n";
  source += "  return(status)\n";
  source += "}\n\n";
  source += "Holder() {\n";
  source += "  [Result<Map<string, i32>, ContainerError> mut] status{Result.ok(mapNew<string, i32>())}\n";
  source += "}\n\n";
  source += "[return<Reference<Holder>>]\n";
  source += "borrowHolder([Reference<Holder>] holder) {\n";
  source += "  return(holder)\n";
  source += "}\n\n";
  source += "[effects(io_err)]\n";
  source += "unexpectedWrappedExperimentalMapResultDerefFieldAssignError([ContainerError] err) {\n";
  source += "  [Result<ContainerError>] current{err.code}\n";
  source += "  print_line_error(Result.why(current))\n";
  source += "}\n\n";
  source +=
      "[return<Result<int, ContainerError>> effects(io_out, heap_alloc) on_error<ContainerError, /unexpectedWrappedExperimentalMapResultDerefFieldAssignError>]\n";
  source += "main() {\n";
  source += "  [Holder mut] holder{Holder()}\n";
  source +=
      "  assign(dereference(borrowHolder(location(holder))).status, wrapStatus(Result.ok(/std/collections/mapPair(\"left\"raw_utf8, 2i32, \"extra\"raw_utf8, 9i32))))\n";
  source += "  [Map<string, i32>] values{try(holder.status)}\n";
  source += "  [i32] count{/std/collections/map/count(values)}\n";
  source += "  [i32] extra{/std/collections/map/at(values, \"extra\"raw_utf8)}\n";
  source += "  print_line(count)\n";
  source += "  print_line(extra)\n";
  source += "  return(Result.ok(plus(count, extra)))\n";
  source += "}\n";
  return source;
}

inline std::string makeWrappedExperimentalMapStorageFieldConformanceSource() {
  std::string source;
  source += "import /std/collections/*\n";
  source += "import /std/collections/map/*\n";
  source += "import /std/collections/experimental_map/*\n\n";
  source += "[return<T> effects(heap_alloc)]\n";
  source += "wrapValues<T>([T] values) {\n";
  source += "  return(values)\n";
  source += "}\n\n";
  source += "Holder() {\n";
  source += "  [uninitialized<Map<string, i32>> mut] storage{uninitialized<Map<string, i32>>()}\n";
  source += "}\n\n";
  source += "[effects(io_out, heap_alloc), return<int>]\n";
  source += "main() {\n";
  source += "  [Holder mut] holder{Holder()}\n";
  source +=
      "  init(holder.storage, wrapValues(/std/collections/mapPair(\"left\"raw_utf8, 4i32, \"right\"raw_utf8, 7i32)))\n";
  source += "  [Map<string, i32>] values{take(holder.storage)}\n";
  source += "  [i32] score{plus(/std/collections/map/count(values),\n";
  source += "                   /std/collections/map/at(values, \"right\"raw_utf8))}\n";
  source += "  print_line(score)\n";
  source += "  return(score)\n";
  source += "}\n";
  return source;
}

inline std::string makeWrappedExperimentalMapStorageDerefFieldConformanceSource() {
  std::string source;
  source += "import /std/collections/*\n";
  source += "import /std/collections/map/*\n";
  source += "import /std/collections/experimental_map/*\n\n";
  source += "[return<T> effects(heap_alloc)]\n";
  source += "wrapValues<T>([T] values) {\n";
  source += "  return(values)\n";
  source += "}\n\n";
  source += "Holder() {\n";
  source += "  [uninitialized<Map<string, i32>> mut] storage{uninitialized<Map<string, i32>>()}\n";
  source += "}\n\n";
  source += "[return<Reference<Holder>>]\n";
  source += "borrowHolder([Reference<Holder>] holder) {\n";
  source += "  return(holder)\n";
  source += "}\n\n";
  source += "[effects(io_out, heap_alloc), return<int>]\n";
  source += "main() {\n";
  source += "  [Holder mut] holder{Holder()}\n";
  source +=
      "  init(dereference(borrowHolder(location(holder))).storage,\n";
  source +=
      "       wrapValues(/std/collections/mapPair(\"left\"raw_utf8, 4i32, \"right\"raw_utf8, 7i32)))\n";
  source += "  [Map<string, i32>] values{take(holder.storage)}\n";
  source += "  [i32] score{plus(/std/collections/map/count(values),\n";
  source += "                   /std/collections/map/at(values, \"right\"raw_utf8))}\n";
  source += "  print_line(score)\n";
  source += "  return(score)\n";
  source += "}\n";
  return source;
}

inline std::string makeWrapperMapHelperExperimentalValueConformanceSource() {
  std::string source;
  source += "import /std/collections/*\n";
  source += "import /std/collections/map/*\n";
  source += "import /std/collections/experimental_map/*\n\n";
  source += "[effects(io_err)]\n";
  source += "unexpectedWrapperMapHelperExperimentalValueError([ContainerError] err) {\n";
  source += "  [Result<ContainerError>] status{err.code}\n";
  source += "  print_line_error(Result.why(status))\n";
  source += "}\n\n";
  source +=
      "[return<Result<int, ContainerError>> effects(io_out, heap_alloc) on_error<ContainerError, /unexpectedWrapperMapHelperExperimentalValueError>]\n";
  source += "main() {\n";
  source +=
      "  [Map<string, i32>] values{mapPair<string, i32>(\"left\"raw_utf8, 4i32, \"right\"raw_utf8, 7i32)}\n";
  source += "  [i32] found{try(/std/collections/mapTryAt(values, \"left\"raw_utf8))}\n";
  source += "  [i32] count{/std/collections/mapCount(values)}\n";
  source += "  [i32] helperAt{/std/collections/mapAt(/std/collections/mapPair(\"extra\"raw_utf8, 9i32, \"other\"raw_utf8, 2i32), \"extra\"raw_utf8)}\n";
  source += "  [i32] helperUnsafe{/std/collections/mapAtUnsafe(/std/collections/map/map(\"bonus\"raw_utf8, 5i32, \"keep\"raw_utf8, 1i32), \"bonus\"raw_utf8)}\n";
  source += "  [i32 mut] total{plus(count, found)}\n";
  source +=
      "  assign(total, plus(total, helperAt))\n";
  source +=
      "  assign(total, plus(total, helperUnsafe))\n";
  source += "  [i32 mut] containsBonus{0i32}\n";
  source += "  if(/std/collections/mapContains(values, \"left\"raw_utf8),\n";
  source += "     then() { assign(containsBonus, 1i32) assign(total, plus(total, containsBonus)) },\n";
  source += "     else() { })\n";
  source += "  print_line(count)\n";
  source += "  print_line(found)\n";
  source += "  print_line(helperAt)\n";
  source += "  print_line(helperUnsafe)\n";
  source += "  print_line(containsBonus)\n";
  source += "  return(Result.ok(total))\n";
  source += "}\n";
  return source;
}

inline std::string makeExperimentalMapAssignConformanceSource() {
  std::string source;
  source += "import /std/collections/*\n";
  source += "import /std/collections/map/*\n";
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
  source += "  print_line(count)\n";
  source += "  print_line(found)\n";
  source += "  print_line(left)\n";
  source += "  print_line(right)\n";
  source += "  print_line(containsBonus)\n";
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
  source += "import /std/collections/map/*\n";
  source += "import /std/collections/experimental_map/*\n\n";
  source += "[return<auto> effects(heap_alloc)]\n";
  source += "buildValues() {\n";
  source +=
      "  [Map<string, i32>] out{/std/collections/mapPair(\"left\"raw_utf8, 4i32, \"right\"raw_utf8, 7i32)}\n";
  source += "  return(out)\n";
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
  source += "import /std/collections/map/*\n";
  source += "import /std/collections/experimental_map/*\n\n";
  source += "[return<auto> effects(heap_alloc)]\n";
  source += "buildValues() {\n";
  source +=
      "  [Map<string, i32>] out{/std/collections/map/map(\"left\"raw_utf8, 4i32, \"right\"raw_utf8, 7i32)}\n";
  source += "  return(out)\n";
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
  source += "  [i32] count{/std/collections/map/count<string, i32>(values)}\n";
  source += "  print_line(count)\n";
  source += "  print_line(left)\n";
  source += "  print_line(extra)\n";
  source += "  return(Result.ok(plus(count, plus(left, extra))))\n";
  source += "}\n";
  return source;
}

inline std::string makeBlockInferredExperimentalMapReturnConformanceSource() {
  std::string source;
  source += "import /std/collections/*\n";
  source += "import /std/collections/map/*\n";
  source += "import /std/collections/experimental_map/*\n\n";
  source += "[return<auto> effects(heap_alloc)]\n";
  source += "buildValues([bool] useCanonical) {\n";
  source += "  if(useCanonical,\n";
  source += "     then() {\n";
  source += "       [Map<string, i32>] values{/std/collections/map/map(\"left\"raw_utf8, 4i32, \"right\"raw_utf8, 7i32)}\n";
  source += "       return(values)\n";
  source += "     },\n";
  source += "     else() {\n";
  source += "       [Map<string, i32>] values{/std/collections/mapPair(\"left\"raw_utf8, 4i32, \"other\"raw_utf8, 2i32)}\n";
  source += "       return(values)\n";
  source += "     })\n";
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
  source += "  [i32] count{/std/collections/map/count<string, i32>(values)}\n";
  source += "  print_line(count)\n";
  source += "  print_line(left)\n";
  source += "  print_line(extra)\n";
  source += "  return(Result.ok(plus(count, plus(left, extra))))\n";
  source += "}\n";
  return source;
}

inline std::string makeAutoBlockInferredExperimentalMapReturnConformanceSource() {
  std::string source;
  source += "import /std/collections/*\n";
  source += "import /std/collections/map/*\n";
  source += "import /std/collections/experimental_map/*\n\n";
  source += "[return<auto> effects(heap_alloc)]\n";
  source += "buildValues([bool] useCanonical) {\n";
  source += "  if(useCanonical,\n";
  source += "     then() {\n";
  source += "       [Map<string, i32> mut] values{/std/collections/map/map(\"left\"raw_utf8, 4i32, \"right\"raw_utf8, 7i32)}\n";
  source += "       mapInsert<string, i32>(values, \"extra\"raw_utf8, 9i32)\n";
  source += "       return(values)\n";
  source += "     },\n";
  source += "     else() {\n";
  source += "       [Map<string, i32>] values{/std/collections/mapPair(\"left\"raw_utf8, 4i32, \"other\"raw_utf8, 2i32)}\n";
  source += "       return(values)\n";
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
  source += "  [i32] count{/std/collections/map/count<string, i32>(values)}\n";
  source += "  print_line(count)\n";
  source += "  print_line(left)\n";
  source += "  print_line(extra)\n";
  source += "  return(Result.ok(plus(count, plus(left, extra))))\n";
  source += "}\n";
  return source;
}

inline std::string makeWrappedInferredExperimentalMapReturnConformanceSource() {
  std::string source;
  source += "import /std/collections/*\n";
  source += "import /std/collections/map/*\n";
  source += "import /std/collections/experimental_map/*\n\n";
  source += "[return<T> effects(heap_alloc)]\n";
  source += "wrapValues<T>([T] values) {\n";
  source += "  return(values)\n";
  source += "}\n\n";
  source += "[return<auto> effects(heap_alloc)]\n";
  source += "buildValues([bool] useCanonical) {\n";
  source += "  if(useCanonical,\n";
  source += "     then() { return(wrapValues(/std/collections/map/map(\"left\"raw_utf8, 4i32, \"right\"raw_utf8, 7i32))) },\n";
  source += "     else() { return(wrapValues(/std/collections/mapPair(\"extra\"raw_utf8, 9i32, \"bonus\"raw_utf8, 5i32))) })\n";
  source += "}\n\n";
  source += "[effects(io_err)]\n";
  source += "unexpectedWrappedExperimentalMapReturnError([ContainerError] err) {\n";
  source += "  [Result<ContainerError>] status{err.code}\n";
  source += "  print_line_error(Result.why(status))\n";
  source += "}\n\n";
  source +=
      "[return<Result<int, ContainerError>> effects(io_out, heap_alloc) on_error<ContainerError, /unexpectedWrappedExperimentalMapReturnError>]\n";
  source += "main() {\n";
  source += "  [Map<string, i32>] canonical{buildValues(true)}\n";
  source += "  [Map<string, i32>] wrapped{buildValues(false)}\n";
  source += "  [i32] left{try(/std/collections/map/tryAt<string, i32>(canonical, \"left\"raw_utf8))}\n";
  source += "  [i32] bonus{try(/std/collections/map/tryAt<string, i32>(wrapped, \"bonus\"raw_utf8))}\n";
  source += "  [i32] count{/std/collections/map/count<string, i32>(canonical)}\n";
  source += "  print_line(count)\n";
  source += "  print_line(left)\n";
  source += "  print_line(bonus)\n";
  source += "  return(Result.ok(plus(count, plus(left, bonus))))\n";
  source += "}\n";
  return source;
}

inline std::string makeInferredExperimentalMapCallReceiverConformanceSource() {
  std::string source;
  source += "import /std/collections/*\n";
  source += "import /std/collections/map/*\n";
  source += "import /std/collections/experimental_map/*\n\n";
  source += "[return<auto> effects(heap_alloc)]\n";
  source += "buildValues([bool] useCanonical) {\n";
  source += "  if(useCanonical,\n";
  source += "     then() {\n";
  source += "       [Map<string, i32>] values{/std/collections/map/map(\"left\"raw_utf8, 4i32, \"right\"raw_utf8, 7i32)}\n";
  source += "       return(values)\n";
  source += "     },\n";
  source += "     else() {\n";
  source += "       [Map<string, i32>] values{/std/collections/mapPair(\"left\"raw_utf8, 4i32, \"other\"raw_utf8, 2i32)}\n";
  source += "       return(values)\n";
  source += "     })\n";
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
  source += "  [i32] count{buildValues(true).count()}\n";
  source += "  [i32 mut] total{plus(count, left)}\n";
  source += "  [i32 mut] containsBonus{0i32}\n";
  source += "  if(buildValues(true).contains(\"right\"raw_utf8),\n";
  source += "     then() { assign(containsBonus, 1i32) assign(total, plus(total, containsBonus)) },\n";
  source += "     else() { })\n";
  source += "  print_line(count)\n";
  source += "  print_line(left)\n";
  source += "  print_line(containsBonus)\n";
  source += "  return(Result.ok(total))\n";
  source += "}\n";
  return source;
}
