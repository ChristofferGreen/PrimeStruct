#pragma once

#include <map>
#include <string>
#include <vector>

namespace primec {

// One step of an exported build graph (docs/BuildSystemPlan.md section 4): the fields of a `compile`, `executable`,
// `copy` or `function` node of the JSON that `graph.export()` writes. A `copy` step has a `source` and an `output` and
// no compiler settings. A `function` step has the file `source`, the definition `function` in it, the files `inputs`
// it reads (relative to the project) and the files `outputs` it writes (relative to the output directory).
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
  // Directories whose listings the step depends on (`Build.watch`); relative paths are relative to the project.
  std::vector<std::string> inputDirs;
  // The ids of the steps this one waits for (`"after"`): it starts when they have finished successfully.
  std::vector<std::string> after;
  // A function step only.
  std::string function;
  std::vector<std::string> inputs;
  std::vector<std::string> outputs;
};

// One option a script declared with `Build.option` (the `"options"` of the export): the text of the default and of the
// value this run gave it, and the problem with the given value (empty when it is fine).
struct BuildOption {
  std::string name;
  std::string type;
  std::string defaultText;
  std::string description;
  std::string value;
  std::string error;
};

// The part of an exported graph that `primec build` runs.
struct BuildGraphData {
  // The compile and executable nodes, in the order of the export (sorted by id).
  std::vector<BuildStep> steps;
  // Library and link nodes, which cannot run yet: their ids.
  std::vector<std::string> unsupportedNodes;
  // The `problems` of the export (duplicate ids, links to missing targets).
  std::vector<std::string> problems;
  // The export ended at a `run` of the script (`"incomplete":true`): later stages follow.
  bool incomplete = false;
  // The folders of `b.subproject` (`"subprojects"`), sorted; the graph of each is merged by `mergeBuildGraphJson`.
  std::vector<std::string> subprojects;
  // The declared options, sorted by name.
  std::vector<BuildOption> options;
  // The export says `"selfContained":false`: the graph has a function step, which only the runner can execute.
  bool selfContained = true;
};

// The graph of one subproject: its folder relative to the project root (normalized, `/` separators) and the JSON
// export of its own script.
struct BuildSubgraph {
  std::string directory;
  std::string text;
};

// Parses the JSON text of a graph export. Returns false and sets `error` when the text is not a graph export.
bool parseBuildGraphJson(const std::string &text, BuildGraphData &graph, std::string &error);

// Merges the exports of subprojects into the export of the project (docs/BuildSystemPlan.md section 7). Each node of a
// subgraph is added with its id (and link ends) prefixed `<directory>/`, its source, output, import paths, watched
// directories and the link arguments that name a file of the subproject made relative to the project root, and its
// problems prefixed `<directory>: `. The result lists the nodes sorted as a single export does, merges the `"options"` (one entry per name, sorted by name; a name declared with another type or default is a
// problem) and keeps the other fields of the project's export and drops `"subprojects"`. `projectDir` is where the link arguments are looked up.
bool mergeBuildGraphJson(const std::string &parentText,
                         const std::vector<BuildSubgraph> &subgraphs,
                         const std::string &projectDir,
                         std::string &merged,
                         std::string &error);

// Adds `"closure":[...]` (after `"outputs"`) to the function nodes named in `closures` (node id to the files of the import
// closure of the step, relative to the project): `primec build --dry-run` lists them so a tool can see what a function
// step depends on. The text is otherwise the export unchanged.
bool addBuildClosures(const std::string &graphText,
                      const std::map<std::string, std::vector<std::string>> &closures,
                      std::string &annotated,
                      std::string &error);

} // namespace primec
