#pragma once

#include <string>
#include <vector>

namespace primec {

// One compile step of an exported build graph (docs/BuildSystemPlan.md section 4): the fields of a `compile` or
// `executable` node of the JSON that `graph.export()` writes.
struct BuildStep {
  std::string id;
  std::string kind;
  std::string name;
  std::string source;
  std::string output;
  std::string emitKind;
  int optimization = 0;
  std::string entry;
  std::vector<std::string> importPaths;
  std::vector<std::string> transforms;
  std::vector<std::string> defaultEffects;
  std::vector<std::string> linkArgs;
};

// The part of an exported graph that `primec build` runs.
struct BuildGraphData {
  // The compile and executable nodes, in the order of the export (sorted by id).
  std::vector<BuildStep> steps;
  // Library and link nodes, which cannot run yet: their ids.
  std::vector<std::string> unsupportedNodes;
  // The `problems` of the export (duplicate ids, links to missing targets).
  std::vector<std::string> problems;
};

// Parses the JSON text of a graph export. Returns false and sets `error` when the text is not a graph export.
bool parseBuildGraphJson(const std::string &text, BuildGraphData &graph, std::string &error);

} // namespace primec
