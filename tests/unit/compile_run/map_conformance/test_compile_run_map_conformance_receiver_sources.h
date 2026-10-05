#pragma once

inline std::string makeExperimentalMapStructFieldConformanceSource() {
  std::string source;
  source += "import /std/collections/*\n";
  source += "import /std/collections/map/*\n";
  source += "import /std/collections/map/*\n\n";
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
      "  [Holder] holder{Holder(/std/collections/map/mapPair(\"left\"raw_utf8, 4i32, \"right\"raw_utf8, 7i32),\n";
  source +=
      "                        /std/collections/map/mapPair(\"other\"raw_utf8, 2i32, \"extra\"raw_utf8, 9i32))}\n";
  source += "  [i32] left{try(/std/collections/map/tryAt<string, i32>(holder.primary, \"left\"raw_utf8))}\n";
  source += "  [i32] extra{try(/std/collections/map/tryAt<string, i32>(holder.secondary, \"extra\"raw_utf8))}\n";
  source += "  return(Result.ok(plus(left, extra)))\n";
  source += "}\n";
  return source;
}

inline std::string makeInferredExperimentalMapStructFieldConformanceSource() {
  std::string source;
  source += "import /std/collections/*\n";
  source += "import /std/collections/map/*\n";
  source += "import /std/collections/map/*\n\n";
  source += "[struct]\n";
  source += "Holder() {\n";
  source += "  primary{Map<string, i32>{}}\n";
  source += "  secondary{Map<string, i32>{}}\n";
  source += "}\n\n";
  source += "[effects(heap_alloc), return<int>]\n";
  source += "main() {\n";
  source += "  [Holder mut] holder{Holder(/std/collections/map/mapPair(\"left\"raw_utf8, 4i32, \"right\"raw_utf8, 7i32))}\n";
  source +=
      "  assign(holder.secondary, /std/collections/map/mapPair(\"extra\"raw_utf8, 9i32, \"other\"raw_utf8, 2i32))\n";
  source +=
      "  return(plus(/std/collections/map/at<string, i32>(holder.primary, \"left\"raw_utf8), /std/collections/map/at<string, i32>(holder.secondary, \"extra\"raw_utf8)))\n";
  source += "}\n";
  return source;
}

inline std::string makeWrappedInferredExperimentalMapStructFieldConformanceSource() {
  std::string source;
  source += "import /std/collections/*\n";
  source += "import /std/collections/map/*\n";
  source += "\n";
  source += "[return<T> effects(heap_alloc)]\n";
  source += "wrapValues<T>([T move] values) {\n";
  source += "  return(values)\n";
  source += "}\n";
  source += "\n";
  source += "[struct]\n";
  source += "Holder() {\n";
  source += "  primary{wrapValues(mapSingle<string, i32>(\"seed\"raw_utf8, 1i32))}\n";
  source += "  secondary{wrapValues(mapSingle<string, i32>(\"seed\"raw_utf8, 2i32))}\n";
  source += "}\n";
  source += "\n";
  source += "[effects(heap_alloc), return<int>]\n";
  source += "main() {\n";
  source += "  [Holder mut] holder{Holder{wrapValues(mapPair(\"left\"raw_utf8, 4i32, \"right\"raw_utf8, 7i32)), wrapValues(mapSingle<string, i32>(\"seed\"raw_utf8, 2i32))}}\n";
  source += "  assign(holder.secondary, wrapValues(mapPair(\"extra\"raw_utf8, 9i32, \"other\"raw_utf8, 2i32)))\n";
  source += "  return(plus(holder.primary.at(\"left\"raw_utf8), holder.secondary.at(\"extra\"raw_utf8)))\n";
  source += "}\n";
  return source;
}

inline std::string makeExperimentalMapMethodParameterConformanceSource() {
  std::string source;
  source += "import /std/collections/*\n";
  source += "import /std/collections/map/*\n";
  source += "import /std/collections/map/*\n\n";
  source += "Holder() {}\n\n";
  source += "[return<int> effects(io_out, heap_alloc)]\n";
  source += "/Holder/score([Holder] self, [Map<string, i32>] values) {\n";
  source += "  [i32] count{/std/collections/map/count(values)}\n";
  source += "  [i32] left{/std/collections/map/at_unsafe(values, \"left\"raw_utf8)}\n";
  source += "  print_line(count)\n";
  source += "  print_line(left)\n";
  source += "  return(plus(count, left))\n";
  source += "}\n\n";
  source += "[effects(io_out, heap_alloc), return<int>]\n";
  source += "main() {\n";
  source += "  [Holder] holder{Holder()}\n";
  source +=
      "  [i32] canonical{holder.score(/std/collections/map/mapPair(\"left\"raw_utf8, 4i32, \"right\"raw_utf8, 7i32))}\n";
  source +=
      "  [i32] wrapped{holder.score(/std/collections/map/mapPair(\"left\"raw_utf8, 2i32, \"extra\"raw_utf8, 9i32))}\n";
  source += "  print_line(canonical)\n";
  source += "  print_line(wrapped)\n";
  source += "  return(plus(canonical, wrapped))\n";
  source += "}\n";
  return source;
}

inline std::string makeInferredExperimentalMapParameterConformanceSource() {
  std::string source;
  source += "import /std/collections/*\n";
  source += "import /std/collections/map/*\n";
  source += "\n";
  source += "Holder() {}\n";
  source += "\n";
  source += "[return<int> effects(io_out, heap_alloc)]\n";
  source += "scoreValues([auto mut] values) {\n";
  source += "  values.insert(\"extra\"raw_utf8, 9i32)\n";
  source += "  [i32] count{values.count()}\n";
  source += "  [i32] left{values.at(\"left\"raw_utf8)}\n";
  source += "  print_line(count)\n";
  source += "  print_line(left)\n";
  source += "  return(plus(count, left))\n";
  source += "}\n";
  source += "\n";
  source += "[return<int> effects(io_out, heap_alloc)]\n";
  source += "/Holder/score([Holder] self, [auto mut] values) {\n";
  source += "  values.insert(\"bonus\"raw_utf8, 5i32)\n";
  source += "  [i32] count{values.count()}\n";
  source += "  [i32] extra{values.at(\"extra\"raw_utf8)}\n";
  source += "  print_line(count)\n";
  source += "  print_line(extra)\n";
  source += "  return(plus(count, extra))\n";
  source += "}\n";
  source += "\n";
  source += "[effects(io_out, heap_alloc), return<int>]\n";
  source += "main() {\n";
  source += "  [Holder] holder{Holder()}\n";
  source += "  [i32] direct{scoreValues(mapPair(\"left\"raw_utf8, 4i32, \"right\"raw_utf8, 7i32))}\n";
  source += "  [i32] wrapped{holder.score(mapPair(\"left\"raw_utf8, 2i32, \"extra\"raw_utf8, 9i32))}\n";
  source += "  print_line(direct)\n";
  source += "  print_line(wrapped)\n";
  source += "  return(plus(direct, wrapped))\n";
  source += "}\n";
  return source;
}

inline std::string makeInferredExperimentalMapDefaultParameterConformanceSource() {
  std::string source;
  source += "import /std/collections/*\n";
  source += "import /std/collections/map/*\n";
  source += "\n";
  source += "[return<int> effects(io_out, heap_alloc)]\n";
  source += "scoreValues([auto mut] values) {\n";
  source += "  values.insert(\"left\"raw_utf8, 4i32)\n";
  source += "  [i32] count{values.count()}\n";
  source += "  [i32] left{values.at(\"left\"raw_utf8)}\n";
  source += "  print_line(count)\n";
  source += "  print_line(left)\n";
  source += "  return(plus(count, left))\n";
  source += "}\n";
  source += "\n";
  source += "[return<int> effects(io_out, heap_alloc)]\n";
  source += "scorePair([auto mut] values) {\n";
  source += "  values.insert(\"bonus\"raw_utf8, 5i32)\n";
  source += "  [i32] count{values.count()}\n";
  source += "  [i32] bonus{values.at(\"bonus\"raw_utf8)}\n";
  source += "  print_line(count)\n";
  source += "  print_line(bonus)\n";
  source += "  return(plus(count, bonus))\n";
  source += "}\n";
  source += "\n";
  source += "[effects(io_out, heap_alloc), return<int>]\n";
  source += "main() {\n";
  source += "  [i32] explicitValues{scoreValues(mapPair(\"left\"raw_utf8, 4i32, \"right\"raw_utf8, 7i32))}\n";
  source += "  [Map<string, i32> mut] empty{Map<string, i32>{}}\n";
  source += "  [i32] emptyValues{scoreValues(empty)}\n";
  source += "  [i32] pairValues{scorePair(mapPair(\"left\"raw_utf8, 6i32, \"right\"raw_utf8, 8i32))}\n";
  source += "  print_line(explicitValues)\n";
  source += "  print_line(emptyValues)\n";
  source += "  print_line(pairValues)\n";
  source += "  return(plus(plus(explicitValues, emptyValues), pairValues))\n";
  source += "}\n";
  return source;
}

inline std::string makeWrappedInferredExperimentalMapDefaultParameterConformanceSource() {
  std::string source;
  source += "import /std/collections/*\n";
  source += "import /std/collections/map/*\n";
  source += "\n";
  source += "[return<T> effects(heap_alloc)]\n";
  source += "wrapValues<T>([T move] values) {\n";
  source += "  return(values)\n";
  source += "}\n";
  source += "\n";
  source += "[return<int> effects(io_out, heap_alloc)]\n";
  source += "scoreValues([auto mut] values) {\n";
  source += "  values.insert(\"left\"raw_utf8, 4i32)\n";
  source += "  values.insert(\"extra\"raw_utf8, 9i32)\n";
  source += "  [i32] count{values.count()}\n";
  source += "  [i32] left{values.at(\"left\"raw_utf8)}\n";
  source += "  print_line(count)\n";
  source += "  print_line(left)\n";
  source += "  return(plus(count, left))\n";
  source += "}\n";
  source += "\n";
  source += "[effects(io_out, heap_alloc), return<int>]\n";
  source += "main() {\n";
  source += "  [i32] explicitValues{scoreValues(wrapValues(mapPair(\"left\"raw_utf8, 4i32, \"right\"raw_utf8, 7i32)))}\n";
  source += "  [i32] otherValues{scoreValues(wrapValues(mapPair(\"left\"raw_utf8, 2i32, \"other\"raw_utf8, 7i32)))}\n";
  source += "  [i32] singleValues{scoreValues(wrapValues(mapSingle<string, i32>(\"seed\"raw_utf8, 1i32)))}\n";
  source += "  print_line(explicitValues)\n";
  source += "  print_line(otherValues)\n";
  source += "  print_line(singleValues)\n";
  source += "  return(plus(plus(explicitValues, otherValues), singleValues))\n";
  source += "}\n";
  return source;
}

inline std::string makeExperimentalMapHelperReceiverConformanceSource() {
  std::string source;
  source += "import /std/collections/*\n";
  source += "import /std/collections/map/*\n";
  source += "import /std/collections/map/*\n\n";
  source += "[effects(io_err)]\n";
  source += "unexpectedExperimentalMapHelperReceiverError([ContainerError] err) {\n";
  source += "  [Result<ContainerError>] status{err.code}\n";
  source += "  print_line_error(Result.why(status))\n";
  source += "}\n\n";
  source +=
      "[return<Result<int, ContainerError>> effects(io_out, heap_alloc) on_error<ContainerError, /unexpectedExperimentalMapHelperReceiverError>]\n";
  source += "main() {\n";
  source +=
      "  [i32] found{try(mapPair(\"left\"raw_utf8, 4i32, \"right\"raw_utf8, 7i32).tryAt(\"left\"raw_utf8))}\n";
  source +=
      "  [i32] count{mapPair(\"left\"raw_utf8, 4i32, \"right\"raw_utf8, 7i32).count()}\n";
  source +=
      "  [i32] extra{mapPair(\"extra\"raw_utf8, 9i32, \"other\"raw_utf8, 2i32).at(\"extra\"raw_utf8)}\n";
  source +=
      "  [i32] bonus{mapPair(\"bonus\"raw_utf8, 5i32, \"keep\"raw_utf8, 1i32).at_unsafe(\"bonus\"raw_utf8)}\n";
  source += "  [i32 mut] total{plus(count, found)}\n";
  source += "  assign(total, plus(total, extra))\n";
  source += "  assign(total, plus(total, bonus))\n";
  source += "  [i32 mut] containsBonus{0i32}\n";
  source +=
      "  if(mapPair(\"left\"raw_utf8, 4i32, \"right\"raw_utf8, 7i32).contains(\"right\"raw_utf8),\n";
  source += "     then() { assign(containsBonus, 1i32) assign(total, plus(total, containsBonus)) },\n";
  source += "     else() { })\n";
  source += "  print_line(count)\n";
  source += "  print_line(found)\n";
  source += "  print_line(extra)\n";
  source += "  print_line(bonus)\n";
  source += "  print_line(containsBonus)\n";
  source += "  return(Result.ok(total))\n";
  source += "}\n";
  return source;
}

inline std::string makeExperimentalMapMethodReceiverConformanceSource() {
  std::string source;
  source += "import /std/collections/*\n";
  source += "import /std/collections/map/*\n";
  source += "import /std/collections/map/*\n\n";
  source += "[effects(io_err)]\n";
  source += "unexpectedExperimentalMapMethodReceiverError([ContainerError] err) {\n";
  source += "  [Result<ContainerError>] status{err.code}\n";
  source += "  print_line_error(Result.why(status))\n";
  source += "}\n\n";
  source +=
      "[return<Result<int, ContainerError>> effects(io_out, heap_alloc) on_error<ContainerError, /unexpectedExperimentalMapMethodReceiverError>]\n";
  source += "main() {\n";
  source +=
      "  [i32] found{try(/std/collections/map/map(\"left\"raw_utf8, 4i32, \"right\"raw_utf8, 7i32).tryAt(\"left\"raw_utf8))}\n";
  source +=
      "  [i32 mut] total{plus(/std/collections/map/mapPair(\"left\"raw_utf8, 4i32, \"right\"raw_utf8, 7i32).count(), found)}\n";
  source +=
      "  assign(total, plus(total, /std/collections/map/mapPair(\"extra\"raw_utf8, 9i32, \"other\"raw_utf8, 2i32).at(\"extra\"raw_utf8)))\n";
  source += "  return(Result.ok(total))\n";
  source += "}\n";
  return source;
}

inline std::string makeWrappedExperimentalMapMethodReceiverConformanceSource() {
  std::string source;
  source += "import /std/collections/*\n";
  source += "import /std/collections/map/*\n";
  source += "import /std/collections/map/*\n\n";
  source += "[return<T> effects(heap_alloc)]\n";
  source += "wrapValues<T>([T move] values) {\n";
  source += "  return(values)\n";
  source += "}\n\n";
  source += "[effects(io_err)]\n";
  source += "unexpectedWrappedExperimentalMapMethodReceiverError([ContainerError] err) {\n";
  source += "  [Result<ContainerError>] status{err.code}\n";
  source += "  print_line_error(Result.why(status))\n";
  source += "}\n\n";
  source +=
      "[return<Result<int, ContainerError>> effects(io_out, heap_alloc) on_error<ContainerError, /unexpectedWrappedExperimentalMapMethodReceiverError>]\n";
  source += "main() {\n";
  source +=
      "  [auto] values{wrapValues(/std/collections/map/mapPair(\"left\"raw_utf8, 4i32, \"right\"raw_utf8, 7i32))}\n";
  source +=
      "  [auto] extraValues{wrapValues(/std/collections/map/mapPair(\"extra\"raw_utf8, 9i32, \"other\"raw_utf8, 2i32))}\n";
  source +=
      "  [i32] found{try(/std/collections/map/tryAt(values, \"left\"raw_utf8))}\n";
  source += "  [i32 mut] total{plus(values.count(), found)}\n";
  source += "  assign(total, plus(total, extraValues.at(\"extra\"raw_utf8)))\n";
  source +=
      "  if(values.contains(\"right\"raw_utf8),\n";
  source += "     then() { assign(total, plus(total, 1i32)) },\n";
  source += "     else() { })\n";
  source += "  return(Result.ok(total))\n";
  source += "}\n";
  return source;
}

inline std::string makeWrappedExperimentalMapHelperReceiverConformanceSource() {
  std::string source;
  source += "import /std/collections/*\n";
  source += "import /std/collections/map/*\n";
  source += "import /std/collections/map/*\n\n";
  source += "[return<T> effects(heap_alloc)]\n";
  source += "wrapValues<T>([T move] values) {\n";
  source += "  return(values)\n";
  source += "}\n\n";
  source += "[effects(io_err)]\n";
  source += "unexpectedWrappedExperimentalMapHelperReceiverError([ContainerError] err) {\n";
  source += "  [Result<ContainerError>] status{err.code}\n";
  source += "  print_line_error(Result.why(status))\n";
  source += "}\n\n";
  source +=
      "[return<Result<int, ContainerError>> effects(io_out, heap_alloc) on_error<ContainerError, /unexpectedWrappedExperimentalMapHelperReceiverError>]\n";
  source += "main() {\n";
  source +=
      "  [i32] found{try(wrapValues(mapPair(\"left\"raw_utf8, 4i32, \"right\"raw_utf8, 7i32)).tryAt(\"left\"raw_utf8))}\n";
  source +=
      "  [i32 mut] total{plus(wrapValues(mapPair(\"left\"raw_utf8, 4i32, \"right\"raw_utf8, 7i32)).count(), found)}\n";
  source +=
      "  assign(total, plus(total, wrapValues(mapPair(\"extra\"raw_utf8, 9i32, \"other\"raw_utf8, 2i32)).at(\"extra\"raw_utf8)))\n";
  source +=
      "  if(wrapValues(mapPair(\"left\"raw_utf8, 4i32, \"right\"raw_utf8, 7i32)).contains(\"right\"raw_utf8),\n";
  source += "     then() { assign(total, plus(total, 1i32)) },\n";
  source += "     else() { })\n";
  source += "  return(Result.ok(total))\n";
  source += "}\n";
  return source;
}

inline std::string makeExperimentalMapFieldAssignConformanceSource() {
  std::string source;
  source += "import /std/collections/*\n";
  source += "import /std/collections/map/*\n";
  source += "import /std/collections/map/*\n\n";
  source += "Holder() {\n";
  source += "  [Map<string, i32> mut] primary{Map<string, i32>{}}\n";
  source += "  [Map<string, i32> mut] secondary{Map<string, i32>{}}\n";
  source += "}\n\n";
  source += "[effects(heap_alloc), return<int>]\n";
  source += "main() {\n";
  source += "  [Holder mut] holder{Holder()}\n";
  source +=
      "  assign(holder.primary, /std/collections/map/mapPair(\"left\"raw_utf8, 4i32, \"right\"raw_utf8, 7i32))\n";
  source +=
      "  assign(holder.secondary, /std/collections/map/mapPair(\"extra\"raw_utf8, 9i32, \"other\"raw_utf8, 2i32))\n";
  source +=
      "  return(plus(/std/collections/map/at<string, i32>(holder.primary, \"left\"raw_utf8), /std/collections/map/at<string, i32>(holder.secondary, \"extra\"raw_utf8)))\n";
  source += "}\n";
  return source;
}
